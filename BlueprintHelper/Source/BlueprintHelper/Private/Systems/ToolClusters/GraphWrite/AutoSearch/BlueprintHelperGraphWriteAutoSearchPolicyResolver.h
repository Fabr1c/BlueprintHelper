// BlueprintHelper GraphWrite AutoSearch policy parser.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteAutoSearchTypes.h"

class FJsonObject;

class FBlueprintHelperGraphWriteAutoSearchPolicyResolver
{
public:
	static bool TryParseFromWriteObject(
		const TSharedPtr<FJsonObject>& WriteObject,
		FBlueprintHelperGraphWriteAutoSearchPolicy& OutPolicy,
		FString& OutError);
};
