#include "Misc/AutomationTest.h"
#include "Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Logic/BlueprintHelperLogicJsonReadService.h"
#include "Logic/BlueprintHelperLogicProcessor.h"
#include "Structure/BlueprintHelperLogicMdTypes.h"
#include "Structure/BlueprintHelperToolResultTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    TSharedRef<FJsonObject> MakeLogicTestEventNode(
        const FString& Id,
        const FString& Type,
        const FString& Name,
        const FString& EventName)
    {
        TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
        Node->SetStringField(TEXT("id"), Id);
        Node->SetStringField(TEXT("type"), Type);
        Node->SetStringField(TEXT("name"), Name);

        TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
        Event->SetStringField(TEXT("event_name"), EventName);
        Node->SetObjectField(TEXT("event"), Event);
        return Node;
    }

    TSharedRef<FJsonObject> MakeLogicTestGraph(
        const FString& GraphName,
        const TArray<TSharedPtr<FJsonValue>>& Nodes)
    {
        TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
        Graph->SetStringField(TEXT("graph"), GraphName);
        Graph->SetArrayField(TEXT("nodes"), Nodes);
        Graph->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());
        return Graph;
    }

    TSharedPtr<FJsonObject> MakeTestRawJsonObject()
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), TEXT("2.2"));
        Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

        TArray<TSharedPtr<FJsonValue>> Nodes;
        TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
        Node->SetStringField(TEXT("id"), TEXT("node_01"));
        Node->SetStringField(TEXT("name"), TEXT("TestNode"));
        Node->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
        Nodes.Add(MakeShared<FJsonValueObject>(Node));
        Root->SetArrayField(TEXT("nodes"), Nodes);

        Root->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());

        return Root;
    }

    TSharedPtr<FJsonObject> MakeCustomEventInCustomGraphRawJsonObject()
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), TEXT("2.2"));
        Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

        TArray<TSharedPtr<FJsonValue>> EventGraphNodes;
        EventGraphNodes.Add(MakeShared<FJsonValueObject>(
            MakeLogicTestEventNode(TEXT("event_graph_entry"), TEXT("K2Node_CustomEvent"), TEXT("EventGraphDoor"), TEXT("EventGraphDoor"))));

        TArray<TSharedPtr<FJsonValue>> CustomGraphNodes;
        CustomGraphNodes.Add(MakeShared<FJsonValueObject>(
            MakeLogicTestEventNode(TEXT("custom_graph_entry"), TEXT("K2Node_CustomEvent"), TEXT("OpenDoor"), TEXT("OpenDoor"))));

        TArray<TSharedPtr<FJsonValue>> Graphs;
        Graphs.Add(MakeShared<FJsonValueObject>(MakeLogicTestGraph(TEXT("EventGraph"), EventGraphNodes)));
        Graphs.Add(MakeShared<FJsonValueObject>(MakeLogicTestGraph(TEXT("EG_DoorFeature"), CustomGraphNodes)));
        Root->SetArrayField(TEXT("graphs"), Graphs);

        return Root;
    }

    FString MakeLogicServiceTestObjectName(const FString& Prefix)
    {
        return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }

    UBlueprint* MakeLogicServiceBlueprint(const FString& Prefix)
    {
        UPackage* Package = CreatePackage(*FString::Printf(
            TEXT("/Game/BlueprintHelperLogic/%s"),
            *MakeLogicServiceTestObjectName(Prefix)));
        Package->SetDirtyFlag(false);

        UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
            AActor::StaticClass(),
            Package,
            *MakeLogicServiceTestObjectName(TEXT("BP_LogicJsonRead")),
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass(),
            TEXT("BlueprintHelperObjectFirstLogicTests"));
        Package->SetDirtyFlag(false);
        return Blueprint;
    }

    UEdGraph* FindLogicServiceGraph(UBlueprint* Blueprint, const FString& GraphName)
    {
        if (!Blueprint)
        {
            return nullptr;
        }

        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
            {
                return Graph;
            }
        }
        return nullptr;
    }

    UEdGraph* AddLogicServiceUbergraph(UBlueprint* Blueprint, const FString& GraphName)
    {
        if (!Blueprint)
        {
            return nullptr;
        }

        UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
            Blueprint,
            FName(*GraphName),
            UEdGraph::StaticClass(),
            UEdGraphSchema_K2::StaticClass());
        if (!Graph)
        {
            return nullptr;
        }

        FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
        Blueprint->GetOutermost()->SetDirtyFlag(false);
        return Graph;
    }

    UK2Node_CustomEvent* AddLogicServiceCustomEvent(UEdGraph* Graph, const FString& EventName)
    {
        if (!Graph)
        {
            return nullptr;
        }

        UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
        Graph->AddNode(EventNode, true, false);
        EventNode->CreateNewGuid();
        EventNode->PostPlacedNewNode();
        EventNode->CustomFunctionName = FName(*EventName);
        EventNode->NodePosX = 0;
        EventNode->NodePosY = 0;
        EventNode->AllocateDefaultPins();
        return EventNode;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_ObjectAndStringProduceIdenticalStats,
    "BlueprintHelper.ObjectFirst.Logic.ObjectAndStringProduceIdenticalStats",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_ObjectAndStringProduceIdenticalStats::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> RootObj = MakeTestRawJsonObject();
    FString JsonText = [&]() {
        FString Out;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
        FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
        return Out;
    }();

    FBlueprintHelperLogicOptions Options;
    Options.Format = EBlueprintHelperLogicOutputFormat::LogicJson;

    FBlueprintHelperLogicResult StringResult = FBlueprintHelperLogicProcessor::ProcessRawJson(JsonText, Options);
    FBlueprintHelperLogicResult ObjectResult = FBlueprintHelperLogicProcessor::ProcessRawJsonObject(RootObj, Options);

    TestTrue(TEXT("String path succeeds"), StringResult.bSuccess);
    TestTrue(TEXT("Object path succeeds"), ObjectResult.bSuccess);
    TestEqual(TEXT("Identical NodeCount"), StringResult.NodeCount, ObjectResult.NodeCount);
    TestEqual(TEXT("Identical ExecLinkCount"), StringResult.ExecLinkCount, ObjectResult.ExecLinkCount);
    TestEqual(TEXT("Identical DataLinkCount"), StringResult.DataLinkCount, ObjectResult.DataLinkCount);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_ObjectAndStringProduceIdenticalMarkdown,
    "BlueprintHelper.ObjectFirst.Logic.ObjectAndStringProduceIdenticalMarkdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_ObjectAndStringProduceIdenticalMarkdown::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> RootObj = MakeTestRawJsonObject();
    FString JsonText = [&]() {
        FString Out;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
        FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
        return Out;
    }();

    FBlueprintHelperLogicOptions Options;
    Options.Format = EBlueprintHelperLogicOutputFormat::Markdown;

    FBlueprintHelperLogicResult StringResult = FBlueprintHelperLogicProcessor::ProcessRawJson(JsonText, Options);
    FBlueprintHelperLogicResult ObjectResult = FBlueprintHelperLogicProcessor::ProcessRawJsonObject(RootObj, Options);

    TestTrue(TEXT("String path succeeds"), StringResult.bSuccess);
    TestTrue(TEXT("Object path succeeds"), ObjectResult.bSuccess);
    TestEqual(TEXT("Identical NodeCount"), StringResult.NodeCount, ObjectResult.NodeCount);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_InvalidObjectReturnsError,
    "BlueprintHelper.ObjectFirst.Logic.InvalidObjectReturnsError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_InvalidObjectReturnsError::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicOptions Options;
    FBlueprintHelperLogicResult Result = FBlueprintHelperLogicProcessor::ProcessRawJsonObject(nullptr, Options);

    TestFalse(TEXT("Null object returns failure"), Result.bSuccess);
    TestFalse(TEXT("Error message is not empty"), Result.ErrorMessage.IsEmpty());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_StringWrapperHandlesEmpty,
    "BlueprintHelper.ObjectFirst.Logic.StringWrapperHandlesEmpty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_StringWrapperHandlesEmpty::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicOptions Options;
    FBlueprintHelperLogicResult Result = FBlueprintHelperLogicProcessor::ProcessRawJson(TEXT(""), Options);

    TestFalse(TEXT("Empty string returns failure"), Result.bSuccess);
    TestTrue(TEXT("Error mentions empty"), Result.ErrorMessage.Contains(TEXT("empty")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_CustomEventTargetUsesCustomGraph,
    "BlueprintHelper.ObjectFirst.Logic.CustomEventTargetUsesCustomGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_CustomEventTargetUsesCustomGraph::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildTargetEntry(
        MakeCustomEventInCustomGraphRawJsonObject(),
        TEXT("/Game/BP/BP_PhysicsDoor"),
        TEXT("EG_DoorFeature"),
        TEXT("OpenDoor"),
        EBlueprintHelperLogicScope::TargetCustomEvent);

    TestEqual(TEXT("custom_event target resolves the requested custom graph"), Payload.Graph, FString(TEXT("EG_DoorFeature")));
    TestTrue(TEXT("custom_event target has an entry"), Payload.Entry.IsSet());
    if (Payload.Entry.IsSet())
    {
        TestEqual(TEXT("custom_event target entry is OpenDoor"), Payload.Entry->Name, FString(TEXT("OpenDoor")));
        TestEqual(TEXT("custom_event target entry kind is custom_event"), Payload.Entry->Kind, EBlueprintHelperLogicNodeKind::CustomEvent);
    }
    TestEqual(TEXT("custom_event target does not return EventGraph nodes"), Payload.Nodes[0].Name, FString(TEXT("OpenDoor")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_ReadLogicJsonCustomEventScansCustomGraphs,
    "BlueprintHelper.ObjectFirst.Logic.ReadLogicJsonCustomEventScansCustomGraphs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_ReadLogicJsonCustomEventScansCustomGraphs::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = MakeLogicServiceBlueprint(TEXT("ReadLogicJsonCustomEvent"));
    TestNotNull(TEXT("test Blueprint is created"), Blueprint);
    if (!Blueprint)
    {
        return false;
    }

    UEdGraph* EventGraph = FindLogicServiceGraph(Blueprint, TEXT("EventGraph"));
    TestNotNull(TEXT("EventGraph exists"), EventGraph);
    UEdGraph* CustomGraph = AddLogicServiceUbergraph(Blueprint, TEXT("EG_DoorFeature"));
    TestNotNull(TEXT("custom graph is created"), CustomGraph);
    if (!EventGraph || !CustomGraph)
    {
        return false;
    }

    TestNotNull(TEXT("EventGraph custom event is created"),
        AddLogicServiceCustomEvent(EventGraph, TEXT("EventGraphDoor")));
    TestNotNull(TEXT("custom graph custom event is created"),
        AddLogicServiceCustomEvent(CustomGraph, TEXT("OpenDoor")));

    FBlueprintHelperTargetRef Target;
    Target.AssetPath = Blueprint->GetPathName();
    Target.TargetType = EBlueprintHelperTargetType::CustomEvent;
    Target.Event = TEXT("OpenDoor");

    FBlueprintHelperLogicJsonReadService ReadService;
    const FBlueprintHelperLogicJsonData Data = ReadService.ReadLogicJson(Target);

    TestEqual(TEXT("ReadLogicJson resolves custom_event from custom graph"),
        Data.Logic.Graph,
        FString(TEXT("EG_DoorFeature")));
    TestTrue(TEXT("ReadLogicJson returns custom_event entry"), Data.Logic.Entry.IsSet());
    if (Data.Logic.Entry.IsSet())
    {
        TestEqual(TEXT("ReadLogicJson entry name is OpenDoor"),
            Data.Logic.Entry->Name,
            FString(TEXT("OpenDoor")));
        TestEqual(TEXT("ReadLogicJson entry kind is custom_event"),
            Data.Logic.Entry->Kind,
            EBlueprintHelperLogicNodeKind::CustomEvent);
    }

    return true;
}

#endif
