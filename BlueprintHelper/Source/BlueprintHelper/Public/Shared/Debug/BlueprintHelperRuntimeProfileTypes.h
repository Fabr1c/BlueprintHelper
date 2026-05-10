// BlueprintHelper Service Layer — Runtime Profile 类型定义
// 第 1 簇：运行时事实接口的数据类型与枚举

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── Bridge 状态枚举 ───

/** UE Bridge 当前连接状态。 */
enum class EBlueprintHelperBridgeStatus : uint8
{
	Connected,
	Disconnected,
	EditorNotRunning,
	McpUnavailable,
	Unknown
};

/** BridgeStatus → MCP snake_case string。 */
inline const TCHAR* BridgeStatusToString(EBlueprintHelperBridgeStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperBridgeStatus::Connected:        return TEXT("connected");
	case EBlueprintHelperBridgeStatus::Disconnected:     return TEXT("disconnected");
	case EBlueprintHelperBridgeStatus::EditorNotRunning: return TEXT("editor_not_running");
	case EBlueprintHelperBridgeStatus::McpUnavailable:   return TEXT("mcp_unavailable");
	case EBlueprintHelperBridgeStatus::Unknown:          return TEXT("unknown");
	default:                                             return TEXT("unknown");
	}
}

// ─── 配置状态枚举 ───

/** 配置粗状态。 */
enum class EBlueprintHelperConfigStatus : uint8
{
	Valid,
	ConfigUnavailable,
	Unknown
};

/** ConfigStatus → MCP snake_case string。 */
inline const TCHAR* ConfigStatusToString(EBlueprintHelperConfigStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperConfigStatus::Valid:             return TEXT("valid");
	case EBlueprintHelperConfigStatus::ConfigUnavailable: return TEXT("config_unavailable");
	case EBlueprintHelperConfigStatus::Unknown:           return TEXT("unknown");
	default:                                              return TEXT("unknown");
	}
}

// ─── 写权限原因枚举 ───

/** 写权限状态原因码。 */
enum class EBlueprintHelperWritePermissionReason : uint8
{
	Ok,
	TokenMissing,
	TokenInvalid,
	TokenExpired,
	TokenMissingOrInvalid,
	WriteSessionMissing,
	ConfigUnavailable,
	SetupNotCompleted,
	SafetyProfileReadOnly,
	WriteDisabled,
	Unknown
};

/** WritePermissionReason → MCP snake_case string。 */
inline const TCHAR* WritePermissionReasonToString(EBlueprintHelperWritePermissionReason Reason)
{
	switch (Reason)
	{
	case EBlueprintHelperWritePermissionReason::Ok:                     return TEXT("ok");
	case EBlueprintHelperWritePermissionReason::TokenMissing:           return TEXT("token_missing");
	case EBlueprintHelperWritePermissionReason::TokenInvalid:           return TEXT("token_invalid");
	case EBlueprintHelperWritePermissionReason::TokenExpired:           return TEXT("token_expired");
	case EBlueprintHelperWritePermissionReason::TokenMissingOrInvalid:  return TEXT("token_missing_or_invalid");
	case EBlueprintHelperWritePermissionReason::WriteSessionMissing:    return TEXT("write_session_missing");
	case EBlueprintHelperWritePermissionReason::ConfigUnavailable:      return TEXT("config_unavailable");
	case EBlueprintHelperWritePermissionReason::SetupNotCompleted:      return TEXT("setup_not_completed");
	case EBlueprintHelperWritePermissionReason::SafetyProfileReadOnly:  return TEXT("safety_profile_read_only");
	case EBlueprintHelperWritePermissionReason::WriteDisabled:          return TEXT("write_disabled");
	case EBlueprintHelperWritePermissionReason::Unknown:                return TEXT("unknown");
	default:                                                            return TEXT("unknown");
	}
}

// ─── 高风险命令原因枚举 ───

/** 高风险命令状态原因码。 */
enum class EBlueprintHelperRiskCommandReason : uint8
{
	Ok,
	RiskCommandMissing,
	RiskCommandInvalid,
	CommandNotAuthorized,
	ConfigUnavailable,
	Unknown
};

/** RiskCommandReason → MCP snake_case string。 */
inline const TCHAR* RiskCommandReasonToString(EBlueprintHelperRiskCommandReason Reason)
{
	switch (Reason)
	{
	case EBlueprintHelperRiskCommandReason::Ok:                   return TEXT("ok");
	case EBlueprintHelperRiskCommandReason::RiskCommandMissing:   return TEXT("risk_command_missing");
	case EBlueprintHelperRiskCommandReason::RiskCommandInvalid:   return TEXT("risk_command_invalid");
	case EBlueprintHelperRiskCommandReason::CommandNotAuthorized: return TEXT("command_not_authorized");
	case EBlueprintHelperRiskCommandReason::ConfigUnavailable:    return TEXT("config_unavailable");
	case EBlueprintHelperRiskCommandReason::Unknown:              return TEXT("unknown");
	default:                                                      return TEXT("unknown");
	}
}

// ─── Safety Profile 枚举 ───

/** 安全档位。 */
enum class EBlueprintHelperSafetyProfile : uint8
{
	ReadOnly,
	Conservative,
	Standard,
	AutoRepair
};

/** SafetyProfile → MCP snake_case string。 */
inline const TCHAR* SafetyProfileToString(EBlueprintHelperSafetyProfile Profile)
{
	switch (Profile)
	{
	case EBlueprintHelperSafetyProfile::ReadOnly:     return TEXT("readonly");
	case EBlueprintHelperSafetyProfile::Conservative: return TEXT("conservative");
	case EBlueprintHelperSafetyProfile::Standard:     return TEXT("standard");
	case EBlueprintHelperSafetyProfile::AutoRepair:   return TEXT("autorepair");
	default:                                          return TEXT("unknown");
	}
}

// ─── 缺失能力策略枚举 ───

/** 缺失能力默认处理策略。 */
enum class EBlueprintHelperMissingCapabilityPolicy : uint8
{
	StopAndReport
};

/** MissingCapabilityPolicy → MCP snake_case string。 */
inline const TCHAR* MissingCapabilityPolicyToString(EBlueprintHelperMissingCapabilityPolicy Policy)
{
	switch (Policy)
	{
	case EBlueprintHelperMissingCapabilityPolicy::StopAndReport: return TEXT("stop_and_report");
	default:                                                     return TEXT("unknown");
	}
}

// ─── 工具能力模式枚举 ───

/** 工具能力返回模式。 */
enum class EBlueprintHelperToolCapabilitiesMode : uint8
{
	UnavailableOnly
};

/** ToolCapabilitiesMode → MCP snake_case string。 */
inline const TCHAR* ToolCapabilitiesModeToString(EBlueprintHelperToolCapabilitiesMode Mode)
{
	switch (Mode)
	{
	case EBlueprintHelperToolCapabilitiesMode::UnavailableOnly: return TEXT("unavailable_only");
	default:                                                    return TEXT("unknown");
	}
}

// ─── 能力状态枚举 ───

/** 能力可用状态。 */
enum class EBlueprintHelperCapabilityStatus : uint8
{
	Unavailable,
	Disabled,
	Degraded,
	Blocked
};

/** CapabilityStatus → MCP snake_case string。 */
inline const TCHAR* CapabilityStatusToString(EBlueprintHelperCapabilityStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperCapabilityStatus::Unavailable: return TEXT("unavailable");
	case EBlueprintHelperCapabilityStatus::Disabled:    return TEXT("disabled");
	case EBlueprintHelperCapabilityStatus::Degraded:    return TEXT("degraded");
	case EBlueprintHelperCapabilityStatus::Blocked:     return TEXT("blocked");
	default:                                            return TEXT("unknown");
	}
}

// ─── 能力不可用原因枚举 ───

/** 能力不可用原因码。 */
enum class EBlueprintHelperCapabilityUnavailableReason : uint8
{
	NotImplemented,
	NotRegistered,
	VersionUnsupported,
	ConfigUnavailable,
	WritePermissionDisabled,
	TokenMissing,
	TokenInvalid,
	TokenExpired,
	SafetyProfileReadOnly,
	BridgeDisconnected,
	EditorNotRunning,
	ResourceStoreUnavailable,
	DependencyMissing,
	Unknown
};

/** CapabilityUnavailableReason → MCP snake_case string。 */
inline const TCHAR* CapabilityUnavailableReasonToString(EBlueprintHelperCapabilityUnavailableReason Reason)
{
	switch (Reason)
	{
	case EBlueprintHelperCapabilityUnavailableReason::NotImplemented:            return TEXT("not_implemented");
	case EBlueprintHelperCapabilityUnavailableReason::NotRegistered:             return TEXT("not_registered");
	case EBlueprintHelperCapabilityUnavailableReason::VersionUnsupported:        return TEXT("version_unsupported");
	case EBlueprintHelperCapabilityUnavailableReason::ConfigUnavailable:         return TEXT("config_unavailable");
	case EBlueprintHelperCapabilityUnavailableReason::WritePermissionDisabled:   return TEXT("write_permission_disabled");
	case EBlueprintHelperCapabilityUnavailableReason::TokenMissing:              return TEXT("token_missing");
	case EBlueprintHelperCapabilityUnavailableReason::TokenInvalid:              return TEXT("token_invalid");
	case EBlueprintHelperCapabilityUnavailableReason::TokenExpired:              return TEXT("token_expired");
	case EBlueprintHelperCapabilityUnavailableReason::SafetyProfileReadOnly:     return TEXT("safety_profile_read_only");
	case EBlueprintHelperCapabilityUnavailableReason::BridgeDisconnected:        return TEXT("bridge_disconnected");
	case EBlueprintHelperCapabilityUnavailableReason::EditorNotRunning:          return TEXT("editor_not_running");
	case EBlueprintHelperCapabilityUnavailableReason::ResourceStoreUnavailable:  return TEXT("resource_store_unavailable");
	case EBlueprintHelperCapabilityUnavailableReason::DependencyMissing:         return TEXT("dependency_missing");
	case EBlueprintHelperCapabilityUnavailableReason::Unknown:                   return TEXT("unknown");
	default:                                                                     return TEXT("unknown");
	}
}

#pragma region Runtime Profile Structs

// ─── 5.9 FBlueprintHelperUnavailableCapability ───

/** 单个不可用能力的描述。 */
struct FBlueprintHelperUnavailableCapability
{
	/** 工具簇名，例如 graph_write。 */
	FString Cluster;

	/** 子能力名，例如 merge。 */
	FString Capability;

	/** 当前能力状态。 */
	EBlueprintHelperCapabilityStatus Status = EBlueprintHelperCapabilityStatus::Unavailable;

	/** 稳定原因码。 */
	EBlueprintHelperCapabilityUnavailableReason Reason = EBlueprintHelperCapabilityUnavailableReason::NotImplemented;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("cluster"), Cluster);
		Json->SetStringField(TEXT("capability"), Capability);
		Json->SetStringField(TEXT("status"), CapabilityStatusToString(Status));
		Json->SetStringField(TEXT("reason"), CapabilityUnavailableReasonToString(Reason));
		return Json;
	}
};

// ─── 5.8 FBlueprintHelperToolCapabilitiesState ───

/** 工具能力状态（负向稀疏返回）。 */
struct FBlueprintHelperToolCapabilitiesState
{
	/** 返回模式，固定为 unavailable_only。 */
	EBlueprintHelperToolCapabilitiesMode Mode = EBlueprintHelperToolCapabilitiesMode::UnavailableOnly;

	/** 当前不可用/禁用/降级/阻断的能力列表。 */
	TArray<FBlueprintHelperUnavailableCapability> Unavailable;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("mode"), ToolCapabilitiesModeToString(Mode));
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const auto& Item : Unavailable) { Arr.Add(MakeShared<FJsonValueObject>(Item.ToJson())); }
		Json->SetArrayField(TEXT("unavailable"), Arr);
		return Json;
	}
};

// ─── 5.5 FBlueprintHelperWritePermissionState ───

/** 写权限状态。 */
struct FBlueprintHelperWritePermissionState
{
	/** 当前是否允许写操作。 */
	bool bEnabled = false;

	/** 稳定原因码。 */
	EBlueprintHelperWritePermissionReason Reason = EBlueprintHelperWritePermissionReason::Unknown;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("enabled"), bEnabled);
		Json->SetStringField(TEXT("reason"), WritePermissionReasonToString(Reason));
		return Json;
	}
};

// ─── 5.6 FBlueprintHelperRiskCommandState ───

/** 高风险命令状态。 */
struct FBlueprintHelperRiskCommandState
{
	/** 高风险命令是否启用。 */
	bool bEnabled = false;

	/** 稳定原因码。 */
	EBlueprintHelperRiskCommandReason Reason = EBlueprintHelperRiskCommandReason::Unknown;

	/** 当前被阻止的高风险命令列表。 */
	TArray<FString> BlockedCommands;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("enabled"), bEnabled);
		Json->SetStringField(TEXT("reason"), RiskCommandReasonToString(Reason));
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& Cmd : BlockedCommands) { Arr.Add(MakeShared<FJsonValueString>(Cmd)); }
		Json->SetArrayField(TEXT("blocked_commands"), Arr);
		return Json;
	}
};

// ─── 5.7 FBlueprintHelperActiveProfileState ───

/** 当前激活的 Safety Profile 状态。 */
struct FBlueprintHelperActiveProfileState
{
	/** 当前安全档位。 */
	EBlueprintHelperSafetyProfile SafetyProfile = EBlueprintHelperSafetyProfile::Conservative;

	/** 缺失能力默认处理策略。 */
	EBlueprintHelperMissingCapabilityPolicy MissingCapabilityPolicy = EBlueprintHelperMissingCapabilityPolicy::StopAndReport;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("safety_profile"), SafetyProfileToString(SafetyProfile));
		Json->SetStringField(TEXT("missing_capability_policy"), MissingCapabilityPolicyToString(MissingCapabilityPolicy));
		return Json;
	}
};

// ─── 5.2 FBlueprintHelperRuntimeProfileData ───

/**
 * Runtime Profile 实际数据。
 * 作为 FBlueprintHelperToolResultBase::Data 的 payload。
 */
struct FBlueprintHelperRuntimeProfileData
{
	/** 固定为 BlueprintHelper.RuntimeProfile.v1。 */
	static constexpr const TCHAR* SchemaString = TEXT("RuntimeProfile.v1");

	/** BlueprintHelper 绑定发布版本。 */
	FString Version;

	/** UE Bridge 当前状态。 */
	EBlueprintHelperBridgeStatus BridgeStatus = EBlueprintHelperBridgeStatus::Unknown;

	/** 配置粗状态。 */
	EBlueprintHelperConfigStatus ConfigStatus = EBlueprintHelperConfigStatus::Unknown;

	/** 写权限状态。 */
	FBlueprintHelperWritePermissionState WritePermission;

	/** 高风险命令状态。 */
	FBlueprintHelperRiskCommandState RiskCommand;

	/** 当前 Safety Profile 与缺失能力策略。 */
	FBlueprintHelperActiveProfileState ActiveProfile;

	/** 不可用能力列表。 */
	FBlueprintHelperToolCapabilitiesState ToolCapabilities;

	/** 序列化到 JSON（即 data.* 的内容）。 */
	FString GetRuntimeStatus() const
	{
		if (BridgeStatus == EBlueprintHelperBridgeStatus::Disconnected ||
			BridgeStatus == EBlueprintHelperBridgeStatus::EditorNotRunning)
			return TEXT("blocked");
		if (ConfigStatus == EBlueprintHelperConfigStatus::ConfigUnavailable)
			return TEXT("blocked");
		if (!WritePermission.bEnabled)
			return TEXT("degraded");
		if (!RiskCommand.bEnabled && RiskCommand.BlockedCommands.Num() > 0)
			return TEXT("degraded");
		return TEXT("ok");
	}

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), SchemaString);

		TSharedRef<FJsonObject> Profile = MakeShared<FJsonObject>();
		const FString Status = GetRuntimeStatus();
		Profile->SetStringField(TEXT("status"), Status);

		if (Status == TEXT("ok"))
		{
			Json->SetObjectField(TEXT("runtime_profile"), Profile);
			return Json;
		}

		if (BridgeStatus != EBlueprintHelperBridgeStatus::Connected)
		{
			TSharedRef<FJsonObject> B = MakeShared<FJsonObject>();
			B->SetStringField(TEXT("status"), BridgeStatusToString(BridgeStatus));
			Profile->SetObjectField(TEXT("bridge"), B);
		}
		if (ConfigStatus == EBlueprintHelperConfigStatus::ConfigUnavailable)
		{
			TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
			C->SetStringField(TEXT("status"), ConfigStatusToString(ConfigStatus));
			Profile->SetObjectField(TEXT("config_status"), C);
		}
		if (!WritePermission.bEnabled)
			Profile->SetObjectField(TEXT("write_permission"), WritePermission.ToJson());
		if (!RiskCommand.bEnabled && RiskCommand.BlockedCommands.Num() > 0)
			Profile->SetObjectField(TEXT("risk_command"), RiskCommand.ToJson());
		if (ToolCapabilities.Unavailable.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const auto& Item : ToolCapabilities.Unavailable)
				Arr.Add(MakeShared<FJsonValueObject>(Item.ToJson()));
			Profile->SetArrayField(TEXT("unavailable"), Arr);
		}

		Json->SetObjectField(TEXT("runtime_profile"), Profile);
		return Json;
	}
};

#pragma endregion Runtime Profile Structs
