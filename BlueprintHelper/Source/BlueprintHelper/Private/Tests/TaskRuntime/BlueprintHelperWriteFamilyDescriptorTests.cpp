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
	TestTrue(TEXT("descriptor matrix is populated"), Descriptors.Num() >= 10);

	struct FExpectedWriteFamilyDescriptor
	{
		FString WriteFamily;
		FString RuntimeAdapterId;
		FString TaskSpecStrategy;
		FString ReadbackProjectionMode;
		FString MetricsIdentity;
	};

	const TArray<FExpectedWriteFamilyDescriptor> RequiredFamilies = {
		{TEXT("graphwrite"), TEXT("graphwrite"), TEXT("graphwrite_route_descriptor"), TEXT("graph_body_adapter"), TEXT("blueprint.write.graphwrite")},
		{TEXT("asset_factory"), TEXT("asset_factory"), TEXT("asset_factory"), TEXT("asset_factory"), TEXT("blueprint.write.asset_factory")},
		{TEXT("blueprint_signature"), TEXT("blueprint_signature"), TEXT("blueprint_signature"), TEXT("blueprint_signature"), TEXT("blueprint.write.signature")},
		{TEXT("blueprint_variables"), TEXT("blueprint_variables"), TEXT("blueprint_variables"), TEXT("blueprint_variables"), TEXT("blueprint.write.variables")},
		{TEXT("class_settings"), TEXT("class_settings"), TEXT("class_settings"), TEXT("class_settings"), TEXT("blueprint.write.class_settings")},
		{TEXT("blueprint_component"), TEXT("blueprint_component"), TEXT("blueprint_component"), TEXT("blueprint_component"), TEXT("blueprint.write.component")},
		{TEXT("object_property"), TEXT("object_property"), TEXT("property_strategy"), TEXT("object_property"), TEXT("blueprint.write.object_property")},
		{TEXT("data_table"), TEXT("data_table"), TEXT("row_strategy"), TEXT("data_table"), TEXT("blueprint.write.data_table")},
		{TEXT("umg_widget"), TEXT("umg_widget"), TEXT("widget_strategy"), TEXT("widget_tree"), TEXT("umg.write.umg_widget")},
		{TEXT("material_instance"), TEXT("material_instance"), TEXT("material_instance_edit"), TEXT("material_instance"), TEXT("material.write.material_instance")}
	};

	for (const FExpectedWriteFamilyDescriptor& Expected : RequiredFamilies)
	{
		FBlueprintHelperWriteFamilyDescriptor Descriptor;
		TestTrue(
			FString::Printf(TEXT("descriptor exists: %s"), *Expected.WriteFamily),
			FBlueprintHelperWriteFamilyDescriptorRegistry::TryFindByWriteFamily(Expected.WriteFamily, Descriptor));
		TestFalse(TEXT("runtime adapter is set"), Descriptor.RuntimeAdapterId.IsEmpty());
		TestFalse(TEXT("bridge command is set"), Descriptor.BridgeCommand.IsEmpty());
		TestFalse(TEXT("dry-run policy is set"), Descriptor.DryRunPolicyId.IsEmpty());
		TestFalse(TEXT("metrics identity is set"), Descriptor.MetricsIdentity.IsEmpty());
		TestEqual(TEXT("runtime adapter matches cross-layer catalog"), Descriptor.RuntimeAdapterId, Expected.RuntimeAdapterId);
		TestEqual(TEXT("task spec strategy matches cross-layer catalog"), Descriptor.TaskSpecStrategy, Expected.TaskSpecStrategy);
		TestEqual(TEXT("readback projection matches cross-layer catalog"), Descriptor.ReadbackProjectionMode, Expected.ReadbackProjectionMode);
		TestEqual(TEXT("metrics identity matches cross-layer catalog"), Descriptor.MetricsIdentity, Expected.MetricsIdentity);
		TestEqual(
			FString::Printf(TEXT("%s is active"), *Expected.WriteFamily),
			static_cast<int32>(Descriptor.Status),
			static_cast<int32>(EBlueprintHelperWriteFamilyCapabilityStatus::Active));
	}
	return true;
}

#endif
