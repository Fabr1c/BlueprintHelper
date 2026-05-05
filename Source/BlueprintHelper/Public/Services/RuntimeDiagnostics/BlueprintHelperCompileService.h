// BlueprintHelper Service Layer — 编译服务

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperServiceTypes.h"

class FBlueprintHelperGraphResolver;

/**
 * 编译服务，触发蓝图编译并聚合编译结果为结构化 DTO。
 */
class BLUEPRINTHELPER_API FBlueprintHelperCompileService
{
public:
	explicit FBlueprintHelperCompileService(const FBlueprintHelperGraphResolver& InResolver);

	/** 编译指定蓝图，返回结构化结果。 */
	FBlueprintHelperCompileResult Compile(const FBlueprintHelperGraphTarget& Target) const;

	/** 获取蓝图当前编译状态（不触发编译）。 */
	FBlueprintHelperCompileResult GetStatus(const FBlueprintHelperGraphTarget& Target) const;

private:
	const FBlueprintHelperGraphResolver& Resolver;
};
