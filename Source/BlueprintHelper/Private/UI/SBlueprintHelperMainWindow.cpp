// BlueprintHelper main window shell implementation.

#include "UI/SBlueprintHelperMainWindow.h"

#include "UI/SHelperMainWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperMainWindow::Construct(const FArguments& InArgs)
{
	ImportService = InArgs._ImportService;
	GraphResolver = InArgs._GraphResolver;
	ReviewStoreService = InArgs._ReviewStoreService;
	ReviewActionService = InArgs._ReviewActionService;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetToolsTabColor)
				.Text(FText::FromString(TEXT("Tools")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowToolsPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetReviewTabColor)
				.Text(FText::FromString(TEXT("Review")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowReviewPage)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(PageSwitcher, SWidgetSwitcher)
			.WidgetIndex(ActivePageIndex)
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHelperMainWidget)
				.ImportService(ImportService)
				.GraphResolver(GraphResolver)
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBlueprintHelperReviewPanel)
				.ReviewStoreService(ReviewStoreService)
				.ReviewActionService(ReviewActionService)
			]
		]
	];
}

FReply SBlueprintHelperMainWindow::ShowToolsPage()
{
	ActivePageIndex = 0;
	if (PageSwitcher.IsValid())
	{
		PageSwitcher->SetActiveWidgetIndex(ActivePageIndex);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperMainWindow::ShowReviewPage()
{
	ActivePageIndex = 1;
	if (PageSwitcher.IsValid())
	{
		PageSwitcher->SetActiveWidgetIndex(ActivePageIndex);
	}
	return FReply::Handled();
}

FSlateColor SBlueprintHelperMainWindow::GetToolsTabColor() const
{
	return FSlateColor(ActivePageIndex == 0
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}

FSlateColor SBlueprintHelperMainWindow::GetReviewTabColor() const
{
	return FSlateColor(ActivePageIndex == 1
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}
