#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWriteFamilyDescriptorMatrixTest,
	"BlueprintHelper.TaskRuntime.WriteFamilyDescriptor.Matrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWriteFamilyDescriptorMatrixTest::RunTest(const FString&)
{
	const TArray<FBlueprintHelperWriteFamilyDescriptor> Descriptors =
		FBlueprintHelperWriteFamilyDescriptorRegistry::GetKnownDescriptors();
	TestTrue(TEXT("descriptor matrix is populated"), Descriptors.Num() >= 9);

	const TArray<FString> RequiredFamilies = {
		TEXT("graphwrite"),
		TEXT("asset_factory"),
		TEXT("blueprint_signature"),
		TEXT("blueprint_variables"),
		TEXT("class_settings"),
		TEXT("blueprint_component"),
		TEXT("object_property"),
		TEXT("data_table"),
		TEXT("umg_widget")
	};

	for (const FString& Family : RequiredFamilies)
	{
		FBlueprintHelperWriteFamilyDescriptor Descriptor;
		TestTrue(
			FString::Printf(TEXT("descriptor exists: %s"), *Family),
			FBlueprintHelperWriteFamilyDescriptorRegistry::TryFindByWriteFamily(Family, Descriptor));
		TestFalse(TEXT("runtime adapter is set"), Descriptor.RuntimeAdapterId.IsEmpty());
		TestFalse(TEXT("bridge command is set"), Descriptor.BridgeCommand.IsEmpty());
		TestFalse(TEXT("dry-run policy is set"), Descriptor.DryRunPolicyId.IsEmpty());
		TestFalse(TEXT("metrics identity is set"), Descriptor.MetricsIdentity.IsEmpty());
		TestEqual(
			FString::Printf(TEXT("%s is active"), *Family),
			static_cast<int32>(Descriptor.Status),
			static_cast<int32>(EBlueprintHelperWriteFamilyCapabilityStatus::Active));
	}
	return true;
}

#endif
