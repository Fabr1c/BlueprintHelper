// BlueprintHelper local variable mutation operation helper.

#pragma once

#include "CoreMinimal.h"
#include "OperationHandlers/BlueprintOperationHandler.h"
#include "Structure/BlueprintVariables/BlueprintHelperBlueprintVariableTypes.h"

class FJsonObject;
class FJsonValue;
class UBlueprint;
class UEdGraph;
class UStruct;
struct FEdGraphPinType;

struct FBlueprintHelperLocalVariableAddRequest
{
	FString VariableName;
	FBlueprintHelperVariableType VariableType;
	TOptional<FString> DefaultValue;
	EBlueprintHelperVariableNameCollisionPolicy NameCollisionPolicy =
		EBlueprintHelperVariableNameCollisionPolicy::FailIfExists;
	TOptional<FString> Category;
	TOptional<FString> Tooltip;
	TOptional<FString> Description;
};

struct FBlueprintHelperLocalVariablePropertyMutation
{
	FString PropertyPath;
	TSharedPtr<FJsonValue> Value;
};

struct FBlueprintHelperLocalVariableRemoveRequest
{
	FString VariableName;
	bool bDryRun = false;
};

struct FBlueprintHelperLocalVariableMutationCounts
{
	int32 RequestedCount = 0;
	int32 AddedCount = 0;
	int32 RemovedCount = 0;
	int32 ChangedCount = 0;
	int32 NoOpCount = 0;
	int32 ReferenceCount = 0;
};

class FBlueprintHelperLocalVariableMutationHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError) override;

	static bool TryReadFunctionName(const TSharedPtr<FJsonObject>& Payload, FString& OutFunctionName);
	static bool TryReadVariableName(const TSharedPtr<FJsonObject>& Payload, FString& OutVariableName);
	static bool TryReadDefaultValue(
		const TSharedPtr<FJsonObject>& Payload,
		TOptional<FString>& OutDefaultValue,
		FString& OutError,
		FString* OutField = nullptr);
	static bool TryReadPropertySetting(
		const TSharedPtr<FJsonObject>& SettingObject,
		FBlueprintHelperLocalVariablePropertyMutation& OutSetting);
	static bool TryReadNameCollisionPolicy(
		const TSharedPtr<FJsonObject>& Payload,
		EBlueprintHelperVariableNameCollisionPolicy& OutPolicy,
		FString& OutError,
		FString* OutField = nullptr);
	static bool TryReadVariableType(
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperVariableType& OutVariableType,
		FString& OutError,
		FString* OutField = nullptr);
	static bool TryReadAddRequest(
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperLocalVariableAddRequest& OutRequest,
		FString& OutError,
		FString* OutField = nullptr);
	static bool TryReadPropertySettings(
		const TSharedPtr<FJsonObject>& Payload,
		TArray<FBlueprintHelperLocalVariablePropertyMutation>& OutSettings,
		FString& OutError,
		FString* OutField = nullptr);
	static bool TryReadRemoveRequest(
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperLocalVariableRemoveRequest& OutRequest,
		FString& OutError,
		FString* OutField = nullptr);

	static bool TryScalarJsonToBlueprintDefaultString(
		const TSharedPtr<FJsonValue>& Value,
		FString& OutDefaultValue);
	static bool TryBuildPinType(
		const FBlueprintHelperVariableType& VariableType,
		FEdGraphPinType& OutPinType,
		FString& OutError);
	static FBlueprintHelperVariableType ConvertPinTypeToVariableType(const FEdGraphPinType& PinType);

	static bool ResolveFunctionGraph(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		UEdGraph*& OutFunctionGraph,
		FString& OutError,
		FString* OutField = nullptr);
	static bool ResolveFunctionScope(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		UStruct*& OutScope,
		FString& OutError,
		FString* OutField = nullptr);
	static bool ReadLocalVariables(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		TArray<FBlueprintHelperLocalVariableItem>& OutLocalVariables,
		FString& OutError,
		FString* OutField = nullptr);

	static bool ApplyAddLocalVariables(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		const TArray<FBlueprintHelperLocalVariableAddRequest>& Requests,
		FBlueprintHelperLocalVariableMutationCounts& OutCounts,
		FString& OutError,
		FString* OutField = nullptr);
	static bool ApplyPropertySettings(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		const FString& VariableName,
		const TArray<FBlueprintHelperLocalVariablePropertyMutation>& Settings,
		FBlueprintHelperLocalVariableMutationCounts& OutCounts,
		FString& OutError,
		FString* OutField = nullptr);
	static bool ApplyRemoveLocalVariables(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		const TArray<FBlueprintHelperLocalVariableRemoveRequest>& Requests,
		FBlueprintHelperLocalVariableMutationCounts& OutCounts,
		FString& OutError,
		FString* OutField = nullptr);
	static int32 CountLocalVariableReferences(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		const FString& VariableName);
};
