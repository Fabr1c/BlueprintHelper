#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/Utils/GraphWritePipelineUtils.h"

#include <type_traits>

template <typename T, typename = void>
struct TBlueprintHelperHasNullablePipelineGenerate : std::false_type
{
};

template <typename T>
struct TBlueprintHelperHasNullablePipelineGenerate<T, std::void_t<decltype(static_cast<FBlueprintGenerateResult(*)(
	UEdGraph*,
	const FString&,
	TArray<TSharedPtr<FUnresolvedNodeItem>>&,
	const FBlueprintGraphWriteConnectivityValidationInput*)>(&T::GenerateBlueprintFromJson))>> : std::true_type
{
};

template <typename T, typename = void>
struct TBlueprintHelperHasNullablePipelineUtilsGenerate : std::false_type
{
};

template <typename T>
struct TBlueprintHelperHasNullablePipelineUtilsGenerate<T, std::void_t<decltype(static_cast<FBlueprintGenerateResult(*)(
	UEdGraph*,
	const TSharedPtr<FJsonObject>&,
	TArray<TSharedPtr<FUnresolvedNodeItem>>&,
	const FBlueprintGraphWriteConnectivityValidationInput*)>(&T::GenerateSemanticGraphFromJsonObject))>> : std::true_type
{
};

template <typename T, typename = void>
struct TBlueprintHelperHasRequiredPipelineGenerate : std::false_type
{
};

template <typename T>
struct TBlueprintHelperHasRequiredPipelineGenerate<T, std::void_t<decltype(static_cast<FBlueprintGenerateResult(*)(
	UEdGraph*,
	const FString&,
	TArray<TSharedPtr<FUnresolvedNodeItem>>&,
	const FBlueprintGraphWriteConnectivityValidationInput&)>(&T::GenerateBlueprintFromJson))>> : std::true_type
{
};

template <typename T, typename = void>
struct TBlueprintHelperHasRequiredPipelineUtilsGenerate : std::false_type
{
};

template <typename T>
struct TBlueprintHelperHasRequiredPipelineUtilsGenerate<T, std::void_t<decltype(static_cast<FBlueprintGenerateResult(*)(
	UEdGraph*,
	const TSharedPtr<FJsonObject>&,
	TArray<TSharedPtr<FUnresolvedNodeItem>>&,
	const FBlueprintGraphWriteConnectivityValidationInput&)>(&T::GenerateSemanticGraphFromJsonObject))>> : std::true_type
{
};

class FBlueprintHelperGraphBodyBoundaryModelTestFileUtils
{
public:
	static void AddCppAndHeaderFiles(const FString& RelativeDirectory, TArray<FString>& OutFiles)
	{
		const FString AbsoluteDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper"),
			RelativeDirectory);
		IFileManager::Get().FindFilesRecursive(OutFiles, *AbsoluteDirectory, TEXT("*.cpp"), true, false);
		IFileManager::Get().FindFilesRecursive(OutFiles, *AbsoluteDirectory, TEXT("*.h"), true, false);
	}

	static TArray<FString> GenericGraphWriteBoundaryScanFiles()
	{
		TArray<FString> Files;
		AddCppAndHeaderFiles(
			TEXT("Private/Systems/ToolClusters/GraphWrite/Pipeline"),
			Files);
		AddCppAndHeaderFiles(
			TEXT("Public/Systems/ToolClusters/GraphWrite/Pipeline"),
			Files);
		AddCppAndHeaderFiles(
			TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement"),
			Files);
		return Files;
	}

	static TArray<FString> GenericBoundaryForbiddenTokens()
	{
		const FString TextPrefix = FString(TEXT("TEXT(\""));
		return {
			FString(TEXT("FindOrAdd(")) + TextPrefix + FString(TEXT("Function")) + TEXT("Result\")"),
			TextPrefix + FString(TEXT("Tunnel")) + TEXT("Entry\")"),
			TextPrefix + FString(TEXT("Tunnel")) + TEXT("Exit\")")
		};
	}
};

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
		TEXT("k2.external_body"),
		TEXT("k2.external_graph.replace_body")
	};
	const TArray<FString> ExpectedTaskSpecStrategies =
	{
		TEXT("append_new_owned_graph"),
		TEXT("replace_owned_graph"),
		TEXT("patch_owned_graph"),
		TEXT("merge_external_flow"),
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
	FBlueprintHelperGraphBodyGeneratedRouteSyncTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.GeneratedRouteSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphBodyGeneratedRouteSyncTest::RunTest(const FString&)
{
	const TArray<FBlueprintHelperGraphWriteRouteSyncValidationIssue> Issues =
		FBlueprintHelperGraphBodyAdapterRegistry::ValidateGeneratedRouteSync();

	for (const FBlueprintHelperGraphWriteRouteSyncValidationIssue& Issue : Issues)
	{
		AddError(FString::Printf(
			TEXT("Route sync issue: route_id=%s runtime_adapter_id=%s status=%s code=%s message=%s"),
			*Issue.RouteId,
			*Issue.RuntimeAdapterId,
			*Issue.Status,
			*Issue.Code,
			*Issue.Message));
	}

	TestEqual(TEXT("generated routes declare runtime sync semantics and active routes resolve to UE adapters"), Issues.Num(), 0);
	return Issues.Num() == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePipelineNoConnectivityFallbackTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.NoConnectivityFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePipelineNoConnectivityFallbackTest::RunTest(const FString&)
{
	const FString PipelinePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/Utils/GraphWritePipelineUtils.cpp"));
	FString PipelineSource;
	TestTrue(TEXT("pipeline utils source loads"), FFileHelper::LoadFileToString(PipelineSource, *PipelinePath));

	const TArray<FString> ForbiddenTokens =
	{
		TEXT("k2.semantic_graph_generation"),
		TEXT("GetEntryRootNodes()"),
		TEXT("TopLevelFlow.Entries"),
		TEXT("CollectAllowedTerminalPureDataNodes("),
		TEXT("if (AdapterConnectivityInput)"),
		TEXT("if (!AdapterConnectivityInput)")
	};
	for (const FString& Token : ForbiddenTokens)
	{
		TestFalse(
			FString::Printf(TEXT("pipeline source no longer contains connectivity fallback token %s"), *Token),
			PipelineSource.Contains(Token));
	}

	const FString ReplaceServicePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp"));
	FString ReplaceServiceSource;
	TestTrue(TEXT("replace service source loads"), FFileHelper::LoadFileToString(ReplaceServiceSource, *ReplaceServicePath));
	TestTrue(
		TEXT("replace service builds adapter connectivity input"),
		ReplaceServiceSource.Contains(TEXT("BuildAdapterConnectivityInput(ReplacePlan)")));
	TestTrue(
		TEXT("replace service passes required adapter connectivity input into pipeline"),
		ReplaceServiceSource.Contains(TEXT("AdapterConnectivityInput);")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteGenericPipelineNoBoundaryIdentityMappingTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.GenericPipelineNoBoundaryIdentityMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteGenericPipelineNoBoundaryIdentityMappingTest::RunTest(const FString&)
{
	const TArray<FString> Files =
		FBlueprintHelperGraphBodyBoundaryModelTestFileUtils::GenericGraphWriteBoundaryScanFiles();
	TestTrue(TEXT("generic GraphWrite source files are discovered"), Files.Num() > 0);

	const TArray<FString> ForbiddenTokens =
		FBlueprintHelperGraphBodyBoundaryModelTestFileUtils::GenericBoundaryForbiddenTokens();
	for (const FString& File : Files)
	{
		FString Source;
		if (!TestTrue(FString::Printf(TEXT("generic GraphWrite source loads: %s"), *File), FFileHelper::LoadFileToString(Source, *File)))
		{
			continue;
		}
		for (const FString& Token : ForbiddenTokens)
		{
			TestFalse(
				FString::Printf(TEXT("generic source does not publish adapter boundary token %s in %s"), *Token, *File),
				Source.Contains(Token));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWritePipelineConnectivityInputContractTest,
	"BlueprintHelper.GraphWrite.GraphBodyBoundaryModel.ConnectivityInputContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWritePipelineConnectivityInputContractTest::RunTest(const FString&)
{
	TestTrue(
		TEXT("generation pipeline exposes required connectivity input contract"),
		TBlueprintHelperHasRequiredPipelineGenerate<FBlueprintGraphGenerationPipeline>::value);
	TestFalse(
		TEXT("generation pipeline does not expose nullable connectivity input overload"),
		TBlueprintHelperHasNullablePipelineGenerate<FBlueprintGraphGenerationPipeline>::value);
	TestTrue(
		TEXT("pipeline utils exposes required connectivity input contract"),
		TBlueprintHelperHasRequiredPipelineUtilsGenerate<UGraphWritePipelineUtils>::value);
	TestFalse(
		TEXT("pipeline utils does not expose nullable connectivity input overload"),
		TBlueprintHelperHasNullablePipelineUtilsGenerate<UGraphWritePipelineUtils>::value);
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
	TestTrue(TEXT("function body allows exit boundary reachability"), FunctionPolicy.bAllowExitBoundaryReachability);
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
