// BlueprintHelper Service Layer — AppendBlueprintGraph 核心服务实现

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphWriteDryRunSandbox.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphWriteRollbackFinalizer.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphWriteConnectivityContext.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityDiagnosticsJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "HAL/PlatformTime.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// ─── 禁止创建的全局事件名称集合 ───

struct FBlueprintHelperAppendEventCatalogEvidence
{
	FString Source;
	FString SignatureEvidenceId;
	FString ActionStableId;
	FString ContextFingerprint;
	bool bMarkedStale = false;

	bool IsPresent() const
	{
		return !Source.IsEmpty()
			|| !SignatureEvidenceId.IsEmpty()
			|| !ActionStableId.IsEmpty()
			|| !ContextFingerprint.IsEmpty();
	}
};

struct FBlueprintHelperAppendEventEntry
{
	FString Name;
	FString EventKind = TEXT("custom_event");
	FBlueprintHelperAppendEventCatalogEvidence CatalogEvidence;
};

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

	static bool IsReservedGeneratedEventName(const FString& Name)
	{
		for (const FString& Forbidden : ForbiddenEventNames())
		{
			if (Name.Equals(Forbidden, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static FString ReadTrimmedStringField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(FieldName, Value);
			Value.TrimStartAndEndInline();
		}
		return Value;
	}

	static FString NormalizeEventKind(const FString& RawEventKind)
	{
		const FString EventKind = RawEventKind.TrimStartAndEnd().ToLower();
		if (EventKind == TEXT("override_event") ||
			EventKind == TEXT("component_bound_event") ||
			EventKind == TEXT("input_action_event") ||
			EventKind == TEXT("dispatcher_event"))
		{
			return EventKind;
		}
		return TEXT("custom_event");
	}

	static FBlueprintHelperAppendEventCatalogEvidence ReadCatalogEvidence(
		const TSharedPtr<FJsonObject>& EntryObject)
	{
		FBlueprintHelperAppendEventCatalogEvidence Evidence;
		const TSharedPtr<FJsonObject>* CatalogEvidenceObject = nullptr;
		if (!EntryObject.IsValid() ||
			!EntryObject->TryGetObjectField(TEXT("catalog_evidence"), CatalogEvidenceObject) ||
			!CatalogEvidenceObject ||
			!CatalogEvidenceObject->IsValid())
		{
			return Evidence;
		}

		Evidence.Source = ReadTrimmedStringField(*CatalogEvidenceObject, TEXT("source")).ToLower();
		Evidence.SignatureEvidenceId = ReadTrimmedStringField(*CatalogEvidenceObject, TEXT("signature_evidence_id"));
		Evidence.ActionStableId = ReadTrimmedStringField(*CatalogEvidenceObject, TEXT("action_stable_id"));
		Evidence.ContextFingerprint = ReadTrimmedStringField(*CatalogEvidenceObject, TEXT("context_fingerprint"));

		bool bMarkedStale = false;
		if ((*CatalogEvidenceObject)->TryGetBoolField(TEXT("stale"), bMarkedStale) && bMarkedStale)
		{
			Evidence.bMarkedStale = true;
		}
		if ((*CatalogEvidenceObject)->TryGetBoolField(TEXT("context_stale"), bMarkedStale) && bMarkedStale)
		{
			Evidence.bMarkedStale = true;
		}
		if ((*CatalogEvidenceObject)->TryGetBoolField(TEXT("is_stale"), bMarkedStale) && bMarkedStale)
		{
			Evidence.bMarkedStale = true;
		}
		const FString Status = ReadTrimmedStringField(*CatalogEvidenceObject, TEXT("status")).ToLower();
		if (Status == TEXT("stale") || Status == TEXT("context_stale"))
		{
			Evidence.bMarkedStale = true;
		}

		return Evidence;
	}

	static TArray<FBlueprintHelperAppendEventEntry> ExtractAppendEventEntries(
		const TSharedPtr<FJsonObject>& LogicSpec)
	{
		TArray<FBlueprintHelperAppendEventEntry> Entries;
		const TSharedPtr<FJsonObject>* EntryObject = nullptr;
		if (!LogicSpec.IsValid() ||
			!LogicSpec->TryGetObjectField(TEXT("entry"), EntryObject) ||
			!EntryObject ||
			!EntryObject->IsValid())
		{
			return Entries;
		}

		FBlueprintHelperAppendEventEntry Entry;
		Entry.Name = ReadTrimmedStringField(*EntryObject, TEXT("name"));
		FString RawEventKind = ReadTrimmedStringField(*EntryObject, TEXT("event_kind"));
		if (RawEventKind.IsEmpty())
		{
			RawEventKind = ReadTrimmedStringField(*EntryObject, TEXT("kind"));
		}
		Entry.EventKind = NormalizeEventKind(RawEventKind);
		Entry.CatalogEvidence = ReadCatalogEvidence(*EntryObject);
		if (!Entry.Name.IsEmpty())
		{
			Entries.Add(MoveTemp(Entry));
		}
		return Entries;
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

	static TArray<UEdGraphNode*> CollectNodesNotInSnapshot(
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodeSnapshot)
	{
		TArray<UEdGraphNode*> Nodes;
		if (!Graph)
		{
			return Nodes;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !NodeSnapshot.Contains(Node))
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

	static void AttachGraphWriteExecutionStats(
		TSharedPtr<FJsonObject> Data,
		const FBlueprintGraphWriteExecutionStats& Stats)
	{
		if (!Data.IsValid())
		{
			return;
		}

		Data->SetObjectField(
			TEXT("graph_write_execution_stats"),
			FBlueprintGraphWriteExecutionStatsSerializer::ToJson(Stats));
	}

	static void AttachDryRunSideEffectProof(
		TSharedPtr<FJsonObject> Data,
		int32 GeneratedNodeCount)
	{
		if (!Data.IsValid())
		{
			return;
		}

		Data->SetStringField(TEXT("dry_run_side_effects"), TEXT("none"));
		Data->SetStringField(TEXT("sandbox"), TEXT("transient_blueprint_duplicate"));
		Data->SetNumberField(TEXT("generated_node_count"), GeneratedNodeCount);
	}

	static void CollectExecReachableOwnershipNodes(
		UEdGraphNode* EntryNode,
		const TSet<UEdGraphNode*>& OwnershipCandidates,
		TArray<UEdGraphNode*>& OutNodes)
	{
		if (!EntryNode)
		{
			return;
		}

		TArray<UEdGraphNode*> Stack;
		TSet<UEdGraphNode*> Visited;
		Stack.Add(EntryNode);
		while (Stack.Num() > 0)
		{
			UEdGraphNode* Current = FBlueprintHelperVersionCompat::PopNoShrink(Stack);
			if (!Current || Visited.Contains(Current))
			{
				continue;
			}
			Visited.Add(Current);

			if (OwnershipCandidates.Contains(Current))
			{
				OutNodes.AddUnique(Current);
			}

			for (UEdGraphPin* Pin : Current->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode && OwnershipCandidates.Contains(LinkedNode) && !Visited.Contains(LinkedNode))
					{
						Stack.Add(LinkedNode);
					}
				}
			}
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	static bool& AutomationOwnershipWriteFailureFlag()
	{
		static bool bFail = false;
		return bFail;
	}

	static FString& AutomationOwnershipWriteFailureMessage()
	{
		static FString Message;
		return Message;
	}

	static bool ShouldForceAutomationOwnershipWriteFailure(FString& OutError)
	{
		if (!AutomationOwnershipWriteFailureFlag())
		{
			return false;
		}

		OutError = AutomationOwnershipWriteFailureMessage().IsEmpty()
			? TEXT("Forced automation ownership write failure.")
			: AutomationOwnershipWriteFailureMessage();
		return true;
	}
#endif

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

#if WITH_DEV_AUTOMATION_TESTS
void FBlueprintHelperAppendBlueprintGraphService::SetAutomationOwnershipWriteFailure(
	bool bFail,
	const FString& ErrorMessage)
{
	FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AutomationOwnershipWriteFailureFlag() = bFail;
	FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AutomationOwnershipWriteFailureMessage() = ErrorMessage;
}
#endif

FBlueprintHelperToolResultBase FBlueprintHelperAppendBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FAppendRequest Request = ParseRequest(Payload);

	return FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
		Request.bDryRun
			? EBlueprintHelperGraphWriteUnitOfWorkMode::Preview
			: EBlueprintHelperGraphWriteUnitOfWorkMode::Execute,
		TEXT("append_blueprint_graph"),
		TEXT("append_new_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2CustomEventBody,
		[this, &Request]()
		{
			return Request.bDryRun ? ExecuteDryRun(Request) : ExecuteWrite(Request);
		});
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
	Payload->TryGetBoolField(TEXT("include_timing"), Request.bIncludeTiming);

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
	OutBlueprint = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());

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
	for (const FBlueprintHelperAppendEventEntry& Entry :
		FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::ExtractAppendEventEntries(Request.LogicSpec))
	{
		const FString& Name = Entry.Name;
		if (Entry.EventKind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase))
		{
			if (FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::IsReservedGeneratedEventName(Name))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("reserved_generated_event_name"));
				OutResult.Conflicts.Add({TEXT("reserved_generated_event_name"), FString::Printf(TEXT("Reserved generated event name cannot be created as a custom event: %s."), *Name), Name, TEXT("logic_spec.entry.name")});
			}
			if (SeenNames.Contains(Name))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("custom_event_already_exists"));
				OutResult.Conflicts.Add({TEXT("custom_event_already_exists"), FString::Printf(TEXT("Custom Event name is duplicated: %s."), *Name), Name, TEXT("logic_spec.entry.name")});
			}
			SeenNames.Add(Name);
			if (Graph && !Request.bReuseExistingEntries && FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::FindExistingCustomEventNode(Graph, Name))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("custom_event_already_exists"));
				OutResult.Conflicts.Add({TEXT("custom_event_already_exists"), FString::Printf(TEXT("Custom Event name already exists: %s."), *Name), Name, TEXT("logic_spec.entry.name")});
			}
			if (Graph && Request.bReuseExistingEntries && !Request.bDryRun && !FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::FindExistingCustomEventNode(Graph, Name))
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("custom_event_entry_not_found"));
				OutResult.Conflicts.Add({TEXT("custom_event_entry_not_found"), FString::Printf(TEXT("Custom Event '%s' must already exist when reuse_existing_entries is enabled."), *Name), Name, TEXT("logic_spec.entry.name")});
			}
			continue;
		}

		const FBlueprintHelperAppendEventCatalogEvidence& Evidence = Entry.CatalogEvidence;
		if (!Evidence.IsPresent())
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("event_catalog_evidence_required"));
			OutResult.Conflicts.Add({TEXT("event_catalog_evidence_required"), FString::Printf(TEXT("%s requires catalog_evidence before append_blueprint_graph can create or bind event '%s'."), *Entry.EventKind, *Name), Name, TEXT("logic_spec.entry.catalog_evidence")});
			continue;
		}
		if (Evidence.bMarkedStale)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("event_catalog_evidence_stale"));
			OutResult.Conflicts.Add({TEXT("event_catalog_evidence_stale"), FString::Printf(TEXT("%s catalog_evidence is stale for event '%s'."), *Entry.EventKind, *Name), Name, TEXT("logic_spec.entry.catalog_evidence.context_fingerprint")});
			continue;
		}
		if (Evidence.Source.Equals(TEXT("signature"), ESearchCase::IgnoreCase))
		{
			if (Evidence.SignatureEvidenceId.IsEmpty())
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("event_catalog_evidence_required"));
				OutResult.Conflicts.Add({TEXT("event_catalog_evidence_required"), FString::Printf(TEXT("%s signature catalog_evidence requires signature_evidence_id for event '%s'."), *Entry.EventKind, *Name), Name, TEXT("logic_spec.entry.catalog_evidence.signature_evidence_id")});
			}
			continue;
		}
		if (Evidence.Source.Equals(TEXT("graph_action_catalog"), ESearchCase::IgnoreCase))
		{
			if (Evidence.ActionStableId.IsEmpty() || Evidence.ContextFingerprint.IsEmpty())
			{
				OutResult.bPassed = false;
				OutResult.BlockedBy.Add(TEXT("event_catalog_evidence_required"));
				OutResult.Conflicts.Add({TEXT("event_catalog_evidence_required"), FString::Printf(TEXT("%s graph_action_catalog evidence requires action_stable_id and context_fingerprint for event '%s'."), *Entry.EventKind, *Name), Name, TEXT("logic_spec.entry.catalog_evidence")});
			}
			continue;
		}

		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("event_catalog_evidence_required"));
		OutResult.Conflicts.Add({TEXT("event_catalog_evidence_required"), FString::Printf(TEXT("%s catalog_evidence has unsupported source for event '%s'."), *Entry.EventKind, *Name), Name, TEXT("logic_spec.entry.catalog_evidence.source")});
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
		UBlueprint* Blueprint = Resolver.ResolveBlueprint(GraphTarget, Diag, FBlueprintHelperResolvePolicy::Mutation());
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
			FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachDryRunSideEffectProof(
				Result.Data,
				0);
		}
		else
		{
			FBlueprintHelperGraphWriteDryRunSandboxInput SandboxInput;
			SandboxInput.SourceBlueprint = Blueprint;
			SandboxInput.GraphName = Request.GraphName;
			SandboxInput.GraphWritePayload = BuildSemanticGraphWritePayload(Request);
			SandboxInput.ConnectivityContext.RuntimeAdapterId = TEXT("k2.owned_graph.ensure_entry");
			SandboxInput.ConnectivityContext.TaskSpecStrategy = TEXT("append_new_owned_graph");
			SandboxInput.ConnectivityContext.TargetAssetPath = Blueprint->GetPathName();
			SandboxInput.ConnectivityContext.GraphName = Request.GraphName;
			SandboxInput.ConnectivityContext.GraphFamily = TEXT("k2");
			SandboxInput.ConnectivityContext.BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
			SandboxInput.ConnectivityContext.EntryNodeRefs.Add(
				FBlueprintHelperGraphWriteConnectivityContextBuilder::MakeSemanticEntryRefFromLogicSpec(Request.LogicSpec));

			const FBlueprintHelperGraphWriteDryRunSandboxResult SandboxResult =
				FBlueprintHelperGraphWriteDryRunSandbox().RunAppendPreview(SandboxInput);
			if (SandboxResult.bSucceeded)
			{
				FBlueprintHelperAppendDryRunData DryRunData;
				DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Passed;
				DryRunData.DryRun.bCanExecute = true;
				Result.Data = DryRunData.ToJson();
				FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachDryRunSideEffectProof(
					Result.Data,
					SandboxResult.GeneratedNodeCount);
				if (Request.bIncludeTiming)
				{
					FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachGraphWriteExecutionStats(
						Result.Data,
						SandboxResult.ExecutionStats);
				}
			}
			else
			{
				const bool bPreserveGraphWriteFailure =
					SandboxResult.ErrorCode == TEXT("graphwrite_connectivity_failed") ||
					SandboxResult.ErrorCode == TEXT("semantic_graph_write_failed");
				const FString FailureCode = bPreserveGraphWriteFailure
					? SandboxResult.ErrorCode
					: TEXT("graphwrite_dry_run_sandbox_failed");
				const FString FailureMessage = SandboxResult.Message.IsEmpty()
					? TEXT("GraphWrite dry-run sandbox failed.")
					: SandboxResult.Message;

				FBlueprintHelperAppendDryRunData DryRunData;
				DryRunData.DryRun.Result = EBlueprintHelperDryRunResult::Blocked;
				DryRunData.DryRun.bCanExecute = false;
				DryRunData.DryRun.BlockedBy.Add(FailureCode);
				DryRunData.DryRun.Errors.Add({
					FailureCode,
					FailureMessage,
					Request.GraphName,
					bPreserveGraphWriteFailure ? TEXT("logic_spec") : TEXT("target.graph")
				});

				FBlueprintHelperToolError Error;
				Error.Code = FailureCode;
				Error.Stage = EBlueprintHelperToolStage::Preflight;
				Error.Message = FailureMessage;
				Error.Field = bPreserveGraphWriteFailure ? TEXT("logic_spec") : TEXT("target.graph");
				Error.bRetryable = false;
				Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

				Result = FBlueprintHelperToolResultBuilder::Failure(
					TEXT("append_blueprint_graph"), TraceId, Error);
				Result.Target = TargetRef;
				Result.Data = DryRunData.ToJson();
				Result.Data->SetStringField(TEXT("sandbox_error_code"), SandboxResult.ErrorCode);
				FBlueprintHelperGraphWriteConnectivityDiagnosticsJson::Attach(
					Result.Data,
					SandboxResult.ConnectivityDiagnostics);
				FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachDryRunSideEffectProof(
					Result.Data,
					SandboxResult.GeneratedNodeCount);
				if (Request.bIncludeTiming)
				{
					FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachGraphWriteExecutionStats(
						Result.Data,
						SandboxResult.ExecutionStats);
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
		FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachDryRunSideEffectProof(
			Result.Data,
			0);
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
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());
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
	const bool bPackageWasDirtyBeforeWrite = Blueprint->GetOutermost()
		? Blueprint->GetOutermost()->IsDirty()
		: false;
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
	const FBlueprintGraphWriteConnectivityValidationInput ConnectivityInput =
		BuildAppendConnectivityInput(Blueprint, TargetGraph, Request);
	const FBlueprintGenerateResult GenerateResult =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
			TargetGraph,
			GraphWritePayload,
			UnresolvedNodes,
			ConnectivityInput);
	FBlueprintGraphWriteExecutionStats ExecutionStats = GenerateResult.ExecutionStats;
	const bool bImportSuccess = GenerateResult.bSucceed;
	const bool bConnectivityFailure = GenerateResult.ConnectivityViolationCount > 0;
	FString ImportErrorCode = GenerateResult.bSucceed
		? TEXT("")
		: (bConnectivityFailure ? TEXT("graphwrite_connectivity_failed") : TEXT("semantic_graph_write_failed"));
	FString ImportMessage = GenerateResult.Message;
	if (!bImportSuccess && UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
	{
		ImportMessage += FString::Printf(TEXT(" First unresolved: %s - %s"), *UnresolvedNodes[0]->DisplayText, *UnresolvedNodes[0]->Reason);
	}

	if (!bImportSuccess)
	{
		FBlueprintHelperGraphWriteRollbackInput RollbackInput;
		RollbackInput.Blueprint = Blueprint;
		RollbackInput.TargetGraph = TargetGraph;
		RollbackInput.bGraphExistedBeforeWrite = bGraphExistedBeforeWrite;
		RollbackInput.bPackageWasDirtyBeforeWrite = bPackageWasDirtyBeforeWrite;
		RollbackInput.NodeSnapshot = NodeSnapshot;
		RollbackInput.ImportedNodes =
			FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::CollectNodesNotInSnapshot(TargetGraph, NodeSnapshot);
		RollbackInput.ReasonCode = ImportErrorCode;
		const FBlueprintHelperGraphWriteRollbackResult RollbackResult =
			FBlueprintHelperGraphWriteRollbackFinalizer().RollbackPostImportFailure(RollbackInput);

		// 清理可能半成品的新图表
		FBlueprintHelperToolError Error;
		Error.Code = ImportErrorCode.IsEmpty() ? TEXT("node_create_failed") : ImportErrorCode;
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ImportMessage.IsEmpty()
			? TEXT("Agent 导入执行失败。") : ImportMessage;
		if (!RollbackResult.bRolledBack && !RollbackResult.ErrorMessage.IsEmpty())
		{
			Error.Message += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackResult.ErrorMessage);
		}
		Error.bRetryable = false;
		Error.RollbackResult = RollbackResult.bRolledBack
			? EBlueprintHelperRollbackResult::RolledBack
			: EBlueprintHelperRollbackResult::Failed;

		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("append_blueprint_graph"), TraceId, Error);

		FBlueprintHelperTargetRef FailTarget;
		FailTarget.AssetPath = Request.AssetPath;
		FailTarget.TargetType = EBlueprintHelperTargetType::Graph;
		FailTarget.Graph = Request.GraphName;
		FailResult.Target = FailTarget;
		FailResult.Data = MakeShared<FJsonObject>();
		FBlueprintHelperGraphWriteConnectivityDiagnosticsJson::Attach(
			FailResult.Data,
			GenerateResult.ConnectivityDiagnostics);
		if (Request.bIncludeTiming)
		{
			FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachGraphWriteExecutionStats(
				FailResult.Data,
				ExecutionStats);
		}

		return FailResult;
	}

	// 6. 分组并为节点写入 ownership
	TArray<FString> BlockRefs;
	const TArray<FString> EntryNames = ExtractCustomEventNames(Request);
	TArray<UEdGraphNode*> ImportedNodes;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node && !NodeSnapshot.Contains(Node))
		{
			ImportedNodes.Add(Node);
		}
	}
	TSet<UEdGraphNode*> OwnershipCandidates;
	for (UEdGraphNode* Node : ImportedNodes)
	{
		OwnershipCandidates.Add(Node);
	}
	if (Request.bReuseExistingEntries)
	{
		for (const FString& EntryName : EntryNames)
		{
			if (UK2Node_CustomEvent* ExistingEntry = FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::FindExistingCustomEventNode(TargetGraph, EntryName))
			{
				OwnershipCandidates.Add(ExistingEntry);
			}
		}
	}

	for (const FString& EntryName : EntryNames)
	{
		const FString BlockRef = BlockIdService.MakeBlockRef(Blueprint, TargetGraph, EntryName);
		const FString FullBlockId = BlockIdService.MakeFullBlockId(Request.GraphName, BlockRef);
		BlockRefs.Add(BlockRef);

		// Keep ownership partitioned per entry instead of stamping every imported node with every block id.
		TArray<UEdGraphNode*> OwnershipNodes;
		UK2Node_CustomEvent* EntryNode = FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::FindExistingCustomEventNode(TargetGraph, EntryName);
		FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::CollectExecReachableOwnershipNodes(
			EntryNode,
			OwnershipCandidates,
			OwnershipNodes);
		if (OwnershipNodes.Num() == 0 && EntryNames.Num() == 1)
		{
			for (UEdGraphNode* CandidateNode : OwnershipCandidates)
			{
				OwnershipNodes.AddUnique(CandidateNode);
			}
		}
		if (OwnershipNodes.Num() == 0)
		{
			continue;
		}

		FString OwnershipError;
		bool bOwnershipWriteSucceeded = true;
#if WITH_DEV_AUTOMATION_TESTS
		if (FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::ShouldForceAutomationOwnershipWriteFailure(OwnershipError))
		{
			bOwnershipWriteSucceeded = false;
		}
		else
#endif
		{
			bOwnershipWriteSucceeded = OwnershipService.WriteBlockOwnership(
				Blueprint, OwnershipNodes, FullBlockId, Request.FeatureName, OwnershipError);
		}
		if (!bOwnershipWriteSucceeded)
		{
			FBlueprintHelperGraphWriteRollbackInput RollbackInput;
			RollbackInput.Blueprint = Blueprint;
			RollbackInput.TargetGraph = TargetGraph;
			RollbackInput.bGraphExistedBeforeWrite = bGraphExistedBeforeWrite;
			RollbackInput.bPackageWasDirtyBeforeWrite = bPackageWasDirtyBeforeWrite;
			RollbackInput.NodeSnapshot = NodeSnapshot;
			RollbackInput.ImportedNodes = ImportedNodes;
			RollbackInput.ReasonCode = TEXT("ownership_write_failed");
			const FBlueprintHelperGraphWriteRollbackResult RollbackResult =
				FBlueprintHelperGraphWriteRollbackFinalizer().RollbackPostImportFailure(RollbackInput);

			// Ownership 写入失败 → 回滚
			FBlueprintHelperToolError Error;
			Error.Code = TEXT("ownership_write_failed");
			Error.Stage = EBlueprintHelperToolStage::Execute;
			Error.Message = OwnershipError;
			if (!RollbackResult.bRolledBack && !RollbackResult.ErrorMessage.IsEmpty())
			{
				Error.Message += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackResult.ErrorMessage);
			}
			Error.bRetryable = false;
			Error.RollbackResult = RollbackResult.bRolledBack
				? EBlueprintHelperRollbackResult::RolledBack
				: EBlueprintHelperRollbackResult::Failed;
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

	const double LayoutStart = FPlatformTime::Seconds();
	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(TargetGraph, ImportedNodes);
	ExecutionStats.RecordLayoutMs = (FPlatformTime::Seconds() - LayoutStart) * 1000.0;
	ExecutionStats.LayoutRecordNodeCount = ImportedNodes.Num();
	if (Request.bIncludeTiming)
	{
		FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::AttachGraphWriteExecutionStats(
			SuccessResult.Data,
			ExecutionStats);
	}

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
	Payload.bDryRun = Request.bDryRun;
	Payload.bReconstructExistingNodes = Request.bReuseExistingEntries;
	Payload.LogicSpec = Request.LogicSpec;
	return Payload.ToJsonString();
}

// ─── Helpers ───

FBlueprintGraphWriteConnectivityValidationInput FBlueprintHelperAppendBlueprintGraphService::BuildAppendConnectivityInput(
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FAppendRequest& Request) const
{
	FBlueprintHelperGraphWriteConnectivityContextInput ContextInput;
	ContextInput.RuntimeAdapterId = TEXT("k2.owned_graph.ensure_entry");
	ContextInput.TaskSpecStrategy = TEXT("append_new_owned_graph");
	ContextInput.TargetAssetPath = Blueprint ? Blueprint->GetPathName() : Request.AssetPath;
	ContextInput.GraphName = TargetGraph ? TargetGraph->GetName() : Request.GraphName;
	ContextInput.GraphFamily = TEXT("k2");
	ContextInput.BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
	ContextInput.EntryNodeRefs.Add(
		FBlueprintHelperGraphWriteConnectivityContextBuilder::MakeSemanticEntryRefFromLogicSpec(Request.LogicSpec));
	return FBlueprintHelperGraphWriteConnectivityContextBuilder::Build(TargetGraph, ContextInput);
}

TArray<FString> FBlueprintHelperAppendBlueprintGraphService::ExtractCustomEventNames(
	const FAppendRequest& Request) const
{
	TArray<FString> Names;
	for (const FBlueprintHelperAppendEventEntry& Entry :
		FBlueprintHelperAppendBlueprintGraphServiceLocalUtils::ExtractAppendEventEntries(Request.LogicSpec))
	{
		if (Entry.EventKind.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase) && !Entry.Name.IsEmpty())
		{
			Names.AddUnique(Entry.Name);
		}
	}
	return Names;
}
