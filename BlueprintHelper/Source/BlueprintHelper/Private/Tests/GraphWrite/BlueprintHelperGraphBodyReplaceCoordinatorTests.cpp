#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReplaceServiceNoGraphTypeProtectionBranchesTest,
	"BlueprintHelper.GraphWrite.GraphBodyReplaceCoordinator.NoGraphTypeBranchesInReplaceService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReplaceServiceNoGraphTypeProtectionBranchesTest::RunTest(const FString&)
{
	const FString SourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp"));
	FString Source;
	TestTrue(TEXT("replace service source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));

	const TArray<FString> ForbiddenTokens =
	{
		TEXT("UK2Node_FunctionResult"),
		TEXT("AddFunctionResultsReachedByImportedExecFlow"),
		TEXT("CanDeferEntryResolvedConnectivityFailure"),
		TEXT("Request.Scope == EBlueprintHelperReplaceScope::FunctionBody")
	};
	for (const FString& Token : ForbiddenTokens)
	{
		TestFalse(
			FString::Printf(TEXT("replace service does not contain %s"), *Token),
			Source.Contains(Token));
	}
	return true;
}

#endif
