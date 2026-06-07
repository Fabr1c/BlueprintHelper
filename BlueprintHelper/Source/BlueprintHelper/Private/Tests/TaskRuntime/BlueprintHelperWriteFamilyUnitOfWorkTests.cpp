#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapterRegistry.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteUnitOfWork.h"

static bool LoadBlueprintHelperWriteFamilyUnitOfWorkTestSource(
	FAutomationTestBase& Test,
	const FString& RelativePath,
	FString& OutSource)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	const FString Path = Plugin.IsValid()
		? FPaths::Combine(Plugin->GetBaseDir(), RelativePath)
		: FString();
	if (Path.IsEmpty() || !FFileHelper::LoadFileToString(OutSource, *Path))
	{
		Test.AddError(FString::Printf(TEXT("Unable to load source file for write-family guard: %s"), *RelativePath));
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWriteFamilyAdapterSpecializationGuardTest,
	"BlueprintHelper.TaskRuntime.WriteFamily.AdapterSpecializationGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWriteFamilyAdapterSpecializationGuardTest::RunTest(const FString&)
{
	FString RegistrySource;
	if (!LoadBlueprintHelperWriteFamilyUnitOfWorkTestSource(
		*this,
		TEXT("Source/BlueprintHelper/Private/Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapterRegistry.cpp"),
		RegistrySource))
	{
		return false;
	}

	const FString StaticAdapterToken =
		FString(TEXT("FBlueprintHelper")) + TEXT("StaticWriteFamilyAdapter");
	TestFalse(TEXT("descriptor-only static adapter is removed"), RegistrySource.Contains(StaticAdapterToken));
	TestTrue(
		TEXT("graphwrite family adapter is registered"),
		RegistrySource.Contains(TEXT("FBlueprintHelperGraphWriteFamilyAdapter")));
	TestTrue(
		TEXT("umg family adapter is registered"),
		RegistrySource.Contains(TEXT("FBlueprintHelperUMGWidgetFamilyAdapter")));

	FString GraphWriteAdapterSource;
	FString UMGWidgetAdapterSource;
	TestTrue(
		TEXT("graphwrite family adapter file exists"),
		LoadBlueprintHelperWriteFamilyUnitOfWorkTestSource(
			*this,
			TEXT("Source/BlueprintHelper/Private/Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperGraphWriteFamilyAdapter.h"),
			GraphWriteAdapterSource));
	TestTrue(
		TEXT("umg family adapter file exists"),
		LoadBlueprintHelperWriteFamilyUnitOfWorkTestSource(
			*this,
			TEXT("Source/BlueprintHelper/Private/Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperUMGWidgetFamilyAdapter.h"),
			UMGWidgetAdapterSource));
	TestTrue(
		TEXT("graphwrite family adapter class exists"),
		GraphWriteAdapterSource.Contains(TEXT("FBlueprintHelperGraphWriteFamilyAdapter")));
	TestTrue(
		TEXT("umg family adapter class exists"),
		UMGWidgetAdapterSource.Contains(TEXT("FBlueprintHelperUMGWidgetFamilyAdapter")));
	return true;
}

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
			FString::Printf(TEXT("preflight builds: %s"), *Descriptor.WriteFamily),
			Adapter->BuildPreflight(AcceptedPayload, Request, Error));
		Request.Mode = EBlueprintHelperWriteUnitOfWorkMode::Preview;
		Request.ApplyMutation = []()
		{
			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
				TEXT("write_family_unit_of_work_test"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
			Result.Data = MakeShared<FJsonObject>();
			return Result;
		};
		TestTrue(
			FString::Printf(TEXT("mutation plan builds: %s"), *Descriptor.WriteFamily),
			Adapter->BuildMutationPlan(AcceptedPayload, Request, Error));
		TestTrue(
			FString::Printf(TEXT("dry-run projection builds: %s"), *Descriptor.WriteFamily),
			Adapter->BuildDryRunProjection(AcceptedPayload, Request, Error));
		TestTrue(
			FString::Printf(TEXT("review/readback hooks build: %s"), *Descriptor.WriteFamily),
			Adapter->BuildReviewAndReadback(AcceptedPayload, Request, Error));

		TestTrue(TEXT("adapter sets CaptureBefore"), !!Request.CaptureBefore);
		TestTrue(TEXT("adapter sets BuildFamilyMutationPlan"), !!Request.BuildFamilyMutationPlan);
		TestTrue(TEXT("adapter sets ApplyMutation"), !!Request.ApplyMutation);
		TestTrue(TEXT("adapter sets ProjectDryRun"), !!Request.ProjectDryRun);
		TestTrue(TEXT("adapter sets RecordOwnershipDelta"), !!Request.RecordOwnershipDelta);
		TestTrue(TEXT("adapter sets Commit"), !!Request.Commit);
		TestTrue(TEXT("adapter sets Rollback"), !!Request.Rollback);
		TestTrue(TEXT("adapter sets BuildDiagnostics"), !!Request.BuildDiagnostics);

		const FBlueprintHelperWriteUnitOfWorkResult Result = FBlueprintHelperWriteUnitOfWork::Run(Request);
		TestTrue(
			FString::Printf(TEXT("%s unit of work succeeds"), *Descriptor.WriteFamily),
			Result.ToolResult.bOk);
		TestTrue(TEXT("stage trace includes capture"), Result.StageTrace.Contains(TEXT("capture_before")));
		TestTrue(TEXT("stage trace includes family mutation plan"), Result.StageTrace.Contains(TEXT("build_family_mutation_plan")));
		TestTrue(TEXT("stage trace includes apply"), Result.StageTrace.Contains(TEXT("apply_mutation")));
		TestTrue(TEXT("stage trace includes dry-run projection"), Result.StageTrace.Contains(TEXT("project_dry_run")));
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
