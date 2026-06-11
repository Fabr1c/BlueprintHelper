#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotFormatter.h"

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.h"

class FBlueprintHelperLogicReadSnapshotFormatterLocalUtils
{
public:
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

	static void FillStatsFromPayload(
		const FBlueprintHelperLogicJsonPayload& Payload,
		FBlueprintHelperLogicStats& Stats)
	{
		Stats.Nodes = Payload.Nodes.Num();
		Stats.ExecLinks = CountLinks(Payload.Nodes, EBlueprintHelperLogicLinkType::Exec);
		Stats.DataLinks = CountLinks(Payload.Nodes, EBlueprintHelperLogicLinkType::Data);
		Stats.OrphanNodes = CountOrphanNodes(Payload);
	}

	static void FillStatsFromLogicResult(
		const FBlueprintHelperLogicResult& LogicResult,
		FBlueprintHelperLogicStats& Stats)
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
	Data.AdapterBoundaryJson = Snapshot.AdapterBoundaryJson;
	if (!Data.AdapterBoundaryJson.IsValid() && Snapshot.RawJsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* AdapterBoundaryJson = nullptr;
		if (Snapshot.RawJsonObject->TryGetObjectField(TEXT("adapter_boundary"), AdapterBoundaryJson)
			&& AdapterBoundaryJson
			&& AdapterBoundaryJson->IsValid())
		{
			Data.AdapterBoundaryJson = *AdapterBoundaryJson;
		}
	}

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

	Data.Logic = GroupBuilder.BuildGroups(
		Snapshot.RawJsonObject,
		Snapshot.AssetPath,
		Snapshot.GraphName,
		Snapshot.Scope);

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

bool FBlueprintHelperLogicReadSnapshotFormatter::BuildLogicJsonPayload(
	const FBlueprintHelperLogicReadSnapshot& Snapshot,
	TSharedPtr<FJsonObject>& OutPayload,
	FString& OutError) const
{
	OutError.Reset();
	OutPayload = BuildLogicJsonData(Snapshot).ToJson();
	return true;
}
