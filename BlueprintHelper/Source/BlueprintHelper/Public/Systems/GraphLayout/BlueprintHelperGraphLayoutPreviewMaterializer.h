#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

namespace BlueprintHelper::GraphLayout
{
struct BLUEPRINTHELPER_API FGraphLayoutPreviewMaterializerResult
{
	TStrongObjectPtr<UBlueprint> PreviewBlueprint;
	TStrongObjectPtr<UEdGraph> PreviewGraph;
	TMap<FString, FGuid> NodeGuidsById;
	TMap<FGuid, FString> NodeIdsByGuid;
	TMap<FGuid, ENodeRole> RolesByGuid;
	TMap<FGuid, ENodeRole> AnchorRolesByGuid;
	TSet<FGuid> PreviewOverlayGuids;
	FString Error;
};

class BLUEPRINTHELPER_API FGraphLayoutPreviewMaterializer
{
public:
	void Begin(const FGraphLayoutPreviewSample& Sample, const FLayoutPlan& LayoutPlan);
	bool Tick(float MaxMillisecondsPerFrame);
	void Cancel();
	bool IsComplete() const;
	const FGraphLayoutPreviewMaterializerResult& GetResult() const;
	bool MaterializeForTest(
		const FGraphLayoutPreviewSample& Sample,
		const FLayoutPlan& LayoutPlan,
		FGraphLayoutPreviewMaterializerResult& OutResult);

private:
	void ResetState();
	bool EnsureGameThread(const TCHAR* Context);
	bool InitializePreviewGraph();
	bool MaterializeNextNode();
	bool MaterializeNextLink();
	void FinishWithError(const FString& ErrorMessage);

	const FGraphLayoutPreviewNodeSpec* FindNodeSpec(const FString& NodeId) const;
	const FNodeSnapshot* FindSnapshotNode(const FString& NodeId) const;
	const FNodePlacement* FindPlacement(const FString& NodeId) const;

	UEdGraphNode* CreateNodeForSpec(const FGraphLayoutPreviewNodeSpec& NodeSpec);
	UEdGraphPin* FindOrCreatePin(UEdGraphNode* Node, const FPinSnapshot& PinSnapshot);
	bool ConnectLink(const FGraphLayoutPreviewLinkSpec& Link);

	FGraphLayoutPreviewSample PendingSample;
	FLayoutPlan PendingLayoutPlan;
	FGraphLayoutPreviewMaterializerResult Result;
	TMap<FString, UEdGraphNode*> MaterializedNodesById;
	int32 NextNodeIndex = 0;
	int32 NextLinkIndex = 0;
	bool bBegun = false;
	bool bComplete = true;
};
}
