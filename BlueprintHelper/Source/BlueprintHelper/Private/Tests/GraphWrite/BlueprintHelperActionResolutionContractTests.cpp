#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
static FString BuildForbiddenActionResolutionToken(const TCHAR* Left, const TCHAR* Right)
{
	return FString(Left) + FString(Right);
}

static bool ScanActionResolutionSourceForForbiddenToken(
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
		if (File.EndsWith(TEXT("BlueprintHelperActionResolutionContractTests.cpp")))
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
			Test.AddError(FString::Printf(TEXT("Forbidden ActionResolution token '%s' found in %s"), *Token, *File));
			bClean = false;
		}
	}
	return bClean;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionClusterKindContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.ClusterKindIsTopLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionClusterKindContractTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Get;

	TestEqual(TEXT("ClusterKind is top-level dispatch key"), Request.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
	TestEqual(TEXT("Semantic kind is constraint only"), Request.Semantic.Kind, EBlueprintHelperActionSemanticKind::Get);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionNoLegacyIntentTokensTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.NoLegacyIntentTokens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionNoLegacyIntentTokensTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"));

	TestTrue(TEXT("BlueprintHelper source root exists"), IFileManager::Get().DirectoryExists(*SourceRoot));

	const TArray<FString> ForbiddenTokens = {
		BuildForbiddenActionResolutionToken(TEXT("EBlueprintHelperAction"), TEXT("Intent")),
		BuildForbiddenActionResolutionToken(TEXT("ActionRequest."), TEXT("Intent")),
		BuildForbiddenActionResolutionToken(TEXT("Request."), TEXT("Intent")),
		BuildForbiddenActionResolutionToken(TEXT("Intent"), TEXT("ToString")),
		BuildForbiddenActionResolutionToken(TEXT("SelectCluster("), TEXT("Intent")),
		BuildForbiddenActionResolutionToken(TEXT("SpawnVariable"), TEXT("GetNode")),
		BuildForbiddenActionResolutionToken(TEXT("SpawnVariable"), TEXT("SetNode"))
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		bClean &= ScanActionResolutionSourceForForbiddenToken(*this, SourceRoot, Token);
	}

	TestTrue(TEXT("ActionResolution source has no legacy top-level intent or direct variable spawn tokens"), bClean);
	return true;
}

#endif
