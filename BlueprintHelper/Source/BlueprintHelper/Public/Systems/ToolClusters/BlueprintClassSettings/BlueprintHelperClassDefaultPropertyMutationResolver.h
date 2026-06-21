#pragma once

#include "CoreMinimal.h"

class FProperty;
class UObject;
class UFunction;

struct BLUEPRINTHELPER_API FBlueprintHelperClassDefaultResolvedMutationTarget
{
	UObject* RootObject = nullptr;
	UObject* OwnerObject = nullptr;
	FProperty* LeafProperty = nullptr;
	void* LeafValuePtr = nullptr;
	FString OwnerRoot = TEXT("blueprint_cdo");
	FString OwnerObjectPath;
	FString OwnerObjectClass;
	FString PropertyPath;
	FString LeafPropertyName;
	FString ExpectedType;
	FString PropertyFlags;
	FString SetterFunctionName;
	FString GetterFunctionName;
	UFunction* SetterFunction = nullptr;
	UFunction* GetterFunction = nullptr;
};

class BLUEPRINTHELPER_API FBlueprintHelperClassDefaultPropertyMutationResolver
{
public:
	bool Resolve(
		UObject* RootObject,
		const FString& PropertyPath,
		FBlueprintHelperClassDefaultResolvedMutationTarget& OutTarget,
		FString& OutErrorCode,
		FString& OutErrorMessage) const;

private:
	static FString ResolveMetadataName(const FProperty* Property, const TCHAR* PrimaryKey, const TCHAR* SecondaryKey);
	static FString BuildOwnerObjectClass(const UObject* OwnerObject);
};
