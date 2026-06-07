#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperAcceptedPayloadModel.h"
#include "Shared/Review/BlueprintHelperReviewBoundaryModel.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Debug/BlueprintHelperDiagnosticProjection.h"

class BLUEPRINTHELPER_API FBlueprintHelperWriteReviewEvidenceProjection
{
public:
	static FBlueprintHelperReviewAtomicTarget BuildAtomicTarget(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		const FBlueprintHelperReviewBoundaryModel& Boundary,
		const FBlueprintHelperDiagnosticProjection& DiagnosticProjection);
	static void ApplyBoundaryToAtomicTarget(
		const FBlueprintHelperReviewBoundaryModel& Boundary,
		FBlueprintHelperReviewAtomicTarget& InOutTarget);
	static void AttachDiagnostics(
		FBlueprintHelperWriteReviewEvidence& Evidence,
		const TArray<FBlueprintHelperDiagnosticProjection>& Diagnostics);

private:
	static FBlueprintHelperDiagnosticItem DiagnosticItemFromProjection(
		const FBlueprintHelperDiagnosticProjection& Projection);
};
