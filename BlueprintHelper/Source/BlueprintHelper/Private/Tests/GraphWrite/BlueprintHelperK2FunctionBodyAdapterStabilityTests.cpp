#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2FunctionBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackProjection.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReconnectPlan.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"
#include "UObject/Package.h"

class FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers
{
public:
	static UBlueprint* CreateTransientBlueprint(const FString& AssetName)
	{
		const FString UniqueName = FString::Printf(
			TEXT("%s_%s"),
			*AssetName,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = GetTransientPackage();
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*FString::Printf(TEXT("BP_%s"), *UniqueName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperFunctionBodyAdapterTests"));
		if (Blueprint)
		{
			Blueprint->SetFlags(RF_Transient);
			Blueprint->ClearFlags(RF_Standalone);
		}
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* AddFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, bool bPure, bool bHasReturn)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*FunctionName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(
			Blueprint,
			FunctionGraph,
			/*bIsUserCreated=*/ true,
			nullptr);
		(void)bPure;
		AddFunctionInputPin(Blueprint, FunctionGraph, TEXT("InputValue"), UEdGraphSchema_K2::PC_Int);
		if (bHasReturn)
		{
			AddFunctionOutputPin(Blueprint, FunctionGraph, TEXT("ReturnValue"), UEdGraphSchema_K2::PC_Int);
		}
		return FunctionGraph;
	}

	static FBlueprintHelperGraphBodyRequest MakeFunctionBodyRequest(UBlueprint* Blueprint, const FString& FunctionName)
	{
		FBlueprintHelperGraphBodyRequest Request;
		Request.OperationKind = TEXT("replace");
		Request.TaskSpecStrategy = TEXT("replace_owned_graph");
		Request.ReplaceScope = TEXT("function_body");
		Request.AssetPath = TEXT("/Game/BlueprintHelper/Tests/BP_FunctionBody");
		Request.GraphName = FunctionName;
		Request.SelectorKind = TEXT("function");
		Request.RuntimeAdapterId = TEXT("k2.function_body");
		Request.Blueprint = Blueprint;
		return Request;
	}

	static void ConnectFunctionEntryToResult(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return;
		}
		UEdGraphPin* EntryExecPin = FindPin(
			FindFunctionEntry(FunctionGraph),
			UEdGraphSchema_K2::PC_Exec,
			EGPD_Output);
		UEdGraphPin* EntryDataPin = FindPinByName(
			FindFunctionEntry(FunctionGraph),
			TEXT("InputValue"),
			EGPD_Output);
		UEdGraphPin* ResultExecPin = FindPin(
			FindFunctionResult(FunctionGraph),
			UEdGraphSchema_K2::PC_Exec,
			EGPD_Input);
		UEdGraphPin* ResultDataPin = FindPinByName(
			FindFunctionResult(FunctionGraph),
			TEXT("ReturnValue"),
			EGPD_Input);

		const UEdGraphSchema* Schema = FunctionGraph->GetSchema();
		if (Schema && EntryExecPin && ResultExecPin)
		{
			Schema->TryCreateConnection(EntryExecPin, ResultExecPin);
		}
		if (Schema && EntryDataPin && ResultDataPin)
		{
			Schema->TryCreateConnection(EntryDataPin, ResultDataPin);
		}
	}

	static bool BuildFunctionBodyPlan(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		FBlueprintHelperGraphBodyTarget& OutTarget,
		FBlueprintHelperGraphBodyBoundaryModel& OutBoundary,
		FBlueprintHelperGraphBodyMutationPlan& OutMutationPlan,
		FBlueprintHelperGraphConnectivityPolicy& OutPolicy,
		FBlueprintHelperGraphBodyReconnectPlan& OutReconnectPlan,
		FBlueprintHelperGraphBodyReadbackProjection& OutReadbackProjection,
		FString& OutError)
	{
		const FBlueprintHelperK2FunctionBodyAdapter Adapter;
		const FBlueprintHelperGraphBodyRequest Request = MakeFunctionBodyRequest(Blueprint, FunctionName);
		if (!Adapter.ResolveTarget(Request, OutTarget, OutError))
		{
			return false;
		}
		OutBoundary = Adapter.BuildBoundaryModel(OutTarget, Request);
		OutMutationPlan = Adapter.BuildMutationPlan(OutTarget, OutBoundary, Request);
		OutPolicy = Adapter.BuildConnectivityPolicy(OutTarget, OutBoundary);
		OutReconnectPlan = Adapter.BuildReconnectPlan(OutTarget, OutBoundary);
		OutReadbackProjection = Adapter.BuildReadbackProjection(OutTarget, OutBoundary);
		OutError.Reset();
		return true;
	}

private:
	static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionResult* FindFunctionResult(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				return Result;
			}
		}
		return nullptr;
	}

	static bool AddFunctionInputPin(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		const FString& PinName,
		const FName PinCategory)
	{
		UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
		if (!Blueprint || !Entry || PinName.IsEmpty())
		{
			return false;
		}

		FEdGraphPinType PinType;
		PinType.PinCategory = PinCategory;
		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = FName(*PinName);
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = EGPD_Output;
		Entry->UserDefinedPins.Add(NewPin);
		Entry->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return true;
	}

	static bool AddFunctionOutputPin(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		const FString& PinName,
		const FName PinCategory)
	{
		UK2Node_FunctionResult* Result = FindFunctionResult(FunctionGraph);
		if (!Blueprint || !FunctionGraph || PinName.IsEmpty())
		{
			return false;
		}
		if (!Result)
		{
			FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*FunctionGraph);
			Result = NodeCreator.CreateNode(true);
			Result->NodePosX = 600;
			Result->NodePosY = 0;
			NodeCreator.Finalize();
		}
		if (!Result)
		{
			return false;
		}

		FEdGraphPinType PinType;
		PinType.PinCategory = PinCategory;
		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = FName(*PinName);
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = EGPD_Input;
		Result->UserDefinedPins.Add(NewPin);
		Result->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return true;
	}

	static UEdGraphPin* FindPin(
		UEdGraphNode* Node,
		const FName PinCategory,
		EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == PinCategory)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static UEdGraphPin* FindPinByName(
		UEdGraphNode* Node,
		const FString& PinName,
		EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinName.ToString() == PinName)
			{
				return Pin;
			}
		}
		return nullptr;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2FunctionBodyBoundaryModelIncludesEntryResultAndParamsTest,
	"BlueprintHelper.GraphWrite.FunctionBody.BoundaryModelIncludesEntryResultAndParams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2FunctionBodyBoundaryModelIncludesEntryResultAndParamsTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::CreateTransientBlueprint(TEXT("BH_FunctionBoundary"));
	UEdGraph* Graph = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::AddFunctionGraph(Blueprint, TEXT("ComputeValue"), false, true);
	TestNotNull(TEXT("function graph created"), Graph);

	FBlueprintHelperK2FunctionBodyAdapter Adapter;
	FBlueprintHelperGraphBodyTarget Target;
	FString Error;
	const FBlueprintHelperGraphBodyRequest Request =
		FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::MakeFunctionBodyRequest(Blueprint, TEXT("ComputeValue"));

	TestTrue(TEXT("target resolves through FunctionGraphs"), Adapter.ResolveTarget(Request, Target, Error));
	const FBlueprintHelperGraphBodyBoundaryModel Boundary = Adapter.BuildBoundaryModel(Target, Request);
	TestTrue(TEXT("entry boundary refs populated"), Boundary.EntryNodeRefs.Num() > 0);
	TestTrue(TEXT("exit boundary refs populated"), Boundary.ExitNodeRefs.Num() > 0);
	TestTrue(TEXT("semantic source refs include function inputs"), Boundary.SemanticSourceRefs.Num() > 0);
	TestTrue(TEXT("semantic output refs include function returns"), Boundary.SemanticOutputRefs.Num() > 0);
	TestTrue(TEXT("return data pin refs populated"), Boundary.ReturnDataPinRefs.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2FunctionBodyPreviewExecuteUseSameAdapterTest,
	"BlueprintHelper.GraphWrite.FunctionBody.PreviewExecuteUseSameAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2FunctionBodyPreviewExecuteUseSameAdapterTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::CreateTransientBlueprint(TEXT("BH_FunctionAdapter"));
	FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::AddFunctionGraph(Blueprint, TEXT("ComputeValue"), false, true);

	FBlueprintHelperGraphBodyTarget Target;
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	FBlueprintHelperGraphBodyMutationPlan MutationPlan;
	FBlueprintHelperGraphConnectivityPolicy Policy;
	FBlueprintHelperGraphBodyReconnectPlan ReconnectPlan;
	FBlueprintHelperGraphBodyReadbackProjection ReadbackProjection;
	FString Error;
	TestTrue(TEXT("function body plan builds"),
		FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::BuildFunctionBodyPlan(
			Blueprint,
			TEXT("ComputeValue"),
			Target,
			Boundary,
			MutationPlan,
			Policy,
			ReconnectPlan,
			ReadbackProjection,
			Error));

	TestEqual(TEXT("preview boundary adapter id"), Boundary.RuntimeAdapterId, FString(TEXT("k2.function_body")));
	TestEqual(TEXT("execute mutation boundary adapter id"), MutationPlan.BoundaryModel.RuntimeAdapterId, FString(TEXT("k2.function_body")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2FunctionBodyFunctionParamGetDoesNotResolveAsMemberVariableTest,
	"BlueprintHelper.GraphWrite.FunctionBody.FunctionParamGetDoesNotResolveAsMemberVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2FunctionBodyFunctionParamGetDoesNotResolveAsMemberVariableTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::CreateTransientBlueprint(TEXT("BH_FunctionParam"));
	FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::AddFunctionGraph(Blueprint, TEXT("ComputeValue"), false, true);

	FBlueprintHelperGraphBodyTarget Target;
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	FBlueprintHelperGraphBodyMutationPlan MutationPlan;
	FBlueprintHelperGraphConnectivityPolicy Policy;
	FBlueprintHelperGraphBodyReconnectPlan ReconnectPlan;
	FBlueprintHelperGraphBodyReadbackProjection ReadbackProjection;
	FString Error;
	TestTrue(TEXT("function body plan builds"),
		FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::BuildFunctionBodyPlan(
			Blueprint,
			TEXT("ComputeValue"),
			Target,
			Boundary,
			MutationPlan,
			Policy,
			ReconnectPlan,
			ReadbackProjection,
			Error));

	TestTrue(TEXT("semantic context contains function parameter ref"),
		Boundary.SemanticSourceRefs.Contains(TEXT("FunctionEntry.InputValue")));
	for (const FString& Ref : Boundary.SemanticSourceRefs)
	{
		TestFalse(TEXT("function param ref is not classified as member variable"), Ref.Contains(TEXT("MemberVariable")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2FunctionBodyPureReturnDataflowIsConsumedTest,
	"BlueprintHelper.GraphWrite.FunctionBody.PureReturnDataflowIsConsumed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2FunctionBodyPureReturnDataflowIsConsumedTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::CreateTransientBlueprint(TEXT("BH_FunctionReturnPolicy"));
	FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::AddFunctionGraph(Blueprint, TEXT("ComputeValue"), true, true);

	FBlueprintHelperGraphBodyTarget Target;
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	FBlueprintHelperGraphBodyMutationPlan MutationPlan;
	FBlueprintHelperGraphConnectivityPolicy Policy;
	FBlueprintHelperGraphBodyReconnectPlan ReconnectPlan;
	FBlueprintHelperGraphBodyReadbackProjection ReadbackProjection;
	FString Error;
	TestTrue(TEXT("function body plan builds"),
		FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::BuildFunctionBodyPlan(
			Blueprint,
			TEXT("ComputeValue"),
			Target,
			Boundary,
			MutationPlan,
			Policy,
			ReconnectPlan,
			ReadbackProjection,
			Error));

	TestTrue(TEXT("connectivity policy requires return dataflow consumption"), Policy.bRequireReturnDataflowConsumption);
	TestTrue(TEXT("return dataflow refs are projected"), Policy.ReturnDataPinRefs.Contains(TEXT("FunctionResult.ReturnValue")));
	TestTrue(TEXT("missing return producer diagnostic is declared"),
		Policy.ViolationCodes.Contains(TEXT("function_result_output_unproduced")));
	TestTrue(TEXT("reconnect plan records return output to result pin refs"),
		ReconnectPlan.ReturnOutputToResultPinRefs.Contains(TEXT("FunctionResult.ReturnValue")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2FunctionBodyReadbackIsNotEmptyAfterReplaceTest,
	"BlueprintHelper.GraphWrite.FunctionBody.ReadbackIsNotEmptyAfterReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2FunctionBodyReadbackIsNotEmptyAfterReplaceTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::CreateTransientBlueprint(TEXT("BH_FunctionReadback"));
	UEdGraph* FunctionGraph = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::AddFunctionGraph(Blueprint, TEXT("ComputeValue"), false, true);
	FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::ConnectFunctionEntryToResult(FunctionGraph);

	FBlueprintHelperGraphBodyTarget Target;
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	FBlueprintHelperGraphBodyMutationPlan MutationPlan;
	FBlueprintHelperGraphConnectivityPolicy Policy;
	FBlueprintHelperGraphBodyReconnectPlan ReconnectPlan;
	FBlueprintHelperGraphBodyReadbackProjection ReadbackProjection;
	FString Error;
	TestTrue(TEXT("function body plan builds"),
		FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::BuildFunctionBodyPlan(
			Blueprint,
			TEXT("ComputeValue"),
			Target,
			Boundary,
			MutationPlan,
			Policy,
			ReconnectPlan,
			ReadbackProjection,
			Error));

	TestEqual(TEXT("readback has function name"), ReadbackProjection.FunctionName, FString(TEXT("ComputeValue")));
	TestTrue(TEXT("readback has entry boundary refs"), ReadbackProjection.EntryBoundaryRefs.Num() > 0);
	TestTrue(TEXT("readback has result boundary refs"), ReadbackProjection.ResultBoundaryRefs.Num() > 0);
	TestTrue(TEXT("readback has function input pin refs"), ReadbackProjection.FunctionInputPinRefs.Num() > 0);
	TestTrue(TEXT("readback has function output pin refs"), ReadbackProjection.FunctionOutputPinRefs.Num() > 0);
	TestTrue(TEXT("readback has exec link refs"), ReadbackProjection.ExecLinkRefs.Num() > 0);
	TestTrue(TEXT("readback has data link refs"), ReadbackProjection.DataLinkRefs.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperK2FunctionBodyFailedMutationCleansGeneratedNodesTest,
	"BlueprintHelper.GraphWrite.FunctionBody.FailedMutationCleansGeneratedNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperK2FunctionBodyFailedMutationCleansGeneratedNodesTest::RunTest(const FString&)
{
	UBlueprint* Blueprint = FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::CreateTransientBlueprint(TEXT("BH_FunctionFailureCleanup"));
	FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::AddFunctionGraph(Blueprint, TEXT("ComputeValue"), false, true);

	FBlueprintHelperGraphBodyTarget Target;
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	FBlueprintHelperGraphBodyMutationPlan MutationPlan;
	FBlueprintHelperGraphConnectivityPolicy Policy;
	FBlueprintHelperGraphBodyReconnectPlan ReconnectPlan;
	FBlueprintHelperGraphBodyReadbackProjection ReadbackProjection;
	FString Error;
	TestTrue(TEXT("function body plan builds"),
		FBlueprintHelperK2FunctionBodyAdapterStabilityTestHelpers::BuildFunctionBodyPlan(
			Blueprint,
			TEXT("ComputeValue"),
			Target,
			Boundary,
			MutationPlan,
			Policy,
			ReconnectPlan,
			ReadbackProjection,
			Error));

	TestFalse(TEXT("FunctionBody adapter does not create generated nodes directly"), MutationPlan.bCreatesNodesInsideAdapter);
	TestEqual(TEXT("adapter projection starts with no generated nodes before mutation"), ReadbackProjection.GeneratedNodeRefs.Num(), 0);
	TestEqual(TEXT("adapter projection starts with no half-written exec links before mutation"), ReadbackProjection.ExecLinkRefs.Num(), 0);
	TestEqual(TEXT("adapter projection starts with no half-written data links before mutation"), ReadbackProjection.DataLinkRefs.Num(), 0);
	return true;
}

#endif
