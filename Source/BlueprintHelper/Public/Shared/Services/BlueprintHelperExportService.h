// BlueprintHelper Service Layer — 导出服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"

class FBlueprintHelperGraphResolver;

/**
 * 导出服务，封装从蓝图图表导出 JSON 的完整流程。
 */
class BLUEPRINTHELPER_API FBlueprintHelperExportService
{
public:
	explicit FBlueprintHelperExportService(const FBlueprintHelperGraphResolver& InResolver);

	/** 将蓝图/图表导出为 JSON。 */
	FBlueprintHelperExportResult Export(const FBlueprintHelperExportRequest& Request) const;

private:
	const FBlueprintHelperGraphResolver& Resolver;
};
