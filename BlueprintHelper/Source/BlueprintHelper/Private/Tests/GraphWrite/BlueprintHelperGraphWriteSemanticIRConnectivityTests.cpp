#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

class FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils
{
public:
	static FString MakeObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint()
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperGraphWriteConnectivity/%s"),
			*MakeObjectName(TEXT("Pkg"))));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeObjectName(TEXT("BP_GraphWriteConnectivity")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphWriteSemanticIRConnectivityTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* FindEventGraph(UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	}

	static UK2Node_CustomEvent* AddCustomEventNode(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph || EventName.IsEmpty())
		{
			return nullptr;
		}

		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->PostPlacedNewNode();
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

	static FString MakeUnconsumedPureDataLogicJson(const FString& EntryName)
	{
		return FString::Printf(TEXT(R"JSON({
			"options": { "reconstruct_existing_nodes": true },
			"logic_spec": {
				"schema": "BlueprintLogicSpec.v2",
				"entry": {
					"kind": "custom_event",
					"name": "%s",
					"id": "%s_entry",
					"signature_evidence_id": "signature:custom_event:%s",
					"signature_dependency": true,
					"source": "signature_dependency",
					"source_cluster": "blueprint_signature"
				},
				"statements": [{
					"id": "stmt_unconsumed_bool",
					"kind": "call",
					"target": "/Script/Engine.KismetMathLibrary:InRange_IntInt",
					"value_type": "bool",
					"result_symbol": "UnusedBool",
					"args": {
						"Value": { "kind": "literal", "value_type": "int", "value": 1 },
						"Min": { "kind": "literal", "value_type": "int", "value": 0 },
						"Max": { "kind": "literal", "value_type": "int", "value": 2 },
						"InclusiveMin": { "kind": "literal", "value_type": "bool", "value": true },
						"InclusiveMax": { "kind": "literal", "value_type": "bool", "value": true }
					}
				}]
			}
		})JSON"), *EntryName, *EntryName, *EntryName);
	}

	static FString MakeUnconsumedPureDataMultiGraphJson(
		const FString& GraphName,
		const FString& EntryName)
	{
		return FString::Printf(TEXT(R"JSON({
			"graphs": [{
				"graph": "%s",
				"options": { "reconstruct_existing_nodes": true },
				"logic_spec": {
					"schema": "BlueprintLogicSpec.v2",
					"entry": {
						"kind": "custom_event",
						"name": "%s",
						"id": "%s_entry",
						"signature_evidence_id": "signature:custom_event:%s",
						"signature_dependency": true,
						"source": "signature_dependency",
						"source_cluster": "blueprint_signature"
					},
					"statements": [{
						"id": "stmt_unconsumed_bool",
						"kind": "call",
						"target": "/Script/Engine.KismetMathLibrary:InRange_IntInt",
						"value_type": "bool",
						"result_symbol": "UnusedBool",
						"args": {
							"Value": { "kind": "literal", "value_type": "int", "value": 1 },
							"Min": { "kind": "literal", "value_type": "int", "value": 0 },
							"Max": { "kind": "literal", "value_type": "int", "value": 2 },
							"InclusiveMin": { "kind": "literal", "value_type": "bool", "value": true },
							"InclusiveMax": { "kind": "literal", "value_type": "bool", "value": true }
						}
					}]
				}
			}]
		})JSON"), *GraphName, *EntryName, *EntryName, *EntryName);
	}

	static bool HasConnectivityDiagnostic(
		const FBlueprintGenerateResult& Result,
		const FString& Code)
	{
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : Result.ConnectivityDiagnostics)
		{
			if (Diagnostic.Code == Code)
			{
				return true;
			}
		}
		return false;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteSemanticIRConnectivityRejectsUnconsumedPureDataResultTest,
	"BlueprintHelper.GraphWrite.SemanticIRConnectivity.RejectsUnconsumedPureDataResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteSemanticIRConnectivityRejectsUnconsumedPureDataResultTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::FindEventGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("event graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FString EntryName = FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::MakeObjectName(TEXT("ConnectivityEntry"));
	TestNotNull(
		TEXT("entry event"),
		FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::AddCustomEventNode(Graph, EntryName));

	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FBlueprintGenerateResult Result =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
			Graph,
			FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::MakeUnconsumedPureDataLogicJson(EntryName),
			Unresolved);
	for (const TSharedPtr<FUnresolvedNodeItem>& Item : Unresolved)
	{
		if (Item.IsValid())
		{
			AddError(FString::Printf(
				TEXT("unexpected unresolved item: %s - %s"),
				*Item->DisplayText,
				*Item->Reason));
		}
	}

	TestFalse(TEXT("unconsumed PureData blocks SemanticIR generation"), Result.bSucceed);
	TestEqual(TEXT("connectivity violation count"), Result.ConnectivityViolationCount, 1);
	TestTrue(
		TEXT("reports unconsumed_pure_data_node"),
		FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::HasConnectivityDiagnostic(
			Result,
			TEXT("unconsumed_pure_data_node")));
	TestEqual(TEXT("unresolved remains semantic-free"), Result.UnresolvedNodeCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteSemanticIRConnectivityMultiGraphPropagatesConnectivityFailureTest,
	"BlueprintHelper.GraphWrite.SemanticIRConnectivity.MultiGraphPropagatesConnectivityFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteSemanticIRConnectivityMultiGraphPropagatesConnectivityFailureTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::FindEventGraph(Blueprint);
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("event graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FString EntryName = FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::MakeObjectName(TEXT("ConnectivityMultiEntry"));
	TestNotNull(
		TEXT("entry event"),
		FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::AddCustomEventNode(Graph, EntryName));

	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FBlueprintGenerateResult Result =
		FBlueprintMultiGraphGenerationPipeline::GenerateMultiGraphFromJson(
			Blueprint,
			FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::MakeUnconsumedPureDataMultiGraphJson(
				Graph->GetName(),
				EntryName),
			Unresolved);

	TestFalse(TEXT("multi-graph propagates connectivity failure"), Result.bSucceed);
	TestEqual(TEXT("connectivity violation count"), Result.ConnectivityViolationCount, 1);
	TestTrue(
		TEXT("reports unconsumed_pure_data_node"),
		FBlueprintHelperGraphWriteSemanticIRConnectivityTestsLocalUtils::HasConnectivityDiagnostic(
			Result,
			TEXT("unconsumed_pure_data_node")));
	return true;
}

#endif
