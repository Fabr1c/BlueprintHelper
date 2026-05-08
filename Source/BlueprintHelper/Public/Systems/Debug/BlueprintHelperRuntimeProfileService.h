// BlueprintHelper Service Layer 。Runtime Profile 服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperRuntimeProfileTypes.h"

struct FBlueprintHelperRuntimeProfileData;

/**
 * 运行时事实接口服务。
 * 负责读取插件版本、连接状态、权限、安全档位和能力可用性。
 * 不负责：完整 setup profile、命名偏好、蓝。C++边界、diagnostics 历史。
 */
class BLUEPRINTHELPER_API FBlueprintHelperRuntimeProfileService
{
public:
	FBlueprintHelperRuntimeProfileService();

	/** 获取当前运行。Profile 完整数据。*/
	FBlueprintHelperRuntimeProfileData GetRuntimeProfile() const;

private:
	/** 判断 Bridge 连接状态。*/
	static EBlueprintHelperBridgeStatus DetectBridgeStatus();

	/** 判断配置状态。*/
	static EBlueprintHelperConfigStatus DetectConfigStatus();

	/** 构建写权限状态。*/
	static FBlueprintHelperWritePermissionState BuildWritePermissionState();

	/** 构建高风险命令状态。*/
	static FBlueprintHelperRiskCommandState BuildRiskCommandState();

	/** 构建 Active Profile 状态。*/
	static FBlueprintHelperActiveProfileState BuildActiveProfileState();

	/** 构建不可用能力列表。*/
	static TArray<FBlueprintHelperUnavailableCapability> BuildUnavailableCapabilities();

	/** 获取插件版本号（。uplugin 或编译宏）。*/
	static FString GetPluginVersion();

	/** 读取 settings / config 中的 token。*/
	static FString GetBridgeToken();
};
