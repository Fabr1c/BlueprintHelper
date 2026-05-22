#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace BlueprintHelperFourClusterContextConsumptionTests
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

static FString SourcePath(const FString& RelativePath)
{
	return FPaths::Combine(SourceRoot(), RelativePath);
}

static bool LoadSource(FAutomationTestBase& Test, const FString& RelativePath, FString& OutText)
{
	const FString Path = SourcePath(RelativePath);
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClustersDoNotBuildActionContextPipelineTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Clusters.DoNotBuildActionContextPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClustersDoNotBuildActionContextPipelineTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Files = {
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp")
	};

	const TArray<FString> ForbiddenTokens = {
		TEXT("BlueprintHelperActionContextDemandCollector"),
		TEXT("BlueprintHelperActionContextSnapshotBuilder"),
		TEXT("BlueprintHelperActionContextInferenceService"),
		TEXT("BlueprintHelperActionContextBundleProjector"),
		TEXT("BlueprintHelperActionContextBuildService"),
		TEXT("FBlueprintHelperActionContextScope::Build"),
		TEXT("BuildSync("),
		TEXT("BuildAsyncFromSnapshot(")
	};

	bool bClean = true;
	for (const FString& File : Files)
	{
		FString Source;
		if (!BlueprintHelperFourClusterContextConsumptionTests::LoadSource(*this, File, Source))
		{
			bClean = false;
			continue;
		}
		for (const FString& Token : ForbiddenTokens)
		{
			if (Source.Contains(Token))
			{
				AddError(FString::Printf(TEXT("%s must not rebuild ActionContextPipeline; found %s"), *File, *Token));
				bClean = false;
			}
		}
	}

	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperClustersUseContextViewTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Clusters.UseContextView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperClustersUseContextViewTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Files = {
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.cpp")
	};

	bool bComplete = true;
	for (const FString& File : Files)
	{
		FString Source;
		if (!BlueprintHelperFourClusterContextConsumptionTests::LoadSource(*this, File, Source))
		{
			bComplete = false;
			continue;
		}
		if (!Source.Contains(TEXT("FBlueprintHelperActionClusterContextView")))
		{
			AddError(FString::Printf(TEXT("%s must consume FBlueprintHelperActionClusterContextView"), *File));
			bComplete = false;
		}
	}

	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperNoExecuteRePreviewPathTest,
	"BlueprintHelper.GraphWrite.ActionResolution.NoExecuteRePreviewPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperNoExecuteRePreviewPathTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Files = {
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.cpp"),
		TEXT("Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp")
	};

	const TArray<FString> ForbiddenTokens = {
		TEXT("allow_execute_repreview"),
		TEXT("execute_repreview"),
		TEXT("repreview_on_execute"),
		TEXT("RunPreviewFromExecute"),
		TEXT("PreviewFallback")
	};

	bool bClean = true;
	for (const FString& File : Files)
	{
		FString Source;
		if (!BlueprintHelperFourClusterContextConsumptionTests::LoadSource(*this, File, Source))
		{
			bClean = false;
			continue;
		}
		for (const FString& Token : ForbiddenTokens)
		{
			if (Source.Contains(Token))
			{
				AddError(FString::Printf(TEXT("%s must not contain execute re-preview path token %s"), *File, *Token));
				bClean = false;
			}
		}
	}

	return bClean;
}

#endif
