// BlueprintHelper Service Layer - replace external body service.

#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperReplaceExternalBodyService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphWriteConnectivityContext.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"
#include "Systems/ToolClusters/GraphWrite/Utils/GraphWriteCoreUtils.h"

namespace BlueprintHelperReplaceExternalBody
{
	static constexpr const TCHAR* OperationName = TEXT("replace_external_body");

	static FBlueprintHelperDryRunIssue MakeIssue(
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		FBlueprintHelperDryRunIssue Issue;
		Issue.Code = Code;
		Issue.Message = Message;
		Issue.Target = Target;
		Issue.Source = Source;
		return Issue;
	}

	static void AddError(
		FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyPreflightResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		Result.bPassed = false;
		Result.BlockedBy.AddUnique(Code);
		Result.Errors.Add(MakeIssue(Code, Message, Target, Source));
	}

	static void AddConflict(
		FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyPreflightResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		Result.bPassed = false;
		Result.BlockedBy.AddUnique(Code);
		Result.Conflicts.Add(MakeIssue(Code, Message, Target, Source));
	}

	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FString& Value : Values)
		{
			Array.Add(MakeShared<FJsonValueString>(Value));
		}
		return Array;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeIssueArray(const TArray<FBlueprintHelperDryRunIssue>& Issues)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FBlueprintHelperDryRunIssue& Issue : Issues)
		{
			Array.Add(MakeShared<FJsonValueObject>(Issue.ToJson()));
		}
		return Array;
	}

	static FBlueprintHelperToolError MakeToolError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field,
		EBlueprintHelperRollbackResult RollbackResult = EBlueprintHelperRollbackResult::NotNeeded)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.Field = Field;
		Error.bRetryable = false;
		Error.RollbackResult = RollbackResult;
		return Error;
	}

	static FBlueprintHelperToolError MakeErrorFromPreflight(
		const FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyPreflightResult& Preflight)
	{
		const FBlueprintHelperDryRunIssue* FirstIssue = Preflight.Conflicts.Num() > 0
			? &Preflight.Conflicts[0]
			: (Preflight.Errors.Num() > 0 ? &Preflight.Errors[0] : nullptr);
		return MakeToolError(
			Preflight.BlockedBy.Num() > 0 ? Preflight.BlockedBy[0] : TEXT("preflight_failed"),
			EBlueprintHelperToolStage::Preflight,
			FirstIssue && !FirstIssue->Message.IsEmpty()
				? FirstIssue->Message
				: TEXT("replace_external_body preflight blocked execution."),
			FirstIssue && !FirstIssue->Source.IsEmpty()
				? FirstIssue->Source
				: TEXT("payload"));
	}

	static void AppendDiagnosticsSummary(
		FString& InOutMessage,
		const FString& Label,
		const TArray<FBlueprintGeneratorDiagnostic>& Diagnostics)
	{
		if (Diagnostics.Num() == 0)
		{
			return;
		}

		InOutMessage += TEXT(" ") + Label + TEXT(":");
		for (int32 Index = 0; Index < Diagnostics.Num(); ++Index)
		{
			const FBlueprintGeneratorDiagnostic& Diagnostic = Diagnostics[Index];
			InOutMessage += FString::Printf(
				TEXT(" [%d] %s node=%s message=%s"),
				Index,
				*Diagnostic.Code,
				*Diagnostic.NodeId,
				*Diagnostic.Message);
		}
	}

	static UBlueprint* FindBlueprint(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		if (UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *AssetPath))
		{
			return Blueprint;
		}
		return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
	}

	static void AddGraphIfValid(TArray<UEdGraph*>& Graphs, UEdGraph* Graph)
	{
		if (Graph)
		{
			Graphs.Add(Graph);
		}
	}

	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint || GraphName.IsEmpty())
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}

		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static bool ScopeMatchesEntryClass(EBlueprintHelperReplaceScope Scope, const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}
		if (Scope == EBlueprintHelperReplaceScope::CustomEventBody)
		{
			return Node->IsA<UK2Node_CustomEvent>();
		}
		if (Scope == EBlueprintHelperReplaceScope::EventBody)
		{
			return Node->IsA<UK2Node_Event>() && !Node->IsA<UK2Node_CustomEvent>();
		}
		if (Scope == EBlueprintHelperReplaceScope::FunctionBody)
		{
			return Node->IsA<UK2Node_FunctionEntry>();
		}
		return false;
	}

	static TSharedRef<FJsonObject> MakeDryRunData(
		const FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyPreflightResult& Preflight,
		const FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyRequest& Request,
		const FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyContext& Context)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("ReplaceExternalBodyDryRun.v1"));

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), Preflight.bPassed ? TEXT("passed") : TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), Preflight.bPassed);
		DryRun->SetArrayField(TEXT("blocked_by"), MakeStringArray(Preflight.BlockedBy));
		DryRun->SetArrayField(TEXT("conflicts"), MakeIssueArray(Preflight.Conflicts));
		DryRun->SetArrayField(TEXT("errors"), MakeIssueArray(Preflight.Errors));
		Data->SetObjectField(TEXT("dry_run"), DryRun);

		TSharedRef<FJsonObject> Plan = MakeShared<FJsonObject>();
		Plan->SetStringField(TEXT("scope"), ReplaceScopeToString(Request.Scope));
		Plan->SetStringField(TEXT("entry_node_guid"), Context.BeforeSnapshot.EntryNodeGuid);
		Plan->SetStringField(TEXT("body_fingerprint"), Context.BeforeSnapshot.BodyFingerprint);
		Plan->SetArrayField(TEXT("nodes_to_remove"), MakeStringArray(Context.BeforeSnapshot.BodyNodeGuids));
		const TArray<TSharedPtr<FJsonValue>>* Statements = nullptr;
		Plan->SetNumberField(
			TEXT("nodes_to_create"),
			Request.Body.IsValid() && Request.Body->TryGetArrayField(TEXT("statements"), Statements) && Statements
				? Statements->Num()
				: 0);
		Plan->SetObjectField(TEXT("dependents_analysis"), Context.DependentsAnalysis.ToJson());
		Data->SetObjectField(TEXT("external_body_replace_plan"), Plan);
		Data->SetObjectField(TEXT("external_body_before_snapshot"), Context.BeforeSnapshot.ToJson());
		return Data;
	}

	static FString BuildSemanticPayload(
		const FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyRequest& Request)
	{
		FBlueprintHelperGraphWriteSemanticPayload Payload;
		Payload.TargetAssetPath = Request.AssetPath;
		Payload.TargetGraph = Request.GraphName;
		Payload.Mode = TEXT("append");
		Payload.bDryRun = Request.bDryRun;
		Payload.LogicSpec = Request.Body;
		return Payload.ToJsonString();
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

	static TArray<UEdGraphNode*> CollectNewNodes(UEdGraph* Graph, const TSet<UEdGraphNode*>& Before)
	{
		TArray<UEdGraphNode*> Nodes;
		if (!Graph)
		{
			return Nodes;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !Before.Contains(Node))
			{
				Nodes.Add(Node);
			}
		}
		return Nodes;
	}

	static bool HasInboundExecFromImportedNode(UEdGraphPin* ExecInputPin, const TSet<UEdGraphNode*>& ImportedNodes)
	{
		if (!ExecInputPin)
		{
			return false;
		}
		for (UEdGraphPin* LinkedPin : ExecInputPin->LinkedTo)
		{
			if (LinkedPin && ImportedNodes.Contains(LinkedPin->GetOwningNode()))
			{
				return true;
			}
		}
		return false;
	}

	static UEdGraphNode* FindFirstImportedExecutableBodyNode(const TArray<UEdGraphNode*>& ImportedNodes)
	{
		TSet<UEdGraphNode*> ImportedSet;
		for (UEdGraphNode* Node : ImportedNodes)
		{
			ImportedSet.Add(Node);
		}
		for (UEdGraphNode* Node : ImportedNodes)
		{
			UEdGraphPin* ExecIn = UGraphWriteCoreUtils::FindFirstExecPin(Node, EGPD_Input);
			if (ExecIn && !HasInboundExecFromImportedNode(ExecIn, ImportedSet))
			{
				return Node;
			}
		}
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (UGraphWriteCoreUtils::FindFirstExecPin(Node, EGPD_Input))
			{
				return Node;
			}
		}
		return nullptr;
	}

	static bool HasExecPin(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (UGraphWriteCoreUtils::IsExecPin(Pin))
			{
				return true;
			}
		}
		return false;
	}

	static void CollectExecReachableFromBodyEntry(
		UEdGraphPin* BodyEntryPin,
		const TSet<UEdGraphNode*>& GeneratedSet,
		TSet<UEdGraphNode*>& OutReachable)
	{
		UEdGraphNode* RootNode = BodyEntryPin ? BodyEntryPin->GetOwningNode() : nullptr;
		if (!RootNode || !GeneratedSet.Contains(RootNode))
		{
			return;
		}

		TArray<UEdGraphNode*> Stack;
		Stack.Add(RootNode);
		while (Stack.Num() > 0)
		{
			UEdGraphNode* Node = FBlueprintHelperVersionCompat::PopNoShrink(Stack);
			if (!Node || OutReachable.Contains(Node) || !GeneratedSet.Contains(Node))
			{
				continue;
			}

			OutReachable.Add(Node);
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || !UGraphWriteCoreUtils::IsExecPin(Pin))
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedPin
						&& LinkedPin->Direction == EGPD_Input
						&& UGraphWriteCoreUtils::IsExecPin(LinkedPin)
						&& LinkedNode
						&& GeneratedSet.Contains(LinkedNode)
						&& !OutReachable.Contains(LinkedNode))
					{
						Stack.Add(LinkedNode);
					}
				}
			}
		}
	}

	static bool AreGeneratedExecNodesReachableFromBodyEntry(
		const TArray<UEdGraphNode*>& GeneratedNodes,
		UEdGraphPin* BodyEntryPin)
	{
		TSet<UEdGraphNode*> GeneratedSet;
		for (UEdGraphNode* Node : GeneratedNodes)
		{
			if (Node)
			{
				GeneratedSet.Add(Node);
			}
		}
		if (!BodyEntryPin || !GeneratedSet.Contains(BodyEntryPin->GetOwningNode()))
		{
			return false;
		}

		TSet<UEdGraphNode*> Reachable;
		CollectExecReachableFromBodyEntry(BodyEntryPin, GeneratedSet, Reachable);
		if (Reachable.Num() == 0)
		{
			return false;
		}

		for (UEdGraphNode* Node : GeneratedNodes)
		{
			if (HasExecPin(Node) && !Reachable.Contains(Node))
			{
				return false;
			}
		}
		return true;
	}

	static bool DataChainReachesReachableExecConsumer(
		const UEdGraphNode* Node,
		const TSet<UEdGraphNode*>& GeneratedSet,
		const TSet<UEdGraphNode*>& ReachableExecNodes,
		TSet<const UEdGraphNode*>& VisitedNodes)
	{
		if (!Node || VisitedNodes.Contains(Node))
		{
			return false;
		}
		VisitedNodes.Add(Node);

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || UGraphWriteCoreUtils::IsExecPin(Pin))
			{
				continue;
			}
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || LinkedPin->Direction != EGPD_Input || UGraphWriteCoreUtils::IsExecPin(LinkedPin))
				{
					continue;
				}

				UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
				if (!LinkedNode || !GeneratedSet.Contains(LinkedNode))
				{
					continue;
				}

				if (HasExecPin(LinkedNode))
				{
					return ReachableExecNodes.Contains(LinkedNode);
				}

				if (DataChainReachesReachableExecConsumer(LinkedNode, GeneratedSet, ReachableExecNodes, VisitedNodes))
				{
					return true;
				}
			}
		}
		return false;
	}

	static bool GeneratedPureDataChainsReachBodyEntryExecFlow(
		const TArray<UEdGraphNode*>& GeneratedNodes,
		const TSet<UEdGraphNode*>& GeneratedSet,
		const TSet<UEdGraphNode*>& ReachableExecNodes)
	{
		for (UEdGraphNode* Node : GeneratedNodes)
		{
			if (!Node || HasExecPin(Node))
			{
				continue;
			}

			TSet<const UEdGraphNode*> VisitedNodes;
			if (!DataChainReachesReachableExecConsumer(Node, GeneratedSet, ReachableExecNodes, VisitedNodes))
			{
				return false;
			}
		}
		return true;
	}

	static bool GeneratedBodyNodesAreInternallyConnected(
		const TArray<UEdGraphNode*>& GeneratedNodes,
		UEdGraphPin* BodyEntryPin)
	{
		TSet<UEdGraphNode*> GeneratedSet;
		for (UEdGraphNode* Node : GeneratedNodes)
		{
			if (Node)
			{
				GeneratedSet.Add(Node);
			}
		}
		if (!BodyEntryPin || !GeneratedSet.Contains(BodyEntryPin->GetOwningNode()))
		{
			return false;
		}

		TSet<UEdGraphNode*> ReachableExecNodes;
		CollectExecReachableFromBodyEntry(BodyEntryPin, GeneratedSet, ReachableExecNodes);
		if (ReachableExecNodes.Num() == 0)
		{
			return false;
		}

		return AreGeneratedExecNodesReachableFromBodyEntry(GeneratedNodes, BodyEntryPin)
			&& GeneratedPureDataChainsReachBodyEntryExecFlow(GeneratedNodes, GeneratedSet, ReachableExecNodes);
	}

	static bool CanDeferEntryResolvedConnectivityFailure(
		const FBlueprintGenerateResult& GenerateResult,
		const TArray<UEdGraphNode*>& GeneratedNodes)
	{
		if (GenerateResult.bSucceed
			|| GenerateResult.UnresolvedNodeCount != 0
			|| GenerateResult.ConnectivityDiagnostics.Num() == 0)
		{
			return false;
		}

		for (const FBlueprintGeneratorDiagnostic& Diagnostic : GenerateResult.ConnectivityDiagnostics)
		{
			if (Diagnostic.Code != TEXT("unreachable_exec_node")
				&& Diagnostic.Code != TEXT("unreachable_pure_data_chain"))
			{
				return false;
			}
		}

		UEdGraphNode* BodyEntryNode = FindFirstImportedExecutableBodyNode(GeneratedNodes);
		UEdGraphPin* BodyEntryPin = UGraphWriteCoreUtils::FindFirstExecPin(BodyEntryNode, EGPD_Input);
		return GeneratedBodyNodesAreInternallyConnected(GeneratedNodes, BodyEntryPin);
	}

	static bool ReconnectEntryToGeneratedBody(
		UEdGraph* Graph,
		UEdGraphNode* EntryNode,
		const TArray<UEdGraphNode*>& GeneratedNodes,
		FString& OutError)
	{
		if (!Graph || !EntryNode)
		{
			OutError = TEXT("entry_reconnect_context_missing");
			return false;
		}

		UEdGraphPin* EntryExecOut = UGraphWriteCoreUtils::FindFirstExecPin(EntryNode, EGPD_Output);
		if (!EntryExecOut)
		{
			OutError = TEXT("entry_exec_output_not_found");
			return false;
		}

		EntryExecOut->Modify();
		EntryExecOut->BreakAllPinLinks(true);

		UEdGraphNode* FirstBodyNode = FindFirstImportedExecutableBodyNode(GeneratedNodes);
		if (!FirstBodyNode)
		{
			Graph->NotifyGraphChanged();
			return true;
		}

		UEdGraphPin* BodyExecIn = UGraphWriteCoreUtils::FindFirstExecPin(FirstBodyNode, EGPD_Input);
		if (!BodyExecIn)
		{
			OutError = TEXT("replacement_body_exec_input_not_found");
			return false;
		}

		BodyExecIn->Modify();
		BodyExecIn->BreakAllPinLinks(true);
		bool bChanged = false;
		if (!UGraphWriteCoreUtils::TrySchemaConnect(Graph, EntryExecOut, BodyExecIn, OutError, bChanged))
		{
			return false;
		}
		Graph->NotifyGraphChanged();
		return true;
	}
}

FBlueprintHelperReplaceExternalBodyService::FBlueprintHelperReplaceExternalBodyService(
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperOwnershipService& InOwnershipService,
	const FBlueprintHelperExternalBodySnapshotService& InSnapshotService,
	const FBlueprintHelperExternalDependentsAnalysisService& InDependentsAnalysisService)
	: BlockIdService(InBlockIdService)
	, OwnershipService(InOwnershipService)
	, SnapshotService(InSnapshotService)
	, DependentsAnalysisService(InDependentsAnalysisService)
{
}

FBlueprintHelperReplaceExternalBodyService::FReplaceExternalBodyRequest
FBlueprintHelperReplaceExternalBodyService::ParseRequest(const TSharedRef<FJsonObject>& Payload) const
{
	FReplaceExternalBodyRequest Request;
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName);
		if (Request.GraphName.IsEmpty())
		{
			(*TargetObject)->TryGetStringField(TEXT("graph_name"), Request.GraphName);
		}
	}

	FString ScopeString;
	if (Payload->TryGetStringField(TEXT("scope"), ScopeString))
	{
		ParseReplaceScope(ScopeString, Request.Scope);
	}
	Payload->TryGetStringField(TEXT("expected_body_fingerprint"), Request.ExpectedBodyFingerprint);
	Payload->TryGetBoolField(TEXT("require_full_dry_run"), Request.bRequireFullDryRun);
	Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
	Payload->TryGetStringField(TEXT("feature_name"), Request.FeatureName);

	const TSharedPtr<FJsonObject>* AnchorObject = nullptr;
	if (!Payload->TryGetObjectField(TEXT("anchor"), AnchorObject) ||
		!AnchorObject || !AnchorObject->IsValid() ||
		!FBlueprintHelperExternalGraphAnchor::FromJson(*AnchorObject, Request.Anchor, Request.AnchorParseError))
	{
		if (Request.AnchorParseError.IsEmpty())
		{
			Request.AnchorParseError = TEXT("external_anchor_schema_unsupported");
		}
	}

	const TSharedPtr<FJsonObject>* BodyObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject && BodyObject->IsValid())
	{
		Request.Body = *BodyObject;
	}
	return Request;
}

bool FBlueprintHelperReplaceExternalBodyService::Preflight(
	const FReplaceExternalBodyRequest& Request,
	FReplaceExternalBodyContext& Context,
	FReplaceExternalBodyPreflightResult& OutResult) const
{
	if (Request.AssetPath.IsEmpty())
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("target_blueprint_not_found"), TEXT("replace_external_body requires target.asset_path."), TEXT("target.asset_path"), TEXT("payload.target.asset_path"));
		return false;
	}
	if (Request.GraphName.IsEmpty())
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("target_graph_not_found"), TEXT("replace_external_body requires target.graph."), TEXT("target.graph"), TEXT("payload.target.graph"));
		return false;
	}
	if (Request.Scope != EBlueprintHelperReplaceScope::CustomEventBody &&
		Request.Scope != EBlueprintHelperReplaceScope::EventBody &&
		Request.Scope != EBlueprintHelperReplaceScope::FunctionBody)
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("replace_external_body_scope_unsupported"), TEXT("replace_external_body only supports custom_event_body, event_body, and function_body."), TEXT("scope"), TEXT("payload.scope"));
		return false;
	}
	if (!Request.bRequireFullDryRun)
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("require_full_dry_run_required"), TEXT("replace_external_body requires require_full_dry_run=true."), TEXT("require_full_dry_run"), TEXT("payload.require_full_dry_run"));
		return false;
	}
	if (!Request.Body.IsValid())
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("logic_spec_required"), TEXT("replace_external_body requires body BlueprintLogicSpec."), TEXT("body"), TEXT("payload.body"));
		return false;
	}
	if (Request.ExpectedBodyFingerprint.IsEmpty())
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("expected_body_fingerprint_required"), TEXT("replace_external_body requires expected_body_fingerprint."), TEXT("expected_body_fingerprint"), TEXT("payload.expected_body_fingerprint"));
		return false;
	}
	if (!Request.AnchorParseError.IsEmpty())
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, Request.AnchorParseError, TEXT("replace_external_body requires a BlueprintHelper.ExternalGraphAnchor.v1 anchor."), TEXT("anchor"), TEXT("payload.anchor"));
		return false;
	}
	if (Request.Anchor.SemanticRole != EBlueprintHelperExternalGraphAnchorRole::BodyEntry)
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("external_anchor_role_unsupported"), TEXT("replace_external_body requires a semantic_role=body_entry external anchor."), TEXT("anchor.semantic_role"), TEXT("payload.anchor.semantic_role"));
		return false;
	}
	if (!Request.Anchor.AssetPath.Equals(Request.AssetPath, ESearchCase::IgnoreCase) ||
		!Request.Anchor.GraphName.Equals(Request.GraphName, ESearchCase::IgnoreCase))
	{
		BlueprintHelperReplaceExternalBody::AddError(OutResult, TEXT("external_anchor_target_mismatch"), TEXT("replace_external_body target must match anchor asset_path and graph_name."), TEXT("anchor"), TEXT("payload.anchor"));
		return false;
	}

	Context.Blueprint = BlueprintHelperReplaceExternalBody::FindBlueprint(Request.AssetPath);
	Context.Graph = BlueprintHelperReplaceExternalBody::FindGraphByName(Context.Blueprint, Request.GraphName);
	if (!Context.Blueprint || !Context.Graph)
	{
		BlueprintHelperReplaceExternalBody::AddError(
			OutResult,
			Context.Blueprint ? TEXT("target_graph_not_found") : TEXT("target_blueprint_not_found"),
			Context.Blueprint ? TEXT("Target graph was not found.") : TEXT("Target blueprint was not found."),
			Context.Blueprint ? TEXT("target.graph") : TEXT("target.asset_path"),
			Context.Blueprint ? TEXT("payload.target.graph") : TEXT("payload.target.asset_path"));
		return false;
	}

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Request.Body, Context.Blueprint, SemanticIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : SemanticIR.Diagnostics)
	{
		if (Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			BlueprintHelperReplaceExternalBody::AddError(OutResult, Diagnostic.Code, Diagnostic.Message, Diagnostic.Path, TEXT("payload.body"));
		}
	}
	if (!OutResult.bPassed)
	{
		return false;
	}

	FString AnchorResolveError;
	const FBlueprintHelperExternalGraphAnchorResolver AnchorResolver;
	if (!AnchorResolver.ResolveNode(Request.Anchor, Context.EntryNode, AnchorResolveError))
	{
		BlueprintHelperReplaceExternalBody::AddConflict(
			OutResult,
			AnchorResolveError.IsEmpty() ? TEXT("external_anchor_resolve_failed") : AnchorResolveError,
			TEXT("External body entry anchor is stale or cannot be resolved."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}
	if (!BlueprintHelperReplaceExternalBody::ScopeMatchesEntryClass(Request.Scope, Context.EntryNode))
	{
		BlueprintHelperReplaceExternalBody::AddConflict(
			OutResult,
			TEXT("external_body_scope_mismatch"),
			TEXT("External body entry node class does not match requested scope."),
			TEXT("scope"),
			TEXT("payload.scope"));
		return false;
	}

	FString SnapshotError;
	if (!SnapshotService.CaptureBody(Context.Graph, Context.EntryNode, Context.BeforeSnapshot, SnapshotError))
	{
		BlueprintHelperReplaceExternalBody::AddConflict(
			OutResult,
			SnapshotError.IsEmpty() ? TEXT("external_body_snapshot_failed") : SnapshotError,
			TEXT("External body snapshot failed."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}
	if (!Context.BeforeSnapshot.BodyFingerprint.Equals(Request.ExpectedBodyFingerprint, ESearchCase::IgnoreCase))
	{
		BlueprintHelperReplaceExternalBody::AddConflict(
			OutResult,
			TEXT("expected_body_fingerprint_mismatch"),
			FString::Printf(TEXT("expected_body_fingerprint '%s' does not match current body fingerprint '%s'."),
				*Request.ExpectedBodyFingerprint,
				*Context.BeforeSnapshot.BodyFingerprint),
			TEXT("expected_body_fingerprint"),
			TEXT("payload.expected_body_fingerprint"));
		return false;
	}

	FString AnalysisError;
	if (!DependentsAnalysisService.Analyze(Context.Graph, Context.BeforeSnapshot, Context.DependentsAnalysis, AnalysisError))
	{
		BlueprintHelperReplaceExternalBody::AddConflict(
			OutResult,
			AnalysisError.IsEmpty() ? TEXT("external_dependents_analysis_failed") : AnalysisError,
			TEXT("External dependents analysis failed."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}
	if (!Context.DependentsAnalysis.bSupported)
	{
		BlueprintHelperReplaceExternalBody::AddConflict(
			OutResult,
			TEXT("unsupported_external_dependents"),
			TEXT("External body has outgoing dependent links that cannot be safely restored."),
			TEXT("body"),
			TEXT("payload.anchor"));
		return false;
	}

	return OutResult.bPassed;
}

FBlueprintHelperToolResultBase FBlueprintHelperReplaceExternalBodyService::Execute(const TSharedRef<FJsonObject>& Payload) const
{
	const FReplaceExternalBodyRequest Request = ParseRequest(Payload);
	return FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
		Request.bDryRun
			? EBlueprintHelperGraphWriteUnitOfWorkMode::Preview
			: EBlueprintHelperGraphWriteUnitOfWorkMode::Execute,
		TEXT("replace_external_body"),
		TEXT("replace_external_body"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		[this, &Request]()
		{
			return Request.bDryRun ? ExecuteDryRun(Request) : ExecuteWrite(Request);
		});
}

FBlueprintHelperToolResultBase FBlueprintHelperReplaceExternalBodyService::ExecuteDryRun(
	const FReplaceExternalBodyRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FReplaceExternalBodyContext Context;
	FReplaceExternalBodyPreflightResult PreflightResult;
	Preflight(Request, Context, PreflightResult);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		BlueprintHelperReplaceExternalBody::OperationName,
		TraceId);
	Result.Data = BlueprintHelperReplaceExternalBody::MakeDryRunData(PreflightResult, Request, Context);
	Result.bOk = PreflightResult.bPassed;
	if (!PreflightResult.bPassed)
	{
		Result.Status = EBlueprintHelperToolStatus::Failed;
		Result.Error = BlueprintHelperReplaceExternalBody::MakeErrorFromPreflight(PreflightResult);
	}
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperReplaceExternalBodyService::ExecuteWrite(
	const FReplaceExternalBodyRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FReplaceExternalBodyContext Context;
	FReplaceExternalBodyPreflightResult PreflightResult;
	if (!Preflight(Request, Context, PreflightResult))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperReplaceExternalBody::OperationName,
			TraceId,
			BlueprintHelperReplaceExternalBody::MakeErrorFromPreflight(PreflightResult));
		Result.Data = BlueprintHelperReplaceExternalBody::MakeDryRunData(PreflightResult, Request, Context);
		return Result;
	}

	FString ApplyErrorCode;
	FString ApplyErrorMessage;
	if (!ApplyReplacement(Request, Context, ApplyErrorCode, ApplyErrorMessage))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperReplaceExternalBody::OperationName,
			TraceId,
			BlueprintHelperReplaceExternalBody::MakeToolError(
				ApplyErrorCode.IsEmpty() ? TEXT("external_body_replace_failed") : ApplyErrorCode,
				EBlueprintHelperToolStage::Execute,
				ApplyErrorMessage.IsEmpty() ? TEXT("replace_external_body execution failed.") : ApplyErrorMessage,
				TEXT("payload"),
				EBlueprintHelperRollbackResult::RolledBack));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		BlueprintHelperReplaceExternalBody::OperationName,
		TraceId);
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("ReplaceExternalBody.v1"));
	Data->SetObjectField(TEXT("external_body_before_snapshot"), Context.BeforeSnapshot.ToJson());
	TSharedRef<FJsonObject> AfterSnapshotJson = Context.AfterSnapshot.ToJson();
	if (!Context.ReplacementBlockId.IsEmpty())
	{
		AfterSnapshotJson->SetStringField(TEXT("replacement_block_id"), Context.ReplacementBlockId);
	}
	Data->SetObjectField(TEXT("external_body_after_snapshot"), AfterSnapshotJson);
	Data->SetObjectField(TEXT("dependents_analysis"), Context.DependentsAnalysis.ToJson());
	Data->SetStringField(TEXT("replacement_block_id"), Context.ReplacementBlockId);
	Result.Data = Data;
	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = true;
	Validation.bShouldSave = true;
	Result.Validation = Validation;
	return Result;
}

bool FBlueprintHelperReplaceExternalBodyService::ApplyReplacement(
	const FReplaceExternalBodyRequest& Request,
	FReplaceExternalBodyContext& Context,
	FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	if (!Context.Blueprint || !Context.Graph || !Context.EntryNode)
	{
		OutErrorCode = TEXT("external_body_replace_context_missing");
		OutErrorMessage = TEXT("replace_external_body context is incomplete.");
		return false;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Replace External Body")),
		Context.Blueprint);
	Mutation.Modify(Context.Graph);
	Mutation.Modify(Context.EntryNode);

	TArray<UEdGraphNode*> BodyNodes = SnapshotService.CollectBodyNodes(Context.Graph, Context.EntryNode);
	for (UEdGraphNode* Node : BodyNodes)
	{
		Mutation.Modify(Node);
	}
	for (UEdGraphNode* Node : BodyNodes)
	{
		if (Node)
		{
			FBlueprintEditorUtils::RemoveNode(Context.Blueprint, Node, true);
		}
	}

	const TSet<UEdGraphNode*> NodesBeforeImport = BlueprintHelperReplaceExternalBody::CaptureGraphNodes(Context.Graph);
	const FString GraphWritePayload = BlueprintHelperReplaceExternalBody::BuildSemanticPayload(Request);
	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
	FBlueprintHelperGraphWriteConnectivityContextInput ContextInput;
	ContextInput.RuntimeAdapterId = TEXT("k2.external_graph.replace_body");
	ContextInput.TaskSpecStrategy = TEXT("replace_external_body");
	ContextInput.TargetAssetPath = Context.Blueprint ? Context.Blueprint->GetPathName() : Request.AssetPath;
	ContextInput.GraphName = Context.Graph ? Context.Graph->GetName() : Request.GraphName;
	ContextInput.GraphFamily = TEXT("k2");
	ContextInput.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;
	ContextInput.EntryNodeRefs.Add(TEXT("external_body_entry"));
	ContextInput.EntryNodes.Add(Context.EntryNode);
	const FBlueprintGraphWriteConnectivityValidationInput ConnectivityInput =
		FBlueprintHelperGraphWriteConnectivityContextBuilder::Build(Context.Graph, ContextInput);
	const FBlueprintGenerateResult GenerateResult =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
			Context.Graph,
			GraphWritePayload,
			UnresolvedNodes,
			ConnectivityInput);
	Context.GeneratedNodes = BlueprintHelperReplaceExternalBody::CollectNewNodes(Context.Graph, NodesBeforeImport);
	if (!GenerateResult.bSucceed &&
		!BlueprintHelperReplaceExternalBody::CanDeferEntryResolvedConnectivityFailure(GenerateResult, Context.GeneratedNodes))
	{
		Mutation.Rollback();
		OutErrorCode = TEXT("semantic_graph_write_failed");
		OutErrorMessage = GenerateResult.Message.IsEmpty()
			? TEXT("Failed to create replacement external body through SemanticIR.")
			: GenerateResult.Message;
		OutErrorMessage += FString::Printf(
			TEXT(" Link stats: requested=%d created=%d generated_nodes=%d."),
			GenerateResult.RequestedConnectionCount,
			GenerateResult.CreatedConnectionCount,
			GenerateResult.GeneratedNodeCount);
		BlueprintHelperReplaceExternalBody::AppendDiagnosticsSummary(
			OutErrorMessage,
			TEXT("Connection diagnostics"),
			GenerateResult.ConnectionDiagnostics);
		BlueprintHelperReplaceExternalBody::AppendDiagnosticsSummary(
			OutErrorMessage,
			TEXT("Connectivity diagnostics"),
			GenerateResult.ConnectivityDiagnostics);
		if (UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
		{
			OutErrorMessage += FString::Printf(
				TEXT(" First unresolved: %s - %s"),
				*UnresolvedNodes[0]->DisplayText,
				*UnresolvedNodes[0]->Reason);
		}
		return false;
	}

	FString ReconnectError;
	if (!BlueprintHelperReplaceExternalBody::ReconnectEntryToGeneratedBody(
		Context.Graph,
		Context.EntryNode,
		Context.GeneratedNodes,
		ReconnectError))
	{
		Mutation.Rollback();
		OutErrorCode = TEXT("entry_reconnect_failed");
		OutErrorMessage = ReconnectError;
		return false;
	}

	const FString BlockRef = BlockIdService.MakeBlockRef(Context.Blueprint, Context.Graph, TEXT("ExternalBodyReplace"));
	Context.ReplacementBlockId = BlockIdService.MakeFullBlockId(Request.GraphName, BlockRef);
	if (Context.ReplacementBlockId.IsEmpty())
	{
		Context.ReplacementBlockId = BlockRef;
	}
	FString OwnershipError;
	if (Context.GeneratedNodes.Num() > 0 &&
		!OwnershipService.WriteBlockOwnership(
			Context.Blueprint,
			Context.GeneratedNodes,
			Context.ReplacementBlockId,
			Request.FeatureName.IsEmpty() ? TEXT("ExternalBodyReplace") : Request.FeatureName,
			OwnershipError))
	{
		Mutation.Rollback();
		OutErrorCode = TEXT("ownership_write_failed");
		OutErrorMessage = OwnershipError;
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
	if (Context.Blueprint->GetOutermost())
	{
		Context.Blueprint->GetOutermost()->MarkPackageDirty();
	}
	FString SnapshotError;
	if (!SnapshotService.CaptureBody(Context.Graph, Context.EntryNode, Context.AfterSnapshot, SnapshotError))
	{
		Mutation.Rollback();
		OutErrorCode = SnapshotError.IsEmpty() ? TEXT("external_body_after_snapshot_failed") : SnapshotError;
		OutErrorMessage = TEXT("replace_external_body applied but failed to capture after body snapshot.");
		return false;
	}
	Mutation.Commit();
	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Context.Graph, Context.GeneratedNodes);
	return true;
}
