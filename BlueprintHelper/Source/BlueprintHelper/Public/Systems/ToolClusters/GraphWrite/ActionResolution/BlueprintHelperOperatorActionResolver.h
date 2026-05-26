#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;
class UBlueprintFunctionNodeSpawner;

class BLUEPRINTHELPER_API FBlueprintHelperOperatorActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);

private:
	static bool TryMapOperatorTokenToPromotionName(const FString& Token, FName& OutOpName);
	static UBlueprintFunctionNodeSpawner* FindPromotableOperatorSpawner(FName OpName);
	static FBlueprintHelperActionResolutionResult MakePromotableOperatorResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FString& OperatorToken,
		FName OpName,
		UBlueprintFunctionNodeSpawner* Spawner);
	static FBlueprintHelperActionResolutionResult MakeInvalidRequestResult(const FString& Message);
	static FBlueprintHelperActionResolutionResult MakeInvalidRequestResult(const FString& ErrorCode, const FString& Message);
	static FBlueprintHelperActionResolutionResult MakeNotFoundResult(const FBlueprintHelperActionResolutionRequest& Request, const FString& Message);
};
