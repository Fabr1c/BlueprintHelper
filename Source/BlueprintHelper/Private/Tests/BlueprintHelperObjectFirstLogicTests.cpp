#include "Misc/AutomationTest.h"
#include "Logic/BlueprintHelperLogicProcessor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
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

#endif
