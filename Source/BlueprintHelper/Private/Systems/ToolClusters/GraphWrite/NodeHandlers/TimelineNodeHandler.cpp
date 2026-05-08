#include "Systems/ToolClusters/GraphWrite/NodeHandlers/TimelineNodeHandler.h"

#include "K2Node_Timeline.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"
#include "EdGraphSchema_K2.h"
#include "Engine/TimelineTemplate.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool FTimelineNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::Timeline;
}

UK2Node* FTimelineNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Timeline 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	if (!Blueprint)
	{
		OutError = TEXT("Timeline 节点生成失败：无法获取蓝图。");
		return nullptr;
	}

	const FParsedTimelineReference& TLRef = NodeData.TimelineReference;
	FName TimelineName = TLRef.TimelineName.IsEmpty()
		? FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint)
		: FName(*TLRef.TimelineName);

	// 创建 UTimelineTemplate，注册到蓝图。Timelines 数组
	UTimelineTemplate* TimelineTemplate = FBlueprintEditorUtils::AddNewTimeline(Blueprint, TimelineName);
	if (!TimelineTemplate)
	{
		OutError = FString::Printf(TEXT("Timeline 节点生成失败：无法创建时间轴模板 '%s'。"), *TimelineName.ToString());
		return nullptr;
	}

	TimelineTemplate->bAutoPlay           = TLRef.bAutoPlay;
	TimelineTemplate->bLoop               = TLRef.bLoop;

	// 添加 Float 轨道
	for (const FString& TrackName : TLRef.FloatTracks)
	{
		FTTFloatTrack NewTrack;
		NewTrack.SetTrackName(FName(*TrackName), TimelineTemplate);
		TimelineTemplate->FloatTracks.Add(NewTrack);
	}

	// 添加 Vector 轨道
	for (const FString& TrackName : TLRef.VectorTracks)
	{
		FTTVectorTrack NewTrack;
		NewTrack.SetTrackName(FName(*TrackName), TimelineTemplate);
		TimelineTemplate->VectorTracks.Add(NewTrack);
	}

	// 添加 Event 轨道
	for (const FString& TrackName : TLRef.EventTracks)
	{
		FTTEventTrack NewTrack;
		NewTrack.SetTrackName(FName(*TrackName), TimelineTemplate);
		TimelineTemplate->EventTracks.Add(NewTrack);
	}

	// 创建蓝图图表节点
	UK2Node_Timeline* TimelineNode = NewObject<UK2Node_Timeline>(TargetGraph);
	TimelineNode->TimelineName = TimelineName;
	TargetGraph->AddNode(TimelineNode, true, false);
	TimelineNode->CreateNewGuid();
	TimelineNode->PostPlacedNewNode();
	TimelineNode->NodePosX = static_cast<int32>(NodeData.X);
	TimelineNode->NodePosY = static_cast<int32>(NodeData.Y);
	TimelineNode->AllocateDefaultPins();

	TextToBlueprintGenerator::ApplyDefaultValues(TimelineNode, NodeData.DefaultValues);

	return TimelineNode;
}
