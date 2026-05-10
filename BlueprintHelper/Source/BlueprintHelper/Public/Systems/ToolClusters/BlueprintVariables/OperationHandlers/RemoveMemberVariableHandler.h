// BlueprintHelper v2.1 — Remove Member Variable Operation Handler

#pragma once

#include "Systems/ToolClusters/GraphWrite/OperationHandlers/BlueprintOperationHandler.h"

/**
 * 处理 remove_member_variable 操作：从蓝图中删除成员变量（包括事件分发器）。
 */
class FRemoveMemberVariableHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<class FJsonObject>& OpPayload, FString& OutError) override;
};
