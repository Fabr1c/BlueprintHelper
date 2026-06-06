#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperExternalBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2BlockBodyAdapter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyK2BlockAdapterProjectionTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.K2BlockProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyK2BlockAdapterProjectionTest::RunTest(const FString&)
{
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	TestTrue(
		TEXT("patch descriptor is registered"),
		FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(TEXT("patch_blueprint_graph"), Descriptor));

	FBlueprintHelperK2BlockBodyAdapter Adapter(Descriptor);
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Door.BP_Door"));
	Target->SetStringField(TEXT("graph"), TEXT("ExecuteUbergraph"));
	Payload->SetObjectField(TEXT("target"), Target);
	TSharedRef<FJsonObject> PatchedRef = MakeShared<FJsonObject>();
	PatchedRef->SetStringField(TEXT("block_id"), TEXT("DoorBlock"));
	PatchedRef->SetStringField(TEXT("node_ref"), TEXT("NodeA"));
	Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);
	Payload->SetArrayField(
		TEXT("connectivity_exception_codes"),
		{MakeShared<FJsonValueString>(TEXT("unreachable_exec_node"))});

	const FBlueprintHelperGraphBodyMutationPlan Plan = Adapter.BuildMutationPlan(Payload);
	TestEqual(TEXT("adapter id comes from descriptor"), Adapter.GetAdapterId(), FString(TEXT("patch_blueprint_graph")));
	TestEqual(TEXT("boundary body kind is block implementation"), Plan.BoundaryModel.BodyKind, EBlueprintHelperGraphBodyKind::K2BlockImplementation);
	TestEqual(TEXT("boundary block id comes from patched_ref"), Plan.BoundaryModel.OwnedBlockId, FString(TEXT("DoorBlock")));
	TestTrue(TEXT("adapter does not create nodes"), !Plan.bCreatesNodesInsideAdapter);
	TestTrue(TEXT("connectivity policy preserves owned preview exception"), Plan.ConnectivityPolicy.bAllowOwnedBlockDisconnectedPreview);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyExternalAdapterProjectionTest,
	"BlueprintHelper.GraphWrite.GraphBodyAdapter.ExternalProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyExternalAdapterProjectionTest::RunTest(const FString&)
{
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	TestTrue(
		TEXT("external descriptor is registered"),
		FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(TEXT("merge_external_flow"), Descriptor));

	FBlueprintHelperExternalBodyAdapter Adapter(Descriptor);
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/BP_Door.BP_Door"));
	Target->SetStringField(TEXT("graph"), TEXT("ExecuteUbergraph"));
	Target->SetStringField(TEXT("external_anchor_ref"), TEXT("anchor:then"));
	Payload->SetObjectField(TEXT("target"), Target);

	const FBlueprintHelperGraphBodyMutationPlan Plan = Adapter.BuildMutationPlan(Payload);
	TestEqual(TEXT("external adapter id comes from descriptor"), Adapter.GetAdapterId(), FString(TEXT("merge_external_flow")));
	TestEqual(TEXT("external body kind is distinct"), Plan.BoundaryModel.BodyKind, EBlueprintHelperGraphBodyKind::K2ExternalBody);
	TestTrue(TEXT("external anchor is projected"), Plan.BoundaryModel.ExternalAnchorRefs.Contains(TEXT("anchor:then")));
	TestTrue(TEXT("external policy allows external anchors"), Plan.ConnectivityPolicy.bAllowExternalAnchorBoundary);
	TestTrue(TEXT("external adapter does not create nodes"), !Plan.bCreatesNodesInsideAdapter);
	return true;
}

#endif
