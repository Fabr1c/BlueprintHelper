// BlueprintHelper MaterialGraph expression candidate cache service.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialExpressionCandidateCacheService
{
public:
	// Material Graph does not expose a reusable Blueprint ActionDatabase spawner path for P0.
	// This service owns a MaterialExpression class action snapshot: GameThread refresh builds
	// UObject class metadata into plain data, then query/ranking/cache consume only that snapshot.
	static bool IsCommonSelector(const FString& Selector);
	static FString ResolveCommonSelectorClassName(const FString& Selector);

	static FString GetCandidateSchemaFingerprint();
	static FString GetMaterialExpressionClassActionSnapshotRevision();
	static FString GetMaterialExpressionClassActionSnapshotFingerprint();
	static void RefreshMaterialExpressionClassActionSnapshotOnGameThread();

	static FString BuildCandidateCacheFingerprint(
		const FString& AssetPath,
		const FString& Query);

	static double GetCandidateTtlSeconds();

	static TArray<TSharedPtr<FJsonValue>> BuildAndCacheCandidates(
		const FString& Query,
		const FString& AssetPath);

	static TArray<TSharedPtr<FJsonValue>> BuildCommonSelectorCandidates(
		const FString& Query);

	static FBlueprintHelperMaterialSelectorResolution ResolveCandidateId(
		const FString& CandidateId,
		const FString& AssetPath);
};
