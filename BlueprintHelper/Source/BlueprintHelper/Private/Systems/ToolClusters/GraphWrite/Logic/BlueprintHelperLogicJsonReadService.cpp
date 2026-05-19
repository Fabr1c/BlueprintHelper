// BlueprintHelper Service Layer - LogicJson read orchestration.

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h"

#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

FBlueprintHelperLogicJsonReadService::FBlueprintHelperLogicJsonReadService()
{
	OwnedGraphResolver = MakeUnique<FBlueprintHelperGraphResolver>();
	OwnedExportService = MakeUnique<FBlueprintHelperExportService>(*OwnedGraphResolver);
	ExportService = OwnedExportService.Get();
	SnapshotService = MakeUnique<FBlueprintHelperLogicReadSnapshotService>(*ExportService);
	Formatter = MakeUnique<FBlueprintHelperLogicReadSnapshotFormatter>();
}

FBlueprintHelperLogicJsonReadService::FBlueprintHelperLogicJsonReadService(
	const FBlueprintHelperExportService& InExportService)
	: ExportService(&InExportService)
{
	SnapshotService = MakeUnique<FBlueprintHelperLogicReadSnapshotService>(InExportService);
	Formatter = MakeUnique<FBlueprintHelperLogicReadSnapshotFormatter>();
}

FBlueprintHelperLogicJsonReadService::~FBlueprintHelperLogicJsonReadService() = default;

FBlueprintHelperLogicJsonData FBlueprintHelperLogicJsonReadService::ReadLogicJson(
	const FBlueprintHelperTargetRef& Target,
	FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache) const
{
	FBlueprintHelperLogicReadSnapshot Snapshot;
	FString Error;
	if (!BuildSnapshot(Target, Snapshot, Error, RequestCache))
	{
		return FBlueprintHelperLogicJsonData();
	}
	return FormatSnapshot(Snapshot);
}

bool FBlueprintHelperLogicJsonReadService::BuildSnapshot(
	const FBlueprintHelperTargetRef& Target,
	FBlueprintHelperLogicReadSnapshot& OutSnapshot,
	FString& OutError,
	FBlueprintHelperLogicReadRequestSnapshotCache* RequestCache) const
{
	return SnapshotService
		? SnapshotService->BuildSnapshot(Target, OutSnapshot, OutError, RequestCache)
		: false;
}

FBlueprintHelperLogicJsonData FBlueprintHelperLogicJsonReadService::FormatSnapshot(
	const FBlueprintHelperLogicReadSnapshot& Snapshot) const
{
	return Formatter
		? Formatter->BuildLogicJsonData(Snapshot)
		: FBlueprintHelperLogicJsonData();
}
