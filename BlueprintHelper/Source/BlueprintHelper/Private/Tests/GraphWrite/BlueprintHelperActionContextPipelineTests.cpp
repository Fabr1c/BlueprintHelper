#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
static FString BuildBlueprintHelperSourceRoot()
{
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"));
}

static FString BuildActionContextPublicPath(const TCHAR* FileName)
{
	return FPaths::Combine(
		BuildBlueprintHelperSourceRoot(),
		TEXT("Public"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"),
		TEXT("Context"),
		FileName);
}

static FString BuildActionContextPrivatePath(const TCHAR* FileName)
{
	return FPaths::Combine(
		BuildBlueprintHelperSourceRoot(),
		TEXT("Private"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"),
		TEXT("Context"),
		FileName);
}

static bool LoadRequiredSourceFile(FAutomationTestBase& Test, const FString& FilePath, FString& OutText)
{
	if (!IFileManager::Get().FileExists(*FilePath))
	{
		Test.AddError(FString::Printf(TEXT("Required ActionContext source file is missing: %s"), *FilePath));
		return false;
	}

	if (!FFileHelper::LoadFileToString(OutText, *FilePath))
	{
		Test.AddError(FString::Printf(TEXT("Required ActionContext source file could not be read: %s"), *FilePath));
		return false;
	}

	return true;
}

static bool RequireTokens(
	FAutomationTestBase& Test,
	const FString& SourceText,
	const FString& FilePath,
	const TArray<FString>& RequiredTokens)
{
	bool bComplete = true;
	for (const FString& Token : RequiredTokens)
	{
		if (!SourceText.Contains(Token))
		{
			Test.AddError(FString::Printf(TEXT("ActionContext contract token '%s' missing from %s"), *Token, *FilePath));
			bComplete = false;
		}
	}
	return bComplete;
}

static bool CollectActionContextSourceFiles(FAutomationTestBase& Test, TArray<FString>& OutFiles)
{
	const FString PublicContextRoot = FPaths::GetPath(BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextTypes.h")));
	const FString PrivateContextRoot = FPaths::GetPath(BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextInferenceService.cpp")));

	const bool bPublicContextExists = IFileManager::Get().DirectoryExists(*PublicContextRoot);
	const bool bPrivateContextExists = IFileManager::Get().DirectoryExists(*PrivateContextRoot);

	if (!bPublicContextExists)
	{
		Test.AddError(FString::Printf(TEXT("ActionContext public source directory is missing: %s"), *PublicContextRoot));
	}

	if (!bPrivateContextExists)
	{
		Test.AddError(FString::Printf(TEXT("ActionContext private source directory is missing: %s"), *PrivateContextRoot));
	}

	if (bPublicContextExists)
	{
		IFileManager::Get().FindFilesRecursive(OutFiles, *PublicContextRoot, TEXT("*.h"), true, false);
		IFileManager::Get().FindFilesRecursive(OutFiles, *PublicContextRoot, TEXT("*.cpp"), true, false);
	}

	if (bPrivateContextExists)
	{
		IFileManager::Get().FindFilesRecursive(OutFiles, *PrivateContextRoot, TEXT("*.h"), true, false);
		IFileManager::Get().FindFilesRecursive(OutFiles, *PrivateContextRoot, TEXT("*.cpp"), true, false);
	}

	return bPublicContextExists && bPrivateContextExists;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextDtoSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.DTO.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextDtoSourceContractTest::RunTest(const FString& Parameters)
{
	const FString FilePath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextTypes.h"));

	FString SourceText;
	if (!LoadRequiredSourceFile(*this, FilePath, SourceText))
	{
		return false;
	}

	const TArray<FString> RequiredTokens = {
		TEXT("enum class EBlueprintHelperActionContextDemandKind"),
		TEXT("enum class EBlueprintHelperActionContextSourceThread"),
		TEXT("GameThreadSnapshot"),
		TEXT("WorkerInference"),
		TEXT("struct FBlueprintHelperActionContextRevisionToken"),
		TEXT("bool IsCompatibleWith"),
		TEXT("struct FBlueprintHelperActionContextDemand"),
		TEXT("TSet<EBlueprintHelperActionContextDemandKind> RequiredKinds"),
		TEXT("TMap<FString, FString> DefaultValues"),
		TEXT("ArgumentPinTypes"),
		TEXT("BindingObjectPath"),
		TEXT("struct FBlueprintHelperActionContextSnapshot"),
		TEXT("struct FBlueprintHelperResolvedActionContextBundle"),
		TEXT("FindByStatementId")
	};

	const bool bComplete = RequireTokens(*this, SourceText, FilePath, RequiredTokens);
	TestTrue(TEXT("ActionContext DTO source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextScopeSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.Scope.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextScopeSourceContractTest::RunTest(const FString& Parameters)
{
	const FString HeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextScope.h"));
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextScope.cpp"));
	const FString BuildServiceHeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextBuildService.h"));
	const FString BuildServiceSourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextBuildService.cpp"));

	FString HeaderText;
	FString SourceText;
	FString BuildServiceHeaderText;
	FString BuildServiceSourceText;
	if (!LoadRequiredSourceFile(*this, HeaderPath, HeaderText)
		|| !LoadRequiredSourceFile(*this, SourcePath, SourceText)
		|| !LoadRequiredSourceFile(*this, BuildServiceHeaderPath, BuildServiceHeaderText)
		|| !LoadRequiredSourceFile(*this, BuildServiceSourcePath, BuildServiceSourceText))
	{
		return false;
	}

	bool bComplete = true;
	bComplete &= RequireTokens(
		*this,
		HeaderText,
		HeaderPath,
		{
			TEXT("class BLUEPRINTHELPER_API FBlueprintHelperActionContextScope"),
			TEXT("static bool Build"),
			TEXT("TryBuildRequest"),
			TEXT("GetBundle")
		});
	bComplete &= RequireTokens(
		*this,
		SourceText,
		SourcePath,
		{
			TEXT("FBlueprintHelperActionContextSnapshotBuilder::BuildSnapshot"),
			TEXT("FBlueprintHelperActionContextInferenceService::Infer"),
			TEXT("FBlueprintHelperActionContextBundleProjector::TryBuildRequest")
		});
	bComplete &= RequireTokens(
		*this,
		BuildServiceHeaderText,
		BuildServiceHeaderPath,
		{
			TEXT("BuildSync"),
			TEXT("BuildAsyncFromSnapshot"),
			TEXT("FBuildComplete")
		});
	bComplete &= RequireTokens(
		*this,
		BuildServiceSourceText,
		BuildServiceSourcePath,
		{
			TEXT("EAsyncExecution::ThreadPool"),
			TEXT("ENamedThreads::GameThread"),
			TEXT("FBlueprintHelperActionContextInferenceService::Infer")
		});

	TestTrue(TEXT("ActionContext scope/build service source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextRevisionGuardSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.RevisionGuard.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextRevisionGuardSourceContractTest::RunTest(const FString& Parameters)
{
	const FString HeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextRevisionGuard.h"));
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextRevisionGuard.cpp"));

	FString HeaderText;
	FString SourceText;
	if (!LoadRequiredSourceFile(*this, HeaderPath, HeaderText) || !LoadRequiredSourceFile(*this, SourcePath, SourceText))
	{
		return false;
	}

	bool bComplete = true;
	bComplete &= RequireTokens(
		*this,
		HeaderText,
		HeaderPath,
		{
			TEXT("class BLUEPRINTHELPER_API FBlueprintHelperActionContextRevisionGuard"),
			TEXT("static bool Validate"),
			TEXT("FBlueprintHelperActionContextRevisionToken")
		});
	bComplete &= RequireTokens(
		*this,
		SourceText,
		SourcePath,
		{
			TEXT("Expected.IsCompatibleWith(Current)"),
			TEXT("action_context_stale"),
			TEXT("OutError")
		});

	TestTrue(TEXT("ActionContext revision guard source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextBundleProjectionSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.Projection.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextBundleProjectionSourceContractTest::RunTest(const FString& Parameters)
{
	const FString HeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextBundleProjector.h"));
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextBundleProjector.cpp"));

	FString HeaderText;
	FString SourceText;
	if (!LoadRequiredSourceFile(*this, HeaderPath, HeaderText) || !LoadRequiredSourceFile(*this, SourcePath, SourceText))
	{
		return false;
	}

	bool bComplete = true;
	bComplete &= RequireTokens(
		*this,
		HeaderText,
		HeaderPath,
		{
			TEXT("class BLUEPRINTHELPER_API FBlueprintHelperActionContextBundleProjector"),
			TEXT("static bool TryBuildRequest"),
			TEXT("FBlueprintHelperResolvedActionContextBundle"),
			TEXT("FBlueprintHelperActionResolutionRequest")
		});
	bComplete &= RequireTokens(
		*this,
		SourceText,
		SourcePath,
		{
			TEXT("Bundle.FindByStatementId"),
			TEXT("action_context_not_found"),
			TEXT("action_context_missing_blueprint_or_graph"),
			TEXT("OutRequest.ClusterKind"),
			TEXT("OutRequest.Blueprint"),
			TEXT("OutRequest.TargetGraph"),
			TEXT("OutRequest.Semantic")
		});

	TestTrue(TEXT("ActionContext bundle projection source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextWorkerInferenceSourceHygieneTest,
	"BlueprintHelper.GraphWrite.ActionContext.SourceHygiene.WorkerInferencePureDto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextWorkerInferenceSourceHygieneTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextInferenceService.cpp"));

	FString SourceText;
	if (!LoadRequiredSourceFile(*this, SourcePath, SourceText))
	{
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		TEXT("UObject*"),
		TEXT("UBlueprint*"),
		TEXT("UEdGraph*"),
		TEXT("UEdGraphPin*"),
		TEXT("FindObject"),
		TEXT("LoadObject"),
		TEXT("GetSchema")
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		if (SourceText.Contains(Token))
		{
			AddError(FString::Printf(
				TEXT("Worker inference must not access UObject or UE graph APIs; forbidden token '%s' found in %s"),
				*Token,
				*SourcePath));
			bClean = false;
		}
	}

	TestTrue(TEXT("Worker inference remains pure DTO source"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSettingsHardcodedSourceHygieneTest,
	"BlueprintHelper.GraphWrite.ActionContext.SourceHygiene.SettingsDefaultsNotHardcoded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSettingsHardcodedSourceHygieneTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	const bool bHasContextRoots = CollectActionContextSourceFiles(*this, Files);
	if (!bHasContextRoots)
	{
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		TEXT("settings_default"),
		TEXT("SearchMode = TEXT("),
		TEXT("AmbiguityPolicy = TEXT("),
		TEXT("MaxCandidates = "),
		TEXT("CandidateLimit = ")
	};

	bool bClean = true;
	for (const FString& File : Files)
	{
		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *File))
		{
			continue;
		}

		for (const FString& Token : ForbiddenTokens)
		{
			if (SourceText.Contains(Token))
			{
				AddError(FString::Printf(
					TEXT("ActionContext policy/default values must come from the unified settings runtime boundary; forbidden token '%s' found in %s"),
					*Token,
					*File));
				bClean = false;
			}
		}
	}

	TestTrue(TEXT("ActionContext source avoids hardcoded settings defaults"), bClean);
	return bClean;
}

#endif
