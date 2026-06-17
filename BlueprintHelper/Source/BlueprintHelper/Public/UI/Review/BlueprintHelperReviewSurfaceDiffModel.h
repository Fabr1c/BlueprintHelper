// BlueprintHelper Review surface diff model.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct FBlueprintHelperReviewSurfaceDiffProjectionModel
{
	FString ReviewEventId;
	FString AssetPath;
	FString SurfaceKind;
	FString TargetKind;
	FString TargetKey;
	TArray<FString> MatchKeys;
	FString DisplayLabel;
	FLinearColor DiffColor = FLinearColor::Transparent;
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	bool bCanAccept = false;
	bool bCanReject = false;
};

inline FLinearColor BlueprintHelperReviewSurfaceDiffColor(EBlueprintHelperReviewChangeKind ChangeKind)
{
	switch (ChangeKind)
	{
	case EBlueprintHelperReviewChangeKind::Added:
		return FLinearColor(0.0f, 0.8f, 0.35f, 0.35f);
	case EBlueprintHelperReviewChangeKind::Removed:
		return FLinearColor(0.9f, 0.1f, 0.1f, 0.35f);
	case EBlueprintHelperReviewChangeKind::VariableModified:
	case EBlueprintHelperReviewChangeKind::SignatureModified:
	case EBlueprintHelperReviewChangeKind::Modified:
		return FLinearColor(1.0f, 0.75f, 0.05f, 0.35f);
	case EBlueprintHelperReviewChangeKind::Renamed:
		return FLinearColor(0.0f, 0.6f, 1.0f, 0.35f);
	default:
		return FLinearColor::Transparent;
	}
}
