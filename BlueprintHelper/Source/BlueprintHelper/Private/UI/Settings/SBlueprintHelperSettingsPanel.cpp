// BlueprintHelper settings panel implementation.

#include "UI/Settings/SBlueprintHelperSettingsPanel.h"

#include "UI/Settings/BlueprintHelperSettingsPresenter.h"
#include "UI/Settings/SBlueprintHelperSettingRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BlueprintHelperSettingsPanel"

void SBlueprintHelperSettingsPanel::Construct(const FArguments& InArgs)
{
	Presenter = MakeShared<FBlueprintHelperSettingsPresenter>();
	Presenter->OnRowsChanged().AddSP(this, &SBlueprintHelperSettingsPanel::RefreshRows);
	Presenter->Reload();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ReloadButton", "重新载入"))
					.ToolTipText(LOCTEXT("ReloadButtonHint", "重新读取默认设置、项目设置和用户覆盖设置。"))
					.OnClicked(this, &SBlueprintHelperSettingsPanel::OnReloadClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateProjectSettingButton", "创建项目 setting.json"))
					.ToolTipText(LOCTEXT("CreateProjectSettingHint", "如果项目设置文件不存在，则从默认设置创建一份。"))
					.OnClicked(this, &SBlueprintHelperSettingsPanel::OnEnsureProjectSettingClicked)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SAssignNew(StatusTextBlock, STextBlock)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(DefaultPathTextBlock, STextBlock)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ProjectPathTextBlock, STextBlock)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(UserPathTextBlock, STextBlock)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(EffectiveSourceTextBlock, STextBlock)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(CategoriesBox, SVerticalBox)
				]
			]
		]
	];

	RefreshView();
	RefreshRows();
}

FReply SBlueprintHelperSettingsPanel::OnReloadClicked()
{
	if (Presenter.IsValid())
	{
		Presenter->Reload();
		RefreshView();
		RefreshRows();
	}
	return FReply::Handled();
}

FReply SBlueprintHelperSettingsPanel::OnEnsureProjectSettingClicked()
{
	if (Presenter.IsValid())
	{
		Presenter->EnsureProjectSetting();
		RefreshView();
		RefreshRows();
	}
	return FReply::Handled();
}

void SBlueprintHelperSettingsPanel::RefreshView()
{
	if (!Presenter.IsValid())
	{
		return;
	}

	const FBlueprintHelperSettingView& View = Presenter->GetView();
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::FromString(View.StatusText));
	}
	if (DefaultPathTextBlock.IsValid())
	{
		DefaultPathTextBlock->SetText(FText::FromString(
			FString::Printf(TEXT("Default: %s"), *View.DefaultSettingPath)));
	}
	if (ProjectPathTextBlock.IsValid())
	{
		ProjectPathTextBlock->SetText(FText::FromString(
			FString::Printf(TEXT("Project: %s%s"),
				*View.ProjectSettingPath,
				View.bProjectSettingExists ? TEXT(" [exists]") : TEXT(" [missing]"))));
	}
	if (UserPathTextBlock.IsValid())
	{
		UserPathTextBlock->SetText(FText::FromString(
			FString::Printf(TEXT("User override: %s%s"),
				*View.UserSettingOverridePath,
				View.bUserOverrideExists ? TEXT(" [exists]") : TEXT(" [missing]"))));
	}
	if (EffectiveSourceTextBlock.IsValid())
	{
		EffectiveSourceTextBlock->SetText(FText::FromString(
			FString::Printf(TEXT("Effective source: %s"), *View.EffectiveSourcePath)));
	}
}

void SBlueprintHelperSettingsPanel::RefreshRows()
{
	if (!CategoriesBox.IsValid() || !Presenter.IsValid())
	{
		return;
	}

	CategoriesBox->ClearChildren();

	const TArray<FBlueprintHelperSettingRowViewModel>& Rows = Presenter->GetRows();
	FText CurrentCategory;
	TArray<FBlueprintHelperSettingRowViewModel> CurrentRows;
	auto FlushCategory = [this, &CurrentCategory, &CurrentRows]()
	{
		if (CurrentRows.Num() == 0)
		{
			return;
		}

		CategoriesBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			BuildCategorySection(CurrentCategory, CurrentRows)
		];
		CurrentRows.Reset();
	};

	for (const FBlueprintHelperSettingRowViewModel& Row : Rows)
	{
		if (CurrentRows.Num() > 0 && !CurrentCategory.EqualTo(Row.CategoryLabel))
		{
			FlushCategory();
		}
		CurrentCategory = Row.CategoryLabel;
		CurrentRows.Add(Row);
	}
	FlushCategory();
}

TSharedRef<SWidget> SBlueprintHelperSettingsPanel::BuildCategorySection(const FText& CategoryLabel, const TArray<FBlueprintHelperSettingRowViewModel>& Rows)
{
	TSharedRef<SVerticalBox> RowBox = SNew(SVerticalBox);
	for (const FBlueprintHelperSettingRowViewModel& Row : Rows)
	{
		RowBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SBlueprintHelperSettingRow)
			.Row(Row)
			.OnValueCommitted_Lambda([this](const FBlueprintHelperSettingEditEvent& Event)
			{
				if (Presenter.IsValid())
				{
					Presenter->HandleSettingValueCommitted(Event);
					RefreshView();
				}
			})
			.OnResetRequested_Lambda([this](const FString& DotPath)
			{
				if (Presenter.IsValid())
				{
					Presenter->HandleSettingResetRequested(DotPath);
					RefreshView();
				}
			})
		];
	}

	return SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderContent()
		[
			SNew(STextBlock)
			.Text(CategoryLabel)
		]
		.BodyContent()
		[
			SNew(SBorder)
			.Padding(6.0f)
			[
				RowBox
			]
		];
}

#undef LOCTEXT_NAMESPACE
