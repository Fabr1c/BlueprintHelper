// BlueprintHelper Review utility functions.
// Consolidates all anonymous namespace functions from Systems/Review/*.cpp files.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Text.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"

#include "BlueprintHelperReviewUtils.generated.h"

class UEdGraphNode;
class UEdGraph;
class UBlueprint;

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperReviewUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region ConfigResolver
	static FString BlueprintHelperResolveProjectPath(FString Path, const FString& DefaultRelativePath);
	static FString BlueprintHelperNormalizeVersion(FString Version);
#pragma endregion ConfigResolver

#pragma region StoreService
	static FSimpleMulticastDelegate& BlueprintHelperReviewPendingReviewChangedDelegate();
	static void NormalizeReviewTargetSemanticSnapshots(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		FBlueprintHelperReviewAtomicTarget& Target);
#pragma endregion StoreService

#pragma region BaselineSnapshotService
	static FString BlueprintHelperReviewMakeStableTextKeyForSnapshot(const FText& Text);
	static TSharedPtr<FJsonObject> CloneReviewSnapshotObjectForHash(const TSharedPtr<FJsonObject>& Source);
	static TSharedPtr<FJsonValue> CloneReviewSnapshotValueForHash(const TSharedPtr<FJsonValue>& Value);
	static FString ExtractReviewSnapshotAnchorName(const FString& TargetKey, const FString& Prefix);
	static bool IsReviewSnapshotIgnoredGraphNode(const UEdGraphNode* Node);
	static FString GetReviewSnapshotNodeMetadataValue(const UEdGraphNode* Node, const TCHAR* Key);
	static FString GetReviewSnapshotNodeBlockId(const UEdGraphNode* Node);
	static FString MakeReviewSnapshotNodeSortKey(const UEdGraphNode* Node);
	static FString BuildReviewSnapshotRestoreText(const TArray<const UEdGraphNode*>& Nodes);
	static void AppendReviewSnapshotGraphs(TArray<const UEdGraph*>& OutGraphs, const TArray<UEdGraph*>& InGraphs);
	static const UEdGraph* FindReviewSnapshotGraph(const UBlueprint* Blueprint, const FString& GraphName);
	static const UEdGraphNode* FindReviewSnapshotNodeByGuid(const UEdGraph* Graph, const FString& NodeGuid);
	static const UEdGraphNode* FindReviewSnapshotNodeByName(const UEdGraph* Graph, const FString& NodeName);
	static TSharedPtr<FJsonObject> FindBaselineGraphObject(const TSharedPtr<FJsonObject>& BlueprintSnapshot, const FString& GraphName);
	static TSharedPtr<FJsonObject> FindBaselineNodeObject(const TSharedPtr<FJsonObject>& GraphObject, const FString& NodeGuid, const FString& NodeName);
	static bool BaselineJsonObjectStringFieldEquals(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const FString& Expected);
	static TArray<TSharedPtr<FJsonValue>> CopyBaselineJsonArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName);
#pragma endregion BaselineSnapshotService

#pragma region StoreMergeUtils
	static FString BlueprintHelperReviewNormalizeCollapseSegment(FString Value);
	static bool BlueprintHelperReviewIsActiveVisibleChangeState(const FBlueprintHelperReviewVisibleChange& Change);
	static void BlueprintHelperReviewIndexVisibleChangeCollapseKeys(
		const FBlueprintHelperReviewVisibleChange& Change,
		int32 Index,
		TMap<FString, int32>& ExistingIndexByChangeId,
		TMap<FString, int32>& ExistingIndexByLifecycleRoot);
	static bool BlueprintHelperReviewFindCollapseReason(
		const FBlueprintHelperReviewVisibleChange& Existing,
		const FBlueprintHelperReviewVisibleChange& Incoming,
		FString& OutReason);
	static void BlueprintHelperReviewLogFoldedVisibleChange(
		const TCHAR* Context,
		const FString& Reason,
		const FBlueprintHelperReviewVisibleChange& Existing,
		const FBlueprintHelperReviewVisibleChange& Incoming);
#pragma endregion StoreMergeUtils

#pragma region RejectService
	static void CaptureReviewRejectCurrentStateDiagnostic(
		const FBlueprintHelperReviewAtomicTarget& Target,
		FBlueprintHelperReviewActionResult& InOutDiagnostic);
#pragma endregion RejectService

#pragma region BaselineSnapshotServiceUtils
	static bool ShouldOmitCanonicalReviewSnapshotField(const FString& Key);
	static void AppendCanonicalJsonString(const FString& Value, FString& Out);
	static void AppendCanonicalJsonValue(const TSharedPtr<FJsonValue>& Value, FString& Out);
	static void AppendCanonicalJsonObject(const TSharedPtr<FJsonObject>& Object, FString& Out);
#pragma endregion BaselineSnapshotServiceUtils
};
