#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
static FBlueprintHelperActionResolutionRequest MakeGenericActionRequest(
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& TypeName = FString())
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.TypeName = TypeName;
	return Request;
}

static bool ScanGenericActionResolutionSourceForForbiddenToken(
	FAutomationTestBase& Test,
	const FString& SourceRoot,
	const FString& Token)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.cpp"), true, false);

	bool bClean = true;
	for (const FString& File : Files)
	{
		if (File.EndsWith(TEXT("BlueprintHelperGenericAssetStructControlActionClusterTests.cpp")))
		{
			continue;
		}

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}

		if (Text.Contains(Token))
		{
			Test.AddError(FString::Printf(TEXT("Forbidden generic ActionResolution token '%s' found in %s"), *Token, *File));
			bClean = false;
		}
	}
	return bClean;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericActionProviderBoundaryMatrixTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.ProviderBoundaryMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericActionProviderBoundaryMatrixTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperGenericActionProviderBoundary ConstructNeedsContext =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Construct));
	TestEqual(TEXT("Construct without type needs context"), ConstructNeedsContext.Mode, EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext);
	TestEqual(TEXT("Construct builder seam"), ConstructNeedsContext.RequiredBuilder, FString(TEXT("ConstructFragmentBuilder")));

	const FBlueprintHelperGenericActionProviderBoundary ConstructCandidate =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Construct, TEXT("Vector")));
	TestEqual(TEXT("Construct with type can query NodeSpawner"), ConstructCandidate.Mode, EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate);

	const FBlueprintHelperGenericActionProviderBoundary DeconstructNeedsContext =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Deconstruct));
	TestEqual(TEXT("Deconstruct without type needs context"), DeconstructNeedsContext.Mode, EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext);

	const FBlueprintHelperGenericActionProviderBoundary DeconstructCandidate =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Deconstruct, TEXT("Transform")));
	TestEqual(TEXT("Deconstruct with type can query NodeSpawner"), DeconstructCandidate.Mode, EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate);

	const FBlueprintHelperGenericActionProviderBoundary SelectBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Select));
	TestEqual(TEXT("Select requires dedicated builder"), SelectBoundary.Mode, EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired);
	TestEqual(TEXT("Select builder seam"), SelectBoundary.RequiredBuilder, FString(TEXT("SelectFragmentBuilder")));
	TestTrue(TEXT("Select reason names ActionDatabase limit"), SelectBoundary.Reason.Contains(TEXT("ActionDatabase")));

	const FBlueprintHelperGenericActionProviderBoundary ControlBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Control));
	TestEqual(TEXT("Control requires dedicated builder"), ControlBoundary.Mode, EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired);
	TestEqual(TEXT("Control builder seam"), ControlBoundary.RequiredBuilder, FString(TEXT("ControlFragmentBuilder")));
	TestTrue(TEXT("Control reason names ActionDatabase limit"), ControlBoundary.Reason.Contains(TEXT("ActionDatabase")));

	const FBlueprintHelperGenericActionProviderBoundary UnsupportedBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Call));
	TestEqual(TEXT("Call is outside generic provider boundary"), UnsupportedBoundary.Mode, EBlueprintHelperGenericActionProviderMode::Unsupported);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericActionClusterBoundaryResultTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.ClusterBoundaryResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericActionClusterBoundaryResultTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperActionResolutionResult ConstructNeedsContext =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Construct));
	TestEqual(TEXT("Construct without TypeName is invalid request"), ConstructNeedsContext.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("Construct without TypeName needs context"), ConstructNeedsContext.ErrorCode, FString(TEXT("needs_more_semantic_context")));

	const FBlueprintHelperActionResolutionResult ConstructCandidate =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Construct, TEXT("Vector")));
	TestTrue(
		TEXT("Construct with TypeName no longer returns migration/not-found placeholder"),
		ConstructCandidate.ErrorCode != (FString(TEXT("generic_action_node_spawner_candidate_")) + FString(TEXT("not_found"))));

	const FBlueprintHelperActionResolutionResult DeconstructCandidate =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Deconstruct, TEXT("Transform")));
	TestTrue(
		TEXT("Deconstruct with TypeName no longer returns migration/not-found placeholder"),
		DeconstructCandidate.ErrorCode != (FString(TEXT("generic_action_node_spawner_candidate_")) + FString(TEXT("not_found"))));

	const FBlueprintHelperActionResolutionResult SelectBlocked =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Select));
	TestEqual(TEXT("Select is blocked until dedicated builder"), SelectBlocked.Status, EBlueprintHelperActionResolutionStatus::Blocked);
	TestEqual(TEXT("Select blocked by dedicated builder seam"), SelectBlocked.ErrorCode, FString(TEXT("dedicated_fragment_builder_required")));
	TestTrue(TEXT("Select blocked reason names ActionDatabase limit"), SelectBlocked.Message.Contains(TEXT("ActionDatabase")));
	TestTrue(TEXT("Select blocked reason names builder"), SelectBlocked.Message.Contains(TEXT("SelectFragmentBuilder")));

	const FBlueprintHelperActionResolutionResult ControlBlocked =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(EBlueprintHelperActionSemanticKind::Control));
	TestEqual(TEXT("Control is blocked until dedicated builder"), ControlBlocked.Status, EBlueprintHelperActionResolutionStatus::Blocked);
	TestEqual(TEXT("Control blocked by dedicated builder seam"), ControlBlocked.ErrorCode, FString(TEXT("dedicated_fragment_builder_required")));
	TestTrue(TEXT("Control blocked reason names ActionDatabase limit"), ControlBlocked.Message.Contains(TEXT("ActionDatabase")));
	TestTrue(TEXT("Control blocked reason names builder"), ControlBlocked.Message.Contains(TEXT("ControlFragmentBuilder")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericActionResolutionSourceHygieneTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.SourceHygiene",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericActionResolutionSourceHygieneTest::RunTest(const FString& Parameters)
{
	const FString ActionResolutionPrivateRoot = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"),
		TEXT("Private"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"));
	const FString ActionResolutionPublicRoot = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"),
		TEXT("Public"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"));

	TestTrue(TEXT("private ActionResolution root exists"), IFileManager::Get().DirectoryExists(*ActionResolutionPrivateRoot));
	TestTrue(TEXT("public ActionResolution root exists"), IFileManager::Get().DirectoryExists(*ActionResolutionPublicRoot));

	const TArray<FString> ForbiddenTokens = {
		FString(TEXT("generic_asset_struct_control_action_cluster_")) + FString(TEXT("migration_pending")),
		FString(TEXT("NewObject<")) + FString(TEXT("UK2Node")),
		FString(TEXT("Node")) + FString(TEXT("Handler"))
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		bClean &= ScanGenericActionResolutionSourceForForbiddenToken(*this, ActionResolutionPrivateRoot, Token);
		bClean &= ScanGenericActionResolutionSourceForForbiddenToken(*this, ActionResolutionPublicRoot, Token);
	}

	TestTrue(TEXT("Generic ActionResolution source has no migration marker, direct UK2Node shortcut, or old NodeHandler"), bClean);
	return true;
}

#endif
