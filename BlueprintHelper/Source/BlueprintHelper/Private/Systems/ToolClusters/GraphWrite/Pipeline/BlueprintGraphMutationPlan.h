#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h"

struct FBlueprintGraphMutationNodePlan
{
	FString NodeId;
	EParsedBlueprintNodeType NodeType = EParsedBlueprintNodeType::Unknown;
	FString FunctionName;
	FString ResolvedCallFunctionStableId;
	FParsedNode ParsedNode;
	TMap<FString, FString> DefaultValues;
};

struct FBlueprintGraphMutationLinkPlan
{
	FString FromId;
	FString FromPin;
	FString ToId;
	FString ToPin;
};

struct FBlueprintGraphMutationLayoutPlan
{
	FString NodeId;
	float X = 0.0f;
	float Y = 0.0f;
};

struct FBlueprintGraphMutationPlan
{
	FString GraphName;
	TArray<FBlueprintGraphMutationNodePlan> Nodes;
	TArray<FBlueprintGraphMutationLinkPlan> Links;
	TArray<FBlueprintGraphMutationLayoutPlan> Layouts;

	bool IsValid() const;
	int32 CountRequestedNodes() const;
	int32 CountRequestedDefaultValues() const;
	int32 CountRequestedLinks() const;
};
