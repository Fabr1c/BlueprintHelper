#include "Misc/AutomationTest.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutArrangeScopePolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BlueprintHelperGraphLayoutArrangeScopeTests
{
using namespace BlueprintHelper::GraphLayout;

static FPinSnapshot MakeExecOutputPin(const FString& PinId, const TArray<FString>& LinkedNodeIds)
{
	FPinSnapshot Pin;
	Pin.PinId = PinId;
	Pin.Name = TEXT("Then");
	Pin.Direction = EPinDirection::Output;
	Pin.bExec = true;
	Pin.LinkedNodeIds = LinkedNodeIds;
	return Pin;
}

static FPinSnapshot MakeDataInputPin(const FString& PinId, const TArray<FString>& LinkedNodeIds)
{
	FPinSnapshot Pin;
	Pin.PinId = PinId;
	Pin.Name = TEXT("Value");
	Pin.Direction = EPinDirection::Input;
	Pin.bExec = false;
	Pin.LinkedNodeIds = LinkedNodeIds;
	return Pin;
}

static FNodeSnapshot MakeNode(const FString& NodeId, const bool bExisting)
{
	FNodeSnapshot Node;
	Node.NodeId = NodeId;
	Node.StableName = NodeId;
	Node.Title = NodeId;
	Node.ClassPath = TEXT("/Script/BlueprintGraph.K2Node_CallFunction");
	Node.bExisting = bExisting;
	Node.Size = FVector2D(180.0f, 80.0f);
	return Node;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ArrangeScopeGeneratedNodesAreToArrange,
	"BlueprintHelper.GraphLayout.ArrangeScope.GeneratedNodesAreToArrange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ArrangeScopeGeneratedNodesAreToArrange::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutArrangeScopeTests;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("GeneratedExec"), false));

	FRuleSet RuleSet;
	RuleSet.bMoveGeneratedNodes = true;
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FGraphLayoutArrangeScope Scope = FGraphLayoutArrangeScopePolicy::Build(Snapshot, Topology, RuleSet);

	TestTrue(TEXT("generated node arranged"), Scope.ToArrangeNodeIds.Contains(TEXT("GeneratedExec")));
	TestEqual(
		TEXT("generated node mobility"),
		Scope.MobilityByNodeId.FindRef(TEXT("GeneratedExec")),
		EGraphLayoutNodeMobility::MovableGenerated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ArrangeScopeExistingLinkedNodesBecomeAnchors,
	"BlueprintHelper.GraphLayout.ArrangeScope.ExistingLinkedNodesBecomeAnchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ArrangeScopeExistingLinkedNodesBecomeAnchors::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutArrangeScopeTests;

	FNodeSnapshot Generated = MakeNode(TEXT("GeneratedProducer"), false);
	Generated.Pins.Add(MakeExecOutputPin(TEXT("GeneratedProducer.Then"), { TEXT("ExistingConsumer") }));
	FNodeSnapshot Existing = MakeNode(TEXT("ExistingConsumer"), true);

	FGraphSnapshot Snapshot;
	Snapshot.Nodes = { Generated, Existing };

	FRuleSet RuleSet;
	RuleSet.bMoveGeneratedNodes = true;
	RuleSet.bMoveExistingNodes = false;
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FGraphLayoutArrangeScope Scope = FGraphLayoutArrangeScopePolicy::Build(Snapshot, Topology, RuleSet);

	TestTrue(TEXT("linked existing node anchor"), Scope.AnchorNodeIds.Contains(TEXT("ExistingConsumer")));
	TestEqual(
		TEXT("anchor mobility"),
		Scope.MobilityByNodeId.FindRef(TEXT("ExistingConsumer")),
		EGraphLayoutNodeMobility::Anchor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ArrangeScopeExistingUnlinkedNodesBecomeObstacles,
	"BlueprintHelper.GraphLayout.ArrangeScope.ExistingUnlinkedNodesBecomeObstacles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ArrangeScopeExistingUnlinkedNodesBecomeObstacles::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutArrangeScopeTests;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("GeneratedExec"), false));
	Snapshot.Nodes.Add(MakeNode(TEXT("ExistingBlocker"), true));

	FRuleSet RuleSet;
	RuleSet.bMoveGeneratedNodes = true;
	RuleSet.bMoveExistingNodes = false;
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FGraphLayoutArrangeScope Scope = FGraphLayoutArrangeScopePolicy::Build(Snapshot, Topology, RuleSet);

	TestTrue(TEXT("unlinked existing node obstacle"), Scope.ObstacleNodeIds.Contains(TEXT("ExistingBlocker")));
	TestEqual(
		TEXT("obstacle mobility"),
		Scope.MobilityByNodeId.FindRef(TEXT("ExistingBlocker")),
		EGraphLayoutNodeMobility::Obstacle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ArrangeScopeMoveExistingNodesMakesExistingMovable,
	"BlueprintHelper.GraphLayout.ArrangeScope.MoveExistingNodesMakesExistingMovable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ArrangeScopeMoveExistingNodesMakesExistingMovable::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutArrangeScopeTests;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("ExistingNode"), true));

	FRuleSet RuleSet;
	RuleSet.bMoveExistingNodes = true;
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FGraphLayoutArrangeScope Scope = FGraphLayoutArrangeScopePolicy::Build(Snapshot, Topology, RuleSet);

	TestTrue(TEXT("existing node arranged"), Scope.ToArrangeNodeIds.Contains(TEXT("ExistingNode")));
	TestEqual(
		TEXT("existing node mobility"),
		Scope.MobilityByNodeId.FindRef(TEXT("ExistingNode")),
		EGraphLayoutNodeMobility::MovableExisting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ArrangeScopeDataLinkedExistingNodesBecomeAnchors,
	"BlueprintHelper.GraphLayout.ArrangeScope.DataLinkedExistingNodesBecomeAnchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ArrangeScopeDataLinkedExistingNodesBecomeAnchors::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutArrangeScopeTests;

	FNodeSnapshot Generated = MakeNode(TEXT("GeneratedData"), false);
	FNodeSnapshot Existing = MakeNode(TEXT("ExistingConsumer"), true);
	Existing.Pins.Add(MakeDataInputPin(TEXT("ExistingConsumer.Value"), { TEXT("GeneratedData") }));

	FGraphSnapshot Snapshot;
	Snapshot.Nodes = { Generated, Existing };

	FRuleSet RuleSet;
	RuleSet.bMoveGeneratedNodes = true;
	RuleSet.bMoveExistingNodes = false;
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FGraphLayoutArrangeScope Scope = FGraphLayoutArrangeScopePolicy::Build(Snapshot, Topology, RuleSet);

	TestTrue(TEXT("data-linked existing node anchor"), Scope.AnchorNodeIds.Contains(TEXT("ExistingConsumer")));
	return true;
}

#endif
