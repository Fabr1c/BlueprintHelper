#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphBodyBoundaryModelDesignVocabularyTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.DesignVocabulary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyBoundaryModelDesignVocabularyTest::RunTest(const FString&)
{
	FBlueprintHelperGraphBodyBoundaryModel Model;
	Model.GraphFamily = TEXT("k2");
	Model.BodyKind = EBlueprintHelperGraphBodyKind::K2MacroBody;
	Model.EntryNodeRefs.Add(TEXT("TunnelEntry"));
	Model.ExitNodeRefs.Add(TEXT("TunnelExit"));
	Model.ProtectedNodeRefs.Append({TEXT("TunnelEntry"), TEXT("TunnelExit")});
	Model.DeletableNodeRefs.Add(TEXT("BodyNode"));
	Model.GeneratedNodeRefs.Add(TEXT("GeneratedPrint"));
	Model.ImportedBodyNodeRefs.Add(TEXT("ImportedPrint"));
	Model.ReachableBodyFlowNodeRefs.Add(TEXT("GeneratedPrint"));
	Model.ExternalAnchorRefs.Add(TEXT("MacroBoundaryAnchor"));
	Model.SemanticSourceRefs.Append({TEXT("TunnelEntry.Execute"), TEXT("TunnelExit.Then")});
	Model.ConnectivityExceptionCodes.Add(TEXT("comment_node"));
	Model.PureDataConsumptionPolicy = EBlueprintHelperGraphBodyPureDataPolicy::RequireReachableExecConsumer;
	Model.AllowedIsolatedNodePolicy = EBlueprintHelperGraphBodyIsolatedNodePolicy::CommentsAndReroutesOnly;

	TestEqual(
		TEXT("macro body token"),
		FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(Model.BodyKind),
		FString(TEXT("k2.macro_body")));
	TestTrue(TEXT("entry boundary present"), Model.EntryNodeRefs.Contains(TEXT("TunnelEntry")));
	TestTrue(TEXT("exit boundary present"), Model.ExitNodeRefs.Contains(TEXT("TunnelExit")));
	TestTrue(TEXT("deletable node present"), Model.DeletableNodeRefs.Contains(TEXT("BodyNode")));
	TestTrue(TEXT("semantic source present"), Model.SemanticSourceRefs.Contains(TEXT("TunnelEntry.Execute")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperConnectivityValidatorConsumesBoundaryPolicyTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidator.ConsumesBoundaryModelPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperConnectivityValidatorConsumesBoundaryPolicyTest::RunTest(const FString&)
{
	const FString HeaderPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h"));
	FString Header;
	TestTrue(TEXT("validator header loads"), FFileHelper::LoadFileToString(Header, *HeaderPath));
	TestTrue(TEXT("input contains boundary model"), Header.Contains(TEXT("FBlueprintHelperGraphBodyBoundaryModel BoundaryModel")));
	TestTrue(TEXT("input contains connectivity policy"), Header.Contains(TEXT("FBlueprintHelperGraphConnectivityPolicy ConnectivityPolicy")));
	TestFalse(TEXT("validator does not expose raw entry roots as the contract"), Header.Contains(TEXT("TSet<UEdGraphNode*> EntryRootNodes")));
	return true;
}

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
		TEXT("k2.custom_event_body"),
		TEXT("k2.event_body"),
		TEXT("k2.function_body"),
		TEXT("k2.macro_body"),
		TEXT("k2.block_implementation"),
		TEXT("k2.external_body")
	};
	const TArray<FString> ExpectedTaskSpecStrategies =
	{
		TEXT("append_new_owned_graph"),
		TEXT("replace_owned_graph"),
		TEXT("patch_owned_graph"),
		TEXT("merge_external_flow")
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

	bool bFoundReservedMaterialFunction = false;
	bool bFoundReservedAnimationGraph = false;
	for (const FBlueprintHelperGraphBodyAdapterDescriptor& Descriptor : Descriptors)
	{
		if (Descriptor.BodyKind == EBlueprintHelperGraphBodyKind::ReservedMaterialFunctionBody)
		{
			bFoundReservedMaterialFunction = true;
			TestTrue(TEXT("material function descriptor is reserved-only"), Descriptor.bReservedOnly);
			TestTrue(TEXT("material function descriptor does not expose a write route"), Descriptor.TaskSpecStrategy.IsEmpty());
		}
		else if (Descriptor.BodyKind == EBlueprintHelperGraphBodyKind::ReservedAnimationGraphBody)
		{
			bFoundReservedAnimationGraph = true;
			TestTrue(TEXT("animation graph descriptor is reserved-only"), Descriptor.bReservedOnly);
			TestTrue(TEXT("animation graph descriptor does not expose a write route"), Descriptor.TaskSpecStrategy.IsEmpty());
		}
	}
	TestTrue(TEXT("reserved material function body is represented"), bFoundReservedMaterialFunction);
	TestTrue(TEXT("reserved animation graph body is represented"), bFoundReservedAnimationGraph);
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
