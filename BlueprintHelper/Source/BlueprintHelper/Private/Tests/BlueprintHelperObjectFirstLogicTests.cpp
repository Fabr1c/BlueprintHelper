#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.h"
#include "Shared/BlueprintHelperLogicReadTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
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
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperObjectFirstLogicTestsLocalUtils
{
public:
    static TSharedRef<FJsonObject> MakeLogicTestEventNode(
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

    static TSharedRef<FJsonObject> MakeLogicTestGraph(
        const FString& GraphName,
        const TArray<TSharedPtr<FJsonValue>>& Nodes)
    {
        TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
        Graph->SetStringField(TEXT("graph"), GraphName);
        Graph->SetArrayField(TEXT("nodes"), Nodes);
        Graph->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());
        return Graph;
    }

    static TSharedRef<FJsonObject> MakeLogicTestGraphWithLinks(
        const FString& GraphName,
        const TArray<TSharedPtr<FJsonValue>>& Nodes,
        const TArray<TSharedPtr<FJsonValue>>& Links)
    {
        TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
        Graph->SetStringField(TEXT("graph"), GraphName);
        Graph->SetArrayField(TEXT("nodes"), Nodes);
        Graph->SetArrayField(TEXT("links"), Links);
        return Graph;
    }

    static TSharedPtr<FJsonObject> MakeTestRawJsonObject()
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

    static TSharedPtr<FJsonObject> MakeCustomEventInCustomGraphRawJsonObject()
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

    static TSharedPtr<FJsonObject> MakeFunctionInFunctionGraphRawJsonObject()
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), TEXT("2.2"));
        Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

        TArray<TSharedPtr<FJsonValue>> EventGraphNodes;
        EventGraphNodes.Add(MakeShared<FJsonValueObject>(
            MakeLogicTestEventNode(TEXT("event_graph_entry"), TEXT("K2Node_CustomEvent"), TEXT("EventGraphDoor"), TEXT("EventGraphDoor"))));

        TArray<TSharedPtr<FJsonValue>> FunctionNodes;
        TSharedRef<FJsonObject> FunctionEntry = MakeShared<FJsonObject>();
        FunctionEntry->SetStringField(TEXT("id"), TEXT("function_entry"));
        FunctionEntry->SetStringField(TEXT("type"), TEXT("K2Node_FunctionEntry"));
        FunctionEntry->SetStringField(TEXT("name"), TEXT("AddMazeRelativeRotation"));
        FunctionEntry->SetStringField(TEXT("function_name"), TEXT("AddMazeRelativeRotation"));
        FunctionNodes.Add(MakeShared<FJsonValueObject>(FunctionEntry));

        TSharedRef<FJsonObject> SetRelativeRotationNode = MakeShared<FJsonObject>();
        SetRelativeRotationNode->SetStringField(TEXT("id"), TEXT("set_relative_rotation"));
        SetRelativeRotationNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
        SetRelativeRotationNode->SetStringField(TEXT("name"), TEXT("SetRelativeRotation"));
        SetRelativeRotationNode->SetStringField(TEXT("function_name"), TEXT("SetRelativeRotation"));
        FunctionNodes.Add(MakeShared<FJsonValueObject>(SetRelativeRotationNode));

		TArray<TSharedPtr<FJsonValue>> Graphs;
		Graphs.Add(MakeShared<FJsonValueObject>(MakeLogicTestGraph(TEXT("EventGraph"), EventGraphNodes)));
		Graphs.Add(MakeShared<FJsonValueObject>(MakeLogicTestGraph(TEXT("AddMazeRelativeRotation"), FunctionNodes)));
		Root->SetArrayField(TEXT("graphs"), Graphs);

        return Root;
    }

    static TSharedPtr<FJsonObject> MakeExportedFunctionGraphWithoutEntryRawJsonObject()
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), TEXT("2.2"));
        Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

        TArray<TSharedPtr<FJsonValue>> EventGraphNodes;
        EventGraphNodes.Add(MakeShared<FJsonValueObject>(
            MakeLogicTestEventNode(TEXT("event_graph_entry"), TEXT("K2Node_CustomEvent"), TEXT("EventGraphDoor"), TEXT("EventGraphDoor"))));

        TArray<TSharedPtr<FJsonValue>> FunctionNodes;
        TSharedRef<FJsonObject> SetRelativeRotationNode = MakeShared<FJsonObject>();
        SetRelativeRotationNode->SetStringField(TEXT("id"), TEXT("set_relative_rotation"));
        SetRelativeRotationNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
        SetRelativeRotationNode->SetStringField(TEXT("name"), TEXT("SetRelativeRotation"));
        SetRelativeRotationNode->SetStringField(TEXT("function_name"), TEXT("SetRelativeRotation"));
        FunctionNodes.Add(MakeShared<FJsonValueObject>(SetRelativeRotationNode));

        TArray<TSharedPtr<FJsonValue>> FunctionLinks;
        TSharedRef<FJsonObject> EntryToBodyLink = MakeShared<FJsonObject>();
        EntryToBodyLink->SetStringField(TEXT("from_id"), TEXT("FunctionEntry"));
        EntryToBodyLink->SetStringField(TEXT("from_pin"), TEXT("then"));
        EntryToBodyLink->SetStringField(TEXT("to_id"), TEXT("set_relative_rotation"));
        EntryToBodyLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
        EntryToBodyLink->SetStringField(TEXT("kind"), TEXT("exec"));
        FunctionLinks.Add(MakeShared<FJsonValueObject>(EntryToBodyLink));

        TSharedRef<FJsonObject> BodyToResultLink = MakeShared<FJsonObject>();
        BodyToResultLink->SetStringField(TEXT("from_id"), TEXT("set_relative_rotation"));
        BodyToResultLink->SetStringField(TEXT("from_pin"), TEXT("then"));
        BodyToResultLink->SetStringField(TEXT("to_id"), TEXT("FunctionResult"));
        BodyToResultLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
        BodyToResultLink->SetStringField(TEXT("kind"), TEXT("exec"));
        FunctionLinks.Add(MakeShared<FJsonValueObject>(BodyToResultLink));

        TSharedRef<FJsonObject> AdapterBoundary = MakeShared<FJsonObject>();
        AdapterBoundary->SetStringField(TEXT("runtime_adapter_id"), TEXT("k2.function_body"));
        AdapterBoundary->SetStringField(TEXT("body_kind"), TEXT("k2.function_body"));
        AdapterBoundary->SetStringField(TEXT("graph_name"), TEXT("AddMazeRelativeRotation"));
        TArray<TSharedPtr<FJsonValue>> EntryBoundaries;
        TSharedRef<FJsonObject> EntryBoundary = MakeShared<FJsonObject>();
        EntryBoundary->SetStringField(TEXT("node_ref"), TEXT("FunctionEntry"));
        EntryBoundary->SetStringField(TEXT("display_name"), TEXT("AddMazeRelativeRotation"));
        EntryBoundaries.Add(MakeShared<FJsonValueObject>(EntryBoundary));
        AdapterBoundary->SetArrayField(TEXT("entry_boundaries"), EntryBoundaries);
        TArray<TSharedPtr<FJsonValue>> ExitBoundaries;
        TSharedRef<FJsonObject> ExitBoundary = MakeShared<FJsonObject>();
        ExitBoundary->SetStringField(TEXT("node_ref"), TEXT("FunctionResult"));
        ExitBoundary->SetStringField(TEXT("display_name"), TEXT("Return"));
        ExitBoundaries.Add(MakeShared<FJsonValueObject>(ExitBoundary));
        AdapterBoundary->SetArrayField(TEXT("exit_boundaries"), ExitBoundaries);
        TArray<TSharedPtr<FJsonValue>> FoldedRefs;
        FoldedRefs.Add(MakeShared<FJsonValueString>(TEXT("FunctionEntry")));
        AdapterBoundary->SetArrayField(TEXT("folded_boundary_node_refs"), FoldedRefs);
        TArray<TSharedPtr<FJsonValue>> VisibleRefs;
        VisibleRefs.Add(MakeShared<FJsonValueString>(TEXT("FunctionResult")));
        AdapterBoundary->SetArrayField(TEXT("visible_boundary_node_refs"), VisibleRefs);
        Root->SetObjectField(TEXT("adapter_boundary"), AdapterBoundary);

        TArray<TSharedPtr<FJsonValue>> Graphs;
        Graphs.Add(MakeShared<FJsonValueObject>(MakeLogicTestGraph(TEXT("EventGraph"), EventGraphNodes)));
        Graphs.Add(MakeShared<FJsonValueObject>(
            MakeLogicTestGraphWithLinks(TEXT("AddMazeRelativeRotation"), FunctionNodes, FunctionLinks)));
        Root->SetArrayField(TEXT("graphs"), Graphs);

        return Root;
    }

    static TSharedPtr<FJsonObject> MakeCustomEventWithGraphLevelExecLinkRawJsonObject()
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), TEXT("2.2"));
        Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

        TArray<TSharedPtr<FJsonValue>> Nodes;
        Nodes.Add(MakeShared<FJsonValueObject>(
            MakeLogicTestEventNode(TEXT("entry"), TEXT("K2Node_CustomEvent"), TEXT("OpenDoor"), TEXT("OpenDoor"))));

        TSharedRef<FJsonObject> PrintStringNode = MakeShared<FJsonObject>();
        PrintStringNode->SetStringField(TEXT("id"), TEXT("print"));
        PrintStringNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
        PrintStringNode->SetStringField(TEXT("name"), TEXT("PrintString"));
        PrintStringNode->SetStringField(TEXT("function_name"), TEXT("PrintString"));
        Nodes.Add(MakeShared<FJsonValueObject>(PrintStringNode));

        TSharedRef<FJsonObject> ExecLink = MakeShared<FJsonObject>();
        ExecLink->SetStringField(TEXT("from_id"), TEXT("entry"));
        ExecLink->SetStringField(TEXT("from_pin"), TEXT("then"));
        ExecLink->SetStringField(TEXT("to_id"), TEXT("print"));
        ExecLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
        ExecLink->SetStringField(TEXT("kind"), TEXT("exec"));

        TArray<TSharedPtr<FJsonValue>> Links;
        Links.Add(MakeShared<FJsonValueObject>(ExecLink));

        TArray<TSharedPtr<FJsonValue>> Graphs;
        TSharedRef<FJsonObject> Graph = MakeLogicTestGraph(TEXT("EG_DoorFeature"), Nodes);
        Graph->SetArrayField(TEXT("links"), Links);
        Graphs.Add(MakeShared<FJsonValueObject>(Graph));
        Root->SetArrayField(TEXT("graphs"), Graphs);

        return Root;
    }

    static void AddBlueprintHelperOwnershipMetadata(
        const TSharedRef<FJsonObject>& Node,
        const FString& BlockId)
    {
        TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
        Metadata->SetStringField(TEXT("BlueprintHelperOwned"), TEXT("true"));
        Metadata->SetStringField(TEXT("BlueprintHelperBlockId"), BlockId);
        Node->SetObjectField(TEXT("metadata"), Metadata);
    }

    static TSharedPtr<FJsonObject> MakeBlueprintHelperOwnedBlockRawJsonObject()
    {
        static const FString BlockId = TEXT("BH_DoorFeature_ToggleDoor");

        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), TEXT("2.2"));
        Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

        TArray<TSharedPtr<FJsonValue>> Nodes;
        TSharedRef<FJsonObject> EntryNode =
            MakeLogicTestEventNode(TEXT("entry"), TEXT("K2Node_CustomEvent"), TEXT("OpenDoor"), TEXT("OpenDoor"));
        EntryNode->SetStringField(TEXT("node_guid"), TEXT("11111111111111111111111111111111"));
        AddBlueprintHelperOwnershipMetadata(EntryNode, BlockId);
        Nodes.Add(MakeShared<FJsonValueObject>(EntryNode));

        TSharedRef<FJsonObject> PrintStringNode = MakeShared<FJsonObject>();
        PrintStringNode->SetStringField(TEXT("id"), TEXT("print"));
        PrintStringNode->SetStringField(TEXT("node_guid"), TEXT("22222222222222222222222222222222"));
        PrintStringNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
        PrintStringNode->SetStringField(TEXT("name"), TEXT("PrintString"));
        PrintStringNode->SetStringField(TEXT("function_name"), TEXT("PrintString"));
        AddBlueprintHelperOwnershipMetadata(PrintStringNode, BlockId);
        Nodes.Add(MakeShared<FJsonValueObject>(PrintStringNode));

        TSharedRef<FJsonObject> ExecLink = MakeShared<FJsonObject>();
        ExecLink->SetStringField(TEXT("from_id"), TEXT("entry"));
        ExecLink->SetStringField(TEXT("from_pin"), TEXT("then"));
        ExecLink->SetStringField(TEXT("to_id"), TEXT("print"));
        ExecLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
        ExecLink->SetStringField(TEXT("kind"), TEXT("exec"));

        TArray<TSharedPtr<FJsonValue>> Links;
        Links.Add(MakeShared<FJsonValueObject>(ExecLink));

        Root->SetArrayField(TEXT("nodes"), Nodes);
        Root->SetArrayField(TEXT("links"), Links);
        return Root;
    }

    static TSharedPtr<FJsonObject> MakeMixedUserAndBlueprintHelperBlockRawJsonObject()
    {
        static const FString BlockId = TEXT("BH_DoorFeature_InsertBetween");

        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("version"), TEXT("2.2"));
        Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

        TArray<TSharedPtr<FJsonValue>> Nodes;
        TSharedRef<FJsonObject> EntryNode =
            MakeLogicTestEventNode(TEXT("entry"), TEXT("K2Node_CustomEvent"), TEXT("OpenDoor"), TEXT("OpenDoor"));
        EntryNode->SetStringField(TEXT("node_guid"), TEXT("11111111111111111111111111111111"));
        Nodes.Add(MakeShared<FJsonValueObject>(EntryNode));

        TSharedRef<FJsonObject> InsertedNode = MakeShared<FJsonObject>();
        InsertedNode->SetStringField(TEXT("id"), TEXT("inserted"));
        InsertedNode->SetStringField(TEXT("node_guid"), TEXT("22222222222222222222222222222222"));
        InsertedNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
        InsertedNode->SetStringField(TEXT("name"), TEXT("PrintInserted"));
        InsertedNode->SetStringField(TEXT("function_name"), TEXT("PrintString"));
        AddBlueprintHelperOwnershipMetadata(InsertedNode, BlockId);
        Nodes.Add(MakeShared<FJsonValueObject>(InsertedNode));

        TSharedRef<FJsonObject> SuccessorNode = MakeShared<FJsonObject>();
        SuccessorNode->SetStringField(TEXT("id"), TEXT("successor"));
        SuccessorNode->SetStringField(TEXT("node_guid"), TEXT("33333333333333333333333333333333"));
        SuccessorNode->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
        SuccessorNode->SetStringField(TEXT("name"), TEXT("PrintOriginal"));
        SuccessorNode->SetStringField(TEXT("function_name"), TEXT("PrintString"));
        Nodes.Add(MakeShared<FJsonValueObject>(SuccessorNode));

        TSharedRef<FJsonObject> EntryToInsertedLink = MakeShared<FJsonObject>();
        EntryToInsertedLink->SetStringField(TEXT("from_id"), TEXT("entry"));
        EntryToInsertedLink->SetStringField(TEXT("from_pin"), TEXT("then"));
        EntryToInsertedLink->SetStringField(TEXT("to_id"), TEXT("inserted"));
        EntryToInsertedLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
        EntryToInsertedLink->SetStringField(TEXT("kind"), TEXT("exec"));

        TSharedRef<FJsonObject> InsertedToSuccessorLink = MakeShared<FJsonObject>();
        InsertedToSuccessorLink->SetStringField(TEXT("from_id"), TEXT("inserted"));
        InsertedToSuccessorLink->SetStringField(TEXT("from_pin"), TEXT("then"));
        InsertedToSuccessorLink->SetStringField(TEXT("to_id"), TEXT("successor"));
        InsertedToSuccessorLink->SetStringField(TEXT("to_pin"), TEXT("execute"));
        InsertedToSuccessorLink->SetStringField(TEXT("kind"), TEXT("exec"));

        TArray<TSharedPtr<FJsonValue>> Links;
        Links.Add(MakeShared<FJsonValueObject>(EntryToInsertedLink));
        Links.Add(MakeShared<FJsonValueObject>(InsertedToSuccessorLink));

        Root->SetArrayField(TEXT("nodes"), Nodes);
        Root->SetArrayField(TEXT("links"), Links);
        return Root;
    }

    static FString MakeLogicServiceTestObjectName(const FString& Prefix)
    {
        return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }

    static UBlueprint* MakeLogicServiceBlueprint(const FString& Prefix)
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

    static UEdGraph* FindLogicServiceGraph(UBlueprint* Blueprint, const FString& GraphName)
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

    static UEdGraph* AddLogicServiceUbergraph(UBlueprint* Blueprint, const FString& GraphName)
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

    static UK2Node_CustomEvent* AddLogicServiceCustomEvent(UEdGraph* Graph, const FString& EventName)
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

    static void MarkLogicServiceNodeAsOwned(UEdGraphNode* Node, const FString& BlockId)
    {
        if (!Node)
        {
            return;
        }

        if (UPackage* Package = Node->GetOutermost())
        {
            FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
            MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
            MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
        }
    }

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_ObjectAndStringProduceIdenticalStats,
    "BlueprintHelper.ObjectFirst.Logic.ObjectAndStringProduceIdenticalStats",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_ObjectAndStringProduceIdenticalStats::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> RootObj = FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeTestRawJsonObject();
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
    TSharedPtr<FJsonObject> RootObj = FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeTestRawJsonObject();
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
    FObjectFirstLogic_PreservesInputDefaultsInLogicJson,
    "BlueprintHelper.ObjectFirst.Logic.PreservesInputDefaultsInLogicJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_PreservesInputDefaultsInLogicJson::RunTest(const FString& Parameters)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("version"), TEXT("2.2"));
    Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint"));

    TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
    Node->SetStringField(TEXT("id"), TEXT("set_actor_tick_enabled"));
    Node->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
    Node->SetStringField(TEXT("name"), TEXT("SetActorTickEnabled"));
    Node->SetStringField(TEXT("function_name"), TEXT("SetActorTickEnabled"));

    TSharedRef<FJsonObject> Inputs = MakeShared<FJsonObject>();
    Inputs->SetStringField(TEXT("bEnabled"), TEXT("false"));
    Inputs->SetStringField(TEXT("TickInterval"), TEXT("0.0"));
    Node->SetObjectField(TEXT("inputs"), Inputs);

    TSharedRef<FJsonObject> InputDefaults = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> BoolDefault = MakeShared<FJsonObject>();
    BoolDefault->SetStringField(TEXT("value"), TEXT("false"));
    BoolDefault->SetStringField(TEXT("source"), TEXT("autogenerated_default_value"));
    BoolDefault->SetBoolField(TEXT("connected"), false);
    TSharedRef<FJsonObject> BoolPinType = MakeShared<FJsonObject>();
    BoolPinType->SetStringField(TEXT("category"), TEXT("bool"));
    BoolDefault->SetObjectField(TEXT("pin_type"), BoolPinType);
    InputDefaults->SetObjectField(TEXT("bEnabled"), BoolDefault);

    TSharedRef<FJsonObject> FloatDefault = MakeShared<FJsonObject>();
    FloatDefault->SetStringField(TEXT("value"), TEXT("0.0"));
    FloatDefault->SetStringField(TEXT("source"), TEXT("default_value"));
    FloatDefault->SetBoolField(TEXT("connected"), false);
    TSharedRef<FJsonObject> FloatPinType = MakeShared<FJsonObject>();
    FloatPinType->SetStringField(TEXT("category"), TEXT("real"));
    FloatDefault->SetObjectField(TEXT("pin_type"), FloatPinType);
    InputDefaults->SetObjectField(TEXT("TickInterval"), FloatDefault);
    Node->SetObjectField(TEXT("input_defaults"), InputDefaults);

    TArray<TSharedPtr<FJsonValue>> Nodes;
    Nodes.Add(MakeShared<FJsonValueObject>(Node));
    Root->SetArrayField(TEXT("nodes"), Nodes);
    Root->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());

    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildGroups(
        Root,
        TEXT("/Game/BP/BP_ReadContextDefaults"),
        TEXT("EventGraph"),
        EBlueprintHelperLogicScope::TargetGraph);

    const FBlueprintHelperLogicNode* LogicNodePtr = nullptr;
    if (Payload.Nodes.Num() > 0)
    {
        LogicNodePtr = &Payload.Nodes[0];
    }
    else if (Payload.Groups.Num() > 0 && Payload.Groups[0].Nodes.Num() > 0)
    {
        LogicNodePtr = &Payload.Groups[0].Nodes[0];
    }

    TestNotNull(TEXT("logic node exists"), LogicNodePtr);
    if (!LogicNodePtr)
    {
        return false;
    }

    const FBlueprintHelperLogicNode& LogicNode = *LogicNodePtr;
    TestTrue(TEXT("inputs are preserved"), LogicNode.Inputs.IsValid());
    TestTrue(TEXT("input_defaults are preserved"), LogicNode.InputDefaults.IsValid());
    if (!LogicNode.Inputs.IsValid() || !LogicNode.InputDefaults.IsValid())
    {
        return false;
    }

    FString BoolValue;
    TestTrue(TEXT("bool default exists in inputs"), LogicNode.Inputs->TryGetStringField(TEXT("bEnabled"), BoolValue));
    TestEqual(TEXT("bool default value is false"), BoolValue, FString(TEXT("false")));

    const TSharedPtr<FJsonObject>* BoolDefaultOut = nullptr;
    TestTrue(TEXT("bool default metadata exists"), LogicNode.InputDefaults->TryGetObjectField(TEXT("bEnabled"), BoolDefaultOut));
    if (BoolDefaultOut && BoolDefaultOut->IsValid())
    {
        FString Source;
        TestTrue(TEXT("bool default source exists"), (*BoolDefaultOut)->TryGetStringField(TEXT("source"), Source));
        TestEqual(TEXT("bool default source is autogenerated"), Source, FString(TEXT("autogenerated_default_value")));
    }

    FString FloatValue;
    TestTrue(TEXT("float default exists in inputs"), LogicNode.Inputs->TryGetStringField(TEXT("TickInterval"), FloatValue));
    TestEqual(TEXT("float default value is preserved"), FloatValue, FString(TEXT("0.0")));

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
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeCustomEventInCustomGraphRawJsonObject(),
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
    FObjectFirstLogic_FunctionTargetUsesFunctionGraph,
    "BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesFunctionGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_FunctionTargetUsesFunctionGraph::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildTargetEntry(
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeFunctionInFunctionGraphRawJsonObject(),
        TEXT("/Game/Gameplay/Maze/BP_Maze"),
        TEXT(""),
        TEXT("AddMazeRelativeRotation"),
        EBlueprintHelperLogicScope::TargetFunction);

    TestEqual(TEXT("function target resolves function graph"), Payload.Graph, FString(TEXT("AddMazeRelativeRotation")));
    TestEqual(TEXT("function target records function name"), Payload.Function, FString(TEXT("AddMazeRelativeRotation")));
    TestTrue(TEXT("function target has an entry"), Payload.Entry.IsSet());
    if (Payload.Entry.IsSet())
    {
        TestEqual(TEXT("function target entry is AddMazeRelativeRotation"), Payload.Entry->Name, FString(TEXT("AddMazeRelativeRotation")));
        TestEqual(TEXT("function target entry kind is function"), Payload.Entry->Kind, EBlueprintHelperLogicNodeKind::FunctionEntry);
    }
    TestTrue(TEXT("function target returns function nodes"), Payload.Nodes.Num() >= 2);
    if (Payload.Nodes.Num() >= 2)
    {
        TestEqual(TEXT("function target first node is function entry"), Payload.Nodes[0].Name, FString(TEXT("AddMazeRelativeRotation")));
        TestEqual(TEXT("function target includes function body node"), Payload.Nodes[1].Name, FString(TEXT("SetRelativeRotation")));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_FunctionTargetUsesExportedFunctionGraphWithoutEntry,
    "BlueprintHelper.ObjectFirst.Logic.FunctionTargetUsesExportedFunctionGraphWithoutEntry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_FunctionTargetUsesExportedFunctionGraphWithoutEntry::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildTargetEntry(
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeExportedFunctionGraphWithoutEntryRawJsonObject(),
        TEXT("/Game/Gameplay/Maze/BP_Maze"),
        TEXT(""),
        TEXT("AddMazeRelativeRotation"),
        EBlueprintHelperLogicScope::TargetFunction);

    TestEqual(TEXT("function target resolves exported function graph"), Payload.Graph, FString(TEXT("AddMazeRelativeRotation")));
    TestEqual(TEXT("function target records function name from target"), Payload.Function, FString(TEXT("AddMazeRelativeRotation")));
    TestTrue(TEXT("function target synthesizes entry metadata"), Payload.Entry.IsSet());
    if (Payload.Entry.IsSet())
    {
        TestEqual(TEXT("synthetic function entry uses target name"), Payload.Entry->Name, FString(TEXT("AddMazeRelativeRotation")));
        TestEqual(TEXT("synthetic function entry kind is function"), Payload.Entry->Kind, EBlueprintHelperLogicNodeKind::FunctionEntry);
        TestEqual(TEXT("adapter function entry ref is stable"), Payload.Entry->NodeRef, FString(TEXT("FunctionEntry")));
        TestEqual(
            TEXT("synthetic function entry path is function scoped"),
            Payload.Entry->NodePath,
            FString(TEXT("$.graphs[AddMazeRelativeRotation].FunctionEntry")));
    }
    TestEqual(TEXT("function target returns function boundary and body nodes"), Payload.Nodes.Num(), 3);
    if (Payload.Nodes.Num() == 3)
    {
        TestEqual(TEXT("function target includes synthetic entry node"), Payload.Nodes[0].Name, FString(TEXT("AddMazeRelativeRotation")));
        TestEqual(TEXT("function target adapter entry node keeps boundary ref"), Payload.Nodes[0].NodeRef, FString(TEXT("FunctionEntry")));
        TestEqual(TEXT("function entry has one exec link"), Payload.Nodes[0].Links.Num(), 1);
        if (Payload.Nodes[0].Links.Num() == 1)
        {
            TestEqual(TEXT("function entry link reaches body node"), Payload.Nodes[0].Links[0].ToNode, FString(TEXT("nodes[0]")));
        }

        TestEqual(TEXT("function target includes function body node"), Payload.Nodes[1].Name, FString(TEXT("SetRelativeRotation")));
        TestEqual(TEXT("function target body node keeps raw node ref"), Payload.Nodes[1].NodeRef, FString(TEXT("nodes[0]")));
        TestEqual(TEXT("function body has one result link"), Payload.Nodes[1].Links.Num(), 1);
        if (Payload.Nodes[1].Links.Num() == 1)
        {
            TestEqual(TEXT("function body link reaches result boundary"), Payload.Nodes[1].Links[0].ToNode, FString(TEXT("FunctionResult")));
        }

        TestEqual(TEXT("function target includes synthetic result node"), Payload.Nodes[2].Name, FString(TEXT("Return")));
        TestEqual(TEXT("function target adapter result node keeps boundary ref"), Payload.Nodes[2].NodeRef, FString(TEXT("FunctionResult")));
        TestEqual(TEXT("function target synthetic result kind"), Payload.Nodes[2].Kind, EBlueprintHelperLogicNodeKind::Return);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_FunctionTargetDoesNotSynthesizeEntryForUnmatchedGraph,
    "BlueprintHelper.ObjectFirst.Logic.FunctionTargetDoesNotSynthesizeEntryForUnmatchedGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_FunctionTargetDoesNotSynthesizeEntryForUnmatchedGraph::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildTargetEntry(
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeExportedFunctionGraphWithoutEntryRawJsonObject(),
        TEXT("/Game/Gameplay/Maze/BP_Maze"),
        TEXT(""),
        TEXT("MissingFunction"),
        EBlueprintHelperLogicScope::TargetFunction);

    TestEqual(TEXT("unmatched function target keeps requested function name"), Payload.Function, FString(TEXT("MissingFunction")));
    TestFalse(TEXT("unmatched function target does not synthesize entry"), Payload.Entry.IsSet());
    TestEqual(TEXT("unmatched function target does not return unrelated graph nodes"), Payload.Nodes.Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_CustomEventTargetPreservesGraphLevelExecLink,
    "BlueprintHelper.ObjectFirst.Logic.CustomEventTargetPreservesGraphLevelExecLink",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_CustomEventTargetPreservesGraphLevelExecLink::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildTargetEntry(
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeCustomEventWithGraphLevelExecLinkRawJsonObject(),
        TEXT("/Game/BP/BP_PhysicsDoor"),
        TEXT("EG_DoorFeature"),
        TEXT("OpenDoor"),
        EBlueprintHelperLogicScope::TargetCustomEvent);

    TestEqual(TEXT("custom_event target returns both linked nodes"), Payload.Nodes.Num(), 2);
    if (Payload.Nodes.Num() < 2)
    {
        return false;
    }

    const FBlueprintHelperLogicNode& EntryNode = Payload.Nodes[0];
    TestEqual(TEXT("custom_event graph-level exec link is attached to source node"), EntryNode.Links.Num(), 1);
    if (EntryNode.Links.Num() == 1)
    {
        const FBlueprintHelperLogicLink& Link = EntryNode.Links[0];
        TestEqual(TEXT("link_ref keeps LogicJson link style"), Link.LinkRef, FString(TEXT("links[0]")));
        TestEqual(TEXT("link type is exec"), Link.Type, EBlueprintHelperLogicLinkType::Exec);
        TestEqual(TEXT("link from pin is preserved"), Link.FromPin, FString(TEXT("then")));
        TestEqual(TEXT("link target node is target node_ref"), Link.ToNode, FString(TEXT("nodes[1]")));
        TestEqual(TEXT("link target pin is preserved"), Link.ToPin, FString(TEXT("execute")));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_GroupedBlueprintHelperOwnedBlockExposesWriteAnchors,
    "BlueprintHelper.ObjectFirst.Logic.GroupedBlueprintHelperOwnedBlockExposesWriteAnchors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_GroupedBlueprintHelperOwnedBlockExposesWriteAnchors::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildGroups(
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeBlueprintHelperOwnedBlockRawJsonObject(),
        TEXT("/Game/BP/BP_PhysicsDoor"),
        TEXT("EG_DoorFeature"),
        EBlueprintHelperLogicScope::TargetGraph);

    TestEqual(TEXT("owned block is returned as a grouped block"), Payload.Groups.Num(), 1);
    if (Payload.Groups.Num() != 1)
    {
        return false;
    }

    const FBlueprintHelperLogicGroup& Group = Payload.Groups[0];
    const TSharedRef<FJsonObject> GroupJson = Group.ToJson();

    TestEqual(TEXT("owned group type is BlueprintHelper block"),
        Group.GroupType,
        EBlueprintHelperLogicGroupType::BlueprintHelperBlock);

    FString BlockId;
    TestTrue(TEXT("owned group serializes block_id"), GroupJson->TryGetStringField(TEXT("block_id"), BlockId));
    TestEqual(TEXT("owned group block_id uses BlueprintHelperBlockId metadata"),
        BlockId,
        FString(TEXT("BH_DoorFeature_ToggleDoor")));

    FString GroupEntryNodePath;
    TestTrue(TEXT("owned group serializes group_entry_node_path"),
        GroupJson->TryGetStringField(TEXT("group_entry_node_path"), GroupEntryNodePath));
    TestFalse(TEXT("group_entry_node_path is populated"), GroupEntryNodePath.IsEmpty());

    const TArray<TSharedPtr<FJsonValue>>* NodesJson = nullptr;
    TestTrue(TEXT("owned group serializes nodes"), GroupJson->TryGetArrayField(TEXT("nodes"), NodesJson));
    if (!NodesJson || NodesJson->Num() < 2)
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* EntryNodeJson = nullptr;
    TestTrue(TEXT("entry node is an object"), (*NodesJson)[0]->TryGetObject(EntryNodeJson));
    if (!EntryNodeJson || !EntryNodeJson->IsValid())
    {
        return false;
    }

    FString EntryNodeRef;
    TestTrue(TEXT("owned group node serializes stable node_ref"),
        (*EntryNodeJson)->TryGetStringField(TEXT("node_ref"), EntryNodeRef));
    TestEqual(TEXT("first owned node uses node_guid node_ref"),
        EntryNodeRef,
        FString(TEXT("11111111111111111111111111111111")));

    const TArray<TSharedPtr<FJsonValue>>* LinksJson = nullptr;
    TestTrue(TEXT("entry node serializes outgoing links"), (*EntryNodeJson)->TryGetArrayField(TEXT("links"), LinksJson));
    if (!LinksJson || LinksJson->Num() < 1)
    {
        return false;
    }

    const TSharedPtr<FJsonObject>* LinkJson = nullptr;
    TestTrue(TEXT("outgoing link is an object"), (*LinksJson)[0]->TryGetObject(LinkJson));
    if (!LinkJson || !LinkJson->IsValid())
    {
        return false;
    }

    FString PinRef;
    TestTrue(TEXT("owned group link serializes group-local pin_ref"),
        (*LinkJson)->TryGetStringField(TEXT("pin_ref"), PinRef));
    TestEqual(TEXT("owned group link pin_ref uses the source pin"), PinRef, FString(TEXT("then")));

    FString ToNodeRef;
    TestTrue(TEXT("owned group link serializes stable target node_ref"),
        (*LinkJson)->TryGetStringField(TEXT("to_node"), ToNodeRef));
    TestEqual(TEXT("owned group link target uses node_guid node_ref"),
        ToNodeRef,
        FString(TEXT("22222222222222222222222222222222")));

    FString LinkRef;
    TestTrue(TEXT("owned group link serializes stable link_ref"),
        (*LinkJson)->TryGetStringField(TEXT("link_ref"), LinkRef));
    TestEqual(TEXT("first owned link has endpoint link_ref"),
        LinkRef,
        FString(TEXT("11111111111111111111111111111111.then->22222222222222222222222222222222.execute")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_GroupedBlueprintHelperOwnedBlockPreservesCrossGroupExecLinks,
    "BlueprintHelper.ObjectFirst.Logic.GroupedBlueprintHelperOwnedBlockPreservesCrossGroupExecLinks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_GroupedBlueprintHelperOwnedBlockPreservesCrossGroupExecLinks::RunTest(const FString& Parameters)
{
    FBlueprintHelperLogicGroupBuilder Builder;
    const FBlueprintHelperLogicJsonPayload Payload = Builder.BuildGroups(
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeMixedUserAndBlueprintHelperBlockRawJsonObject(),
        TEXT("/Game/BP/BP_PhysicsDoor"),
        TEXT("EG_DoorFeature"),
        EBlueprintHelperLogicScope::TargetGraph);

    TestEqual(TEXT("mixed graph is split into owned and user groups"), Payload.Groups.Num(), 2);
    if (Payload.Groups.Num() != 2)
    {
        return false;
    }

    const FBlueprintHelperLogicGroup& OwnedGroup = Payload.Groups[0];
    const FBlueprintHelperLogicGroup& UserGroup = Payload.Groups[1];
    TestEqual(TEXT("first group is the owned block"),
        OwnedGroup.GroupType,
        EBlueprintHelperLogicGroupType::BlueprintHelperBlock);
    TestEqual(TEXT("second group is the user event flow"),
        UserGroup.GroupType,
        EBlueprintHelperLogicGroupType::GlobalEventFlow);

    TestEqual(TEXT("owned group has inserted node"), OwnedGroup.Nodes.Num(), 1);
    TestEqual(TEXT("user group has entry and successor nodes"), UserGroup.Nodes.Num(), 2);
    if (OwnedGroup.Nodes.Num() != 1 || UserGroup.Nodes.Num() != 2)
    {
        return false;
    }

    const FBlueprintHelperLogicNode& InsertedNode = OwnedGroup.Nodes[0];
    const FBlueprintHelperLogicNode& EntryNode = UserGroup.Nodes[0];
    const FBlueprintHelperLogicNode& SuccessorNode = UserGroup.Nodes[1];

    TestEqual(TEXT("entry node uses stable node_ref"),
        EntryNode.NodeRef,
        FString(TEXT("11111111111111111111111111111111")));
    TestEqual(TEXT("inserted node uses stable node_ref"),
        InsertedNode.NodeRef,
        FString(TEXT("22222222222222222222222222222222")));
    TestEqual(TEXT("successor node uses stable node_ref"),
        SuccessorNode.NodeRef,
        FString(TEXT("33333333333333333333333333333333")));

    TestEqual(TEXT("user entry keeps exec link into owned group"), EntryNode.Links.Num(), 1);
    if (EntryNode.Links.Num() == 1)
    {
        const FBlueprintHelperLogicLink& EntryLink = EntryNode.Links[0];
        TestEqual(TEXT("entry link type is exec"), EntryLink.Type, EBlueprintHelperLogicLinkType::Exec);
        TestEqual(TEXT("entry link targets inserted owned node"),
            EntryLink.ToNode,
            FString(TEXT("22222222222222222222222222222222")));
        TestEqual(TEXT("entry link target pin is execute"),
            EntryLink.ToPin,
            FString(TEXT("execute")));
    }

    TestEqual(TEXT("owned node keeps exec link back to user successor"), InsertedNode.Links.Num(), 1);
    if (InsertedNode.Links.Num() == 1)
    {
        const FBlueprintHelperLogicLink& InsertedLink = InsertedNode.Links[0];
        TestEqual(TEXT("inserted link type is exec"), InsertedLink.Type, EBlueprintHelperLogicLinkType::Exec);
        TestEqual(TEXT("inserted link targets original user successor"),
            InsertedLink.ToNode,
            FString(TEXT("33333333333333333333333333333333")));
        TestEqual(TEXT("inserted link target pin is execute"),
            InsertedLink.ToPin,
            FString(TEXT("execute")));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_ReadLogicJsonCustomEventScansCustomGraphs,
    "BlueprintHelper.ObjectFirst.Logic.ReadLogicJsonCustomEventScansCustomGraphs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_ReadLogicJsonCustomEventScansCustomGraphs::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeLogicServiceBlueprint(TEXT("ReadLogicJsonCustomEvent"));
    TestNotNull(TEXT("test Blueprint is created"), Blueprint);
    if (!Blueprint)
    {
        return false;
    }

    UEdGraph* EventGraph = FBlueprintHelperObjectFirstLogicTestsLocalUtils::FindLogicServiceGraph(Blueprint, TEXT("EventGraph"));
    TestNotNull(TEXT("EventGraph exists"), EventGraph);
    UEdGraph* CustomGraph = FBlueprintHelperObjectFirstLogicTestsLocalUtils::AddLogicServiceUbergraph(Blueprint, TEXT("EG_DoorFeature"));
    TestNotNull(TEXT("custom graph is created"), CustomGraph);
    if (!EventGraph || !CustomGraph)
    {
        return false;
    }

    TestNotNull(TEXT("EventGraph custom event is created"),
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::AddLogicServiceCustomEvent(EventGraph, TEXT("EventGraphDoor")));
    TestNotNull(TEXT("custom graph custom event is created"),
        FBlueprintHelperObjectFirstLogicTestsLocalUtils::AddLogicServiceCustomEvent(CustomGraph, TEXT("OpenDoor")));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectFirstLogic_ReadLogicJsonIncludesOwnedBlockIdFromGraphMetadata,
    "BlueprintHelper.ObjectFirst.Logic.ReadLogicJsonIncludesOwnedBlockIdFromGraphMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FObjectFirstLogic_ReadLogicJsonIncludesOwnedBlockIdFromGraphMetadata::RunTest(const FString& Parameters)
{
    UBlueprint* Blueprint = FBlueprintHelperObjectFirstLogicTestsLocalUtils::MakeLogicServiceBlueprint(TEXT("ReadLogicJsonOwnedBlockId"));
    TestNotNull(TEXT("test Blueprint is created"), Blueprint);
    if (!Blueprint)
    {
        return false;
    }

    UEdGraph* EventGraph = FBlueprintHelperObjectFirstLogicTestsLocalUtils::FindLogicServiceGraph(Blueprint, TEXT("EventGraph"));
    TestNotNull(TEXT("EventGraph exists"), EventGraph);
    if (!EventGraph)
    {
        return false;
    }

    const FString BlockId = TEXT("EventGraph_OpenDoor");
    UK2Node_CustomEvent* EventNode = FBlueprintHelperObjectFirstLogicTestsLocalUtils::AddLogicServiceCustomEvent(EventGraph, TEXT("OpenDoor"));
    TestNotNull(TEXT("custom event is created"), EventNode);
    FBlueprintHelperObjectFirstLogicTestsLocalUtils::MarkLogicServiceNodeAsOwned(EventNode, BlockId);
    TestFalse(TEXT("owned node does not need legacy block_id comment for LogicJson"),
        EventNode && EventNode->NodeComment.Contains(TEXT("block_id=")));

    FBlueprintHelperTargetRef Target;
    Target.AssetPath = Blueprint->GetPathName();
    Target.TargetType = EBlueprintHelperTargetType::CustomEvent;
    Target.Graph = TEXT("EventGraph");
    Target.Event = TEXT("OpenDoor");

    FBlueprintHelperLogicJsonReadService ReadService;
    const FBlueprintHelperLogicJsonData Data = ReadService.ReadLogicJson(Target);

    TestEqual(TEXT("ReadLogicJson carries block_id from exported node metadata"),
        Data.Logic.BlockId,
        BlockId);
    return true;
}

#endif
