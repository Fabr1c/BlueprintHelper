// BlueprintHelper Review target validity resolver automation tests.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityResolver.h"
#include "Systems/Review/BlueprintHelperReviewValiditySweepCoordinator.h"
#include "Systems/Review/Utils/BlueprintHelperReviewUtils.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace BlueprintHelperReviewTargetValidityResolverTests
{
	struct FSavedBlueprintFixture
	{
		UBlueprint* Blueprint = nullptr;
		FString PackageName;
		FString PackageFilename;
	};

	static FSavedBlueprintFixture CreateSavedBlueprintFixture(
		FAutomationTestBase& Test,
		const FString& Prefix)
	{
		FSavedBlueprintFixture Fixture;
		const FString AssetName = FString::Printf(
			TEXT("BP_ReviewValidity_%s_%s"),
			*Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		Fixture.PackageName = FString::Printf(TEXT("/Game/BlueprintHelperReview/%s"), *AssetName);
		Fixture.PackageFilename = FPackageName::LongPackageNameToFilename(
			Fixture.PackageName,
			FPackageName::GetAssetPackageExtension());

		UPackage* Package = CreatePackage(*Fixture.PackageName);
		if (!Test.TestNotNull(TEXT("test package is created"), Package))
		{
			return Fixture;
		}

		Fixture.Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*AssetName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperReviewTargetValidityResolverTests"));
		if (!Test.TestNotNull(TEXT("test blueprint is created"), Fixture.Blueprint))
		{
			return Fixture;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Fixture.PackageFilename), true);
		FAssetRegistryModule::AssetCreated(Fixture.Blueprint);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!Test.TestTrue(
			TEXT("test blueprint package is saved"),
			UPackage::SavePackage(Package, Fixture.Blueprint, *Fixture.PackageFilename, SaveArgs)))
		{
			Fixture.Blueprint = nullptr;
		}
		Package->SetDirtyFlag(false);
		return Fixture;
	}

	static void CleanupSavedBlueprintFixture(const FSavedBlueprintFixture& Fixture)
	{
		if (Fixture.Blueprint)
		{
			Fixture.Blueprint->ClearFlags(RF_Standalone);
			if (UPackage* Package = Fixture.Blueprint->GetOutermost())
			{
				Package->SetDirtyFlag(false);
			}
		}
		if (!Fixture.PackageFilename.IsEmpty())
		{
			IFileManager::Get().Delete(*Fixture.PackageFilename, false, true);
		}
	}

	static FBlueprintHelperReviewValidityCandidate MakeCandidate(
		const FString& AssetPath,
		const FString& TargetKind,
		const FString& TargetKey)
	{
		FBlueprintHelperReviewValidityCandidate Candidate;
		Candidate.ReviewRecordId = TEXT("review_validity_resolver_test");
		Candidate.ChangeId = TargetKey;
		Candidate.AssetPath = AssetPath;
		Candidate.Target.AssetPath = AssetPath;
		Candidate.Target.TargetKind = TargetKind;
		Candidate.Target.TargetKey = TargetKey;
		Candidate.Target.DisplayLabel = TargetKey;
		return Candidate;
	}

	static FBlueprintHelperReviewVisibleChange MakeVisibleChange(
		const FString& ChangeId,
		const FString& AssetPath,
		const FBlueprintHelperReviewAtomicTarget& Target)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.GraphName = Target.GraphName;
		Change.LocationKey = Target.TargetKey;
		Change.LatestEvidenceId = ChangeId;
		Change.SourceEvidenceIds.Add(ChangeId);
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.DisplayLabel = Target.DisplayLabel;
		Change.AtomicTargets.Add(Target);
		return Change;
	}

	static FString GetReviewRecordPath(const FString& ReviewRecordId)
	{
		return FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Review")
			/ TEXT("Records")
			/ FString::Printf(TEXT("%s.json"), *ReviewRecordId);
	}

	struct FValiditySweepPurgeLatentState
	{
		TSharedPtr<FBlueprintHelperReviewStoreService> Store;
		TSharedPtr<FBlueprintHelperReviewValiditySweepCoordinator> Coordinator;
		FSavedBlueprintFixture Fixture;
		FString ReviewRecordId;
		FString ReviewRecordPath;
		double StartSeconds = FPlatformTime::Seconds();
	};
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FWaitForBlueprintHelperValiditySweepPurge,
	TSharedPtr<BlueprintHelperReviewTargetValidityResolverTests::FValiditySweepPurgeLatentState>,
	State,
	FAutomationTestBase*,
	Test);

bool FWaitForBlueprintHelperValiditySweepPurge::Update()
{
	if (!State.IsValid() || State->ReviewRecordPath.IsEmpty())
	{
		Test->AddError(TEXT("validity sweep purge state is invalid"));
		return true;
	}
	if (!FPaths::FileExists(State->ReviewRecordPath))
	{
		return true;
	}
	if ((FPlatformTime::Seconds() - State->StartSeconds) > 5.0)
	{
		Test->AddError(FString::Printf(
			TEXT("validity sweep did not delete review record file: %s"),
			*State->ReviewRecordPath));
		return true;
	}
	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewTargetValidityBlueprintVariablePrefixTest,
	"BlueprintHelper.Review.Validity.BlueprintVariablePrefixMissingVariableInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewTargetValidityBlueprintVariablePrefixTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReviewTargetValidityResolverTests;

	const FSavedBlueprintFixture Fixture = CreateSavedBlueprintFixture(*this, TEXT("MissingVariable"));
	if (!Fixture.Blueprint)
	{
		CleanupSavedBlueprintFixture(Fixture);
		return false;
	}

	FBlueprintHelperReviewValidityCandidate Candidate = MakeCandidate(
		Fixture.PackageName,
		TEXT("blueprint_variable"),
		TEXT("blueprint_variable:MissingReviewVariable"));

	const FBlueprintHelperReviewTargetValidityResolver Resolver;
	const FBlueprintHelperReviewValidityResult Result = Resolver.ValidateOnGameThread(Candidate);

	TestFalse(TEXT("missing blueprint_variable target is invalid"), Result.bValid);
	TestEqual(
		TEXT("missing blueprint_variable reports variable reason"),
		Result.InvalidReason,
		EBlueprintHelperReviewInvalidReason::VariableMissingOrRenamed);

	CleanupSavedBlueprintFixture(Fixture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewTargetValidityGraphBlockMissingTest,
	"BlueprintHelper.Review.Validity.GraphBlockMissingBlockInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewTargetValidityGraphBlockMissingTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReviewTargetValidityResolverTests;

	const FSavedBlueprintFixture Fixture = CreateSavedBlueprintFixture(*this, TEXT("MissingGraphBlock"));
	if (!Fixture.Blueprint)
	{
		CleanupSavedBlueprintFixture(Fixture);
		return false;
	}

	const UEdGraph* EventGraph =
		UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(Fixture.Blueprint, TEXT("EventGraph"));
	if (!TestNotNull(TEXT("test blueprint has EventGraph"), EventGraph))
	{
		CleanupSavedBlueprintFixture(Fixture);
		return false;
	}

	FBlueprintHelperReviewValidityCandidate Candidate = MakeCandidate(
		Fixture.PackageName,
		TEXT("graph_block"),
		TEXT("graph:EventGraph:block:MissingReviewBlock"));
	Candidate.Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Candidate.Target.GraphName = TEXT("EventGraph");
	Candidate.Target.VisualGroupKey = Candidate.Target.TargetKey;

	const FBlueprintHelperReviewTargetValidityResolver Resolver;
	const FBlueprintHelperReviewValidityResult Result = Resolver.ValidateOnGameThread(Candidate);

	TestFalse(TEXT("missing graph block target is invalid"), Result.bValid);
	TestEqual(
		TEXT("missing graph block reports graph node reason"),
		Result.InvalidReason,
		EBlueprintHelperReviewInvalidReason::GraphNodeMissing);

	CleanupSavedBlueprintFixture(Fixture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewValiditySweepPurgesInvalidTargetsTest,
	"BlueprintHelper.Review.ValiditySweep.PurgesInvalidTargetsAndDeletesRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewValiditySweepPurgesInvalidTargetsTest::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperReviewTargetValidityResolverTests;

	TSharedPtr<FValiditySweepPurgeLatentState> State = MakeShared<FValiditySweepPurgeLatentState>();
	State->Store = MakeShared<FBlueprintHelperReviewStoreService>();
	State->Fixture = CreateSavedBlueprintFixture(*this, TEXT("SweepPurge"));
	if (!State->Fixture.Blueprint)
	{
		CleanupSavedBlueprintFixture(State->Fixture);
		return false;
	}

	FBlueprintHelperReviewAtomicTarget VariableTarget;
	VariableTarget.AssetPath = State->Fixture.PackageName;
	VariableTarget.Surface = EBlueprintHelperReviewSurface::Details;
	VariableTarget.TargetKind = TEXT("blueprint_variable");
	VariableTarget.TargetKey = TEXT("blueprint_variable:MissingReviewVariable");
	VariableTarget.DisplayLabel = TEXT("MissingReviewVariable");

	FBlueprintHelperReviewAtomicTarget GraphBlockTarget;
	GraphBlockTarget.AssetPath = State->Fixture.PackageName;
	GraphBlockTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphBlockTarget.GraphName = TEXT("EventGraph");
	GraphBlockTarget.TargetKind = TEXT("graph_block");
	GraphBlockTarget.TargetKey = TEXT("graph:EventGraph:block:MissingReviewBlock");
	GraphBlockTarget.VisualGroupKey = GraphBlockTarget.TargetKey;
	GraphBlockTarget.DisplayLabel = TEXT("MissingReviewBlock");

	const FString ArchiveId = TEXT("archive_validity_sweep_purge_")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString TaskRunId = TEXT("task_validity_sweep_purge");
	FBlueprintHelperReviewRecord Record;
	Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
		ArchiveId,
		State->Fixture.PackageName);
	Record.ArchiveSessionId = ArchiveId;
	Record.AssetPath = State->Fixture.PackageName;
	Record.SourceTaskRunIds.Add(TaskRunId);
	Record.SourceReviewSummary.TaskRunIds.Add(TaskRunId);
	Record.SourceReviewSummary.AssetPaths.Add(State->Fixture.PackageName);
	Record.SourceReviewSummary.CreatedAtFirst = ArchiveId;
	Record.SourceReviewSummary.CreatedAtLast = ArchiveId;
	Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
	Record.StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
	Record.VisibleChanges.Add(MakeVisibleChange(
		TEXT("change_missing_variable"),
		State->Fixture.PackageName,
		VariableTarget));
	Record.VisibleChanges.Add(MakeVisibleChange(
		TEXT("change_missing_graph_block"),
		State->Fixture.PackageName,
		GraphBlockTarget));

	FString SaveError;
	TestTrue(TEXT("review record saves before sweep"), State->Store->SaveReviewRecord(Record, SaveError));
	State->ReviewRecordId = Record.ReviewRecordId;
	State->ReviewRecordPath = GetReviewRecordPath(Record.ReviewRecordId);
	TestTrue(TEXT("review record file exists before sweep"), FPaths::FileExists(State->ReviewRecordPath));

	FBlueprintHelperReviewPerformanceSettings Settings;
	Settings.bValiditySweepEnabled = true;
	Settings.ValiditySweepMaxGameThreadTargetsPerFrame = 16;
	Settings.ValiditySweepMaxInvalidPurgesPerBatch = 16;
	State->Coordinator = MakeShared<FBlueprintHelperReviewValiditySweepCoordinator>(State->Store.Get(), Settings);

	TArray<FBlueprintHelperReviewValidityCandidate> Candidates;
	FBlueprintHelperReviewValidityCandidate VariableCandidate = MakeCandidate(
		State->Fixture.PackageName,
		VariableTarget.TargetKind,
		VariableTarget.TargetKey);
	VariableCandidate.ReviewRecordId = Record.ReviewRecordId;
	VariableCandidate.ChangeId = TEXT("change_missing_variable");
	Candidates.Add(VariableCandidate);

	FBlueprintHelperReviewValidityCandidate GraphBlockCandidate = MakeCandidate(
		State->Fixture.PackageName,
		GraphBlockTarget.TargetKind,
		GraphBlockTarget.TargetKey);
	GraphBlockCandidate.ReviewRecordId = Record.ReviewRecordId;
	GraphBlockCandidate.ChangeId = TEXT("change_missing_graph_block");
	GraphBlockCandidate.Target.Surface = EBlueprintHelperReviewSurface::Graph;
	GraphBlockCandidate.Target.GraphName = TEXT("EventGraph");
	GraphBlockCandidate.Target.VisualGroupKey = GraphBlockTarget.VisualGroupKey;
	Candidates.Add(GraphBlockCandidate);

	State->Coordinator->EnqueueCandidatesFromPendingLoad(TEXT("test_validity_sweep_purge"), MoveTemp(Candidates));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForBlueprintHelperValiditySweepPurge(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		TestFalse(
			TEXT("invalid sweep deletes review record file"),
			FPaths::FileExists(State->ReviewRecordPath));
		if (State->Coordinator.IsValid())
		{
			State->Coordinator->Cancel();
			State->Coordinator.Reset();
		}
		if (State->Store.IsValid() && !State->ReviewRecordId.IsEmpty())
		{
			FString DeleteError;
			State->Store->DeleteReviewRecord(State->ReviewRecordId, DeleteError);
		}
		CleanupSavedBlueprintFixture(State->Fixture);
		return true;
	}));
	return true;
}

#endif
