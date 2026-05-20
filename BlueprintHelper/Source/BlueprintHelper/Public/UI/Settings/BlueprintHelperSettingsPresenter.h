// BlueprintHelper settings presenter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"
#include "UI/Settings/BlueprintHelperSettingRowViewModel.h"

DECLARE_MULTICAST_DELEGATE(FBlueprintHelperSettingsRowsChanged);

class BLUEPRINTHELPER_API FBlueprintHelperSettingsPresenter
{
public:
	const FBlueprintHelperSettingView& Reload();
	const FBlueprintHelperSettingView& EnsureProjectSetting();
	const FBlueprintHelperSettingView& GetView() const;
	const TArray<FBlueprintHelperSettingRowViewModel>& GetRows() const;
	FBlueprintHelperSettingsRowsChanged& OnRowsChanged();
	void ReloadRows();
	void HandleSettingValueCommitted(const FBlueprintHelperSettingEditEvent& Event);
	void HandleSettingResetRequested(const FString& DotPath);

private:
	bool ValidateRowValue(const FBlueprintHelperSettingRowViewModel& Row, const FString& NewValue, FString& OutNormalizedValue, FText& OutErrorText) const;
	const FBlueprintHelperSettingRowViewModel* FindRowByPath(const FString& DotPath) const;
	void SetRowErrorAndBroadcast(const FString& DotPath, const FText& ErrorText);

	FBlueprintHelperSettingView View;
	TArray<FBlueprintHelperSettingRowViewModel> Rows;
	FBlueprintHelperSettingsRowsChanged RowsChanged;
	TMap<FString, FString> RowErrorsByPath;
};
