#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperActionContextRevisionGuard
{
public:
	static bool Validate(
		const FBlueprintHelperActionContextRevisionToken& Expected,
		const FBlueprintHelperActionContextRevisionToken& Current,
		FString& OutError);

	static bool ValidateBundle(
		const FBlueprintHelperResolvedActionContextBundle& Bundle,
		const FBlueprintHelperActionContextRevisionToken& Current,
		FString& OutError);
};
