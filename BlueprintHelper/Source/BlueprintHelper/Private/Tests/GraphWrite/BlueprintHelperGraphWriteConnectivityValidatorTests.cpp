#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Knot.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h"

namespace BlueprintHelperGraphWriteConnectivityValidatorTests
{
	static UEdGraphNode* AddNodeWithPins(
		UEdGraph* Graph,
		const FName NodeName,
		const bool bExecInput,
		const bool bExecOutput,
		const bool bDataInput,
		const bool bDataOutput)
	{
		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeName);
		Graph->AddNode(Node, true, false);

		FEdGraphPinType ExecPinType;
		ExecPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;

		FEdGraphPinType BoolPinType;
		BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

		if (bExecInput)
		{
			Node->CreatePin(EGPD_Input, ExecPinType, FName(TEXT("execute")));
		}
		if (bExecOutput)
		{
			Node->CreatePin(EGPD_Output, ExecPinType, FName(TEXT("then")));
		}
		if (bDataInput)
		{
			Node->CreatePin(EGPD_Input, BoolPinType, FName(TEXT("condition")));
		}
		if (bDataOutput)
		{
			Node->CreatePin(EGPD_Output, BoolPinType, FName(TEXT("value")));
		}

		return Node;
	}

	static UEdGraphPin* FindPin(
		UEdGraphNode* Node,
		const FName PinName,
		const EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName == PinName && Pin->Direction == Direction)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static void ForceLink(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
	{
		check(FromPin);
		check(ToPin);

		FromPin->LinkedTo.AddUnique(ToPin);
		ToPin->LinkedTo.AddUnique(FromPin);
	}

	static void RegisterBoundaryNode(
		FBlueprintGraphWriteConnectivityValidationInput& Input,
		const FString& Ref,
		UEdGraphNode* Node,
		const bool bEntry,
		const bool bExit)
	{
		if (!Node || Ref.IsEmpty())
		{
			return;
		}

		Input.NodeRefs.Add(Ref, Node);
		Input.BoundaryModel.GeneratedNodeRefs.AddUnique(Ref);
		if (bEntry)
		{
			Input.BoundaryModel.EntryNodeRefs.AddUnique(Ref);
		}
		if (bExit)
		{
			Input.BoundaryModel.ExitNodeRefs.AddUnique(Ref);
		}
	}

	static void RegisterEntryNode(
		FBlueprintGraphWriteConnectivityValidationInput& Input,
		const FString& Ref,
		UEdGraphNode* Node)
	{
		RegisterBoundaryNode(Input, Ref, Node, true, false);
	}

	static FBlueprintGraphWriteConnectivityValidationResult ValidateInput(
		FBlueprintGraphWriteConnectivityValidationInput& Input)
	{
		Input.ConnectivityPolicy =
			FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Input.BoundaryModel);
		return FBlueprintHelperGraphWriteConnectivityValidator::Validate(Input);
	}

	static bool HasViolation(
		const FBlueprintGraphWriteConnectivityValidationResult& Result,
		const FString& Code)
	{
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : Result.Diagnostics)
		{
			if (Diagnostic.Code == Code)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorRejectsExecWithoutIncomingTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.RejectsExecWithoutIncoming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorRejectsExecWithoutIncomingTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_ExecMissing")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);
	UEdGraphNode* OrphanExec = AddNodeWithPins(Graph, FName(TEXT("OrphanExec")), true, true, false, false);

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { Entry, OrphanExec };
	RegisterEntryNode(Input, TEXT("Entry"), Entry);

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestFalse(TEXT("validation blocks orphan exec"), Result.bPassed);
	TestTrue(TEXT("reports unreachable_exec_node"), HasViolation(Result, TEXT("unreachable_exec_node")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorRejectsExecWithoutEntryReachabilityTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.RejectsExecWithoutEntryReachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorRejectsExecWithoutEntryReachabilityTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_ExecUnreachableCycle")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);
	UEdGraphNode* CycleA = AddNodeWithPins(Graph, FName(TEXT("CycleA")), true, true, false, false);
	UEdGraphNode* CycleB = AddNodeWithPins(Graph, FName(TEXT("CycleB")), true, true, false, false);

	ForceLink(
		FindPin(CycleA, FName(TEXT("then")), EGPD_Output),
		FindPin(CycleB, FName(TEXT("execute")), EGPD_Input));
	ForceLink(
		FindPin(CycleB, FName(TEXT("then")), EGPD_Output),
		FindPin(CycleA, FName(TEXT("execute")), EGPD_Input));

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { Entry, CycleA, CycleB };
	RegisterEntryNode(Input, TEXT("Entry"), Entry);

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestFalse(TEXT("validation blocks exec cycle not reachable from entry"), Result.bPassed);
	TestTrue(TEXT("reports unreachable_exec_node"), HasViolation(Result, TEXT("unreachable_exec_node")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorReportsMissingExpectedLinkTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.ReportsMissingExpectedLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorReportsMissingExpectedLinkTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_MissingExpectedLink")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { Entry };
	RegisterEntryNode(Input, TEXT("Entry"), Entry);
	Input.RequestedConnectionCount = 2;
	Input.CreatedConnectionCount = 1;

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestFalse(TEXT("validation blocks missing requested link"), Result.bPassed);
	TestTrue(TEXT("reports missing_expected_link"), HasViolation(Result, TEXT("missing_expected_link")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataWithoutConsumerTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.RejectsPureDataWithoutConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataWithoutConsumerTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_PureMissing")));
	UEdGraphNode* PureData = AddNodeWithPins(Graph, FName(TEXT("PureData")), false, false, false, true);

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { PureData };

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestFalse(TEXT("validation blocks unconsumed pure data"), Result.bPassed);
	TestTrue(TEXT("reports unconsumed_pure_data_node"), HasViolation(Result, TEXT("unconsumed_pure_data_node")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorAcceptsConsumedPureDataTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.AcceptsConsumedPureData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorAcceptsConsumedPureDataTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_PureConsumed")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);
	UEdGraphNode* ExecNode = AddNodeWithPins(Graph, FName(TEXT("ExecNode")), true, true, true, false);
	UEdGraphNode* PureData = AddNodeWithPins(Graph, FName(TEXT("PureData")), false, false, false, true);

	ForceLink(
		FindPin(Entry, FName(TEXT("then")), EGPD_Output),
		FindPin(ExecNode, FName(TEXT("execute")), EGPD_Input));
	ForceLink(
		FindPin(PureData, FName(TEXT("value")), EGPD_Output),
		FindPin(ExecNode, FName(TEXT("condition")), EGPD_Input));

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { Entry, ExecNode, PureData };
	RegisterEntryNode(Input, TEXT("Entry"), Entry);

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestTrue(TEXT("consumed pure data passes"), Result.bPassed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataChainWithoutReachableExecTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.RejectsPureDataChainWithoutReachableExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataChainWithoutReachableExecTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_PureChainMissingExec")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);
	UEdGraphNode* PureData = AddNodeWithPins(Graph, FName(TEXT("PureData")), false, false, false, true);
	UEdGraphNode* PureConsumer = AddNodeWithPins(Graph, FName(TEXT("PureConsumer")), false, false, true, true);

	ForceLink(
		FindPin(PureData, FName(TEXT("value")), EGPD_Output),
		FindPin(PureConsumer, FName(TEXT("condition")), EGPD_Input));

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { Entry, PureData, PureConsumer };
	RegisterEntryNode(Input, TEXT("Entry"), Entry);

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestFalse(TEXT("pure data chain without reachable exec is blocked"), Result.bPassed);
	TestTrue(TEXT("reports unreachable_pure_data_chain"), HasViolation(Result, TEXT("unreachable_pure_data_chain")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataLinkedToExecOutputTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.RejectsPureDataLinkedToExecOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorRejectsPureDataLinkedToExecOutputTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_PureLinkedToExecOutput")));
	UEdGraphNode* Entry = AddNodeWithPins(Graph, FName(TEXT("Entry")), false, true, false, false);
	UEdGraphNode* ExecNode = AddNodeWithPins(Graph, FName(TEXT("ExecNode")), true, true, true, true);
	UEdGraphNode* PureData = AddNodeWithPins(Graph, FName(TEXT("PureData")), false, false, false, true);

	ForceLink(
		FindPin(Entry, FName(TEXT("then")), EGPD_Output),
		FindPin(ExecNode, FName(TEXT("execute")), EGPD_Input));
	ForceLink(
		FindPin(PureData, FName(TEXT("value")), EGPD_Output),
		FindPin(ExecNode, FName(TEXT("value")), EGPD_Output));

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { Entry, ExecNode, PureData };
	RegisterEntryNode(Input, TEXT("Entry"), Entry);

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestFalse(TEXT("pure data linked to an output pin is not consumed"), Result.bPassed);
	TestTrue(TEXT("reports unconsumed_pure_data_node"), HasViolation(Result, TEXT("unconsumed_pure_data_node")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteConnectivityValidatorWhitelistsCommentAndRerouteTest,
	"BlueprintHelper.GraphWrite.ConnectivityValidation.WhitelistsCommentAndReroute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteConnectivityValidatorWhitelistsCommentAndRerouteTest::RunTest(const FString&)
{
	using namespace BlueprintHelperGraphWriteConnectivityValidatorTests;

	UEdGraph* Graph = NewObject<UEdGraph>(
		GetTransientPackage(),
		FName(TEXT("BH_Connectivity_Whitelist")));
	UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph);
	Graph->AddNode(Comment, true, false);

	UK2Node_Knot* Reroute = NewObject<UK2Node_Knot>(Graph);
	Graph->AddNode(Reroute, true, false);
	Reroute->AllocateDefaultPins();

	FBlueprintGraphWriteConnectivityValidationInput Input;
	Input.TargetGraph = Graph;
	Input.GeneratedNodes = { Comment, Reroute };

	const FBlueprintGraphWriteConnectivityValidationResult Result = ValidateInput(Input);

	TestTrue(TEXT("comment/reroute whitelist passes"), Result.bPassed);
	return true;
}

#endif
