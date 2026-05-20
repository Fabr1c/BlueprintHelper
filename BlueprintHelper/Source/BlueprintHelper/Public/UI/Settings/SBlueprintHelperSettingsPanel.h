// BlueprintHelper settings panel.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperSettingsPresenter;
class SMultiLineEditableTextBox;
class STextBlock;

class SBlueprintHelperSettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperSettingsPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnReloadClicked();
	FReply OnEnsureProjectSettingClicked();
	void RefreshView();

	TSharedPtr<FBlueprintHelperSettingsPresenter> Presenter;
	TSharedPtr<STextBlock> StatusTextBlock;
	TSharedPtr<STextBlock> DefaultPathTextBlock;
	TSharedPtr<STextBlock> ProjectPathTextBlock;
	TSharedPtr<STextBlock> UserPathTextBlock;
	TSharedPtr<STextBlock> EffectiveSourceTextBlock;
	TSharedPtr<SMultiLineEditableTextBox> SettingJsonTextBox;
};
