// BlueprintHelper MaterialInstance resolver types.

#pragma once

#include "CoreMinimal.h"
#include "MaterialTypes.h"

class UMaterialInstanceConstant;
class UMaterialInterface;
class UTexture;

enum class EBlueprintHelperMaterialInstanceParameterType : uint8
{
	Unknown,
	Scalar,
	Vector,
	Texture,
	StaticSwitch
};

enum class EBlueprintHelperMaterialInstanceParameterSource : uint8
{
	None,
	Inherited,
	Override
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceParameterValue
{
	bool bHasValue = false;
	float Scalar = 0.0f;
	FLinearColor Vector = FLinearColor::Transparent;
	TWeakObjectPtr<UTexture> Texture;
	FString TexturePath;
	bool bStaticSwitch = false;
	FGuid StaticSwitchExpressionGuid;

	FString ToDebugString(EBlueprintHelperMaterialInstanceParameterType Type) const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceParameterSchemaEntry
{
	FMaterialParameterInfo ParameterInfo;
	FGuid ParameterId;
	EBlueprintHelperMaterialInstanceParameterType Type =
		EBlueprintHelperMaterialInstanceParameterType::Unknown;
	EBlueprintHelperMaterialInstanceParameterSource Source =
		EBlueprintHelperMaterialInstanceParameterSource::None;
	bool bHasOverride = false;
	FBlueprintHelperMaterialInstanceParameterValue EffectiveValue;
	FBlueprintHelperMaterialInstanceParameterValue OverrideValue;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceAssetResolveResult
{
	bool bSuccess = false;
	bool bHasParent = false;
	FString ErrorCode;
	FString ErrorMessage;
	FString InputAssetPath;
	FString ObjectPath;
	UMaterialInstanceConstant* Instance = nullptr;
	UMaterialInterface* Parent = nullptr;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceParameterResolveResult
{
	bool bSuccess = false;
	FString ErrorCode;
	FString ErrorMessage;
	FBlueprintHelperMaterialInstanceParameterSchemaEntry Parameter;
};

BLUEPRINTHELPER_API FString BlueprintHelperMaterialInstanceParameterTypeToString(
	EBlueprintHelperMaterialInstanceParameterType Type);

BLUEPRINTHELPER_API FString BlueprintHelperMaterialInstanceParameterSourceToString(
	EBlueprintHelperMaterialInstanceParameterSource Source);
