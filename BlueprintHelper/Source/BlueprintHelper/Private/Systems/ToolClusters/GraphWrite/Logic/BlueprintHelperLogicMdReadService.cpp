// BlueprintHelper Service Layer — LogicMD Read Service 实现

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.h"
#include "Shared/BlueprintHelperLogicMdTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.h"

FBlueprintHelperLogicMdReadService::FBlueprintHelperLogicMdReadService()
{
	OwnedGraphResolver = MakeUnique<FBlueprintHelperGraphResolver>();
	OwnedExportService = MakeUnique<FBlueprintHelperExportService>(*OwnedGraphResolver);
	ExportService = OwnedExportService.Get();
}

FBlueprintHelperLogicMdReadService::FBlueprintHelperLogicMdReadService(
	const FBlueprintHelperExportService& InExportService)
	: ExportService(&InExportService)
{
}

FBlueprintHelperLogicMdReadService::~FBlueprintHelperLogicMdReadService() = default;

class FBlueprintHelperLogicMdReadServiceLocalUtils
{
public:
	static bool IsTargetEntryScope(EBlueprintHelperLogicScope Scope)
	{
		return Scope == EBlueprintHelperLogicScope::TargetFunction
			|| Scope == EBlueprintHelperLogicScope::TargetEvent
			|| Scope == EBlueprintHelperLogicScope::TargetCustomEvent;
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

	static const TCHAR* GetTargetTitle(EBlueprintHelperLogicScope Scope)
	{
		switch (Scope)
		{
		case EBlueprintHelperLogicScope::TargetFunction:
			return TEXT("Function");
		case EBlueprintHelperLogicScope::TargetEvent:
			return TEXT("Event");
		case EBlueprintHelperLogicScope::TargetCustomEvent:
			return TEXT("Custom Event");
		default:
			return TEXT("Target");
		}
	}

	static int32 CountLinks(const TArray<FBlueprintHelperLogicNode>& Nodes, EBlueprintHelperLogicLinkType Type)
	{
		int32 Count = 0;
		for (const FBlueprintHelperLogicNode& Node : Nodes)
		{
			for (const FBlueprintHelperLogicLink& Link : Node.Links)
			{
				if (Link.Type == Type)
				{
					++Count;
				}
			}
		}
		return Count;
	}

	static int32 CountOrphanNodes(const FBlueprintHelperLogicJsonPayload& Payload)
	{
		TSet<FString> LinkedNodeRefs;
		const FString EntryNodeRef = Payload.Entry.IsSet() ? Payload.Entry->NodeRef : TEXT("");

		for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
		{
			for (const FBlueprintHelperLogicLink& Link : Node.Links)
			{
				if (!Node.NodeRef.IsEmpty())
				{
					LinkedNodeRefs.Add(Node.NodeRef);
				}
				if (!Link.ToNode.IsEmpty())
				{
					LinkedNodeRefs.Add(Link.ToNode);
				}
			}
		}

		int32 Count = 0;
		for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
		{
			if (!Node.NodeRef.IsEmpty()
				&& !Node.NodeRef.Equals(EntryNodeRef, ESearchCase::IgnoreCase)
				&& !LinkedNodeRefs.Contains(Node.NodeRef))
			{
				++Count;
			}
		}
		return Count;
	}

	static void AppendLine(FString& Output, const FString& Line = TEXT(""))
	{
		Output += Line;
		Output += TEXT("\n");
	}

	static FString FormatNodeLine(const FBlueprintHelperLogicNode& Node)
	{
		if (Node.Owner.IsEmpty())
		{
			return FString::Printf(
				TEXT("- %s [%s] %s"),
				*Node.NodeRef,
				LogicNodeKindToString(Node.Kind),
				*Node.Name);
		}

		return FString::Printf(
			TEXT("- %s [%s] %s owner=%s"),
			*Node.NodeRef,
			LogicNodeKindToString(Node.Kind),
			*Node.Name,
			*Node.Owner);
	}

	static FString FormatEntryLine(const FBlueprintHelperLogicEntry& Entry)
	{
		return FString::Printf(
			TEXT("- %s [%s] %s"),
			*Entry.NodeRef,
			LogicNodeKindToString(Entry.Kind),
			*Entry.Name);
	}

	static void FillStatsFromPayload(const FBlueprintHelperLogicJsonPayload& Payload, FBlueprintHelperLogicMdStats& Stats)
	{
		Stats.Nodes = Payload.Nodes.Num();
		Stats.ExecLinks = CountLinks(Payload.Nodes, EBlueprintHelperLogicLinkType::Exec);
		Stats.DataLinks = CountLinks(Payload.Nodes, EBlueprintHelperLogicLinkType::Data);
		Stats.OrphanNodes = CountOrphanNodes(Payload);
	}

	static FString BuildTargetEntryMarkdown(
		const FBlueprintHelperLogicJsonPayload& Payload,
		EBlueprintHelperLogicScope Scope,
		const FString& TargetName,
		const FBlueprintHelperLogicMdStats& Stats)
	{
		FString Output;
		const FString EffectiveTargetName = !TargetName.IsEmpty()
			? TargetName
			: (Payload.Entry.IsSet() ? Payload.Entry->Name : TEXT("<unnamed>"));

		AppendLine(Output, TEXT("# Logic Graph"));
		AppendLine(Output);
		AppendLine(Output, FString::Printf(TEXT("%s: %s"), GetTargetTitle(Scope), *EffectiveTargetName));
		if (!Payload.Graph.IsEmpty())
		{
			AppendLine(Output, FString::Printf(TEXT("Graph: %s"), *Payload.Graph));
		}
		AppendLine(Output, FString::Printf(
			TEXT("Nodes: %d | Exec Links: %d | Data Links: %d | Orphans: %d"),
			Stats.Nodes,
			Stats.ExecLinks,
			Stats.DataLinks,
			Stats.OrphanNodes));

		AppendLine(Output);
		AppendLine(Output, TEXT("## Entry"));
		if (Payload.Entry.IsSet())
		{
			AppendLine(Output, FormatEntryLine(Payload.Entry.GetValue()));
		}
		else
		{
			AppendLine(Output, TEXT("- <missing>"));
		}

		AppendLine(Output);
		AppendLine(Output, TEXT("## Nodes"));
		if (Payload.Nodes.Num() == 0)
		{
			AppendLine(Output, TEXT("- None"));
		}
		else
		{
			for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
			{
				AppendLine(Output, FormatNodeLine(Node));
			}
		}

		AppendLine(Output);
		AppendLine(Output, TEXT("## Execution"));
		bool bHasExec = false;
		for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
		{
			for (const FBlueprintHelperLogicLink& Link : Node.Links)
			{
				if (Link.Type != EBlueprintHelperLogicLinkType::Exec)
				{
					continue;
				}
				bHasExec = true;
				AppendLine(Output, FString::Printf(
					TEXT("- %s.%s -> %s.%s"),
					*Node.NodeRef,
					*Link.FromPin,
					*Link.ToNode,
					*Link.ToPin));
			}
		}
		if (!bHasExec)
		{
			AppendLine(Output, TEXT("- None"));
		}

		if (Stats.DataLinks > 0)
		{
			AppendLine(Output);
			AppendLine(Output, TEXT("## Data Dependencies"));
			for (const FBlueprintHelperLogicNode& Node : Payload.Nodes)
			{
				for (const FBlueprintHelperLogicLink& Link : Node.Links)
				{
					if (Link.Type != EBlueprintHelperLogicLinkType::Data)
					{
						continue;
					}
					AppendLine(Output, FString::Printf(
						TEXT("- %s.%s -> %s.%s"),
						*Node.NodeRef,
						*Link.FromPin,
						*Link.ToNode,
						*Link.ToPin));
				}
			}
		}

		return Output;
	}
};

FBlueprintHelperLogicMdData FBlueprintHelperLogicMdReadService::ReadLogicMd(const FBlueprintHelperTargetRef& Target) const
{
	FBlueprintHelperLogicMdData Data;

	// 确定 scope
	const EBlueprintHelperLogicScope Scope = TargetTypeToScope(Target.TargetType);
	Data.Scope = Scope;
	const bool bTargetEntryScope = FBlueprintHelperLogicMdReadServiceLocalUtils::IsTargetEntryScope(Scope);

	// 准备 ExportRequest
	FBlueprintHelperExportRequest ExportReq;
	ExportReq.Target.BlueprintPath = Target.AssetPath;
	if (!Target.Graph.IsEmpty())
	{
		ExportReq.Target.GraphName = Target.Graph;
	}
	ExportReq.Scope = bTargetEntryScope && Target.Graph.IsEmpty()
		? EBlueprintHelperExportScope::FullBlueprint
		: ScopeToExportScope(Scope);

	// 使用模块的 ExportService 导出 Raw JSON
	const FBlueprintHelperExportResult ExportResult = ExportService->Export(ExportReq);

	if (!ExportResult.bSuccess)
	{
		// 导出失败，返回空 Markdown 标记失败
		Data.Markdown = TEXT("(导出失败)");
		Data.bImportable = false;
		return Data;
	}

	// 通过 LogicProcessor 生成 Markdown
	if (bTargetEntryScope)
	{
		const FString TargetName = FBlueprintHelperLogicMdReadServiceLocalUtils::GetTargetEntryName(Target, Scope);
		const FBlueprintHelperLogicJsonPayload Payload = GroupBuilder.BuildTargetEntry(
			ExportResult.JsonObject,
			Target.AssetPath,
			Target.Graph,
			TargetName,
			Scope);

		FBlueprintHelperLogicMdStats& Stats = Data.Stats;
		FBlueprintHelperLogicMdReadServiceLocalUtils::FillStatsFromPayload(Payload, Stats);
		Data.Markdown = FBlueprintHelperLogicMdReadServiceLocalUtils::BuildTargetEntryMarkdown(
			Payload,
			Scope,
			TargetName,
			Stats);
		Data.bImportable = false;
		return Data;
	}

	FBlueprintHelperLogicOptions LogicOptions;
	LogicOptions.Format = EBlueprintHelperLogicOutputFormat::Markdown;
	LogicOptions.DetailLevel = EBlueprintHelperLogicDetailLevel::Normal;
	LogicOptions.bIncludeDataDependencies = true;
	LogicOptions.bIncludeOrphanNodes = true;
	LogicOptions.bIncludeNodeIds = false;
	LogicOptions.bIncludePositions = false;
	LogicOptions.bIncludeRawNodeTypes = false;

	const FBlueprintHelperLogicResult LogicResult =
		FBlueprintHelperLogicProcessor::ProcessRawJsonObject(ExportResult.JsonObject, LogicOptions);

	if (!LogicResult.bSuccess)
	{
		Data.Markdown = TEXT("(Logic Processor 失败)");
		Data.bImportable = false;
		return Data;
	}

	// 填充 Markdown
	Data.Markdown = LogicResult.OutputText;
	Data.bImportable = false;

	// ─── 构建 scope 收敛的 stats ───
	FBlueprintHelperLogicMdStats& Stats = Data.Stats;
	Stats.Nodes = LogicResult.NodeCount;
	Stats.ExecLinks = LogicResult.ExecLinkCount;
	Stats.DataLinks = LogicResult.DataLinkCount;
	Stats.OrphanNodes = LogicResult.OrphanNodeCount;

	// 多入口 scope 下使用 group builder 构建分组并设置 grouped
	const bool bIsMultiEntry = FBlueprintHelperLogicGroupBuilder::IsMultiEntryScope(Scope);
	if (bIsMultiEntry)
	{
		const FString GraphName = Target.Graph.IsEmpty() ? TEXT("EventGraph") : Target.Graph;
		const FBlueprintHelperLogicJsonPayload GroupPayload = GroupBuilder.BuildGroups(
			ExportResult.JsonObject, Target.AssetPath, GraphName, Scope);
		Stats.Groups = GroupPayload.Groups.Num();
		Data.bGrouped = true;
		// Markdown is already generated by LogicProcessor, we just add grouped flag
	}

	switch (Scope)
	{
	case EBlueprintHelperLogicScope::Blueprint:
		Stats.Graphs = 1;
		Stats.Functions = 0;
		Stats.Events = LogicResult.EntryPointCount;
		Data.bGrouped = true;
		break;

	case EBlueprintHelperLogicScope::TargetGraph:
		Stats.Events = LogicResult.EntryPointCount;
		Data.bGrouped = true;
		break;

	case EBlueprintHelperLogicScope::TargetFunction:
	case EBlueprintHelperLogicScope::TargetEvent:
	case EBlueprintHelperLogicScope::TargetCustomEvent:
	case EBlueprintHelperLogicScope::TargetBlock:
		// 只返回基础 node/exec/data/orphan 统计
		break;

	case EBlueprintHelperLogicScope::MultiTarget:
		Stats.Targets = 1;
		break;
	}

	return Data;
}

EBlueprintHelperLogicScope FBlueprintHelperLogicMdReadService::TargetTypeToScope(EBlueprintHelperTargetType Type)
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

EBlueprintHelperExportScope FBlueprintHelperLogicMdReadService::ScopeToExportScope(EBlueprintHelperLogicScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperLogicScope::Blueprint:
		return EBlueprintHelperExportScope::FullBlueprint;
	case EBlueprintHelperLogicScope::TargetGraph:
	case EBlueprintHelperLogicScope::TargetFunction:
	case EBlueprintHelperLogicScope::TargetEvent:
	case EBlueprintHelperLogicScope::TargetCustomEvent:
	case EBlueprintHelperLogicScope::TargetBlock:
	default:
		return EBlueprintHelperExportScope::SingleGraph;
	}
}
