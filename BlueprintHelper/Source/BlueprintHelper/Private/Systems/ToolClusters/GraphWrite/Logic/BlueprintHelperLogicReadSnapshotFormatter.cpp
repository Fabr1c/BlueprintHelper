#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h"

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.h"

class FBlueprintHelperLogicReadSnapshotFormatterLocalUtils
{
public:
	static const TCHAR* GetTargetTitle(EBlueprintHelperLogicScope Scope)
	{
		if (Scope == EBlueprintHelperLogicScope::TargetFunction)
		{
			return TEXT("Function");
		}
		if (Scope == EBlueprintHelperLogicScope::TargetEvent)
		{
			return TEXT("Event");
		}
		if (Scope == EBlueprintHelperLogicScope::TargetCustomEvent)
		{
			return TEXT("Custom Event");
		}
		return TEXT("Target");
	}

	static int32 CountLinks(
		const TArray<FBlueprintHelperLogicNode>& Nodes,
		EBlueprintHelperLogicLinkType Type)
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

	static void FillStatsFromPayload(
		const FBlueprintHelperLogicJsonPayload& Payload,
		FBlueprintHelperLogicMdStats& Stats)
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

	static void FillStatsFromLogicResult(
		const FBlueprintHelperLogicResult& LogicResult,
		FBlueprintHelperLogicMdStats& Stats)
	{
		Stats.Nodes = LogicResult.NodeCount;
		Stats.ExecLinks = LogicResult.ExecLinkCount;
		Stats.DataLinks = LogicResult.DataLinkCount;
		Stats.OrphanNodes = LogicResult.OrphanNodeCount;
	}
};

FBlueprintHelperLogicReadSnapshotFormatter::FBlueprintHelperLogicReadSnapshotFormatter()
{
	FormatHandlers.Add(TEXT("logic_json"), [this](
		const FBlueprintHelperLogicReadSnapshot& Snapshot,
		TSharedPtr<FJsonObject>& OutPayload,
		FString& OutError)
	{
		return BuildLogicJsonPayload(Snapshot, OutPayload, OutError);
	});
	FormatHandlers.Add(TEXT("logic_md"), [this](
		const FBlueprintHelperLogicReadSnapshot& Snapshot,
		TSharedPtr<FJsonObject>& OutPayload,
		FString& OutError)
	{
		return BuildLogicMdPayload(Snapshot, OutPayload, OutError);
	});
}

bool FBlueprintHelperLogicReadSnapshotFormatter::BuildFormattedPayload(
	const FString& Format,
	const FBlueprintHelperLogicReadSnapshot& Snapshot,
	TSharedPtr<FJsonObject>& OutPayload,
	FString& OutError) const
{
	OutPayload.Reset();
	OutError.Reset();

	const FBlueprintHelperLogicSnapshotFormatHandler* Handler = FormatHandlers.Find(Format);
	if (!Handler)
	{
		OutError = FString::Printf(TEXT("Unsupported read format: %s"), *Format);
		return false;
	}
	return (*Handler)(Snapshot, OutPayload, OutError);
}

FBlueprintHelperLogicJsonData FBlueprintHelperLogicReadSnapshotFormatter::BuildLogicJsonData(
	const FBlueprintHelperLogicReadSnapshot& Snapshot) const
{
	FBlueprintHelperLogicJsonData Data;
	Data.Scope = Snapshot.Scope;
	Data.bImportable = false;

	if (!Snapshot.bExportSucceeded || !Snapshot.RawJsonObject.IsValid())
	{
		return Data;
	}

	if (Snapshot.bTargetEntryScope)
	{
		Data.Logic = GroupBuilder.BuildTargetEntry(
			Snapshot.RawJsonObject,
			Snapshot.AssetPath,
			Snapshot.Target.Graph,
			Snapshot.TargetEntryName,
			Snapshot.Scope);
		FBlueprintHelperLogicReadSnapshotFormatterLocalUtils::FillStatsFromPayload(Data.Logic, Data.Stats);
		return Data;
	}
	else
	{
		Data.Logic = GroupBuilder.BuildGroups(
			Snapshot.RawJsonObject,
			Snapshot.AssetPath,
			Snapshot.GraphName,
			Snapshot.Scope);
	}

	FBlueprintHelperLogicOptions LogicOptions;
	LogicOptions.Format = EBlueprintHelperLogicOutputFormat::Markdown;
	LogicOptions.DetailLevel = EBlueprintHelperLogicDetailLevel::Normal;
	const FBlueprintHelperLogicResult LogicResult =
		FBlueprintHelperLogicProcessor::ProcessRawJsonObject(Snapshot.RawJsonObject, LogicOptions);

	FBlueprintHelperLogicReadSnapshotFormatterLocalUtils::FillStatsFromLogicResult(LogicResult, Data.Stats);
	if (FBlueprintHelperLogicGroupBuilder::IsMultiEntryScope(Snapshot.Scope))
	{
		Data.Stats.Groups = Data.Logic.Groups.Num();
	}

	if (Snapshot.Scope == EBlueprintHelperLogicScope::Blueprint)
	{
		Data.Stats.Graphs = 1;
		Data.Stats.Functions = 0;
		Data.Stats.Events = LogicResult.EntryPointCount;
	}
	else if (Snapshot.Scope == EBlueprintHelperLogicScope::TargetGraph)
	{
		Data.Stats.Events = LogicResult.EntryPointCount;
	}

	return Data;
}

FBlueprintHelperLogicMdData FBlueprintHelperLogicReadSnapshotFormatter::BuildLogicMdData(
	const FBlueprintHelperLogicReadSnapshot& Snapshot) const
{
	FBlueprintHelperLogicMdData Data;
	Data.Scope = Snapshot.Scope;

	if (!Snapshot.bExportSucceeded || !Snapshot.RawJsonObject.IsValid())
	{
		Data.Markdown = TEXT("(导出失败)");
		Data.bImportable = false;
		return Data;
	}

	if (Snapshot.bTargetEntryScope)
	{
		const FBlueprintHelperLogicJsonPayload Payload = GroupBuilder.BuildTargetEntry(
			Snapshot.RawJsonObject,
			Snapshot.AssetPath,
			Snapshot.Target.Graph,
			Snapshot.TargetEntryName,
			Snapshot.Scope);

		FBlueprintHelperLogicReadSnapshotFormatterLocalUtils::FillStatsFromPayload(Payload, Data.Stats);
		Data.Markdown = FBlueprintHelperLogicReadSnapshotFormatterLocalUtils::BuildTargetEntryMarkdown(
			Payload,
			Snapshot.Scope,
			Snapshot.TargetEntryName,
			Data.Stats);
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
		FBlueprintHelperLogicProcessor::ProcessRawJsonObject(Snapshot.RawJsonObject, LogicOptions);

	if (!LogicResult.bSuccess)
	{
		Data.Markdown = TEXT("(Logic Processor 失败)");
		Data.bImportable = false;
		return Data;
	}

	Data.Markdown = LogicResult.OutputText;
	Data.bImportable = false;
	FBlueprintHelperLogicReadSnapshotFormatterLocalUtils::FillStatsFromLogicResult(LogicResult, Data.Stats);

	const bool bIsMultiEntry = FBlueprintHelperLogicGroupBuilder::IsMultiEntryScope(Snapshot.Scope);
	if (bIsMultiEntry)
	{
		const FBlueprintHelperLogicJsonPayload GroupPayload = GroupBuilder.BuildGroups(
			Snapshot.RawJsonObject,
			Snapshot.AssetPath,
			Snapshot.GraphName,
			Snapshot.Scope);
		Data.Stats.Groups = GroupPayload.Groups.Num();
		Data.bGrouped = true;
	}

	if (Snapshot.Scope == EBlueprintHelperLogicScope::Blueprint)
	{
		Data.Stats.Graphs = 1;
		Data.Stats.Functions = 0;
		Data.Stats.Events = LogicResult.EntryPointCount;
		Data.bGrouped = true;
	}
	else if (Snapshot.Scope == EBlueprintHelperLogicScope::TargetGraph)
	{
		Data.Stats.Events = LogicResult.EntryPointCount;
		Data.bGrouped = true;
	}
	else if (Snapshot.Scope == EBlueprintHelperLogicScope::MultiTarget)
	{
		Data.Stats.Targets = 1;
	}

	return Data;
}

bool FBlueprintHelperLogicReadSnapshotFormatter::BuildLogicJsonPayload(
	const FBlueprintHelperLogicReadSnapshot& Snapshot,
	TSharedPtr<FJsonObject>& OutPayload,
	FString& OutError) const
{
	OutError.Reset();
	OutPayload = BuildLogicJsonData(Snapshot).ToJson();
	return true;
}

bool FBlueprintHelperLogicReadSnapshotFormatter::BuildLogicMdPayload(
	const FBlueprintHelperLogicReadSnapshot& Snapshot,
	TSharedPtr<FJsonObject>& OutPayload,
	FString& OutError) const
{
	OutError.Reset();
	OutPayload = BuildLogicMdData(Snapshot).ToJson();
	return true;
}
