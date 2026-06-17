// BlueprintHelper Review baseline dirty debug evidence projection implementation.

#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyDebugEvidenceProjection.h"

FString FBlueprintHelperReviewBaselineDirtyDebugEvidenceProjection::ClassifyEvidenceRefRole(
	const FString& EvidenceRef)
{
	if (EvidenceRef.StartsWith(TEXT("source_control:"), ESearchCase::IgnoreCase))
	{
		return TEXT("source_control");
	}
	if (EvidenceRef.StartsWith(TEXT("external_dirty:"), ESearchCase::IgnoreCase))
	{
		return TEXT("external_dirty");
	}
	if (EvidenceRef.StartsWith(TEXT("task_run:"), ESearchCase::IgnoreCase))
	{
		return TEXT("failed_task_run");
	}
	if (EvidenceRef.StartsWith(TEXT("pre_run_dirty:"), ESearchCase::IgnoreCase))
	{
		return TEXT("pre_run_dirty");
	}
	return TEXT("review_baseline_dirty");
}

TArray<FBlueprintHelperDebugEvidenceLink>
FBlueprintHelperReviewBaselineDirtyDebugEvidenceProjection::MakeEvidenceLinksFromRefs(
	const TArray<FString>& EvidenceRefs)
{
	TArray<FBlueprintHelperDebugEvidenceLink> Links;
	for (const FString& EvidenceRef : EvidenceRefs)
	{
		if (EvidenceRef.IsEmpty())
		{
			continue;
		}

		FBlueprintHelperDebugEvidenceLink Link;
		Link.EvidenceId = EvidenceRef;
		Link.Role = ClassifyEvidenceRefRole(EvidenceRef);
		Link.Source = TEXT("review_baseline_dirty_classifier");
		Link.Summary = EvidenceRef;
		Links.Add(Link);
	}
	return Links;
}
