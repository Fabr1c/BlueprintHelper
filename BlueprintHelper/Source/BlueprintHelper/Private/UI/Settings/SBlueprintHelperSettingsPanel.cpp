// BlueprintHelper settings panel implementation.

#include "UI/Settings/SBlueprintHelperSettingsPanel.h"

#include "UI/Settings/BlueprintHelperSettingsPresenter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperSettingsPanel::Construct(const FArguments& InArgs)
{
	Presenter = MakeShared<FBlueprintHelperSettingsPresenter>();
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
					.Text(FText::FromString(TEXT("Reload")))
					.OnClicked(this, &SBlueprintHelperSettingsPanel::OnReloadClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Create Project setting.json")))
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
				SAssignNew(SettingJsonTextBox, SMultiLineEditableTextBox)
				.IsReadOnly(true)
			]
		]
	];

	RefreshView();
}

FReply SBlueprintHelperSettingsPanel::OnReloadClicked()
{
	if (Presenter.IsValid())
	{
		Presenter->Reload();
		RefreshView();
	}
	return FReply::Handled();
}

FReply SBlueprintHelperSettingsPanel::OnEnsureProjectSettingClicked()
{
	if (Presenter.IsValid())
	{
		Presenter->EnsureProjectSetting();
		RefreshView();
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
	if (SettingJsonTextBox.IsValid())
	{
		SettingJsonTextBox->SetText(FText::FromString(View.EffectiveJson));
	}
}
