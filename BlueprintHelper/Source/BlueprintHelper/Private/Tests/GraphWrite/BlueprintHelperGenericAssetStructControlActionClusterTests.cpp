#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
static FString MakeGenericActionTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericActionTestBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericAction/%s"),
		*MakeGenericActionTestObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericActionTestObjectName(TEXT("BP_GenericAction")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericActionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* GetGenericActionTestGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static FBlueprintHelperActionResolutionRequest MakeGenericActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query = FString(),
	const FString& TypeName = FString())
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("generic_action_projected_context");
	Request.SemanticConstraintsHash = TEXT("generic_action_semantic_constraints");
	Request.Semantic.Kind = SemanticKind;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.Semantic.TypeName = TypeName;
	Request.MaxCandidates = 8;
	return Request;
}

static bool HasCandidateNodeClassPath(
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedNodeClassPathPart)
{
	for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : Result.CandidateActions)
	{
		if (Candidate.NodeClassPath.Contains(ExpectedNodeClassPathPart))
		{
			return true;
		}
	}
	return false;
}

static bool HasStructCandidateEvidence(
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedStructIdentity)
{
	for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : Result.CandidateActions)
	{
		if (Candidate.Category.Equals(TEXT("Struct"), ESearchCase::IgnoreCase)
			&& (Candidate.StableId.Contains(ExpectedStructIdentity)
				|| Candidate.DisplayName.Contains(ExpectedStructIdentity)
				|| Candidate.ReturnType.Contains(ExpectedStructIdentity)
				|| Candidate.MatchReason.Contains(TEXT("struct"))))
		{
			return true;
		}
	}
	return false;
}

static bool AssertResolvedSingletonControlFlowResult(
	FAutomationTestBase& Test,
	const FBlueprintHelperActionResolutionResult& Result,
	const FString& ExpectedStableIdPart,
	const FString& ExpectedNodeClassPathPart)
{
	bool bPassed = true;
	bPassed &= Test.TestEqual(TEXT("singleton result status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	bPassed &= Test.TestTrue(TEXT("singleton stable id names provider identity"), Result.SelectedStableId.Contains(TEXT("singleton_control_flow")));
	bPassed &= Test.TestTrue(TEXT("singleton stable id names expected control identity"), Result.SelectedStableId.Contains(ExpectedStableIdPart));
	bPassed &= Test.TestNotNull(TEXT("singleton selected spawner"), Result.SelectedSpawner.Get());
	bPassed &= Test.TestTrue(TEXT("singleton candidate action records node class path"), HasCandidateNodeClassPath(Result, ExpectedNodeClassPathPart));
	return bPassed;
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
		if (File.EndsWith(TEXT("BlueprintHelperSingletonControlFlowEvidenceProvider.cpp")))
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
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperGenericActionProviderBoundary ConstructNeedsContext =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Construct));
	TestEqual(TEXT("Construct without type needs context"), ConstructNeedsContext.Mode, EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext);
	TestEqual(TEXT("Construct builder seam"), ConstructNeedsContext.RequiredBuilder, FString(TEXT("ConstructFragmentBuilder")));

	const FBlueprintHelperGenericActionProviderBoundary ConstructCandidate =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Construct, FString(), TEXT("Vector")));
	TestEqual(TEXT("Construct with type can query NodeSpawner"), ConstructCandidate.Mode, EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate);

	const FBlueprintHelperGenericActionProviderBoundary DeconstructNeedsContext =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Deconstruct));
	TestEqual(TEXT("Deconstruct without type needs context"), DeconstructNeedsContext.Mode, EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext);

	const FBlueprintHelperGenericActionProviderBoundary DeconstructCandidate =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Deconstruct, FString(), TEXT("Transform")));
	TestEqual(TEXT("Deconstruct with type can query NodeSpawner"), DeconstructCandidate.Mode, EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate);

	const FBlueprintHelperGenericActionProviderBoundary SelectBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Select));
	TestEqual(TEXT("Select resolves through singleton provider"), SelectBoundary.Mode, EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate);
	TestEqual(TEXT("Select builder seam"), SelectBoundary.RequiredBuilder, FString(TEXT("SelectFragmentBuilder")));
	TestTrue(TEXT("Select reason names singleton provider"), SelectBoundary.Reason.Contains(TEXT("singleton")));

	const FBlueprintHelperGenericActionProviderBoundary ControlNeedsContext =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Control));
	TestEqual(TEXT("Control without query needs context"), ControlNeedsContext.Mode, EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext);
	TestEqual(TEXT("Control builder seam"), ControlNeedsContext.RequiredBuilder, FString(TEXT("ControlFragmentBuilder")));

	const FBlueprintHelperGenericActionProviderBoundary ControlBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Control, TEXT("branch")));
	TestEqual(TEXT("Control with singleton query resolves through provider"), ControlBoundary.Mode, EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate);
	TestEqual(TEXT("Control builder seam with query"), ControlBoundary.RequiredBuilder, FString(TEXT("ControlFragmentBuilder")));
	TestTrue(TEXT("Control reason names singleton provider"), ControlBoundary.Reason.Contains(TEXT("singleton")));

	const FBlueprintHelperGenericActionProviderBoundary UnsupportedBoundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Call, TEXT("SetActorLocation")));
	TestEqual(TEXT("Call is outside generic provider boundary"), UnsupportedBoundary.Mode, EBlueprintHelperGenericActionProviderMode::Unsupported);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericActionClusterBoundaryResultTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.ClusterBoundaryResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericActionClusterBoundaryResultTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperActionResolutionResult ConstructNeedsContext =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Construct));
	TestEqual(TEXT("Construct without TypeName is invalid request"), ConstructNeedsContext.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("Construct without TypeName reports missing required evidence"), ConstructNeedsContext.ErrorCode, FString(TEXT("missing_required_evidence")));

	const FBlueprintHelperActionResolutionResult ConstructCandidate =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Construct, FString(), TEXT("Vector")));
	TestTrue(
		TEXT("Construct with TypeName no longer returns migration/not-found placeholder"),
		ConstructCandidate.ErrorCode != (FString(TEXT("generic_action_node_spawner_candidate_")) + FString(TEXT("not_found"))));

	const FBlueprintHelperActionResolutionResult DeconstructCandidate =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Deconstruct, FString(), TEXT("Transform")));
	TestTrue(
		TEXT("Deconstruct with TypeName no longer returns migration/not-found placeholder"),
		DeconstructCandidate.ErrorCode != (FString(TEXT("generic_action_node_spawner_candidate_")) + FString(TEXT("not_found"))));

	const FBlueprintHelperActionResolutionResult SelectResolved =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Select));
	AssertResolvedSingletonControlFlowResult(
		*this,
		SelectResolved,
		TEXT("select"),
		TEXT("K2Node_Select"));

	const FBlueprintHelperActionResolutionResult ControlNeedsContext =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Control));
	TestEqual(TEXT("Control without query needs context"), ControlNeedsContext.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("Control without query reports context error"), ControlNeedsContext.ErrorCode, FString(TEXT("needs_more_semantic_context")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericActionStructMakeBreakEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.P5.StructMakeBreakEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericActionStructMakeBreakEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const FBlueprintHelperActionResolutionResult ConstructVector =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Construct, FString(), TEXT("Vector")));
	TestEqual(TEXT("construct vector status"), ConstructVector.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("construct vector selected spawner"), ConstructVector.SelectedSpawner.Get());
	TestTrue(TEXT("construct vector selected stable id includes struct identity"), ConstructVector.SelectedStableId.Contains(TEXT("Vector")));
	TestTrue(TEXT("construct vector candidate records struct evidence"), HasStructCandidateEvidence(ConstructVector, TEXT("Vector")));

	const FBlueprintHelperActionResolutionResult DeconstructRotator =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Deconstruct, FString(), TEXT("Rotator")));
	TestEqual(TEXT("deconstruct rotator status"), DeconstructRotator.Status, EBlueprintHelperActionResolutionStatus::Resolved);
	TestNotNull(TEXT("deconstruct rotator selected spawner"), DeconstructRotator.SelectedSpawner.Get());
	TestTrue(TEXT("deconstruct rotator selected stable id includes struct identity"), DeconstructRotator.SelectedStableId.Contains(TEXT("Rotator")));
	TestTrue(TEXT("deconstruct rotator candidate records struct evidence"), HasStructCandidateEvidence(DeconstructRotator, TEXT("Rotator")));

	const FBlueprintHelperActionResolutionResult ConstructMissingType =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Construct));
	TestEqual(TEXT("construct missing target type status"), ConstructMissingType.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("construct missing target type error"), ConstructMissingType.ErrorCode, FString(TEXT("missing_required_evidence")));

	const FBlueprintHelperActionResolutionResult DeconstructUnsupportedStruct =
		FBlueprintHelperActionResolutionCore::Resolve(
			MakeGenericActionRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Deconstruct, FString(), TEXT("DefinitelyMissingP5Struct")));
	TestEqual(TEXT("deconstruct unsupported struct status"), DeconstructUnsupportedStruct.Status, EBlueprintHelperActionResolutionStatus::NotFound);
	TestEqual(TEXT("deconstruct unsupported struct error"), DeconstructUnsupportedStruct.ErrorCode, FString(TEXT("not_found")));
	TestNull(TEXT("deconstruct unsupported struct has no spawner"), DeconstructUnsupportedStruct.SelectedSpawner.Get());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericActionRejectsUnsupportedWideSurfaceWithoutStructFallbackTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.P5.NoFallbackSuccessForCreateConvertSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericActionRejectsUnsupportedWideSurfaceWithoutStructFallbackTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const EBlueprintHelperActionSemanticKind UnsupportedKinds[] = {
		EBlueprintHelperActionSemanticKind::Create,
		EBlueprintHelperActionSemanticKind::Convert,
		EBlueprintHelperActionSemanticKind::Schedule
	};

	for (const EBlueprintHelperActionSemanticKind Kind : UnsupportedKinds)
	{
		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperActionResolutionCore::Resolve(
				MakeGenericActionRequest(Blueprint, Graph, Kind, TEXT("Vector"), TEXT("Vector")));
		TestTrue(
			FString::Printf(TEXT("%s is not resolved through struct fallback"), *FBlueprintHelperActionResolutionCore::SemanticKindToString(Kind)),
			Result.Status == EBlueprintHelperActionResolutionStatus::UnsupportedIntent
			|| Result.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest);
		TestFalse(
			FString::Printf(TEXT("%s has no selected spawner"), *FBlueprintHelperActionResolutionCore::SemanticKindToString(Kind)),
			Result.SelectedSpawner.IsValid());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSingletonControlFlowProviderPositiveTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.SingletonControlFlowProviderPositive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSingletonControlFlowProviderPositiveTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const struct FCase
	{
		EBlueprintHelperActionSemanticKind SemanticKind;
		FString Query;
		EBlueprintHelperSingletonControlFlowKind ExpectedKind;
		FString ExpectedStableIdPart;
		FString ExpectedNodeClassPathPart;
	} Cases[] = {
		{EBlueprintHelperActionSemanticKind::Control, TEXT("branch"), EBlueprintHelperSingletonControlFlowKind::Branch, TEXT("branch"), TEXT("K2Node_IfThenElse")},
		{EBlueprintHelperActionSemanticKind::Control, TEXT("sequence"), EBlueprintHelperSingletonControlFlowKind::Sequence, TEXT("sequence"), TEXT("K2Node_ExecutionSequence")},
		{EBlueprintHelperActionSemanticKind::Select, FString(), EBlueprintHelperSingletonControlFlowKind::Select, TEXT("select"), TEXT("K2Node_Select")},
		{EBlueprintHelperActionSemanticKind::Control, TEXT("return"), EBlueprintHelperSingletonControlFlowKind::Return, TEXT("return"), TEXT("K2Node_FunctionResult")}
	};

	for (const FCase& Case : Cases)
	{
		const FBlueprintHelperActionResolutionRequest Request =
			MakeGenericActionRequest(Blueprint, Graph, Case.SemanticKind, Case.Query);

		FBlueprintHelperSingletonControlFlowEvidence Evidence;
		TestTrue(FString::Printf(TEXT("provider resolves %s"), *Case.ExpectedStableIdPart),
			FBlueprintHelperSingletonControlFlowEvidenceProvider::TryResolve(Request, Evidence));
		TestEqual(FString::Printf(TEXT("%s singleton kind"), *Case.ExpectedStableIdPart), Evidence.SingletonKind, Case.ExpectedKind);
		TestNotNull(FString::Printf(TEXT("%s node class"), *Case.ExpectedStableIdPart), Evidence.NodeClass.Get());
		TestTrue(FString::Printf(TEXT("%s evidence stable id"), *Case.ExpectedStableIdPart), Evidence.StableId.Contains(TEXT("singleton_control_flow")));
		TestTrue(FString::Printf(TEXT("%s evidence node class path"), *Case.ExpectedStableIdPart), Evidence.NodeClass->GetPathName().Contains(Case.ExpectedNodeClassPathPart));

		const FBlueprintHelperActionResolutionResult ProviderResult =
			FBlueprintHelperSingletonControlFlowEvidenceProvider::MakeResolvedResult(Request, Evidence);
		AssertResolvedSingletonControlFlowResult(
			*this,
			ProviderResult,
			Case.ExpectedStableIdPart,
			Case.ExpectedNodeClassPathPart);

		const FBlueprintHelperActionResolutionResult CoreResult =
			FBlueprintHelperActionResolutionCore::Resolve(Request);
		AssertResolvedSingletonControlFlowResult(
			*this,
			CoreResult,
			Case.ExpectedStableIdPart,
			Case.ExpectedNodeClassPathPart);

		FBlueprintHelperActionResolutionRequest CanonicalRequest;
		TestTrue(FString::Printf(TEXT("%s provider builds canonical request"), *Case.ExpectedStableIdPart),
			FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
				Case.ExpectedKind,
				Blueprint,
				Graph,
				TEXT("singleton_boundary_positive"),
				TEXT("test_provider_positive"),
				CanonicalRequest));
		TestEqual(FString::Printf(TEXT("%s canonical request first-level cluster"), *Case.ExpectedStableIdPart),
			CanonicalRequest.ClusterKind,
			EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
		TestEqual(FString::Printf(TEXT("%s canonical request target graph"), *Case.ExpectedStableIdPart),
			CanonicalRequest.TargetGraph,
			Graph);
		if (Case.ExpectedKind == EBlueprintHelperSingletonControlFlowKind::Select)
		{
			TestEqual(TEXT("select canonical semantic kind"),
				CanonicalRequest.Semantic.Kind,
				EBlueprintHelperActionSemanticKind::Select);
		}
		else
		{
			TestEqual(TEXT("control canonical semantic kind"),
				CanonicalRequest.Semantic.Kind,
				EBlueprintHelperActionSemanticKind::Control);
			TestTrue(TEXT("control canonical query is provider-owned"),
				!CanonicalRequest.Semantic.Query.IsEmpty());
		}

		const FBlueprintHelperActionResolutionResult CanonicalResult =
			FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
				Case.ExpectedKind,
				Blueprint,
				Graph,
				TEXT("singleton_boundary_positive"),
				TEXT("test_provider_positive"));
		AssertResolvedSingletonControlFlowResult(
			*this,
			CanonicalResult,
			Case.ExpectedStableIdPart,
			Case.ExpectedNodeClassPathPart);
	}

	FBlueprintHelperActionResolutionRequest InvalidRequest;
	TestFalse(TEXT("canonical request rejects missing Blueprint"),
		FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			nullptr,
			Graph,
			TEXT("singleton_boundary_invalid"),
			TEXT("test_provider_invalid"),
			InvalidRequest));
	TestFalse(TEXT("canonical request rejects missing target graph"),
		FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			Blueprint,
			nullptr,
			TEXT("singleton_boundary_invalid"),
			TEXT("test_provider_invalid"),
			InvalidRequest));
	TestFalse(TEXT("canonical request rejects unknown singleton kind"),
		FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
			EBlueprintHelperSingletonControlFlowKind::Unknown,
			Blueprint,
			Graph,
			TEXT("singleton_boundary_invalid"),
			TEXT("test_provider_invalid"),
			InvalidRequest));

	const FBlueprintHelperActionResolutionResult MissingBlueprintCanonicalResult =
		FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			nullptr,
			Graph,
			TEXT("singleton_boundary_invalid"),
			TEXT("test_provider_invalid"));
	TestEqual(TEXT("missing Blueprint canonical resolve status"),
		MissingBlueprintCanonicalResult.Status,
		EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing Blueprint canonical resolve error"),
		MissingBlueprintCanonicalResult.ErrorCode,
		FString(TEXT("missing_required_evidence")));

	const FBlueprintHelperActionResolutionResult MissingGraphCanonicalResult =
		FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			Blueprint,
			nullptr,
			TEXT("singleton_boundary_invalid"),
			TEXT("test_provider_invalid"));
	TestEqual(TEXT("missing graph canonical resolve status"),
		MissingGraphCanonicalResult.Status,
		EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("missing graph canonical resolve error"),
		MissingGraphCanonicalResult.ErrorCode,
		FString(TEXT("missing_required_evidence")));

	const FBlueprintHelperActionResolutionResult UnknownKindCanonicalResult =
		FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
			EBlueprintHelperSingletonControlFlowKind::Unknown,
			Blueprint,
			Graph,
			TEXT("singleton_boundary_invalid"),
			TEXT("test_provider_invalid"));
	TestEqual(TEXT("unknown kind canonical resolve status"),
		UnknownKindCanonicalResult.Status,
		EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("unknown kind canonical resolve error"),
		UnknownKindCanonicalResult.ErrorCode,
		FString(TEXT("missing_required_evidence")));

	FBlueprintHelperActionResolutionRequest ReasonARequest;
	FBlueprintHelperActionResolutionRequest ReasonBRequest;
	TestTrue(TEXT("canonical hash request reason A"),
		FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			Blueprint,
			Graph,
			TEXT("singleton_hash_identity"),
			TEXT("reason_a"),
			ReasonARequest));
	TestTrue(TEXT("canonical hash request reason B"),
		FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			Blueprint,
			Graph,
			TEXT("singleton_hash_identity"),
			TEXT("reason_b"),
			ReasonBRequest));
	TestEqual(TEXT("canonical projected context hash ignores diagnostic reason"),
		ReasonARequest.ProjectedContextHash,
		ReasonBRequest.ProjectedContextHash);
	TestEqual(TEXT("canonical semantic constraints hash ignores diagnostic reason"),
		ReasonARequest.SemanticConstraintsHash,
		ReasonBRequest.SemanticConstraintsHash);

	FBlueprintHelperActionResolutionRequest StatementChangedRequest;
	TestTrue(TEXT("canonical hash request statement changed"),
		FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			Blueprint,
			Graph,
			TEXT("singleton_hash_identity_changed"),
			TEXT("reason_a"),
			StatementChangedRequest));
	TestNotEqual(TEXT("canonical projected context hash uses statement id"),
		ReasonARequest.ProjectedContextHash,
		StatementChangedRequest.ProjectedContextHash);
	TestNotEqual(TEXT("canonical semantic constraints hash uses statement id"),
		ReasonARequest.SemanticConstraintsHash,
		StatementChangedRequest.SemanticConstraintsHash);

	UBlueprint* OtherBlueprint = MakeGenericActionTestBlueprint();
	UEdGraph* OtherGraph = GetGenericActionTestGraph(OtherBlueprint);
	TestNotNull(TEXT("other blueprint"), OtherBlueprint);
	TestNotNull(TEXT("other graph"), OtherGraph);

	FBlueprintHelperActionResolutionRequest GraphChangedRequest;
	TestTrue(TEXT("canonical hash request graph changed"),
		FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			OtherBlueprint,
			OtherGraph,
			TEXT("singleton_hash_identity"),
			TEXT("reason_a"),
			GraphChangedRequest));
	TestNotEqual(TEXT("canonical projected context hash uses graph context"),
		ReasonARequest.ProjectedContextHash,
		GraphChangedRequest.ProjectedContextHash);
	TestNotEqual(TEXT("canonical semantic constraints hash uses graph context"),
		ReasonARequest.SemanticConstraintsHash,
		GraphChangedRequest.SemanticConstraintsHash);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSingletonControlFlowProviderRejectsWideSurfaceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.SingletonControlFlowProviderRejectsWideSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSingletonControlFlowProviderRejectsWideSurfaceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericActionTestBlueprint();
	UEdGraph* Graph = GetGenericActionTestGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	const EBlueprintHelperActionSemanticKind RejectedKinds[] = {
		EBlueprintHelperActionSemanticKind::Call,
		EBlueprintHelperActionSemanticKind::Get,
		EBlueprintHelperActionSemanticKind::Set,
		EBlueprintHelperActionSemanticKind::Bind,
		EBlueprintHelperActionSemanticKind::Create,
		EBlueprintHelperActionSemanticKind::Convert,
		EBlueprintHelperActionSemanticKind::Schedule
	};

	for (const EBlueprintHelperActionSemanticKind RejectedKind : RejectedKinds)
	{
		const FBlueprintHelperActionResolutionRequest Request =
			MakeGenericActionRequest(Blueprint, Graph, RejectedKind, TEXT("branch"));
		FBlueprintHelperSingletonControlFlowEvidence Evidence;
		TestFalse(
			FString::Printf(TEXT("provider rejects %s"), *FBlueprintHelperActionResolutionCore::SemanticKindToString(RejectedKind)),
			FBlueprintHelperSingletonControlFlowEvidenceProvider::TryResolve(Request, Evidence));
		TestNull(
			FString::Printf(TEXT("%s rejection has no node class"), *FBlueprintHelperActionResolutionCore::SemanticKindToString(RejectedKind)),
			Evidence.NodeClass.Get());
	}

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
		FString(TEXT("Node")) + FString(TEXT("Handler")),
		TEXT("UBlueprintNodeSpawner::Create(NodeClass)"),
		TEXT("UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass())")
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
