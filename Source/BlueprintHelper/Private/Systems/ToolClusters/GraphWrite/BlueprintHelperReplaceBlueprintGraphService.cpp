// BlueprintHelper Service Layer — ReplaceBlueprintGraph 核心服务实现

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Shared/Services/BlueprintHelperAgentImportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils
{
public:
	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static bool NodeMatchesEntryName(UEdGraphNode* Node, const FString& EntryName)
	{
		if (!Node)
		{
			return false;
		}
		if (EntryName.IsEmpty())
		{
			return true;
		}

		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
		{
			if (CustomEvent->CustomFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			if (EventNode->GetFunctionName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				EventNode->EventReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
		{
			if (FunctionEntry->FunctionReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				FunctionEntry->CustomGeneratedFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		const FString NodeName = Node->GetName();
		const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		return NodeName.Equals(EntryName, ESearchCase::IgnoreCase) ||
			NodeTitle.Equals(EntryName, ESearchCase::IgnoreCase);
	}

	static bool TryReadBlueprintHelperBlockId(UEdGraphNode* Node, FString& OutBlockId)
	{
		OutBlockId.Reset();
		if (!Node)
		{
			return false;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			return false;
		}

		FMetaData& MetaData = Package->GetMetaData();
		if (MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) != TEXT("true"))
		{
			return false;
		}

		OutBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		return !OutBlockId.IsEmpty();
	}

	static bool HasInboundExecLinkFromImportedNode(UEdGraphPin* ExecInputPin, const TSet<UEdGraphNode*>& ImportedNodes)
	{
		if (!ExecInputPin)
		{
			return false;
		}

		for (UEdGraphPin* LinkedPin : ExecInputPin->LinkedTo)
		{
			if (!LinkedPin ||
				LinkedPin->Direction != EGPD_Output ||
				LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}

			if (ImportedNodes.Contains(LinkedPin->GetOwningNode()))
			{
				return true;
			}
		}

		return false;
	}

	static UEdGraphNode* FindFirstImportedExecutableBodyNode(
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport)
	{
		TArray<UEdGraphNode*> ImportedExecutableNodes;
		TSet<UEdGraphNode*> ImportedNodes;

		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || NodesBeforeImport.Contains(Node))
			{
				continue;
			}

			ImportedNodes.Add(Node);
			if (FindFirstExecPin(Node, EGPD_Input))
			{
				ImportedExecutableNodes.Add(Node);
			}
		}

		for (UEdGraphNode* Node : ImportedExecutableNodes)
		{
			if (!HasInboundExecLinkFromImportedNode(FindFirstExecPin(Node, EGPD_Input), ImportedNodes))
			{
				return Node;
			}
		}

		return ImportedExecutableNodes.Num() > 0 ? ImportedExecutableNodes[0] : nullptr;
	}

	static bool PinsHaveSingleConnectionToEachOther(UEdGraphPin* FirstPin, UEdGraphPin* SecondPin)
	{
		return FirstPin &&
			SecondPin &&
			FirstPin->LinkedTo.Num() == 1 &&
			SecondPin->LinkedTo.Num() == 1 &&
			FirstPin->LinkedTo[0] == SecondPin &&
			SecondPin->LinkedTo[0] == FirstPin;
	}

	static void BreakAllPinLinksWithModify(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return;
		}

		Pin->Modify();
		Pin->BreakAllPinLinks(true);
	}

};

// ─── 构造 ───

FBlueprintHelperReplaceBlueprintGraphService::FBlueprintHelperReplaceBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperAgentImportService& InAgentImportService,
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperOwnershipService& InOwnershipService,
	const FBlueprintHelperTransactionJournalService& InJournalService,
	const FBlueprintHelperGraphSnapshotService& InSnapshotService)
	: Resolver(InResolver)
	, AgentImportService(InAgentImportService)
	, BlockIdService(InBlockIdService)
	, OwnershipService(InOwnershipService)
	, JournalService(InJournalService)
	, SnapshotService(InSnapshotService)
{
}

// ─── 公共入口 ───

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FReplaceRequest Request = ParseRequest(Payload);

	if (Request.bDryRun)
	{
		return ExecuteDryRun(Request);
	}

	return ExecuteWrite(Request);
}

// ─── 解析 ───

FBlueprintHelperReplaceBlueprintGraphService::FReplaceRequest
FBlueprintHelperReplaceBlueprintGraphService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FReplaceRequest Request;

	if (!Payload.IsValid())
	{
		return Request;
	}

	// target
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName);

		FString ScopeStr;
		if ((*TargetObject)->TryGetStringField(TEXT("replace_scope"), ScopeStr))
		{
			ParseReplaceScope(ScopeStr, Request.Scope);
		}
	}

	// selector
	const TSharedPtr<FJsonObject>* SelectorObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("selector"), SelectorObject) && SelectorObject->IsValid())
	{
		(*SelectorObject)->TryGetStringField(TEXT("block_id"), Request.BlockId);
		(*SelectorObject)->TryGetStringField(TEXT("target_ref"), Request.TargetRef);
		(*SelectorObject)->TryGetStringField(TEXT("entry_name"), Request.EntryName);
		(*SelectorObject)->TryGetStringField(TEXT("node_path"), Request.NodePath);
	}

	// replacement
	const TSharedPtr<FJsonObject>* ReplacementObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("replacement"), ReplacementObject) && ReplacementObject->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		if ((*ReplacementObject)->TryGetArrayField(TEXT("nodes"), NodesArray))
		{
			Request.Nodes = *NodesArray;
		}

		const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
		if ((*ReplacementObject)->TryGetArrayField(TEXT("links"), LinksArray))
		{
			Request.Links = *LinksArray;
		}
	}

	// options
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("options"), OptionsObject) && OptionsObject->IsValid())
	{
		(*OptionsObject)->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
		(*OptionsObject)->TryGetBoolField(TEXT("strict"), Request.bStrict);
		(*OptionsObject)->TryGetBoolField(TEXT("preserve_layout"), Request.bPreserveLayout);
	}

	return Request;
}

// ─── Preflight ───

FBlueprintHelperReplaceBlueprintGraphService::FReplacePreflightResult
FBlueprintHelperReplaceBlueprintGraphService::Preflight(const FReplaceRequest& Request) const
{
	FReplacePreflightResult Result;

	// 1. asset_path
	if (Request.AssetPath.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		Result.Conflicts.Add({TEXT("target_blueprint_not_found"),
			TEXT("缺少 target.asset_path。"), TEXT("target.asset_path"), TEXT("payload")});
		return Result;
	}

	// 2. graph
	if (Request.GraphName.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_graph_not_found"));
		Result.Conflicts.Add({TEXT("target_graph_not_found"),
			TEXT("缺少 target.graph。"), TEXT("target.graph"), TEXT("payload")});
		return Result;
	}

	// 3. replace_scope
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionDefinition ||
		Request.Scope == EBlueprintHelperReplaceScope::EventDefinition)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("replace_scope_unsupported"));
		Result.Conflicts.Add({TEXT("replace_scope_unsupported"),
			TEXT("function_definition / event_definition 的正式写入暂不支持，请使用 dry_run。"),
			TEXT("target.replace_scope"), TEXT("payload")});
		return Result;
	}

	// 4. replacement nodes
	if (Request.Nodes.Num() == 0)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("empty_replacement"));
		Result.Conflicts.Add({TEXT("empty_replacement"),
			TEXT("replacement.nodes 不能为空。"), TEXT("replacement.nodes"), TEXT("payload")});
		return Result;
	}

	// 5. 蓝图校验
	UBlueprint* Blueprint = nullptr;
	if (!PreflightBlueprint(Request.AssetPath, Blueprint, Result))
	{
		return Result;
	}

	// 6. 图表校验
	UEdGraph* Graph = nullptr;
	if (!PreflightGraphTarget(Blueprint, Request.GraphName, Request.Scope, Graph, Result))
	{
		return Result;
	}

	// 7. scope 校验
	if (!PreflightReplaceScope(Request.Scope, Result))
	{
		return Result;
	}

	// 8. selector 存在性检查
	if (Request.Scope == EBlueprintHelperReplaceScope::BlockImplementation)
	{
		if (Request.BlockId.IsEmpty() && Request.TargetRef.IsEmpty() && Request.NodePath.IsEmpty())
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("target_block_not_found"));
			Result.Conflicts.Add({TEXT("target_block_not_found"),
				TEXT("block_implementation 需要 selector.block_id、selector.target_ref 或 selector.node_path。"),
				TEXT("selector"), TEXT("payload")});
		}
	}

	return Result;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightBlueprint(
	const FString& AssetPath, UBlueprint*& OutBlueprint, FReplacePreflightResult& OutResult) const
{
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;

	FBlueprintHelperDiagnosticSet Diag;
	OutBlueprint = Resolver.ResolveBlueprint(Target, Diag);

	if (!OutBlueprint || Diag.HasErrors())
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		OutResult.Conflicts.Add({TEXT("target_blueprint_not_found"),
			FString::Printf(TEXT("蓝图资产未找到：%s"), *AssetPath),
			AssetPath, TEXT("target.asset_path")});
		return false;
	}

	return true;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightGraphTarget(
	UBlueprint* Blueprint, const FString& GraphName, EBlueprintHelperReplaceScope Scope,
	UEdGraph*& OutGraph, FReplacePreflightResult& OutResult) const
{
	// 函数图 (function_body)
	if (Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		for (UEdGraph* FnGraph : Blueprint->FunctionGraphs)
		{
			if (FnGraph && FnGraph->GetName() == GraphName)
			{
				OutGraph = FnGraph;
				return true;
			}
		}
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("target_function_not_found"));
		OutResult.Conflicts.Add({TEXT("target_function_not_found"),
			FString::Printf(TEXT("函数图 %s 未找到。"), *GraphName), GraphName, TEXT("target.graph")});
		return false;
	}

	// 事件图 (block_implementation / event_body / custom_event_body / graph)
	for (UEdGraph* UbergraphPage : Blueprint->UbergraphPages)
	{
		if (UbergraphPage && UbergraphPage->GetName() == GraphName)
		{
			OutGraph = UbergraphPage;
			return true;
		}
	}

	OutResult.bPassed = false;
	OutResult.BlockedBy.Add(TEXT("target_graph_not_found"));
	OutResult.Conflicts.Add({TEXT("target_graph_not_found"),
		FString::Printf(TEXT("图表 %s 未找到。"), *GraphName), GraphName, TEXT("target.graph")});
	return false;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightReplaceScope(
	EBlueprintHelperReplaceScope Scope, FReplacePreflightResult& OutResult) const
{
	if (Scope == EBlueprintHelperReplaceScope::FunctionDefinition ||
		Scope == EBlueprintHelperReplaceScope::EventDefinition)
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("replace_scope_unsupported"));
		OutResult.Conflicts.Add({TEXT("replace_scope_unsupported"),
			TEXT("该 replace_scope 的正式写入尚未实现。"), TEXT("target.replace_scope"), TEXT("payload")});
		return false;
	}
	return true;
}

// ─── DryRun ───

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::ExecuteDryRun(
	const FReplaceRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FReplacePreflightResult PreflightResult = Preflight(Request);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("replace_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef TargetRef;
	TargetRef.AssetPath = Request.AssetPath;
	TargetRef.TargetType = EBlueprintHelperTargetType::Graph;
	TargetRef.Graph = Request.GraphName;
	Result.Target = TargetRef;

	// 特殊：Replace dry_run target 不输出 target_type，但 include replace_scope
	// 通过直接设置 JSON 字段实现
	TargetRef.TargetType = EBlueprintHelperTargetType::None;

	if (PreflightResult.bPassed)
	{
		FBlueprintHelperReplaceDryRunData DryRunData;
		DryRunData.DryRun.Result = TEXT("passed");
		DryRunData.DryRun.bCanExecute = true;
		Result.Data = DryRunData.ToJson();
	}
	else
	{
		FBlueprintHelperReplaceDryRunData DryRunData;
		DryRunData.DryRun.Result = TEXT("blocked");
		DryRunData.DryRun.bCanExecute = false;
		DryRunData.DryRun.BlockedBy = PreflightResult.BlockedBy;
		DryRunData.DryRun.Conflicts = PreflightResult.Conflicts;
		DryRunData.DryRun.Errors = PreflightResult.Errors;

		const FBlueprintHelperGraphWriteIssue* FirstIssue = PreflightResult.Conflicts.Num() > 0
			? &PreflightResult.Conflicts[0]
			: (PreflightResult.Errors.Num() > 0 ? &PreflightResult.Errors[0] : nullptr);

		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Replace dry-run preflight blocked execution.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: TEXT("target.graph");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

		Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("replace_blueprint_graph"), TraceId, Error);
		Result.Target = TargetRef;
		Result.Data = DryRunData.ToJson();
	}

	return Result;
}

// ─── 正式写入 ───

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::ExecuteWrite(
	const FReplaceRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FString TransactionId = JournalService.GenerateTransactionId();

	// 1. Preflight
	FReplacePreflightResult PreflightResult = Preflight(Request);
	if (!PreflightResult.bPassed)
	{
		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = PreflightResult.Conflicts.Num() > 0
			? PreflightResult.Conflicts[0].Message : TEXT("Preflight 未通过。");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 2. 解析蓝图
	FBlueprintHelperGraphTarget BPTarget;
	BPTarget.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(BPTarget, Diag);
	if (!Blueprint)
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("target_blueprint_not_found");
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = FString::Printf(TEXT("蓝图 %s 未找到。"), *Request.AssetPath);
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 3. 解析替换目标
	FResolvedReplaceTarget Resolved;
	FString ResolveError;
	if (!ResolveReplaceTarget(Request, Blueprint, Resolved, ResolveError))
	{
		FBlueprintHelperToolError Error;
		Error.Code = ResolveError.Contains(TEXT("block")) ? TEXT("target_block_not_found")
			: (ResolveError.Contains(TEXT("function")) ? TEXT("target_function_not_found") : TEXT("target_not_found"));
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = ResolveError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 4. 捕获 before snapshot
	const FBlueprintHelperGraphSnapshot BeforeSnapshot = SnapshotService.CaptureNodeSnapshot(
		Resolved.Graph, Resolved.NodesToDelete);

	// 5. 开始修改
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Replace Blueprint Graph")), Blueprint);
	Mutation.Modify(Resolved.Graph);

	// 6. 删除旧实现
	if (!DeleteOldImplementation(Blueprint, Resolved.Graph, Resolved.NodesToDelete))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("node_create_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = TEXT("删除旧实现失败。");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	TSet<UEdGraphNode*> NodesBeforeImport;
	for (UEdGraphNode* Node : Resolved.Graph->Nodes)
	{
		if (Node)
		{
			NodesBeforeImport.Add(Node);
		}
	}

	// 7. 通过 AgentImportService 创建新节点/连线
	const FString ImportPayload = BuildAgentImportPayload(Request);

	FBlueprintHelperAgentImportRequest ImportReq;
	ImportReq.JsonText = ImportPayload;
	const FBlueprintHelperAgentImportResult ImportResult = AgentImportService.Import(ImportReq);

	if (!ImportResult.bSuccess)
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = ImportResult.ErrorCode.IsEmpty() ? TEXT("node_create_failed") : ImportResult.ErrorCode;
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ImportResult.Message.IsEmpty()
			? TEXT("替换实现创建失败。") : ImportResult.Message;
		Error.bRetryable = false;
		Error.RollbackResult = ImportResult.bRolledBack
			? EBlueprintHelperRollbackResult::RolledBack : EBlueprintHelperRollbackResult::Failed;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	FString ReconnectError;
	if (!ReconnectPreservedEntryToNewBody(Request, Resolved, NodesBeforeImport, ReconnectError))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("entry_reconnect_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ReconnectError.IsEmpty()
			? TEXT("替换实现后重建入口执行连线失败。") : ReconnectError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 8. 写入 ownership（如果目标为 owned block）
	if (Resolved.bIsBlueprintHelperOwned)
	{
		TArray<UEdGraphNode*> NewNodes;
		for (UEdGraphNode* Node : Resolved.Graph->Nodes)
		{
			if (Node && !NodesBeforeImport.Contains(Node))
			{
				NewNodes.Add(Node);
			}
		}

		if (NewNodes.Num() > 0)
		{
			FString OwnershipError;
			if (!OwnershipService.WriteBlockOwnership(
				Blueprint, NewNodes, Resolved.OriginalBlockId,
				TransactionId, TEXT("Replace"), OwnershipError))
			{
				Mutation.Rollback();

				FBlueprintHelperToolError Error;
				Error.Code = TEXT("ownership_write_failed");
				Error.Stage = EBlueprintHelperToolStage::Execute;
				Error.Message = OwnershipError;
				Error.bRetryable = false;
				Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
				return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
			}
		}
	}

	// 9. 写入 Journal
	FBlueprintHelperAppendJournalRecord JournalRecord;
	JournalRecord.TransactionId = TransactionId;
	JournalRecord.Tool = TEXT("ReplaceBlueprintGraph");
	JournalRecord.Status = TEXT("applied");
	JournalRecord.TargetAssets.Add(Request.AssetPath);
	JournalRecord.GraphId = Request.GraphName;
	JournalRecord.GraphName = Request.GraphName;
	JournalRecord.BlockIds.Add(Resolved.OriginalBlockId);
	JournalRecord.RollbackData = BeforeSnapshot.ToJsonString();

	FString JournalError;
	if (!JournalService.WriteAppendJournal(JournalRecord, JournalError))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("journal_write_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = JournalError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 10. 标记修改
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (Blueprint->GetOutermost())
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}
	Mutation.Commit();

	// 11. 成功结果
	FBlueprintHelperToolResultBase SuccessResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("replace_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef SuccessTarget;
	SuccessTarget.AssetPath = Request.AssetPath;
	SuccessTarget.TargetType = EBlueprintHelperTargetType::None; // 不输出 target_type
	SuccessTarget.Graph = Request.GraphName;
	SuccessResult.Target = SuccessTarget;

	FBlueprintHelperReplaceGraphResultData Data;
	Data.ReplaceResult.ReplacedRef.GraphId = Resolved.GraphId.IsEmpty() ? Request.GraphName : Resolved.GraphId;
	Data.ReplaceResult.ReplacedRef.TargetRef = Resolved.TargetRef;
	Data.WriteRef.TransactionId = TransactionId;
	Data.WriteRef.bJournalRecorded = true;
	SuccessResult.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = true;
	Validation.bShouldSave = true;
	SuccessResult.Validation = Validation;

	return SuccessResult;
}

// ─── 目标解析 ───

bool FBlueprintHelperReplaceBlueprintGraphService::ResolveReplaceTarget(
	const FReplaceRequest& Request, UBlueprint* Blueprint, FResolvedReplaceTarget& OutTarget, FString& OutError) const
{
	// 查找图表
	UEdGraph* Graph = nullptr;
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		for (UEdGraph* FnGraph : Blueprint->FunctionGraphs)
		{
			if (FnGraph && FnGraph->GetName() == Request.GraphName)
			{
				Graph = FnGraph;
				break;
			}
		}
	}
	else
	{
		for (UEdGraph* Page : Blueprint->UbergraphPages)
		{
			if (Page && Page->GetName() == Request.GraphName)
			{
				Graph = Page;
				break;
			}
		}
	}

	if (!Graph)
	{
		OutError = FString::Printf(TEXT("图表 %s 未找到。"), *Request.GraphName);
		return false;
	}

	OutTarget.Blueprint = Blueprint;
	OutTarget.Graph = Graph;
	OutTarget.Scope = Request.Scope;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = Request.GraphName;
	OutTarget.GraphId = Request.GraphName;

	if (Request.Scope == EBlueprintHelperReplaceScope::BlockImplementation)
	{
		return ResolveBlockImplementation(Graph, Request, OutTarget, OutError);
	}

	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		OutTarget.TargetRef = Request.GraphName;
		// 收集 body 节点（保留 FunctionEntry/Result）
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !Node->IsA<UK2Node_FunctionEntry>())
			{
				OutTarget.NodesToDelete.Add(Node);
			}
			else if (Node)
			{
				OutTarget.NodesToPreserve.Add(Node);
			}
		}
		OutTarget.bExternalDependentsMayBreak = false;
		return true;
	}

	// event_body / custom_event_body / graph: 回退到 block_implementation 语义
	if (Request.Scope == EBlueprintHelperReplaceScope::CustomEventBody ||
		Request.Scope == EBlueprintHelperReplaceScope::EventBody ||
		Request.Scope == EBlueprintHelperReplaceScope::Graph)
	{
		// 简化：删除所有非 entry 节点
		OutTarget.TargetRef = Request.EntryName.IsEmpty() ? Request.GraphName : Request.EntryName;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				bool bIsCustomEventNode = Node->GetClass()->GetName().Contains(TEXT("K2Node_CustomEvent"));
				bool bIsEventNode = Node->GetClass()->GetName().Contains(TEXT("K2Node_Event"));
				if (bIsCustomEventNode || bIsEventNode)
				{
					OutTarget.NodesToPreserve.Add(Node);
					if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::NodeMatchesEntryName(Node, Request.EntryName))
					{
						FString EntryBlockId;
						if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::TryReadBlueprintHelperBlockId(Node, EntryBlockId))
						{
							OutTarget.OriginalBlockId = EntryBlockId;
							OutTarget.OriginalBlockRef = Request.EntryName.IsEmpty() ? Request.GraphName : Request.EntryName;
							OutTarget.TargetRef = OutTarget.OriginalBlockRef;
							OutTarget.bIsBlueprintHelperOwned = true;
						}
					}
				}
				else
				{
					OutTarget.NodesToDelete.Add(Node);
				}
			}
		}
		return true;
	}

	OutError = TEXT("不支持的 replace_scope。");
	return false;
}

bool FBlueprintHelperReplaceBlueprintGraphService::ResolveBlockImplementation(
	UEdGraph* Graph, const FReplaceRequest& Request, FResolvedReplaceTarget& OutTarget, FString& OutError) const
{
	// 扫描图表中的 BlueprintHelper-owned 节点
	FString SearchBlockId = Request.BlockId;
	if (SearchBlockId.IsEmpty() && !Request.TargetRef.IsEmpty())
	{
		SearchBlockId = FString::Printf(TEXT("%s_%s"), *Request.GraphName, *Request.TargetRef);
	}

	TArray<UEdGraphNode*> OwnedNodes;
	FString FoundBlockRef;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		UPackage* Package = Node->GetOutermost();
		if (!Package) continue;

		FMetaData& MetaData = Package->GetMetaData();
		const FString OwnedStr = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
		if (OwnedStr != TEXT("true")) continue;

		const FString NodeBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		if (!SearchBlockId.IsEmpty() && NodeBlockId != SearchBlockId) continue;

		OwnedNodes.Add(Node);
		if (FoundBlockRef.IsEmpty())
		{
			// 从 block_id 提取 block_ref
			const FString Prefix = Request.GraphName + TEXT("_");
			if (NodeBlockId.StartsWith(Prefix))
			{
				FoundBlockRef = NodeBlockId.Mid(Prefix.Len());
			}
		}
	}

	if (OwnedNodes.Num() == 0)
	{
		if (SearchBlockId.IsEmpty())
		{
			OutError = TEXT("未找到任何 BlueprintHelper-owned 节点。请指定 selector.block_id 或 selector.target_ref。");
		}
		else
		{
			OutError = FString::Printf(TEXT("目标 block %s 未找到或不属于 BlueprintHelper。"), *SearchBlockId);
		}
		return false;
	}

	OutTarget.NodesToDelete = OwnedNodes;
	OutTarget.ExistingOwnedNodes = OwnedNodes;
	OutTarget.OriginalBlockId = SearchBlockId;
	OutTarget.OriginalBlockRef = FoundBlockRef.IsEmpty() ? Request.TargetRef : FoundBlockRef;
	OutTarget.TargetRef = FoundBlockRef.IsEmpty() ? Request.TargetRef : FoundBlockRef;
	OutTarget.bIsBlueprintHelperOwned = true;

	return true;
}

// ─── 删除旧实现 ───

bool FBlueprintHelperReplaceBlueprintGraphService::DeleteOldImplementation(
	UBlueprint* Blueprint, UEdGraph* Graph, const TArray<UEdGraphNode*>& NodesToDelete) const
{
	if (!Blueprint || !Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : NodesToDelete)
	{
		if (Node)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}

	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool FBlueprintHelperReplaceBlueprintGraphService::ReconnectPreservedEntryToNewBody(
	const FReplaceRequest& Request,
	const FResolvedReplaceTarget& Resolved,
	const TSet<UEdGraphNode*>& NodesBeforeImport,
	FString& OutError) const
{
	if (Request.Scope != EBlueprintHelperReplaceScope::FunctionBody &&
		Request.Scope != EBlueprintHelperReplaceScope::EventBody &&
		Request.Scope != EBlueprintHelperReplaceScope::CustomEventBody)
	{
		return true;
	}

	if (!Resolved.Graph)
	{
		OutError = TEXT("替换图表为空，无法重建入口执行连线。");
		return false;
	}

	UEdGraphNode* EntryNode = nullptr;
	for (UEdGraphNode* Node : Resolved.NodesToPreserve)
	{
		if (!Node)
		{
			continue;
		}

		const bool bMatchesScope =
			(Request.Scope == EBlueprintHelperReplaceScope::FunctionBody && Node->IsA<UK2Node_FunctionEntry>()) ||
			(Request.Scope != EBlueprintHelperReplaceScope::FunctionBody && FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(Node, EGPD_Output) != nullptr);
		if (bMatchesScope && FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::NodeMatchesEntryName(Node, Request.EntryName))
		{
			EntryNode = Node;
			break;
		}
	}

	if (!EntryNode)
	{
		OutError = Request.EntryName.IsEmpty()
			? TEXT("未找到可重连的保留入口节点。")
			: FString::Printf(TEXT("未找到可重连的保留入口节点：%s。"), *Request.EntryName);
		return false;
	}

	UEdGraphPin* EntryExecOut = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	UEdGraphNode* FirstBodyNode = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstImportedExecutableBodyNode(Resolved.Graph, NodesBeforeImport);
	if (!EntryExecOut)
	{
		OutError = TEXT("入口节点或替换 body 首节点缺少 Exec Pin。");
		return false;
	}

	if (!FirstBodyNode)
	{
		if (EntryExecOut->LinkedTo.Num() > 0)
		{
			FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(EntryExecOut);
			Resolved.Graph->NotifyGraphChanged();
		}
		return true;
	}

	UEdGraphPin* BodyExecIn = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(FirstBodyNode, EGPD_Input);
	if (!BodyExecIn)
	{
		OutError = TEXT("Replacement body first node is missing an Exec input pin.");
		return false;
	}

	if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
	{
		return true;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutError = TEXT("K2 schema is unavailable; cannot rebuild entry exec link.");
		return false;
	}

	FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(EntryExecOut);
	FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(BodyExecIn);
	if (!Schema->TryCreateConnection(EntryExecOut, BodyExecIn) ||
		!FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
	{
		OutError = FString::Printf(TEXT("无法连接入口 %s 到替换 body 首节点 %s。"),
			*EntryNode->GetName(), *FirstBodyNode->GetName());
		return false;
	}

	Resolved.Graph->NotifyGraphChanged();
	return true;
}

// ─── AgentImport payload 构建 ───

FString FBlueprintHelperReplaceBlueprintGraphService::BuildAgentImportPayload(
	const FReplaceRequest& Request) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.AgentImportGraph"));
	Root->SetStringField(TEXT("version"), TEXT("1.0"));
	Root->SetStringField(TEXT("target_blueprint"), Request.AssetPath);
	Root->SetStringField(TEXT("target_graph"), Request.GraphName);
	Root->SetStringField(TEXT("mode"), TEXT("append"));
	Root->SetStringField(TEXT("layout"), TEXT("auto"));

	TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetBoolField(TEXT("compile"), false);
	Options->SetBoolField(TEXT("save"), false);
	Options->SetBoolField(TEXT("strict"), true);
	Options->SetBoolField(TEXT("dry_run"), false);
	Options->SetBoolField(TEXT("create_missing_variables"), false);
	Options->SetBoolField(TEXT("reconstruct_existing_nodes"), false);
	Root->SetObjectField(TEXT("options"), Options);

	TArray<TSharedPtr<FJsonValue>> NodesCopy;
	for (const TSharedPtr<FJsonValue>& Node : Request.Nodes)
	{
		NodesCopy.Add(Node);
	}
	Root->SetArrayField(TEXT("nodes"), NodesCopy);

	TArray<TSharedPtr<FJsonValue>> LinksCopy;
	for (const TSharedPtr<FJsonValue>& Link : Request.Links)
	{
		LinksCopy.Add(Link);
	}
	Root->SetArrayField(TEXT("links"), LinksCopy);

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Root, Writer);
	return JsonText;
}
