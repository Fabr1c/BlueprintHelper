// BlueprintHelper Service Layer — LogicMD Read Service

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperLogicMdTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotService.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"

struct FBlueprintHelperLogicMdData;
struct FBlueprintHelperTargetRef;
class FBlueprintHelperExportService;
class FBlueprintHelperGraphResolver;

/**
 * LogicMD 只读服务。
 * 根据 target 读取蓝图逻辑并以 Agent 友好的 Markdown 格式返回。
 * 多入口 scope 下按 group 分段，并返回 grouped=true。
 * 不负责：导入、导出、写入、Editor undo、Review。
 */
class BLUEPRINTHELPER_API FBlueprintHelperLogicMdReadService
{
public:
	FBlueprintHelperLogicMdReadService();
	explicit FBlueprintHelperLogicMdReadService(const FBlueprintHelperExportService& InExportService);
	~FBlueprintHelperLogicMdReadService();

	/**
	 * 根据 TargetRef 读取 LogicMD。
	 */
	FBlueprintHelperLogicMdData ReadLogicMd(
		const FBlueprintHelperTargetRef& Target,
		FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache = nullptr) const;

	bool BuildSnapshot(
		const FBlueprintHelperTargetRef& Target,
		FBlueprintHelperLogicReadSnapshot& OutSnapshot,
		FString& OutError,
		FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache = nullptr) const;

	FBlueprintHelperLogicMdData FormatSnapshot(
		const FBlueprintHelperLogicReadSnapshot& Snapshot) const;

private:
	TUniquePtr<FBlueprintHelperGraphResolver> OwnedGraphResolver;
	TUniquePtr<FBlueprintHelperExportService> OwnedExportService;
	const FBlueprintHelperExportService* ExportService = nullptr;
	TUniquePtr<FBlueprintHelperLogicReadSnapshotService> SnapshotService;
	TUniquePtr<FBlueprintHelperLogicReadSnapshotFormatter> Formatter;
};
