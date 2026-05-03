// BlueprintHelper Service Layer — LogicJson Read Service 实现

#include "Services/BlueprintHelperLogicJsonReadService.h"
#include "Services/BlueprintHelperLogicMdTypes.h"
#include "Services/BlueprintHelperLogicGroupBuilder.h"
#include "Services/BlueprintHelperToolResultTypes.h"
#include "Services/BlueprintHelperServiceTypes.h"
#include "Services/BlueprintHelperExportService.h"
#include "Services/BlueprintHelperGraphResolver.h"
#include "Services/BlueprintHelperLogicProcessor.h"
#include "BlueprintHelper.h"

FBlueprintHelperLogicJsonReadService::FBlueprintHelperLogicJsonReadService() = default;

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
	ExportReq.Scope = ScopeToExportScope(Scope);

	const FBlueprintHelperModule& Module = FBlueprintHelperModule::Get();
	const FBlueprintHelperExportResult ExportResult = Module.GetExportService().Export(ExportReq);

	if (!ExportResult.bSuccess || !ExportResult.JsonObject.IsValid())
	{
		return Data;
	}

	// 使用 GroupBuilder 构建分组内容
	const FString GraphName = Target.Graph.IsEmpty() ? TEXT("EventGraph") : Target.Graph;
	Data.Logic = GroupBuilder.BuildGroups(ExportResult.JsonObject, Target.AssetPath, GraphName, Scope);

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
