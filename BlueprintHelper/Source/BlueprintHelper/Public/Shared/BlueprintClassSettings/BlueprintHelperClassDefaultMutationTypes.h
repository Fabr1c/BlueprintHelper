#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Shared/BlueprintHelperServiceTypes.h"

enum class EBlueprintHelperClassDefaultMutationStrategy : uint8
{
	DirectProperty,
	SetterAwareProperty,
	Blocked
};

BLUEPRINTHELPER_API const TCHAR* ToString(EBlueprintHelperClassDefaultMutationStrategy Strategy);
BLUEPRINTHELPER_API EBlueprintHelperClassDefaultMutationStrategy BlueprintHelperClassDefaultMutationStrategyFromString(
	const FString& Strategy);

struct BLUEPRINTHELPER_API FBlueprintHelperClassDefaultSetterMutationEvidence
{
	FString Schema = TEXT("BlueprintHelper.ClassDefaultSetterMutationEvidence.v1");
	FString AssetPath;
	FString OwnerRoot = TEXT("blueprint_cdo");
	FString OwnerObjectPath;
	FString OwnerObjectClass;
	FString PropertyPath;
	FString LeafPropertyName;
	FString MutationStrategy = TEXT("setter_aware_property");
	FString SetterFunction;
	FString GetterFunction;
	FString ExpectedType;
	FString InputValue;
	FString BeforeValue;
	FString AfterValue;
	FString PropertyFlags;
	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;

	TSharedRef<FJsonObject> ToJson() const;
	static bool FromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperClassDefaultSetterMutationEvidence& OutEvidence);
};
