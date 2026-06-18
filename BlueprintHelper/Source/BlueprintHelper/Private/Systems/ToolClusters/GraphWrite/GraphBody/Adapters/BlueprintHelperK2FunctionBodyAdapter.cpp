#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2FunctionBodyAdapter.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"

FString FBlueprintHelperK2FunctionBodyAdapter::GetAdapterId() const
{
	return TEXT("k2.function_body");
}

bool FBlueprintHelperK2FunctionBodyAdapter::ResolveTarget(
	const FBlueprintHelperGraphBodyRequest& Request,
	FBlueprintHelperGraphBodyTarget& OutTarget,
	FString& OutError) const
{
	if (!Request.Blueprint)
	{
		OutError = TEXT("Blueprint is required for k2.function_body.");
		return false;
	}

	UEdGraph* FunctionGraph = FBlueprintHelperK2GraphBodyAdapterUtils::FindGraphByName(
		Request.Blueprint->FunctionGraphs,
		Request.GraphName);
	if (!FunctionGraph)
	{
		OutError = FString::Printf(TEXT("Function graph %s was not found."), *Request.GraphName);
		return false;
	}

	OutTarget.Blueprint = Request.Blueprint;
	OutTarget.Graph = FunctionGraph;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = FunctionGraph->GetName();
	OutTarget.EntryName = Request.EntryName;
	OutTarget.BodyIdentity = FString::Printf(
		TEXT("%s|%s|k2.function_body"),
		*Request.AssetPath,
		*FunctionGraph->GetName());
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		if (FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionEntry(Node))
		{
			OutTarget.EntryBoundaryNodes.AddUnique(Node);
			OutTarget.ProtectedNodes.AddUnique(Node);
		}
		else if (FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionResult(Node))
		{
			OutTarget.ExitBoundaryNodes.AddUnique(Node);
			OutTarget.ProtectedNodes.AddUnique(Node);
		}
		else
		{
			OutTarget.DeletableNodes.AddUnique(Node);
		}
	}
	OutError.Reset();
	return true;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperK2FunctionBodyAdapter::BuildBoundaryModel(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyRequest& Request) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = GetAdapterId();
	Boundary.TaskSpecStrategy = Request.TaskSpecStrategy.IsEmpty() ? TEXT("replace_owned_graph") : Request.TaskSpecStrategy;
	Boundary.TargetAssetPath = Target.AssetPath.IsEmpty() ? Request.AssetPath : Target.AssetPath;
	Boundary.GraphName = Target.GraphName.IsEmpty() ? Request.GraphName : Target.GraphName;
	Boundary.GraphFamily = TEXT("k2");
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2FunctionBody;

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
		if (FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionEntry(Node))
		{
			Boundary.EntryNodeRefs.AddUnique(Ref);
			Boundary.EntryBoundaryRefs.AddUnique(Ref);
			Boundary.ProtectedNodeRefs.AddUnique(Ref);
			FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticSources(Node, Ref, Boundary.SemanticSourceRefs);
		}
		else if (FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionResult(Node))
		{
			Boundary.ExitNodeRefs.AddUnique(Ref);
			Boundary.ExitBoundaryRefs.AddUnique(Ref);
			Boundary.ProtectedNodeRefs.AddUnique(Ref);
			FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticOutputs(
				Node,
				Ref,
				Boundary.SemanticOutputRefs,
				Boundary.ReturnDataPinRefs);
		}
		else
		{
			Boundary.DeletableNodeRefs.AddUnique(Ref);
		}
	}
	return Boundary;
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperK2FunctionBodyAdapter::BuildMutationPlan(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyRequest&) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = Boundary;
	Plan.ConnectivityPolicy = BuildConnectivityPolicy(Target, Boundary);
	Plan.Steps.Add({TEXT("resolve_function_boundaries"), TEXT("Resolve FunctionEntry and FunctionResult through the function adapter.")});
	Plan.Steps.Add({TEXT("delegate_graphwrite_function_mutation"), TEXT("Delegate statement import and graph mutation to the shared GraphWrite pipeline.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}

FBlueprintHelperGraphBodySemanticContext FBlueprintHelperK2FunctionBodyAdapter::BuildSemanticContext(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodySemanticContext Context;
	Context.ContextId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	Context.FunctionParamRefs = Boundary.SemanticSourceRefs;
	return Context;
}

FBlueprintHelperGraphBodyReconnectPlan FBlueprintHelperK2FunctionBodyAdapter::BuildReconnectPlan(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReconnectPlan Plan;
	Plan.EntryBoundaryRefs = Boundary.EntryBoundaryRefs.Num() > 0 ? Boundary.EntryBoundaryRefs : Boundary.EntryNodeRefs;
	Plan.ExitBoundaryRefs = Boundary.ExitBoundaryRefs.Num() > 0 ? Boundary.ExitBoundaryRefs : Boundary.ExitNodeRefs;
	Plan.ReturnDataPinRefs = Boundary.ReturnDataPinRefs;
	Plan.bReconnectEntryToFirstImportedExec = true;
	Plan.bReconnectImportedExecToExitBoundary = Plan.ExitBoundaryRefs.Num() > 0;
	for (const FString& ReturnPinRef : Boundary.ReturnDataPinRefs)
	{
		Plan.ReturnOutputToResultPinRefs.Add(ReturnPinRef, ReturnPinRef);
	}
	return Plan;
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperK2FunctionBodyAdapter::BuildConnectivityPolicy(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphBodyReadbackProjection FBlueprintHelperK2FunctionBodyAdapter::BuildReadbackProjection(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReadbackProjection Projection;
	Projection.ProjectionId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	Projection.FunctionName = Boundary.GraphName;
	Projection.EntryBoundaryRefs = Boundary.EntryBoundaryRefs.Num() > 0 ? Boundary.EntryBoundaryRefs : Boundary.EntryNodeRefs;
	Projection.ResultBoundaryRefs = Boundary.ExitBoundaryRefs.Num() > 0 ? Boundary.ExitBoundaryRefs : Boundary.ExitNodeRefs;
	Projection.FunctionInputPinRefs = Boundary.SemanticSourceRefs;
	Projection.FunctionOutputPinRefs = Boundary.SemanticOutputRefs;
	Projection.GeneratedNodeRefs = Boundary.GeneratedNodeRefs.Num() > 0 ? Boundary.GeneratedNodeRefs : Boundary.ImportedBodyNodeRefs;
	FBlueprintHelperK2GraphBodyAdapterUtils::AppendGraphLinkRefs(
		Target.Graph,
		Projection.ExecLinkRefs,
		Projection.DataLinkRefs);
	if (Boundary.EntryNodeRefs.Num() > 0)
	{
		Projection.EntryNodeRef = Boundary.EntryNodeRefs[0];
		Projection.FoldedBoundaryNodeRefs.AddUnique(Boundary.EntryNodeRefs[0]);
		Projection.BoundaryDisplayNames.Add(
			Boundary.EntryNodeRefs[0],
			Boundary.GraphName.IsEmpty() ? FString(TEXT("Function")) : Boundary.GraphName);
	}
	Projection.ExitNodeRefs = Projection.ResultBoundaryRefs;
	Projection.VisibleBoundaryNodeRefs = Projection.ResultBoundaryRefs;
	for (const FString& ExitRef : Projection.ResultBoundaryRefs)
	{
		Projection.BoundaryDisplayNames.Add(ExitRef, TEXT("Return"));
	}
	Projection.bSynthesizeLogicEntry = false;
	Projection.bSynthesizeLogicResult = false;
	return Projection;
}
