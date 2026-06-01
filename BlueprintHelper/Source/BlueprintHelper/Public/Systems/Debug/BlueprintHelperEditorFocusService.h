#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"

class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperBlockIdService;
class FBlueprintHelperGraphResolver;
class FBlueprintHelperLogicJsonPathService;
class UEdGraph;
class UEdGraphNode;

struct BLUEPRINTHELPER_API FBlueprintHelperEditorFocusedGraphSelection
{
	UEdGraph* Graph = nullptr;
	TArray<UEdGraphNode*> Nodes;
	FString GraphName;
	bool bHasSelection = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperEditorFocusService
{
public:
	FBlueprintHelperEditorFocusService(
		const FBlueprintHelperAssetBrowseService& InAssetBrowseService,
		const FBlueprintHelperGraphResolver& InGraphResolver,
		const FBlueprintHelperBlockIdService& InBlockIdService,
		const FBlueprintHelperLogicJsonPathService& InPathService);

	FBlueprintHelperEditorFocusResult FocusBlueprintEditorTarget(
		const FBlueprintHelperEditorFocusRequest& Request) const;
	bool TryGetLastFocusedGraphSelection(
		FBlueprintHelperEditorFocusedGraphSelection& OutSelection) const;

private:
	FBlueprintHelperEditorFocusResult ResolveAndFocusGraph(
		const FBlueprintHelperEditorFocusRequest& Request) const;
	bool TryResolveNode(
		UEdGraph* Graph,
		const FBlueprintHelperEditorFocusRequest& Request,
		UEdGraphNode*& OutNode,
		FString& OutErrorCode,
		FString& OutErrorMessage) const;
	bool FocusResolvedNodeInGraphEditor(
		UEdGraph* Graph,
		UEdGraphNode* Node) const;
	bool FocusNodeSetInGraphEditor(
		UEdGraph* Graph,
		const TArray<UEdGraphNode*>& Nodes) const;
	bool CollectBlockNodes(
		UEdGraph* Graph,
		const FString& BlockId,
		TArray<UEdGraphNode*>& OutNodes) const;
	void CollectEventLogicNodes(
		UEdGraphNode* EntryNode,
		TArray<UEdGraphNode*>& OutNodes) const;
	void StoreLastFocusedGraphSelection(
		UEdGraph* Graph,
		const TArray<UEdGraphNode*>& Nodes) const;
	void ClearLastFocusedGraphSelection() const;

	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
	const FBlueprintHelperGraphResolver& GraphResolver;
	const FBlueprintHelperBlockIdService& BlockIdService;
	const FBlueprintHelperLogicJsonPathService& PathService;
	mutable FBlueprintHelperEditorFocusedGraphSelection LastFocusedGraphSelection;
};
