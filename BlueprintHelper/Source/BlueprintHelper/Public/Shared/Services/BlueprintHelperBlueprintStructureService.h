// BlueprintHelper Service Layer — 蓝图结构查询与操作服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"

class FBlueprintHelperGraphResolver;
class UBlueprint;
class UEdGraph;

// ─── 图表摘要 ───

/** 单个图表的摘要信息。 */
struct BLUEPRINTHELPER_API FBlueprintHelperGraphInfo
{
	/** 图表名称。 */
	FString Name;

	/** 图表类型：EventGraph / Function / Macro / AnimGraph 等。 */
	FString GraphType;

	/** 节点数量。 */
	int32 NodeCount = 0;

	/** 函数是否为 Pure（仅 Function 图表有效）。 */
	bool bIsPure = false;
};

// ─── 变量摘要 ───

/** 单个成员变量的摘要信息。 */
struct BLUEPRINTHELPER_API FBlueprintHelperVariableInfo
{
	/** 变量名称。 */
	FString Name;

	/** 引脚类型类别（如 bool / int / float / object / struct）。 */
	FString TypeCategory;

	/** 子类型对象路径（如 /Script/Engine.Actor）。 */
	FString SubCategoryObject;

	/** 容器类型（None / Array / Set / Map）。 */
	FString ContainerType;

	/** 默认值字符串。 */
	FString DefaultValue;

	/** 分类标签。 */
	FString Category;

	/** 是否可在蓝图中编辑。 */
	bool bIsEditable = true;
};

// ─── 事件分发器摘要 ───

/** 单个事件分发器的摘要信息。 */
struct BLUEPRINTHELPER_API FBlueprintHelperEventDispatcherInfo
{
	/** 分发器名称。 */
	FString Name;

	/** 参数列表（名称:类型）。 */
	TArray<FString> Params;
};

// ─── 查询结果 ───

struct BLUEPRINTHELPER_API FBlueprintHelperListGraphsResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TArray<FBlueprintHelperGraphInfo> Graphs;
};

struct BLUEPRINTHELPER_API FBlueprintHelperListVariablesResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TArray<FBlueprintHelperVariableInfo> Variables;
};

struct BLUEPRINTHELPER_API FBlueprintHelperListDispatchersResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	TArray<FBlueprintHelperEventDispatcherInfo> Dispatchers;
};

/**
 * 蓝图结构查询与精细操作服务。
 * 提供图表列表、变量列表、独立增删变量/图表/分发器等操作。
 */
class BLUEPRINTHELPER_API FBlueprintHelperBlueprintStructureService
{
public:
	explicit FBlueprintHelperBlueprintStructureService(const FBlueprintHelperGraphResolver& InResolver);

#pragma region API
	/** 列出蓝图中所有图表。 */
	FBlueprintHelperListGraphsResult ListGraphs(const FBlueprintHelperGraphTarget& Target) const;

	/** 列出蓝图中所有成员变量。 */
	FBlueprintHelperListVariablesResult ListVariables(const FBlueprintHelperGraphTarget& Target) const;

	/** 列出蓝图中所有事件分发器。 */
	FBlueprintHelperListDispatchersResult ListEventDispatchers(const FBlueprintHelperGraphTarget& Target) const;

	/** 添加成员变量（委托给 service implementation）。 */
	bool AddVariable(const FBlueprintHelperGraphTarget& Target, const TSharedPtr<class FJsonObject>& Params, FString& OutError) const;

	/** 删除成员变量（委托给 service implementation）。 */
	bool RemoveVariable(const FBlueprintHelperGraphTarget& Target, const FString& VarName, FString& OutError) const;

	/** 添加函数图表（委托给 service implementation）。 */
	bool AddGraph(const FBlueprintHelperGraphTarget& Target, const TSharedPtr<class FJsonObject>& Params, FString& OutError) const;

	/** 删除图表（委托给 service implementation）。 */
	bool RemoveGraph(const FBlueprintHelperGraphTarget& Target, const FString& GraphName, FString& OutError) const;

	/** 添加事件分发器（委托给 service implementation）。 */
	bool AddEventDispatcher(const FBlueprintHelperGraphTarget& Target, const TSharedPtr<class FJsonObject>& Params, FString& OutError) const;

	/** 删除指定图表中的节点。 */
	bool DeleteNodes(const FBlueprintHelperGraphTarget& Target, const TArray<FString>& NodeIds, int32& OutDeletedCount, FString& OutError) const;
#pragma endregion API

private:
	/** 解析蓝图目标并添加诊断。 */
	UBlueprint* ResolveBP(const FBlueprintHelperGraphTarget& Target, FString& OutError) const;

	const FBlueprintHelperGraphResolver& Resolver;
};
