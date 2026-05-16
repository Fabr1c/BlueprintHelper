// Review graph diff bounds helpers.

#include "UI/Review/BlueprintHelperReviewGraphBounds.h"
#include "UI/Review/Utils/BlueprintHelperReviewGraphBoundsUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"

bool FBlueprintHelperReviewGraphBounds::DoesNodeMatchTargetKey(const UEdGraphNode* Node, const FString& TargetKey)
{
	TArray<FString> Candidates;
	FBlueprintHelperReviewGraphBoundsUtils::AddTargetKeyCandidates(TargetKey, Candidates);
	for (const FString& Candidate : Candidates)
	{
		if (FBlueprintHelperReviewGraphBoundsUtils::DoesNodeMatchSingleCandidate(Node, Candidate))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperReviewGraphBounds::BuildCommentStyleBoundsForNodes(
	const TArray<UEdGraphNode*>& Nodes,
	const TSharedPtr<SGraphEditor>& GraphEditor,
	FVector2D& OutPosition,
	FVector2D& OutSize)
{
	FBox2D Bounds(ForceInit);
	bool bHasBounds = false;
	for (UEdGraphNode* Node : Nodes)
	{
		FBlueprintHelperReviewGraphBoundsUtils::IncludeNodeBounds(Node, GraphEditor, Bounds, bHasBounds);
	}
	return FBlueprintHelperReviewGraphBoundsUtils::BuildPaddedBounds(Bounds, bHasBounds, OutPosition, OutSize);
}

bool FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
	const TArray<FBlueprintHelperReviewAtomicTarget>& Targets,
	const UEdGraph* Graph,
	const FString& GraphName,
	const TSharedPtr<SGraphEditor>& GraphEditor,
	FVector2D& OutPosition,
	FVector2D& OutSize,
	FString* OutDebugSummary)
{
	FBox2D Bounds(ForceInit);
	bool bHasBounds = false;
	FBlueprintHelperReviewGraphBoundsUtils::FBoundsDebugCounters DebugCounters;
	TSet<const UEdGraphNode*> IncludedNodes;

	for (const FBlueprintHelperReviewAtomicTarget& Target : Targets)
	{
		++DebugCounters.TargetCount;
		if (Target.Surface != EBlueprintHelperReviewSurface::Graph)
		{
			++DebugCounters.SkippedSurfaceCount;
			continue;
		}
		++DebugCounters.GraphTargetCount;
		if (!Target.GraphName.IsEmpty() && !GraphName.IsEmpty() && Target.GraphName != GraphName)
		{
			++DebugCounters.SkippedGraphCount;
			continue;
		}

		if (!Target.NodeGuid.IsEmpty())
		{
			++DebugCounters.NodeGuidTargetCount;
		}
		const FBlueprintHelperReviewGraphBoundsUtils::FRecordedGraphBounds RecordedBounds =
			FBlueprintHelperReviewGraphBoundsUtils::GetRecordedBoundsForTarget(Target);
		if (RecordedBounds.bHasGraphBounds)
		{
			++DebugCounters.RecordedBoundsTargetCount;
		}
		if (RecordedBounds.AnchorSource.Equals(TEXT("structured"), ESearchCase::IgnoreCase))
		{
			++DebugCounters.StructuredAnchorSourceCount;
		}
		else if (RecordedBounds.AnchorSource.Equals(TEXT("legacy"), ESearchCase::IgnoreCase))
		{
			++DebugCounters.LegacyAnchorSourceCount;
		}

		bool bMatchedNode = false;
		TArray<TArray<FString>> CandidatePasses;
		FBlueprintHelperReviewGraphBoundsUtils::AddGraphTargetCandidatePasses(Target, CandidatePasses);
		for (const TArray<FString>& CandidatePass : CandidatePasses)
		{
			DebugCounters.CandidateCount += CandidatePass.Num();
		}
		if (Graph && CandidatePasses.Num() > 0)
		{
			for (const TArray<FString>& CandidatePass : CandidatePasses)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (!Node)
					{
						continue;
					}

					for (const FString& Candidate : CandidatePass)
					{
						if (FBlueprintHelperReviewGraphBoundsUtils::DoesNodeMatchSingleCandidate(Node, Candidate))
						{
							if (IncludedNodes.Contains(Node))
							{
								++DebugCounters.DuplicateMatchedNodeCount;
								bMatchedNode = true;
								break;
							}

							const FBlueprintHelperReviewGraphBoundsUtils::ENodeBoundsSource Source = FBlueprintHelperReviewGraphBoundsUtils::IncludeNodeBounds(Node, GraphEditor, Bounds, bHasBounds);
							if (Source == FBlueprintHelperReviewGraphBoundsUtils::ENodeBoundsSource::EditorWidget)
							{
								++DebugCounters.EditorBoundsCount;
							}
							else if (Source == FBlueprintHelperReviewGraphBoundsUtils::ENodeBoundsSource::FallbackNode)
							{
								++DebugCounters.FallbackBoundsCount;
							}
							++DebugCounters.MatchedNodeCount;
							IncludedNodes.Add(Node);
							DebugCounters.MatchedNodeSummaries.Add(FBlueprintHelperReviewGraphBoundsUtils::BuildNodeDebugSummary(Node, Source));
							bMatchedNode = true;
							break;
						}
					}
				}

				if (bMatchedNode)
				{
					break;
				}
			}
		}

		if (!bMatchedNode && RecordedBounds.bHasGraphBounds)
		{
			Bounds += RecordedBounds.GraphPosition;
			Bounds += RecordedBounds.GraphPosition + RecordedBounds.GraphSize;
			bHasBounds = true;
			++DebugCounters.RecordBoundsCount;
		}
	}

	const bool bBuilt = FBlueprintHelperReviewGraphBoundsUtils::BuildPaddedBounds(Bounds, bHasBounds, OutPosition, OutSize);
	if (OutDebugSummary)
	{
		*OutDebugSummary = FString::Printf(
			TEXT("built=%d targets=%d graphTargets=%d skippedSurface=%d skippedGraph=%d candidates=%d matchedNodes=%d duplicateMatches=%d editorBounds=%d fallbackBounds=%d recordBounds=%d hasNodeGuidTargets=%d hasRecordedBounds=%d anchorSource=%s padding=%.1f pos=(%.1f,%.1f) size=(%.1f,%.1f) graph=\"%s\" graphNodeCount=%d matched=\"%s\""),
			bBuilt ? 1 : 0,
			DebugCounters.TargetCount,
			DebugCounters.GraphTargetCount,
			DebugCounters.SkippedSurfaceCount,
			DebugCounters.SkippedGraphCount,
			DebugCounters.CandidateCount,
			DebugCounters.MatchedNodeCount,
			DebugCounters.DuplicateMatchedNodeCount,
			DebugCounters.EditorBoundsCount,
			DebugCounters.FallbackBoundsCount,
			DebugCounters.RecordBoundsCount,
			DebugCounters.NodeGuidTargetCount,
			DebugCounters.RecordedBoundsTargetCount,
			*FBlueprintHelperReviewGraphBoundsUtils::BuildAnchorSourceSummary(DebugCounters),
			FBlueprintHelperReviewGraphBoundsUtils::CommentStylePadding,
			static_cast<double>(OutPosition.X),
			static_cast<double>(OutPosition.Y),
			static_cast<double>(OutSize.X),
			static_cast<double>(OutSize.Y),
			*GraphName,
			Graph ? Graph->Nodes.Num() : 0,
			*FString::Join(DebugCounters.MatchedNodeSummaries, TEXT(";")));
	}
	return bBuilt;
}
