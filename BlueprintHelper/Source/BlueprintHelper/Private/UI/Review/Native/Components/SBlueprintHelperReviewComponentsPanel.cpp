// BlueprintHelper Review native Components panel.

#include "UI/Review/Native/Components/SBlueprintHelperReviewComponentsPanel.h"

#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Styling/AppStyle.h"
#include "UI/Review/Native/Components/SBlueprintHelperReviewComponentRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

static FString BlueprintHelperReviewCleanComponentDisplayName(FString Name)
{
	Name.TrimStartAndEndInline();
	int32 DelimiterIndex = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), DelimiterIndex))
	{
		Name = Name.Mid(DelimiterIndex + 1);
	}
	if (Name.EndsWith(TEXT("_GEN_VARIABLE")))
	{
		Name.LeftChopInline(13);
	}
	Name.TrimStartAndEndInline();
	return Name;
}

void SBlueprintHelperReviewComponentsPanel::Construct(const FArguments& InArgs)
{
	AssetPath = InArgs._AssetPath;
	OnGeometryInvalidated = InArgs._OnGeometryInvalidated;
	RebuildItems(InArgs._Blueprint);

	TSharedPtr<SWidget> ContentWidget;
	if (RootItems.Num() > 0)
	{
		ContentWidget = SAssignNew(TreeView, STreeView<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>)
			.TreeItemsSource(&RootItems)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SBlueprintHelperReviewComponentsPanel::OnGenerateRow)
			.OnGetChildren(this, &SBlueprintHelperReviewComponentsPanel::OnGetChildren)
			.ItemHeight(25.0f);
	}
	else
	{
		ContentWidget = SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.58f, 0.58f, 1.0f)))
			.AutoWrapText(true)
			.Text(FText::FromString(TEXT("No Blueprint component tree loaded.")));
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(4.0f)
		[
			ContentWidget.ToSharedRef()
		]
	];

	if (TreeView.IsValid())
	{
		for (const TSharedPtr<FBlueprintHelperReviewComponentRowItem>& RootItem : RootItems)
		{
			TreeView->SetItemExpansion(RootItem, true);
		}
	}
}

void SBlueprintHelperReviewComponentsPanel::RebuildItems(UBlueprint* Blueprint)
{
	RootItems.Reset();
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return;
	}

	const TArray<USCS_Node*>& RootNodes = Blueprint->SimpleConstructionScript->GetRootNodes();
	for (USCS_Node* RootNode : RootNodes)
	{
		if (TSharedPtr<FBlueprintHelperReviewComponentRowItem> Item = BuildItemFromSCSNode(RootNode, 0))
		{
			RootItems.Add(Item);
		}
	}

	if (RootItems.Num() == 0)
	{
		if (USCS_Node* DefaultSceneRoot = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode())
		{
			if (TSharedPtr<FBlueprintHelperReviewComponentRowItem> Item = BuildItemFromSCSNode(DefaultSceneRoot, 0))
			{
				RootItems.Add(Item);
			}
		}
	}
}

TSharedPtr<FBlueprintHelperReviewComponentRowItem> SBlueprintHelperReviewComponentsPanel::BuildItemFromSCSNode(
	USCS_Node* Node,
	int32 Depth) const
{
	if (!Node)
	{
		return nullptr;
	}

	TSharedRef<FBlueprintHelperReviewComponentRowItem> Item = MakeShared<FBlueprintHelperReviewComponentRowItem>();
	Item->Depth = Depth;
	Item->ComponentName = Node->GetVariableName().ToString();
	Item->DisplayName = BlueprintHelperReviewCleanComponentDisplayName(Item->ComponentName);

	if (UActorComponent* Template = Node->ComponentTemplate)
	{
		if (UClass* ComponentClass = Template->GetClass())
		{
			Item->ComponentClassObject = ComponentClass;
			Item->ComponentClass = ComponentClass->GetName();
		}
	}
	if (Item->DisplayName.IsEmpty())
	{
		Item->DisplayName = BlueprintHelperReviewCleanComponentDisplayName(
			Item->ComponentName.IsEmpty() && Node->ComponentTemplate
				? Node->ComponentTemplate->GetReadableName()
				: Item->ComponentName);
	}
	Item->ToolTipText = FText::FromString(FString::Printf(
		TEXT("%s\n%s"),
		*Item->DisplayName,
		*Item->ComponentClass));

	for (USCS_Node* ChildNode : Node->GetChildNodes())
	{
		if (TSharedPtr<FBlueprintHelperReviewComponentRowItem> Child = BuildItemFromSCSNode(ChildNode, Depth + 1))
		{
			Item->Children.Add(Child);
		}
	}
	return Item;
}

TSharedPtr<FBlueprintHelperReviewComponentRowItem> SBlueprintHelperReviewComponentsPanel::FindRowByCandidates(
	const TArray<FString>& Candidates) const
{
	return FindRowByCandidatesRecursive(RootItems, Candidates);
}

TSharedPtr<SWidget> SBlueprintHelperReviewComponentsPanel::GetRowWidgetForItem(
	const TSharedPtr<FBlueprintHelperReviewComponentRowItem>& Item) const
{
	return Item.IsValid() ? Item->RowWidget.Pin() : nullptr;
}

void SBlueprintHelperReviewComponentsPanel::RequestScrollIntoView(
	const TSharedPtr<FBlueprintHelperReviewComponentRowItem>& Item) const
{
	if (TreeView.IsValid() && Item.IsValid())
	{
		TreeView->RequestScrollIntoView(Item);
	}
}

TSharedPtr<FBlueprintHelperReviewComponentRowItem> SBlueprintHelperReviewComponentsPanel::FindRowByCandidatesRecursive(
	const TArray<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>& Items,
	const TArray<FString>& Candidates) const
{
	for (const TSharedPtr<FBlueprintHelperReviewComponentRowItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		if (SearchTextMatchesAnyCandidate(Item->ComponentName, Candidates)
			|| SearchTextMatchesAnyCandidate(Item->DisplayName, Candidates)
			|| SearchTextMatchesAnyCandidate(Item->ComponentClass, Candidates))
		{
			return Item;
		}

		if (TSharedPtr<FBlueprintHelperReviewComponentRowItem> Child = FindRowByCandidatesRecursive(Item->Children, Candidates))
		{
			return Child;
		}
	}
	return nullptr;
}

bool SBlueprintHelperReviewComponentsPanel::SearchTextMatchesAnyCandidate(
	const FString& SearchText,
	const TArray<FString>& Candidates)
{
	const FString NormalizedSearchText = NormalizeGeometrySearchText(SearchText);
	if (NormalizedSearchText.IsEmpty())
	{
		return false;
	}

	for (const FString& Candidate : Candidates)
	{
		TArray<FString> TargetTerms;
		AddGeometrySearchTerms(Candidate, TargetTerms);
		for (const FString& TargetTerm : TargetTerms)
		{
			if (TargetTerm.Len() < 2)
			{
				continue;
			}
			if (NormalizedSearchText.Contains(TargetTerm) || TargetTerm.Contains(NormalizedSearchText))
			{
				return true;
			}
		}
	}
	return false;
}

FString SBlueprintHelperReviewComponentsPanel::NormalizeGeometrySearchText(FString Text)
{
	Text.TrimStartAndEndInline();
	Text = Text.ToLower();
	Text.ReplaceInline(TEXT(" "), TEXT(""));
	Text.ReplaceInline(TEXT("_"), TEXT(""));
	Text.ReplaceInline(TEXT("-"), TEXT(""));
	Text.ReplaceInline(TEXT("["), TEXT(""));
	Text.ReplaceInline(TEXT("]"), TEXT(""));
	return Text;
}

void SBlueprintHelperReviewComponentsPanel::AddGeometrySearchTerms(const FString& RawText, TArray<FString>& OutTerms)
{
	FString Text = RawText;
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty())
	{
		return;
	}

	OutTerms.AddUnique(NormalizeGeometrySearchText(Text));

	int32 DelimiterIndex = INDEX_NONE;
	if (Text.FindLastChar(TEXT('/'), DelimiterIndex)
		|| Text.FindLastChar(TEXT(':'), DelimiterIndex)
		|| Text.FindLastChar(TEXT('.'), DelimiterIndex))
	{
		OutTerms.AddUnique(NormalizeGeometrySearchText(Text.Mid(DelimiterIndex + 1)));
	}
}

TSharedRef<ITableRow> SBlueprintHelperReviewComponentsPanel::OnGenerateRow(
	TSharedPtr<FBlueprintHelperReviewComponentRowItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBlueprintHelperReviewComponentRow, OwnerTable)
		.Item(Item)
		.AssetPath(AssetPath)
		.OnGeometryInvalidated(OnGeometryInvalidated);
}

void SBlueprintHelperReviewComponentsPanel::OnGetChildren(
	TSharedPtr<FBlueprintHelperReviewComponentRowItem> Item,
	TArray<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>& OutChildren) const
{
	if (Item.IsValid())
	{
		OutChildren.Append(Item->Children);
	}
}
