#pragma once

#include "CoreMinimal.h"
#include "Runtime/Capabilities/BlueprintHelperCapabilityDescriptorTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperRuntimeCapabilityStateBuilder
{
public:
	static FBlueprintHelperCapabilityDescriptorRuntimeState BuildRegisteredRuntimeState();
};
