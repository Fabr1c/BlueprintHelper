#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.h"
#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h"

class FBlueprintHelperTaskSpecWorkbenchServicesTestData
{
public:
	static FString MakeTaskSpecText()
	{
		return TEXT(R"JSON(
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "edit_blueprint_graph",
  "target": {
    "asset_path": "/Game/BP_Test",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EventGraph",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "DoThing",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "call_function",
              "name": "PrintString",
              "args": {
                "InString": {
                  "kind": "literal",
                  "value_type": "string",
                  "value": "hello"
                }
              }
            }
          ]
        }
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
)JSON");
	}

	static FString MakeMinimalT3DText()
	{
		return TEXT(R"T3D(
Begin Object Class=/Script/BlueprintGraph.K2Node_CallFunction Name="K2Node_CallFunction_0"
   NodePosX=0
   NodePosY=0
   FunctionReference=(MemberParent="/Script/Engine.KismetSystemLibrary",MemberName="PrintString")
End Object
)T3D");
	}

	static FString MakeLinkedExecT3DText()
	{
		return TEXT(R"T3D(
Begin Object Class=/Script/BlueprintGraph.K2Node_Event Name="K2Node_Event_0"
   CustomProperties Pin (PinId=11111111111111111111111111111111,PinName="then",Direction="EGPD_Output",PinType.PinCategory="exec",LinkedTo=(K2Node_CallFunction_0 22222222222222222222222222222222,))
End Object
Begin Object Class=/Script/BlueprintGraph.K2Node_CallFunction Name="K2Node_CallFunction_0"
   FunctionReference=(MemberParent="/Script/Engine.KismetSystemLibrary",MemberName="PrintString")
   CustomProperties Pin (PinId=22222222222222222222222222222222,PinName="execute",Direction="EGPD_Input",PinType.PinCategory="exec")
End Object
)T3D");
	}

	static TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonText)
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		FJsonSerializer::Deserialize(Reader, JsonObject);
		return JsonObject;
	}

	static bool HasStringArrayValue(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& ExpectedValue)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (Value.IsValid()
				&& Value->Type == EJson::String
				&& Value->AsString().Equals(ExpectedValue, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static bool HasExecLinkWithPinTypes(const TSharedPtr<FJsonObject>& RootObject)
	{
		const TSharedPtr<FJsonObject>* LogicObject = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
		if (!RootObject.IsValid()
			|| !RootObject->TryGetObjectField(TEXT("logic"), LogicObject)
			|| !LogicObject
			|| !LogicObject->IsValid()
			|| !(*LogicObject)->TryGetArrayField(TEXT("links"), Links)
			|| !Links)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& LinkValue : *Links)
		{
			const TSharedPtr<FJsonObject> LinkObject = LinkValue.IsValid()
				? LinkValue->AsObject()
				: nullptr;
			if (!LinkObject.IsValid())
			{
				continue;
			}

			if (LinkObject->GetStringField(TEXT("kind")).Equals(TEXT("exec"), ESearchCase::IgnoreCase)
				&& LinkObject->GetStringField(TEXT("from_pin_type")).Equals(TEXT("exec"), ESearchCase::IgnoreCase)
				&& LinkObject->GetStringField(TEXT("to_pin_type")).Equals(TEXT("exec"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskSpecWorkbenchClassifiesAndPreviewsTaskSpecTest,
	"BlueprintHelper.UI.TaskSpecWorkbench.ClassifiesAndPreviewsTaskSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskSpecWorkbenchClassifiesAndPreviewsTaskSpecTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperInputDocument Input =
		FBlueprintHelperWorkbenchInputClassifier::Classify(
			FBlueprintHelperTaskSpecWorkbenchServicesTestData::MakeTaskSpecText());

	TestEqual(TEXT("input type"), Input.InputType, EBlueprintHelperWorkbenchInputType::TaskSpec);
	TestTrue(TEXT("parse succeeded"), Input.bParseSucceeded);

	const FBlueprintHelperTaskSpecPreviewModel Preview =
		FBlueprintHelperTaskSpecPreviewModelBuilder::BuildPreviewModel(Input);

	const bool bHasGraphBlock = Preview.Blocks.ContainsByPredicate(
		[](const FBlueprintHelperTaskSpecPreviewBlock& Block)
		{
			return Block.Kind == EBlueprintHelperTaskSpecPreviewBlockKind::GraphLogic;
		});
	const bool bHasNonGraphBlock = Preview.Blocks.ContainsByPredicate(
		[](const FBlueprintHelperTaskSpecPreviewBlock& Block)
		{
			return Block.Kind == EBlueprintHelperTaskSpecPreviewBlockKind::NonGraphLogic;
		});

	TestTrue(TEXT("graph preview block"), bHasGraphBlock);
	TestTrue(TEXT("non-graph preview block"), bHasNonGraphBlock);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskSpecWorkbenchExportsT3DReadContextFormatsTest,
	"BlueprintHelper.UI.TaskSpecWorkbench.ExportsT3DReadContextFormats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskSpecWorkbenchExportsT3DReadContextFormatsTest::RunTest(const FString& Parameters)
{
	const FString T3DText = FBlueprintHelperTaskSpecWorkbenchServicesTestData::MakeMinimalT3DText();
	const FBlueprintHelperInputDocument Input =
		FBlueprintHelperWorkbenchInputClassifier::Classify(T3DText);

	TestEqual(TEXT("input type"), Input.InputType, EBlueprintHelperWorkbenchInputType::T3D);
	TestTrue(TEXT("parse succeeded"), Input.bParseSucceeded);

	FBlueprintHelperReadContextExportRequest LogicFlowRequest;
	LogicFlowRequest.SourceText = T3DText;
	LogicFlowRequest.Format = EBlueprintHelperReadContextExportFormat::LogicFlow;
	const FBlueprintHelperReadContextExportResult LogicFlowResult =
		FBlueprintHelperReadContextExportService::Export(LogicFlowRequest);

	TestTrue(TEXT("logicflow request degrades successfully"), LogicFlowResult.bSucceeded);
	const TSharedPtr<FJsonObject> LogicFlowJson =
		FBlueprintHelperTaskSpecWorkbenchServicesTestData::ParseJsonObject(LogicFlowResult.ExportText);
	TestTrue(TEXT("logicflow request exports JSON object"), LogicFlowJson.IsValid());
	if (LogicFlowJson.IsValid())
	{
		TestEqual(TEXT("logicflow request exports ReadContext schema"), LogicFlowJson->GetStringField(TEXT("schema")), TEXT("ReadContextPack.v1"));
		TestEqual(TEXT("logicflow request degrades to logic_json"), LogicFlowJson->GetStringField(TEXT("format")), TEXT("logic_json"));
		TestEqual(TEXT("logicflow request records requested format"), LogicFlowJson->GetStringField(TEXT("requested_format")), TEXT("logic_flow"));
		TestEqual(TEXT("logicflow records task-core projection owner"), LogicFlowJson->GetStringField(TEXT("projection_owner")), TEXT("task-core"));
		TestEqual(TEXT("logicflow records UE callback schema"), LogicFlowJson->GetStringField(TEXT("ue_callback_schema")), TEXT("LogicSnapshot.v1"));
		TestTrue(
			TEXT("logicflow request has warning"),
			FBlueprintHelperTaskSpecWorkbenchServicesTestData::HasStringArrayValue(
				LogicFlowJson,
				TEXT("warnings"),
				TEXT("logic_flow_degraded_workbench_canonical_builder_unavailable")));
		const TArray<TSharedPtr<FJsonValue>>* Diagnostics = nullptr;
		TestTrue(TEXT("logicflow records projection diagnostic"), LogicFlowJson->TryGetArrayField(TEXT("diagnostics"), Diagnostics));
	}
	TestTrue(TEXT("logicflow status message reports degradation"), LogicFlowResult.Message.Contains(TEXT("degraded to logicjson")));

	FBlueprintHelperReadContextExportRequest LogicJsonRequest;
	LogicJsonRequest.SourceText = T3DText;
	LogicJsonRequest.Format = EBlueprintHelperReadContextExportFormat::LogicJson;
	const FBlueprintHelperReadContextExportResult LogicJsonResult =
		FBlueprintHelperReadContextExportService::Export(LogicJsonRequest);

	TestTrue(TEXT("logicjson export succeeds"), LogicJsonResult.bSucceeded);
	const TSharedPtr<FJsonObject> LogicJson =
		FBlueprintHelperTaskSpecWorkbenchServicesTestData::ParseJsonObject(LogicJsonResult.ExportText);
	TestTrue(TEXT("logicjson exports JSON object"), LogicJson.IsValid());
	if (LogicJson.IsValid())
	{
		TestEqual(TEXT("logicjson has ReadContext schema"), LogicJson->GetStringField(TEXT("schema")), TEXT("ReadContextPack.v1"));
		TestEqual(TEXT("logicjson has format"), LogicJson->GetStringField(TEXT("format")), TEXT("logic_json"));
		TestEqual(TEXT("logicjson records task-core projection owner"), LogicJson->GetStringField(TEXT("projection_owner")), TEXT("task-core"));
	}
	TestEqual(TEXT("source text not mutated"), LogicJsonRequest.SourceText, T3DText);

	const FString LinkedT3DText = FBlueprintHelperTaskSpecWorkbenchServicesTestData::MakeLinkedExecT3DText();
	FBlueprintHelperReadContextExportRequest LinkedLogicJsonRequest;
	LinkedLogicJsonRequest.SourceText = LinkedT3DText;
	LinkedLogicJsonRequest.Format = EBlueprintHelperReadContextExportFormat::LogicJson;
	const FBlueprintHelperReadContextExportResult LinkedLogicJsonResult =
		FBlueprintHelperReadContextExportService::Export(LinkedLogicJsonRequest);

	TestTrue(TEXT("linked logicjson export succeeds"), LinkedLogicJsonResult.bSucceeded);
	const TSharedPtr<FJsonObject> LinkedLogicJson =
		FBlueprintHelperTaskSpecWorkbenchServicesTestData::ParseJsonObject(LinkedLogicJsonResult.ExportText);
	TestTrue(TEXT("linked logicjson exports JSON object"), LinkedLogicJson.IsValid());
	TestTrue(
		TEXT("linked logicjson has exec link with pin types"),
		FBlueprintHelperTaskSpecWorkbenchServicesTestData::HasExecLinkWithPinTypes(LinkedLogicJson));

	FBlueprintHelperReadContextExportRequest LogicMdRequest;
	LogicMdRequest.SourceText = T3DText;
	LogicMdRequest.Format = EBlueprintHelperReadContextExportFormat::LogicMd;
	const FBlueprintHelperReadContextExportResult LogicMdResult =
		FBlueprintHelperReadContextExportService::Export(LogicMdRequest);

	TestTrue(TEXT("logicmd export succeeds"), LogicMdResult.bSucceeded);
	TestTrue(TEXT("logicmd has title"), LogicMdResult.ExportText.Contains(TEXT("ReadContext LogicMD")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskSpecWorkbenchReadContextProjectionMetadataShapeTest,
	"BlueprintHelper.TaskSpecWorkbench.ReadContextProjection.MetadataShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskSpecWorkbenchReadContextProjectionMetadataShapeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReadContextExportRequest Request;
	Request.SourceText = FBlueprintHelperTaskSpecWorkbenchServicesTestData::MakeMinimalT3DText();
	Request.Format = EBlueprintHelperReadContextExportFormat::LogicFlow;

	const FBlueprintHelperReadContextExportResult Result =
		FBlueprintHelperReadContextExportService::Export(Request);
	TestTrue(TEXT("logicflow projection export succeeds"), Result.bSucceeded);

	const TSharedPtr<FJsonObject> Payload =
		FBlueprintHelperTaskSpecWorkbenchServicesTestData::ParseJsonObject(Result.ExportText);
	TestTrue(TEXT("projection export is JSON"), Payload.IsValid());
	if (!Payload.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("projection owner is task-core"), Payload->GetStringField(TEXT("projection_owner")), TEXT("task-core"));
	TestEqual(TEXT("UE callback schema is recorded"), Payload->GetStringField(TEXT("ue_callback_schema")), TEXT("LogicSnapshot.v1"));
	TestEqual(TEXT("requested format is preserved"), Payload->GetStringField(TEXT("requested_format")), TEXT("logic_flow"));

	const TArray<TSharedPtr<FJsonValue>>* Diagnostics = nullptr;
	TestTrue(TEXT("projection unavailable diagnostic exists"), Payload->TryGetArrayField(TEXT("diagnostics"), Diagnostics));
	TestTrue(TEXT("projection unavailable diagnostic has entries"), Diagnostics && Diagnostics->Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskSpecWorkbenchStoreCandidateRadioSelectionTest,
	"BlueprintHelper.UI.TaskSpecWorkbench.StoreCandidateRadioSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskSpecWorkbenchStoreCandidateRadioSelectionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskSpecWorkbenchSnapshot Snapshot;
	FBlueprintHelperCallFunctionCardModel Card;
	Card.CardId = TEXT("card_1");
	Card.SourcePath = TEXT("$.behavior.entries[0].body.statements[0]");

	FBlueprintHelperCallFunctionCandidateRowModel First;
	First.CandidateId = TEXT("candidate_a");
	First.DisplayName = TEXT("A");
	Card.Candidates.Add(First);

	FBlueprintHelperCallFunctionCandidateRowModel Second;
	Second.CandidateId = TEXT("candidate_b");
	Second.DisplayName = TEXT("B");
	Card.Candidates.Add(Second);
	Snapshot.CandidateCards.Add(Card);

	FBlueprintHelperTaskSpecPreviewBlock Block;
	Block.SourcePath = Card.SourcePath;
	Block.Kind = EBlueprintHelperTaskSpecPreviewBlockKind::GraphLogic;
	Snapshot.Preview.Blocks.Add(Block);

	FBlueprintHelperTaskSpecWorkbenchStore Store;
	Store.ReplaceSnapshot(Snapshot);
	Store.SelectCandidate(TEXT("card_1"), TEXT("candidate_b"));

	const FBlueprintHelperTaskSpecWorkbenchSnapshot& Result = Store.GetSnapshot();
	TestFalse(TEXT("first candidate not selected"), Result.CandidateCards[0].Candidates[0].bSelected);
	TestTrue(TEXT("second candidate selected"), Result.CandidateCards[0].Candidates[1].bSelected);
	TestTrue(TEXT("preview block highlighted"), Result.Preview.Blocks[0].bSelected);
	return true;
}

#endif
