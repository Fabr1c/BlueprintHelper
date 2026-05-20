#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationPlanner.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperTaskRuntimePostOperationPlannerLocalUtils
{
public:
	static bool TryReadExecutionPolicyBool(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TCHAR* FieldName,
		bool& OutValue)
	{
		OutValue = false;
		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		return TaskPlan.IsValid() &&
			TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) &&
			ExecutionPolicyPtr &&
			ExecutionPolicyPtr->IsValid() &&
			(*ExecutionPolicyPtr)->TryGetBoolField(FieldName, OutValue);
	}

	static void AppendOperationItems(
		FBlueprintHelperTaskRuntimePostOperationPlan& Plan,
		const TArray<FString>& TargetAssets,
		EBlueprintHelperTaskRuntimePostOperationKind Kind,
		const FString& Operation)
	{
		for (const FString& AssetPath : TargetAssets)
		{
			FBlueprintHelperTaskRuntimePostOperationPlanItem Item;
			Item.Kind = Kind;
			Item.Operation = Operation;
			Item.AssetPath = AssetPath;
			Plan.Items.Add(Item);
		}
	}
};

FBlueprintHelperTaskRuntimePostOperationPlan FBlueprintHelperTaskRuntimePostOperationPlanner::BuildPlan(
	const TSharedPtr<FJsonObject>& TaskPlan,
	bool bDryRun)
{
	FBlueprintHelperTaskRuntimePostOperationPlan Plan;
	if (bDryRun)
	{
		Plan.MissingTargetAssetsReason = TEXT("dry_run");
		return Plan;
	}

	const bool bHasCompilePolicy = FBlueprintHelperTaskRuntimePostOperationPlannerLocalUtils::TryReadExecutionPolicyBool(
		TaskPlan,
		TEXT("should_compile"),
		Plan.bRequestedCompile);
	const bool bHasSavePolicy = FBlueprintHelperTaskRuntimePostOperationPlannerLocalUtils::TryReadExecutionPolicyBool(
		TaskPlan,
		TEXT("should_save"),
		Plan.bRequestedSave);
	Plan.bRequestedCompile = bHasCompilePolicy && Plan.bRequestedCompile;
	Plan.bRequestedSave = bHasSavePolicy && Plan.bRequestedSave;

	if (!Plan.bRequestedCompile && !Plan.bRequestedSave)
	{
		return Plan;
	}

	const TArray<FString> TargetAssets = ReadUniqueTargetAssets(TaskPlan);
	if (TargetAssets.Num() == 0)
	{
		Plan.bHasTargetAssets = false;
		Plan.MissingTargetAssetsReason = TEXT("missing_target_assets_for_post_operation");
		return Plan;
	}

	if (Plan.bRequestedCompile)
	{
		FBlueprintHelperTaskRuntimePostOperationPlannerLocalUtils::AppendOperationItems(
			Plan,
			TargetAssets,
			EBlueprintHelperTaskRuntimePostOperationKind::Compile,
			TEXT("compile_blueprint_asset"));
	}

	if (Plan.bRequestedSave)
	{
		FBlueprintHelperTaskRuntimePostOperationPlannerLocalUtils::AppendOperationItems(
			Plan,
			TargetAssets,
			EBlueprintHelperTaskRuntimePostOperationKind::Save,
			TEXT("save_asset"));
	}

	return Plan;
}

FString FBlueprintHelperTaskRuntimePostOperationPlanner::NormalizeAssetPath(const FString& AssetPath)
{
	FString Normalized = AssetPath;
	Normalized.TrimStartAndEndInline();
	const int32 LastSlashIndex = Normalized.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	const int32 DotIndex = Normalized.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (DotIndex != INDEX_NONE && (LastSlashIndex == INDEX_NONE || DotIndex > LastSlashIndex))
	{
		Normalized.LeftInline(DotIndex, EAllowShrinking::No);
	}
	return Normalized;
}

TArray<FString> FBlueprintHelperTaskRuntimePostOperationPlanner::ReadUniqueTargetAssets(
	const TSharedPtr<FJsonObject>& TaskPlan)
{
	TArray<FString> Assets;
	const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
	if (!TaskPlan.IsValid() ||
		!TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) ||
		!TargetAssets)
	{
		return Assets;
	}

	for (const TSharedPtr<FJsonValue>& AssetValue : *TargetAssets)
	{
		if (!AssetValue.IsValid() || AssetValue->Type != EJson::String)
		{
			continue;
		}

		const FString NormalizedAssetPath = NormalizeAssetPath(AssetValue->AsString());
		if (!NormalizedAssetPath.IsEmpty() && !Assets.Contains(NormalizedAssetPath))
		{
			Assets.Add(NormalizedAssetPath);
		}
	}

	return Assets;
}
