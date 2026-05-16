// BlueprintHelper Service Layer — LogicJson Read Service 实现

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h"
#include "Shared/BlueprintHelperLogicMdTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.h"

FBlueprintHelperLogicJsonReadService::FBlueprintHelperLogicJsonReadService()
{
	OwnedGraphResolver = MakeUnique<FBlueprintHelperGraphResolver>();
	OwnedExportService = MakeUnique<FBlueprintHelperExportService>(*OwnedGraphResolver);
	ExportService = OwnedExportService.Get();
}

FBlueprintHelperLogicJsonReadService::FBlueprintHelperLogicJsonReadService(
	const FBlueprintHelperExportService& InExportService)
	: ExportService(&InExportService)
{
}

FBlueprintHelperLogicJsonReadService::~FBlueprintHelperLogicJsonReadService() = default;

class FBlueprintHelperLogicJsonReadServiceLocalUtils
{
public:
	static bool IsTargetEntryScope(EBlueprintHelperLogicScope Scope)
	{
		return Scope == EBlueprintHelperLogicScope::TargetFunction ||
			Scope == EBlueprintHelperLogicScope::TargetEvent ||
			Scope == EBlueprintHelperLogicScope::TargetCustomEvent;
	}

	static FString GetTargetEntryName(const FBlueprintHelperTargetRef& Target, EBlueprintHelperLogicScope Scope)
	{
		switch (Scope)
		{
		case EBlueprintHelperLogicScope::TargetFunction:
			return Target.Function;
		case EBlueprintHelperLogicScope::TargetEvent:
		case EBlueprintHelperLogicScope::TargetCustomEvent:
			return Target.Event;
		default:
			return TEXT("");
		}
	}

};

FBlueprintHelperLogicJsonData FBlueprintHelperLogicJsonReadService::ReadLogicJson(const FBlueprintHelperTargetRef& Target) const
{
	FBlueprintHelperLogicJsonData Data;

	const EBlueprintHelperLogicScope Scope = TargetTypeToScope(Target.TargetType);
	Data.Scope = Scope;
	Data.bImportable = false;

	// 准备 ExportRequest
	FBlueprintHelperExportRequest ExportReq;
	ExportReq.Target.BlueprintPath = Target.AssetPath;
	if (!Target.Graph.IsEmpty())
	{
		ExportReq.Target.GraphName = Target.Graph;
	}
	ExportReq.Scope = FBlueprintHelperLogicJsonReadServiceLocalUtils::IsTargetEntryScope(Scope) && Target.Graph.IsEmpty()
		? EBlueprintHelperExportScope::FullBlueprint
		: ScopeToExportScope(Scope);

	const FBlueprintHelperExportResult ExportResult = ExportService->Export(ExportReq);

	if (!ExportResult.bSuccess || !ExportResult.JsonObject.IsValid())
	{
		return Data;
	}

	// 使用 GroupBuilder 构建分组内容。单入口 target 在未指定图表时扫描完整蓝图，
	// 避免 custom_event / function / event 读回被默认 EventGraph 吞掉。
	const FString GraphName = Target.Graph.IsEmpty() ? TEXT("EventGraph") : Target.Graph;
	if (FBlueprintHelperLogicJsonReadServiceLocalUtils::IsTargetEntryScope(Scope))
	{
		Data.Logic = GroupBuilder.BuildTargetEntry(
			ExportResult.JsonObject,
			Target.AssetPath,
			Target.Graph,
			FBlueprintHelperLogicJsonReadServiceLocalUtils::GetTargetEntryName(Target, Scope),
			Scope);
	}
	else
	{
		Data.Logic = GroupBuilder.BuildGroups(ExportResult.JsonObject, Target.AssetPath, GraphName, Scope);
	}

	// ─── Stats ───
	// 通过 LogicProcessor 获取统计
	FBlueprintHelperLogicOptions LogicOptions;
	LogicOptions.Format = EBlueprintHelperLogicOutputFormat::Markdown;
	LogicOptions.DetailLevel = EBlueprintHelperLogicDetailLevel::Normal;
	const FBlueprintHelperLogicResult LogicResult =
		FBlueprintHelperLogicProcessor::ProcessRawJsonObject(ExportResult.JsonObject, LogicOptions);

	FBlueprintHelperLogicMdStats& Stats = Data.Stats;
	Stats.Nodes = LogicResult.NodeCount;
	Stats.ExecLinks = LogicResult.ExecLinkCount;
	Stats.DataLinks = LogicResult.DataLinkCount;
	Stats.OrphanNodes = LogicResult.OrphanNodeCount;

	if (FBlueprintHelperLogicGroupBuilder::IsMultiEntryScope(Scope))
	{
		Stats.Groups = Data.Logic.Groups.Num();
	}

	switch (Scope)
	{
	case EBlueprintHelperLogicScope::Blueprint:
		Stats.Graphs = 1;
		Stats.Functions = 0;
		Stats.Events = LogicResult.EntryPointCount;
		break;
	case EBlueprintHelperLogicScope::TargetGraph:
		Stats.Events = LogicResult.EntryPointCount;
		break;
	default:
		break;
	}

	return Data;
}

EBlueprintHelperLogicScope FBlueprintHelperLogicJsonReadService::TargetTypeToScope(EBlueprintHelperTargetType Type)
{
	switch (Type)
	{
	case EBlueprintHelperTargetType::Blueprint:   return EBlueprintHelperLogicScope::Blueprint;
	case EBlueprintHelperTargetType::Graph:        return EBlueprintHelperLogicScope::TargetGraph;
	case EBlueprintHelperTargetType::Function:     return EBlueprintHelperLogicScope::TargetFunction;
	case EBlueprintHelperTargetType::Event:        return EBlueprintHelperLogicScope::TargetEvent;
	case EBlueprintHelperTargetType::CustomEvent:  return EBlueprintHelperLogicScope::TargetCustomEvent;
	case EBlueprintHelperTargetType::Block:        return EBlueprintHelperLogicScope::TargetBlock;
	default:                                       return EBlueprintHelperLogicScope::TargetGraph;
	}
}

EBlueprintHelperExportScope FBlueprintHelperLogicJsonReadService::ScopeToExportScope(EBlueprintHelperLogicScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperLogicScope::Blueprint:
		return EBlueprintHelperExportScope::FullBlueprint;
	default:
		return EBlueprintHelperExportScope::SingleGraph;
	}
}
