#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/Capabilities/BlueprintHelperRuntimeCapabilityStateBuilder.h"
#include "Runtime/Capabilities/BlueprintHelperCapabilityDescriptorTypes.h"
#include "Runtime/Capabilities/BlueprintHelperGeneratedCapabilityRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCapabilityDescriptorRegistryTest,
	"BlueprintHelper.CapabilityDescriptor.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCapabilityDescriptorRegistryTest::RunTest(const FString& Parameters)
{
	TSet<FString> Ids;
	for (const FBlueprintHelperGeneratedCapabilityDescriptor& Descriptor :
		FBlueprintHelperGeneratedCapabilityRegistry::ListDescriptors())
	{
		const FString Id(Descriptor.Id);
		TestFalse(FString::Printf(TEXT("%s descriptor id is unique"), *Id), Ids.Contains(Id));
		Ids.Add(Id);
		TestFalse(FString::Printf(TEXT("%s runtime adapter id is set"), *Id), FString(Descriptor.RuntimeAdapterId).IsEmpty());
	}

	TestTrue(TEXT("GraphWrite descriptor exists"),
		FBlueprintHelperGeneratedCapabilityRegistry::FindById(TEXT("graphwrite.execute")) != nullptr);
	TestTrue(TEXT("MaterialGraph descriptor exists"),
		FBlueprintHelperGeneratedCapabilityRegistry::FindById(TEXT("material_graph.edit")) != nullptr);

	const FBlueprintHelperCapabilityDescriptorRuntimeState RuntimeState =
		FBlueprintHelperRuntimeCapabilityStateBuilder::BuildRegisteredRuntimeState();

	const FBlueprintHelperGeneratedCapabilityDescriptor* MaterialGraph =
		FBlueprintHelperGeneratedCapabilityRegistry::FindById(TEXT("material_graph.edit"));
	TestTrue(TEXT("MaterialGraph is visible with active runtime adapter"),
		MaterialGraph != nullptr &&
		FBlueprintHelperGeneratedCapabilityRegistry::IsDescriptorAgentVisible(*MaterialGraph, RuntimeState));

	const FBlueprintHelperGeneratedCapabilityDescriptor* MaterialInstance =
		FBlueprintHelperGeneratedCapabilityRegistry::FindById(TEXT("material_instance.edit"));
	TestTrue(TEXT("MaterialInstance descriptor exists"),
		MaterialInstance != nullptr);
	TestTrue(TEXT("MaterialInstance is visible after P4 through generic visibility"),
		MaterialInstance != nullptr &&
		FBlueprintHelperGeneratedCapabilityRegistry::IsDescriptorAgentVisible(*MaterialInstance, RuntimeState));
	TestTrue(TEXT("MaterialInstance descriptor is active after Task 5 closure"),
		MaterialInstance != nullptr &&
		FString(MaterialInstance->RuntimeStatus).Equals(TEXT("active"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("MaterialInstance descriptor is no longer reserved-only after Task 5 closure"),
		MaterialInstance != nullptr &&
		MaterialInstance->bReservedOnly);
	TestTrue(TEXT("MaterialInstance runtime adapter is registered independently of descriptor visibility"),
		RuntimeState.RegisteredRuntimeAdapterIds.Contains(TEXT("material_instance_runtime_adapter")));

	const FBlueprintHelperGeneratedCapabilityDescriptor* StructFields =
		FBlueprintHelperGeneratedCapabilityRegistry::FindById(TEXT("struct.fields.edit"));
	TestTrue(TEXT("Struct descriptor exists"), StructFields != nullptr);
	TestTrue(TEXT("Struct descriptor stays planned until runtime adapter is implemented"),
		StructFields != nullptr &&
		FString(StructFields->RuntimeStatus).Equals(TEXT("planned"), ESearchCase::IgnoreCase));
	TestTrue(TEXT("Struct descriptor stays reserved until runtime adapter is implemented"),
		StructFields != nullptr &&
		StructFields->bReservedOnly);
	TestFalse(TEXT("Struct descriptor remains hidden when no UE runtime adapter is registered"),
		StructFields != nullptr &&
		FBlueprintHelperGeneratedCapabilityRegistry::IsDescriptorAgentVisible(*StructFields, RuntimeState));
	TestFalse(TEXT("Struct runtime adapter is not synthesized from active descriptor status"),
		RuntimeState.RegisteredRuntimeAdapterIds.Contains(TEXT("struct_runtime_adapter")));
	return true;
}

#endif
