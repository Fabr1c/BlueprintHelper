#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyBoundaryModelFamilyMatrixTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.FamilyMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyBoundaryModelFamilyMatrixTest::RunTest(const FString&)
{
	const TArray<FBlueprintHelperGraphBodyAdapterDescriptor> Descriptors =
		FBlueprintHelperGraphBodyAdapterRegistry::GetKnownDescriptors();

	const TArray<FString> ExpectedRuntimeAdapters =
	{
		TEXT("append_blueprint_graph"),
		TEXT("replace_blueprint_graph"),
		TEXT("patch_blueprint_graph"),
		TEXT("merge_blueprint_graph"),
		TEXT("merge_external_flow"),
		TEXT("patch_external_graph"),
		TEXT("replace_external_body")
	};
	const TArray<FString> ExpectedTaskSpecStrategies =
	{
		TEXT("append_new_owned_graph"),
		TEXT("replace_owned_graph"),
		TEXT("patch_owned_graph"),
		TEXT("merge_owned_graph"),
		TEXT("merge_external_flow"),
		TEXT("patch_external_graph"),
		TEXT("replace_external_body")
	};

	for (const FString& RuntimeAdapterId : ExpectedRuntimeAdapters)
	{
		FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
		TestTrue(
			FString::Printf(TEXT("runtime adapter is registered: %s"), *RuntimeAdapterId),
			FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(RuntimeAdapterId, Descriptor));
		TestFalse(TEXT("runtime adapter is not reserved-only"), Descriptor.bReservedOnly);
	}

	for (const FString& Strategy : ExpectedTaskSpecStrategies)
	{
		FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
		TestTrue(
			FString::Printf(TEXT("TaskSpec strategy is registered: %s"), *Strategy),
			FBlueprintHelperGraphBodyAdapterRegistry::TryFindByTaskSpecStrategy(Strategy, Descriptor));
		TestFalse(TEXT("TaskSpec strategy is not reserved-only"), Descriptor.bReservedOnly);
	}

	bool bFoundReservedMacro = false;
	for (const FBlueprintHelperGraphBodyAdapterDescriptor& Descriptor : Descriptors)
	{
		if (Descriptor.BodyKind == EBlueprintHelperGraphBodyKind::ReservedMacroBody)
		{
			bFoundReservedMacro = true;
			TestTrue(TEXT("macro descriptor is reserved-only"), Descriptor.bReservedOnly);
			TestTrue(TEXT("macro descriptor does not expose a write route"), Descriptor.TaskSpecStrategy.IsEmpty());
		}
	}
	TestTrue(TEXT("reserved macro body is represented"), bFoundReservedMacro);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyBoundaryModelIdentityTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyBoundaryModelIdentityTest::RunTest(const FString&)
{
	FBlueprintHelperGraphBodyBoundaryModel Model;
	Model.RuntimeAdapterId = TEXT("replace_blueprint_graph");
	Model.TaskSpecStrategy = TEXT("replace_owned_graph");
	Model.TargetAssetPath = TEXT("/Game/BP_Door.BP_Door");
	Model.GraphName = TEXT("OpenDoor");
	Model.OwnedBlockId = TEXT("DoorBlock");
	Model.BodyKind = EBlueprintHelperGraphBodyKind::K2FunctionBody;

	TestEqual(
		TEXT("BodyKindToString returns stable function token"),
		FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(Model.BodyKind),
		FString(TEXT("k2.function_body")));
	TestEqual(
		TEXT("BodyKindFromString parses stable function token"),
		FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindFromString(TEXT("k2.function_body")),
		EBlueprintHelperGraphBodyKind::K2FunctionBody);
	TestEqual(
		TEXT("body identity is stable"),
		FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Model),
		FString(TEXT("/Game/BP_Door.BP_Door|OpenDoor|k2.function_body|DoorBlock")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphConnectivityPolicyProjectionTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.ConnectivityPolicyProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphConnectivityPolicyProjectionTest::RunTest(const FString&)
{
	FBlueprintHelperGraphBodyBoundaryModel FunctionBoundary;
	FunctionBoundary.BodyKind = EBlueprintHelperGraphBodyKind::K2FunctionBody;
	FunctionBoundary.ExitNodeRefs.Add(TEXT("FunctionResult"));
	const FBlueprintHelperGraphConnectivityPolicy FunctionPolicy =
		FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(FunctionBoundary);
	TestTrue(TEXT("function body allows FunctionResult boundary"), FunctionPolicy.bAllowFunctionResultBoundary);
	TestFalse(TEXT("function body does not imply external anchors"), FunctionPolicy.bAllowExternalAnchorBoundary);

	FBlueprintHelperGraphBodyBoundaryModel OwnedBlockBoundary;
	OwnedBlockBoundary.BodyKind = EBlueprintHelperGraphBodyKind::K2BlockImplementation;
	OwnedBlockBoundary.OwnedBlockId = TEXT("OwnedBlock");
	OwnedBlockBoundary.ConnectivityExceptionCodes.Add(TEXT("unreachable_exec_node"));
	const FBlueprintHelperGraphConnectivityPolicy OwnedPolicy =
		FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(OwnedBlockBoundary);
	TestTrue(TEXT("owned block allows disconnected preview"), OwnedPolicy.bAllowOwnedBlockDisconnectedPreview);
	TestTrue(TEXT("owned block preserves exception code"), OwnedPolicy.ViolationCodes.Contains(TEXT("unreachable_exec_node")));

	FBlueprintHelperGraphBodyBoundaryModel ExternalBoundary;
	ExternalBoundary.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;
	ExternalBoundary.ExternalAnchorRefs.Add(TEXT("anchor:exec"));
	const FBlueprintHelperGraphConnectivityPolicy ExternalPolicy =
		FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(ExternalBoundary);
	TestTrue(TEXT("external body allows external anchor boundary"), ExternalPolicy.bAllowExternalAnchorBoundary);
	return true;
}

#endif
