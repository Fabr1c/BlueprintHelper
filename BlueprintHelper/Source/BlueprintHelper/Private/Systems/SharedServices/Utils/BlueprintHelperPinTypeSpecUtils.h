#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FParsedPinType;
struct FEdGraphPinType;

struct FBlueprintHelperPinTypeSpecError
{
	FString Code;
	FString FieldPath;
	FString Message;
};

class FBlueprintHelperPinTypeSpecUtils
{
public:
	static bool ReadParsedPinType(
		const TSharedPtr<FJsonObject>& PinTypeObject,
		FParsedPinType& OutParsedPinType,
		FBlueprintHelperPinTypeSpecError& OutError,
		const FString& FieldPath);

	static bool TryConvertPinTypeObject(
		const TSharedPtr<FJsonObject>& PinTypeObject,
		FEdGraphPinType& OutPinType,
		FString& OutError);

	static bool TryConvertPinTypeObject(
		const TSharedPtr<FJsonObject>& PinTypeObject,
		FEdGraphPinType& OutPinType,
		FBlueprintHelperPinTypeSpecError& OutError,
		const FString& FieldPath);
};
