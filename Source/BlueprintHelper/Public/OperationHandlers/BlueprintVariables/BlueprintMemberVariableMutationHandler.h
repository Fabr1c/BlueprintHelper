// BlueprintHelper v2.0 - Member Variable mutation operation handler

#pragma once

#include "OperationHandlers/BlueprintOperationHandler.h"

class FJsonObject;
class FJsonValue;

struct FBlueprintHelperMemberDefaultMutation
{
	FString VariableName;
	FString DefaultValue;
};

struct FBlueprintHelperMemberPropertyMutation
{
	FString PropertyPath;
	TSharedPtr<FJsonValue> Value;
};

struct FBlueprintHelperVariableMutationCounts
{
	int32 RequestedCount = 0;
	int32 ChangedCount = 0;
	int32 NoOpCount = 0;
};

class FBlueprintHelperMemberVariableMutationHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError) override;

	static bool TryReadVariableName(const TSharedPtr<FJsonObject>& Payload, FString& OutVariableName);
	static bool TryReadDefaultValue(const TSharedPtr<FJsonObject>& Payload, FString& OutDefaultValue);
	static bool TryReadPropertySetting(
		const TSharedPtr<FJsonObject>& SettingObject,
		FBlueprintHelperMemberPropertyMutation& OutSetting);
	static bool TryScalarJsonToBlueprintDefaultString(
		const TSharedPtr<FJsonValue>& Value,
		FString& OutDefaultValue);

	static bool ApplyDefaultChanges(
		UBlueprint* Blueprint,
		const TArray<FBlueprintHelperMemberDefaultMutation>& Changes,
		FBlueprintHelperVariableMutationCounts& OutCounts,
		FString& OutError,
		FString* OutField = nullptr);

	static bool ApplyPropertySettings(
		UBlueprint* Blueprint,
		const FString& VariableName,
		const TArray<FBlueprintHelperMemberPropertyMutation>& Settings,
		FBlueprintHelperVariableMutationCounts& OutCounts,
		FString& OutError,
		FString* OutField = nullptr);
};
