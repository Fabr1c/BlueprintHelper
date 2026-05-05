// BlueprintHelper v2.0 — Add Function Graph Operation Handler

#pragma once

#include "OperationHandlers/BlueprintOperationHandler.h"

/**
 * 处理 add_function_graph 操作：在蓝图中创建自定义函数图。
 */
class FAddFunctionGraphHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<class FJsonObject>& OpPayload, FString& OutError) override;
};
