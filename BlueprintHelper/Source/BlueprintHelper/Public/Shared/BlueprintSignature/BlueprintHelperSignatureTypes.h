// BlueprintHelper Service Layer - Blueprint signature capability DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

struct BLUEPRINTHELPER_API FBlueprintHelperEnsureFunctionSignatureRequest
{
	FString AssetPath;
	FString FunctionName;
	FString InterfacePath;
	FString InterfaceEntryKind = TEXT("function");
	FString NameCollisionPolicy = TEXT("reuse_if_exists");
	FString SignatureMismatchPolicy = TEXT("block");
	TArray<TSharedPtr<FJsonValue>> Inputs;
	TArray<TSharedPtr<FJsonValue>> Outputs;
	bool bDryRun = false;
	bool bIsPure = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperEnsureCustomEventSignatureRequest
{
	FString AssetPath;
	FString GraphName;
	FString EventName;
	FString InterfacePath;
	FString InterfaceEntryKind;
	FString NameCollisionPolicy = TEXT("reuse_if_exists");
	TArray<TSharedPtr<FJsonValue>> Inputs;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperEnsureMacroSignatureRequest
{
	FString AssetPath;
	FString MacroName;
	FString NameCollisionPolicy = TEXT("reuse_if_exists");
	TArray<TSharedPtr<FJsonValue>> Inputs;
	TArray<TSharedPtr<FJsonValue>> Outputs;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperRemoveSignatureRequest
{
	FString AssetPath;
	FString GraphName;
	FString SignatureName;
	FString SignatureKind;
	FString ExecutePolicy = TEXT("blocked_preflight");
	bool bDryRun = false;
	bool bRequireReferenceContext = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperEnsureEventDispatcherSignatureRequest
{
	FString AssetPath;
	FString DispatcherName;
	FString NameCollisionPolicy = TEXT("reuse_if_exists");
	FString SignatureMismatchPolicy = TEXT("block");
	TArray<TSharedPtr<FJsonValue>> Inputs;
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperEnsureOverrideEventSignatureRequest
{
	FString AssetPath;
	FString GraphName;
	FString EventName;
	FString EventKind = TEXT("native_event");
	FString ExecutePolicy = TEXT("blocked_preflight");
	TArray<TSharedPtr<FJsonValue>> Inputs;
	bool bDryRun = false;
};
