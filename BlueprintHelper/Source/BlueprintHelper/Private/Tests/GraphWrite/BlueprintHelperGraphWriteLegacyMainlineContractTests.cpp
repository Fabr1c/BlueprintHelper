#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"

namespace BlueprintHelperGraphWriteLegacyMainlineContractTests
{
static FString SourceRoot()
{
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"));
}

static bool LoadSource(FAutomationTestBase& Test, const FString& RelativePath, FString& OutText)
{
	const FString Path = FPaths::Combine(SourceRoot(), RelativePath);
	if (!IFileManager::Get().FileExists(*Path))
	{
		Test.AddError(FString::Printf(TEXT("Required source file is missing: %s"), *Path));
		return false;
	}
	if (!FFileHelper::LoadFileToString(OutText, *Path))
	{
		Test.AddError(FString::Printf(TEXT("Required source file could not be read: %s"), *Path));
		return false;
	}
	return true;
}

static bool AssertNoTokens(
	FAutomationTestBase& Test,
	const FString& RelativePath,
	const TArray<FString>& ForbiddenTokens)
{
	FString Source;
	if (!LoadSource(Test, RelativePath, Source))
	{
		return false;
	}

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		if (Source.Contains(Token))
		{
			Test.AddError(FString::Printf(TEXT("%s must not contain legacy mainline token: %s"), *RelativePath, *Token));
			bClean = false;
		}
	}
	return bClean;
}

static bool AssertMissingOrNoTokens(
	FAutomationTestBase& Test,
	const FString& RelativePath,
	const TArray<FString>& ForbiddenTokens)
{
	const FString Path = FPaths::Combine(SourceRoot(), RelativePath);
	if (!IFileManager::Get().FileExists(*Path))
	{
		return true;
	}
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *Path))
	{
		Test.AddError(FString::Printf(TEXT("Optional source file exists but could not be read: %s"), *Path));
		return false;
	}

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		if (Source.Contains(Token))
		{
			Test.AddError(FString::Printf(TEXT("%s must not expose legacy public token: %s"), *RelativePath, *Token));
			bClean = false;
		}
	}
	return bClean;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMergePatchUseMutationCoordinatorContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.MergePatchUseMutationCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMergePatchUseMutationCoordinatorContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	bool bClean = true;
	bClean &= AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp"),
		{
			TEXT("CreateMergeCallFunctionNode"),
			TEXT("Schema->TryCreateConnection"),
			TEXT("BreakLinkTo"),
			TEXT("ApplyAppendAfter("),
			TEXT("ApplyInsertBetween("),
			TEXT("ApplyBranchFork(")
		});
	bClean &= AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp"),
		{
			TEXT("ApplySetPinDefault("),
			TEXT("ApplyConnectPins("),
			TEXT("ApplyDisconnectLink("),
			TEXT("ApplyReplaceLink("),
			TEXT("BreakLinkTo"),
			TEXT("TryCreateConnection")
		});
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSelectControlUseGenericClusterContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.SelectControlUseGenericCluster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSelectControlUseGenericClusterContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	bool bClean = true;
	bClean &= AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.cpp"),
		{
			TEXT("UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass())"),
			TEXT("dedicated_select_node_spawner")
		});
	bClean &= AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp"),
		{
			TEXT("UBlueprintNodeSpawner::Create(NodeClass)"),
			TEXT("ActionResult.Status = EBlueprintHelperActionResolutionStatus::Resolved"),
			TEXT("RequireDedicatedControlBuilderBoundary")
		});
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSemanticBuilderNoParsedNodeInputContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.SemanticBuilderNoParsedNodeInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSemanticBuilderNoParsedNodeInputContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	bool bClean = true;
	bClean &= AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp"),
		{
			TEXT("const FParsedNode& NodeData"),
			TEXT("FParsedNode NodeData"),
			TEXT("FParsedNode BoundNodeData")
		});
	bClean &= AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp"),
		{
			TEXT("FParsedNode NodeData"),
			TEXT("FParsedNode BoundNodeData")
		});
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperPublicLegacyGraphWriteApiContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.NoPublicParsedNodeGraphWriteApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperPublicLegacyGraphWriteApiContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	bool bClean = true;
	bClean &= AssertMissingOrNoTokens(
		*this,
		TEXT("Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"),
		{
			TEXT("SpawnMacroNode("),
			TEXT("const FParsedNode& NodeData"),
			TEXT("FParsedNode NodeData"),
			TEXT("struct FParsedNode"),
			TEXT("struct FParsedLink"),
			TEXT("struct FParsedMacroReference")
		});
	bClean &= AssertMissingOrNoTokens(
		*this,
		TEXT("Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"),
		{
			TEXT("FParsedMacroReference"),
			TEXT("FParsedNode")
		});
	bClean &= AssertMissingOrNoTokens(
		*this,
		TEXT("Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"),
		{
			TEXT("FParsedLink"),
			TEXT("FParsedNode")
		});
	bClean &= AssertMissingOrNoTokens(
		*this,
		TEXT("Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"),
		{
			TEXT("FParsedNode"),
			TEXT("SpawnMacroNode(")
		});
	bClean &= AssertMissingOrNoTokens(
		*this,
		TEXT("Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h"),
		{
			TEXT("FBlueprintGraphMutationPlan"),
			TEXT("FParsedNode")
		});
	bClean &= AssertMissingOrNoTokens(
		*this,
		TEXT("Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"),
		{
			TEXT("FParsedNode"),
			TEXT("FParsedLink"),
			TEXT("FParsedMacroReference")
		});
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperEventDelegateDeclaredCapabilityContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.EventDelegateDeclaredCapabilityMatchesSuccessPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperEventDelegateDeclaredCapabilityContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	return AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp"),
		{
			TEXT("EBlueprintHelperActionSemanticKind::ComponentBoundEvent"),
			TEXT("EBlueprintHelperActionSemanticKind::Bind")
		});
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperResolvableExpressionsDoNotUsePlaceholderPathContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.ResolvableExpressionsDoNotUsePlaceholderPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperResolvableExpressionsDoNotUsePlaceholderPathContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	return AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp"),
		{
			TEXT("TEXT(\"expr_call\")"),
			TEXT("TEXT(\"expr_op\")"),
			TEXT("TEXT(\"expr_construct\")"),
			TEXT("TEXT(\"expr_deconstruct\")")
		});
}

#endif
