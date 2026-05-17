// BlueprintHelper Review BlueprintHelperReviewActionRecordUtils implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"

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
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"

FBlueprintHelperReviewActionRecord FBlueprintHelperReviewActionRecordUtils::MakeReviewActionRecord(
		const FString& Action,
		const TArray<FString>& TargetKeys,
		const FString& OwnershipPolicy,
		const FString& SourceTransactionId,
		const FString& Message)
	{
		FBlueprintHelperReviewActionRecord ActionRecord;
		ActionRecord.Action = Action;
		ActionRecord.TargetKeys = TargetKeys;
		ActionRecord.OwnershipPolicy = OwnershipPolicy;
		ActionRecord.SourceTransactionId = SourceTransactionId;
		ActionRecord.Message = Message;
		ActionRecord.CreatedAt = FDateTime::UtcNow().ToIso8601();
		return ActionRecord;
	}
bool FBlueprintHelperReviewActionRecordUtils::DeleteDebugCasesForReviewRecord(
		const FString& ReviewRecordId,
		const TArray<FString>& ExplicitDebugCaseIds,
		FString& OutError)
	{
		FBlueprintHelperDebugCaseStoreService DebugStore;
		for (const FString& DebugCaseId : ExplicitDebugCaseIds)
		{
			if (DebugCaseId.IsEmpty())
			{
				continue;
			}

			FString DeleteError;
			if (!DebugStore.DeleteCase(DebugCaseId, &DeleteError))
			{
				OutError = DeleteError;
				return false;
			}
		}

		TArray<FString> DeletedCaseIds;
		if (!DebugStore.DeleteCasesForReviewRecord(ReviewRecordId, DeletedCaseIds, &OutError))
		{
			return false;
		}

		OutError.Reset();
		return true;
	}
bool FBlueprintHelperReviewActionRecordUtils::DeleteReviewRecordAndLinkedDebugCases(
		FBlueprintHelperReviewStoreService& Store,
		const FString& ReviewRecordId,
		FString& OutError)
	{
		TArray<FString> DebugCaseIdsToDelete;
		FBlueprintHelperReviewRecord ExistingRecord;
		FString LoadError;
		if (Store.LoadReviewRecordById(ReviewRecordId, ExistingRecord, LoadError))
		{
			DebugCaseIdsToDelete = ExistingRecord.DebugCaseIds;
		}

		if (!Store.DeleteReviewRecord(ReviewRecordId, OutError))
		{
			return false;
		}

		return DeleteDebugCasesForReviewRecord(ReviewRecordId, DebugCaseIdsToDelete, OutError);
	}
bool FBlueprintHelperReviewActionRecordUtils::HasInjectedRejectOptions(const FBlueprintHelperReviewRejectOptions& Options)
	{
		return Options.CurrentHashesByTargetKey.Num() > 0
			|| Options.bRollbackExecutorAvailable
			|| Options.bRollbackSucceeded
			|| !Options.RollbackFailureMessage.IsEmpty();
	}
TSharedRef<FJsonObject> FBlueprintHelperReviewActionRecordUtils::BuildJournalRecordFromPreparedRollbackJournal(
		const FBlueprintHelperReviewPreparedRollbackJournal& Prepared)
	{
		TSharedRef<FJsonObject> JournalRecord = MakeShared<FJsonObject>();
		JournalRecord->SetStringField(TEXT("tool"), Prepared.Tool);
		if (Prepared.bHasRollbackData)
		{
			TSharedRef<FJsonObject> RollbackData = MakeShared<FJsonObject>();
			RollbackData->SetStringField(TEXT("exported_text"), Prepared.ExportedText);
			RollbackData->SetStringField(TEXT("entry_identity"), Prepared.EntryIdentity);
			RollbackData->SetStringField(TEXT("replace_scope"), Prepared.ReplaceScope);
			RollbackData->SetStringField(TEXT("owner_block_id"), Prepared.OwnerBlockId);
			JournalRecord->SetObjectField(TEXT("rollback_data"), RollbackData);
		}
		return JournalRecord;
	}
FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewChangeStatus Status,
		const FString& Message)
	{
		FBlueprintHelperReviewActionResult Result;
		Result.TargetTransactionId = Change.LatestTransactionId;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.NewStatus = Status;
		Result.Message = Message;
		return Result;
	}
