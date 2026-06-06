#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2CustomEventBodyAdapter.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"

FString FBlueprintHelperK2CustomEventBodyAdapter::GetAdapterId() const
{
	return TEXT("k2.custom_event_body");
}

bool FBlueprintHelperK2CustomEventBodyAdapter::ResolveTarget(
	const FBlueprintHelperGraphBodyRequest& Request,
	FBlueprintHelperGraphBodyTarget& OutTarget,
	FString& OutError) const
{
	if (!Request.Blueprint)
	{
		OutError = TEXT("Blueprint is required for k2.custom_event_body.");
		return false;
	}
	UEdGraph* EventGraph = FBlueprintHelperK2GraphBodyAdapterUtils::FindGraphByName(
		Request.Blueprint->UbergraphPages,
		Request.GraphName.IsEmpty() ? FString(TEXT("EventGraph")) : Request.GraphName);
	if (!EventGraph && Request.Blueprint->UbergraphPages.Num() > 0)
	{
		EventGraph = Request.Blueprint->UbergraphPages[0];
	}
	if (!EventGraph)
	{
		OutError = TEXT("Custom event graph was not found.");
		return false;
	}

	OutTarget.Blueprint = Request.Blueprint;
	OutTarget.Graph = EventGraph;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = EventGraph->GetName();
	OutTarget.EntryName = Request.EntryName;
	OutTarget.BodyIdentity = FString::Printf(
		TEXT("%s|%s|%s|k2.custom_event_body"),
		*Request.AssetPath,
		*EventGraph->GetName(),
		*Request.EntryName);
	UEdGraphNode* EntryNode = nullptr;
	FString EntryBlockId;
	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		if (FBlueprintHelperK2GraphBodyAdapterUtils::IsCustomEventEntry(Node, Request.EntryName))
		{
			if (!EntryNode)
			{
				EntryNode = Node;
				FBlueprintHelperK2GraphBodyAdapterUtils::TryReadBlueprintHelperBlockId(Node, EntryBlockId);
			}
			OutTarget.EntryBoundaryNodes.AddUnique(Node);
			OutTarget.ProtectedNodes.AddUnique(Node);
		}
	}
	FBlueprintHelperK2GraphBodyAdapterUtils::AppendOwnedBodyNodesForBlock(
		EventGraph,
		EntryBlockId,
		EntryNode,
		OutTarget.DeletableNodes);
	if (!EntryNode)
	{
		OutError = FString::Printf(TEXT("Entry %s was not found."), *Request.EntryName);
		return false;
	}
	if (EntryBlockId.IsEmpty())
	{
		OutError = FString::Printf(
			TEXT("owned_replace_target_not_blueprinthelper_owned: Entry %s does not have BlueprintHelper ownership metadata; owned replace cannot adopt user-authored nodes."),
			*Request.EntryName);
		return false;
	}
	OutError.Reset();
	return true;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperK2CustomEventBodyAdapter::BuildBoundaryModel(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyRequest& Request) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = GetAdapterId();
	Boundary.TaskSpecStrategy = Request.TaskSpecStrategy.IsEmpty() ? TEXT("replace_owned_graph") : Request.TaskSpecStrategy;
	Boundary.TargetAssetPath = Target.AssetPath.IsEmpty() ? Request.AssetPath : Target.AssetPath;
	Boundary.GraphName = Target.GraphName.IsEmpty() ? Request.GraphName : Target.GraphName;
	Boundary.GraphFamily = TEXT("k2");
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2CustomEventBody;

	for (UEdGraphNode* Node : Target.EntryBoundaryNodes)
	{
		if (Node)
		{
			const FString Ref = FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(Node);
			Boundary.EntryNodeRefs.AddUnique(Ref);
			Boundary.ProtectedNodeRefs.AddUnique(Ref);
			FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticSources(Node, Ref, Boundary.SemanticSourceRefs);
			if (Boundary.OwnedBlockId.IsEmpty())
			{
				FBlueprintHelperK2GraphBodyAdapterUtils::TryReadBlueprintHelperBlockId(Node, Boundary.OwnedBlockId);
			}
		}
	}
	for (UEdGraphNode* Node : Target.DeletableNodes)
	{
		if (!Node)
		{
			continue;
		}
		const FString Ref = FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(Node);
		if (!Ref.IsEmpty())
		{
			Boundary.DeletableNodeRefs.AddUnique(Ref);
		}
	}
	return Boundary;
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperK2CustomEventBodyAdapter::BuildMutationPlan(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyRequest&) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = Boundary;
	Plan.ConnectivityPolicy = BuildConnectivityPolicy(Target, Boundary);
	Plan.Steps.Add({TEXT("resolve_custom_event_boundary"), TEXT("Resolve custom event entry boundary through the custom-event adapter.")});
	Plan.Steps.Add({TEXT("delegate_graphwrite_custom_event_mutation"), TEXT("Delegate statement import and graph mutation to the shared GraphWrite pipeline.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}

FBlueprintHelperGraphBodySemanticContext FBlueprintHelperK2CustomEventBodyAdapter::BuildSemanticContext(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodySemanticContext Context;
	Context.ContextId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	Context.GraphOwnedSymbolRefs = Boundary.SemanticSourceRefs;
	return Context;
}

FBlueprintHelperGraphBodyReconnectPlan FBlueprintHelperK2CustomEventBodyAdapter::BuildReconnectPlan(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReconnectPlan Plan;
	Plan.EntryBoundaryRefs = Boundary.EntryNodeRefs;
	return Plan;
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperK2CustomEventBodyAdapter::BuildConnectivityPolicy(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphBodyReadbackProjection FBlueprintHelperK2CustomEventBodyAdapter::BuildReadbackProjection(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReadbackProjection Projection;
	Projection.ProjectionId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	if (Boundary.EntryNodeRefs.Num() > 0)
	{
		Projection.EntryNodeRef = Boundary.EntryNodeRefs[0];
	}
	Projection.VisibleBoundaryNodeRefs = Boundary.EntryNodeRefs;
	return Projection;
}
