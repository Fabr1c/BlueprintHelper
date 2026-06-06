#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2MacroBodyAdapter.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_Tunnel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"

FString FBlueprintHelperK2MacroBodyAdapter::GetAdapterId() const
{
	return TEXT("k2.macro_body");
}

bool FBlueprintHelperK2MacroBodyAdapter::ResolveTarget(
	const FBlueprintHelperGraphBodyRequest& Request,
	FBlueprintHelperGraphBodyTarget& OutTarget,
	FString& OutError) const
{
	if (!Request.Blueprint)
	{
		OutError = TEXT("Blueprint is required for k2.macro_body.");
		return false;
	}

	UEdGraph* MacroGraph = FBlueprintHelperK2GraphBodyAdapterUtils::FindGraphByName(
		Request.Blueprint->MacroGraphs,
		Request.GraphName);
	if (!MacroGraph)
	{
		OutError = FString::Printf(TEXT("Macro graph %s was not found."), *Request.GraphName);
		return false;
	}

	OutTarget.Blueprint = Request.Blueprint;
	OutTarget.Graph = MacroGraph;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = MacroGraph->GetName();
	OutTarget.EntryName = Request.EntryName;
	OutTarget.BodyIdentity = FString::Printf(
		TEXT("%s|%s|k2.macro_body"),
		*Request.AssetPath,
		*MacroGraph->GetName());
	OutError.Reset();
	return true;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperK2MacroBodyAdapter::BuildBoundaryModel(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyRequest& Request) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = GetAdapterId();
	Boundary.TaskSpecStrategy = Request.TaskSpecStrategy.IsEmpty() ? TEXT("replace_owned_graph") : Request.TaskSpecStrategy;
	Boundary.TargetAssetPath = Target.AssetPath.IsEmpty() ? Request.AssetPath : Target.AssetPath;
	Boundary.GraphName = Target.GraphName.IsEmpty() ? Request.GraphName : Target.GraphName;
	Boundary.GraphFamily = TEXT("k2");
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2MacroBody;

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

		if (const UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node))
		{
			const FString Ref = FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(Node);
			Boundary.ProtectedNodeRefs.AddUnique(Ref);
			if (FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelEntry(Tunnel))
			{
				Boundary.EntryNodeRefs.AddUnique(Ref);
			}
			if (FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelExit(Tunnel))
			{
				Boundary.ExitNodeRefs.AddUnique(Ref);
			}
			FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticSources(Tunnel, Ref, Boundary.SemanticSourceRefs);
		}
		else
		{
			Boundary.DeletableNodeRefs.AddUnique(FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(Node));
		}
	}
	return Boundary;
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperK2MacroBodyAdapter::BuildMutationPlan(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyRequest&) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = Boundary;
	Plan.ConnectivityPolicy = BuildConnectivityPolicy(Target, Boundary);
	Plan.Steps.Add({TEXT("resolve_macro_tunnel_boundaries"), TEXT("Resolve UK2Node_Tunnel entry and exit boundaries through the macro adapter.")});
	Plan.Steps.Add({TEXT("delegate_graphwrite_macro_mutation"), TEXT("Delegate statement import and graph mutation to the shared GraphWrite pipeline.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}

FBlueprintHelperGraphBodySemanticContext FBlueprintHelperK2MacroBodyAdapter::BuildSemanticContext(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodySemanticContext Context;
	Context.ContextId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	Context.MacroTunnelPinRefs = Boundary.SemanticSourceRefs;
	return Context;
}

FBlueprintHelperGraphBodyReconnectPlan FBlueprintHelperK2MacroBodyAdapter::BuildReconnectPlan(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReconnectPlan Plan;
	Plan.EntryBoundaryRefs = Boundary.EntryNodeRefs;
	Plan.ExitBoundaryRefs = Boundary.ExitNodeRefs;
	Plan.bReconnectEntryToFirstImportedExec = true;
	Plan.bReconnectImportedExecToExitBoundary = Boundary.ExitNodeRefs.Num() > 0;
	return Plan;
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperK2MacroBodyAdapter::BuildConnectivityPolicy(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphBodyReadbackProjection FBlueprintHelperK2MacroBodyAdapter::BuildReadbackProjection(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReadbackProjection Projection;
	Projection.ProjectionId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	if (Boundary.EntryNodeRefs.Num() > 0)
	{
		Projection.EntryNodeRef = Boundary.EntryNodeRefs[0];
	}
	Projection.ExitNodeRefs = Boundary.ExitNodeRefs;
	Projection.VisibleBoundaryNodeRefs.Append(Boundary.EntryNodeRefs);
	Projection.VisibleBoundaryNodeRefs.Append(Boundary.ExitNodeRefs);
	return Projection;
}
