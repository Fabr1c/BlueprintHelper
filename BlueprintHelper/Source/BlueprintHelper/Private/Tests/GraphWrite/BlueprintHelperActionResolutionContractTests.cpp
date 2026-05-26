#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"

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

static FString BuildGraphWritePublicSourcePath(const TCHAR* Area, const TCHAR* FileName)
{
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"),
		TEXT("Public"),
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

static bool TryExtractSourceSlice(
	const FString& Source,
	const FString& StartToken,
	const FString& EndToken,
	FString& OutSlice)
{
	const int32 StartIndex = Source.Find(StartToken, ESearchCase::CaseSensitive);
	if (StartIndex == INDEX_NONE)
	{
		return false;
	}

	const int32 EndIndex = Source.Find(EndToken, ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIndex + StartToken.Len());
	if (EndIndex == INDEX_NONE || EndIndex <= StartIndex)
	{
		return false;
	}

	OutSlice = Source.Mid(StartIndex, EndIndex - StartIndex);
	return true;
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
	FBlueprintHelperActionResolutionContractCarriesFieldCapabilityFactsTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.CarriesFieldCapabilityFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionContractCarriesFieldCapabilityFactsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionSemanticConstraints Constraints;
	Constraints.Kind = EBlueprintHelperActionSemanticKind::Field;
	Constraints.CapabilityId = TEXT("field.member_get");
	Constraints.FieldOperation = TEXT("get");
	Constraints.FieldScope = TEXT("variable");
	Constraints.CapabilityFacts.Add(TEXT("field.owner_class"), TEXT("/Script/Engine.Actor"));
	Constraints.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("Health"));
	Constraints.CapabilityFacts.Add(TEXT("field.member_guid"), FGuid(0x11111111, 0x22222222, 0x33333333, 0x44444444).ToString(EGuidFormats::Digits));

	TestEqual(TEXT("capability id"), Constraints.CapabilityId, FString(TEXT("field.member_get")));
	TestEqual(TEXT("owner class"), Constraints.CapabilityFacts.FindRef(TEXT("field.owner_class")), FString(TEXT("/Script/Engine.Actor")));
	TestEqual(TEXT("member name"), Constraints.CapabilityFacts.FindRef(TEXT("field.member_name")), FString(TEXT("Health")));
	TestFalse(TEXT("member guid fact"), Constraints.CapabilityFacts.FindRef(TEXT("field.member_guid")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionContractFieldDiagnosticsRejectByRefAndUiOnlyTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.FieldDiagnostics.RejectByRefAndUiOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionContractFieldDiagnosticsRejectByRefAndUiOnlyTest::RunTest(const FString& Parameters)
{
	struct FRejectedFieldCapabilityExpectation
	{
		const TCHAR* Id;
		const TCHAR* Reason;
	};

	const FRejectedFieldCapabilityExpectation Expectations[] = {
		{TEXT("field.by_ref_set"), TEXT("unsupported_by_ref_set_deferred")},
		{TEXT("field.pin_drag_set"), TEXT("unsupported_ui_entry_not_statement")}
	};

	for (const FRejectedFieldCapabilityExpectation& Expectation : Expectations)
	{
		FString RejectReason;
		const bool bAllowed = FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(Expectation.Id, RejectReason);
		TestFalse(FString::Printf(TEXT("field diagnostic rejects %s"), Expectation.Id), bAllowed);
		TestEqual(FString::Printf(TEXT("field diagnostic reason %s"), Expectation.Id), RejectReason, FString(Expectation.Reason));
	}

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
	FBlueprintHelperActionResolutionFunctionFragmentLifecycleCoordinatorContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.FunctionFragmentLifecycleCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionFunctionFragmentLifecycleCoordinatorContractTest::RunTest(const FString& Parameters)
{
	const FString GraphStatementBuilderPath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperGraphStatementBuilder.cpp"));
	const FString CoordinatorPath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperActionFragmentSpawnCoordinator.cpp"));

	FString GraphStatementBuilderSource;
	FString CoordinatorSource;
	bool bClean = true;
	if (!FFileHelper::LoadFileToString(GraphStatementBuilderSource, *GraphStatementBuilderPath))
	{
		AddError(FString::Printf(TEXT("GraphStatementBuilder source could not be read: %s"), *GraphStatementBuilderPath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(CoordinatorSource, *CoordinatorPath))
	{
		AddError(FString::Printf(TEXT("ActionFragmentSpawnCoordinator source could not be read: %s"), *CoordinatorPath));
		bClean = false;
	}
	if (!bClean)
	{
		return false;
	}

	FString CallFragmentSlice;
	FString CreateFragmentSlice;
	FString ActionProviderSlice;
	if (!TryExtractSourceSlice(
		GraphStatementBuilderSource,
		TEXT("BuildCallFunctionFragment("),
		TEXT("BuildVariableSetFragment("),
		CallFragmentSlice))
	{
		AddError(TEXT("Could not extract BuildCallFunctionFragment source slice."));
		bClean = false;
	}
	if (!TryExtractSourceSlice(
		GraphStatementBuilderSource,
		TEXT("BuildCreateFragment("),
		TEXT("BuildActionProviderFragment("),
		CreateFragmentSlice))
	{
		AddError(TEXT("Could not extract BuildCreateFragment source slice."));
		bClean = false;
	}
	if (!TryExtractSourceSlice(
		GraphStatementBuilderSource,
		TEXT("BuildActionProviderFragment("),
		TEXT("BuildSequenceFragment("),
		ActionProviderSlice))
	{
		AddError(TEXT("Could not extract BuildActionProviderFragment source slice."));
		bClean = false;
	}
	if (!bClean)
	{
		return false;
	}

	const TArray<TPair<FString, FString>> FragmentSlices = {
		TPair<FString, FString>(TEXT("BuildCallFunctionFragment"), CallFragmentSlice),
		TPair<FString, FString>(TEXT("BuildActionProviderFragment"), ActionProviderSlice)
	};
	for (const TPair<FString, FString>& Slice : FragmentSlices)
	{
		bClean &= TestTrue(
			*FString::Printf(TEXT("%s delegates lifecycle to coordinator"), *Slice.Key),
			Slice.Value.Contains(TEXT("FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment")));
		bClean &= TestFalse(
			*FString::Printf(TEXT("%s does not resolve action locally"), *Slice.Key),
			Slice.Value.Contains(TEXT("ResolveActionForGraph(")));
		bClean &= TestFalse(
			*FString::Printf(TEXT("%s does not invoke selected spawner locally"), *Slice.Key),
			Slice.Value.Contains(TEXT("InvokeSelectedSpawner(")));
	}
	bClean &= TestTrue(
		TEXT("BuildCallFunctionFragment appends semantic kind ownership tag"),
		CallFragmentSlice.Contains(TEXT("CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;")));
	bClean &= TestTrue(
		TEXT("BuildActionProviderFragment appends semantic kind ownership tag"),
		ActionProviderSlice.Contains(TEXT("CoordinatorRequest.bAppendSemanticKindOwnershipTag = true;")));
	bClean &= TestTrue(
		TEXT("BuildCallFunctionFragment passes build request by scoped reference"),
		CallFragmentSlice.Contains(TEXT("CoordinatorRequest.BuildRequest = &BoundRequest;")));
	bClean &= TestTrue(
		TEXT("BuildActionProviderFragment passes build request by scoped reference"),
		ActionProviderSlice.Contains(TEXT("CoordinatorRequest.BuildRequest = &Request;")));
	bClean &= TestTrue(
		TEXT("BuildCallFunctionFragment appends context evidence to action request"),
		CallFragmentSlice.Contains(TEXT("ActionRequest.ContextEvidence.Append(BoundRequest.ContextEvidence);")));
	bClean &= TestTrue(
		TEXT("BuildCreateFragment appends context evidence to action request"),
		CreateFragmentSlice.Contains(TEXT("ActionRequest.ContextEvidence.Append(Request.ContextEvidence);")));
	bClean &= TestTrue(
		TEXT("BuildActionProviderFragment appends context evidence to action request"),
		ActionProviderSlice.Contains(TEXT("ActionRequest.ContextEvidence.Append(Request.ContextEvidence);")));

	bClean &= TestTrue(
		TEXT("ActionFragmentSpawnCoordinator owns ActionResolution resolve"),
		CoordinatorSource.Contains(TEXT("FBlueprintGraphWriteFacade::ResolveActionForGraph")));
	bClean &= TestTrue(
		TEXT("ActionFragmentSpawnCoordinator owns selected spawner invocation"),
		CoordinatorSource.Contains(TEXT("FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner")));
	bClean &= TestTrue(
		TEXT("ActionFragmentSpawnCoordinator owns common fragment pin/metadata population"),
		CoordinatorSource.Contains(TEXT("FBlueprintHelperActionFragmentBuildUtils::PopulatePins"))
		&& CoordinatorSource.Contains(TEXT("FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata")));

	TestTrue(TEXT("Function call/action-provider fragment lifecycle is coordinator-owned"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphFragmentBuildRequestRuntimePayloadContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.GraphFragmentBuildRequestRuntimePayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphFragmentBuildRequestRuntimePayloadContractTest::RunTest(const FString& Parameters)
{
	const FString BuildRequestHeaderPath = BuildGraphWritePublicSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperGraphFragmentBuildRequest.h"));
	const FString BuildRequestSourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperGraphFragmentBuildRequest.cpp"));

	FString HeaderSource;
	FString CppSource;
	bool bClean = true;
	if (!FFileHelper::LoadFileToString(HeaderSource, *BuildRequestHeaderPath))
	{
		AddError(FString::Printf(TEXT("GraphFragmentBuildRequest header could not be read: %s"), *BuildRequestHeaderPath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(CppSource, *BuildRequestSourcePath))
	{
		AddError(FString::Printf(TEXT("GraphFragmentBuildRequest source could not be read: %s"), *BuildRequestSourcePath));
		bClean = false;
	}
	if (!bClean)
	{
		return false;
	}

	bClean &= TestFalse(
		TEXT("GraphFragmentBuildRequest does not store recursive statement payload"),
		HeaderSource.Contains(TEXT("FBlueprintHelperGraphStatementIR Statement;")));
	bClean &= TestFalse(
		TEXT("GraphFragmentBuildRequest does not store recursive expression payload"),
		HeaderSource.Contains(TEXT("FBlueprintHelperGraphExpressionIR Expression;")));
	bClean &= TestFalse(
		TEXT("FromStatement does not copy recursive statement payload"),
		CppSource.Contains(TEXT("Request.Statement = Statement;")));
	bClean &= TestFalse(
		TEXT("FromExpression does not copy recursive expression payload"),
		CppSource.Contains(TEXT("Request.Expression = Expression;")));

	TestTrue(TEXT("GraphFragmentBuildRequest is a shallow runtime build payload"), bClean);
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
	FBlueprintHelperActionResolutionEventDelegateNoCustomEventScanContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.EventDelegateNoCustomEventScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionEventDelegateNoCustomEventScanContractTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperEventDelegateUseSiteEvidence.cpp"));

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SourcePath))
	{
		AddError(FString::Printf(TEXT("EventDelegate evidence source could not be read: %s"), *SourcePath));
		return false;
	}

	bool bClean = true;
	const TArray<FString> ForbiddenTokens = {
		TEXT("#include \"K2Node_CustomEvent.h\""),
		TEXT("UK2Node_CustomEvent"),
		TEXT("UbergraphPages")
	};
	for (const FString& Token : ForbiddenTokens)
	{
		if (Text.Contains(Token))
		{
			AddError(FString::Printf(TEXT("EventDelegate resolver must consume projected handler evidence, not scan custom events; forbidden token '%s' found."), *Token));
			bClean = false;
		}
	}
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

	bClean &= TestTrue(
		TEXT("GenericAssetStructControlActionCluster routes generic Convert/Schedule through the dedicated transform/schedule resolver"),
		GenericClusterSource.Contains(TEXT("FBlueprintHelperGenericTransformScheduleActionResolver::Resolve")));

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

	TestTrue(TEXT("Convert/Schedule keep distinct FunctionAction and Generic transform/schedule boundaries"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteDelegatePublicInternalBoundaryContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.DelegatePublicInternalBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteDelegatePublicInternalBoundaryContractTest::RunTest(const FString& Parameters)
{
	const FString TsCompilerSourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("AgentFaceService"),
		TEXT("task-core"),
		TEXT("src"),
		TEXT("task"),
		TEXT("compiler"),
		TEXT("task-compiler.ts"));
	const FString TsContractSourcePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("AgentFaceService"),
		TEXT("task-core"),
		TEXT("src"),
		TEXT("task"),
		TEXT("schema"),
		TEXT("task-contract.ts"));
	const FString GraphSemanticIRSourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement"),
		TEXT("BlueprintHelperGraphSemanticIR.cpp"));
	const FString GraphSemanticIRUtilsSourcePath = BuildGraphWritePrivateSourcePath(
		TEXT("GraphStatement/Utils"),
		TEXT("BlueprintHelperGraphSemanticIRUtils.cpp"));

	FString TsCompilerSource;
	FString TsContractSource;
	FString GraphSemanticIRSource;
	FString GraphSemanticIRUtilsSource;
	bool bClean = true;
	if (!FFileHelper::LoadFileToString(TsCompilerSource, *TsCompilerSourcePath))
	{
		AddError(FString::Printf(TEXT("TaskSpec TS compiler source could not be read: %s"), *TsCompilerSourcePath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(TsContractSource, *TsContractSourcePath))
	{
		AddError(FString::Printf(TEXT("TaskSpec TS contract source could not be read: %s"), *TsContractSourcePath));
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

	const TArray<FString> RequiredTsCompilerTokens = {
		TEXT("PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES"),
		TEXT("INTERNAL_DELEGATE_STATEMENT_KIND"),
		TEXT("FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS"),
		TEXT("delegate_operation")
	};
	for (const FString& Token : RequiredTsCompilerTokens)
	{
		bClean &= TestTrue(*FString::Printf(TEXT("TS compiler declares delegate boundary token %s"), *Token), TsCompilerSource.Contains(Token));
	}

	const TArray<FString> RequiredTsContractTokens = {
		TEXT("event_delegate_use_site_boundary"),
		TEXT("delegate.bind"),
		TEXT("delegate.unbind_all"),
		TEXT("public_to_internal_lowering")
	};
	for (const FString& Token : RequiredTsContractTokens)
	{
		bClean &= TestTrue(*FString::Printf(TEXT("TS contract declares delegate boundary token %s"), *Token), TsContractSource.Contains(Token));
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

	TestTrue(TEXT("Delegate TS compiler/contract to C++ internal lowering boundary is source-guarded"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionAssetActionNoSyntheticSpawnerContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.AssetActionNoSyntheticSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionAssetActionNoSyntheticSpawnerContractTest::RunTest(const FString& Parameters)
{
	const FString GenericCreateResolverPath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperGenericCreateActionResolver.cpp"));
	const FString GenericAssetResolverPath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperGenericAssetActionResolver.cpp"));
	const FString ProjectionServicePath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperAssetActionProjectionService.cpp"));
	const FString NeutralProjectionServicePath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperActionDatabaseProjectionService.cpp"));

	FString GenericCreateResolverSource;
	FString GenericAssetResolverSource;
	FString ProjectionServiceSource;
	FString NeutralProjectionServiceSource;
	bool bClean = true;
	if (!FFileHelper::LoadFileToString(GenericCreateResolverSource, *GenericCreateResolverPath))
	{
		AddError(FString::Printf(TEXT("GenericCreateActionResolver source could not be read: %s"), *GenericCreateResolverPath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(GenericAssetResolverSource, *GenericAssetResolverPath))
	{
		AddError(FString::Printf(TEXT("GenericAssetActionResolver source could not be read: %s"), *GenericAssetResolverPath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(ProjectionServiceSource, *ProjectionServicePath))
	{
		AddError(FString::Printf(TEXT("AssetActionProjectionService source could not be read: %s"), *ProjectionServicePath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(NeutralProjectionServiceSource, *NeutralProjectionServicePath))
	{
		AddError(FString::Printf(TEXT("ActionDatabaseProjectionService source could not be read: %s"), *NeutralProjectionServicePath));
		bClean = false;
	}
	if (!bClean)
	{
		return false;
	}

	bClean &= TestTrue(
		TEXT("Generic create routes asset_action through the dedicated asset action resolver"),
		GenericCreateResolverSource.Contains(TEXT("FBlueprintHelperGenericAssetActionResolver::Resolve")));
	bClean &= TestFalse(
		TEXT("Dedicated asset_action resolver does not synthesize node spawners with UBlueprintNodeSpawner::Create"),
		GenericAssetResolverSource.Contains(TEXT("UBlueprintNodeSpawner::Create")));
	bClean &= TestTrue(
		TEXT("Asset action resolver uses projection service"),
		GenericAssetResolverSource.Contains(TEXT("FBlueprintHelperAssetActionProjectionService::Project")));
	bClean &= TestTrue(
		TEXT("Asset action projection delegates to neutral ActionDatabase projection service"),
		ProjectionServiceSource.Contains(TEXT("FBlueprintHelperActionDatabaseProjectionService::Project")));
	bClean &= TestFalse(
		TEXT("Asset action resolver does not directly refresh ActionDatabase after extraction"),
		GenericAssetResolverSource.Contains(TEXT("FBlueprintActionDatabase::Get().RefreshAll()")));
	bClean &= TestTrue(
		TEXT("Neutral projection service refreshes and consumes ActionDatabase registry"),
		NeutralProjectionServiceSource.Contains(TEXT("FBlueprintActionDatabase::Get().RefreshAll()"))
		&& NeutralProjectionServiceSource.Contains(TEXT("GetAllActions()")));
	bClean &= TestFalse(
		TEXT("Neutral projection service does not synthesize node spawners with UBlueprintNodeSpawner::Create"),
		NeutralProjectionServiceSource.Contains(TEXT("UBlueprintNodeSpawner::Create")));

	TestTrue(TEXT("asset_action keeps real ActionDatabase-only spawner resolution"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionGenericScheduleNoSyntheticSpawnerContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.GenericScheduleNoSyntheticSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionGenericScheduleNoSyntheticSpawnerContractTest::RunTest(const FString& Parameters)
{
	const FString GenericTransformResolverPath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperGenericTransformScheduleActionResolver.cpp"));

	FString GenericTransformResolverSource;
	if (!FFileHelper::LoadFileToString(GenericTransformResolverSource, *GenericTransformResolverPath))
	{
		AddError(FString::Printf(TEXT("GenericTransformScheduleActionResolver source could not be read: %s"), *GenericTransformResolverPath));
		return false;
	}

	const int32 ScheduleStart = GenericTransformResolverSource.Find(TEXT("static FBlueprintHelperActionResolutionResult ResolveSchedule"));
	const int32 ScheduleEnd = GenericTransformResolverSource.Find(TEXT("bool FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedTransformOperation"));
	if (ScheduleStart == INDEX_NONE || ScheduleEnd == INDEX_NONE || ScheduleEnd <= ScheduleStart)
	{
		AddError(TEXT("Could not isolate ResolveSchedule source section."));
		return false;
	}

	const FString ScheduleSource = GenericTransformResolverSource.Mid(ScheduleStart, ScheduleEnd - ScheduleStart);
	bool bClean = true;
	bClean &= TestTrue(
		TEXT("Generic schedule resolver reads projected schedule evidence"),
		ScheduleSource.Contains(TEXT("ReadScheduleActionEvidence")));
	bClean &= TestTrue(
		TEXT("Generic schedule resolver revalidates through neutral ActionDatabase projection service"),
		ScheduleSource.Contains(TEXT("FBlueprintHelperActionDatabaseProjectionService::Project")));
	bClean &= TestFalse(
		TEXT("Generic schedule resolver does not synthesize schedule spawners with UBlueprintNodeSpawner::Create"),
		ScheduleSource.Contains(TEXT("UBlueprintNodeSpawner::Create")));
	TestTrue(TEXT("Generic schedule uses projected ActionDatabase spawner resolution only"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionTypePromotionUsesRegisteredSpawnerContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.TypePromotionUsesRegisteredSpawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionTypePromotionUsesRegisteredSpawnerContractTest::RunTest(const FString& Parameters)
{
	const FString GenericTransformResolverPath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperGenericTransformScheduleActionResolver.cpp"));
	const FString TypePromotionResolverPath = BuildGraphWritePrivateSourcePath(
		TEXT("ActionResolution"),
		TEXT("BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp"));

	FString GenericTransformResolverSource;
	FString TypePromotionResolverSource;
	bool bClean = true;
	if (!FFileHelper::LoadFileToString(GenericTransformResolverSource, *GenericTransformResolverPath))
	{
		AddError(FString::Printf(TEXT("GenericTransformScheduleActionResolver source could not be read: %s"), *GenericTransformResolverPath));
		bClean = false;
	}
	if (!FFileHelper::LoadFileToString(TypePromotionResolverSource, *TypePromotionResolverPath))
	{
		AddError(FString::Printf(TEXT("TypePromotionSpawnerEvidenceResolver source could not be read: %s"), *TypePromotionResolverPath));
		bClean = false;
	}
	if (!bClean)
	{
		return false;
	}

	bClean &= TestTrue(
		TEXT("Generic transform routes type_promotion through the dedicated resolver"),
		GenericTransformResolverSource.Contains(TEXT("FBlueprintHelperTypePromotionSpawnerEvidenceResolver::Resolve")));
	bClean &= TestFalse(
		TEXT("Dedicated type_promotion resolver does not synthesize node spawners with UBlueprintNodeSpawner::Create"),
		TypePromotionResolverSource.Contains(TEXT("UBlueprintNodeSpawner::Create")));
	bClean &= TestTrue(
		TEXT("Dedicated type_promotion resolver consumes registered FTypePromotion spawners"),
		TypePromotionResolverSource.Contains(TEXT("FTypePromotion::GetOperatorSpawner"))
		&& TypePromotionResolverSource.Contains(TEXT("FTypePromotion::Get()"))
		&& TypePromotionResolverSource.Contains(TEXT("FBlueprintActionDatabase::Get().RefreshAll()")));
	bClean &= TestTrue(
		TEXT("Dedicated type_promotion resolver validates typed promotion compatibility"),
		TypePromotionResolverSource.Contains(TEXT("FTypePromotion::IsValidPromotion")));

	TestTrue(TEXT("type_promotion keeps registered FTypePromotion-only spawner resolution"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionContractComponentRefUsesVariableSpawnerFactsTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.ComponentRef.UsesVariableSpawnerFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionContractComponentRefUsesVariableSpawnerFactsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionCandidate Candidate;
	Candidate.CapabilityId = TEXT("field.component_ref_get");
	Candidate.ExpectedNodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_VariableGet");
	Candidate.NodeClassPath = TEXT("/Script/BlueprintGraph.K2Node_VariableGet");
	Candidate.ReadbackFacts.Add(TEXT("component_ref_spawner"), TEXT("UBlueprintVariableNodeSpawner"));

	TestFalse(TEXT("candidate node class is not add component"), Candidate.NodeClassPath.Contains(TEXT("K2Node_AddComponent")));
	TestFalse(TEXT("candidate does not name component node spawner"), Candidate.ReadbackFacts.FindRef(TEXT("component_ref_spawner")).Contains(TEXT("UBlueprintComponentNodeSpawner")));
	return true;
}

#endif
