#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

#include <initializer_list>

namespace BlueprintHelperGraphLayoutSolverTests
{
using namespace BlueprintHelper::GraphLayout;

static FPinSnapshot MakePin(
	const FString& Name,
	EPinDirection Direction,
	bool bExec,
	std::initializer_list<const TCHAR*> LinkedNodeIds = {})
{
	FPinSnapshot Pin;
	Pin.PinId = Name;
	Pin.Name = Name;
	Pin.Direction = Direction;
	Pin.bExec = bExec;
	Pin.Category = bExec ? TEXT("exec") : TEXT("object");
	for (const TCHAR* LinkedNodeId : LinkedNodeIds)
	{
		Pin.LinkedNodeIds.Add(LinkedNodeId);
	}
	return Pin;
}

static FNodeSnapshot MakeNode(
	const FString& NodeId,
	const FString& ClassPath,
	const FString& Title,
	const FVector2D& Position,
	const FVector2D& Size,
	bool bExisting,
	std::initializer_list<FPinSnapshot> Pins)
{
	FNodeSnapshot Node;
	Node.NodeId = NodeId;
	Node.StableName = NodeId;
	Node.ClassPath = ClassPath;
	Node.Title = Title;
	Node.Position = Position;
	Node.Size = Size;
	Node.bExisting = bExisting;
	for (const FPinSnapshot& Pin : Pins)
	{
		Node.Pins.Add(Pin);
	}
	return Node;
}

static const FNodePlacement* FindPlacement(const FLayoutPlan& Plan, const FString& NodeId)
{
	for (const FNodePlacement& Placement : Plan.Placements)
	{
		if (Placement.NodeId == NodeId)
		{
			return &Placement;
		}
	}
	return nullptr;
}

static bool RectsOverlap(const FNodePlacement& A, const FVector2D& ASize, const FNodePlacement& B, const FVector2D& BSize)
{
	const FVector2D AMin = A.TargetPosition;
	const FVector2D AMax = A.TargetPosition + ASize;
	const FVector2D BMin = B.TargetPosition;
	const FVector2D BMax = B.TargetPosition + BSize;
	return AMin.X < BMax.X && AMax.X > BMin.X && AMin.Y < BMax.Y && AMax.Y > BMin.Y;
}

static bool ExpandedRectsOverlap(
	const FNodePlacement& A,
	const FVector2D& ASize,
	const FNodePlacement& B,
	const FVector2D& BSize,
	const float PaddingX,
	const float PaddingY)
{
	const FVector2D Padding(PaddingX, PaddingY);
	const FVector2D AMin = A.TargetPosition - Padding;
	const FVector2D AMax = A.TargetPosition + ASize + Padding;
	const FVector2D BMin = B.TargetPosition - Padding;
	const FVector2D BMax = B.TargetPosition + BSize + Padding;
	return AMin.X < BMax.X && AMax.X > BMin.X && AMin.Y < BMax.Y && AMax.Y > BMin.Y;
}

static FRuleSet MakeRuleSetWithCanvasOffsets()
{
	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 420.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.BranchRowSpacing = 160.0f;
	RuleSet.PureInputOffsetX = 260.0f;
	RuleSet.VariableInputOffsetX = 240.0f;
	RuleSet.InputPinRowSpacing = 48.0f;
	RuleSet.bMoveGeneratedNodes = true;
	RuleSet.bMoveExistingNodes = false;
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::ExecNode, FVector2D(300.0f, 100.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::PureFunction, FVector2D(80.0f, 230.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(90.0f, 310.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::VariableInput, FVector2D(70.0f, 190.0f));
	return RuleSet;
}

static UEdGraphNode* AddCoordinatorTestNode(
	UEdGraph* Graph,
	const FName NodeName,
	const int32 X,
	const int32 Y,
	const bool bHasExecInput,
	const bool bHasExecOutput)
{
	UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeName);
	Graph->AddNode(Node, true, false);
	Node->CreateNewGuid();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Node->NodeWidth = 220;
	Node->NodeHeight = 100;

	FEdGraphPinType ExecPinType;
	ExecPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;
	if (bHasExecInput)
	{
		Node->CreatePin(EGPD_Input, ExecPinType, FName(TEXT("execute")));
	}
	if (bHasExecOutput)
	{
		Node->CreatePin(EGPD_Output, ExecPinType, FName(TEXT("then")));
	}
	return Node;
}

static UEdGraphPin* FindCoordinatorTestPin(UEdGraphNode* Node, const FName PinName)
{
	if (!Node)
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName == PinName)
		{
			return Pin;
		}
	}
	return nullptr;
}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DataInputsUseEditorCanvasVerticalOffsets,
	"BlueprintHelper.GraphLayout.Solver.DataInputsUseEditorCanvasVerticalOffsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataInputsUseEditorCanvasVerticalOffsets::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print String"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("Pure")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Pure"),
		TEXT("K2Node_CallFunction"),
		TEXT("Get Display Name"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Exec")})}));

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, MakeRuleSetWithCanvasOffsets());
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* PurePlacement = FindPlacement(Plan, TEXT("Pure"));

	TestNotNull(TEXT("Exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("Pure placement exists"), PurePlacement);
	if (!ExecPlacement || !PurePlacement)
	{
		return false;
	}

	TestTrue(TEXT("Pure input is below exec by editor canvas role offset"), PurePlacement->TargetPosition.Y > ExecPlacement->TargetPosition.Y + 80.0f);
	TestTrue(TEXT("Pure input remains left of consumer"), PurePlacement->TargetPosition.X < ExecPlacement->TargetPosition.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_CoordinatorFlushAppliesBeforeReturn,
	"BlueprintHelper.GraphLayout.Coordinator.FlushAppliesBeforeReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_CoordinatorFlushAppliesBeforeReturn::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;

	FBlueprintHelperGraphLayoutCoordinator::Startup();

	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_CoordinatorFlushGraph")));
	UEdGraphNode* EntryNode = AddCoordinatorTestNode(Graph, FName(TEXT("K2Node_CustomEvent_Test")), 0, 0, false, true);
	UEdGraphNode* ExecNode = AddCoordinatorTestNode(Graph, FName(TEXT("K2Node_CallFunction_Test")), 0, 0, true, true);
	TestNotNull(TEXT("Entry node is created"), EntryNode);
	TestNotNull(TEXT("Exec node is created"), ExecNode);
	if (!EntryNode || !ExecNode)
	{
		FBlueprintHelperGraphLayoutCoordinator::Shutdown();
		return false;
	}

	UEdGraphPin* EntryThenPin = FindCoordinatorTestPin(EntryNode, FName(TEXT("then")));
	UEdGraphPin* ExecInputPin = FindCoordinatorTestPin(ExecNode, FName(TEXT("execute")));
	TestNotNull(TEXT("Entry exec output pin exists"), EntryThenPin);
	TestNotNull(TEXT("Exec input pin exists"), ExecInputPin);
	if (!EntryThenPin || !ExecInputPin)
	{
		FBlueprintHelperGraphLayoutCoordinator::Shutdown();
		return false;
	}
	EntryThenPin->MakeLinkTo(ExecInputPin);

	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Graph, {EntryNode, ExecNode});
	const bool bFlushSucceeded = FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts();

	const bool bExecMovedRight = ExecNode->NodePosX > EntryNode->NodePosX + 100;
	FBlueprintHelperGraphLayoutCoordinator::Shutdown();

	TestTrue(TEXT("FlushPendingTaskLayouts reports success before returning"), bFlushSucceeded);
	TestTrue(TEXT("FlushPendingTaskLayouts applies generated-node layout before returning"), bExecMovedRight);
	return bFlushSucceeded && bExecMovedRight;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_OffThreadRecordThenFlushAppliesBeforeReturn,
	"BlueprintHelper.GraphLayout.Coordinator.OffThreadRecordThenFlushAppliesBeforeReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OffThreadRecordThenFlushAppliesBeforeReturn::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;

	FBlueprintHelperGraphLayoutCoordinator::Startup();

	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_OffThreadCoordinatorFlushGraph")));
	UEdGraphNode* EntryNode = AddCoordinatorTestNode(Graph, FName(TEXT("K2Node_CustomEvent_OffThread")), 0, 0, false, true);
	UEdGraphNode* ExecNode = AddCoordinatorTestNode(Graph, FName(TEXT("K2Node_CallFunction_OffThread")), 0, 0, true, true);
	TestNotNull(TEXT("Entry node is created"), EntryNode);
	TestNotNull(TEXT("Exec node is created"), ExecNode);
	if (!EntryNode || !ExecNode)
	{
		FBlueprintHelperGraphLayoutCoordinator::Shutdown();
		return false;
	}

	UEdGraphPin* EntryThenPin = FindCoordinatorTestPin(EntryNode, FName(TEXT("then")));
	UEdGraphPin* ExecInputPin = FindCoordinatorTestPin(ExecNode, FName(TEXT("execute")));
	TestNotNull(TEXT("Entry exec output pin exists"), EntryThenPin);
	TestNotNull(TEXT("Exec input pin exists"), ExecInputPin);
	if (!EntryThenPin || !ExecInputPin)
	{
		FBlueprintHelperGraphLayoutCoordinator::Shutdown();
		return false;
	}
	EntryThenPin->MakeLinkTo(ExecInputPin);

	TFuture<bool> WorkerResult = Async(EAsyncExecution::ThreadPool, [Graph, EntryNode, ExecNode]()
	{
		FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Graph, {EntryNode, ExecNode});
		return FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts();
	});
	const double DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
	while (!WorkerResult.IsReady() && FPlatformTime::Seconds() < DeadlineSeconds)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FPlatformProcess::Sleep(0.001f);
	}
	if (!WorkerResult.IsReady())
	{
		FBlueprintHelperGraphLayoutCoordinator::Shutdown();
		AddError(TEXT("Off-thread graph layout worker did not complete while the game thread was pumped."));
		return false;
	}
	const bool bFlushSucceeded = WorkerResult.Get();
	const bool bExecMovedRight = ExecNode->NodePosX > EntryNode->NodePosX + 100;
	FBlueprintHelperGraphLayoutCoordinator::Shutdown();

	TestTrue(TEXT("Off-thread record followed by flush reports success"), bFlushSucceeded);
	TestTrue(TEXT("Off-thread record followed by flush applies generated-node layout before returning"), bExecMovedRight);
	return bFlushSucceeded && bExecMovedRight;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DataInputCanvasOffsetsAreNormalizedLeftAndBelow,
	"BlueprintHelper.GraphLayout.Solver.DataInputCanvasOffsetsAreNormalizedLeftAndBelow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataInputCanvasOffsetsAreNormalizedLeftAndBelow::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Set Value"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("Pure")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Pure"),
		TEXT("K2Node_CallFunction"),
		TEXT("Unsafe Canvas Pure"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Exec")})}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::ExecNode, FVector2D(300.0f, 100.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::PureFunction, FVector2D(560.0f, 40.0f));

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* PurePlacement = FindPlacement(Plan, TEXT("Pure"));

	TestNotNull(TEXT("Exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("Pure placement exists"), PurePlacement);
	if (!ExecPlacement || !PurePlacement)
	{
		return false;
	}

	TestTrue(TEXT("Unsafe right-side canvas input falls back to left side"), PurePlacement->TargetPosition.X < ExecPlacement->TargetPosition.X);
	TestTrue(TEXT("Unsafe upper canvas input falls back below consumer"), PurePlacement->TargetPosition.Y > ExecPlacement->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_GeneratedDataInputsAvoidExistingNodes,
	"BlueprintHelper.GraphLayout.Solver.GeneratedDataInputsAvoidExistingNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_GeneratedDataInputsAvoidExistingNodes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Set Actor Location"),
		FVector2D::ZeroVector,
		FVector2D(240.0f, 100.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("NewLocation"), EPinDirection::Input, false, {TEXT("MakeVector")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeVector"),
		TEXT("K2Node_CallFunction"),
		TEXT("Make Vector"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingBlocker"),
		TEXT("K2Node_CallFunction"),
		TEXT("User Existing Node"),
		FVector2D(140.0f, 120.0f),
		FVector2D(260.0f, 120.0f),
		true,
		{}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::PureFunction, FVector2D(120.0f, 220.0f));
	RuleSet.CollisionPaddingX = 40.0f;
	RuleSet.CollisionPaddingY = 30.0f;
	RuleSet.CollisionStepY = 60.0f;
	RuleSet.MaxCollisionAttempts = 16;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* DataPlacement = FindPlacement(Plan, TEXT("MakeVector"));
	const FNodePlacement* BlockerPlacement = FindPlacement(Plan, TEXT("ExistingBlocker"));

	TestNotNull(TEXT("Data placement exists"), DataPlacement);
	TestNotNull(TEXT("Existing blocker placement exists"), BlockerPlacement);
	if (!DataPlacement || !BlockerPlacement)
	{
		return false;
	}

	TestFalse(
		TEXT("generated data input does not overlap existing blocker"),
		RectsOverlap(*DataPlacement, FVector2D(220.0f, 90.0f), *BlockerPlacement, FVector2D(260.0f, 120.0f)));
	TestFalse(
		TEXT("generated data input expanded rect does not overlap existing blocker expanded rect"),
		ExpandedRectsOverlap(
			*DataPlacement,
			FVector2D(220.0f, 90.0f),
			*BlockerPlacement,
			FVector2D(260.0f, 120.0f),
			RuleSet.CollisionPaddingX,
			RuleSet.CollisionPaddingY));
	TestFalse(TEXT("existing blocker is not moved"), BlockerPlacement->bMoveExisting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_NonMovableExistingConsumersStayAtCurrentPosition,
	"BlueprintHelper.GraphLayout.Solver.NonMovableExistingConsumersStayAtCurrentPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_NonMovableExistingConsumersStayAtCurrentPosition::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingConsumer"),
		TEXT("K2Node_CallFunction"),
		TEXT("Existing Consumer"),
		FVector2D(1000.0f, 800.0f),
		FVector2D(240.0f, 100.0f),
		true,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("GeneratedPure")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("GeneratedPure"),
		TEXT("K2Node_CallFunction"),
		TEXT("Generated Pure"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("ExistingConsumer")})}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	RuleSet.bMoveExistingNodes = false;
	RuleSet.CollisionPaddingX = 20.0f;
	RuleSet.CollisionPaddingY = 20.0f;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ExistingPlacement = FindPlacement(Plan, TEXT("ExistingConsumer"));
	const FNodePlacement* PurePlacement = FindPlacement(Plan, TEXT("GeneratedPure"));

	TestNotNull(TEXT("existing consumer placement exists"), ExistingPlacement);
	TestNotNull(TEXT("generated pure placement exists"), PurePlacement);
	if (!ExistingPlacement || !PurePlacement)
	{
		return false;
	}

	TestFalse(TEXT("existing consumer is not movable"), ExistingPlacement->bMoveExisting);
	TestEqual(TEXT("existing consumer target x remains current"), ExistingPlacement->TargetPosition.X, 1000.0);
	TestEqual(TEXT("existing consumer target y remains current"), ExistingPlacement->TargetPosition.Y, 800.0);
	TestTrue(TEXT("generated input anchors to existing consumer real x"), PurePlacement->TargetPosition.X < ExistingPlacement->TargetPosition.X);
	TestTrue(TEXT("generated input anchors below existing consumer real y"), PurePlacement->TargetPosition.Y > ExistingPlacement->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_NonMovableExistingForEachAnchorsGeneratedPureCluster,
	"BlueprintHelper.GraphLayout.Solver.NonMovableExistingForEachAnchorsGeneratedPureCluster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_NonMovableExistingForEachAnchorsGeneratedPureCluster::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingForEach"),
		TEXT("K2Node_MacroInstance"),
		TEXT("For Each Loop"),
		FVector2D(1000.0f, 800.0f),
		FVector2D(260.0f, 150.0f),
		true,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true),
			MakePin(TEXT("LoopBody"), EPinDirection::Output, true),
			MakePin(TEXT("Completed"), EPinDirection::Output, true),
			MakePin(TEXT("Array"), EPinDirection::Input, false, {TEXT("MakeArray")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("[1]"), EPinDirection::Input, false, {TEXT("Proxy1")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("ExistingForEach")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy1"), TEXT("K2Node_VariableGet"), TEXT("Proxy 1"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	RuleSet.bMoveExistingNodes = false;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ExistingPlacement = FindPlacement(Plan, TEXT("ExistingForEach"));
	const FNodePlacement* MakeArrayPlacement = FindPlacement(Plan, TEXT("MakeArray"));
	const FNodePlacement* ProxyPlacement = FindPlacement(Plan, TEXT("Proxy0"));

	TestNotNull(TEXT("existing ForEach placement exists"), ExistingPlacement);
	TestNotNull(TEXT("make array placement exists"), MakeArrayPlacement);
	TestNotNull(TEXT("proxy placement exists"), ProxyPlacement);
	if (!ExistingPlacement || !MakeArrayPlacement || !ProxyPlacement)
	{
		return false;
	}

	TestFalse(TEXT("existing ForEach is not movable"), ExistingPlacement->bMoveExisting);
	TestEqual(TEXT("existing consumer target x remains current"), ExistingPlacement->TargetPosition.X, 1000.0);
	TestEqual(TEXT("existing consumer target y remains current"), ExistingPlacement->TargetPosition.Y, 800.0);
	TestTrue(TEXT("make array anchors to existing consumer"), MakeArrayPlacement->TargetPosition.X < ExistingPlacement->TargetPosition.X);
	TestTrue(TEXT("proxy leaf anchors to make array"), ProxyPlacement->TargetPosition.X < MakeArrayPlacement->TargetPosition.X);
	TestEqual(TEXT("make array placement is produced by input cluster policy"), MakeArrayPlacement->Reason, FString(TEXT("pure_data_subgraph_alignment")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_OccupancyResolverReturnsNonOverlappingEmergencyTarget,
	"BlueprintHelper.GraphLayout.Solver.OccupancyResolverReturnsNonOverlappingEmergencyTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OccupancyResolverReturnsNonOverlappingEmergencyTarget::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 10.0f;
	RuleSet.MaxCollisionAttempts = 1;

	FOccupancyResolver Occupancy(RuleSet);
	Occupancy.ReserveTarget(TEXT("BlockerA"), FVector2D(0.0f, 0.0f), FVector2D(100.0f, 100.0f), false);
	Occupancy.ReserveTarget(TEXT("BlockerB"), FVector2D(0.0f, 10.0f), FVector2D(100.0f, 100.0f), false);
	Occupancy.ReserveTarget(TEXT("BlockerC"), FVector2D(0.0f, 20.0f), FVector2D(100.0f, 100.0f), false);

	const FVector2D ResolvedTarget = Occupancy.ResolveNearestFreeTarget(
		TEXT("Candidate"),
		FVector2D::ZeroVector,
		FVector2D(100.0f, 100.0f));

	TestEqual(TEXT("emergency fallback keeps semantic x column"), ResolvedTarget.X, 0.0);
	TestTrue(TEXT("emergency fallback continues downward"), ResolvedTarget.Y >= 120.0);
	TestFalse(
		TEXT("emergency target does not overlap reserved rects"),
		Occupancy.WouldOverlap(TEXT("Candidate"), ResolvedTarget, FVector2D(100.0f, 100.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ExecSuccessorsFollowResolvedParentRow,
	"BlueprintHelper.GraphLayout.Solver.ExecSuccessorsFollowResolvedParentRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ExecSuccessorsFollowResolvedParentRow::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("RootColumnBlocker"),
		TEXT("K2Node_CallFunction"),
		TEXT("Root Column Blocker"),
		FVector2D(0.0f, 0.0f),
		FVector2D(200.0f, 500.0f),
		true,
		{}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 300.0f;
	RuleSet.ExecRowSpacing = 100.0f;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 100.0f;
	RuleSet.MaxCollisionAttempts = 8;
	RuleSet.bMoveExistingNodes = false;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("Event"));
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));

	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("exec placement exists"), ExecPlacement);
	if (!EventPlacement || !ExecPlacement)
	{
		return false;
	}

	TestTrue(TEXT("event was pushed below root-column blocker"), EventPlacement->TargetPosition.Y >= 500.0f);
	TestTrue(TEXT("exec successor stays at or below resolved parent row"), ExecPlacement->TargetPosition.Y >= EventPlacement->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RowReflowPropagatesPinnedBaselineBlocker,
	"BlueprintHelper.GraphLayout.Solver.RowReflowPropagatesPinnedBaselineBlocker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RowReflowPropagatesPinnedBaselineBlocker::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("RootA"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Root A"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("PinnedBlocker"),
		TEXT("EdGraphNode_Comment"),
		TEXT("Pinned Blocker"),
		FVector2D(0.0f, 450.0f),
		FVector2D(180.0f, 100.0f),
		true,
		{}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 300.0f;
	RuleSet.ExecRowSpacing = 100.0f;
	RuleSet.BranchRowSpacing = 300.0f;
	RuleSet.BranchRowPaddingY = 50.0f;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 100.0f;
	RuleSet.MaxCollisionAttempts = 8;
	RuleSet.bMoveExistingNodes = false;
	RuleSet.bUsePatternRowHeightBudget = true;
	RuleSet.bAlignExecNodesHorizontally = true;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("Event"));
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* PinnedBlockerPlacement = FindPlacement(Plan, TEXT("PinnedBlocker"));

	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("pinned blocker placement exists"), PinnedBlockerPlacement);
	if (!EventPlacement || !ExecPlacement || !PinnedBlockerPlacement)
	{
		return false;
	}

	TestEqual(TEXT("pinned blocker stays at current x"), PinnedBlockerPlacement->TargetPosition.X, 0.0);
	TestEqual(TEXT("pinned blocker stays at current y"), PinnedBlockerPlacement->TargetPosition.Y, 450.0);
	TestTrue(TEXT("parent row is bumped by pinned blocker during row reflow"), EventPlacement->TargetPosition.Y > PinnedBlockerPlacement->TargetPosition.Y);
	TestTrue(TEXT("child successor is not above the bumped parent row"), ExecPlacement->TargetPosition.Y >= EventPlacement->TargetPosition.Y);
	TestEqual(TEXT("single-output exec chain remains row-aligned after propagated bump"), ExecPlacement->TargetPosition.Y, EventPlacement->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_MultipleExecRootsUseDistinctRows,
	"BlueprintHelper.GraphLayout.Solver.MultipleExecRootsUseDistinctRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_MultipleExecRootsUseDistinctRows::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("CustomA"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Custom A"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ExecA")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExecA"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print A"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("CustomA")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("CustomB"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Custom B"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ExecB")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExecB"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print B"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("CustomB")})}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 420.0f;
	RuleSet.ExecRowSpacing = 180.0f;
	RuleSet.CollisionPaddingY = 40.0f;
	RuleSet.CollisionStepY = 80.0f;
	RuleSet.MaxCollisionAttempts = 16;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* CustomA = FindPlacement(Plan, TEXT("CustomA"));
	const FNodePlacement* CustomB = FindPlacement(Plan, TEXT("CustomB"));
	const FNodePlacement* ExecA = FindPlacement(Plan, TEXT("ExecA"));
	const FNodePlacement* ExecB = FindPlacement(Plan, TEXT("ExecB"));

	TestNotNull(TEXT("CustomA placement exists"), CustomA);
	TestNotNull(TEXT("CustomB placement exists"), CustomB);
	TestNotNull(TEXT("ExecA placement exists"), ExecA);
	TestNotNull(TEXT("ExecB placement exists"), ExecB);
	if (!CustomA || !CustomB || !ExecA || !ExecB)
	{
		return false;
	}

	TestNotEqual(TEXT("custom event roots do not share Y"), CustomA->TargetPosition.Y, CustomB->TargetPosition.Y);
	TestNotEqual(TEXT("exec successors do not share Y"), ExecA->TargetPosition.Y, ExecB->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_MultiExecOutputNodeUsesBranchRows,
	"BlueprintHelper.GraphLayout.Solver.MultiExecOutputNodeUsesBranchRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_MultiExecOutputNodeUsesBranchRows::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Split"),
		TEXT("K2Node_CallFunction"),
		TEXT("Generic Multi Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("ThenA"), EPinDirection::Output, true, {TEXT("A")}),
			MakePin(TEXT("ThenB"), EPinDirection::Output, true, {TEXT("B")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("A"),
		TEXT("K2Node_CallFunction"),
		TEXT("A"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("B"),
		TEXT("K2Node_CallFunction"),
		TEXT("B"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Split")})}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 300.0f;
	RuleSet.ExecRowSpacing = 80.0f;
	RuleSet.BranchRowSpacing = 320.0f;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* APlacement = FindPlacement(Plan, TEXT("A"));
	const FNodePlacement* BPlacement = FindPlacement(Plan, TEXT("B"));

	TestNotNull(TEXT("A placement exists"), APlacement);
	TestNotNull(TEXT("B placement exists"), BPlacement);
	if (!APlacement || !BPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("generic multi-exec outputs use branch row spacing"),
		FMath::Abs(BPlacement->TargetPosition.Y - APlacement->TargetPosition.Y) >= 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DisabledExecAlignmentUsesRoleBranchOnly,
	"BlueprintHelper.GraphLayout.Solver.DisabledExecAlignmentUsesRoleBranchOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DisabledExecAlignmentUsesRoleBranchOnly::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Split"),
		TEXT("K2Node_CallFunction"),
		TEXT("Generic Multi Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("In"), EPinDirection::Input, true),
			MakePin(TEXT("ThenA"), EPinDirection::Output, true, {TEXT("A")}),
			MakePin(TEXT("ThenB"), EPinDirection::Output, true, {TEXT("B")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("A"),
		TEXT("K2Node_CallFunction"),
		TEXT("A"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("B"),
		TEXT("K2Node_CallFunction"),
		TEXT("B"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));

	FRuleSet RuleSet;
	RuleSet.bAlignExecNodesHorizontally = false;
	RuleSet.ExecRowSpacing = 80.0f;
	RuleSet.BranchRowSpacing = 320.0f;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* APlacement = FindPlacement(Plan, TEXT("A"));
	const FNodePlacement* BPlacement = FindPlacement(Plan, TEXT("B"));

	TestNotNull(TEXT("A placement exists"), APlacement);
	TestNotNull(TEXT("B placement exists"), BPlacement);
	if (!APlacement || !BPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("disabled mode does not apply generic multi-output branch spacing"),
		FMath::Abs(BPlacement->TargetPosition.Y - APlacement->TargetPosition.Y) < 200.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PlacesMakeArrayBetweenLeavesAndForEach,
	"BlueprintHelper.GraphLayout.Solver.PlacesMakeArrayBetweenLeavesAndForEach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PlacesMakeArrayBetweenLeavesAndForEach::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ForEach")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ForEach"),
		TEXT("K2Node_MacroInstance"),
		TEXT("For Each Loop"),
		FVector2D::ZeroVector,
		FVector2D(260.0f, 150.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("LoopBody"), EPinDirection::Output, true),
			MakePin(TEXT("Completed"), EPinDirection::Output, true),
			MakePin(TEXT("Array"), EPinDirection::Input, false, {TEXT("MakeArray")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("[1]"), EPinDirection::Input, false, {TEXT("Proxy1")}),
			MakePin(TEXT("[2]"), EPinDirection::Input, false, {TEXT("Proxy2")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("ForEach")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy1"), TEXT("K2Node_VariableGet"), TEXT("Proxy 1"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy2"), TEXT("K2Node_VariableGet"), TEXT("Proxy 2"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::PureFunction, FVector2D(60.0f, 230.0f));

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ForEachPlacement = FindPlacement(Plan, TEXT("ForEach"));
	const FNodePlacement* MakeArrayPlacement = FindPlacement(Plan, TEXT("MakeArray"));
	const FNodePlacement* Proxy0Placement = FindPlacement(Plan, TEXT("Proxy0"));
	const FNodePlacement* Proxy2Placement = FindPlacement(Plan, TEXT("Proxy2"));

	TestNotNull(TEXT("ForEach placement exists"), ForEachPlacement);
	TestNotNull(TEXT("MakeArray placement exists"), MakeArrayPlacement);
	TestNotNull(TEXT("Proxy0 placement exists"), Proxy0Placement);
	TestNotNull(TEXT("Proxy2 placement exists"), Proxy2Placement);
	if (!ForEachPlacement || !MakeArrayPlacement || !Proxy0Placement || !Proxy2Placement)
	{
		return false;
	}

	TestTrue(TEXT("MakeArray is left of ForEach"), MakeArrayPlacement->TargetPosition.X < ForEachPlacement->TargetPosition.X);
	TestTrue(TEXT("Proxy0 is left of MakeArray"), Proxy0Placement->TargetPosition.X < MakeArrayPlacement->TargetPosition.X);
	TestTrue(TEXT("Proxy leaves preserve MakeArray input order"), Proxy0Placement->TargetPosition.Y < Proxy2Placement->TargetPosition.Y);
	TestEqual(TEXT("MakeArray placement is produced by the input cluster policy"), MakeArrayPlacement->Reason, FString(TEXT("pure_data_subgraph_alignment")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsCollisionSettings,
	"BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsCollisionSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsCollisionSettings::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 73.0f;
	RuleSet.CollisionPaddingY = 41.0f;
	RuleSet.CollisionStepY = 67.0f;
	RuleSet.MaxCollisionAttempts = 19;

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("json imports"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	TestEqual(TEXT("collision padding x"), Parsed.CollisionPaddingX, 73.0f);
	TestEqual(TEXT("collision padding y"), Parsed.CollisionPaddingY, 41.0f);
	TestEqual(TEXT("collision step y"), Parsed.CollisionStepY, 67.0f);
	TestEqual(TEXT("max collision attempts"), Parsed.MaxCollisionAttempts, 19);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsPatternLayoutSettings,
	"BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsPatternLayoutSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsPatternLayoutSettings::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet Defaults;
	TestTrue(TEXT("exec horizontal alignment defaults on"), Defaults.bAlignExecNodesHorizontally);
	TestTrue(TEXT("pure data subgraph layout defaults on"), Defaults.bUsePureDataSubgraphLayout);
	TestTrue(TEXT("pattern row budget defaults on"), Defaults.bUsePatternRowHeightBudget);

	FRuleSet RuleSet;
	RuleSet.bAlignExecNodesHorizontally = false;
	RuleSet.bUsePureDataSubgraphLayout = false;
	RuleSet.bUsePatternRowHeightBudget = false;
	RuleSet.DataClusterPaddingX = 37.0f;
	RuleSet.DataClusterPaddingY = 43.0f;
	RuleSet.BranchRowPaddingY = 71.0f;

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("json imports"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	TestFalse(TEXT("exec horizontal alignment roundtrips"), Parsed.bAlignExecNodesHorizontally);
	TestFalse(TEXT("pure data subgraph roundtrips"), Parsed.bUsePureDataSubgraphLayout);
	TestFalse(TEXT("pattern row budget roundtrips"), Parsed.bUsePatternRowHeightBudget);
	TestEqual(TEXT("data cluster padding x"), Parsed.DataClusterPaddingX, 37.0f);
	TestEqual(TEXT("data cluster padding y"), Parsed.DataClusterPaddingY, 43.0f);
	TestEqual(TEXT("branch row padding y"), Parsed.BranchRowPaddingY, 71.0f);

	const TSharedRef<FJsonObject> NestedJson = MakeShared<FJsonObject>();
	NestedJson->SetStringField(TEXT("schema"), RuleSetSchemaV1);
	const TSharedRef<FJsonObject> SolverJson = MakeShared<FJsonObject>();
	SolverJson->SetBoolField(TEXT("exec_node_horizontal_alignment_enabled"), false);
	SolverJson->SetBoolField(TEXT("pure_data_subgraph_layout_enabled"), false);
	SolverJson->SetBoolField(TEXT("pattern_row_height_budget_enabled"), false);
	SolverJson->SetNumberField(TEXT("data_cluster_padding_x"), 57.0f);
	SolverJson->SetNumberField(TEXT("data_cluster_padding_y"), 63.0f);
	SolverJson->SetNumberField(TEXT("branch_row_padding_y"), 91.0f);
	NestedJson->SetObjectField(TEXT("solver"), SolverJson);

	FRuleSet ParsedNested;
	FValidationResult NestedValidation;
	TestTrue(TEXT("nested solver json imports"), FRuleSetJson::Import(NestedJson, ParsedNested, NestedValidation));
	TestFalse(TEXT("nested exec horizontal alignment imports"), ParsedNested.bAlignExecNodesHorizontally);
	TestFalse(TEXT("nested pure data subgraph imports"), ParsedNested.bUsePureDataSubgraphLayout);
	TestFalse(TEXT("nested pattern row budget imports"), ParsedNested.bUsePatternRowHeightBudget);
	TestEqual(TEXT("nested data cluster padding x"), ParsedNested.DataClusterPaddingX, 57.0f);
	TestEqual(TEXT("nested data cluster padding y"), ParsedNested.DataClusterPaddingY, 63.0f);
	TestEqual(TEXT("nested branch row padding y"), ParsedNested.BranchRowPaddingY, 91.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_TopologyPreservesExecOutputPins,
	"BlueprintHelper.GraphLayout.Topology.PreservesExecOutputPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_TopologyPreservesExecOutputPins::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Split"),
		TEXT("K2Node_CallFunction"),
		TEXT("Generic Multi Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("In"), EPinDirection::Input, true),
			MakePin(TEXT("ThenA"), EPinDirection::Output, true, {TEXT("A")}),
			MakePin(TEXT("ThenB"), EPinDirection::Output, true, {TEXT("B")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("A"), TEXT("K2Node_CallFunction"), TEXT("A"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("B"), TEXT("K2Node_CallFunction"), TEXT("B"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const TArray<FExecEdge> Edges = Topology.GetExecOutputEdges(TEXT("Split"));

	TestEqual(TEXT("two exec output edges"), Edges.Num(), 2);
	TestEqual(TEXT("first edge pin"), Edges[0].SourceOutputPinName, FString(TEXT("ThenA")));
	TestEqual(TEXT("first target"), Edges[0].TargetNodeId, FString(TEXT("A")));
	TestEqual(TEXT("second edge pin"), Edges[1].SourceOutputPinName, FString(TEXT("ThenB")));
	TestEqual(TEXT("second target"), Edges[1].TargetNodeId, FString(TEXT("B")));
	TestTrue(TEXT("split is branch by output count"), Topology.IsMultiExecOutputNode(TEXT("Split")));

	Snapshot.Nodes.Add(MakeNode(
		TEXT("SparseSplit"),
		TEXT("K2Node_CallFunction"),
		TEXT("Sparse Multi Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("In"), EPinDirection::Input, true),
			MakePin(TEXT("ThenA"), EPinDirection::Output, true, {TEXT("SparseA")}),
			MakePin(TEXT("ThenB"), EPinDirection::Output, true)
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("SparseA"), TEXT("K2Node_CallFunction"), TEXT("Sparse A"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("SparseSplit")})}));

	const FGraphTopology SparseTopology = FGraphLayoutTopology::Build(Snapshot);
	const TArray<FExecEdge> SparseEdges = SparseTopology.GetExecOutputEdges(TEXT("SparseSplit"));
	TestEqual(TEXT("sparse split has one linked edge"), SparseEdges.Num(), 1);
	TestTrue(TEXT("unlinked exec outputs still make branch topology"), SparseTopology.IsMultiExecOutputNode(TEXT("SparseSplit")));

	Snapshot.Nodes.Add(MakeNode(
		TEXT("DataConsumer"),
		TEXT("K2Node_CallFunction"),
		TEXT("Data Consumer"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ZPin"), EPinDirection::Input, false, {TEXT("SourceB"), TEXT("MissingSource"), TEXT("SourceA")}),
			MakePin(TEXT("APin"), EPinDirection::Input, false, {TEXT("SourceC")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("SourceA"), TEXT("K2Node_VariableGet"), TEXT("Source A"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("DataConsumer")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("SourceB"), TEXT("K2Node_VariableGet"), TEXT("Source B"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("DataConsumer")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("SourceC"), TEXT("K2Node_VariableGet"), TEXT("Source C"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("DataConsumer")})}));

	const FGraphTopology DataTopology = FGraphLayoutTopology::Build(Snapshot);
	const TArray<FDataEdge> DataEdges = DataTopology.GetDataInputs(TEXT("DataConsumer"));
	TestEqual(TEXT("invalid data source is filtered"), DataEdges.Num(), 3);
	TestEqual(TEXT("same pin follows linked node order"), DataEdges[0].SourceNodeId, FString(TEXT("SourceB")));
	TestEqual(TEXT("same pin keeps later valid linked node"), DataEdges[1].SourceNodeId, FString(TEXT("SourceA")));
	TestEqual(TEXT("later pin ordinal follows"), DataEdges[2].SourceNodeId, FString(TEXT("SourceC")));
	TestNotNull(TEXT("find node returns owned snapshot"), DataTopology.FindNode(TEXT("DataConsumer")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PureDataSubgraphMeasuresMakeArrayEnvelope,
	"BlueprintHelper.GraphLayout.PureDataSubgraph.MeasuresMakeArrayEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PureDataSubgraphMeasuresMakeArrayEnvelope::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ForEach"),
		TEXT("K2Node_MacroInstance"),
		TEXT("For Each Loop"),
		FVector2D::ZeroVector,
		FVector2D(260.0f, 150.0f),
		false,
		{MakePin(TEXT("Array"), EPinDirection::Input, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("[1]"), EPinDirection::Input, false, {TEXT("Proxy1")}),
			MakePin(TEXT("[2]"), EPinDirection::Input, false, {TEXT("Proxy2")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("ForEach")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy1"), TEXT("K2Node_VariableGet"), TEXT("Proxy 1"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy2"), TEXT("K2Node_VariableGet"), TEXT("Proxy 2"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FPureDataSubgraphEnvelope Envelope =
		FPureDataSubgraphPolicy::MeasureForSink(Snapshot, Topology, TEXT("ForEach"), TEXT("Array"), FRuleSet());

	TestEqual(TEXT("root transform"), Envelope.RootNodeId, FString(TEXT("MakeArray")));
	TestTrue(TEXT("contains make array"), Envelope.NodeIds.Contains(TEXT("MakeArray")));
	TestTrue(TEXT("contains first leaf"), Envelope.NodeIds.Contains(TEXT("Proxy0")));
	TestTrue(TEXT("contains second leaf"), Envelope.NodeIds.Contains(TEXT("Proxy1")));
	TestTrue(TEXT("contains third leaf"), Envelope.NodeIds.Contains(TEXT("Proxy2")));
	TestTrue(TEXT("envelope is taller than transform node"), Envelope.Size.Y > 190.0f);
	TestTrue(TEXT("leaf target is left of transform"), Envelope.RelativeTargets.FindRef(TEXT("Proxy0")).X < Envelope.RelativeTargets.FindRef(TEXT("MakeArray")).X);
	TestTrue(TEXT("leaf order follows input pins"), Envelope.RelativeTargets.FindRef(TEXT("Proxy0")).Y < Envelope.RelativeTargets.FindRef(TEXT("Proxy1")).Y);
	TestTrue(TEXT("leaf order follows input pins 2"), Envelope.RelativeTargets.FindRef(TEXT("Proxy1")).Y < Envelope.RelativeTargets.FindRef(TEXT("Proxy2")).Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PureDataSubgraphSkipsNonPureFirstSinkSource,
	"BlueprintHelper.GraphLayout.PureDataSubgraph.SkipsNonPureFirstSinkSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PureDataSubgraphSkipsNonPureFirstSinkSource::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Consumer"),
		TEXT("K2Node_CallFunction"),
		TEXT("Consumer"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("AExecSource"), TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("AExecSource"),
		TEXT("K2Node_CallFunction"),
		TEXT("Impure Source"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true),
			MakePin(TEXT("Then"), EPinDirection::Output, true),
			MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("Consumer")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("Consumer")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FPureDataSubgraphEnvelope Envelope =
		FPureDataSubgraphPolicy::MeasureForSink(Snapshot, Topology, TEXT("Consumer"), TEXT("Value"), FRuleSet());

	TestEqual(TEXT("first pure source becomes root"), Envelope.RootNodeId, FString(TEXT("MakeArray")));
	TestFalse(TEXT("impure first source is skipped"), Envelope.NodeIds.Contains(TEXT("AExecSource")));
	TestTrue(TEXT("pure transform is measured"), Envelope.NodeIds.Contains(TEXT("MakeArray")));
	TestTrue(TEXT("leaf is measured"), Envelope.NodeIds.Contains(TEXT("Proxy0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_NodeInputClusterBudgetIncludesPureDataEnvelope,
	"BlueprintHelper.GraphLayout.NodeInputCluster.BudgetIncludesPureDataEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_NodeInputClusterBudgetIncludesPureDataEnvelope::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_MacroInstance"),
		TEXT("For Each Loop"),
		FVector2D::ZeroVector,
		FVector2D(260.0f, 150.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true),
			MakePin(TEXT("LoopBody"), EPinDirection::Output, true),
			MakePin(TEXT("Completed"), EPinDirection::Output, true),
			MakePin(TEXT("Array"), EPinDirection::Input, false, {TEXT("MakeArray")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("[1]"), EPinDirection::Input, false, {TEXT("Proxy1")}),
			MakePin(TEXT("[2]"), EPinDirection::Input, false, {TEXT("Proxy2")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("Exec")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy1"), TEXT("K2Node_VariableGet"), TEXT("Proxy 1"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy2"), TEXT("K2Node_VariableGet"), TEXT("Proxy 2"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	FRuleSet RuleSet;
	RuleSet.InputPinRowSpacing = 44.0f;
	RuleSet.DataClusterPaddingY = 40.0f;

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FNodeInputClusterBudget Budget =
		FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, TEXT("Exec"), RuleSet);

	TestEqual(TEXT("consumer id"), Budget.ConsumerNodeId, FString(TEXT("Exec")));
	TestTrue(TEXT("budget includes make array"), Budget.NodeIds.Contains(TEXT("MakeArray")));
	TestTrue(TEXT("budget includes proxy"), Budget.NodeIds.Contains(TEXT("Proxy2")));
	TestTrue(TEXT("budget height includes data envelope"), Budget.Height > 230.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_NodeInputClusterMeasuresEachTransformLinkOnSamePin,
	"BlueprintHelper.GraphLayout.NodeInputCluster.MeasuresEachTransformLinkOnSamePin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_NodeInputClusterMeasuresEachTransformLinkOnSamePin::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Consumer"),
		FVector2D::ZeroVector,
		FVector2D(240.0f, 120.0f),
		false,
		{
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("MakeArrayA"), TEXT("MakeArrayB")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArrayA"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array A"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("ProxyA")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("Exec")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArrayB"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array B"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("ProxyB")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("Exec")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("ProxyA"), TEXT("K2Node_VariableGet"), TEXT("Proxy A"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArrayA")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("ProxyB"), TEXT("K2Node_VariableGet"), TEXT("Proxy B"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArrayB")})}));

	FRuleSet RuleSet;
	RuleSet.InputPinRowSpacing = 44.0f;
	RuleSet.DataClusterPaddingY = 40.0f;

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FNodeInputClusterBudget Budget =
		FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, TEXT("Exec"), RuleSet);

	TestTrue(TEXT("first transform is present"), Budget.NodeIds.Contains(TEXT("MakeArrayA")));
	TestTrue(TEXT("second transform is present"), Budget.NodeIds.Contains(TEXT("MakeArrayB")));
	TestTrue(TEXT("first leaf is present"), Budget.NodeIds.Contains(TEXT("ProxyA")));
	TestTrue(TEXT("second leaf is present"), Budget.NodeIds.Contains(TEXT("ProxyB")));
	TestTrue(
		TEXT("linked transform order is preserved"),
		Budget.RelativeTargets.FindRef(TEXT("MakeArrayA")).Y < Budget.RelativeTargets.FindRef(TEXT("MakeArrayB")).Y);
	TestTrue(
		TEXT("linked leaf order follows transform order"),
		Budget.RelativeTargets.FindRef(TEXT("ProxyA")).Y < Budget.RelativeTargets.FindRef(TEXT("ProxyB")).Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_NodeInputClusterDuplicateTransformDoesNotInflateBounds,
	"BlueprintHelper.GraphLayout.NodeInputCluster.DuplicateTransformDoesNotInflateBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_NodeInputClusterDuplicateTransformDoesNotInflateBounds::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Consumer"),
		FVector2D::ZeroVector,
		FVector2D(240.0f, 120.0f),
		false,
		{
			MakePin(TEXT("ValueA"), EPinDirection::Input, false, {TEXT("MakeArray")}),
			MakePin(TEXT("ValueB"), EPinDirection::Input, false, {TEXT("MakeArray")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("Exec")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	FRuleSet RuleSet;
	RuleSet.InputPinRowSpacing = 44.0f;
	RuleSet.DataClusterPaddingY = 40.0f;

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FNodeInputClusterBudget Budget =
		FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, TEXT("Exec"), RuleSet);

	TestEqual(TEXT("duplicate transform keeps first-owner y"), Budget.RelativeTargets.FindRef(TEXT("MakeArray")).Y, 44.0);
	TestTrue(TEXT("duplicate transform does not inflate budget height"), Budget.Height < 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RowAllocationUsesDataClusterHeight,
	"BlueprintHelper.GraphLayout.RowAllocation.UsesDataClusterHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RowAllocationUsesDataClusterHeight::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.ExecRowSpacing = 100.0f;
	RuleSet.BranchRowSpacing = 120.0f;
	RuleSet.BranchRowPaddingY = 30.0f;

	FExecRowBudget Parent;
	Parent.RowId = 0;
	Parent.MinHeight = 80.0f;

	FExecRowBudget ChildA;
	ChildA.RowId = 1;
	ChildA.MinHeight = 260.0f;

	FExecRowBudget ChildB;
	ChildB.RowId = 2;
	ChildB.MinHeight = 80.0f;

	const TArray<FExecRowAllocation> Allocations = FGraphLayoutRowAllocationPolicy::Allocate({Parent, ChildA, ChildB}, RuleSet);

	TestEqual(TEXT("allocation count"), Allocations.Num(), 3);
	if (Allocations.Num() != 3)
	{
		return false;
	}

	TestEqual(TEXT("first baseline is zero"), Allocations[0].BaselineY, 0.0f);
	TestTrue(TEXT("second baseline includes first row height and padding"), Allocations[1].BaselineY >= 110.0f);
	TestTrue(
		TEXT("third baseline includes tall cluster height and padding"),
		Allocations[2].BaselineY >= Allocations[1].BaselineY + 290.0f);
	return true;
}

#endif
