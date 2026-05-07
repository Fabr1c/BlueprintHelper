// BlueprintHelper Service Layer — AppendBlueprintGraph 核心服务实现

#include "Services/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "GraphSupport/BlueprintHelperBlockIdService.h"
#include "GraphSupport/BlueprintHelperOwnershipService.h"
#include "Transactions/Transactions/BlueprintHelperTransactionJournalService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// ─── 禁止创建的全局事件名称集合 ───

namespace
{
	const TSet<FString>& ForbiddenEventNames()
	{
		static const TSet<FString> Names = {
			TEXT("BeginPlay"),
			TEXT("Tick"),
			TEXT("ConstructionScript"),
			TEXT("ReceiveBeginPlay"),
			TEXT("ReceiveTick"),
			TEXT("UserConstructionScript"),
			TEXT("BndEvt__"),
			TEXT("InpAct_"),
			TEXT("OnComponentBeginOverlap"),
			TEXT("OnComponentEndOverlap"),
			TEXT("OnComponentHit"),
			TEXT("OnComponentWake"),
			TEXT("OnComponentSleep")
		};
		return Names;
	}

	bool LooksLikeGlobalEvent(const FString& Name)
	{
		for (const FString& Forbidden : ForbiddenEventNames())
		{
			if (Name.StartsWith(Forbidden) || Name.Equals(Forbidden, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	UK2Node_CustomEvent* FindExistingCustomEventNode(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph || EventName.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
			if (CustomEvent && CustomEvent->CustomFunctionName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
			{
				return CustomEvent;
			}
		}
		return nullptr;
	}

	TSet<UEdGraphNode*> CaptureGraphNodes(UEdGraph* Graph)
	{
		TSet<UEdGraphNode*> Nodes;
		if (!Graph)
		{
			return Nodes;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				Nodes.Add(Node);
			}
		}
		return Nodes;
	}
}

// ─── 构造 ───

FBlueprintHelperAppendBlueprintGraphService::FBlueprintHelperAppendBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperAgentImportService& InAgentImportService,
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperOwnershipService& InOwnershipService,
	const FBlueprintHelperTransactionJournalService& InJournalService)
	: Resolver(InResolver)
	, AgentImportService(InAgentImportService)
	, BlockIdService(InBlockIdService)
	, OwnershipService(InOwnershipService)
	, JournalService(InJournalService)
{
}

// ─── 公共入口 ───

FBlueprintHelperToolResultBase FBlueprintHelperAppendBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FAppendRequest Request = ParseRequest(Payload);

	if (Request.bDryRun)
	{
		return ExecuteDryRun(Request);
	}

	return ExecuteWrite(Request);
}

// ─── 解析 ───

FBlueprintHelperAppendBlueprintGraphService::FAppendRequest
FBlueprintHelperAppendBlueprintGraphService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FAppendRequest Request;

	if (!Payload.IsValid())
	{
		return Request;
	}

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName);
	}

	Payload->TryGetStringField(TEXT("feature_name"), Request.FeatureName);
	Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
	Payload->TryGetBoolField(TEXT("reuse_existing_entries"), Request.bReuseExistingEntries);

	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		Request.Nodes = *NodesArray;
	}

	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("links"), LinksArray))
	{
		Request.Links = *LinksArray;
	}

	return Request;
}

// ─── Preflight ───

FBlueprintHelperAppendBlueprintGraphService::FAppendPreflightResult
FBlueprintHelperAppendBlueprintGraphService::Preflight(const FAppendRequest& Request) const
{
	FAppendPreflightResult Result;

	// 1. 检查 asset_path 存在
	if (Request.AssetPath.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		Result.Conflicts.Add({TEXT("target_blueprint_not_found"),
			TEXT("缺少 target.asset_path。"), TEXT("target.asset_path"), TEXT("payload")});
		return Result;
	}

	// 2. 检查蓝图
	UBlueprint* Blueprint = nullptr;
	if (!PreflightBlueprint(Request.AssetPath, Blueprint, Result))
	{
		return Result;
	}

	// 3. 检查图表
	UEdGraph* Graph = nullptr;
	if (Request.GraphName.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("missing_graph_name"));
		Result.Conflicts.Add({TEXT("missing_graph_name"),
			TEXT("缺少 target.graph。"), TEXT("target.graph"), TEXT("payload")});
		return Result;
	}

	if (!PreflightGraphTarget(Blueprint, Request, Graph, Result))
	{
		return Result;
	}

	// 4. 检查节点
	if (!PreflightNodePayload(Request, Graph, Result))
	{
		return Result;
	}

	return Result;
}

bool FBlueprintHelperAppendBlueprintGraphService::PreflightBlueprint(
	const FString& AssetPath,
	UBlueprint*& OutBlueprint,
	FAppendPreflightResult& OutResult) const
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

bool FBlueprintHelperAppendBlueprintGraphService::PreflightGraphTarget(
	UBlueprint* Blueprint,
	const FAppendRequest& Request,
	UEdGraph*& OutGraph,
	FAppendPreflightResult& OutResult) const
{
	const FString& GraphName = Request.GraphName;
	// 检查 FunctionGraphs / MacroGraphs 中是否存在同名图表
	for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
	{
		if (FunctionGraph && FunctionGraph->GetName() == GraphName)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("target_graph_type_invalid"));
			OutResult.Conflicts.Add({TEXT("target_graph_type_invalid"),
				FString::Printf(TEXT("图表 %s 已作为函数图存在，Append 不允许写入函数图。"), *GraphName),
				GraphName, TEXT("target.graph")});
			return false;
		}
	}

	for (UEdGraph* MacroGraph : Blueprint->MacroGraphs)
	{
		if (MacroGraph && MacroGraph->GetName() == GraphName)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("target_graph_type_invalid"));
			OutResult.Conflicts.Add({TEXT("target_graph_type_invalid"),
				FString::Printf(TEXT("图表 %s 已作为宏图存在，Append 不允许写入宏图。"), *GraphName),
				GraphName, TEXT("target.graph")});
			return false;
		}
	}

	// 在 UbergraphPages 中查找
	for (UEdGraph* UbergraphPage : Blueprint->UbergraphPages)
	{
		if (UbergraphPage && UbergraphPage->GetName() == GraphName)
		{
			OutGraph = UbergraphPage;

			// 空图表允许写入
			if (OutGraph->Nodes.Num() == 0)
			{
				return true;
			}

			if (Request.bReuseExistingEntries)
			{
				return true;
			}

			// 非空图表拒绝
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("target_graph_not_empty"));
			OutResult.Conflicts.Add({TEXT("target_graph_not_empty"),
				FString::Printf(TEXT("图表 %s 非空，Append 不允许写入已有内容的图表。"), *GraphName),
				GraphName, TEXT("target.graph")});
			return false;
		}
	}

	// 不存在 — 允许创建
	OutGraph = nullptr;
	return true;
}

bool FBlueprintHelperAppendBlueprintGraphService::PreflightNodePayload(
	const FAppendRequest& Request,
	UEdGraph* Graph,
	FAppendPreflightResult& OutResult) const
{
	if (Request.Nodes.Num() == 0)
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("empty_nodes"));
		OutResult.Conflicts.Add({TEXT("empty_nodes"),
			TEXT("nodes 数组不能为空。"), TEXT("nodes"), TEXT("payload")});
		return false;
	}

	// 检查 forbidden event kinds
	const TArray<FString> CustomEventNames = ExtractCustomEventNames(Request);
	TSet<FString> SeenNames;

	for (const TSharedPtr<FJsonValue>& NodeValue : Request.Nodes)
	{
		const TSharedPtr<FJsonObject> NodeObject = NodeValue.IsValid() ? NodeValue->AsObject() : nullptr;
		if (!NodeObject.IsValid())
		{
			continue;
		}

		FString Kind;
		NodeObject->TryGetStringField(TEXT("kind"), Kind);

		if (Kind.Equals(TEXT("event"), ESearchCase::IgnoreCase))
		{
			FString EventName;
			NodeObject->TryGetStringField(TEXT("event"), EventName);
			if (EventName.IsEmpty())
			{
				NodeObject->TryGetStringField(TEXT("event_name"), EventName);
			}

			if (LooksLikeGlobalEvent(EventName))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("global_event_creation_disallowed"));
				OutResult.Conflicts.Add({TEXT("global_event_creation_disallowed"),
					FString::Printf(TEXT("不允许创建全局事件节点：%s。Append 只能创建 Custom Event。"), *EventName),
					EventName, TEXT("nodes[].event")});
			}
		}

		if (Kind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
		{
			FString Name;
			NodeObject->TryGetStringField(TEXT("name"), Name);

			if (LooksLikeGlobalEvent(Name))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("global_event_creation_disallowed"));
				OutResult.Conflicts.Add({TEXT("global_event_creation_disallowed"),
					FString::Printf(TEXT("不允许创建全局事件节点：%s。"), *Name),
					Name, TEXT("nodes[].name")});
			}

			if (SeenNames.Contains(Name))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("custom_event_already_exists"));
				OutResult.Conflicts.Add({TEXT("custom_event_already_exists"),
					FString::Printf(TEXT("Custom Event 名称重复：%s。"), *Name),
					Name, TEXT("nodes[].name")});
			}
			SeenNames.Add(Name);

			if (Request.bReuseExistingEntries && !Request.bDryRun && !FindExistingCustomEventNode(Graph, Name))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("custom_event_entry_not_found"));
				OutResult.Conflicts.Add({TEXT("custom_event_entry_not_found"),
					FString::Printf(TEXT("Custom Event '%s' must already exist when reuse_existing_entries is enabled."), *Name),
					Name, TEXT("nodes[].name")});
			}
		}
	}

	return OutResult.bPassed;
}

// ─── 图表创建/查找 ───

UEdGraph* FBlueprintHelperAppendBlueprintGraphService::FindOrCreateAppendGraph(
	UBlueprint* Blueprint,
	const FString& GraphName,
	FString& OutError) const
{
	if (!Blueprint)
	{
		OutError = TEXT("蓝图为空。");
		return nullptr;
	}

	// 1. 在 UbergraphPages 中查找
	for (UEdGraph* Page : Blueprint->UbergraphPages)
	{
		if (Page && Page->GetName() == GraphName)
		{
			return Page;
		}
	}

	// 2. 确认不在 FunctionGraphs / MacroGraphs 中
	for (UEdGraph* Fn : Blueprint->FunctionGraphs)
	{
		if (Fn && Fn->GetName() == GraphName)
		{
			OutError = TEXT("target_graph_type_invalid：同名函数图已存在。");
			return nullptr;
		}
	}
	for (UEdGraph* Macro : Blueprint->MacroGraphs)
	{
		if (Macro && Macro->GetName() == GraphName)
		{
			OutError = TEXT("target_graph_type_invalid：同名宏图已存在。");
			return nullptr;
		}
	}

	// 3. 创建新事件图
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*GraphName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());

	if (!NewGraph)
	{
		OutError = TEXT("无法创建新图表。");
		return nullptr;
	}

	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	return NewGraph;
}

bool FBlueprintHelperAppendBlueprintGraphService::IsEventGraph(UEdGraph* Graph) const
{
	if (!Graph)
	{
		return false;
	}

	const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!Blueprint)
	{
		return false;
	}

	for (UEdGraph* Page : Blueprint->UbergraphPages)
	{
		if (Page == Graph)
		{
			return true;
		}
	}

	return false;
}

// ─── DryRun 执行 ───

FBlueprintHelperToolResultBase FBlueprintHelperAppendBlueprintGraphService::ExecuteDryRun(
	const FAppendRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FAppendPreflightResult PreflightResult = Preflight(Request);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("append_blueprint_graph"), TraceId);

	// Target
	FBlueprintHelperTargetRef TargetRef;
	TargetRef.AssetPath = Request.AssetPath;
	TargetRef.TargetType = EBlueprintHelperTargetType::Graph;
	TargetRef.Graph = Request.GraphName;
	Result.Target = TargetRef;

	if (PreflightResult.bPassed)
	{
		FBlueprintHelperAppendDryRunData DryRunData;
		DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Passed;
		DryRunData.DryRun.bCanExecute = true;
		Result.Data = DryRunData.ToJson();
	}
	else
	{
		FBlueprintHelperAppendDryRunData DryRunData;
		DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Blocked;
		DryRunData.DryRun.bCanExecute = false;
		DryRunData.DryRun.BlockedBy = PreflightResult.BlockedBy;
		DryRunData.DryRun.Conflicts = PreflightResult.Conflicts;
		DryRunData.DryRun.Errors = PreflightResult.Errors;

		const FBlueprintHelperDryRunIssue* FirstIssue = PreflightResult.Conflicts.Num() > 0
			? &PreflightResult.Conflicts[0]
			: (PreflightResult.Errors.Num() > 0 ? &PreflightResult.Errors[0] : nullptr);

		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Append dry-run preflight blocked execution.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: TEXT("target.graph");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

		Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("append_blueprint_graph"), TraceId, Error);
		Result.Target = TargetRef;
		Result.Data = DryRunData.ToJson();
	}

	return Result;
}

// ─── 正式写入 ───

FBlueprintHelperToolResultBase FBlueprintHelperAppendBlueprintGraphService::ExecuteWrite(
	const FAppendRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FString TransactionId = JournalService.GenerateTransactionId();

	// 1. Preflight
	FAppendPreflightResult PreflightResult = Preflight(Request);
	if (!PreflightResult.bPassed)
	{
		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = PreflightResult.Conflicts.Num() > 0
			? PreflightResult.Conflicts[0].Message : TEXT("Preflight 未通过。");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("append_blueprint_graph"), TraceId, Error);

		FBlueprintHelperTargetRef FailTarget;
		FailTarget.AssetPath = Request.AssetPath;
		FailTarget.TargetType = EBlueprintHelperTargetType::Graph;
		FailTarget.Graph = Request.GraphName;
		FailResult.Target = FailTarget;

		return FailResult;
	}

	// 2. 解析蓝图
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diag);
	if (!Blueprint)
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("target_blueprint_not_found");
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = FString::Printf(TEXT("蓝图 %s 未找到。"), *Request.AssetPath);
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("append_blueprint_graph"), TraceId, Error);
	}

	// 3. 快照 + 开始事务
	const FString SnapshotJson = BuildAgentImportPayload(Request);

	FBlueprintHelperAgentImportRequest ImportReq;
	ImportReq.JsonText = SnapshotJson;

	// 4. 查找/创建目标图表
	FString GraphError;
	UEdGraph* TargetGraph = FindOrCreateAppendGraph(Blueprint, Request.GraphName, GraphError);
	if (!TargetGraph)
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("node_create_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = GraphError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("append_blueprint_graph"), TraceId, Error);
	}
	const TSet<UEdGraphNode*> NodeSnapshot = CaptureGraphNodes(TargetGraph);

	// 5. 通过 AgentImportService 执行节点/连线创建
	const FBlueprintHelperAgentImportResult ImportResult = AgentImportService.Import(ImportReq);

	if (!ImportResult.bSuccess)
	{
		// 清理可能半成品的新图表
		if (TargetGraph->Nodes.Num() == 0)
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, TargetGraph);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}

		FBlueprintHelperToolError Error;
		Error.Code = ImportResult.ErrorCode.IsEmpty() ? TEXT("node_create_failed") : ImportResult.ErrorCode;
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ImportResult.Message.IsEmpty()
			? TEXT("Agent 导入执行失败。") : ImportResult.Message;
		Error.bRetryable = false;
		Error.RollbackResult = ImportResult.bRolledBack
			? EBlueprintHelperRollbackResult::RolledBack : EBlueprintHelperRollbackResult::Failed;

		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("append_blueprint_graph"), TraceId, Error);

		FBlueprintHelperTargetRef FailTarget;
		FailTarget.AssetPath = Request.AssetPath;
		FailTarget.TargetType = EBlueprintHelperTargetType::Graph;
		FailTarget.Graph = Request.GraphName;
		FailResult.Target = FailTarget;

		return FailResult;
	}

	// 6. 分组并为节点写入 ownership
	TArray<FString> BlockRefs;
	const TArray<FString> EntryNames = ExtractCustomEventNames(Request);
	TArray<UEdGraphNode*> CreatedNodes;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node && !NodeSnapshot.Contains(Node))
		{
			CreatedNodes.Add(Node);
		}
	}
	if (Request.bReuseExistingEntries)
	{
		for (const FString& EntryName : EntryNames)
		{
			if (UK2Node_CustomEvent* ExistingEntry = FindExistingCustomEventNode(TargetGraph, EntryName))
			{
				CreatedNodes.AddUnique(ExistingEntry);
			}
		}
	}

	for (const FString& EntryName : EntryNames)
	{
		const FString BlockRef = BlockIdService.MakeBlockRef(Blueprint, TargetGraph, EntryName);
		const FString FullBlockId = BlockIdService.MakeFullBlockId(Request.GraphName, BlockRef);
		BlockRefs.Add(BlockRef);

		// 为每个 block 关联的节点写入 ownership
		// 第一版简化：所有新节点归属到第一个 block
		FString OwnershipError;
		if (!OwnershipService.WriteBlockOwnership(
			Blueprint, CreatedNodes, FullBlockId, TransactionId, Request.FeatureName, OwnershipError))
		{
			// Ownership 写入失败 → 回滚
			for (UEdGraphNode* Node : CreatedNodes)
			{
				if (Node)
				{
					FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
				}
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

			FBlueprintHelperToolError Error;
			Error.Code = TEXT("ownership_write_failed");
			Error.Stage = EBlueprintHelperToolStage::Execute;
			Error.Message = OwnershipError;
			Error.bRetryable = false;
			Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
			return FBlueprintHelperToolResultBuilder::Failure(TEXT("append_blueprint_graph"), TraceId, Error);
		}
	}

	// 7. 写入 Journal
	FBlueprintHelperAppendJournalRecord JournalRecord;
	JournalRecord.TransactionId = TransactionId;
	JournalRecord.Status = TEXT("applied");
	JournalRecord.TargetAssets.Add(Request.AssetPath);
	JournalRecord.GraphId = Request.GraphName;
	JournalRecord.GraphName = Request.GraphName;
	JournalRecord.BlockIds = BlockRefs;
	for (UEdGraphNode* Node : CreatedNodes)
	{
		if (Node)
		{
			JournalRecord.CreatedNodePaths.Add(FString::Printf(TEXT("/%s"), *Node->GetPathName()));
		}
	}

	FString JournalError;
	if (!JournalService.WriteAppendJournal(JournalRecord, JournalError))
	{
		// Journal 写入失败 → 回滚
		for (UEdGraphNode* Node : CreatedNodes)
		{
			if (Node)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("journal_write_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = JournalError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("append_blueprint_graph"), TraceId, Error);
	}

	// 8. 标记修改
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (Blueprint->GetOutermost())
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	// 9. 构造成功结果
	FBlueprintHelperToolResultBase SuccessResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("append_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef SuccessTarget;
	SuccessTarget.AssetPath = Request.AssetPath;
	SuccessTarget.TargetType = EBlueprintHelperTargetType::Graph;
	SuccessTarget.Graph = Request.GraphName;
	SuccessResult.Target = SuccessTarget;

	FBlueprintHelperAppendGraphResultData Data;
	Data.AppendResult.Graph.GraphId = Request.GraphName;
	Data.AppendResult.Graph.GraphName = Request.GraphName;
	Data.AppendResult.BlockRefs = BlockRefs;
	Data.WriteRef.TransactionId = TransactionId;
	Data.WriteRef.bJournalRecorded = true;
	SuccessResult.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = true;
	Validation.bShouldSave = true;
	SuccessResult.Validation = Validation;

	return SuccessResult;
}

// ─── AgentImport 兼容 payload 构建 ───

FString FBlueprintHelperAppendBlueprintGraphService::BuildAgentImportPayload(
	const FAppendRequest& Request) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.AgentImportGraph"));
	Root->SetStringField(TEXT("version"), TEXT("1.0"));
	Root->SetStringField(TEXT("target_blueprint"), Request.AssetPath);
	Root->SetStringField(TEXT("target_graph"), Request.GraphName);
	Root->SetStringField(TEXT("mode"), TEXT("append"));
	Root->SetStringField(TEXT("layout"), TEXT("auto"));

	// options
	TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetBoolField(TEXT("compile"), false);
	Options->SetBoolField(TEXT("save"), false);
	Options->SetBoolField(TEXT("strict"), true);
	Options->SetBoolField(TEXT("dry_run"), false);
	Options->SetBoolField(TEXT("create_missing_variables"), false);
	Options->SetBoolField(TEXT("reconstruct_existing_nodes"), Request.bReuseExistingEntries);
	Root->SetObjectField(TEXT("options"), Options);

	// nodes
	TArray<TSharedPtr<FJsonValue>> NodesCopy;
	for (const TSharedPtr<FJsonValue>& Node : Request.Nodes)
	{
		NodesCopy.Add(Node);
	}
	Root->SetArrayField(TEXT("nodes"), NodesCopy);

	// links
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

// ─── Helpers ───

bool FBlueprintHelperAppendBlueprintGraphService::IsForbiddenEventKind(
	const FString& Kind, const FString& EventName) const
{
	if (!Kind.Equals(TEXT("event"), ESearchCase::IgnoreCase) &&
		!Kind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	return LooksLikeGlobalEvent(EventName);
}

TArray<FString> FBlueprintHelperAppendBlueprintGraphService::ExtractCustomEventNames(
	const FAppendRequest& Request) const
{
	TArray<FString> Names;

	for (const TSharedPtr<FJsonValue>& NodeValue : Request.Nodes)
	{
		const TSharedPtr<FJsonObject> NodeObject = NodeValue.IsValid() ? NodeValue->AsObject() : nullptr;
		if (!NodeObject.IsValid())
		{
			continue;
		}

		FString Kind;
		NodeObject->TryGetStringField(TEXT("kind"), Kind);

		if (Kind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
		{
			FString Name;
			NodeObject->TryGetStringField(TEXT("name"), Name);
			if (!Name.IsEmpty())
			{
				Names.Add(Name);
			}
		}
	}

	return Names;
}
