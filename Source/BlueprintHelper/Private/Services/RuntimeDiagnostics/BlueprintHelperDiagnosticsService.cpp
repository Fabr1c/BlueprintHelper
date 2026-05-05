// BlueprintHelper Service Layer 。Diagnostics 服务实现（Runtime。

#include "Services/RuntimeDiagnostics/BlueprintHelperDiagnosticsService.h"
#include "Structure/RuntimeDiagnostics/BlueprintHelperDiagnosticsTypes.h"
#include "Structure/RuntimeDiagnostics/BlueprintHelperRuntimeProfileTypes.h"
#include "Structure/BlueprintHelperToolResultTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"

FBlueprintHelperDiagnosticsService::FBlueprintHelperDiagnosticsService() = default;

FBlueprintHelperDiagnosticsData FBlueprintHelperDiagnosticsService::RunRuntimeDiagnostics() const
{
	FBlueprintHelperDiagnosticsData Data;
	Data.Mode = EBlueprintHelperDiagnosticsMode::Runtime;
	Data.Format = EBlueprintHelperDiagnosticsFormat::Markdown;

	FBlueprintHelperDiagnosticsMarkdownReport Report;

	// ══════。Blocking 检。══════。

	// UE Editor 是否运行（此代码。UE Editor 中执行，因此必然运行。
	Report.AddInfo(TEXT("ue_editor.running"));

	// Bridge 是否可达（Service 在模块启动时创建，说。Bridge 已完成初始化。
	Report.AddInfo(TEXT("mcp_server.available"));
	Report.AddInfo(TEXT("bridge.connected"));

	// Runtime Profile 是否可用
	Report.AddInfo(TEXT("runtime_profile.available"));

	// Config 状态
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	bool bConfigValid = false;
	if (Plugin.IsValid())
	{
		const FString ConfigPath = Plugin->GetBaseDir() / TEXT("Config/FilterPlugin.ini");
		bConfigValid = FPaths::FileExists(ConfigPath);
	}

	if (bConfigValid)
	{
		Report.AddInfo(TEXT("config_status.valid"));
	}
	else
	{
		Report.AddBlocking(TEXT("config_status.unavailable"));
	}

	// ══════。Warning 检。══════。

	// Write Permission
	const FString Token = GetBridgeToken();
	const FString SafetyProfile = GetSafetyProfileStr();

	if (Token.IsEmpty())
	{
		Report.AddWarning(TEXT("write_permission.disabled"),
			TEXT("reason: token_missing"));
	}
	else if (SafetyProfile.Equals(TEXT("readonly"), ESearchCase::IgnoreCase))
	{
		Report.AddWarning(TEXT("write_permission.disabled"),
			TEXT("reason: safety_profile_read_only"));
	}
	else
	{
		Report.AddInfo(TEXT("write_permission.enabled"));
	}

	// Risk Command
	// 默认禁用，需。BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS=1
	const FString RiskEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS")).ToLower();
	const bool bRiskEnabled = RiskEnv == TEXT("1") || RiskEnv == TEXT("true") || RiskEnv == TEXT("yes");

	if (bRiskEnabled)
	{
		Report.AddInfo(TEXT("risk_command.enabled"));
	}
	else
	{
		Report.AddWarning(TEXT("risk_command.disabled"),
			TEXT("reason: risk_command_missing\nblocked_commands: close_editor, exec_console_command"));
	}

	// Project Marker
	if (HasProjectMarker())
	{
		Report.AddInfo(TEXT("project_marker.present"));
	}
	else
	{
		Report.AddWarning(TEXT("project_marker.missing"));
	}

	Data.Markdown = Report.ToMarkdown();
	return Data;
}

FString FBlueprintHelperDiagnosticsService::GetPluginVersion()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	if (Plugin.IsValid())
	{
		return Plugin->GetDescriptor().VersionName;
	}
	return TEXT("0.5.0-dev");
}

FString FBlueprintHelperDiagnosticsService::GetBridgeToken()
{
	FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("BLUEPRINTHELPER_BRIDGE_TOKEN"));
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

FString FBlueprintHelperDiagnosticsService::GetSafetyProfileStr()
{
	FString ProfileStr;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	if (Plugin.IsValid())
	{
		const FString ConfigPath = Plugin->GetBaseDir() / TEXT("Config/FilterPlugin.ini");
		if (FPaths::FileExists(ConfigPath))
		{
			GConfig->GetString(TEXT("BlueprintHelper"), TEXT("SafetyProfile"), ProfileStr, ConfigPath);
		}
	}
	return ProfileStr;
}

bool FBlueprintHelperDiagnosticsService::HasProjectMarker()
{
	// 检查项目目录下是否有关。Marker 文件
	// 1. .claude/CLAUDE.md
	// 2. AGENTS.md
	// 3. .blueprinthelper/
	const FString ProjectDir = FPaths::ProjectDir();

	static const TArray<FString> Markers = {
		TEXT(".claude/CLAUDE.md"),
		TEXT("AGENTS.md"),
		TEXT(".blueprinthelper/agent-profile.json"),
	};

	for (const FString& Marker : Markers)
	{
		if (FPaths::FileExists(ProjectDir / Marker))
		{
			return true;
		}
	}

	return false;
}
