#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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

	TestTrue(TEXT("logicflow export succeeds"), LogicFlowResult.bSucceeded);
	TestTrue(TEXT("logicflow has schema"), LogicFlowResult.ExportText.Contains(TEXT("LogicFlow.v1")));
	TestTrue(TEXT("logicflow has flow field"), LogicFlowResult.ExportText.Contains(TEXT("\"flow\"")));
	TestTrue(TEXT("logicflow has stats"), LogicFlowResult.ExportText.Contains(TEXT("\"stats\"")));
	TestTrue(TEXT("logicflow status message"), LogicFlowResult.Message.Contains(TEXT("logicflow")));

	FBlueprintHelperReadContextExportRequest LogicJsonRequest;
	LogicJsonRequest.SourceText = T3DText;
	LogicJsonRequest.Format = EBlueprintHelperReadContextExportFormat::LogicJson;
	const FBlueprintHelperReadContextExportResult LogicJsonResult =
		FBlueprintHelperReadContextExportService::Export(LogicJsonRequest);

	TestTrue(TEXT("logicjson export succeeds"), LogicJsonResult.bSucceeded);
	TestTrue(TEXT("logicjson has ReadContext schema"), LogicJsonResult.ExportText.Contains(TEXT("ReadContextPack.v1")));
	TestTrue(TEXT("logicjson has format"), LogicJsonResult.ExportText.Contains(TEXT("logicjson")));
	TestEqual(TEXT("source text not mutated"), LogicJsonRequest.SourceText, T3DText);

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
