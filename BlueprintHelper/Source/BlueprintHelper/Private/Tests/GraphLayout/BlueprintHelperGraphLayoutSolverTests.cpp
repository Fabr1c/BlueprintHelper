#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
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

#endif
