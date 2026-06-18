// BlueprintHelper Review MaterialInstance restore adapter implementation.

#include "Systems/Review/BlueprintHelperReviewMaterialInstanceRestoreAdapter.h"

#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceParameterJsonUtils.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"

FBlueprintHelperReviewMaterialInstanceRestoreAdapter::FBlueprintHelperReviewMaterialInstanceRestoreAdapter(
	const FString& InTargetKind)
	: TargetKind(InTargetKind)
{
}

FString FBlueprintHelperReviewMaterialInstanceRestoreAdapter::GetTargetKind() const
{
	return TargetKind;
}

FBlueprintHelperReviewRestoreResult FBlueprintHelperReviewMaterialInstanceRestoreAdapter::RestoreBeforeSnapshot(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FBlueprintHelperReviewRestoreResult Result;
	Result.RollbackMode = TEXT("material_instance_before_snapshot");
	if (Change.AtomicTargets.Num() != 1)
	{
		Result.Message = TEXT("material_instance_restore_requires_single_atomic_target");
		return Result;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Change.AtomicTargets[0];
	const FString AssetPath = Target.AssetPath.IsEmpty() ? Change.AssetPath : Target.AssetPath;
	const FString ObjectPath =
		FBlueprintHelperReviewSnapshotRestoreService::MakeObjectPathFromAssetPath(AssetPath);
	UMaterialInstanceConstant* Instance = FindObject<UMaterialInstanceConstant>(nullptr, *ObjectPath);
	if (!Instance)
	{
		Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *ObjectPath);
	}
	if (!Instance)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = FString::Printf(TEXT("material_instance_not_found:%s"), *AssetPath);
		return Result;
	}

	FString Error;
	const bool bRestored = Target.TargetKind.Equals(TEXT("material_instance"), ESearchCase::IgnoreCase)
		? RestoreParent(Instance, Target, Error)
		: RestoreParameter(Instance, Target, Error);
	if (!bRestored)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = Error.IsEmpty() ? TEXT("material_instance_restore_failed") : Error;
		return Result;
	}

	UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
	Instance->MarkPackageDirty();
	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	Result.Message = TEXT("rejected");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

bool FBlueprintHelperReviewMaterialInstanceRestoreAdapter::RestoreParent(
	UMaterialInstanceConstant* Instance,
	const FBlueprintHelperReviewAtomicTarget& Target,
	FString& OutError)
{
	const TSharedPtr<FJsonObject> Before =
		FBlueprintHelperMaterialInstanceParameterJsonUtils::ParseJsonObject(Target.BeforeSnapshotJson);
	FString ParentPath = Target.BeforeParent;
	if (Before.IsValid())
	{
		Before->TryGetStringField(TEXT("before_parent_material"), ParentPath);
		if (ParentPath.IsEmpty())
		{
			Before->TryGetStringField(TEXT("parent_material"), ParentPath);
		}
	}
	if (ParentPath.IsEmpty())
	{
		OutError = TEXT("material_instance_restore_parent_missing_before_snapshot");
		return false;
	}

	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
	if (!Parent)
	{
		Parent = LoadObject<UMaterialInterface>(
			nullptr,
			*FBlueprintHelperMaterialInstanceResolver::NormalizeMaterialInstanceObjectPath(ParentPath));
	}
	if (!Parent)
	{
		OutError = FString::Printf(TEXT("material_instance_restore_parent_not_found:%s"), *ParentPath);
		return false;
	}

	Instance->Modify();
	Instance->SetParentEditorOnly(Parent);
	return true;
}

bool FBlueprintHelperReviewMaterialInstanceRestoreAdapter::RestoreParameter(
	UMaterialInstanceConstant* Instance,
	const FBlueprintHelperReviewAtomicTarget& Target,
	FString& OutError)
{
	const TSharedPtr<FJsonObject> Before =
		FBlueprintHelperMaterialInstanceParameterJsonUtils::ParseJsonObject(Target.BeforeSnapshotJson);
	if (!Before.IsValid())
	{
		OutError = TEXT("material_instance_restore_parameter_missing_before_snapshot");
		return false;
	}

	FString ParameterName;
	Before->TryGetStringField(TEXT("parameter_name"), ParameterName);
	if (ParameterName.IsEmpty())
	{
		ParameterName = Target.PropertyPath;
	}
	if (ParameterName.IsEmpty())
	{
		OutError = TEXT("material_instance_restore_parameter_name_missing");
		return false;
	}

	EBlueprintHelperMaterialInstanceParameterType Type =
		EBlueprintHelperMaterialInstanceParameterType::Unknown;
	if (!FBlueprintHelperMaterialInstanceParameterJsonUtils::TryReadParameterType(Before, Type))
	{
		OutError = TEXT("material_instance_restore_parameter_type_missing");
		return false;
	}

	const FBlueprintHelperMaterialInstanceParameterResolveResult Current =
		FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			Instance,
			FName(*ParameterName),
			Type);
	if (!Current.bSuccess)
	{
		OutError = Current.ErrorMessage.IsEmpty() ? Current.ErrorCode : Current.ErrorMessage;
		return false;
	}

	bool bHasOverride = false;
	Before->TryGetBoolField(TEXT("has_override"), bHasOverride);
	Instance->Modify();
	if (!bHasOverride)
	{
		return ClearParameterOverride(Instance, Current.Parameter.ParameterInfo, Type, OutError);
	}

	FBlueprintHelperMaterialInstanceParameterValue Value;
	if (!FBlueprintHelperMaterialInstanceParameterJsonUtils::TryReadParameterValue(Before, Type, Value, OutError))
	{
		return false;
	}
	return ApplyParameterOverride(Instance, Current.Parameter.ParameterInfo, Type, Value, OutError);
}

bool FBlueprintHelperReviewMaterialInstanceRestoreAdapter::ApplyParameterOverride(
	UMaterialInstanceConstant* Instance,
	const FMaterialParameterInfo& ParameterInfo,
	EBlueprintHelperMaterialInstanceParameterType Type,
	const FBlueprintHelperMaterialInstanceParameterValue& Value,
	FString& OutError)
{
	switch (Type)
	{
	case EBlueprintHelperMaterialInstanceParameterType::Scalar:
		Instance->SetScalarParameterValueEditorOnly(ParameterInfo, Value.Scalar);
		return true;
	case EBlueprintHelperMaterialInstanceParameterType::Vector:
		Instance->SetVectorParameterValueEditorOnly(ParameterInfo, Value.Vector);
		return true;
	case EBlueprintHelperMaterialInstanceParameterType::Texture:
	{
		UTexture* Texture = nullptr;
		if (!Value.TexturePath.IsEmpty())
		{
			Texture = LoadObject<UTexture>(
				nullptr,
				*FBlueprintHelperMaterialInstanceResolver::NormalizeMaterialInstanceObjectPath(Value.TexturePath));
			if (!Texture)
			{
				OutError = FString::Printf(TEXT("material_instance_restore_texture_not_found:%s"), *Value.TexturePath);
				return false;
			}
		}
		Instance->SetTextureParameterValueEditorOnly(ParameterInfo, Texture);
		return true;
	}
	case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
		Instance->SetStaticSwitchParameterValueEditorOnly(ParameterInfo, Value.bStaticSwitch);
		Instance->UpdateStaticPermutation();
		return true;
	case EBlueprintHelperMaterialInstanceParameterType::Unknown:
	default:
		OutError = TEXT("material_instance_restore_unknown_parameter_type");
		return false;
	}
}

bool FBlueprintHelperReviewMaterialInstanceRestoreAdapter::ClearParameterOverride(
	UMaterialInstanceConstant* Instance,
	const FMaterialParameterInfo& ParameterInfo,
	EBlueprintHelperMaterialInstanceParameterType Type,
	FString& OutError)
{
	switch (Type)
	{
	case EBlueprintHelperMaterialInstanceParameterType::Scalar:
		Instance->ScalarParameterValues.RemoveAll(
			[&ParameterInfo](const FScalarParameterValue& Entry)
			{
				return Entry.ParameterInfo == ParameterInfo;
			});
		return true;
	case EBlueprintHelperMaterialInstanceParameterType::Vector:
		Instance->VectorParameterValues.RemoveAll(
			[&ParameterInfo](const FVectorParameterValue& Entry)
			{
				return Entry.ParameterInfo == ParameterInfo;
			});
		return true;
	case EBlueprintHelperMaterialInstanceParameterType::Texture:
		Instance->TextureParameterValues.RemoveAll(
			[&ParameterInfo](const FTextureParameterValue& Entry)
			{
				return Entry.ParameterInfo == ParameterInfo;
			});
		return true;
	case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
	{
		FStaticParameterSet StaticParameters = Instance->GetStaticParameters();
		StaticParameters.StaticSwitchParameters.RemoveAll(
			[&ParameterInfo](const FStaticSwitchParameter& Entry)
			{
				return Entry.ParameterInfo == ParameterInfo;
			});
		Instance->UpdateStaticPermutation(StaticParameters);
		return true;
	}
	case EBlueprintHelperMaterialInstanceParameterType::Unknown:
	default:
		OutError = TEXT("material_instance_clear_unknown_parameter_type");
		return false;
	}
}
