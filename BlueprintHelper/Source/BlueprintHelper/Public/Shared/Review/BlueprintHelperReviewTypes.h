// BlueprintHelper Review UI data types.

#pragma once

#include "CoreMinimal.h"

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
	DataAsset
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
		{ EBlueprintHelperReviewSurface::DataAsset, TEXT("data_asset") }
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

inline EBlueprintHelperReviewSurface BlueprintHelperReviewNormalizeSurfaceForTarget(
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKind,
	const FString& TargetKey,
	const FString& VisualGroupKey = FString(),
	const FString& LocationKey = FString())
{
	FString Text = TargetKind + TEXT(" ") + TargetKey + TEXT(" ") + VisualGroupKey + TEXT(" ") + LocationKey;
	Text.ToLowerInline();

	if (Text.Contains(TEXT("graph_block"))
		|| Text.Contains(TEXT("graph_node"))
		|| Text.Contains(TEXT("graph_pin"))
		|| Text.Contains(TEXT("graph_link"))
		|| Text.Contains(TEXT("graph:"))
		|| Text.Contains(TEXT("node:"))
		|| Text.Contains(TEXT("pin:")))
	{
		return EBlueprintHelperReviewSurface::Graph;
	}

	if (Text.Contains(TEXT("component")))
	{
		return EBlueprintHelperReviewSurface::Components;
	}

	if (Text.Contains(TEXT("blueprint_variable"))
		|| Text.Contains(TEXT("signature"))
		|| Text.Contains(TEXT("dispatcher"))
		|| Text.Contains(TEXT("delegate"))
		|| Text.Contains(TEXT("function"))
		|| Text.Contains(TEXT("macro"))
		|| Text.Contains(TEXT("my_blueprint")))
	{
		return EBlueprintHelperReviewSurface::MyBlueprint;
	}

	if (Text.Contains(TEXT("umg_widget"))
		|| Text.Contains(TEXT("widget_tree"))
		|| Text.Contains(TEXT("widgetblueprint"))
		|| Text.Contains(TEXT("widget_blueprint")))
	{
		return EBlueprintHelperReviewSurface::UMGWidgetTree;
	}

	if (Text.Contains(TEXT("datatable"))
		|| Text.Contains(TEXT("data_table")))
	{
		return EBlueprintHelperReviewSurface::DataTable;
	}

	if (Text.Contains(TEXT("data_asset_property"))
		|| Text.Contains(TEXT("dataasset"))
		|| Text.Contains(TEXT("data_asset"))
		|| Text.Contains(TEXT("structure"))
		|| Text.Contains(TEXT("struct_field"))
		|| Text.Contains(TEXT("object_property")))
	{
		return EBlueprintHelperReviewSurface::DataAsset;
	}

	if (Surface != EBlueprintHelperReviewSurface::Unknown)
	{
		return Surface;
	}

	return EBlueprintHelperReviewSurface::Details;
}

struct FBlueprintHelperReviewAtomicTarget
{
	FString AssetPath;
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Graph;
	FString GraphName;
	FString TargetKey;
	FString TargetKind;
	FString ScopeIdentity;
	FString VisualGroupKey;
	FString DisplayLabel;
	FString FirstTransactionId;
	FString LatestTransactionId;
	TArray<FString> SourceTransactionIds;
	FString Ownership = TEXT("unknown");
	FString NodeGuid;
	FString PinPath;
	FString PropertyPath;
	FString ComponentPath;
	FString AnchorJson;
	FString BeforeSnapshotJson;
	FString AfterSnapshotJson;
	FString RecordedAfterHash;
	FString BaselineHash;
	FString RollbackDataRef;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	int32 ExecutionOrder = INDEX_NONE;
	int32 TaskStepIndex = INDEX_NONE;
	int32 AtomicIndex = INDEX_NONE;
	bool bHasGraphBounds = false;
	FVector2D GraphPosition = FVector2D::ZeroVector;
	FVector2D GraphSize = FVector2D(360.0f, 180.0f);
};

struct FBlueprintHelperReviewTransactionInput
{
	FString TransactionId;
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
	FString LatestTransactionId;
	TArray<FString> LatestTransactionIds;
	TArray<FString> SourceTransactionIds;
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
	bool bRejectRemovesChildren = false;
};

struct FBlueprintHelperWriteReviewEvidence
{
	FString Schema = TEXT("BlueprintHelper.WriteReviewEvidence.v1");
	FString ArchiveSessionId;
	FString TaskRunId;
	FString TransactionId;
	FString CreatedAt;
	FString AssetPath;
	FString OperationKind;
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	FString DisplayLabel;
	FString BeforeSummary;
	FString AfterSummary;
	int32 TaskStepIndex = INDEX_NONE;
	TArray<FString> DebugCaseIds;
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets;
};

struct FBlueprintHelperReviewArchiveSession
{
	FString Schema = TEXT("BlueprintHelper.ArchiveSession.v1");
	FString ArchiveSessionId;
	FString TaskRunId;
	TArray<FString> AllowedTargetAssets;
	TArray<FString> BaselineSnapshotRefs;
	TArray<FString> BaselineSemanticSnapshotRefs;
	TArray<FString> DirtyTargetAssets;
	TArray<FString> BaselineWarnings;
	FString BaselineDirtyAssetPolicy;
	FString BaselineSnapshotTrust;
	FString CreatedAt;
};

struct FBlueprintHelperReviewActionRecord
{
	FString Action;
	TArray<FString> TargetKeys;
	FString OwnershipPolicy;
	FString CreatedAt;
	FString SourceTransactionId;
	FString Message;
};

struct FBlueprintHelperReviewSourceTransactionSummary
{
	int32 TransactionCount = 0;
	TArray<FString> TaskRunIds;
	TArray<FString> OperationKinds;
	TArray<FString> AssetPaths;
	TArray<FString> TransactionIds;
	FString CreatedAtFirst;
	FString CreatedAtLast;
	EBlueprintHelperReviewChangeStatus FinalReviewStatus = EBlueprintHelperReviewChangeStatus::Pending;
};

struct FBlueprintHelperReviewRecord
{
	FString Schema = TEXT("BlueprintHelper.ReviewRecord.v1");
	FString ReviewRecordId;
	FString ArchiveSessionId;
	FString AssetPath;
	TArray<FString> SourceTaskRunIds;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	EBlueprintHelperReviewStorageStatus StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
	TArray<FBlueprintHelperReviewVisibleChange> VisibleChanges;
	TArray<FBlueprintHelperReviewActionRecord> ReviewActions;
	TArray<FString> DebugCaseIds;
	FBlueprintHelperReviewSourceTransactionSummary SourceTransactionSummary;
};

struct FBlueprintHelperReviewRecordQuery
{
	FString ArchiveSessionIdFilter;
	FString AssetPathFilter;
	FString TaskRunIdFilter;
	bool bPendingOnly = true;
};

struct FBlueprintHelperReviewConvertOwnerBlockRequest
{
	FString ReviewRecordId;
	FString Direction;
	FString BlockTargetKey;
	FString EntryAnchor;
	TArray<FString> NodeAnchors;
	TArray<FString> LinkAnchors;
	FString DesiredBlockRef;
	FString ConversionTransactionId;
	bool bSettingProfileAllowsConversion = false;
};

inline FString BlueprintHelperReviewNormalizeLocation(const FBlueprintHelperReviewVisibleChange& Change)
{
	FString Location = Change.LocationKey;
	if (Location.IsEmpty())
	{
		Location = Change.DisplayLabel;
	}
	Location.ToLowerInline();
	return Location;
}

inline int32 BlueprintHelperReviewCountSurfaceTargets(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	int32 Count = 0;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == Surface)
		{
			++Count;
		}
	}
	return Count;
}

inline bool BlueprintHelperReviewTargetKindCanRouteToDetails(const FString& TargetKind)
{
	FString Kind = TargetKind;
	Kind.ToLowerInline();
	return Kind.Contains(TEXT("class_default"))
		|| Kind.Contains(TEXT("blueprint_default"))
		|| Kind.Contains(TEXT("blueprint_setting"))
		|| Kind.Contains(TEXT("blueprint_variable"))
		|| Kind.Contains(TEXT("variable"))
		|| Kind.Contains(TEXT("component"))
		|| Kind.Contains(TEXT("object_property"))
		|| Kind.Contains(TEXT("property"))
		|| Kind.Contains(TEXT("class_setting"))
		|| Kind.Contains(TEXT("blueprint_class"))
		|| Kind.Contains(TEXT("interface"))
		|| Kind.Contains(TEXT("signature"))
		|| Kind.Contains(TEXT("dispatcher"))
		|| Kind.Contains(TEXT("blueprint_variable"))
		|| Kind.Contains(TEXT("variable_default"))
		|| Kind.Contains(TEXT("object_property"));
}

inline int32 BlueprintHelperReviewCountDetailsTargets(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	int32 Count = 0;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::Details)
		{
			++Count;
			continue;
		}

		if (Target.Surface != EBlueprintHelperReviewSurface::DataAsset
			&& Target.Surface != EBlueprintHelperReviewSurface::DataTable
			&& Target.Surface != EBlueprintHelperReviewSurface::UMGWidgetTree
			&& BlueprintHelperReviewTargetKindCanRouteToDetails(Target.TargetKind))
		{
			++Count;
		}
	}
	return Count;
}

inline bool BlueprintHelperReviewHasExplicitTargets(const FBlueprintHelperReviewVisibleChange& Change)
{
	return Change.AtomicTargets.Num() > 0;
}

inline bool BlueprintHelperReviewShouldShowInComponents(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::Components) > 0;
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Location.Contains(TEXT("component"));
}

inline bool BlueprintHelperReviewShouldShowInGraph(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::Graph) > 0;
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return !Change.GraphName.IsEmpty()
		|| Location.Contains(TEXT("graph:"))
		|| Location.Contains(TEXT("node:"))
		|| Location.Contains(TEXT("pin:"));
}

inline bool BlueprintHelperReviewShouldShowInDetails(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return BlueprintHelperReviewCountDetailsTargets(Change) > 0;
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Change.ChangeKind == EBlueprintHelperReviewChangeKind::VariableModified
		|| Change.ChangeKind == EBlueprintHelperReviewChangeKind::SignatureModified
		|| Location.Contains(TEXT("property"))
		|| Location.Contains(TEXT("variable"))
		|| Location.Contains(TEXT("signature"))
		|| Location.Contains(TEXT("dispatcher"));
}

inline bool BlueprintHelperReviewShouldShowInUMGWidgetTree(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::UMGWidgetTree) > 0;
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Location.Contains(TEXT("umg_widget"))
		|| Location.Contains(TEXT("widget_tree"))
		|| Location.Contains(TEXT("widgetblueprint"))
		|| Location.Contains(TEXT("widget_blueprint"));
}

inline bool BlueprintHelperReviewShouldShowInDataTable(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::DataTable) > 0;
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Location.Contains(TEXT("datatable"))
		|| Location.Contains(TEXT("data_table"));
}

inline bool BlueprintHelperReviewShouldShowInDataAsset(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::DataAsset) > 0;
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Location.Contains(TEXT("data_asset"))
		|| Location.Contains(TEXT("dataasset"))
		|| Location.Contains(TEXT("structure"))
		|| Location.Contains(TEXT("struct_field"))
		|| Location.Contains(TEXT("object_property"));
}

inline bool BlueprintHelperReviewShouldShowInMyBlueprint(const FBlueprintHelperReviewVisibleChange& Change)
{
	if (BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return BlueprintHelperReviewCountSurfaceTargets(Change, EBlueprintHelperReviewSurface::MyBlueprint) > 0;
	}

	if (BlueprintHelperReviewShouldShowInComponents(Change))
	{
		return false;
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Location.Contains(TEXT("my_blueprint"))
		|| Location.Contains(TEXT("function"))
		|| Location.Contains(TEXT("macro"))
		|| Location.Contains(TEXT("variable"))
		|| Location.Contains(TEXT("dispatcher"))
		|| Location.Contains(TEXT("delegate"));
}
