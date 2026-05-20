#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.h"

#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"

FBlueprintGenerateResult FBlueprintGraphMutationPlanExecutor::Execute(
	FBlueprintGraphWriteContext& Context,
	const FBlueprintGraphMutationPlan& Plan)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("Graph mutation plan execution failed.");
	if (!Context.IsValid())
	{
		Result.Message = TEXT("GraphWrite context is invalid.");
		return Result;
	}

	SpawnNodes(Context, Plan, Result);
	ApplyDefaults(Context, Plan, Result);
	ConnectLinks(Context, Plan, Result);

	Result.GeneratedNodeCount = Result.ExecutionStats.SpawnedNodeCount;
	Result.RequestedDefaultValueCount = Result.ExecutionStats.RequestedDefaultValueCount;
	Result.AppliedDefaultValueCount = Result.ExecutionStats.AppliedDefaultValueCount;
	Result.RequestedConnectionCount = Result.ExecutionStats.RequestedLinkCount;
	Result.CreatedConnectionCount = Result.ExecutionStats.CreatedLinkCount;
	Result.UnresolvedNodeCount = 0;
	Result.bSucceed = Result.GeneratedNodeCount > 0 && Result.ConnectionDiagnostics.Num() == 0;
	Result.Message = Result.bSucceed
		? FString::Printf(TEXT("Graph mutation plan executed: %d nodes, %d links."), Result.GeneratedNodeCount, Result.CreatedConnectionCount)
		: TEXT("Graph mutation plan executed with diagnostics.");
	return Result;
}

void FBlueprintGraphMutationPlanExecutor::SpawnNodes(
	FBlueprintGraphWriteContext& Context,
	const FBlueprintGraphMutationPlan& Plan,
	FBlueprintGenerateResult& Result)
{
	Result.ExecutionStats.RequestedNodeCount = Plan.CountRequestedNodes();
	for (const FBlueprintGraphMutationNodePlan& NodePlan : Plan.Nodes)
	{
		FString Error;
		IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(NodePlan.NodeType);
		UK2Node* Node = Handler ? Handler->Spawn(Context.GetGraph(), NodePlan.ParsedNode, Error) : nullptr;
		if (!Node)
		{
			Result.ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("node_spawn_failed"),
				NodePlan.NodeId,
				TEXT(""),
				Error.IsEmpty() ? TEXT("No node handler produced a node.") : Error));
			continue;
		}
		Context.RegisterNode(NodePlan.NodeId, Node, true);
		++Result.ExecutionStats.SpawnedNodeCount;
	}
}

void FBlueprintGraphMutationPlanExecutor::ApplyDefaults(
	FBlueprintGraphWriteContext& Context,
	const FBlueprintGraphMutationPlan& Plan,
	FBlueprintGenerateResult& Result)
{
	Result.ExecutionStats.RequestedDefaultValueCount = Plan.CountRequestedDefaultValues();
	for (const FBlueprintGraphMutationNodePlan& NodePlan : Plan.Nodes)
	{
		const TArray<FBlueprintGeneratorDiagnostic> Diagnostics =
			FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(Context, NodePlan.NodeId, NodePlan.DefaultValues);
		Result.DefaultValueDiagnostics.Append(Diagnostics);

		int32 ErrorCount = 0;
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.IsError())
			{
				++ErrorCount;
			}
		}
		Result.ExecutionStats.AppliedDefaultValueCount += FMath::Max(0, NodePlan.DefaultValues.Num() - ErrorCount);
	}
}

void FBlueprintGraphMutationPlanExecutor::ConnectLinks(
	FBlueprintGraphWriteContext& Context,
	const FBlueprintGraphMutationPlan& Plan,
	FBlueprintGenerateResult& Result)
{
	TArray<FParsedLink> ParsedLinks;
	for (const FBlueprintGraphMutationLinkPlan& LinkPlan : Plan.Links)
	{
		FParsedLink ParsedLink;
		ParsedLink.FromId = LinkPlan.FromId;
		ParsedLink.FromPin = LinkPlan.FromPin;
		ParsedLink.ToId = LinkPlan.ToId;
		ParsedLink.ToPin = LinkPlan.ToPin;
		ParsedLinks.Add(ParsedLink);
	}

	Result.ExecutionStats.RequestedLinkCount = ParsedLinks.Num();
	Result.ExecutionStats.CreatedLinkCount =
		FBlueprintGraphLinker::ConnectExplicitLinks(Context, ParsedLinks, Result.ConnectionDiagnostics);
}
