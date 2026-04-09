// BlueprintHelper v2.0 — Add Event Dispatcher Operation Handler

#pragma once

#include "OperationHandlers/BlueprintOperationHandler.h"

/**
 * 处理 add_event_dispatcher 操作：在蓝图中创建事件分发器（多播委托成员变量）。
 */
class FAddEventDispatcherHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<class FJsonObject>& OpPayload, FString& OutError) override;
};
