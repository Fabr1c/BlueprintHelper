// BlueprintHelper Service Layer — AppendBlueprintGraph 核心服务实现

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

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

class FBlueprintHelperAppendBlueprintGraphServiceLocalUtils
{
public:
	static const TSet<FString>& ForbiddenEventNames()
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

		static bool LooksLikeGlobalEvent(const FString& Name)
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

	static UK2Node_CustomEvent* FindExistingCustomEventNode(UEdGraph* Graph, const FString& EventName)
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

	static TSet<UEdGraphNode*> CaptureGraphNodes(UEdGraph* Graph)
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

	static FBlueprintHelperGraphReviewNodeAnchor MakeReviewNodeAnchor(const UEdGraphNode* Node)
	{
		FBlueprintHelperGraphReviewNodeAnchor Anchor;
		if (!Node)
		{
			return Anchor;
		}

		Anchor.NodePath = Node->GetPathName();
		Anchor.NodeGuid = Node->NodeGuid.IsValid()
			? Node->NodeGuid.ToString(EGuidFormats::Digits)
			: FString();
		Anchor.DisplayLabel = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		if (Anchor.DisplayLabel.IsEmpty())
		{
			Anchor.DisplayLabel = Node->GetName();
		}
		Anchor.GraphPosition = FVector2D(
			static_cast<float>(Node->NodePosX),
			static_cast<float>(Node->NodePosY));
		Anchor.GraphSize = FVector2D(
			Node->NodeWidth > 0 ? static_cast<float>(Node->NodeWidth) : 360.0f,
			Node->NodeHeight > 0 ? static_cast<float>(Node->NodeHeight) : 180.0f);
		Anchor.bHasGraphBounds = true;
		return Anchor;
	}

};

// ─── 构造 ───

FBlueprintHelperAppendBlueprintGraphService::FBlueprintHelperAppendBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperOwnershipService& InOwnershipService)
	: Resolver(InResolver)
	, BlockIdService(InBlockIdService)
	, OwnershipService(InOwnershipService)
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
	Payload->TryGetBoolField(TEXT("allow_existing_graph"), Request.bAllowExistingGraph);

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject && LogicSpecObject->IsValid())
	{
		Request.LogicSpec = *LogicSpecObject;
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
	if (!PreflightNodePayload(Request, Blueprint, Graph, Result))
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

			if (Request.bReuseExistingEntries || Request.bAllowExistingGraph)
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
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	FAppendPreflightResult& OutResult) const
{
	if (!Request.LogicSpec.IsValid())
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("logic_spec_required"));
		OutResult.Conflicts.Add({TEXT("logic_spec_required"),
			TEXT("append_blueprint_graph requires logic_spec/SemanticIR input."), TEXT("logic_spec"), TEXT("payload")});
		return false;
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
	if (!OutResult.bPassed)
	{
		return false;
	}

	TSet<FString> SeenNames;
	for (const FString& Name : ExtractCustomEventNames(Request))
	{
		if (FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::LooksLikeGlobalEvent(Name))
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("global_event_creation_disallowed"));
			OutResult.Conflicts.Add({TEXT("global_event_creation_disallowed"), FString::Printf(TEXT("Global event creation is not allowed in append_blueprint_graph: %s."), *Name), Name, TEXT("logic_spec.entry.name")});
		}
		if (SeenNames.Contains(Name))
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("custom_event_already_exists"));
			OutResult.Conflicts.Add({TEXT("custom_event_already_exists"), FString::Printf(TEXT("Custom Event name is duplicated: %s."), *Name), Name, TEXT("logic_spec.entry.name")});
		}
		SeenNames.Add(Name);
		if (Graph && Request.bReuseExistingEntries && !Request.bDryRun && !FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::FindExistingCustomEventNode(Graph, Name))
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("custom_event_entry_not_found"));
			OutResult.Conflicts.Add({TEXT("custom_event_entry_not_found"), FString::Printf(TEXT("Custom Event '%s' must already exist when reuse_existing_entries is enabled."), *Name), Name, TEXT("logic_spec.entry.name")});
		}
	}
	return OutResult.bPassed;
}

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
		FBlueprintHelperGraphTarget GraphTarget;
		GraphTarget.BlueprintPath = Request.AssetPath;
		FBlueprintHelperDiagnosticSet Diag;
		UBlueprint* Blueprint = Resolver.ResolveBlueprint(GraphTarget, Diag);
		if (!Blueprint)
		{
			FBlueprintHelperAppendDryRunData DryRunData;
			DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Blocked;
			DryRunData.DryRun.bCanExecute = false;
			DryRunData.DryRun.BlockedBy.Add(TEXT("target_blueprint_not_found"));
			DryRunData.DryRun.Errors.Add({TEXT("target_blueprint_not_found"),
				FString::Printf(TEXT("Blueprint %s was not found."), *Request.AssetPath),
				Request.AssetPath,
				TEXT("target.asset_path")});

			FBlueprintHelperToolError Error;
			Error.Code = TEXT("target_blueprint_not_found");
			Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
			Error.Message = FString::Printf(TEXT("Blueprint %s was not found."), *Request.AssetPath);
			Error.Field = TEXT("target.asset_path");
			Error.bRetryable = false;
			Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

			Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("append_blueprint_graph"), TraceId, Error);
			Result.Target = TargetRef;
			Result.Data = DryRunData.ToJson();
		}
		else
		{
			UEdGraph* ExistingGraph = nullptr;
			for (UEdGraph* Page : Blueprint->UbergraphPages)
			{
				if (Page && Page->GetName() == Request.GraphName)
				{
					ExistingGraph = Page;
					break;
				}
			}

			const bool bGraphExisted = ExistingGraph != nullptr;
			const bool bPackageWasDirty = Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false;
			FString GraphError;
			UEdGraph* PreviewGraph = FindOrCreateAppendGraph(Blueprint, Request.GraphName, GraphError);
			if (!PreviewGraph)
			{
				FBlueprintHelperAppendDryRunData DryRunData;
				DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Blocked;
				DryRunData.DryRun.bCanExecute = false;
				DryRunData.DryRun.BlockedBy.Add(TEXT("preview_graph_create_failed"));
				DryRunData.DryRun.Errors.Add({TEXT("preview_graph_create_failed"), GraphError, Request.GraphName, TEXT("target.graph")});

				FBlueprintHelperToolError Error;
				Error.Code = TEXT("preview_graph_create_failed");
				Error.Stage = EBlueprintHelperToolStage::Preflight;
				Error.Message = GraphError;
				Error.Field = TEXT("target.graph");
				Error.bRetryable = false;
				Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

				Result = FBlueprintHelperToolResultBuilder::Failure(
					TEXT("append_blueprint_graph"), TraceId, Error);
				Result.Target = TargetRef;
				Result.Data = DryRunData.ToJson();
			}
			else
			{
				const TSet<UEdGraphNode*> NodeSnapshot = FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::CaptureGraphNodes(PreviewGraph);
				const FString GraphWritePayload = BuildSemanticGraphWritePayload(Request);
				TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
				const FBlueprintGenerateResult GenerateResult =
					FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(PreviewGraph, GraphWritePayload, UnresolvedNodes);

				if (!bGraphExisted)
				{
					FBlueprintEditorUtils::RemoveGraph(Blueprint, PreviewGraph);
					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
				}
				else
				{
					TArray<UEdGraphNode*> NodesToRemove;
					for (UEdGraphNode* Node : PreviewGraph->Nodes)
					{
						if (Node && !NodeSnapshot.Contains(Node))
						{
							NodesToRemove.Add(Node);
						}
					}
					for (UEdGraphNode* Node : NodesToRemove)
					{
						FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
					}
				}
				if (Blueprint->GetOutermost())
				{
					Blueprint->GetOutermost()->SetDirtyFlag(bPackageWasDirty);
				}

				if (GenerateResult.bSucceed)
				{
					FBlueprintHelperAppendDryRunData DryRunData;
					DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Passed;
					DryRunData.DryRun.bCanExecute = true;
					Result.Data = DryRunData.ToJson();
				}
				else
				{
					FString ImportMessage = GenerateResult.Message;
					if (UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
					{
						ImportMessage += FString::Printf(TEXT(" First unresolved: %s - %s"), *UnresolvedNodes[0]->DisplayText, *UnresolvedNodes[0]->Reason);
					}

					FBlueprintHelperAppendDryRunData DryRunData;
					DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Blocked;
					DryRunData.DryRun.bCanExecute = false;
					DryRunData.DryRun.BlockedBy.Add(TEXT("semantic_graph_write_failed"));
					FBlueprintHelperDryRunIssue SemanticIssue{
						TEXT("semantic_graph_write_failed"),
						ImportMessage,
						TEXT("logic_spec"),
						TEXT("logic_spec")
					};
					for (const TSharedPtr<FUnresolvedNodeItem>& UnresolvedNode : UnresolvedNodes)
					{
						if (!UnresolvedNode.IsValid())
						{
							continue;
						}
						for (const FBlueprintHelperCandidateFunctionGroup& Group : UnresolvedNode->CandidateFunctions)
						{
							FBlueprintHelperDryRunCandidateFunctionGroup DryRunGroup;
							DryRunGroup.Target = Group.Target;
							DryRunGroup.Candidates = Group.Candidates;
							SemanticIssue.CandidateFunctions.Add(MoveTemp(DryRunGroup));
						}
					}
					DryRunData.DryRun.Errors.Add(MoveTemp(SemanticIssue));

					FBlueprintHelperToolError Error;
					Error.Code = TEXT("semantic_graph_write_failed");
					Error.Stage = EBlueprintHelperToolStage::Preflight;
					Error.Message = ImportMessage;
					Error.Field = TEXT("logic_spec");
					Error.bRetryable = false;
					Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

					Result = FBlueprintHelperToolResultBuilder::Failure(
						TEXT("append_blueprint_graph"), TraceId, Error);
					Result.Target = TargetRef;
					Result.Data = DryRunData.ToJson();
				}
			}
		}
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

	// 1. Preflight
	FAppendPreflightResult PreflightResult = Preflight(Request);
	if (!PreflightResult.bPassed)
	{
		const FBlueprintHelperDryRunIssue* FirstIssue = PreflightResult.Conflicts.Num() > 0
			? &PreflightResult.Conflicts[0]
			: (PreflightResult.Errors.Num() > 0 ? &PreflightResult.Errors[0] : nullptr);

		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Preflight failed.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: TEXT("target.graph");
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
	const FString GraphWritePayload = BuildSemanticGraphWritePayload(Request);


	// 4. 查找/创建目标图表
	UEdGraph* ExistingGraph = nullptr;
	for (UEdGraph* Page : Blueprint->UbergraphPages)
	{
		if (Page && Page->GetName() == Request.GraphName)
		{
			ExistingGraph = Page;
			break;
		}
	}
	const bool bGraphExistedBeforeWrite = ExistingGraph != nullptr;
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
	const TSet<UEdGraphNode*> NodeSnapshot = FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::CaptureGraphNodes(TargetGraph);

	// 5. Create graph nodes through the SemanticIR pipeline.
	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
	const FBlueprintGenerateResult GenerateResult =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(TargetGraph, GraphWritePayload, UnresolvedNodes);
	const bool bImportSuccess = GenerateResult.bSucceed;
	FString ImportErrorCode = GenerateResult.bSucceed ? TEXT("") : TEXT("semantic_graph_write_failed");
	FString ImportMessage = GenerateResult.Message;
	if (!bImportSuccess && UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
	{
		ImportMessage += FString::Printf(TEXT(" First unresolved: %s - %s"), *UnresolvedNodes[0]->DisplayText, *UnresolvedNodes[0]->Reason);
	}

	if (!bImportSuccess)
	{
		// 清理可能半成品的新图表
		if (!bGraphExistedBeforeWrite)
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, TargetGraph);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		else
		{
			TArray<UEdGraphNode*> NodesToRemove;
			for (UEdGraphNode* Node : TargetGraph->Nodes)
			{
				if (Node && !NodeSnapshot.Contains(Node))
				{
					NodesToRemove.Add(Node);
				}
			}
			for (UEdGraphNode* Node : NodesToRemove)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}

		FBlueprintHelperToolError Error;
		Error.Code = ImportErrorCode.IsEmpty() ? TEXT("node_create_failed") : ImportErrorCode;
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ImportMessage.IsEmpty()
			? TEXT("Agent 导入执行失败。") : ImportMessage;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;

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
			if (UK2Node_CustomEvent* ExistingEntry = FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::FindExistingCustomEventNode(TargetGraph, EntryName))
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
			Blueprint, CreatedNodes, FullBlockId, Request.FeatureName, OwnershipError))
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
	SuccessResult.Data = Data.ToJson();
	FBlueprintHelperGraphFragmentDebugData::AttachToData(SuccessResult.Data, PreflightResult.FragmentDebugData);

	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = true;
	Validation.bShouldSave = true;
	SuccessResult.Validation = Validation;

	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(TargetGraph, CreatedNodes);

	return SuccessResult;
}

// GraphWrite SemanticIR payload

FString FBlueprintHelperAppendBlueprintGraphService::BuildSemanticGraphWritePayload(
	const FAppendRequest& Request) const
{
	FBlueprintHelperGraphWriteSemanticPayload Payload;
	Payload.TargetAssetPath = Request.AssetPath;
	Payload.TargetGraph = Request.GraphName;
	Payload.Mode = TEXT("append");
	Payload.bReconstructExistingNodes = Request.bReuseExistingEntries;
	Payload.LogicSpec = Request.LogicSpec;
	return Payload.ToJsonString();
}

// ─── Helpers ───

TArray<FString> FBlueprintHelperAppendBlueprintGraphService::ExtractCustomEventNames(
	const FAppendRequest& Request) const
{
	TArray<FString> Names;
	if (Request.LogicSpec.IsValid())
	{
		const TSharedPtr<FJsonObject>* EntryObject = nullptr;
		if (Request.LogicSpec->TryGetObjectField(TEXT("entry"), EntryObject) && EntryObject && EntryObject->IsValid())
		{
			FString EntryKind;
			(*EntryObject)->TryGetStringField(TEXT("kind"), EntryKind);
			FString EntryName;
			(*EntryObject)->TryGetStringField(TEXT("name"), EntryName);
			if (EntryKind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase) && !EntryName.IsEmpty())
			{
				Names.AddUnique(EntryName);
			}
		}
	}
	return Names;
}
