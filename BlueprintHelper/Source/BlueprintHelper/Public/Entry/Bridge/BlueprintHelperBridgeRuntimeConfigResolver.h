// BlueprintHelper Bridge runtime config resolver.

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperBridgeRuntimeDefaults
{
	static constexpr int32 BridgePort = 32147;
};

struct FBlueprintHelperBridgeRuntimeConfig
{
	int32 Port = FBlueprintHelperBridgeRuntimeDefaults::BridgePort;
	int32 MaxPendingConnections = 8;
	int32 AcceptWaitMs = 250;
	double IdleTimeoutSeconds = 2.0;
	int32 MaxFrameBytes = 16777216;
	int32 SocketBufferBytes = 262144;
};

class BLUEPRINTHELPER_API FBlueprintHelperBridgeRuntimeConfigResolver
{
public:
	static FBlueprintHelperBridgeRuntimeConfig Load();
};
