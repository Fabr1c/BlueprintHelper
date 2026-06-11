// BlueprintHelper Service Layer — LogicJson Read Service

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperLogicReadTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotService.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"

struct FBlueprintHelperLogicJsonData;
struct FBlueprintHelperTargetRef;
class FBlueprintHelperExportService;
class FBlueprintHelperGraphResolver;

/**
 * LogicJson 只读服务。
 * 根据 target 读取蓝图逻辑并以结构化 JSON 返回。
 * 多入口 scope 下返回 groups[]，单入口 scope 返回 entry+nodes。
 * 不负责：导入、导出、写入、Editor undo、Review。
 */
class BLUEPRINTHELPER_API FBlueprintHelperLogicJsonReadService
{
public:
	FBlueprintHelperLogicJsonReadService();
	explicit FBlueprintHelperLogicJsonReadService(const FBlueprintHelperExportService& InExportService);
	~FBlueprintHelperLogicJsonReadService();

	/** 根据 TargetRef 读取 LogicJson。 */
	FBlueprintHelperLogicJsonData ReadLogicJson(
		const FBlueprintHelperTargetRef& Target,
		FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache = nullptr) const;

	bool BuildSnapshot(
		const FBlueprintHelperTargetRef& Target,
		FBlueprintHelperLogicReadSnapshot& OutSnapshot,
		FString& OutError,
		FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache = nullptr) const;

	FBlueprintHelperLogicJsonData FormatSnapshot(
		const FBlueprintHelperLogicReadSnapshot& Snapshot) const;

private:
	TUniquePtr<FBlueprintHelperGraphResolver> OwnedGraphResolver;
	TUniquePtr<FBlueprintHelperExportService> OwnedExportService;
	const FBlueprintHelperExportService* ExportService = nullptr;
	TUniquePtr<FBlueprintHelperLogicReadSnapshotService> SnapshotService;
	TUniquePtr<FBlueprintHelperLogicReadSnapshotFormatter> Formatter;
};
