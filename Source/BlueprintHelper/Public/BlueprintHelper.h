// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class UEdGraph;
class FSpawnTabArgs;

/**
 * BlueprintHelper 模块，负责注册编辑器窗口与提供当前蓝图图表访问能力。
 */
class BLUEPRINTHELPER_API FBlueprintHelperModule : public IModuleInterface
{
public:
	/** 获取模块实例。 */
	static FBlueprintHelperModule& Get();

	/** 模块启动，注册页签与菜单入口。 */
	virtual void StartupModule() override;

	/** 模块关闭，注销页签与菜单入口。 */
	virtual void ShutdownModule() override;

	/** 打开插件主窗口。 */
	void OpenMainWindow();

	/** 获取当前激活的蓝图图表。 */
	UEdGraph* GetActiveBlueprintGraph() const;

	/** 读取 Json -> 蓝图规则 Markdown。 */
	FString GetJsonToBlueprintRuleMarkdown() const;

private:
	/** 注册编辑器菜单。 */
	void RegisterMenus();

	/** 生成插件主页签。 */
	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

private:
	/** 插件主页签名称。 */
	static const FName HelperTabName;
};
