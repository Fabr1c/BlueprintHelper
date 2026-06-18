// BlueprintHelper Review MaterialInstance evidence adapter implementation.

#include "Systems/Review/BlueprintHelperReviewMaterialInstanceEvidenceAdapter.h"

class FBlueprintHelperReviewMaterialInstanceEvidenceAdapterLocalUtils
{
public:
	static bool IsMaterialInstanceTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return Target.TargetKind.Equals(TEXT("material_instance"), ESearchCase::IgnoreCase)
			|| Target.TargetKind.Equals(TEXT("material_instance_parameter"), ESearchCase::IgnoreCase)
			|| (Target.TargetKind.Equals(TEXT("asset_factory"), ESearchCase::IgnoreCase)
				&& Target.TargetSubKind.Equals(TEXT("material_instance"), ESearchCase::IgnoreCase));
	}

	static void NormalizeTarget(FBlueprintHelperReviewAtomicTarget& Target)
	{
		if (!IsMaterialInstanceTarget(Target))
		{
			return;
		}
		if (Target.Surface == EBlueprintHelperReviewSurface::Unknown)
		{
			Target.Surface = EBlueprintHelperReviewSurface::Material;
		}
		if (Target.VisualGroupKey.IsEmpty())
		{
			Target.VisualGroupKey = Target.TargetKey;
		}
		if (Target.ScopeIdentity.IsEmpty()
			&& Target.TargetKind.Equals(TEXT("material_instance_parameter"), ESearchCase::IgnoreCase))
		{
			Target.ScopeIdentity = Target.TargetKey;
		}
	}
};

FBlueprintHelperReviewMaterialInstanceEvidenceAdapter::FBlueprintHelperReviewMaterialInstanceEvidenceAdapter(
	const FString& InTargetKind)
	: TargetKind(InTargetKind)
{
}

FString FBlueprintHelperReviewMaterialInstanceEvidenceAdapter::GetTargetKind() const
{
	return TargetKind;
}

FBlueprintHelperReviewEvidenceBuildResult FBlueprintHelperReviewMaterialInstanceEvidenceAdapter::BuildEvidence(
	const FBlueprintHelperReviewEvidenceInput& Input) const
{
	FBlueprintHelperReviewEvidenceBuildResult Result;
	Result.Evidence.ArchiveSessionId = Input.EvidenceId;
	Result.Evidence.EvidenceId = Input.EvidenceId;
	Result.Evidence.AssetPath = Input.AssetPath;
	Result.Evidence.ChangeKind = Input.ChangeKind;
	Result.Evidence.DisplayLabel = Input.DisplayLabel;
	Result.Evidence.BeforeSummary = Input.BeforeSummary;
	Result.Evidence.AfterSummary = Input.AfterSummary;
	Result.Evidence.AtomicTargets = Input.AtomicTargets;
	Result.Evidence.OperationKind = TEXT("material_instance_edit");
	for (FBlueprintHelperReviewAtomicTarget& Target : Result.Evidence.AtomicTargets)
	{
		FBlueprintHelperReviewMaterialInstanceEvidenceAdapterLocalUtils::NormalizeTarget(Target);
	}
	Result.bSucceeded = Result.Evidence.AtomicTargets.Num() > 0;
	Result.Message = Result.bSucceeded ? TEXT("built") : TEXT("missing_atomic_targets");
	return Result;
}
