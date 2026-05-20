// BlueprintHelper settings panel.

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/BlueprintHelperSettingRowViewModel.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperSettingsPresenter;
class STextBlock;
class SVerticalBox;

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
	void RefreshRows();
	TSharedRef<SWidget> BuildCategorySection(const FText& CategoryLabel, const TArray<FBlueprintHelperSettingRowViewModel>& Rows);

	TSharedPtr<FBlueprintHelperSettingsPresenter> Presenter;
	TSharedPtr<STextBlock> StatusTextBlock;
	TSharedPtr<STextBlock> DefaultPathTextBlock;
	TSharedPtr<STextBlock> ProjectPathTextBlock;
	TSharedPtr<STextBlock> UserPathTextBlock;
	TSharedPtr<STextBlock> EffectiveSourceTextBlock;
	TSharedPtr<SVerticalBox> CategoriesBox;
};
