#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintHelperTaskRuntimeClusterFamilyRegistry.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeClusterRegistryCoversWriteFamiliesTest,
	"BlueprintHelper.TaskRuntime.ClusterRegistry.CoversWriteFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeClusterRegistryCoversWriteFamiliesTest::RunTest(const FString&)
{
	for (const FBlueprintHelperWriteFamilyDescriptor& WriteDescriptor :
		FBlueprintHelperWriteFamilyDescriptorRegistry::GetKnownDescriptors())
	{
		if (WriteDescriptor.Status != EBlueprintHelperWriteFamilyCapabilityStatus::Active)
		{
			continue;
		}

		FBlueprintHelperTaskRuntimeClusterFamilyDescriptor ClusterDescriptor;
		TestTrue(
			FString::Printf(TEXT("cluster descriptor exists: %s"), *WriteDescriptor.WriteFamily),
			FBlueprintHelperTaskRuntimeClusterFamilyRegistry::TryFindByWriteFamily(
				WriteDescriptor.WriteFamily,
				ClusterDescriptor));
		TestEqual(
			TEXT("cluster family matches write descriptor"),
			ClusterDescriptor.ClusterFamily,
			WriteDescriptor.ClusterFamily);
		TestTrue(TEXT("can-execute predicate is registered"), ClusterDescriptor.CanExecuteStep != nullptr);
	}
	return true;
}

#endif
