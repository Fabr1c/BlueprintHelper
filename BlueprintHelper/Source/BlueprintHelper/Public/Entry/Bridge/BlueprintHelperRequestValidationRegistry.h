#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EBlueprintHelperRequestFieldType : uint8
{
	String,
	Bool,
	Number,
	Object,
	Array
};

struct BLUEPRINTHELPER_API FBlueprintHelperFieldRule
{
	FString FieldName;
	EBlueprintHelperRequestFieldType Type = EBlueprintHelperRequestFieldType::String;
};

struct BLUEPRINTHELPER_API FBlueprintHelperRequestValidationDescriptor
{
	FString Command;
	TArray<FBlueprintHelperFieldRule> RequiredFields;
	TArray<FBlueprintHelperFieldRule> OptionalFields;
	TFunction<bool(const TSharedPtr<FJsonObject>&, FString&)> CustomValidator;
};

class BLUEPRINTHELPER_API FBlueprintHelperRequestValidationRegistry
{
public:
	static bool TryFindDescriptor(
		const FString& Command,
		FBlueprintHelperRequestValidationDescriptor& OutDescriptor);
	static TArray<FBlueprintHelperRequestValidationDescriptor> GetRepresentativeDescriptors();
};
