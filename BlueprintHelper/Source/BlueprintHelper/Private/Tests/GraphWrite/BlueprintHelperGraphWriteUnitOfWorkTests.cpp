#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteUnitOfWorkStageTraceCoversFamiliesTest,
	"BlueprintHelper.GraphWrite.DryRunSafety.UnitOfWorkStageTraceCoversFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteUnitOfWorkStageTraceCoversFamiliesTest::RunTest(const FString&)
{
	for (const FBlueprintHelperGraphBodyAdapterDescriptor& Descriptor :
		FBlueprintHelperGraphBodyAdapterRegistry::GetKnownDescriptors())
	{
		if (Descriptor.bReservedOnly)
		{
			continue;
		}

		FBlueprintHelperGraphWriteUnitOfWorkRequest Request;
		Request.Mode = EBlueprintHelperGraphWriteUnitOfWorkMode::Preview;
		Request.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
		Request.BoundaryModel.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
		Request.BoundaryModel.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
		Request.BoundaryModel.BodyKind = Descriptor.BodyKind;
		Request.ApplyMutation = []()
		{
			FBlueprintHelperToolResultBase Result;
			Result.bOk = true;
			Result.Schema = FBlueprintHelperProtocol::ToolResultSchema;
			Result.Operation = TEXT("unit_of_work_preview_test");
			Result.Status = EBlueprintHelperToolStatus::DryRun;
			Result.Data = MakeShared<FJsonObject>();
			return Result;
		};

		const FBlueprintHelperGraphWriteUnitOfWorkResult Result =
			FBlueprintHelperGraphWriteUnitOfWork::Run(Request);

		TestTrue(
			FString::Printf(TEXT("%s unit of work succeeds"), *Descriptor.RuntimeAdapterId),
			Result.ToolResult.bOk);
		TestTrue(
			FString::Printf(TEXT("%s has capture stage"), *Descriptor.RuntimeAdapterId),
			Result.StageTrace.Contains(TEXT("capture_before")));
		TestTrue(
			FString::Printf(TEXT("%s has mutation stage"), *Descriptor.RuntimeAdapterId),
			Result.StageTrace.Contains(TEXT("apply_mutation")));
		TestTrue(
			FString::Printf(TEXT("%s has preview projection stage"), *Descriptor.RuntimeAdapterId),
			Result.StageTrace.Contains(TEXT("project_preview")));
		TestTrue(
			FString::Printf(TEXT("%s has diagnostics stage"), *Descriptor.RuntimeAdapterId),
			Result.StageTrace.Contains(TEXT("build_diagnostics")));
		TestTrue(TEXT("stage trace is attached to data"), Result.ToolResult.Data.IsValid());
		const TArray<TSharedPtr<FJsonValue>>* TraceJson = nullptr;
		TestTrue(
			FString::Printf(TEXT("%s stage trace is in result data"), *Descriptor.RuntimeAdapterId),
			Result.ToolResult.Data.IsValid() &&
			Result.ToolResult.Data->TryGetArrayField(TEXT("unit_of_work_stage_trace"), TraceJson) &&
			TraceJson &&
			TraceJson->Num() == Result.StageTrace.Num());
	}
	return true;
}

#endif
