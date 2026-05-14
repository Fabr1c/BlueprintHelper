// BlueprintHelper Service Layer 。MergeBlueprintGraph 核心服务实现

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

FBlueprintHelperMergeBlueprintGraphService::FBlueprintHelperMergeBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperLogicJsonPathService& InPathService,
	const FBlueprintHelperTransactionJournalService& InJournalService)
	: Resolver(InResolver), PathService(InPathService), JournalService(InJournalService)
{
}

// ─── 公共入口 ───

class FBlueprintHelperMergeBlueprintGraphServiceLocalUtils
{
public:
	static UFunction* ResolveMergeCallableFunction(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& FunctionName,
		FString* OutErrorCode = nullptr,
		FString* OutMessage = nullptr,
		FBlueprintHelperCallFunctionCandidate* OutCandidate = nullptr)
	{
		if (OutErrorCode)
		{
			OutErrorCode->Reset();
		}
		if (OutMessage)
		{
			OutMessage->Reset();
		}
		if (OutCandidate)
		{
			*OutCandidate = FBlueprintHelperCallFunctionCandidate();
		}

		if (FunctionName.IsEmpty())
		{
			if (OutErrorCode)
			{
				*OutErrorCode = TEXT("function_call_not_found");
			}
			if (OutMessage)
			{
				*OutMessage = TEXT("call_function.name is empty.");
			}
			return nullptr;
		}

		FBlueprintHelperCallFunctionResolveRequest ResolveRequest;
		ResolveRequest.Blueprint = Blueprint;
		ResolveRequest.Graph = Graph;
		ResolveRequest.Query = FunctionName;
		const FBlueprintHelperCallFunctionResolveResult ResolveResult =
			FBlueprintHelperCallFunctionResolver::Resolve(ResolveRequest);
		if (!ResolveResult.IsResolved())
		{
			if (OutErrorCode)
			{
				*OutErrorCode = ResolveResult.ErrorCode;
			}
			if (OutMessage)
			{
				*OutMessage = ResolveResult.Message;
			}
			return nullptr;
		}

		if (OutCandidate)
		{
			*OutCandidate = ResolveResult.Selected;
		}
		return ResolveResult.Selected.Function.Get();
	}

	static FString FormatResolverFailure(const FString& ResolveCode, const FString& ResolveMessage, const FString& FunctionName)
	{
		if (ResolveMessage.IsEmpty())
		{
			return FString::Printf(TEXT("%s: %s"), *ResolveCode, *FunctionName);
		}

		return ResolveCode.IsEmpty()
			? ResolveMessage
			: FString::Printf(TEXT("%s: %s"), *ResolveCode, *ResolveMessage);
	}

	static UK2Node_CallFunction* CreateMergeCallFunctionNode(
		UEdGraph* Graph,
		const FBlueprintHelperCallFunctionCandidate& Candidate)
	{
		if (!Graph || !Candidate.Function.IsValid())
		{
			return nullptr;
		}

		FParsedNode NodeData;
		NodeData.Id = Candidate.Function->GetName();
		NodeData.FunctionName = Candidate.Function->GetName();

		FBlueprintHelperNodeFragment Fragment;
		FString Error;
		return FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(Graph, NodeData, Fragment, Error)
			? Cast<UK2Node_CallFunction>(Fragment.PrimaryNode)
			: nullptr;
	}

	static UK2Node_CallFunction* CreateMergeCallFunctionNode(UEdGraph* Graph, UFunction* Function)
	{
		if (!Graph || !Function)
		{
			return nullptr;
		}

		FParsedNode NodeData;
		NodeData.Id = Function->GetName();
		NodeData.FunctionName = Function->GetName();
		FBlueprintHelperNodeFragment Fragment;
		FString Error;
		return FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(Graph, NodeData, Fragment, Error)
			? Cast<UK2Node_CallFunction>(Fragment.PrimaryNode)
			: nullptr;
	}

	static void MarkMergeNodeAsBlueprintHelperOwned(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node || BlockId.IsEmpty())
		{
			return;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
			MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
		}
	}

	static bool TryReadMergeNodeBlockId(UEdGraphNode* Node, FString& OutBlockId)
	{
		OutBlockId.Reset();
		if (!Node)
		{
			return false;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			if (MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) == TEXT("true"))
			{
				OutBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
				if (!OutBlockId.IsEmpty())
				{
					return true;
				}
			}
		}

		const FString LegacyPrefix = TEXT("block_id=");
		const int32 PrefixIndex = Node->NodeComment.Find(LegacyPrefix);
		if (PrefixIndex != INDEX_NONE)
		{
			FString Remainder = Node->NodeComment.RightChop(PrefixIndex + LegacyPrefix.Len());
			Remainder.Split(TEXT("\n"), &OutBlockId, nullptr);
			OutBlockId.TrimStartAndEndInline();
		}
		return !OutBlockId.IsEmpty();
	}

	static UEdGraphNode* FindOwnedNodeByBlockId(UEdGraph* Graph, const FString& BlockId)
	{
		if (!Graph || BlockId.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			FString NodeBlockId;
			if (TryReadMergeNodeBlockId(Node, NodeBlockId) &&
				NodeBlockId == BlockId)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static UK2Node_CustomEvent* FindOwnedCustomEventEntryNode(UEdGraph* Graph, const FString& BlockId)
	{
		if (!Graph || BlockId.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			FString NodeBlockId;
			if (TryReadMergeNodeBlockId(Node, NodeBlockId) &&
				NodeBlockId == BlockId)
			{
				if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
				{
					return CustomEvent;
				}
			}
		}
		return nullptr;
	}

	static FString GetOwnedCustomEventCallableName(const UK2Node_CustomEvent* EventNode)
	{
		if (!EventNode)
		{
			return FString();
		}

		return EventNode->CustomFunctionName.IsNone()
			? EventNode->GetName()
			: EventNode->CustomFunctionName.ToString();
	}

	static UFunction* FindOwnedCustomEventFunction(UBlueprint* Blueprint, const FString& EventName)
	{
		if (!Blueprint || EventName.IsEmpty())
		{
			return nullptr;
		}

		const FName FunctionName(*EventName);
		if (Blueprint->SkeletonGeneratedClass)
		{
			if (UFunction* Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(FunctionName))
			{
				return Function;
			}
		}

		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass != Blueprint->SkeletonGeneratedClass)
		{
			if (UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(FunctionName))
			{
				return Function;
			}
		}

		return nullptr;
	}

	struct FOwnedBlockCallableCheck
	{
		bool bOk = false;
		FString Code;
		FString Message;
		FString EventName;
		UK2Node_CustomEvent* EventNode = nullptr;
		UFunction* Function = nullptr;
		FBlueprintHelperCallFunctionCandidate Candidate;
	};

	static FOwnedBlockCallableCheck CheckOwnedBlockCallable(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& BlockId)
	{
		FOwnedBlockCallableCheck Result;
		if (BlockId.IsEmpty())
		{
			Result.Code = TEXT("inserted_logic_not_found");
			Result.Message = TEXT("inserted_logic_not_found: missing inserted.block_id.");
			return Result;
		}

		UEdGraphNode* OwnedNode = FindOwnedNodeByBlockId(Graph, BlockId);
		if (!OwnedNode)
		{
			Result.Code = TEXT("inserted_logic_not_found");
			Result.Message = FString::Printf(
				TEXT("inserted_logic_not_found: BlueprintHelper-owned custom event block '%s' was not found in the target graph."),
				*BlockId);
			return Result;
		}

		Result.EventNode = FindOwnedCustomEventEntryNode(Graph, BlockId);
		if (!Result.EventNode)
		{
			Result.Code = TEXT("inserted_logic_not_callable");
			Result.Message = FString::Printf(
				TEXT("inserted_logic_not_callable: owned block '%s' is not a CustomEvent entry."),
				*BlockId);
			return Result;
		}

		Result.EventName = GetOwnedCustomEventCallableName(Result.EventNode);
		if (Result.EventName.IsEmpty())
		{
			Result.Code = TEXT("inserted_logic_not_callable");
			Result.Message = FString::Printf(
				TEXT("inserted_logic_not_callable: owned block '%s' has no callable CustomEvent name."),
				*BlockId);
			return Result;
		}

		Result.Function = FindOwnedCustomEventFunction(Blueprint, Result.EventName);
		if (!Result.Function)
		{
			Result.Code = TEXT("inserted_logic_requires_compile");
			Result.Message = FString::Printf(
				TEXT("inserted_logic_requires_compile: owned block '%s' CustomEvent '%s' exists but is not compiled into a callable UFunction."),
				*BlockId,
				*Result.EventName);
			return Result;
		}

		Result.bOk = true;
		return Result;
	}

};

FBlueprintHelperToolResultBase FBlueprintHelperMergeBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FMergeRequest Request = ParseRequest(Payload);
	if (Request.bDryRun) return ExecuteDryRun(Request);
	return ExecuteWrite(Request);
}

// ─── 解析 ───

FBlueprintHelperMergeBlueprintGraphService::FMergeRequest
FBlueprintHelperMergeBlueprintGraphService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FMergeRequest Req;
	if (!Payload.IsValid()) return Req;

	const TSharedPtr<FJsonObject>* Tgt = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), Tgt) && Tgt->IsValid())
	{
		(*Tgt)->TryGetStringField(TEXT("asset_path"), Req.AssetPath);
		(*Tgt)->TryGetStringField(TEXT("graph"), Req.GraphName);
		FString S; if ((*Tgt)->TryGetStringField(TEXT("merge_scope"), S)) ParseMergeScope(S, Req.MergeScope);
		if ((*Tgt)->TryGetStringField(TEXT("insert_strategy"), S)) ParseInsertStrategy(S, Req.InsertStrategy);
	}

	const TSharedPtr<FJsonObject>* Anchor = nullptr;
	if (Payload->TryGetObjectField(TEXT("anchor"), Anchor) && Anchor->IsValid())
	{
		(*Anchor)->TryGetStringField(TEXT("block_id"), Req.AnchorBlockId);
		(*Anchor)->TryGetStringField(TEXT("group_entry_node_path"), Req.AnchorGroupEntryNodePath);
		(*Anchor)->TryGetStringField(TEXT("node_ref"), Req.AnchorNodeRef);
		(*Anchor)->TryGetStringField(TEXT("pin_ref"), Req.AnchorPinRef);
		(*Anchor)->TryGetStringField(TEXT("node_path"), Req.AnchorNodePath);
		(*Anchor)->TryGetStringField(TEXT("pin_path"), Req.AnchorPinPath);
	}

	const TSharedPtr<FJsonObject>* Inserted = nullptr;
	if (Payload->TryGetObjectField(TEXT("inserted"), Inserted) && Inserted->IsValid())
	{
		(*Inserted)->TryGetStringField(TEXT("block_id"), Req.InsertedBlockId);
		(*Inserted)->TryGetStringField(TEXT("block_ref"), Req.InsertedBlockRef);
		(*Inserted)->TryGetStringField(TEXT("function"), Req.InsertedFunctionName);
		(*Inserted)->TryGetStringField(TEXT("custom_event"), Req.InsertedCustomEventName);
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject && LogicSpecObject->IsValid())
	{
		Req.LogicSpec = *LogicSpecObject;
	}

	const TArray<TSharedPtr<FJsonValue>>* SeqOrder = nullptr;
	if (Payload->TryGetArrayField(TEXT("sequence_order"), SeqOrder))
		for (const auto& V : *SeqOrder)
		{ FString Item; if (V->TryGetString(Item)) Req.SequenceOrder.Add(Item); }

	Payload->TryGetBoolField(TEXT("allow_compile_before_call"), Req.bAllowCompileBeforeCall);
	Payload->TryGetBoolField(TEXT("dry_run"), Req.bDryRun);
	return Req;
}

// ─── Preflight ───

FBlueprintHelperMergeBlueprintGraphService::FMergePreflightResult
FBlueprintHelperMergeBlueprintGraphService::Preflight(
	const FMergeRequest& Request,
	FMergeContext& Context,
	bool bAllowInsertedLogicRequiresCompile) const
{
	FMergePreflightResult Result;

	if (!PreflightLogicSpec(Request, Context.Blueprint, Result))
	{
		return Result;
	}

	// 1. unsupported scopes
	if (Request.MergeScope == EBlueprintHelperMergeScope::InlineNodes ||
		Request.MergeScope == EBlueprintHelperMergeScope::EventEntryLogic)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("unsupported_merge_scope"));
		Result.Conflicts.Add({TEXT("unsupported_merge_scope"),
			FString::Printf(TEXT("merge_scope '%s' 在第一版中暂不支持。"), MergeScopeToString(Request.MergeScope)),
			TEXT("target.merge_scope"), TEXT("payload")});
		return Result;
	}

	// 2. anchor resolve
	FString AnchorError;
	if (!ResolveAnchor(Request, Context, AnchorError))
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(AnchorError.Contains(TEXT("exec")) ? TEXT("anchor_pin_not_exec")
			: AnchorError.Contains(TEXT("node")) ? TEXT("anchor_node_not_found") : TEXT("anchor_pin_not_found"));
		Result.Conflicts.Add({Result.BlockedBy.Last(), AnchorError, TEXT("anchor"), TEXT("payload")});
		return Result;
	}

	// 3. successor check
	if (!CheckSuccessorCount(Request, Context, Result))
		return Result;

	Context.OriginalSuccessorPin = Context.Successors.Num() > 0 ? Context.Successors[0] : nullptr;

	// 4. sequence_order check
	if (Request.InsertStrategy == EBlueprintHelperInsertStrategy::BranchFork)
	{
		if (Request.SequenceOrder.Num() == 0)
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("sequence_order_required"));
			Result.Conflicts.Add({TEXT("sequence_order_required"),
				TEXT("branch_fork 需要显式指定 sequence_order。"), TEXT("sequence_order"), TEXT("payload")});
			return Result;
		}
		bool bHasOrig = false, bHasInserted = false;
		for (const FString& Item : Request.SequenceOrder)
		{
			if (Item == TEXT("original_successor")) bHasOrig = true;
			else if (Item == TEXT("inserted_logic")) bHasInserted = true;
			else { Result.bPassed = false; Result.BlockedBy.Add(TEXT("sequence_order_invalid")); return Result; }
		}
		if (!bHasOrig && Context.OriginalSuccessorPin) { Result.bPassed = false; Result.BlockedBy.Add(TEXT("sequence_order_invalid")); }
		if (bHasOrig && !Context.OriginalSuccessorPin) { Result.bPassed = false; Result.BlockedBy.Add(TEXT("sequence_order_invalid")); }
		if (!bHasInserted) { Result.bPassed = false; Result.BlockedBy.Add(TEXT("sequence_order_invalid")); }
		if (!Result.bPassed)
		{
			Result.Errors.Add({TEXT("sequence_order_invalid"),
				TEXT("branch_fork sequence_order must match inserted_logic and the available original_successor."),
				TEXT("sequence_order"), TEXT("payload")});
			return Result;
		}
	}

	if (Request.InsertStrategy == EBlueprintHelperInsertStrategy::BranchFork &&
		Request.MergeScope == EBlueprintHelperMergeScope::OwnedBlockCall)
	{
		const FString InsertedRef = Request.InsertedBlockId.IsEmpty()
			? Request.InsertedBlockRef
			: Request.InsertedBlockId;
		const FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::FOwnedBlockCallableCheck InsertedCheck = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::CheckOwnedBlockCallable(Context.Blueprint, Context.Graph, InsertedRef);
		if (!InsertedCheck.bOk &&
			!(bAllowInsertedLogicRequiresCompile && InsertedCheck.Code == TEXT("inserted_logic_requires_compile")))
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(InsertedCheck.Code.IsEmpty() ? TEXT("inserted_logic_not_found") : InsertedCheck.Code);
			Result.Errors.Add({Result.BlockedBy.Last(),
				InsertedCheck.Message.IsEmpty() ? TEXT("Inserted owned block call preflight failed.") : InsertedCheck.Message,
				TEXT("inserted.block_id"), TEXT("payload")});
			return Result;
		}
	}

	if (Request.MergeScope == EBlueprintHelperMergeScope::FunctionCall)
	{
		if (Request.InsertedFunctionName.IsEmpty())
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("inserted_logic_not_found"));
			Result.Errors.Add({TEXT("inserted_logic_not_found"),
				TEXT("inserted_logic_not_found: missing inserted.function."),
				TEXT("inserted.function"), TEXT("payload")});
			return Result;
		}

		FString ResolveCode;
		FString ResolveMessage;
		if (!FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::ResolveMergeCallableFunction(
			Context.Blueprint,
			Context.Graph,
			Request.InsertedFunctionName,
			&ResolveCode,
			&ResolveMessage))
		{
			const FString ResolverMessage = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::FormatResolverFailure(
				ResolveCode,
				ResolveMessage,
				Request.InsertedFunctionName);
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("inserted_logic_not_found"));
			Result.Errors.Add({TEXT("inserted_logic_not_found"),
				FString::Printf(TEXT("inserted_logic_not_found: call_function resolve failed: %s"), *ResolverMessage),
				TEXT("inserted.function"), TEXT("payload")});
			return Result;
		}
	}

	return Result;
}

// ─── Anchor 解析 ───

bool FBlueprintHelperMergeBlueprintGraphService::PreflightLogicSpec(
	const FMergeRequest& Request,
	UBlueprint* Blueprint,
	FMergePreflightResult& OutResult) const
{
	if (!Request.LogicSpec.IsValid())
	{
		return true;
	}

	OutResult.FragmentDebugData = FBlueprintHelperGraphFragmentDebugData::BuildFromLogicSpec(Request.LogicSpec, Blueprint);

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Request.LogicSpec, Blueprint, SemanticIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : SemanticIR.Diagnostics)
	{
		if (Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(Diagnostic.Code);
			OutResult.Errors.Add({Diagnostic.Code, Diagnostic.Message, Diagnostic.Path, TEXT("logic_spec")});
		}
	}
	return OutResult.bPassed;
}

bool FBlueprintHelperMergeBlueprintGraphService::ResolveAnchor(
	const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const
{
	// 定位 node
	FBlueprintHelperGraphWriteAnchorRef Anchor;
	Anchor.BlockId = Request.AnchorBlockId;
	Anchor.GroupEntryNodePath = Request.AnchorGroupEntryNodePath;
	Anchor.NodeRef = Request.AnchorNodeRef;
	Anchor.PinRef = Request.AnchorPinRef;
	Anchor.NodePath = Request.AnchorNodePath;
	Anchor.PinPath = Request.AnchorPinPath;

	FBlueprintHelperPatchResolveError NodeErr;
	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(PathService, Context.Graph, Anchor, Context.AnchorNode, NodeErr))
	{
		OutError = NodeErr.Message;
		return false;
	}

	// 定位 pin
	FBlueprintHelperPatchResolveError PinErr;
	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolvePin(PathService, Context.Graph, Context.AnchorNode, Anchor, Context.AnchorPin, PinErr))
	{
		OutError = PinErr.Message;
		return false;
	}

	// 必须。Exec output
	if (!Context.AnchorPin)
	{
		OutError = TEXT("anchor_pin_not_found：目。Pin 不存在。");
		return false;
	}
	if (Context.AnchorPin->Direction != EGPD_Output)
	{
		OutError = TEXT("anchor_pin_not_exec：Anchor Pin 必须是输出 Pin。");
		return false;
	}
	if (Context.AnchorPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
	{
		OutError = TEXT("anchor_pin_not_exec：Anchor Pin 必须。Exec 类型。");
		return false;
	}

	// 收集后继
	for (UEdGraphPin* Linked : Context.AnchorPin->LinkedTo)
	{
		if (Linked && Linked->Direction == EGPD_Input)
			Context.Successors.Add(Linked);
	}

	return true;
}

// ─── Successor 校验 ───

bool FBlueprintHelperMergeBlueprintGraphService::CheckSuccessorCount(
	const FMergeRequest& Request, const FMergeContext& Context, FMergePreflightResult& OutResult) const
{
	switch (Request.InsertStrategy)
	{
	case EBlueprintHelperInsertStrategy::AppendAfter:
		if (Context.Successors.Num() > 0)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("anchor_exec_pin_already_connected"));
			OutResult.Conflicts.Add({TEXT("anchor_exec_pin_already_connected"),
				TEXT("append_after 要求 Anchor Pin 没有后继。"), TEXT("anchor"), TEXT("payload")});
		}
		break;
	case EBlueprintHelperInsertStrategy::InsertBetween:
		if (Context.Successors.Num() == 0)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("original_successor_not_found"));
			OutResult.Conflicts.Add({TEXT("original_successor_not_found"),
				TEXT("insert_between 要求 Anchor Pin 有且仅有一个后继。"), TEXT("anchor"), TEXT("payload")});
		}
		else if (Context.Successors.Num() > 1)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("anchor_exec_pin_has_multiple_successors"));
			OutResult.Conflicts.Add({TEXT("anchor_exec_pin_has_multiple_successors"),
				TEXT("insert_between 要求 Anchor Pin 只有单一后继。"), TEXT("anchor"), TEXT("payload")});
		}
		break;
	case EBlueprintHelperInsertStrategy::BranchFork:
		if (Context.Successors.Num() > 1)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("anchor_exec_pin_has_multiple_successors"));
			OutResult.Conflicts.Add({TEXT("anchor_exec_pin_has_multiple_successors"),
				TEXT("branch_fork 要求 Anchor Pin 不超出一个后继。"), TEXT("anchor"), TEXT("payload")});
		}
		break;
	}
	return OutResult.bPassed;
}

// ─── DryRun ───

FBlueprintHelperToolResultBase FBlueprintHelperMergeBlueprintGraphService::ExecuteDryRun(
	const FMergeRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	// 解析蓝图/图表
	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Tgt, Diag);
	UEdGraph* Graph = nullptr;
	if (BP)
	{
		for (UEdGraph* P : BP->UbergraphPages) if (P && P->GetName() == Request.GraphName) { Graph = P; break; }
		if (!Graph) for (UEdGraph* F : BP->FunctionGraphs) if (F && F->GetName() == Request.GraphName) { Graph = F; break; }
	}

	FMergeContext Context;
	Context.Blueprint = BP;
	Context.Graph = Graph;

	FMergePreflightResult Pre;
	if (BP && Graph) Pre = Preflight(Request, Context, false);
	else { Pre.bPassed = false; Pre.BlockedBy.Add(BP ? TEXT("target_graph_not_found") : TEXT("target_blueprint_not_found")); }

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(TEXT("merge_blueprint_graph"), TraceId);

	FBlueprintHelperMergeTargetRef MTarget;
	MTarget.AssetPath = Request.AssetPath;
	MTarget.Graph = Request.GraphName;
	MTarget.MergeScope = MergeScopeToString(Request.MergeScope);
	MTarget.InsertStrategy = InsertStrategyToString(Request.InsertStrategy);
	Result.CustomTargetJson = MTarget.ToJson();

	if (Pre.bPassed)
	{
		FBlueprintHelperMergeDryRunData Data;
		Data.DryRun.Result = TEXT("passed");
		Data.DryRun.bCanExecute = true;
		Result.Data = Data.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, Pre.FragmentDebugData);
	}
	else
	{
		FBlueprintHelperMergeDryRunData Data;
		Data.DryRun.Result = TEXT("blocked");
		Data.DryRun.bCanExecute = false;
		Data.DryRun.BlockedBy = Pre.BlockedBy;
		for (const auto& C : Pre.Conflicts) Data.DryRun.Conflicts.Add(C);
		for (const auto& E : Pre.Errors) Data.DryRun.Errors.Add(E);

		const FBlueprintHelperDryRunIssue* FirstIssue = Pre.Conflicts.Num() > 0
			? &Pre.Conflicts[0]
			: (Pre.Errors.Num() > 0 ? &Pre.Errors[0] : nullptr);

		FBlueprintHelperToolError Error;
		Error.Code = Pre.BlockedBy.Num() > 0 ? Pre.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Merge dry-run preflight blocked execution.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: (Error.Code == TEXT("target_blueprint_not_found") ? TEXT("target.asset_path") : TEXT("target.graph"));
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

		Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("merge_blueprint_graph"), TraceId, Error);
		Result.CustomTargetJson = MTarget.ToJson();
		Result.Data = Data.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, Pre.FragmentDebugData);
	}
	return Result;
}

// ─── 正式写入 ───

FBlueprintHelperToolResultBase FBlueprintHelperMergeBlueprintGraphService::ExecuteWrite(
	const FMergeRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FString TxId = JournalService.GenerateTransactionId();

	// 1-2. Resolve BP/Graph
	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Tgt, Diag);
	if (!BP) return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
		{TEXT("target_blueprint_not_found"), EBlueprintHelperToolStage::ResolveTarget, TEXT("蓝图未找到。"), false, EBlueprintHelperRollbackResult::NotNeeded});

	UEdGraph* Graph = nullptr;
	for (UEdGraph* P : BP->UbergraphPages) if (P && P->GetName() == Request.GraphName) { Graph = P; break; }
	if (!Graph) for (UEdGraph* F : BP->FunctionGraphs) if (F && F->GetName() == Request.GraphName) { Graph = F; break; }
	if (!Graph) return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
		{TEXT("target_graph_not_found"), EBlueprintHelperToolStage::ResolveTarget,
		 FString::Printf(TEXT("图表 %s 未找到。"), *Request.GraphName), false, EBlueprintHelperRollbackResult::NotNeeded});

	// 3. Context
	FMergeContext Context;
	Context.Blueprint = BP;
	Context.Graph = Graph;

	// 4. Preflight
	FMergePreflightResult Pre = Preflight(Request, Context, Request.bAllowCompileBeforeCall);
	if (!Pre.bPassed)
	{
		const FBlueprintHelperDryRunIssue* FirstIssue = Pre.Conflicts.Num() > 0
			? &Pre.Conflicts[0]
			: (Pre.Errors.Num() > 0 ? &Pre.Errors[0] : nullptr);

		FBlueprintHelperToolError Err;
		Err.Code = Pre.BlockedBy.Num() > 0 ? Pre.BlockedBy[0] : TEXT("preflight_failed");
		Err.Stage = EBlueprintHelperToolStage::Preflight;
		Err.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Preflight failed.");
		Err.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: TEXT("target.graph");
		Err.bRetryable = false;
		Err.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId, Err);
		FBlueprintHelperGraphFragmentDebugData::AttachToData(FailResult.Data, Pre.FragmentDebugData);
		return FailResult;
	}

	// 5. Scoped mutation + resolve inserted logic
	FBlueprintHelperScopedAssetMutation Mutation(FText::FromString(TEXT("BlueprintHelper Merge Graph")), BP);
	Mutation.Modify(Graph);

	FString InsertedErrorCode;
	FString InsertedError;
	if (!ResolveInsertedLogic(Request, Context, InsertedErrorCode, InsertedError))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
			{InsertedErrorCode.IsEmpty() ? TEXT("inserted_logic_not_found") : InsertedErrorCode, EBlueprintHelperToolStage::ResolveTarget,
			 InsertedError, false, EBlueprintHelperRollbackResult::RolledBack});
	}

	// 6. Apply links
	FString ApplyError;
	bool bSucceeded = false;
	switch (Request.InsertStrategy)
	{
	case EBlueprintHelperInsertStrategy::AppendAfter:   bSucceeded = ApplyAppendAfter(BP, Graph, Request, Context, ApplyError); break;
	case EBlueprintHelperInsertStrategy::InsertBetween: bSucceeded = ApplyInsertBetween(BP, Graph, Request, Context, ApplyError); break;
	case EBlueprintHelperInsertStrategy::BranchFork:    bSucceeded = ApplyBranchFork(BP, Graph, Request, Context, ApplyError); break;
	}

	if (!bSucceeded)
	{
		Mutation.Rollback();
		if (ApplyError.IsEmpty())
		{
			ApplyError = TEXT("merge_apply_failed");
		}
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
			{TEXT("link_create_failed"), EBlueprintHelperToolStage::Execute,
			 ApplyError, false, EBlueprintHelperRollbackResult::RolledBack});
	}

	// 7. Journal
	FBlueprintHelperAppendJournalRecord JRec;
	JRec.TransactionId = TxId;
	JRec.Tool = TEXT("MergeBlueprintGraph");
	JRec.Status = TEXT("applied");
	JRec.TargetAssets.Add(Request.AssetPath);
	JRec.GraphId = Request.GraphName;
	JRec.GraphName = Request.GraphName;

	FString JErr;
	if (!JournalService.WriteAppendJournal(JRec, JErr))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
			{TEXT("journal_write_failed"), EBlueprintHelperToolStage::Execute,
			 JErr, false, EBlueprintHelperRollbackResult::RolledBack});
	}

	// 8. Mark + commit
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	if (BP->GetOutermost()) BP->GetOutermost()->MarkPackageDirty();
	Mutation.Commit();

	// 9. Success result
	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Applied(TEXT("merge_blueprint_graph"), TraceId);

	FBlueprintHelperMergeTargetRef MTarget;
	MTarget.AssetPath = Request.AssetPath;
	MTarget.Graph = Request.GraphName;
	MTarget.MergeScope = MergeScopeToString(Request.MergeScope);
	MTarget.InsertStrategy = InsertStrategyToString(Request.InsertStrategy);
	Success.CustomTargetJson = MTarget.ToJson();

	FBlueprintHelperMergeGraphResultData Data;
	Data.MergeResult.MergedRef.GraphId = Request.GraphName;
	Data.MergeResult.MergedRef.AnchorRef = FString::Printf(TEXT("%s.%s"),
		Context.AnchorNode ? *Context.AnchorNode->GetName() : TEXT("?"),
		Context.AnchorPin ? *Context.AnchorPin->PinName.ToString() : TEXT("?"));
	Data.MergeResult.MergedRef.InsertedRef = Context.InsertedRef;
	if (Context.SequenceNode) Data.MergeResult.MergedRef.SequenceRef = Context.SequenceNode->GetName();
	Data.WriteRef.TransactionId = TxId;
	Data.WriteRef.bJournalRecorded = true;
	Success.Data = Data.ToJson();
	FBlueprintHelperGraphFragmentDebugData::AttachToData(Success.Data, Pre.FragmentDebugData);

	FBlueprintHelperValidationSummary Val;
	Val.bShouldCompile = true;
	Val.bShouldSave = true;
	Success.Validation = Val;

	return Success;
}

// ─── Inserted Logic 解析 ───

bool FBlueprintHelperMergeBlueprintGraphService::ResolveInsertedLogic(
	const FMergeRequest& Request, FMergeContext& Context, FString& OutErrorCode, FString& OutError) const
{
	OutErrorCode.Reset();
	OutError.Reset();

	switch (Request.MergeScope)
	{
	case EBlueprintHelperMergeScope::OwnedBlockCall:
	{
		Context.InsertedRef = Request.InsertedBlockId.IsEmpty() ? Request.InsertedBlockRef : Request.InsertedBlockId;
		{
			FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::FOwnedBlockCallableCheck InsertedCheck = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::CheckOwnedBlockCallable(Context.Blueprint, Context.Graph, Context.InsertedRef);
			if (!InsertedCheck.bOk && InsertedCheck.Code == TEXT("inserted_logic_requires_compile") && Request.bAllowCompileBeforeCall)
			{
				if (Context.Blueprint)
				{
					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
					FKismetEditorUtilities::CompileBlueprint(Context.Blueprint);
					InsertedCheck = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::CheckOwnedBlockCallable(Context.Blueprint, Context.Graph, Context.InsertedRef);
				}
			}
			if (!InsertedCheck.bOk)
			{
				OutErrorCode = InsertedCheck.Code.IsEmpty() ? TEXT("inserted_logic_not_found") : InsertedCheck.Code;
				OutError = InsertedCheck.Message.IsEmpty()
					? TEXT("inserted_logic_not_found: owned block call could not be resolved.")
					: InsertedCheck.Message;
				return false;
			}

			UK2Node_CallFunction* CallNode = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::CreateMergeCallFunctionNode(Context.Graph, InsertedCheck.Function);
			if (!CallNode)
			{
				OutErrorCode = TEXT("inserted_logic_not_callable");
				OutError = FString::Printf(
					TEXT("inserted_logic_not_callable: unable to create owned block call '%s'."),
					*InsertedCheck.EventName);
				return false;
			}

			Context.InsertedNode = CallNode;
			FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::MarkMergeNodeAsBlueprintHelperOwned(Context.InsertedNode, Request.AnchorBlockId);
			return true;
		}
	}
	case EBlueprintHelperMergeScope::CustomEventCall:
	{
		if (Request.InsertedCustomEventName.IsEmpty()) { OutError = TEXT("inserted_logic_not_found: 缺少 custom_event 名。"); return false; }
		Context.InsertedRef = Request.InsertedCustomEventName;

		FString ResolveCode;
		FString ResolveMessage;
		FBlueprintHelperCallFunctionCandidate ResolveCandidate;
		UFunction* TargetFunc = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::ResolveMergeCallableFunction(
			Context.Blueprint,
			Context.Graph,
			Request.InsertedCustomEventName,
			&ResolveCode,
			&ResolveMessage,
			&ResolveCandidate);
		if (!TargetFunc)
		{
			const FString ResolverMessage = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::FormatResolverFailure(
				ResolveCode,
				ResolveMessage,
				Request.InsertedCustomEventName);
			OutError = FString::Printf(TEXT("inserted_logic_not_found: call_function resolve failed: %s"), *ResolverMessage);
			return false;
		}

		UK2Node_CallFunction* CallNode = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::CreateMergeCallFunctionNode(Context.Graph, ResolveCandidate);
		if (!CallNode)
		{
			OutError = FString::Printf(TEXT("inserted_logic_not_found: unable to create custom_event call '%s'."), *Request.InsertedCustomEventName);
			return false;
		}

		Context.InsertedNode = CallNode;
		FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::MarkMergeNodeAsBlueprintHelperOwned(Context.InsertedNode, Request.AnchorBlockId);
		return true;
	}

	case EBlueprintHelperMergeScope::FunctionCall:
	{
		if (Request.InsertedFunctionName.IsEmpty()) { OutError = TEXT("inserted_logic_not_found: 缺少 function 名。"); return false; }
		Context.InsertedRef = Request.InsertedFunctionName;

		FString ResolveCode;
		FString ResolveMessage;
		FBlueprintHelperCallFunctionCandidate ResolveCandidate;
		UFunction* TargetFunc = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::ResolveMergeCallableFunction(
			Context.Blueprint,
			Context.Graph,
			Request.InsertedFunctionName,
			&ResolveCode,
			&ResolveMessage,
			&ResolveCandidate);
		if (!TargetFunc)
		{
			const FString ResolverMessage = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::FormatResolverFailure(
				ResolveCode,
				ResolveMessage,
				Request.InsertedFunctionName);
			OutError = FString::Printf(TEXT("inserted_logic_not_found: call_function resolve failed: %s"), *ResolverMessage);
			return false;
		}

		UK2Node_CallFunction* CallNode = FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::CreateMergeCallFunctionNode(Context.Graph, ResolveCandidate);
		if (!CallNode)
		{
			OutError = FString::Printf(TEXT("inserted_logic_not_found: unable to create function call '%s'."), *Request.InsertedFunctionName);
			return false;
		}

		Context.InsertedNode = CallNode;
		FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::MarkMergeNodeAsBlueprintHelperOwned(Context.InsertedNode, Request.AnchorBlockId);
		return true;
	}

	default:
		OutError = TEXT("unsupported_merge_scope");
		return false;
	}
}

// ─── append_after ───

bool FBlueprintHelperMergeBlueprintGraphService::ApplyAppendAfter(
	UBlueprint* BP, UEdGraph* Graph, const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const
{
	// 确保。inserted node
	if (!Context.InsertedNode)
	{
		// 。owned_block_call / custom_event_call 创建简单的占位节点
		// 第一版简化：仅支。function_call (已有 node) 和已存在。inserted node
		if (Request.MergeScope == EBlueprintHelperMergeScope::FunctionCall && Context.InsertedNode)
		{
			// already created in ResolveInsertedLogic
		}
		else
		{
			OutError = TEXT("inserted_logic_not_found: 。merge_scope 的节点创建尚未实现。");
			return false;
		}
	}

	// 查找 inserted node 。exec input
	UEdGraphPin* InsertedExecIn = nullptr;
	for (UEdGraphPin* Pin : Context.InsertedNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			InsertedExecIn = Pin;
			break;
		}
	}
	if (!InsertedExecIn) { OutError = TEXT("inserted_logic_has_no_exec_pins"); return false; }

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	FPinConnectionResponse Resp = Schema->CanCreateConnection(Context.AnchorPin, InsertedExecIn);
	if (Resp.Response == CONNECT_RESPONSE_DISALLOW)
	{
		OutError = FString::Printf(TEXT("pin_type_mismatch: %s"), *Resp.Message.ToString());
		return false;
	}

	Context.AnchorPin->Modify();
	InsertedExecIn->Modify();
	if (!Schema->TryCreateConnection(Context.AnchorPin, InsertedExecIn))
	{
		OutError = TEXT("link_create_failed");
		return false;
	}

	Graph->NotifyGraphChanged();
	return true;
}

// ─── insert_between ───

bool FBlueprintHelperMergeBlueprintGraphService::ApplyInsertBetween(
	UBlueprint* BP, UEdGraph* Graph, const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const
{
	UEdGraphPin* OrigSucc = Context.OriginalSuccessorPin;
	if (!OrigSucc) { OutError = TEXT("original_successor_not_found"); return false; }

	// Ensure inserted node
	if (!Context.InsertedNode)
	{
		if (Request.MergeScope == EBlueprintHelperMergeScope::FunctionCall && Context.InsertedNode)
		{
			// already created
		}
		else
		{
			OutError = TEXT("inserted_logic_not_found");
			return false;
		}
	}

	UEdGraphPin* InsExecIn = nullptr;
	UEdGraphPin* InsExecOut = nullptr;
	for (UEdGraphPin* Pin : Context.InsertedNode->Pins)
	{
		if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			if (Pin->Direction == EGPD_Input && !InsExecIn) InsExecIn = Pin;
			if (Pin->Direction == EGPD_Output && !InsExecOut) InsExecOut = Pin;
		}
	}
	if (!InsExecIn || !InsExecOut) { OutError = TEXT("inserted_logic_has_no_exec_pins"); return false; }

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Check both connections
	if (Schema->CanCreateConnection(Context.AnchorPin, InsExecIn).Response == CONNECT_RESPONSE_DISALLOW)
	{
		OutError = TEXT("pin_type_mismatch: anchor -> inserted");
		return false;
	}
	if (Schema->CanCreateConnection(InsExecOut, OrigSucc).Response == CONNECT_RESPONSE_DISALLOW)
	{
		OutError = TEXT("pin_type_mismatch: inserted -> original_successor");
		return false;
	}

	// Break old link
	Context.AnchorPin->Modify();
	OrigSucc->Modify();
	Context.AnchorPin->BreakLinkTo(OrigSucc);

	// Create new links
	if (!Schema->TryCreateConnection(Context.AnchorPin, InsExecIn))
	{
		// Restore old link
		Schema->TryCreateConnection(Context.AnchorPin, OrigSucc);
		OutError = TEXT("link_create_failed: anchor -> inserted");
		return false;
	}
	if (!Schema->TryCreateConnection(InsExecOut, OrigSucc))
	{
		// Restore
		Context.AnchorPin->BreakLinkTo(InsExecIn);
		Schema->TryCreateConnection(Context.AnchorPin, OrigSucc);
		OutError = TEXT("link_create_failed: inserted -> original_successor");
		return false;
	}

	Graph->NotifyGraphChanged();
	return true;
}

// ─── branch_fork ───

bool FBlueprintHelperMergeBlueprintGraphService::ApplyBranchFork(
	UBlueprint* BP, UEdGraph* Graph, const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const
{
	// Ensure inserted node
	if (!Context.InsertedNode)
	{
		if (Request.MergeScope == EBlueprintHelperMergeScope::FunctionCall && Context.InsertedNode)
		{
			// already created
		}
		else
		{
			OutError = TEXT("inserted_logic_not_found");
			return false;
		}
	}

	// Create Sequence node through the graph statement builder so GraphWrite node creation remains inside the SemanticIR fragment path.
	FBlueprintHelperNodeFragment SequenceFragment;
	FString SequenceError;
	if (!FBlueprintHelperGraphStatementBuilder::BuildSequenceFragment(
		Graph,
		Request.AnchorBlockId + TEXT("_merge_sequence"),
		SequenceFragment,
		SequenceError))
	{
		OutError = SequenceError.IsEmpty() ? TEXT("sequence_node_create_failed") : SequenceError;
		return false;
	}

	UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(SequenceFragment.PrimaryNode);
	if (!SeqNode)
	{
		OutError = TEXT("sequence_node_create_failed");
		return false;
	}
	FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::MarkMergeNodeAsBlueprintHelperOwned(SeqNode, Request.AnchorBlockId);
	Context.SequenceNode = SeqNode;

	// Find Sequence Exec In
	UEdGraphPin* SeqExecIn = nullptr;
	for (UEdGraphPin* Pin : SeqNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{ SeqExecIn = Pin; break; }
	}
	if (!SeqExecIn) { OutError = TEXT("Sequence 节点。Exec 输入。"); return false; }

	// Collect Sequence Then pins
	TArray<UEdGraphPin*> SeqThenPins;
	auto CollectSequenceThenPins = [&SeqNode, &SeqThenPins]()
	{
		SeqThenPins.Reset();
		for (UEdGraphPin* Pin : SeqNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				SeqThenPins.Add(Pin);
			}
		}
	};
	CollectSequenceThenPins();
	while (SeqThenPins.Num() < 2)
	{
		SeqNode->AddInputPin();
		CollectSequenceThenPins();
	}
	if (SeqThenPins.Num() < 2) { OutError = TEXT("Sequence 节点需要至。2 。Then 输出。"); return false; }

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Find inserted exec in
	UEdGraphPin* InsExecIn = nullptr;
	for (UEdGraphPin* Pin : Context.InsertedNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{ InsExecIn = Pin; break; }
	}
	if (!InsExecIn) { OutError = TEXT("inserted_logic_has_no_exec_pins"); return false; }

	UEdGraphPin* OrigSucc = Context.OriginalSuccessorPin;

	// Break original
	Context.AnchorPin->Modify();
	if (OrigSucc) { OrigSucc->Modify(); Context.AnchorPin->BreakLinkTo(OrigSucc); }

	// Connect anchor -> sequence
	if (!Schema->TryCreateConnection(Context.AnchorPin, SeqExecIn))
	{
		if (OrigSucc) Schema->TryCreateConnection(Context.AnchorPin, OrigSucc);
		OutError = TEXT("link_create_failed: anchor -> sequence");
		return false;
	}

	// Apply sequence_order
	bool bOrigFirst = Request.SequenceOrder.Num() > 0 && Request.SequenceOrder[0] == TEXT("original_successor");
	UEdGraphPin* Then0 = SeqThenPins[0];
	UEdGraphPin* Then1 = SeqThenPins[1];

	if (bOrigFirst)
	{
		if (OrigSucc && !Schema->TryCreateConnection(Then0, OrigSucc))
		{
			OutError = TEXT("link_create_failed: sequence.Then0 -> original_successor");
			return false;
		}
		if (!Schema->TryCreateConnection(Then1, InsExecIn))
		{
			OutError = TEXT("link_create_failed: sequence.Then1 -> inserted");
			return false;
		}
	}
	else
	{
		if (!Schema->TryCreateConnection(Then0, InsExecIn))
		{
			OutError = TEXT("link_create_failed: sequence.Then0 -> inserted");
			return false;
		}
		if (OrigSucc && !Schema->TryCreateConnection(Then1, OrigSucc))
		{
			OutError = TEXT("link_create_failed: sequence.Then1 -> original_successor");
			return false;
		}
	}

	Graph->NotifyGraphChanged();
	return true;
}
