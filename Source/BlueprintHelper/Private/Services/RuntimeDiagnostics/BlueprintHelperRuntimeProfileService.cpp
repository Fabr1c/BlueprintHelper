// BlueprintHelper Service Layer 。Runtime Profile 服务实现

#include "Services/RuntimeDiagnostics/BlueprintHelperRuntimeProfileService.h"
#include "Structure/RuntimeDiagnostics/BlueprintHelperRuntimeProfileTypes.h"
#include "Structure/BlueprintHelperToolResultTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProcess.h"

FBlueprintHelperRuntimeProfileService::FBlueprintHelperRuntimeProfileService() = default;

FBlueprintHelperRuntimeProfileData FBlueprintHelperRuntimeProfileService::GetRuntimeProfile() const
{
	FBlueprintHelperRuntimeProfileData Data;
	Data.Version = GetPluginVersion();
	Data.BridgeStatus = DetectBridgeStatus();
	Data.ConfigStatus = DetectConfigStatus();
	Data.WritePermission = BuildWritePermissionState();
	Data.RiskCommand = BuildRiskCommandState();
	Data.ActiveProfile = BuildActiveProfileState();
	Data.ToolCapabilities.Mode = EBlueprintHelperToolCapabilitiesMode::UnavailableOnly;
	Data.ToolCapabilities.Unavailable = BuildUnavailableCapabilities();
	return Data;
}

EBlueprintHelperBridgeStatus FBlueprintHelperRuntimeProfileService::DetectBridgeStatus()
{
	// 简化实现：检查进程是否运行（Server 已在模块启动时创建）
	// 实际更精确的判断需。BridgeServer 暴露 IsRunning() 接口
	return EBlueprintHelperBridgeStatus::Connected;
}

EBlueprintHelperConfigStatus FBlueprintHelperRuntimeProfileService::DetectConfigStatus()
{
	// 检。FilterPlugin.ini 是否存在
	const FString ConfigPath = FPaths::ProjectPluginsDir() / TEXT("BlueprintHelper/Config/FilterPlugin.ini");
	if (FPaths::FileExists(ConfigPath))
	{
		return EBlueprintHelperConfigStatus::Valid;
	}

	// 也尝试绝对路径
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	if (Plugin.IsValid())
	{
		const FString PluginConfigPath = Plugin->GetBaseDir() / TEXT("Config/FilterPlugin.ini");
		if (FPaths::FileExists(PluginConfigPath))
		{
			return EBlueprintHelperConfigStatus::Valid;
		}
	}

	return EBlueprintHelperConfigStatus::ConfigUnavailable;
}

FBlueprintHelperWritePermissionState FBlueprintHelperRuntimeProfileService::BuildWritePermissionState()
{
	FBlueprintHelperWritePermissionState State;

	const FString Token = GetBridgeToken();
	if (Token.IsEmpty())
	{
		State.bEnabled = false;
		State.Reason = EBlueprintHelperWritePermissionReason::TokenMissing;
		return State;
	}

	// Token 存在则允许写（不做复杂校验）
	State.bEnabled = true;
	State.Reason = EBlueprintHelperWritePermissionReason::Ok;

	// 检。Safety Profile 是否只读
	const EBlueprintHelperSafetyProfile Profile = BuildActiveProfileState().SafetyProfile;
	if (Profile == EBlueprintHelperSafetyProfile::ReadOnly)
	{
		State.bEnabled = false;
		State.Reason = EBlueprintHelperWritePermissionReason::SafetyProfileReadOnly;
	}

	return State;
}

FBlueprintHelperRiskCommandState FBlueprintHelperRuntimeProfileService::BuildRiskCommandState()
{
	FBlueprintHelperRiskCommandState State;

	const FString Token = GetBridgeToken();
	if (Token.IsEmpty())
	{
		State.bEnabled = false;
		State.Reason = EBlueprintHelperRiskCommandReason::RiskCommandMissing;
		State.BlockedCommands = { TEXT("close_editor"), TEXT("exec_console_command") };
		return State;
	}

	// 默认风险命令禁用（需要显式配置）
	State.bEnabled = false;
	State.Reason = EBlueprintHelperRiskCommandReason::RiskCommandMissing;
	State.BlockedCommands = { TEXT("close_editor"), TEXT("exec_console_command") };

	return State;
}

FBlueprintHelperActiveProfileState FBlueprintHelperRuntimeProfileService::BuildActiveProfileState()
{
	FBlueprintHelperActiveProfileState State;

	// 默认使用 conservative 安全档位
	State.SafetyProfile = EBlueprintHelperSafetyProfile::Conservative;
	State.MissingCapabilityPolicy = EBlueprintHelperMissingCapabilityPolicy::StopAndReport;

	// 尝试从插件配置中读取
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	if (Plugin.IsValid())
	{
		const FString ConfigPath = Plugin->GetBaseDir() / TEXT("Config/FilterPlugin.ini");
		if (FPaths::FileExists(ConfigPath))
		{
			FString ProfileStr;
			if (GConfig->GetString(TEXT("BlueprintHelper"), TEXT("SafetyProfile"), ProfileStr, ConfigPath))
			{
				if (ProfileStr.Equals(TEXT("readonly"), ESearchCase::IgnoreCase))
				{
					State.SafetyProfile = EBlueprintHelperSafetyProfile::ReadOnly;
				}
				else if (ProfileStr.Equals(TEXT("standard"), ESearchCase::IgnoreCase))
				{
					State.SafetyProfile = EBlueprintHelperSafetyProfile::Standard;
				}
				else if (ProfileStr.Equals(TEXT("autorepair"), ESearchCase::IgnoreCase))
				{
					State.SafetyProfile = EBlueprintHelperSafetyProfile::AutoRepair;
				}
			}
		}
	}

	return State;
}

TArray<FBlueprintHelperUnavailableCapability> FBlueprintHelperRuntimeProfileService::BuildUnavailableCapabilities()
{
	TArray<FBlueprintHelperUnavailableCapability> Unavailable;

	// 标记当前尚未实现的能力（按计划逐步实现后会从此列表移除）
	auto AddUnavailable = [&](const TCHAR* Cluster, const TCHAR* Capability,
		EBlueprintHelperCapabilityUnavailableReason Reason = EBlueprintHelperCapabilityUnavailableReason::NotImplemented)
	{
		FBlueprintHelperUnavailableCapability Item;
		Item.Cluster = Cluster;
		Item.Capability = Capability;
		Item.Status = EBlueprintHelperCapabilityStatus::Unavailable;
		Item.Reason = Reason;
		Unavailable.Add(Item);
	};

	// cleanup 。- 整体尚未实现
	AddUnavailable(TEXT("cleanup"), TEXT("cleanup_blueprinthelper_block"));

	// transaction / review 。- 尚未实现完整结构
	AddUnavailable(TEXT("transaction"), TEXT("journal"));
	AddUnavailable(TEXT("review"), TEXT("store"));

	// 如果写权限被禁用，标记所。graph_write 能力。blocked
	const FBlueprintHelperWritePermissionState WritePerm = BuildWritePermissionState();
	if (!WritePerm.bEnabled)
	{
		const TArray<TPair<const TCHAR*, const TCHAR*>> WriteCapabilities = {
			{ TEXT("graph_write"), TEXT("append") },
			{ TEXT("graph_write"), TEXT("patch") },
			{ TEXT("graph_write"), TEXT("replace") },
			{ TEXT("graph_write"), TEXT("merge") },
		};
		for (const auto& Cap : WriteCapabilities)
		{
			FBlueprintHelperUnavailableCapability Item;
			Item.Cluster = Cap.Key;
			Item.Capability = Cap.Value;
			Item.Status = EBlueprintHelperCapabilityStatus::Blocked;
			Item.Reason = EBlueprintHelperCapabilityUnavailableReason::WritePermissionDisabled;
			Unavailable.Add(Item);
		}
	}

	return Unavailable;
}

FString FBlueprintHelperRuntimeProfileService::GetPluginVersion()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	if (Plugin.IsValid())
	{
		return Plugin->GetDescriptor().VersionName;
	}
	return TEXT("0.5.0-dev");
}

FString FBlueprintHelperRuntimeProfileService::GetBridgeToken()
{
	// 从环境变量读取
	FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("BLUEPRINTHELPER_BRIDGE_TOKEN"));

	// 回退：从插件配置读取
	if (Token.IsEmpty())
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
		if (Plugin.IsValid())
		{
			const FString ConfigPath = Plugin->GetBaseDir() / TEXT("Config/FilterPlugin.ini");
			if (FPaths::FileExists(ConfigPath))
			{
				GConfig->GetString(TEXT("BlueprintHelper"), TEXT("BridgeToken"), Token, ConfigPath);
			}
		}
	}

	return Token;
}
