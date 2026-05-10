// BlueprintHelper Service Layer — Diagnostics 服务（Runtime）

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperDiagnosticsData;

/**
 * 只读诊断服务。
 * 负责执行 Runtime 诊断检查并构建 Markdown 报告。
 * Static 诊断由 MCP 侧独立执行，不经过此服务。
 * 不负责：写文件、修复配置、启动编辑器、判断具体蓝图任务。
 */
class BLUEPRINTHELPER_API FBlueprintHelperDiagnosticsService
{
public:
	FBlueprintHelperDiagnosticsService();

	/** 执行 Runtime 诊断，返回 Markdown 格式报告。 */
	FBlueprintHelperDiagnosticsData RunRuntimeDiagnostics() const;

private:
	/** 获取插件版本号。 */
	static FString GetPluginVersion();

	/** 读取 Safety Profile 配置。 */
	static FString GetSafetyProfileStr();

	/** 判断项目是否包含 Project Marker (MrStone/.claude/CLAUDE.md 或 equivalent)。 */
	static bool HasProjectMarker();
};
