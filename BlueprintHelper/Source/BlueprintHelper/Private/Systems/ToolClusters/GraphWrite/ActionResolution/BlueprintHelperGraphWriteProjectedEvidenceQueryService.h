#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

class FBlueprintHelperGraphWriteProjectedEvidenceQueryService
{
public:
	static FBlueprintHelperToolResultBase Project(const TSharedPtr<FJsonObject>& Payload);
};
