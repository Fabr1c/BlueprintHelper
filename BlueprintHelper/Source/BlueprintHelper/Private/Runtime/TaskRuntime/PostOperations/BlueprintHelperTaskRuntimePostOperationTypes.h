#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

enum class EBlueprintHelperTaskRuntimePostOperationKind : uint8
{
	Compile,
	Save
};

enum class EBlueprintHelperTaskRuntimePostOperationStatus : uint8
{
	Planned,
	Executed,
	Skipped,
	Failed
};

struct FBlueprintHelperTaskRuntimePostOperationPlanItem
{
	EBlueprintHelperTaskRuntimePostOperationKind Kind = EBlueprintHelperTaskRuntimePostOperationKind::Compile;
	FString Operation;
	FString AssetPath;
	FString Reason;
};

struct FBlueprintHelperTaskRuntimePostOperationPlan
{
	TArray<FBlueprintHelperTaskRuntimePostOperationPlanItem> Items;
	bool bRequestedCompile = false;
	bool bRequestedSave = false;
	bool bHasTargetAssets = true;
	FString MissingTargetAssetsReason;
};

struct FBlueprintHelperTaskRuntimePostOperationRecordEx
{
	EBlueprintHelperTaskRuntimePostOperationKind Kind = EBlueprintHelperTaskRuntimePostOperationKind::Compile;
	FString Operation;
	FString AssetPath;
	EBlueprintHelperTaskRuntimePostOperationStatus Status = EBlueprintHelperTaskRuntimePostOperationStatus::Planned;
	FString Reason;
	double DurationMs = 0.0;
	FBlueprintHelperToolResultBase Result;
};

class FBlueprintHelperTaskRuntimePostOperationJson
{
public:
	static const TCHAR* KindToString(EBlueprintHelperTaskRuntimePostOperationKind Kind);
	static const TCHAR* StatusToString(EBlueprintHelperTaskRuntimePostOperationStatus Status);
	static TSharedRef<FJsonObject> RecordToJson(const FBlueprintHelperTaskRuntimePostOperationRecordEx& Record);
	static TSharedRef<FJsonObject> PlanToJson(const FBlueprintHelperTaskRuntimePostOperationPlan& Plan);
};
