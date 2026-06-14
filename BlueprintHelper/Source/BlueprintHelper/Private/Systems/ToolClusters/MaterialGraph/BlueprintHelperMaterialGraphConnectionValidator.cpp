// BlueprintHelper MaterialGraph connection validator.

#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphConnectionValidator.h"

class FBlueprintHelperMaterialGraphConnectionValidatorPrivate
{
public:
	static FBlueprintHelperMaterialGraphValidationResult MakeInvalid(
		const FString& Code,
		const FString& Message,
		const FString& FieldPath)
	{
		FBlueprintHelperMaterialGraphValidationResult Result;
		Result.bValid = false;
		Result.ErrorCode = Code;
		Result.ErrorMessage = Message;
		Result.FieldPath = FieldPath;
		return Result;
	}

	static bool ReadEndpoint(
		const TSharedPtr<FJsonObject>& LinkObject,
		const TCHAR* FieldName,
		FString& OutNodeKey,
		FString& OutPin)
	{
		const TSharedPtr<FJsonObject>* EndpointPtr = nullptr;
		if (!LinkObject.IsValid() ||
			!LinkObject->TryGetObjectField(FieldName, EndpointPtr) ||
			!EndpointPtr ||
			!EndpointPtr->IsValid())
		{
			return false;
		}
		return (*EndpointPtr)->TryGetStringField(TEXT("node_key"), OutNodeKey) &&
			(*EndpointPtr)->TryGetStringField(TEXT("pin"), OutPin) &&
			!OutNodeKey.IsEmpty() &&
			!OutPin.IsEmpty();
	}

};

FBlueprintHelperMaterialGraphValidationResult FBlueprintHelperMaterialGraphConnectionValidator::ValidateLink(
	const TSharedPtr<FJsonObject>& LinkObject,
	const FString& FieldPath)
{
	FBlueprintHelperMaterialGraphValidationResult Result;
	if (!LinkObject.IsValid())
	{
		return FBlueprintHelperMaterialGraphConnectionValidatorPrivate::MakeInvalid(
			TEXT("material_connection_schema_invalid"),
			TEXT("Material graph link must be an object."),
			FieldPath);
	}

	FString FromNodeKey;
	FString FromPin;
	FString ToNodeKey;
	FString ToPin;
	if (!FBlueprintHelperMaterialGraphConnectionValidatorPrivate::ReadEndpoint(
		LinkObject,
		TEXT("from"),
		FromNodeKey,
		FromPin) ||
		!FBlueprintHelperMaterialGraphConnectionValidatorPrivate::ReadEndpoint(
			LinkObject,
			TEXT("to"),
			ToNodeKey,
			ToPin))
	{
		return FBlueprintHelperMaterialGraphConnectionValidatorPrivate::MakeInvalid(
			TEXT("material_connection_schema_invalid"),
			TEXT("Material graph links require from/to endpoints with node_key and pin."),
			FieldPath);
	}

	if (FromNodeKey == TEXT("$material_output"))
	{
		return FBlueprintHelperMaterialGraphConnectionValidatorPrivate::MakeInvalid(
			TEXT("material_connection_schema_invalid"),
			TEXT("$material_output can only be used as a material link target."),
			FieldPath);
	}

	if (ToNodeKey == TEXT("$material_output") && !IsSupportedMaterialOutputProperty(ToPin))
	{
		return FBlueprintHelperMaterialGraphConnectionValidatorPrivate::MakeInvalid(
			TEXT("material_property_not_supported"),
			FString::Printf(TEXT("Unsupported material output property: %s."), *ToPin),
			FieldPath);
	}

	Result.bValid = true;
	return Result;
}

bool FBlueprintHelperMaterialGraphConnectionValidator::IsSupportedMaterialOutputProperty(const FString& PinName)
{
	return PinName == TEXT("BaseColor") ||
		PinName == TEXT("Metallic") ||
		PinName == TEXT("Specular") ||
		PinName == TEXT("Roughness") ||
		PinName == TEXT("EmissiveColor") ||
		PinName == TEXT("Opacity") ||
		PinName == TEXT("OpacityMask") ||
		PinName == TEXT("Normal") ||
		PinName == TEXT("WorldPositionOffset");
}
