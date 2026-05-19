// BlueprintHelper Service Layer - LogicMD read orchestration.

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.h"

#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

FBlueprintHelperLogicMdReadService::FBlueprintHelperLogicMdReadService()
{
	OwnedGraphResolver = MakeUnique<FBlueprintHelperGraphResolver>();
	OwnedExportService = MakeUnique<FBlueprintHelperExportService>(*OwnedGraphResolver);
	ExportService = OwnedExportService.Get();
	SnapshotService = MakeUnique<FBlueprintHelperLogicReadSnapshotService>(*ExportService);
	Formatter = MakeUnique<FBlueprintHelperLogicReadSnapshotFormatter>();
}

FBlueprintHelperLogicMdReadService::FBlueprintHelperLogicMdReadService(
	const FBlueprintHelperExportService& InExportService)
	: ExportService(&InExportService)
{
	SnapshotService = MakeUnique<FBlueprintHelperLogicReadSnapshotService>(InExportService);
	Formatter = MakeUnique<FBlueprintHelperLogicReadSnapshotFormatter>();
}

FBlueprintHelperLogicMdReadService::~FBlueprintHelperLogicMdReadService() = default;

FBlueprintHelperLogicMdData FBlueprintHelperLogicMdReadService::ReadLogicMd(
	const FBlueprintHelperTargetRef& Target,
	FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache) const
{
	FBlueprintHelperLogicReadSnapshot Snapshot;
	FString Error;
	if (!BuildSnapshot(Target, Snapshot, Error, RequestCache))
	{
		FBlueprintHelperLogicMdData Data;
		Data.Markdown = TEXT("(导出失败)");
		Data.bImportable = false;
		return Data;
	}
	return FormatSnapshot(Snapshot);
}

bool FBlueprintHelperLogicMdReadService::BuildSnapshot(
	const FBlueprintHelperTargetRef& Target,
	FBlueprintHelperLogicReadSnapshot& OutSnapshot,
	FString& OutError,
	FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache) const
{
	return SnapshotService
		? SnapshotService->BuildSnapshot(Target, OutSnapshot, OutError, RequestCache)
		: false;
}

FBlueprintHelperLogicMdData FBlueprintHelperLogicMdReadService::FormatSnapshot(
	const FBlueprintHelperLogicReadSnapshot& Snapshot) const
{
	return Formatter
		? Formatter->BuildLogicMdData(Snapshot)
		: FBlueprintHelperLogicMdData();
}
