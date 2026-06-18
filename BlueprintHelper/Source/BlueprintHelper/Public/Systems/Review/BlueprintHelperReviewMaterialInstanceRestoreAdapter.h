// BlueprintHelper Review MaterialInstance restore adapter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewRestoreAdapter.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceTypes.h"

class UMaterialInstanceConstant;

class BLUEPRINTHELPER_API FBlueprintHelperReviewMaterialInstanceRestoreAdapter final
	: public IBlueprintHelperReviewRestoreAdapter
{
public:
	explicit FBlueprintHelperReviewMaterialInstanceRestoreAdapter(const FString& InTargetKind);

	virtual FString GetTargetKind() const override;
	virtual FBlueprintHelperReviewRestoreResult RestoreBeforeSnapshot(
		const FBlueprintHelperReviewVisibleChange& Change) const override;

private:
	static bool RestoreParent(
		UMaterialInstanceConstant* Instance,
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutError);

	static bool RestoreParameter(
		UMaterialInstanceConstant* Instance,
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutError);

	static bool ApplyParameterOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& ParameterInfo,
		EBlueprintHelperMaterialInstanceParameterType Type,
		const FBlueprintHelperMaterialInstanceParameterValue& Value,
		FString& OutError);

	static bool ClearParameterOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& ParameterInfo,
		EBlueprintHelperMaterialInstanceParameterType Type,
		FString& OutError);

	FString TargetKind;
};
