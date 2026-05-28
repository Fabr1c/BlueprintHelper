// BlueprintHelper Settings UI utility functions.
// Aggregates static helpers extracted from anonymous namespaces across Settings UI files.

#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/BlueprintHelperSettingRowViewModel.h"

#include "BlueprintHelperSettingsUIUtils.generated.h"

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperSettingsUIUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// === From BlueprintHelperSettingsPresenter.cpp ===

	static bool IsRuntimeConsumedSetting(const FString& DotPath);
	static bool ShouldShowDeveloperSettings();
	static FString ReadSettingValueOrDefault(const FString& DotPath, FString& OutDefaultValue, bool& bOutHasProjectOverride);

	static FBlueprintHelperSettingRowViewModel MakeBaseRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		EBlueprintHelperSettingValueType ValueType,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeNumberRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		double MinValue,
		double MaxValue,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeIntegerRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		int32 MinValue,
		int32 MaxValue,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeBooleanRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeChoiceRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		TArray<FBlueprintHelperSettingChoiceViewModel> Choices,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeVector2Row(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		double MinValue,
		double MaxValue,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeMarginRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		double MinValue,
		double MaxValue,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeStringRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		bool bDeveloperOnly = false);

	static FBlueprintHelperSettingRowViewModel MakeColorArrayRow(
		const FString& DotPath,
		const FText& Category,
		const FText& Label,
		const FText& Hint,
		bool bDeveloperOnly = false);

	static bool ParseNumberList(const FString& Input, int32 ExpectedCount, TArray<double>& OutValues);
	static FString NumberListToJsonArray(const TArray<double>& Values);
};
