// BlueprintHelper v2.0 — Blueprint Operation Handler Strategy Pattern

#pragma once

#include "CoreMinimal.h"

class UBlueprint;

/**
 * 蓝图级操作处理器接口。
 * 与节点 Handler（IBlueprintNodeHandler）平行，用于在节点操作之前
 * 处理蓝图级操作（创建变量、函数图、事件分发器等）。
 */
class IBlueprintOperationHandler
{
public:
	virtual ~IBlueprintOperationHandler() = default;

	/** 判断此处理器是否可处理指定操作名。 */
	virtual bool CanHandle(const FString& OpName) const = 0;

	/** 执行蓝图级操作。成功返回 true，失败返回 false 并填写 OutError。 */
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<class FJsonObject>& OpPayload, FString& OutError) = 0;
};

/**
 * 蓝图操作处理器注册表单例。
 * 模块启动时注册内置操作处理器，GenerateBlueprintFromJson 通过此注册表分发蓝图级操作。
 */
class BLUEPRINTHELPER_API FBlueprintOperationHandlerRegistry
{
public:
	/** 获取全局唯一实例。 */
	static FBlueprintOperationHandlerRegistry& Get();

	/** 注册一个操作处理器。 */
	void Register(TSharedRef<IBlueprintOperationHandler> Handler);

	/** 查找可处理指定操作名的处理器，未找到时返回 nullptr。 */
	IBlueprintOperationHandler* FindHandler(const FString& OpName) const;

	/** 清空所有已注册的处理器（供测试和关闭使用）。 */
	void Reset();

private:
	TArray<TSharedRef<IBlueprintOperationHandler>> Handlers;
};
