#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperCapabilityDescriptorRuntimeState
{
	TSet<FString> RegisteredRuntimeAdapterIds;
	bool bAllowWriteCapabilities = true;
	bool bAllowHighRiskCapabilities = true;
};

