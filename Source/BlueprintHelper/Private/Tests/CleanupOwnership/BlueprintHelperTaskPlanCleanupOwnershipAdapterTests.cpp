#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskPlanAdapter.h"

namespace
{
	TSharedPtr<FJsonObject> MakeCleanupOwnershipStep(const FString& OpName)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_cleanup_ownership"));
		Step->SetStringField(TEXT("capability"), FBlueprintHelperCleanupOwnershipTaskPlanAdapter::CapabilityName);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Target->SetStringField(TEXT("graph"), TEXT("EventGraph"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), OpName);

		if (OpName == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationRollbackCleanupTransaction)
		{
			Op->SetStringField(TEXT("transaction_id"), TEXT("tx_cleanup_001"));
			Op->SetStringField(TEXT("rollback_scope"), TEXT("cleanup_transaction"));
		}
		else
		{
			Op->SetStringField(TEXT("block_ref"), TEXT("DoorSetup0"));
			Op->SetStringField(TEXT("block_id"), TEXT("EventGraph_DoorSetup0"));
		}

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperCleanupOwnershipTaskPlanAdapter::StrategyOwnedBlockLifecycle);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	TSharedPtr<FJsonObject> GetFirstCleanupOwnershipOp(const TSharedPtr<FJsonObject>& Step)
	{
		const TSharedPtr<FJsonObject>* Write = nullptr;
		if (!Step.IsValid() || !Step->TryGetObjectField(TEXT("write"), Write) || !Write || !Write->IsValid())
		{
			return nullptr;
		}

		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (!(*Write)->TryGetArrayField(TEXT("ops"), Ops) || !Ops || Ops->Num() == 0)
		{
			return nullptr;
		}

		return (*Ops)[0].IsValid() ? (*Ops)[0]->AsObject() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanCleanupOwnershipAdapterCleanupBlockTest,
	"BlueprintHelper.TaskPlan.CleanupOwnershipAdapter.CleanupBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanCleanupOwnershipAdapterCleanupBlockTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeCleanupOwnershipStep(
		FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationCleanupBlueprintHelperBlock);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperCleanupOwnershipTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("cleanup block step lowers successfully"), bLowered);
	TestEqual(TEXT("capability is graph_cleanup_ownership"), LoweredStep.Capability, FString(TEXT("graph_cleanup_ownership")));
	TestEqual(TEXT("runtime operation is graph_cleanup_ownership"), LoweredStep.RuntimeOperation, FString(TEXT("graph_cleanup_ownership")));
	TestEqual(TEXT("adapter operation is cleanup_blueprint_helper_block"), LoweredStep.AdapterOperation, FString(TEXT("cleanup_blueprint_helper_block")));
	TestTrue(TEXT("cleanup ownership adapter supports service dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestNotNull(TEXT("lowered payload exists"), LoweredStep.Payload.Get());
	if (!bLowered || !LoweredStep.Payload.IsValid())
	{
		return false;
	}

	FString AssetPath;
	FString GraphName;
	FString BlockRef;
	FString BlockId;
	bool bDryRun = false;
	TestTrue(TEXT("payload carries asset_path"), LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries graph"), LoweredStep.Payload->TryGetStringField(TEXT("graph"), GraphName));
	TestTrue(TEXT("payload carries block_ref"), LoweredStep.Payload->TryGetStringField(TEXT("block_ref"), BlockRef));
	TestTrue(TEXT("payload carries block_id"), LoweredStep.Payload->TryGetStringField(TEXT("block_id"), BlockId));
	TestTrue(TEXT("payload carries dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Blueprints/BP_Door")));
	TestEqual(TEXT("graph is preserved"), GraphName, FString(TEXT("EventGraph")));
	TestEqual(TEXT("block_ref is preserved"), BlockRef, FString(TEXT("DoorSetup0")));
	TestEqual(TEXT("block_id is preserved"), BlockId, FString(TEXT("EventGraph_DoorSetup0")));
	TestTrue(TEXT("dry_run is preserved"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanCleanupOwnershipAdapterConvertAliasTest,
	"BlueprintHelper.TaskPlan.CleanupOwnershipAdapter.ConvertAlias",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanCleanupOwnershipAdapterConvertAliasTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeCleanupOwnershipStep(TEXT("convert_block_to_user_owned"));
	TSharedPtr<FJsonObject> Op = GetFirstCleanupOwnershipOp(Step);
	TestNotNull(TEXT("cleanup ownership op exists"), Op.Get());
	Op->SetStringField(TEXT("already_user_owned_policy"), TEXT("ignore"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperCleanupOwnershipTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("convert alias lowers successfully"), bLowered);
	TestEqual(TEXT("convert alias maps to canonical adapter operation"), LoweredStep.AdapterOperation, FString(TEXT("convert_blueprint_helper_block_to_user_owned")));
	TestTrue(TEXT("cleanup ownership adapter supports service dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestNotNull(TEXT("lowered payload exists"), LoweredStep.Payload.Get());
	if (!bLowered || !LoweredStep.Payload.IsValid())
	{
		return false;
	}

	bool bDryRun = true;
	FString AlreadyUserOwnedPolicy;
	TestTrue(TEXT("payload carries dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("payload carries already_user_owned_policy"), LoweredStep.Payload->TryGetStringField(TEXT("already_user_owned_policy"), AlreadyUserOwnedPolicy));
	TestFalse(TEXT("execute dry_run is preserved as false"), bDryRun);
	TestEqual(TEXT("already_user_owned_policy preserved"), AlreadyUserOwnedPolicy, FString(TEXT("ignore")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanCleanupOwnershipAdapterRollbackDryRunTest,
	"BlueprintHelper.TaskPlan.CleanupOwnershipAdapter.RollbackDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanCleanupOwnershipAdapterRollbackDryRunTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeCleanupOwnershipStep(
		FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationRollbackCleanupTransaction);
	TSharedPtr<FJsonObject> Op = GetFirstCleanupOwnershipOp(Step);
	TestNotNull(TEXT("cleanup ownership op exists"), Op.Get());
	Op->SetStringField(TEXT("already_rolled_back_policy"), TEXT("ignore"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperCleanupOwnershipTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("rollback step lowers successfully"), bLowered);
	TestEqual(TEXT("rollback adapter operation is canonical"), LoweredStep.AdapterOperation, FString(TEXT("rollback_cleanup_transaction")));
	TestTrue(TEXT("rollback adapter supports service dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestNotNull(TEXT("lowered payload exists"), LoweredStep.Payload.Get());
	if (!bLowered || !LoweredStep.Payload.IsValid())
	{
		return false;
	}

	FString TransactionId;
	FString RollbackScope;
	FString AlreadyRolledBackPolicy;
	bool bDryRun = false;
	TestTrue(TEXT("payload carries transaction_id"), LoweredStep.Payload->TryGetStringField(TEXT("transaction_id"), TransactionId));
	TestTrue(TEXT("payload carries rollback_scope"), LoweredStep.Payload->TryGetStringField(TEXT("rollback_scope"), RollbackScope));
	TestTrue(TEXT("payload carries already_rolled_back_policy"), LoweredStep.Payload->TryGetStringField(TEXT("already_rolled_back_policy"), AlreadyRolledBackPolicy));
	TestTrue(TEXT("payload carries dry_run"), LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestEqual(TEXT("transaction_id preserved"), TransactionId, FString(TEXT("tx_cleanup_001")));
	TestEqual(TEXT("rollback_scope preserved"), RollbackScope, FString(TEXT("cleanup_transaction")));
	TestEqual(TEXT("already_rolled_back_policy preserved"), AlreadyRolledBackPolicy, FString(TEXT("ignore")));
	TestTrue(TEXT("rollback preview dry_run is preserved"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanCleanupOwnershipAdapterRejectsInvalidOpTest,
	"BlueprintHelper.TaskPlan.CleanupOwnershipAdapter.RejectsInvalidOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanCleanupOwnershipAdapterRejectsInvalidOpTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeCleanupOwnershipStep(TEXT("cleanup_feature"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperCleanupOwnershipTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		false,
		LoweredStep,
		Error);

	TestFalse(TEXT("unsupported cleanup ownership op is rejected"), bLowered);
	TestEqual(TEXT("invalid op error code"), Error.Code, FString(TEXT("unsupported_cleanup_ownership_op")));
	TestEqual(TEXT("invalid op error path"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].op")));
	TestEqual(TEXT("invalid op error stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanCleanupOwnershipAdapterOperationNamesTest,
	"BlueprintHelper.TaskPlan.CleanupOwnershipAdapter.OperationNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanCleanupOwnershipAdapterOperationNamesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("capability name"), FString(FBlueprintHelperCleanupOwnershipTaskPlanAdapter::CapabilityName), FString(TEXT("graph_cleanup_ownership")));
	TestEqual(TEXT("strategy name"), FString(FBlueprintHelperCleanupOwnershipTaskPlanAdapter::StrategyOwnedBlockLifecycle), FString(TEXT("owned_block_lifecycle")));
	TestEqual(TEXT("cleanup operation name"), FString(FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationCleanupBlueprintHelperBlock), FString(TEXT("cleanup_blueprint_helper_block")));
	TestEqual(TEXT("convert operation name"), FString(FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationConvertBlueprintHelperBlockToUserOwned), FString(TEXT("convert_blueprint_helper_block_to_user_owned")));
	TestEqual(TEXT("rollback operation name"), FString(FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationRollbackCleanupTransaction), FString(TEXT("rollback_cleanup_transaction")));

	const TSharedPtr<FJsonObject> Step = MakeCleanupOwnershipStep(TEXT("cleanup_blueprinthelper_block"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperCleanupOwnershipTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("legacy cleanup alias lowers successfully"), bLowered);
	TestEqual(TEXT("legacy cleanup alias maps to canonical adapter operation"), LoweredStep.AdapterOperation, FString(TEXT("cleanup_blueprint_helper_block")));

	return true;
}

#endif
