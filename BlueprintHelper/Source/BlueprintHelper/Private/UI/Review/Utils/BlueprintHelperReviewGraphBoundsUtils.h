// Review graph diff bounds utility helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class SGraphEditor;
class FJsonObject;
class FSlateRect;
class UEdGraphNode;

class FBlueprintHelperReviewGraphBoundsUtils
{
public:
	static constexpr float CommentStylePadding = 20.0f;
	static constexpr float FallbackNodeWidth = 220.0f;
	static constexpr float FallbackNodeHeight = 92.0f;

	enum class ENodeBoundsSource : uint8
	{
		None,
		EditorWidget,
		FallbackNode
	};

	struct FBoundsDebugCounters
	{
		int32 TargetCount = 0;
		int32 GraphTargetCount = 0;
		int32 SkippedSurfaceCount = 0;
		int32 SkippedGraphCount = 0;
		int32 CandidateCount = 0;
		int32 MatchedNodeCount = 0;
		int32 DuplicateMatchedNodeCount = 0;
		int32 EditorBoundsCount = 0;
		int32 FallbackBoundsCount = 0;
		int32 RecordBoundsCount = 0;
		int32 NodeGuidTargetCount = 0;
		int32 RecordedBoundsTargetCount = 0;
		int32 StructuredAnchorSourceCount = 0;
		int32 LegacyAnchorSourceCount = 0;
		TArray<FString> MatchedNodeSummaries;
	};

	struct FRecordedGraphBounds
	{
		bool bHasGraphBounds = false;
		FVector2D GraphPosition = FVector2D::ZeroVector;
		FVector2D GraphSize = FVector2D(360.0f, 180.0f);
		FString AnchorSource;
	};

	static void AddTargetKeyCandidates(const FString& RawTargetKey, TArray<FString>& OutCandidates);
	static bool DoesNodeMatchSingleCandidate(const UEdGraphNode* Node, const FString& Candidate);
	static void AddGraphTargetCandidatePasses(
		const FBlueprintHelperReviewAtomicTarget& Target,
		TArray<TArray<FString>>& OutCandidatePasses);
	static FRecordedGraphBounds GetRecordedBoundsForTarget(const FBlueprintHelperReviewAtomicTarget& Target);
	static FString BuildAnchorSourceSummary(const FBoundsDebugCounters& DebugCounters);
	static FString BuildNodeDebugSummary(const UEdGraphNode* Node, ENodeBoundsSource Source);
	static ENodeBoundsSource IncludeNodeBounds(
		const UEdGraphNode* Node,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FBox2D& InOutBounds,
		bool& bInOutHasBounds);
	static bool BuildPaddedBounds(
		const FBox2D& Bounds,
		bool bHasBounds,
		FVector2D& OutPosition,
		FVector2D& OutSize);

private:
	static void AddUniqueTrimmed(TArray<FString>& OutValues, FString Value);
	static FString LastSegmentAfter(const FString& Value, TCHAR Separator);
	static FString StripObjectPath(const FString& Value);
	static bool IsUsefulTargetToken(const FString& Token);
	static FString NormalizeNodeLabelForCompare(FString Value);
	static bool TryReadVector2D(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		FVector2D& OutValue);
	static bool TryReadAnchorJson(
		const FString& AnchorJson,
		FRecordedGraphBounds& OutRecordedBounds);
	static bool TryGetEditorNodeBounds(
		const UEdGraphNode* Node,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FSlateRect& OutRect);
	static FVector2D GetFallbackNodeSize(const UEdGraphNode* Node);
};
