// BlueprintHelper TaskRuntime cluster hub utilities.

#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterHubUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

EBlueprintHelperTaskRuntimeCluster FBlueprintHelperTaskRuntimeClusterHubUtils::ResolveClusterForLoweredStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	using FClusterRoute = TTuple<EBlueprintHelperTaskRuntimeCluster, FCanExecutePredicate>;

	const FClusterRoute Routes[] = {
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::GraphWrite,
			&FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::BlueprintVariables,
			&FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::AssetFactory,
			&FBlueprintHelperAssetFactoryTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::Component,
			&FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::ClassSettings,
			&FBlueprintHelperClassSettingsTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::Signature,
			&FBlueprintHelperSignatureTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::UMGWidget,
			&FBlueprintHelperUMGWidgetTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::DataTable,
			&FBlueprintHelperDataTableTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::ObjectProperty,
			&FBlueprintHelperObjectPropertyTaskRuntimeCluster::CanExecuteStep),
		MakeTuple(
			EBlueprintHelperTaskRuntimeCluster::CleanupOwnership,
			&FBlueprintHelperCleanupOwnershipTaskRuntimeCluster::CanExecuteStep)
	};

	for (const FClusterRoute& Route : Routes)
	{
		if (Route.Get<1>()(LoweredStep))
		{
			return Route.Get<0>();
		}
	}

	return EBlueprintHelperTaskRuntimeCluster::Unknown;
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeClusterHubUtils::MakeSyntheticDryRunData()
{
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
	DryRun->SetBoolField(TEXT("can_execute"), true);
	DryRun->SetStringField(TEXT("preview_kind"), TEXT("synthetic"));
	DryRun->SetStringField(TEXT("validated_scope"), TEXT("taskplan_lowering_only"));
	DryRun->SetStringField(
		TEXT("limitation"),
		TEXT("Adapter service dry-run is not implemented; target asset state and service preflight were not validated."));

	TArray<TSharedPtr<FJsonValue>> Warnings;
	Warnings.Add(MakeShared<FJsonValueString>(
		TEXT("Synthetic preview only validates TaskPlan lowering. Execute may still fail in the underlying service preflight.")));
	DryRun->SetArrayField(TEXT("warnings"), MoveTemp(Warnings));
	DryRun->SetArrayField(TEXT("conflicts"), {});
	DryRun->SetArrayField(TEXT("errors"), {});
	Data->SetObjectField(TEXT("dry_run"), DryRun);
	return Data;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeClusterHubUtils::MakeUnsupportedAdapterOperationResult(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	FBlueprintHelperToolError Error;
	Error.Code = TEXT("unsupported_taskplan_adapter_operation");
	Error.Stage = EBlueprintHelperToolStage::ParseInput;
	Error.Message = TEXT("Task Runtime lowering produced an unsupported adapter operation.");
	Error.bRetryable = false;
	Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
	Error.Field = TEXT("task_plan.steps[0]");

	return FBlueprintHelperToolResultBuilder::Failure(
		LoweredStep.RuntimeOperation.IsEmpty() ? TEXT("execute_task_plan") : LoweredStep.RuntimeOperation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		Error);
}
