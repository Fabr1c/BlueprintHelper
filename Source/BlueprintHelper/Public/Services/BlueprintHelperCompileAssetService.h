// BlueprintHelper Service Layer — CompileBlueprintAsset 服务

#pragma once

#include "CoreMinimal.h"
#include "Services/BlueprintHelperCompileAssetTypes.h"
#include "Services/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperCompileService;
class FJsonObject;
class UBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperCompileAssetService
{
public:
	explicit FBlueprintHelperCompileAssetService(const FBlueprintHelperCompileService& InCompileService);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	const FBlueprintHelperCompileService& CompileService;
};
