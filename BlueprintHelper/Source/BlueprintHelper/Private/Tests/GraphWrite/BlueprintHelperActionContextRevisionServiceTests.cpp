#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionService.h"

class FBlueprintHelperActionContextRevisionServiceTestUtils
{
public:
	static UBlueprint* CreateTransientBlueprint()
	{
		UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage(), NAME_None, RF_Transient);
		Blueprint->BlueprintType = BPTYPE_Normal;
		Blueprint->ParentClass = AActor::StaticClass();
		return Blueprint;
	}

	static UEdGraph* AddTransientGraph(UBlueprint* Blueprint, const FString& GraphName)
	{
		UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, FName(*GraphName), RF_Transient);
		Graph->Schema = UEdGraphSchema_K2::StaticClass();
		Blueprint->UbergraphPages.Add(Graph);
		return Graph;
	}

	static UEdGraphNode* AddNode(UEdGraph* Graph, const FString& NodeName)
	{
		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, FName(*NodeName), RF_Transient);
		Node->CreateNewGuid();
		Graph->AddNode(Node, false, false);
		return Node;
	}

	static UEdGraphPin* AddPin(
		UEdGraphNode* Node,
		const FString& PinName,
		EEdGraphPinDirection Direction,
		const FName& Category)
	{
		return Node->CreatePin(Direction, Category, FName(*PinName));
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextBlueprintRevisionVariableTest,
	"BlueprintHelper.GraphWrite.ActionContext.RevisionService.BlueprintVariableChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextBlueprintRevisionVariableTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperActionContextRevisionServiceTestUtils::CreateTransientBlueprint();
	UEdGraph* Graph = FBlueprintHelperActionContextRevisionServiceTestUtils::AddTransientGraph(Blueprint, TEXT("EventGraph"));

	const FBlueprintHelperActionContextRevisionToken Before =
		FBlueprintHelperActionContextRevisionService::BuildRevisionToken(Blueprint, Graph, TEXT("test"), TEXT("plan"));

	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(TEXT("bP0AFlag")), PinType);

	const FBlueprintHelperActionContextRevisionToken After =
		FBlueprintHelperActionContextRevisionService::BuildRevisionToken(Blueprint, Graph, TEXT("test"), TEXT("plan"));

	TestNotEqual(TEXT("member variable changes BlueprintRevision"), Before.BlueprintRevision, After.BlueprintRevision);
	TestEqual(TEXT("same graph without graph edits keeps GraphRevision"), Before.GraphRevision, After.GraphRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextGraphRevisionNodePinLinkTest,
	"BlueprintHelper.GraphWrite.ActionContext.RevisionService.GraphNodePinLinkChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextGraphRevisionNodePinLinkTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperActionContextRevisionServiceTestUtils::CreateTransientBlueprint();
	UEdGraph* Graph = FBlueprintHelperActionContextRevisionServiceTestUtils::AddTransientGraph(Blueprint, TEXT("EventGraph"));
	const FBlueprintHelperActionContextRevisionToken Before =
		FBlueprintHelperActionContextRevisionService::BuildRevisionToken(Blueprint, Graph, TEXT("test"), TEXT("plan"));

	UEdGraphNode* NodeA = FBlueprintHelperActionContextRevisionServiceTestUtils::AddNode(Graph, TEXT("NodeA"));
	UEdGraphNode* NodeB = FBlueprintHelperActionContextRevisionServiceTestUtils::AddNode(Graph, TEXT("NodeB"));
	UEdGraphPin* OutPin = FBlueprintHelperActionContextRevisionServiceTestUtils::AddPin(NodeA, TEXT("Then"), EGPD_Output, UEdGraphSchema_K2::PC_Exec);
	UEdGraphPin* InPin = FBlueprintHelperActionContextRevisionServiceTestUtils::AddPin(NodeB, TEXT("Execute"), EGPD_Input, UEdGraphSchema_K2::PC_Exec);
	FBlueprintHelperVersionCompat::MakePinLinkTo(OutPin, InPin, true);

	const FBlueprintHelperActionContextRevisionToken After =
		FBlueprintHelperActionContextRevisionService::BuildRevisionToken(Blueprint, Graph, TEXT("test"), TEXT("plan"));

	TestNotEqual(TEXT("node/pin/link changes GraphRevision"), Before.GraphRevision, After.GraphRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextGraphRevisionIgnoresPositionTest,
	"BlueprintHelper.GraphWrite.ActionContext.RevisionService.GraphPositionIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextGraphRevisionIgnoresPositionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperActionContextRevisionServiceTestUtils::CreateTransientBlueprint();
	UEdGraph* Graph = FBlueprintHelperActionContextRevisionServiceTestUtils::AddTransientGraph(Blueprint, TEXT("EventGraph"));
	UEdGraphNode* Node = FBlueprintHelperActionContextRevisionServiceTestUtils::AddNode(Graph, TEXT("PositionNode"));

	const FBlueprintHelperActionContextRevisionToken Before =
		FBlueprintHelperActionContextRevisionService::BuildRevisionToken(Blueprint, Graph, TEXT("test"), TEXT("plan"));

	Node->NodePosX += 400;
	Node->NodePosY += 200;

	const FBlueprintHelperActionContextRevisionToken After =
		FBlueprintHelperActionContextRevisionService::BuildRevisionToken(Blueprint, Graph, TEXT("test"), TEXT("plan"));

	TestEqual(TEXT("position changes do not affect GraphRevision"), Before.GraphRevision, After.GraphRevision);
	return true;
}

#endif
