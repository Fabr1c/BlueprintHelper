// BlueprintHelper Review UI data types.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"

enum class EBlueprintHelperReviewChangeKind : uint8
{
	Added,
	Removed,
	VariableModified,
	SignatureModified,
	Modified,
	Renamed
};

inline const TCHAR* BlueprintHelperReviewChangeKindToString(EBlueprintHelperReviewChangeKind Kind)
{
	struct FBlueprintHelperReviewChangeKindName
	{
		EBlueprintHelperReviewChangeKind Kind;
		const TCHAR* Name;
	};
	static const FBlueprintHelperReviewChangeKindName Names[] =
	{
		{ EBlueprintHelperReviewChangeKind::Added, TEXT("added") },
		{ EBlueprintHelperReviewChangeKind::Removed, TEXT("removed") },
		{ EBlueprintHelperReviewChangeKind::VariableModified, TEXT("variable_modified") },
		{ EBlueprintHelperReviewChangeKind::SignatureModified, TEXT("signature_modified") },
		{ EBlueprintHelperReviewChangeKind::Modified, TEXT("modified") },
		{ EBlueprintHelperReviewChangeKind::Renamed, TEXT("renamed") }
	};
	for (const FBlueprintHelperReviewChangeKindName& Entry : Names)
	{
		if (Entry.Kind == Kind)
		{
			return Entry.Name;
		}
	}
	return TEXT("unknown");
}

inline const TCHAR* BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind Kind)
{
	struct FBlueprintHelperReviewChangeKindColor
	{
		EBlueprintHelperReviewChangeKind Kind;
		const TCHAR* Color;
	};
	static const FBlueprintHelperReviewChangeKindColor Colors[] =
	{
		{ EBlueprintHelperReviewChangeKind::Added, TEXT("green") },
		{ EBlueprintHelperReviewChangeKind::Removed, TEXT("red") },
		{ EBlueprintHelperReviewChangeKind::VariableModified, TEXT("yellow") },
		{ EBlueprintHelperReviewChangeKind::SignatureModified, TEXT("yellow") },
		{ EBlueprintHelperReviewChangeKind::Modified, TEXT("yellow") },
		{ EBlueprintHelperReviewChangeKind::Renamed, TEXT("green") }
	};
	for (const FBlueprintHelperReviewChangeKindColor& Entry : Colors)
	{
		if (Entry.Kind == Kind)
		{
			return Entry.Color;
		}
	}
	return TEXT("none");
}

enum class EBlueprintHelperReviewChangeStatus : uint8
{
	Pending,
	Accepted,
	Rejected,
	NeedsAction,
	Superseded,
	RejectFailed
};

inline const TCHAR* BlueprintHelperReviewChangeStatusToString(EBlueprintHelperReviewChangeStatus Status)
{
	struct FBlueprintHelperReviewChangeStatusName
	{
		EBlueprintHelperReviewChangeStatus Status;
		const TCHAR* Name;
	};
	static const FBlueprintHelperReviewChangeStatusName Names[] =
	{
		{ EBlueprintHelperReviewChangeStatus::Pending, TEXT("pending") },
		{ EBlueprintHelperReviewChangeStatus::Accepted, TEXT("accepted") },
		{ EBlueprintHelperReviewChangeStatus::Rejected, TEXT("rejected") },
		{ EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("needs_action") },
		{ EBlueprintHelperReviewChangeStatus::Superseded, TEXT("superseded") },
		{ EBlueprintHelperReviewChangeStatus::RejectFailed, TEXT("reject_failed") }
	};
	for (const FBlueprintHelperReviewChangeStatusName& Entry : Names)
	{
		if (Entry.Status == Status)
		{
			return Entry.Name;
		}
	}
	return TEXT("unknown");
}

enum class EBlueprintHelperReviewStorageStatus : uint8
{
	Active,
	Compacted
};

inline const TCHAR* BlueprintHelperReviewStorageStatusToString(EBlueprintHelperReviewStorageStatus Status)
{
	struct FBlueprintHelperReviewStorageStatusName
	{
		EBlueprintHelperReviewStorageStatus Status;
		const TCHAR* Name;
	};
	static const FBlueprintHelperReviewStorageStatusName Names[] =
	{
		{ EBlueprintHelperReviewStorageStatus::Active, TEXT("active") },
		{ EBlueprintHelperReviewStorageStatus::Compacted, TEXT("compacted") }
	};
	for (const FBlueprintHelperReviewStorageStatusName& Entry : Names)
	{
		if (Entry.Status == Status)
		{
			return Entry.Name;
		}
	}
	return TEXT("unknown");
}

enum class EBlueprintHelperReviewSurface : uint8
{
	Unknown,
	Graph,
	Components,
	MyBlueprint,
	Details,
	UMGWidgetTree,
	DataTable,
	DataAsset,
	Material
};

inline const TCHAR* BlueprintHelperReviewSurfaceToString(EBlueprintHelperReviewSurface Surface)
{
	struct FBlueprintHelperReviewSurfaceName
	{
		EBlueprintHelperReviewSurface Surface;
		const TCHAR* Name;
	};
	static const FBlueprintHelperReviewSurfaceName Names[] =
	{
		{ EBlueprintHelperReviewSurface::Unknown, TEXT("unknown") },
		{ EBlueprintHelperReviewSurface::Graph, TEXT("graph") },
		{ EBlueprintHelperReviewSurface::Components, TEXT("components") },
		{ EBlueprintHelperReviewSurface::MyBlueprint, TEXT("my_blueprint") },
		{ EBlueprintHelperReviewSurface::Details, TEXT("details") },
		{ EBlueprintHelperReviewSurface::UMGWidgetTree, TEXT("umg_widget_tree") },
		{ EBlueprintHelperReviewSurface::DataTable, TEXT("data_table") },
		{ EBlueprintHelperReviewSurface::DataAsset, TEXT("data_asset") },
		{ EBlueprintHelperReviewSurface::Material, TEXT("material") }
	};
	for (const FBlueprintHelperReviewSurfaceName& Entry : Names)
	{
		if (Entry.Surface == Surface)
		{
			return Entry.Name;
		}
	}
	return TEXT("unknown");
}

BLUEPRINTHELPER_API EBlueprintHelperReviewSurface BlueprintHelperReviewNormalizeSurfaceForTarget(
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKind,
	const FString& TargetKey,
	const FString& VisualGroupKey = FString(),
	const FString& LocationKey = FString());

struct FBlueprintHelperReviewAtomicTarget
{
	FString AssetPath;
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Graph;
	FString GraphName;
	FString TargetKey;
	FString TargetKind;
	FString TargetSubKind;
	FString SignatureRole;
	FString SignatureEvidenceId;
	FString DependencyOwnerStepId;
	FString DependentStepId;
	FString ScopeIdentity;
	FString VisualGroupKey;
	FString DisplayLabel;
	FString FirstEvidenceId;
	FString LatestEvidenceId;
	TArray<FString> SourceEvidenceIds;
	FString Ownership = TEXT("unknown");
	FString NodeGuid;
	FString PinPath;
	FString PropertyPath;
	FString ComponentPath;
	FString ComponentId;
	FString ComponentTemplatePath;
	FString ComponentOrigin;
	FString BeforeParent;
	FString AfterParent;
	FString BeforeRoot;
	FString AfterRoot;
	FString DeletePolicy;
	FString DeletedComponentIdsJson;
	FString MovedComponentIdsJson;
	FString ChangedPropertiesJson;
	FString ReadbackFingerprintBefore;
	FString ReadbackFingerprintAfter;
	FString LifecycleObjectKey;
	FString LifecycleParentKey;
	FString AnchorJson;
	FString GraphBodyBoundaryJson;
	FString BeforeSnapshotJson;
	FString AfterSnapshotJson;
	FString RecordedAfterHash;
	FString BaselineHash;
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	int32 ExecutionOrder = INDEX_NONE;
	int32 TaskStepIndex = INDEX_NONE;
	int32 AtomicIndex = INDEX_NONE;
	bool bHasGraphBounds = false;
	FVector2D GraphPosition = FVector2D::ZeroVector;
	FVector2D GraphSize = FVector2D(360.0f, 180.0f);
};

struct FBlueprintHelperReviewEvidenceInput
{
	FString EvidenceId;
	FString AssetPath;
	FString GraphName;
	FString LocationKey;
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FString DisplayLabel;
	FString BeforeSummary;
	FString AfterSummary;
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets;
};

struct FBlueprintHelperReviewVisibleChange
{
	FString ChangeId;
	FString AssetPath;
	FString GraphName;
	FString LocationKey;
	FString LatestEvidenceId;
	TArray<FString> LatestEvidenceIds;
	TArray<FString> SourceEvidenceIds;
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets;
	FString ScopeIdentity;
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	FString DisplayLabel;
	FString BeforeSummary;
	FString AfterSummary;
	FString BeforeSnapshotJson;
	FString AfterSnapshotJson;
	FString BeforeHash;
	FString AfterHash;
	FString NeedsActionReason;
	FString ParentChangeId;
	int32 ExecutionOrder = INDEX_NONE;
	int32 TaskStepIndex = INDEX_NONE;
	int32 AtomicIndex = INDEX_NONE;
	bool bIsAssetLifecycleRoot = false;
	bool bIsObjectLifecycleRoot = false;
	bool bRejectRemovesChildren = false;
};

struct FBlueprintHelperWriteReviewEvidence
{
	FString Schema = TEXT("BlueprintHelper.WriteReviewEvidence.v2");
	FString ArchiveSessionId;
	FString TaskRunId;
	FString EvidenceId;
	FString CreatedAt;
	FString AssetPath;
	FString OperationKind;
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FString DisplayLabel;
	FString BeforeSummary;
	FString AfterSummary;
	int32 TaskStepIndex = INDEX_NONE;
	TArray<FString> DebugCaseIds;
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets;
};

struct FBlueprintHelperReviewArchiveSession
{
	FString Schema = TEXT("BlueprintHelper.ArchiveSession.v2");
	FString ArchiveSessionId;
	FString TaskRunId;
	TArray<FString> AllowedTargetAssets;
	TArray<FString> BaselineSnapshotRefs;
	TArray<FString> BaselineSemanticSnapshotRefs;
	TArray<FString> DirtyTargetAssets;
	TArray<FString> BaselineWarnings;
	FString BaselineDirtyAssetPolicy;
	FString BaselineSnapshotTrust;
	FString DirtyState;
	FString SafeNextAction;
	TArray<FString> AllowedRecoveryActions;
	TArray<FString> RiskyRecoveryActions;
	TArray<FString> DirtyEvidenceRefs;
	FString CreatedAt;
};

struct FBlueprintHelperReviewActionRecord
{
	FString Action;
	TArray<FString> TargetKeys;
	FString OwnershipPolicy;
	FString CreatedAt;
	FString SourceEvidenceId;
	FString Message;
};

struct FBlueprintHelperReviewSourceSummary
{
	int32 EvidenceCount = 0;
	TArray<FString> TaskRunIds;
	TArray<FString> OperationKinds;
	TArray<FString> AssetPaths;
	TArray<FString> EvidenceIds;
	FString CreatedAtFirst;
	FString CreatedAtLast;
	EBlueprintHelperReviewChangeStatus FinalReviewStatus = EBlueprintHelperReviewChangeStatus::Pending;
};

struct FBlueprintHelperReviewRecord
{
	FString Schema = TEXT("BlueprintHelper.ReviewRecord.v2");
	FString ReviewRecordId;
	FString ArchiveSessionId;
	FString AssetPath;
	TArray<FString> SourceTaskRunIds;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	EBlueprintHelperReviewStorageStatus StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
	TArray<FBlueprintHelperReviewVisibleChange> VisibleChanges;
	TArray<FBlueprintHelperReviewActionRecord> ReviewActions;
	TArray<FString> DebugCaseIds;
	FBlueprintHelperReviewSourceSummary SourceReviewSummary;
};

struct FBlueprintHelperReviewRecordQuery
{
	FString ArchiveSessionIdFilter;
	FString AssetPathFilter;
	FString TaskRunIdFilter;
	bool bPendingOnly = true;
};

BLUEPRINTHELPER_API FString BlueprintHelperReviewNormalizeLocation(
	const FBlueprintHelperReviewVisibleChange& Change);

BLUEPRINTHELPER_API int32 BlueprintHelperReviewCountSurfaceTargets(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface);

BLUEPRINTHELPER_API bool BlueprintHelperReviewTargetKindCanRouteToDetails(const FString& TargetKind);

BLUEPRINTHELPER_API int32 BlueprintHelperReviewCountDetailsTargets(
	const FBlueprintHelperReviewVisibleChange& Change);

BLUEPRINTHELPER_API bool BlueprintHelperReviewHasExplicitTargets(const FBlueprintHelperReviewVisibleChange& Change);

BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowOnSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface);

BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInComponents(const FBlueprintHelperReviewVisibleChange& Change);
BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInGraph(const FBlueprintHelperReviewVisibleChange& Change);
BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInDetails(const FBlueprintHelperReviewVisibleChange& Change);
BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInUMGWidgetTree(const FBlueprintHelperReviewVisibleChange& Change);
BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInDataTable(const FBlueprintHelperReviewVisibleChange& Change);
BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInDataAsset(const FBlueprintHelperReviewVisibleChange& Change);
BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInMaterial(const FBlueprintHelperReviewVisibleChange& Change);
BLUEPRINTHELPER_API bool BlueprintHelperReviewShouldShowInMyBlueprint(const FBlueprintHelperReviewVisibleChange& Change);

