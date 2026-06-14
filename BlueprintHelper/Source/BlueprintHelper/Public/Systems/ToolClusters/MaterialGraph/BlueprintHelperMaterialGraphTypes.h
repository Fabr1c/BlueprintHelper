// BlueprintHelper MaterialGraph service DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;
class FJsonValue;
class UMaterial;
class UMaterialExpression;

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialSelectorResolution
{
	bool bResolved = false;
	bool bRequiresCandidateSearch = false;
	bool bCandidateExpired = false;
	FString SelectorId;
	FString CandidateId;
	FString Fingerprint;
	FString ClassName;
	FString ErrorCode;
	FString ErrorMessage;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphValidationResult
{
	bool bValid = true;
	FString ErrorCode;
	FString ErrorMessage;
	FString FieldPath;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphPlannedConnection
{
	FString FromNodeKey;
	FString FromPin;
	FString ToNodeKey;
	FString ToPin;
	FString FieldPath;
	bool bMaterialOutput = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphExecutionState
{
	UMaterial* Material = nullptr;
	FString AssetPath;
	FString MaterialStrategy;
	FString CurrentBlockId;
	TMap<FString, UMaterialExpression*> ExpressionsByNodeKey;
	TMap<FString, FString> ExpressionClassNameByNodeKey;
	TMap<UMaterialExpression*, FString> NodeKeyByExpression;
	TMap<UMaterialExpression*, FString> BlockIdByExpression;
	TSet<FString> GeneratedExpressionNodeKeys;
	TSet<FString> DeletedExpressionNodeKeys;
	TMap<FString, FString> GeneratedExpressionFieldByNodeKey;
	TArray<FBlueprintHelperDiagnosticItem> CompileDiagnostics;
	int32 CreatedExpressionCount = 0;
	int32 UpdatedPropertyCount = 0;
	int32 DeletedExpressionCount = 0;
	int32 ExpressionConnectionCount = 0;
	int32 MaterialOutputConnectionCount = 0;
	int32 RequestedConnectionCount = 0;
	int32 VerifiedConnectionCount = 0;
	int32 GraphSyncConnectionCount = 0;
	TArray<TSharedPtr<FJsonValue>> CreatedExpressions;
	TArray<TSharedPtr<FJsonValue>> UpdatedProperties;
	TArray<TSharedPtr<FJsonValue>> DeletedExpressions;
	TArray<TSharedPtr<FJsonValue>> Connections;
	TArray<TSharedPtr<FJsonValue>> MaterialOutputs;
	TArray<FBlueprintHelperMaterialGraphPlannedConnection> PlannedConnections;
	TArray<FBlueprintHelperDiagnosticItem> ConnectivityDiagnostics;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphPreflightInput
{
	TSharedPtr<FJsonObject> StepObject;
	bool bDryRun = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphPreflightResult
{
	bool bValid = true;
	TArray<FString> BlockIds;
	TArray<FString> NodeKeys;
	TArray<FString> Diagnostics;
	FString ErrorCode;
	FString ErrorMessage;
	FString FieldPath;
};

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphExecutionInput
{
	TSharedPtr<FJsonObject> Payload;
	bool bDryRun = true;
};
