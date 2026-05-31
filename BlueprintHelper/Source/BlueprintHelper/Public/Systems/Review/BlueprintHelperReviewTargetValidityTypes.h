// BlueprintHelper Review target validity DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

enum class EBlueprintHelperReviewInvalidReason : uint8
{
	None,
	AssetMissing,
	GraphMissing,
	GraphNodeMissing,
	VariableMissingOrRenamed,
	FunctionMissingOrRenamed,
	ComponentMissingOrRenamed,
	WidgetMissingOrRenamed,
	DataTableRowMissing,
	DataAssetPropertyMissing
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewValidityCandidate
{
	FString ReviewRecordId;
	FString ChangeId;
	FString AssetPath;
	FBlueprintHelperReviewAtomicTarget Target;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewValidityResult
{
	FBlueprintHelperReviewValidityCandidate Candidate;
	bool bValid = true;
	EBlueprintHelperReviewInvalidReason InvalidReason = EBlueprintHelperReviewInvalidReason::None;
	FString Message;
};

BLUEPRINTHELPER_API const TCHAR* BlueprintHelperReviewInvalidReasonToString(
	EBlueprintHelperReviewInvalidReason Reason);
