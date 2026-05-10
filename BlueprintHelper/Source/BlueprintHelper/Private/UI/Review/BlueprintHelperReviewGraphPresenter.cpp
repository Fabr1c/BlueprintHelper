// BlueprintHelper Review graph presenter.

#include "UI/Review/BlueprintHelperReviewGraphPresenter.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "GraphEditor.h"
#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewDiffBlockNode.h"
#include "UI/Review/BlueprintHelperReviewGraphBounds.h"
#include "UI/Review/BlueprintHelperReviewGraphResolver.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UObjectGlobals.h"

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
		return BuildReviewPlaceholder(TEXT("No review change selected."));
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
		return BuildReviewPlaceholder(TEXT("Selected review change has no Graph anchor."));
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
		return BuildReviewPlaceholder(TEXT("No Blueprint graph loaded for this review asset."));
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

	State.PreviewBlueprint = TStrongObjectPtr<UBlueprint>(CreateReviewPreviewBlueprint(SourceBlueprint));

	if (SourceGraph && State.PreviewBlueprint.IsValid())
	{
		State.PreviewGraph = TStrongObjectPtr<UEdGraph>(FEdGraphUtilities::CloneGraph(SourceGraph, State.PreviewBlueprint.Get()));
		if (State.PreviewGraph.IsValid())
		{
			State.PreviewGraph->SetFlags(RF_Transient);
			State.PreviewGraph->bEditable = false;
			AttachPreviewGraphToMatchingBlueprintList(
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
			State.PreviewBlueprint = TStrongObjectPtr<UBlueprint>(CreateReviewPreviewBlueprint(SourceBlueprint));
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

	AddGraphDiffBlocks(
		State.PreviewGraph.Get(),
		SourceGraph,
		Editor,
		Args);
	Editor->NotifyGraphChanged();

	JumpToSelectedGraphDiffBlock(
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

TSharedRef<SWidget> FBlueprintHelperReviewGraphPresenter::BuildReviewPlaceholder(const FString& Message)
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

UBlueprint* FBlueprintHelperReviewGraphPresenter::CreateReviewPreviewBlueprint(const UBlueprint* SourceBlueprint)
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

void FBlueprintHelperReviewGraphPresenter::AttachPreviewGraphToMatchingBlueprintList(
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

bool FBlueprintHelperReviewGraphPresenter::IsSameChange(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Left,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Right)
{
	return Left.IsValid() && Right.IsValid() && Left->ChangeId == Right->ChangeId;
}

bool FBlueprintHelperReviewGraphPresenter::BuildGraphBoundsForChange(
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

void FBlueprintHelperReviewGraphPresenter::AddGraphDiffBlocks(
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

void FBlueprintHelperReviewGraphPresenter::JumpToSelectedGraphDiffBlock(
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
