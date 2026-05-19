// GameThread snapshot service for logic read tools.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotTypes.h"

class FBlueprintHelperExportService;
class FBlueprintHelperGraphResolver;

class BLUEPRINTHELPER_API FBlueprintHelperLogicReadSnapshotService
{
public:
	FBlueprintHelperLogicReadSnapshotService();
	explicit FBlueprintHelperLogicReadSnapshotService(const FBlueprintHelperExportService& InExportService);
	~FBlueprintHelperLogicReadSnapshotService();

	bool BuildSnapshot(
		const FBlueprintHelperTargetRef& Target,
		FBlueprintHelperLogicReadSnapshot& OutSnapshot,
		FString& OutError,
		FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache = nullptr) const;

	static FBlueprintHelperLogicReadSnapshotCacheKey MakeCacheKey(
		const FBlueprintHelperTargetRef& Target,
		EBlueprintHelperLogicScope Scope);

	static EBlueprintHelperLogicScope TargetTypeToScope(EBlueprintHelperTargetType Type);
	static EBlueprintHelperExportScope ScopeToExportScope(EBlueprintHelperLogicScope Scope);
	static bool IsTargetEntryScope(EBlueprintHelperLogicScope Scope);
	static FString GetTargetEntryName(
		const FBlueprintHelperTargetRef& Target,
		EBlueprintHelperLogicScope Scope);

private:
	TUniquePtr<FBlueprintHelperGraphResolver> OwnedGraphResolver;
	TUniquePtr<FBlueprintHelperExportService> OwnedExportService;
	const FBlueprintHelperExportService* ExportService = nullptr;
};
