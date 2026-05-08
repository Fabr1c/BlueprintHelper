// Transient graph-space Review diff block node.

#include "UI/Review/BlueprintHelperReviewDiffBlockNode.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "SGraphNode.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

namespace
{
	class SBlueprintHelperReviewDiffBlockGraphNode : public SGraphNode
	{
	public:
		SLATE_BEGIN_ARGS(SBlueprintHelperReviewDiffBlockGraphNode) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UBlueprintHelperReviewDiffBlockNode* InNode)
		{
			GraphNode = InNode;
			DiffNode = InNode;
			UpdateGraphNode();
		}

		virtual void UpdateGraphNode() override
		{
			InputPins.Empty();
			OutputPins.Empty();
			RightNodeBox.Reset();
			LeftNodeBox.Reset();
			ContentScale.Bind(this, &SGraphNode::GetContentScale);

			UBlueprintHelperReviewDiffBlockNode* Node = DiffNode.Get();
			const float Width = Node ? FMath::Max(80.0f, static_cast<float>(Node->NodeWidth)) : 80.0f;
			const float Height = Node ? FMath::Max(40.0f, static_cast<float>(Node->NodeHeight)) : 40.0f;

			GetOrAddSlot(ENodeZone::Center)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBox)
				.WidthOverride(Width)
				.HeightOverride(Height)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(this, &SBlueprintHelperReviewDiffBlockGraphNode::GetBlockBrush)
						.Padding(0.0f)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(0.0f, 0.0f, 10.0f, 10.0f)
					[
						SNew(SBorder)
						.BorderImage(&ActionBrush)
						.Padding(5.0f)
						.Visibility(this, &SBlueprintHelperReviewDiffBlockGraphNode::GetActionVisibility)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SButton)
								.Text(FText::FromString(TEXT("Accept")))
								.OnClicked(this, &SBlueprintHelperReviewDiffBlockGraphNode::OnAcceptClicked)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
								.Text(FText::FromString(TEXT("Reject")))
								.OnClicked(this, &SBlueprintHelperReviewDiffBlockGraphNode::OnRejectClicked)
							]
						]
					]
				]
			];
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			if (const UBlueprintHelperReviewDiffBlockNode* Node = DiffNode.Get())
			{
				return FVector2D(
					FMath::Max(80.0f, static_cast<float>(Node->NodeWidth)),
					FMath::Max(40.0f, static_cast<float>(Node->NodeHeight)));
			}
			return FVector2D(80.0f, 40.0f);
		}

		virtual int32 GetSortDepth() const override
		{
			return -10000;
		}

		virtual bool CanBeSelected(const FVector2D& MousePositionInNode) const override
		{
			return false;
		}

		virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override
		{
		}

	private:
		const FSlateBrush* GetBlockBrush() const
		{
			FLinearColor Color = FLinearColor::Transparent;
			if (const UBlueprintHelperReviewDiffBlockNode* Node = DiffNode.Get())
			{
				Color = Node->DiffColor;
				Color.A = Node->bHighlighted ? 0.72f : 0.46f;
			}

			BlockBrush = FSlateRoundedBoxBrush(
				Color,
				0.0f,
				FLinearColor(Color.R, Color.G, Color.B, 0.95f),
				3.0f);
			return &BlockBrush;
		}

		EVisibility GetActionVisibility() const
		{
			return IsHovered() ? EVisibility::Visible : EVisibility::Collapsed;
		}

		FReply OnAcceptClicked() const
		{
			if (const UBlueprintHelperReviewDiffBlockNode* Node = DiffNode.Get())
			{
				if (Node->OnAccept)
				{
					return Node->OnAccept(Node->ChangeId);
				}
			}
			return FReply::Handled();
		}

		FReply OnRejectClicked() const
		{
			if (const UBlueprintHelperReviewDiffBlockNode* Node = DiffNode.Get())
			{
				if (Node->OnReject)
				{
					return Node->OnReject(Node->ChangeId);
				}
			}
			return FReply::Handled();
		}

		TWeakObjectPtr<UBlueprintHelperReviewDiffBlockNode> DiffNode;
		mutable FSlateRoundedBoxBrush BlockBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
		FSlateRoundedBoxBrush ActionBrush = FSlateRoundedBoxBrush(
			FLinearColor(0.02f, 0.02f, 0.02f, 0.95f),
			5.0f);
	};
}

void UBlueprintHelperReviewDiffBlockNode::Configure(
	const FString& InChangeId,
	const FString& InDisplayLabel,
	const FLinearColor& InDiffColor,
	bool bInHighlighted,
	TFunction<FReply(const FString&)> InOnAccept,
	TFunction<FReply(const FString&)> InOnReject)
{
	ChangeId = InChangeId;
	DisplayLabel = InDisplayLabel;
	DiffColor = InDiffColor;
	bHighlighted = bInHighlighted;
	OnAccept = MoveTemp(InOnAccept);
	OnReject = MoveTemp(InOnReject);

	NodeComment.Reset();
	CommentColor = DiffColor;
	CommentColor.A = 0.46f;
	FontSize = 1;
	MoveMode = ECommentBoxMode::NoGroupMovement;
	CommentDepth = -10000;
	bCanRenameNode = false;
	bCanResizeNode = false;
	bCommentBubbleVisible_InDetailsPanel = false;
}

FText UBlueprintHelperReviewDiffBlockNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::GetEmpty();
}

FText UBlueprintHelperReviewDiffBlockNode::GetTooltipText() const
{
	return FText::FromString(DisplayLabel);
}

TSharedPtr<SGraphNode> UBlueprintHelperReviewDiffBlockNode::CreateVisualWidget()
{
	return SNew(SBlueprintHelperReviewDiffBlockGraphNode, this);
}
