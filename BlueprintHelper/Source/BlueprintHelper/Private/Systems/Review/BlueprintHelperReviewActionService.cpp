// BlueprintHelper Review action service implementation.

#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewHashService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Components/ActorComponent.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperReviewActionServiceLocalUtils
{
public:
	struct FPersistedReviewTargetMatch
	{
		FString ReviewRecordId;
		TArray<FString> TargetKeys;
	};

	static bool ReviewTargetMatches(const FBlueprintHelperReviewAtomicTarget& Target, const TArray<FString>& TargetKeys)
	{
		return TargetKeys.Num() == 0 || TargetKeys.Contains(Target.TargetKey);
	}

	static EBlueprintHelperReviewChangeStatus CombineTargetStatuses(
		const TArray<FBlueprintHelperReviewAtomicTarget>& Targets)
	{
		if (Targets.Num() == 0)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}

		bool bAllAccepted = true;
		bool bAllRejected = true;
		bool bAnyNeedsAction = false;
		bool bAnyRejectFailed = false;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Targets)
		{
			bAllAccepted &= Target.Status == EBlueprintHelperReviewChangeStatus::Accepted;
			bAllRejected &= Target.Status == EBlueprintHelperReviewChangeStatus::Rejected;
			bAnyNeedsAction |= Target.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
			bAnyRejectFailed |= Target.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
		}

		if (bAnyRejectFailed)
		{
			return EBlueprintHelperReviewChangeStatus::RejectFailed;
		}
		if (bAnyNeedsAction)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}
		if (bAllAccepted)
		{
			return EBlueprintHelperReviewChangeStatus::Accepted;
		}
		if (bAllRejected)
		{
			return EBlueprintHelperReviewChangeStatus::Rejected;
		}
		return EBlueprintHelperReviewChangeStatus::Pending;
	}

	static EBlueprintHelperReviewChangeStatus CombineChangeStatuses(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		if (Changes.Num() == 0)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}

		bool bAllAccepted = true;
		bool bAllRejected = true;
		bool bAnyPending = false;
		bool bAnyNeedsAction = false;
		bool bAnyRejectFailed = false;
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			bAllAccepted &= Change.Status == EBlueprintHelperReviewChangeStatus::Accepted;
			bAllRejected &= Change.Status == EBlueprintHelperReviewChangeStatus::Rejected;
			bAnyPending |= Change.Status == EBlueprintHelperReviewChangeStatus::Pending;
			bAnyNeedsAction |= Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
			bAnyRejectFailed |= Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
		}

		if (bAnyRejectFailed)
		{
			return EBlueprintHelperReviewChangeStatus::RejectFailed;
		}
		if (bAnyNeedsAction)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}
		if (bAnyPending)
		{
			return EBlueprintHelperReviewChangeStatus::Pending;
		}
		if (bAllAccepted)
		{
			return EBlueprintHelperReviewChangeStatus::Accepted;
		}
		if (bAllRejected)
		{
			return EBlueprintHelperReviewChangeStatus::Rejected;
		}
		return EBlueprintHelperReviewChangeStatus::Pending;
	}

	static void RefreshReviewRecordStatus(FBlueprintHelperReviewRecord& Record)
	{
		for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			Change.Status = CombineTargetStatuses(Change.AtomicTargets);
		}
		Record.Status = CombineChangeStatuses(Record.VisibleChanges);
		Record.SourceTransactionSummary.FinalReviewStatus = Record.Status;
	}

	static FBlueprintHelperReviewActionRecord MakeReviewActionRecord(
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

	static TArray<FString> CollectPendingTargetKeys(const FBlueprintHelperReviewRecord& Record)
	{
		TArray<FString> TargetKeys;
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
			{
				if (Target.Status == EBlueprintHelperReviewChangeStatus::Pending)
				{
					TargetKeys.AddUnique(Target.TargetKey);
				}
			}
		}
		return TargetKeys;
	}

	static TArray<FString> CollectTargetKeysFromVisibleChange(const FBlueprintHelperReviewVisibleChange& Change)
	{
		TArray<FString> TargetKeys;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!Target.TargetKey.IsEmpty())
			{
				TargetKeys.AddUnique(Target.TargetKey);
			}
		}
		return TargetKeys;
	}

	static FString MakeReviewPackageKey(FString AssetPath)
	{
		AssetPath.TrimStartAndEndInline();
		if (AssetPath.IsEmpty())
		{
			return FString();
		}
		if (FPackageName::IsValidObjectPath(AssetPath))
		{
			return FPackageName::ObjectPathToPackageName(AssetPath);
		}

		int32 SubObjectIndex = INDEX_NONE;
		if (AssetPath.FindChar(TEXT(':'), SubObjectIndex))
		{
			AssetPath = AssetPath.Left(SubObjectIndex);
		}

		int32 ObjectIndex = INDEX_NONE;
		if (AssetPath.FindChar(TEXT('.'), ObjectIndex))
		{
			AssetPath = AssetPath.Left(ObjectIndex);
		}
		return AssetPath;
	}

	static bool ReviewAssetPathMatches(const FString& Left, const FString& Right)
	{
		const FString LeftKey = MakeReviewPackageKey(Left);
		const FString RightKey = MakeReviewPackageKey(Right);
		return !LeftKey.IsEmpty() && !RightKey.IsEmpty() && LeftKey == RightKey;
	}

	static bool IntersectTargetKeys(
		const TArray<FString>& RequestedTargetKeys,
		const TArray<FString>& CandidateTargetKeys,
		TArray<FString>& OutMatchedTargetKeys)
	{
		OutMatchedTargetKeys.Reset();
		for (const FString& CandidateTargetKey : CandidateTargetKeys)
		{
			if (!CandidateTargetKey.IsEmpty() && RequestedTargetKeys.Contains(CandidateTargetKey))
			{
				OutMatchedTargetKeys.AddUnique(CandidateTargetKey);
			}
		}
		return OutMatchedTargetKeys.Num() > 0;
	}

	static void AddPersistedReviewTargetMatch(
		TArray<FPersistedReviewTargetMatch>& Matches,
		const FString& ReviewRecordId,
		const TArray<FString>& TargetKeys)
	{
		if (ReviewRecordId.IsEmpty() || TargetKeys.Num() == 0)
		{
			return;
		}

		FPersistedReviewTargetMatch* Existing = Matches.FindByPredicate(
			[&ReviewRecordId](const FPersistedReviewTargetMatch& Candidate)
			{
				return Candidate.ReviewRecordId == ReviewRecordId;
			});
		if (!Existing)
		{
			FPersistedReviewTargetMatch NewMatch;
			NewMatch.ReviewRecordId = ReviewRecordId;
			Matches.Add(NewMatch);
			Existing = &Matches.Last();
		}

		for (const FString& TargetKey : TargetKeys)
		{
			if (!TargetKey.IsEmpty())
			{
				Existing->TargetKeys.AddUnique(TargetKey);
			}
		}
	}

	static TArray<FPersistedReviewTargetMatch> ResolvePersistedReviewTargetMatches(
		const FBlueprintHelperReviewVisibleChange& Change)
	{
		TArray<FPersistedReviewTargetMatch> Matches;
		if (Change.AssetPath.IsEmpty())
		{
			return Matches;
		}

		const TArray<FString> RequestedTargetKeys = CollectTargetKeysFromVisibleChange(Change);
		FBlueprintHelperReviewRecordQuery Query;
		Query.bPendingOnly = false;

		FBlueprintHelperReviewStoreService Store;
		const TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
		for (const FBlueprintHelperReviewRecord& Record : Records)
		{
			if (!ReviewAssetPathMatches(Change.AssetPath, Record.AssetPath))
			{
				continue;
			}

			for (const FBlueprintHelperReviewVisibleChange& Candidate : Record.VisibleChanges)
			{
				if (!ReviewAssetPathMatches(Change.AssetPath, Candidate.AssetPath))
				{
					continue;
				}
				if (Candidate.Status != EBlueprintHelperReviewChangeStatus::Pending
					&& Candidate.Status != EBlueprintHelperReviewChangeStatus::NeedsAction
					&& Candidate.Status != EBlueprintHelperReviewChangeStatus::RejectFailed)
				{
					continue;
				}

				const TArray<FString> CandidateTargetKeys = CollectTargetKeysFromVisibleChange(Candidate);
				TArray<FString> MatchedTargetKeys;
				const bool bHasRequestedTarget = RequestedTargetKeys.Num() > 0
					&& IntersectTargetKeys(RequestedTargetKeys, CandidateTargetKeys, MatchedTargetKeys);
				const bool bSameChangeIdentity =
					(!Change.ChangeId.IsEmpty() && Candidate.ChangeId == Change.ChangeId) ||
					(!Change.LocationKey.IsEmpty() && Candidate.LocationKey == Change.LocationKey) ||
					(!Change.LatestTransactionId.IsEmpty() && Candidate.LatestTransactionId == Change.LatestTransactionId);

				if (!bHasRequestedTarget && !bSameChangeIdentity)
				{
					continue;
				}
				if (RequestedTargetKeys.Num() == 0)
				{
					MatchedTargetKeys = CandidateTargetKeys;
				}
				if (MatchedTargetKeys.Num() == 0)
				{
					continue;
				}

				AddPersistedReviewTargetMatch(Matches, Record.ReviewRecordId, MatchedTargetKeys);
			}
		}

		return Matches;
	}

	static bool DeleteDebugCasesForReviewRecord(
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

	static bool DeleteReviewRecordAndLinkedDebugCases(
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

	static bool TryResolvePersistedReviewChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		FString& OutReviewRecordId,
		TArray<FString>& OutTargetKeys)
	{
		const TArray<FPersistedReviewTargetMatch> Matches = ResolvePersistedReviewTargetMatches(Change);
		if (Matches.Num() == 0)
		{
			return false;
		}

		OutReviewRecordId = Matches[0].ReviewRecordId;
		OutTargetKeys = Matches[0].TargetKeys;
		return !OutReviewRecordId.IsEmpty() && OutTargetKeys.Num() > 0;
	}

	static bool HasInjectedRejectOptions(const FBlueprintHelperReviewRejectOptions& Options)
	{
		return Options.CurrentHashesByTargetKey.Num() > 0
			|| Options.bRollbackExecutorAvailable
			|| Options.bRollbackSucceeded
			|| !Options.RollbackFailureMessage.IsEmpty();
	}

	static FBlueprintHelperReviewActionResult MakeRejectFailureResult(
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

	static bool IsAssetFactoryTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return Target.TargetKind.Equals(TEXT("asset_factory"), ESearchCase::IgnoreCase)
			|| Target.TargetKey.StartsWith(TEXT("asset_factory:"), ESearchCase::IgnoreCase);
	}

	static FString ExtractTargetName(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		if (!Target.PropertyPath.IsEmpty())
		{
			return Target.PropertyPath;
		}
		if (!Target.ComponentPath.IsEmpty())
		{
			return Target.ComponentPath;
		}

		int32 LastColon = INDEX_NONE;
		if (Target.TargetKey.FindLastChar(TEXT(':'), LastColon))
		{
			return Target.TargetKey.Mid(LastColon + 1);
		}
		return Target.DisplayLabel;
	}

	static void SplitWidgetPropertyTarget(
		const FString& TargetName,
		FString& OutWidgetName,
		FString& OutPropertyName)
	{
		OutWidgetName = TargetName;
		OutPropertyName.Reset();

		FString Left;
		FString Right;
		if (TargetName.Split(TEXT("."), &Left, &Right) && !Left.IsEmpty())
		{
			OutWidgetName = Left;
			OutPropertyName = Right;
		}
	}

	static bool ParseReviewSnapshotJson(
		const FString& SnapshotJson,
		TSharedPtr<FJsonObject>& OutSnapshot,
		FString& OutError)
	{
		if (SnapshotJson.IsEmpty())
		{
			OutError = TEXT("missing_before_snapshot_json");
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SnapshotJson);
		if (!FJsonSerializer::Deserialize(Reader, OutSnapshot) || !OutSnapshot.IsValid())
		{
			OutError = TEXT("before_snapshot_json_parse_failed");
			return false;
		}
		return true;
	}

	static int32 FindBlueprintVariableIndex(UBlueprint* Blueprint, const FName VariableName)
	{
		if (!Blueprint)
		{
			return INDEX_NONE;
		}

		for (int32 Index = 0; Index < Blueprint->NewVariables.Num(); ++Index)
		{
			if (Blueprint->NewVariables[Index].VarName == VariableName)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	static USCS_Node* FindScsNodeByName(UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static void MarkBlueprintReviewRestoreModified(UBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}
	}

	static bool RestoreBlueprintVariableFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString VariableName = ExtractTargetName(Target);
		if (VariableName.IsEmpty())
		{
			OutError = TEXT("missing_variable_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		const FName VariableFName(*VariableName);
		const int32 VariableIndex = FindBlueprintVariableIndex(Blueprint, VariableFName);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Variable")));
		Blueprint->Modify();
		if (!bSnapshotExists)
		{
			if (VariableIndex != INDEX_NONE)
			{
				FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VariableFName);
				MarkBlueprintReviewRestoreModified(Blueprint);
			}
			return true;
		}

		if (VariableIndex == INDEX_NONE)
		{
			FString PinCategory;
			FString PinSubCategory;
			FString PinSubCategoryObjectPath;
			Snapshot->TryGetStringField(TEXT("pin_category"), PinCategory);
			Snapshot->TryGetStringField(TEXT("pin_sub_category"), PinSubCategory);
			Snapshot->TryGetStringField(TEXT("pin_sub_category_object"), PinSubCategoryObjectPath);
			if (PinCategory.IsEmpty())
			{
				OutError = FString::Printf(TEXT("snapshot_restore_variable_missing_type:%s"), *VariableName);
				return false;
			}

			FEdGraphPinType PinType;
			PinType.PinCategory = FName(*PinCategory);
			PinType.PinSubCategory = FName(*PinSubCategory);
			if (!PinSubCategoryObjectPath.IsEmpty())
			{
				PinType.PinSubCategoryObject = LoadObject<UObject>(nullptr, *PinSubCategoryObjectPath);
			}

			if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableFName, PinType))
			{
				OutError = FString::Printf(TEXT("snapshot_restore_variable_recreate_failed:%s"), *VariableName);
				return false;
			}

			const int32 NewVariableIndex = FindBlueprintVariableIndex(Blueprint, VariableFName);
			if (NewVariableIndex == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_variable_recreate_missing:%s"), *VariableName);
				return false;
			}

			FString Category;
			if (Snapshot->TryGetStringField(TEXT("category"), Category))
			{
				Blueprint->NewVariables[NewVariableIndex].Category = FText::FromString(Category);
			}

			FString GuidString;
			FGuid ParsedGuid;
			if (Snapshot->TryGetStringField(TEXT("guid"), GuidString) && FGuid::Parse(GuidString, ParsedGuid))
			{
				Blueprint->NewVariables[NewVariableIndex].VarGuid = ParsedGuid;
			}

			FString DefaultValue;
			if (Snapshot->TryGetStringField(TEXT("default_value"), DefaultValue))
			{
				Blueprint->NewVariables[NewVariableIndex].DefaultValue = DefaultValue;
			}
			MarkBlueprintReviewRestoreModified(Blueprint);
			return true;
		}

		FString DefaultValue;
		if (Snapshot->TryGetStringField(TEXT("default_value"), DefaultValue))
		{
			Blueprint->NewVariables[VariableIndex].DefaultValue = DefaultValue;
		}
		MarkBlueprintReviewRestoreModified(Blueprint);
		return true;
	}

	static bool RestoreComponentPropertiesFromSnapshot(
		UObject* ComponentTemplate,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		if (!ComponentTemplate)
		{
			OutError = TEXT("missing_component_template");
			return false;
		}

		const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
		if (!Snapshot->TryGetObjectField(TEXT("properties"), PropertiesObject) || !PropertiesObject || !PropertiesObject->IsValid())
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* PropertiesArray = nullptr;
		if (!(*PropertiesObject)->TryGetArrayField(TEXT("properties"), PropertiesArray) || !PropertiesArray)
		{
			return true;
		}

		for (const TSharedPtr<FJsonValue>& PropertyValue : *PropertiesArray)
		{
			const TSharedPtr<FJsonObject> PropertyJson = PropertyValue.IsValid() ? PropertyValue->AsObject() : nullptr;
			if (!PropertyJson.IsValid())
			{
				continue;
			}

			FString PropertyName;
			FString ValueText;
			if (!PropertyJson->TryGetStringField(TEXT("name"), PropertyName) ||
				!PropertyJson->TryGetStringField(TEXT("value"), ValueText) ||
				PropertyName.IsEmpty())
			{
				continue;
			}

			FProperty* Property = ComponentTemplate->GetClass()
				? ComponentTemplate->GetClass()->FindPropertyByName(FName(*PropertyName))
				: nullptr;
			if (!Property)
			{
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ComponentTemplate);
			if (!Property->ImportText_Direct(*ValueText, ValuePtr, ComponentTemplate, PPF_None))
			{
				OutError = FString::Printf(TEXT("component_property_restore_failed:%s"), *PropertyName);
				return false;
			}
		}

		return true;
	}

	static bool RestoreComponentFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString ComponentName = ExtractTargetName(Target);
		if (ComponentName.IsEmpty())
		{
			OutError = TEXT("missing_component_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		USCS_Node* Node = FindScsNodeByName(Blueprint, ComponentName);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Component")));
		Blueprint->Modify();
		Blueprint->SimpleConstructionScript->Modify();
		if (!bSnapshotExists)
		{
			if (Node)
			{
				Node->Modify();
				Blueprint->SimpleConstructionScript->RemoveNode(Node);
				MarkBlueprintReviewRestoreModified(Blueprint);
			}
			return true;
		}

		if (!Node || !Node->ComponentTemplate)
		{
			FString ComponentClassPath;
			if (!Snapshot->TryGetStringField(TEXT("component_class"), ComponentClassPath) || ComponentClassPath.IsEmpty())
			{
				OutError = FString::Printf(TEXT("snapshot_restore_component_missing_class:%s"), *ComponentName);
				return false;
			}

			UClass* ComponentClass = LoadObject<UClass>(nullptr, *ComponentClassPath);
			if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				OutError = FString::Printf(TEXT("snapshot_restore_component_invalid_class:%s"), *ComponentClassPath);
				return false;
			}

			Node = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, FName(*ComponentName));
			if (!Node || !Node->ComponentTemplate)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_component_recreate_failed:%s"), *ComponentName);
				return false;
			}

			FString ParentComponentName;
			Snapshot->TryGetStringField(TEXT("parent_component"), ParentComponentName);
			if (!ParentComponentName.IsEmpty())
			{
				if (USCS_Node* ParentNode = FindScsNodeByName(Blueprint, ParentComponentName))
				{
					ParentNode->Modify();
					ParentNode->AddChildNode(Node);
				}
				else
				{
					Blueprint->SimpleConstructionScript->AddNode(Node);
				}
			}
			else
			{
				Blueprint->SimpleConstructionScript->AddNode(Node);
			}
		}

		Node->Modify();
		Node->ComponentTemplate->Modify();
		if (!RestoreComponentPropertiesFromSnapshot(Node->ComponentTemplate, Snapshot, OutError))
		{
			return false;
		}
		MarkBlueprintReviewRestoreModified(Blueprint);
		return true;
	}

	static UObject* LoadReviewTargetAsset(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}

		if (UObject* Asset = FSoftObjectPath(AssetPath).TryLoad())
		{
			return Asset;
		}
		if (!AssetPath.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(AssetPath))
		{
			const FString ObjectPath = FString::Printf(
				TEXT("%s.%s"),
				*AssetPath,
				*FPackageName::GetShortName(AssetPath));
			return FSoftObjectPath(ObjectPath).TryLoad();
		}
		return nullptr;
	}

	static bool RestoreDataTableRowFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UDataTable* DataTable = Cast<UDataTable>(LoadReviewTargetAsset(Target.AssetPath));
		if (!DataTable || !DataTable->GetRowStruct())
		{
			OutError = FString::Printf(TEXT("datatable_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString RowName = ExtractTargetName(Target);
		if (RowName.IsEmpty())
		{
			OutError = TEXT("missing_datatable_row_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		const FName RowFName(*RowName);
		uint8* const* RowData = DataTable->GetRowMap().Find(RowFName);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore DataTable Row")));
		DataTable->Modify();
		if (!bSnapshotExists)
		{
			if (RowData)
			{
				DataTable->RemoveRow(RowFName);
				DataTable->MarkPackageDirty();
			}
			return true;
		}

		if (!RowData || !*RowData)
		{
			FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
			uint8* NewRowData = FDataTableEditorUtils::AddRow(DataTable, RowFName);
			FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
			if (!NewRowData)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_datatable_row_recreate_failed:%s"), *RowName);
				return false;
			}
			RowData = DataTable->GetRowMap().Find(RowFName);
			if (!RowData || !*RowData)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_datatable_row_recreate_missing:%s"), *RowName);
				return false;
			}
		}

		FString RowValue;
		if (Snapshot->TryGetStringField(TEXT("value"), RowValue))
		{
			const TCHAR* ImportResult = DataTable->GetRowStruct()->ImportText(
				*RowValue,
				*RowData,
				nullptr,
				PPF_None,
				nullptr,
				DataTable->GetRowStruct()->GetName());
			if (!ImportResult)
			{
				OutError = FString::Printf(TEXT("datatable_row_restore_failed:%s"), *RowName);
				return false;
			}
			DataTable->MarkPackageDirty();
		}
		return true;
	}

	static bool RestoreObjectPropertyFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UObject* Asset = LoadReviewTargetAsset(Target.AssetPath);
		if (!Asset || !Asset->GetClass())
		{
			OutError = FString::Printf(TEXT("asset_not_found:%s"), *Target.AssetPath);
			return false;
		}

		UBlueprint* Blueprint = Target.TargetKind == TEXT("class_default_property")
			? Cast<UBlueprint>(Asset)
			: nullptr;
		UObject* PropertyOwner = Asset;
		if (Blueprint)
		{
			UClass* DefaultClass = Blueprint->GeneratedClass
				? Blueprint->GeneratedClass
				: Blueprint->SkeletonGeneratedClass;
			PropertyOwner = DefaultClass ? DefaultClass->GetDefaultObject() : nullptr;
		}
		if (!PropertyOwner || !PropertyOwner->GetClass())
		{
			OutError = FString::Printf(TEXT("property_owner_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString PropertyPath = ExtractTargetName(Target);
		if (PropertyPath.IsEmpty())
		{
			OutError = TEXT("missing_property_path");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		if (!bSnapshotExists)
		{
			return true;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ExpectedType;
		FString ErrorCode;
		FString ErrorMessage;
		if (!FBlueprintHelperPropertyReflectionService::ResolvePropertyPath(
			PropertyOwner,
			PropertyPath,
			Property,
			ValuePtr,
			ExpectedType,
			ErrorCode,
			ErrorMessage) ||
			!Property ||
			!ValuePtr)
		{
			OutError = FString::Printf(
				TEXT("property_not_found:%s:%s:%s"),
				*PropertyPath,
				*ErrorCode,
				*ErrorMessage);
			return false;
		}

		FString ValueText;
		if (!Snapshot->TryGetStringField(TEXT("value"), ValueText))
		{
			return true;
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Object Property")));
		if (Blueprint)
		{
			Blueprint->Modify();
		}
		PropertyOwner->Modify();
		PropertyOwner->PreEditChange(Property);
		if (!Property->ImportText_Direct(*ValueText, ValuePtr, PropertyOwner, PPF_None))
		{
			OutError = FString::Printf(TEXT("object_property_restore_failed:%s"), *PropertyPath);
			return false;
		}
		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
		PropertyOwner->PostEditChangeProperty(PropertyChangedEvent);
		if (Blueprint)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
		else
		{
			Asset->MarkPackageDirty();
		}
		return true;
	}

	static bool RestoreWidgetFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(LoadReviewTargetAsset(Target.AssetPath));
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			OutError = FString::Printf(TEXT("widget_blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		FString WidgetName;
		FString PropertyName;
		SplitWidgetPropertyTarget(ExtractTargetName(Target), WidgetName, PropertyName);
		if (WidgetName.IsEmpty())
		{
			OutError = TEXT("missing_widget_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Widget")));
		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		if (!bSnapshotExists)
		{
			if (Widget)
			{
				Widget->Modify();
				WidgetBlueprint->WidgetTree->RemoveWidget(Widget);
				MarkBlueprintReviewRestoreModified(WidgetBlueprint);
			}
			return true;
		}

		if (!Widget)
		{
			FString WidgetClassPath;
			if (!Snapshot->TryGetStringField(TEXT("widget_class"), WidgetClassPath) || WidgetClassPath.IsEmpty())
			{
				OutError = FString::Printf(TEXT("snapshot_restore_widget_missing_class:%s"), *WidgetName);
				return false;
			}

			UClass* WidgetClass = LoadObject<UClass>(nullptr, *WidgetClassPath);
			if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
			{
				OutError = FString::Printf(TEXT("snapshot_restore_widget_invalid_class:%s"), *WidgetClassPath);
				return false;
			}

			Widget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
			if (!Widget)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_widget_recreate_failed:%s"), *WidgetName);
				return false;
			}

			FString ParentWidgetName;
			Snapshot->TryGetStringField(TEXT("parent_widget"), ParentWidgetName);
			if (!ParentWidgetName.IsEmpty())
			{
				UWidget* ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentWidgetName));
				UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
				if (!ParentPanel)
				{
					OutError = FString::Printf(TEXT("snapshot_restore_widget_parent_not_panel:%s"), *ParentWidgetName);
					return false;
				}

				int32 ChildIndex = INDEX_NONE;
				double ChildIndexNumber = INDEX_NONE;
				if (Snapshot->TryGetNumberField(TEXT("child_index"), ChildIndexNumber))
				{
					ChildIndex = FMath::RoundToInt(ChildIndexNumber);
				}
				UPanelSlot* NewSlot = ChildIndex >= 0 && ChildIndex <= ParentPanel->GetChildrenCount()
					? ParentPanel->InsertChildAt(ChildIndex, Widget)
					: ParentPanel->AddChild(Widget);
				if (!NewSlot)
				{
					OutError = FString::Printf(TEXT("snapshot_restore_widget_attach_failed:%s"), *WidgetName);
					return false;
				}
			}
			else if (!WidgetBlueprint->WidgetTree->RootWidget)
			{
				WidgetBlueprint->WidgetTree->RootWidget = Widget;
			}
		}

		if (Target.TargetKind == TEXT("umg_widget_property"))
		{
			if (PropertyName.IsEmpty())
			{
				Snapshot->TryGetStringField(TEXT("property_path"), PropertyName);
			}
			if (PropertyName.IsEmpty())
			{
				return true;
			}

			FProperty* Property = Widget->GetClass()
				? Widget->GetClass()->FindPropertyByName(FName(*PropertyName))
				: nullptr;
			if (!Property)
			{
				OutError = FString::Printf(TEXT("widget_property_not_found:%s"), *PropertyName);
				return false;
			}

			FString ValueText;
			if (Snapshot->TryGetStringField(TEXT("value"), ValueText))
			{
				Widget->Modify();
				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
				if (!Property->ImportText_Direct(*ValueText, ValuePtr, Widget, PPF_None))
				{
					OutError = FString::Printf(TEXT("widget_property_restore_failed:%s"), *PropertyName);
					return false;
				}
				MarkBlueprintReviewRestoreModified(WidgetBlueprint);
			}
		}
		else
		{
			Widget->Modify();
			if (!RestoreComponentPropertiesFromSnapshot(Widget, Snapshot, OutError))
			{
				return false;
			}
			MarkBlueprintReviewRestoreModified(WidgetBlueprint);
		}

		return true;
	}

	static bool ExecuteSnapshotRestore(
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Snapshot;
		if (!ParseReviewSnapshotJson(Target.BeforeSnapshotJson, Snapshot, OutError))
		{
			return false;
		}

		if (Target.TargetKind == TEXT("blueprint_variable"))
		{
			return RestoreBlueprintVariableFromSnapshot(Target, Snapshot, OutError);
		}
		if (Target.TargetKind == TEXT("component"))
		{
			return RestoreComponentFromSnapshot(Target, Snapshot, OutError);
		}
		if (Target.TargetKind == TEXT("datatable_row"))
		{
			return RestoreDataTableRowFromSnapshot(Target, Snapshot, OutError);
		}
		if (Target.TargetKind == TEXT("object_property") ||
			Target.TargetKind == TEXT("data_asset_property") ||
			Target.TargetKind == TEXT("class_default_property"))
		{
			return RestoreObjectPropertyFromSnapshot(Target, Snapshot, OutError);
		}
		if (Target.TargetKind == TEXT("umg_widget") || Target.TargetKind == TEXT("umg_widget_property"))
		{
			return RestoreWidgetFromSnapshot(Target, Snapshot, OutError);
		}

		OutError = FString::Printf(TEXT("snapshot_restore_unsupported_target_kind:%s"), *Target.TargetKind);
		return false;
	}

	static bool ShouldUseSnapshotRestore(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return !Target.BeforeSnapshotJson.IsEmpty()
			&& (Target.TargetKind == TEXT("blueprint_variable")
				|| Target.TargetKind == TEXT("component")
				|| Target.TargetKind == TEXT("datatable_row")
				|| Target.TargetKind == TEXT("object_property")
				|| Target.TargetKind == TEXT("data_asset_property")
				|| Target.TargetKind == TEXT("class_default_property")
				|| Target.TargetKind == TEXT("umg_widget")
				|| Target.TargetKind == TEXT("umg_widget_property"));
	}

	static FString MakeObjectPathFromAssetPath(FString AssetPath)
	{
		AssetPath.TrimStartAndEndInline();
		if (AssetPath.IsEmpty())
		{
			return FString();
		}
		if (AssetPath.Contains(TEXT(".")))
		{
			return AssetPath;
		}

		const FString PackageName = AssetPath;
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return AssetName.IsEmpty()
			? FString()
			: FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
	}

	static FBlueprintHelperReviewActionResult RejectAssetFactoryTargetWithDefaultDispatcher(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewAtomicTarget& Target)
	{
		const FString ObjectPath = MakeObjectPathFromAssetPath(Target.AssetPath.IsEmpty() ? Change.AssetPath : Target.AssetPath);
		if (ObjectPath.IsEmpty())
		{
			return MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::NeedsAction,
				TEXT("asset_factory_missing_asset_path"));
		}

		UObject* AssetObject = FindObject<UObject>(nullptr, *ObjectPath);
		if (!AssetObject)
		{
			AssetObject = LoadObject<UObject>(nullptr, *ObjectPath);
		}

		FBlueprintHelperReviewActionResult Result;
		Result.TargetTransactionId = Change.LatestTransactionId;
		Result.RollbackMode = TEXT("asset_lifecycle_delete");
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.bSupersededDataCompactionEligible = true;

		if (!AssetObject)
		{
			Result.bSucceeded = true;
			Result.Message = FString::Printf(TEXT("asset_already_missing:%s"), *ObjectPath);
			return Result;
		}

		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(AssetObject);
		const int32 DeletedCount = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
		if (DeletedCount != ObjectsToDelete.Num())
		{
			return MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::RejectFailed,
				FString::Printf(TEXT("asset_delete_failed:%s"), *ObjectPath));
		}

		Result.bSucceeded = true;
		Result.Message = FString::Printf(TEXT("asset_deleted:%s"), *ObjectPath);
		return Result;
	}

	static bool ExtractRollbackTransactionId(const FString& RollbackDataRef, FString& OutTransactionId)
	{
		const FString Prefix = TEXT("transaction://");
		const FString Suffix = TEXT("/rollback_data");
		if (!RollbackDataRef.StartsWith(Prefix) || !RollbackDataRef.EndsWith(Suffix))
		{
			return false;
		}

		OutTransactionId = RollbackDataRef.Mid(Prefix.Len());
		OutTransactionId.LeftChopInline(Suffix.Len());
		return !OutTransactionId.IsEmpty();
	}

	static bool LoadJournalRecordForReviewRollback(
		const FString& TransactionId,
		TSharedPtr<FJsonObject>& OutRecord,
		FString& OutError)
	{
		const FString ActivePath = FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Transactions")
			/ TEXT("Active")
			/ FString::Printf(TEXT("%s.json"), *TransactionId);
		const FString ReviewPath = FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Review")
			/ FString::Printf(TEXT("%s.json"), *TransactionId);

		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *ActivePath)
			&& !FFileHelper::LoadFileToString(Content, *ReviewPath))
		{
			OutError = FString::Printf(TEXT("rollback_ref_not_found:%s"), *TransactionId);
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, OutRecord) || !OutRecord.IsValid())
		{
			OutError = FString::Printf(TEXT("rollback_ref_parse_failed:%s"), *TransactionId);
			return false;
		}

		FString RollbackDataString;
		const TSharedPtr<FJsonObject>* RollbackDataObject = nullptr;
		const bool bHasRollbackData =
			(OutRecord->TryGetStringField(TEXT("rollback_data"), RollbackDataString) && !RollbackDataString.IsEmpty()) ||
			(OutRecord->TryGetObjectField(TEXT("rollback_data"), RollbackDataObject) && RollbackDataObject && RollbackDataObject->IsValid());
		if (!bHasRollbackData)
		{
			OutError = FString::Printf(TEXT("rollback_data_missing:%s"), *TransactionId);
			return false;
		}

		return true;
	}

	static FString ExtractReviewTargetTail(const FString& TargetKey, const FString& Marker)
	{
		const FString Token = Marker + TEXT(":");
		const int32 Pos = TargetKey.Find(Token, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Pos != INDEX_NONE)
		{
			return TargetKey.Mid(Pos + Token.Len());
		}

		int32 LastColon = INDEX_NONE;
		if (TargetKey.FindLastChar(TEXT(':'), LastColon))
		{
			return TargetKey.Mid(LastColon + 1);
		}
		return TargetKey;
	}

	static FString NormalizeReviewGuidCandidate(const FString& Candidate)
	{
		FString Trimmed = Candidate;
		Trimmed.TrimStartAndEndInline();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}

		FGuid ParsedGuid;
		if (FGuid::Parse(Trimmed, ParsedGuid))
		{
			return ParsedGuid.ToString(EGuidFormats::Digits);
		}

		FString HexDigits;
		HexDigits.Reserve(Trimmed.Len());
		for (const TCHAR Ch : Trimmed)
		{
			if (FChar::IsHexDigit(Ch))
			{
				HexDigits.AppendChar(Ch);
			}
		}
		return HexDigits.Len() == 32 ? HexDigits : Trimmed;
	}

	static bool DoesReviewNodeMatchStableId(const UEdGraphNode* Node, const FString& Candidate)
	{
		if (!Node || Candidate.IsEmpty())
		{
			return false;
		}

		if (Node->GetName().Equals(Candidate, ESearchCase::IgnoreCase))
		{
			return true;
		}

		const FString NodeGuidDigits = Node->NodeGuid.ToString(EGuidFormats::Digits);
		const FString CandidateGuidDigits = NormalizeReviewGuidCandidate(Candidate);
		if (!NodeGuidDigits.IsEmpty() && NodeGuidDigits.Equals(CandidateGuidDigits, ESearchCase::IgnoreCase))
		{
			return true;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			return MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")).Equals(Candidate, ESearchCase::IgnoreCase)
				|| MetaData.GetValue(Node, TEXT("BlueprintHelperTransactionId")).Equals(Candidate, ESearchCase::IgnoreCase)
				|| MetaData.GetValue(Node, TEXT("BlueprintHelperFeatureName")).Equals(Candidate, ESearchCase::IgnoreCase);
		}
		return false;
	}

	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static bool NodeMatchesEntryName(UEdGraphNode* Node, const FString& EntryName)
	{
		if (!Node)
		{
			return false;
		}
		if (EntryName.IsEmpty())
		{
			return true;
		}

		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
		{
			if (CustomEvent->CustomFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			if (EventNode->GetFunctionName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				EventNode->EventReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
		{
			if (FunctionEntry->FunctionReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				FunctionEntry->CustomGeneratedFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		const FString NodeName = Node->GetName();
		const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		return NodeName.Equals(EntryName, ESearchCase::IgnoreCase) ||
			NodeTitle.Equals(EntryName, ESearchCase::IgnoreCase);
	}

	static bool HasInboundExecLinkFromImportedNode(UEdGraphPin* ExecInputPin, const TSet<UEdGraphNode*>& ImportedNodes)
	{
		if (!ExecInputPin)
		{
			return false;
		}

		for (UEdGraphPin* LinkedPin : ExecInputPin->LinkedTo)
		{
			if (!LinkedPin ||
				LinkedPin->Direction != EGPD_Output ||
				LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}

			if (ImportedNodes.Contains(LinkedPin->GetOwningNode()))
			{
				return true;
			}
		}

		return false;
	}

	static UEdGraphNode* FindFirstExecutableBodyNode(const TSet<UEdGraphNode*>& ImportedNodes)
	{
		TArray<UEdGraphNode*> ImportedExecutableNodes;
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (Node && FindFirstExecPin(Node, EGPD_Input))
			{
				ImportedExecutableNodes.Add(Node);
			}
		}

		ImportedExecutableNodes.Sort(
			[](const UEdGraphNode& Left, const UEdGraphNode& Right)
			{
				return Left.NodePosX == Right.NodePosX
					? Left.NodePosY < Right.NodePosY
					: Left.NodePosX < Right.NodePosX;
			});

		for (UEdGraphNode* Node : ImportedExecutableNodes)
		{
			if (!HasInboundExecLinkFromImportedNode(FindFirstExecPin(Node, EGPD_Input), ImportedNodes))
			{
				return Node;
			}
		}

		return ImportedExecutableNodes.Num() > 0 ? ImportedExecutableNodes[0] : nullptr;
	}

	static bool PinsHaveSingleConnectionToEachOther(UEdGraphPin* FirstPin, UEdGraphPin* SecondPin)
	{
		return FirstPin &&
			SecondPin &&
			FirstPin->LinkedTo.Num() == 1 &&
			SecondPin->LinkedTo.Num() == 1 &&
			FirstPin->LinkedTo[0] == SecondPin &&
			SecondPin->LinkedTo[0] == FirstPin;
	}

	static void BreakAllPinLinksWithModify(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return;
		}

		Pin->Modify();
		Pin->BreakAllPinLinks(true);
	}

	static bool TryGetRollbackDataObject(
		const TSharedPtr<FJsonObject>& JournalRecord,
		TSharedPtr<FJsonObject>& OutRollbackData,
		FString& OutError)
	{
		OutRollbackData.Reset();
		if (!JournalRecord.IsValid())
		{
			OutError = TEXT("rollback_journal_missing");
			return false;
		}

		const TSharedPtr<FJsonObject>* RollbackDataObject = nullptr;
		if (JournalRecord->TryGetObjectField(TEXT("rollback_data"), RollbackDataObject) &&
			RollbackDataObject &&
			RollbackDataObject->IsValid())
		{
			OutRollbackData = *RollbackDataObject;
			return true;
		}

		FString RollbackDataString;
		if (JournalRecord->TryGetStringField(TEXT("rollback_data"), RollbackDataString) &&
			!RollbackDataString.IsEmpty())
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RollbackDataString);
			if (FJsonSerializer::Deserialize(Reader, OutRollbackData) && OutRollbackData.IsValid())
			{
				return true;
			}
			OutError = TEXT("rollback_data_parse_failed");
			return false;
		}

		OutError = TEXT("rollback_data_missing");
		return false;
	}

	static bool TryFindReviewAtomicTarget(
		const FBlueprintHelperReviewRecord& Record,
		const FString& TargetKey,
		FBlueprintHelperReviewAtomicTarget& OutTarget)
	{
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
			{
				if (Target.TargetKey == TargetKey)
				{
					OutTarget = Target;
					return true;
				}
			}
		}
		return false;
	}

	static FString ResolveConversionTransactionId(
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
		const FBlueprintHelperTransactionJournalService& JournalService)
	{
		return Request.ConversionTransactionId.IsEmpty()
			? JournalService.GenerateTransactionId()
			: Request.ConversionTransactionId;
	}

	static FString MakeConvertBlockFailureMessage(const FBlueprintHelperToolResultBase& ToolResult)
	{
		if (ToolResult.Error.IsSet())
		{
			if (!ToolResult.Error->Code.IsEmpty() && !ToolResult.Error->Message.IsEmpty())
			{
				return FString::Printf(TEXT("%s:%s"), *ToolResult.Error->Code, *ToolResult.Error->Message);
			}
			if (!ToolResult.Error->Code.IsEmpty())
			{
				return ToolResult.Error->Code;
			}
			if (!ToolResult.Error->Message.IsEmpty())
			{
				return ToolResult.Error->Message;
			}
		}
		return TEXT("convert_owner_block_failed");
	}

	static bool ExecuteBhToUserOwnerBlockConversion(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
		const FString& ConversionTransactionId,
		FString& OutError)
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperOwnershipService OwnershipService;
		FBlueprintHelperTransactionJournalService JournalService;
		FBlueprintHelperConvertBlockToUserOwnedService ConvertService(
			Resolver,
			OwnershipService,
			JournalService);

		const FString BlockRef = Request.DesiredBlockRef.IsEmpty()
			? ExtractReviewTargetTail(Target.TargetKey, TEXT("block"))
			: Request.DesiredBlockRef;
		const FString BlockId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(
			Target.GraphName,
			BlockRef);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), Target.AssetPath);
		Payload->SetStringField(TEXT("graph"), Target.GraphName);
		Payload->SetStringField(TEXT("block_ref"), BlockRef);
		Payload->SetStringField(TEXT("block_id"), BlockId);
		Payload->SetStringField(TEXT("ownership_scope"), TEXT("block"));
		Payload->SetStringField(TEXT("already_user_owned_policy"), TEXT("error"));
		Payload->SetStringField(TEXT("transaction_id"), ConversionTransactionId);
		Payload->SetBoolField(TEXT("dry_run"), false);

		const FBlueprintHelperToolResultBase ToolResult = ConvertService.Execute(Payload);
		if (!ToolResult.bOk)
		{
			OutError = MakeConvertBlockFailureMessage(ToolResult);
			return false;
		}
		return true;
	}

	static UEdGraphNode* FindReviewNodeByAnchor(UEdGraph* Graph, const FString& Anchor)
	{
		if (!Graph || Anchor.IsEmpty())
		{
			return nullptr;
		}

		FString NodeName = Anchor.Contains(TEXT(":entry:"))
			? ExtractReviewTargetTail(Anchor, TEXT("entry"))
			: ExtractReviewTargetTail(Anchor, TEXT("node"));
		if (NodeName.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (DoesReviewNodeMatchStableId(Node, NodeName))
			{
				return Node;
			}
		}
		return nullptr;
	}

	static bool ExecuteUserToBhOwnerBlockConversion(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
		const FString& ConversionTransactionId,
		FString& OutError)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		UEdGraph* Graph = FindReviewRollbackGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		TArray<UEdGraphNode*> Nodes;
		if (UEdGraphNode* EntryNode = FindReviewNodeByAnchor(Graph, Request.EntryAnchor))
		{
			Nodes.AddUnique(EntryNode);
		}
		for (const FString& NodeAnchor : Request.NodeAnchors)
		{
			if (UEdGraphNode* Node = FindReviewNodeByAnchor(Graph, NodeAnchor))
			{
				Nodes.AddUnique(Node);
			}
			else
			{
				OutError = FString::Printf(TEXT("node_anchor_not_found:%s"), *NodeAnchor);
				return false;
			}
		}

		if (Nodes.Num() == 0)
		{
			OutError = TEXT("missing_convert_owner_block_nodes");
			return false;
		}

		const FString BlockId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(
			Target.GraphName,
			Request.DesiredBlockRef);

		FBlueprintHelperScopedAssetMutation Mutation(
			FText::FromString(TEXT("BlueprintHelper Review Convert Owner Block")),
			Blueprint);
		Mutation.Modify(Graph);
		for (UEdGraphNode* Node : Nodes)
		{
			if (Node)
			{
				Mutation.Modify(Node);
			}
		}

		FBlueprintHelperOwnershipService OwnershipService;
		FString OwnershipError;
		if (!OwnershipService.WriteBlockOwnership(
			Blueprint,
			Nodes,
			BlockId,
			ConversionTransactionId,
			Request.DesiredBlockRef,
			OwnershipError))
		{
			Mutation.Rollback();
			OutError = OwnershipError;
			return false;
		}

		FBlueprintHelperAppendJournalRecord JournalRecord;
		JournalRecord.TransactionId = ConversionTransactionId;
		JournalRecord.Tool = TEXT("ConvertOwnerBlock");
		JournalRecord.Status = TEXT("applied");
		JournalRecord.TargetAssets.Add(Target.AssetPath);
		JournalRecord.GraphId = Target.GraphName;
		JournalRecord.GraphName = Target.GraphName;
		JournalRecord.BlockIds.Add(BlockId);
		JournalRecord.RollbackData = FString::Printf(
			TEXT("{\"direction\":\"user_to_bh\",\"block_id\":\"%s\"}"),
			*BlockId);

		FBlueprintHelperTransactionJournalService JournalService;
		FString JournalError;
		if (!JournalService.WriteAppendJournal(JournalRecord, JournalError))
		{
			Mutation.Rollback();
			OutError = JournalError;
			return false;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}
		Mutation.Commit();
		return true;
	}

	static UEdGraph* FindReviewRollbackGraph(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		auto FindIn = [&GraphName](const TArray<UEdGraph*>& Graphs) -> UEdGraph*
		{
			for (UEdGraph* Graph : Graphs)
			{
				if (Graph && (GraphName.IsEmpty() || Graph->GetName() == GraphName))
				{
					return Graph;
				}
			}
			return nullptr;
		};

		if (UEdGraph* Graph = FindIn(Blueprint->UbergraphPages))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindIn(Blueprint->FunctionGraphs))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindIn(Blueprint->MacroGraphs))
		{
			return Graph;
		}
		return nullptr;
	}

	static void CollectRollbackNodesForTarget(
		UEdGraph* Graph,
		const FBlueprintHelperReviewAtomicTarget& Target,
		TArray<UEdGraphNode*>& OutNodes)
	{
		if (!Graph)
		{
			return;
		}

		if (Target.TargetKind == TEXT("graph_node") || Target.TargetKey.Contains(TEXT(":node:")))
		{
			const FString NodeName = Target.NodeGuid.IsEmpty()
				? ExtractReviewTargetTail(Target.TargetKey, TEXT("node"))
				: Target.NodeGuid;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (DoesReviewNodeMatchStableId(Node, NodeName))
				{
					OutNodes.AddUnique(Node);
					return;
				}
			}
			return;
		}

		if (Target.TargetKind == TEXT("graph_block") || Target.TargetKey.Contains(TEXT(":block:")))
		{
			const FString BlockId = ExtractReviewTargetTail(Target.TargetKey, TEXT("block"));
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					continue;
				}
				UPackage* Package = Node->GetOutermost();
				if (!Package)
				{
					continue;
				}
				FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
				if (MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")) == BlockId)
				{
					OutNodes.AddUnique(Node);
				}
			}
		}
	}

	static bool ExecuteGraphAppendRollback(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& JournalRecord,
		FString& OutError)
	{
		FString Tool;
		if (!JournalRecord.IsValid() || !JournalRecord->TryGetStringField(TEXT("tool"), Tool))
		{
			OutError = TEXT("rollback_journal_tool_missing");
			return false;
		}
		const bool bAppendRollback = Tool == TEXT("AppendBlueprintGraph");
		const bool bReplaceRollback = Tool == TEXT("ReplaceBlueprintGraph");
		if (!bAppendRollback && !bReplaceRollback)
		{
			OutError = FString::Printf(TEXT("rollback_executor_unimplemented:%s"), *Tool);
			return false;
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		UEdGraph* Graph = FindReviewRollbackGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		TArray<UEdGraphNode*> NodesToRemove;
		CollectRollbackNodesForTarget(Graph, Target, NodesToRemove);
		if (!bReplaceRollback && NodesToRemove.Num() == 0)
		{
			OutError = FString::Printf(TEXT("anchor_not_found:%s"), *Target.TargetKey);
			return false;
		}

		TSharedPtr<FJsonObject> RollbackData;
		FString ExportedText;
		FString EntryIdentity;
		FString ReplaceScope;
		FString OwnerBlockId;
		bool bNeedsEntryReconnect = false;
		if (bReplaceRollback)
		{
			if (!TryGetRollbackDataObject(JournalRecord, RollbackData, OutError))
			{
				return false;
			}
			RollbackData->TryGetStringField(TEXT("exported_text"), ExportedText);
			RollbackData->TryGetStringField(TEXT("entry_identity"), EntryIdentity);
			RollbackData->TryGetStringField(TEXT("replace_scope"), ReplaceScope);
			RollbackData->TryGetStringField(TEXT("owner_block_id"), OwnerBlockId);
			bNeedsEntryReconnect =
				ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase) ||
				ReplaceScope.Equals(TEXT("event_body"), ESearchCase::IgnoreCase) ||
				ReplaceScope.Equals(TEXT("custom_event_body"), ESearchCase::IgnoreCase);
			if (ExportedText.IsEmpty())
			{
				OutError = TEXT("replace_rollback_exported_text_missing");
				return false;
			}
			if (!FEdGraphUtilities::CanImportNodesFromText(Graph, ExportedText))
			{
				OutError = TEXT("replace_rollback_exported_text_not_importable");
				return false;
			}
			if (bNeedsEntryReconnect)
			{
				NodesToRemove.RemoveAll(
					[](UEdGraphNode* Node)
					{
						return Node && (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_CustomEvent>() || Node->IsA<UK2Node_Event>());
					});
			}
			if (NodesToRemove.Num() == 0)
			{
				OutError = FString::Printf(TEXT("anchor_not_found:%s"), *Target.TargetKey);
				return false;
			}
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Reject")));
		Blueprint->Modify();
		Graph->Modify();
		for (UEdGraphNode* Node : NodesToRemove)
		{
			if (Node)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}
		if (bReplaceRollback)
		{
			TSet<UEdGraphNode*> ImportedNodes;
			FEdGraphUtilities::ImportNodesFromText(Graph, ExportedText, ImportedNodes);
			if (ImportedNodes.Num() == 0)
			{
				OutError = TEXT("replace_rollback_imported_no_nodes");
				return false;
			}

			if (!OwnerBlockId.IsEmpty())
			{
				TArray<UEdGraphNode*> ImportedNodeArray;
				for (UEdGraphNode* Node : ImportedNodes)
				{
					if (Node)
					{
						ImportedNodeArray.Add(Node);
					}
				}

				FBlueprintHelperOwnershipService OwnershipService;
				FString OwnershipError;
				const FString RollbackTransactionId = Target.LatestTransactionId.IsEmpty()
					? TEXT("review_reject")
					: FString::Printf(TEXT("%s_reject"), *Target.LatestTransactionId);
				if (!OwnershipService.WriteBlockOwnership(
					Blueprint,
					ImportedNodeArray,
					OwnerBlockId,
					RollbackTransactionId,
					TEXT("ReplaceRollback"),
					OwnershipError))
				{
					OutError = OwnershipError.IsEmpty() ? TEXT("replace_rollback_ownership_write_failed") : OwnershipError;
					return false;
				}
			}

			if (bNeedsEntryReconnect)
			{
				UEdGraphNode* EntryNode = nullptr;
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (!Node || ImportedNodes.Contains(Node))
					{
						continue;
					}

					const bool bMatchesScope =
						(ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase) && Node->IsA<UK2Node_FunctionEntry>()) ||
						(!ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase) && FindFirstExecPin(Node, EGPD_Output) != nullptr);
					if (bMatchesScope && NodeMatchesEntryName(Node, EntryIdentity))
					{
						EntryNode = Node;
						break;
					}
				}

				if (!EntryNode)
				{
					OutError = EntryIdentity.IsEmpty()
						? TEXT("replace_rollback_entry_not_found")
						: FString::Printf(TEXT("replace_rollback_entry_not_found:%s"), *EntryIdentity);
					return false;
				}

				UEdGraphPin* EntryExecOut = FindFirstExecPin(EntryNode, EGPD_Output);
				if (!EntryExecOut)
				{
					OutError = TEXT("replace_rollback_entry_exec_missing");
					return false;
				}

				if (UEdGraphNode* FirstBodyNode = FindFirstExecutableBodyNode(ImportedNodes))
				{
					UEdGraphPin* BodyExecIn = FindFirstExecPin(FirstBodyNode, EGPD_Input);
					if (!BodyExecIn)
					{
						OutError = TEXT("replace_rollback_body_exec_missing");
						return false;
					}

					if (!PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
					{
						const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
						if (!Schema)
						{
							OutError = TEXT("replace_rollback_k2_schema_missing");
							return false;
						}
						BreakAllPinLinksWithModify(EntryExecOut);
						BreakAllPinLinksWithModify(BodyExecIn);
						if (!Schema->TryCreateConnection(EntryExecOut, BodyExecIn) ||
							!PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
						{
							OutError = TEXT("replace_rollback_entry_reconnect_failed");
							return false;
						}
					}
				}
				else
				{
					BreakAllPinLinksWithModify(EntryExecOut);
				}
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}
		Graph->NotifyGraphChanged();
		return true;
	}

	static FBlueprintHelperReviewActionResult RejectVisibleChangeWithDefaultDispatcher(
		const FBlueprintHelperReviewVisibleChange& Change)
	{
		if (Change.AtomicTargets.Num() == 0)
		{
			return MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::NeedsAction,
				TEXT("missing_atomic_targets"));
		}

		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (IsAssetFactoryTarget(Target))
			{
				const FBlueprintHelperReviewActionResult AssetFactoryResult =
					RejectAssetFactoryTargetWithDefaultDispatcher(Change, Target);
				if (!AssetFactoryResult.bSucceeded)
				{
					return AssetFactoryResult;
				}
				continue;
			}

			if (Target.TargetKey.IsEmpty())
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_anchor"));
			}
			if (ShouldUseSnapshotRestore(Target))
			{
				FString SnapshotRestoreError;
				if (!ExecuteSnapshotRestore(Target, SnapshotRestoreError))
				{
					return MakeRejectFailureResult(
						Change,
						SnapshotRestoreError.Contains(TEXT("_recreate_required"))
							? EBlueprintHelperReviewChangeStatus::NeedsAction
							: EBlueprintHelperReviewChangeStatus::RejectFailed,
						SnapshotRestoreError);
				}
				continue;
			}
			if (Target.RollbackDataRef.IsEmpty())
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_rollback_data_ref"));
			}
			if (Target.RecordedAfterHash.IsEmpty())
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_recorded_after_hash"));
			}

			FString CurrentHash;
			FString HashError;
			if (!FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(Target, CurrentHash, HashError))
			{
				if (HashError.Contains(TEXT("graph_not_found")) || HashError.Contains(TEXT("anchor_not_found")))
				{
					FBlueprintHelperReviewActionResult Result;
					Result.bSucceeded = true;
					Result.TargetTransactionId = Change.LatestTransactionId;
					Result.RollbackMode = TEXT("archive_baseline");
					Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
					Result.Message = FString::Printf(TEXT("target_already_missing:%s"), *HashError);
					Result.bSupersededDataCompactionEligible = true;
					return Result;
				}
				return MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("current_hash_unavailable:%s"), *HashError));
			}
			FString RollbackTransactionId;
			if (!ExtractRollbackTransactionId(Target.RollbackDataRef, RollbackTransactionId))
			{
				return MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("rollback_ref_unresolved:%s"), *Target.RollbackDataRef));
			}

			TSharedPtr<FJsonObject> JournalRecord;
			FString JournalError;
			if (!LoadJournalRecordForReviewRollback(RollbackTransactionId, JournalRecord, JournalError))
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, JournalError);
			}

			FString RollbackError;
			if (!ExecuteGraphAppendRollback(Target, JournalRecord, RollbackError))
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::RejectFailed, RollbackError);
			}
		}

		FBlueprintHelperReviewActionResult Result;
		Result.bSucceeded = true;
		Result.TargetTransactionId = Change.LatestTransactionId;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("rejected");
		Result.bSupersededDataCompactionEligible = true;
		return Result;
	}

	static FBlueprintHelperReviewCascadeActionResult CascadeRejectLifecycleChildrenAfterRootResult(
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
		for (const FBlueprintHelperReviewVisibleChange& PendingChange : PendingChanges)
		{
			if (PendingChange.AssetPath == Root.AssetPath
				&& !PendingChange.bIsAssetLifecycleRoot
				&& PendingChange.Status == EBlueprintHelperReviewChangeStatus::Pending
				&& !PendingChange.ChangeId.IsEmpty())
			{
				ChildChangeIds.Add(PendingChange.ChangeId);
			}
		}

		FString ReviewRecordId = ResolvedReviewRecordId;
		TArray<FString> RootTargetKeys;
		if (ReviewRecordId.IsEmpty()
			&& !TryResolvePersistedReviewChange(Root, ReviewRecordId, RootTargetKeys))
		{
			return CascadeResult;
		}

		FBlueprintHelperReviewStoreService Store;
		FString Error;
		if (!DeleteReviewRecordAndLinkedDebugCases(Store, ReviewRecordId, Error))
		{
			CascadeResult.RootResult.bSucceeded = false;
			CascadeResult.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			CascadeResult.RootResult.Message = Error;
			return CascadeResult;
		}

		for (const FString& ChildChangeId : ChildChangeIds)
		{
			CascadeResult.RemovedChildChangeIds.Add(ChildChangeId);
		}
		CascadeResult.bChildrenRemoved = CascadeResult.RemovedChildChangeIds.Num() > 0;
		return CascadeResult;
	}

};

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService() = default;

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService(
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: DebugEntryService(InDebugEntryService)
{
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::AcceptVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	const TArray<FBlueprintHelperReviewActionServiceLocalUtils::FPersistedReviewTargetMatch> Matches =
		FBlueprintHelperReviewActionServiceLocalUtils::ResolvePersistedReviewTargetMatches(Change);
	if (Matches.Num() > 0)
	{
		FBlueprintHelperReviewActionResult LastResult;
		for (const FBlueprintHelperReviewActionServiceLocalUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			LastResult = AcceptReviewTargets(Match.ReviewRecordId, Match.TargetKeys);
			if (!LastResult.bSucceeded)
			{
				return LastResult;
			}
		}

		LastResult.bSucceeded = true;
		LastResult.TargetTransactionId = Change.LatestTransactionId;
		LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
		LastResult.Message = TEXT("accepted");
		LastResult.bSupersededDataCompactionEligible =
			Change.SourceTransactionIds.Num() > FMath::Max(1, Change.LatestTransactionIds.Num());
		return LastResult;
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = true;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible =
		Change.SourceTransactionIds.Num() > FMath::Max(1, Change.LatestTransactionIds.Num());
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	const TArray<FBlueprintHelperReviewActionServiceLocalUtils::FPersistedReviewTargetMatch> Matches =
		FBlueprintHelperReviewActionServiceLocalUtils::ResolvePersistedReviewTargetMatches(Change);
	if (Matches.Num() > 0)
	{
		FBlueprintHelperReviewRejectOptions Options;
		FBlueprintHelperReviewActionResult LastResult;
		for (const FBlueprintHelperReviewActionServiceLocalUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			LastResult = RejectReviewTargets(Match.ReviewRecordId, Match.TargetKeys, Options);
			if (!LastResult.bSucceeded)
			{
				return LastResult;
			}
		}

		LastResult.bSucceeded = true;
		LastResult.TargetTransactionId = Change.LatestTransactionId;
		LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		LastResult.Message = TEXT("rejected");
		LastResult.bSupersededDataCompactionEligible = true;
		return LastResult;
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = false;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = TEXT("Archive-baseline rollback backend is not wired in the first Review UI slice.");
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.RollbackMode = TEXT("archive_baseline");

	if (Change.AtomicTargets.Num() == 0)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = TEXT("missing_atomic_targets");
		return Result;
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.TargetKey.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_anchor");
			return Result;
		}
		if (FBlueprintHelperReviewActionServiceLocalUtils::ShouldUseSnapshotRestore(Target))
		{
			FString SnapshotRestoreError;
			if (!FBlueprintHelperReviewActionServiceLocalUtils::ExecuteSnapshotRestore(Target, SnapshotRestoreError))
			{
				Result.NewStatus = SnapshotRestoreError.Contains(TEXT("_recreate_required"))
					? EBlueprintHelperReviewChangeStatus::NeedsAction
					: EBlueprintHelperReviewChangeStatus::RejectFailed;
				Result.Message = SnapshotRestoreError;
				return Result;
			}
			continue;
		}
		if (Target.RollbackDataRef.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_rollback_data_ref");
			return Result;
		}
		if (Target.RecordedAfterHash.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_recorded_after_hash");
			return Result;
		}

		const FString* CurrentHash = Options.CurrentHashesByTargetKey.Find(Target.TargetKey);
		if (!CurrentHash)
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = FString::Printf(TEXT("missing_current_hash:%s"), *Target.TargetKey);
			return Result;
		}
	}

	if (!Options.bRollbackExecutorAvailable)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = TEXT("rollback_executor_unavailable");
		return Result;
	}

	if (!Options.bRollbackSucceeded)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = Options.RollbackFailureMessage.IsEmpty()
			? TEXT("rollback_failed")
			: Options.RollbackFailureMessage;
		return Result;
	}

	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	Result.Message = TEXT("rejected");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewActionService::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges) const
{
	FBlueprintHelperReviewActionResult RootResult;
	FString ReviewRecordId;
	TArray<FString> TargetKeys;
	if (FBlueprintHelperReviewActionServiceLocalUtils::TryResolvePersistedReviewChange(Root, ReviewRecordId, TargetKeys))
	{
		FBlueprintHelperReviewRejectOptions Options;
		RootResult = RejectReviewTargets(ReviewRecordId, TargetKeys, Options);
	}
	else
	{
		RootResult = RejectVisibleChange(Root);
	}

	return FBlueprintHelperReviewActionServiceLocalUtils::CascadeRejectLifecycleChildrenAfterRootResult(
		Root,
		PendingChanges,
		RootResult,
		ReviewRecordId);
}

FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewActionService::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult RootResult;
	FString ReviewRecordId;
	TArray<FString> TargetKeys;
	if (FBlueprintHelperReviewActionServiceLocalUtils::TryResolvePersistedReviewChange(Root, ReviewRecordId, TargetKeys))
	{
		RootResult = RejectReviewTargets(ReviewRecordId, TargetKeys, Options);
	}
	else
	{
		RootResult = RejectVisibleChange(Root, Options);
	}

	return FBlueprintHelperReviewActionServiceLocalUtils::CascadeRejectLifecycleChildrenAfterRootResult(
		Root,
		PendingChanges,
		RootResult,
		ReviewRecordId);
}

void FBlueprintHelperReviewActionService::RecordRejectDebugCaseBestEffort(
	FBlueprintHelperReviewRecord& Record,
	const TArray<FString>& TargetKeys,
	const FString& SourceTransactionId,
	EBlueprintHelperReviewChangeStatus RejectStatus,
	const FString& RejectMessage) const
{
	if (!DebugEntryService ||
		(RejectStatus != EBlueprintHelperReviewChangeStatus::NeedsAction &&
		 RejectStatus != EBlueprintHelperReviewChangeStatus::RejectFailed))
	{
		return;
	}

	TSharedRef<FJsonObject> ToolSummary = MakeShared<FJsonObject>();
	ToolSummary->SetStringField(TEXT("review_record_id"), Record.ReviewRecordId);
	ToolSummary->SetStringField(TEXT("archive_session_id"), Record.ArchiveSessionId);
	ToolSummary->SetStringField(TEXT("asset_path"), Record.AssetPath);
	ToolSummary->SetStringField(TEXT("review_status"), BlueprintHelperReviewChangeStatusToString(RejectStatus));
	if (!SourceTransactionId.IsEmpty())
	{
		ToolSummary->SetStringField(TEXT("source_transaction_id"), SourceTransactionId);
	}
	if (!RejectMessage.IsEmpty())
	{
		ToolSummary->SetStringField(TEXT("message"), RejectMessage);
	}
	TArray<TSharedPtr<FJsonValue>> TargetKeyValues;
	for (const FString& TargetKey : TargetKeys)
	{
		if (!TargetKey.IsEmpty())
		{
			TargetKeyValues.Add(MakeShared<FJsonValueString>(TargetKey));
		}
	}
	if (TargetKeyValues.Num() > 0)
	{
		ToolSummary->SetArrayField(TEXT("target_keys"), TargetKeyValues);
	}

	FBlueprintHelperDebugEntryEventInput DebugInput;
	DebugInput.SourceLayer = TEXT("review");
	DebugInput.Source = RejectStatus == EBlueprintHelperReviewChangeStatus::RejectFailed
		? TEXT("review_reject_failed")
		: TEXT("review_reject_needs_action");
	DebugInput.Operation = TEXT("reject_review_targets");
	DebugInput.Stage = TEXT("reject");
	DebugInput.Severity = EBlueprintHelperDebugSeverity::Error;
	if (Record.SourceTaskRunIds.Num() > 0)
	{
		DebugInput.TaskRunId = Record.SourceTaskRunIds[0];
	}
	if (!Record.AssetPath.IsEmpty())
	{
		DebugInput.AssetPaths.Add(Record.AssetPath);
	}
	if (!Record.ReviewRecordId.IsEmpty())
	{
		DebugInput.ReviewRecordIds.Add(Record.ReviewRecordId);
	}
	if (!SourceTransactionId.IsEmpty())
	{
		FBlueprintHelperDebugTransactionLink TransactionLink;
		TransactionLink.TransactionId = SourceTransactionId;
		TransactionLink.Role = TEXT("review_reject_failed");
		TransactionLink.Source = TEXT("review");
		TransactionLink.Summary = TEXT("source transaction for review reject action");
		DebugInput.TransactionLinks.Add(TransactionLink);
	}
	DebugInput.Error.Code = DebugInput.Source;
	DebugInput.Error.Message = RejectMessage;
	DebugInput.RecommendedNext = TEXT("get_debug_case");
	DebugInput.ToolResultSummary = ToolSummary;

	const FBlueprintHelperDebugEntryRecordResult DebugResult =
		DebugEntryService->RecordEventBestEffort(DebugInput);
	if (DebugResult.bRecorded && !DebugResult.DebugCaseId.IsEmpty())
	{
		Record.DebugCaseIds.AddUnique(DebugResult.DebugCaseId);
	}
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::AcceptReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	bool bMatchedAny = false;
	FString SourceTransactionId;
	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!FBlueprintHelperReviewActionServiceLocalUtils::ReviewTargetMatches(Target, TargetKeys))
			{
				continue;
			}
			bMatchedAny = true;
			Target.Status = EBlueprintHelperReviewChangeStatus::Accepted;
			SourceTransactionId = Target.LatestTransactionId;
		}
	}

	if (!bMatchedAny)
	{
		Result.Message = TEXT("target_keys_not_found");
		return Result;
	}

	FBlueprintHelperReviewActionServiceLocalUtils::RefreshReviewRecordStatus(Record);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
		TEXT("accept"),
		TargetKeys,
		TEXT("keep_managed"),
		SourceTransactionId,
		TEXT("accepted")));

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.TargetTransactionId = SourceTransactionId;
	Result.NewStatus = Record.Status;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	bool bMatchedAny = false;
	bool bAllRejected = true;
	bool bAllTargetStatusesRejected = true;
	const bool bUseInjectedOptions = FBlueprintHelperReviewActionServiceLocalUtils::HasInjectedRejectOptions(Options);
	FString SourceTransactionId;
	FString LastMessage;
	EBlueprintHelperReviewChangeStatus LastStatus = EBlueprintHelperReviewChangeStatus::Rejected;

	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!FBlueprintHelperReviewActionServiceLocalUtils::ReviewTargetMatches(Target, TargetKeys))
			{
				continue;
			}

			bMatchedAny = true;
			FBlueprintHelperReviewVisibleChange TargetChange = Change;
			TargetChange.AtomicTargets.Reset();
			FBlueprintHelperReviewAtomicTarget TargetForReject = Target;
			if (TargetForReject.BeforeSnapshotJson.IsEmpty())
			{
				TargetForReject.BeforeSnapshotJson = Change.BeforeSnapshotJson;
			}
			if (TargetForReject.AfterSnapshotJson.IsEmpty())
			{
				TargetForReject.AfterSnapshotJson = Change.AfterSnapshotJson;
			}
			if (TargetForReject.BaselineHash.IsEmpty())
			{
				TargetForReject.BaselineHash = Change.BeforeHash;
			}
			if (TargetForReject.RecordedAfterHash.IsEmpty())
			{
				TargetForReject.RecordedAfterHash = Change.AfterHash;
			}
			TargetChange.AtomicTargets.Add(TargetForReject);
			const FBlueprintHelperReviewActionResult TargetResult = bUseInjectedOptions
				? RejectVisibleChange(TargetChange, Options)
				: FBlueprintHelperReviewActionServiceLocalUtils::RejectVisibleChangeWithDefaultDispatcher(TargetChange);
			const bool bTargetStatusRejected = TargetResult.NewStatus == EBlueprintHelperReviewChangeStatus::Rejected;
			Target.Status = TargetResult.NewStatus;
			Change.NeedsActionReason = bTargetStatusRejected ? FString() : TargetResult.Message;
			SourceTransactionId = Target.LatestTransactionId;
			LastMessage = TargetResult.Message;
			LastStatus = TargetResult.NewStatus;
			bAllRejected &= TargetResult.bSucceeded;
			bAllTargetStatusesRejected &= bTargetStatusRejected;
		}
	}

	if (!bMatchedAny)
	{
		Result.Message = TEXT("target_keys_not_found");
		return Result;
	}

	if (bAllTargetStatusesRejected)
	{
		TArray<FString> DebugCaseIdsToDelete;
		bool bRecordDeleted = false;
		if (!Store.PurgeReviewTargets(ReviewRecordId, TargetKeys, DebugCaseIdsToDelete, bRecordDeleted, Error))
		{
			Result.Message = Error;
			return Result;
		}
		if (bRecordDeleted
			&& !FBlueprintHelperReviewActionServiceLocalUtils::DeleteDebugCasesForReviewRecord(
				ReviewRecordId,
				DebugCaseIdsToDelete,
				Error))
		{
			Result.Message = Error;
			return Result;
		}

		Result.bSucceeded = true;
		Result.TargetTransactionId = SourceTransactionId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.Message = LastMessage.IsEmpty() ? TEXT("rejected_purged") : LastMessage;
		Result.bSupersededDataCompactionEligible = true;
		return Result;
	}

	FBlueprintHelperReviewActionServiceLocalUtils::RefreshReviewRecordStatus(Record);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
		TEXT("reject"),
		TargetKeys,
		TEXT("archive_baseline"),
		SourceTransactionId,
		LastMessage));
	RecordRejectDebugCaseBestEffort(
		Record,
		TargetKeys,
		SourceTransactionId,
		bAllTargetStatusesRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus,
		LastMessage);

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = bAllTargetStatusesRejected;
	Result.TargetTransactionId = SourceTransactionId;
	Result.NewStatus = bAllTargetStatusesRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = LastMessage;
	Result.bSupersededDataCompactionEligible = bAllTargetStatusesRejected;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectAll(
	const FBlueprintHelperReviewRecordQuery& Query,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
	if (Records.Num() == 0)
	{
		Result.bSucceeded = true;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("no_pending_review_targets");
		return Result;
	}

	bool bAllRejected = true;
	EBlueprintHelperReviewChangeStatus LastStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	FString LastMessage;
	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		const TArray<FString> TargetKeys = FBlueprintHelperReviewActionServiceLocalUtils::CollectPendingTargetKeys(Record);
		if (TargetKeys.Num() == 0)
		{
			continue;
		}

		const FBlueprintHelperReviewActionResult RecordResult = RejectReviewTargets(
			Record.ReviewRecordId,
			TargetKeys,
			Options);
		bAllRejected &= RecordResult.bSucceeded;
		LastStatus = RecordResult.NewStatus;
		LastMessage = RecordResult.Message;
	}

	Result.bSucceeded = bAllRejected;
	Result.NewStatus = bAllRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = LastMessage.IsEmpty() ? TEXT("reject_all") : LastMessage;
	Result.bSupersededDataCompactionEligible = bAllRejected;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::ConvertOwnerBlock(
	const FBlueprintHelperReviewConvertOwnerBlockRequest& Request) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(Request.ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	auto PersistFailure = [&Store, &Record, &Request, &Result](
		const FString& Message,
		const FString& SourceTransactionId) -> FBlueprintHelperReviewActionResult
	{
		TArray<FString> TargetKeys;
		if (!Request.BlockTargetKey.IsEmpty())
		{
			TargetKeys.Add(Request.BlockTargetKey);
		}
		Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
			TEXT("convert_owner_block"),
			TargetKeys,
			Request.Direction,
			SourceTransactionId,
			Message));

		FString SaveError;
		if (!Store.SaveReviewRecord(Record, SaveError))
		{
			Result.Message = SaveError;
			return Result;
		}

		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = Message;
		return Result;
	};

	if (!Request.bSettingProfileAllowsConversion)
	{
		return PersistFailure(TEXT("convert_owner_block_not_allowed_by_setting_profile"), FString());
	}
	if (Request.Direction != TEXT("bh_to_user") && Request.Direction != TEXT("user_to_bh"))
	{
		return PersistFailure(TEXT("invalid_convert_owner_block_direction"), FString());
	}
	if (Request.BlockTargetKey.IsEmpty() || Request.EntryAnchor.IsEmpty() || Request.DesiredBlockRef.IsEmpty())
	{
		return PersistFailure(TEXT("missing_convert_owner_block_anchor"), FString());
	}
	if (Request.NodeAnchors.Num() == 0)
	{
		return PersistFailure(TEXT("missing_convert_owner_block_node_anchors"), FString());
	}

	FBlueprintHelperReviewAtomicTarget MatchedTarget;
	if (!FBlueprintHelperReviewActionServiceLocalUtils::TryFindReviewAtomicTarget(Record, Request.BlockTargetKey, MatchedTarget))
	{
		return PersistFailure(TEXT("convert_owner_block_target_not_found"), FString());
	}
	if (MatchedTarget.TargetKind != TEXT("graph_block") && !MatchedTarget.TargetKey.Contains(TEXT(":block:")))
	{
		return PersistFailure(TEXT("convert_owner_block_requires_graph_block_target"), FString());
	}
	if (MatchedTarget.AssetPath.IsEmpty())
	{
		MatchedTarget.AssetPath = Record.AssetPath;
	}

	FBlueprintHelperTransactionJournalService JournalService;
	const FString ConversionTransactionId = FBlueprintHelperReviewActionServiceLocalUtils::ResolveConversionTransactionId(Request, JournalService);
	FString ConversionError;
	const bool bConverted = Request.Direction == TEXT("bh_to_user")
		? FBlueprintHelperReviewActionServiceLocalUtils::ExecuteBhToUserOwnerBlockConversion(
			MatchedTarget,
			Request,
			ConversionTransactionId,
			ConversionError)
		: FBlueprintHelperReviewActionServiceLocalUtils::ExecuteUserToBhOwnerBlockConversion(
			MatchedTarget,
			Request,
			ConversionTransactionId,
			ConversionError);
	if (!bConverted)
	{
		const FString FailureMessage = ConversionError.IsEmpty()
			? TEXT("convert_owner_block_failed")
			: ConversionError;
		return PersistFailure(FailureMessage, ConversionTransactionId);
	}

	bool bMatchedAny = false;
	const FString NewOwnership = Request.Direction == TEXT("bh_to_user")
		? TEXT("user_owned")
		: TEXT("blueprinthelper_owned");
	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (Target.TargetKey == Request.BlockTargetKey)
			{
				Target.Ownership = NewOwnership;
				bMatchedAny = true;
			}
		}
	}

	TArray<FString> ConvertedTargetKeys;
	ConvertedTargetKeys.Add(Request.BlockTargetKey);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
		TEXT("convert_owner_block"),
		ConvertedTargetKeys,
		Request.Direction,
		ConversionTransactionId,
		TEXT("converted_owner_block")));
	Record.SourceTransactionSummary.TransactionIds.AddUnique(ConversionTransactionId);
	Record.SourceTransactionSummary.OperationKinds.AddUnique(TEXT("convert_owner_block"));
	if (!Record.AssetPath.IsEmpty())
	{
		Record.SourceTransactionSummary.AssetPaths.AddUnique(Record.AssetPath);
	}
	Record.SourceTransactionSummary.TransactionCount = Record.SourceTransactionSummary.TransactionIds.Num();

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.TargetTransactionId = ConversionTransactionId;
	Result.NewStatus = Record.Status;
	Result.Message = TEXT("converted_owner_block");
	return Result;
}
