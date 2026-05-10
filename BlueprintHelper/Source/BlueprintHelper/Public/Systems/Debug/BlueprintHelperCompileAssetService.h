// BlueprintHelper Service Layer 。CompileBlueprintAsset 服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperCompileAssetTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperCompileService;
class FBlueprintHelperDebugEntryService;
class FJsonObject;
class UBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperCompileAssetService
{
public:
	explicit FBlueprintHelperCompileAssetService(
		const FBlueprintHelperCompileService& InCompileService,
		const FBlueprintHelperDebugEntryService* InDebugEntryService = nullptr);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;
	static FBlueprintHelperToolResultBase BuildResultFromCompileResult(
		const FString& TraceId,
		const FString& AssetPath,
		const FBlueprintHelperCompileResult& CompileResult,
		const FBlueprintHelperDebugEntryService* DebugEntryService = nullptr);

private:
	const FBlueprintHelperCompileService& CompileService;
	const FBlueprintHelperDebugEntryService* DebugEntryService = nullptr;
};
