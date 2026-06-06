#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2EventBodyAdapter.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"

FString FBlueprintHelperK2EventBodyAdapter::GetAdapterId() const
{
	return TEXT("k2.event_body");
}

bool FBlueprintHelperK2EventBodyAdapter::ResolveTarget(
	const FBlueprintHelperGraphBodyRequest& Request,
	FBlueprintHelperGraphBodyTarget& OutTarget,
	FString& OutError) const
{
	if (!Request.Blueprint)
	{
		OutError = TEXT("Blueprint is required for k2.event_body.");
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
		OutError = TEXT("Event graph was not found.");
		return false;
	}

	OutTarget.Blueprint = Request.Blueprint;
	OutTarget.Graph = EventGraph;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = EventGraph->GetName();
	OutTarget.EntryName = Request.EntryName;
	OutTarget.BodyIdentity = FString::Printf(TEXT("%s|%s|k2.event_body"), *Request.AssetPath, *EventGraph->GetName());
	OutError.Reset();
	return true;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperK2EventBodyAdapter::BuildBoundaryModel(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyRequest& Request) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = GetAdapterId();
	Boundary.TaskSpecStrategy = Request.TaskSpecStrategy.IsEmpty() ? TEXT("replace_owned_graph") : Request.TaskSpecStrategy;
	Boundary.TargetAssetPath = Target.AssetPath.IsEmpty() ? Request.AssetPath : Target.AssetPath;
	Boundary.GraphName = Target.GraphName.IsEmpty() ? Request.GraphName : Target.GraphName;
	Boundary.GraphFamily = TEXT("k2");
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2EventBody;

	if (!Target.Graph)
	{
		return Boundary;
	}

	for (UEdGraphNode* Node : Target.Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		const FString Ref = FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(Node);
		if (FBlueprintHelperK2GraphBodyAdapterUtils::IsEventEntry(Node, Request.EntryName))
		{
			Boundary.EntryNodeRefs.AddUnique(Ref);
			Boundary.ProtectedNodeRefs.AddUnique(Ref);
			FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticSources(Node, Ref, Boundary.SemanticSourceRefs);
		}
		else
		{
			Boundary.DeletableNodeRefs.AddUnique(Ref);
		}
	}
	return Boundary;
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperK2EventBodyAdapter::BuildMutationPlan(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyRequest&) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = Boundary;
	Plan.ConnectivityPolicy = BuildConnectivityPolicy(Target, Boundary);
	Plan.Steps.Add({TEXT("resolve_event_boundary"), TEXT("Resolve native event entry boundary through the event adapter.")});
	Plan.Steps.Add({TEXT("delegate_graphwrite_event_mutation"), TEXT("Delegate statement import and graph mutation to the shared GraphWrite pipeline.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}

FBlueprintHelperGraphBodySemanticContext FBlueprintHelperK2EventBodyAdapter::BuildSemanticContext(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodySemanticContext Context;
	Context.ContextId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	Context.GraphOwnedSymbolRefs = Boundary.SemanticSourceRefs;
	return Context;
}

FBlueprintHelperGraphBodyReconnectPlan FBlueprintHelperK2EventBodyAdapter::BuildReconnectPlan(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReconnectPlan Plan;
	Plan.EntryBoundaryRefs = Boundary.EntryNodeRefs;
	return Plan;
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperK2EventBodyAdapter::BuildConnectivityPolicy(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphBodyReadbackProjection FBlueprintHelperK2EventBodyAdapter::BuildReadbackProjection(
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
