#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;

struct BLUEPRINTHELPER_API FBlueprintHelperAssetFactoryToolClusterPolicy
{
	FString DefaultParentClass;
	FString DefaultValueType;
	FString DefaultCollisionPolicy = TEXT("fail_if_exists");
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperComponentToolClusterPolicy
{
	FString DefaultAttachRule = TEXT("keep_relative");
	FString DefaultNameCollisionPolicy = TEXT("fail_if_exists");
	FString DefaultPropertyMode = TEXT("batch");
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperClassSettingsToolClusterPolicy
{
	bool bDryRun = false;
	bool bValidationShouldCompile = false;
	bool bValidationShouldSave = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperBlueprintVariablesToolClusterPolicy
{
	bool bDryRun = false;
	FString ReadMemberDefaultsScope = TEXT("member_variables");
	FString AssetPathFallback = TEXT("focused");
};

struct BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyToolClusterPolicy
{
	bool bDryRun = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperDataTableToolClusterPolicy
{
	bool bDryRun = false;
	bool bWriteRequiresRowStruct = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperUmgWidgetToolClusterPolicy
{
	bool bDryRun = false;
	bool bAssetPathRequired = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperSignatureToolClusterPolicy
{
	FString ReferenceContextSearchScope = TEXT("project");
	FString ReferenceContextResolutionPolicy = TEXT("ue_then_name");
	FString ReferenceContextDetail = TEXT("summary");
	int32 ReferenceContextMaxResults = 50;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteToolClusterPolicy
{
	bool bDryRun = false;
	bool bStrict = true;
	bool bCreateMissingVariables = false;
	bool bReconstructExistingNodes = false;
	bool bCompile = false;
	bool bSave = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReadContextToolClusterPolicy
{
	int32 MaxOutputRows = 0;
	int32 MaxOutputBytes = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperToolClusterConfigResolver
{
public:
	static FBlueprintHelperAssetFactoryToolClusterPolicy LoadAssetFactoryPolicy();
	static FBlueprintHelperComponentToolClusterPolicy LoadComponentPolicy();
	static FBlueprintHelperClassSettingsToolClusterPolicy LoadClassSettingsPolicy();
	static FBlueprintHelperBlueprintVariablesToolClusterPolicy LoadBlueprintVariablesPolicy();
	static FBlueprintHelperObjectPropertyToolClusterPolicy LoadObjectPropertyPolicy();
	static FBlueprintHelperDataTableToolClusterPolicy LoadDataTablePolicy();
	static FBlueprintHelperUmgWidgetToolClusterPolicy LoadUmgWidgetPolicy();
	static FBlueprintHelperSignatureToolClusterPolicy LoadSignaturePolicy();
	static FBlueprintHelperGraphWriteToolClusterPolicy LoadGraphWritePolicy();
	static FBlueprintHelperReadContextToolClusterPolicy LoadReadContextPolicy();
};

class BLUEPRINTHELPER_API FBlueprintHelperReadContextOutputLimiter
{
public:
	static bool IsReadContextCommand(const FString& Command);
	static void ApplyToBridgeResult(const FString& Command, const TSharedPtr<FJsonObject>& ResultJson);

private:
	static bool LimitObjectRows(const TSharedPtr<FJsonObject>& Object, int32 MaxRows);
	static bool LimitArrayRows(TArray<TSharedPtr<FJsonValue>>& Array, int32 MaxRows);
	static int32 MeasureJsonUtf8Bytes(const TSharedPtr<FJsonObject>& Object);
	static void ReplaceDataWithByteLimitSummary(const TSharedPtr<FJsonObject>& Object, int32 MaxBytes, int32 ActualBytes);
	static void ReplaceFindAssetsWithByteLimitSummary(const TSharedPtr<FJsonObject>& Object);
};
