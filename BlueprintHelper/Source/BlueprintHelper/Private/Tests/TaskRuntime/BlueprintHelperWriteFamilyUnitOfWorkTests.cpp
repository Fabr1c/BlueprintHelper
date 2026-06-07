#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapterRegistry.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteUnitOfWork.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWriteFamilyUnitOfWorkCoversFamiliesTest,
	"BlueprintHelper.TaskRuntime.WriteFamilyUnitOfWork.CoversFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWriteFamilyUnitOfWorkCoversFamiliesTest::RunTest(const FString&)
{
	for (const FBlueprintHelperWriteFamilyDescriptor& Descriptor :
		FBlueprintHelperWriteFamilyDescriptorRegistry::GetKnownDescriptors())
	{
		if (Descriptor.Status != EBlueprintHelperWriteFamilyCapabilityStatus::Active)
		{
			continue;
		}

		TSharedPtr<IBlueprintHelperWriteFamilyAdapter> Adapter;
		TestTrue(
			FString::Printf(TEXT("adapter resolves: %s"), *Descriptor.WriteFamily),
			FBlueprintHelperWriteFamilyAdapterRegistry::TryFindByWriteFamily(Descriptor.WriteFamily, Adapter));
		if (!Adapter.IsValid())
		{
			continue;
		}

		FBlueprintHelperAcceptedPayloadModel AcceptedPayload;
		AcceptedPayload.TaskId = TEXT("task_uow");
		AcceptedPayload.OperationId = TEXT("unit_of_work_test");
		AcceptedPayload.WriteFamily = Descriptor.WriteFamily;
		AcceptedPayload.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
		AcceptedPayload.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
		AcceptedPayload.BridgeCommand = Descriptor.BridgeCommand;
		AcceptedPayload.Mode = TEXT("preview");

		FBlueprintHelperWriteUnitOfWorkRequest Request;
		FBlueprintHelperToolError Error;
		TestTrue(
			FString::Printf(TEXT("request builds: %s"), *Descriptor.WriteFamily),
			Adapter->BuildUnitOfWorkRequest(AcceptedPayload, Request, Error));
		Request.Mode = EBlueprintHelperWriteUnitOfWorkMode::Preview;
		Request.ApplyMutation = []()
		{
			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
				TEXT("write_family_unit_of_work_test"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
			Result.Data = MakeShared<FJsonObject>();
			return Result;
		};

		const FBlueprintHelperWriteUnitOfWorkResult Result = FBlueprintHelperWriteUnitOfWork::Run(Request);
		TestTrue(
			FString::Printf(TEXT("%s unit of work succeeds"), *Descriptor.WriteFamily),
			Result.ToolResult.bOk);
		TestTrue(TEXT("stage trace includes apply"), Result.StageTrace.Contains(TEXT("apply_mutation")));
		TestTrue(TEXT("stage trace includes diagnostics"), Result.StageTrace.Contains(TEXT("build_diagnostics")));
		TestTrue(TEXT("result data exists"), Result.ToolResult.Data.IsValid());
		TestEqual(
			TEXT("result carries write family"),
			Result.ToolResult.Data->GetStringField(TEXT("write_family")),
			Descriptor.WriteFamily);
		TestEqual(
			TEXT("result carries runtime adapter"),
			Result.ToolResult.Data->GetStringField(TEXT("runtime_adapter_id")),
			Descriptor.RuntimeAdapterId);
	}
	return true;
}

#endif
