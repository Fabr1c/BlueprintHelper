// BlueprintHelper v1.2 — Node Handler Strategy Pattern

#pragma once

#include "CoreMinimal.h"

class UK2Node;
class UEdGraph;
struct FParsedNode;
enum class EParsedBlueprintNodeType : uint8;

/**
 * 节点解析中间数据基类。
 * v1.2 阶段仅定义结构，实际分发仍使用 FParsedNode。
 * v1.3+ 将逐步迁移至子类化数据模型。
 */
struct FParsedNodeBase
{
	FString Id;
	FString SourceType;
	float X = 0.f;
	float Y = 0.f;
	TMap<FString, FString> DefaultValues;
	virtual ~FParsedNodeBase() = default;
};

/**
 * 蓝图节点处理器接口。
 * 每种节点类型（CallFunction、VariableGet 等）实现此接口，
 * 由 FBlueprintNodeHandlerRegistry 统一管理和分发。
 */
class IBlueprintNodeHandler
{
public:
	virtual ~IBlueprintNodeHandler() = default;

	/** 判断此处理器是否可处理指定类型的节点。 */
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const = 0;

	/** 在目标图表中生成节点。返回生成的 UK2Node，失败时返回 nullptr 并填写 OutError。 */
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const = 0;
};

/**
 * 节点处理器注册表单例，管理所有 IBlueprintNodeHandler 实例。
 * 模块启动时注册内置处理器，GenerateBlueprintFromJson 通过此注册表分发节点生成。
 */
class BLUEPRINTHELPER_API FBlueprintNodeHandlerRegistry
{
public:
	/** 获取全局唯一实例。 */
	static FBlueprintNodeHandlerRegistry& Get();

	/** 注册一个节点处理器。后注册的处理器优先匹配。 */
	void Register(TSharedRef<IBlueprintNodeHandler> Handler);

	/** 查找可处理指定节点类型的处理器，未找到时返回 nullptr。 */
	IBlueprintNodeHandler* FindHandler(EParsedBlueprintNodeType NodeType) const;

	/** 清空所有已注册的处理器（供测试和关闭使用）。 */
	void Reset();

private:
	TArray<TSharedRef<IBlueprintNodeHandler>> Handlers;
};
