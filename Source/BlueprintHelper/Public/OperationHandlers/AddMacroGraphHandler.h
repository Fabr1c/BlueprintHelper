// BlueprintHelper v2.1 — Add Macro Graph Operation Handler

#pragma once

#include "OperationHandlers/BlueprintOperationHandler.h"

/**
 * 处理 add_macro_graph 操作：在蓝图中创建宏图。
 */
class FAddMacroGraphHandler : public IBlueprintOperationHandler
{
public:
	virtual bool CanHandle(const FString& OpName) const override;
	virtual bool Execute(UBlueprint* Blueprint, const TSharedPtr<class FJsonObject>& OpPayload, FString& OutError) override;
};
