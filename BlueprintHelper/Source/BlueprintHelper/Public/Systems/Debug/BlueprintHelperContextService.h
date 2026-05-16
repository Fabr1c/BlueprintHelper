// BlueprintHelper Service Layer — 编辑器上下文查询服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperGraphResolver;

/**
 * 编辑器上下文服务，提供当前蓝图编辑器状态的结构化快照。
 */
class BLUEPRINTHELPER_API FBlueprintHelperContextService
{
public:
	explicit FBlueprintHelperContextService(const FBlueprintHelperGraphResolver& InResolver);

	/** 获取当前编辑器上下文快照。必须在 GameThread 调用。 */
	FBlueprintHelperEditorContext GetContext() const;

private:
	const FBlueprintHelperGraphResolver& Resolver;
};
