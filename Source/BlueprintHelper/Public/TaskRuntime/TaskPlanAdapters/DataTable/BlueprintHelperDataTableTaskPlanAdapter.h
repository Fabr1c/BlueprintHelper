// BlueprintHelper Service Layer - DataTable TaskPlan lowering adapter

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

struct FBlueprintHelperTaskRuntimeLoweredStep;

struct BLUEPRINTHELPER_API FBlueprintHelperDataTableTaskPlanPayload
{
	FString StepId;
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperDataTableTaskPlanAdapter
{
public:
	static constexpr const TCHAR* CapabilityDataTable = TEXT("data_table");
	static constexpr const TCHAR* RuntimeOperationDataTable = TEXT("data_table");
	static constexpr const TCHAR* StrategyRowEdit = TEXT("row_edit");

	static constexpr const TCHAR* OpAddRow = TEXT("add_row");
	static constexpr const TCHAR* OpUpdateRow = TEXT("update_row");
	static constexpr const TCHAR* OpDeleteRow = TEXT("delete_row");

	static constexpr const TCHAR* AdapterOperationAddRow = TEXT("add_datatable_row");
	static constexpr const TCHAR* AdapterOperationUpdateRow = TEXT("update_datatable_row");
	static constexpr const TCHAR* AdapterOperationDeleteRow = TEXT("delete_datatable_row");

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperDataTableTaskPlanPayload& OutPayload,
		FBlueprintHelperToolError& OutError);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);
};
