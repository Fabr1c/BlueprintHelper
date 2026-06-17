// BlueprintHelper MaterialInstance resolver.

#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"

#include "Engine/Texture.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

FString BlueprintHelperMaterialInstanceParameterTypeToString(
	EBlueprintHelperMaterialInstanceParameterType Type)
{
	switch (Type)
	{
	case EBlueprintHelperMaterialInstanceParameterType::Scalar:
		return TEXT("scalar");
	case EBlueprintHelperMaterialInstanceParameterType::Vector:
		return TEXT("vector");
	case EBlueprintHelperMaterialInstanceParameterType::Texture:
		return TEXT("texture");
	case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
		return TEXT("static_switch");
	case EBlueprintHelperMaterialInstanceParameterType::Unknown:
	default:
		return TEXT("unknown");
	}
}

FString BlueprintHelperMaterialInstanceParameterSourceToString(
	EBlueprintHelperMaterialInstanceParameterSource Source)
{
	switch (Source)
	{
	case EBlueprintHelperMaterialInstanceParameterSource::Inherited:
		return TEXT("inherited");
	case EBlueprintHelperMaterialInstanceParameterSource::Override:
		return TEXT("override");
	case EBlueprintHelperMaterialInstanceParameterSource::None:
	default:
		return TEXT("none");
	}
}

FString FBlueprintHelperMaterialInstanceParameterValue::ToDebugString(
	EBlueprintHelperMaterialInstanceParameterType Type) const
{
	if (!bHasValue)
	{
		return TEXT("<unset>");
	}

	switch (Type)
	{
	case EBlueprintHelperMaterialInstanceParameterType::Scalar:
		return FString::SanitizeFloat(Scalar);
	case EBlueprintHelperMaterialInstanceParameterType::Vector:
		return Vector.ToString();
	case EBlueprintHelperMaterialInstanceParameterType::Texture:
		return TexturePath;
	case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
		return bStaticSwitch ? TEXT("true") : TEXT("false");
	case EBlueprintHelperMaterialInstanceParameterType::Unknown:
	default:
		return TEXT("<unknown>");
	}
}

class FBlueprintHelperMaterialInstanceResolverLocalUtils
{
public:
	static bool ReadParameterValue(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& ParameterInfo,
		EBlueprintHelperMaterialInstanceParameterType Type,
		bool bOverriddenOnly,
		FBlueprintHelperMaterialInstanceParameterValue& OutValue)
	{
		if (!Instance)
		{
			return false;
		}

		const FHashedMaterialParameterInfo HashedInfo(ParameterInfo);
		OutValue = FBlueprintHelperMaterialInstanceParameterValue();
		switch (Type)
		{
		case EBlueprintHelperMaterialInstanceParameterType::Scalar:
		{
			float ScalarValue = 0.0f;
			if (!Instance->GetScalarParameterValue(HashedInfo, ScalarValue, bOverriddenOnly))
			{
				return false;
			}
			OutValue.bHasValue = true;
			OutValue.Scalar = ScalarValue;
			return true;
		}
		case EBlueprintHelperMaterialInstanceParameterType::Vector:
		{
			FLinearColor VectorValue = FLinearColor::Transparent;
			if (!Instance->GetVectorParameterValue(HashedInfo, VectorValue, bOverriddenOnly))
			{
				return false;
			}
			OutValue.bHasValue = true;
			OutValue.Vector = VectorValue;
			return true;
		}
		case EBlueprintHelperMaterialInstanceParameterType::Texture:
		{
			UTexture* TextureValue = nullptr;
			if (!Instance->GetTextureParameterValue(HashedInfo, TextureValue, bOverriddenOnly))
			{
				return false;
			}
			OutValue.bHasValue = true;
			OutValue.Texture = TextureValue;
			OutValue.TexturePath = TextureValue ? TextureValue->GetPathName() : FString();
			return true;
		}
		case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
		{
			bool bSwitchValue = false;
			FGuid ExpressionGuid;
			if (!Instance->GetStaticSwitchParameterValue(
					HashedInfo,
					bSwitchValue,
					ExpressionGuid,
					bOverriddenOnly))
			{
				return false;
			}
			OutValue.bHasValue = true;
			OutValue.bStaticSwitch = bSwitchValue;
			OutValue.StaticSwitchExpressionGuid = ExpressionGuid;
			return true;
		}
		case EBlueprintHelperMaterialInstanceParameterType::Unknown:
		default:
			return false;
		}
	}

	static void AppendParameterInfo(
		UMaterialInstanceConstant* Instance,
		const TArray<FMaterialParameterInfo>& ParameterInfos,
		const TArray<FGuid>& ParameterIds,
		EBlueprintHelperMaterialInstanceParameterType Type,
		TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry>& OutSchema)
	{
		for (int32 Index = 0; Index < ParameterInfos.Num(); ++Index)
		{
			FBlueprintHelperMaterialInstanceParameterSchemaEntry Entry;
			Entry.ParameterInfo = ParameterInfos[Index];
			Entry.ParameterId = ParameterIds.IsValidIndex(Index) ? ParameterIds[Index] : FGuid();
			Entry.Type = Type;
			Entry.bHasOverride = ReadParameterValue(
				Instance,
				Entry.ParameterInfo,
				Type,
				/*bOverriddenOnly=*/ true,
				Entry.OverrideValue);
			const bool bHasEffectiveValue = ReadParameterValue(
				Instance,
				Entry.ParameterInfo,
				Type,
				/*bOverriddenOnly=*/ false,
				Entry.EffectiveValue);
			Entry.Source = Entry.bHasOverride
				? EBlueprintHelperMaterialInstanceParameterSource::Override
				: (bHasEffectiveValue
					? EBlueprintHelperMaterialInstanceParameterSource::Inherited
					: EBlueprintHelperMaterialInstanceParameterSource::None);
			OutSchema.Add(Entry);
		}
	}

	static void CollectTypedParameterInfo(
		UMaterialInstanceConstant* Instance,
		EBlueprintHelperMaterialInstanceParameterType Type,
		TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry>& OutSchema)
	{
		if (!Instance)
		{
			return;
		}

		TArray<FMaterialParameterInfo> ParameterInfos;
		TArray<FGuid> ParameterIds;
		switch (Type)
		{
		case EBlueprintHelperMaterialInstanceParameterType::Scalar:
			Instance->GetAllScalarParameterInfo(ParameterInfos, ParameterIds);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Vector:
			Instance->GetAllVectorParameterInfo(ParameterInfos, ParameterIds);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Texture:
			Instance->GetAllTextureParameterInfo(ParameterInfos, ParameterIds);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
			Instance->GetAllStaticSwitchParameterInfo(ParameterInfos, ParameterIds);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Unknown:
		default:
			return;
		}
		AppendParameterInfo(Instance, ParameterInfos, ParameterIds, Type, OutSchema);
	}
};

FString FBlueprintHelperMaterialInstanceResolver::NormalizeMaterialInstanceObjectPath(
	const FString& AssetPath)
{
	const FString TrimmedPath = AssetPath.TrimStartAndEnd();
	if (TrimmedPath.IsEmpty() || TrimmedPath.Contains(TEXT(".")))
	{
		return TrimmedPath;
	}

	if (!TrimmedPath.StartsWith(TEXT("/")))
	{
		return TrimmedPath;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(TrimmedPath);
	if (AssetName.IsEmpty())
	{
		return TrimmedPath;
	}

	return FString::Printf(TEXT("%s.%s"), *TrimmedPath, *AssetName);
}

FBlueprintHelperMaterialInstanceAssetResolveResult FBlueprintHelperMaterialInstanceResolver::ResolveAsset(
	const FString& AssetPath)
{
	FBlueprintHelperMaterialInstanceAssetResolveResult Result;
	Result.InputAssetPath = AssetPath;
	Result.ObjectPath = NormalizeMaterialInstanceObjectPath(AssetPath);
	if (Result.ObjectPath.IsEmpty())
	{
		Result.ErrorCode = TEXT("material_instance_asset_path_empty");
		Result.ErrorMessage = TEXT("MaterialInstance asset path is empty.");
		return Result;
	}

	UObject* LoadedObject = LoadObject<UObject>(nullptr, *Result.ObjectPath);
	if (!LoadedObject)
	{
		Result.ErrorCode = TEXT("material_instance_asset_not_found");
		Result.ErrorMessage = FString::Printf(
			TEXT("MaterialInstance asset was not found: %s."),
			*Result.ObjectPath);
		return Result;
	}

	Result.Instance = Cast<UMaterialInstanceConstant>(LoadedObject);
	if (!Result.Instance)
	{
		Result.ErrorCode = TEXT("material_instance_asset_type_mismatch");
		Result.ErrorMessage = FString::Printf(
			TEXT("Asset is not a UMaterialInstanceConstant: %s."),
			*Result.ObjectPath);
		return Result;
	}

	Result.Parent = Result.Instance->Parent;
	Result.bHasParent = Result.Parent != nullptr;
	Result.bSuccess = true;
	return Result;
}

bool FBlueprintHelperMaterialInstanceResolver::CollectParameterSchema(
	UMaterialInstanceConstant* Instance,
	TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry>& OutSchema,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	OutSchema.Reset();
	OutErrorCode.Reset();
	OutErrorMessage.Reset();

	if (!Instance)
	{
		OutErrorCode = TEXT("material_instance_asset_invalid");
		OutErrorMessage = TEXT("MaterialInstance asset is invalid.");
		return false;
	}

	if (!Instance->Parent)
	{
		OutErrorCode = TEXT("material_instance_missing_parent");
		OutErrorMessage = FString::Printf(
			TEXT("MaterialInstance has no parent material: %s."),
			*Instance->GetPathName());
		return false;
	}

	FBlueprintHelperMaterialInstanceResolverLocalUtils::CollectTypedParameterInfo(
		Instance,
		EBlueprintHelperMaterialInstanceParameterType::Scalar,
		OutSchema);
	FBlueprintHelperMaterialInstanceResolverLocalUtils::CollectTypedParameterInfo(
		Instance,
		EBlueprintHelperMaterialInstanceParameterType::Vector,
		OutSchema);
	FBlueprintHelperMaterialInstanceResolverLocalUtils::CollectTypedParameterInfo(
		Instance,
		EBlueprintHelperMaterialInstanceParameterType::Texture,
		OutSchema);
	FBlueprintHelperMaterialInstanceResolverLocalUtils::CollectTypedParameterInfo(
		Instance,
		EBlueprintHelperMaterialInstanceParameterType::StaticSwitch,
		OutSchema);
	return true;
}

FBlueprintHelperMaterialInstanceParameterResolveResult FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
	UMaterialInstanceConstant* Instance,
	const FName ParameterName,
	EBlueprintHelperMaterialInstanceParameterType RequestedType)
{
	FBlueprintHelperMaterialInstanceParameterResolveResult Result;
	if (ParameterName.IsNone())
	{
		Result.ErrorCode = TEXT("material_instance_parameter_name_empty");
		Result.ErrorMessage = TEXT("MaterialInstance parameter name is empty.");
		return Result;
	}

	TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry> Schema;
	if (!CollectParameterSchema(Instance, Schema, Result.ErrorCode, Result.ErrorMessage))
	{
		return Result;
	}

	TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry> Matches;
	for (const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Entry : Schema)
	{
		if (!Entry.ParameterInfo.Name.IsEqual(ParameterName))
		{
			continue;
		}
		if (RequestedType != EBlueprintHelperMaterialInstanceParameterType::Unknown &&
			Entry.Type != RequestedType)
		{
			continue;
		}
		Matches.Add(Entry);
	}

	if (Matches.IsEmpty())
	{
		Result.ErrorCode = TEXT("material_instance_parameter_not_found");
		Result.ErrorMessage = FString::Printf(
			TEXT("MaterialInstance parameter was not found: %s."),
			*ParameterName.ToString());
		return Result;
	}

	if (RequestedType == EBlueprintHelperMaterialInstanceParameterType::Unknown && Matches.Num() > 1)
	{
		TArray<FString> MatchTypes;
		for (const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Match : Matches)
		{
			MatchTypes.Add(BlueprintHelperMaterialInstanceParameterTypeToString(Match.Type));
		}
		Result.ErrorCode = TEXT("material_instance_parameter_ambiguous");
		Result.ErrorMessage = FString::Printf(
			TEXT("MaterialInstance parameter name is ambiguous: %s (%s)."),
			*ParameterName.ToString(),
			*FString::Join(MatchTypes, TEXT(", ")));
		return Result;
	}

	Result.Parameter = Matches[0];
	Result.bSuccess = true;
	return Result;
}

bool FBlueprintHelperMaterialInstanceResolver::TryParseParameterType(
	const FString& Input,
	EBlueprintHelperMaterialInstanceParameterType& OutType)
{
	const FString Normalized = Input.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("scalar") || Normalized == TEXT("float"))
	{
		OutType = EBlueprintHelperMaterialInstanceParameterType::Scalar;
		return true;
	}
	if (Normalized == TEXT("vector") || Normalized == TEXT("color"))
	{
		OutType = EBlueprintHelperMaterialInstanceParameterType::Vector;
		return true;
	}
	if (Normalized == TEXT("texture"))
	{
		OutType = EBlueprintHelperMaterialInstanceParameterType::Texture;
		return true;
	}
	if (Normalized == TEXT("static_switch") || Normalized == TEXT("static-switch") ||
		Normalized == TEXT("switch") || Normalized == TEXT("bool"))
	{
		OutType = EBlueprintHelperMaterialInstanceParameterType::StaticSwitch;
		return true;
	}

	OutType = EBlueprintHelperMaterialInstanceParameterType::Unknown;
	return Normalized == TEXT("unknown") || Normalized.IsEmpty();
}
