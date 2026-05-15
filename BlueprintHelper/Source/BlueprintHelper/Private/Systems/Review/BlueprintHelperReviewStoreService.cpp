// BlueprintHelper Review Store service implementation.

#include "Systems/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

class FBlueprintHelperReviewStoreServiceLocalUtils
{
public:
	static FString ExtractReviewNodeIdentifier(const FString& RawNodePath)
	{
		FString Identifier = RawNodePath;

		int32 DotIndex = INDEX_NONE;
		if (Identifier.FindLastChar(TEXT('.'), DotIndex))
		{
			Identifier = Identifier.Mid(DotIndex + 1);
		}

		return Identifier;
	}

	static FBlueprintHelperReviewAtomicTarget MakeGraphRecordTarget(
		const FBlueprintHelperReviewTransactionInput& Input,
		const FString& TargetId,
		const FString& TargetPrefix)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = Input.AssetPath;
		Target.GraphName = Input.GraphName;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.TargetKey = FString::Printf(TEXT("%s:%s:%s"), *Input.LocationKey, *TargetPrefix, *TargetId);
		Target.VisualGroupKey = Input.LocationKey;
		Target.DisplayLabel = Input.DisplayLabel;
		Target.NodeGuid = TargetId;
		Target.SourceTransactionIds.Add(Input.TransactionId);
		return Target;
	}

	static void AddGraphTargetsFromStringArrayField(
		const TSharedPtr<FJsonObject>& Record,
		const TCHAR* FieldName,
		const FString& TargetPrefix,
		bool bExtractNodeName,
		FBlueprintHelperReviewTransactionInput& Input)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Record.IsValid() || !Record->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (!Value.IsValid())
			{
				continue;
			}

			const FString RawValue = Value->AsString();
			FString TargetId = bExtractNodeName ? ExtractReviewNodeIdentifier(RawValue) : RawValue;
			if (FCString::Stricmp(*TargetPrefix, TEXT("block")) == 0)
			{
				TargetId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(
					Input.GraphName,
					TargetId);
			}
			if (TargetId.IsEmpty())
			{
				continue;
			}

			Input.AtomicTargets.Add(MakeGraphRecordTarget(Input, TargetId, TargetPrefix));
		}
	}

	static void AddGraphTargetsFromRollbackData(FBlueprintHelperReviewTransactionInput& Input, const TSharedPtr<FJsonObject>& Record)
	{
		TSharedPtr<FJsonObject> RollbackObject;
		if (!Record.IsValid())
		{
			return;
		}

		FString RollbackDataString;
		if (Record->TryGetStringField(TEXT("rollback_data"), RollbackDataString) && !RollbackDataString.IsEmpty())
		{
			const TSharedRef<TJsonReader<>> RollbackReader = TJsonReaderFactory<>::Create(RollbackDataString);
			FJsonSerializer::Deserialize(RollbackReader, RollbackObject);
		}
		else
		{
			const TSharedPtr<FJsonObject>* RollbackObjectPtr = nullptr;
			if (Record->TryGetObjectField(TEXT("rollback_data"), RollbackObjectPtr) && RollbackObjectPtr)
			{
				RollbackObject = *RollbackObjectPtr;
			}
		}

		if (!RollbackObject.IsValid())
		{
			return;
		}

		AddGraphTargetsFromStringArrayField(
			RollbackObject,
			TEXT("node_guids"),
			TEXT("rollback_node"),
			false,
			Input);
	}

	static FString MakeReviewInternalMissingAnchorKey(const FString& TransactionId, int32 Index)
	{
		return FString::Printf(TEXT("__missing_anchor|%s|%d"), *TransactionId, Index);
	}

	static FString MakeReviewInternalMissingGroupKey(const FString& TransactionId, int32 Index)
	{
		return FString::Printf(TEXT("__missing_visual_group|%s|%d"), *TransactionId, Index);
	}

	static FString MakeReviewAtomicLookupKey(const FBlueprintHelperReviewAtomicTarget& Target, const FString& FallbackKey)
	{
		return FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*Target.AssetPath,
			BlueprintHelperReviewSurfaceToString(Target.Surface),
			*Target.GraphName,
			Target.TargetKey.IsEmpty() ? *FallbackKey : *Target.TargetKey);
	}

	static FString SanitizeReviewIdSegment(const FString& Value)
	{
		FString Result;
		Result.Reserve(FMath::Min(Value.Len(), 64));
		for (TCHAR Ch : Value)
		{
			const bool bIsAlphaNumeric =
				(Ch >= TEXT('A') && Ch <= TEXT('Z')) ||
				(Ch >= TEXT('a') && Ch <= TEXT('z')) ||
				(Ch >= TEXT('0') && Ch <= TEXT('9'));
			Result.AppendChar(bIsAlphaNumeric ? Ch : TEXT('_'));
			if (Result.Len() >= 64)
			{
				break;
			}
		}
		Result.TrimStartAndEndInline();
		return Result;
	}

	static FString MakeReviewVisibleChangeId(const FString& TransactionId, const FString& VisualGroupKey)
	{
		const FString SanitizedGroup = SanitizeReviewIdSegment(VisualGroupKey);
		if (TransactionId.IsEmpty())
		{
			return SanitizedGroup.IsEmpty() ? TEXT("review_change") : SanitizedGroup;
		}
		return SanitizedGroup.IsEmpty()
			? TransactionId
			: FString::Printf(TEXT("%s_%s"), *TransactionId, *SanitizedGroup);
	}

	static bool ShouldAggregateGraphBodyTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		if (Target.Surface != EBlueprintHelperReviewSurface::Graph || Target.GraphName.IsEmpty())
		{
			return false;
		}

		const FString TargetKindLower = Target.TargetKind.ToLower();
		const FString GroupLower = Target.VisualGroupKey.ToLower();
		return !TargetKindLower.Contains(TEXT("component"))
			&& !TargetKindLower.Contains(TEXT("variable"))
			&& !TargetKindLower.Contains(TEXT("property"))
			&& !GroupLower.Contains(TEXT("component"))
			&& !GroupLower.Contains(TEXT("variable"))
			&& !GroupLower.Contains(TEXT("property"));
	}

	static void ApplyGraphBodyAggregation(FBlueprintHelperReviewAtomicTarget& Target)
	{
		if (!ShouldAggregateGraphBodyTarget(Target))
		{
			return;
		}

		Target.TargetKind = Target.TargetKind.IsEmpty() ? TEXT("graph_body") : Target.TargetKind;
		Target.VisualGroupKey = TEXT("graph_body|") + Target.GraphName;
		if (Target.DisplayLabel.IsEmpty())
		{
			Target.DisplayLabel = FString::Printf(TEXT("Modified [%s] graph body"), *Target.GraphName);
		}
	}

	static FString MakeReviewPackageNameFromAssetPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return FString();
		}
		if (FPackageName::IsValidObjectPath(AssetPath))
		{
			return FPackageName::ObjectPathToPackageName(AssetPath);
		}

		FString PackageName = AssetPath;
		int32 SubObjectIndex = INDEX_NONE;
		if (PackageName.FindChar(TEXT(':'), SubObjectIndex))
		{
			PackageName = PackageName.Left(SubObjectIndex);
		}

		int32 ObjectIndex = INDEX_NONE;
		if (PackageName.FindChar(TEXT('.'), ObjectIndex))
		{
			PackageName = PackageName.Left(ObjectIndex);
		}
		return PackageName;
	}

	static FString MakeReviewAssetLinkKey(const FString& AssetPath)
	{
		const FString PackageName = MakeReviewPackageNameFromAssetPath(AssetPath);
		return PackageName.IsEmpty() ? AssetPath : PackageName;
	}

	static bool DoesReviewAssetPackageExist(const FString& AssetPath)
	{
		const FString PackageName = MakeReviewPackageNameFromAssetPath(AssetPath);
		return FPackageName::IsValidLongPackageName(PackageName)
			&& FPackageName::DoesPackageExist(PackageName);
	}

	static bool IsReviewEvidenceTargetComplete(const FBlueprintHelperReviewAtomicTarget& Target, FString& OutReason)
	{
		if (Target.TargetKey.IsEmpty())
		{
			OutReason = TEXT("missing_anchor");
			return false;
		}
		if (Target.VisualGroupKey.IsEmpty())
		{
			OutReason = TEXT("missing_visible_change_group");
			return false;
		}
		if (Target.RecordedAfterHash.IsEmpty())
		{
			OutReason = TEXT("missing_recorded_after_hash");
			return false;
		}
		if (Target.BaselineHash.IsEmpty())
		{
			OutReason = TEXT("missing_baseline_hash");
			return false;
		}
		if (Target.RollbackDataRef.IsEmpty())
		{
			OutReason = TEXT("missing_rollback_data_ref");
			return false;
		}
		return true;
	}

	static bool IsAssetLifecycleRootTarget(
		const FBlueprintHelperReviewAtomicTarget& Target,
		EBlueprintHelperReviewChangeKind ChangeKind)
	{
		return ChangeKind == EBlueprintHelperReviewChangeKind::Added
			&& Target.TargetKind.Equals(TEXT("asset_factory"), ESearchCase::IgnoreCase);
	}

	static void ApplyAssetLifecycleRootMetadata(FBlueprintHelperReviewVisibleChange& Change)
	{
		const bool bHasAssetFactoryAddedTarget = Change.AtomicTargets.ContainsByPredicate(
			[&Change](const FBlueprintHelperReviewAtomicTarget& Target)
			{
				return IsAssetLifecycleRootTarget(Target, Change.ChangeKind);
			});
		if (!bHasAssetFactoryAddedTarget)
		{
			return;
		}

		Change.bIsAssetLifecycleRoot = true;
		Change.bRejectRemovesChildren = true;
		Change.ParentChangeId.Reset();
	}

	static bool IsPendingLifecycleLinkCandidate(const FBlueprintHelperReviewVisibleChange& Change)
	{
		return Change.Status == EBlueprintHelperReviewChangeStatus::Pending;
	}

	static void LinkPendingChildrenToLifecycleRoots(TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		TMap<FString, int32> PendingRootIndexByAssetPath;
		for (int32 Index = 0; Index < Changes.Num(); ++Index)
		{
			const FBlueprintHelperReviewVisibleChange& Change = Changes[Index];
			if (!IsPendingLifecycleLinkCandidate(Change)
				|| !Change.bIsAssetLifecycleRoot
				|| Change.AssetPath.IsEmpty())
			{
				continue;
			}

			PendingRootIndexByAssetPath.Add(MakeReviewAssetLinkKey(Change.AssetPath), Index);
		}

		for (FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			if (!IsPendingLifecycleLinkCandidate(Change) || Change.AssetPath.IsEmpty())
			{
				continue;
			}

			if (Change.bIsAssetLifecycleRoot)
			{
				Change.ParentChangeId.Reset();
				continue;
			}

			const int32* RootIndex = PendingRootIndexByAssetPath.Find(MakeReviewAssetLinkKey(Change.AssetPath));
			if (!RootIndex || !Changes.IsValidIndex(*RootIndex))
			{
				continue;
			}

			Change.ParentChangeId = Changes[*RootIndex].ChangeId;
		}
	}

	static int32 GetReviewSortValue(int32 Value)
	{
		return Value == INDEX_NONE ? MAX_int32 : Value;
	}

	static void SortVisibleChangesByReviewOrder(TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		Changes.Sort([](
			const FBlueprintHelperReviewVisibleChange& Left,
			const FBlueprintHelperReviewVisibleChange& Right)
		{
			const FString LeftAsset = MakeReviewAssetLinkKey(Left.AssetPath);
			const FString RightAsset = MakeReviewAssetLinkKey(Right.AssetPath);
			if (LeftAsset != RightAsset)
			{
				return LeftAsset < RightAsset;
			}

			const int32 LeftExecutionOrder = GetReviewSortValue(Left.ExecutionOrder);
			const int32 RightExecutionOrder = GetReviewSortValue(Right.ExecutionOrder);
			if (LeftExecutionOrder != RightExecutionOrder)
			{
				return LeftExecutionOrder < RightExecutionOrder;
			}

			const int32 LeftStep = GetReviewSortValue(Left.TaskStepIndex);
			const int32 RightStep = GetReviewSortValue(Right.TaskStepIndex);
			if (LeftStep != RightStep)
			{
				return LeftStep < RightStep;
			}

			const int32 LeftAtomic = GetReviewSortValue(Left.AtomicIndex);
			const int32 RightAtomic = GetReviewSortValue(Right.AtomicIndex);
			if (LeftAtomic != RightAtomic)
			{
				return LeftAtomic < RightAtomic;
			}

			if (Left.bIsAssetLifecycleRoot != Right.bIsAssetLifecycleRoot)
			{
				return Left.bIsAssetLifecycleRoot;
			}

			return Left.LocationKey < Right.LocationKey;
		});
	}

	static FString MakeLoadedVisibleChangeCollapseKey(const FBlueprintHelperReviewVisibleChange& Change)
	{
		const FString AssetKey = MakeReviewAssetLinkKey(Change.AssetPath);
		const FString RootPrefix = Change.bIsAssetLifecycleRoot ? TEXT("root") : TEXT("change");
		if (Change.AtomicTargets.Num() > 0)
		{
			return FString::Printf(
				TEXT("%s|%s|%s"),
				*RootPrefix,
				*AssetKey,
				*MakeReviewAtomicLookupKey(Change.AtomicTargets[0], Change.LocationKey));
		}

		return FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*RootPrefix,
			*AssetKey,
			*Change.GraphName,
			*Change.LocationKey);
	}

	static void AddUniqueReviewStrings(TArray<FString>& Target, const TArray<FString>& Source)
	{
		for (const FString& Value : Source)
		{
			if (!Value.IsEmpty())
			{
				Target.AddUnique(Value);
			}
		}
	}

	static void MergeReviewAtomicTargetsLatestWins(
		TArray<FBlueprintHelperReviewAtomicTarget>& ExistingTargets,
		const TArray<FBlueprintHelperReviewAtomicTarget>& IncomingTargets)
	{
		for (const FBlueprintHelperReviewAtomicTarget& IncomingTarget : IncomingTargets)
		{
			const FString IncomingKey = MakeReviewAtomicLookupKey(IncomingTarget, FString());
			bool bReplaced = false;
			for (FBlueprintHelperReviewAtomicTarget& ExistingTarget : ExistingTargets)
			{
				if (MakeReviewAtomicLookupKey(ExistingTarget, FString()) == IncomingKey)
				{
					ExistingTarget = IncomingTarget;
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				ExistingTargets.Add(IncomingTarget);
			}
		}
	}

	static void MergeVisibleChangeLatestWins(
		FBlueprintHelperReviewVisibleChange& Existing,
		const FBlueprintHelperReviewVisibleChange& Incoming)
	{
		TArray<FString> SourceTransactionIds = Existing.SourceTransactionIds;
		TArray<FString> LatestTransactionIds = Existing.LatestTransactionIds;
		TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets = Existing.AtomicTargets;

		AddUniqueReviewStrings(SourceTransactionIds, Incoming.SourceTransactionIds);
		AddUniqueReviewStrings(LatestTransactionIds, Incoming.LatestTransactionIds);
		MergeReviewAtomicTargetsLatestWins(AtomicTargets, Incoming.AtomicTargets);

		Existing = Incoming;
		Existing.SourceTransactionIds = SourceTransactionIds;
		Existing.LatestTransactionIds = LatestTransactionIds;
		Existing.AtomicTargets = AtomicTargets;
		ApplyAssetLifecycleRootMetadata(Existing);
	}

	static void CollapseVisibleChangesLatestWins(TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		TArray<FBlueprintHelperReviewVisibleChange> Collapsed;
		TMap<FString, int32> ExistingIndexByKey;
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			const FString CollapseKey = MakeLoadedVisibleChangeCollapseKey(Change);
			if (int32* ExistingIndex = ExistingIndexByKey.Find(CollapseKey))
			{
				MergeVisibleChangeLatestWins(Collapsed[*ExistingIndex], Change);
				continue;
			}

			ExistingIndexByKey.Add(CollapseKey, Collapsed.Num());
			Collapsed.Add(Change);
		}

		Changes = MoveTemp(Collapsed);
	}

	static FBlueprintHelperReviewVisibleChange MakeVisibleChangeFromEvidence(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FString& VisualGroupKey)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = MakeReviewVisibleChangeId(Evidence.TransactionId, VisualGroupKey);
		Change.AssetPath = Target.AssetPath.IsEmpty() ? Evidence.AssetPath : Target.AssetPath;
		Change.GraphName = Target.GraphName;
		Change.LocationKey = VisualGroupKey;
		Change.LatestTransactionId = Evidence.TransactionId;
		Change.LatestTransactionIds.Add(Evidence.TransactionId);
		Change.SourceTransactionIds.Add(Evidence.TransactionId);
		Change.ChangeKind = Evidence.ChangeKind;
		Change.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Evidence.DisplayLabel : Target.DisplayLabel;
		Change.BeforeSummary = Evidence.BeforeSummary;
		Change.AfterSummary = Evidence.AfterSummary;
		Change.ExecutionOrder = Target.ExecutionOrder;
		Change.TaskStepIndex = Target.TaskStepIndex;
		Change.AtomicIndex = Target.AtomicIndex;
		if (IsAssetLifecycleRootTarget(Target, Change.ChangeKind))
		{
			Change.bIsAssetLifecycleRoot = true;
			Change.bRejectRemovesChildren = true;
			Change.ParentChangeId.Reset();
		}
		return Change;
	}

	static EBlueprintHelperReviewChangeStatus ParseReviewChangeStatus(const FString& Status)
	{
		if (Status.Equals(TEXT("accepted"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeStatus::Accepted;
		}
		if (Status.Equals(TEXT("rejected"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeStatus::Rejected;
		}
		if (Status.Equals(TEXT("needs_action"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}
		if (Status.Equals(TEXT("superseded"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeStatus::Superseded;
		}
		if (Status.Equals(TEXT("reject_failed"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeStatus::RejectFailed;
		}
		return EBlueprintHelperReviewChangeStatus::Pending;
	}

	static EBlueprintHelperReviewChangeKind ParseReviewChangeKind(const FString& ChangeKind)
	{
		if (ChangeKind.Equals(TEXT("added"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeKind::Added;
		}
		if (ChangeKind.Equals(TEXT("removed"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeKind::Removed;
		}
		if (ChangeKind.Equals(TEXT("renamed"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeKind::Renamed;
		}
		if (ChangeKind.Equals(TEXT("variable_modified"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeKind::VariableModified;
		}
		if (ChangeKind.Equals(TEXT("signature_modified"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeKind::SignatureModified;
		}
		return EBlueprintHelperReviewChangeKind::Modified;
	}

	static EBlueprintHelperReviewSurface ParseReviewSurface(const FString& Surface)
	{
		if (Surface.Equals(TEXT("components"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewSurface::Components;
		}
		if (Surface.Equals(TEXT("my_blueprint"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewSurface::MyBlueprint;
		}
		if (Surface.Equals(TEXT("details"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewSurface::Details;
		}
		if (Surface.Equals(TEXT("umg_widget_tree"), ESearchCase::IgnoreCase)
			|| Surface.Equals(TEXT("umg"), ESearchCase::IgnoreCase)
			|| Surface.Equals(TEXT("umg_widget"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewSurface::UMGWidgetTree;
		}
		if (Surface.Equals(TEXT("data_table"), ESearchCase::IgnoreCase)
			|| Surface.Equals(TEXT("datatable"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewSurface::DataTable;
		}
		if (Surface.Equals(TEXT("data_asset"), ESearchCase::IgnoreCase)
			|| Surface.Equals(TEXT("object_details"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewSurface::DataAsset;
		}
		if (Surface.Equals(TEXT("graph"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewSurface::Graph;
		}
		return EBlueprintHelperReviewSurface::Unknown;
	}

	static void ReadReviewStringArray(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		TArray<FString>& OutValues)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Json.IsValid() || !Json->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (!Value.IsValid())
			{
				continue;
			}
			FString StringValue;
			if (Value->TryGetString(StringValue))
			{
				OutValues.AddUnique(StringValue);
			}
		}
	}

	static TArray<TSharedPtr<FJsonValue>> MakeReviewJsonStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
		return JsonValues;
	}

	static TSharedRef<FJsonObject> MakeReviewJsonVector2D(const FVector2D& Value)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("x"), Value.X);
		Json->SetNumberField(TEXT("y"), Value.Y);
		return Json;
	}

	static bool ReadReviewJsonVector2D(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		FVector2D& OutValue)
	{
		const TSharedPtr<FJsonObject>* VectorJson = nullptr;
		if (!Json.IsValid() ||
			!Json->TryGetObjectField(FieldName, VectorJson) ||
			!VectorJson ||
			!VectorJson->IsValid())
		{
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		if (!(*VectorJson)->TryGetNumberField(TEXT("x"), X) ||
			!(*VectorJson)->TryGetNumberField(TEXT("y"), Y))
		{
			return false;
		}

		OutValue = FVector2D(X, Y);
		return true;
	}

	static TSharedRef<FJsonObject> ReviewAtomicTargetToJson(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), Target.AssetPath);
		Json->SetStringField(TEXT("surface"), BlueprintHelperReviewSurfaceToString(Target.Surface));
		if (!Target.GraphName.IsEmpty()) Json->SetStringField(TEXT("graph_name"), Target.GraphName);
		Json->SetStringField(TEXT("target_key"), Target.TargetKey);
		if (!Target.TargetKind.IsEmpty()) Json->SetStringField(TEXT("target_kind"), Target.TargetKind);
		if (!Target.VisualGroupKey.IsEmpty()) Json->SetStringField(TEXT("visual_group_key"), Target.VisualGroupKey);
		if (!Target.DisplayLabel.IsEmpty()) Json->SetStringField(TEXT("display_label"), Target.DisplayLabel);
		if (!Target.LatestTransactionId.IsEmpty()) Json->SetStringField(TEXT("latest_transaction_id"), Target.LatestTransactionId);
		Json->SetArrayField(TEXT("source_transaction_ids"), MakeReviewJsonStringArray(Target.SourceTransactionIds));
		if (!Target.Ownership.IsEmpty()) Json->SetStringField(TEXT("ownership"), Target.Ownership);
		if (!Target.NodeGuid.IsEmpty()) Json->SetStringField(TEXT("node_guid"), Target.NodeGuid);
		if (!Target.PinPath.IsEmpty()) Json->SetStringField(TEXT("pin_path"), Target.PinPath);
		if (!Target.PropertyPath.IsEmpty()) Json->SetStringField(TEXT("property_path"), Target.PropertyPath);
		if (!Target.ComponentPath.IsEmpty()) Json->SetStringField(TEXT("component_path"), Target.ComponentPath);
		if (!Target.AnchorJson.IsEmpty()) Json->SetStringField(TEXT("anchor"), Target.AnchorJson);
		if (!Target.RecordedAfterHash.IsEmpty()) Json->SetStringField(TEXT("recorded_after_hash"), Target.RecordedAfterHash);
		if (!Target.BaselineHash.IsEmpty()) Json->SetStringField(TEXT("baseline_hash"), Target.BaselineHash);
		if (!Target.RollbackDataRef.IsEmpty()) Json->SetStringField(TEXT("rollback_data_ref"), Target.RollbackDataRef);
		if (Target.ExecutionOrder != INDEX_NONE) Json->SetNumberField(TEXT("execution_order"), Target.ExecutionOrder);
		if (Target.TaskStepIndex != INDEX_NONE) Json->SetNumberField(TEXT("task_step_index"), Target.TaskStepIndex);
		if (Target.AtomicIndex != INDEX_NONE) Json->SetNumberField(TEXT("atomic_index"), Target.AtomicIndex);
		if (Target.bHasGraphBounds)
		{
			Json->SetBoolField(TEXT("has_graph_bounds"), true);
			Json->SetObjectField(TEXT("graph_position"), MakeReviewJsonVector2D(Target.GraphPosition));
			Json->SetObjectField(TEXT("graph_size"), MakeReviewJsonVector2D(Target.GraphSize));
		}
		Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Target.Status));
		return Json;
	}

	static TSharedRef<FJsonObject> ReviewVisibleChangeToJson(const FBlueprintHelperReviewVisibleChange& Change)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("change_id"), Change.ChangeId);
		Json->SetStringField(TEXT("asset_path"), Change.AssetPath);
		if (!Change.GraphName.IsEmpty()) Json->SetStringField(TEXT("graph_name"), Change.GraphName);
		Json->SetStringField(TEXT("visual_group_key"), Change.LocationKey);
		Json->SetStringField(TEXT("latest_transaction_id"), Change.LatestTransactionId);
		Json->SetArrayField(TEXT("latest_transaction_ids"), MakeReviewJsonStringArray(Change.LatestTransactionIds));
		Json->SetArrayField(TEXT("source_transaction_ids"), MakeReviewJsonStringArray(Change.SourceTransactionIds));
		Json->SetStringField(TEXT("change_kind"), BlueprintHelperReviewChangeKindToString(Change.ChangeKind));
		Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Change.Status));
		if (!Change.DisplayLabel.IsEmpty()) Json->SetStringField(TEXT("display_label"), Change.DisplayLabel);
		if (!Change.BeforeSummary.IsEmpty()) Json->SetStringField(TEXT("before_summary"), Change.BeforeSummary);
		if (!Change.AfterSummary.IsEmpty()) Json->SetStringField(TEXT("after_summary"), Change.AfterSummary);
		if (!Change.NeedsActionReason.IsEmpty()) Json->SetStringField(TEXT("needs_action_reason"), Change.NeedsActionReason);
		if (!Change.ParentChangeId.IsEmpty()) Json->SetStringField(TEXT("parent_change_id"), Change.ParentChangeId);
		if (Change.ExecutionOrder != INDEX_NONE) Json->SetNumberField(TEXT("execution_order"), Change.ExecutionOrder);
		if (Change.TaskStepIndex != INDEX_NONE) Json->SetNumberField(TEXT("task_step_index"), Change.TaskStepIndex);
		if (Change.AtomicIndex != INDEX_NONE) Json->SetNumberField(TEXT("atomic_index"), Change.AtomicIndex);
		if (Change.bIsAssetLifecycleRoot) Json->SetBoolField(TEXT("is_asset_lifecycle_root"), true);
		if (Change.bRejectRemovesChildren) Json->SetBoolField(TEXT("reject_removes_children"), true);

		TArray<TSharedPtr<FJsonValue>> Targets;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			Targets.Add(MakeShared<FJsonValueObject>(ReviewAtomicTargetToJson(Target)));
		}
		Json->SetArrayField(TEXT("atomic_targets"), Targets);
		return Json;
	}

	static TSharedRef<FJsonObject> ReviewActionRecordToJson(const FBlueprintHelperReviewActionRecord& Action)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("action"), Action.Action);
		Json->SetArrayField(TEXT("target_keys"), MakeReviewJsonStringArray(Action.TargetKeys));
		if (!Action.OwnershipPolicy.IsEmpty()) Json->SetStringField(TEXT("ownership_policy"), Action.OwnershipPolicy);
		if (!Action.CreatedAt.IsEmpty()) Json->SetStringField(TEXT("created_at"), Action.CreatedAt);
		if (!Action.SourceTransactionId.IsEmpty()) Json->SetStringField(TEXT("source_transaction_id"), Action.SourceTransactionId);
		if (!Action.Message.IsEmpty()) Json->SetStringField(TEXT("message"), Action.Message);
		return Json;
	}

	static TSharedRef<FJsonObject> ReviewRecordToJson(const FBlueprintHelperReviewRecord& Record)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Record.Schema);
		Json->SetStringField(TEXT("review_record_id"), Record.ReviewRecordId);
		Json->SetStringField(TEXT("archive_session_id"), Record.ArchiveSessionId);
		Json->SetStringField(TEXT("asset_path"), Record.AssetPath);
		Json->SetArrayField(TEXT("source_task_run_ids"), MakeReviewJsonStringArray(Record.SourceTaskRunIds));
		Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Record.Status));
		Json->SetStringField(TEXT("storage_status"), BlueprintHelperReviewStorageStatusToString(Record.StorageStatus));

		TArray<TSharedPtr<FJsonValue>> Changes;
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			Changes.Add(MakeShared<FJsonValueObject>(ReviewVisibleChangeToJson(Change)));
		}
		Json->SetArrayField(TEXT("visible_changes"), Changes);

		TArray<TSharedPtr<FJsonValue>> Actions;
		for (const FBlueprintHelperReviewActionRecord& Action : Record.ReviewActions)
		{
			Actions.Add(MakeShared<FJsonValueObject>(ReviewActionRecordToJson(Action)));
		}
		Json->SetArrayField(TEXT("review_actions"), Actions);

		TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetNumberField(TEXT("transaction_count"), Record.SourceTransactionSummary.TransactionCount);
		Summary->SetArrayField(TEXT("task_run_ids"), MakeReviewJsonStringArray(Record.SourceTransactionSummary.TaskRunIds));
		Summary->SetArrayField(TEXT("operation_kinds"), MakeReviewJsonStringArray(Record.SourceTransactionSummary.OperationKinds));
		Summary->SetArrayField(TEXT("asset_paths"), MakeReviewJsonStringArray(Record.SourceTransactionSummary.AssetPaths));
		Summary->SetArrayField(TEXT("transaction_ids"), MakeReviewJsonStringArray(Record.SourceTransactionSummary.TransactionIds));
		if (!Record.SourceTransactionSummary.CreatedAtFirst.IsEmpty())
		{
			Summary->SetStringField(TEXT("created_at_first"), Record.SourceTransactionSummary.CreatedAtFirst);
		}
		if (!Record.SourceTransactionSummary.CreatedAtLast.IsEmpty())
		{
			Summary->SetStringField(TEXT("created_at_last"), Record.SourceTransactionSummary.CreatedAtLast);
		}
		Summary->SetStringField(TEXT("final_review_status"),
			BlueprintHelperReviewChangeStatusToString(Record.SourceTransactionSummary.FinalReviewStatus));
		Json->SetObjectField(TEXT("source_transaction_summary"), Summary);

		TSharedRef<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetArrayField(TEXT("debug_case_ids"), MakeReviewJsonStringArray(Record.DebugCaseIds));
		Json->SetObjectField(TEXT("diagnostics"), Diagnostics);
		return Json;
	}

	static TSharedRef<FJsonObject> ReviewArchiveSessionToJson(const FBlueprintHelperReviewArchiveSession& ArchiveSession)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), ArchiveSession.Schema);
		Json->SetStringField(TEXT("archive_session_id"), ArchiveSession.ArchiveSessionId);
		Json->SetStringField(TEXT("task_run_id"), ArchiveSession.TaskRunId);
		Json->SetArrayField(TEXT("allowed_target_assets"), MakeReviewJsonStringArray(ArchiveSession.AllowedTargetAssets));
		Json->SetArrayField(TEXT("baseline_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSnapshotRefs));
		Json->SetArrayField(TEXT("baseline_semantic_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSemanticSnapshotRefs));
		TSharedRef<FJsonObject> Baseline = MakeShared<FJsonObject>();
		if (!ArchiveSession.BaselineDirtyAssetPolicy.IsEmpty())
		{
			Baseline->SetStringField(TEXT("dirty_asset_policy"), ArchiveSession.BaselineDirtyAssetPolicy);
		}
		if (!ArchiveSession.BaselineSnapshotTrust.IsEmpty())
		{
			Baseline->SetStringField(TEXT("snapshot_trust"), ArchiveSession.BaselineSnapshotTrust);
		}
		Baseline->SetArrayField(TEXT("dirty_target_assets"), MakeReviewJsonStringArray(ArchiveSession.DirtyTargetAssets));
		Baseline->SetArrayField(TEXT("warnings"), MakeReviewJsonStringArray(ArchiveSession.BaselineWarnings));
		Baseline->SetArrayField(TEXT("disk_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSnapshotRefs));
		Baseline->SetArrayField(TEXT("semantic_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSemanticSnapshotRefs));
		Json->SetObjectField(TEXT("baseline"), Baseline);
		if (!ArchiveSession.CreatedAt.IsEmpty())
		{
			Json->SetStringField(TEXT("created_at"), ArchiveSession.CreatedAt);
		}
		return Json;
	}

	static bool ReadReviewArchiveSessionFromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperReviewArchiveSession& OutArchiveSession)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		FString Schema;
		Json->TryGetStringField(TEXT("schema"), Schema);
		if (Schema != TEXT("BlueprintHelper.ArchiveSession.v1"))
		{
			return false;
		}

		OutArchiveSession = FBlueprintHelperReviewArchiveSession();
		OutArchiveSession.Schema = Schema;
		Json->TryGetStringField(TEXT("archive_session_id"), OutArchiveSession.ArchiveSessionId);
		Json->TryGetStringField(TEXT("task_run_id"), OutArchiveSession.TaskRunId);
		Json->TryGetStringField(TEXT("created_at"), OutArchiveSession.CreatedAt);
		ReadReviewStringArray(Json, TEXT("allowed_target_assets"), OutArchiveSession.AllowedTargetAssets);
		ReadReviewStringArray(Json, TEXT("baseline_snapshot_refs"), OutArchiveSession.BaselineSnapshotRefs);
		ReadReviewStringArray(Json, TEXT("baseline_semantic_snapshot_refs"), OutArchiveSession.BaselineSemanticSnapshotRefs);

		const TSharedPtr<FJsonObject>* BaselineJson = nullptr;
		if (Json->TryGetObjectField(TEXT("baseline"), BaselineJson) && BaselineJson && BaselineJson->IsValid())
		{
			(*BaselineJson)->TryGetStringField(TEXT("dirty_asset_policy"), OutArchiveSession.BaselineDirtyAssetPolicy);
			(*BaselineJson)->TryGetStringField(TEXT("snapshot_trust"), OutArchiveSession.BaselineSnapshotTrust);
			ReadReviewStringArray(*BaselineJson, TEXT("dirty_target_assets"), OutArchiveSession.DirtyTargetAssets);
			ReadReviewStringArray(*BaselineJson, TEXT("warnings"), OutArchiveSession.BaselineWarnings);
		}
		return !OutArchiveSession.ArchiveSessionId.IsEmpty();
	}

	static bool ReadReviewRecordFromJson(const TSharedPtr<FJsonObject>& Json, FBlueprintHelperReviewRecord& OutRecord)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		FString Schema;
		Json->TryGetStringField(TEXT("schema"), Schema);
		if (Schema != TEXT("BlueprintHelper.ReviewRecord.v1"))
		{
			return false;
		}

		OutRecord = FBlueprintHelperReviewRecord();
		OutRecord.Schema = Schema;
		Json->TryGetStringField(TEXT("review_record_id"), OutRecord.ReviewRecordId);
		Json->TryGetStringField(TEXT("archive_session_id"), OutRecord.ArchiveSessionId);
		Json->TryGetStringField(TEXT("asset_path"), OutRecord.AssetPath);
		ReadReviewStringArray(Json, TEXT("source_task_run_ids"), OutRecord.SourceTaskRunIds);
		FString Status;
		Json->TryGetStringField(TEXT("status"), Status);
		OutRecord.Status = ParseReviewChangeStatus(Status);

		FString StorageStatus;
		Json->TryGetStringField(TEXT("storage_status"), StorageStatus);
		if (StorageStatus.Equals(TEXT("compacted"), ESearchCase::IgnoreCase))
		{
			OutRecord.StorageStatus = EBlueprintHelperReviewStorageStatus::Compacted;
		}

		const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
		if (Json->TryGetArrayField(TEXT("visible_changes"), Changes) && Changes)
		{
			for (const TSharedPtr<FJsonValue>& ChangeValue : *Changes)
			{
				const TSharedPtr<FJsonObject> ChangeJson = ChangeValue.IsValid()
					? ChangeValue->AsObject()
					: nullptr;
				if (!ChangeJson.IsValid())
				{
					continue;
				}

				FBlueprintHelperReviewVisibleChange Change;
				ChangeJson->TryGetStringField(TEXT("change_id"), Change.ChangeId);
				ChangeJson->TryGetStringField(TEXT("asset_path"), Change.AssetPath);
				ChangeJson->TryGetStringField(TEXT("graph_name"), Change.GraphName);
				ChangeJson->TryGetStringField(TEXT("visual_group_key"), Change.LocationKey);
				ChangeJson->TryGetStringField(TEXT("latest_transaction_id"), Change.LatestTransactionId);
				ReadReviewStringArray(ChangeJson, TEXT("latest_transaction_ids"), Change.LatestTransactionIds);
				ReadReviewStringArray(ChangeJson, TEXT("source_transaction_ids"), Change.SourceTransactionIds);
				FString ChangeStatus;
				ChangeJson->TryGetStringField(TEXT("status"), ChangeStatus);
				Change.Status = ParseReviewChangeStatus(ChangeStatus);
				FString ChangeKind;
				ChangeJson->TryGetStringField(TEXT("change_kind"), ChangeKind);
				Change.ChangeKind = ParseReviewChangeKind(ChangeKind);
				ChangeJson->TryGetStringField(TEXT("display_label"), Change.DisplayLabel);
				ChangeJson->TryGetStringField(TEXT("before_summary"), Change.BeforeSummary);
				ChangeJson->TryGetStringField(TEXT("after_summary"), Change.AfterSummary);
				ChangeJson->TryGetStringField(TEXT("needs_action_reason"), Change.NeedsActionReason);
				ChangeJson->TryGetStringField(TEXT("parent_change_id"), Change.ParentChangeId);
				double OrderValue = 0.0;
				if (ChangeJson->TryGetNumberField(TEXT("execution_order"), OrderValue))
				{
					Change.ExecutionOrder = static_cast<int32>(OrderValue);
				}
				if (ChangeJson->TryGetNumberField(TEXT("task_step_index"), OrderValue))
				{
					Change.TaskStepIndex = static_cast<int32>(OrderValue);
				}
				if (ChangeJson->TryGetNumberField(TEXT("atomic_index"), OrderValue))
				{
					Change.AtomicIndex = static_cast<int32>(OrderValue);
				}
				ChangeJson->TryGetBoolField(TEXT("is_asset_lifecycle_root"), Change.bIsAssetLifecycleRoot);
				ChangeJson->TryGetBoolField(TEXT("reject_removes_children"), Change.bRejectRemovesChildren);

				const TArray<TSharedPtr<FJsonValue>>* Targets = nullptr;
				if (ChangeJson->TryGetArrayField(TEXT("atomic_targets"), Targets) && Targets)
				{
					for (const TSharedPtr<FJsonValue>& TargetValue : *Targets)
					{
						const TSharedPtr<FJsonObject> TargetJson = TargetValue.IsValid()
							? TargetValue->AsObject()
							: nullptr;
						if (!TargetJson.IsValid())
						{
							continue;
						}

						FBlueprintHelperReviewAtomicTarget Target;
						TargetJson->TryGetStringField(TEXT("asset_path"), Target.AssetPath);
						FString Surface;
						TargetJson->TryGetStringField(TEXT("surface"), Surface);
						Target.Surface = ParseReviewSurface(Surface);
						TargetJson->TryGetStringField(TEXT("graph_name"), Target.GraphName);
						TargetJson->TryGetStringField(TEXT("target_key"), Target.TargetKey);
						TargetJson->TryGetStringField(TEXT("target_kind"), Target.TargetKind);
						TargetJson->TryGetStringField(TEXT("visual_group_key"), Target.VisualGroupKey);
						TargetJson->TryGetStringField(TEXT("display_label"), Target.DisplayLabel);
						TargetJson->TryGetStringField(TEXT("latest_transaction_id"), Target.LatestTransactionId);
						ReadReviewStringArray(TargetJson, TEXT("source_transaction_ids"), Target.SourceTransactionIds);
						TargetJson->TryGetStringField(TEXT("ownership"), Target.Ownership);
						TargetJson->TryGetStringField(TEXT("node_guid"), Target.NodeGuid);
						TargetJson->TryGetStringField(TEXT("pin_path"), Target.PinPath);
						TargetJson->TryGetStringField(TEXT("property_path"), Target.PropertyPath);
						TargetJson->TryGetStringField(TEXT("component_path"), Target.ComponentPath);
						TargetJson->TryGetStringField(TEXT("anchor"), Target.AnchorJson);
						TargetJson->TryGetStringField(TEXT("recorded_after_hash"), Target.RecordedAfterHash);
						TargetJson->TryGetStringField(TEXT("baseline_hash"), Target.BaselineHash);
						TargetJson->TryGetStringField(TEXT("rollback_data_ref"), Target.RollbackDataRef);
						double TargetOrderValue = 0.0;
						if (TargetJson->TryGetNumberField(TEXT("execution_order"), TargetOrderValue))
						{
							Target.ExecutionOrder = static_cast<int32>(TargetOrderValue);
						}
						if (TargetJson->TryGetNumberField(TEXT("task_step_index"), TargetOrderValue))
						{
							Target.TaskStepIndex = static_cast<int32>(TargetOrderValue);
						}
						if (TargetJson->TryGetNumberField(TEXT("atomic_index"), TargetOrderValue))
						{
							Target.AtomicIndex = static_cast<int32>(TargetOrderValue);
						}
						TargetJson->TryGetBoolField(TEXT("has_graph_bounds"), Target.bHasGraphBounds);
						ReadReviewJsonVector2D(TargetJson, TEXT("graph_position"), Target.GraphPosition);
						ReadReviewJsonVector2D(TargetJson, TEXT("graph_size"), Target.GraphSize);
						FString TargetStatus;
						TargetJson->TryGetStringField(TEXT("status"), TargetStatus);
						Target.Status = ParseReviewChangeStatus(TargetStatus);
						Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
							Target.Surface,
							Target.TargetKind,
							Target.TargetKey,
							Target.VisualGroupKey,
							Change.LocationKey);
						Change.AtomicTargets.Add(Target);
					}
				}

				ApplyAssetLifecycleRootMetadata(Change);
				OutRecord.VisibleChanges.Add(Change);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
		if (Json->TryGetArrayField(TEXT("review_actions"), Actions) && Actions)
		{
			for (const TSharedPtr<FJsonValue>& ActionValue : *Actions)
			{
				const TSharedPtr<FJsonObject> ActionJson = ActionValue.IsValid()
					? ActionValue->AsObject()
					: nullptr;
				if (!ActionJson.IsValid())
				{
					continue;
				}

				FBlueprintHelperReviewActionRecord Action;
				ActionJson->TryGetStringField(TEXT("action"), Action.Action);
				ReadReviewStringArray(ActionJson, TEXT("target_keys"), Action.TargetKeys);
				ActionJson->TryGetStringField(TEXT("ownership_policy"), Action.OwnershipPolicy);
				ActionJson->TryGetStringField(TEXT("created_at"), Action.CreatedAt);
				ActionJson->TryGetStringField(TEXT("source_transaction_id"), Action.SourceTransactionId);
				ActionJson->TryGetStringField(TEXT("message"), Action.Message);
				OutRecord.ReviewActions.Add(Action);
			}
		}

		const TSharedPtr<FJsonObject>* Summary = nullptr;
		if (Json->TryGetObjectField(TEXT("source_transaction_summary"), Summary) && Summary)
		{
			OutRecord.SourceTransactionSummary.TransactionCount =
				static_cast<int32>((*Summary)->GetNumberField(TEXT("transaction_count")));
			ReadReviewStringArray(*Summary, TEXT("task_run_ids"), OutRecord.SourceTransactionSummary.TaskRunIds);
			ReadReviewStringArray(*Summary, TEXT("operation_kinds"), OutRecord.SourceTransactionSummary.OperationKinds);
			ReadReviewStringArray(*Summary, TEXT("asset_paths"), OutRecord.SourceTransactionSummary.AssetPaths);
			ReadReviewStringArray(*Summary, TEXT("transaction_ids"), OutRecord.SourceTransactionSummary.TransactionIds);
			(*Summary)->TryGetStringField(TEXT("created_at_first"), OutRecord.SourceTransactionSummary.CreatedAtFirst);
			(*Summary)->TryGetStringField(TEXT("created_at_last"), OutRecord.SourceTransactionSummary.CreatedAtLast);
			FString FinalReviewStatus;
			(*Summary)->TryGetStringField(TEXT("final_review_status"), FinalReviewStatus);
			OutRecord.SourceTransactionSummary.FinalReviewStatus = ParseReviewChangeStatus(FinalReviewStatus);
		}

		const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
		if (Json->TryGetObjectField(TEXT("diagnostics"), Diagnostics) && Diagnostics)
		{
			ReadReviewStringArray(*Diagnostics, TEXT("debug_case_ids"), OutRecord.DebugCaseIds);
		}

		return !OutRecord.ReviewRecordId.IsEmpty();
	}

	static void MergeReviewRecord(FBlueprintHelperReviewRecord& Existing, const FBlueprintHelperReviewRecord& Incoming)
	{
		for (const FString& TaskRunId : Incoming.SourceTaskRunIds)
		{
			Existing.SourceTaskRunIds.AddUnique(TaskRunId);
		}
		for (const FString& DebugCaseId : Incoming.DebugCaseIds)
		{
			Existing.DebugCaseIds.AddUnique(DebugCaseId);
		}

		for (const FBlueprintHelperReviewVisibleChange& IncomingChange : Incoming.VisibleChanges)
		{
			FBlueprintHelperReviewVisibleChange* ExistingChange = Existing.VisibleChanges.FindByPredicate(
				[&IncomingChange](const FBlueprintHelperReviewVisibleChange& Candidate)
				{
					return Candidate.LocationKey == IncomingChange.LocationKey;
				});
			if (!ExistingChange)
			{
				Existing.VisibleChanges.Add(IncomingChange);
				continue;
			}

			for (const FBlueprintHelperReviewAtomicTarget& IncomingTarget : IncomingChange.AtomicTargets)
			{
				FBlueprintHelperReviewAtomicTarget* ExistingTarget = nullptr;
				if (!IncomingTarget.TargetKey.IsEmpty())
				{
					ExistingTarget = ExistingChange->AtomicTargets.FindByPredicate(
						[&IncomingTarget](const FBlueprintHelperReviewAtomicTarget& Candidate)
						{
							return Candidate.TargetKey == IncomingTarget.TargetKey
								&& Candidate.Surface == IncomingTarget.Surface
								&& Candidate.GraphName == IncomingTarget.GraphName;
						});
				}

				if (!ExistingTarget)
				{
					ExistingChange->AtomicTargets.Add(IncomingTarget);
					continue;
				}

				TArray<FString> SourceTransactionIds = ExistingTarget->SourceTransactionIds;
				for (const FString& SourceTransactionId : IncomingTarget.SourceTransactionIds)
				{
					SourceTransactionIds.AddUnique(SourceTransactionId);
				}
				*ExistingTarget = IncomingTarget;
				ExistingTarget->SourceTransactionIds = SourceTransactionIds;
			}

			for (const FString& SourceTransactionId : IncomingChange.SourceTransactionIds)
			{
				ExistingChange->SourceTransactionIds.AddUnique(SourceTransactionId);
			}
			for (const FString& LatestTransactionId : IncomingChange.LatestTransactionIds)
			{
				ExistingChange->LatestTransactionIds.AddUnique(LatestTransactionId);
			}
			ExistingChange->LatestTransactionId = IncomingChange.LatestTransactionId;
			ExistingChange->ChangeId = IncomingChange.ChangeId;
			ExistingChange->ChangeKind = IncomingChange.ChangeKind;
			ExistingChange->AfterSummary = IncomingChange.AfterSummary;
			ExistingChange->ParentChangeId = IncomingChange.ParentChangeId;
			ExistingChange->ExecutionOrder = IncomingChange.ExecutionOrder;
			ExistingChange->TaskStepIndex = IncomingChange.TaskStepIndex;
			ExistingChange->AtomicIndex = IncomingChange.AtomicIndex;
			ExistingChange->bIsAssetLifecycleRoot = IncomingChange.bIsAssetLifecycleRoot;
			ExistingChange->bRejectRemovesChildren = IncomingChange.bRejectRemovesChildren;
			if (IncomingChange.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
				|| IncomingChange.Status == EBlueprintHelperReviewChangeStatus::RejectFailed)
			{
				ExistingChange->Status = IncomingChange.Status;
				ExistingChange->NeedsActionReason = IncomingChange.NeedsActionReason;
			}
			ApplyAssetLifecycleRootMetadata(*ExistingChange);
		}

		for (const FString& TaskRunId : Incoming.SourceTransactionSummary.TaskRunIds)
		{
			Existing.SourceTransactionSummary.TaskRunIds.AddUnique(TaskRunId);
		}
		for (const FString& OperationKind : Incoming.SourceTransactionSummary.OperationKinds)
		{
			Existing.SourceTransactionSummary.OperationKinds.AddUnique(OperationKind);
		}
		for (const FString& AssetPath : Incoming.SourceTransactionSummary.AssetPaths)
		{
			Existing.SourceTransactionSummary.AssetPaths.AddUnique(AssetPath);
		}
		for (const FString& TransactionId : Incoming.SourceTransactionSummary.TransactionIds)
		{
			Existing.SourceTransactionSummary.TransactionIds.AddUnique(TransactionId);
		}
		if (!Incoming.SourceTransactionSummary.CreatedAtFirst.IsEmpty()
			&& (Existing.SourceTransactionSummary.CreatedAtFirst.IsEmpty()
				|| Incoming.SourceTransactionSummary.CreatedAtFirst < Existing.SourceTransactionSummary.CreatedAtFirst))
		{
			Existing.SourceTransactionSummary.CreatedAtFirst = Incoming.SourceTransactionSummary.CreatedAtFirst;
		}
		if (!Incoming.SourceTransactionSummary.CreatedAtLast.IsEmpty()
			&& (Existing.SourceTransactionSummary.CreatedAtLast.IsEmpty()
				|| Incoming.SourceTransactionSummary.CreatedAtLast > Existing.SourceTransactionSummary.CreatedAtLast))
		{
			Existing.SourceTransactionSummary.CreatedAtLast = Incoming.SourceTransactionSummary.CreatedAtLast;
		}
		Existing.SourceTransactionSummary.TransactionCount =
			Existing.SourceTransactionSummary.TransactionIds.Num();

		if (Incoming.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Incoming.Status == EBlueprintHelperReviewChangeStatus::RejectFailed)
		{
			Existing.Status = Incoming.Status;
		}
		Existing.SourceTransactionSummary.FinalReviewStatus = Existing.Status;
	}

	static bool IsSafeReviewRecordId(const FString& ReviewRecordId)
	{
		if (ReviewRecordId.IsEmpty())
		{
			return false;
		}

		for (const TCHAR Ch : ReviewRecordId)
		{
			if (!(FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-')))
			{
				return false;
			}
		}
		return true;
	}

	static FString GetRecordsDir()
	{
		return FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Review")
			/ TEXT("Records");
	}

	static FString GetRecordPath(const FString& ReviewRecordId)
	{
		return GetRecordsDir() / FString::Printf(TEXT("%s.json"), *ReviewRecordId);
	}

	static bool ReviewTargetMatches(const FBlueprintHelperReviewAtomicTarget& Target, const TSet<FString>& TargetKeys)
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
		bool bAnyPending = false;
		bool bAnyNeedsAction = false;
		bool bAnyRejectFailed = false;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Targets)
		{
			bAllAccepted &= Target.Status == EBlueprintHelperReviewChangeStatus::Accepted;
			bAllRejected &= Target.Status == EBlueprintHelperReviewChangeStatus::Rejected;
			bAnyPending |= Target.Status == EBlueprintHelperReviewChangeStatus::Pending;
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

};

FString FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(
	const FString& GraphName,
	const FString& BlockRefOrId)
{
	FString Normalized = BlockRefOrId;
	Normalized.TrimStartAndEndInline();
	if (GraphName.IsEmpty() || Normalized.IsEmpty())
	{
		return Normalized;
	}

	const FString GraphPrefix = GraphName + TEXT("_");
	if (Normalized.StartsWith(GraphPrefix, ESearchCase::CaseSensitive))
	{
		return Normalized;
	}

	return GraphPrefix + Normalized;
}

FString FBlueprintHelperReviewStoreService::MakeReviewRecordId(
	const FString& ArchiveSessionId,
	const FString& AssetPath)
{
	FString NormalizedAsset = AssetPath;
	NormalizedAsset.TrimStartAndEndInline();
	NormalizedAsset.ReplaceInline(TEXT("/"), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT("\\"), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT(":"), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT("."), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT(" "), TEXT("_"));
	while (NormalizedAsset.Contains(TEXT("__")))
	{
		NormalizedAsset.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	while (NormalizedAsset.StartsWith(TEXT("_")))
	{
		NormalizedAsset.RemoveFromStart(TEXT("_"));
	}
	while (NormalizedAsset.EndsWith(TEXT("_")))
	{
		NormalizedAsset.RemoveFromEnd(TEXT("_"));
	}
	return FString::Printf(TEXT("review_%s_%s"), *ArchiveSessionId, *NormalizedAsset);
}

TArray<FBlueprintHelperReviewVisibleChange> FBlueprintHelperReviewStoreService::BuildVisibleChanges(
	const TArray<FBlueprintHelperReviewTransactionInput>& Transactions) const
{
	TMap<FString, FBlueprintHelperReviewVisibleChange> AtomicChanges;
	TArray<FString> AtomicOrder;

	for (const FBlueprintHelperReviewTransactionInput& Input : Transactions)
	{
		if (Input.ChangeKind == EBlueprintHelperReviewChangeKind::Renamed)
		{
			FBlueprintHelperReviewTransactionInput Removed = Input;
			Removed.ChangeKind = EBlueprintHelperReviewChangeKind::Removed;
			Removed.LocationKey = Input.LocationKey + TEXT(":rename_removed");
			Removed.AfterSummary = TEXT("");
			for (FBlueprintHelperReviewAtomicTarget& Target : Removed.AtomicTargets)
			{
				Target.TargetKey += TEXT(":rename_removed");
				Target.VisualGroupKey += TEXT(":rename_removed");
			}
			AddAtomicTargetsForInput(Removed, AtomicChanges, AtomicOrder);

			FBlueprintHelperReviewTransactionInput Added = Input;
			Added.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
			Added.LocationKey = Input.LocationKey + TEXT(":rename_added");
			Added.BeforeSummary = TEXT("");
			for (FBlueprintHelperReviewAtomicTarget& Target : Added.AtomicTargets)
			{
				Target.TargetKey += TEXT(":rename_added");
				Target.VisualGroupKey += TEXT(":rename_added");
			}
			AddAtomicTargetsForInput(Added, AtomicChanges, AtomicOrder);
			continue;
		}

		AddAtomicTargetsForInput(Input, AtomicChanges, AtomicOrder);
	}

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	TMap<FString, int32> GroupToIndex;
	for (const FString& AtomicKey : AtomicOrder)
	{
		if (const FBlueprintHelperReviewVisibleChange* AtomicChange = AtomicChanges.Find(AtomicKey))
		{
			GroupAtomicVisibleChange(*AtomicChange, GroupToIndex, Changes);
		}
	}

	return Changes;
}

TArray<FBlueprintHelperReviewRecord> FBlueprintHelperReviewStoreService::BuildReviewRecordsFromEvidence(
	const TArray<FBlueprintHelperWriteReviewEvidence>& Evidences) const
{
	TMap<FString, FBlueprintHelperReviewRecord> RecordsById;
	TArray<FString> RecordOrder;

	for (const FBlueprintHelperWriteReviewEvidence& Evidence : Evidences)
	{
		if (Evidence.ArchiveSessionId.IsEmpty() || Evidence.AssetPath.IsEmpty())
		{
			continue;
		}

		const FString RecordId = MakeReviewRecordId(Evidence.ArchiveSessionId, Evidence.AssetPath);
		FBlueprintHelperReviewRecord* Record = RecordsById.Find(RecordId);
		if (!Record)
		{
			FBlueprintHelperReviewRecord NewRecord;
			NewRecord.ReviewRecordId = RecordId;
			NewRecord.ArchiveSessionId = Evidence.ArchiveSessionId;
			NewRecord.AssetPath = Evidence.AssetPath;
			NewRecord.SourceTransactionSummary.AssetPaths.AddUnique(Evidence.AssetPath);
			RecordsById.Add(RecordId, NewRecord);
			RecordOrder.Add(RecordId);
			Record = RecordsById.Find(RecordId);
		}

		if (!Record)
		{
			continue;
		}

		if (!Evidence.TaskRunId.IsEmpty())
		{
			Record->SourceTaskRunIds.AddUnique(Evidence.TaskRunId);
			Record->SourceTransactionSummary.TaskRunIds.AddUnique(Evidence.TaskRunId);
		}
		if (!Evidence.OperationKind.IsEmpty())
		{
			Record->SourceTransactionSummary.OperationKinds.AddUnique(Evidence.OperationKind);
		}
		if (!Evidence.TransactionId.IsEmpty())
		{
			Record->SourceTransactionSummary.TransactionIds.AddUnique(Evidence.TransactionId);
		}
		if (!Evidence.CreatedAt.IsEmpty())
		{
			if (Record->SourceTransactionSummary.CreatedAtFirst.IsEmpty()
				|| Evidence.CreatedAt < Record->SourceTransactionSummary.CreatedAtFirst)
			{
				Record->SourceTransactionSummary.CreatedAtFirst = Evidence.CreatedAt;
			}
			if (Record->SourceTransactionSummary.CreatedAtLast.IsEmpty()
				|| Evidence.CreatedAt > Record->SourceTransactionSummary.CreatedAtLast)
			{
				Record->SourceTransactionSummary.CreatedAtLast = Evidence.CreatedAt;
			}
		}
		for (const FString& DebugCaseId : Evidence.DebugCaseIds)
		{
			Record->DebugCaseIds.AddUnique(DebugCaseId);
		}

		AddEvidenceAtomicTargets(Evidence, *Record);
		Record->SourceTransactionSummary.TransactionCount =
			Record->SourceTransactionSummary.TransactionIds.Num();
	}

	TArray<FBlueprintHelperReviewRecord> Records;
	for (const FString& RecordId : RecordOrder)
	{
		if (FBlueprintHelperReviewRecord* Record = RecordsById.Find(RecordId))
		{
			FBlueprintHelperReviewStoreServiceLocalUtils::LinkPendingChildrenToLifecycleRoots(Record->VisibleChanges);
			FBlueprintHelperReviewStoreServiceLocalUtils::SortVisibleChangesByReviewOrder(Record->VisibleChanges);
			bool bNeedsAction = false;
			bool bHasPending = false;
			for (const FBlueprintHelperReviewVisibleChange& Change : Record->VisibleChanges)
			{
				bNeedsAction |= Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
				bHasPending |= Change.Status == EBlueprintHelperReviewChangeStatus::Pending;
			}
			Record->Status = bNeedsAction
				? EBlueprintHelperReviewChangeStatus::NeedsAction
				: (bHasPending ? EBlueprintHelperReviewChangeStatus::Pending : Record->Status);
			Record->SourceTransactionSummary.FinalReviewStatus = Record->Status;
			Records.Add(*Record);
		}
	}

	return Records;
}

TArray<FBlueprintHelperReviewRecord> FBlueprintHelperReviewStoreService::BuildReviewRecordsFromFragmentEvidence(
	const FBlueprintHelperGraphFragmentEvidenceBundle& FragmentEvidence,
	const FString& ArchiveSessionId,
	const FString& AssetPath,
	const FString& OperationKind,
	const FString& TaskRunId,
	const FString& TransactionId,
	const FString& CreatedAt) const
{
	if (ArchiveSessionId.IsEmpty() || AssetPath.IsEmpty() || FragmentEvidence.IsEmpty())
	{
		return {};
	}

	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.ArchiveSessionId = ArchiveSessionId;
	Evidence.TaskRunId = TaskRunId;
	Evidence.TransactionId = TransactionId.IsEmpty() ? FragmentEvidence.BundleId : TransactionId;
	Evidence.CreatedAt = CreatedAt;
	Evidence.AssetPath = AssetPath;
	Evidence.OperationKind = OperationKind.IsEmpty() ? TEXT("graph_fragment") : OperationKind;
	Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Evidence.DisplayLabel = TEXT("Graph body changes");
	Evidence.AfterSummary = FString::Printf(
		TEXT("Fragment evidence scopes=%d fragments=%d diagnostics=%d"),
		FragmentEvidence.ReviewScopes.Num(),
		FragmentEvidence.Fragments.Num(),
		FragmentEvidence.Diagnostics.Num());

	for (const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope : FragmentEvidence.ReviewScopes)
	{
		const FString GraphName = !Scope.GraphName.IsEmpty()
			? Scope.GraphName
			: (!Scope.ScopeName.IsEmpty() ? Scope.ScopeName : TEXT("Graph"));
		const FString ScopeLabel = !Scope.ScopeName.IsEmpty() ? Scope.ScopeName : GraphName;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = GraphName;
		Target.TargetKind = TEXT("graph_body");
		Target.TargetKey = Scope.ScopeId.IsEmpty()
			? FString::Printf(TEXT("graph_body:%s"), *GraphName)
			: Scope.ScopeId;
		Target.VisualGroupKey = TEXT("graph_body|") + GraphName;
		Target.DisplayLabel = FString::Printf(TEXT("Modified [%s] graph body"), *ScopeLabel);
		if (!Evidence.TransactionId.IsEmpty())
		{
			Target.LatestTransactionId = Evidence.TransactionId;
			Target.SourceTransactionIds.AddUnique(Evidence.TransactionId);
		}
		Evidence.AtomicTargets.Add(Target);
	}

	if (Evidence.AtomicTargets.Num() == 0)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = TEXT("Graph");
		Target.TargetKind = TEXT("graph_body");
		Target.TargetKey = FragmentEvidence.BundleId.IsEmpty() ? TEXT("graph_body") : FragmentEvidence.BundleId;
		Target.VisualGroupKey = TEXT("graph_body|Graph");
		Target.DisplayLabel = TEXT("Modified graph body");
		if (!Evidence.TransactionId.IsEmpty())
		{
			Target.LatestTransactionId = Evidence.TransactionId;
			Target.SourceTransactionIds.AddUnique(Evidence.TransactionId);
		}
		Evidence.AtomicTargets.Add(Target);
	}

	return BuildReviewRecordsFromEvidence({Evidence});
}

TArray<FBlueprintHelperReviewRecord> FBlueprintHelperReviewStoreService::QueryReviewRecords(
	const FBlueprintHelperReviewRecordQuery& Query) const
{
	TArray<FBlueprintHelperReviewRecord> Records;
	const FString RecordsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records");

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(RecordsDir / TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		const FString Path = RecordsDir / File;
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *Path))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			continue;
		}

		FBlueprintHelperReviewRecord Record;
		if (!FBlueprintHelperReviewStoreServiceLocalUtils::ReadReviewRecordFromJson(Json, Record))
		{
			continue;
		}

		if (!Query.ArchiveSessionIdFilter.IsEmpty() && Record.ArchiveSessionId != Query.ArchiveSessionIdFilter)
		{
			continue;
		}
		if (!Query.AssetPathFilter.IsEmpty() && Record.AssetPath != Query.AssetPathFilter)
		{
			continue;
		}
		if (!Query.TaskRunIdFilter.IsEmpty()
			&& !Record.SourceTaskRunIds.Contains(Query.TaskRunIdFilter)
			&& !Record.SourceTransactionSummary.TaskRunIds.Contains(Query.TaskRunIdFilter))
		{
			continue;
		}
		if (Query.bPendingOnly
			&& (Record.Status == EBlueprintHelperReviewChangeStatus::Accepted
				|| Record.Status == EBlueprintHelperReviewChangeStatus::Rejected))
		{
			continue;
		}

		Records.Add(Record);
	}

	return Records;
}

bool FBlueprintHelperReviewStoreService::LoadReviewRecordById(
	const FString& ReviewRecordId,
	FBlueprintHelperReviewRecord& OutRecord,
	FString& OutError) const
{
	if (ReviewRecordId.IsEmpty())
	{
		OutError = TEXT("review_record_id is required");
		return false;
	}

	const FString Path = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records")
		/ FString::Printf(TEXT("%s.json"), *ReviewRecordId);

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		OutError = FString::Printf(TEXT("review record not found: %s"), *ReviewRecordId);
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !FBlueprintHelperReviewStoreServiceLocalUtils::ReadReviewRecordFromJson(Json, OutRecord))
	{
		OutError = FString::Printf(TEXT("failed to parse review record: %s"), *ReviewRecordId);
		return false;
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::DeleteReviewRecord(
	const FString& ReviewRecordId,
	FString& OutError) const
{
	if (!FBlueprintHelperReviewStoreServiceLocalUtils::IsSafeReviewRecordId(ReviewRecordId))
	{
		OutError = TEXT("invalid review_record_id");
		return false;
	}

	const FString Path = FBlueprintHelperReviewStoreServiceLocalUtils::GetRecordPath(ReviewRecordId);
	if (!IFileManager::Get().FileExists(*Path))
	{
		OutError.Reset();
		return true;
	}

	if (!IFileManager::Get().Delete(*Path, false, true))
	{
		OutError = FString::Printf(TEXT("failed to delete review record: %s"), *ReviewRecordId);
		return false;
	}

	OutError.Reset();
	return true;
}

bool FBlueprintHelperReviewStoreService::PurgeReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys,
	TArray<FString>& OutDebugCaseIdsToDelete,
	bool& bOutRecordDeleted,
	FString& OutError) const
{
	OutDebugCaseIdsToDelete.Reset();
	bOutRecordDeleted = false;

	FBlueprintHelperReviewRecord Record;
	if (!LoadReviewRecordById(ReviewRecordId, Record, OutError))
	{
		return false;
	}

	TSet<FString> TargetKeySet;
	for (const FString& TargetKey : TargetKeys)
	{
		if (!TargetKey.IsEmpty())
		{
			TargetKeySet.Add(TargetKey);
		}
	}

	bool bMatchedAny = false;
	for (int32 ChangeIndex = Record.VisibleChanges.Num() - 1; ChangeIndex >= 0; --ChangeIndex)
	{
		FBlueprintHelperReviewVisibleChange& Change = Record.VisibleChanges[ChangeIndex];
		const int32 RemovedTargetCount = Change.AtomicTargets.RemoveAll(
			[&TargetKeySet](const FBlueprintHelperReviewAtomicTarget& Target)
			{
				return FBlueprintHelperReviewStoreServiceLocalUtils::ReviewTargetMatches(Target, TargetKeySet);
			});
		if (RemovedTargetCount > 0)
		{
			bMatchedAny = true;
		}
		if (Change.AtomicTargets.Num() == 0)
		{
			Record.VisibleChanges.RemoveAt(ChangeIndex);
		}
	}

	if (!bMatchedAny)
	{
		OutError = TEXT("target_keys_not_found");
		return false;
	}

	if (Record.VisibleChanges.Num() == 0)
	{
		OutDebugCaseIdsToDelete = Record.DebugCaseIds;
		if (!DeleteReviewRecord(ReviewRecordId, OutError))
		{
			return false;
		}
		bOutRecordDeleted = true;
		return true;
	}

	FBlueprintHelperReviewStoreServiceLocalUtils::RefreshReviewRecordStatus(Record);
	return SaveReviewRecord(Record, OutError);
}

TSharedRef<FJsonObject> FBlueprintHelperReviewStoreService::BuildReviewRecordSummaryArtifact(
	const FBlueprintHelperReviewRecord& Record) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewSummaryArtifact.v1"));
	Json->SetStringField(TEXT("review_record_id"), Record.ReviewRecordId);
	Json->SetStringField(TEXT("archive_session_id"), Record.ArchiveSessionId);
	Json->SetStringField(TEXT("asset_path"), Record.AssetPath);
	Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Record.Status));
	Json->SetStringField(TEXT("storage_status"), BlueprintHelperReviewStorageStatusToString(Record.StorageStatus));
	Json->SetArrayField(TEXT("source_task_run_ids"), FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(Record.SourceTaskRunIds));
	Json->SetArrayField(TEXT("debug_case_ids"), FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(Record.DebugCaseIds));

	FBlueprintHelperReviewArchiveSession ArchiveSession;
	FString ArchiveSessionError;
	if (!Record.ArchiveSessionId.IsEmpty() && LoadArchiveSession(Record.ArchiveSessionId, ArchiveSession, ArchiveSessionError))
	{
		TSharedRef<FJsonObject> Baseline = MakeShared<FJsonObject>();
		if (!ArchiveSession.BaselineDirtyAssetPolicy.IsEmpty())
		{
			Baseline->SetStringField(TEXT("dirty_asset_policy"), ArchiveSession.BaselineDirtyAssetPolicy);
		}
		if (!ArchiveSession.BaselineSnapshotTrust.IsEmpty())
		{
			Baseline->SetStringField(TEXT("snapshot_trust"), ArchiveSession.BaselineSnapshotTrust);
		}
		Baseline->SetArrayField(TEXT("dirty_target_assets"),
			FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(ArchiveSession.DirtyTargetAssets));
		Baseline->SetArrayField(TEXT("warnings"),
			FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(ArchiveSession.BaselineWarnings));
		Baseline->SetArrayField(TEXT("disk_snapshot_refs"),
			FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(ArchiveSession.BaselineSnapshotRefs));
		Baseline->SetArrayField(TEXT("semantic_snapshot_refs"),
			FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(ArchiveSession.BaselineSemanticSnapshotRefs));
		Json->SetObjectField(TEXT("baseline"), Baseline);
	}

	TSharedRef<FJsonObject> SourceSummary = MakeShared<FJsonObject>();
	SourceSummary->SetNumberField(TEXT("transaction_count"), Record.SourceTransactionSummary.TransactionCount);
	SourceSummary->SetArrayField(TEXT("task_run_ids"), FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.TaskRunIds));
	SourceSummary->SetArrayField(TEXT("operation_kinds"), FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.OperationKinds));
	SourceSummary->SetArrayField(TEXT("asset_paths"), FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.AssetPaths));
	SourceSummary->SetArrayField(TEXT("transaction_ids"), FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.TransactionIds));
	if (!Record.SourceTransactionSummary.CreatedAtFirst.IsEmpty())
	{
		SourceSummary->SetStringField(TEXT("created_at_first"), Record.SourceTransactionSummary.CreatedAtFirst);
	}
	if (!Record.SourceTransactionSummary.CreatedAtLast.IsEmpty())
	{
		SourceSummary->SetStringField(TEXT("created_at_last"), Record.SourceTransactionSummary.CreatedAtLast);
	}
	SourceSummary->SetStringField(TEXT("final_review_status"),
		BlueprintHelperReviewChangeStatusToString(Record.SourceTransactionSummary.FinalReviewStatus));
	Json->SetObjectField(TEXT("source_transaction_summary"), SourceSummary);

	TArray<TSharedPtr<FJsonValue>> ChangeSummaries;
	for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		TSharedRef<FJsonObject> ChangeJson = MakeShared<FJsonObject>();
		ChangeJson->SetStringField(TEXT("change_id"), Change.ChangeId);
		ChangeJson->SetStringField(TEXT("asset_path"), Change.AssetPath);
		if (!Change.GraphName.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("graph_name"), Change.GraphName);
		}
		if (!Change.LocationKey.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("visual_group_key"), Change.LocationKey);
		}
		ChangeJson->SetStringField(TEXT("change_kind"), BlueprintHelperReviewChangeKindToString(Change.ChangeKind));
		ChangeJson->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Change.Status));
		if (!Change.DisplayLabel.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("display_label"), Change.DisplayLabel);
		}
		if (!Change.BeforeSummary.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("before_summary"), Change.BeforeSummary);
		}
		if (!Change.AfterSummary.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("after_summary"), Change.AfterSummary);
		}
		if (!Change.NeedsActionReason.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("needs_action_reason"), Change.NeedsActionReason);
		}
		if (!Change.ParentChangeId.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("parent_change_id"), Change.ParentChangeId);
		}
		if (Change.ExecutionOrder != INDEX_NONE)
		{
			ChangeJson->SetNumberField(TEXT("execution_order"), Change.ExecutionOrder);
		}
		if (Change.TaskStepIndex != INDEX_NONE)
		{
			ChangeJson->SetNumberField(TEXT("task_step_index"), Change.TaskStepIndex);
		}
		if (Change.AtomicIndex != INDEX_NONE)
		{
			ChangeJson->SetNumberField(TEXT("atomic_index"), Change.AtomicIndex);
		}
		if (Change.bIsAssetLifecycleRoot)
		{
			ChangeJson->SetBoolField(TEXT("is_asset_lifecycle_root"), true);
		}
		if (Change.bRejectRemovesChildren)
		{
			ChangeJson->SetBoolField(TEXT("reject_removes_children"), true);
		}
		ChangeJson->SetNumberField(TEXT("atomic_target_count"), Change.AtomicTargets.Num());
		ChangeSummaries.Add(MakeShared<FJsonValueObject>(ChangeJson));
	}
	Json->SetNumberField(TEXT("visible_change_count"), Record.VisibleChanges.Num());
	Json->SetArrayField(TEXT("visible_changes"), ChangeSummaries);
	Json->SetNumberField(TEXT("review_action_count"), Record.ReviewActions.Num());
	return Json;
}

bool FBlueprintHelperReviewStoreService::SaveReviewRecord(
	const FBlueprintHelperReviewRecord& Record,
	FString& OutError) const
{
	if (Record.ReviewRecordId.IsEmpty())
	{
		OutError = TEXT("review_record_id is required");
		return false;
	}

	const FString RecordsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records");
	if (!IFileManager::Get().DirectoryExists(*RecordsDir))
	{
		IFileManager::Get().MakeDirectory(*RecordsDir, true);
	}

	FBlueprintHelperReviewRecord RecordToWrite = Record;
	FBlueprintHelperReviewStoreServiceLocalUtils::LinkPendingChildrenToLifecycleRoots(RecordToWrite.VisibleChanges);
	FBlueprintHelperReviewStoreServiceLocalUtils::SortVisibleChangesByReviewOrder(RecordToWrite.VisibleChanges);

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(FBlueprintHelperReviewStoreServiceLocalUtils::ReviewRecordToJson(RecordToWrite), Writer))
	{
		OutError = FString::Printf(TEXT("failed to serialize review record: %s"), *Record.ReviewRecordId);
		return false;
	}

	const FString Path = RecordsDir / FString::Printf(TEXT("%s.json"), *Record.ReviewRecordId);
	if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("failed to write review record: %s"), *Path);
		return false;
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::SaveReviewRecords(
	const TArray<FBlueprintHelperReviewRecord>& Records,
	FString& OutError) const
{
	const FString RecordsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records");
	if (!IFileManager::Get().DirectoryExists(*RecordsDir))
	{
		IFileManager::Get().MakeDirectory(*RecordsDir, true);
	}

	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		if (Record.ReviewRecordId.IsEmpty())
		{
			OutError = TEXT("review_record_id is required");
			return false;
		}

		FBlueprintHelperReviewRecord RecordToWrite = Record;
		const FString Path = RecordsDir / FString::Printf(TEXT("%s.json"), *Record.ReviewRecordId);
		FString ExistingContent;
		if (FFileHelper::LoadFileToString(ExistingContent, *Path))
		{
			TSharedPtr<FJsonObject> ExistingJson;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingContent);
			FBlueprintHelperReviewRecord ExistingRecord;
			if (FJsonSerializer::Deserialize(Reader, ExistingJson)
				&& FBlueprintHelperReviewStoreServiceLocalUtils::ReadReviewRecordFromJson(ExistingJson, ExistingRecord))
			{
				FBlueprintHelperReviewStoreServiceLocalUtils::MergeReviewRecord(ExistingRecord, Record);
				RecordToWrite = ExistingRecord;
			}
		}

		FBlueprintHelperReviewStoreServiceLocalUtils::LinkPendingChildrenToLifecycleRoots(RecordToWrite.VisibleChanges);
		FBlueprintHelperReviewStoreServiceLocalUtils::SortVisibleChangesByReviewOrder(RecordToWrite.VisibleChanges);

		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		if (!FJsonSerializer::Serialize(FBlueprintHelperReviewStoreServiceLocalUtils::ReviewRecordToJson(RecordToWrite), Writer))
		{
			OutError = FString::Printf(TEXT("failed to serialize review record: %s"), *Record.ReviewRecordId);
			return false;
		}

		if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("failed to write review record: %s"), *Path);
			return false;
		}
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::SaveArchiveSession(
	const FBlueprintHelperReviewArchiveSession& ArchiveSession,
	FString& OutError) const
{
	if (ArchiveSession.ArchiveSessionId.IsEmpty())
	{
		OutError = TEXT("archive_session_id is required");
		return false;
	}

	const FString SessionsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("ArchiveSessions");
	if (!IFileManager::Get().DirectoryExists(*SessionsDir))
	{
		IFileManager::Get().MakeDirectory(*SessionsDir, true);
	}

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(FBlueprintHelperReviewStoreServiceLocalUtils::ReviewArchiveSessionToJson(ArchiveSession), Writer))
	{
		OutError = FString::Printf(TEXT("failed to serialize archive session: %s"), *ArchiveSession.ArchiveSessionId);
		return false;
	}

	const FString Path = SessionsDir / FString::Printf(TEXT("%s.json"), *ArchiveSession.ArchiveSessionId);
	if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("failed to write archive session: %s"), *Path);
		return false;
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::LoadArchiveSession(
	const FString& ArchiveSessionId,
	FBlueprintHelperReviewArchiveSession& OutArchiveSession,
	FString& OutError) const
{
	if (ArchiveSessionId.IsEmpty())
	{
		OutError = TEXT("archive_session_id is required");
		return false;
	}

	const FString Path = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("ArchiveSessions")
		/ FString::Printf(TEXT("%s.json"), *ArchiveSessionId);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("archive session not found: %s"), *ArchiveSessionId);
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Json)
		|| !FBlueprintHelperReviewStoreServiceLocalUtils::ReadReviewArchiveSessionFromJson(Json, OutArchiveSession))
	{
		OutError = FString::Printf(TEXT("failed to parse archive session: %s"), *ArchiveSessionId);
		return false;
	}

	OutError.Empty();
	return true;
}

void FBlueprintHelperReviewStoreService::AddAtomicTargetsForInput(
	const FBlueprintHelperReviewTransactionInput& Input,
	TMap<FString, FBlueprintHelperReviewVisibleChange>& AtomicChanges,
	TArray<FString>& AtomicOrder) const
{
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets = MakeAtomicTargetsForInput(Input);
	for (FBlueprintHelperReviewAtomicTarget& Target : AtomicTargets)
	{
		Target.AssetPath = Target.AssetPath.IsEmpty() ? Input.AssetPath : Target.AssetPath;
		Target.GraphName = Target.GraphName.IsEmpty() ? Input.GraphName : Target.GraphName;
		Target.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Input.DisplayLabel : Target.DisplayLabel;
		Target.VisualGroupKey = Target.VisualGroupKey.IsEmpty() ? Target.TargetKey : Target.VisualGroupKey;
		Target.LatestTransactionId = Input.TransactionId;

		const FString AtomicKey = FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*Target.AssetPath,
			BlueprintHelperReviewSurfaceToString(Target.Surface),
			*Target.GraphName,
			*Target.TargetKey);

		FBlueprintHelperReviewVisibleChange* Existing = AtomicChanges.Find(AtomicKey);
		if (!Existing)
		{
			FBlueprintHelperReviewVisibleChange AtomicChange = MakeVisibleChange(Input);
			AtomicChange.AssetPath = Target.AssetPath;
			AtomicChange.GraphName = Target.GraphName;
			AtomicChange.LocationKey = Target.VisualGroupKey;
			AtomicChange.DisplayLabel = Target.DisplayLabel.IsEmpty() ? AtomicChange.DisplayLabel : Target.DisplayLabel;
			AtomicChange.AtomicTargets.Reset();
			AtomicChange.AtomicTargets.Add(Target);
			AtomicChange.LatestTransactionIds.Reset();
			AtomicChange.LatestTransactionIds.Add(Input.TransactionId);
			FBlueprintHelperReviewStoreServiceLocalUtils::ApplyAssetLifecycleRootMetadata(AtomicChange);
			AtomicChanges.Add(AtomicKey, AtomicChange);
			AtomicOrder.Add(AtomicKey);
			continue;
		}

		FBlueprintHelperReviewVisibleChange& AtomicChange = *Existing;
		FBlueprintHelperReviewAtomicTarget& ExistingTarget = AtomicChange.AtomicTargets[0];
		ExistingTarget.LatestTransactionId = Input.TransactionId;
		ExistingTarget.SourceTransactionIds.Add(Input.TransactionId);
		ExistingTarget.GraphName = Target.GraphName;
		ExistingTarget.VisualGroupKey = Target.VisualGroupKey;
		ExistingTarget.DisplayLabel = Target.DisplayLabel;
		ExistingTarget.NodeGuid = Target.NodeGuid;
		ExistingTarget.PinPath = Target.PinPath;
		ExistingTarget.PropertyPath = Target.PropertyPath;
		ExistingTarget.ComponentPath = Target.ComponentPath;
		ExistingTarget.bHasGraphBounds = Target.bHasGraphBounds;
		ExistingTarget.GraphPosition = Target.GraphPosition;
		ExistingTarget.GraphSize = Target.GraphSize;

		AtomicChange.LatestTransactionId = Input.TransactionId;
		AtomicChange.LatestTransactionIds.Reset();
		AtomicChange.LatestTransactionIds.Add(Input.TransactionId);
		AtomicChange.SourceTransactionIds.Add(Input.TransactionId);
		AtomicChange.ChangeKind = Input.ChangeKind;
		AtomicChange.GraphName = Target.GraphName;
		AtomicChange.LocationKey = Target.VisualGroupKey;
		AtomicChange.DisplayLabel = Target.DisplayLabel.IsEmpty() ? AtomicChange.DisplayLabel : Target.DisplayLabel;
		AtomicChange.AfterSummary = Input.AfterSummary;
		AtomicChange.ChangeId = Input.TransactionId;
		FBlueprintHelperReviewStoreServiceLocalUtils::ApplyAssetLifecycleRootMetadata(AtomicChange);
	}
}

TArray<FBlueprintHelperReviewVisibleChange> FBlueprintHelperReviewStoreService::LoadPendingVisibleChanges(
	const FString& AssetPathFilter) const
{
	FBlueprintHelperReviewRecordQuery Query;
	Query.AssetPathFilter = AssetPathFilter;
	Query.bPendingOnly = true;

	TArray<FBlueprintHelperReviewVisibleChange> RecordChanges;
	const TArray<FBlueprintHelperReviewRecord> Records = QueryReviewRecords(Query);
	const bool bSkipMissingAssetRecords = AssetPathFilter.IsEmpty();
	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		if (bSkipMissingAssetRecords
			&& !FBlueprintHelperReviewStoreServiceLocalUtils::DoesReviewAssetPackageExist(Record.AssetPath))
		{
			continue;
		}

		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			if (bSkipMissingAssetRecords
				&& !Change.AssetPath.IsEmpty()
				&& Change.AssetPath != Record.AssetPath
				&& !FBlueprintHelperReviewStoreServiceLocalUtils::DoesReviewAssetPackageExist(Change.AssetPath))
			{
				continue;
			}

			if (Change.Status == EBlueprintHelperReviewChangeStatus::Pending
				|| Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
				|| Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed)
			{
				RecordChanges.Add(Change);
			}
		}
	}
	FBlueprintHelperReviewStoreServiceLocalUtils::CollapseVisibleChangesLatestWins(RecordChanges);
	FBlueprintHelperReviewStoreServiceLocalUtils::LinkPendingChildrenToLifecycleRoots(RecordChanges);
	FBlueprintHelperReviewStoreServiceLocalUtils::SortVisibleChangesByReviewOrder(RecordChanges);
	return RecordChanges;
}

FDelegateHandle FBlueprintHelperReviewStoreService::AddPendingReviewChangedHandler(const FSimpleDelegate& Handler) const
{
	return PendingReviewChangedDelegate.Add(Handler);
}

void FBlueprintHelperReviewStoreService::RemovePendingReviewChangedHandler(FDelegateHandle Handle) const
{
	if (Handle.IsValid())
	{
		PendingReviewChangedDelegate.Remove(Handle);
	}
}

void FBlueprintHelperReviewStoreService::NotifyPendingReviewChanged() const
{
	PendingReviewChangedDelegate.Broadcast();
}

FBlueprintHelperReviewVisibleChange FBlueprintHelperReviewStoreService::MakeVisibleChange(
	const FBlueprintHelperReviewTransactionInput& Input,
	const FString& ChangeIdSuffix) const
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.AssetPath = Input.AssetPath;
	Change.GraphName = Input.GraphName;
	Change.LocationKey = Input.LocationKey;
	Change.LatestTransactionId = Input.TransactionId;
	Change.LatestTransactionIds.Add(Input.TransactionId);
	Change.SourceTransactionIds.Add(Input.TransactionId);
	Change.ChangeKind = Input.ChangeKind;
	Change.DisplayLabel = Input.DisplayLabel.IsEmpty() ? Input.LocationKey : Input.DisplayLabel;
	Change.BeforeSummary = Input.BeforeSummary;
	Change.AfterSummary = Input.AfterSummary;
	Change.ChangeId = Input.TransactionId;
	if (!ChangeIdSuffix.IsEmpty())
	{
		Change.ChangeId += TEXT("_") + ChangeIdSuffix;
	}
	return Change;
}

void FBlueprintHelperReviewStoreService::GroupAtomicVisibleChange(
	const FBlueprintHelperReviewVisibleChange& AtomicChange,
	TMap<FString, int32>& GroupToIndex,
	TArray<FBlueprintHelperReviewVisibleChange>& OutChanges) const
{
	if (AtomicChange.AtomicTargets.Num() == 0)
	{
		return;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = AtomicChange.AtomicTargets[0];
	const FString GroupKey = FString::Printf(
		TEXT("%s|%s"),
		*AtomicChange.AssetPath,
		*Target.VisualGroupKey);

	if (const int32* ExistingIndex = GroupToIndex.Find(GroupKey))
	{
		FBlueprintHelperReviewVisibleChange& Existing = OutChanges[*ExistingIndex];
		Existing.AtomicTargets.Add(Target);
		for (const FString& SourceTransactionId : AtomicChange.SourceTransactionIds)
		{
			Existing.SourceTransactionIds.AddUnique(SourceTransactionId);
		}
		for (const FString& LatestTransactionId : AtomicChange.LatestTransactionIds)
		{
			Existing.LatestTransactionIds.AddUnique(LatestTransactionId);
		}
		Existing.LatestTransactionId = AtomicChange.LatestTransactionId;
		Existing.ChangeKind = AtomicChange.ChangeKind;
		Existing.GraphName = AtomicChange.GraphName.IsEmpty() ? Existing.GraphName : AtomicChange.GraphName;
		Existing.DisplayLabel = AtomicChange.DisplayLabel.IsEmpty() ? Existing.DisplayLabel : AtomicChange.DisplayLabel;
		Existing.AfterSummary = AtomicChange.AfterSummary;
		Existing.ChangeId = AtomicChange.ChangeId;
		Existing.ExecutionOrder = AtomicChange.ExecutionOrder;
		Existing.TaskStepIndex = AtomicChange.TaskStepIndex;
		Existing.AtomicIndex = AtomicChange.AtomicIndex;
		FBlueprintHelperReviewStoreServiceLocalUtils::ApplyAssetLifecycleRootMetadata(Existing);
		return;
	}

	const int32 NewIndex = OutChanges.Add(AtomicChange);
	GroupToIndex.Add(GroupKey, NewIndex);
}

void FBlueprintHelperReviewStoreService::AddEvidenceAtomicTargets(
	const FBlueprintHelperWriteReviewEvidence& Evidence,
	FBlueprintHelperReviewRecord& Record) const
{
	if (Evidence.AtomicTargets.Num() == 0)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = Evidence.TransactionId;
		Change.AssetPath = Evidence.AssetPath;
		Change.LocationKey = Evidence.OperationKind;
		Change.LatestTransactionId = Evidence.TransactionId;
		Change.SourceTransactionIds.Add(Evidence.TransactionId);
		Change.ChangeKind = Evidence.ChangeKind;
		Change.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Change.DisplayLabel = Evidence.DisplayLabel.IsEmpty() ? Evidence.OperationKind : Evidence.DisplayLabel;
		Change.NeedsActionReason = TEXT("missing_atomic_targets");
		Record.VisibleChanges.Add(Change);
		return;
	}

	for (int32 Index = 0; Index < Evidence.AtomicTargets.Num(); ++Index)
	{
		FBlueprintHelperReviewAtomicTarget Target = Evidence.AtomicTargets[Index];
		Target.AssetPath = Target.AssetPath.IsEmpty() ? Evidence.AssetPath : Target.AssetPath;
		Target.TaskStepIndex = Evidence.TaskStepIndex;
		Target.AtomicIndex = Index;
		if (Target.ExecutionOrder == INDEX_NONE && Target.TaskStepIndex != INDEX_NONE)
		{
			Target.ExecutionOrder = Target.TaskStepIndex * 1000 + Index;
		}
		Record.SourceTransactionSummary.AssetPaths.AddUnique(Target.AssetPath);
		Target.LatestTransactionId = Evidence.TransactionId;
		Target.SourceTransactionIds.AddUnique(Evidence.TransactionId);
		if (Target.Ownership.IsEmpty())
		{
			Target.Ownership = TEXT("unknown");
		}
		Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
			Target.Surface,
			Target.TargetKind,
			Target.TargetKey,
			Target.VisualGroupKey,
			Evidence.OperationKind);
		FBlueprintHelperReviewStoreServiceLocalUtils::ApplyGraphBodyAggregation(Target);

		FString NeedsActionReason;
		if (!FBlueprintHelperReviewStoreServiceLocalUtils::IsReviewEvidenceTargetComplete(Target, NeedsActionReason))
		{
			Target.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
		}

		const FString VisualGroupKey = Target.VisualGroupKey.IsEmpty()
			? FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewInternalMissingGroupKey(Evidence.TransactionId, Index)
			: Target.VisualGroupKey;
		const FString ChangeAssetPath = Target.AssetPath.IsEmpty() ? Evidence.AssetPath : Target.AssetPath;
		const FString AtomicLookupKey = FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewAtomicLookupKey(
			Target,
			FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewInternalMissingAnchorKey(Evidence.TransactionId, Index));

		FBlueprintHelperReviewVisibleChange* Change = Record.VisibleChanges.FindByPredicate(
			[&VisualGroupKey, &ChangeAssetPath](const FBlueprintHelperReviewVisibleChange& Candidate)
			{
				return Candidate.LocationKey == VisualGroupKey
					&& Candidate.AssetPath == ChangeAssetPath;
			});
		if (!Change)
		{
			FBlueprintHelperReviewVisibleChange NewChange = FBlueprintHelperReviewStoreServiceLocalUtils::MakeVisibleChangeFromEvidence(
				Evidence,
				Target,
				VisualGroupKey);
			NewChange.AtomicTargets.Add(Target);
			FBlueprintHelperReviewStoreServiceLocalUtils::ApplyAssetLifecycleRootMetadata(NewChange);
			if (!NeedsActionReason.IsEmpty())
			{
				NewChange.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
				NewChange.NeedsActionReason = NeedsActionReason;
			}
			Record.VisibleChanges.Add(NewChange);
			continue;
		}

		FBlueprintHelperReviewAtomicTarget* ExistingTarget = Change->AtomicTargets.FindByPredicate(
			[&AtomicLookupKey](const FBlueprintHelperReviewAtomicTarget& Candidate)
			{
				return FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewAtomicLookupKey(Candidate, FString()) == AtomicLookupKey;
			});
		if (!ExistingTarget)
		{
			Change->AtomicTargets.Add(Target);
		}
		else
		{
			TArray<FString> SourceTransactionIds = ExistingTarget->SourceTransactionIds;
			for (const FString& SourceTransactionId : Target.SourceTransactionIds)
			{
				SourceTransactionIds.AddUnique(SourceTransactionId);
			}

			*ExistingTarget = Target;
			ExistingTarget->SourceTransactionIds = SourceTransactionIds;
		}

		Change->LatestTransactionId = Evidence.TransactionId;
		Change->LatestTransactionIds.Reset();
		Change->LatestTransactionIds.Add(Evidence.TransactionId);
		Change->SourceTransactionIds.AddUnique(Evidence.TransactionId);
		Change->ChangeId = FBlueprintHelperReviewStoreServiceLocalUtils::MakeReviewVisibleChangeId(Evidence.TransactionId, VisualGroupKey);
		Change->ChangeKind = Evidence.ChangeKind;
		Change->AfterSummary = Evidence.AfterSummary;
		Change->ExecutionOrder = Target.ExecutionOrder;
		Change->TaskStepIndex = Target.TaskStepIndex;
		Change->AtomicIndex = Target.AtomicIndex;
		FBlueprintHelperReviewStoreServiceLocalUtils::ApplyAssetLifecycleRootMetadata(*Change);
		if (!NeedsActionReason.IsEmpty())
		{
			Change->Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Change->NeedsActionReason = NeedsActionReason;
		}
	}
}

TArray<FBlueprintHelperReviewAtomicTarget> FBlueprintHelperReviewStoreService::MakeAtomicTargetsForInput(
	const FBlueprintHelperReviewTransactionInput& Input) const
{
	if (Input.AtomicTargets.Num() > 0)
	{
		TArray<FBlueprintHelperReviewAtomicTarget> Targets = Input.AtomicTargets;
		for (FBlueprintHelperReviewAtomicTarget& Target : Targets)
		{
			Target.AssetPath = Target.AssetPath.IsEmpty() ? Input.AssetPath : Target.AssetPath;
			Target.GraphName = Target.GraphName.IsEmpty() ? Input.GraphName : Target.GraphName;
			Target.TargetKey = Target.TargetKey.IsEmpty() ? Input.LocationKey : Target.TargetKey;
			Target.VisualGroupKey = Target.VisualGroupKey.IsEmpty() ? Input.LocationKey : Target.VisualGroupKey;
			Target.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Input.DisplayLabel : Target.DisplayLabel;
			Target.SourceTransactionIds.Add(Input.TransactionId);
			Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
				Target.Surface,
				Target.TargetKind,
				Target.TargetKey,
				Target.VisualGroupKey,
				Input.LocationKey);
			FBlueprintHelperReviewStoreServiceLocalUtils::ApplyGraphBodyAggregation(Target);
		}
		return Targets;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Input.AssetPath;
	Target.GraphName = Input.GraphName;
	Target.TargetKey = Input.LocationKey.IsEmpty() ? Input.DisplayLabel : Input.LocationKey;
	Target.VisualGroupKey = Target.TargetKey;
	Target.DisplayLabel = Input.DisplayLabel;
	Target.SourceTransactionIds.Add(Input.TransactionId);

	FBlueprintHelperReviewVisibleChange TempChange;
	TempChange.LocationKey = Input.LocationKey;
	TempChange.GraphName = Input.GraphName;
	TempChange.DisplayLabel = Input.DisplayLabel;
	TempChange.ChangeKind = Input.ChangeKind;
	if (BlueprintHelperReviewShouldShowInComponents(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Components;
	}
	else if (BlueprintHelperReviewShouldShowInDetails(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Details;
	}
	else if (BlueprintHelperReviewShouldShowInGraph(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
	}
	else if (BlueprintHelperReviewShouldShowInMyBlueprint(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	}
	else
	{
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
	}
	Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
		Target.Surface,
		Target.TargetKind,
		Target.TargetKey,
		Target.VisualGroupKey,
		Input.LocationKey);
	FBlueprintHelperReviewStoreServiceLocalUtils::ApplyGraphBodyAggregation(Target);

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);
	return Targets;
}
