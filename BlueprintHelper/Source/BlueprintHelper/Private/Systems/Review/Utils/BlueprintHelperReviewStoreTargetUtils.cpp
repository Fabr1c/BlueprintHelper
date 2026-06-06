// BlueprintHelper Review BlueprintHelperReviewStoreTargetUtils implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

FString FBlueprintHelperReviewStoreTargetUtils::ExtractReviewNodeIdentifier(const FString& RawNodePath)
	{
		FString Identifier = RawNodePath;

		int32 DotIndex = INDEX_NONE;
		if (Identifier.FindLastChar(TEXT('.'), DotIndex))
		{
			Identifier = Identifier.Mid(DotIndex + 1);
		}

		return Identifier;
	}
FBlueprintHelperReviewAtomicTarget FBlueprintHelperReviewStoreTargetUtils::MakeGraphRecordTarget(
		const FBlueprintHelperReviewEvidenceInput& Input,
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
		Target.SourceEvidenceIds.Add(Input.EvidenceId);
		return Target;
	}
void FBlueprintHelperReviewStoreTargetUtils::AddGraphTargetsFromStringArrayField(
		const TSharedPtr<FJsonObject>& Record,
		const TCHAR* FieldName,
		const FString& TargetPrefix,
		bool bExtractNodeName,
		FBlueprintHelperReviewEvidenceInput& Input)
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
FString FBlueprintHelperReviewStoreTargetUtils::MakeReviewInternalMissingAnchorKey(const FString& EvidenceId, int32 Index)
	{
		return FString::Printf(TEXT("__missing_anchor|%s|%d"), *EvidenceId, Index);
	}
FString FBlueprintHelperReviewStoreTargetUtils::MakeReviewInternalMissingGroupKey(const FString& EvidenceId, int32 Index)
	{
		return FString::Printf(TEXT("__missing_visual_group|%s|%d"), *EvidenceId, Index);
	}
FString FBlueprintHelperReviewStoreTargetUtils::MakeReviewAtomicLookupKey(const FBlueprintHelperReviewAtomicTarget& Target, const FString& FallbackKey)
	{
		return FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*Target.AssetPath,
			BlueprintHelperReviewSurfaceToString(Target.Surface),
			*Target.GraphName,
			Target.TargetKey.IsEmpty() ? *FallbackKey : *Target.TargetKey);
	}
FString FBlueprintHelperReviewStoreTargetUtils::MakeReviewScopeIdentity(const FBlueprintHelperReviewAtomicTarget& Target, const FString& FallbackKey)
	{
		if (!Target.ScopeIdentity.IsEmpty())
		{
			return Target.ScopeIdentity;
		}

		return MakeReviewAtomicLookupKey(Target, FallbackKey);
	}
bool FBlueprintHelperReviewStoreTargetUtils::IsReviewTargetNetNoChange(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return !Target.BaselineHash.IsEmpty()
			&& !Target.RecordedAfterHash.IsEmpty()
			&& Target.BaselineHash == Target.RecordedAfterHash;
	}
void FBlueprintHelperReviewStoreTargetUtils::PreserveFirstBaselineFields(
		FBlueprintHelperReviewAtomicTarget& Target,
		const FBlueprintHelperReviewAtomicTarget& Existing,
		const FBlueprintHelperReviewAtomicTarget& Incoming)
	{
		Target.ScopeIdentity = !Existing.ScopeIdentity.IsEmpty() ? Existing.ScopeIdentity : Incoming.ScopeIdentity;
		Target.FirstEvidenceId = !Existing.FirstEvidenceId.IsEmpty()
			? Existing.FirstEvidenceId
			: (!Existing.LatestEvidenceId.IsEmpty() ? Existing.LatestEvidenceId : Incoming.LatestEvidenceId);
		Target.BaselineHash = !Existing.BaselineHash.IsEmpty() ? Existing.BaselineHash : Incoming.BaselineHash;
		Target.BeforeSnapshotJson = !Existing.BeforeSnapshotJson.IsEmpty() ? Existing.BeforeSnapshotJson : Incoming.BeforeSnapshotJson;
		Target.ComponentId = !Incoming.ComponentId.IsEmpty() ? Incoming.ComponentId : Existing.ComponentId;
		Target.ComponentTemplatePath = !Incoming.ComponentTemplatePath.IsEmpty() ? Incoming.ComponentTemplatePath : Existing.ComponentTemplatePath;
		Target.ComponentOrigin = !Incoming.ComponentOrigin.IsEmpty() ? Incoming.ComponentOrigin : Existing.ComponentOrigin;
		Target.BeforeParent = !Incoming.BeforeParent.IsEmpty() ? Incoming.BeforeParent : Existing.BeforeParent;
		Target.AfterParent = !Incoming.AfterParent.IsEmpty() ? Incoming.AfterParent : Existing.AfterParent;
		Target.BeforeRoot = !Incoming.BeforeRoot.IsEmpty() ? Incoming.BeforeRoot : Existing.BeforeRoot;
		Target.AfterRoot = !Incoming.AfterRoot.IsEmpty() ? Incoming.AfterRoot : Existing.AfterRoot;
		Target.DeletePolicy = !Incoming.DeletePolicy.IsEmpty() ? Incoming.DeletePolicy : Existing.DeletePolicy;
		Target.DeletedComponentIdsJson = !Incoming.DeletedComponentIdsJson.IsEmpty() ? Incoming.DeletedComponentIdsJson : Existing.DeletedComponentIdsJson;
		Target.MovedComponentIdsJson = !Incoming.MovedComponentIdsJson.IsEmpty() ? Incoming.MovedComponentIdsJson : Existing.MovedComponentIdsJson;
		Target.ChangedPropertiesJson = !Incoming.ChangedPropertiesJson.IsEmpty() ? Incoming.ChangedPropertiesJson : Existing.ChangedPropertiesJson;
		Target.ReadbackFingerprintBefore = !Incoming.ReadbackFingerprintBefore.IsEmpty() ? Incoming.ReadbackFingerprintBefore : Existing.ReadbackFingerprintBefore;
		Target.ReadbackFingerprintAfter = !Incoming.ReadbackFingerprintAfter.IsEmpty() ? Incoming.ReadbackFingerprintAfter : Existing.ReadbackFingerprintAfter;
		Target.LifecycleObjectKey = !Incoming.LifecycleObjectKey.IsEmpty()
			? Incoming.LifecycleObjectKey
			: Existing.LifecycleObjectKey;
		Target.LifecycleParentKey = !Incoming.LifecycleParentKey.IsEmpty()
			? Incoming.LifecycleParentKey
			: Existing.LifecycleParentKey;
		Target.GraphBodyBoundaryJson = !Incoming.GraphBodyBoundaryJson.IsEmpty()
			? Incoming.GraphBodyBoundaryJson
			: Existing.GraphBodyBoundaryJson;
	}
void FBlueprintHelperReviewStoreTargetUtils::PreserveFirstBaselineFields(
		FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewVisibleChange& Existing,
		const FBlueprintHelperReviewVisibleChange& Incoming)
	{
		Change.ScopeIdentity = !Existing.ScopeIdentity.IsEmpty() ? Existing.ScopeIdentity : Incoming.ScopeIdentity;
		Change.BeforeSummary = !Existing.BeforeSummary.IsEmpty() ? Existing.BeforeSummary : Incoming.BeforeSummary;
		Change.BeforeHash = !Existing.BeforeHash.IsEmpty() ? Existing.BeforeHash : Incoming.BeforeHash;
		Change.BeforeSnapshotJson = !Existing.BeforeSnapshotJson.IsEmpty() ? Existing.BeforeSnapshotJson : Incoming.BeforeSnapshotJson;
	}
FString FBlueprintHelperReviewStoreTargetUtils::SanitizeReviewIdSegment(const FString& Value)
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
FString FBlueprintHelperReviewStoreTargetUtils::MakeReviewVisibleChangeId(const FString& EvidenceId, const FString& VisualGroupKey)
	{
		const FString SanitizedGroup = SanitizeReviewIdSegment(VisualGroupKey);
		if (EvidenceId.IsEmpty())
		{
			return SanitizedGroup.IsEmpty() ? TEXT("review_change") : SanitizedGroup;
		}
		return SanitizedGroup.IsEmpty()
			? EvidenceId
			: FString::Printf(TEXT("%s_%s"), *EvidenceId, *SanitizedGroup);
	}
bool FBlueprintHelperReviewStoreTargetUtils::ShouldAggregateGraphBodyTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return FBlueprintHelperReviewTargetKindRegistry::ShouldAggregateAsGraphBody(Target);
	}
void FBlueprintHelperReviewStoreTargetUtils::ApplyGraphBodyAggregation(FBlueprintHelperReviewAtomicTarget& Target)
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
FString FBlueprintHelperReviewStoreTargetUtils::MakeReviewPackageNameFromAssetPath(const FString& AssetPath)
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
FString FBlueprintHelperReviewStoreTargetUtils::MakeReviewAssetLinkKey(const FString& AssetPath)
	{
		const FString PackageName = MakeReviewPackageNameFromAssetPath(AssetPath);
		return PackageName.IsEmpty() ? AssetPath : PackageName;
	}
bool FBlueprintHelperReviewStoreTargetUtils::DoesReviewAssetPackageExist(const FString& AssetPath)
	{
		const FString PackageName = MakeReviewPackageNameFromAssetPath(AssetPath);
		return FPackageName::IsValidLongPackageName(PackageName)
			&& FPackageName::DoesPackageExist(PackageName);
	}
bool FBlueprintHelperReviewStoreTargetUtils::IsReviewEvidenceTargetComplete(const FBlueprintHelperReviewAtomicTarget& Target, FString& OutReason)
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
		const bool bSupportsSnapshotRestore =
			FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind);
		if (bSupportsSnapshotRestore && Target.BeforeSnapshotJson.IsEmpty())
		{
			OutReason = TEXT("missing_recoverable_snapshot");
			return false;
		}
		if (!bSupportsSnapshotRestore)
		{
			OutReason = TEXT("unsupported_snapshot_restore_target");
			return false;
		}
		return true;
	}
bool FBlueprintHelperReviewStoreTargetUtils::IsAssetLifecycleRootTarget(
		const FBlueprintHelperReviewAtomicTarget& Target,
		EBlueprintHelperReviewChangeKind ChangeKind)
	{
		if (ChangeKind != EBlueprintHelperReviewChangeKind::Added)
		{
			return false;
		}

		const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
			FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
		return HandlerKind == EBlueprintHelperReviewTargetHandlerKind::AssetFactory
			|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component
			|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget;
	}

static void BlueprintHelperReviewEnsureAtomicTargetLifecycleMetadata(FBlueprintHelperReviewAtomicTarget& Target);

void FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(FBlueprintHelperReviewVisibleChange& Change)
	{
		EnsureLifecycleMetadata(Change);
		const bool bHasLifecycleRootTarget = Change.AtomicTargets.ContainsByPredicate(
			[&Change](const FBlueprintHelperReviewAtomicTarget& Target)
			{
				return IsAssetLifecycleRootTarget(Target, Change.ChangeKind);
			});
		if (!bHasLifecycleRootTarget)
		{
			return;
		}

		Change.bIsAssetLifecycleRoot = true;
		Change.bRejectRemovesChildren = true;
		Change.ParentChangeId.Reset();
	}
bool FBlueprintHelperReviewStoreTargetUtils::IsPendingLifecycleLinkCandidate(const FBlueprintHelperReviewVisibleChange& Change)
	{
		return Change.Status == EBlueprintHelperReviewChangeStatus::Pending
			|| Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
	}

static FString BlueprintHelperReviewExtractLifecycleName(FString Value)
	{
		Value.TrimStartAndEndInline();
		if (Value.IsEmpty())
		{
			return FString();
		}

		int32 DelimiterIndex = INDEX_NONE;
		if (Value.FindChar(TEXT(':'), DelimiterIndex))
		{
			Value = Value.Mid(DelimiterIndex + 1);
		}
		if (Value.FindChar(TEXT('.'), DelimiterIndex))
		{
			Value = Value.Left(DelimiterIndex);
		}
		Value.TrimStartAndEndInline();
		Value.ToLowerInline();
		return Value;
	}

static FString BlueprintHelperReviewMakeCompactLifecycleKey(const TCHAR* Kind, const FString& Name)
	{
		FString Normalized = BlueprintHelperReviewExtractLifecycleName(Name);
		if (!Kind || Normalized.IsEmpty())
		{
			return FString();
		}

		return FString::Printf(TEXT("%s:%s"), Kind, *Normalized);
	}

static FString BlueprintHelperReviewLifecycleObjectNameFromTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
			FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
		{
			if (!Target.ComponentPath.IsEmpty())
			{
				return BlueprintHelperReviewExtractLifecycleName(Target.ComponentPath);
			}
			if (!Target.TargetKey.IsEmpty())
			{
				return BlueprintHelperReviewExtractLifecycleName(Target.TargetKey);
			}
			return BlueprintHelperReviewExtractLifecycleName(Target.DisplayLabel);
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget
			|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
		{
			if (!Target.PropertyPath.IsEmpty())
			{
				return BlueprintHelperReviewExtractLifecycleName(Target.PropertyPath);
			}
			if (!Target.TargetKey.IsEmpty())
			{
				return BlueprintHelperReviewExtractLifecycleName(Target.TargetKey);
			}
			return BlueprintHelperReviewExtractLifecycleName(Target.DisplayLabel);
		}

		return FString();
	}

static FString BlueprintHelperReviewLifecycleKey(
	const FString& AssetPath,
	const TCHAR* ObjectKind,
	const FString& ObjectName)
	{
		if (AssetPath.IsEmpty() || !ObjectKind || ObjectName.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(
			TEXT("%s|%s|%s"),
			*FBlueprintHelperReviewStoreTargetUtils::MakeReviewAssetLinkKey(AssetPath),
			ObjectKind,
			*ObjectName);
	}

static bool BlueprintHelperReviewTryGetSnapshotString(
	const FString& SnapshotJson,
	const TCHAR* FieldName,
	FString& OutValue)
	{
		OutValue.Reset();
		if (SnapshotJson.IsEmpty() || !FieldName)
		{
			return false;
		}

		TSharedPtr<FJsonObject> Snapshot;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SnapshotJson);
		if (!FJsonSerializer::Deserialize(Reader, Snapshot) || !Snapshot.IsValid())
		{
			return false;
		}
		return Snapshot->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
	}

static bool BlueprintHelperReviewTryGetTargetSnapshotString(
	const FBlueprintHelperReviewAtomicTarget& Target,
	const TCHAR* FieldName,
	FString& OutValue)
	{
		return BlueprintHelperReviewTryGetSnapshotString(Target.AfterSnapshotJson, FieldName, OutValue)
			|| BlueprintHelperReviewTryGetSnapshotString(Target.BeforeSnapshotJson, FieldName, OutValue);
	}

static bool BlueprintHelperReviewTryGetTargetAnchorString(
	const FBlueprintHelperReviewAtomicTarget& Target,
	const TCHAR* FieldName,
	FString& OutValue)
	{
		return BlueprintHelperReviewTryGetSnapshotString(Target.AnchorJson, FieldName, OutValue);
	}

static void BlueprintHelperReviewEnsureAtomicTargetLifecycleMetadata(FBlueprintHelperReviewAtomicTarget& Target)
	{
		const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
			FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::AssetFactory)
		{
			if (Target.LifecycleObjectKey.IsEmpty())
			{
				Target.LifecycleObjectKey = TEXT("asset:asset");
			}
			return;
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
		{
			if (Target.LifecycleObjectKey.IsEmpty())
			{
				Target.LifecycleObjectKey = BlueprintHelperReviewMakeCompactLifecycleKey(
					TEXT("component"),
					BlueprintHelperReviewLifecycleObjectNameFromTarget(Target));
			}

			if (Target.LifecycleParentKey.IsEmpty())
			{
				FString ParentComponentName;
				if (BlueprintHelperReviewTryGetTargetSnapshotString(Target, TEXT("parent_component"), ParentComponentName)
					|| BlueprintHelperReviewTryGetTargetAnchorString(Target, TEXT("parent_component"), ParentComponentName))
				{
					Target.LifecycleParentKey = BlueprintHelperReviewMakeCompactLifecycleKey(
						TEXT("component"),
						ParentComponentName);
				}
			}
			return;
		}

		if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget
			|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
		{
			if (Target.LifecycleObjectKey.IsEmpty())
			{
				Target.LifecycleObjectKey = BlueprintHelperReviewMakeCompactLifecycleKey(
					TEXT("widget"),
					BlueprintHelperReviewLifecycleObjectNameFromTarget(Target));
			}

			if (Target.LifecycleParentKey.IsEmpty())
			{
				FString ParentWidgetName;
				if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
				{
					ParentWidgetName = BlueprintHelperReviewLifecycleObjectNameFromTarget(Target);
				}
				else if (!BlueprintHelperReviewTryGetTargetSnapshotString(Target, TEXT("parent_widget"), ParentWidgetName))
				{
					BlueprintHelperReviewTryGetTargetAnchorString(Target, TEXT("parent_widget"), ParentWidgetName);
				}

				if (!ParentWidgetName.IsEmpty())
				{
					Target.LifecycleParentKey = BlueprintHelperReviewMakeCompactLifecycleKey(
						TEXT("widget"),
						ParentWidgetName);
				}
			}
		}
	}

void FBlueprintHelperReviewStoreTargetUtils::EnsureLifecycleMetadata(FBlueprintHelperReviewAtomicTarget& Target)
	{
		BlueprintHelperReviewEnsureAtomicTargetLifecycleMetadata(Target);
	}

void FBlueprintHelperReviewStoreTargetUtils::EnsureLifecycleMetadata(FBlueprintHelperReviewVisibleChange& Change)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			BlueprintHelperReviewEnsureAtomicTargetLifecycleMetadata(Target);
		}
	}

static bool BlueprintHelperReviewChangeHasAssetFactoryRootKey(const FBlueprintHelperReviewVisibleChange& Change)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind)
				== EBlueprintHelperReviewTargetHandlerKind::AssetFactory)
			{
				return true;
			}
		}
		return false;
	}

static void BlueprintHelperReviewCollectLifecycleRootKeys(
	const FBlueprintHelperReviewVisibleChange& Change,
	TArray<FString>& OutKeys)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
				FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::AssetFactory)
			{
				OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
					Change.AssetPath,
					TEXT("asset"),
					TEXT("asset")));
				continue;
			}
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
			{
				if (!Target.LifecycleObjectKey.IsEmpty())
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("object"),
						Target.LifecycleObjectKey));
					continue;
				}
				OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
					Change.AssetPath,
					TEXT("component"),
					BlueprintHelperReviewLifecycleObjectNameFromTarget(Target)));
				continue;
			}
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget)
			{
				if (!Target.LifecycleObjectKey.IsEmpty())
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("object"),
						Target.LifecycleObjectKey));
					continue;
				}
				OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
					Change.AssetPath,
					TEXT("widget"),
					BlueprintHelperReviewLifecycleObjectNameFromTarget(Target)));
			}
		}
	}

static void BlueprintHelperReviewCollectLifecycleParentKeys(
	const FBlueprintHelperReviewVisibleChange& Change,
	TArray<FString>& OutKeys)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
				FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
			{
				if (!Target.LifecycleParentKey.IsEmpty())
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("object"),
						Target.LifecycleParentKey));
					continue;
				}
				FString ParentComponentName;
				if (BlueprintHelperReviewTryGetTargetSnapshotString(Target, TEXT("parent_component"), ParentComponentName))
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("component"),
						BlueprintHelperReviewExtractLifecycleName(ParentComponentName)));
				}
				continue;
			}
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget)
			{
				if (!Target.LifecycleParentKey.IsEmpty())
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("object"),
						Target.LifecycleParentKey));
					continue;
				}
				FString ParentWidgetName;
				if (BlueprintHelperReviewTryGetTargetSnapshotString(Target, TEXT("parent_widget"), ParentWidgetName))
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("widget"),
						BlueprintHelperReviewExtractLifecycleName(ParentWidgetName)));
				}
				continue;
			}
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
			{
				if (!Target.LifecycleParentKey.IsEmpty())
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("object"),
						Target.LifecycleParentKey));
					continue;
				}
				OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
					Change.AssetPath,
					TEXT("widget"),
					BlueprintHelperReviewLifecycleObjectNameFromTarget(Target)));
			}
		}
	}

static void BlueprintHelperReviewCollectLifecycleChildKeys(
	const FBlueprintHelperReviewVisibleChange& Change,
	TArray<FString>& OutKeys)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
				FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
			{
				if (!Target.LifecycleObjectKey.IsEmpty())
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("object"),
						Target.LifecycleObjectKey));
					continue;
				}
				OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
					Change.AssetPath,
					TEXT("component"),
					BlueprintHelperReviewLifecycleObjectNameFromTarget(Target)));
				continue;
			}
			if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidget
				|| HandlerKind == EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
			{
				if (!Target.LifecycleObjectKey.IsEmpty())
				{
					OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
						Change.AssetPath,
						TEXT("object"),
						Target.LifecycleObjectKey));
					continue;
				}
				OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(
					Change.AssetPath,
					TEXT("widget"),
					BlueprintHelperReviewLifecycleObjectNameFromTarget(Target)));
			}
		}
	}

void FBlueprintHelperReviewStoreTargetUtils::LinkPendingChildrenToLifecycleRoots(TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		for (FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			EnsureLifecycleMetadata(Change);
		}

		TMap<FString, int32> PendingAssetRootIndexByAssetPath;
		TMap<FString, int32> PendingRootIndexByLifecycleKey;
		for (int32 Index = 0; Index < Changes.Num(); ++Index)
		{
			const FBlueprintHelperReviewVisibleChange& Change = Changes[Index];
			if (!IsPendingLifecycleLinkCandidate(Change)
				|| !Change.bIsAssetLifecycleRoot
				|| Change.AssetPath.IsEmpty())
			{
				continue;
			}

			TArray<FString> RootKeys;
			BlueprintHelperReviewCollectLifecycleRootKeys(Change, RootKeys);
			for (const FString& RootKey : RootKeys)
			{
				if (!RootKey.IsEmpty())
				{
					PendingRootIndexByLifecycleKey.Add(RootKey, Index);
				}
			}

			const FString AssetRootKey = BlueprintHelperReviewLifecycleKey(
				Change.AssetPath,
				TEXT("asset"),
				TEXT("asset"));
			if (RootKeys.Contains(AssetRootKey))
			{
				PendingAssetRootIndexByAssetPath.Add(MakeReviewAssetLinkKey(Change.AssetPath), Index);
			}
		}

		for (FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			if (!IsPendingLifecycleLinkCandidate(Change) || Change.AssetPath.IsEmpty())
			{
				continue;
			}

			Change.ParentChangeId.Reset();
			if (Change.bIsAssetLifecycleRoot
				&& BlueprintHelperReviewChangeHasAssetFactoryRootKey(Change))
			{
				continue;
			}

			const int32* RootIndex = nullptr;
			TArray<FString> ChildKeys;
			BlueprintHelperReviewCollectLifecycleParentKeys(Change, ChildKeys);
			if (ChildKeys.Num() == 0 && !Change.bIsAssetLifecycleRoot)
			{
				BlueprintHelperReviewCollectLifecycleChildKeys(Change, ChildKeys);
			}
			for (const FString& ChildKey : ChildKeys)
			{
				RootIndex = PendingRootIndexByLifecycleKey.Find(ChildKey);
				if (RootIndex && Changes.IsValidIndex(*RootIndex) && Changes[*RootIndex].ChangeId != Change.ChangeId)
				{
					break;
				}
				RootIndex = nullptr;
			}

			if (!RootIndex)
			{
				RootIndex = PendingAssetRootIndexByAssetPath.Find(MakeReviewAssetLinkKey(Change.AssetPath));
			}
			if (!RootIndex || !Changes.IsValidIndex(*RootIndex))
			{
				continue;
			}

			Change.ParentChangeId = Changes[*RootIndex].ChangeId;
		}
	}
int32 FBlueprintHelperReviewStoreTargetUtils::GetReviewSortValue(int32 Value)
	{
		return Value == INDEX_NONE ? MAX_int32 : Value;
	}
void FBlueprintHelperReviewStoreTargetUtils::SortVisibleChangesByReviewOrder(TArray<FBlueprintHelperReviewVisibleChange>& Changes)
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
FBlueprintHelperReviewVisibleChange FBlueprintHelperReviewStoreTargetUtils::MakeVisibleChangeFromEvidence(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FString& VisualGroupKey)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = MakeReviewVisibleChangeId(Evidence.EvidenceId, VisualGroupKey);
		Change.AssetPath = Target.AssetPath.IsEmpty() ? Evidence.AssetPath : Target.AssetPath;
		Change.GraphName = Target.GraphName;
		Change.LocationKey = VisualGroupKey;
		Change.LatestEvidenceId = Evidence.EvidenceId;
		Change.LatestEvidenceIds.Add(Evidence.EvidenceId);
		Change.SourceEvidenceIds.Add(Evidence.EvidenceId);
		Change.ScopeIdentity = Target.ScopeIdentity;
		Change.ChangeKind = Evidence.ChangeKind;
		Change.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Evidence.DisplayLabel : Target.DisplayLabel;
		Change.BeforeSummary = Evidence.BeforeSummary;
		Change.AfterSummary = Evidence.AfterSummary;
		Change.BeforeHash = Target.BaselineHash;
		Change.AfterHash = Target.RecordedAfterHash;
		Change.BeforeSnapshotJson = Target.BeforeSnapshotJson;
		Change.AfterSnapshotJson = Target.AfterSnapshotJson;
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
