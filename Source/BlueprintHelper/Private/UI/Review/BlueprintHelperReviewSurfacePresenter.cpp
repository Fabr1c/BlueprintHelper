// BlueprintHelper Review surface presenter routing helpers.

#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "GraphEditor.h"
#include "SSubobjectEditor.h"
#include "SKismetInspector.h"
#include "SSubobjectBlueprintEditor.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewDiffBlockNode.h"
#include "UI/Review/BlueprintHelperReviewGraphBounds.h"
#include "UI/Review/BlueprintHelperReviewGraphResolver.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "UObject/UObjectGlobals.h"

void SBlueprintHelperReviewGeometryProbe::Construct(const FArguments& InArgs)
{
	Surface = InArgs._Surface;
	TargetKey = InArgs._TargetKey;
	OnGeometryInvalidated = InArgs._OnGeometryInvalidated;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void SBlueprintHelperReviewGeometryProbe::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D AbsolutePosition = AllottedGeometry.LocalToAbsolute(FVector2D::ZeroVector);
	const bool bHasValidGeometry = LocalSize.X > 0.0f && LocalSize.Y > 0.0f;
	const bool bGeometryChanged = bHasValidGeometry
		&& (!bHadValidGeometry
			|| !AbsolutePosition.Equals(LastAbsolutePosition, 0.5f)
			|| !LocalSize.Equals(LastLocalSize, 0.5f));

	bHadValidGeometry = bHasValidGeometry;
	LastAbsolutePosition = AbsolutePosition;
	LastLocalSize = LocalSize;

	if (bGeometryChanged && OnGeometryInvalidated.IsBound())
	{
		OnGeometryInvalidated.Execute(Surface);
	}
}

namespace BlueprintHelperReviewSurfacePresenterPrivate
{
	static constexpr float ReviewFrameBackgroundOpacity = 0.60f;
	static constexpr float ReviewFrameSelectedBackgroundOpacity = 0.74f;
	static constexpr float ReviewFrameRowGeometryPadding = 10.0f;
	static const FLinearColor ReviewFrameInnerBg = FLinearColor(0.06f, 0.06f, 0.06f, ReviewFrameBackgroundOpacity);

	static FLinearColor GetReviewFrameBackgroundColor(bool bFillBackground)
	{
		return bFillBackground ? ReviewFrameInnerBg : FLinearColor::Transparent;
	}

	static FLinearColor GetReviewFrameFillColor(
		const FLinearColor& FrameColor,
		bool bFillBackground,
		bool bSelected)
	{
		if (!bFillBackground)
		{
			return FLinearColor::Transparent;
		}

		FLinearColor FillColor = FrameColor;
		if (FillColor == FLinearColor::Transparent)
		{
			FillColor = ReviewFrameInnerBg;
		}
		FillColor.A = bSelected ? ReviewFrameSelectedBackgroundOpacity : ReviewFrameBackgroundOpacity;
		return FillColor;
	}

	struct FSlateRowGeometryRecord
	{
		FString AssetPath;
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
		FString SearchText;
		FString DebugMode;
		TWeakPtr<SWidget> RowWidget;
	};

	static TArray<FSlateRowGeometryRecord>& GetSlateRowGeometryRecords()
	{
		static TArray<FSlateRowGeometryRecord> Records;
		return Records;
	}

	static FString NormalizeGeometrySearchText(FString Text)
	{
		Text.ToLowerInline();
		for (int32 Index = Text.Len() - 1; Index >= 0; --Index)
		{
			const TCHAR Character = Text[Index];
			if (!FChar::IsAlnum(Character))
			{
				Text.RemoveAt(Index);
			}
		}
		return Text;
	}

	static void AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms)
	{
		OutTerms.AddUnique(NormalizeGeometrySearchText(RawText));
		FString CurrentPart;
		for (int32 Index = 0; Index < RawText.Len(); ++Index)
		{
			const TCHAR Character = RawText[Index];
			if (FChar::IsAlnum(Character))
			{
				CurrentPart.AppendChar(Character);
				continue;
			}

			const FString Term = NormalizeGeometrySearchText(CurrentPart);
			if (Term.Len() >= 2)
			{
				OutTerms.AddUnique(Term);
			}
			CurrentPart.Reset();
		}

		const FString TailTerm = NormalizeGeometrySearchText(CurrentPart);
		if (TailTerm.Len() >= 2)
		{
			OutTerms.AddUnique(TailTerm);
		}
	}

	static bool GeometrySearchTextMatches(const FString& RowSearchText, const FString& TargetText)
	{
		const FString NormalizedRow = NormalizeGeometrySearchText(RowSearchText);
		TArray<FString> TargetTerms;
		AddGeometrySearchTerms(TargetText, TargetTerms);

		for (const FString& TargetTerm : TargetTerms)
		{
			if (TargetTerm.Len() < 2)
			{
				continue;
			}
			if (NormalizedRow.Contains(TargetTerm) || TargetTerm.Contains(NormalizedRow))
			{
				return true;
			}
		}
		return false;
	}

	static FString ExtractReadableTail(FString Text)
	{
		Text.TrimStartAndEndInline();
		if (Text.IsEmpty())
		{
			return Text;
		}

		int32 DelimiterIndex = INDEX_NONE;
		if (Text.FindLastChar(TEXT(':'), DelimiterIndex)
			|| Text.FindLastChar(TEXT('/'), DelimiterIndex)
			|| Text.FindLastChar(TEXT('.'), DelimiterIndex))
		{
			Text = Text.Mid(DelimiterIndex + 1);
		}
		Text.TrimStartAndEndInline();

		if (Text.EndsWith(TEXT(" Widget"), ESearchCase::IgnoreCase))
		{
			Text.LeftChopInline(7);
		}
		if (Text.EndsWith(TEXT(" Row"), ESearchCase::IgnoreCase))
		{
			Text.LeftChopInline(4);
		}
		Text.TrimStartAndEndInline();
		return Text;
	}

	static FString GetReadableTargetName(const FBlueprintHelperReviewVisibleChange& Change)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if ((Target.Surface == EBlueprintHelperReviewSurface::UMGWidgetTree
				|| Target.Surface == EBlueprintHelperReviewSurface::DataTable)
				&& !Target.TargetKey.IsEmpty())
			{
				return ExtractReadableTail(Target.TargetKey);
			}
			if (!Target.PropertyPath.IsEmpty())
			{
				return ExtractReadableTail(Target.PropertyPath);
			}
			if (Target.Surface == EBlueprintHelperReviewSurface::Components
				&& !Target.ComponentPath.IsEmpty())
			{
				return ExtractReadableTail(Target.ComponentPath);
			}
			if (!Target.TargetKey.IsEmpty())
			{
				return ExtractReadableTail(Target.TargetKey);
			}
			if (!Target.DisplayLabel.IsEmpty())
			{
				return ExtractReadableTail(Target.DisplayLabel);
			}
		}

		return ExtractReadableTail(Change.DisplayLabel.IsEmpty() ? Change.LocationKey : Change.DisplayLabel);
	}

	static FString GetReadableTargetSuffix(const FBlueprintHelperReviewVisibleChange& Change)
	{
		const FBlueprintHelperReviewAtomicTarget* Target = Change.AtomicTargets.Num() > 0
			? &Change.AtomicTargets[0]
			: nullptr;
		const FString TargetKind = Target ? Target->TargetKind.ToLower() : FString();
		const EBlueprintHelperReviewSurface Surface = Target ? Target->Surface : EBlueprintHelperReviewSurface::Unknown;

		if (Surface == EBlueprintHelperReviewSurface::UMGWidgetTree)
		{
			return FString();
		}
		if (TargetKind.Contains(TEXT("datatable_row")) || Surface == EBlueprintHelperReviewSurface::DataTable)
		{
			return TEXT("\u884c");
		}
		if (TargetKind.Contains(TEXT("component")) || Surface == EBlueprintHelperReviewSurface::Components)
		{
			return TEXT("\u7ec4\u4ef6");
		}
		if (TargetKind.Contains(TEXT("signature")))
		{
			return TEXT("\u7b7e\u540d");
		}
		if (TargetKind.Contains(TEXT("variable"))
			|| TargetKind.Contains(TEXT("property"))
			|| Surface == EBlueprintHelperReviewSurface::DataAsset)
		{
			return TEXT("\u53d8\u91cf");
		}
		return FString();
	}

	static FString GetReadableChangeVerb(EBlueprintHelperReviewChangeKind ChangeKind)
	{
		switch (ChangeKind)
		{
		case EBlueprintHelperReviewChangeKind::Added:
			return TEXT("\u65b0\u589e\u4e86");
		case EBlueprintHelperReviewChangeKind::Removed:
			return TEXT("\u5220\u9664\u4e86");
		case EBlueprintHelperReviewChangeKind::Renamed:
			return TEXT("\u91cd\u547d\u540d\u4e86");
		default:
			return TEXT("\u4fee\u6539\u4e86");
		}
	}

	static void ApplyRowGeometryPadding(FBlueprintHelperReviewSurfaceGeometryAnchor& Anchor)
	{
		const FVector2D Padding(ReviewFrameRowGeometryPadding, ReviewFrameRowGeometryPadding);
		FVector2D PaddedPosition = Anchor.Position - Padding;
		FVector2D PaddedSize = Anchor.Size + Padding * 2.0f;

		if (PaddedPosition.X < 0.0f)
		{
			PaddedSize.X += PaddedPosition.X;
			PaddedPosition.X = 0.0f;
		}
		if (PaddedPosition.Y < 0.0f)
		{
			PaddedSize.Y += PaddedPosition.Y;
			PaddedPosition.Y = 0.0f;
		}
		if (Anchor.HostSize.X > 0.0f && PaddedPosition.X + PaddedSize.X > Anchor.HostSize.X)
		{
			PaddedSize.X = Anchor.HostSize.X - PaddedPosition.X;
		}
		if (Anchor.HostSize.Y > 0.0f && PaddedPosition.Y + PaddedSize.Y > Anchor.HostSize.Y)
		{
			PaddedSize.Y = Anchor.HostSize.Y - PaddedPosition.Y;
		}

		Anchor.Position = PaddedPosition;
		Anchor.Size = FVector2D(FMath::Max(0.0f, PaddedSize.X), FMath::Max(0.0f, PaddedSize.Y));
	}

	static bool BuildGeometryAnchorFromRowWidget(
		const TSharedPtr<SWidget>& RowWidget,
		const TSharedPtr<SWidget>& OverlayWidget,
		const FString& TargetText,
		const TCHAR* DebugMode,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		if (!RowWidget.IsValid())
		{
			OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
			OutAnchor.TargetText = TargetText;
			return false;
		}
		if (!OverlayWidget.IsValid())
		{
			OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
			OutAnchor.TargetText = TargetText;
			return false;
		}

		const FGeometry& RowGeometry = RowWidget->GetTickSpaceGeometry();
		const FGeometry& OverlayGeometry = OverlayWidget->GetTickSpaceGeometry();
		const FVector2D RowLocalSize = RowGeometry.GetLocalSize();
		const FVector2D OverlayLocalSize = OverlayGeometry.GetLocalSize();
		if (RowLocalSize.X <= 0.0f || RowLocalSize.Y <= 0.0f
			|| OverlayLocalSize.X <= 0.0f || OverlayLocalSize.Y <= 0.0f)
		{
			OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
			OutAnchor.TargetText = TargetText;
			return false;
		}

		const FVector2D AbsoluteTopLeft = RowGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D AbsoluteBottomRight = RowGeometry.LocalToAbsolute(RowLocalSize);
		const FVector2D LocalTopLeft = OverlayGeometry.AbsoluteToLocal(AbsoluteTopLeft);
		const FVector2D LocalBottomRight = OverlayGeometry.AbsoluteToLocal(AbsoluteBottomRight);
		const FVector2D LocalSize = LocalBottomRight - LocalTopLeft;
		if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
		{
			OutAnchor.Reason = TEXT("invalid_slate_row_geometry");
			OutAnchor.TargetText = TargetText;
			return false;
		}

		OutAnchor.bIsValid = true;
		OutAnchor.Position = LocalTopLeft;
		OutAnchor.Size = LocalSize;
		OutAnchor.HostSize = OverlayLocalSize;
		OutAnchor.TargetText = TargetText;
		OutAnchor.Reason = TEXT("stable_slate_row_geometry");
		OutAnchor.DebugMode = DebugMode ? DebugMode : TEXT("slate_row");
		return true;
	}

	static void AddUniqueReviewCandidate(TArray<FString>& OutCandidates, FString Candidate)
	{
		Candidate.TrimStartAndEndInline();
		if (!Candidate.IsEmpty())
		{
			OutCandidates.AddUnique(Candidate);
		}
	}

	static void AddComponentNameCandidatesFromText(const FString& RawText, TArray<FString>& OutCandidates)
	{
		FString Text = RawText;
		Text.TrimStartAndEndInline();
		if (Text.IsEmpty())
		{
			return;
		}

		int32 DelimiterIndex = INDEX_NONE;
		if (Text.FindLastChar(TEXT(':'), DelimiterIndex))
		{
			Text = Text.Mid(DelimiterIndex + 1);
		}
		if (Text.FindLastChar(TEXT('/'), DelimiterIndex))
		{
			Text = Text.Mid(DelimiterIndex + 1);
		}

		FString ComponentPart = Text;
		if (ComponentPart.FindChar(TEXT('.'), DelimiterIndex))
		{
			ComponentPart.LeftInline(DelimiterIndex);
		}
		AddUniqueReviewCandidate(OutCandidates, ComponentPart);
		AddUniqueReviewCandidate(OutCandidates, Text);
	}

	static void AddMyBlueprintSearchAliases(const FString& Name, TArray<FString>& OutAliases)
	{
		AddUniqueReviewCandidate(OutAliases, Name);
		AddUniqueReviewCandidate(OutAliases, FString::Printf(TEXT("signature:%s"), *Name));
		AddUniqueReviewCandidate(OutAliases, FString::Printf(TEXT("function:%s"), *Name));
		AddUniqueReviewCandidate(OutAliases, FString::Printf(TEXT("macro:%s"), *Name));
		AddUniqueReviewCandidate(OutAliases, FString::Printf(TEXT("event_dispatcher:%s"), *Name));
		AddUniqueReviewCandidate(OutAliases, FString::Printf(TEXT("blueprint_variable:%s"), *Name));
	}

	static bool SearchTextMatchesAnyCandidate(const FString& SearchText, const TArray<FString>& Candidates)
	{
		for (const FString& Candidate : Candidates)
		{
			if (GeometrySearchTextMatches(SearchText, Candidate))
			{
				return true;
			}
		}
		return false;
	}

	static FSubobjectEditorTreeNodePtrType FindComponentNodeByText(
		const TArray<FSubobjectEditorTreeNodePtrType>& Nodes,
		const TArray<FString>& Candidates)
	{
		for (const FSubobjectEditorTreeNodePtrType& Node : Nodes)
		{
			if (!Node.IsValid())
			{
				continue;
			}

			if (SearchTextMatchesAnyCandidate(Node->GetVariableName().ToString(), Candidates)
				|| SearchTextMatchesAnyCandidate(Node->GetDisplayString(), Candidates))
			{
				return Node;
			}

			if (FSubobjectEditorTreeNodePtrType ChildMatch = FindComponentNodeByText(Node->GetChildren(), Candidates))
			{
				return ChildMatch;
			}
		}
		return nullptr;
	}

	static TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> FindMyBlueprintRowByText(
		const TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>& Rows,
		const FString& TargetText)
	{
		for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Row : Rows)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			if (!Row->SearchText.IsEmpty()
				&& GeometrySearchTextMatches(Row->SearchText, TargetText))
			{
				return Row;
			}

			if (TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem> ChildMatch =
				FindMyBlueprintRowByText(Row->Children, TargetText))
			{
				return ChildMatch;
			}
		}
		return nullptr;
	}

	static bool LegacyFallbackMatchesSurface(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface)
	{
		const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
		switch (Surface)
		{
		case EBlueprintHelperReviewSurface::Graph:
			return !Change.GraphName.IsEmpty()
				|| Location.Contains(TEXT("graph:"))
				|| Location.Contains(TEXT("node:"))
				|| Location.Contains(TEXT("pin:"));
		case EBlueprintHelperReviewSurface::Components:
			return Location.Contains(TEXT("component"));
		case EBlueprintHelperReviewSurface::MyBlueprint:
			if (Location.Contains(TEXT("component")))
			{
				return false;
			}
			return Location.Contains(TEXT("my_blueprint"))
				|| Location.Contains(TEXT("function"))
				|| Location.Contains(TEXT("macro"))
				|| Location.Contains(TEXT("variable"))
				|| Location.Contains(TEXT("dispatcher"))
				|| Location.Contains(TEXT("delegate"));
		case EBlueprintHelperReviewSurface::Details:
			return Change.ChangeKind == EBlueprintHelperReviewChangeKind::VariableModified
				|| Change.ChangeKind == EBlueprintHelperReviewChangeKind::SignatureModified
				|| Location.Contains(TEXT("property"))
				|| Location.Contains(TEXT("variable"))
				|| Location.Contains(TEXT("signature"))
				|| Location.Contains(TEXT("dispatcher"));
		case EBlueprintHelperReviewSurface::UMGWidgetTree:
			return BlueprintHelperReviewShouldShowInUMGWidgetTree(Change);
		case EBlueprintHelperReviewSurface::DataTable:
			return BlueprintHelperReviewShouldShowInDataTable(Change);
		case EBlueprintHelperReviewSurface::DataAsset:
			return BlueprintHelperReviewShouldShowInDataAsset(Change);
		default:
			return false;
		}
	}

	static const TCHAR* SurfaceDebugName(EBlueprintHelperReviewSurface Surface)
	{
		switch (Surface)
		{
		case EBlueprintHelperReviewSurface::Graph:       return TEXT("Graph");
		case EBlueprintHelperReviewSurface::Components:  return TEXT("Components");
		case EBlueprintHelperReviewSurface::MyBlueprint: return TEXT("MyBlueprint");
		case EBlueprintHelperReviewSurface::Details:     return TEXT("Details");
		case EBlueprintHelperReviewSurface::UMGWidgetTree: return TEXT("UMGWidgetTree");
		case EBlueprintHelperReviewSurface::DataTable:   return TEXT("DataTable");
		case EBlueprintHelperReviewSurface::DataAsset:   return TEXT("DataAsset");
		default:                                         return TEXT("Unknown");
		}
	}

	static TSharedRef<SWidget> BuildReviewPlaceholder(const FString& Message)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
			.Padding(8.0f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.58f, 0.58f, 1.0f)))
				.AutoWrapText(true)
				.Text(FText::FromString(Message))
			];
	}

	static bool IsReviewPropertyEditingEnabled()
	{
		return false;
	}

	static UBlueprint* CreateReviewPreviewBlueprint(const UBlueprint* SourceBlueprint)
	{
		const FName PreviewName = MakeUniqueObjectName(
			GetTransientPackage(),
			UBlueprint::StaticClass(),
			FName(TEXT("BlueprintHelperReviewPreviewBP")));

		UBlueprint* PreviewBlueprint = NewObject<UBlueprint>(
			GetTransientPackage(),
			PreviewName,
			RF_Transient);

		if (SourceBlueprint)
		{
			PreviewBlueprint->BlueprintType = SourceBlueprint->BlueprintType;
			PreviewBlueprint->ParentClass = SourceBlueprint->ParentClass;
			PreviewBlueprint->GeneratedClass = SourceBlueprint->GeneratedClass;
			PreviewBlueprint->SkeletonGeneratedClass = SourceBlueprint->SkeletonGeneratedClass;
		}

		return PreviewBlueprint;
	}

	static void AttachPreviewGraphToMatchingBlueprintList(
		const UBlueprint* SourceBlueprint,
		const UEdGraph* SourceGraph,
		UBlueprint* PreviewBlueprint,
		UEdGraph* PreviewGraph)
	{
		if (!SourceBlueprint || !SourceGraph || !PreviewBlueprint || !PreviewGraph)
		{
			return;
		}

		if (SourceBlueprint->UbergraphPages.Contains(SourceGraph))
		{
			PreviewBlueprint->UbergraphPages.Add(PreviewGraph);
			return;
		}
		if (SourceBlueprint->FunctionGraphs.Contains(SourceGraph))
		{
			PreviewBlueprint->FunctionGraphs.Add(PreviewGraph);
			return;
		}
		if (SourceBlueprint->MacroGraphs.Contains(SourceGraph))
		{
			PreviewBlueprint->MacroGraphs.Add(PreviewGraph);
			return;
		}
		if (SourceBlueprint->DelegateSignatureGraphs.Contains(SourceGraph))
		{
			PreviewBlueprint->DelegateSignatureGraphs.Add(PreviewGraph);
		}
	}

	class SBlueprintHelperReviewDiffFrame : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBlueprintHelperReviewDiffFrame)
			: _FrameColor(FSlateColor(FLinearColor::Transparent))
			, _ShowActions(false)
			, _FillBackground(true)
			, _Selected(false)
		{
		}

			SLATE_ATTRIBUTE(FSlateColor, FrameColor)
			SLATE_ATTRIBUTE(bool, ShowActions)
			SLATE_ARGUMENT(bool, FillBackground)
			SLATE_ARGUMENT(bool, Selected)
			SLATE_EVENT(FOnClicked, OnAccept)
			SLATE_EVENT(FOnClicked, OnReject)
			SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			FrameColor = InArgs._FrameColor;
			ShowActions = InArgs._ShowActions;
			bFillBackground = InArgs._FillBackground;
			bSelected = InArgs._Selected;
			OnAccept = InArgs._OnAccept;
			OnReject = InArgs._OnReject;

			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(this, &SBlueprintHelperReviewDiffFrame::GetFrameBrush)
					.Padding(3.0f)
					[
						SNew(SBorder)
						.BorderImage(this, &SBlueprintHelperReviewDiffFrame::GetInnerBrush)
						.Padding(0.0f)
						[
							InArgs._Content.Widget
						]
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(0.0f, 0.0f, 8.0f, 8.0f)
				[
					SNew(SBorder)
					.BorderImage(&ActionsBrush)
					.Padding(5.0f)
					.Visibility(this, &SBlueprintHelperReviewDiffFrame::GetActionsVisibility)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Accept")))
							.OnClicked(OnAccept)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.Text(FText::FromString(TEXT("Reject")))
							.OnClicked(OnReject)
						]
					]
				]
			];
		}

	private:
		FSlateColor GetFrameColor() const
		{
			return FrameColor.Get();
		}

		const FSlateBrush* GetFrameBrush() const
		{
			FrameBrush = FSlateRoundedBoxBrush(
				FLinearColor::Transparent,
				7.0f,
				GetFrameColor().GetSpecifiedColor(),
				4.0f);
			return &FrameBrush;
		}

		const FSlateBrush* GetInnerBrush() const
		{
			InnerBrush = FSlateRoundedBoxBrush(
				GetReviewFrameFillColor(GetFrameColor().GetSpecifiedColor(), bFillBackground, bSelected),
				5.0f);
			return &InnerBrush;
		}

		EVisibility GetActionsVisibility() const
		{
			return ShowActions.Get(false) && IsHovered()
				? EVisibility::Visible
				: EVisibility::Collapsed;
		}

		TAttribute<FSlateColor> FrameColor;
		TAttribute<bool> ShowActions;
		bool bFillBackground = true;
		bool bSelected = false;
		FOnClicked OnAccept;
		FOnClicked OnReject;
		mutable FSlateRoundedBoxBrush FrameBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 4.0f);
		mutable FSlateRoundedBoxBrush InnerBrush = FSlateRoundedBoxBrush(ReviewFrameInnerBg, 5.0f);
		FSlateRoundedBoxBrush ActionsBrush = FSlateRoundedBoxBrush(
			FLinearColor(0.02f, 0.02f, 0.02f, 0.95f),
			5.0f);
	};

	static bool IsSameChange(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Left,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Right)
	{
		return Left.IsValid() && Right.IsValid() && Left->ChangeId == Right->ChangeId;
	}

	static bool BuildGraphBoundsForChange(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const UEdGraph* PreviewGraphToEdit,
		const FString& GraphName,
		const TSharedPtr<SGraphEditor>& GraphEditorForBounds,
		FVector2D& OutPosition,
		FVector2D& OutSize,
		FString* OutDebugSummary)
	{
		if (!Item.IsValid())
		{
			if (OutDebugSummary)
			{
				*OutDebugSummary = TEXT("built=0 invalid change item");
			}
			return false;
		}

		if (Item->AtomicTargets.Num() > 0)
		{
			return FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
				Item->AtomicTargets,
				PreviewGraphToEdit,
				GraphName,
				GraphEditorForBounds,
				OutPosition,
				OutSize,
				OutDebugSummary);
		}

		if (!Item->GraphName.IsEmpty() && !GraphName.IsEmpty() && Item->GraphName != GraphName)
		{
			if (OutDebugSummary)
			{
				*OutDebugSummary = FString::Printf(
					TEXT("built=0 graph mismatch itemGraph=\"%s\" currentGraph=\"%s\""),
					*Item->GraphName,
					*GraphName);
			}
			return false;
		}

		FBlueprintHelperReviewAtomicTarget FallbackTarget;
		FallbackTarget.Surface = EBlueprintHelperReviewSurface::Graph;
		FallbackTarget.GraphName = Item->GraphName;
		FallbackTarget.TargetKey = Item->LocationKey;
		FallbackTarget.NodeGuid = Item->LocationKey;
		TArray<FBlueprintHelperReviewAtomicTarget> FallbackTargets;
		FallbackTargets.Add(FallbackTarget);
		return FBlueprintHelperReviewGraphBounds::BuildBoundsForTargets(
			FallbackTargets,
			PreviewGraphToEdit,
			GraphName,
			GraphEditorForBounds,
			OutPosition,
			OutSize,
			OutDebugSummary);
	}

	static void AddGraphDiffBlocks(
		UEdGraph* PreviewGraphToEdit,
		const UEdGraph* SourceGraph,
		const TSharedPtr<SGraphEditor>& GraphEditorForBounds,
		const FBlueprintHelperReviewGraphPresenterArgs& Args)
	{
		if (!PreviewGraphToEdit)
		{
			return;
		}

		const FString CurrentAssetPath = Args.SelectedChange.IsValid() ? Args.SelectedChange->AssetPath : FString();
		const FString GraphName = SourceGraph ? SourceGraph->GetName() : (Args.SelectedChange.IsValid() ? Args.SelectedChange->GraphName : FString());
		const UEdGraph* BoundsGraph = SourceGraph ? SourceGraph : PreviewGraphToEdit;
		if (!Args.ChangeItems)
		{
			return;
		}

		for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : *Args.ChangeItems)
		{
			if (!Item.IsValid())
			{
				continue;
			}
			if (!CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
			{
				continue;
			}

			const FBlueprintHelperReviewSurfaceRouteDecision RouteDecision =
				FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
					*Item,
					EBlueprintHelperReviewSurface::Graph);
			if (Args.AddDebugMessage)
			{
				const EBlueprintHelperReviewAssetKind AssetKind = Args.AssetContext
					? Args.AssetContext->AssetKind
					: EBlueprintHelperReviewAssetKind::Unknown;
				Args.AddDebugMessage(FBlueprintHelperReviewSurfacePresenterRouter::BuildRouteDebugSummary(
					*Item,
					EBlueprintHelperReviewSurface::Graph,
					RouteDecision,
					BlueprintHelperReviewAssetKindToString(AssetKind)));
			}
			if (!RouteDecision.bShouldShow)
			{
				continue;
			}

			FVector2D Position = FVector2D::ZeroVector;
			FVector2D Size = FVector2D::ZeroVector;
			FString BoundsDebug;
			if (!BuildGraphBoundsForChange(Item, BoundsGraph, GraphName, GraphEditorForBounds, Position, Size, &BoundsDebug))
			{
				if (Args.AddDebugMessage)
				{
					Args.AddDebugMessage(FString::Printf(
						TEXT("GraphDiff bounds failed change=%s selected=%d boundsGraph=\"%s\" result=hidden reason=no_real_graph_anchor %s"),
						*Item->ChangeId,
						IsSameChange(Item, Args.SelectedChange) ? 1 : 0,
						BoundsGraph ? *BoundsGraph->GetName() : TEXT("<none>"),
						*BoundsDebug));
				}
				continue;
			}
			else if (Args.AddDebugMessage)
			{
				Args.AddDebugMessage(FString::Printf(
					TEXT("GraphDiff bounds change=%s selected=%d boundsGraph=\"%s\" %s"),
					*Item->ChangeId,
					IsSameChange(Item, Args.SelectedChange) ? 1 : 0,
					BoundsGraph ? *BoundsGraph->GetName() : TEXT("<none>"),
					*BoundsDebug));
			}

			UBlueprintHelperReviewDiffBlockNode* DiffNode = NewObject<UBlueprintHelperReviewDiffBlockNode>(PreviewGraphToEdit);
			DiffNode->SetFlags(RF_Transient);
			DiffNode->CreateNewGuid();
			DiffNode->NodePosX = FMath::FloorToInt(Position.X);
			DiffNode->NodePosY = FMath::FloorToInt(Position.Y);
			DiffNode->NodeWidth = FMath::CeilToInt(Size.X);
			DiffNode->NodeHeight = FMath::CeilToInt(Size.Y);
			DiffNode->Configure(
				Item->ChangeId,
				Item->DisplayLabel,
				Args.GetChangeColor ? Args.GetChangeColor(Item->ChangeKind).GetSpecifiedColor() : FLinearColor::Transparent,
				IsSameChange(Item, Args.SelectedChange),
				[OnAcceptChangeId = Args.OnAcceptChangeId](const FString& ChangeId)
				{
					return OnAcceptChangeId ? OnAcceptChangeId(ChangeId) : FReply::Handled();
				},
				[OnRejectChangeId = Args.OnRejectChangeId](const FString& ChangeId)
				{
					return OnRejectChangeId ? OnRejectChangeId(ChangeId) : FReply::Handled();
				});
			PreviewGraphToEdit->AddNode(DiffNode, false, false);
			if (Args.AddDebugMessage)
			{
				Args.AddDebugMessage(FString::Printf(
					TEXT("GraphDiff node created change=%s graph=\"%s\" pos=(%d,%d) size=(%d,%d) previewNodes=%d"),
					*Item->ChangeId,
					*GraphName,
					DiffNode->NodePosX,
					DiffNode->NodePosY,
					DiffNode->NodeWidth,
					DiffNode->NodeHeight,
					PreviewGraphToEdit->Nodes.Num()));
			}
		}
	}

	static void JumpToSelectedGraphDiffBlock(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange,
		FBlueprintHelperReviewGraphPresenterState& State,
		const TFunction<void(const FString&)>& AddDebugMessage)
	{
		if (!SelectedChange.IsValid() || !State.PreviewGraph.IsValid() || !State.GraphEditorWidget.IsValid())
		{
			if (AddDebugMessage)
			{
				AddDebugMessage(TEXT("GraphDiff jump skipped because selection, preview graph, or editor is invalid."));
			}
			return;
		}

		bool bJumped = false;
		for (UEdGraphNode* Node : State.PreviewGraph->Nodes)
		{
			if (const UBlueprintHelperReviewDiffBlockNode* DiffNode = Cast<UBlueprintHelperReviewDiffBlockNode>(Node))
			{
				if (DiffNode->ChangeId == SelectedChange->ChangeId)
				{
					State.GraphEditorWidget->JumpToNode(Node, false, false);
					bJumped = true;
					if (AddDebugMessage)
					{
						AddDebugMessage(FString::Printf(
							TEXT("GraphDiff jump change=%s pos=(%d,%d) size=(%d,%d)"),
							*SelectedChange->ChangeId,
							DiffNode->NodePosX,
							DiffNode->NodePosY,
							DiffNode->NodeWidth,
							DiffNode->NodeHeight));
					}
					break;
				}
			}
		}

		if (!bJumped && AddDebugMessage)
		{
			AddDebugMessage(FString::Printf(
				TEXT("GraphDiff jump missed change=%s previewNodes=%d"),
				*SelectedChange->ChangeId,
				State.PreviewGraph->Nodes.Num()));
		}
	}

	static FString GetReviewListTargetText(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		EBlueprintHelperReviewSurface Surface)
	{
		if (!Item.IsValid())
		{
			return FString();
		}

		for (const FBlueprintHelperReviewAtomicTarget& Target : Item->AtomicTargets)
		{
			if (Target.Surface != Surface)
			{
				continue;
			}
			if (!Target.PropertyPath.IsEmpty())
			{
				return Target.PropertyPath;
			}
			if (!Target.ComponentPath.IsEmpty())
			{
				return Target.ComponentPath;
			}
			if (!Target.TargetKey.IsEmpty())
			{
				return Target.TargetKey;
			}
			if (!Target.DisplayLabel.IsEmpty())
			{
				return Target.DisplayLabel;
			}
		}

		return Item->LocationKey.IsEmpty() ? Item->DisplayLabel : Item->LocationKey;
	}

	static TSharedRef<SWidget> BuildSlateRowGeometryFrame(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
	{
		const bool bSelected = IsSameChange(Item, Args.SelectedChange);
		const FSlateColor FrameColor = bSelected && Item.IsValid() && Args.GetSelectedDiffColor
			? Args.GetSelectedDiffColor()
			: (Item.IsValid() && Args.GetChangeColor ? Args.GetChangeColor(Item->ChangeKind) : FSlateColor(FLinearColor::Transparent));

		return FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
			Item,
			SNullWidget::NullWidget,
			false,
			true,
			FrameColor,
			Args.OnAcceptChange,
			Args.OnRejectChange,
			bSelected);
	}

	struct FReviewPanelVisibleFrame
	{
		TSharedPtr<FBlueprintHelperReviewVisibleChange> Item;
		FString TargetText;
		FBlueprintHelperReviewSurfaceGeometryAnchor GeometryAnchor;
		bool bHasStableGeometry = false;
	};

	static bool TryResolveSlateRowGeometry(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetText,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
	{
		if (!Item.IsValid() || !Args.ResolveRowGeometry.IsBound())
		{
			OutAnchor.Reason = TEXT("no_stable_slate_geometry");
			return false;
		}

		FBlueprintHelperReviewSurfaceGeometryAnchor CandidateAnchor;
		const bool bResolved = Args.ResolveRowGeometry.Execute(*Item, Surface, CandidateAnchor);
		if (!bResolved || !CandidateAnchor.bIsValid)
		{
			OutAnchor = CandidateAnchor;
			if (OutAnchor.Reason.IsEmpty())
			{
				OutAnchor.Reason = TEXT("no_stable_slate_geometry");
			}
			return false;
		}

		if (CandidateAnchor.Size.X <= 0.0f || CandidateAnchor.Size.Y <= 0.0f)
		{
			OutAnchor = CandidateAnchor;
			OutAnchor.bIsValid = false;
			OutAnchor.Reason = TEXT("invalid_slate_row_geometry");
			return false;
		}

		if (CandidateAnchor.TargetText.IsEmpty())
		{
			CandidateAnchor.TargetText = TargetText;
		}
		if (CandidateAnchor.Reason.IsEmpty())
		{
			CandidateAnchor.Reason = TEXT("stable_slate_row_geometry");
		}
		if (CandidateAnchor.DebugMode.IsEmpty())
		{
			CandidateAnchor.DebugMode = TEXT("slate_row");
		}

		OutAnchor = CandidateAnchor;
		return true;
	}

	static TSharedRef<SWidget> BuildGeometryOnlyOverlay(
		const TArray<FReviewPanelVisibleFrame>& VisibleFrames,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		EBlueprintHelperReviewSurface Surface)
	{
		TSharedRef<SCanvas> GeometryCanvas = SNew(SCanvas);
		int32 StableFrameCount = 0;
		for (const FReviewPanelVisibleFrame& Frame : VisibleFrames)
		{
			const FString TargetText = Frame.GeometryAnchor.TargetText.IsEmpty()
				? Frame.TargetText
				: Frame.GeometryAnchor.TargetText;
			if (!Frame.bHasStableGeometry)
			{
				if (Args.AddDebugMessage && Frame.Item.IsValid())
				{
					const FString FrameReason = Frame.GeometryAnchor.Reason.IsEmpty()
						? TEXT("geometry_not_ready")
						: Frame.GeometryAnchor.Reason;
					Args.AddDebugMessage(FString::Printf(
						TEXT("ReviewFrameGeometry change=%s surface=%s mode=slate_row result=pending reason=geometry_not_ready frameReason=%s target=\"%s\""),
						*Frame.Item->ChangeId,
						BlueprintHelperReviewSurfaceToString(Surface),
						*FrameReason,
						*TargetText));
				}
				continue;
			}

			if (Args.AddDebugMessage && Frame.Item.IsValid())
			{
				const FString GeometryMode = Frame.GeometryAnchor.DebugMode.IsEmpty()
					? TEXT("slate_row")
					: Frame.GeometryAnchor.DebugMode;
				Args.AddDebugMessage(FString::Printf(
					TEXT("ReviewFrameGeometry change=%s surface=%s mode=%s result=shown reason=%s pos=(%.1f,%.1f) size=(%.1f,%.1f) target=\"%s\""),
					*Frame.Item->ChangeId,
					BlueprintHelperReviewSurfaceToString(Surface),
					*GeometryMode,
					*Frame.GeometryAnchor.Reason,
					static_cast<double>(Frame.GeometryAnchor.Position.X),
					static_cast<double>(Frame.GeometryAnchor.Position.Y),
					static_cast<double>(Frame.GeometryAnchor.Size.X),
					static_cast<double>(Frame.GeometryAnchor.Size.Y),
					*TargetText));
			}

			GeometryCanvas->AddSlot()
			.Position(Frame.GeometryAnchor.Position)
			.Size(Frame.GeometryAnchor.Size)
			[
				BuildSlateRowGeometryFrame(Frame.Item, Args)
			];
			++StableFrameCount;
		}

		if (StableFrameCount > 0)
		{
			return GeometryCanvas;
		}
		return SNullWidget::NullWidget;
	}

	static TSharedRef<SWidget> BuildPanelDiffFrames(
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		EBlueprintHelperReviewSurface Surface,
		bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&))
	{
		const FString CurrentAssetPath = Args.SelectedChange.IsValid() ? Args.SelectedChange->AssetPath : FString();
		TArray<FReviewPanelVisibleFrame> VisibleFrames;

		if (!Args.ChangeItems)
		{
			return SNullWidget::NullWidget;
		}

		for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : *Args.ChangeItems)
		{
			if (!Item.IsValid())
			{
				continue;
			}
			if (!CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
			{
				continue;
			}

			const FBlueprintHelperReviewSurfaceRouteDecision RouteDecision =
				FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(*Item, Surface);
			if (Args.AddDebugMessage)
			{
				const EBlueprintHelperReviewAssetKind AssetKind = Args.AssetContext
					? Args.AssetContext->AssetKind
					: EBlueprintHelperReviewAssetKind::Unknown;
				Args.AddDebugMessage(FBlueprintHelperReviewSurfacePresenterRouter::BuildRouteDebugSummary(
					*Item,
					Surface,
					RouteDecision,
					BlueprintHelperReviewAssetKindToString(AssetKind)));
			}
			if (!RouteDecision.bShouldShow || !Predicate(*Item))
			{
				continue;
			}

			const FString TargetText = GetReviewListTargetText(Item, Surface);
			FReviewPanelVisibleFrame Frame;
			Frame.Item = Item;
			Frame.TargetText = TargetText;
			Frame.bHasStableGeometry = TryResolveSlateRowGeometry(
				Item,
				Surface,
				TargetText,
				Args,
				Frame.GeometryAnchor);
			if (Frame.bHasStableGeometry)
			{
				ApplyRowGeometryPadding(Frame.GeometryAnchor);
			}
			VisibleFrames.Add(Frame);
		}

		if (VisibleFrames.Num() == 0)
		{
			return SNullWidget::NullWidget;
		}

		return BuildGeometryOnlyOverlay(VisibleFrames, Args, Surface);
	}
}

FBlueprintHelperReviewSurfaceRouteDecision FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	FBlueprintHelperReviewSurfaceRouteDecision Decision;
	Decision.bHasExplicitTargets = BlueprintHelperReviewHasExplicitTargets(Change);
	Decision.ExplicitTargetCount = Change.AtomicTargets.Num();
	Decision.MatchingTargetCount = Surface == EBlueprintHelperReviewSurface::Details
		? BlueprintHelperReviewCountDetailsTargets(Change)
		: BlueprintHelperReviewCountSurfaceTargets(Change, Surface);

	if (Decision.bHasExplicitTargets)
	{
		Decision.bShouldShow = Decision.MatchingTargetCount > 0;
		Decision.Reason = Decision.bShouldShow ? TEXT("target_match") : TEXT("no_surface_anchor");
		return Decision;
	}

	Decision.bShouldShow = BlueprintHelperReviewSurfacePresenterPrivate::LegacyFallbackMatchesSurface(Change, Surface);
	Decision.Reason = Decision.bShouldShow ? TEXT("legacy_fallback") : TEXT("legacy_no_match");
	return Decision;
}

bool FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	return RouteChangeToSurface(Change, Surface).bShouldShow;
}

FString FBlueprintHelperReviewSurfacePresenterRouter::BuildRouteDebugSummary(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	const FBlueprintHelperReviewSurfaceRouteDecision& Decision,
	const TCHAR* AssetKindName)
{
	return FString::Printf(
		TEXT("ReviewRoute change=%s surface=%s explicitTargets=%d graphTargets=%d result=%s reason=%s assetKind=%s matchingTargets=%d"),
		*Change.ChangeId,
		BlueprintHelperReviewSurfacePresenterPrivate::SurfaceDebugName(Surface),
		Decision.ExplicitTargetCount,
		BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::Graph),
		Decision.bShouldShow ? TEXT("shown") : TEXT("hidden"),
		*Decision.Reason,
		AssetKindName ? AssetKindName : TEXT("unknown"),
		Decision.MatchingTargetCount);
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewSurfacePresenterRouter::GetStructurePanelSurfaceForAssetKind(
	EBlueprintHelperReviewAssetKind AssetKind)
{
	return AssetKind == EBlueprintHelperReviewAssetKind::WidgetBlueprint
		? EBlueprintHelperReviewSurface::UMGWidgetTree
		: EBlueprintHelperReviewSurface::Components;
}

EBlueprintHelperReviewSurface FBlueprintHelperReviewSurfacePresenterRouter::GetMainWorkspaceSurfaceForAssetKind(
	EBlueprintHelperReviewAssetKind AssetKind)
{
	switch (AssetKind)
	{
	case EBlueprintHelperReviewAssetKind::DataTable:
		return EBlueprintHelperReviewSurface::DataTable;
	case EBlueprintHelperReviewAssetKind::DataAsset:
	case EBlueprintHelperReviewAssetKind::GenericObject:
		return EBlueprintHelperReviewSurface::DataAsset;
	case EBlueprintHelperReviewAssetKind::Blueprint:
	case EBlueprintHelperReviewAssetKind::WidgetBlueprint:
	case EBlueprintHelperReviewAssetKind::Unknown:
	default:
		return EBlueprintHelperReviewSurface::Graph;
	}
}

bool FBlueprintHelperReviewSurfacePresenterRouter::ShouldDetailsPanelOwnOverlay(EBlueprintHelperReviewSurface Surface)
{
	return Surface == EBlueprintHelperReviewSurface::Details;
}

bool FBlueprintHelperReviewSurfacePresenterRouter::ShouldMainWorkspaceOwnOverlay(EBlueprintHelperReviewSurface Surface)
{
	return Surface == EBlueprintHelperReviewSurface::DataTable
		|| Surface == EBlueprintHelperReviewSurface::DataAsset;
}

FString FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	return BlueprintHelperReviewSurfacePresenterPrivate::GetReviewListTargetText(
		MakeShared<FBlueprintHelperReviewVisibleChange>(Change),
		Surface);
}

FString FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	const FString TargetName = BlueprintHelperReviewSurfacePresenterPrivate::GetReadableTargetName(Change);
	const FString Verb = BlueprintHelperReviewSurfacePresenterPrivate::GetReadableChangeVerb(Change.ChangeKind);
	const FString Suffix = BlueprintHelperReviewSurfacePresenterPrivate::GetReadableTargetSuffix(Change);
	if (TargetName.IsEmpty())
	{
		return Change.DisplayLabel.IsEmpty() ? Change.ChangeId : Change.DisplayLabel;
	}
	return FString::Printf(TEXT("%s[%s]%s"), *Verb, *TargetName, *Suffix);
}

void FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	const TSharedRef<SWidget>& RowWidget,
	const TCHAR* DebugMode)
{
	if (AssetPath.IsEmpty() || SearchText.IsEmpty() || Surface == EBlueprintHelperReviewSurface::Unknown)
	{
		return;
	}

	TArray<BlueprintHelperReviewSurfacePresenterPrivate::FSlateRowGeometryRecord>& Records =
		BlueprintHelperReviewSurfacePresenterPrivate::GetSlateRowGeometryRecords();
	Records.RemoveAll([](const BlueprintHelperReviewSurfacePresenterPrivate::FSlateRowGeometryRecord& Record)
	{
		return !Record.RowWidget.IsValid();
	});

	BlueprintHelperReviewSurfacePresenterPrivate::FSlateRowGeometryRecord Record;
	Record.AssetPath = AssetPath;
	Record.Surface = Surface;
	Record.SearchText = SearchText;
	Record.DebugMode = DebugMode ? DebugMode : TEXT("slate_row");
	Record.RowWidget = RowWidget;
	Records.Add(Record);
}

bool FBlueprintHelperReviewSlateRowGeometryRegistry::ResolveRowGeometry(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetText,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (AssetPath.IsEmpty() || TargetText.IsEmpty())
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	TArray<BlueprintHelperReviewSurfacePresenterPrivate::FSlateRowGeometryRecord>& Records =
		BlueprintHelperReviewSurfacePresenterPrivate::GetSlateRowGeometryRecords();
	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		BlueprintHelperReviewSurfacePresenterPrivate::FSlateRowGeometryRecord& Record = Records[Index];
		TSharedPtr<SWidget> RowWidget = Record.RowWidget.Pin();
		if (!RowWidget.IsValid())
		{
			Records.RemoveAt(Index);
			continue;
		}
		if (Record.Surface != Surface || Record.AssetPath != AssetPath)
		{
			continue;
		}
		if (!BlueprintHelperReviewSurfacePresenterPrivate::GeometrySearchTextMatches(Record.SearchText, TargetText))
		{
			continue;
		}

		const FGeometry& RowGeometry = RowWidget->GetTickSpaceGeometry();
		const FGeometry& OverlayGeometry = OverlayWidget->GetTickSpaceGeometry();
		const FVector2D RowLocalSize = RowGeometry.GetLocalSize();
		const FVector2D OverlayLocalSize = OverlayGeometry.GetLocalSize();
		if (RowLocalSize.X <= 0.0f || RowLocalSize.Y <= 0.0f
			|| OverlayLocalSize.X <= 0.0f || OverlayLocalSize.Y <= 0.0f)
		{
			OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
			return false;
		}

		const FVector2D AbsoluteTopLeft = RowGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D AbsoluteBottomRight = RowGeometry.LocalToAbsolute(RowLocalSize);
		const FVector2D LocalTopLeft = OverlayGeometry.AbsoluteToLocal(AbsoluteTopLeft);
		const FVector2D LocalBottomRight = OverlayGeometry.AbsoluteToLocal(AbsoluteBottomRight);
		const FVector2D LocalSize = LocalBottomRight - LocalTopLeft;
		if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
		{
			OutAnchor.Reason = TEXT("invalid_slate_row_geometry");
			return false;
		}

		OutAnchor.bIsValid = true;
		OutAnchor.Position = LocalTopLeft;
		OutAnchor.Size = LocalSize;
		OutAnchor.HostSize = OverlayLocalSize;
		OutAnchor.TargetText = Record.SearchText;
		OutAnchor.Reason = TEXT("stable_slate_row_geometry");
		OutAnchor.DebugMode = Record.DebugMode.IsEmpty() ? TEXT("slate_row") : Record.DebugMode;
		return true;
	}

	OutAnchor.Reason = TEXT("no_matching_slate_row_geometry");
	return false;
}

void FBlueprintHelperReviewGraphPresenterState::Reset()
{
	GraphEditorWidget.Reset();
	PreviewBlueprint.Reset();
	PreviewGraph.Reset();
}

bool FBlueprintHelperReviewGraphPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::Graph);
}

TSharedRef<SWidget> FBlueprintHelperReviewGraphPresenter::BuildContent(
	const FBlueprintHelperReviewGraphPresenterArgs& Args,
	FBlueprintHelperReviewGraphPresenterState& State)
{
	State.Reset();

	if (!Args.SelectedChange.IsValid())
	{
		if (Args.AddDebugMessage)
		{
			Args.AddDebugMessage(TEXT("GraphEditor hidden reason=no_selected_change"));
		}
		return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("No review change selected."));
	}

	const FBlueprintHelperReviewSurfaceRouteDecision RouteDecision =
		FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
			*Args.SelectedChange,
			EBlueprintHelperReviewSurface::Graph);
	if (Args.AddDebugMessage)
	{
		const EBlueprintHelperReviewAssetKind AssetKind = Args.AssetContext
			? Args.AssetContext->AssetKind
			: EBlueprintHelperReviewAssetKind::Unknown;
		Args.AddDebugMessage(FBlueprintHelperReviewSurfacePresenterRouter::BuildRouteDebugSummary(
			*Args.SelectedChange,
			EBlueprintHelperReviewSurface::Graph,
			RouteDecision,
			BlueprintHelperReviewAssetKindToString(AssetKind)));
	}

	if (!RouteDecision.bShouldShow)
	{
		if (Args.AddDebugMessage)
		{
			Args.AddDebugMessage(FString::Printf(
				TEXT("GraphEditor hidden change=%s reason=%s"),
				*Args.SelectedChange->ChangeId,
				*RouteDecision.Reason));
		}
		return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("Selected review change has no Graph anchor."));
	}

	const UBlueprint* SourceBlueprint = Args.AssetContext ? Args.AssetContext->Blueprint.Get() : nullptr;
	if (!SourceBlueprint)
	{
		if (Args.AddDebugMessage)
		{
			const EBlueprintHelperReviewAssetKind AssetKind = Args.AssetContext
				? Args.AssetContext->AssetKind
				: EBlueprintHelperReviewAssetKind::Unknown;
			Args.AddDebugMessage(FString::Printf(
				TEXT("GraphEditor hidden change=%s asset=\"%s\" assetKind=%s reason=no_blueprint_context"),
				*Args.SelectedChange->ChangeId,
				*Args.SelectedChange->AssetPath,
				BlueprintHelperReviewAssetKindToString(AssetKind)));
		}
		return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("No Blueprint graph loaded for this review asset."));
	}

	UEdGraph* SourceGraph = Args.AssetContext
		? ResolveGraphForSelection(*Args.AssetContext, Args.SelectedChange)
		: nullptr;
	if (!SourceGraph && !Args.SelectedChange->GraphName.IsEmpty() && Args.AddDebugMessage)
	{
		Args.AddDebugMessage(FString::Printf(
			TEXT("GraphEditor source graph missing selectedGraph=\"%s\" asset=\"%s\"; using empty Review graph."),
			*Args.SelectedChange->GraphName,
			*Args.SelectedChange->AssetPath));
	}

	State.PreviewBlueprint = TStrongObjectPtr<UBlueprint>(
		BlueprintHelperReviewSurfacePresenterPrivate::CreateReviewPreviewBlueprint(SourceBlueprint));

	if (SourceGraph && State.PreviewBlueprint.IsValid())
	{
		State.PreviewGraph = TStrongObjectPtr<UEdGraph>(FEdGraphUtilities::CloneGraph(SourceGraph, State.PreviewBlueprint.Get()));
		if (State.PreviewGraph.IsValid())
		{
			State.PreviewGraph->SetFlags(RF_Transient);
			State.PreviewGraph->bEditable = false;
			BlueprintHelperReviewSurfacePresenterPrivate::AttachPreviewGraphToMatchingBlueprintList(
				SourceBlueprint,
				SourceGraph,
				State.PreviewBlueprint.Get(),
				State.PreviewGraph.Get());
		}
	}

	if (!State.PreviewGraph.IsValid())
	{
		if (!State.PreviewBlueprint.IsValid())
		{
			State.PreviewBlueprint = TStrongObjectPtr<UBlueprint>(
				BlueprintHelperReviewSurfacePresenterPrivate::CreateReviewPreviewBlueprint(SourceBlueprint));
		}
		State.PreviewGraph = TStrongObjectPtr<UEdGraph>(NewObject<UEdGraph>(
			State.PreviewBlueprint.Get(),
			NAME_None,
			RF_Transient));
		State.PreviewGraph->Schema = UEdGraphSchema_K2::StaticClass();
		State.PreviewGraph->bEditable = false;
	}

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = FText::FromString(TEXT("Review"));
	Appearance.InstructionText = FText::FromString(TEXT("Read-only Review Graph"));
	Appearance.ReadOnlyText = FText::FromString(TEXT("Review Only"));

	TSharedRef<SGraphEditor> Editor = SAssignNew(State.GraphEditorWidget, SGraphEditor)
		.IsEditable(false)
		.DisplayAsReadOnly(false)
		.GraphToEdit(State.PreviewGraph.Get())
		.Appearance(Appearance)
		.ShowGraphStateOverlay(false);

	if (Args.AddDebugMessage)
	{
		Args.AddDebugMessage(FString::Printf(
			TEXT("GraphEditor build sourceGraph=\"%s\" previewGraph=\"%s\" previewNodes=%d timer=disabled"),
			SourceGraph ? *SourceGraph->GetName() : TEXT("<none>"),
			State.PreviewGraph.IsValid() ? *State.PreviewGraph->GetName() : TEXT("<none>"),
			State.PreviewGraph.IsValid() ? State.PreviewGraph->Nodes.Num() : 0));
	}
	BlueprintHelperReviewSurfacePresenterPrivate::AddGraphDiffBlocks(
		State.PreviewGraph.Get(),
		SourceGraph,
		Editor,
		Args);
	Editor->NotifyGraphChanged();

	BlueprintHelperReviewSurfacePresenterPrivate::JumpToSelectedGraphDiffBlock(
		Args.SelectedChange,
		State,
		Args.AddDebugMessage);

	return Editor;
}

UEdGraph* FBlueprintHelperReviewGraphPresenter::ResolveGraphForSelection(
	const FBlueprintHelperReviewAssetContext& Context,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange)
{
	const UBlueprint* Blueprint = Context.Blueprint.Get();
	const FString RequestedGraphName = SelectedChange.IsValid() ? SelectedChange->GraphName : FString();
	return FBlueprintHelperReviewGraphResolver::ResolveGraphForReviewSelection(Blueprint, RequestedGraphName);
}

bool FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::Components);
}

TSharedRef<SWidget> FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.SubobjectEditor.Reset();
	State.OnGeometryInvalidated = OnGeometryInvalidated;

	if (UBlueprint* Blueprint = Context.Blueprint.Get())
	{
		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			if (AActor* ActorCDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
			{
				TSharedRef<SSubobjectBlueprintEditor> Editor = SAssignNew(State.SubobjectEditor, SSubobjectBlueprintEditor)
					.ObjectContext(ActorCDO)
					.AllowEditing(false)
					.HideComponentClassCombo(true);
				return SNew(SBlueprintHelperReviewGeometryProbe)
					.Surface(EBlueprintHelperReviewSurface::Components)
					.TargetKey(Context.AssetPath)
					.OnGeometryInvalidated(OnGeometryInvalidated)
					[
						Editor
					];
			}
		}
	}

	return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("No Blueprint component tree loaded."));
}

bool FBlueprintHelperReviewBlueprintComponentsPresenter::ResolveRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	FState& State,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!State.SubobjectEditor.IsValid())
	{
		OutAnchor.Reason = TEXT("component_editor_unavailable");
		return false;
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	TArray<FString> Candidates;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface != EBlueprintHelperReviewSurface::Components)
		{
			continue;
		}
		BlueprintHelperReviewSurfacePresenterPrivate::AddComponentNameCandidatesFromText(Target.ComponentPath, Candidates);
		BlueprintHelperReviewSurfacePresenterPrivate::AddComponentNameCandidatesFromText(Target.TargetKey, Candidates);
		BlueprintHelperReviewSurfacePresenterPrivate::AddComponentNameCandidatesFromText(Target.PropertyPath, Candidates);
		BlueprintHelperReviewSurfacePresenterPrivate::AddComponentNameCandidatesFromText(Target.DisplayLabel, Candidates);
	}
	BlueprintHelperReviewSurfacePresenterPrivate::AddComponentNameCandidatesFromText(Change.LocationKey, Candidates);
	BlueprintHelperReviewSurfacePresenterPrivate::AddComponentNameCandidatesFromText(Change.DisplayLabel, Candidates);

	if (Candidates.Num() == 0)
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	const TSharedPtr<SSubobjectEditorDragDropTree> Tree = State.SubobjectEditor->GetDragDropTree();
	if (!Tree.IsValid())
	{
		OutAnchor.Reason = TEXT("component_tree_unavailable");
		return false;
	}

	bool bFoundNode = false;
	for (const FString& Candidate : Candidates)
	{
		FSubobjectEditorTreeNodePtrType Node =
			State.SubobjectEditor->FindSlateNodeForVariableName(FName(*Candidate));
		if (!Node.IsValid())
		{
			Node = BlueprintHelperReviewSurfacePresenterPrivate::FindComponentNodeByText(
				State.SubobjectEditor->GetRootNodes(),
				Candidates);
		}
		if (!Node.IsValid())
		{
			continue;
		}

		bFoundNode = true;
		const TSharedPtr<ITableRow> Row = Tree->WidgetFromItem(Node);
		if (!Row.IsValid())
		{
			Tree->RequestScrollIntoView(Node);
			OutAnchor.TargetText = Candidate;
			OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
			return false;
		}

		return BlueprintHelperReviewSurfacePresenterPrivate::BuildGeometryAnchorFromRowWidget(
			Row->AsWidget(),
			OverlayWidget,
			Candidate,
			TEXT("subobject_row"),
			OutAnchor);
	}

	OutAnchor.Reason = bFoundNode ? TEXT("slate_row_geometry_not_ready") : TEXT("no_matching_component_row");
	return false;
}

TSharedRef<SWidget> FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return BlueprintHelperReviewSurfacePresenterPrivate::BuildPanelDiffFrames(
		Args,
		EBlueprintHelperReviewSurface::Components,
		&FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::MyBlueprint);
}

TSharedRef<SWidget> FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FState& State,
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.RootItems.Reset();
	State.TreeView.Reset();
	State.OnGeometryInvalidated = OnGeometryInvalidated;

	UBlueprint* Blueprint = Context.Blueprint.Get();
	if (!Blueprint)
	{
		return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("No Blueprint outline loaded."));
	}

	using FRowItem = FBlueprintHelperReviewMyBlueprintPresenter::FRowItem;
	using ERowKind = FBlueprintHelperReviewMyBlueprintPresenter::ERowKind;

	auto MakeRow = [](const FString& Label, const FString& SearchText, ERowKind Kind)
	{
		TSharedRef<FRowItem> Row = MakeShared<FRowItem>();
		Row->Label = FText::FromString(Label);
		Row->SearchText = SearchText;
		Row->Kind = Kind;
		return Row;
	};

	auto AddSection = [&State, &MakeRow](const TCHAR* Label)
	{
		TSharedRef<FRowItem> Section = MakeRow(Label, FString(), ERowKind::Section);
		State.RootItems.Add(Section);
		return Section;
	};

	auto AddGraphRows = [&MakeRow](
		const TSharedRef<FRowItem>& Section,
		const TArray<TObjectPtr<UEdGraph>>& Graphs,
		ERowKind Kind)
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			const FString GraphName = Graph->GetName();
			Section->Children.Add(MakeRow(GraphName, GraphName, Kind));
		}
	};

	TSharedRef<FRowItem> GraphSection = AddSection(TEXT("Graphs"));
	AddGraphRows(GraphSection, Blueprint->UbergraphPages, ERowKind::Graph);
	AddGraphRows(GraphSection, Blueprint->FunctionGraphs, ERowKind::Function);
	AddGraphRows(GraphSection, Blueprint->MacroGraphs, ERowKind::Macro);
	AddGraphRows(GraphSection, Blueprint->DelegateSignatureGraphs, ERowKind::Dispatcher);

	TSharedRef<FRowItem> VariableSection = AddSection(TEXT("Variables"));
	TSharedRef<FRowItem> DispatcherSection = AddSection(TEXT("Dispatchers"));
	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		const FString VariableName = Variable.VarName.ToString();
		if (VariableName.IsEmpty())
		{
			continue;
		}
		const bool bIsDispatcher = Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate;
		if (bIsDispatcher)
		{
			DispatcherSection->Children.Add(MakeRow(VariableName, VariableName, ERowKind::Dispatcher));
		}
		else
		{
			VariableSection->Children.Add(MakeRow(VariableName, VariableName, ERowKind::Variable));
		}
	}

	TSharedRef<FRowItem> ReviewOnlySection = AddSection(TEXT("Review Anchors"));
	auto RowMatchesTarget = [](const TSharedPtr<FRowItem>& Row, const FString& TargetText, auto&& RowMatchesTargetRef) -> bool
	{
		if (!Row.IsValid())
		{
			return false;
		}
		if (!Row->SearchText.IsEmpty()
			&& BlueprintHelperReviewSurfacePresenterPrivate::GeometrySearchTextMatches(Row->SearchText, TargetText))
		{
			return true;
		}
		for (const TSharedPtr<FRowItem>& Child : Row->Children)
		{
			if (RowMatchesTargetRef(Child, TargetText, RowMatchesTargetRef))
			{
				return true;
			}
		}
		return false;
	};

	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change : ChangeItems)
	{
		if (!Change.IsValid()
			|| Change->AssetPath != Context.AssetPath
			|| !ShouldShowChange(*Change))
		{
			continue;
		}
		const FString TargetText = FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(
			*Change,
			EBlueprintHelperReviewSurface::MyBlueprint);
		if (TargetText.IsEmpty())
		{
			continue;
		}

		bool bHasExistingRow = false;
		for (const TSharedPtr<FRowItem>& Root : State.RootItems)
		{
			if (Root != ReviewOnlySection && RowMatchesTarget(Root, TargetText, RowMatchesTarget))
			{
				bHasExistingRow = true;
				break;
			}
		}
		if (!bHasExistingRow)
		{
			ReviewOnlySection->Children.Add(MakeRow(
				FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(*Change),
				TargetText,
				ERowKind::ReviewOnly));
		}
	}

	State.RootItems.RemoveAll([](const TSharedPtr<FRowItem>& Item)
	{
		return Item.IsValid() && Item->Children.Num() == 0;
	});

	if (State.RootItems.Num() == 0)
	{
		return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("No Blueprint outline loaded."));
	}

	const FString AssetPath = Context.AssetPath;
	TSharedRef<STreeView<TSharedPtr<FRowItem>>> Tree = SAssignNew(State.TreeView, STreeView<TSharedPtr<FRowItem>>)
		.TreeItemsSource(&State.RootItems)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			TSharedPtr<FRowItem> Item,
			const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			const bool bIsSection = Item.IsValid() && Item->Kind == ERowKind::Section;
			const FSlateColor TextColor = bIsSection
				? FSlateColor(FLinearColor(0.84f, 0.84f, 0.84f, 1.0f))
				: FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.0f));
			const FString SearchText = Item.IsValid() ? Item->SearchText : FString();
			TSharedRef<SWidget> RowContent =
				SNew(SBlueprintHelperReviewGeometryProbe)
				.Surface(EBlueprintHelperReviewSurface::MyBlueprint)
				.TargetKey(SearchText)
				.OnGeometryInvalidated(OnGeometryInvalidated)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
						.ColorAndOpacity(TextColor)
					]
				];
			TSharedRef<STableRow<TSharedPtr<FRowItem>>> RowWidget =
				SNew(STableRow<TSharedPtr<FRowItem>>, OwnerTable)
				.Padding(FMargin(2.0f, 1.0f))
				[
					RowContent
				];

			if (Item.IsValid())
			{
				Item->RowWidget = RowContent;
				if (!SearchText.IsEmpty())
				{
					FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
						AssetPath,
						EBlueprintHelperReviewSurface::MyBlueprint,
						SearchText,
						RowContent,
						TEXT("owned_tree_row"));
				}
			}
			return RowWidget;
		})
		.OnGetChildren_Lambda([](TSharedPtr<FRowItem> Item, TArray<TSharedPtr<FRowItem>>& OutChildren)
		{
			if (Item.IsValid())
			{
				OutChildren.Append(Item->Children);
			}
		});

	for (const TSharedPtr<FRowItem>& Root : State.RootItems)
	{
		State.TreeView->SetItemExpansion(Root, true);
	}

	return SNew(SBorder)
		.Padding(6.0f)
		[
			Tree
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return BlueprintHelperReviewSurfacePresenterPrivate::BuildPanelDiffFrames(
		Args,
		EBlueprintHelperReviewSurface::MyBlueprint,
		&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewMyBlueprintPresenter::ResolveRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	FState& State,
	const TSharedPtr<SWidget>& OverlayWidget,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor)
{
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	const FString TargetText = FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(
		Change,
		EBlueprintHelperReviewSurface::MyBlueprint);
	if (TargetText.IsEmpty())
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	const TSharedPtr<FRowItem> Row =
		BlueprintHelperReviewSurfacePresenterPrivate::FindMyBlueprintRowByText(State.RootItems, TargetText);
	if (!Row.IsValid())
	{
		OutAnchor.TargetText = TargetText;
		OutAnchor.Reason = TEXT("no_matching_my_blueprint_row");
		return false;
	}

	TSharedPtr<SWidget> RowWidget = Row->RowWidget.Pin();
	if (!RowWidget.IsValid())
	{
		if (State.TreeView.IsValid())
		{
			State.TreeView->RequestScrollIntoView(Row);
		}
		OutAnchor.TargetText = TargetText;
		OutAnchor.Reason = TEXT("slate_row_geometry_not_ready");
		return false;
	}

	return BlueprintHelperReviewSurfacePresenterPrivate::BuildGeometryAnchorFromRowWidget(
		RowWidget.ToSharedRef(),
		OverlayWidget,
		TargetText,
		TEXT("owned_tree_row"),
		OutAnchor);
}

bool FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::Details);
}

TSharedRef<SWidget> FBlueprintHelperReviewObjectDetailsPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext&,
	TSharedPtr<SKismetInspector>& OutKismetInspector)
{
	TSharedRef<SKismetInspector> Inspector = SAssignNew(OutKismetInspector, SKismetInspector)
		.HideNameArea(true)
		.ViewIdentifier(FName(TEXT("BlueprintHelperReviewInspector")))
		.IsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic(
			&BlueprintHelperReviewSurfacePresenterPrivate::IsReviewPropertyEditingEnabled))
		.ShowLocalVariables(true);
	return Inspector;
}

TSharedRef<SWidget> FBlueprintHelperReviewObjectDetailsPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
		Args,
		EBlueprintHelperReviewSurface::Details,
		&FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange);
}

TSharedRef<SWidget> FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
	EBlueprintHelperReviewSurface Surface,
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&))
{
	return BlueprintHelperReviewSurfacePresenterPrivate::BuildPanelDiffFrames(Args, Surface, Predicate);
}

FLinearColor FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameBackgroundColor(bool bFillBackground)
{
	return BlueprintHelperReviewSurfacePresenterPrivate::GetReviewFrameBackgroundColor(bFillBackground);
}

FLinearColor FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameFillColor(
	const FLinearColor& FrameColor,
	bool bFillBackground,
	bool bSelected)
{
	return BlueprintHelperReviewSurfacePresenterPrivate::GetReviewFrameFillColor(
		FrameColor,
		bFillBackground,
		bSelected);
}

TSharedRef<SWidget> FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	const TSharedRef<SWidget>& Content,
	bool bShowActions,
	bool bFillBackground,
	const FSlateColor& FrameColor,
	const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnAcceptChange,
	const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnRejectChange,
	bool bSelected)
{
	return SNew(BlueprintHelperReviewSurfacePresenterPrivate::SBlueprintHelperReviewDiffFrame)
		.FrameColor(FrameColor)
		.ShowActions(bShowActions && Item.IsValid())
		.FillBackground(bFillBackground)
		.Selected(bSelected)
		.OnAccept(FOnClicked::CreateLambda([Item, OnAcceptChange]()
		{
			return OnAcceptChange ? OnAcceptChange(Item) : FReply::Handled();
		}))
		.OnReject(FOnClicked::CreateLambda([Item, OnRejectChange]()
		{
			return OnRejectChange ? OnRejectChange(Item) : FReply::Handled();
		}))
		[
			Content
		];
}
