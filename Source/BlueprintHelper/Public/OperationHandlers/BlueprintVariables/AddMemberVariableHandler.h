// BlueprintHelper v2.0 — Add Member Variable Operation Handler

#pragma once

#include "OperationHandlers/BlueprintOperationHandler.h"

/**
 * 处理 add_member_variable 操作：在蓝图中创建成员变量。
 */
class FAddMemberVariableHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<class FJsonObject>& OpPayload, FString& OutError) override;
};
