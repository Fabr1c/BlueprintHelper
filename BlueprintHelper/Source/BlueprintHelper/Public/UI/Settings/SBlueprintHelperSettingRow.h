#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/BlueprintHelperSettingRowViewModel.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FBlueprintHelperSettingValueCommitted, const FBlueprintHelperSettingEditEvent&);
DECLARE_DELEGATE_OneParam(FBlueprintHelperSettingResetRequested, const FString&);

class SBlueprintHelperSettingRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperSettingRow)
	{
	}
		SLATE_ARGUMENT(FBlueprintHelperSettingRowViewModel, Row)
		SLATE_EVENT(FBlueprintHelperSettingValueCommitted, OnValueCommitted)
		SLATE_EVENT(FBlueprintHelperSettingResetRequested, OnResetRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildValueWidget();
	TSharedRef<SWidget> BuildNumberWidget();
	TSharedRef<SWidget> BuildIntegerWidget();
	TSharedRef<SWidget> BuildBooleanWidget();
	TSharedRef<SWidget> BuildChoiceWidget();
	TSharedRef<SWidget> BuildStringWidget();
	TSharedRef<SWidget> BuildTextValueWidget(const FText& HintText);
	void CommitValue(const FString& NewValue) const;
	FReply HandleResetClicked() const;
	TSharedPtr<FBlueprintHelperSettingChoiceViewModel> FindCurrentChoice() const;

	FBlueprintHelperSettingRowViewModel Row;
	FBlueprintHelperSettingValueCommitted OnValueCommitted;
	FBlueprintHelperSettingResetRequested OnResetRequested;
	TArray<TSharedPtr<FBlueprintHelperSettingChoiceViewModel>> ChoiceItems;
};
