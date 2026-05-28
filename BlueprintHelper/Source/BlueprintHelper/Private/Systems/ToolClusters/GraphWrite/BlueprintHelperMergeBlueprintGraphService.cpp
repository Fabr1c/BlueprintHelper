// BlueprintHelper Service Layer - MergeBlueprintGraph implementation

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeCallableFragmentService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
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
#include "Systems/ToolClusters/GraphWrite/Utils/GraphWriteCoreUtils.h"

FBlueprintHelperMergeBlueprintGraphService::FBlueprintHelperMergeBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperLogicJsonPathService& InPathService)
	: Resolver(InResolver), PathService(InPathService)
{
}

// 鈹€鈹€鈹€ 鍏叡鍏ュ彛 鈹€鈹€鈹€

class FBlueprintHelperMergeBlueprintGraphServiceLocalUtils
{
public:
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

// 鈹€鈹€鈹€ 瑙ｆ瀽 鈹€鈹€鈹€

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

// 鈹€鈹€鈹€ Preflight 鈹€鈹€鈹€

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
			FString::Printf(TEXT("merge_scope '%s' is not supported."), MergeScopeToString(Request.MergeScope)),
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
				TEXT("branch_fork requires explicit sequence_order."), TEXT("sequence_order"), TEXT("payload")});
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
				TEXT("branch_fork requires explicit sequence_order."), TEXT("sequence_order"), TEXT("payload")});
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

	if (Request.MergeScope == EBlueprintHelperMergeScope::FunctionCall ||
		Request.MergeScope == EBlueprintHelperMergeScope::CustomEventCall)
	{
		const bool bIsFunctionCall = Request.MergeScope == EBlueprintHelperMergeScope::FunctionCall;
		const FString CallableQuery = bIsFunctionCall
			? Request.InsertedFunctionName
			: Request.InsertedCustomEventName;
		const FString SourceField = bIsFunctionCall
			? TEXT("inserted.function")
			: TEXT("inserted.custom_event");
		const FString MissingFieldMessage = bIsFunctionCall
			? TEXT("inserted_logic_not_found: missing inserted.function.")
			: TEXT("inserted_logic_not_found: missing inserted.custom_event.");

		if (CallableQuery.IsEmpty())
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("inserted_logic_not_found"));
			Result.Errors.Add({TEXT("inserted_logic_not_found"),
				MissingFieldMessage,
				SourceField, TEXT("payload")});
			return Result;
		}

		const FBlueprintHelperMergeCallableFragmentResult CallableResult =
			FBlueprintHelperMergeCallableFragmentService::ValidateCallable(
				UGraphWriteCoreUtils::MakeMergeCallableRequest(
					Context.Blueprint,
					Context.Graph,
					CallableQuery,
					CallableQuery));
		if (!CallableResult.bOk)
		{
			const FString ResolverMessage = UGraphWriteCoreUtils::FormatMergeCallableFailure(CallableResult, CallableQuery);
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("inserted_logic_not_found"));
			Result.Errors.Add({TEXT("inserted_logic_not_found"),
				FString::Printf(TEXT("inserted_logic_not_found: call_function resolve failed: %s"), *ResolverMessage),
				SourceField, TEXT("payload")});
			return Result;
		}
	}

	return Result;
}

// 鈹€鈹€鈹€ Anchor 瑙ｆ瀽 鈹€鈹€鈹€

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
	// Resolve anchor node.
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

	// Resolve anchor pin.
	FBlueprintHelperPatchResolveError PinErr;
	if (!FBlueprintHelperGraphWriteBlockScopedResolver::ResolvePin(PathService, Context.Graph, Context.AnchorNode, Anchor, Context.AnchorPin, PinErr))
	{
		OutError = PinErr.Message;
		return false;
	}

	// 蹇呴』銆侲xec output
	if (!Context.AnchorPin)
	{
		OutError = TEXT("anchor_pin_not_found: target pin does not exist.");
		return false;
	}
	if (Context.AnchorPin->Direction != EGPD_Output)
	{
		OutError = TEXT("anchor_pin_not_exec: anchor pin must be an exec output pin.");
		return false;
	}
	if (Context.AnchorPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
	{
		OutError = TEXT("anchor_pin_not_exec: anchor pin must be an exec output pin.");
		return false;
	}

	// 鏀堕泦鍚庣户
	for (UEdGraphPin* Linked : Context.AnchorPin->LinkedTo)
	{
		if (Linked && Linked->Direction == EGPD_Input)
			Context.Successors.Add(Linked);
	}

	return true;
}

// 鈹€鈹€鈹€ Successor 鏍￠獙 鈹€鈹€鈹€

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
				TEXT("append_after requires anchor pin to have no successor."), TEXT("anchor"), TEXT("payload")});
		}
		break;
	case EBlueprintHelperInsertStrategy::InsertBetween:
		if (Context.Successors.Num() == 0)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("original_successor_not_found"));
			OutResult.Conflicts.Add({TEXT("original_successor_not_found"),
				TEXT("insert_between requires anchor pin to have exactly one successor."), TEXT("anchor"), TEXT("payload")});
		}
		else if (Context.Successors.Num() > 1)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("anchor_exec_pin_has_multiple_successors"));
			OutResult.Conflicts.Add({TEXT("anchor_exec_pin_has_multiple_successors"),
				TEXT("insert_between requires anchor pin to have exactly one successor."), TEXT("anchor"), TEXT("payload")});
		}
		break;
	case EBlueprintHelperInsertStrategy::BranchFork:
		if (Context.Successors.Num() > 1)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("anchor_exec_pin_has_multiple_successors"));
			OutResult.Conflicts.Add({TEXT("anchor_exec_pin_has_multiple_successors"),
				TEXT("branch_fork requires anchor pin to have at most one successor."), TEXT("anchor"), TEXT("payload")});
		}
		break;
	}
	return OutResult.bPassed;
}

// 鈹€鈹€鈹€ DryRun 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperMergeBlueprintGraphService::ExecuteDryRun(
	const FMergeRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	// 瑙ｆ瀽钃濆浘/鍥捐〃
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
	if (BP && Graph) Pre = Preflight(Request, Context, Request.bAllowCompileBeforeCall);
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

// 鈹€鈹€鈹€ 姝ｅ紡鍐欏叆 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperMergeBlueprintGraphService::ExecuteWrite(
	const FMergeRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	// 1-2. Resolve BP/Graph
	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Tgt, Diag);
	if (!BP) return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
		{TEXT("target_blueprint_not_found"), EBlueprintHelperToolStage::ResolveTarget, TEXT("target blueprint not found."), false, EBlueprintHelperRollbackResult::NotNeeded});

	UEdGraph* Graph = nullptr;
	for (UEdGraph* P : BP->UbergraphPages) if (P && P->GetName() == Request.GraphName) { Graph = P; break; }
	if (!Graph) for (UEdGraph* F : BP->FunctionGraphs) if (F && F->GetName() == Request.GraphName) { Graph = F; break; }
	if (!Graph) return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
		{TEXT("target_graph_not_found"), EBlueprintHelperToolStage::ResolveTarget,
		 FString::Printf(TEXT("graph %s not found."), *Request.GraphName), false, EBlueprintHelperRollbackResult::NotNeeded});

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
	case EBlueprintHelperInsertStrategy::AppendAfter:
	case EBlueprintHelperInsertStrategy::InsertBetween:
	case EBlueprintHelperInsertStrategy::BranchFork:
		bSucceeded = ApplyMergeIntent(BP, Graph, Request, Context, ApplyError);
		break;
	default:
		ApplyError = TEXT("unsupported_insert_strategy");
		break;
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
	Success.Data = Data.ToJson();
	FBlueprintHelperGraphFragmentDebugData::AttachToData(Success.Data, Pre.FragmentDebugData);

	FBlueprintHelperValidationSummary Val;
	Val.bShouldCompile = true;
	Val.bShouldSave = true;
	Success.Validation = Val;

	TArray<UEdGraphNode*> GeneratedNodes;
	if (Context.InsertedNode)
	{
		GeneratedNodes.Add(Context.InsertedNode);
	}
	if (Context.SequenceNode)
	{
		GeneratedNodes.AddUnique(Context.SequenceNode);
	}
	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Graph, GeneratedNodes);

	return Success;
}

// 鈹€鈹€鈹€ Inserted Logic 瑙ｆ瀽 鈹€鈹€鈹€

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

			const FBlueprintHelperMergeCallableFragmentResult CallableResult =
				FBlueprintHelperMergeCallableFragmentService::BuildCallableFragment(
					UGraphWriteCoreUtils::MakeMergeCallableRequest(
						Context.Blueprint,
						Context.Graph,
						InsertedCheck.EventName,
						Context.InsertedRef));
			if (!CallableResult.bOk || !CallableResult.PrimaryNode)
			{
				OutErrorCode = TEXT("inserted_logic_not_callable");
				OutError = FString::Printf(
					TEXT("inserted_logic_not_callable: unable to create owned block call '%s': %s"),
					*InsertedCheck.EventName,
					*UGraphWriteCoreUtils::FormatMergeCallableFailure(CallableResult, InsertedCheck.EventName));
				return false;
			}

			Context.InsertedNode = CallableResult.PrimaryNode;
			FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::MarkMergeNodeAsBlueprintHelperOwned(Context.InsertedNode, Request.AnchorBlockId);
			return true;
		}
	}
	case EBlueprintHelperMergeScope::CustomEventCall:
	{
		if (Request.InsertedCustomEventName.IsEmpty())
		{
			OutErrorCode = TEXT("inserted_logic_not_found");
			OutError = TEXT("inserted_logic_not_found: missing inserted.custom_event.");
			return false;
		}
		Context.InsertedRef = Request.InsertedCustomEventName;

		const FBlueprintHelperMergeCallableFragmentResult CallableResult =
			FBlueprintHelperMergeCallableFragmentService::BuildCallableFragment(
				UGraphWriteCoreUtils::MakeMergeCallableRequest(
					Context.Blueprint,
					Context.Graph,
					Request.InsertedCustomEventName,
					Context.InsertedRef));
		if (!CallableResult.bOk || !CallableResult.PrimaryNode)
		{
			OutErrorCode = TEXT("inserted_logic_not_found");
			const FString ResolverMessage = UGraphWriteCoreUtils::FormatMergeCallableFailure(CallableResult, Request.InsertedCustomEventName);
			OutError = FString::Printf(TEXT("inserted_logic_not_found: call_function resolve failed: %s"), *ResolverMessage);
			return false;
		}

		Context.InsertedNode = CallableResult.PrimaryNode;
		FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::MarkMergeNodeAsBlueprintHelperOwned(Context.InsertedNode, Request.AnchorBlockId);
		return true;
	}

	case EBlueprintHelperMergeScope::FunctionCall:
	{
		if (Request.InsertedFunctionName.IsEmpty())
		{
			OutErrorCode = TEXT("inserted_logic_not_found");
			OutError = TEXT("inserted_logic_not_found: missing inserted.function.");
			return false;
		}
		Context.InsertedRef = Request.InsertedFunctionName;

		const FBlueprintHelperMergeCallableFragmentResult CallableResult =
			FBlueprintHelperMergeCallableFragmentService::BuildCallableFragment(
				UGraphWriteCoreUtils::MakeMergeCallableRequest(
					Context.Blueprint,
					Context.Graph,
					Request.InsertedFunctionName,
					Context.InsertedRef));
		if (!CallableResult.bOk || !CallableResult.PrimaryNode)
		{
			OutErrorCode = TEXT("inserted_logic_not_found");
			const FString ResolverMessage = UGraphWriteCoreUtils::FormatMergeCallableFailure(CallableResult, Request.InsertedFunctionName);
			OutError = FString::Printf(TEXT("inserted_logic_not_found: call_function resolve failed: %s"), *ResolverMessage);
			return false;
		}

		Context.InsertedNode = CallableResult.PrimaryNode;
		FBlueprintHelperMergeBlueprintGraphServiceLocalUtils::MarkMergeNodeAsBlueprintHelperOwned(Context.InsertedNode, Request.AnchorBlockId);
		return true;
	}

	default:
		OutError = TEXT("unsupported_merge_scope");
		return false;
	}
}

// 鈹€鈹€鈹€ append_after 鈹€鈹€鈹€

bool FBlueprintHelperMergeBlueprintGraphService::ApplyMergeIntent(
	UBlueprint* BP,
	UEdGraph* Graph,
	const FMergeRequest& Request,
	FMergeContext& Context,
	FString& OutError) const
{
	if (!Context.InsertedNode)
	{
		OutError = TEXT("inserted_logic_not_found");
		return false;
	}

	FBlueprintHelperGraphWriteMutationIntent Intent;
	Intent.IntentId = Request.AnchorBlockId.IsEmpty() ? TEXT("merge_intent") : Request.AnchorBlockId;
	Intent.Source.Pin = Context.AnchorPin;
	Intent.InsertedNode = Context.InsertedNode;
	Intent.OriginalSuccessorPin = Context.OriginalSuccessorPin;
	Intent.SequenceOrder = Request.SequenceOrder;
	Intent.OutSequenceNode = &Context.SequenceNode;

	switch (Request.InsertStrategy)
	{
	case EBlueprintHelperInsertStrategy::AppendAfter:
		Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::AppendSemanticBody;
		break;
	case EBlueprintHelperInsertStrategy::InsertBetween:
		Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::InsertSemanticBodyBetweenPins;
		break;
	case EBlueprintHelperInsertStrategy::BranchFork:
		Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::BranchForkSemanticBody;
		break;
	default:
		OutError = TEXT("unsupported_merge_strategy");
		return false;
	}

	TArray<FString> Unresolved;
	const FBlueprintGenerateResult Result =
		FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents(Graph, {Intent}, Unresolved);
	if (!Result.bSucceed)
	{
		OutError = Unresolved.Num() > 0 ? Unresolved[0] : Result.Message;
		return false;
	}
	return true;
}
