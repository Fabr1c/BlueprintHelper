// BlueprintHelper MaterialGraph compile service boundary.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class UMaterial;
class UMaterialExpression;
class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphCompileService
{
public:
	static FString BuildCompilePendingDiagnostic();
	static TArray<FBlueprintHelperDiagnosticItem> CollectExpressionDiagnostics(
		UMaterial* Material,
		const TMap<UMaterialExpression*, FString>& NodeKeyByExpression);
	static bool HasBlockingCompileErrors(
		const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics);
	static FBlueprintHelperToolError BuildBlockingCompileError(
		const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics);
	static TSharedRef<FJsonObject> BuildCompileResultJson(
		const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics);
};
