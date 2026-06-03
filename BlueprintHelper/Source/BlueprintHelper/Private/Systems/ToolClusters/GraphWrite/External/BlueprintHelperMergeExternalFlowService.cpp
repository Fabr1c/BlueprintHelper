// BlueprintHelper Service Layer - Merge external flow service.

#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperMergeExternalFlowService.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorResolver.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Utils/GraphWriteCoreUtils.h"

namespace BlueprintHelperMergeExternalFlow
{
	static constexpr const TCHAR* OperationName = TEXT("merge_external_flow");

	static FString DirectionToString(const EEdGraphPinDirection Direction)
	{
		switch (Direction)
		{
		case EGPD_Input:
			return TEXT("input");
		case EGPD_Output:
			return TEXT("output");
		default:
			return TEXT("unknown");
		}
	}

	static FString NodeGuidString(const UEdGraphNode* Node)
	{
		return Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
	}

	static FBlueprintHelperExternalBoundaryEndpoint MakeEndpoint(const UEdGraphPin* Pin)
	{
		FBlueprintHelperExternalBoundaryEndpoint Endpoint;
		const UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
		Endpoint.NodeGuid = NodeGuidString(Node);
		Endpoint.PinName = Pin ? Pin->PinName.ToString() : FString();
		Endpoint.PinDirection = Pin ? DirectionToString(Pin->Direction) : FString();
		return Endpoint;
	}

	static TArray<FBlueprintHelperExternalBoundaryLink> CaptureBoundaryLinks(const UEdGraphPin* AnchorPin)
	{
		TArray<FBlueprintHelperExternalBoundaryLink> Links;
		if (!AnchorPin)
		{
			return Links;
		}

		for (const UEdGraphPin* LinkedPin : AnchorPin->LinkedTo)
		{
			if (!LinkedPin)
			{
				continue;
			}

			FBlueprintHelperExternalBoundaryLink Link;
			Link.From = MakeEndpoint(AnchorPin);
			Link.To = MakeEndpoint(LinkedPin);
			Links.Add(Link);
		}
		Links.Sort([](const FBlueprintHelperExternalBoundaryLink& A, const FBlueprintHelperExternalBoundaryLink& B)
		{
			return A.ToStableString() < B.ToStableString();
		});
		return Links;
	}

	static TSharedRef<FJsonObject> MakeTargetJson(const FString& AssetPath, const FString& GraphName, const FString& InsertStrategy)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);
		Target->SetStringField(TEXT("insert_strategy"), InsertStrategy);
		return Target;
	}

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
		FBlueprintHelperMergeExternalFlowService::FMergeExternalFlowPreflightResult& Result,
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
		FBlueprintHelperMergeExternalFlowService::FMergeExternalFlowPreflightResult& Result,
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

	static TSharedRef<FJsonObject> MakeDryRunData(
		const FBlueprintHelperMergeExternalFlowService::FMergeExternalFlowPreflightResult& Preflight,
		const FBlueprintHelperExternalBoundaryRelation& Relation)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("MergeExternalFlowDryRun.v1"));

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), Preflight.bPassed ? TEXT("passed") : TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), Preflight.bPassed);
		DryRun->SetArrayField(TEXT("blocked_by"), MakeStringArray(Preflight.BlockedBy));
		DryRun->SetArrayField(TEXT("conflicts"), MakeIssueArray(Preflight.Conflicts));
		DryRun->SetArrayField(TEXT("errors"), MakeIssueArray(Preflight.Errors));
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		Data->SetObjectField(TEXT("external_boundary_relation"), Relation.ToJson());
		return Data;
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
		const FBlueprintHelperMergeExternalFlowService::FMergeExternalFlowPreflightResult& Preflight)
	{
		const FBlueprintHelperDryRunIssue* FirstIssue = Preflight.Conflicts.Num() > 0
			? &Preflight.Conflicts[0]
			: (Preflight.Errors.Num() > 0 ? &Preflight.Errors[0] : nullptr);
		return MakeToolError(
			Preflight.BlockedBy.Num() > 0 ? Preflight.BlockedBy[0] : TEXT("preflight_failed"),
			EBlueprintHelperToolStage::Preflight,
			FirstIssue && !FirstIssue->Message.IsEmpty()
				? FirstIssue->Message
				: TEXT("merge_external_flow preflight blocked execution."),
			FirstIssue && !FirstIssue->Source.IsEmpty()
				? FirstIssue->Source
				: TEXT("payload"));
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

	static bool IsGeneratedNode(const TSet<UEdGraphNode*>& GeneratedSet, const UEdGraphPin* Pin)
	{
		const UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
		return Node && GeneratedSet.Contains(const_cast<UEdGraphNode*>(Node));
	}

	static UEdGraphPin* FindBodyEntryPin(const TArray<UEdGraphNode*>& GeneratedNodes)
	{
		TSet<UEdGraphNode*> GeneratedSet;
		for (UEdGraphNode* Node : GeneratedNodes)
		{
			GeneratedSet.Add(Node);
		}

		for (UEdGraphNode* Node : GeneratedNodes)
		{
			if (!Node)
			{
				continue;
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || !UGraphWriteCoreUtils::IsExecPin(Pin))
				{
					continue;
				}

				bool bHasGeneratedIncoming = false;
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (IsGeneratedNode(GeneratedSet, LinkedPin))
					{
						bHasGeneratedIncoming = true;
						break;
					}
				}
				if (!bHasGeneratedIncoming)
				{
					return Pin;
				}
			}
		}
		return nullptr;
	}

	static TArray<UEdGraphPin*> FindBodyExitPins(const TArray<UEdGraphNode*>& GeneratedNodes)
	{
		TSet<UEdGraphNode*> GeneratedSet;
		for (UEdGraphNode* Node : GeneratedNodes)
		{
			GeneratedSet.Add(Node);
		}

		TArray<UEdGraphPin*> ExitPins;
		for (UEdGraphNode* Node : GeneratedNodes)
		{
			if (!Node)
			{
				continue;
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || !UGraphWriteCoreUtils::IsExecPin(Pin))
				{
					continue;
				}

				bool bLinksToGenerated = false;
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (IsGeneratedNode(GeneratedSet, LinkedPin))
					{
						bLinksToGenerated = true;
						break;
					}
				}
				if (!bLinksToGenerated)
				{
					ExitPins.Add(Pin);
				}
			}
		}
		return ExitPins;
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
			UEdGraphNode* Node = Stack.Pop(EAllowShrinking::No);
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

	static bool CanDeferAnchorResolvedConnectivityFailure(
		const FBlueprintGenerateResult& GenerateResult,
		const TArray<UEdGraphNode*>& GeneratedNodes,
		UEdGraphPin* BodyEntryPin)
	{
		if (GenerateResult.bSucceed
			|| GenerateResult.UnresolvedNodeCount != 0
			|| GenerateResult.ConnectivityDiagnostics.Num() == 0)
		{
			return false;
		}

		for (const FBlueprintGeneratorDiagnostic& Diagnostic : GenerateResult.ConnectivityDiagnostics)
		{
			if (Diagnostic.Code != TEXT("unreachable_exec_node"))
			{
				return false;
			}
		}
		return AreGeneratedExecNodesReachableFromBodyEntry(GeneratedNodes, BodyEntryPin);
	}

	static void RemoveGeneratedNodes(
		UBlueprint* Blueprint,
		const FBlueprintHelperMergeExternalFlowService::FMergeExternalFlowContext& Context)
	{
		TArray<UEdGraphNode*> NodesToRemove = Context.GeneratedNodes;
		if (Context.SequenceNode)
		{
			NodesToRemove.AddUnique(Context.SequenceNode);
		}

		for (UEdGraphNode* Node : NodesToRemove)
		{
			if (Node)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}
	}

	static void RestoreAnchorLinks(
		UEdGraph* Graph,
		UEdGraphPin* AnchorPin,
		const TArray<UEdGraphPin*>& BeforeSuccessors)
	{
		if (!Graph || !AnchorPin)
		{
			return;
		}

		TArray<UEdGraphPin*> CurrentLinks = AnchorPin->LinkedTo;
		for (UEdGraphPin* LinkedPin : CurrentLinks)
		{
			FString IgnoredError;
			bool bIgnoredChanged = false;
			UGraphWriteCoreUtils::TryBreakLink(AnchorPin, LinkedPin, IgnoredError, bIgnoredChanged);
		}

		for (UEdGraphPin* Successor : BeforeSuccessors)
		{
			if (!Successor)
			{
				continue;
			}
			FString IgnoredError;
			bool bIgnoredChanged = false;
			UGraphWriteCoreUtils::TrySchemaConnect(Graph, AnchorPin, Successor, IgnoredError, bIgnoredChanged);
		}
	}

	static FString BuildSemanticPayload(const FBlueprintHelperMergeExternalFlowService::FMergeExternalFlowRequest& Request)
	{
		FBlueprintHelperGraphWriteSemanticPayload Payload;
		Payload.TargetAssetPath = Request.AssetPath;
		Payload.TargetGraph = Request.GraphName;
		Payload.Mode = TEXT("append");
		Payload.bDryRun = Request.bDryRun;
		Payload.LogicSpec = Request.LogicSpec;
		return Payload.ToJsonString();
	}
}

FBlueprintHelperMergeExternalFlowService::FBlueprintHelperMergeExternalFlowService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperOwnershipService& InOwnershipService,
	const FBlueprintHelperLogicJsonPathService& InPathService)
	: Resolver(InResolver)
	, BlockIdService(InBlockIdService)
	, OwnershipService(InOwnershipService)
	, PathService(InPathService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperMergeExternalFlowService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FMergeExternalFlowRequest Request = ParseRequest(Payload);
	return Request.bDryRun ? ExecuteDryRun(Request) : ExecuteWrite(Request);
}

FBlueprintHelperMergeExternalFlowService::FMergeExternalFlowRequest
FBlueprintHelperMergeExternalFlowService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FMergeExternalFlowRequest Request;
	if (!Payload.IsValid())
	{
		return Request;
	}

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		if (!(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName))
		{
			(*TargetObject)->TryGetStringField(TEXT("graph_name"), Request.GraphName);
		}

		FString TargetInsertStrategy;
		if ((*TargetObject)->TryGetStringField(TEXT("insert_strategy"), TargetInsertStrategy))
		{
			ParseInsertStrategy(TargetInsertStrategy, Request.InsertStrategy);
		}
	}

	FString InsertStrategy;
	if (Payload->TryGetStringField(TEXT("insert_strategy"), InsertStrategy))
	{
		ParseInsertStrategy(InsertStrategy, Request.InsertStrategy);
	}

	const TSharedPtr<FJsonObject>* AnchorObject = nullptr;
	if (!Payload->TryGetObjectField(TEXT("anchor"), AnchorObject) ||
		!AnchorObject || !AnchorObject->IsValid())
	{
		Request.AnchorParseError = TEXT("external_anchor_schema_unsupported");
	}
	else
	{
		FString AnchorSchema;
		(*AnchorObject)->TryGetStringField(TEXT("schema"), AnchorSchema);
		if (AnchorSchema == FBlueprintHelperLogicJsonAnchorSelector::SchemaString)
		{
			Request.bHasAnchorSelector = true;
			FBlueprintHelperLogicJsonAnchorSelector::FromJson(
				*AnchorObject,
				Request.AnchorSelector,
				Request.AnchorParseError);
		}
		else if (!FBlueprintHelperExternalGraphAnchor::FromJson(*AnchorObject, Request.Anchor, Request.AnchorParseError))
		{
			if (Request.AnchorParseError.IsEmpty())
			{
				Request.AnchorParseError = TEXT("external_anchor_schema_unsupported");
			}
		}
	}

	const TSharedPtr<FJsonObject>* InsertedObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("inserted"), InsertedObject) && InsertedObject && InsertedObject->IsValid())
	{
		(*InsertedObject)->TryGetStringField(TEXT("block_id"), Request.InsertedBlockId);
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if ((*InsertedObject)->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject && BodyObject->IsValid())
		{
			Request.LogicSpec = *BodyObject;
		}
	}

	Payload->TryGetStringField(TEXT("feature_name"), Request.FeatureName);
	Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

	const TArray<TSharedPtr<FJsonValue>>* SequenceOrder = nullptr;
	if (Payload->TryGetArrayField(TEXT("sequence_order"), SequenceOrder) && SequenceOrder)
	{
		for (const TSharedPtr<FJsonValue>& Value : *SequenceOrder)
		{
			FString Item;
			if (Value.IsValid() && Value->TryGetString(Item))
			{
				Request.SequenceOrder.Add(Item);
			}
			else
			{
				Request.bSequenceOrderHadInvalidEntry = true;
			}
		}
	}

	return Request;
}

bool FBlueprintHelperMergeExternalFlowService::ResolveTarget(
	const FMergeExternalFlowRequest& Request,
	FMergeExternalFlowContext& Context,
	FMergeExternalFlowPreflightResult& OutResult) const
{
	if (Request.AssetPath.IsEmpty())
	{
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("target_blueprint_not_found"),
			TEXT("merge_external_flow requires target.asset_path."),
			TEXT("target.asset_path"),
			TEXT("payload.target.asset_path"));
		return false;
	}
	if (Request.GraphName.IsEmpty())
	{
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("target_graph_not_found"),
			TEXT("merge_external_flow requires target.graph."),
			TEXT("target.graph"),
			TEXT("payload.target.graph"));
		return false;
	}

	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diagnostics;
	Context.Blueprint = Resolver.ResolveBlueprint(Target, Diagnostics);
	if (!Context.Blueprint)
	{
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("target_blueprint_not_found"),
			FString::Printf(TEXT("target blueprint not found: %s"), *Request.AssetPath),
			TEXT("target.asset_path"),
			TEXT("payload.target.asset_path"));
		return false;
	}

	Context.Graph = BlueprintHelperMergeExternalFlow::FindGraphByName(Context.Blueprint, Request.GraphName);
	if (!Context.Graph)
	{
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("target_graph_not_found"),
			FString::Printf(TEXT("target graph not found: %s"), *Request.GraphName),
			TEXT("target.graph"),
			TEXT("payload.target.graph"));
		return false;
	}

	return true;
}

bool FBlueprintHelperMergeExternalFlowService::ResolveRequestAnchor(
	const FMergeExternalFlowRequest& Request,
	const FMergeExternalFlowContext& Context,
	FBlueprintHelperExternalGraphAnchor& OutAnchor,
	FString& OutErrorCode,
	FString& OutErrorMessage,
	FString& OutErrorTarget,
	FString& OutErrorSource,
	bool& bOutConflict) const
{
	bOutConflict = false;
	OutAnchor = FBlueprintHelperExternalGraphAnchor();
	if (!Request.AnchorParseError.IsEmpty())
	{
		OutErrorCode = Request.AnchorParseError;
		OutErrorMessage = Request.bHasAnchorSelector
			? TEXT("merge_external_flow requires a valid BlueprintHelper.LogicJsonAnchorSelector.v1 anchor selector.")
			: TEXT("merge_external_flow requires a BlueprintHelper.ExternalGraphAnchor.v1 anchor.");
		OutErrorTarget = TEXT("anchor");
		OutErrorSource = TEXT("payload.anchor");
		return false;
	}

	if (!Request.bHasAnchorSelector)
	{
		OutAnchor = Request.Anchor;
		return true;
	}

	const FBlueprintHelperLogicJsonAnchorSelector& Selector = Request.AnchorSelector;
	if (!Selector.AssetPath.Equals(Request.AssetPath, ESearchCase::IgnoreCase) ||
		!Selector.GraphName.Equals(Request.GraphName, ESearchCase::IgnoreCase))
	{
		OutErrorCode = TEXT("external_anchor_target_mismatch");
		OutErrorMessage = TEXT("logic_json anchor selector asset_path and graph_name must match target.");
		OutErrorTarget = TEXT("anchor");
		OutErrorSource = TEXT("payload.anchor");
		return false;
	}

	UEdGraphPin* SelectedPin = nullptr;
	if (!Selector.NodeRef.IsEmpty())
	{
		UEdGraphNode* SelectedNode = nullptr;
		FBlueprintHelperPatchResolveError ResolveError;
		if (!PathService.ResolveNode(Context.Graph, Selector.NodeRef, FString(), SelectedNode, ResolveError) || !SelectedNode)
		{
			bOutConflict = true;
			OutErrorCode = ResolveError.Code.IsEmpty() ? TEXT("target_node_not_found") : ResolveError.Code;
			OutErrorMessage = ResolveError.Message.IsEmpty()
				? TEXT("logic_json anchor selector node_ref could not be resolved.")
				: ResolveError.Message;
			OutErrorTarget = TEXT("anchor.node_ref");
			OutErrorSource = TEXT("payload.anchor.node_ref");
			return false;
		}

		ResolveError = FBlueprintHelperPatchResolveError();
		if (!PathService.ResolvePin(Context.Graph, SelectedNode, Selector.PinRef, FString(), SelectedPin, ResolveError) || !SelectedPin)
		{
			bOutConflict = true;
			OutErrorCode = ResolveError.Code.IsEmpty() ? TEXT("target_pin_not_found") : ResolveError.Code;
			OutErrorMessage = ResolveError.Message.IsEmpty()
				? TEXT("logic_json anchor selector pin_ref could not be resolved.")
				: ResolveError.Message;
			OutErrorTarget = TEXT("anchor.pin_ref");
			OutErrorSource = TEXT("payload.anchor.pin_ref");
			return false;
		}
	}
	else if (!Selector.LinkRef.IsEmpty())
	{
		FBlueprintHelperResolvedLink ResolvedLink;
		FBlueprintHelperPatchResolveError ResolveError;
		if (!PathService.ResolveLink(Context.Graph, Selector.LinkRef, FString(), ResolvedLink, ResolveError) ||
			!ResolvedLink.SourcePin)
		{
			bOutConflict = true;
			OutErrorCode = ResolveError.Code.IsEmpty() ? TEXT("target_link_not_found") : ResolveError.Code;
			OutErrorMessage = ResolveError.Message.IsEmpty()
				? TEXT("logic_json anchor selector link_ref could not be resolved.")
				: ResolveError.Message;
			OutErrorTarget = TEXT("anchor.link_ref");
			OutErrorSource = TEXT("payload.anchor.link_ref");
			return false;
		}
		SelectedPin = ResolvedLink.SourcePin;
	}
	else
	{
		OutErrorCode = TEXT("external_anchor_selector_invalid");
		OutErrorMessage = TEXT("logic_json anchor selector requires node_ref or link_ref.");
		OutErrorTarget = TEXT("anchor");
		OutErrorSource = TEXT("payload.anchor");
		return false;
	}

	FString BuildError;
	const FBlueprintHelperExternalGraphAnchorService AnchorService;
	if (!AnchorService.BuildExecBoundaryAnchor(Request.AssetPath, Request.GraphName, SelectedPin, OutAnchor, BuildError))
	{
		bOutConflict = true;
		OutErrorCode = BuildError.IsEmpty() ? TEXT("external_anchor_pin_not_found") : BuildError;
		OutErrorMessage = TEXT("logic_json anchor selector must resolve to an external exec output pin.");
		OutErrorTarget = TEXT("anchor.pin_ref");
		OutErrorSource = TEXT("payload.anchor");
		return false;
	}

	return true;
}

bool FBlueprintHelperMergeExternalFlowService::Preflight(
	const FMergeExternalFlowRequest& Request,
	FMergeExternalFlowContext& Context,
	FMergeExternalFlowPreflightResult& OutResult) const
{
	if (!ResolveTarget(Request, Context, OutResult))
	{
		return false;
	}

	Context.Relation.InsertedBlockId = Request.InsertedBlockId;

	FBlueprintHelperExternalGraphAnchor EffectiveAnchor;
	FString AnchorErrorCode;
	FString AnchorErrorMessage;
	FString AnchorErrorTarget;
	FString AnchorErrorSource;
	bool bAnchorConflict = false;
	if (!ResolveRequestAnchor(
		Request,
		Context,
		EffectiveAnchor,
		AnchorErrorCode,
		AnchorErrorMessage,
		AnchorErrorTarget,
		AnchorErrorSource,
		bAnchorConflict))
	{
		if (bAnchorConflict)
		{
			BlueprintHelperMergeExternalFlow::AddConflict(
				OutResult,
				AnchorErrorCode.IsEmpty() ? TEXT("external_anchor_pin_not_found") : AnchorErrorCode,
				AnchorErrorMessage.IsEmpty()
					? TEXT("external anchor selector could not be resolved.")
					: AnchorErrorMessage,
				AnchorErrorTarget.IsEmpty() ? TEXT("anchor") : AnchorErrorTarget,
				AnchorErrorSource.IsEmpty() ? TEXT("payload.anchor") : AnchorErrorSource);
		}
		else
		{
			BlueprintHelperMergeExternalFlow::AddError(
				OutResult,
				AnchorErrorCode.IsEmpty() ? TEXT("external_anchor_schema_unsupported") : AnchorErrorCode,
				AnchorErrorMessage.IsEmpty()
					? TEXT("merge_external_flow requires a BlueprintHelper.ExternalGraphAnchor.v1 anchor.")
					: AnchorErrorMessage,
				AnchorErrorTarget.IsEmpty() ? TEXT("anchor") : AnchorErrorTarget,
				AnchorErrorSource.IsEmpty() ? TEXT("payload.anchor") : AnchorErrorSource);
		}
		return false;
	}
	Context.Relation.Anchor = EffectiveAnchor;

	if (!EffectiveAnchor.AssetPath.Equals(Request.AssetPath, ESearchCase::IgnoreCase) ||
		!EffectiveAnchor.GraphName.Equals(Request.GraphName, ESearchCase::IgnoreCase))
	{
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("external_anchor_target_mismatch"),
			TEXT("external anchor asset_path and graph_name must match target."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}

	if (EffectiveAnchor.SemanticRole != EBlueprintHelperExternalGraphAnchorRole::ExecBoundary ||
		!EffectiveAnchor.PinDirection.Equals(TEXT("output"), ESearchCase::IgnoreCase))
	{
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("external_anchor_role_invalid"),
			TEXT("merge_external_flow requires an output exec_boundary anchor."),
			TEXT("anchor.semantic_role"),
			TEXT("payload.anchor"));
		return false;
	}

	FString AnchorResolveError;
	FBlueprintHelperExternalGraphAnchorResolver AnchorResolver;
	if (!AnchorResolver.ResolvePin(EffectiveAnchor, Context.AnchorPin, AnchorResolveError) || !Context.AnchorPin)
	{
		BlueprintHelperMergeExternalFlow::AddConflict(
			OutResult,
			AnchorResolveError.IsEmpty() ? TEXT("external_anchor_pin_not_found") : AnchorResolveError,
			TEXT("external anchor pin could not be resolved or fingerprint is stale."),
			TEXT("anchor"),
			TEXT("payload.anchor"));
		return false;
	}

	Context.AnchorNode = Context.AnchorPin->GetOwningNode();
	if (!Context.AnchorNode || Context.AnchorNode->GetGraph() != Context.Graph)
	{
		BlueprintHelperMergeExternalFlow::AddConflict(
			OutResult,
			TEXT("external_anchor_graph_not_found"),
			TEXT("external anchor resolved outside the target graph."),
			TEXT("anchor.graph_name"),
			TEXT("payload.anchor"));
		return false;
	}

	if (Context.AnchorPin->Direction != EGPD_Output || !UGraphWriteCoreUtils::IsExecPin(Context.AnchorPin))
	{
		BlueprintHelperMergeExternalFlow::AddConflict(
			OutResult,
			TEXT("anchor_pin_not_exec"),
			TEXT("external merge anchor must be an exec output pin."),
			TEXT("anchor.pin_name"),
			TEXT("payload.anchor"));
		return false;
	}

	for (UEdGraphPin* LinkedPin : Context.AnchorPin->LinkedTo)
	{
		if (LinkedPin && LinkedPin->Direction == EGPD_Input && UGraphWriteCoreUtils::IsExecPin(LinkedPin))
		{
			Context.Successors.Add(LinkedPin);
		}
	}
	Context.OriginalSuccessorPin = Context.Successors.Num() > 0 ? Context.Successors[0] : nullptr;
	Context.Relation.BeforeLinks = BlueprintHelperMergeExternalFlow::CaptureBoundaryLinks(Context.AnchorPin);

	switch (Request.InsertStrategy)
	{
	case EBlueprintHelperInsertStrategy::AppendAfter:
		if (Context.Successors.Num() > 0)
		{
			BlueprintHelperMergeExternalFlow::AddConflict(
				OutResult,
				TEXT("anchor_exec_pin_already_connected"),
				TEXT("append_after requires the external anchor exec pin to have no successor."),
				TEXT("anchor"),
				TEXT("payload.anchor"));
		}
		break;
	case EBlueprintHelperInsertStrategy::InsertBetween:
		if (Context.Successors.Num() == 0)
		{
			BlueprintHelperMergeExternalFlow::AddConflict(
				OutResult,
				TEXT("original_successor_not_found"),
				TEXT("insert_between requires the external anchor exec pin to have exactly one successor."),
				TEXT("anchor"),
				TEXT("payload.anchor"));
		}
		else if (Context.Successors.Num() > 1)
		{
			BlueprintHelperMergeExternalFlow::AddConflict(
				OutResult,
				TEXT("anchor_exec_pin_has_multiple_successors"),
				TEXT("insert_between requires the external anchor exec pin to have exactly one successor."),
				TEXT("anchor"),
				TEXT("payload.anchor"));
		}
		break;
	case EBlueprintHelperInsertStrategy::BranchFork:
		if (Context.Successors.Num() > 1)
		{
			BlueprintHelperMergeExternalFlow::AddConflict(
				OutResult,
				TEXT("anchor_exec_pin_has_multiple_successors"),
				TEXT("branch_fork requires the external anchor exec pin to have at most one successor."),
				TEXT("anchor"),
				TEXT("payload.anchor"));
		}
		if (Request.SequenceOrder.Num() == 0)
		{
			BlueprintHelperMergeExternalFlow::AddError(
				OutResult,
				TEXT("sequence_order_required"),
				TEXT("branch_fork requires explicit sequence_order."),
				TEXT("sequence_order"),
				TEXT("payload.sequence_order"));
		}
		break;
	default:
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("unsupported_insert_strategy"),
			TEXT("merge_external_flow supports append_after, insert_between, and branch_fork."),
			TEXT("insert_strategy"),
			TEXT("payload.insert_strategy"));
		break;
	}

	if (Request.InsertStrategy == EBlueprintHelperInsertStrategy::BranchFork)
	{
		int32 InsertedCount = 0;
		int32 OriginalCount = 0;
		for (const FString& Item : Request.SequenceOrder)
		{
			if (Item == TEXT("inserted_logic"))
			{
				++InsertedCount;
			}
			else if (Item == TEXT("original_successor"))
			{
				++OriginalCount;
			}
			else
			{
				BlueprintHelperMergeExternalFlow::AddError(
					OutResult,
					TEXT("sequence_order_invalid"),
					TEXT("branch_fork sequence_order entries must be inserted_logic or original_successor."),
					TEXT("sequence_order"),
					TEXT("payload.sequence_order"));
			}
		}
		if (Request.bSequenceOrderHadInvalidEntry ||
			Request.SequenceOrder.Num() > 2 ||
			InsertedCount != 1 ||
			OriginalCount > 1 ||
			(Context.OriginalSuccessorPin && OriginalCount != 1) ||
			(!Context.OriginalSuccessorPin && OriginalCount != 0))
		{
			BlueprintHelperMergeExternalFlow::AddError(
				OutResult,
				TEXT("sequence_order_invalid"),
				TEXT("branch_fork sequence_order must contain unique entries, include inserted_logic exactly once, and match the presence of an original successor."),
				TEXT("sequence_order"),
				TEXT("payload.sequence_order"));
		}
	}

	if (!Request.LogicSpec.IsValid())
	{
		BlueprintHelperMergeExternalFlow::AddError(
			OutResult,
			TEXT("inserted_logic_not_found"),
			TEXT("merge_external_flow requires inserted.body BlueprintLogicSpec."),
			TEXT("inserted.body"),
			TEXT("payload.inserted.body"));
		return false;
	}

	Context.FragmentDebugData = FBlueprintHelperGraphFragmentDebugData::BuildFromLogicSpec(
		Request.LogicSpec,
		Context.Blueprint);
	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Request.LogicSpec, Context.Blueprint, SemanticIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : SemanticIR.Diagnostics)
	{
		if (Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			BlueprintHelperMergeExternalFlow::AddError(
				OutResult,
				Diagnostic.Code,
				Diagnostic.Message,
				Diagnostic.Path,
				TEXT("payload.inserted.body"));
		}
	}

	return OutResult.bPassed;
}

FBlueprintHelperToolResultBase FBlueprintHelperMergeExternalFlowService::ExecuteDryRun(
	const FMergeExternalFlowRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FMergeExternalFlowContext Context;
	FMergeExternalFlowPreflightResult PreflightResult;
	Preflight(Request, Context, PreflightResult);

	FBlueprintHelperToolResultBase Result = PreflightResult.bPassed
		? FBlueprintHelperToolResultBuilder::DryRun(BlueprintHelperMergeExternalFlow::OperationName, TraceId)
		: FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperMergeExternalFlow::OperationName,
			TraceId,
			BlueprintHelperMergeExternalFlow::MakeErrorFromPreflight(PreflightResult));
	Result.CustomTargetJson = BlueprintHelperMergeExternalFlow::MakeTargetJson(
		Request.AssetPath,
		Request.GraphName,
		InsertStrategyToString(Request.InsertStrategy));
	Result.Data = BlueprintHelperMergeExternalFlow::MakeDryRunData(PreflightResult, Context.Relation);
	FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, Context.FragmentDebugData);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperMergeExternalFlowService::ExecuteWrite(
	const FMergeExternalFlowRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FMergeExternalFlowContext Context;
	FMergeExternalFlowPreflightResult PreflightResult;
	if (!Preflight(Request, Context, PreflightResult))
	{
		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperMergeExternalFlow::OperationName,
			TraceId,
			BlueprintHelperMergeExternalFlow::MakeErrorFromPreflight(PreflightResult));
		FailResult.CustomTargetJson = BlueprintHelperMergeExternalFlow::MakeTargetJson(
			Request.AssetPath,
			Request.GraphName,
			InsertStrategyToString(Request.InsertStrategy));
		FailResult.Data = BlueprintHelperMergeExternalFlow::MakeDryRunData(PreflightResult, Context.Relation);
		FBlueprintHelperGraphFragmentDebugData::AttachToData(FailResult.Data, Context.FragmentDebugData);
		return FailResult;
	}

	const FString BlockRef = Request.InsertedBlockId.IsEmpty()
		? BlockIdService.MakeBlockRef(Context.Blueprint, Context.Graph, TEXT("ExternalMerge"))
		: Request.InsertedBlockId;
	Context.Relation.InsertedBlockId = BlockIdService.MakeFullBlockId(Request.GraphName, BlockRef);
	if (Context.Relation.InsertedBlockId.IsEmpty())
	{
		Context.Relation.InsertedBlockId = BlockRef;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Merge External Flow")),
		Context.Blueprint);
	Mutation.Modify(Context.Graph);
	Mutation.Modify(Context.AnchorNode);
	for (UEdGraphPin* Successor : Context.Successors)
	{
		Mutation.Modify(Successor ? Successor->GetOwningNode() : nullptr);
	}

	const TSet<UEdGraphNode*> NodeSnapshot = BlueprintHelperMergeExternalFlow::CaptureGraphNodes(Context.Graph);
	const FString GraphWritePayload = BlueprintHelperMergeExternalFlow::BuildSemanticPayload(Request);
	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
	const FBlueprintGenerateResult GenerateResult =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(Context.Graph, GraphWritePayload, UnresolvedNodes);
	Context.GeneratedNodes = BlueprintHelperMergeExternalFlow::CollectNewNodes(Context.Graph, NodeSnapshot);
	Context.BodyEntryPin = BlueprintHelperMergeExternalFlow::FindBodyEntryPin(Context.GeneratedNodes);
	Context.BodyExitPins = BlueprintHelperMergeExternalFlow::FindBodyExitPins(Context.GeneratedNodes);
	if (!GenerateResult.bSucceed)
	{
		const bool bCanDeferConnectivityFailure =
			BlueprintHelperMergeExternalFlow::CanDeferAnchorResolvedConnectivityFailure(
				GenerateResult,
				Context.GeneratedNodes,
				Context.BodyEntryPin);
		if (!bCanDeferConnectivityFailure)
		{
			BlueprintHelperMergeExternalFlow::RemoveGeneratedNodes(Context.Blueprint, Context);
			Mutation.Rollback();

			FString Message = GenerateResult.Message;
			if (Message.IsEmpty())
			{
				Message = TEXT("inserted body graph generation failed.");
			}
			if (UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
			{
				Message += FString::Printf(
					TEXT(" First unresolved: %s - %s"),
					*UnresolvedNodes[0]->DisplayText,
					*UnresolvedNodes[0]->Reason);
			}
			return FBlueprintHelperToolResultBuilder::Failure(
				BlueprintHelperMergeExternalFlow::OperationName,
				TraceId,
				BlueprintHelperMergeExternalFlow::MakeToolError(
					TEXT("node_create_failed"),
					EBlueprintHelperToolStage::Execute,
					Message,
					TEXT("payload.inserted.body"),
					EBlueprintHelperRollbackResult::RolledBack));
		}
	}

	if (!Context.BodyEntryPin)
	{
		BlueprintHelperMergeExternalFlow::RemoveGeneratedNodes(Context.Blueprint, Context);
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperMergeExternalFlow::OperationName,
			TraceId,
			BlueprintHelperMergeExternalFlow::MakeToolError(
				TEXT("inserted_logic_has_no_exec_pins"),
				EBlueprintHelperToolStage::Execute,
				TEXT("inserted body must generate at least one exec input pin."),
				TEXT("payload.inserted.body"),
				EBlueprintHelperRollbackResult::RolledBack));
	}
	if (Request.InsertStrategy == EBlueprintHelperInsertStrategy::InsertBetween && Context.BodyExitPins.Num() != 1)
	{
		BlueprintHelperMergeExternalFlow::RemoveGeneratedNodes(Context.Blueprint, Context);
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperMergeExternalFlow::OperationName,
			TraceId,
			BlueprintHelperMergeExternalFlow::MakeToolError(
				TEXT("inserted_logic_has_no_exec_pins"),
				EBlueprintHelperToolStage::Execute,
				TEXT("insert_between requires inserted body to have exactly one open exec output."),
				TEXT("payload.inserted.body"),
				EBlueprintHelperRollbackResult::RolledBack));
	}

	FString ApplyErrorCode;
	FString ApplyErrorMessage;
	if (!ApplyExternalMerge(Request, Context, ApplyErrorCode, ApplyErrorMessage))
	{
		BlueprintHelperMergeExternalFlow::RestoreAnchorLinks(Context.Graph, Context.AnchorPin, Context.Successors);
		BlueprintHelperMergeExternalFlow::RemoveGeneratedNodes(Context.Blueprint, Context);
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperMergeExternalFlow::OperationName,
			TraceId,
			BlueprintHelperMergeExternalFlow::MakeToolError(
				ApplyErrorCode.IsEmpty() ? TEXT("link_create_failed") : ApplyErrorCode,
				EBlueprintHelperToolStage::Execute,
				ApplyErrorMessage.IsEmpty() ? TEXT("merge_external_flow link application failed.") : ApplyErrorMessage,
				TEXT("payload.anchor"),
				EBlueprintHelperRollbackResult::RolledBack));
	}

	TArray<UEdGraphNode*> OwnershipNodes = Context.GeneratedNodes;
	if (Context.SequenceNode)
	{
		OwnershipNodes.AddUnique(Context.SequenceNode);
	}
	FString OwnershipError;
	if (!OwnershipService.WriteBlockOwnership(
		Context.Blueprint,
		OwnershipNodes,
		Context.Relation.InsertedBlockId,
		Request.FeatureName,
		OwnershipError))
	{
		BlueprintHelperMergeExternalFlow::RestoreAnchorLinks(Context.Graph, Context.AnchorPin, Context.Successors);
		BlueprintHelperMergeExternalFlow::RemoveGeneratedNodes(Context.Blueprint, Context);
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperMergeExternalFlow::OperationName,
			TraceId,
			BlueprintHelperMergeExternalFlow::MakeToolError(
				TEXT("ownership_write_failed"),
				EBlueprintHelperToolStage::Execute,
				OwnershipError,
				TEXT("payload.inserted"),
				EBlueprintHelperRollbackResult::RolledBack));
	}

	Context.Relation.AfterLinks = BlueprintHelperMergeExternalFlow::CaptureBoundaryLinks(Context.AnchorPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
	if (Context.Blueprint->GetOutermost())
	{
		Context.Blueprint->GetOutermost()->MarkPackageDirty();
	}
	Mutation.Commit();

	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Applied(
		BlueprintHelperMergeExternalFlow::OperationName,
		TraceId);
	Success.CustomTargetJson = BlueprintHelperMergeExternalFlow::MakeTargetJson(
		Request.AssetPath,
		Request.GraphName,
		InsertStrategyToString(Request.InsertStrategy));

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("MergeExternalFlow.v1"));
	TSharedRef<FJsonObject> MergeResult = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> MergedRef = MakeShared<FJsonObject>();
	MergedRef->SetStringField(TEXT("graph_id"), Request.GraphName);
	MergedRef->SetStringField(
		TEXT("anchor_ref"),
		FString::Printf(
			TEXT("%s.%s"),
			Context.AnchorNode ? *Context.AnchorNode->GetName() : TEXT("?"),
			Context.AnchorPin ? *Context.AnchorPin->PinName.ToString() : TEXT("?")));
	MergedRef->SetStringField(TEXT("inserted_ref"), Context.Relation.InsertedBlockId);
	if (Context.SequenceNode)
	{
		MergedRef->SetStringField(TEXT("sequence_ref"), Context.SequenceNode->GetName());
	}
	MergeResult->SetObjectField(TEXT("merged_ref"), MergedRef);
	Data->SetObjectField(TEXT("merge_result"), MergeResult);
	Data->SetObjectField(TEXT("external_boundary_relation"), Context.Relation.ToJson());
	Success.Data = Data;
	FBlueprintHelperGraphFragmentDebugData::AttachToData(Success.Data, Context.FragmentDebugData);

	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = true;
	Validation.bShouldSave = true;
	Success.Validation = Validation;

	TArray<UEdGraphNode*> LayoutNodes = OwnershipNodes;
	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Context.Graph, LayoutNodes);
	return Success;
}

bool FBlueprintHelperMergeExternalFlowService::ApplyExternalMerge(
	const FMergeExternalFlowRequest& Request,
	FMergeExternalFlowContext& Context,
	FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	if (!Context.Graph || !Context.AnchorPin || !Context.BodyEntryPin)
	{
		OutErrorCode = TEXT("link_create_failed");
		OutErrorMessage = TEXT("external merge context is incomplete.");
		return false;
	}

	auto TryConnect = [&](UEdGraphPin* FromPin, UEdGraphPin* ToPin, const TCHAR* ErrorCode) -> bool
	{
		FString Error;
		bool bChanged = false;
		if (!UGraphWriteCoreUtils::TrySchemaConnect(Context.Graph, FromPin, ToPin, Error, bChanged))
		{
			OutErrorCode = ErrorCode;
			OutErrorMessage = Error;
			return false;
		}
		return true;
	};

	auto TryBreak = [&](UEdGraphPin* FromPin, UEdGraphPin* ToPin, const TCHAR* ErrorCode) -> bool
	{
		FString Error;
		bool bChanged = false;
		if (!UGraphWriteCoreUtils::TryBreakLink(FromPin, ToPin, Error, bChanged))
		{
			OutErrorCode = ErrorCode;
			OutErrorMessage = Error;
			return false;
		}
		return true;
	};

	if (Request.InsertStrategy == EBlueprintHelperInsertStrategy::AppendAfter)
	{
		return TryConnect(Context.AnchorPin, Context.BodyEntryPin, TEXT("link_create_failed"));
	}

	if (Request.InsertStrategy == EBlueprintHelperInsertStrategy::InsertBetween)
	{
		if (!Context.OriginalSuccessorPin || Context.BodyExitPins.Num() != 1)
		{
			OutErrorCode = TEXT("original_successor_not_found");
			OutErrorMessage = TEXT("insert_between requires one original successor and one inserted body exit.");
			return false;
		}
		if (!TryBreak(Context.AnchorPin, Context.OriginalSuccessorPin, TEXT("link_disconnect_failed")))
		{
			return false;
		}
		if (!TryConnect(Context.AnchorPin, Context.BodyEntryPin, TEXT("link_create_failed")))
		{
			return false;
		}
		return TryConnect(Context.BodyExitPins[0], Context.OriginalSuccessorPin, TEXT("link_create_failed"));
	}

	if (Request.InsertStrategy != EBlueprintHelperInsertStrategy::BranchFork)
	{
		OutErrorCode = TEXT("unsupported_insert_strategy");
		OutErrorMessage = TEXT("unsupported insert_strategy.");
		return false;
	}

	FString SequenceError;
	Context.SequenceNode = UGraphWriteCoreUtils::SpawnSequenceNode(
		Context.Graph,
		Context.Relation.InsertedBlockId,
		SequenceError);
	if (!Context.SequenceNode)
	{
		OutErrorCode = TEXT("node_create_failed");
		OutErrorMessage = SequenceError;
		return false;
	}

	if (Context.OriginalSuccessorPin &&
		!TryBreak(Context.AnchorPin, Context.OriginalSuccessorPin, TEXT("link_disconnect_failed")))
	{
		return false;
	}

	UEdGraphPin* SequenceExecIn = UGraphWriteCoreUtils::FindFirstExecPin(Context.SequenceNode, EGPD_Input);
	if (!SequenceExecIn)
	{
		OutErrorCode = TEXT("inserted_logic_has_no_exec_pins");
		OutErrorMessage = TEXT("sequence node has no exec input pin.");
		return false;
	}
	if (!TryConnect(Context.AnchorPin, SequenceExecIn, TEXT("link_create_failed")))
	{
		return false;
	}

	TArray<UEdGraphPin*> SequenceOutputs;
	if (!UGraphWriteCoreUtils::EnsureSequenceOutputCount(
		Context.SequenceNode,
		Request.SequenceOrder.Num(),
		SequenceOutputs,
		SequenceError))
	{
		OutErrorCode = TEXT("node_create_failed");
		OutErrorMessage = SequenceError;
		return false;
	}

	for (int32 Index = 0; Index < Request.SequenceOrder.Num(); ++Index)
	{
		if (!SequenceOutputs.IsValidIndex(Index) || !SequenceOutputs[Index])
		{
			OutErrorCode = TEXT("link_create_failed");
			OutErrorMessage = TEXT("sequence output pin missing.");
			return false;
		}

		const FString& Branch = Request.SequenceOrder[Index];
		if (Branch == TEXT("inserted_logic"))
		{
			if (!TryConnect(SequenceOutputs[Index], Context.BodyEntryPin, TEXT("link_create_failed")))
			{
				return false;
			}
		}
		else if (Branch == TEXT("original_successor"))
		{
			if (!Context.OriginalSuccessorPin ||
				!TryConnect(SequenceOutputs[Index], Context.OriginalSuccessorPin, TEXT("link_create_failed")))
			{
				return false;
			}
		}
		else
		{
			OutErrorCode = TEXT("sequence_order_invalid");
			OutErrorMessage = TEXT("unsupported branch_fork sequence_order entry.");
			return false;
		}
	}

	return true;
}
