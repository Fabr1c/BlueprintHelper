#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintClassSettings/BlueprintHelperClassDefaultMutationTypes.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassDefaultPropertyMutationResolver.h"

class FJsonValue;

struct BLUEPRINTHELPER_API FBlueprintHelperClassDefaultSetterMutationRequest
{
	FString AssetPath;
	UObject* RootObject = nullptr;
	FBlueprintHelperClassDefaultResolvedMutationTarget Target;
	TSharedPtr<FJsonValue> Value;
	bool bDryRun = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperClassDefaultSetterMutationResult
{
	bool bOk = false;
	bool bModified = false;
	bool bWouldChange = false;
	FString ErrorCode;
	FString ErrorMessage;
	FBlueprintHelperClassDefaultSetterMutationEvidence Evidence;
};

class BLUEPRINTHELPER_API FBlueprintHelperClassDefaultSetterMutationService
{
public:
	FBlueprintHelperClassDefaultSetterMutationResult Apply(
		const FBlueprintHelperClassDefaultSetterMutationRequest& Request) const;

private:
	static bool ConvertJsonValueToImportText(
		const TSharedPtr<FJsonValue>& Value,
		FString& OutText,
		FString& OutActualType,
		FString& OutError);

	static FProperty* FindSetterInput(UFunction* Function);
	static FProperty* FindGetterReturn(UFunction* Function);
	static FString SerializePropertyValue(const FProperty* Property, const void* ValuePtr, const UObject* OwnerObject);
	static bool BuildSetterParamBuffer(
		UFunction* SetterFunction,
		const TSharedPtr<FJsonValue>& Value,
		TArray<uint8>& OutBuffer,
		FString& OutExpectedValue,
		FString& OutErrorCode,
		FString& OutErrorMessage);
	static bool InvokeGetter(
		UObject* OwnerObject,
		UFunction* GetterFunction,
		FString& OutValue,
		FString& OutErrorCode,
		FString& OutErrorMessage);
};
