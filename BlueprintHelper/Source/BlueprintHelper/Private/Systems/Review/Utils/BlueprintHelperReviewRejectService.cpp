// BlueprintHelper Review BlueprintHelperReviewRejectService implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewRejectService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"
#include "BlueprintHelperReviewUtils.h"

	static void BlueprintHelperReviewCopyHashGuardDiagnostic(
		FBlueprintHelperReviewActionResult& Result,
		const FBlueprintHelperReviewActionResult& Diagnostic)
	{
		Result.HashGuardTargetKey = Diagnostic.HashGuardTargetKey;
		Result.HashGuardExpectedHash = Diagnostic.HashGuardExpectedHash;
		Result.HashGuardCurrentHash = Diagnostic.HashGuardCurrentHash;
		Result.HashGuardCurrentSnapshotJson = Diagnostic.HashGuardCurrentSnapshotJson;
		Result.HashGuardRecordedAfterSnapshotJson = Diagnostic.HashGuardRecordedAfterSnapshotJson;
	}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewRejectOptions* Options)
	{
		if (Change.AtomicTargets.Num() == 0)
		{
			return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::NeedsAction,
				TEXT("missing_atomic_targets"));
		}

		FBlueprintHelperReviewActionResult CurrentStateDiagnostic;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (FBlueprintHelperReviewSnapshotRestoreService::IsAssetFactoryTarget(Target))
			{
				const FBlueprintHelperReviewActionResult AssetFactoryResult =
					FBlueprintHelperReviewSnapshotRestoreService::RejectAssetFactoryTargetWithDefaultDispatcher(Change, Target);
				if (!AssetFactoryResult.bSucceeded)
				{
					return AssetFactoryResult;
				}
				continue;
			}

			if (Target.TargetKey.IsEmpty())
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_anchor"));
			}
			UBlueprintHelperReviewUtils::CaptureReviewRejectCurrentStateDiagnostic(Target, CurrentStateDiagnostic);
			if (FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind)
				&& Target.BeforeSnapshotJson.IsEmpty())
			{
				FBlueprintHelperReviewActionResult Result =
					FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
						Change,
						EBlueprintHelperReviewChangeStatus::NeedsAction,
						TEXT("missing_recoverable_snapshot"));
				BlueprintHelperReviewCopyHashGuardDiagnostic(Result, CurrentStateDiagnostic);
				return Result;
			}
			if (FBlueprintHelperReviewSnapshotRestoreService::ShouldUseSnapshotRestore(Target))
			{
				FString SnapshotRestoreError;
				if (!FBlueprintHelperReviewSnapshotRestoreService::ExecuteSnapshotRestore(Target, SnapshotRestoreError))
				{
					FBlueprintHelperReviewActionResult Result =
						FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
							Change,
							SnapshotRestoreError.Contains(TEXT("_recreate_required"))
								? EBlueprintHelperReviewChangeStatus::NeedsAction
								: EBlueprintHelperReviewChangeStatus::RejectFailed,
							SnapshotRestoreError);
					BlueprintHelperReviewCopyHashGuardDiagnostic(Result, CurrentStateDiagnostic);
					return Result;
				}
				continue;
			}
			FBlueprintHelperReviewActionResult Result =
				FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("snapshot_restore_unsupported_target_kind:%s"), *Target.TargetKind));
			BlueprintHelperReviewCopyHashGuardDiagnostic(Result, CurrentStateDiagnostic);
			return Result;
		}

		FBlueprintHelperReviewActionResult Result;
		Result.bSucceeded = true;
		Result.TargetEvidenceId = Change.LatestEvidenceId;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("rejected");
		Result.bSupersededDataCompactionEligible = true;
		BlueprintHelperReviewCopyHashGuardDiagnostic(Result, CurrentStateDiagnostic);
		return Result;
	}

	static bool BlueprintHelperReviewIsCascadeActionable(const FBlueprintHelperReviewVisibleChange& Change)
	{
		return Change.Status == EBlueprintHelperReviewChangeStatus::Pending
			|| Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

	static int32 BlueprintHelperReviewDescendingOrderValue(int32 Value)
	{
		return Value == INDEX_NONE ? MIN_int32 : Value;
	}

TArray<FBlueprintHelperReviewVisibleChange> FBlueprintHelperReviewRejectService::CollectLifecycleDescendantsDeepestFirst(
		const FBlueprintHelperReviewVisibleChange& Root,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges)
	{
		TArray<FBlueprintHelperReviewVisibleChange> Descendants;
		if (Root.ChangeId.IsEmpty())
		{
			return Descendants;
		}

		TSet<FString> CollectedChangeIds;
		TMap<FString, int32> DepthByChangeId;
		CollectedChangeIds.Add(Root.ChangeId);
		DepthByChangeId.Add(Root.ChangeId, 0);

		bool bAddedThisPass = true;
		while (bAddedThisPass)
		{
			bAddedThisPass = false;
			for (const FBlueprintHelperReviewVisibleChange& PendingChange : PendingChanges)
			{
				if (!BlueprintHelperReviewIsCascadeActionable(PendingChange)
					|| PendingChange.ChangeId.IsEmpty()
					|| CollectedChangeIds.Contains(PendingChange.ChangeId)
					|| PendingChange.ParentChangeId.IsEmpty()
					|| !CollectedChangeIds.Contains(PendingChange.ParentChangeId))
				{
					continue;
				}

				const int32 ParentDepth = DepthByChangeId.Contains(PendingChange.ParentChangeId)
					? DepthByChangeId[PendingChange.ParentChangeId]
					: 0;
				CollectedChangeIds.Add(PendingChange.ChangeId);
				DepthByChangeId.Add(PendingChange.ChangeId, ParentDepth + 1);
				Descendants.Add(PendingChange);
				bAddedThisPass = true;
			}
		}

		Descendants.Sort([&DepthByChangeId](
			const FBlueprintHelperReviewVisibleChange& Left,
			const FBlueprintHelperReviewVisibleChange& Right)
		{
			const int32 LeftDepth = DepthByChangeId.Contains(Left.ChangeId) ? DepthByChangeId[Left.ChangeId] : 0;
			const int32 RightDepth = DepthByChangeId.Contains(Right.ChangeId) ? DepthByChangeId[Right.ChangeId] : 0;
			if (LeftDepth != RightDepth)
			{
				return LeftDepth > RightDepth;
			}

			const int32 LeftExecutionOrder = BlueprintHelperReviewDescendingOrderValue(Left.ExecutionOrder);
			const int32 RightExecutionOrder = BlueprintHelperReviewDescendingOrderValue(Right.ExecutionOrder);
			if (LeftExecutionOrder != RightExecutionOrder)
			{
				return LeftExecutionOrder > RightExecutionOrder;
			}

			const int32 LeftTaskStep = BlueprintHelperReviewDescendingOrderValue(Left.TaskStepIndex);
			const int32 RightTaskStep = BlueprintHelperReviewDescendingOrderValue(Right.TaskStepIndex);
			if (LeftTaskStep != RightTaskStep)
			{
				return LeftTaskStep > RightTaskStep;
			}

			const int32 LeftAtomic = BlueprintHelperReviewDescendingOrderValue(Left.AtomicIndex);
			const int32 RightAtomic = BlueprintHelperReviewDescendingOrderValue(Right.AtomicIndex);
			if (LeftAtomic != RightAtomic)
			{
				return LeftAtomic > RightAtomic;
			}

			return Left.ChangeId > Right.ChangeId;
		});
		return Descendants;
	}
FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewRejectService::CascadeRejectLifecycleChildrenAfterRootResult(
		const FBlueprintHelperReviewVisibleChange& Root,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		const FBlueprintHelperReviewActionResult& RootResult,
		const FString& ResolvedReviewRecordId)
	{
		FBlueprintHelperReviewCascadeActionResult CascadeResult;
		CascadeResult.RootResult = RootResult;
		if (!RootResult.bSucceeded || !Root.bRejectRemovesChildren || Root.ChangeId.IsEmpty())
		{
			return CascadeResult;
		}

		TSet<FString> ChildChangeIds;
		TSet<FString> ChildReviewRecordIdsToDelete;
		TSet<FString> CascadeRootChangeIds;
		CascadeRootChangeIds.Add(Root.ChangeId);
		const bool bRootIsAssetFactory = Root.AtomicTargets.ContainsByPredicate(
			[](const FBlueprintHelperReviewAtomicTarget& Target)
			{
				return FBlueprintHelperReviewTargetKindRegistry::IsAssetFactoryTargetKind(Target.TargetKind);
			});
		bool bAddedChildThisPass = true;
		while (bAddedChildThisPass)
		{
			bAddedChildThisPass = false;
			for (const FBlueprintHelperReviewVisibleChange& PendingChange : PendingChanges)
			{
				const bool bLinkedChild =
					!bRootIsAssetFactory
					&& CascadeRootChangeIds.Contains(PendingChange.ParentChangeId);
				const bool bAssetLifecycleChild =
					bRootIsAssetFactory
					&& PendingChange.AssetPath == Root.AssetPath
					&& PendingChange.ChangeId != Root.ChangeId;
				if (!BlueprintHelperReviewIsCascadeActionable(PendingChange)
					|| PendingChange.ChangeId.IsEmpty()
					|| ChildChangeIds.Contains(PendingChange.ChangeId)
					|| (!bLinkedChild && !bAssetLifecycleChild))
				{
					continue;
				}

				ChildChangeIds.Add(PendingChange.ChangeId);
				CascadeRootChangeIds.Add(PendingChange.ChangeId);
				bAddedChildThisPass = true;

				FString ChildReviewRecordId;
				TArray<FString> ChildTargetKeys;
				if (FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(
					PendingChange,
					ChildReviewRecordId,
					ChildTargetKeys)
					&& !ChildReviewRecordId.IsEmpty())
				{
					ChildReviewRecordIdsToDelete.Add(ChildReviewRecordId);
				}
			}
		}

		FString ReviewRecordId = ResolvedReviewRecordId;
		TArray<FString> RootTargetKeys;
		if (ReviewRecordId.IsEmpty()
			&& !FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(Root, ReviewRecordId, RootTargetKeys))
		{
			return CascadeResult;
		}

		FBlueprintHelperReviewStoreService Store;
		FString Error;
		if (!FBlueprintHelperReviewActionRecordUtils::DeleteReviewRecordAndLinkedDebugCases(Store, ReviewRecordId, Error))
		{
			CascadeResult.RootResult.bSucceeded = false;
			CascadeResult.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			CascadeResult.RootResult.Message = Error;
			return CascadeResult;
		}

		ChildReviewRecordIdsToDelete.Remove(ReviewRecordId);
		for (const FString& ChildReviewRecordId : ChildReviewRecordIdsToDelete)
		{
			if (!FBlueprintHelperReviewActionRecordUtils::DeleteReviewRecordAndLinkedDebugCases(Store, ChildReviewRecordId, Error))
			{
				CascadeResult.RootResult.bSucceeded = false;
				CascadeResult.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
				CascadeResult.RootResult.Message = Error;
				return CascadeResult;
			}
		}

		for (const FString& ChildChangeId : ChildChangeIds)
		{
			CascadeResult.RemovedChildChangeIds.Add(ChildChangeId);
		}
		CascadeResult.bChildrenRemoved = CascadeResult.RemovedChildChangeIds.Num() > 0;
		return CascadeResult;
	}
