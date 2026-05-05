// BlueprintHelper Service Layer — MergeBlueprintGraph 核心服务实现

#include "Services/BlueprintHelperMergeBlueprintGraphService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"
#include "Transactions/BlueprintHelperTransactionJournalService.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/BlueprintHelperAppendGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FBlueprintHelperMergeBlueprintGraphService::FBlueprintHelperMergeBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperLogicJsonPathService& InPathService,
	const FBlueprintHelperTransactionJournalService& InJournalService)
	: Resolver(InResolver), PathService(InPathService), JournalService(InJournalService)
{
}

// ─── 公共入口 ───

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

	const TArray<TSharedPtr<FJsonValue>>* SeqOrder = nullptr;
	if (Payload->TryGetArrayField(TEXT("sequence_order"), SeqOrder))
		for (const auto& V : *SeqOrder)
		{ FString Item; if (V->TryGetString(Item)) Req.SequenceOrder.Add(Item); }

	Payload->TryGetBoolField(TEXT("dry_run"), Req.bDryRun);
	return Req;
}

// ─── Preflight ───

FBlueprintHelperMergeBlueprintGraphService::FMergePreflightResult
FBlueprintHelperMergeBlueprintGraphService::Preflight(const FMergeRequest& Request, FMergeContext& Context) const
{
	FMergePreflightResult Result;

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
	}

	return Result;
}

// ─── Anchor 解析 ───

bool FBlueprintHelperMergeBlueprintGraphService::ResolveAnchor(
	const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const
{
	// 定位 node
	FBlueprintHelperPatchResolveError NodeErr;
	if (!PathService.ResolveNode(Context.Graph, Request.AnchorNodeRef, Request.AnchorNodePath, Context.AnchorNode, NodeErr))
	{
		OutError = NodeErr.Message;
		return false;
	}

	// 定位 pin
	FBlueprintHelperPatchResolveError PinErr;
	if (!PathService.ResolvePin(Context.Graph, Context.AnchorNode, Request.AnchorPinRef, Request.AnchorPinPath, Context.AnchorPin, PinErr))
	{
		OutError = PinErr.Message;
		return false;
	}

	// 必须是 Exec output
	if (!Context.AnchorPin)
	{
		OutError = TEXT("anchor_pin_not_found：目标 Pin 不存在。");
		return false;
	}
	if (Context.AnchorPin->Direction != EGPD_Output)
	{
		OutError = TEXT("anchor_pin_not_exec：Anchor Pin 必须是输出 Pin。");
		return false;
	}
	if (Context.AnchorPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
	{
		OutError = TEXT("anchor_pin_not_exec：Anchor Pin 必须是 Exec 类型。");
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
	if (BP && Graph) Pre = Preflight(Request, Context);
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
	}
	else
	{
		FBlueprintHelperMergeDryRunData Data;
		Data.DryRun.Result = TEXT("blocked");
		Data.DryRun.bCanExecute = false;
		Data.DryRun.BlockedBy = Pre.BlockedBy;
		for (const auto& C : Pre.Conflicts) Data.DryRun.Conflicts.Add(C);
		for (const auto& E : Pre.Errors) Data.DryRun.Errors.Add(E);
		Result.Data = Data.ToJson();
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
	if (Context.Successors.Num() > 0) Context.OriginalSuccessorPin = Context.Successors[0];

	// 4. Preflight
	FMergePreflightResult Pre = Preflight(Request, Context);
	if (!Pre.bPassed)
	{
		FBlueprintHelperToolError Err;
		Err.Code = Pre.BlockedBy.Num() > 0 ? Pre.BlockedBy[0] : TEXT("preflight_failed");
		Err.Stage = EBlueprintHelperToolStage::Preflight;
		Err.Message = Pre.Conflicts.Num() > 0 ? Pre.Conflicts[0].Message : TEXT("Preflight 未通过。");
		Err.bRetryable = false;
		Err.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId, Err);
	}

	// 5. Resolve inserted logic
	FString InsertedError;
	if (!ResolveInsertedLogic(Request, Context, InsertedError))
	{
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("merge_blueprint_graph"), TraceId,
			{TEXT("inserted_logic_not_found"), EBlueprintHelperToolStage::ResolveTarget,
			 InsertedError, false, EBlueprintHelperRollbackResult::NotNeeded});
	}

	// 6. Scoped mutation + apply
	FBlueprintHelperScopedAssetMutation Mutation(FText::FromString(TEXT("BlueprintHelper Merge Graph")), BP);
	Mutation.Modify(Graph);

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

	FBlueprintHelperValidationSummary Val;
	Val.bShouldCompile = true;
	Val.bShouldSave = true;
	Success.Validation = Val;

	return Success;
}

// ─── Inserted Logic 解析 ───

bool FBlueprintHelperMergeBlueprintGraphService::ResolveInsertedLogic(
	const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const
{
	switch (Request.MergeScope)
	{
	case EBlueprintHelperMergeScope::OwnedBlockCall:
		Context.InsertedRef = Request.InsertedBlockId.IsEmpty() ? Request.InsertedBlockRef : Request.InsertedBlockId;
		if (Context.InsertedRef.IsEmpty()) { OutError = TEXT("inserted_logic_not_found: 缺少 block_id。"); return false; }
		return true;

	case EBlueprintHelperMergeScope::CustomEventCall:
	{
		if (Request.InsertedCustomEventName.IsEmpty()) { OutError = TEXT("inserted_logic_not_found: 缺少 custom_event 名。"); return false; }
		Context.InsertedRef = Request.InsertedCustomEventName;

		// 查找 CustomEvent
		for (UEdGraphNode* Node : Context.Graph->Nodes)
		{
			if (Node && Node->IsA<UK2Node_CustomEvent>())
			{
				const FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				if (Title.Contains(Request.InsertedCustomEventName))
				{
					Context.InsertedNode = Node;
					return true;
				}
			}
		}
		// 未找到也可创建 CallCustomEvent 节点
		return true;
	}

	case EBlueprintHelperMergeScope::FunctionCall:
	{
		if (Request.InsertedFunctionName.IsEmpty()) { OutError = TEXT("inserted_logic_not_found: 缺少 function 名。"); return false; }
		Context.InsertedRef = Request.InsertedFunctionName;

		// 生成 CallFunction 节点
		UFunction* TargetFunc = Context.Blueprint ? Context.Blueprint->SkeletonGeneratedClass
			? Context.Blueprint->SkeletonGeneratedClass->FindFunctionByName(*Request.InsertedFunctionName) : nullptr : nullptr;
		if (!TargetFunc) { OutError = FString::Printf(TEXT("inserted_logic_not_found: 函数 '%s' 不存在。"), *Request.InsertedFunctionName); return false; }

		UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Context.Graph);
		CallNode->SetFromFunction(TargetFunc);
		Context.Graph->AddNode(CallNode, true, false);
		CallNode->CreateNewGuid();
		CallNode->PostPlacedNewNode();
		CallNode->AllocateDefaultPins();
		Context.InsertedNode = CallNode;
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
	// 确保有 inserted node
	if (!Context.InsertedNode)
	{
		// 对 owned_block_call / custom_event_call 创建简单的占位节点
		// 第一版简化：仅支持 function_call (已有 node) 和已存在的 inserted node
		if (Request.MergeScope == EBlueprintHelperMergeScope::FunctionCall && Context.InsertedNode)
		{
			// already created in ResolveInsertedLogic
		}
		else
		{
			OutError = TEXT("inserted_logic_not_found: 该 merge_scope 的节点创建尚未实现。");
			return false;
		}
	}

	// 查找 inserted node 的 exec input
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

	// Create Sequence node
	UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(Graph);
	Graph->AddNode(SeqNode, true, false);
	SeqNode->CreateNewGuid();
	SeqNode->PostPlacedNewNode();
	SeqNode->AllocateDefaultPins();
	Context.SequenceNode = SeqNode;

	// Find Sequence Exec In
	UEdGraphPin* SeqExecIn = nullptr;
	for (UEdGraphPin* Pin : SeqNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{ SeqExecIn = Pin; break; }
	}
	if (!SeqExecIn) { OutError = TEXT("Sequence 节点无 Exec 输入。"); return false; }

	// Collect Sequence Then pins
	TArray<UEdGraphPin*> SeqThenPins;
	for (UEdGraphPin* Pin : SeqNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			SeqThenPins.Add(Pin);
	}
	if (SeqThenPins.Num() < 2) { OutError = TEXT("Sequence 节点需要至少 2 个 Then 输出。"); return false; }

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
