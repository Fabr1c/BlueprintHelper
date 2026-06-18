// BlueprintHelper Review MaterialInstance presenter model.

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperReviewDataAssetRowItem;

class UMaterialInstanceConstant;

class BLUEPRINTHELPER_API FBlueprintHelperReviewMaterialInstancePresenterModel
{
public:
	static FString MakeParameterTargetKey(
		const FString& ParameterName,
		const FString& ParameterType);

	static FString MakeParameterDisplayLabel(
		const FString& ParameterName,
		const FString& ParameterType);

	static void AppendStableMatchKeys(
		const FString& ParameterName,
		const FString& ParameterType,
		TArray<FString>& OutKeys,
		const FString& TargetKey = FString(),
		const FString& PropertyPath = FString(),
		const FString& DisplayLabel = FString());

	static void AppendRows(
		UMaterialInstanceConstant* MaterialInstance,
		TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>& OutRows);
};
