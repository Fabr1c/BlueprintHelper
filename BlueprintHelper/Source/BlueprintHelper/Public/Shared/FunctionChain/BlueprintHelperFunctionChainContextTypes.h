// BlueprintHelper Service Layer - FunctionChainContext read DTOs

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FBlueprintHelperFunctionChainContextProtocol
{
public:
	static constexpr const TCHAR* Schema = TEXT("FunctionChainContext.v1");
};

struct FBlueprintHelperFunctionChainContextRequest
{
	FString AssetPath;
	FString TargetType;
	FString TargetName;
	FString GraphName;
	int32 MaxDepth = 3;
	bool bIncludeDataDependencies = true;
	bool bExpandCrossAsset = true;
};

struct FBlueprintHelperFunctionChainLogicRef
{
	int32 Order = 0;
	int32 Depth = 0;
	int32 ParentOrder = 0;
	FString AssetPath;
	FString TargetType;
	FString TargetName;
	FString GraphName;
	FString CallKind;
	FString Reason;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperFunctionChainIssue
{
	FString Code;
	FString Message;
	FString AssetPath;
	FString GraphName;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperFunctionChainSummary
{
	int32 VisitedNodes = 0;
	int32 ReturnedCustomRefs = 0;
	int32 FilteredEngineOrTrustedPluginCalls = 0;
	int32 FilteredNativePureCalls = 0;
	int32 ProjectNativeTerminalCalls = 0;
	int32 UnresolvedCalls = 0;
	int32 AmbiguousCalls = 0;
	int32 CycleCount = 0;
	bool bTruncated = false;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperFunctionChainContextPack
{
	TArray<FBlueprintHelperFunctionChainLogicRef> CustomLogicRefs;
	FBlueprintHelperFunctionChainSummary Summary;
	TArray<FBlueprintHelperFunctionChainIssue> Unresolved;
	TArray<FBlueprintHelperFunctionChainIssue> Ambiguous;

	TSharedRef<FJsonObject> ToJson() const;
};
