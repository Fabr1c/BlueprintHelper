// BlueprintHelper Service Layer - Material LogicJson read extraction.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UMaterial;
class UMaterialExpression;

/**
 * Builds the canonical read_context LogicJson payload for Material graphs.
 *
 * This class is a read-only runtime adapter helper. It extracts material
 * expression nodes, expression links, material property output links, and
 * parameter defaults. It does not mutate assets, open editors, or own CLI
 * routing.
 */
class BLUEPRINTHELPER_API FBlueprintHelperMaterialLogicJsonExtractor
{
public:
	bool BuildLogicJson(
		const FString& AssetPath,
		TSharedPtr<FJsonObject>& OutLogicJson,
		FString& OutError) const;

private:
	UMaterial* LoadMaterial(const FString& AssetPath, FString& OutError) const;
	FString BuildObjectPath(const FString& AssetPath) const;
	FString BuildExpressionRef(const UMaterialExpression* Expression, bool bForcePathRef) const;
	FString BuildExpressionNodeKey(const UMaterialExpression* Expression, const FString& ExpressionRef) const;
	FString BuildOutputPinName(const UMaterialExpression* Expression, int32 OutputIndex) const;

	void AddExpressionNodes(
		UMaterial* Material,
		TArray<TSharedPtr<FJsonValue>>& OutNodes,
		TArray<TSharedPtr<FJsonValue>>& OutParameters,
		TMap<const UMaterialExpression*, FString>& OutRefs,
		TArray<TSharedPtr<FJsonValue>>& OutDiagnostics) const;

	void AddExpressionLinks(
		UMaterial* Material,
		const TMap<const UMaterialExpression*, FString>& Refs,
		TArray<TSharedPtr<FJsonValue>>& OutLinks) const;

	void AddMaterialOutputs(
		UMaterial* Material,
		const TMap<const UMaterialExpression*, FString>& Refs,
		TArray<TSharedPtr<FJsonValue>>& OutOutputs,
		TArray<TSharedPtr<FJsonValue>>& OutLinks) const;

	TSharedPtr<FJsonObject> BuildExpressionNodeJson(
		const UMaterialExpression* Expression,
		const FString& ExpressionRef) const;

	TSharedPtr<FJsonObject> BuildParameterJson(
		const UMaterialExpression* Expression,
		const FString& ExpressionRef,
		TArray<TSharedPtr<FJsonValue>>& OutDiagnostics) const;

	void AddDiagnostic(
		TArray<TSharedPtr<FJsonValue>>& OutDiagnostics,
		const FString& Code,
		const FString& Severity,
		const FString& Message,
		const UMaterialExpression* Expression,
		const FString& ExpressionRef) const;

	void AppendOwnershipMetadata(
		const UMaterialExpression* Expression,
		TSharedPtr<FJsonObject> TargetJson) const;

	FString ReadOwnershipMetadataValue(
		const UMaterialExpression* Expression,
		const TCHAR* Key) const;
};
