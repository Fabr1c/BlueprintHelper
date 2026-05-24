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

static FString BuildGraphWritePrivateSourcePath(const TCHAR* Area, const TCHAR* FileName)
{
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"),
		TEXT("Private"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		Area,
		FileName);
}

static bool IsAllowedActionContextPipelineImplementationFile(const FString& SourcePath)
{
	FString Normalized = FPaths::ConvertRelativePathToFull(SourcePath);
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Normalized.Contains(TEXT("/GraphWrite/ActionResolution/Context/"))
		|| Normalized.Contains(TEXT("/Context/BlueprintHelperActionContext"));
}

static FString MakeNormalizedRelativeSourcePath(const FString& SourceRoot, const FString& File)
{
	FString RelativePath = File;
	FPaths::MakePathRelativeTo(RelativePath, *SourceRoot);
	RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
	return RelativePath;
}

static bool ScanActionResolutionClusterFilesForForbiddenPipelineToken(
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
		if (IsAllowedActionContextPipelineImplementationFile(File))
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
			Test.AddError(FString::Printf(
				TEXT("ActionResolution clusters must consume request context, not rebuild the context pipeline; forbidden token '%s' found in %s"),
				*Token,
				*File));
			bClean = false;
		}
	}
	return bClean;
}

static bool ScanSpecificGraphWriteSourcesForForbiddenToken(
	FAutomationTestBase& Test,
	const TArray<FString>& SourcePaths,
	const FString& Token)
{
	bool bClean = true;
	for (const FString& SourcePath : SourcePaths)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *SourcePath))
		{
			Test.AddError(FString::Printf(TEXT("GraphWrite source could not be read: %s"), *SourcePath));
			bClean = false;
			continue;
		}
		if (Text.Contains(Token))
		{
			Test.AddError(FString::Printf(
				TEXT("GraphWrite/EventDelegate must not own declaration/signature mutation and must keep delegate suboperations as second-level semantics; forbidden token '%s' found in %s"),
				*Token,
				*SourcePath));
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
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Field;
	Request.Semantic.FieldOperation = TEXT("get");
	Request.Semantic.FieldScope = TEXT("variable");

	TestEqual(TEXT("ClusterKind is top-level dispatch key"), Request.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
	TestEqual(TEXT("Semantic kind is constraint only"), Request.Semantic.Kind, EBlueprintHelperActionSemanticKind::Field);
	TestEqual(TEXT("Field operation is second-level constraint"), Request.Semantic.FieldOperation, FString(TEXT("get")));
	TestEqual(TEXT("Field scope is second-level constraint"), Request.Semantic.FieldScope, FString(TEXT("variable")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionGenericCreateSecondLevelContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.GenericCreateUsesSecondLevelOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionGenericCreateSecondLevelContractTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Create;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Create;
	Request.Semantic.CreateOperation = TEXT("spawn_actor");

	TestEqual(TEXT("Create stays under GenericAssetStructControlAction cluster"), Request.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("Create is the first-stage semantic constraint"), Request.Semantic.Kind, EBlueprintHelperActionSemanticKind::Create);
	TestEqual(TEXT("Create family is explicit"), Request.Semantic.SemanticFamily, EBlueprintHelperActionSemanticFamily::Create);
	TestEqual(TEXT("Create operation is second-level evidence"), Request.Semantic.CreateOperation, FString(TEXT("spawn_actor")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionGraphStatementProjectionContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.GraphStatementUsesActionContextProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionGraphStatementProjectionContractTest::RunTest(const FString& Parameters)
{
	const FString GraphStatementBuilderPath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperGraphStatementBuilder.cpp"));

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *GraphStatementBuilderPath))
	{
		AddError(FString::Printf(TEXT("GraphStatementBuilder source could not be read: %s"), *GraphStatementBuilderPath));
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		BuildForbiddenActionResolutionToken(TEXT("ActionRequest."), TEXT("ClusterKind =")),
		BuildForbiddenActionResolutionToken(TEXT("ActionRequest."), TEXT("Semantic =")),
		TEXT("BuildSingleActionContextDemand("),
		TEXT("ResolveSpawnerClusterForSemanticKind("),
		TEXT("Demand.ClusterKind ="),
		TEXT("Demand.SemanticKind ="),
		TEXT("ContextDemands.Add(BuildSingleActionContextDemand(")
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		if (Text.Contains(Token))
		{
			AddError(FString::Printf(
				TEXT("GraphStatementBuilder must not own local ActionContext demand construction or semantic-to-cluster projection; forbidden token '%s' found in %s"),
				*Token,
				*GraphStatementBuilderPath));
			bClean = false;
		}
	}

	TestTrue(TEXT("GraphStatementBuilder projects ActionResolutionRequest from ActionContextBundle"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionClustersConsumeProjectedContextContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.ClustersConsumeProjectedContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionClustersConsumeProjectedContextContractTest::RunTest(const FString& Parameters)
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

	if (!IFileManager::Get().DirectoryExists(*ActionResolutionPrivateRoot))
	{
		AddError(FString::Printf(TEXT("ActionResolution private source root is missing: %s"), *ActionResolutionPrivateRoot));
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		TEXT("CollectFromStatements("),
		TEXT("BuildSnapshot("),
		TEXT("Infer("),
		TEXT("TryBuildRequest("),
		TEXT("UnsupportedClusterMigration"),
		TEXT("migration_pending"),
		TEXT("unsupported_cluster_migration"),
		TEXT("legacy_fallback"),
		BuildForbiddenActionResolutionToken(TEXT("ActionRequest."), TEXT("Semantic =")),
		BuildForbiddenActionResolutionToken(TEXT("ActionRequest."), TEXT("ClusterKind ="))
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		bClean &= ScanActionResolutionClusterFilesForForbiddenPipelineToken(*this, ActionResolutionPrivateRoot, Token);
	}

	TestTrue(TEXT("ActionResolution clusters consume projected context without rebuilding the pipeline"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionEventDelegateUseSiteBoundaryContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.EventDelegateUseSiteBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionEventDelegateUseSiteBoundaryContractTest::RunTest(const FString& Parameters)
{
	const TArray<FString> SourcePaths = {
		BuildGraphWritePrivateSourcePath(TEXT("ActionResolution"), TEXT("BlueprintHelperEventDelegateActionCluster.cpp")),
		BuildGraphWritePrivateSourcePath(TEXT("ActionResolution"), TEXT("BlueprintHelperEventDelegateUseSiteEvidence.cpp")),
		BuildGraphWritePrivateSourcePath(TEXT("GraphStatement"), TEXT("BlueprintHelperEventDelegateFragmentBuilder.cpp"))
	};

	const TArray<FString> ForbiddenTokens = {
		TEXT("ensure_function"),
		TEXT("ensure_custom_event"),
		TEXT("ensure_event_dispatcher"),
		TEXT("ensure_override_event"),
		TEXT("BlueprintSignatureService"),
		TEXT("SignatureTaskPlanAdapter"),
		TEXT("EBlueprintHelperActionSemanticKind::Assign"),
		TEXT("EBlueprintHelperActionSemanticKind::Unbind"),
		TEXT("EBlueprintHelperActionSemanticKind::DelegateCall"),
		TEXT("EBlueprintHelperActionSemanticKind::DelegateClear"),
		TEXT("EBlueprintHelperGraphStatementKind::Assign"),
		TEXT("EBlueprintHelperGraphStatementKind::Unbind"),
		TEXT("EBlueprintHelperGraphStatementKind::DelegateCall"),
		TEXT("EBlueprintHelperGraphStatementKind::DelegateClear")
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		bClean &= ScanSpecificGraphWriteSourcesForForbiddenToken(*this, SourcePaths, Token);
	}

	FString ActionClusterSource;
	if (FFileHelper::LoadFileToString(ActionClusterSource, *SourcePaths[0]))
	{
		bClean &= TestTrue(TEXT("EventDelegate uses ComponentBoundEvent first-stage semantic"), ActionClusterSource.Contains(TEXT("EBlueprintHelperActionSemanticKind::ComponentBoundEvent")));
		bClean &= TestTrue(TEXT("EventDelegate uses Delegate first-stage semantic"), ActionClusterSource.Contains(TEXT("EBlueprintHelperActionSemanticKind::Delegate")));
		bClean &= TestTrue(TEXT("EventDelegate uses delegate_operation evidence"), ActionClusterSource.Contains(TEXT("DelegateOperation")));
		bClean &= TestTrue(TEXT("EventDelegate assign uses manual factory candidate"), ActionClusterSource.Contains(TEXT("ue_delegate_manual_assign_factory")));
	}
	else
	{
		AddError(FString::Printf(TEXT("EventDelegate action cluster source could not be read: %s"), *SourcePaths[0]));
		bClean = false;
	}

	TestTrue(TEXT("EventDelegate GraphWrite boundary stays use-site only"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionFunctionSemanticResolverBoundaryContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionSemanticResolverBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionFunctionSemanticResolverBoundaryContractTest::RunTest(const FString& Parameters)
{
	const FString FunctionClusterSourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperFunctionActionCluster.cpp"));
	const FString GenericClusterSourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperGenericAssetStructControlActionCluster.cpp"));

	FString FunctionClusterSource;
	FString GenericClusterSource;
	bool bClean = true;
	if (!FFileHelper::LoadFileToString(FunctionClusterSource, *FunctionClusterSourcePath))
	{
		AddError(FString::Printf(TEXT("FunctionActionCluster source could not be read: %s"), *FunctionClusterSourcePath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(GenericClusterSource, *GenericClusterSourcePath))
	{
		AddError(FString::Printf(TEXT("GenericAssetStructControlActionCluster source could not be read: %s"), *GenericClusterSourcePath));
		bClean = false;
	}
	if (!bClean)
	{
		return false;
	}

	bClean &= TestTrue(
		TEXT("FunctionActionCluster routes Convert/Schedule through the reusable function semantic resolver"),
		FunctionClusterSource.Contains(TEXT("BlueprintHelperFunctionSemanticActionResolver"))
		&& FunctionClusterSource.Contains(TEXT("IsSupportedSemanticKind"))
		&& FunctionClusterSource.Contains(TEXT("FBlueprintHelperFunctionSemanticActionResolver::Resolve")));

	const TArray<FString> GenericForbiddenTokens = {
		TEXT("convert_function"),
		TEXT("schedule_function"),
		TEXT("latent_or_async_function"),
		TEXT("FunctionSemanticActionResolver"),
		TEXT("FBlueprintHelperCallFunctionResolver::Resolve")
	};
	for (const FString& Token : GenericForbiddenTokens)
	{
		if (GenericClusterSource.Contains(Token))
		{
			AddError(FString::Printf(
				TEXT("GenericAssetStructControlActionCluster must not become the fallback success path for Convert/Schedule; forbidden token '%s' found in %s"),
				*Token,
				*GenericClusterSourcePath));
			bClean = false;
		}
	}

	TestTrue(TEXT("Convert/Schedule stay owned by FunctionAction semantic resolver boundary"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteDelegatePublicInternalBoundaryContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.DelegatePublicInternalBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteDelegatePublicInternalBoundaryContractTest::RunTest(const FString& Parameters)
{
	const FString PythonCompilerPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("AgentFaceService"),
		TEXT("task-core"),
		TEXT("python"),
		TEXT("blueprinthelper_task"),
		TEXT("compiler"),
		TEXT("graph_write_append.py"));
	const FString GraphSemanticIRSourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperGraphSemanticIR.cpp"));
	const FString GraphSemanticIRUtilsSourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement/Utils"),
		TEXT("BlueprintHelperGraphSemanticIRUtils.cpp"));

	FString PythonCompilerSource;
	FString GraphSemanticIRSource;
	FString GraphSemanticIRUtilsSource;
	bool bClean = true;
	if (!FFileHelper::LoadFileToString(PythonCompilerSource, *PythonCompilerPath))
	{
		AddError(FString::Printf(TEXT("Python GraphWrite compiler source could not be read: %s"), *PythonCompilerPath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(GraphSemanticIRSource, *GraphSemanticIRSourcePath))
	{
		AddError(FString::Printf(TEXT("GraphSemanticIR source could not be read: %s"), *GraphSemanticIRSourcePath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(GraphSemanticIRUtilsSource, *GraphSemanticIRUtilsSourcePath))
	{
		AddError(FString::Printf(TEXT("GraphSemanticIRUtils source could not be read: %s"), *GraphSemanticIRUtilsSourcePath));
		bClean = false;
	}

	if (!bClean)
	{
		return false;
	}

	const TArray<FString> RequiredPythonTokens = {
		TEXT("PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES"),
		TEXT("INTERNAL_DELEGATE_STATEMENT_KIND"),
		TEXT("FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS"),
		TEXT("delegate_operation")
	};
	for (const FString& Token : RequiredPythonTokens)
	{
		bClean &= TestTrue(*FString::Printf(TEXT("Python compiler declares delegate boundary token %s"), *Token), PythonCompilerSource.Contains(Token));
	}

	bClean &= TestTrue(TEXT("C++ parser accepts canonical component_bound_event"), GraphSemanticIRUtilsSource.Contains(TEXT("TEXT(\"component_bound_event\")")));
	bClean &= TestTrue(TEXT("C++ parser accepts canonical delegate"), GraphSemanticIRUtilsSource.Contains(TEXT("TEXT(\"delegate\")")));
	bClean &= TestTrue(TEXT("C++ parser reports unsupported statement kinds"), GraphSemanticIRSource.Contains(TEXT("statement_kind_unsupported")));

	const TArray<FString> ForbiddenCppParserTokens = {
		TEXT("delegate.bind"),
		TEXT("delegate.assign"),
		TEXT("delegate.unbind"),
		TEXT("delegate.unbind_all"),
		TEXT("delegate.call"),
		TEXT("delegate_call"),
		TEXT("delegate_clear")
	};
	for (const FString& Token : ForbiddenCppParserTokens)
	{
		if (GraphSemanticIRUtilsSource.Contains(Token))
		{
			AddError(FString::Printf(
				TEXT("C++ GraphSemanticIR parser must consume only canonical internal delegate shape; forbidden public/top-level token '%s' found in %s"),
				*Token,
				*GraphSemanticIRUtilsSourcePath));
			bClean = false;
		}
	}

	TestTrue(TEXT("Delegate public schema/internal lowering boundary is source-guarded"), bClean);
	return bClean;
}

#endif
