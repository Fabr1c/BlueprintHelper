#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperAssetFactoryTaskPlanAdapter
{
public:
	static constexpr const TCHAR* SupportedCapability = TEXT("asset_factory");
	static constexpr const TCHAR* SupportedStrategy = TEXT("asset_create");
	static constexpr const TCHAR* SupportedOp = TEXT("create_asset");
	static constexpr const TCHAR* AdapterOperation = TEXT("create_asset");

	static bool SupportsStep(const TSharedPtr<FJsonObject>& StepObject);

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError);

	bool TryBuildPayload(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError) const;

private:
	static FBlueprintHelperToolError MakeError(
		const FString& Code,
		const FString& Message,
		const FString& Field);
};
