// Review graph diff bounds utility helpers implementation.

#include "UI/Review/Utils/BlueprintHelperReviewGraphBoundsUtils.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"

void FBlueprintHelperReviewGraphBoundsUtils::AddUniqueTrimmed(TArray<FString>& OutValues, FString Value)
{
	Value.TrimStartAndEndInline();
	if (!Value.IsEmpty())
	{
		OutValues.AddUnique(Value);
	}
}

FString FBlueprintHelperReviewGraphBoundsUtils::LastSegmentAfter(const FString& Value, const TCHAR Separator)
{
	int32 Index = INDEX_NONE;
	if (Value.FindLastChar(Separator, Index))
	{
		return Value.Mid(Index + 1);
	}
	return Value;
}

FString FBlueprintHelperReviewGraphBoundsUtils::StripObjectPath(const FString& Value)
{
	return LastSegmentAfter(LastSegmentAfter(Value, TCHAR('/')), TCHAR('.'));
}

bool FBlueprintHelperReviewGraphBoundsUtils::IsUsefulTargetToken(const FString& Token)
{
	if (Token.Len() < 2)
	{
		return false;
	}

	static const TCHAR* IgnoredTokens[] =
	{
		TEXT("graph"),
		TEXT("node"),
		TEXT("pin"),
		TEXT("block"),
		TEXT("created_node"),
		TEXT("rollback_node"),
		TEXT("rename_added"),
		TEXT("rename_removed"),
	};

	for (const TCHAR* IgnoredToken : IgnoredTokens)
	{
		if (Token.Equals(IgnoredToken, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}
	return true;
}

void FBlueprintHelperReviewGraphBoundsUtils::AddTargetKeyCandidates(
	const FString& RawTargetKey,
	TArray<FString>& OutCandidates)
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
	for (const FString& Part : Parts)
	{
		const FString CleanPart = StripObjectPath(Part);
		if (IsUsefulTargetToken(CleanPart))
		{
			AddUniqueTrimmed(OutCandidates, CleanPart);
		}
	}
}

FString FBlueprintHelperReviewGraphBoundsUtils::NormalizeNodeLabelForCompare(FString Value)
{
	Value.ToLowerInline();
	Value.ReplaceInline(TEXT(" "), TEXT(""));
	Value.ReplaceInline(TEXT("\t"), TEXT(""));
	return Value;
}

bool FBlueprintHelperReviewGraphBoundsUtils::DoesNodeMatchSingleCandidate(
	const UEdGraphNode* Node,
	const FString& Candidate)
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

void FBlueprintHelperReviewGraphBoundsUtils::AddGraphTargetCandidatePasses(
	const FBlueprintHelperReviewAtomicTarget& Target,
	TArray<TArray<FString>>& OutCandidatePasses)
{
	const FString RawValues[] =
	{
		Target.NodeGuid,
		Target.TargetKey,
		Target.PinPath,
		Target.VisualGroupKey,
		Target.DisplayLabel,
	};

	for (const FString& RawValue : RawValues)
	{
		TArray<FString> Candidates;
		AddTargetKeyCandidates(RawValue, Candidates);
		if (Candidates.Num() > 0)
		{
			OutCandidatePasses.Add(Candidates);
		}
	}
}

bool FBlueprintHelperReviewGraphBoundsUtils::TryReadVector2D(
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

bool FBlueprintHelperReviewGraphBoundsUtils::TryReadAnchorJson(
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
	}

	return true;
}

FBlueprintHelperReviewGraphBoundsUtils::FRecordedGraphBounds FBlueprintHelperReviewGraphBoundsUtils::GetRecordedBoundsForTarget(
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	FRecordedGraphBounds RecordedBounds;
	if (Target.bHasGraphBounds)
	{
		RecordedBounds.bHasGraphBounds = true;
		RecordedBounds.GraphPosition = Target.GraphPosition;
		RecordedBounds.GraphSize = Target.GraphSize;
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

FString FBlueprintHelperReviewGraphBoundsUtils::BuildAnchorSourceSummary(
	const FBoundsDebugCounters& DebugCounters)
{
	if (DebugCounters.StructuredAnchorSourceCount > 0)
	{
		return TEXT("structured");
	}
	return TEXT("none");
}

bool FBlueprintHelperReviewGraphBoundsUtils::TryGetEditorNodeBounds(
	const UEdGraphNode* Node,
	const TSharedPtr<SGraphEditor>& GraphEditor,
	FSlateRect& OutRect)
{
	return Node
		&& GraphEditor.IsValid()
		&& GraphEditor->GetBoundsForNode(Node, OutRect, 0.0f);
}

FString FBlueprintHelperReviewGraphBoundsUtils::BuildNodeDebugSummary(
	const UEdGraphNode* Node,
	ENodeBoundsSource Source)
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

FVector2D FBlueprintHelperReviewGraphBoundsUtils::GetFallbackNodeSize(const UEdGraphNode* Node)
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

FBlueprintHelperReviewGraphBoundsUtils::ENodeBoundsSource FBlueprintHelperReviewGraphBoundsUtils::IncludeNodeBounds(
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

bool FBlueprintHelperReviewGraphBoundsUtils::BuildPaddedBounds(
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
