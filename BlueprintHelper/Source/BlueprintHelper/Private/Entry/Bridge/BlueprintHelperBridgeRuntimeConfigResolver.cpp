// BlueprintHelper Bridge runtime config resolver implementation.

#include "Entry/Bridge/BlueprintHelperBridgeRuntimeConfigResolver.h"

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

FBlueprintHelperBridgeRuntimeConfig FBlueprintHelperBridgeRuntimeConfigResolver::Load()
{
	FBlueprintHelperBridgeRuntimeConfig Config;
	Config.Port = FMath::Clamp(
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.bridge.port"), Config.Port),
		1,
		65535);
	Config.MaxPendingConnections = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.bridge.max_pending_connections"), Config.MaxPendingConnections));
	Config.AcceptWaitMs = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.bridge.accept_wait_ms"), Config.AcceptWaitMs));
	Config.IdleTimeoutSeconds = FMath::Max(
		0.01,
		FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("runtime.bridge.idle_timeout_seconds"), Config.IdleTimeoutSeconds));
	Config.MaxFrameBytes = FMath::Max(
		1,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.bridge.max_frame_bytes"), Config.MaxFrameBytes));
	Config.SocketBufferBytes = FMath::Max(
		4096,
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.bridge.socket_buffer_bytes"), Config.SocketBufferBytes));
	return Config;
}
