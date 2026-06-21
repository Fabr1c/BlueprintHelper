#include "Misc/AutomationTest.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPriorityCollisionResolver.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BlueprintHelperGraphLayoutPriorityCollisionResolverTests
{
using namespace BlueprintHelper::GraphLayout;

static FGraphLayoutCollisionNode MakeCollisionNode(
	const FString& NodeId,
	const ENodeRole Role,
	const FVector2D& Position,
	const float Priority,
	const bool bMovable,
	const int32 StableOrder)
{
	FGraphLayoutCollisionNode Node;
	Node.NodeId = NodeId;
	Node.Role = Role;
	Node.Position = Position;
	Node.Size = FVector2D(100.0f, 100.0f);
	Node.Priority = Priority;
	Node.bMovable = bMovable;
	Node.StableOrder = StableOrder;
	return Node;
}

static FRuleSet MakeCollisionRuleSet()
{
	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 120.0f;
	RuleSet.MaxCollisionAttempts = 4;
	RuleSet.OverlapToleranceRatio = 0.0f;
	return RuleSet;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_CollisionAllowsOverlapWithinTolerance,
	"BlueprintHelper.GraphLayout.Collision.AllowsOverlapWithinTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_CollisionAllowsOverlapWithinTolerance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutPriorityCollisionResolverTests;

	FRuleSet RuleSet = MakeCollisionRuleSet();
	RuleSet.OverlapToleranceRatio = 0.5f;
	const TArray<FGraphLayoutCollisionNode> Nodes = {
		MakeCollisionNode(TEXT("A"), ENodeRole::ExecNode, FVector2D(0.0f, 0.0f), 10.0f, true, 0),
		MakeCollisionNode(TEXT("B"), ENodeRole::ExecNode, FVector2D(60.0f, 0.0f), 1.0f, true, 1)
	};

	const TMap<FString, FVector2D> Positions = FGraphLayoutPriorityCollisionResolver::Resolve(Nodes, RuleSet);
	TestEqual(TEXT("minor overlap accepted"), Positions.FindRef(TEXT("B")), FVector2D(60.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_CollisionRejectsOverlapBeyondTolerance,
	"BlueprintHelper.GraphLayout.Collision.RejectsOverlapBeyondTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_CollisionRejectsOverlapBeyondTolerance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutPriorityCollisionResolverTests;

	FRuleSet RuleSet = MakeCollisionRuleSet();
	RuleSet.OverlapToleranceRatio = 0.1f;
	const TArray<FGraphLayoutCollisionNode> Nodes = {
		MakeCollisionNode(TEXT("A"), ENodeRole::ExecNode, FVector2D(0.0f, 0.0f), 10.0f, true, 0),
		MakeCollisionNode(TEXT("B"), ENodeRole::ExecNode, FVector2D(20.0f, 0.0f), 1.0f, true, 1)
	};

	const TMap<FString, FVector2D> Positions = FGraphLayoutPriorityCollisionResolver::Resolve(Nodes, RuleSet);
	TestTrue(TEXT("large overlap rejected"), !Positions.FindRef(TEXT("B")).Equals(FVector2D(20.0f, 0.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_CollisionLowPriorityExecMovesRightThenDown,
	"BlueprintHelper.GraphLayout.Collision.LowPriorityExecMovesRightThenDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_CollisionLowPriorityExecMovesRightThenDown::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutPriorityCollisionResolverTests;

	FRuleSet RuleSet = MakeCollisionRuleSet();
	const TArray<FGraphLayoutCollisionNode> Nodes = {
		MakeCollisionNode(TEXT("High"), ENodeRole::ExecNode, FVector2D(0.0f, 0.0f), 10.0f, true, 0),
		MakeCollisionNode(TEXT("Low"), ENodeRole::ExecNode, FVector2D(0.0f, 0.0f), 1.0f, true, 1)
	};

	const TMap<FString, FVector2D> Positions = FGraphLayoutPriorityCollisionResolver::Resolve(Nodes, RuleSet);
	TestTrue(TEXT("low priority exec moved right"), Positions.FindRef(TEXT("Low")).X > 0.0f);
	TestEqual(TEXT("exec tries same-row right first"), static_cast<double>(Positions.FindRef(TEXT("Low")).Y), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_CollisionLowPriorityDataMovesLeftThenDown,
	"BlueprintHelper.GraphLayout.Collision.LowPriorityDataMovesLeftThenDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_CollisionLowPriorityDataMovesLeftThenDown::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutPriorityCollisionResolverTests;

	FRuleSet RuleSet = MakeCollisionRuleSet();
	const TArray<FGraphLayoutCollisionNode> Nodes = {
		MakeCollisionNode(TEXT("High"), ENodeRole::PureFunction, FVector2D(0.0f, 0.0f), 10.0f, true, 0),
		MakeCollisionNode(TEXT("Low"), ENodeRole::VariableInput, FVector2D(0.0f, 0.0f), 1.0f, true, 1)
	};

	const TMap<FString, FVector2D> Positions = FGraphLayoutPriorityCollisionResolver::Resolve(Nodes, RuleSet);
	TestTrue(TEXT("low priority data moved left"), Positions.FindRef(TEXT("Low")).X < 0.0f);
	TestEqual(TEXT("data tries same-row left first"), static_cast<double>(Positions.FindRef(TEXT("Low")).Y), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_CollisionAnchorNeverLosesCollision,
	"BlueprintHelper.GraphLayout.Collision.AnchorNeverLosesCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_CollisionAnchorNeverLosesCollision::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutPriorityCollisionResolverTests;

	FRuleSet RuleSet = MakeCollisionRuleSet();
	const TArray<FGraphLayoutCollisionNode> Nodes = {
		MakeCollisionNode(TEXT("Anchor"), ENodeRole::ExecNode, FVector2D(0.0f, 0.0f), -100.0f, false, 0),
		MakeCollisionNode(TEXT("Movable"), ENodeRole::ExecNode, FVector2D(0.0f, 0.0f), 100.0f, true, 1)
	};

	const TMap<FString, FVector2D> Positions = FGraphLayoutPriorityCollisionResolver::Resolve(Nodes, RuleSet);
	TestEqual(TEXT("anchor stays fixed"), Positions.FindRef(TEXT("Anchor")), FVector2D(0.0f, 0.0f));
	TestTrue(TEXT("movable avoids anchor"), !Positions.FindRef(TEXT("Movable")).Equals(FVector2D(0.0f, 0.0f)));
	return true;
}

#endif
