// BlueprintHelper Service Layer — PatchBlueprintGraph 核心服务实现

#include "Services/BlueprintHelperPatchBlueprintGraphService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"
#include "Transactions/BlueprintHelperTransactionJournalService.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/BlueprintHelperAppendGraphTypes.h"
#include "Structure/BlueprintHelperReplaceGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FBlueprintHelperPatchBlueprintGraphService::FBlueprintHelperPatchBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperLogicJsonPathService& InPathService,
	const FBlueprintHelperTransactionJournalService& InJournalService)
	: Resolver(InResolver)
	, PathService(InPathService)
	, JournalService(InJournalService)
{
}

// ─── 公共入口 ───

FBlueprintHelperToolResultBase FBlueprintHelperPatchBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FPatchRequest Request = ParseRequest(Payload);

	if (Request.bDryRun)
	{
		return ExecuteDryRun(Request);
	}

	return ExecuteWrite(Request);
}

// ─── 解析 ───

FBlueprintHelperPatchBlueprintGraphService::FPatchRequest
FBlueprintHelperPatchBlueprintGraphService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FPatchRequest Req;

	if (!Payload.IsValid()) return Req;

	const TSharedPtr<FJsonObject>* TargetObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObj) && TargetObj->IsValid())
	{
		(*TargetObj)->TryGetStringField(TEXT("asset_path"), Req.AssetPath);
		(*TargetObj)->TryGetStringField(TEXT("graph"), Req.GraphName);
		FString ScopeStr;
		if ((*TargetObj)->TryGetStringField(TEXT("patch_scope"), ScopeStr))
			ParsePatchScope(ScopeStr, Req.PatchScope);
	}

	FString TypeStr;
	if (Payload->TryGetStringField(TEXT("patch_type"), TypeStr))
		ParsePatchType(TypeStr, Req.PatchType);

	Payload->TryGetBoolField(TEXT("dry_run"), Req.bDryRun);

	const TSharedPtr<FJsonObject>* PatchedRefObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("patched_ref"), PatchedRefObj) && PatchedRefObj->IsValid())
	{
		(*PatchedRefObj)->TryGetStringField(TEXT("node_ref"), Req.NodeRef);
		(*PatchedRefObj)->TryGetStringField(TEXT("pin_ref"), Req.PinRef);
		(*PatchedRefObj)->TryGetStringField(TEXT("link_ref"), Req.LinkRef);
		(*PatchedRefObj)->TryGetStringField(TEXT("node_path"), Req.NodePath);
		(*PatchedRefObj)->TryGetStringField(TEXT("pin_path"), Req.PinPath);
		(*PatchedRefObj)->TryGetStringField(TEXT("link_path"), Req.LinkPath);
	}

	const TSharedPtr<FJsonObject>* PatchObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("patch"), PatchObj) && PatchObj->IsValid())
	{
		Req.PatchPayload = *PatchObj;
	}

	const TSharedPtr<FJsonObject>* ExpectObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("expected_old_state"), ExpectObj) && ExpectObj->IsValid())
	{
		Req.bExpectedOldStateProvided = true;
		(*ExpectObj)->TryGetStringField(TEXT("value"), Req.ExpectedOldValue);
	}

	return Req;
}

// ─── Preflight ───

FBlueprintHelperPatchBlueprintGraphService::FPatchPreflightResult
FBlueprintHelperPatchBlueprintGraphService::Preflight(
	const FPatchRequest& Request, UEdGraph* Graph, FBlueprintHelperResolvedPatchTarget& OutTarget) const
{
	FPatchPreflightResult Result;

	if (Request.PatchType == EBlueprintHelperPatchType::RenameLocalVariableRef ||
		Request.PatchType == EBlueprintHelperPatchType::SetCallTarget)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("unsupported_patch_type"));
		Result.Conflicts.Add({TEXT("unsupported_patch_type"),
			FString::Printf(TEXT("patch_type '%s' 在第一版中暂不支持。"),
				PatchTypeToString(Request.PatchType)), TEXT("patch_type"), TEXT("payload")});
		return Result;
	}

	// 定位 node
	FBlueprintHelperPatchResolveError NodeError;
	UEdGraphNode* Node = nullptr;
	if (!PathService.ResolveNode(Graph, Request.NodeRef, Request.NodePath, Node, NodeError))
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(NodeError.Code);
		Result.Conflicts.Add({NodeError.Code, NodeError.Message, NodeError.Target, TEXT("patched_ref")});
		return Result;
	}
	OutTarget.Node = Node;
	OutTarget.PatchedRef.GraphId = Graph->GetName();
	OutTarget.PatchedRef.NodeRef = Request.NodeRef;

	// 定位 pin（如需）
	if (Request.PatchType == EBlueprintHelperPatchType::SetPinDefault ||
		Request.PatchType == EBlueprintHelperPatchType::ConnectPins ||
		Request.PatchType == EBlueprintHelperPatchType::DisconnectLink ||
		Request.PatchType == EBlueprintHelperPatchType::ReplaceLink)
	{
		FBlueprintHelperPatchResolveError PinError;
		UEdGraphPin* Pin = nullptr;
		if (!PathService.ResolvePin(Graph, Node, Request.PinRef, Request.PinPath, Pin, PinError))
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(PinError.Code);
			Result.Conflicts.Add({PinError.Code, PinError.Message, PinError.Target, TEXT("patched_ref")});
			return Result;
		}
		OutTarget.Pin = Pin;
		OutTarget.PatchedRef.PinRef = Request.PinRef;
	}

	// 定位 link（如需）
	if (Request.PatchType == EBlueprintHelperPatchType::DisconnectLink ||
		Request.PatchType == EBlueprintHelperPatchType::ReplaceLink)
	{
		FBlueprintHelperPatchResolveError LinkError;
		if (!PathService.ResolveLink(Graph, Request.LinkRef, Request.LinkPath, OutTarget.Link, LinkError))
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(LinkError.Code);
			Result.Conflicts.Add({LinkError.Code, LinkError.Message, LinkError.Target, TEXT("patched_ref")});
			return Result;
		}
		OutTarget.PatchedRef.LinkRef = Request.LinkRef;
	}

	// expected_old_state 校验
	if (Request.bExpectedOldStateProvided && !Request.ExpectedOldValue.IsEmpty())
	{
		FString CurrentValue;
		if (OutTarget.Pin)
		{
			CurrentValue = OutTarget.Pin->DefaultValue;
		}
		Result.BeforeValue = CurrentValue;

		if (CurrentValue != Request.ExpectedOldValue)
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("expected_old_state_mismatch"));
			Result.Conflicts.Add({TEXT("expected_old_state_mismatch"),
				FString::Printf(TEXT("expected_old_value '%s' 与当前值 '%s' 不匹配。"),
					*Request.ExpectedOldValue, *CurrentValue), TEXT("expected_old_state.value"), TEXT("payload")});
		}
	}

	return Result;
}

// ─── DryRun ───

FBlueprintHelperToolResultBase FBlueprintHelperPatchBlueprintGraphService::ExecuteDryRun(
	const FPatchRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	// 解析
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Target, Diag);
	UEdGraph* Graph = nullptr;
	if (BP)
	{
		for (UEdGraph* Page : BP->UbergraphPages)
		{
			if (Page && Page->GetName() == Request.GraphName) { Graph = Page; break; }
		}
		if (!Graph)
		{
			for (UEdGraph* Fn : BP->FunctionGraphs)
			{
				if (Fn && Fn->GetName() == Request.GraphName) { Graph = Fn; break; }
			}
		}
	}

	FBlueprintHelperResolvedPatchTarget ResolvedTarget;
	FPatchPreflightResult PreflightResult;
	if (BP && Graph)
	{
		PreflightResult = Preflight(Request, Graph, ResolvedTarget);
	}
	else
	{
		PreflightResult.bPassed = false;
		PreflightResult.BlockedBy.Add(Graph ? TEXT("target_blueprint_not_found") : TEXT("target_graph_not_found"));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("patch_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef TargetRef;
	TargetRef.AssetPath = Request.AssetPath;
	TargetRef.Graph = Request.GraphName;
	Result.Target = TargetRef;

	if (PreflightResult.bPassed)
	{
		FBlueprintHelperPatchDryRunData Data;
		Data.DryRun.Result = TEXT("passed");
		Data.DryRun.bCanExecute = true;
		Result.Data = Data.ToJson();
	}
	else
	{
		FBlueprintHelperPatchDryRunData Data;
		Data.DryRun.Result = TEXT("blocked");
		Data.DryRun.bCanExecute = false;
		Data.DryRun.BlockedBy = PreflightResult.BlockedBy;
		Data.DryRun.Conflicts = PreflightResult.Conflicts;
		Data.DryRun.Errors = PreflightResult.Errors;
		Result.Data = Data.ToJson();
	}

	return Result;
}

// ─── 正式写入 ───

FBlueprintHelperToolResultBase FBlueprintHelperPatchBlueprintGraphService::ExecuteWrite(
	const FPatchRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FString TransactionId = JournalService.GenerateTransactionId();

	// 1-2. 解析蓝图和图表
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Target, Diag);
	if (!BP)
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId,
			{TEXT("target_blueprint_not_found"), EBlueprintHelperToolStage::ResolveTarget,
			 TEXT("蓝图未找到。"), false, EBlueprintHelperRollbackResult::NotNeeded});

	UEdGraph* Graph = nullptr;
	for (UEdGraph* Page : BP->UbergraphPages)
		if (Page && Page->GetName() == Request.GraphName) { Graph = Page; break; }
	if (!Graph)
		for (UEdGraph* Fn : BP->FunctionGraphs)
			if (Fn && Fn->GetName() == Request.GraphName) { Graph = Fn; break; }
	if (!Graph)
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId,
			{TEXT("target_graph_not_found"), EBlueprintHelperToolStage::ResolveTarget,
			 FString::Printf(TEXT("图表 %s 未找到。"), *Request.GraphName), false, EBlueprintHelperRollbackResult::NotNeeded});

	// 3. Preflight
	FBlueprintHelperResolvedPatchTarget ResolvedTarget;
	FPatchPreflightResult PreflightResult = Preflight(Request, Graph, ResolvedTarget);
	if (!PreflightResult.bPassed)
	{
		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = PreflightResult.Conflicts.Num() > 0 ? PreflightResult.Conflicts[0].Message : TEXT("Preflight 未通过。");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId, Error);
	}

	// 4. 开始修改
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Patch Graph")), BP);
	Mutation.Modify(Graph);

	// 5. Apply patch
	bool bChanged = false;
	FString ApplyError;
	if (!ApplyPatch(BP, Graph, Request, ResolvedTarget, bChanged, ApplyError))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("link_create_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ApplyError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId, Error);
	}

	// 6. 写 Journal
	FBlueprintHelperAppendJournalRecord JRecord;
	JRecord.TransactionId = TransactionId;
	JRecord.Tool = TEXT("PatchBlueprintGraph");
	JRecord.Status = bChanged ? TEXT("applied") : TEXT("no_op");
	JRecord.TargetAssets.Add(Request.AssetPath);
	JRecord.GraphId = Request.GraphName;
	JRecord.GraphName = Request.GraphName;

	FString JError;
	if (!JournalService.WriteAppendJournal(JRecord, JError))
	{
		Mutation.Rollback();
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("journal_write_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = JError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("patch_blueprint_graph"), TraceId, Error);
	}

	// 7. 标记修改
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	if (BP->GetOutermost()) BP->GetOutermost()->MarkPackageDirty();
	Mutation.Commit();

	// 8. 成功结果
	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("patch_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef STarget;
	STarget.AssetPath = Request.AssetPath;
	STarget.Graph = Request.GraphName;
	Success.Target = STarget;

	FBlueprintHelperPatchGraphResultData Data;
	Data.PatchResult.PatchedRef = ResolvedTarget.PatchedRef;
	Data.PatchResult.Patch.PatchType = PatchTypeToString(Request.PatchType);
	Data.PatchResult.Patch.bExpectedOldStateProvided = Request.bExpectedOldStateProvided;
	Data.PatchResult.Patch.bChanged = bChanged;
	Data.WriteRef.TransactionId = TransactionId;
	Data.WriteRef.bJournalRecorded = true;
	Success.Data = Data.ToJson();

	if (!bChanged)
	{
		Success.bModified = false;
		Success.Status = EBlueprintHelperToolStatus::Applied;
	}

	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = bChanged;
	Validation.bShouldSave = bChanged;
	Success.Validation = Validation;

	return Success;
}

// ─── ApplyPatch 分发 ───

bool FBlueprintHelperPatchBlueprintGraphService::ApplyPatch(
	UBlueprint* Blueprint, UEdGraph* Graph,
	const FPatchRequest& Request, const FBlueprintHelperResolvedPatchTarget& Target,
	bool& bOutChanged, FString& OutError) const
{
	switch (Request.PatchType)
	{
	case EBlueprintHelperPatchType::SetPinDefault:
	{
		FString NewVal;
		if (Request.PatchPayload.IsValid())
			Request.PatchPayload->TryGetStringField(TEXT("value"), NewVal);
		return ApplySetPinDefault(Graph, Target.Pin, NewVal, bOutChanged, OutError);
	}
	case EBlueprintHelperPatchType::SetNodeComment:
	{
		FString Comment;
		if (Request.PatchPayload.IsValid())
			Request.PatchPayload->TryGetStringField(TEXT("comment"), Comment);
		return ApplySetNodeComment(Target.Node, Comment, bOutChanged, OutError);
	}
	case EBlueprintHelperPatchType::SetNodePosition:
		return ApplySetNodePosition(Target.Node, Request.PatchPayload, bOutChanged, OutError);
	case EBlueprintHelperPatchType::ConnectPins:
	{
		// 目标 Pin 从 patched_ref.pin_ref 解析
		if (!Target.Pin)
		{
			OutError = TEXT("connect_pins 需要目标 pin_ref。");
			return false;
		}
		// 源 Pin 从 patch payload
		UEdGraphPin* FromPin = nullptr;
		FString FromNodeRef, FromPinRef;
		if (Request.PatchPayload.IsValid())
		{
			Request.PatchPayload->TryGetStringField(TEXT("from_node"), FromNodeRef);
			Request.PatchPayload->TryGetStringField(TEXT("from_pin"), FromPinRef);
		}
		FBlueprintHelperPatchResolveError Ignored;
		UEdGraphNode* FromNode = nullptr;
		if (!PathService.ResolveNode(Graph, FromNodeRef, FString(), FromNode, Ignored))
		{
			OutError = FString::Printf(TEXT("无法定位源节点: %s"), *FromNodeRef);
			return false;
		}
		if (!PathService.ResolvePin(Graph, FromNode, FromPinRef, FString(), FromPin, Ignored))
		{
			OutError = FString::Printf(TEXT("无法定位源 Pin: %s"), *FromPinRef);
			return false;
		}
		return ApplyConnectPins(Graph, FromPin, Target.Pin, bOutChanged, OutError);
	}
	case EBlueprintHelperPatchType::DisconnectLink:
	{
		if (!Target.Link.SourcePin || !Target.Link.TargetPin)
		{
			OutError = TEXT("disconnect_link 需要指定来源和目标 Pin。");
			return false;
		}
		return ApplyDisconnectLink(Target.Link.SourcePin, Target.Link.TargetPin, bOutChanged, OutError);
	}
	case EBlueprintHelperPatchType::ReplaceLink:
	{
		if (!Target.Link.SourcePin)
		{
			OutError = TEXT("replace_link 需要指定旧连接。");
			return false;
		}
		// 新目标 Pin
		UEdGraphPin* NewToPin = Target.Pin;
		if (!NewToPin)
		{
			OutError = TEXT("replace_link 需要指定新目标 Pin。");
			return false;
		}
		return ApplyReplaceLink(Graph, Target.Link, NewToPin, bOutChanged, OutError);
	}
	default:
		OutError = FString::Printf(TEXT("不支持的 patch_type: %s"), PatchTypeToString(Request.PatchType));
		return false;
	}
}

// ─── set_pin_default ───

bool FBlueprintHelperPatchBlueprintGraphService::ApplySetPinDefault(
	UEdGraph* Graph, UEdGraphPin* Pin, const FString& NewValue, bool& bOutChanged, FString& OutError) const
{
	if (!Pin)
	{
		OutError = TEXT("target_pin_not_found");
		return false;
	}

	const FString OldValue = Pin->DefaultValue;
	if (OldValue == NewValue)
	{
		bOutChanged = false;
		return true;
	}

	Pin->Modify();
	Pin->DefaultValue = NewValue;
	bOutChanged = true;
	Graph->NotifyGraphChanged();
	return true;
}

// ─── set_node_comment ───

bool FBlueprintHelperPatchBlueprintGraphService::ApplySetNodeComment(
	UEdGraphNode* Node, const FString& NewComment, bool& bOutChanged, FString& OutError) const
{
	if (!Node)
	{
		OutError = TEXT("target_node_not_found");
		return false;
	}

	if (Node->NodeComment == NewComment)
	{
		bOutChanged = false;
		return true;
	}

	Node->Modify();
	Node->NodeComment = NewComment;
	bOutChanged = true;
	return true;
}

// ─── set_node_position ───

bool FBlueprintHelperPatchBlueprintGraphService::ApplySetNodePosition(
	UEdGraphNode* Node, const TSharedPtr<FJsonObject>& Payload, bool& bOutChanged, FString& OutError) const
{
	if (!Node || !Payload.IsValid())
	{
		OutError = TEXT("target_node_not_found");
		return false;
	}

	int32 NewX = 0, NewY = 0;
	bool bHasX = false, bHasY = false;
	if (Payload->TryGetNumberField(TEXT("x"), NewX)) { bHasX = true; }
	else if (Payload->TryGetNumberField(TEXT("node_x"), NewX)) { bHasX = true; }
	if (Payload->TryGetNumberField(TEXT("y"), NewY)) { bHasY = true; }
	else if (Payload->TryGetNumberField(TEXT("node_y"), NewY)) { bHasY = true; }

	if (!bHasX && !bHasY)
	{
		OutError = TEXT("缺少 x/y 位置参数。");
		return false;
	}

	const int32 OldX = Node->NodePosX;
	const int32 OldY = Node->NodePosY;

	if (OldX == NewX && OldY == NewY)
	{
		bOutChanged = false;
		return true;
	}

	Node->Modify();
	if (bHasX) Node->NodePosX = NewX;
	if (bHasY) Node->NodePosY = NewY;
	bOutChanged = true;
	Node->GetGraph()->NotifyGraphChanged();
	return true;
}

// ─── connect_pins ───

bool FBlueprintHelperPatchBlueprintGraphService::ApplyConnectPins(
	UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, bool& bOutChanged, FString& OutError) const
{
	if (!FromPin || !ToPin)
	{
		OutError = TEXT("pin_not_found");
		return false;
	}

	// 已连接的跳过
	if (FromPin->LinkedTo.Contains(ToPin))
	{
		bOutChanged = false;
		return true;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, ToPin);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		OutError = FString::Printf(TEXT("pin_type_mismatch: %s"), *Response.Message.ToString());
		return false;
	}

	// Exec Pin 已有后继时不允许多连
	if (FromPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && FromPin->LinkedTo.Num() > 0)
	{
		OutError = TEXT("exec_flow_requires_merge: Exec Pin 已有后继，不允许自动重排。请使用 Merge 工具。");
		return false;
	}

	FromPin->Modify();
	ToPin->Modify();
	bOutChanged = Schema->TryCreateConnection(FromPin, ToPin);
	if (!bOutChanged)
	{
		OutError = TEXT("link_create_failed");
		return false;
	}

	Graph->NotifyGraphChanged();
	return true;
}

// ─── disconnect_link ───

bool FBlueprintHelperPatchBlueprintGraphService::ApplyDisconnectLink(
	UEdGraphPin* FromPin, UEdGraphPin* ToPin, bool& bOutChanged, FString& OutError) const
{
	if (!FromPin || !ToPin)
	{
		OutError = TEXT("pin_not_found");
		return false;
	}

	if (!FromPin->LinkedTo.Contains(ToPin))
	{
		bOutChanged = false;
		return true;
	}

	FromPin->Modify();
	ToPin->Modify();
	FromPin->BreakLinkTo(ToPin);
	bOutChanged = true;
	return true;
}

// ─── replace_link ───

bool FBlueprintHelperPatchBlueprintGraphService::ApplyReplaceLink(
	UEdGraph* Graph, const FBlueprintHelperResolvedLink& OldLink,
	UEdGraphPin* NewToPin, bool& bOutChanged, FString& OutError) const
{
	if (!OldLink.SourcePin || !OldLink.TargetPin || !NewToPin)
	{
		OutError = TEXT("pin_not_found");
		return false;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(OldLink.SourcePin, NewToPin);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		OutError = FString::Printf(TEXT("pin_type_mismatch: %s"), *Response.Message.ToString());
		return false;
	}

	OldLink.SourcePin->Modify();
	OldLink.TargetPin->Modify();
	NewToPin->Modify();

	OldLink.SourcePin->BreakLinkTo(OldLink.TargetPin);
	bool bCreated = Schema->TryCreateConnection(OldLink.SourcePin, NewToPin);
	if (!bCreated)
	{
		// 尝试恢复旧连接
		Schema->TryCreateConnection(OldLink.SourcePin, OldLink.TargetPin);
		OutError = TEXT("link_create_failed: 新连接失败，已恢复旧连接。");
		return false;
	}

	bOutChanged = true;
	Graph->NotifyGraphChanged();
	return true;
}
