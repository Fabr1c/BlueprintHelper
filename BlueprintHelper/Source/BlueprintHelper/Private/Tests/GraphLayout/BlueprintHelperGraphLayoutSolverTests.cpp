#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Self.h"
#include "InputCoreTypes.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewInteractionModel.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"
#include "UI/Layout/SBlueprintHelperLayoutPreviewInteractionSurface.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "Input/Events.h"
#include "Widgets/SNullWidget.h"

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

static FVector2D FindPreviewNodeSize(const FGraphLayoutPreviewSample& Sample, const FString& NodeId)
{
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (NodeSpec.NodeId == NodeId)
		{
			return NodeSpec.Size;
		}
	}
	return FVector2D(240.0f, 120.0f);
}

static const FGraphLayoutPreviewNodeSpec* FindPreviewNodeSpec(const FGraphLayoutPreviewSample& Sample, const FString& NodeId)
{
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (NodeSpec.NodeId == NodeId)
		{
			return &NodeSpec;
		}
	}
	return nullptr;
}

static bool MaterializedNodeMatchesPlacement(
	const FGraphLayoutPreviewMaterializerResult& Result,
	const FString& NodeId,
	const FVector2D& TargetPosition)
{
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	const FGuid* ExpectedGuid = Result.NodeGuidsById.Find(NodeId);
	if (!ExpectedGuid)
	{
		return false;
	}

	const int32 ExpectedX = FMath::RoundToInt(TargetPosition.X);
	const int32 ExpectedY = FMath::RoundToInt(TargetPosition.Y);
	for (const UEdGraphNode* Node : Result.PreviewGraph->Nodes)
	{
		if (Node &&
			Node->NodeGuid == *ExpectedGuid &&
			Node->NodePosX == ExpectedX &&
			Node->NodePosY == ExpectedY)
		{
			return true;
		}
	}
	return false;
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

static FRuleSet MakeRuleSetWithScalarInputOffsets()
{
	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 420.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.BranchRowSpacing = 160.0f;
	RuleSet.PureInputOffsetX = 220.0f;
	RuleSet.VariableInputOffsetX = 230.0f;
	RuleSet.InputPinRowSpacing = 90.0f;
	RuleSet.bMoveGeneratedNodes = true;
	RuleSet.bMoveExistingNodes = false;
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

static FGraphLayoutPreviewSample MakeMaterializerTestSample()
{
	FGraphLayoutPreviewSample Sample;
	Sample.Scene = ESemanticScene::MultiExecOutput;
	Sample.Snapshot.GraphName = TEXT("Preview_MaterializerTest");

	FNodeSnapshot EventNode;
	EventNode.NodeId = TEXT("EventStart");
	EventNode.StableName = EventNode.NodeId;
	EventNode.ClassPath = TEXT("K2Node_CustomEvent");
	EventNode.Title = TEXT("On Preview Trigger");
	EventNode.Position = FVector2D(0.0f, 0.0f);
	EventNode.Size = FVector2D(220.0f, 88.0f);
	EventNode.bExisting = false;
	EventNode.Pins = {
		MakePin(TEXT("then"), EPinDirection::Output, true, {TEXT("Sequence")})
	};
	Sample.Snapshot.Nodes.Add(EventNode);

	FNodeSnapshot SequenceNode;
	SequenceNode.NodeId = TEXT("Sequence");
	SequenceNode.StableName = SequenceNode.NodeId;
	SequenceNode.ClassPath = TEXT("K2Node_ExecutionSequence");
	SequenceNode.Title = TEXT("Sequence");
	SequenceNode.Position = FVector2D(40.0f, 0.0f);
	SequenceNode.Size = FVector2D(236.0f, 104.0f);
	SequenceNode.bExisting = false;
	SequenceNode.Pins = {
		MakePin(TEXT("execute"), EPinDirection::Input, true, {TEXT("EventStart")}),
		MakePin(TEXT("Then_0"), EPinDirection::Output, true, {TEXT("Branch")}),
		MakePin(TEXT("Then_1"), EPinDirection::Output, true, {TEXT("PrintNode")})
	};
	Sample.Snapshot.Nodes.Add(SequenceNode);

	FNodeSnapshot BranchNode;
	BranchNode.NodeId = TEXT("Branch");
	BranchNode.StableName = BranchNode.NodeId;
	BranchNode.ClassPath = TEXT("K2Node_IfThenElse");
	BranchNode.Title = TEXT("Branch");
	BranchNode.Position = FVector2D(80.0f, 0.0f);
	BranchNode.Size = FVector2D(228.0f, 104.0f);
	BranchNode.bExisting = false;
	BranchNode.Pins = {
		MakePin(TEXT("execute"), EPinDirection::Input, true, {TEXT("Sequence")}),
		MakePin(TEXT("Condition"), EPinDirection::Input, false, {TEXT("GenericData")}),
		MakePin(TEXT("Then"), EPinDirection::Output, true),
		MakePin(TEXT("Else"), EPinDirection::Output, true)
	};
	Sample.Snapshot.Nodes.Add(BranchNode);

	FNodeSnapshot BuildArrayNode;
	BuildArrayNode.NodeId = TEXT("BuildArray");
	BuildArrayNode.StableName = BuildArrayNode.NodeId;
	BuildArrayNode.ClassPath = TEXT("K2Node_MakeArray");
	BuildArrayNode.Title = TEXT("Make Array");
	BuildArrayNode.Position = FVector2D(120.0f, 0.0f);
	BuildArrayNode.Size = FVector2D(228.0f, 104.0f);
	BuildArrayNode.bExisting = false;
	BuildArrayNode.Pins = {
		MakePin(TEXT("In0"), EPinDirection::Input, false, {TEXT("SelfRef")}),
		MakePin(TEXT("In1"), EPinDirection::Input, false, {TEXT("SelfRef")}),
		MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("PrintNode")})
	};
	Sample.Snapshot.Nodes.Add(BuildArrayNode);

	FNodeSnapshot SelfNode;
	SelfNode.NodeId = TEXT("SelfRef");
	SelfNode.StableName = SelfNode.NodeId;
	SelfNode.ClassPath = TEXT("K2Node_Self");
	SelfNode.Title = TEXT("Self");
	SelfNode.Position = FVector2D(160.0f, 0.0f);
	SelfNode.Size = FVector2D(168.0f, 72.0f);
	SelfNode.bExisting = false;
	SelfNode.Pins = {
		MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("BuildArray")})
	};
	Sample.Snapshot.Nodes.Add(SelfNode);

	FNodeSnapshot CommentNode;
	CommentNode.NodeId = TEXT("CommentBlocker");
	CommentNode.StableName = CommentNode.NodeId;
	CommentNode.ClassPath = TEXT("EdGraphNode_Comment");
	CommentNode.Title = TEXT("Existing Comment");
	CommentNode.Position = FVector2D(200.0f, 0.0f);
	CommentNode.Size = FVector2D(340.0f, 120.0f);
	CommentNode.bExisting = true;
	Sample.Snapshot.Nodes.Add(CommentNode);

	FNodeSnapshot PrintNode;
	PrintNode.NodeId = TEXT("PrintNode");
	PrintNode.StableName = PrintNode.NodeId;
	PrintNode.ClassPath = TEXT("K2Node_CallFunction");
	PrintNode.Title = TEXT("Print String");
	PrintNode.Position = FVector2D(240.0f, 0.0f);
	PrintNode.Size = FVector2D(232.0f, 96.0f);
	PrintNode.bExisting = false;
	PrintNode.Pins = {
		MakePin(TEXT("execute"), EPinDirection::Input, true, {TEXT("Sequence")}),
		MakePin(TEXT("In"), EPinDirection::Input, false, {TEXT("BuildArray")}),
		MakePin(TEXT("then"), EPinDirection::Output, true)
	};
	Sample.Snapshot.Nodes.Add(PrintNode);

	FNodeSnapshot GenericNode;
	GenericNode.NodeId = TEXT("GenericData");
	GenericNode.StableName = GenericNode.NodeId;
	GenericNode.ClassPath = TEXT("K2Node_VariableGet");
	GenericNode.Title = TEXT("Generic Data");
	GenericNode.Position = FVector2D(280.0f, 0.0f);
	GenericNode.Size = FVector2D(180.0f, 72.0f);
	GenericNode.bExisting = false;
	GenericNode.Pins = {
		MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("Branch")})
	};
	Sample.Snapshot.Nodes.Add(GenericNode);

	auto AddSpec = [&Sample](const FString& NodeId, const FString& Title, const EGraphLayoutPreviewNodeFactory Factory, const ENodeRole Role, const FVector2D& Size)
	{
		FGraphLayoutPreviewNodeSpec Spec;
		Spec.NodeId = NodeId;
		Spec.Title = Title;
		Spec.Factory = Factory;
		Spec.Role = Role;
		Spec.Size = Size;
		Sample.Nodes.Add(Spec);
	};

	AddSpec(TEXT("EventStart"), TEXT("On Preview Trigger"), EGraphLayoutPreviewNodeFactory::CustomEvent, ENodeRole::EventEntry, FVector2D(220.0f, 88.0f));
	AddSpec(TEXT("Sequence"), TEXT("Sequence"), EGraphLayoutPreviewNodeFactory::ExecutionSequence, ENodeRole::BranchControl, FVector2D(236.0f, 104.0f));
	AddSpec(TEXT("Branch"), TEXT("Branch"), EGraphLayoutPreviewNodeFactory::IfThenElse, ENodeRole::BranchControl, FVector2D(228.0f, 104.0f));
	AddSpec(TEXT("BuildArray"), TEXT("Make Array"), EGraphLayoutPreviewNodeFactory::MakeArray, ENodeRole::PureFunction, FVector2D(228.0f, 104.0f));
	AddSpec(TEXT("SelfRef"), TEXT("Self"), EGraphLayoutPreviewNodeFactory::Self, ENodeRole::VariableInput, FVector2D(168.0f, 72.0f));
	AddSpec(TEXT("CommentBlocker"), TEXT("Existing Comment"), EGraphLayoutPreviewNodeFactory::Comment, ENodeRole::Comment, FVector2D(340.0f, 120.0f));
	AddSpec(TEXT("PrintNode"), TEXT("Print String"), EGraphLayoutPreviewNodeFactory::CallFunction, ENodeRole::ExecNode, FVector2D(232.0f, 96.0f));
	AddSpec(TEXT("GenericData"), TEXT("Generic Data"), EGraphLayoutPreviewNodeFactory::GenericK2, ENodeRole::VariableInput, FVector2D(180.0f, 72.0f));

	auto AddLink = [&Sample](const FString& FromNodeId, const FString& FromPinName, const FString& ToNodeId, const FString& ToPinName, const bool bExec)
	{
		FGraphLayoutPreviewLinkSpec Link;
		Link.FromNodeId = FromNodeId;
		Link.FromPinName = FromPinName;
		Link.ToNodeId = ToNodeId;
		Link.ToPinName = ToPinName;
		Link.bExec = bExec;
		Sample.Links.Add(Link);
	};

	AddLink(TEXT("EventStart"), TEXT("then"), TEXT("Sequence"), TEXT("execute"), true);
	AddLink(TEXT("Sequence"), TEXT("Then_0"), TEXT("Branch"), TEXT("execute"), true);
	AddLink(TEXT("Sequence"), TEXT("Then_1"), TEXT("PrintNode"), TEXT("execute"), true);
	AddLink(TEXT("GenericData"), TEXT("Value"), TEXT("Branch"), TEXT("Condition"), false);
	AddLink(TEXT("SelfRef"), TEXT("Value"), TEXT("BuildArray"), TEXT("In0"), false);
	AddLink(TEXT("SelfRef"), TEXT("Value"), TEXT("BuildArray"), TEXT("In1"), false);
	AddLink(TEXT("BuildArray"), TEXT("Array"), TEXT("PrintNode"), TEXT("In"), false);

	return Sample;
}

static FLayoutPlan MakeMaterializerTestPlan()
{
	FLayoutPlan Plan;

	auto AddPlacement = [&Plan](const FString& NodeId, const ENodeRole Role, const FVector2D& CurrentPosition, const FVector2D& TargetPosition)
	{
		FNodePlacement Placement;
		Placement.NodeId = NodeId;
		Placement.Role = Role;
		Placement.CurrentPosition = CurrentPosition;
		Placement.TargetPosition = TargetPosition;
		Placement.bMoveExisting = true;
		Plan.Placements.Add(Placement);
	};

	AddPlacement(TEXT("EventStart"), ENodeRole::EventEntry, FVector2D(0.0f, 0.0f), FVector2D(100.0f, 100.0f));
	AddPlacement(TEXT("Sequence"), ENodeRole::BranchControl, FVector2D(40.0f, 0.0f), FVector2D(420.0f, 100.0f));
	AddPlacement(TEXT("Branch"), ENodeRole::BranchControl, FVector2D(80.0f, 0.0f), FVector2D(700.0f, 260.0f));
	AddPlacement(TEXT("BuildArray"), ENodeRole::PureFunction, FVector2D(120.0f, 0.0f), FVector2D(360.0f, 520.0f));
	AddPlacement(TEXT("SelfRef"), ENodeRole::VariableInput, FVector2D(160.0f, 0.0f), FVector2D(120.0f, 520.0f));
	AddPlacement(TEXT("CommentBlocker"), ENodeRole::Comment, FVector2D(200.0f, 0.0f), FVector2D(900.0f, 80.0f));
	AddPlacement(TEXT("PrintNode"), ENodeRole::ExecNode, FVector2D(240.0f, 0.0f), FVector2D(1020.0f, 100.0f));
	AddPlacement(TEXT("GenericData"), ENodeRole::VariableInput, FVector2D(280.0f, 0.0f), FVector2D(420.0f, 680.0f));
	return Plan;
}

static UEdGraphNode* FindNodeByExactClass(const UEdGraph* Graph, UClass* NodeClass)
{
	if (!Graph || !NodeClass)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->GetClass() == NodeClass)
		{
			return Node;
		}
	}
	return nullptr;
}

static UEdGraphPin* FindPinByNameAndDirection(UEdGraphNode* Node, const FName PinName, const EEdGraphPinDirection Direction)
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

static UEdGraphNode* FindMaterializedNodeById(
	const FGraphLayoutPreviewMaterializerResult& Result,
	const FString& NodeId)
{
	if (!Result.PreviewGraph.IsValid())
	{
		return nullptr;
	}

	const FGuid* ExpectedGuid = Result.NodeGuidsById.Find(NodeId);
	if (!ExpectedGuid)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Result.PreviewGraph->Nodes)
	{
		if (Node && Node->NodeGuid == *ExpectedGuid)
		{
			return Node;
		}
	}
	return nullptr;
}

static int32 CountExecPins(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return 0;
	}

	int32 ExecPinCount = 0;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			++ExecPinCount;
		}
	}
	return ExecPinCount;
}

static float ExpectedExecBaselineOffsetY(const ENodeRole Role)
{
	return Role == ENodeRole::EventEntry ? 62.5f : 48.0f;
}

static float ExpectedExecBaselineY(const FNodePlacement& Placement, const ENodeRole Role)
{
	return Placement.TargetPosition.Y + ExpectedExecBaselineOffsetY(Role);
}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DataInputsUseScalarVerticalOffsets,
	"BlueprintHelper.GraphLayout.Solver.DataInputsUseScalarVerticalOffsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataInputsUseScalarVerticalOffsets::RunTest(const FString& Parameters)
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

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, MakeRuleSetWithScalarInputOffsets());
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* PurePlacement = FindPlacement(Plan, TEXT("Pure"));

	TestNotNull(TEXT("Exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("Pure placement exists"), PurePlacement);
	if (!ExecPlacement || !PurePlacement)
	{
		return false;
	}

	TestTrue(TEXT("Pure input is below exec by scalar row spacing"), PurePlacement->TargetPosition.Y > ExecPlacement->TargetPosition.Y + 80.0f);
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
	FBlueprintHelperGraphLayout_DataInputScalarOffsetsPlaceInputsLeftAndBelow,
	"BlueprintHelper.GraphLayout.Solver.DataInputScalarOffsetsPlaceInputsLeftAndBelow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataInputScalarOffsetsPlaceInputsLeftAndBelow::RunTest(const FString& Parameters)
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
		TEXT("Scalar Pure"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Exec")})}));

	FRuleSet RuleSet = MakeRuleSetWithScalarInputOffsets();

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* PurePlacement = FindPlacement(Plan, TEXT("Pure"));

	TestNotNull(TEXT("Exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("Pure placement exists"), PurePlacement);
	if (!ExecPlacement || !PurePlacement)
	{
		return false;
	}

	TestTrue(TEXT("Scalar input is placed left of consumer"), PurePlacement->TargetPosition.X < ExecPlacement->TargetPosition.X);
	TestTrue(TEXT("Scalar input is placed below consumer"), PurePlacement->TargetPosition.Y > ExecPlacement->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverAlignsExecPinBaselines,
	"BlueprintHelper.GraphLayout.Solver.AlignsExecPinBaselines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverAlignsExecPinBaselines::RunTest(const FString& Parameters)
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
		FVector2D(220.0f, 88.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Branch")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Branch"),
		TEXT("K2Node_IfThenElse"),
		TEXT("Branch"),
		FVector2D::ZeroVector,
		FVector2D(228.0f, 104.0f),
		false,
		{
			MakePin(TEXT("execute"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Condition"), EPinDirection::Input, false, {TEXT("Condition")}),
			MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ThenPrint")}),
			MakePin(TEXT("Else"), EPinDirection::Output, true, {TEXT("ElsePrint")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ThenPrint"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print String"),
		FVector2D::ZeroVector,
		FVector2D(232.0f, 96.0f),
		false,
		{MakePin(TEXT("execute"), EPinDirection::Input, true, {TEXT("Branch")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ElsePrint"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print String"),
		FVector2D::ZeroVector,
		FVector2D(232.0f, 96.0f),
		false,
		{MakePin(TEXT("execute"), EPinDirection::Input, true, {TEXT("Branch")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Condition"),
		TEXT("K2Node_CallFunction"),
		TEXT("In Range"),
		FVector2D::ZeroVector,
		FVector2D(260.0f, 180.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Branch")})}));

	FRuleSet RuleSet = MakeRuleSetWithScalarInputOffsets();
	RuleSet.bUsePatternRowHeightBudget = true;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("Event"));
	const FNodePlacement* BranchPlacement = FindPlacement(Plan, TEXT("Branch"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("branch placement exists"), BranchPlacement);
	if (!EventPlacement || !BranchPlacement)
	{
		return false;
	}

	const float EventBaselineY =
		EventPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::EventEntry);
	const float BranchBaselineY =
		BranchPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::BranchControl);
	TestEqual(TEXT("event output and branch input exec pins share a baseline"), BranchBaselineY, EventBaselineY);
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

	FRuleSet RuleSet = MakeRuleSetWithScalarInputOffsets();
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

	FRuleSet RuleSet = MakeRuleSetWithScalarInputOffsets();
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

	FRuleSet RuleSet = MakeRuleSetWithScalarInputOffsets();
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
	FBlueprintHelperGraphLayout_OccupancyResolverPrefersSameRowHorizontalCandidate,
	"BlueprintHelper.GraphLayout.Solver.OccupancyResolverPrefersSameRowHorizontalCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OccupancyResolverPrefersSameRowHorizontalCandidate::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 240.0f;
	RuleSet.CollisionPaddingY = 8.0f;
	RuleSet.CollisionStepY = 240.0f;
	RuleSet.MaxCollisionAttempts = 4;

	FOccupancyResolver Occupancy(RuleSet);
	Occupancy.ReserveTarget(TEXT("Entry"), FVector2D::ZeroVector, FVector2D(180.0f, 80.0f), true);

	const FVector2D DesiredTarget(360.0f, 0.0f);
	const FVector2D ResolvedTarget = Occupancy.ResolveNearestFreeTargetPreferSameRow(
		TEXT("Branch"),
		DesiredTarget,
		FVector2D(228.0f, 104.0f));

	TestEqual(TEXT("same-row preference keeps target y"), ResolvedTarget.Y, DesiredTarget.Y);
	TestTrue(TEXT("same-row preference shifts right when padded rect overlaps"), ResolvedTarget.X > DesiredTarget.X);
	TestFalse(
		TEXT("same-row target does not overlap reserved entry"),
		Occupancy.WouldOverlap(TEXT("Branch"), ResolvedTarget, FVector2D(228.0f, 104.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_OccupancyResolverFallsBackDownWhenSameRowBlocked,
	"BlueprintHelper.GraphLayout.Solver.OccupancyResolverFallsBackDownWhenSameRowBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OccupancyResolverFallsBackDownWhenSameRowBlocked::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 100.0f;
	RuleSet.MaxCollisionAttempts = 2;

	FOccupancyResolver Occupancy(RuleSet);
	Occupancy.ReserveTarget(TEXT("BlockerA"), FVector2D(0.0f, 0.0f), FVector2D(100.0f, 100.0f), false);
	Occupancy.ReserveTarget(TEXT("BlockerB"), FVector2D(100.0f, 0.0f), FVector2D(100.0f, 100.0f), false);

	const FVector2D ResolvedTarget = Occupancy.ResolveNearestFreeTargetPreferSameRow(
		TEXT("Candidate"),
		FVector2D::ZeroVector,
		FVector2D(100.0f, 100.0f));

	TestEqual(TEXT("fallback keeps semantic x column"), ResolvedTarget.X, 0.0);
	TestTrue(TEXT("fallback moves down only after same-row candidates are blocked"), ResolvedTarget.Y > 0.0f);
	TestFalse(
		TEXT("fallback target does not overlap blockers"),
		Occupancy.WouldOverlap(TEXT("Candidate"), ResolvedTarget, FVector2D(100.0f, 100.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RootExecSuccessorPrefersSameRowHorizontalAvoidance,
	"BlueprintHelper.GraphLayout.Solver.RootExecSuccessorPrefersSameRowHorizontalAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RootExecSuccessorPrefersSameRowHorizontalAvoidance::RunTest(const FString& Parameters)
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
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Branch")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Branch"),
		TEXT("K2Node_IfThenElse"),
		TEXT("Branch"),
		FVector2D::ZeroVector,
		FVector2D(228.0f, 104.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Then"), EPinDirection::Output, true),
			MakePin(TEXT("Else"), EPinDirection::Output, true)
		}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 360.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.CollisionPaddingX = 240.0f;
	RuleSet.CollisionPaddingY = 8.0f;
	RuleSet.CollisionStepY = 240.0f;
	RuleSet.MaxCollisionAttempts = 8;
	RuleSet.bAlignExecNodesHorizontally = true;
	RuleSet.bUsePatternRowHeightBudget = true;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("Event"));
	const FNodePlacement* BranchPlacement = FindPlacement(Plan, TEXT("Branch"));

	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("branch placement exists"), BranchPlacement);
	if (!EventPlacement || !BranchPlacement)
	{
		return false;
	}

	TestEqual(
		TEXT("first exec successor keeps root exec pin baseline"),
		ExpectedExecBaselineY(*BranchPlacement, ENodeRole::BranchControl),
		ExpectedExecBaselineY(*EventPlacement, ENodeRole::EventEntry));
	TestTrue(TEXT("first exec successor shifts horizontally instead of vertically"), BranchPlacement->TargetPosition.X > RuleSet.ExecColumnSpacing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DisabledExecHorizontalAlignmentKeepsDownwardOverlapAvoidance,
	"BlueprintHelper.GraphLayout.Solver.DisabledExecHorizontalAlignmentKeepsDownwardOverlapAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DisabledExecHorizontalAlignmentKeepsDownwardOverlapAvoidance::RunTest(const FString& Parameters)
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
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Branch")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Branch"),
		TEXT("K2Node_IfThenElse"),
		TEXT("Branch"),
		FVector2D::ZeroVector,
		FVector2D(228.0f, 104.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Then"), EPinDirection::Output, true),
			MakePin(TEXT("Else"), EPinDirection::Output, true)
		}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 360.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.CollisionPaddingX = 240.0f;
	RuleSet.CollisionPaddingY = 8.0f;
	RuleSet.CollisionStepY = 240.0f;
	RuleSet.MaxCollisionAttempts = 8;
	RuleSet.bAlignExecNodesHorizontally = false;
	RuleSet.bUsePatternRowHeightBudget = true;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("Event"));
	const FNodePlacement* BranchPlacement = FindPlacement(Plan, TEXT("Branch"));

	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("branch placement exists"), BranchPlacement);
	if (!EventPlacement || !BranchPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("disabled exec horizontal alignment uses downward overlap avoidance"),
		BranchPlacement->TargetPosition.Y > EventPlacement->TargetPosition.Y);
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
	TestEqual(
		TEXT("single-output exec chain remains pin-baseline aligned after propagated bump"),
		ExpectedExecBaselineY(*ExecPlacement, ENodeRole::ExecNode),
		ExpectedExecBaselineY(*EventPlacement, ENodeRole::EventEntry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_IntraBlockDataInputsInheritExecPriority,
	"BlueprintHelper.GraphLayout.Solver.IntraBlockDataInputsInheritExecPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_IntraBlockDataInputsInheritExecPriority::RunTest(const FString& Parameters)
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
		FVector2D(20.0f, 20.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("HighExec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("LowExec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Low Exec"),
		FVector2D::ZeroVector,
		FVector2D(20.0f, 20.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("HighExec")}),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("LowData")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("LowData"),
		TEXT("K2Node_VariableGet"),
		TEXT("Low Data"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 72.0f),
		false,
		{MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("LowExec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("HighExec"),
		TEXT("K2Node_CallFunction"),
		TEXT("High Exec"),
		FVector2D::ZeroVector,
		FVector2D(20.0f, 20.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("LowExec")}),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("HighData")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("HighData"),
		TEXT("K2Node_VariableGet"),
		TEXT("High Data"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 72.0f),
		false,
		{MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("HighExec")})}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 100.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.VariableInputOffsetX = 300.0f;
	RuleSet.InputPinRowSpacing = 60.0f;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 180.0f;
	RuleSet.MaxCollisionAttempts = 8;
	RuleSet.bAlignExecNodesHorizontally = true;
	RuleSet.bUsePatternRowHeightBudget = true;
	RuleSet.bUsePureDataSubgraphLayout = true;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* HighDataPlacement = FindPlacement(Plan, TEXT("HighData"));
	const FNodePlacement* LowDataPlacement = FindPlacement(Plan, TEXT("LowData"));
	TestNotNull(TEXT("high-priority data placement exists"), HighDataPlacement);
	TestNotNull(TEXT("low-priority data placement exists"), LowDataPlacement);
	if (!HighDataPlacement || !LowDataPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("input data inherits earlier exec priority and is placed before later exec data"),
		HighDataPlacement->TargetPosition.Y <= LowDataPlacement->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_InterBlockOrderOverridesSnapshotRootOrder,
	"BlueprintHelper.GraphLayout.Solver.InterBlockOrderOverridesSnapshotRootOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_InterBlockOrderOverridesSnapshotRootOrder::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FNodeSnapshot NewEntry = MakeNode(
		TEXT("NewEntry"),
		TEXT("K2Node_CustomEvent"),
		TEXT("New Entry"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{});
	NewEntry.LayoutBlockId = TEXT("NewBlock");
	NewEntry.LayoutBlockOrder = 1;
	NewEntry.LayoutNodeOrder = 0;

	FNodeSnapshot OldEntry = MakeNode(
		TEXT("OldEntry"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Old Entry"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{});
	OldEntry.LayoutBlockId = TEXT("OldBlock");
	OldEntry.LayoutBlockOrder = 0;
	OldEntry.LayoutNodeOrder = 0;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(NewEntry);
	Snapshot.Nodes.Add(OldEntry);

	FRuleSet RuleSet;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.bUsePatternRowHeightBudget = false;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* OldPlacement = FindPlacement(Plan, TEXT("OldEntry"));
	const FNodePlacement* NewPlacement = FindPlacement(Plan, TEXT("NewEntry"));
	TestNotNull(TEXT("old block entry placement exists"), OldPlacement);
	TestNotNull(TEXT("new block entry placement exists"), NewPlacement);
	if (!OldPlacement || !NewPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("older block entry keeps priority over later block even when snapshot order is reversed"),
		OldPlacement->TargetPosition.Y < NewPlacement->TargetPosition.Y);
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

	FRuleSet RuleSet = MakeRuleSetWithScalarInputOffsets();
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRejectsLegacyEditorCanvasRoleCenters,
	"BlueprintHelper.GraphLayout.RuleSetJson.RejectsLegacyEditorCanvasRoleCenters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRejectsLegacyEditorCanvasRoleCenters::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	const FString LegacyJson = TEXT(R"JSON(
{
  "schema": "BlueprintHelper.GraphLayoutRuleSet.v1",
  "editor_canvas": {
    "role_centers": {
      "ExecNode": { "x": 300, "y": 100 }
    }
  },
  "role_rules": []
}
)JSON");

	FRuleSet Parsed;
	FValidationResult Validation;
	TestFalse(TEXT("legacy editor_canvas.role_centers is rejected"), FRuleSetJson::ImportString(LegacyJson, Parsed, Validation));
	TestTrue(TEXT("validation has errors"), Validation.Errors.Num() > 0);
	TestTrue(TEXT("validation has an explicit legacy error"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("editor_canvas.role_centers"));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsSemanticSceneCenters,
	"BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsSemanticSceneCenters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsSemanticSceneCenters::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState PureDataScene;
	PureDataScene.RoleCenters.Add(ENodeRole::VariableInput, FVector2D(91.0f, 120.0f));
	PureDataScene.RoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(355.0f, 164.0f));
	PureDataScene.RoleCenters.Add(ENodeRole::PureFunction, FVector2D(682.0f, 212.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::PureDataSubgraph, PureDataScene);

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("exports valid json"), FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid());

	const TSharedPtr<FJsonObject>* EditorCanvasObject = nullptr;
	TestTrue(
		TEXT("exports editor_canvas object"),
		RootObject.IsValid() && RootObject->TryGetObjectField(TEXT("editor_canvas"), EditorCanvasObject) && EditorCanvasObject && EditorCanvasObject->IsValid());
	if (EditorCanvasObject && EditorCanvasObject->IsValid())
	{
		const TSharedPtr<FJsonObject>* ScenesObject = nullptr;
		TestTrue(
			TEXT("exports editor_canvas.scenes"),
			(*EditorCanvasObject)->TryGetObjectField(TEXT("scenes"), ScenesObject) && ScenesObject && ScenesObject->IsValid());
		TestFalse(TEXT("does not export legacy role_centers at editor_canvas root"), (*EditorCanvasObject)->HasField(TEXT("role_centers")));
	}

	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("scene json imports"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	const FEditorCanvasSceneState* ParsedScene = Parsed.EditorCanvasScenes.Find(ESemanticScene::PureDataSubgraph);
	TestNotNull(TEXT("pure data scene exists"), ParsedScene);
	if (ParsedScene)
	{
		TestEqual(TEXT("variable input x"), ParsedScene->RoleCenters.FindRef(ENodeRole::VariableInput).X, 91.0);
		TestEqual(TEXT("variable input y"), ParsedScene->RoleCenters.FindRef(ENodeRole::VariableInput).Y, 120.0);
		TestEqual(TEXT("operator x"), ParsedScene->RoleCenters.FindRef(ENodeRole::OperatorOrCompare).X, 355.0);
		TestEqual(TEXT("operator y"), ParsedScene->RoleCenters.FindRef(ENodeRole::OperatorOrCompare).Y, 164.0);
		TestEqual(TEXT("pure function x"), ParsedScene->RoleCenters.FindRef(ENodeRole::PureFunction).X, 682.0);
		TestEqual(TEXT("pure function y"), ParsedScene->RoleCenters.FindRef(ENodeRole::PureFunction).Y, 212.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRejectsInvalidSemanticSceneEntries,
	"BlueprintHelper.GraphLayout.RuleSetJson.RejectsInvalidSemanticSceneEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRejectsInvalidSemanticSceneEntries::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	const FString InvalidScenesJson = TEXT(R"JSON(
{
  "schema": "BlueprintHelper.GraphLayoutRuleSet.v1",
  "editor_canvas": {
    "scenes": {
      "bogus_scene": {
        "role_centers": {}
      },
      "pure_data_subgraph": 42,
      "node_input_cluster": {},
      "multi_exec_output": {
        "role_centers": 42
      },
      "occupancy": {
        "role_centers": {
          "BogusRole": { "x": 11, "y": 17 },
          "ExecNode": { "x": "bad", "y": 29 },
          "Reroute": { "x": 3, "y": 5 }
        }
      }
    }
  },
  "role_rules": []
}
)JSON");

	FRuleSet Parsed;
	FValidationResult Validation;
	TestFalse(TEXT("invalid scene entries are rejected"), FRuleSetJson::ImportString(InvalidScenesJson, Parsed, Validation));
	TestTrue(TEXT("invalid scene entries add validation errors"), Validation.Errors.Num() > 0);
	TestTrue(TEXT("unknown scene key is reported"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("bogus_scene")) && Error.Contains(TEXT("unsupported scene"));
	}));
	TestTrue(TEXT("non-object scene value is reported"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("pure_data_subgraph")) && Error.Contains(TEXT("must be an object"));
	}));
	TestTrue(TEXT("missing role_centers is reported"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("node_input_cluster")) && Error.Contains(TEXT("role_centers")) && Error.Contains(TEXT("required"));
	}));
	TestTrue(TEXT("non-object role_centers is reported"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("multi_exec_output")) && Error.Contains(TEXT("role_centers")) && Error.Contains(TEXT("must be an object"));
	}));
	TestTrue(TEXT("unknown role is reported"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("BogusRole")) && Error.Contains(TEXT("unsupported role"));
	}));
	TestTrue(TEXT("invalid vector object is reported"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("ExecNode")) && Error.Contains(TEXT("numeric x and y"));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonSkipsInvalidSemanticSceneKeysOnExport,
	"BlueprintHelper.GraphLayout.RuleSetJson.SkipsInvalidSemanticSceneKeysOnExport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonSkipsInvalidSemanticSceneKeysOnExport::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	TestEqual(TEXT("invalid scene string is explicit"), FString(ToString(static_cast<ESemanticScene>(255))), FString(TEXT("unknown")));

	FRuleSet RuleSet;
	FEditorCanvasSceneState InvalidScene;
	InvalidScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(100.0f, 120.0f));
	RuleSet.EditorCanvasScenes.Add(static_cast<ESemanticScene>(255), InvalidScene);

	FEditorCanvasSceneState ValidScene;
	ValidScene.RoleCenters.Add(ENodeRole::PureFunction, FVector2D(260.0f, 140.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::PureDataSubgraph, ValidScene);

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("exports valid json"), FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid());

	const TSharedPtr<FJsonObject>* EditorCanvasObject = nullptr;
	TestTrue(
		TEXT("exports editor_canvas object"),
		RootObject.IsValid() && RootObject->TryGetObjectField(TEXT("editor_canvas"), EditorCanvasObject) && EditorCanvasObject && EditorCanvasObject->IsValid());
	if (!EditorCanvasObject || !EditorCanvasObject->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ScenesObject = nullptr;
	TestTrue(
		TEXT("exports editor_canvas.scenes"),
		(*EditorCanvasObject)->TryGetObjectField(TEXT("scenes"), ScenesObject) && ScenesObject && ScenesObject->IsValid());
	if (!ScenesObject || !ScenesObject->IsValid())
	{
		return false;
	}

	TestTrue(TEXT("valid scene key is exported"), (*ScenesObject)->HasField(TEXT("pure_data_subgraph")));
	TestFalse(TEXT("invalid scene does not serialize as linear_exec_chain"), (*ScenesObject)->HasField(TEXT("linear_exec_chain")));
	TestFalse(TEXT("invalid scene key is skipped instead of exported as unknown"), (*ScenesObject)->HasField(TEXT("unknown")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonExportsSemanticScenesDeterministically,
	"BlueprintHelper.GraphLayout.RuleSetJson.ExportsSemanticScenesDeterministically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonExportsSemanticScenesDeterministically::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FEditorCanvasSceneState LinearScene;
	LinearScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(420.0f, 126.0f));
	LinearScene.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(92.0f, 126.0f));

	FEditorCanvasSceneState OccupancyScene;
	OccupancyScene.RoleCenters.Add(ENodeRole::AsyncNode, FVector2D(300.0f, 220.0f));
	OccupancyScene.RoleCenters.Add(ENodeRole::Comment, FVector2D(300.0f, 156.0f));
	OccupancyScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(220.0f, 116.0f));

	FRuleSet FirstRuleSet;
	FirstRuleSet.EditorCanvasScenes.Add(ESemanticScene::Occupancy, OccupancyScene);
	FirstRuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);

	FRuleSet SecondRuleSet;
	SecondRuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);
	SecondRuleSet.EditorCanvasScenes.Add(ESemanticScene::Occupancy, OccupancyScene);

	TestEqual(TEXT("scene export does not depend on insertion order"), FRuleSetJson::ExportString(FirstRuleSet), FRuleSetJson::ExportString(SecondRuleSet));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SemanticSceneCatalogContainsFiveScenes,
	"BlueprintHelper.GraphLayout.SemanticScene.CatalogContainsFiveScenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SemanticSceneCatalogContainsFiveScenes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	const TArray<FSemanticSceneDefinition> Scenes = FSemanticSceneCatalog::GetAllScenes();
	TestEqual(TEXT("five scenes"), Scenes.Num(), 5);
	const FBlueprintHelperLayoutRuleEditorSettings EditorSettings;
	TestEqual(TEXT("layout rule editor uses 1:1 graph-unit authoring scale"), EditorSettings.CanvasRuleScale, 1.0f);
	const FVector2D HalfNodeSize = EditorSettings.NodeSize * 0.5f;
	constexpr float FooterReserveY = 40.0f;
	TSet<ESemanticScene> SceneIds;
	for (const FSemanticSceneDefinition& Scene : Scenes)
	{
		SceneIds.Add(Scene.Scene);
		TestFalse(TEXT("scene display name is set"), Scene.DisplayName.IsEmpty());
		TestTrue(TEXT("scene has nodes"), Scene.Nodes.Num() >= 2);
		TestTrue(TEXT("scene has directed edges"), Scene.Edges.Num() >= 1);
		for (const FSemanticSceneNodeDefinition& Node : Scene.Nodes)
		{
			TestFalse(TEXT("node chinese tooltip is set"), Node.TooltipZh.IsEmpty());
			TestTrue(TEXT("default center exists"), Scene.DefaultRoleCenters.Contains(Node.Role));
			if (const FVector2D* Center = Scene.DefaultRoleCenters.Find(Node.Role))
			{
				TestTrue(TEXT("default center keeps node inside editor canvas x"), Center->X >= HalfNodeSize.X && Center->X <= EditorSettings.CanvasDesiredSize.X - HalfNodeSize.X);
				TestTrue(TEXT("default center keeps node inside editor canvas y"), Center->Y >= HalfNodeSize.Y && Center->Y <= EditorSettings.CanvasDesiredSize.Y - FooterReserveY - HalfNodeSize.Y);
			}
		}
	}
	TestTrue(TEXT("has linear exec scene"), SceneIds.Contains(ESemanticScene::LinearExecChain));
	TestTrue(TEXT("has pure data scene"), SceneIds.Contains(ESemanticScene::PureDataSubgraph));
	TestTrue(TEXT("has node input cluster scene"), SceneIds.Contains(ESemanticScene::NodeInputCluster));
	TestTrue(TEXT("has multi exec output scene"), SceneIds.Contains(ESemanticScene::MultiExecOutput));
	TestTrue(TEXT("has occupancy scene"), SceneIds.Contains(ESemanticScene::Occupancy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SemanticSceneProjectsPureDataCenters,
	"BlueprintHelper.GraphLayout.SemanticScene.ProjectsPureDataCenters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SemanticSceneProjectsPureDataCenters::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState State;
	State.RoleCenters.Add(ENodeRole::VariableInput, FVector2D(100.0f, 100.0f));
	State.RoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(360.0f, 150.0f));
	State.RoleCenters.Add(ENodeRole::PureFunction, FVector2D(660.0f, 210.0f));

	FSemanticSceneAdapter::ApplySceneStateToRuleSet(ESemanticScene::PureDataSubgraph, State, RuleSet, 1.0f);

	TestEqual(TEXT("variable offset projected"), RuleSet.VariableInputOffsetX, 260.0f);
	TestEqual(TEXT("pure offset projected"), RuleSet.PureInputOffsetX, 300.0f);
	TestEqual(TEXT("pin row projected"), RuleSet.InputPinRowSpacing, 60.0f);
	TestTrue(TEXT("scene state stored"), RuleSet.EditorCanvasScenes.Contains(ESemanticScene::PureDataSubgraph));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SemanticSceneProjectsAllScenes,
	"BlueprintHelper.GraphLayout.SemanticScene.ProjectsAllScenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SemanticSceneProjectsAllScenes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet LinearRuleSet;
	FEditorCanvasSceneState LinearState;
	LinearState.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0f, 100.0f));
	LinearState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(500.0f, 106.0f));
	FSemanticSceneAdapter::ApplySceneStateToRuleSet(ESemanticScene::LinearExecChain, LinearState, LinearRuleSet, 2.0f);
	TestEqual(TEXT("linear exec column projected"), LinearRuleSet.ExecColumnSpacing, 200.0f);
	TestTrue(TEXT("linear exec alignment projected"), LinearRuleSet.bAlignExecNodesHorizontally);

	FRuleSet NodeInputRuleSet;
	FEditorCanvasSceneState NodeInputState;
	NodeInputState.RoleCenters.Add(ENodeRole::VariableInput, FVector2D(100.0f, 360.0f));
	NodeInputState.RoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(300.0f, 220.0f));
	NodeInputState.RoleCenters.Add(ENodeRole::PureFunction, FVector2D(340.0f, 160.0f));
	NodeInputState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(620.0f, 220.0f));
	FSemanticSceneAdapter::ApplySceneStateToRuleSet(ESemanticScene::NodeInputCluster, NodeInputState, NodeInputRuleSet, 2.0f);
	TestEqual(TEXT("node input pure offset projected"), NodeInputRuleSet.PureInputOffsetX, 140.0f);
	TestEqual(TEXT("node input variable offset projected"), NodeInputRuleSet.VariableInputOffsetX, 260.0f);
	TestEqual(TEXT("node input row spacing projected"), NodeInputRuleSet.InputPinRowSpacing, 70.0f);
	TestEqual(TEXT("node input padding x projected"), NodeInputRuleSet.DataClusterPaddingX, 20.0f);
	TestEqual(TEXT("node input padding y projected"), NodeInputRuleSet.DataClusterPaddingY, 100.0f);

	FRuleSet MultiExecRuleSet;
	FEditorCanvasSceneState MultiExecState;
	MultiExecState.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0f, 100.0f));
	MultiExecState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(700.0f, 116.0f));
	MultiExecState.RoleCenters.Add(ENodeRole::BranchControl, FVector2D(700.0f, 620.0f));
	FSemanticSceneAdapter::ApplySceneStateToRuleSet(ESemanticScene::MultiExecOutput, MultiExecState, MultiExecRuleSet, 2.0f);
	TestEqual(TEXT("multi exec column projected"), MultiExecRuleSet.ExecColumnSpacing, 300.0f);
	TestFalse(TEXT("multi exec misalignment projected"), MultiExecRuleSet.bAlignExecNodesHorizontally);
	TestEqual(TEXT("multi exec branch row projected"), MultiExecRuleSet.BranchRowSpacing, 260.0f);
	TestEqual(TEXT("multi exec branch padding projected"), MultiExecRuleSet.BranchRowPaddingY, 252.0f);

	FRuleSet OccupancyRuleSet;
	FEditorCanvasSceneState OccupancyState;
	OccupancyState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(200.0f, 100.0f));
	OccupancyState.RoleCenters.Add(ENodeRole::Comment, FVector2D(500.0f, 220.0f));
	OccupancyState.RoleCenters.Add(ENodeRole::AsyncNode, FVector2D(500.0f, 340.0f));
	FSemanticSceneAdapter::ApplySceneStateToRuleSet(ESemanticScene::Occupancy, OccupancyState, OccupancyRuleSet, 2.0f);
	TestEqual(TEXT("occupancy padding x projected"), OccupancyRuleSet.CollisionPaddingX, 150.0f);
	TestEqual(TEXT("occupancy padding y projected"), OccupancyRuleSet.CollisionPaddingY, 60.0f);
	TestEqual(TEXT("occupancy step projected"), OccupancyRuleSet.CollisionStepY, 60.0f);

	FRuleSet SanitizedScaleRuleSet;
	FEditorCanvasSceneState SanitizedScaleState;
	SanitizedScaleState.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0f, 100.0f));
	SanitizedScaleState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(2100.0f, 100.0f));
	FSemanticSceneAdapter::ApplySceneStateToRuleSet(ESemanticScene::LinearExecChain, SanitizedScaleState, SanitizedScaleRuleSet, 0.0f);
	TestEqual(TEXT("zero scale clamps instead of dividing unsafely"), SanitizedScaleRuleSet.ExecColumnSpacing, 900.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SemanticSceneResolveSceneStateUsesSavedOverrides,
	"BlueprintHelper.GraphLayout.SemanticScene.ResolveSceneStateUsesSavedOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SemanticSceneResolveSceneStateUsesSavedOverrides::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	const FEditorCanvasSceneState DefaultState = FSemanticSceneAdapter::ResolveSceneState(FRuleSet(), ESemanticScene::LinearExecChain);
	TestTrue(TEXT("default state contains event entry"), DefaultState.RoleCenters.Contains(ENodeRole::EventEntry));
	TestTrue(TEXT("default state contains exec node"), DefaultState.RoleCenters.Contains(ENodeRole::ExecNode));

	FRuleSet RuleSet;
	FEditorCanvasSceneState SavedState;
	SavedState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(522.0f, 188.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, SavedState);

	const FEditorCanvasSceneState ResolvedState = FSemanticSceneAdapter::ResolveSceneState(RuleSet, ESemanticScene::LinearExecChain);
	TestEqual(TEXT("missing saved event entry uses default x"), ResolvedState.RoleCenters.FindRef(ENodeRole::EventEntry).X, DefaultState.RoleCenters.FindRef(ENodeRole::EventEntry).X);
	TestEqual(TEXT("missing saved event entry uses default y"), ResolvedState.RoleCenters.FindRef(ENodeRole::EventEntry).Y, DefaultState.RoleCenters.FindRef(ENodeRole::EventEntry).Y);
	TestEqual(TEXT("saved exec node x"), ResolvedState.RoleCenters.FindRef(ENodeRole::ExecNode).X, 522.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSampleFactoryBuildsFiveComplexSamples,
	"BlueprintHelper.GraphLayout.Preview.SampleFactoryBuildsFiveComplexSamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSampleFactoryBuildsFiveComplexSamples::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	for (const FSemanticSceneDefinition& Scene : FSemanticSceneCatalog::GetAllScenes())
	{
		FGraphLayoutPreviewSample Sample;
		FString Error;
		TestTrue(FString::Printf(TEXT("sample builds for %s"), ToString(Scene.Scene)), FGraphLayoutPreviewSampleFactory::BuildSample(Scene.Scene, Sample, Error));
		TestTrue(TEXT("sample scene matches request"), Sample.Scene == Scene.Scene);
		TestTrue(TEXT("sample has nodes"), Sample.Snapshot.Nodes.Num() >= 4);
		TestTrue(TEXT("sample has materialization nodes"), Sample.Nodes.Num() >= Sample.Snapshot.Nodes.Num());
		TestTrue(TEXT("sample has links"), Sample.Links.Num() >= 1);

		auto ExpectAnchor = [this, &Sample](const FString& NodeId, const ENodeRole Role)
		{
			const FGraphLayoutPreviewNodeSpec* NodeSpec = FindPreviewNodeSpec(Sample, NodeId);
			TestNotNull(FString::Printf(TEXT("%s anchor node exists"), *NodeId), NodeSpec);
			if (!NodeSpec)
			{
				return;
			}
			TestTrue(FString::Printf(TEXT("%s uses preview role anchor"), *NodeId), NodeSpec->bUsePreviewRoleAnchor);
			TestTrue(FString::Printf(TEXT("%s anchor role matches"), *NodeId), NodeSpec->PreviewAnchorRole == Role);
		};

		auto ExpectNotAnchor = [this, &Sample](const FString& NodeId)
		{
			const FGraphLayoutPreviewNodeSpec* NodeSpec = FindPreviewNodeSpec(Sample, NodeId);
			TestNotNull(FString::Printf(TEXT("%s non-anchor node exists"), *NodeId), NodeSpec);
			if (!NodeSpec)
			{
				return;
			}
			TestFalse(FString::Printf(TEXT("%s is not a draggable role anchor"), *NodeId), NodeSpec->bUsePreviewRoleAnchor);
			TestTrue(FString::Printf(TEXT("%s has no preview anchor role"), *NodeId), NodeSpec->PreviewAnchorRole == ENodeRole::Unknown);
		};

		switch (Scene.Scene)
		{
		case ESemanticScene::LinearExecChain:
			ExpectAnchor(TEXT("EventStart"), ENodeRole::EventEntry);
			ExpectAnchor(TEXT("ResetState"), ENodeRole::ExecNode);
			ExpectNotAnchor(TEXT("SetCounter"));
			break;
		case ESemanticScene::PureDataSubgraph:
			ExpectAnchor(TEXT("SelfRef"), ENodeRole::VariableInput);
			ExpectAnchor(TEXT("ComposeKey"), ENodeRole::OperatorOrCompare);
			ExpectAnchor(TEXT("BuildArray"), ENodeRole::PureFunction);
			break;
		case ESemanticScene::NodeInputCluster:
			ExpectAnchor(TEXT("Consumer"), ENodeRole::ExecNode);
			ExpectAnchor(TEXT("ContextGet"), ENodeRole::VariableInput);
			ExpectAnchor(TEXT("IsValidGate"), ENodeRole::OperatorOrCompare);
			ExpectAnchor(TEXT("ComposePayload"), ENodeRole::PureFunction);
			break;
		case ESemanticScene::MultiExecOutput:
			ExpectAnchor(TEXT("EventStart"), ENodeRole::EventEntry);
			ExpectAnchor(TEXT("PrimaryPrint"), ENodeRole::ExecNode);
			ExpectAnchor(TEXT("Branch"), ENodeRole::BranchControl);
			ExpectNotAnchor(TEXT("BranchPrint"));
			ExpectNotAnchor(TEXT("CompletedPrint"));
			break;
		case ESemanticScene::Occupancy:
			ExpectAnchor(TEXT("CandidateExec"), ENodeRole::ExecNode);
			ExpectAnchor(TEXT("DelayAsync"), ENodeRole::AsyncNode);
			ExpectAnchor(TEXT("CommentBlocker"), ENodeRole::Comment);
			ExpectNotAnchor(TEXT("FallbackExec"));
			break;
		default:
			AddError(TEXT("unexpected semantic scene in preview sample catalog"));
			return false;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSampleFactoryProducesLayoutPlan,
	"BlueprintHelper.GraphLayout.Preview.SampleFactoryProducesLayoutPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSampleFactoryProducesLayoutPlan::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("pure data sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::PureDataSubgraph, Sample, Error));
	const FLayoutPlan Plan = FSolver::Solve(Sample.Snapshot, RuleSet);
	TestTrue(TEXT("layout produces placements"), Plan.Placements.Num() >= 4);
	TestEqual(TEXT("no issues"), Plan.Issues.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewServiceBuildsPureDataResult,
	"BlueprintHelper.GraphLayout.Preview.ServiceBuildsPureDataResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceBuildsPureDataResult::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::PureDataSubgraph;
	Request.RuleSetJson = FRuleSetJson::ExportString(FRuleSet());

	FGraphLayoutPreviewBuildResult Result;
	TestTrue(TEXT("sync test helper builds"), Service.BuildPreviewDataForTest(Request, Result));
	TestTrue(TEXT("result success"), Result.bSuccess);
	TestEqual(TEXT("scene"), Result.Sample.Scene, ESemanticScene::PureDataSubgraph);
	TestTrue(TEXT("has layout"), Result.LayoutPlan.Placements.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerKeepsPureDataNodesExecFree,
	"BlueprintHelper.GraphLayout.Preview.MaterializerKeepsPureDataNodesExecFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerKeepsPureDataNodesExecFree::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("preview materializer test runs on the game thread"), IsInGameThread());

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("node input sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::NodeInputCluster, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer creates node input preview graph"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("materializer result has no error"), Result.Error.IsEmpty());
	TestTrue(TEXT("preview graph valid"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	UEdGraphNode* NormalizeValueNode = FindMaterializedNodeById(Result, TEXT("NormalizeValue"));
	UEdGraphNode* ComposePayloadNode = FindMaterializedNodeById(Result, TEXT("ComposePayload"));
	TestNotNull(TEXT("normalize pure node materialized"), NormalizeValueNode);
	TestNotNull(TEXT("compose pure node materialized"), ComposePayloadNode);
	if (!NormalizeValueNode || !ComposePayloadNode)
	{
		return false;
	}

	TestEqual(TEXT("normalize pure preview node has no exec pins"), CountExecPins(NormalizeValueNode), 0);
	TestEqual(TEXT("compose pure preview node has no exec pins"), CountExecPins(ComposePayloadNode), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensEntryToFirstExec,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorStraightensEntryToFirstExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensEntryToFirstExec::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState LinearScene;
	LinearScene.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0f, 120.0f));
	LinearScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(500.0f, 120.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);
	RuleSet.ExecColumnSpacing = 360.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.CollisionPaddingX = 240.0f;
	RuleSet.CollisionPaddingY = 240.0f;

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::LinearExecChain;
	Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

	FGraphLayoutPreviewBuildResult Result;
	TestTrue(TEXT("preview builds"), Service.BuildPreviewDataForTest(Request, Result));
	TestTrue(TEXT("result success"), Result.bSuccess);

	const FNodePlacement* EventPlacement = FindPlacement(Result.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* ResetPlacement = FindPlacement(Result.LayoutPlan, TEXT("ResetState"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("reset placement exists"), ResetPlacement);
	if (!EventPlacement || !ResetPlacement)
	{
		return false;
	}

	const float EventExecOutputY =
		EventPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::EventEntry);
	const float ResetExecInputY =
		ResetPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::ExecNode);
	TestEqual(TEXT("event output pin and first exec input pin are horizontal"), ResetExecInputY, EventExecOutputY);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensLinearExecChain,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorStraightensLinearExecChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensLinearExecChain::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);
	const TArray<TPair<FString, ENodeRole>> Chain = {
		{TEXT("EventStart"), ENodeRole::EventEntry},
		{TEXT("ResetState"), ENodeRole::ExecNode},
		{TEXT("SetCounter"), ENodeRole::ExecNode},
		{TEXT("PrintLabel"), ENodeRole::ExecNode},
		{TEXT("DelayAsync"), ENodeRole::AsyncNode}
	};

	float ExpectedBaselineY = 0.0f;
	for (int32 NodeIndex = 0; NodeIndex < Chain.Num(); ++NodeIndex)
	{
		const FNodePlacement* Placement = FindPlacement(Plan, Chain[NodeIndex].Key);
		TestNotNull(
			FString::Printf(TEXT("%s placement exists"), *Chain[NodeIndex].Key),
			Placement);
		if (!Placement)
		{
			return false;
		}

		const float BaselineY = Placement->TargetPosition.Y + ExpectedExecBaselineOffsetY(Chain[NodeIndex].Value);
		if (NodeIndex == 0)
		{
			ExpectedBaselineY = BaselineY;
			continue;
		}

		TestEqual(
			FString::Printf(TEXT("%s exec pin is on the linear chain baseline"), *Chain[NodeIndex].Key),
			BaselineY,
			ExpectedBaselineY);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensDataSceneEntryToConsumer,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorStraightensDataSceneEntryToConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensDataSceneEntryToConsumer::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	struct FSceneExpectation
	{
		ESemanticScene Scene;
		FString ConsumerNodeId;
	};

	const TArray<FSceneExpectation> Expectations = {
		{ESemanticScene::PureDataSubgraph, TEXT("ConsumeArray")},
		{ESemanticScene::NodeInputCluster, TEXT("Consumer")}
	};

	for (const FSceneExpectation& Expectation : Expectations)
	{
		FGraphLayoutPreviewSample Sample;
		FString Error;
		TestTrue(
			FString::Printf(TEXT("sample builds for %s"), ToString(Expectation.Scene)),
			FGraphLayoutPreviewSampleFactory::BuildSample(Expectation.Scene, Sample, Error));

		FRuleSet RuleSet;
		const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);
		const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("EventStart"));
		const FNodePlacement* ConsumerPlacement = FindPlacement(Plan, Expectation.ConsumerNodeId);
		TestNotNull(FString::Printf(TEXT("%s event placement exists"), ToString(Expectation.Scene)), EventPlacement);
		TestNotNull(FString::Printf(TEXT("%s consumer placement exists"), ToString(Expectation.Scene)), ConsumerPlacement);
		if (!EventPlacement || !ConsumerPlacement)
		{
			return false;
		}

		const float EventExecOutputY =
			EventPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::EventEntry);
		const float ConsumerExecInputY =
			ConsumerPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::ExecNode);
		TestEqual(
			FString::Printf(TEXT("%s event output and consumer input pins are horizontal"), ToString(Expectation.Scene)),
			ConsumerExecInputY,
			EventExecOutputY);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerConnectsOccupancyExistingGuard,
	"BlueprintHelper.GraphLayout.Preview.MaterializerConnectsOccupancyExistingGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerConnectsOccupancyExistingGuard::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("preview materializer test runs on the game thread"), IsInGameThread());

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("occupancy sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::Occupancy, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer creates occupancy preview graph"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("materializer result has no error"), Result.Error.IsEmpty());
	TestTrue(TEXT("preview graph valid"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	UEdGraphNode* DelayNode = FindMaterializedNodeById(Result, TEXT("DelayAsync"));
	UEdGraphNode* ExistingGuardNode = FindMaterializedNodeById(Result, TEXT("ExistingGuard"));
	TestNotNull(TEXT("delay async preview node materialized"), DelayNode);
	TestNotNull(TEXT("existing guard preview node materialized"), ExistingGuardNode);
	if (!DelayNode || !ExistingGuardNode)
	{
		return false;
	}

	UEdGraphPin* DelayCompletedPin = FindPinByNameAndDirection(DelayNode, FName(TEXT("Completed")), EGPD_Output);
	UEdGraphPin* ExistingGuardExecPin = FindPinByNameAndDirection(ExistingGuardNode, UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	TestNotNull(TEXT("delay async completed pin exists"), DelayCompletedPin);
	TestNotNull(TEXT("existing guard exec input pin exists"), ExistingGuardExecPin);
	if (!DelayCompletedPin || !ExistingGuardExecPin)
	{
		return false;
	}

	TestTrue(TEXT("delay async completed exec links to existing guard"), DelayCompletedPin->LinkedTo.Contains(ExistingGuardExecPin));
	TestTrue(TEXT("existing guard exec input links back to delay async"), ExistingGuardExecPin->LinkedTo.Contains(DelayCompletedPin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorKeepsOccupancyEntrySeparate,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorKeepsOccupancyEntrySeparate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorKeepsOccupancyEntrySeparate::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("occupancy sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::Occupancy, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("EventStart"));
	const FNodePlacement* CandidatePlacement = FindPlacement(Plan, TEXT("CandidateExec"));
	const FNodePlacement* FallbackPlacement = FindPlacement(Plan, TEXT("FallbackExec"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("candidate placement exists"), CandidatePlacement);
	TestNotNull(TEXT("fallback placement exists"), FallbackPlacement);
	if (!EventPlacement || !CandidatePlacement || !FallbackPlacement)
	{
		return false;
	}

	const FVector2D EventSize = FindPreviewNodeSize(Sample, TEXT("EventStart"));
	const FVector2D CandidateSize = FindPreviewNodeSize(Sample, TEXT("CandidateExec"));
	const FVector2D FallbackSize = FindPreviewNodeSize(Sample, TEXT("FallbackExec"));
	TestFalse(
		TEXT("occupancy preview event entry does not overlap candidate exec node"),
		RectsOverlap(*EventPlacement, EventSize, *CandidatePlacement, CandidateSize));
	TestFalse(
		TEXT("occupancy preview candidate exec node does not overlap fallback exec node"),
		RectsOverlap(*CandidatePlacement, CandidateSize, *FallbackPlacement, FallbackSize));
	TestFalse(
		TEXT("occupancy preview event entry does not overlap fallback exec node"),
		RectsOverlap(*EventPlacement, EventSize, *FallbackPlacement, FallbackSize));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorKeepsOccupancyFallbackClearOfBlocker,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorKeepsOccupancyFallbackClearOfBlocker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorKeepsOccupancyFallbackClearOfBlocker::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("occupancy sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::Occupancy, Sample, Error));

	FRuleSet RuleSet;
	FEditorCanvasSceneState OccupancyScene;
	OccupancyScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(216.0f, 128.0f));
	OccupancyScene.RoleCenters.Add(ENodeRole::AsyncNode, FVector2D(636.0f, 308.0f));
	OccupancyScene.RoleCenters.Add(ENodeRole::Comment, FVector2D(730.0f, 130.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::Occupancy, OccupancyScene);

	const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);
	const FNodePlacement* FallbackPlacement = FindPlacement(Plan, TEXT("FallbackExec"));
	const FNodePlacement* CommentPlacement = FindPlacement(Plan, TEXT("CommentBlocker"));
	TestNotNull(TEXT("fallback placement exists"), FallbackPlacement);
	TestNotNull(TEXT("comment blocker placement exists"), CommentPlacement);
	if (!FallbackPlacement || !CommentPlacement)
	{
		return false;
	}

	const FVector2D FallbackSize = FindPreviewNodeSize(Sample, TEXT("FallbackExec"));
	const FVector2D CommentSize = FindPreviewNodeSize(Sample, TEXT("CommentBlocker"));
	TestFalse(
		TEXT("occupancy preview fallback exec node does not overlap existing comment blocker"),
		RectsOverlap(*FallbackPlacement, FallbackSize, *CommentPlacement, CommentSize));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewServiceUsesCurrentRuleSetSceneAnchors,
	"BlueprintHelper.GraphLayout.Preview.ServiceUsesCurrentRuleSetSceneAnchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceUsesCurrentRuleSetSceneAnchors::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	auto BuildResultForAnchors = [](const FVector2D& EventAnchor, const FVector2D& ExecAnchor)
	{
		FRuleSet RuleSet;
		FEditorCanvasSceneState LinearScene;
		LinearScene.RoleCenters.Add(ENodeRole::EventEntry, EventAnchor);
		LinearScene.RoleCenters.Add(ENodeRole::ExecNode, ExecAnchor);
		RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);

		FGraphLayoutPreviewService Service;
		FGraphLayoutPreviewRequest Request;
		Request.Scene = ESemanticScene::LinearExecChain;
		Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

		FGraphLayoutPreviewBuildResult Result;
		Service.BuildPreviewDataForTest(Request, Result);
		return Result;
	};

	const FGraphLayoutPreviewBuildResult FirstResult = BuildResultForAnchors(FVector2D(120.0f, 140.0f), FVector2D(420.0f, 140.0f));
	const FGraphLayoutPreviewBuildResult SecondResult = BuildResultForAnchors(FVector2D(300.0f, 260.0f), FVector2D(660.0f, 260.0f));
	TestTrue(TEXT("first result succeeds"), FirstResult.bSuccess);
	TestTrue(TEXT("second result succeeds"), SecondResult.bSuccess);

	const FNodePlacement* FirstEventPlacement = FindPlacement(FirstResult.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* FirstResetPlacement = FindPlacement(FirstResult.LayoutPlan, TEXT("ResetState"));
	const FNodePlacement* SecondEventPlacement = FindPlacement(SecondResult.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* SecondResetPlacement = FindPlacement(SecondResult.LayoutPlan, TEXT("ResetState"));
	TestNotNull(TEXT("first event placement exists"), FirstEventPlacement);
	TestNotNull(TEXT("first reset placement exists"), FirstResetPlacement);
	TestNotNull(TEXT("second event placement exists"), SecondEventPlacement);
	TestNotNull(TEXT("second reset placement exists"), SecondResetPlacement);
	if (!FirstEventPlacement || !FirstResetPlacement || !SecondEventPlacement || !SecondResetPlacement)
	{
		return false;
	}

	TestEqual(TEXT("first event role center drives preview x"), FirstEventPlacement->TargetPosition.X + 202.0f, 120.0);
	TestEqual(
		TEXT("first event role center drives preview y"),
		FirstEventPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::EventEntry),
		140.0);
	TestEqual(TEXT("first exec role center drives preview x"), FirstResetPlacement->TargetPosition.X + 16.0f, 420.0);
	TestEqual(
		TEXT("first exec role center drives preview y"),
		FirstResetPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::ExecNode),
		140.0);
	TestEqual(TEXT("second event role center drives preview x"), SecondEventPlacement->TargetPosition.X + 202.0f, 300.0);
	TestEqual(
		TEXT("second event role center drives preview y"),
		SecondEventPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::EventEntry),
		260.0);
	TestEqual(TEXT("second exec role center drives preview x"), SecondResetPlacement->TargetPosition.X + 16.0f, 660.0);
	TestEqual(
		TEXT("second exec role center drives preview y"),
		SecondResetPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::ExecNode),
		260.0);
	TestTrue(TEXT("changing scene anchors changes preview event position"), !FirstEventPlacement->TargetPosition.Equals(SecondEventPlacement->TargetPosition));
	TestTrue(TEXT("changing scene anchors changes preview exec position"), !FirstResetPlacement->TargetPosition.Equals(SecondResetPlacement->TargetPosition));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesRoleOverlap,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorPreservesRoleOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesRoleOverlap::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState LinearScene;
	LinearScene.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(240.0f, 160.0f));
	LinearScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(240.0f, 160.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);
	RuleSet.ExecColumnSpacing = 900.0f;
	RuleSet.CollisionPaddingX = 400.0f;
	RuleSet.CollisionPaddingY = 400.0f;
	RuleSet.CollisionStepY = 400.0f;

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::LinearExecChain;
	Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

	FGraphLayoutPreviewBuildResult Result;
	TestTrue(TEXT("preview builds"), Service.BuildPreviewDataForTest(Request, Result));
	TestTrue(TEXT("result success"), Result.bSuccess);

	const FNodePlacement* EventPlacement = FindPlacement(Result.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* ResetPlacement = FindPlacement(Result.LayoutPlan, TEXT("ResetState"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("reset placement exists"), ResetPlacement);
	if (!EventPlacement || !ResetPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("overlapped role anchors produce overlapping native node bounds"),
		RectsOverlap(*EventPlacement, FVector2D(220.0f, 88.0f), *ResetPlacement, FVector2D(228.0f, 96.0f)));
	TestTrue(
		TEXT("overlap is not removed by solver occupancy"),
		FMath::Abs(EventPlacement->TargetPosition.X - ResetPlacement->TargetPosition.X) < 260.0f &&
		FMath::Abs(EventPlacement->TargetPosition.Y - ResetPlacement->TargetPosition.Y) < 140.0f);
	TestEqual(TEXT("event semantic anchor x remains on dragged role center"), EventPlacement->TargetPosition.X + 202.0f, 240.0);
	TestEqual(
		TEXT("event semantic anchor y remains on dragged role center"),
		EventPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::EventEntry),
		160.0);
	TestEqual(TEXT("exec semantic anchor x remains on dragged role center"), ResetPlacement->TargetPosition.X + 16.0f, 240.0);
	TestEqual(
		TEXT("exec semantic anchor y remains on dragged role center"),
		ResetPlacement->TargetPosition.Y + ExpectedExecBaselineOffsetY(ENodeRole::ExecNode),
		160.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesOverlapAcrossScenes,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorPreservesOverlapAcrossScenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesOverlapAcrossScenes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	for (const FSemanticSceneDefinition& SceneDefinition : FSemanticSceneCatalog::GetAllScenes())
	{
		FRuleSet RuleSet;
		FEditorCanvasSceneState SceneState;
		for (const FSemanticSceneNodeDefinition& NodeDefinition : SceneDefinition.Nodes)
		{
			SceneState.RoleCenters.Add(NodeDefinition.Role, FVector2D(320.0f, 180.0f));
		}
		RuleSet.EditorCanvasScenes.Add(SceneDefinition.Scene, SceneState);

		FGraphLayoutPreviewService Service;
		FGraphLayoutPreviewRequest Request;
		Request.Scene = SceneDefinition.Scene;
		Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

		FGraphLayoutPreviewBuildResult Result;
		TestTrue(FString::Printf(TEXT("preview builds for %s"), ToString(SceneDefinition.Scene)), Service.BuildPreviewDataForTest(Request, Result));
		TestTrue(TEXT("result success"), Result.bSuccess);
		TestTrue(TEXT("has placements"), Result.LayoutPlan.Placements.Num() > 0);

		int32 OverlapPairCount = 0;
		for (int32 LeftIndex = 0; LeftIndex < Result.LayoutPlan.Placements.Num(); ++LeftIndex)
		{
			for (int32 RightIndex = LeftIndex + 1; RightIndex < Result.LayoutPlan.Placements.Num(); ++RightIndex)
			{
				const FNodePlacement& Left = Result.LayoutPlan.Placements[LeftIndex];
				const FNodePlacement& Right = Result.LayoutPlan.Placements[RightIndex];
				if (Left.Reason == TEXT("preview_semantic_role_anchor") &&
					Right.Reason == TEXT("preview_semantic_role_anchor") &&
					RectsOverlap(
						Left,
						FindPreviewNodeSize(Result.Sample, Left.NodeId),
						Right,
						FindPreviewNodeSize(Result.Sample, Right.NodeId)))
				{
					++OverlapPairCount;
				}
			}
		}
		TestTrue(
			FString::Printf(TEXT("anchored preview nodes overlap for %s"), ToString(SceneDefinition.Scene)),
			OverlapPairCount > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerPreservesSemanticOverlap,
	"BlueprintHelper.GraphLayout.Preview.MaterializerPreservesSemanticOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerPreservesSemanticOverlap::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("preview materializer test runs on the game thread"), IsInGameThread());

	FRuleSet RuleSet;
	FEditorCanvasSceneState LinearScene;
	LinearScene.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(260.0f, 180.0f));
	LinearScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(260.0f, 180.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));
	const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);

	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("EventStart"));
	const FNodePlacement* ResetPlacement = FindPlacement(Plan, TEXT("ResetState"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("reset placement exists"), ResetPlacement);
	if (!EventPlacement || !ResetPlacement)
	{
		return false;
	}

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer creates graph"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("preview graph valid"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	TestTrue(
		TEXT("event materializer uses semantic plan position"),
		MaterializedNodeMatchesPlacement(Result, TEXT("EventStart"), EventPlacement->TargetPosition));
	TestTrue(
		TEXT("reset materializer uses semantic plan position"),
		MaterializedNodeMatchesPlacement(Result, TEXT("ResetState"), ResetPlacement->TargetPosition));
	TestTrue(
		TEXT("materialized semantic placements still overlap"),
		RectsOverlap(
			*EventPlacement,
			FindPreviewNodeSize(Sample, TEXT("EventStart")),
			*ResetPlacement,
			FindPreviewNodeSize(Sample, TEXT("ResetState"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewServiceCancelsStaleJob,
	"BlueprintHelper.GraphLayout.Preview.ServiceCancelsStaleJob",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceCancelsStaleJob::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewService Service;
	const uint64 FirstJob = Service.StartPreviewBuild(FGraphLayoutPreviewRequest{ ESemanticScene::LinearExecChain, FRuleSetJson::ExportString(FRuleSet()) });
	Service.Cancel(FirstJob);
	TestTrue(TEXT("job marked stale"), Service.IsJobCancelledForTest(FirstJob));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerCreatesTransientGraph,
	"BlueprintHelper.GraphLayout.Preview.MaterializerCreatesTransientGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerCreatesTransientGraph::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("preview materializer test runs on the game thread"), IsInGameThread());

	const FGraphLayoutPreviewSample Sample = MakeMaterializerTestSample();
	const FLayoutPlan Plan = MakeMaterializerTestPlan();

	FGraphLayoutPreviewMaterializer Materializer;
	Materializer.Begin(Sample, Plan);

	int32 TickGuard = 0;
	while (Materializer.Tick(0.01f))
	{
		++TickGuard;
		if (TickGuard > 128)
		{
			AddError(TEXT("preview materializer tick loop exceeded guard"));
			return false;
		}
	}

	TestTrue(TEXT("materializer completes"), Materializer.IsComplete());
	const FGraphLayoutPreviewMaterializerResult& Result = Materializer.GetResult();
	TestTrue(TEXT("materializer result has no error"), Result.Error.IsEmpty());
	TestTrue(TEXT("preview blueprint is valid"), Result.PreviewBlueprint.IsValid());
	TestTrue(TEXT("preview graph is valid"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewBlueprint.IsValid() || !Result.PreviewGraph.IsValid())
	{
		return false;
	}

	UBlueprint* PreviewBlueprint = Result.PreviewBlueprint.Get();
	UEdGraph* PreviewGraph = Result.PreviewGraph.Get();
	TestTrue(TEXT("preview blueprint is transient"), PreviewBlueprint->HasAnyFlags(RF_Transient));
	TestTrue(TEXT("preview graph is transient"), PreviewGraph->HasAnyFlags(RF_Transient));
	TestTrue(TEXT("preview graph is editable for layout calibration"), PreviewGraph->bEditable);
	TestTrue(TEXT("preview graph is attached to blueprint ubergraph pages"), PreviewBlueprint->UbergraphPages.Contains(PreviewGraph));
	TestTrue(TEXT("preview graph uses K2 schema"), PreviewGraph->Schema == UEdGraphSchema_K2::StaticClass());
	TestEqual(TEXT("preview graph materializes all requested nodes"), PreviewGraph->Nodes.Num(), Sample.Nodes.Num());

	const FGuid* EventGuid = Result.NodeGuidsById.Find(TEXT("EventStart"));
	TestNotNull(TEXT("event guid exists"), EventGuid);
	if (EventGuid)
	{
		const FString* EventNodeId = Result.NodeIdsByGuid.Find(*EventGuid);
		const ENodeRole* EventRole = Result.RolesByGuid.Find(*EventGuid);
		const ENodeRole* EventAnchorRole = Result.AnchorRolesByGuid.Find(*EventGuid);
		TestNotNull(TEXT("event reverse node id exists"), EventNodeId);
		TestNotNull(TEXT("event reverse role exists"), EventRole);
		TestNotNull(TEXT("event reverse anchor role exists"), EventAnchorRole);
		if (EventNodeId && EventRole && EventAnchorRole)
		{
			TestEqual(TEXT("event reverse node id"), *EventNodeId, FString(TEXT("EventStart")));
			TestEqual(TEXT("event reverse role"), *EventRole, ENodeRole::EventEntry);
			TestEqual(TEXT("event reverse anchor role"), *EventAnchorRole, ENodeRole::EventEntry);
		}
	}

	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("EventStart"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(FindNodeByExactClass(PreviewGraph, UK2Node_CustomEvent::StaticClass()));
	TestNotNull(TEXT("custom event node is materialized"), EventNode);
	if (EventNode && EventPlacement)
	{
		TestEqual<int32>(TEXT("custom event uses plan x"), EventNode->NodePosX, static_cast<int32>(EventPlacement->TargetPosition.X));
		TestEqual<int32>(TEXT("custom event uses plan y"), EventNode->NodePosY, static_cast<int32>(EventPlacement->TargetPosition.Y));
	}

	UEdGraphNode* GenericNode = FindNodeByExactClass(PreviewGraph, UEdGraphNode::StaticClass());
	TestNotNull(TEXT("generic fallback node is materialized"), GenericNode);
	UEdGraphPin* GenericValuePin = FindPinByNameAndDirection(GenericNode, FName(TEXT("Value")), EGPD_Output);
	TestNotNull(TEXT("generic fallback node creates preview output pin"), GenericValuePin);

	UK2Node_Self* SelfNode = Cast<UK2Node_Self>(FindNodeByExactClass(PreviewGraph, UK2Node_Self::StaticClass()));
	TestNotNull(TEXT("self node is materialized"), SelfNode);
	UEdGraphPin* SelfValuePin = FindPinByNameAndDirection(SelfNode, FName(TEXT("Value")), EGPD_Output);
	TestNotNull(TEXT("self node creates preview-only value pin when default pin names differ"), SelfValuePin);

	UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(FindNodeByExactClass(PreviewGraph, UK2Node_CallFunction::StaticClass()));
	TestNotNull(TEXT("call function node is materialized"), CallFunctionNode);
	UEdGraphPin* PreviewInputPin = FindPinByNameAndDirection(CallFunctionNode, FName(TEXT("In")), EGPD_Input);
	TestNotNull(TEXT("call function node creates preview-only input pin when sample pin is missing"), PreviewInputPin);

	UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(FindNodeByExactClass(PreviewGraph, UK2Node_ExecutionSequence::StaticClass()));
	TestNotNull(TEXT("sequence node is materialized"), SequenceNode);
	UEdGraphPin* EventThenPin = FindPinByNameAndDirection(EventNode, FName(TEXT("then")), EGPD_Output);
	UEdGraphPin* SequenceExecPin = FindPinByNameAndDirection(SequenceNode, FName(TEXT("execute")), EGPD_Input);
	TestNotNull(TEXT("event exec pin exists"), EventThenPin);
	TestNotNull(TEXT("sequence exec pin exists"), SequenceExecPin);
	if (EventThenPin && SequenceExecPin)
	{
		TestTrue(TEXT("event is linked to sequence"), EventThenPin->LinkedTo.Contains(SequenceExecPin));
	}

	UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(FindNodeByExactClass(PreviewGraph, UEdGraphNode_Comment::StaticClass()));
	TestNotNull(TEXT("comment node is materialized"), CommentNode);

	FGraphLayoutPreviewMaterializerResult SyncResult;
	TestTrue(TEXT("sync helper materializes preview graph"), Materializer.MaterializeForTest(Sample, Plan, SyncResult));
	TestTrue(TEXT("sync helper returns valid graph"), SyncResult.PreviewGraph.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionModelDetectsMovedNodes,
	"BlueprintHelper.GraphLayout.Preview.InteractionModelDetectsMovedNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionModelDetectsMovedNodes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	const FGraphLayoutPreviewSample Sample = MakeMaterializerTestSample();
	const FLayoutPlan Plan = MakeMaterializerTestPlan();

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer succeeds"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("preview graph exists"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	FGraphLayoutPreviewInteractionModel Model;
	TestTrue(TEXT("model initializes"), Model.Initialize(Result, Result.PreviewGraph.Get()));
	Model.BeginInteraction(Result.PreviewGraph.Get());

	UEdGraphNode* EventNode = nullptr;
	for (UEdGraphNode* Node : Result.PreviewGraph->Nodes)
	{
		if (Node && Result.NodeIdsByGuid.FindRef(Node->NodeGuid) == TEXT("EventStart"))
		{
			EventNode = Node;
			break;
		}
	}
	TestNotNull(TEXT("event node found"), EventNode);
	if (!EventNode)
	{
		return false;
	}

	EventNode->NodePosX += 123;
	EventNode->NodePosY += 45;

	FGraphLayoutPreviewInteractionCommit Commit;
	TestTrue(TEXT("end interaction detects change"), Model.EndInteraction(Result.PreviewGraph.Get(), Commit));
	TestEqual(TEXT("one moved node"), Commit.MovedNodes.Num(), 1);
	TestEqual(TEXT("moved node id"), Commit.MovedNodes[0].NodeId, FString(TEXT("EventStart")));
	TestEqual(TEXT("moved anchor role"), Commit.MovedNodes[0].AnchorRole, ENodeRole::EventEntry);
	TestEqual(TEXT("moved target x"), Commit.MovedNodes[0].EndTopLeft.X, static_cast<double>(EventNode->NodePosX));
	TestEqual(TEXT("moved target y"), Commit.MovedNodes[0].EndTopLeft.Y, static_cast<double>(EventNode->NodePosY));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionModelCommitsMovedAnchorsToRuleSet,
	"BlueprintHelper.GraphLayout.Preview.InteractionModelCommitsMovedAnchorsToRuleSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionModelCommitsMovedAnchorsToRuleSet::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewInteractionCommit Commit;
	FGraphLayoutPreviewMovedNode& Moved = Commit.MovedNodes.AddDefaulted_GetRef();
	Moved.NodeId = TEXT("ResetState");
	Moved.NodeGuid = FGuid::NewGuid();
	Moved.Role = ENodeRole::ExecNode;
	Moved.AnchorRole = ENodeRole::ExecNode;
	Moved.BeginTopLeft = FVector2D(100.0, 100.0);
	Moved.EndTopLeft = FVector2D(480.0, 220.0);
	Moved.Size = FVector2D(220.0, 96.0);

	FRuleSet RuleSet;
	FEditorCanvasSceneState SceneState;
	SceneState.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0, 100.0));
	SceneState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(300.0, 100.0));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, SceneState);

	const FString InputJson = FRuleSetJson::ExportString(RuleSet);
	FString OutputJson;
	FString Error;
	TestTrue(
		TEXT("commit succeeds"),
		FGraphLayoutPreviewInteractionModel::BuildRuleSetJsonForCommit(
			InputJson,
			ESemanticScene::LinearExecChain,
			Commit,
			OutputJson,
			Error));

	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("output imports"), FRuleSetJson::ImportString(OutputJson, Parsed, Validation));
	const FEditorCanvasSceneState Resolved =
		FSemanticSceneAdapter::ResolveSceneState(Parsed, ESemanticScene::LinearExecChain);
	const FVector2D ExecCenter = Resolved.RoleCenters.FindRef(ENodeRole::ExecNode);
	TestEqual(TEXT("exec center x follows moved preview node center"), ExecCenter.X, 590.0);
	TestEqual(TEXT("exec center y follows moved preview node center"), ExecCenter.Y, 268.0);
	TestTrue(TEXT("column spacing recalculated"), Parsed.ExecColumnSpacing > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionModelRejectsNodeCountMutation,
	"BlueprintHelper.GraphLayout.Preview.InteractionModelRejectsNodeCountMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionModelRejectsNodeCountMutation::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	const FGraphLayoutPreviewSample Sample = MakeMaterializerTestSample();
	const FLayoutPlan Plan = MakeMaterializerTestPlan();
	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer succeeds"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("preview graph exists"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	FGraphLayoutPreviewInteractionModel Model;
	TestTrue(TEXT("model initializes"), Model.Initialize(Result, Result.PreviewGraph.Get()));
	Model.BeginInteraction(Result.PreviewGraph.Get());

	UEdGraphNode* AddedNode = NewObject<UEdGraphNode>(Result.PreviewGraph.Get());
	Result.PreviewGraph->AddNode(AddedNode, true, false);

	FGraphLayoutPreviewInteractionCommit Commit;
	TestFalse(TEXT("end rejects node count mutation"), Model.EndInteraction(Result.PreviewGraph.Get(), Commit));
	TestTrue(TEXT("rejection reason mentions node count"), Commit.RejectionReason.Contains(TEXT("node count")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionModelRejectsLinkRewireMutation,
	"BlueprintHelper.GraphLayout.Preview.InteractionModelRejectsLinkRewireMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionModelRejectsLinkRewireMutation::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	const FGraphLayoutPreviewSample Sample = MakeMaterializerTestSample();
	const FLayoutPlan Plan = MakeMaterializerTestPlan();
	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer succeeds"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("preview graph exists"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	UEdGraphNode* SequenceNode = nullptr;
	UEdGraphNode* BranchNode = nullptr;
	UEdGraphNode* PrintNode = nullptr;
	for (UEdGraphNode* Node : Result.PreviewGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		const FString NodeId = Result.NodeIdsByGuid.FindRef(Node->NodeGuid);
		if (NodeId == TEXT("Sequence"))
		{
			SequenceNode = Node;
		}
		else if (NodeId == TEXT("Branch"))
		{
			BranchNode = Node;
		}
		else if (NodeId == TEXT("PrintNode"))
		{
			PrintNode = Node;
		}
	}

	UEdGraphPin* Then0Pin = FindPinByNameAndDirection(SequenceNode, FName(TEXT("Then_0")), EGPD_Output);
	UEdGraphPin* Then1Pin = FindPinByNameAndDirection(SequenceNode, FName(TEXT("Then_1")), EGPD_Output);
	UEdGraphPin* BranchExecPin = FindPinByNameAndDirection(BranchNode, FName(TEXT("execute")), EGPD_Input);
	UEdGraphPin* PrintExecPin = FindPinByNameAndDirection(PrintNode, FName(TEXT("execute")), EGPD_Input);
	TestNotNull(TEXT("sequence Then_0 pin exists"), Then0Pin);
	TestNotNull(TEXT("sequence Then_1 pin exists"), Then1Pin);
	TestNotNull(TEXT("branch exec pin exists"), BranchExecPin);
	TestNotNull(TEXT("print exec pin exists"), PrintExecPin);
	if (!Then0Pin || !Then1Pin || !BranchExecPin || !PrintExecPin)
	{
		return false;
	}

	FGraphLayoutPreviewInteractionModel Model;
	TestTrue(TEXT("model initializes"), Model.Initialize(Result, Result.PreviewGraph.Get()));
	Model.BeginInteraction(Result.PreviewGraph.Get());

	Then0Pin->BreakLinkTo(BranchExecPin, false);
	Then1Pin->BreakLinkTo(PrintExecPin, false);
	Then0Pin->MakeLinkTo(PrintExecPin, false);
	Then1Pin->MakeLinkTo(BranchExecPin, false);
	TestFalse(TEXT("Then_0 no longer links to branch"), Then0Pin->LinkedTo.Contains(BranchExecPin));
	TestFalse(TEXT("Then_1 no longer links to print"), Then1Pin->LinkedTo.Contains(PrintExecPin));
	TestTrue(TEXT("Then_0 links to print"), Then0Pin->LinkedTo.Contains(PrintExecPin));
	TestTrue(TEXT("Then_1 links to branch"), Then1Pin->LinkedTo.Contains(BranchExecPin));

	FGraphLayoutPreviewInteractionCommit Commit;
	TestFalse(TEXT("end rejects link rewire mutation"), Model.EndInteraction(Result.PreviewGraph.Get(), Commit));
	AddInfo(FString::Printf(TEXT("link rewire rejection reason: %s"), *Commit.RejectionReason));
	TestTrue(TEXT("rejection reason mentions link"), Commit.RejectionReason.Contains(TEXT("link")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionSurfaceForwardsMouseBoundaries,
	"BlueprintHelper.GraphLayout.Preview.InteractionSurfaceForwardsMouseBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionSurfaceForwardsMouseBoundaries::RunTest(const FString& Parameters)
{
	int32 BeginCount = 0;
	int32 EndCount = 0;
	TSharedRef<SBlueprintHelperLayoutPreviewInteractionSurface> Surface =
		SNew(SBlueprintHelperLayoutPreviewInteractionSurface)
		.OnInteractionBegin(FBlueprintHelperLayoutPreviewInteractionEvent::CreateLambda([&BeginCount]()
		{
			++BeginCount;
		}))
		.OnInteractionEnd(FBlueprintHelperLayoutPreviewInteractionEvent::CreateLambda([&EndCount]()
		{
			++EndCount;
		}))
		[
			SNullWidget::NullWidget
		];

	FGeometry Geometry = FGeometry::MakeRoot(FVector2D(400.0f, 300.0f), FSlateLayoutTransform());
	TSet<FKey> PressedButtons;
	PressedButtons.Add(EKeys::LeftMouseButton);
	const FPointerEvent LeftMouseEvent(
		0,
		0,
		FVector2D(32.0f, 32.0f),
		FVector2D(32.0f, 32.0f),
		PressedButtons,
		EKeys::LeftMouseButton,
		0.0f,
		FModifierKeysState());
	const FPointerEvent RightMouseEvent(
		0,
		0,
		FVector2D(48.0f, 48.0f),
		FVector2D(48.0f, 48.0f),
		PressedButtons,
		EKeys::RightMouseButton,
		0.0f,
		FModifierKeysState());

	TestFalse(
		TEXT("left mouse down remains unhandled for SGraphEditor drag"),
		Surface->OnPreviewMouseButtonDown(Geometry, LeftMouseEvent).IsEventHandled());
	TestEqual(TEXT("begin forwarded once"), BeginCount, 1);
	TestEqual(TEXT("end not forwarded before mouse up"), EndCount, 0);

	TestFalse(
		TEXT("right mouse up remains unhandled"),
		Surface->OnMouseButtonUp(Geometry, RightMouseEvent).IsEventHandled());
	TestEqual(TEXT("right mouse up does not finish left drag"), EndCount, 0);

	TestFalse(
		TEXT("left mouse up remains unhandled for SGraphEditor"),
		Surface->OnMouseButtonUp(Geometry, LeftMouseEvent).IsEventHandled());
	TestEqual(TEXT("end forwarded once"), EndCount, 1);

	Surface->OnFocusLost(FFocusEvent(EFocusCause::SetDirectly, 0));
	TestEqual(TEXT("focus lost after finished interaction is idempotent"), EndCount, 1);

	Surface->OnPreviewMouseButtonDown(Geometry, LeftMouseEvent);
	Surface->OnFocusLost(FFocusEvent(EFocusCause::SetDirectly, 0));
	TestEqual(TEXT("begin forwarded for second interaction"), BeginCount, 2);
	TestEqual(TEXT("focus lost finalizes open interaction"), EndCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerUsesOneStepFallbackForInvalidBudget,
	"BlueprintHelper.GraphLayout.Preview.MaterializerUsesOneStepFallbackForInvalidBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerUsesOneStepFallbackForInvalidBudget::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("preview materializer budget test runs on the game thread"), IsInGameThread());

	const FGraphLayoutPreviewSample Sample = MakeMaterializerTestSample();
	const FLayoutPlan Plan = MakeMaterializerTestPlan();

	FGraphLayoutPreviewMaterializer Materializer;
	Materializer.Begin(Sample, Plan);

	TestTrue(TEXT("first invalid-budget tick leaves more work"), Materializer.Tick(-1.0f));
	const FGraphLayoutPreviewMaterializerResult& FirstTickResult = Materializer.GetResult();
	TestTrue(TEXT("preview graph exists after first tick"), FirstTickResult.PreviewGraph.IsValid());
	if (!FirstTickResult.PreviewGraph.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("invalid budget materializes only one node on first tick"), FirstTickResult.PreviewGraph->Nodes.Num(), 1);
	TestFalse(TEXT("materializer is not complete after one invalid-budget tick"), Materializer.IsComplete());

	TestTrue(TEXT("second invalid-budget tick leaves more work"), Materializer.Tick(-1.0f));
	TestEqual(TEXT("invalid budget materializes one additional node per tick"), FirstTickResult.PreviewGraph->Nodes.Num(), 2);
	return true;
}

#endif
