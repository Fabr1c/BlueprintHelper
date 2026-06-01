#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct BLUEPRINTHELPER_API FExecEdge
{
	FString SourceNodeId;
	FString SourceOutputPinId;
	FString SourceOutputPinName;
	int32 SourceOutputOrdinal = 0;
	FString TargetNodeId;
	int32 TargetOrdinalWithinOutput = 0;
};

struct BLUEPRINTHELPER_API FDataEdge
{
	FString SourceNodeId;
	FString TargetNodeId;
	FString TargetInputPinId;
	FString TargetInputPinName;
	int32 TargetInputOrdinal = 0;
	int32 TargetLinkedNodeOrdinal = 0;
};

class BLUEPRINTHELPER_API FGraphTopology
{
public:
	void AddNode(const FNodeSnapshot& Node);
	void AddExecEdge(const FExecEdge& Edge);
	void AddDataEdge(const FDataEdge& Edge);

	const FNodeSnapshot* FindNode(const FString& NodeId) const;
	TArray<FExecEdge> GetExecOutputEdges(const FString& NodeId) const;
	TArray<FDataEdge> GetDataInputs(const FString& NodeId) const;
	bool IsMultiExecOutputNode(const FString& NodeId) const;
	int32 CountExecInputs(const FString& NodeId) const;

private:
	friend class FGraphLayoutTopology;

	TMultiMap<FString, FExecEdge> ExecEdgesBySource;
	TMultiMap<FString, FDataEdge> DataEdgesByTarget;
	TMap<FString, FNodeSnapshot> OwnedNodesById;
	TMap<FString, int32> ExecInputCounts;
	TMap<FString, int32> ExecOutputPinCounts;
};

class BLUEPRINTHELPER_API FGraphLayoutTopology
{
public:
	static FGraphTopology Build(const FGraphSnapshot& Snapshot);
};
}
