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
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutApplyScheduler.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutGroupAvoidancePolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewInteractionCommitCoordinator.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewInteractionModel.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewOverlayProjector.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSolverInput.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutQualityGate.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"
#include "UI/Layout/SBlueprintHelperLayoutPreviewInteractionSurface.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "Input/Events.h"
#include "UObject/Package.h"
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

static bool PlacementPositionsNearlyEqual(
	const FNodePlacement* Left,
	const FNodePlacement* Right,
	const float Tolerance = 0.5f)
{
	return Left &&
		Right &&
		FMath::Abs(Left->TargetPosition.X - Right->TargetPosition.X) <= Tolerance &&
		FMath::Abs(Left->TargetPosition.Y - Right->TargetPosition.Y) <= Tolerance;
}

static bool RectsOverlap(const FNodePlacement& A, const FVector2D& ASize, const FNodePlacement& B, const FVector2D& BSize)
{
	const FVector2D AMin = A.TargetPosition;
	const FVector2D AMax = A.TargetPosition + ASize;
	const FVector2D BMin = B.TargetPosition;
	const FVector2D BMax = B.TargetPosition + BSize;
	return AMin.X < BMax.X && AMax.X > BMin.X && AMin.Y < BMax.Y && AMax.Y > BMin.Y;
}

static float ComputeOverlapRatioForTest(
	const FVector2D& APosition,
	const FVector2D& ASize,
	const FVector2D& BPosition,
	const FVector2D& BSize)
{
	const float Left = FMath::Max(APosition.X, BPosition.X);
	const float Right = FMath::Min(APosition.X + ASize.X, BPosition.X + BSize.X);
	const float Top = FMath::Max(APosition.Y, BPosition.Y);
	const float Bottom = FMath::Min(APosition.Y + ASize.Y, BPosition.Y + BSize.Y);
	if (Right <= Left || Bottom <= Top)
	{
		return 0.0f;
	}

	const float OverlapArea = (Right - Left) * (Bottom - Top);
	const float MinArea = FMath::Max(1.0f, FMath::Min(ASize.X * ASize.Y, BSize.X * BSize.Y));
	return OverlapArea / MinArea;
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

static UEdGraphNode_Comment* FindCommentNodeByComment(UEdGraph* Graph, const FString& CommentText)
{
	if (!Graph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node);
		if (CommentNode && CommentNode->NodeComment.Contains(CommentText))
		{
			return CommentNode;
		}
	}
	return nullptr;
}

static UEdGraphNode_Comment* FindCommentNodeContaining(UEdGraph* Graph, const FString& CommentText)
{
	if (!Graph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node);
		if (CommentNode && CommentNode->NodeComment.Contains(CommentText))
		{
			return CommentNode;
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

static void AssignLayoutBlock(
	FNodeSnapshot& Node,
	const FString& LayoutBlockId,
	const int32 LayoutBlockOrder,
	const int32 LayoutNodeOrder)
{
	Node.LayoutBlockId = LayoutBlockId;
	Node.LayoutBlockOrder = LayoutBlockOrder;
	Node.LayoutNodeOrder = LayoutNodeOrder;
}

static FGraphSnapshot BuildSetClampMinusMultiplyLeafFixture()
{
	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	const FString LayoutBlockId = TEXT("StaminaFunctionEvent");
	int32 NodeOrder = 0;

	FNodeSnapshot Event = MakeNode(
		TEXT("EventStart"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event Start"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("SetStamina")})});
	AssignLayoutBlock(Event, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(Event);

	FNodeSnapshot SetStamina = MakeNode(
		TEXT("SetStamina"),
		TEXT("K2Node_VariableSet"),
		TEXT("Set Current Stamina"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("EventStart")}),
			MakePin(TEXT("Then"), EPinDirection::Output, true),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("ClampFloat")})
		});
	AssignLayoutBlock(SetStamina, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(SetStamina);

	FNodeSnapshot ClampFloat = MakeNode(
		TEXT("ClampFloat"),
		TEXT("K2Node_CallFunction"),
		TEXT("Clamp (Float)"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 120.0f),
		false,
		{
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("Subtract")}),
			MakePin(TEXT("Min"), EPinDirection::Input, false),
			MakePin(TEXT("Max"), EPinDirection::Input, false, {TEXT("MaxStamina")}),
			MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("SetStamina")})
		});
	AssignLayoutBlock(ClampFloat, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(ClampFloat);

	FNodeSnapshot Subtract = MakeNode(
		TEXT("Subtract"),
		TEXT("K2Node_CommutativeAssociativeBinaryOperator"),
		TEXT("-"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 90.0f),
		false,
		{
			MakePin(TEXT("A"), EPinDirection::Input, false, {TEXT("CurrentStamina")}),
			MakePin(TEXT("B"), EPinDirection::Input, false, {TEXT("Multiply")}),
			MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("ClampFloat")})
		});
	AssignLayoutBlock(Subtract, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(Subtract);

	FNodeSnapshot Multiply = MakeNode(
		TEXT("Multiply"),
		TEXT("K2Node_CommutativeAssociativeBinaryOperator"),
		TEXT("*"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 90.0f),
		false,
		{
			MakePin(TEXT("A"), EPinDirection::Input, false, {TEXT("StaminaDrainPerSecond")}),
			MakePin(TEXT("B"), EPinDirection::Input, false, {TEXT("WorldDeltaSeconds")}),
			MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Subtract")})
		});
	AssignLayoutBlock(Multiply, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(Multiply);

	FNodeSnapshot CurrentStamina = MakeNode(
		TEXT("CurrentStamina"),
		TEXT("K2Node_VariableGet"),
		TEXT("Current Stamina"),
		FVector2D::ZeroVector,
		FVector2D(170.0f, 60.0f),
		false,
		{MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("Subtract")})});
	AssignLayoutBlock(CurrentStamina, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(CurrentStamina);

	FNodeSnapshot StaminaDrain = MakeNode(
		TEXT("StaminaDrainPerSecond"),
		TEXT("K2Node_VariableGet"),
		TEXT("Stamina Drain Per Second"),
		FVector2D::ZeroVector,
		FVector2D(210.0f, 60.0f),
		false,
		{MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("Multiply")})});
	AssignLayoutBlock(StaminaDrain, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(StaminaDrain);

	FNodeSnapshot DeltaSeconds = MakeNode(
		TEXT("WorldDeltaSeconds"),
		TEXT("K2Node_CallFunction"),
		TEXT("Get World Delta Seconds"),
		FVector2D::ZeroVector,
		FVector2D(210.0f, 60.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Multiply")})});
	AssignLayoutBlock(DeltaSeconds, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(DeltaSeconds);

	FNodeSnapshot MaxStamina = MakeNode(
		TEXT("MaxStamina"),
		TEXT("K2Node_VariableGet"),
		TEXT("Max Stamina"),
		FVector2D::ZeroVector,
		FVector2D(170.0f, 60.0f),
		false,
		{MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("ClampFloat")})});
	AssignLayoutBlock(MaxStamina, LayoutBlockId, 0, NodeOrder++);
	Snapshot.Nodes.Add(MaxStamina);

	return Snapshot;
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

static FString GetCoordinatorTestNodeLayoutId(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}
	return Node->NodeGuid.IsValid()
		? Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens)
		: Node->GetName();
}

static FNodePlacement MakeSchedulerPlacement(
	const UEdGraphNode* Node,
	const FVector2D& TargetPosition)
{
	FNodePlacement Placement;
	Placement.NodeId = GetCoordinatorTestNodeLayoutId(Node);
	Placement.Role = ENodeRole::ExecNode;
	Placement.CurrentPosition = Node ? FVector2D(Node->NodePosX, Node->NodePosY) : FVector2D::ZeroVector;
	Placement.TargetPosition = TargetPosition;
	Placement.TargetSize = FVector2D(220.0f, 100.0f);
	Placement.bMoveExisting = true;
	Placement.Reason = TEXT("test_scheduler");
	return Placement;
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

static FLayoutPlan BuildSolverPreviewPlanForTest(
	FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet)
{
	FLayoutPlan Plan = FSolver::Solve(FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(Sample), RuleSet);
	FGraphLayoutPreviewOverlayProjector::AppendOverlays(Sample, RuleSet, Plan);
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
	FBlueprintHelperGraphLayout_CoordinatorFlushSchedulesApply,
	"BlueprintHelper.GraphLayout.Coordinator.FlushSchedulesApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_CoordinatorFlushSchedulesApply::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutApplyScheduler::ResetForTests();
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
	FBlueprintHelperVersionCompat::MakePinLinkTo(EntryThenPin, ExecInputPin, true);

	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Graph, {EntryNode, ExecNode});
	const bool bFlushSucceeded = FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts();
	const int32 PendingAfterFlush = FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests();
	while (FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests() > 0)
	{
		FGraphLayoutApplyScheduler::Tick(0.0f);
	}

	const bool bExecMovedRight = ExecNode->NodePosX > EntryNode->NodePosX + 100;
	FBlueprintHelperGraphLayoutCoordinator::Shutdown();
	FGraphLayoutApplyScheduler::ResetForTests();

	TestTrue(TEXT("FlushPendingTaskLayouts reports success after scheduling"), bFlushSucceeded);
	TestTrue(TEXT("FlushPendingTaskLayouts enqueues scheduler work"), PendingAfterFlush > 0);
	TestTrue(TEXT("scheduler applies generated-node layout when ticked"), bExecMovedRight);
	return bFlushSucceeded && PendingAfterFlush > 0 && bExecMovedRight;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_OffThreadRecordThenFlushSchedulesApply,
	"BlueprintHelper.GraphLayout.Coordinator.OffThreadRecordThenFlushSchedulesApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OffThreadRecordThenFlushSchedulesApply::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutApplyScheduler::ResetForTests();
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
	FBlueprintHelperVersionCompat::MakePinLinkTo(EntryThenPin, ExecInputPin, true);

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
	const int32 PendingAfterFlush = FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests();
	while (FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests() > 0)
	{
		FGraphLayoutApplyScheduler::Tick(0.0f);
	}
	const bool bExecMovedRight = ExecNode->NodePosX > EntryNode->NodePosX + 100;
	FBlueprintHelperGraphLayoutCoordinator::Shutdown();
	FGraphLayoutApplyScheduler::ResetForTests();

	TestTrue(TEXT("Off-thread record followed by flush reports success"), bFlushSucceeded);
	TestTrue(TEXT("Off-thread flush enqueues scheduler work"), PendingAfterFlush > 0);
	TestTrue(TEXT("Off-thread scheduled layout applies when ticked"), bExecMovedRight);
	return bFlushSucceeded && PendingAfterFlush > 0 && bExecMovedRight;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ApplySchedulerRespectsMaxNodesPerFrame,
	"BlueprintHelper.GraphLayout.ApplyScheduler.RespectsMaxNodesPerFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ApplySchedulerRespectsMaxNodesPerFrame::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutApplyScheduler::ResetForTests();
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_ApplySchedulerBudgetGraph")));
	UEdGraphNode* First = AddCoordinatorTestNode(Graph, FName(TEXT("Node_First")), 0, 0, false, false);
	UEdGraphNode* Second = AddCoordinatorTestNode(Graph, FName(TEXT("Node_Second")), 0, 100, false, false);
	UEdGraphNode* Third = AddCoordinatorTestNode(Graph, FName(TEXT("Node_Third")), 0, 200, false, false);

	FLayoutPlan Plan;
	Plan.Placements.Add(MakeSchedulerPlacement(First, FVector2D(100.0f, 0.0f)));
	Plan.Placements.Add(MakeSchedulerPlacement(Second, FVector2D(100.0f, 100.0f)));
	Plan.Placements.Add(MakeSchedulerPlacement(Third, FVector2D(100.0f, 200.0f)));

	FRuleSet RuleSet;
	RuleSet.MaxNodesPerFrame = 1;
	RuleSet.MaxMillisecondsPerFrame = 100.0f;
	FGraphLayoutApplyScheduler::Enqueue(Graph, Plan, RuleSet);
	TestEqual(TEXT("all placements pending before tick"), FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests(), 3);

	FGraphLayoutApplyScheduler::Tick(0.0f);
	TestEqual(TEXT("one placement consumed by first tick"), FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests(), 2);
	TestEqual(TEXT("first node moved"), First->NodePosX, 100);
	TestEqual(TEXT("second node waits for later frame"), Second->NodePosX, 0);

	FGraphLayoutApplyScheduler::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ApplySchedulerFinishesAndNotifiesGraph,
	"BlueprintHelper.GraphLayout.ApplyScheduler.FinishesAndNotifiesGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ApplySchedulerFinishesAndNotifiesGraph::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutApplyScheduler::ResetForTests();
	UEdGraph* Graph = NewObject<UEdGraph>(GetTransientPackage(), FName(TEXT("BH_ApplySchedulerNotifyGraph")));
	UEdGraphNode* First = AddCoordinatorTestNode(Graph, FName(TEXT("Notify_First")), 0, 0, false, false);
	UEdGraphNode* Second = AddCoordinatorTestNode(Graph, FName(TEXT("Notify_Second")), 0, 100, false, false);

	int32 NotifyCount = 0;
	const FDelegateHandle GraphChangedHandle = Graph->AddOnGraphChangedHandler(
		FOnGraphChanged::FDelegate::CreateLambda([&NotifyCount](const FEdGraphEditAction&)
		{
			++NotifyCount;
		}));

	FLayoutPlan Plan;
	Plan.Placements.Add(MakeSchedulerPlacement(First, FVector2D(120.0f, 0.0f)));
	Plan.Placements.Add(MakeSchedulerPlacement(Second, FVector2D(120.0f, 100.0f)));

	FRuleSet RuleSet;
	RuleSet.MaxNodesPerFrame = 8;
	RuleSet.MaxMillisecondsPerFrame = 100.0f;
	FGraphLayoutApplyScheduler::Enqueue(Graph, Plan, RuleSet);
	while (FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests() > 0)
	{
		FGraphLayoutApplyScheduler::Tick(0.0f);
	}

	Graph->RemoveOnGraphChangedHandler(GraphChangedHandle);
	TestEqual(TEXT("scheduler drained all placements"), FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests(), 0);
	TestEqual(TEXT("first node moved"), First->NodePosX, 120);
	TestEqual(TEXT("second node moved"), Second->NodePosX, 120);
	TestEqual(TEXT("graph changed once after final placement"), NotifyCount, 1);
	FGraphLayoutApplyScheduler::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_ApplySchedulerSaveAfterApplyDefersToTaskRuntime,
	"BlueprintHelper.GraphLayout.ApplyScheduler.SaveAfterApplyDefersToTaskRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_ApplySchedulerSaveAfterApplyDefersToTaskRuntime::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutApplyScheduler::ResetForTests();
	UPackage* Package = CreatePackage(TEXT("/Temp/BH_ApplySchedulerSaveAfterApply"));
	UEdGraph* Graph = NewObject<UEdGraph>(Package, FName(TEXT("BH_ApplySchedulerSaveGraph")));
	UEdGraphNode* Node = AddCoordinatorTestNode(Graph, FName(TEXT("SaveAfter_Node")), 0, 0, false, false);

	FLayoutPlan Plan;
	Plan.Placements.Add(MakeSchedulerPlacement(Node, FVector2D(180.0f, 0.0f)));

	FRuleSet RuleSet;
	RuleSet.bSaveAfterApply = true;
	RuleSet.bMarkDirtyAfterApply = true;
	RuleSet.MaxNodesPerFrame = 8;
	RuleSet.MaxMillisecondsPerFrame = 100.0f;
	FGraphLayoutApplyScheduler::Enqueue(Graph, Plan, RuleSet);
	while (FGraphLayoutApplyScheduler::GetPendingPlacementCountForTests() > 0)
	{
		FGraphLayoutApplyScheduler::Tick(0.0f);
	}

	TestEqual(TEXT("node moved despite save_after_apply"), Node->NodePosX, 180);
	TestTrue(TEXT("package marked dirty for TaskRuntime post-operation save"), Package->IsDirty());
	FGraphLayoutApplyScheduler::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverUsesArrangeScopeForGeneratedNodes,
	"BlueprintHelper.GraphLayout.Solver.UsesArrangeScopeForGeneratedNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverUsesArrangeScopeForGeneratedNodes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("ArrangeScopeSolver");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("GeneratedEvent"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Generated Event"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("GeneratedExec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("GeneratedExec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Generated Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("GeneratedEvent")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingBlocker"),
		TEXT("K2Node_CallFunction"),
		TEXT("Existing Blocker"),
		FVector2D(0.0f, 500.0f),
		FVector2D(220.0f, 90.0f),
		true,
		{}));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* GeneratedExec = FindPlacement(Plan, TEXT("GeneratedExec"));
	const FNodePlacement* ExistingBlocker = FindPlacement(Plan, TEXT("ExistingBlocker"));
	TestNotNull(TEXT("generated exec placement exists"), GeneratedExec);
	TestNotNull(TEXT("existing blocker placement exists"), ExistingBlocker);
	if (!GeneratedExec || !ExistingBlocker)
	{
		return false;
	}

	TestTrue(TEXT("generated node is movable by arrange scope"), GeneratedExec->bMoveExisting);
	TestFalse(TEXT("unlinked existing node is fixed obstacle"), ExistingBlocker->bMoveExisting);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverExistingNodesActAsAnchorsAndObstacles,
	"BlueprintHelper.GraphLayout.Solver.ExistingNodesActAsAnchorsAndObstacles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverExistingNodesActAsAnchorsAndObstacles::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("ExistingAnchorObstacleSolver");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("GeneratedEvent"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Generated Event"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ExistingConsumer")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingConsumer"),
		TEXT("K2Node_CallFunction"),
		TEXT("Existing Consumer"),
		FVector2D(360.0f, 0.0f),
		FVector2D(220.0f, 90.0f),
		true,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("GeneratedEvent")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingObstacle"),
		TEXT("K2Node_CallFunction"),
		TEXT("Existing Obstacle"),
		FVector2D(0.0f, 0.0f),
		FVector2D(220.0f, 90.0f),
		true,
		{}));

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 120.0f;
	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* GeneratedEvent = FindPlacement(Plan, TEXT("GeneratedEvent"));
	const FNodePlacement* ExistingConsumer = FindPlacement(Plan, TEXT("ExistingConsumer"));
	const FNodePlacement* ExistingObstacle = FindPlacement(Plan, TEXT("ExistingObstacle"));
	TestNotNull(TEXT("generated event placement exists"), GeneratedEvent);
	TestNotNull(TEXT("existing consumer placement exists"), ExistingConsumer);
	TestNotNull(TEXT("existing obstacle placement exists"), ExistingObstacle);
	if (!GeneratedEvent || !ExistingConsumer || !ExistingObstacle)
	{
		return false;
	}

	TestFalse(TEXT("existing connected consumer stays anchor"), ExistingConsumer->bMoveExisting);
	TestFalse(TEXT("existing unlinked obstacle stays fixed"), ExistingObstacle->bMoveExisting);
	TestEqual(TEXT("existing consumer keeps x"), ExistingConsumer->TargetPosition.X, 360.0);
	TestEqual(TEXT("existing obstacle keeps x"), ExistingObstacle->TargetPosition.X, 0.0);
	TestTrue(TEXT("generated event avoids existing obstacle"), GeneratedEvent->TargetPosition.X > 0.0f || GeneratedEvent->TargetPosition.Y > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverMultipleGeneratedEventsDoNotOverlap,
	"BlueprintHelper.GraphLayout.Solver.MultipleGeneratedEventsDoNotOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverMultipleGeneratedEventsDoNotOverlap::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("MultipleEventsSolver");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("EventA"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event A"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("EventB"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event B"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{}));

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.OverlapToleranceRatio = 0.0f;
	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* EventA = FindPlacement(Plan, TEXT("EventA"));
	const FNodePlacement* EventB = FindPlacement(Plan, TEXT("EventB"));
	TestNotNull(TEXT("event A placement exists"), EventA);
	TestNotNull(TEXT("event B placement exists"), EventB);
	if (!EventA || !EventB)
	{
		return false;
	}

	const float Ratio = ComputeOverlapRatioForTest(
		EventA->TargetPosition,
		FVector2D(220.0f, 90.0f),
		EventB->TargetPosition,
		FVector2D(220.0f, 90.0f));
	TestEqual(TEXT("generated custom events do not overlap"), Ratio, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverExecCollisionMovesRightThenDown,
	"BlueprintHelper.GraphLayout.Solver.ExecCollisionMovesRightThenDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverExecCollisionMovesRightThenDown::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("ExecCollisionSolver");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("GeneratedExec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("GeneratedExec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Generated Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")})}));

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 160.0f;
	RuleSet.MaxCollisionAttempts = 4;
	const FLayoutPlan BaselinePlan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* BaselineExec = FindPlacement(BaselinePlan, TEXT("GeneratedExec"));
	TestNotNull(TEXT("baseline exec placement exists"), BaselineExec);
	if (!BaselineExec)
	{
		return false;
	}

	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingBlocker"),
		TEXT("K2Node_CallFunction"),
		TEXT("Existing Blocker"),
		BaselineExec->TargetPosition,
		FVector2D(220.0f, 90.0f),
		true,
		{}));

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* GeneratedExec = FindPlacement(Plan, TEXT("GeneratedExec"));
	TestNotNull(TEXT("generated exec placement exists"), GeneratedExec);
	if (!GeneratedExec)
	{
		return false;
	}
	TestTrue(TEXT("low-priority exec moves right before down"), GeneratedExec->TargetPosition.X > BaselineExec->TargetPosition.X);
	TestEqual(TEXT("exec keeps row when right candidate is available"), GeneratedExec->TargetPosition.Y, BaselineExec->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverDataCollisionMovesLeftThenDown,
	"BlueprintHelper.GraphLayout.Solver.DataCollisionMovesLeftThenDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverDataCollisionMovesLeftThenDown::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("DataCollisionSolver");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Consumer")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Consumer"),
		TEXT("K2Node_CallFunction"),
		TEXT("Consumer"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("DataLeaf")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("DataLeaf"),
		TEXT("K2Node_VariableGet"),
		TEXT("Data Leaf"),
		FVector2D::ZeroVector,
		FVector2D(160.0f, 60.0f),
		false,
		{MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("Consumer")})}));

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 160.0f;
	RuleSet.MaxCollisionAttempts = 4;
	RuleSet.bUseTargetPinOrderForVariableInputs = true;
	const FLayoutPlan BaselinePlan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* BaselineDataLeaf = FindPlacement(BaselinePlan, TEXT("DataLeaf"));
	TestNotNull(TEXT("baseline data leaf placement exists"), BaselineDataLeaf);
	if (!BaselineDataLeaf)
	{
		return false;
	}

	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingBlocker"),
		TEXT("K2Node_VariableGet"),
		TEXT("Existing Blocker"),
		BaselineDataLeaf->TargetPosition,
		FVector2D(160.0f, 60.0f),
		true,
		{}));

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* DataLeaf = FindPlacement(Plan, TEXT("DataLeaf"));
	TestNotNull(TEXT("data leaf placement exists"), DataLeaf);
	if (!DataLeaf)
	{
		return false;
	}

	TestTrue(TEXT("low-priority data moves left before down"), DataLeaf->TargetPosition.X < BaselineDataLeaf->TargetPosition.X);
	TestEqual(TEXT("data keeps row when left candidate is available"), DataLeaf->TargetPosition.Y, BaselineDataLeaf->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_QualityGateReportsOverlapBeyondTolerance,
	"BlueprintHelper.GraphLayout.QualityGate.ReportsOverlapBeyondTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_QualityGateReportsOverlapBeyondTolerance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("A"), TEXT("K2Node_CallFunction"), TEXT("A"), FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), false, {}));
	Snapshot.Nodes.Add(MakeNode(TEXT("B"), TEXT("K2Node_CallFunction"), TEXT("B"), FVector2D(20.0f, 0.0f), FVector2D(100.0f, 100.0f), false, {}));

	FLayoutPlan Plan;
	Plan.Placements.Add({TEXT("A"), ENodeRole::ExecNode, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), true, TEXT("test")});
	Plan.Placements.Add({TEXT("B"), ENodeRole::ExecNode, FVector2D(20.0f, 0.0f), FVector2D(20.0f, 0.0f), FVector2D(100.0f, 100.0f), true, TEXT("test")});
	FRuleSet RuleSet;
	RuleSet.OverlapToleranceRatio = 0.1f;
	const FGraphLayoutQualityResult Result = FGraphLayoutQualityGate::Evaluate(Snapshot, Plan, RuleSet);
	TestTrue(TEXT("overlap beyond tolerance reported"), Result.HasBlockingIssues());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_QualityGateAcceptsOverlapWithinTolerance,
	"BlueprintHelper.GraphLayout.QualityGate.AcceptsOverlapWithinTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_QualityGateAcceptsOverlapWithinTolerance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("A"), TEXT("K2Node_CallFunction"), TEXT("A"), FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), false, {}));
	Snapshot.Nodes.Add(MakeNode(TEXT("B"), TEXT("K2Node_CallFunction"), TEXT("B"), FVector2D(60.0f, 0.0f), FVector2D(100.0f, 100.0f), false, {}));

	FLayoutPlan Plan;
	Plan.Placements.Add({TEXT("A"), ENodeRole::ExecNode, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), true, TEXT("test")});
	Plan.Placements.Add({TEXT("B"), ENodeRole::ExecNode, FVector2D(60.0f, 0.0f), FVector2D(60.0f, 0.0f), FVector2D(100.0f, 100.0f), true, TEXT("test")});
	FRuleSet RuleSet;
	RuleSet.OverlapToleranceRatio = 0.5f;
	const FGraphLayoutQualityResult Result = FGraphLayoutQualityGate::Evaluate(Snapshot, Plan, RuleSet);
	TestFalse(TEXT("overlap within tolerance accepted"), Result.HasBlockingIssues());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_QualityGateReportsExistingNodeIntrusion,
	"BlueprintHelper.GraphLayout.QualityGate.ReportsExistingNodeIntrusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_QualityGateReportsExistingNodeIntrusion::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("Moved"), TEXT("K2Node_CallFunction"), TEXT("Moved"), FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), false, {}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Existing"), TEXT("K2Node_CallFunction"), TEXT("Existing"), FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), true, {}));

	FLayoutPlan Plan;
	Plan.Placements.Add({TEXT("Moved"), ENodeRole::ExecNode, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), true, TEXT("test")});
	Plan.Placements.Add({TEXT("Existing"), ENodeRole::ExecNode, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(100.0f, 100.0f), false, TEXT("anchor")});
	FRuleSet RuleSet;
	RuleSet.OverlapToleranceRatio = 0.0f;
	const FGraphLayoutQualityResult Result = FGraphLayoutQualityGate::Evaluate(Snapshot, Plan, RuleSet);
	TestTrue(TEXT("existing intrusion reported"), Result.HasBlockingIssues());
	TestEqual(TEXT("existing intrusion code"), Result.Issues[0].Code, FString(TEXT("existing_node_intrusion")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_QualityGateReportsReverseEdge,
	"BlueprintHelper.GraphLayout.QualityGate.ReportsReverseEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_QualityGateReportsReverseEdge::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Source"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Source"),
		FVector2D(400.0f, 0.0f),
		FVector2D(100.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Target")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Target"),
		TEXT("K2Node_CallFunction"),
		TEXT("Target"),
		FVector2D(0.0f, 0.0f),
		FVector2D(100.0f, 80.0f),
		false,
		{MakePin(TEXT("Execute"), EPinDirection::Input, true, {TEXT("Source")})}));

	FLayoutPlan Plan;
	Plan.Placements.Add({TEXT("Source"), ENodeRole::EventEntry, FVector2D(400.0f, 0.0f), FVector2D(400.0f, 0.0f), FVector2D(100.0f, 80.0f), true, TEXT("test")});
	Plan.Placements.Add({TEXT("Target"), ENodeRole::ExecNode, FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(100.0f, 80.0f), true, TEXT("test")});

	FRuleSet RuleSet;
	const FGraphLayoutQualityResult Result = FGraphLayoutQualityGate::Evaluate(Snapshot, Plan, RuleSet);
	TestTrue(TEXT("reverse edge reported"), Result.Issues.ContainsByPredicate([](const FGraphLayoutQualityIssue& Issue)
	{
		return Issue.Code == TEXT("reverse_edge");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_QualityGateReportsOverlongEdge,
	"BlueprintHelper.GraphLayout.QualityGate.ReportsOverlongEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_QualityGateReportsOverlongEdge::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Source"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Source"),
		FVector2D(0.0f, 0.0f),
		FVector2D(100.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Target")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Target"),
		TEXT("K2Node_CallFunction"),
		TEXT("Target"),
		FVector2D(2200.0f, 0.0f),
		FVector2D(100.0f, 80.0f),
		false,
		{MakePin(TEXT("Execute"), EPinDirection::Input, true, {TEXT("Source")})}));

	FLayoutPlan Plan;
	Plan.Placements.Add({TEXT("Source"), ENodeRole::EventEntry, FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(100.0f, 80.0f), true, TEXT("test")});
	Plan.Placements.Add({TEXT("Target"), ENodeRole::ExecNode, FVector2D(2200.0f, 0.0f), FVector2D(2200.0f, 0.0f), FVector2D(100.0f, 80.0f), true, TEXT("test")});

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 300.0f;
	const FGraphLayoutQualityResult Result = FGraphLayoutQualityGate::Evaluate(Snapshot, Plan, RuleSet);
	TestTrue(TEXT("overlong edge reported"), Result.Issues.ContainsByPredicate([](const FGraphLayoutQualityIssue& Issue)
	{
		return Issue.Code == TEXT("overlong_edge");
	}));
	return true;
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
	FBlueprintHelperGraphLayout_OccupancyIgnoresSameGroupForLocalPlacement,
	"BlueprintHelper.GraphLayout.Occupancy.IgnoresSameGroupForLocalPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OccupancyIgnoresSameGroupForLocalPlacement::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 80.0f;
	RuleSet.CollisionPaddingY = 80.0f;
	RuleSet.CollisionStepY = 100.0f;
	RuleSet.MaxCollisionAttempts = 4;

	FOccupancyResolver Occupancy(RuleSet);
	Occupancy.ReserveTarget(
		TEXT("SameGroupA"),
		FVector2D(0.0f, 0.0f),
		FVector2D(180.0f, 80.0f),
		true,
		TEXT("GroupA"));

	FResolveTargetRequest Request;
	Request.NodeId = TEXT("SameGroupB");
	Request.LayoutGroupId = TEXT("GroupA");
	Request.DesiredPosition = FVector2D(0.0f, 0.0f);
	Request.Size = FVector2D(180.0f, 80.0f);
	Request.SearchMode = EGraphLayoutCollisionSearchMode::PreferSameRow;
	Request.bIgnoreSameGroupReservations = true;

	const FVector2D Resolved = Occupancy.ResolveTarget(Request);
	TestEqual(TEXT("same group local placement keeps y"), static_cast<double>(Resolved.Y), 0.0);
	TestEqual(TEXT("same group local placement keeps x when ignored"), static_cast<double>(Resolved.X), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_OccupancyKeepsDifferentGroupBlocking,
	"BlueprintHelper.GraphLayout.Occupancy.KeepsDifferentGroupBlocking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OccupancyKeepsDifferentGroupBlocking::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 80.0f;
	RuleSet.CollisionPaddingY = 80.0f;
	RuleSet.CollisionStepY = 100.0f;
	RuleSet.MaxCollisionAttempts = 4;

	FOccupancyResolver Occupancy(RuleSet);
	Occupancy.ReserveTarget(
		TEXT("OtherGroup"),
		FVector2D(0.0f, 0.0f),
		FVector2D(180.0f, 80.0f),
		true,
		TEXT("GroupA"));

	FResolveTargetRequest Request;
	Request.NodeId = TEXT("Candidate");
	Request.LayoutGroupId = TEXT("GroupB");
	Request.DesiredPosition = FVector2D(0.0f, 0.0f);
	Request.Size = FVector2D(180.0f, 80.0f);
	Request.SearchMode = EGraphLayoutCollisionSearchMode::DownwardOnly;

	const FVector2D Resolved = Occupancy.ResolveTarget(Request);
	TestTrue(TEXT("different group moves down"), Resolved.Y > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_GroupAvoidancePolicyOffsetsOnlyWholeGroups,
	"BlueprintHelper.GraphLayout.GroupAvoidance.PolicyOffsetsOnlyWholeGroups",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_GroupAvoidancePolicyOffsetsOnlyWholeGroups::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	TArray<FGraphLayoutGroupNode> Nodes;
	FGraphLayoutGroupNode& Existing = Nodes.AddDefaulted_GetRef();
	Existing.NodeId = TEXT("ExistingBlocker");
	Existing.LayoutGroupId = TEXT("existing");
	Existing.LayoutGroupOrder = 0;
	Existing.NodeOrder = 0;
	Existing.TargetPosition = FVector2D(0.0f, 0.0f);
	Existing.Size = FVector2D(500.0f, 180.0f);
	Existing.bGenerated = false;

	FGraphLayoutGroupNode& Entry = Nodes.AddDefaulted_GetRef();
	Entry.NodeId = TEXT("Entry");
	Entry.LayoutGroupId = TEXT("GeneratedEvent");
	Entry.LayoutGroupOrder = 1;
	Entry.NodeOrder = 0;
	Entry.TargetPosition = FVector2D(0.0f, 0.0f);
	Entry.Size = FVector2D(180.0f, 80.0f);
	Entry.bGenerated = true;

	FGraphLayoutGroupNode& Exec = Nodes.AddDefaulted_GetRef();
	Exec.NodeId = TEXT("Exec");
	Exec.LayoutGroupId = TEXT("GeneratedEvent");
	Exec.LayoutGroupOrder = 1;
	Exec.NodeOrder = 1;
	Exec.TargetPosition = FVector2D(360.0f, 0.0f);
	Exec.Size = FVector2D(220.0f, 90.0f);
	Exec.bGenerated = true;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 160.0f;
	RuleSet.MaxCollisionAttempts = 4;

	const TArray<FGraphLayoutGroupOffset> Offsets =
		FGraphLayoutGroupAvoidancePolicy::ResolveGroupOffsets(Nodes, RuleSet);
	TestEqual(TEXT("one generated group offset"), Offsets.Num(), 1);
	if (Offsets.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("offset applies to generated group id"), Offsets[0].LayoutGroupId, FString(TEXT("GeneratedEvent")));
	TestTrue(TEXT("group offset moves downward"), Offsets[0].Offset.Y > 0.0f);
	TestTrue(TEXT("reason is group collision"), Offsets[0].Reason.Contains(TEXT("group_avoided_overlap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverPinnedExistingSameGroupDoesNotForceVerticalAvoidance,
	"BlueprintHelper.GraphLayout.Solver.PinnedExistingSameGroupDoesNotForceVerticalAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverPinnedExistingSameGroupDoesNotForceVerticalAvoidance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	const FString SharedLayoutBlockId = TEXT("SharedFunctionOrEvent");

	FNodeSnapshot ExistingComment = MakeNode(
		TEXT("ExistingComment"),
		TEXT("EdGraphNode_Comment"),
		TEXT("Existing Comment"),
		FVector2D(0.0f, 0.0f),
		FVector2D(520.0f, 180.0f),
		true,
		{});
	AssignLayoutBlock(ExistingComment, SharedLayoutBlockId, 0, 0);
	Snapshot.Nodes.Add(ExistingComment);

	FNodeSnapshot Event = MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})});
	AssignLayoutBlock(Event, SharedLayoutBlockId, 0, 1);
	Snapshot.Nodes.Add(Event);

	FNodeSnapshot Exec = MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")})});
	AssignLayoutBlock(Exec, SharedLayoutBlockId, 0, 2);
	Snapshot.Nodes.Add(Exec);

	FRuleSet RuleSet = MakeRuleSetWithScalarInputOffsets();
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;
	RuleSet.CollisionStepY = 180.0f;
	RuleSet.MaxCollisionAttempts = 4;
	RuleSet.bMoveExistingNodes = false;
	RuleSet.bUsePatternRowHeightBudget = false;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ExistingPlacement = FindPlacement(Plan, TEXT("ExistingComment"));
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("Event"));
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	TestNotNull(TEXT("existing placement exists"), ExistingPlacement);
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("exec placement exists"), ExecPlacement);
	if (!ExistingPlacement || !EventPlacement || !ExecPlacement)
	{
		return false;
	}

	TestFalse(TEXT("existing comment remains pinned"), ExistingPlacement->bMoveExisting);
	TestEqual(TEXT("existing comment keeps x"), ExistingPlacement->TargetPosition.X, 0.0);
	TestEqual(TEXT("existing comment keeps y"), ExistingPlacement->TargetPosition.Y, 0.0);
	TestFalse(TEXT("same explicit group does not trigger event group avoidance"), EventPlacement->Reason.Contains(TEXT("group_avoided_overlap")));
	TestFalse(TEXT("same explicit group does not trigger exec group avoidance"), ExecPlacement->Reason.Contains(TEXT("group_avoided_overlap")));
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
	FBlueprintHelperGraphLayout_OrdinaryRootColumnObstacleUsesPriorityCollision,
	"BlueprintHelper.GraphLayout.Solver.OrdinaryRootColumnObstacleUsesPriorityCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_OrdinaryRootColumnObstacleUsesPriorityCollision::RunTest(const FString& Parameters)
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

	TestTrue(TEXT("ordinary obstacle uses node priority horizontal avoidance"), EventPlacement->TargetPosition.X > 0.0f);
	TestTrue(TEXT("ordinary obstacle does not force vertical group avoidance"), EventPlacement->TargetPosition.Y < 500.0f);
	TestEqual(
		TEXT("single-output exec chain remains pin-baseline aligned after priority collision"),
		ExpectedExecBaselineY(*ExecPlacement, ENodeRole::ExecNode),
		ExpectedExecBaselineY(*EventPlacement, ENodeRole::EventEntry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RowReflowDoesNotPromoteUnscopedPinnedObstacle,
	"BlueprintHelper.GraphLayout.Solver.RowReflowDoesNotPromoteUnscopedPinnedObstacle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RowReflowDoesNotPromoteUnscopedPinnedObstacle::RunTest(const FString& Parameters)
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
	TestTrue(TEXT("unscoped pinned obstacle does not bump parent row during row reflow"), EventPlacement->TargetPosition.Y < PinnedBlockerPlacement->TargetPosition.Y);
	TestFalse(TEXT("event does not record cross-group avoidance"), EventPlacement->Reason.Contains(TEXT("group_avoided_overlap")));
	TestFalse(TEXT("exec does not record cross-group avoidance"), ExecPlacement->Reason.Contains(TEXT("group_avoided_overlap")));
	TestEqual(
		TEXT("single-output exec chain remains pin-baseline aligned without unscoped row bump"),
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
	FBlueprintHelperGraphLayout_DataPriorityDecaysPerDataLink,
	"BlueprintHelper.GraphLayout.Solver.DataPriorityDecaysPerDataLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataPriorityDecaysPerDataLink::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	const FGraphSnapshot Snapshot = BuildSetClampMinusMultiplyLeafFixture();
	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FNodeInputClusterBudget Budget =
		FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, TEXT("SetStamina"), FRuleSet());

	TestEqual(TEXT("Clamp direct depth"), Budget.DataDepthByNodeId.FindRef(TEXT("ClampFloat")), 1);
	TestEqual(TEXT("Minus depth"), Budget.DataDepthByNodeId.FindRef(TEXT("Subtract")), 2);
	TestEqual(TEXT("Multiply depth"), Budget.DataDepthByNodeId.FindRef(TEXT("Multiply")), 3);
	TestEqual(TEXT("CurrentStamina leaf depth"), Budget.DataDepthByNodeId.FindRef(TEXT("CurrentStamina")), 3);
	TestEqual(TEXT("MaxStamina leaf depth"), Budget.DataDepthByNodeId.FindRef(TEXT("MaxStamina")), 2);

	const float ClampPriority = Budget.LayoutPriorityByNodeId.FindRef(TEXT("ClampFloat"));
	const float MinusPriority = Budget.LayoutPriorityByNodeId.FindRef(TEXT("Subtract"));
	const float MultiplyPriority = Budget.LayoutPriorityByNodeId.FindRef(TEXT("Multiply"));
	const float DrainPriority = Budget.LayoutPriorityByNodeId.FindRef(TEXT("StaminaDrainPerSecond"));

	TestTrue(TEXT("minus priority decays by one link"), FMath::IsNearlyEqual(ClampPriority - MinusPriority, 0.1f));
	TestTrue(TEXT("multiply priority decays by second link"), FMath::IsNearlyEqual(MinusPriority - MultiplyPriority, 0.1f));
	TestTrue(TEXT("leaf priority decays from transform parent"), FMath::IsNearlyEqual(MultiplyPriority - DrainPriority, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DataTransformPrefersHorizontalAvoidance,
	"BlueprintHelper.GraphLayout.Solver.DataTransformPrefersHorizontalAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataTransformPrefersHorizontalAvoidance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	const FGraphSnapshot Snapshot = BuildSetClampMinusMultiplyLeafFixture();
	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 80.0f;
	RuleSet.CollisionPaddingY = 80.0f;
	RuleSet.CollisionStepY = 160.0f;
	RuleSet.InputPinRowSpacing = 44.0f;
	RuleSet.bUsePatternRowHeightBudget = true;
	RuleSet.bUsePureDataSubgraphLayout = true;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ClampPlacement = FindPlacement(Plan, TEXT("ClampFloat"));
	const FNodePlacement* MinusPlacement = FindPlacement(Plan, TEXT("Subtract"));
	const FNodePlacement* MultiplyPlacement = FindPlacement(Plan, TEXT("Multiply"));
	const FNodePlacement* MaxPlacement = FindPlacement(Plan, TEXT("MaxStamina"));

	TestNotNull(TEXT("clamp placement"), ClampPlacement);
	TestNotNull(TEXT("minus placement"), MinusPlacement);
	TestNotNull(TEXT("multiply placement"), MultiplyPlacement);
	TestNotNull(TEXT("max leaf placement"), MaxPlacement);
	if (!ClampPlacement || !MinusPlacement || !MultiplyPlacement || !MaxPlacement)
	{
		return false;
	}
	TestTrue(
		*FString::Printf(
			TEXT("minus transform stays left of clamp: minus=(%.2f, %.2f), clamp=(%.2f, %.2f)"),
			MinusPlacement->TargetPosition.X,
			MinusPlacement->TargetPosition.Y,
			ClampPlacement->TargetPosition.X,
			ClampPlacement->TargetPosition.Y),
		MinusPlacement->TargetPosition.X < ClampPlacement->TargetPosition.X);
	TestTrue(
		*FString::Printf(
			TEXT("multiply transform stays left of minus: multiply=(%.2f, %.2f), minus=(%.2f, %.2f)"),
			MultiplyPlacement->TargetPosition.X,
			MultiplyPlacement->TargetPosition.Y,
			MinusPlacement->TargetPosition.X,
			MinusPlacement->TargetPosition.Y),
		MultiplyPlacement->TargetPosition.X < MinusPlacement->TargetPosition.X);
	TestTrue(
		TEXT("multiply remains on data chain row or pin row, not collision-pushed far down"),
		FMath::Abs(MultiplyPlacement->TargetPosition.Y - MinusPlacement->TargetPosition.Y) <= RuleSet.InputPinRowSpacing * 2.0f);
	TestTrue(
		TEXT("max stamina leaf follows lower clamp pin"),
		MaxPlacement->TargetPosition.Y > ClampPlacement->TargetPosition.Y);
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
	FBlueprintHelperGraphLayout_GroupAvoidanceMovesWholeGeneratedGroup,
	"BlueprintHelper.GraphLayout.Solver.GroupAvoidanceMovesWholeGeneratedGroup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_GroupAvoidanceMovesWholeGeneratedGroup::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FNodeSnapshot Entry = MakeNode(
		TEXT("Entry"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Entry"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})});
	AssignLayoutBlock(Entry, TEXT("FunctionEventA"), 0, 0);

	FNodeSnapshot Exec = MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Entry")})});
	AssignLayoutBlock(Exec, TEXT("FunctionEventA"), 0, 1);

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("GroupAvoidance");
	Snapshot.Nodes.Add(Entry);
	Snapshot.Nodes.Add(Exec);
	FNodeSnapshot ExistingBlocker = MakeNode(
		TEXT("ExistingBlocker"),
		TEXT("EdGraphNode_Comment"),
		TEXT("Existing Blocker"),
		FVector2D(-40.0f, -120.0f),
		FVector2D(760.0f, 260.0f),
		true,
		{});
	AssignLayoutBlock(ExistingBlocker, TEXT("ExistingDomain"), 1, 0);
	Snapshot.Nodes.Add(ExistingBlocker);

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 360.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.CollisionPaddingX = 20.0f;
	RuleSet.CollisionPaddingY = 20.0f;
	RuleSet.CollisionStepY = 180.0f;
	RuleSet.MaxCollisionAttempts = 8;
	RuleSet.bAlignExecNodesHorizontally = true;
	RuleSet.bUsePatternRowHeightBudget = false;
	RuleSet.bMoveExistingNodes = false;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* EntryPlacement = FindPlacement(Plan, TEXT("Entry"));
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* BlockerPlacement = FindPlacement(Plan, TEXT("ExistingBlocker"));
	TestNotNull(TEXT("entry placement exists"), EntryPlacement);
	TestNotNull(TEXT("exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("blocker placement exists"), BlockerPlacement);
	if (!EntryPlacement || !ExecPlacement || !BlockerPlacement)
	{
		return false;
	}

	TestTrue(TEXT("entry moved by group avoidance"), EntryPlacement->Reason.Contains(TEXT("group_avoided_overlap")));
	TestTrue(TEXT("exec moved by group avoidance"), ExecPlacement->Reason.Contains(TEXT("group_avoided_overlap")));
	TestTrue(TEXT("entry group clears blocker"), EntryPlacement->TargetPosition.Y > BlockerPlacement->TargetPosition.Y);
	TestEqual(
		TEXT("whole group preserves exec pin baseline after group offset"),
		ExpectedExecBaselineY(*ExecPlacement, ENodeRole::ExecNode),
		ExpectedExecBaselineY(*EntryPlacement, ENodeRole::EventEntry));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsOverlapTolerance,
	"BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsOverlapTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsOverlapTolerance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.OverlapToleranceRatio = 0.25f;

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	TestTrue(TEXT("json contains overlap_tolerance_ratio"), Json.Contains(TEXT("\"overlap_tolerance_ratio\"")));

	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("json imports"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	TestEqual(TEXT("overlap tolerance round trips"), Parsed.OverlapToleranceRatio, 0.25f);

	const TSharedRef<FJsonObject> NestedJson = MakeShared<FJsonObject>();
	NestedJson->SetStringField(TEXT("schema"), RuleSetSchemaV1);
	const TSharedRef<FJsonObject> SolverJson = MakeShared<FJsonObject>();
	SolverJson->SetNumberField(TEXT("overlap_tolerance_ratio"), 0.4f);
	NestedJson->SetObjectField(TEXT("solver"), SolverJson);

	FRuleSet ParsedNested;
	FValidationResult NestedValidation;
	TestTrue(TEXT("nested solver json imports"), FRuleSetJson::Import(NestedJson, ParsedNested, NestedValidation));
	TestEqual(TEXT("nested overlap tolerance imports"), ParsedNested.OverlapToleranceRatio, 0.4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRejectsInvalidOverlapTolerance,
	"BlueprintHelper.GraphLayout.RuleSetJson.RejectsInvalidOverlapTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRejectsInvalidOverlapTolerance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	const FString Json = TEXT(R"({
		"schema": "BlueprintHelper.GraphLayoutRuleSet.v1",
		"overlap_tolerance_ratio": 0.75
	})");

	FRuleSet Parsed;
	FValidationResult Validation;
	TestFalse(TEXT("invalid tolerance rejected"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	TestTrue(TEXT("error names field"), Validation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("overlap_tolerance_ratio"));
	}));

	const FString NestedJson = TEXT(R"({
		"schema": "BlueprintHelper.GraphLayoutRuleSet.v1",
		"solver": {
			"overlap_tolerance_ratio": -0.1
		}
	})");

	FRuleSet ParsedNested;
	FValidationResult NestedValidation;
	TestFalse(TEXT("invalid nested tolerance rejected"), FRuleSetJson::ImportString(NestedJson, ParsedNested, NestedValidation));
	TestTrue(TEXT("nested error names field"), NestedValidation.Errors.ContainsByPredicate([](const FString& Error)
	{
		return Error.Contains(TEXT("overlap_tolerance_ratio"));
	}));
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_LayoutRuleEditorOverlapToleranceSyncsJson,
	"BlueprintHelper.GraphLayout.LayoutRuleEditor.OverlapToleranceSyncsJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_LayoutRuleEditorOverlapToleranceSyncsJson::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.OverlapToleranceRatio = 0.1f;

	const FString InputJson = FRuleSetJson::ExportString(RuleSet);
	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("input imports"), FRuleSetJson::ImportString(InputJson, Parsed, Validation));
	TestEqual(TEXT("initial overlap tolerance"), Parsed.OverlapToleranceRatio, 0.1f);

	Parsed.OverlapToleranceRatio = 0.5f;
	const FString OutputJson = FRuleSetJson::ExportString(Parsed);
	TestTrue(TEXT("output json contains overlap_tolerance_ratio"), OutputJson.Contains(TEXT("\"overlap_tolerance_ratio\"")));

	FRuleSet Reimported;
	FValidationResult ReimportValidation;
	TestTrue(TEXT("output imports"), FRuleSetJson::ImportString(OutputJson, Reimported, ReimportValidation));
	TestEqual(TEXT("edited overlap tolerance round trips"), Reimported.OverlapToleranceRatio, 0.5f);
	return true;
}

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

		auto ExpectOverlay = [this, &Sample, &ExpectNotAnchor](const FString& NodeId)
		{
			ExpectNotAnchor(NodeId);
			const FGraphLayoutPreviewNodeSpec* NodeSpec = FindPreviewNodeSpec(Sample, NodeId);
			if (!NodeSpec)
			{
				return;
			}
			TestTrue(FString::Printf(TEXT("%s is preview overlay"), *NodeId), NodeSpec->bPreviewOverlay);
		};

		ExpectOverlay(TEXT("HorizontalAvoidanceRange"));
		ExpectOverlay(TEXT("VerticalAvoidanceRange"));

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
	FBlueprintHelperGraphLayout_PreviewSampleFactoryAddsSemanticLabelComments,
	"BlueprintHelper.GraphLayout.Preview.SampleFactoryAddsSemanticLabelComments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSampleFactoryAddsSemanticLabelComments::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(
		TEXT("linear sample builds"),
		FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	const FGraphLayoutPreviewNodeSpec* EventLabel = FindPreviewNodeSpec(Sample, TEXT("SemanticLabel_EventStart"));
	const FGraphLayoutPreviewNodeSpec* ExecLabel = FindPreviewNodeSpec(Sample, TEXT("SemanticLabel_ResetState"));
	TestNotNull(TEXT("event semantic label exists"), EventLabel);
	TestNotNull(TEXT("exec semantic label exists"), ExecLabel);
	if (!EventLabel || !ExecLabel)
	{
		return false;
	}

	TestTrue(TEXT("event label is preview overlay"), EventLabel->bPreviewOverlay);
	TestTrue(TEXT("event label is semantic label"), EventLabel->bPreviewSemanticLabel);
	TestEqual(TEXT("event label target"), EventLabel->PreviewLabelTargetNodeId, FString(TEXT("EventStart")));
	TestTrue(TEXT("event label text contains Child"), EventLabel->Title.Contains(TEXT("Child:")));
	TestTrue(TEXT("event label text contains role id"), EventLabel->Title.Contains(TEXT("EventEntry")));
	TestTrue(
		TEXT("exec label text contains writeback path"),
		ExecLabel->Title.Contains(TEXT("editor_canvas.role_centers.ExecNode")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewDrawsEntryAvoidanceRangeComments,
	"BlueprintHelper.GraphLayout.Preview.DrawsEntryAvoidanceRangeComments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewDrawsEntryAvoidanceRangeComments::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	const FGraphLayoutPreviewNodeSpec* HorizontalSpec = FindPreviewNodeSpec(Sample, TEXT("HorizontalAvoidanceRange"));
	const FGraphLayoutPreviewNodeSpec* VerticalSpec = FindPreviewNodeSpec(Sample, TEXT("VerticalAvoidanceRange"));
	TestNotNull(TEXT("horizontal range spec"), HorizontalSpec);
	TestNotNull(TEXT("vertical range spec"), VerticalSpec);
	if (!HorizontalSpec || !VerticalSpec)
	{
		return false;
	}

	TestTrue(TEXT("horizontal range is preview overlay"), HorizontalSpec->bPreviewOverlay);
	TestTrue(TEXT("vertical range is preview overlay"), VerticalSpec->bPreviewOverlay);
	TestFalse(TEXT("horizontal range is not draggable anchor"), HorizontalSpec->bUsePreviewRoleAnchor);
	TestFalse(TEXT("vertical range is not draggable anchor"), VerticalSpec->bUsePreviewRoleAnchor);

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 120.0f;
	RuleSet.CollisionPaddingY = 40.0f;
	RuleSet.CollisionStepY = 80.0f;
	RuleSet.MaxCollisionAttempts = 5;

	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("EventStart"));
	const FNodePlacement* HorizontalPlacement = FindPlacement(Plan, TEXT("HorizontalAvoidanceRange"));
	const FNodePlacement* VerticalPlacement = FindPlacement(Plan, TEXT("VerticalAvoidanceRange"));
	TestNotNull(TEXT("event placement"), EventPlacement);
	TestNotNull(TEXT("horizontal range placement"), HorizontalPlacement);
	TestNotNull(TEXT("vertical range placement"), VerticalPlacement);
	if (!EventPlacement || !HorizontalPlacement || !VerticalPlacement)
	{
		return false;
	}

	const FVector2D EntrySize = FindPreviewNodeSize(Sample, TEXT("EventStart"));
	const float ExpectedHorizontalWidth =
		EntrySize.X + RuleSet.CollisionPaddingX * 2.0f +
		RuleSet.MaxCollisionAttempts * FMath::Max(EntrySize.X, RuleSet.CollisionPaddingX);
	const float ExpectedHorizontalHeight = EntrySize.Y + RuleSet.CollisionPaddingY * 2.0f;
	const float ExpectedVerticalWidth = EntrySize.X + RuleSet.CollisionPaddingX * 2.0f;
	const float ExpectedVerticalHeight =
		EntrySize.Y + RuleSet.CollisionPaddingY * 2.0f +
		RuleSet.MaxCollisionAttempts * RuleSet.CollisionStepY;

	TestEqual(
		TEXT("horizontal range width follows settings"),
		static_cast<double>(HorizontalPlacement->TargetSize.X),
		static_cast<double>(ExpectedHorizontalWidth));
	TestEqual(
		TEXT("horizontal range height follows settings"),
		static_cast<double>(HorizontalPlacement->TargetSize.Y),
		static_cast<double>(ExpectedHorizontalHeight));
	TestEqual(
		TEXT("vertical range width follows settings"),
		static_cast<double>(VerticalPlacement->TargetSize.X),
		static_cast<double>(ExpectedVerticalWidth));
	TestEqual(
		TEXT("vertical range height follows settings"),
		static_cast<double>(VerticalPlacement->TargetSize.Y),
		static_cast<double>(ExpectedVerticalHeight));
	TestTrue(TEXT("horizontal range is drawn above entry"), HorizontalPlacement->TargetPosition.Y < EventPlacement->TargetPosition.Y);
	TestTrue(TEXT("vertical range is drawn below entry"), VerticalPlacement->TargetPosition.Y > EventPlacement->TargetPosition.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewOverlayReflectsOverlapTolerance,
	"BlueprintHelper.GraphLayout.Preview.OverlayReflectsOverlapTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewOverlayReflectsOverlapTolerance::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FRuleSet RuleSet;
	RuleSet.OverlapToleranceRatio = 0.25f;
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
	TestNotNull(TEXT("horizontal overlay placement exists"), FindPlacement(Plan, TEXT("HorizontalAvoidanceRange")));
	TestNotNull(TEXT("vertical overlay placement exists"), FindPlacement(Plan, TEXT("VerticalAvoidanceRange")));

	const FGraphLayoutPreviewNodeSpec* HorizontalSpec = FindPreviewNodeSpec(Sample, TEXT("HorizontalAvoidanceRange"));
	const FGraphLayoutPreviewNodeSpec* VerticalSpec = FindPreviewNodeSpec(Sample, TEXT("VerticalAvoidanceRange"));
	TestNotNull(TEXT("horizontal overlay spec exists"), HorizontalSpec);
	TestNotNull(TEXT("vertical overlay spec exists"), VerticalSpec);
	if (!HorizontalSpec || !VerticalSpec)
	{
		return false;
	}

	TestTrue(TEXT("horizontal overlay title shows tolerance"), HorizontalSpec->Title.Contains(TEXT("0.25")));
	TestTrue(TEXT("vertical overlay title shows tolerance"), VerticalSpec->Title.Contains(TEXT("0.25")));
	TestTrue(TEXT("horizontal overlay title names overlap tolerance"), HorizontalSpec->Title.Contains(TEXT("重叠容忍度")));
	TestTrue(TEXT("vertical overlay title names overlap tolerance"), VerticalSpec->Title.Contains(TEXT("重叠容忍度")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerAppliesAvoidanceCommentColors,
	"BlueprintHelper.GraphLayout.Preview.MaterializerAppliesAvoidanceCommentColors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerAppliesAvoidanceCommentColors::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 100.0f;
	RuleSet.CollisionPaddingY = 50.0f;
	RuleSet.CollisionStepY = 70.0f;
	RuleSet.MaxCollisionAttempts = 3;

	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("preview materializes"), Materializer.MaterializeForTest(Sample, Plan, Result));

	UEdGraphNode_Comment* HorizontalComment = FindCommentNodeByComment(Result.PreviewGraph.Get(), TEXT("水平避让范围"));
	UEdGraphNode_Comment* VerticalComment = FindCommentNodeByComment(Result.PreviewGraph.Get(), TEXT("垂直避让范围"));
	TestNotNull(TEXT("horizontal comment"), HorizontalComment);
	TestNotNull(TEXT("vertical comment"), VerticalComment);
	if (!HorizontalComment || !VerticalComment)
	{
		return false;
	}

	const FGraphLayoutPreviewNodeSpec* HorizontalSpec = FindPreviewNodeSpec(Sample, TEXT("HorizontalAvoidanceRange"));
	const FGraphLayoutPreviewNodeSpec* VerticalSpec = FindPreviewNodeSpec(Sample, TEXT("VerticalAvoidanceRange"));
	const FNodePlacement* HorizontalPlacement = FindPlacement(Plan, TEXT("HorizontalAvoidanceRange"));
	const FNodePlacement* VerticalPlacement = FindPlacement(Plan, TEXT("VerticalAvoidanceRange"));
	TestNotNull(TEXT("horizontal spec"), HorizontalSpec);
	TestNotNull(TEXT("vertical spec"), VerticalSpec);
	TestNotNull(TEXT("horizontal placement"), HorizontalPlacement);
	TestNotNull(TEXT("vertical placement"), VerticalPlacement);
	if (!HorizontalSpec || !VerticalSpec || !HorizontalPlacement || !VerticalPlacement)
	{
		return false;
	}

	TestEqual(TEXT("horizontal comment color"), HorizontalComment->CommentColor, HorizontalSpec->CommentColor);
	TestEqual(TEXT("vertical comment color"), VerticalComment->CommentColor, VerticalSpec->CommentColor);
	TestEqual(
		TEXT("horizontal comment width"),
		static_cast<double>(HorizontalComment->NodeWidth),
		static_cast<double>(HorizontalPlacement->TargetSize.X));
	TestEqual(
		TEXT("vertical comment height"),
		static_cast<double>(VerticalComment->NodeHeight),
		static_cast<double>(VerticalPlacement->TargetSize.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerCreatesSemanticLabelComments,
	"BlueprintHelper.GraphLayout.Preview.MaterializerCreatesSemanticLabelComments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerCreatesSemanticLabelComments::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(
		TEXT("linear sample builds"),
		FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));
	FRuleSet RuleSet;
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("preview materializes"), Materializer.MaterializeForTest(Sample, Plan, Result));

	const FGuid* LabelGuid = Result.NodeGuidsById.Find(TEXT("SemanticLabel_EventStart"));
	TestNotNull(TEXT("label guid exists"), LabelGuid);
	if (!LabelGuid)
	{
		return false;
	}
	TestTrue(TEXT("semantic label is preview overlay"), Result.PreviewOverlayGuids.Contains(*LabelGuid));

	UEdGraphNode_Comment* LabelComment = FindCommentNodeContaining(Result.PreviewGraph.Get(), TEXT("Child: EventEntry"));
	TestNotNull(TEXT("semantic label comment exists"), LabelComment);
	if (!LabelComment)
	{
		return false;
	}
	TestTrue(TEXT("semantic label contains Chinese effect"), LabelComment->NodeComment.Contains(TEXT("作用:")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerSkipsOverlayWithoutPlacement,
	"BlueprintHelper.GraphLayout.Preview.MaterializerSkipsOverlayWithoutPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerSkipsOverlayWithoutPlacement::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(
		TEXT("linear sample builds"),
		FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FRuleSet RuleSet;
	FLayoutPlan Plan = FSolver::Solve(FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(Sample), RuleSet);
	Plan.Placements.RemoveAll([](const FNodePlacement& Placement)
	{
		return Placement.NodeId == TEXT("EventStart");
	});
	FGraphLayoutPreviewOverlayProjector::AppendOverlays(Sample, RuleSet, Plan);
	TestNull(TEXT("event label placement is absent without target placement"), FindPlacement(Plan, TEXT("SemanticLabel_EventStart")));

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("preview materializes"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestFalse(TEXT("missing-placement overlay is not materialized"), Result.NodeGuidsById.Contains(TEXT("SemanticLabel_EventStart")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerSkipsLinksToSkippedOverlays,
	"BlueprintHelper.GraphLayout.Preview.MaterializerSkipsLinksToSkippedOverlays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerSkipsLinksToSkippedOverlays::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(
		TEXT("linear sample builds"),
		FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FNodeSnapshot* EventSnapshot = Sample.Snapshot.Nodes.FindByPredicate([](const FNodeSnapshot& Node)
	{
		return Node.NodeId == TEXT("EventStart");
	});
	TestNotNull(TEXT("event snapshot exists"), EventSnapshot);
	if (!EventSnapshot)
	{
		return false;
	}

	FPinSnapshot OverlayLinkPin;
	OverlayLinkPin.PinId = TEXT("OverlayRef");
	OverlayLinkPin.Name = TEXT("OverlayRef");
	OverlayLinkPin.Direction = EPinDirection::Output;
	OverlayLinkPin.bExec = false;
	OverlayLinkPin.Category = TEXT("object");
	OverlayLinkPin.LinkedNodeIds.Add(TEXT("SemanticLabel_EventStart"));
	EventSnapshot->Pins.Add(OverlayLinkPin);

	FGraphLayoutPreviewLinkSpec OverlayLink;
	OverlayLink.FromNodeId = TEXT("EventStart");
	OverlayLink.FromPinName = TEXT("OverlayRef");
	OverlayLink.ToNodeId = TEXT("SemanticLabel_EventStart");
	OverlayLink.ToPinName = TEXT("IgnoredInput");
	OverlayLink.bExec = false;
	Sample.Links.Add(OverlayLink);

	FRuleSet RuleSet;
	FLayoutPlan Plan = FSolver::Solve(FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(Sample), RuleSet);
	Plan.Placements.RemoveAll([](const FNodePlacement& Placement)
	{
		return Placement.NodeId == TEXT("EventStart");
	});
	FGraphLayoutPreviewOverlayProjector::AppendOverlays(Sample, RuleSet, Plan);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("preview materializes despite skipped overlay link"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("no materializer error"), Result.Error.IsEmpty());
	TestFalse(TEXT("skipped overlay remains unmaterialized"), Result.NodeGuidsById.Contains(TEXT("SemanticLabel_EventStart")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerReportsMissingRealNodeBesideSkippedOverlay,
	"BlueprintHelper.GraphLayout.Preview.MaterializerReportsMissingRealNodeBesideSkippedOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerReportsMissingRealNodeBesideSkippedOverlay::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(
		TEXT("linear sample builds"),
		FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FGraphLayoutPreviewLinkSpec BadLink;
	BadLink.FromNodeId = TEXT("MissingRealNode");
	BadLink.FromPinName = TEXT("Out");
	BadLink.ToNodeId = TEXT("SemanticLabel_EventStart");
	BadLink.ToPinName = TEXT("IgnoredInput");
	BadLink.bExec = false;
	Sample.Links.Add(BadLink);

	FRuleSet RuleSet;
	FLayoutPlan Plan = FSolver::Solve(FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(Sample), RuleSet);
	Plan.Placements.RemoveAll([](const FNodePlacement& Placement)
	{
		return Placement.NodeId == TEXT("EventStart");
	});
	FGraphLayoutPreviewOverlayProjector::AppendOverlays(Sample, RuleSet, Plan);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestFalse(TEXT("missing real endpoint still fails"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("unknown real endpoint is reported"), Result.Error.Contains(TEXT("preview link references unknown node")));
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
	TestTrue(TEXT("layout diagnostics do not block preview plan"), Plan.Placements.Num() > 0);
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
	FBlueprintHelperGraphLayout_PreviewOverlayProjectorPlacesSemanticLabelsNearTargets,
	"BlueprintHelper.GraphLayout.Preview.OverlayProjectorPlacesSemanticLabelsNearTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewOverlayProjectorPlacesSemanticLabelsNearTargets::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(
		TEXT("linear sample builds"),
		FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
	const FNodePlacement* TargetPlacement = FindPlacement(Plan, TEXT("EventStart"));
	const FNodePlacement* LabelPlacement = FindPlacement(Plan, TEXT("SemanticLabel_EventStart"));
	TestNotNull(TEXT("target placement exists"), TargetPlacement);
	TestNotNull(TEXT("label placement exists"), LabelPlacement);
	if (!TargetPlacement || !LabelPlacement)
	{
		return false;
	}

	TestTrue(TEXT("label sits above target"), LabelPlacement->TargetPosition.Y < TargetPlacement->TargetPosition.Y);
	TestTrue(
		TEXT("label stays horizontally near target"),
		FMath::Abs(LabelPlacement->TargetPosition.X - TargetPlacement->TargetPosition.X) <= 48.0f);
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
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);

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
	FBlueprintHelperGraphLayout_PreviewSolverBaselineStraightensEntryToFirstExec,
	"BlueprintHelper.GraphLayout.Preview.SolverBaselineStraightensEntryToFirstExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSolverBaselineStraightensEntryToFirstExec::RunTest(const FString& Parameters)
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
	FBlueprintHelperGraphLayout_PreviewSolverBaselineStraightensLinearExecChain,
	"BlueprintHelper.GraphLayout.Preview.SolverBaselineStraightensLinearExecChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSolverBaselineStraightensLinearExecChain::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
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
	FBlueprintHelperGraphLayout_PreviewSolverBaselineStraightensDataSceneEntryToConsumer,
	"BlueprintHelper.GraphLayout.Preview.SolverBaselineStraightensDataSceneEntryToConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSolverBaselineStraightensDataSceneEntryToConsumer::RunTest(const FString& Parameters)
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
		const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
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
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);

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
	FBlueprintHelperGraphLayout_PreviewSolverBaselineKeepsOccupancyEntrySeparate,
	"BlueprintHelper.GraphLayout.Preview.SolverBaselineKeepsOccupancyEntrySeparate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSolverBaselineKeepsOccupancyEntrySeparate::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("occupancy sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::Occupancy, Sample, Error));

	FRuleSet RuleSet;
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
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
	FBlueprintHelperGraphLayout_PreviewSolverBaselineKeepsOccupancyFallbackClearOfBlocker,
	"BlueprintHelper.GraphLayout.Preview.SolverBaselineKeepsOccupancyFallbackClearOfBlocker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSolverBaselineKeepsOccupancyFallbackClearOfBlocker::RunTest(const FString& Parameters)
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

	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);
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
	FBlueprintHelperGraphLayout_PreviewServiceMatchesSolverBaselineForLinearExecRules,
	"BlueprintHelper.GraphLayout.Preview.ServiceMatchesSolverBaselineForLinearExecRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceMatchesSolverBaselineForLinearExecRules::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	auto BuildResultForRuleSet = [](const FRuleSet& RuleSet)
	{
		FGraphLayoutPreviewService Service;
		FGraphLayoutPreviewRequest Request;
		Request.Scene = ESemanticScene::LinearExecChain;
		Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

		FGraphLayoutPreviewBuildResult Result;
		Service.BuildPreviewDataForTest(Request, Result);
		return Result;
	};

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	FRuleSet FirstRuleSet;
	FirstRuleSet.ExecColumnSpacing = 360.0f;
	FirstRuleSet.ExecRowSpacing = 180.0f;
	FRuleSet SecondRuleSet = FirstRuleSet;
	SecondRuleSet.ExecColumnSpacing = 640.0f;
	SecondRuleSet.ExecRowSpacing = 260.0f;

	const FLayoutPlan FirstExpectedPlan = BuildSolverPreviewPlanForTest(Sample, FirstRuleSet);
	const FLayoutPlan SecondExpectedPlan = BuildSolverPreviewPlanForTest(Sample, SecondRuleSet);
	const FGraphLayoutPreviewBuildResult FirstResult = BuildResultForRuleSet(FirstRuleSet);
	const FGraphLayoutPreviewBuildResult SecondResult = BuildResultForRuleSet(SecondRuleSet);
	TestTrue(TEXT("first result succeeds"), FirstResult.bSuccess);
	TestTrue(TEXT("second result succeeds"), SecondResult.bSuccess);

	const FNodePlacement* FirstExpectedEventPlacement = FindPlacement(FirstExpectedPlan, TEXT("EventStart"));
	const FNodePlacement* FirstExpectedResetPlacement = FindPlacement(FirstExpectedPlan, TEXT("ResetState"));
	const FNodePlacement* SecondExpectedEventPlacement = FindPlacement(SecondExpectedPlan, TEXT("EventStart"));
	const FNodePlacement* SecondExpectedResetPlacement = FindPlacement(SecondExpectedPlan, TEXT("ResetState"));
	const FNodePlacement* FirstEventPlacement = FindPlacement(FirstResult.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* FirstResetPlacement = FindPlacement(FirstResult.LayoutPlan, TEXT("ResetState"));
	const FNodePlacement* SecondEventPlacement = FindPlacement(SecondResult.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* SecondResetPlacement = FindPlacement(SecondResult.LayoutPlan, TEXT("ResetState"));
	TestNotNull(TEXT("first expected event placement exists"), FirstExpectedEventPlacement);
	TestNotNull(TEXT("first expected reset placement exists"), FirstExpectedResetPlacement);
	TestNotNull(TEXT("second expected event placement exists"), SecondExpectedEventPlacement);
	TestNotNull(TEXT("second expected reset placement exists"), SecondExpectedResetPlacement);
	TestNotNull(TEXT("first event placement exists"), FirstEventPlacement);
	TestNotNull(TEXT("first reset placement exists"), FirstResetPlacement);
	TestNotNull(TEXT("second event placement exists"), SecondEventPlacement);
	TestNotNull(TEXT("second reset placement exists"), SecondResetPlacement);
	if (!FirstExpectedEventPlacement || !FirstExpectedResetPlacement || !SecondExpectedEventPlacement || !SecondExpectedResetPlacement ||
		!FirstEventPlacement || !FirstResetPlacement || !SecondEventPlacement || !SecondResetPlacement)
	{
		return false;
	}

	TestTrue(TEXT("first event matches solver baseline"), PlacementPositionsNearlyEqual(FirstExpectedEventPlacement, FirstEventPlacement));
	TestTrue(TEXT("first reset matches solver baseline"), PlacementPositionsNearlyEqual(FirstExpectedResetPlacement, FirstResetPlacement));
	TestTrue(TEXT("second event matches solver baseline"), PlacementPositionsNearlyEqual(SecondExpectedEventPlacement, SecondEventPlacement));
	TestTrue(TEXT("second reset matches solver baseline"), PlacementPositionsNearlyEqual(SecondExpectedResetPlacement, SecondResetPlacement));
	TestTrue(TEXT("changing solver spacing changes preview exec position"), !FirstResetPlacement->TargetPosition.Equals(SecondResetPlacement->TargetPosition));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewServiceUsesSolverBaselineForNodeInputCluster,
	"BlueprintHelper.GraphLayout.Preview.ServiceUsesSolverBaselineForNodeInputCluster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceUsesSolverBaselineForNodeInputCluster::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.DataClusterPaddingX = 280.0f;
	RuleSet.DataClusterPaddingY = 96.0f;
	RuleSet.InputPinRowSpacing = 64.0f;
	RuleSet.bUseTargetPinOrderForVariableInputs = true;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("node input sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::NodeInputCluster, Sample, Error));

	const FGraphSnapshot SolverSnapshot = FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(Sample);
	const FLayoutPlan ExpectedPlan = FSolver::Solve(SolverSnapshot, RuleSet);

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::NodeInputCluster;
	Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

	FGraphLayoutPreviewBuildResult PreviewResult;
	TestTrue(TEXT("preview builds"), Service.BuildPreviewDataForTest(Request, PreviewResult));
	TestTrue(TEXT("preview result succeeds"), PreviewResult.bSuccess);

	const FString NodeIds[] = {
		TEXT("Consumer"),
		TEXT("ContextGet"),
		TEXT("FlagGet"),
		TEXT("IsValidGate"),
		TEXT("ValueGet"),
		TEXT("NormalizeValue"),
		TEXT("ComposePayload")
	};

	for (const FString& NodeId : NodeIds)
	{
		const FNodePlacement* ExpectedPlacement = FindPlacement(ExpectedPlan, NodeId);
		const FNodePlacement* ActualPlacement = FindPlacement(PreviewResult.LayoutPlan, NodeId);
		TestNotNull(FString::Printf(TEXT("expected solver placement exists for %s"), *NodeId), ExpectedPlacement);
		TestNotNull(FString::Printf(TEXT("preview placement exists for %s"), *NodeId), ActualPlacement);
		TestTrue(
			FString::Printf(TEXT("preview placement uses solver baseline for %s"), *NodeId),
			PlacementPositionsNearlyEqual(ExpectedPlacement, ActualPlacement));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSolverInputExcludesOverlayNodes,
	"BlueprintHelper.GraphLayout.Preview.SolverInputExcludesOverlayNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSolverInputExcludesOverlayNodes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));

	const FGraphSnapshot SolverSnapshot = FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(Sample);
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (!NodeSpec.bPreviewOverlay)
		{
			continue;
		}

		TestNull(
			FString::Printf(TEXT("overlay node %s excluded from solver input"), *NodeSpec.NodeId),
			SolverSnapshot.Nodes.FindByPredicate([&NodeSpec](const FNodeSnapshot& Node)
			{
				return Node.NodeId == NodeSpec.NodeId;
			}));
	}
	TestNotNull(
		TEXT("real event node remains in solver input"),
		SolverSnapshot.Nodes.FindByPredicate([](const FNodeSnapshot& Node)
		{
			return Node.NodeId == TEXT("EventStart");
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSolverInputStripsLinksToFilteredOverlayNodes,
	"BlueprintHelper.GraphLayout.Preview.SolverInputStripsLinksToFilteredOverlayNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSolverInputStripsLinksToFilteredOverlayNodes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	Sample.Scene = ESemanticScene::LinearExecChain;
	Sample.Snapshot.GraphName = TEXT("Preview_SolverInputLinkCleanup");

	Sample.Snapshot.Nodes.Add(BlueprintHelperGraphLayoutSolverTests::MakeNode(
		TEXT("Source"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Source"),
		FVector2D(32.0f, 48.0f),
		FVector2D(220.0f, 88.0f),
		false,
		{
			BlueprintHelperGraphLayoutSolverTests::MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("RealTarget"), TEXT("HorizontalAvoidanceRange")}),
			BlueprintHelperGraphLayoutSolverTests::MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("RealDataTarget"), TEXT("HorizontalAvoidanceRange")})
		}));
	Sample.Snapshot.Nodes.Add(BlueprintHelperGraphLayoutSolverTests::MakeNode(
		TEXT("RealTarget"),
		TEXT("K2Node_CallFunction"),
		TEXT("Real Target"),
		FVector2D(280.0f, 48.0f),
		FVector2D(240.0f, 96.0f),
		false,
		{
			BlueprintHelperGraphLayoutSolverTests::MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Source")})
		}));
	Sample.Snapshot.Nodes.Add(BlueprintHelperGraphLayoutSolverTests::MakeNode(
		TEXT("RealDataTarget"),
		TEXT("K2Node_VariableSet"),
		TEXT("Real Data Target"),
		FVector2D(280.0f, 180.0f),
		FVector2D(220.0f, 88.0f),
		true,
		{
			BlueprintHelperGraphLayoutSolverTests::MakePin(TEXT("Input"), EPinDirection::Input, false, {TEXT("Source")})
		}));
	Sample.Snapshot.Nodes.Add(BlueprintHelperGraphLayoutSolverTests::MakeNode(
		TEXT("HorizontalAvoidanceRange"),
		TEXT("EdGraphNode_Comment"),
		TEXT("Horizontal Avoidance Range"),
		FVector2D(120.0f, 300.0f),
		FVector2D(420.0f, 120.0f),
		true,
		{}));

	auto AddNodeSpec = [&Sample](const FString& NodeId, const bool bPreviewOverlay)
	{
		FGraphLayoutPreviewNodeSpec NodeSpec;
		NodeSpec.NodeId = NodeId;
		NodeSpec.Title = NodeId;
		NodeSpec.bPreviewOverlay = bPreviewOverlay;
		Sample.Nodes.Add(NodeSpec);
	};

	AddNodeSpec(TEXT("Source"), false);
	AddNodeSpec(TEXT("RealTarget"), false);
	AddNodeSpec(TEXT("RealDataTarget"), false);
	AddNodeSpec(TEXT("HorizontalAvoidanceRange"), true);

	const FGraphSnapshot SolverSnapshot = FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(Sample);
	TestEqual(TEXT("graph name preserved"), SolverSnapshot.GraphName, Sample.Snapshot.GraphName);

	const FNodeSnapshot* SourceNode = SolverSnapshot.Nodes.FindByPredicate([](const FNodeSnapshot& Node)
	{
		return Node.NodeId == TEXT("Source");
	});
	TestNotNull(TEXT("source node remains in solver snapshot"), SourceNode);
	if (!SourceNode)
	{
		return false;
	}

	TestEqual(TEXT("source title preserved"), SourceNode->Title, TEXT("Source"));
	TestEqual(TEXT("source position x preserved"), SourceNode->Position.X, 32.0);
	TestEqual(TEXT("source position y preserved"), SourceNode->Position.Y, 48.0);
	TestEqual(TEXT("source pin count preserved"), SourceNode->Pins.Num(), 2);
	if (SourceNode->Pins.Num() != 2)
	{
		return false;
	}

	const FPinSnapshot& ExecPin = SourceNode->Pins[0];
	TestTrue(TEXT("real exec link preserved"), ExecPin.LinkedNodeIds.Contains(TEXT("RealTarget")));
	TestFalse(TEXT("overlay exec link removed"), ExecPin.LinkedNodeIds.Contains(TEXT("HorizontalAvoidanceRange")));
	TestEqual(TEXT("exec pin link count cleaned"), ExecPin.LinkedNodeIds.Num(), 1);

	const FPinSnapshot& DataPin = SourceNode->Pins[1];
	TestTrue(TEXT("real data link preserved"), DataPin.LinkedNodeIds.Contains(TEXT("RealDataTarget")));
	TestFalse(TEXT("overlay data link removed"), DataPin.LinkedNodeIds.Contains(TEXT("HorizontalAvoidanceRange")));
	TestEqual(TEXT("data pin link count cleaned"), DataPin.LinkedNodeIds.Num(), 1);

	TestNull(
		TEXT("overlay node removed from solver snapshot"),
		SolverSnapshot.Nodes.FindByPredicate([](const FNodeSnapshot& Node)
		{
			return Node.NodeId == TEXT("HorizontalAvoidanceRange");
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewServiceReflectsInputPinRowSpacing,
	"BlueprintHelper.GraphLayout.Preview.ServiceReflectsInputPinRowSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewServiceReflectsInputPinRowSpacing::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	auto BuildPreview = [](const float RowSpacing)
	{
		FRuleSet RuleSet;
		RuleSet.InputPinRowSpacing = RowSpacing;
		RuleSet.DataClusterPaddingX = 260.0f;
		RuleSet.DataClusterPaddingY = 72.0f;
		RuleSet.bUseTargetPinOrderForVariableInputs = true;

		FGraphLayoutPreviewService Service;
		FGraphLayoutPreviewRequest Request;
		Request.Scene = ESemanticScene::NodeInputCluster;
		Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

		FGraphLayoutPreviewBuildResult Result;
		Service.BuildPreviewDataForTest(Request, Result);
		return Result;
	};

	const FGraphLayoutPreviewBuildResult TightResult = BuildPreview(32.0f);
	const FGraphLayoutPreviewBuildResult LooseResult = BuildPreview(96.0f);
	TestTrue(TEXT("tight result succeeds"), TightResult.bSuccess);
	TestTrue(TEXT("loose result succeeds"), LooseResult.bSuccess);

	const FNodePlacement* TightContext = FindPlacement(TightResult.LayoutPlan, TEXT("ContextGet"));
	const FNodePlacement* TightFlag = FindPlacement(TightResult.LayoutPlan, TEXT("FlagGet"));
	const FNodePlacement* LooseContext = FindPlacement(LooseResult.LayoutPlan, TEXT("ContextGet"));
	const FNodePlacement* LooseFlag = FindPlacement(LooseResult.LayoutPlan, TEXT("FlagGet"));
	TestNotNull(TEXT("tight context exists"), TightContext);
	TestNotNull(TEXT("tight flag exists"), TightFlag);
	TestNotNull(TEXT("loose context exists"), LooseContext);
	TestNotNull(TEXT("loose flag exists"), LooseFlag);
	if (!TightContext || !TightFlag || !LooseContext || !LooseFlag)
	{
		return false;
	}

	const float TightDistance = FMath::Abs(TightFlag->TargetPosition.Y - TightContext->TargetPosition.Y);
	const float LooseDistance = FMath::Abs(LooseFlag->TargetPosition.Y - LooseContext->TargetPosition.Y);
	TestTrue(TEXT("larger input pin row spacing changes preview data node vertical distance"), LooseDistance > TightDistance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewOverlayFollowsSolverPlacement,
	"BlueprintHelper.GraphLayout.Preview.OverlayFollowsSolverPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewOverlayFollowsSolverPlacement::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 150.0f;
	RuleSet.CollisionPaddingY = 60.0f;
	RuleSet.CollisionStepY = 90.0f;
	RuleSet.MaxCollisionAttempts = 4;

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::LinearExecChain;
	Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

	FGraphLayoutPreviewBuildResult Result;
	TestTrue(TEXT("preview builds"), Service.BuildPreviewDataForTest(Request, Result));
	TestTrue(TEXT("preview result succeeds"), Result.bSuccess);

	const FNodePlacement* EventPlacement = FindPlacement(Result.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* LabelPlacement = FindPlacement(Result.LayoutPlan, TEXT("SemanticLabel_EventStart"));
	const FNodePlacement* HorizontalPlacement = FindPlacement(Result.LayoutPlan, TEXT("HorizontalAvoidanceRange"));
	const FNodePlacement* VerticalPlacement = FindPlacement(Result.LayoutPlan, TEXT("VerticalAvoidanceRange"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("semantic label placement exists"), LabelPlacement);
	TestNotNull(TEXT("horizontal range placement exists"), HorizontalPlacement);
	TestNotNull(TEXT("vertical range placement exists"), VerticalPlacement);
	if (!EventPlacement || !LabelPlacement || !HorizontalPlacement || !VerticalPlacement)
	{
		return false;
	}

	TestTrue(TEXT("label stays above event solver placement"), LabelPlacement->TargetPosition.Y < EventPlacement->TargetPosition.Y);
	TestTrue(
		TEXT("label tracks event solver placement horizontally"),
		FMath::Abs(LabelPlacement->TargetPosition.X - EventPlacement->TargetPosition.X) <= 48.0f);

	const FVector2D EntrySize = FindPreviewNodeSize(Result.Sample, TEXT("EventStart"));
	const float ExpectedHorizontalWidth =
		EntrySize.X + RuleSet.CollisionPaddingX * 2.0f +
		RuleSet.MaxCollisionAttempts * FMath::Max(EntrySize.X, RuleSet.CollisionPaddingX);
	const float ExpectedVerticalHeight =
		EntrySize.Y + RuleSet.CollisionPaddingY * 2.0f +
		RuleSet.MaxCollisionAttempts * RuleSet.CollisionStepY;

	TestEqual(TEXT("horizontal range width follows ruleset"), static_cast<double>(HorizontalPlacement->TargetSize.X), static_cast<double>(ExpectedHorizontalWidth));
	TestEqual(TEXT("vertical range height follows ruleset"), static_cast<double>(VerticalPlacement->TargetSize.Y), static_cast<double>(ExpectedVerticalHeight));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerUsesSolverBaselinePlacements,
	"BlueprintHelper.GraphLayout.Preview.MaterializerUsesSolverBaselinePlacements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerUsesSolverBaselinePlacements::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("preview materializer test runs on the game thread"), IsInGameThread());

	FRuleSet RuleSet;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);

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
		TEXT("event materializer uses solver plan position"),
		MaterializedNodeMatchesPlacement(Result, TEXT("EventStart"), EventPlacement->TargetPosition));
	TestTrue(
		TEXT("reset materializer uses solver plan position"),
		MaterializedNodeMatchesPlacement(Result, TEXT("ResetState"), ResetPlacement->TargetPosition));
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
	FBlueprintHelperGraphLayout_PreviewInteractionModelIgnoresAvoidanceRangeOverlays,
	"BlueprintHelper.GraphLayout.Preview.InteractionModelIgnoresAvoidanceRangeOverlays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionModelIgnoresAvoidanceRangeOverlays::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("occupancy sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::Occupancy, Sample, Error));

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 120.0f;
	RuleSet.CollisionPaddingY = 40.0f;
	RuleSet.CollisionStepY = 80.0f;
	RuleSet.MaxCollisionAttempts = 5;
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer succeeds"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("preview graph exists"), Result.PreviewGraph.IsValid());
	const FGuid* OverlayGuid = Result.NodeGuidsById.Find(TEXT("HorizontalAvoidanceRange"));
	TestNotNull(TEXT("horizontal overlay guid exists"), OverlayGuid);
	if (!Result.PreviewGraph.IsValid() || !OverlayGuid)
	{
		return false;
	}

	TestTrue(TEXT("horizontal overlay is registered as preview-only"), Result.PreviewOverlayGuids.Contains(*OverlayGuid));

	UEdGraphNode* OverlayNode = nullptr;
	for (UEdGraphNode* Node : Result.PreviewGraph->Nodes)
	{
		if (Node && Node->NodeGuid == *OverlayGuid)
		{
			OverlayNode = Node;
			break;
		}
	}
	TestNotNull(TEXT("horizontal overlay node found"), OverlayNode);
	if (!OverlayNode)
	{
		return false;
	}

	FGraphLayoutPreviewInteractionModel Model;
	TestTrue(TEXT("model initializes with non-overlay nodes"), Model.Initialize(Result, Result.PreviewGraph.Get()));
	Model.BeginInteraction(Result.PreviewGraph.Get());

	OverlayNode->NodePosX += 256;
	OverlayNode->NodePosY += 128;

	FGraphLayoutPreviewInteractionCommit Commit;
	TestFalse(TEXT("overlay-only move does not create a commit"), Model.EndInteraction(Result.PreviewGraph.Get(), Commit));
	TestEqual(TEXT("overlay-only move has no moved nodes"), Commit.MovedNodes.Num(), 0);
	TestEqual(TEXT("overlay-only move has no resized overlays"), Commit.ResizedOverlays.Num(), 0);
	TestTrue(TEXT("overlay-only move is ignored without rejection"), Commit.RejectionReason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionModelIgnoresSemanticLabelComments,
	"BlueprintHelper.GraphLayout.Preview.InteractionModelIgnoresSemanticLabelComments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionModelIgnoresSemanticLabelComments::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(
		TEXT("linear sample builds"),
		FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));
	FRuleSet RuleSet;
	const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("preview materializes"), Materializer.MaterializeForTest(Sample, Plan, Result));

	const FGuid* LabelGuid = Result.NodeGuidsById.Find(TEXT("SemanticLabel_EventStart"));
	TestNotNull(TEXT("label guid exists"), LabelGuid);
	if (!LabelGuid)
	{
		return false;
	}

	UEdGraphNode* LabelNode = nullptr;
	for (UEdGraphNode* Node : Result.PreviewGraph->Nodes)
	{
		if (Node && Node->NodeGuid == *LabelGuid)
		{
			LabelNode = Node;
			break;
		}
	}
	TestNotNull(TEXT("label node found"), LabelNode);
	if (!LabelNode)
	{
		return false;
	}

	FGraphLayoutPreviewInteractionModel Model;
	TestTrue(TEXT("model initializes"), Model.Initialize(Result, Result.PreviewGraph.Get()));
	Model.BeginInteraction(Result.PreviewGraph.Get());
	LabelNode->NodePosX += 100;
	LabelNode->NodePosY += 40;

	FGraphLayoutPreviewInteractionCommit Commit;
	TestFalse(TEXT("moving only semantic label creates no commit"), Model.EndInteraction(Result.PreviewGraph.Get(), Commit));
	TestEqual(TEXT("no moved nodes"), Commit.MovedNodes.Num(), 0);
	TestEqual(TEXT("no resized overlays"), Commit.ResizedOverlays.Num(), 0);
	TestTrue(TEXT("no rejection for moving ignored label"), Commit.RejectionReason.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionModelCommitsAvoidanceRangeOverlaySizesToRuleSet,
	"BlueprintHelper.GraphLayout.Preview.InteractionModelCommitsAvoidanceRangeOverlaySizesToRuleSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionModelCommitsAvoidanceRangeOverlaySizesToRuleSet::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	const ESemanticScene TestScenes[] = {
		ESemanticScene::LinearExecChain,
		ESemanticScene::Occupancy
	};
	for (const ESemanticScene Scene : TestScenes)
	{
		FGraphLayoutPreviewSample Sample;
		FString Error;
		TestTrue(
			FString::Printf(TEXT("%s sample builds"), ToString(Scene)),
			FGraphLayoutPreviewSampleFactory::BuildSample(Scene, Sample, Error));

		FRuleSet RuleSet;
		RuleSet.CollisionPaddingX = 120.0f;
		RuleSet.CollisionPaddingY = 40.0f;
		RuleSet.CollisionStepY = 80.0f;
		RuleSet.MaxCollisionAttempts = 5;
		const FLayoutPlan Plan = BuildSolverPreviewPlanForTest(Sample, RuleSet);

		FGraphLayoutPreviewMaterializer Materializer;
		FGraphLayoutPreviewMaterializerResult Result;
		TestTrue(TEXT("materializer succeeds"), Materializer.MaterializeForTest(Sample, Plan, Result));
		TestTrue(TEXT("preview graph exists"), Result.PreviewGraph.IsValid());
		if (!Result.PreviewGraph.IsValid())
		{
			return false;
		}

		UEdGraphNode* HorizontalOverlay = nullptr;
		UEdGraphNode* VerticalOverlay = nullptr;
		for (UEdGraphNode* Node : Result.PreviewGraph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			const FString NodeId = Result.NodeIdsByGuid.FindRef(Node->NodeGuid);
			if (NodeId == TEXT("HorizontalAvoidanceRange"))
			{
				HorizontalOverlay = Node;
			}
			else if (NodeId == TEXT("VerticalAvoidanceRange"))
			{
				VerticalOverlay = Node;
			}
		}
		TestNotNull(TEXT("horizontal overlay node"), HorizontalOverlay);
		TestNotNull(TEXT("vertical overlay node"), VerticalOverlay);
		if (!HorizontalOverlay || !VerticalOverlay)
		{
			return false;
		}

		FGraphLayoutPreviewInteractionModel Model;
		TestTrue(TEXT("model initializes"), Model.Initialize(Result, Result.PreviewGraph.Get()));
		Model.BeginInteraction(Result.PreviewGraph.Get());

		const FVector2D EntrySize = FindPreviewNodeSize(Sample, TEXT("EventStart"));
		const float DesiredPaddingX = 160.0f;
		const float DesiredPaddingY = 55.0f;
		const int32 DesiredAttempts = 7;
		const float DesiredStepY = 90.0f;
		HorizontalOverlay->NodeWidth = static_cast<int32>(
			EntrySize.X + DesiredPaddingX * 2.0f + DesiredAttempts * FMath::Max(EntrySize.X, DesiredPaddingX));
		HorizontalOverlay->NodeHeight = static_cast<int32>(EntrySize.Y + DesiredPaddingY * 2.0f);
		VerticalOverlay->NodeWidth = static_cast<int32>(EntrySize.X + DesiredPaddingX * 2.0f);
		VerticalOverlay->NodeHeight = static_cast<int32>(
			EntrySize.Y + DesiredPaddingY * 2.0f + DesiredAttempts * DesiredStepY);

		FGraphLayoutPreviewInteractionCommit Commit;
		TestTrue(TEXT("overlay resize creates a commit"), Model.EndInteraction(Result.PreviewGraph.Get(), Commit));
		TestEqual(TEXT("overlay resize does not move draggable anchors"), Commit.MovedNodes.Num(), 0);

		FString UpdatedJson;
		FString UpdateError;
		TestTrue(
			TEXT("overlay resize updates ruleset json"),
			FGraphLayoutPreviewInteractionModel::BuildRuleSetJsonForCommit(
				FRuleSetJson::ExportString(RuleSet),
				Scene,
				Commit,
				UpdatedJson,
				UpdateError));

		FRuleSet UpdatedRuleSet;
		FValidationResult Validation;
		TestTrue(TEXT("updated ruleset imports"), FRuleSetJson::ImportString(UpdatedJson, UpdatedRuleSet, Validation));
		TestEqual(TEXT("collision padding x from vertical range width"), UpdatedRuleSet.CollisionPaddingX, DesiredPaddingX);
		TestEqual(TEXT("collision padding y from horizontal range height"), UpdatedRuleSet.CollisionPaddingY, DesiredPaddingY);
		TestEqual(TEXT("max collision attempts from horizontal range width"), UpdatedRuleSet.MaxCollisionAttempts, DesiredAttempts);
		TestEqual(TEXT("collision step y from vertical range height"), UpdatedRuleSet.CollisionStepY, DesiredStepY);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionAccumulatorMergesPendingCommits,
	"BlueprintHelper.GraphLayout.Preview.InteractionAccumulatorMergesPendingCommits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionAccumulatorMergesPendingCommits::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewInteractionCommit FirstCommit;
	FGraphLayoutPreviewMovedNode& FirstMoved = FirstCommit.MovedNodes.AddDefaulted_GetRef();
	FirstMoved.NodeId = TEXT("ExecStep");
	FirstMoved.NodeGuid = FGuid::NewGuid();
	FirstMoved.Role = ENodeRole::ExecNode;
	FirstMoved.AnchorRole = ENodeRole::ExecNode;
	FirstMoved.BeginTopLeft = FVector2D(100.0, 100.0);
	FirstMoved.EndTopLeft = FVector2D(200.0, 100.0);
	FirstMoved.Size = FVector2D(220.0, 96.0);

	FGraphLayoutPreviewInteractionCommit SecondCommit;
	FGraphLayoutPreviewMovedNode& SecondMoved = SecondCommit.MovedNodes.AddDefaulted_GetRef();
	SecondMoved.NodeId = TEXT("ExecStep");
	SecondMoved.NodeGuid = FirstMoved.NodeGuid;
	SecondMoved.Role = ENodeRole::ExecNode;
	SecondMoved.AnchorRole = ENodeRole::ExecNode;
	SecondMoved.BeginTopLeft = FVector2D(200.0, 100.0);
	SecondMoved.EndTopLeft = FVector2D(340.0, 160.0);
	SecondMoved.Size = FVector2D(220.0, 96.0);

	FGraphLayoutPreviewResizedOverlay& ResizedOverlay = SecondCommit.ResizedOverlays.AddDefaulted_GetRef();
	ResizedOverlay.NodeId = TEXT("HorizontalAvoidanceRange");
	ResizedOverlay.NodeGuid = FGuid::NewGuid();
	ResizedOverlay.BeginSize = FVector2D(420.0, 120.0);
	ResizedOverlay.EndSize = FVector2D(520.0, 160.0);
	SecondCommit.AvoidanceEntrySize = FVector2D(220.0, 96.0);

	FGraphLayoutPreviewInteractionCommitAccumulator Accumulator;
	TestFalse(TEXT("starts empty"), Accumulator.HasPendingChanges());

	Accumulator.Append(FirstCommit);
	Accumulator.Append(SecondCommit);

	TestTrue(TEXT("has pending changes"), Accumulator.HasPendingChanges());
	const FGraphLayoutPreviewInteractionCommit& PendingCommit = Accumulator.GetCommit();
	TestEqual(TEXT("one moved node after merge"), PendingCommit.MovedNodes.Num(), 1);
	TestEqual(TEXT("merged moved node keeps original begin x"), PendingCommit.MovedNodes[0].BeginTopLeft.X, 100.0);
	TestEqual(TEXT("merged moved node keeps latest end x"), PendingCommit.MovedNodes[0].EndTopLeft.X, 340.0);
	TestEqual(TEXT("one resized overlay"), PendingCommit.ResizedOverlays.Num(), 1);
	TestEqual(TEXT("overlay keeps begin width"), PendingCommit.ResizedOverlays[0].BeginSize.X, 420.0);
	TestEqual(TEXT("overlay keeps latest end width"), PendingCommit.ResizedOverlays[0].EndSize.X, 520.0);
	TestEqual(TEXT("avoidance entry size retained"), PendingCommit.AvoidanceEntrySize.X, 220.0);

	Accumulator.Reset();
	TestFalse(TEXT("reset clears pending changes"), Accumulator.HasPendingChanges());
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
	FBlueprintHelperGraphLayout_PreviewInteractionPendingCommitBuildsRuleSetJson,
	"BlueprintHelper.GraphLayout.Preview.InteractionPendingCommitBuildsRuleSetJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionPendingCommitBuildsRuleSetJson::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewInteractionCommit Commit;
	FGraphLayoutPreviewMovedNode& Moved = Commit.MovedNodes.AddDefaulted_GetRef();
	Moved.NodeId = TEXT("ResetState");
	Moved.NodeGuid = FGuid::NewGuid();
	Moved.Role = ENodeRole::ExecNode;
	Moved.AnchorRole = ENodeRole::ExecNode;
	Moved.BeginTopLeft = FVector2D(100.0, 100.0);
	Moved.EndTopLeft = FVector2D(520.0, 180.0);
	Moved.Size = FVector2D(220.0, 96.0);

	FGraphLayoutPreviewInteractionCommitAccumulator Accumulator;
	Accumulator.Append(Commit);

	FRuleSet RuleSet;
	FEditorCanvasSceneState SceneState;
	SceneState.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0, 100.0));
	SceneState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(300.0, 100.0));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, SceneState);

	const FString InputJson = FRuleSetJson::ExportString(RuleSet);
	FString OutputJson;
	FString Error;
	TestTrue(
		TEXT("pending commit builds RuleSet JSON"),
		FGraphLayoutPreviewInteractionModel::BuildRuleSetJsonForCommit(
			InputJson,
			ESemanticScene::LinearExecChain,
			Accumulator.GetCommit(),
			OutputJson,
			Error));

	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("output imports"), FRuleSetJson::ImportString(OutputJson, Parsed, Validation));
	const FEditorCanvasSceneState Resolved =
		FSemanticSceneAdapter::ResolveSceneState(Parsed, ESemanticScene::LinearExecChain);
	const FVector2D ExecCenter = Resolved.RoleCenters.FindRef(ENodeRole::ExecNode);
	TestEqual(TEXT("exec center x follows pending moved preview node center"), ExecCenter.X, 630.0);
	TestEqual(TEXT("exec center y follows pending moved preview node center"), ExecCenter.Y, 228.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewInteractionCoordinatorConsumesPendingRuleSetJson,
	"BlueprintHelper.GraphLayout.Preview.InteractionCoordinatorConsumesPendingRuleSetJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewInteractionCoordinatorConsumesPendingRuleSetJson::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FGraphLayoutPreviewInteractionCommit Commit;
	FGraphLayoutPreviewMovedNode& Moved = Commit.MovedNodes.AddDefaulted_GetRef();
	Moved.NodeId = TEXT("ResetState");
	Moved.NodeGuid = FGuid::NewGuid();
	Moved.Role = ENodeRole::ExecNode;
	Moved.AnchorRole = ENodeRole::ExecNode;
	Moved.BeginTopLeft = FVector2D(100.0, 100.0);
	Moved.EndTopLeft = FVector2D(520.0, 180.0);
	Moved.Size = FVector2D(220.0, 96.0);

	FRuleSet RuleSet;
	FEditorCanvasSceneState SceneState;
	SceneState.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0, 100.0));
	SceneState.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(300.0, 100.0));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, SceneState);

	FGraphLayoutPreviewInteractionCommitCoordinator Coordinator;
	TestFalse(TEXT("coordinator starts empty"), Coordinator.HasPendingChanges());
	const FGraphLayoutPreviewInteractionApplyResult EmptyResult =
		Coordinator.ConsumePendingRuleSetJson(FRuleSetJson::ExportString(RuleSet), ESemanticScene::LinearExecChain);
	TestEqual(
		TEXT("empty apply reports no pending changes"),
		EmptyResult.Status,
		EGraphLayoutPreviewInteractionApplyStatus::NoPendingChanges);

	Coordinator.Append(Commit);
	TestTrue(TEXT("coordinator has pending changes"), Coordinator.HasPendingChanges());
	const FGraphLayoutPreviewInteractionApplyResult ApplyResult =
		Coordinator.ConsumePendingRuleSetJson(FRuleSetJson::ExportString(RuleSet), ESemanticScene::LinearExecChain);
	TestEqual(TEXT("coordinator applies pending changes"), ApplyResult.Status, EGraphLayoutPreviewInteractionApplyStatus::Applied);
	TestFalse(TEXT("coordinator consumes pending changes"), Coordinator.HasPendingChanges());

	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("coordinator output imports"), FRuleSetJson::ImportString(ApplyResult.UpdatedRuleSetJson, Parsed, Validation));
	const FEditorCanvasSceneState Resolved =
		FSemanticSceneAdapter::ResolveSceneState(Parsed, ESemanticScene::LinearExecChain);
	const FVector2D ExecCenter = Resolved.RoleCenters.FindRef(ENodeRole::ExecNode);
	TestEqual(TEXT("exec center x follows coordinator pending moved preview node center"), ExecCenter.X, 630.0);
	TestEqual(TEXT("exec center y follows coordinator pending moved preview node center"), ExecCenter.Y, 228.0);
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

	FBlueprintHelperVersionCompat::BreakPinLinkTo(Then0Pin, BranchExecPin, false);
	FBlueprintHelperVersionCompat::BreakPinLinkTo(Then1Pin, PrintExecPin, false);
	FBlueprintHelperVersionCompat::MakePinLinkTo(Then0Pin, PrintExecPin, false);
	FBlueprintHelperVersionCompat::MakePinLinkTo(Then1Pin, BranchExecPin, false);
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
