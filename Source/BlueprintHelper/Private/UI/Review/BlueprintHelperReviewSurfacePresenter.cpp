// BlueprintHelper Review surface presenter routing helpers.

#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "GraphEditor.h"
#include "SKismetInspector.h"
#include "SMyBlueprint.h"
#include "SSubobjectBlueprintEditor.h"
#include "UI/Review/BlueprintHelperReviewAssetPresenters.h"
#include "UI/Review/BlueprintHelperReviewDiffBlockNode.h"
#include "UI/Review/BlueprintHelperReviewGraphBounds.h"
#include "UI/Review/BlueprintHelperReviewGraphResolver.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
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
#include "UObject/UObjectGlobals.h"

class FBlueprintEditor;

namespace BlueprintHelperReviewSurfacePresenterPrivate
{
	static const FLinearColor ReviewFrameInnerBg = FLinearColor(0.06f, 0.06f, 0.06f, 0.92f);

	struct FSlateRowGeometryRecord
	{
		FString AssetPath;
		EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
		FString SearchText;
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
		{
		}

			SLATE_ATTRIBUTE(FSlateColor, FrameColor)
			SLATE_ATTRIBUTE(bool, ShowActions)
			SLATE_ARGUMENT(bool, FillBackground)
			SLATE_EVENT(FOnClicked, OnAccept)
			SLATE_EVENT(FOnClicked, OnReject)
			SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			FrameColor = InArgs._FrameColor;
			ShowActions = InArgs._ShowActions;
			bFillBackground = InArgs._FillBackground;
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
				bFillBackground ? ReviewFrameInnerBg : FLinearColor::Transparent,
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

	static TSharedRef<SWidget> BuildPanelDiffFrame(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		const TSharedRef<SWidget>& Content)
	{
		const FSlateColor FrameColor = IsSameChange(Item, Args.SelectedChange) && Item.IsValid() && Args.GetSelectedDiffColor
			? Args.GetSelectedDiffColor()
			: (Item.IsValid() && Args.GetChangeColor ? Args.GetChangeColor(Item->ChangeKind) : FSlateColor(FLinearColor::Transparent));

		return FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
			Item,
			Content,
			Item.IsValid(),
			true,
			FrameColor,
			Args.OnAcceptChange,
			Args.OnRejectChange);
	}

	static TSharedRef<SWidget> BuildSlateRowGeometryFrame(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
	{
		const FSlateColor FrameColor = IsSameChange(Item, Args.SelectedChange) && Item.IsValid() && Args.GetSelectedDiffColor
			? Args.GetSelectedDiffColor()
			: (Item.IsValid() && Args.GetChangeColor ? Args.GetChangeColor(Item->ChangeKind) : FSlateColor(FLinearColor::Transparent));

		return FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
			Item,
			SNullWidget::NullWidget,
			Item.IsValid(),
			false,
			FrameColor,
			Args.OnAcceptChange,
			Args.OnRejectChange);
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

		OutAnchor = CandidateAnchor;
		return true;
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
			VisibleFrames.Add(Frame);
		}

		if (VisibleFrames.Num() == 0)
		{
			return SNullWidget::NullWidget;
		}

		bool bAllFramesHaveStableGeometry = true;
		for (const FReviewPanelVisibleFrame& Frame : VisibleFrames)
		{
			bAllFramesHaveStableGeometry &= Frame.bHasStableGeometry;
		}

		if (bAllFramesHaveStableGeometry)
		{
			TSharedRef<SCanvas> GeometryCanvas = SNew(SCanvas);
			for (const FReviewPanelVisibleFrame& Frame : VisibleFrames)
			{
				if (Args.AddDebugMessage && Frame.Item.IsValid())
				{
					Args.AddDebugMessage(FString::Printf(
						TEXT("ReviewFrameGeometry change=%s surface=%s mode=slate_row result=shown reason=%s pos=(%.1f,%.1f) size=(%.1f,%.1f) target=\"%s\""),
						*Frame.Item->ChangeId,
						BlueprintHelperReviewSurfaceToString(Surface),
						*Frame.GeometryAnchor.Reason,
						static_cast<double>(Frame.GeometryAnchor.Position.X),
						static_cast<double>(Frame.GeometryAnchor.Position.Y),
						static_cast<double>(Frame.GeometryAnchor.Size.X),
						static_cast<double>(Frame.GeometryAnchor.Size.Y),
						*Frame.GeometryAnchor.TargetText));
				}

				GeometryCanvas->AddSlot()
				.Position(Frame.GeometryAnchor.Position)
				.Size(Frame.GeometryAnchor.Size)
				[
					BuildSlateRowGeometryFrame(Frame.Item, Args)
				];
			}

			return GeometryCanvas;
		}

		TSharedRef<SScrollBox> ReviewList = SNew(SScrollBox);
		int32 VisibleFrameCount = 0;
		bool bAnyFrameHasStableGeometry = false;
		for (const FReviewPanelVisibleFrame& Frame : VisibleFrames)
		{
			bAnyFrameHasStableGeometry |= Frame.bHasStableGeometry;
		}
		const FString ReviewListReason = bAnyFrameHasStableGeometry
			? TEXT("partial_slate_row_geometry")
			: TEXT("no_stable_slate_geometry");
		for (const FReviewPanelVisibleFrame& Frame : VisibleFrames)
		{
			if (Args.AddDebugMessage && Frame.Item.IsValid())
			{
				Args.AddDebugMessage(FString::Printf(
					TEXT("ReviewFrameGeometry change=%s surface=%s mode=review_list result=shown reason=%s row=%d target=\"%s\""),
					*Frame.Item->ChangeId,
					BlueprintHelperReviewSurfaceToString(Surface),
					*ReviewListReason,
					VisibleFrameCount,
					*Frame.TargetText));
			}

			const FString KindLabel = Frame.Item.IsValid()
				? FString(BlueprintHelperReviewChangeKindToString(Frame.Item->ChangeKind))
				: FString(TEXT("unknown"));
			TSharedRef<SWidget> CardContent = SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(FText::FromString(Frame.Item->DisplayLabel.IsEmpty() ? Frame.Item->ChangeId : Frame.Item->DisplayLabel))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.62f, 0.62f, 1.0f)))
					.AutoWrapText(true)
					.Text(FText::FromString(FString::Printf(
						TEXT("%s  %s"),
						*KindLabel,
						*Frame.TargetText)))
				];

			ReviewList->AddSlot()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				BuildPanelDiffFrame(Frame.Item, Args, CardContent)
			];

			++VisibleFrameCount;
		}

		return ReviewList;
	}
}

FBlueprintHelperReviewSurfaceRouteDecision FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	FBlueprintHelperReviewSurfaceRouteDecision Decision;
	Decision.bHasExplicitTargets = BlueprintHelperReviewHasExplicitTargets(Change);
	Decision.ExplicitTargetCount = Change.AtomicTargets.Num();
	Decision.MatchingTargetCount = BlueprintHelperReviewCountSurfaceTargets(Change, Surface);

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

FString FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	return BlueprintHelperReviewSurfacePresenterPrivate::GetReviewListTargetText(
		MakeShared<FBlueprintHelperReviewVisibleChange>(Change),
		Surface);
}

void FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
	const FString& AssetPath,
	EBlueprintHelperReviewSurface Surface,
	const FString& SearchText,
	const TSharedRef<SWidget>& RowWidget)
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
		OutAnchor.TargetText = Record.SearchText;
		OutAnchor.Reason = TEXT("stable_slate_row_geometry");
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
	const FBlueprintHelperReviewAssetContext& Context)
{
	if (UBlueprint* Blueprint = Context.Blueprint.Get())
	{
		if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			if (AActor* ActorCDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
			{
				return SNew(SSubobjectBlueprintEditor)
					.ObjectContext(ActorCDO)
					.AllowEditing(false)
					.HideComponentClassCombo(true);
			}
		}
	}

	return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("No Blueprint component tree loaded."));
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
	TSharedPtr<SMyBlueprint>& OutMyBlueprintWidget,
	const TSharedPtr<SKismetInspector>& KismetInspector)
{
	if (UBlueprint* Blueprint = Context.Blueprint.Get())
	{
		TSharedRef<SMyBlueprint> Widget = SAssignNew(OutMyBlueprintWidget, SMyBlueprint, TWeakPtr<FBlueprintEditor>(), Blueprint);
		if (KismetInspector.IsValid())
		{
			OutMyBlueprintWidget->SetInspector(KismetInspector);
		}
		return Widget;
	}

	OutMyBlueprintWidget.Reset();
	return BlueprintHelperReviewSurfacePresenterPrivate::BuildReviewPlaceholder(TEXT("No Blueprint outline loaded."));
}

TSharedRef<SWidget> FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return BlueprintHelperReviewSurfacePresenterPrivate::BuildPanelDiffFrames(
		Args,
		EBlueprintHelperReviewSurface::MyBlueprint,
		&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange);
}

bool FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewSurfacePresenterRouter::ShouldShowChangeOnSurface(
		Change,
		EBlueprintHelperReviewSurface::Details);
}

TSharedRef<SWidget> FBlueprintHelperReviewObjectDetailsPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext&,
	TSharedPtr<SKismetInspector>& OutKismetInspector,
	const TSharedPtr<SMyBlueprint>& MyBlueprintWidget)
{
	TSharedRef<SKismetInspector> Inspector = SAssignNew(OutKismetInspector, SKismetInspector)
		.HideNameArea(true)
		.ViewIdentifier(FName(TEXT("BlueprintHelperReviewInspector")))
		.MyBlueprintWidget(MyBlueprintWidget)
		.IsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic(
			&BlueprintHelperReviewSurfacePresenterPrivate::IsReviewPropertyEditingEnabled))
		.ShowLocalVariables(true);
	if (MyBlueprintWidget.IsValid())
	{
		MyBlueprintWidget->SetInspector(OutKismetInspector);
	}
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

TSharedRef<SWidget> FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	const TSharedRef<SWidget>& Content,
	bool bShowActions,
	bool bFillBackground,
	const FSlateColor& FrameColor,
	const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnAcceptChange,
	const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnRejectChange)
{
	return SNew(BlueprintHelperReviewSurfacePresenterPrivate::SBlueprintHelperReviewDiffFrame)
		.FrameColor(FrameColor)
		.ShowActions(bShowActions && Item.IsValid())
		.FillBackground(bFillBackground)
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
