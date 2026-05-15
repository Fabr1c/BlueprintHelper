// BlueprintHelper Service Layer — 图表快照服务

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * 图表快照服务。
 * 用于在 Replace 操作前捕获目标节点快照，失败时可通过快照恢复。
 * Snapshot 不进入 Agent-facing 结果，只写入 Journal rollback_data。
 */
struct FBlueprintHelperGraphSnapshot
{
	FString GraphName;
	TArray<FString> NodeGuids;
	TArray<FString> NodeClasses;
	TArray<FString> NodeTitles;
	TArray<FVector2D> NodePositions;
	TArray<FString> PinSummaries;
	TArray<FString> LinkSummaries;
	TArray<FString> NodeComments;
	TArray<FString> OwnershipMetadata;
	FString OwnerBlockId;
	FString EntryIdentity;
	FString ReplaceScope;
	FString ExportedText;

	bool IsEmpty() const { return NodeGuids.Num() == 0 && PinSummaries.Num() == 0 && LinkSummaries.Num() == 0; }

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("graph_name"), GraphName);
		if (!OwnerBlockId.IsEmpty()) Json->SetStringField(TEXT("owner_block_id"), OwnerBlockId);
		if (!EntryIdentity.IsEmpty()) Json->SetStringField(TEXT("entry_identity"), EntryIdentity);
		if (!ReplaceScope.IsEmpty()) Json->SetStringField(TEXT("replace_scope"), ReplaceScope);
		if (!ExportedText.IsEmpty()) Json->SetStringField(TEXT("exported_text"), ExportedText);

		auto StringArray = [](const TCHAR* Key, const TArray<FString>& Arr, TSharedRef<FJsonObject> J)
		{
			TArray<TSharedPtr<FJsonValue>> Vals;
			for (const FString& S : Arr) { Vals.Add(MakeShared<FJsonValueString>(S)); }
			J->SetArrayField(Key, Vals);
		};

		StringArray(TEXT("node_guids"), NodeGuids, Json);
		StringArray(TEXT("node_classes"), NodeClasses, Json);
		StringArray(TEXT("node_titles"), NodeTitles, Json);
		StringArray(TEXT("pin_summaries"), PinSummaries, Json);
		StringArray(TEXT("link_summaries"), LinkSummaries, Json);
		StringArray(TEXT("node_comments"), NodeComments, Json);
		StringArray(TEXT("ownership_metadata"), OwnershipMetadata, Json);

		return Json;
	}

	FString ToJsonString() const
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(ToJson(), Writer);
		return Output;
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphSnapshotService
{
public:
	/** 捕获指定节点集合的完整快照。 */
	FBlueprintHelperGraphSnapshot CaptureNodeSnapshot(
		UEdGraph* Graph,
		const TArray<UEdGraphNode*>& Nodes) const;

	/** 捕获整个图表所有节点的快照。 */
	FBlueprintHelperGraphSnapshot CaptureGraphSnapshot(
		UEdGraph* Graph) const;
};
