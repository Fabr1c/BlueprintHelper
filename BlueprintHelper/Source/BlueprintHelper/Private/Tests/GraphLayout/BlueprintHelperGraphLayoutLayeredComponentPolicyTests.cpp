#include "Misc/AutomationTest.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutArrangeScopePolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutLayeredComponentPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BlueprintHelperGraphLayoutLayeredComponentPolicyTests
{
using namespace BlueprintHelper::GraphLayout;

static FPinSnapshot MakeOutputPin(const FString& PinId, const TArray<FString>& LinkedNodeIds)
{
	FPinSnapshot Pin;
	Pin.PinId = PinId;
	Pin.Name = TEXT("Out");
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

static FNodeSnapshot MakeNode(
	const FString& NodeId,
	const bool bExisting,
	const FVector2D& Position = FVector2D::ZeroVector,
	const FVector2D& Size = FVector2D(160.0f, 80.0f))
{
	FNodeSnapshot Node;
	Node.NodeId = NodeId;
	Node.StableName = NodeId;
	Node.Title = NodeId;
	Node.ClassPath = TEXT("/Script/BlueprintGraph.K2Node_CallFunction");
	Node.bExisting = bExisting;
	Node.Position = Position;
	Node.Size = Size;
	return Node;
}

static const FGraphLayoutLayeredPlacement* FindPlacement(
	const FGraphLayoutLayeredResult& Result,
	const FString& NodeId)
{
	for (const FGraphLayoutLayeredPlacement& Placement : Result.Placements)
	{
		if (Placement.NodeId == NodeId)
		{
			return &Placement;
		}
	}
	return nullptr;
}

static FGraphLayoutLayeredResult SolveLayered(FGraphSnapshot Snapshot, const FRuleSet& RuleSet)
{
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FGraphLayoutArrangeScope Scope =
		FGraphLayoutArrangeScopePolicy::Build(Snapshot, Topology, RuleSet);
	return FGraphLayoutLayeredComponentPolicy::Layout(Snapshot, Topology, Scope, RuleSet);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_LayeredComponentProducerLeftOfConsumer,
	"BlueprintHelper.GraphLayout.LayeredComponent.ProducerLeftOfConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_LayeredComponentProducerLeftOfConsumer::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutLayeredComponentPolicyTests;

	FNodeSnapshot Producer = MakeNode(TEXT("Producer"), false, FVector2D(500.0f, 200.0f));
	Producer.Pins.Add(MakeOutputPin(TEXT("Producer.Then"), { TEXT("Consumer") }));
	FNodeSnapshot Consumer = MakeNode(TEXT("Consumer"), false, FVector2D(500.0f, 200.0f));

	FGraphSnapshot Snapshot;
	Snapshot.Nodes = { Producer, Consumer };
	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 240.0f;

	const FGraphLayoutLayeredResult Result = SolveLayered(Snapshot, RuleSet);
	const FGraphLayoutLayeredPlacement* ProducerPlacement = FindPlacement(Result, TEXT("Producer"));
	const FGraphLayoutLayeredPlacement* ConsumerPlacement = FindPlacement(Result, TEXT("Consumer"));
	TestNotNull(TEXT("producer placement"), ProducerPlacement);
	TestNotNull(TEXT("consumer placement"), ConsumerPlacement);
	if (!ProducerPlacement || !ConsumerPlacement)
	{
		return false;
	}

	TestTrue(TEXT("consumer right of producer"), ConsumerPlacement->TargetPosition.X > ProducerPlacement->TargetPosition.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_LayeredComponentSourceDataPulledAdjacentToConsumer,
	"BlueprintHelper.GraphLayout.LayeredComponent.SourceDataPulledAdjacentToConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_LayeredComponentSourceDataPulledAdjacentToConsumer::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutLayeredComponentPolicyTests;

	FNodeSnapshot DataLeaf = MakeNode(TEXT("DataLeaf"), false, FVector2D(800.0f, 0.0f));
	FNodeSnapshot Consumer = MakeNode(TEXT("Consumer"), false, FVector2D(800.0f, 0.0f));
	Consumer.Pins.Add(MakeDataInputPin(TEXT("Consumer.Value"), { TEXT("DataLeaf") }));

	FGraphSnapshot Snapshot;
	Snapshot.Nodes = { DataLeaf, Consumer };
	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 220.0f;

	const FGraphLayoutLayeredResult Result = SolveLayered(Snapshot, RuleSet);
	const FGraphLayoutLayeredPlacement* DataPlacement = FindPlacement(Result, TEXT("DataLeaf"));
	const FGraphLayoutLayeredPlacement* ConsumerPlacement = FindPlacement(Result, TEXT("Consumer"));
	TestNotNull(TEXT("data placement"), DataPlacement);
	TestNotNull(TEXT("consumer placement"), ConsumerPlacement);
	if (!DataPlacement || !ConsumerPlacement)
	{
		return false;
	}

	TestTrue(TEXT("data leaf left of consumer"), DataPlacement->TargetPosition.X < ConsumerPlacement->TargetPosition.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_LayeredComponentPartialComponentUsesAnchorPosition,
	"BlueprintHelper.GraphLayout.LayeredComponent.PartialComponentUsesAnchorPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_LayeredComponentPartialComponentUsesAnchorPosition::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutLayeredComponentPolicyTests;

	FNodeSnapshot Anchor = MakeNode(TEXT("Anchor"), true, FVector2D(1000.0f, 1200.0f));
	Anchor.Pins.Add(MakeOutputPin(TEXT("Anchor.Then"), { TEXT("Mover") }));
	FNodeSnapshot Mover = MakeNode(TEXT("Mover"), false, FVector2D(0.0f, 0.0f));

	FGraphSnapshot Snapshot;
	Snapshot.Nodes = { Anchor, Mover };
	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 220.0f;

	const FGraphLayoutLayeredResult Result = SolveLayered(Snapshot, RuleSet);
	const FGraphLayoutLayeredPlacement* MoverPlacement = FindPlacement(Result, TEXT("Mover"));
	TestNotNull(TEXT("mover placement"), MoverPlacement);
	if (!MoverPlacement)
	{
		return false;
	}

	TestTrue(TEXT("mover stays near anchor x"), MoverPlacement->TargetPosition.X > 1000.0f);
	TestTrue(TEXT("mover stays near anchor y"), MoverPlacement->TargetPosition.Y >= 1200.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_LayeredComponentFullComponentsPackAsRigidGroups,
	"BlueprintHelper.GraphLayout.LayeredComponent.FullComponentsPackAsRigidGroups",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_LayeredComponentFullComponentsPackAsRigidGroups::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutLayeredComponentPolicyTests;

	FNodeSnapshot A = MakeNode(TEXT("A"), false, FVector2D(0.0f, 0.0f));
	A.Pins.Add(MakeOutputPin(TEXT("A.Then"), { TEXT("B") }));
	FNodeSnapshot B = MakeNode(TEXT("B"), false, FVector2D(0.0f, 0.0f));
	FNodeSnapshot C = MakeNode(TEXT("C"), false, FVector2D(0.0f, 0.0f));
	C.Pins.Add(MakeOutputPin(TEXT("C.Then"), { TEXT("D") }));
	FNodeSnapshot D = MakeNode(TEXT("D"), false, FVector2D(0.0f, 0.0f));

	FGraphSnapshot Snapshot;
	Snapshot.Nodes = { A, B, C, D };
	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 80.0f;
	RuleSet.ExecColumnSpacing = 220.0f;

	const FGraphLayoutLayeredResult Result = SolveLayered(Snapshot, RuleSet);
	const FGraphLayoutLayeredPlacement* APlacement = FindPlacement(Result, TEXT("A"));
	const FGraphLayoutLayeredPlacement* CPlacement = FindPlacement(Result, TEXT("C"));
	TestNotNull(TEXT("a placement"), APlacement);
	TestNotNull(TEXT("c placement"), CPlacement);
	if (!APlacement || !CPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("full components have separated vertical groups"),
		FMath::Abs(APlacement->TargetPosition.Y - CPlacement->TargetPosition.Y) >= 80.0f);
	return true;
}

#endif
