// BlueprintHelper read-side pure snapshot DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperLogicMdTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Services/BlueprintHelperExportService.h"

struct FBlueprintHelperLogicReadSnapshot
{
	FBlueprintHelperTargetRef Target;
	EBlueprintHelperLogicScope Scope = EBlueprintHelperLogicScope::TargetGraph;
	EBlueprintHelperExportScope ExportScope = EBlueprintHelperExportScope::SingleGraph;
	FString AssetPath;
	FString GraphName;
	FString TargetEntryName;
	FString SchemaVersion = TEXT("LogicReadSnapshot.v1");
	bool bTargetEntryScope = false;
	bool bExportSucceeded = false;
	TSharedPtr<FJsonObject> RawJsonObject;
	TSharedPtr<FJsonObject> AdapterBoundaryJson;
	TArray<FString> Warnings;
	int32 NodeCount = 0;
	int32 EdgeCount = 0;
};

struct FBlueprintHelperLogicReadSnapshotCacheKey
{
	FString AssetPath;
	FString GraphName;
	FString Scope;
	FString ReadDetail;
	FString SchemaVersion;

	FString ToStableString() const
	{
		return FString::Printf(
			TEXT("%s|%s|%s|%s|%s"),
			*AssetPath,
			*GraphName,
			*Scope,
			*ReadDetail,
			*SchemaVersion);
	}
};
