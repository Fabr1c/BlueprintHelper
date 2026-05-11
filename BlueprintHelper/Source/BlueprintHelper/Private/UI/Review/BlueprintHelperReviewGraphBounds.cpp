// Review graph diff bounds helpers.

#include "UI/Review/BlueprintHelperReviewGraphBounds.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"

class FBlueprintHelperReviewGraphBoundsLocalUtils
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

	static void AddUniqueTrimmed(TArray<FString>& OutValues, FString Value)
	{
		Value.TrimStartAndEndInline();
		if (!Value.IsEmpty())
		{
			OutValues.AddUnique(Value);
		}
	}

	static FString LastSegmentAfter(const FString& Value, const TCHAR Separator)
	{
		int32 Index = INDEX_NONE;
		if (Value.FindLastChar(Separator, Index))
		{
			return Value.Mid(Index + 1);
		}
		return Value;
	}

	static FString StripObjectPath(const FString& Value)
	{
		return LastSegmentAfter(LastSegmentAfter(Value, TCHAR('/')), TCHAR('.'));
	}

	static bool IsUsefulTargetToken(const FString& Token)
	{
		if (Token.Len() < 2)
		{
			return false;
		}
		return !Token.Equals(TEXT("graph"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("node"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("pin"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("block"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("created_node"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("rollback_node"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("rename_added"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("rename_removed"), ESearchCase::IgnoreCase);
	}

	static void AddTargetKeyCandidates(const FString& RawTargetKey, TArray<FString>& OutCandidates)
	{
		if (RawTargetKey.IsEmpty())
		{
			return;
		}

		FString BaseTargetKey = RawTargetKey;
		const int32 RenameSuffixIndex = BaseTargetKey.Find(TEXT(":rename_"), ESearchCase::IgnoreCase);
		if (RenameSuffixIndex != INDEX_NONE)
		{
			BaseTargetKey = BaseTargetKey.Left(RenameSuffixIndex);
		}

		AddUniqueTrimmed(OutCandidates, BaseTargetKey);
		AddUniqueTrimmed(OutCandidates, StripObjectPath(BaseTargetKey));

		TArray<FString> Parts;
		BaseTargetKey.ParseIntoArray(Parts, TEXT(":"), true);
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			const FString CleanPart = StripObjectPath(Parts[Index]);
			if (IsUsefulTargetToken(CleanPart))
			{
				AddUniqueTrimmed(OutCandidates, CleanPart);
			}
		}
	}

	static FString NormalizeNodeLabelForCompare(FString Value)
	{
		Value.ToLowerInline();
		Value.ReplaceInline(TEXT(" "), TEXT(""));
		Value.ReplaceInline(TEXT("\t"), TEXT(""));
		return Value;
	}

	static bool DoesNodeMatchSingleCandidate(const UEdGraphNode* Node, const FString& Candidate)
	{
		if (!Node || Candidate.IsEmpty())
		{
			return false;
		}

		FGuid ParsedGuid;
		if (FGuid::Parse(Candidate, ParsedGuid) && Node->NodeGuid == ParsedGuid)
		{
			return true;
		}

		if (Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens).Equals(Candidate, ESearchCase::IgnoreCase)
			|| Node->NodeGuid.ToString(EGuidFormats::Digits).Equals(Candidate, ESearchCase::IgnoreCase)
			|| Node->GetName().Equals(Candidate, ESearchCase::IgnoreCase))
		{
			return true;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			if (MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")).Equals(Candidate, ESearchCase::IgnoreCase)
				|| MetaData.GetValue(Node, TEXT("BlueprintHelperTransactionId")).Equals(Candidate, ESearchCase::IgnoreCase)
				|| MetaData.GetValue(Node, TEXT("BlueprintHelperFeatureName")).Equals(Candidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (Node->NodeComment.Contains(Candidate, ESearchCase::IgnoreCase))
		{
			return true;
		}

		const FString CandidateLabel = NormalizeNodeLabelForCompare(Candidate);
		const FString NodeTitle = NormalizeNodeLabelForCompare(Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		return !CandidateLabel.IsEmpty()
			&& (NodeTitle.Equals(CandidateLabel, ESearchCase::IgnoreCase)
				|| NodeTitle.Contains(CandidateLabel, ESearchCase::IgnoreCase));
	}

	static void AddGraphTargetCandidatePasses(
		const FBlueprintHelperReviewAtomicTarget& Target,
		TArray<TArray<FString>>& OutCandidatePasses)
	{
		auto AddPass = [&OutCandidatePasses](const FString& RawValue)
		{
			TArray<FString> Candidates;
			AddTargetKeyCandidates(RawValue, Candidates);
			if (Candidates.Num() > 0)
			{
				OutCandidatePasses.Add(Candidates);
			}
		};

		AddPass(Target.NodeGuid);
		AddPass(Target.TargetKey);
		AddPass(Target.PinPath);
		AddPass(Target.VisualGroupKey);
		AddPass(Target.DisplayLabel);
	}

	static bool TryReadVector2D(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		FVector2D& OutValue)
	{
		const TSharedPtr<FJsonObject>* VectorJson = nullptr;
		if (!Json.IsValid() || !Json->TryGetObjectField(FieldName, VectorJson) || !VectorJson || !VectorJson->IsValid())
		{
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		if (!(*VectorJson)->TryGetNumberField(TEXT("x"), X)
			|| !(*VectorJson)->TryGetNumberField(TEXT("y"), Y))
		{
			return false;
		}

		OutValue = FVector2D(static_cast<float>(X), static_cast<float>(Y));
		return true;
	}

	static bool TryReadAnchorJson(
		const FString& AnchorJson,
		FRecordedGraphBounds& OutRecordedBounds)
	{
		if (AnchorJson.IsEmpty())
		{
			return false;
		}

		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(AnchorJson);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			return false;
		}

		Json->TryGetStringField(TEXT("anchor_source"), OutRecordedBounds.AnchorSource);
		FVector2D GraphPosition = FVector2D::ZeroVector;
		FVector2D GraphSize = FVector2D(360.0f, 180.0f);
		bool bHasGraphBounds = false;
		if (Json->TryGetBoolField(TEXT("has_graph_bounds"), bHasGraphBounds)
			&& bHasGraphBounds
			&& TryReadVector2D(Json, TEXT("graph_position"), GraphPosition)
			&& TryReadVector2D(Json, TEXT("graph_size"), GraphSize))
		{
			OutRecordedBounds.bHasGraphBounds = true;
			OutRecordedBounds.GraphPosition = GraphPosition;
			OutRecordedBounds.GraphSize = GraphSize;
			if (OutRecordedBounds.AnchorSource.IsEmpty())
			{
				OutRecordedBounds.AnchorSource = TEXT("structured");
			}
		}
		else if (OutRecordedBounds.AnchorSource.IsEmpty())
		{
			OutRecordedBounds.AnchorSource = TEXT("legacy");
		}

		return true;
	}

	static FRecordedGraphBounds GetRecordedBoundsForTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		FRecordedGraphBounds RecordedBounds;
		if (Target.bHasGraphBounds)
		{
			RecordedBounds.bHasGraphBounds = true;
			RecordedBounds.GraphPosition = Target.GraphPosition;
			RecordedBounds.GraphSize = Target.GraphSize;
			RecordedBounds.AnchorSource = TEXT("legacy");
		}

		FRecordedGraphBounds AnchorBounds;
		if (TryReadAnchorJson(Target.AnchorJson, AnchorBounds))
		{
			if (!AnchorBounds.AnchorSource.IsEmpty())
			{
				RecordedBounds.AnchorSource = AnchorBounds.AnchorSource;
			}
			if (AnchorBounds.bHasGraphBounds)
			{
				RecordedBounds.bHasGraphBounds = true;
				RecordedBounds.GraphPosition = AnchorBounds.GraphPosition;
				RecordedBounds.GraphSize = AnchorBounds.GraphSize;
			}
		}

		return RecordedBounds;
	}

	static FString BuildAnchorSourceSummary(const FBoundsDebugCounters& DebugCounters)
	{
		if (DebugCounters.StructuredAnchorSourceCount > 0 && DebugCounters.LegacyAnchorSourceCount > 0)
		{
			return TEXT("mixed");
		}
		if (DebugCounters.StructuredAnchorSourceCount > 0)
		{
			return TEXT("structured");
		}
		if (DebugCounters.LegacyAnchorSourceCount > 0)
		{
			return TEXT("legacy");
		}
		return TEXT("none");
	}

	static bool TryGetEditorNodeBounds(
		const UEdGraphNode* Node,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FSlateRect& OutRect)
	{
		return Node
			&& GraphEditor.IsValid()
			&& GraphEditor->GetBoundsForNode(Node, OutRect, 0.0f);
	}

	static FString BuildNodeDebugSummary(const UEdGraphNode* Node, ENodeBoundsSource Source)
	{
		if (!Node)
		{
			return TEXT("<null>");
		}

		const FString SourceName = Source == ENodeBoundsSource::EditorWidget
			? TEXT("editor")
			: (Source == ENodeBoundsSource::FallbackNode ? TEXT("fallback") : TEXT("none"));
		return FString::Printf(
			TEXT("%s[%s]@(%d,%d,%d,%d)"),
			*Node->GetName(),
			*SourceName,
			Node->NodePosX,
			Node->NodePosY,
			Node->NodeWidth,
			Node->NodeHeight);
	}

	static FVector2D GetFallbackNodeSize(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return FVector2D(FallbackNodeWidth, FallbackNodeHeight);
		}

		const float Width = Node->NodeWidth > 0
			? static_cast<float>(Node->NodeWidth)
			: FallbackNodeWidth;
		const float EstimatedHeight = UEdGraphSchema_K2::EstimateNodeHeight(const_cast<UEdGraphNode*>(Node));
		const float Height = Node->NodeHeight > 0
			? static_cast<float>(Node->NodeHeight)
			: FMath::Max(FallbackNodeHeight, EstimatedHeight);
		return FVector2D(Width, Height);
	}

	static ENodeBoundsSource IncludeNodeBounds(
		const UEdGraphNode* Node,
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FBox2D& InOutBounds,
		bool& bInOutHasBounds)
	{
		if (!Node)
		{
			return ENodeBoundsSource::None;
		}

		FSlateRect EditorRect;
		if (TryGetEditorNodeBounds(Node, GraphEditor, EditorRect))
		{
			InOutBounds += FVector2D(EditorRect.Left, EditorRect.Top);
			InOutBounds += FVector2D(EditorRect.Right, EditorRect.Bottom);
			bInOutHasBounds = true;
			return ENodeBoundsSource::EditorWidget;
		}

		const FVector2D NodePosition(static_cast<float>(Node->NodePosX), static_cast<float>(Node->NodePosY));
		const FVector2D NodeSize = GetFallbackNodeSize(Node);
		InOutBounds += NodePosition;
		InOutBounds += NodePosition + NodeSize;
		bInOutHasBounds = true;
		return ENodeBoundsSource::FallbackNode;
	}

	static bool BuildPaddedBounds(
		const FBox2D& Bounds,
		const bool bHasBounds,
		FVector2D& OutPosition,
		FVector2D& OutSize)
	{
		if (!bHasBounds)
		{
			return false;
		}

		const FVector2D Padding(CommentStylePadding, CommentStylePadding);
		OutPosition = Bounds.Min - Padding;
		OutSize = (Bounds.Max - Bounds.Min) + Padding * 2.0f;
		OutSize.X = FMath::Max(80.0f, OutSize.X);
		OutSize.Y = FMath::Max(40.0f, OutSize.Y);
		return true;
	}

};

bool FBlueprintHelperReviewGraphBounds::DoesNodeMatchTargetKey(const UEdGraphNode* Node, const FString& TargetKey)
{
	TArray<FString> Candidates;
	FBlueprintHelperReviewGraphBoundsLocalUtils::AddTargetKeyCandidates(TargetKey, Candidates);
	for (const FString& Candidate : Candidates)
	{
		if (FBlueprintHelperReviewGraphBoundsLocalUtils::DoesNodeMatchSingleCandidate(Node, Candidate))
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
		FBlueprintHelperReviewGraphBoundsLocalUtils::IncludeNodeBounds(Node, GraphEditor, Bounds, bHasBounds);
	}
	return FBlueprintHelperReviewGraphBoundsLocalUtils::BuildPaddedBounds(Bounds, bHasBounds, OutPosition, OutSize);
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
	FBlueprintHelperReviewGraphBoundsLocalUtils::FBoundsDebugCounters DebugCounters;
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
		const FBlueprintHelperReviewGraphBoundsLocalUtils::FRecordedGraphBounds RecordedBounds =
			FBlueprintHelperReviewGraphBoundsLocalUtils::GetRecordedBoundsForTarget(Target);
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
		FBlueprintHelperReviewGraphBoundsLocalUtils::AddGraphTargetCandidatePasses(Target, CandidatePasses);
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
						if (FBlueprintHelperReviewGraphBoundsLocalUtils::DoesNodeMatchSingleCandidate(Node, Candidate))
						{
							if (IncludedNodes.Contains(Node))
							{
								++DebugCounters.DuplicateMatchedNodeCount;
								bMatchedNode = true;
								break;
							}

							const FBlueprintHelperReviewGraphBoundsLocalUtils::ENodeBoundsSource Source = FBlueprintHelperReviewGraphBoundsLocalUtils::IncludeNodeBounds(Node, GraphEditor, Bounds, bHasBounds);
							if (Source == FBlueprintHelperReviewGraphBoundsLocalUtils::ENodeBoundsSource::EditorWidget)
							{
								++DebugCounters.EditorBoundsCount;
							}
							else if (Source == FBlueprintHelperReviewGraphBoundsLocalUtils::ENodeBoundsSource::FallbackNode)
							{
								++DebugCounters.FallbackBoundsCount;
							}
							++DebugCounters.MatchedNodeCount;
							IncludedNodes.Add(Node);
							DebugCounters.MatchedNodeSummaries.Add(FBlueprintHelperReviewGraphBoundsLocalUtils::BuildNodeDebugSummary(Node, Source));
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

	const bool bBuilt = FBlueprintHelperReviewGraphBoundsLocalUtils::BuildPaddedBounds(Bounds, bHasBounds, OutPosition, OutSize);
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
			*FBlueprintHelperReviewGraphBoundsLocalUtils::BuildAnchorSourceSummary(DebugCounters),
			FBlueprintHelperReviewGraphBoundsLocalUtils::CommentStylePadding,
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
