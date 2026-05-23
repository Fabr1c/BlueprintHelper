#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperFieldVariableActionResolver
{
public:
	FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context) const;

private:
	static bool IsSupportedSemanticKind(const FBlueprintHelperActionSemanticConstraints& Semantic);
	static bool IsWritableFieldOperation(const FString& FieldOperation);
};
