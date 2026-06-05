#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutPreviewMovedNode
{
	FString NodeId;
	FGuid NodeGuid;
	ENodeRole Role = ENodeRole::Unknown;
	ENodeRole AnchorRole = ENodeRole::Unknown;
	FVector2D BeginTopLeft = FVector2D::ZeroVector;
	FVector2D EndTopLeft = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
};

struct FGraphLayoutPreviewInteractionCommit
{
	TArray<FGraphLayoutPreviewMovedNode> MovedNodes;
	FString RejectionReason;
};

class BLUEPRINTHELPER_API FGraphLayoutPreviewInteractionModel
{
public:
	bool Initialize(const FGraphLayoutPreviewMaterializerResult& MaterializerResult, UEdGraph* PreviewGraph);
	void Reset();
	void BeginInteraction(UEdGraph* PreviewGraph);
	bool EndInteraction(UEdGraph* PreviewGraph, FGraphLayoutPreviewInteractionCommit& OutCommit);
	bool HasActiveInteraction() const;
	static bool BuildRuleSetJsonForCommit(
		const FString& InputRuleSetJson,
		ESemanticScene Scene,
		const FGraphLayoutPreviewInteractionCommit& Commit,
		FString& OutRuleSetJson,
		FString& OutError);

private:
	struct FTrackedNode
	{
		FString NodeId;
		FGuid NodeGuid;
		ENodeRole Role = ENodeRole::Unknown;
		ENodeRole AnchorRole = ENodeRole::Unknown;
		FVector2D BeginTopLeft = FVector2D::ZeroVector;
		FVector2D LastTopLeft = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
	};

	bool CapturePositions(UEdGraph* PreviewGraph, TMap<FGuid, FVector2D>& OutPositions) const;
	bool CaptureTopology(
		UEdGraph* PreviewGraph,
		TMap<FGuid, FString>& OutNodeSignatures,
		TSet<FString>& OutLinkSignatures,
		int32& OutLinkEndpointCount) const;
	bool ValidateMoveOnly(UEdGraph* PreviewGraph, FString& OutReason) const;
	FVector2D ResolveNodeSize(const UEdGraphNode& Node) const;
	FString BuildNodeSignature(const UEdGraphNode& Node) const;
	FString BuildPinSignature(const UEdGraphNode& Node, const UEdGraphPin& Pin) const;
	FString BuildLinkEndpointSignature(const UEdGraphNode& Node, const UEdGraphPin& Pin) const;
	FString BuildLinkSignature(
		const UEdGraphNode& FromNode,
		const UEdGraphPin& FromPin,
		const UEdGraphNode& ToNode,
		const UEdGraphPin& ToPin) const;

	TMap<FGuid, FTrackedNode> TrackedNodesByGuid;
	TMap<FGuid, FString> InitialNodeSignaturesByGuid;
	TSet<FString> InitialLinkSignatures;
	int32 InitialNodeCount = 0;
	int32 InitialLinkEndpointCount = 0;
	bool bInteractionActive = false;
};
}
