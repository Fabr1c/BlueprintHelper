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
	switch (Kind)
	{
	case EBlueprintHelperReviewChangeKind::Added:             return TEXT("added");
	case EBlueprintHelperReviewChangeKind::Removed:           return TEXT("removed");
	case EBlueprintHelperReviewChangeKind::VariableModified:  return TEXT("variable_modified");
	case EBlueprintHelperReviewChangeKind::SignatureModified: return TEXT("signature_modified");
	case EBlueprintHelperReviewChangeKind::Modified:          return TEXT("modified");
	case EBlueprintHelperReviewChangeKind::Renamed:           return TEXT("renamed");
	default:                                                  return TEXT("unknown");
	}
}

inline const TCHAR* BlueprintHelperReviewChangeKindToColorName(EBlueprintHelperReviewChangeKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperReviewChangeKind::Added:             return TEXT("green");
	case EBlueprintHelperReviewChangeKind::Removed:           return TEXT("red");
	case EBlueprintHelperReviewChangeKind::VariableModified:  return TEXT("yellow");
	case EBlueprintHelperReviewChangeKind::SignatureModified: return TEXT("yellow");
	case EBlueprintHelperReviewChangeKind::Modified:          return TEXT("yellow");
	case EBlueprintHelperReviewChangeKind::Renamed:           return TEXT("green");
	default:                                                  return TEXT("none");
	}
}

enum class EBlueprintHelperReviewChangeStatus : uint8
{
	Pending,
	Accepted,
	Rejected,
	NeedsAction,
	Superseded
};

inline const TCHAR* BlueprintHelperReviewChangeStatusToString(EBlueprintHelperReviewChangeStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperReviewChangeStatus::Pending:     return TEXT("pending");
	case EBlueprintHelperReviewChangeStatus::Accepted:    return TEXT("accepted");
	case EBlueprintHelperReviewChangeStatus::Rejected:    return TEXT("rejected");
	case EBlueprintHelperReviewChangeStatus::NeedsAction: return TEXT("needs_action");
	case EBlueprintHelperReviewChangeStatus::Superseded:  return TEXT("superseded");
	default:                                              return TEXT("unknown");
	}
}

enum class EBlueprintHelperReviewSurface : uint8
{
	Graph,
	Components,
	MyBlueprint,
	Details
};

inline const TCHAR* BlueprintHelperReviewSurfaceToString(EBlueprintHelperReviewSurface Surface)
{
	switch (Surface)
	{
	case EBlueprintHelperReviewSurface::Graph:       return TEXT("graph");
	case EBlueprintHelperReviewSurface::Components:  return TEXT("components");
	case EBlueprintHelperReviewSurface::MyBlueprint: return TEXT("my_blueprint");
	case EBlueprintHelperReviewSurface::Details:     return TEXT("details");
	default:                                         return TEXT("unknown");
	}
}

struct FBlueprintHelperReviewAtomicTarget
{
	FString AssetPath;
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Graph;
	FString GraphName;
	FString TargetKey;
	FString VisualGroupKey;
	FString DisplayLabel;
	FString LatestTransactionId;
	TArray<FString> SourceTransactionIds;
	FString NodeGuid;
	FString PinPath;
	FString PropertyPath;
	FString ComponentPath;
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
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	FString DisplayLabel;
	FString BeforeSummary;
	FString AfterSummary;
	FString NeedsActionReason;
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

inline bool BlueprintHelperReviewShouldShowInComponents(const FBlueprintHelperReviewVisibleChange& Change)
{
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::Components)
		{
			return true;
		}
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Location.Contains(TEXT("component"));
}

inline bool BlueprintHelperReviewShouldShowInGraph(const FBlueprintHelperReviewVisibleChange& Change)
{
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::Graph)
		{
			return true;
		}
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return !Change.GraphName.IsEmpty()
		|| Location.Contains(TEXT("graph:"))
		|| Location.Contains(TEXT("node:"))
		|| Location.Contains(TEXT("pin:"));
}

inline bool BlueprintHelperReviewShouldShowInDetails(const FBlueprintHelperReviewVisibleChange& Change)
{
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::Details)
		{
			return true;
		}
	}

	const FString Location = BlueprintHelperReviewNormalizeLocation(Change);
	return Change.ChangeKind == EBlueprintHelperReviewChangeKind::VariableModified
		|| Change.ChangeKind == EBlueprintHelperReviewChangeKind::SignatureModified
		|| Location.Contains(TEXT("property"))
		|| Location.Contains(TEXT("variable"))
		|| Location.Contains(TEXT("signature"))
		|| Location.Contains(TEXT("dispatcher"));
}

inline bool BlueprintHelperReviewShouldShowInMyBlueprint(const FBlueprintHelperReviewVisibleChange& Change)
{
	bool bHasExplicitTargets = false;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		bHasExplicitTargets = true;
		if (Target.Surface == EBlueprintHelperReviewSurface::MyBlueprint)
		{
			return true;
		}
	}

	if (bHasExplicitTargets)
	{
		return false;
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
