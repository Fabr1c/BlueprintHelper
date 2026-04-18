// BlueprintHelper v2.1 — Remove Graph Operation Handler

#pragma once

#include "OperationHandlers/BlueprintOperationHandler.h"

/**
 * 处理 remove_graph 操作：从蓝图中删除函数图或宏图。
 * 注意：不允许删除 EventGraph。
 */
class FRemoveGraphHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<class FJsonObject>& OpPayload, FString& OutError) override;
};
