// BlueprintHelper MaterialInstance Review surface projection adapter implementation.

#include "UI/Review/BlueprintHelperMaterialInstanceSurfaceProjectionAdapter.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceParameterJsonUtils.h"
#include "UI/Review/BlueprintHelperReviewMaterialInstancePresenterModel.h"

FBlueprintHelperMaterialInstanceSurfaceProjectionAdapter::FBlueprintHelperMaterialInstanceSurfaceProjectionAdapter(
	const FString& InAssetKind,
	const FString& InSurfaceKind,
	const FString& InTargetKind)
	: AssetKind(InAssetKind)
	, SurfaceKind(InSurfaceKind)
	, TargetKind(InTargetKind)
{
}

FString FBlueprintHelperMaterialInstanceSurfaceProjectionAdapter::GetAssetKind() const
{
	return AssetKind;
}

FString FBlueprintHelperMaterialInstanceSurfaceProjectionAdapter::GetSurfaceKind() const
{
	return SurfaceKind;
}

FString FBlueprintHelperMaterialInstanceSurfaceProjectionAdapter::GetTargetKind() const
{
	return TargetKind;
}

bool FBlueprintHelperMaterialInstanceSurfaceProjectionAdapter::CanProject(
	const FBlueprintHelperReviewTargetIdentity& Identity) const
{
	return (AssetKind.IsEmpty() || Identity.AssetKind.Equals(AssetKind, ESearchCase::IgnoreCase))
		&& Identity.SurfaceKind.Equals(SurfaceKind, ESearchCase::IgnoreCase)
		&& Identity.TargetKind.Equals(TargetKind, ESearchCase::IgnoreCase);
}

FBlueprintHelperReviewSurfaceProjectionResult FBlueprintHelperMaterialInstanceSurfaceProjectionAdapter::Project(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FBlueprintHelperReviewSurfaceProjectionResult Result;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (!FBlueprintHelperReviewStatusUtils::IsOpenReviewStatus(Target.Status))
		{
			continue;
		}

		const FBlueprintHelperReviewTargetIdentity Identity =
			FBlueprintHelperReviewTargetIdentity::FromAtomicTarget(Change, Target);
		FBlueprintHelperReviewTargetIdentity AdapterIdentity = Identity;
		if (AdapterIdentity.AssetKind.IsEmpty())
		{
			AdapterIdentity.AssetKind = AssetKind;
		}
		if (!CanProject(AdapterIdentity))
		{
			continue;
		}

		const TSharedPtr<FJsonObject> Before =
			FBlueprintHelperMaterialInstanceParameterJsonUtils::ParseJsonObject(Target.BeforeSnapshotJson);
		const TSharedPtr<FJsonObject> After =
			FBlueprintHelperMaterialInstanceParameterJsonUtils::ParseJsonObject(Target.AfterSnapshotJson);
		if (!Before.IsValid() && !After.IsValid())
		{
			continue;
		}

		FBlueprintHelperReviewSurfaceDiffProjectionModel Model;
		Model.ReviewEventId = Change.ChangeId;
		Model.AssetPath = Identity.AssetPath;
		Model.SurfaceKind = SurfaceKind;
		Model.TargetKind = Target.TargetKind;
		Model.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Change.DisplayLabel : Target.DisplayLabel;
		Model.DiffColor = BlueprintHelperReviewSurfaceDiffColor(Change.ChangeKind);
		Model.ChangeKind = Change.ChangeKind;
		Model.bCanAccept = Target.Status == EBlueprintHelperReviewChangeStatus::Pending;
		Model.bCanReject = Target.Status == EBlueprintHelperReviewChangeStatus::Pending;
		if (!Model.TargetKey.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Model.TargetKey);
		}
		if (!Target.PropertyPath.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Target.PropertyPath);
		}
		if (!Target.DisplayLabel.IsEmpty())
		{
			Model.MatchKeys.AddUnique(Target.DisplayLabel);
		}

		if (Before.IsValid())
		{
			Before->TryGetStringField(TEXT("parameter_name"), Model.ParameterName);
			Before->TryGetStringField(TEXT("parameter_type"), Model.ParameterType);
			Model.BeforeValue =
				FBlueprintHelperMaterialInstanceParameterJsonUtils::ReadParameterValueString(Before, TEXT("effective_value"));
		}
		if (After.IsValid())
		{
			if (Model.ParameterName.IsEmpty())
			{
				After->TryGetStringField(TEXT("parameter_name"), Model.ParameterName);
			}
			if (Model.ParameterType.IsEmpty())
			{
				After->TryGetStringField(TEXT("parameter_type"), Model.ParameterType);
			}
			Model.AfterValue =
				FBlueprintHelperMaterialInstanceParameterJsonUtils::ReadParameterValueString(After, TEXT("effective_value"));
			Model.EffectiveValue = Model.AfterValue;
			After->TryGetStringField(TEXT("source"), Model.Source);
			After->TryGetStringField(TEXT("override_state"), Model.OverrideState);
		}
		const FString CanonicalTargetKey =
			FBlueprintHelperReviewMaterialInstancePresenterModel::MakeParameterTargetKey(
				Model.ParameterName,
				Model.ParameterType);
		Model.TargetKey = Target.TargetKey.IsEmpty() ? CanonicalTargetKey : Target.TargetKey;
		FBlueprintHelperReviewMaterialInstancePresenterModel::AppendStableMatchKeys(
			Model.ParameterName,
			Model.ParameterType,
			Model.MatchKeys,
			Model.TargetKey,
			Target.PropertyPath,
			Model.DisplayLabel);
		Result.DiffModels.Add(Model);
	}

	Result.bProjected = Result.DiffModels.Num() > 0;
	Result.Message = Result.bProjected ? TEXT("projected") : TEXT("no_matching_material_instance_targets");
	return Result;
}
