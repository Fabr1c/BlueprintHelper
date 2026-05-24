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
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper")));
}

static const TCHAR* ForbiddenControlFallbackTokens[] = {
	TEXT("manual_control_context"),
	TEXT("manual_control_semantic"),
	TEXT("RequireDedicatedControlBuilderBoundary")
};

static const TCHAR* ForbiddenParsedNodeMainlineTokens[] = {
	TEXT("const FParsedNode& NodeData"),
	TEXT("FParsedNode NodeData"),
	TEXT("FParsedNode BoundNodeData"),
	TEXT("parsed_node_plan_unsupported")
};

static const TCHAR* ForbiddenWideSurfaceFallbackTokens[] = {
	TEXT("CreateMergeCallFunctionNode"),
	TEXT("call_function.name"),
	TEXT("set_member_variable"),
	TEXT("make_struct"),
	TEXT("compare"),
	TEXT("ref")
};

static const TCHAR* ForbiddenSingletonControlDirectSpawnTokens[] = {
	TEXT("SpawnK2Node<"),
	TEXT("NewObject<UK2Node"),
	TEXT("UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass())"),
	TEXT("UBlueprintNodeSpawner::Create(NodeClass)"),
	TEXT("UK2Node_ExecutionSequence::StaticClass()"),
	TEXT("UK2Node_IfThenElse::StaticClass()")
};

static TArray<FString> ActiveGraphWriteSourceRoots()
{
	return {
		FPaths::Combine(SourceRoot(), TEXT("Private/Systems/ToolClusters/GraphWrite")),
		FPaths::Combine(SourceRoot(), TEXT("Public/Systems/ToolClusters/GraphWrite"))
	};
}

static bool IsSourceFilePath(const FString& FilePath)
{
	return FilePath.EndsWith(TEXT(".h"), ESearchCase::IgnoreCase)
		|| FilePath.EndsWith(TEXT(".cpp"), ESearchCase::IgnoreCase);
}

class FGraphWriteSourceFileVisitor final : public IPlatformFile::FDirectoryVisitor
{
public:
	explicit FGraphWriteSourceFileVisitor(TArray<FString>& InOutFiles)
		: OutFiles(InOutFiles)
	{
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory && IsSourceFilePath(FilenameOrDirectory))
		{
			OutFiles.Add(FilenameOrDirectory);
		}
		return true;
	}

private:
	TArray<FString>& OutFiles;
};

static bool IsAllowedDiagnosticOnlyMatch(const FString& RelativePath, const FString& Token)
{
	if (Token == TEXT("parsed_node_plan_unsupported"))
	{
		return RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlanExecutor.cpp"));
	}
	if (Token == TEXT("call_function.name"))
	{
		return RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp"))
			|| RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.cpp"));
	}
	if (Token == TEXT("make_struct"))
	{
		return RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp"))
			|| RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.cpp"));
	}
	if (Token == TEXT("compare"))
	{
		return RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.cpp"));
	}
	if (Token == TEXT("ref"))
	{
		return RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.cpp"))
			|| RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp"))
			|| RelativePath.EndsWith(TEXT("Private/Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.cpp"));
	}
	return false;
}

static bool ContainsForbiddenToken(const FString& Source, const FString& Token)
{
	if (Token == TEXT("ref") || Token == TEXT("compare"))
	{
		return Source.Contains(FString::Printf(TEXT("TEXT(\"%s\")"), *Token))
			|| Source.Contains(FString::Printf(TEXT("\"%s\""), *Token))
			|| Source.Contains(FString::Printf(TEXT("'%s'"), *Token));
	}
	return Source.Contains(Token);
}

template <int32 TokenCount>
static bool AssertActiveGraphWriteSourceHasNoTokens(
	FAutomationTestBase& Test,
	const TCHAR* GroupName,
	const TCHAR* const (&ForbiddenTokens)[TokenCount])
{
	bool bClean = true;
	TArray<FString> SourceFiles;
	FGraphWriteSourceFileVisitor Visitor(SourceFiles);
	for (const FString& Root : ActiveGraphWriteSourceRoots())
	{
		IFileManager::Get().IterateDirectoryRecursively(*Root, Visitor);
	}

	int32 ScannedFileCount = 0;
	for (const FString& FilePath : SourceFiles)
	{
		if (!IsSourceFilePath(FilePath))
		{
			continue;
		}

		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *FilePath))
		{
			Test.AddError(FString::Printf(TEXT("Active GraphWrite source file could not be read: %s"), *FilePath));
			bClean = false;
			continue;
		}

		FString RelativePath = FilePath;
		FPaths::MakePathRelativeTo(RelativePath, *SourceRoot());
		RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		++ScannedFileCount;
		for (const TCHAR* TokenText : ForbiddenTokens)
		{
			const FString Token(TokenText);
			if (ContainsForbiddenToken(Source, Token) && !IsAllowedDiagnosticOnlyMatch(RelativePath, Token))
			{
				Test.AddError(FString::Printf(
					TEXT("%s contains forbidden %s token: %s"),
					*RelativePath,
					GroupName,
					*Token));
				bClean = false;
			}
		}
	}
	if (ScannedFileCount == 0)
	{
		Test.AddError(TEXT("Active GraphWrite source scan did not find any source files."));
		bClean = false;
	}
	return bClean;
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

template <int32 TokenCount>
static bool AssertNoSingletonControlDirectSpawnTokens(
	FAutomationTestBase& Test,
	const FString& RelativePath,
	const TCHAR* const (&ForbiddenTokens)[TokenCount])
{
	FString Source;
	if (!LoadSource(Test, RelativePath, Source))
	{
		return false;
	}

	bool bClean = true;
	for (const TCHAR* TokenText : ForbiddenTokens)
	{
		const FString Token(TokenText);
		if (Source.Contains(Token))
		{
			Test.AddError(FString::Printf(
				TEXT("%s must route singleton control direct spawn through BlueprintHelperSingletonControlFlowEvidenceProvider.cpp; forbidden token: %s"),
				*RelativePath,
				*Token));
			bClean = false;
		}
	}
	return bClean;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActiveGraphWriteSourceLegacyTokenGateContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.ActiveGraphWriteSourceLegacyTokenGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActiveGraphWriteSourceLegacyTokenGateContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	bool bClean = true;
	bClean &= AssertActiveGraphWriteSourceHasNoTokens(*this, TEXT("control fallback"), ForbiddenControlFallbackTokens);
	bClean &= AssertActiveGraphWriteSourceHasNoTokens(*this, TEXT("parsed-node mainline"), ForbiddenParsedNodeMainlineTokens);
	bClean &= AssertActiveGraphWriteSourceHasNoTokens(*this, TEXT("wide-surface fallback"), ForbiddenWideSurfaceFallbackTokens);
	return bClean;
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
			TEXT("manual_control_context"),
			TEXT("manual_control_semantic"),
			TEXT("RequireDedicatedControlBuilderBoundary")
	});
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSingletonControlDirectSpawnProviderBoundaryContractTest,
	"BlueprintHelper.GraphWrite.LegacyMainline.SingletonControlDirectSpawnProviderBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSingletonControlDirectSpawnProviderBoundaryContractTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphWriteLegacyMainlineContractTests;
	bool bClean = true;
	bClean &= AssertNoSingletonControlDirectSpawnTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp"),
		ForbiddenSingletonControlDirectSpawnTokens);
	bClean &= AssertNoSingletonControlDirectSpawnTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.cpp"),
		ForbiddenSingletonControlDirectSpawnTokens);
	bClean &= AssertNoSingletonControlDirectSpawnTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp"),
		ForbiddenSingletonControlDirectSpawnTokens);
	bClean &= AssertNoTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp"),
		{
			TEXT("Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Control"),
			TEXT("Request.Semantic.Query = TEXT(\"sequence\")"),
			TEXT("mutation_branch_fork_sequence_projected_context"),
			TEXT("mutation_branch_fork_sequence_semantic_constraints")
		});
	bClean &= AssertNoSingletonControlDirectSpawnTokens(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.cpp"),
		ForbiddenSingletonControlDirectSpawnTokens);

	FString ProviderSource;
	if (LoadSource(
		*this,
		TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.cpp"),
		ProviderSource))
	{
		TestTrue(
			TEXT("provider owns singleton control direct spawn"),
			ProviderSource.Contains(TEXT("UBlueprintNodeSpawner::Create(NodeClass)")));
	}
	else
	{
		bClean = false;
	}

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
			TEXT("enum class EParsedBlueprintNodeType"),
			TEXT("struct FParsedPinType"),
			TEXT("struct FParsedLocalVariableDeclaration"),
			TEXT("FParsedVariableReference"),
			TEXT("FParsedEventReference"),
			TEXT("GenerateBlueprintFromJson("),
			TEXT("GenerateMultiGraphFromJson("),
			TEXT("EnsureLocalVariableExists("),
			TEXT("ConvertToEdGraphPinType("),
			TEXT("SpawnMacroNode("),
			TEXT("const FParsedNode& NodeData"),
			TEXT("FParsedNode NodeData"),
			TEXT("struct FParsedNode"),
			TEXT("struct FParsedLink"),
			TEXT("struct FParsedMacroReference")
		});
	bClean &= AssertMissingOrNoTokens(
		*this,
		TEXT("Public/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"),
		{
			TEXT("FParsedPinType"),
			TEXT("FParsedLocalVariableDeclaration"),
			TEXT("FParsedVariableReference")
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
	const FString RelativePath = TEXT("Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp");
	FString Source;
	if (!LoadSource(*this, RelativePath, Source))
	{
		return false;
	}

	bool bClean = true;
	const TArray<FString> RequiredTokens = {
		TEXT("EBlueprintHelperActionSemanticKind::ComponentBoundEvent"),
		TEXT("EBlueprintHelperActionSemanticKind::Delegate"),
		TEXT("delegate_operation"),
		TEXT("bind"),
		TEXT("missing_required_evidence"),
		TEXT("ue_bound_event_node_spawner"),
		TEXT("ue_delegate_node_spawner"),
		TEXT("ue_delegate_manual_assign_factory")
	};
	for (const FString& Token : RequiredTokens)
	{
		if (!Source.Contains(Token))
		{
			AddError(FString::Printf(TEXT("%s must declare current P5 event/delegate boundary token: %s"), *RelativePath, *Token));
			bClean = false;
		}
	}

	bClean &= AssertNoTokens(
		*this,
		RelativePath,
		{
			TEXT("legacy_fallback"),
			TEXT("fake_delegate_success"),
			TEXT("fake component-bound delegate success")
		});
	TestTrue(TEXT("EventDelegateActionCluster owns P5 semantics without fake delegate success"), bClean);
	return bClean;
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
