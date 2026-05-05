// BlueprintHelper Service Layer — 图表定位器

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperServiceTypes.h"

class UEdGraph;
class UBlueprint;
class FBlueprintEditor;

/**
 * 图表定位器，按 FBlueprintHelperGraphTarget 统一解析蓝图与图表。
 * 支持按资产路径定位（打开编辑器），或降级到当前焦点图表。
 */
class BLUEPRINTHELPER_API FBlueprintHelperGraphResolver
{
public:
	/** 解析 Target，返回目标图表。失败时 OutDiag 有错误。 */
	UEdGraph* ResolveGraph(const FBlueprintHelperGraphTarget& Target, FBlueprintHelperDiagnosticSet& OutDiag) const;

	/** 解析 Target，返回目标蓝图。失败时 OutDiag 有错误。 */
	UBlueprint* ResolveBlueprint(const FBlueprintHelperGraphTarget& Target, FBlueprintHelperDiagnosticSet& OutDiag) const;

	/** 获取当前焦点图表（降级路径）。 */
	UEdGraph* GetFocusedGraph() const;

	/** 获取当前焦点蓝图（降级路径）。 */
	UBlueprint* GetFocusedBlueprint() const;

private:
	/** 按路径加载蓝图资产。 */
	UBlueprint* LoadBlueprintByPath(const FString& AssetPath, FBlueprintHelperDiagnosticSet& OutDiag) const;

	/** 确保蓝图编辑器已打开并获取编辑器实例。 */
	FBlueprintEditor* EnsureBlueprintEditorOpen(UBlueprint* Blueprint, FBlueprintHelperDiagnosticSet& OutDiag) const;

	/** 在蓝图中查找指定名称的图表。 */
	UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& GraphName, FBlueprintHelperDiagnosticSet& OutDiag) const;
};
