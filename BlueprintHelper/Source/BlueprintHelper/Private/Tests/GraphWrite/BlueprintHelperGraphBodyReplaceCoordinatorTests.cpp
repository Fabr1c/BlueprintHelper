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
		TEXT("Request.Scope == EBlueprintHelperReplaceScope::FunctionBody"),
		TEXT("PreflightGraphTarget("),
		TEXT("ResolveReplaceTarget("),
		TEXT("ReconnectPreservedEntryToNewBody("),
		TEXT("CanAcceptBoundaryConnectivityDiagnostics("),
		TEXT("ResolveBlockImplementation(")
	};
	for (const FString& Token : ForbiddenTokens)
	{
		TestFalse(
			FString::Printf(TEXT("replace service does not contain %s"), *Token),
			Source.Contains(Token));
	}

	const FString CoordinatorPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReplaceCoordinator.cpp"));
	FString CoordinatorSource;
	TestTrue(TEXT("replace coordinator source loads"), FFileHelper::LoadFileToString(CoordinatorSource, *CoordinatorPath));

	const TArray<FString> ForbiddenCoordinatorTokens =
	{
		TEXT("K2Node_FunctionResult.h"),
		TEXT("UK2Node_FunctionResult"),
		TEXT("AddReachedResultBoundaries"),
		TEXT("BodyKindForReplaceScope(Scope) != EBlueprintHelperGraphBodyKind::K2FunctionBody"),
		TEXT("Material"),
		TEXT("Animation")
	};
	for (const FString& Token : ForbiddenCoordinatorTokens)
	{
		TestFalse(
			FString::Printf(TEXT("replace coordinator does not contain %s"), *Token),
			CoordinatorSource.Contains(Token));
	}
	return true;
}

#endif
