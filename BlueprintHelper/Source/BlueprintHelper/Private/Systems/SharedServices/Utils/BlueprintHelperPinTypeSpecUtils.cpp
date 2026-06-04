#include "Systems/SharedServices/Utils/BlueprintHelperPinTypeSpecUtils.h"

#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h"

bool FBlueprintHelperPinTypeSpecUtils::ReadParsedPinType(
	const TSharedPtr<FJsonObject>& PinTypeObject,
	FParsedPinType& OutParsedPinType,
	FBlueprintHelperPinTypeSpecError& OutError,
	const FString& FieldPath)
{
	OutParsedPinType = FParsedPinType();
	OutError = FBlueprintHelperPinTypeSpecError();

	if (!PinTypeObject.IsValid())
	{
		OutError.Code = TEXT("invalid_pin_type");
		OutError.FieldPath = FieldPath;
		OutError.Message = TEXT("pin_type must be an object.");
		return false;
	}

	PinTypeObject->TryGetStringField(TEXT("category"), OutParsedPinType.Category);
	PinTypeObject->TryGetStringField(TEXT("sub_category"), OutParsedPinType.SubCategory);
	PinTypeObject->TryGetStringField(TEXT("object_path"), OutParsedPinType.SubCategoryObjectPath);
	PinTypeObject->TryGetStringField(TEXT("container_type"), OutParsedPinType.ContainerType);
	PinTypeObject->TryGetBoolField(TEXT("is_reference"), OutParsedPinType.bIsReference);
	PinTypeObject->TryGetBoolField(TEXT("is_const"), OutParsedPinType.bIsConst);

	const TSharedPtr<FJsonObject>* ValueTypeObject = nullptr;
	if (PinTypeObject->TryGetObjectField(TEXT("value_type"), ValueTypeObject) && ValueTypeObject && ValueTypeObject->IsValid())
	{
		OutParsedPinType.ValueType = MakeShared<FParsedPinType>();
		return ReadParsedPinType(*ValueTypeObject, *OutParsedPinType.ValueType, OutError, FieldPath + TEXT(".value_type"));
	}

	FString ValueTypeCategory;
	if (PinTypeObject->TryGetStringField(TEXT("value_type"), ValueTypeCategory) && !ValueTypeCategory.IsEmpty())
	{
		OutError.Code = TEXT("legacy_pin_type_token_unsupported");
		OutError.FieldPath = FieldPath + TEXT(".value_type");
		OutError.Message = TEXT("pin_type.value_type must be a structured object.");
		return false;
	}

	return true;
}

bool FBlueprintHelperPinTypeSpecUtils::TryConvertPinTypeObject(
	const TSharedPtr<FJsonObject>& PinTypeObject,
	FEdGraphPinType& OutPinType,
	FString& OutError)
{
	FBlueprintHelperPinTypeSpecError Error;
	if (!TryConvertPinTypeObject(PinTypeObject, OutPinType, Error, TEXT("pin_type")))
	{
		OutError = Error.Message;
		return false;
	}

	OutError.Reset();
	return true;
}

bool FBlueprintHelperPinTypeSpecUtils::TryConvertPinTypeObject(
	const TSharedPtr<FJsonObject>& PinTypeObject,
	FEdGraphPinType& OutPinType,
	FBlueprintHelperPinTypeSpecError& OutError,
	const FString& FieldPath)
{
	FParsedPinType ParsedPinType;
	if (!ReadParsedPinType(PinTypeObject, ParsedPinType, OutError, FieldPath))
	{
		return false;
	}

	FString ConvertError;
	if (!FBlueprintGraphLocalVariableService::ConvertToEdGraphPinType(ParsedPinType, OutPinType, ConvertError))
	{
		OutError.Code = ConvertError.Contains(TEXT("load")) ? TEXT("pin_type_object_not_found") : TEXT("invalid_pin_type");
		OutError.FieldPath = FieldPath;
		OutError.Message = ConvertError.IsEmpty() ? TEXT("Unable to convert pin_type.") : ConvertError;
		return false;
	}

	OutError = FBlueprintHelperPinTypeSpecError();
	return true;
}
