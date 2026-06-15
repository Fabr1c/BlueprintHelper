#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MakeArray.h"
#include "K2Node_Self.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Guid.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectGlobals.h"

struct FBlueprintHelperGraphLayoutPreviewMaterializerUtils
{
	static EEdGraphPinDirection ToEdGraphDirection(const BlueprintHelper::GraphLayout::EPinDirection Direction)
	{
		return Direction == BlueprintHelper::GraphLayout::EPinDirection::Input ? EGPD_Input : EGPD_Output;
	}

	static FEdGraphPinType MakePinType(const BlueprintHelper::GraphLayout::FPinSnapshot& PinSnapshot)
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = PinSnapshot.bExec
			? UEdGraphSchema_K2::PC_Exec
			: (PinSnapshot.Category.IsEmpty() ? UEdGraphSchema_K2::PC_Object : FName(*PinSnapshot.Category));
		return PinType;
	}

	static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, const EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName == FName(*PinName) && Pin->Direction == Direction)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static FString BuildNodeName(const FString& NodeId, const FString& Title)
	{
		return Title.IsEmpty() ? NodeId : Title;
	}
};

namespace BlueprintHelper::GraphLayout
{
void FGraphLayoutPreviewMaterializer::Begin(const FGraphLayoutPreviewSample& Sample, const FLayoutPlan& LayoutPlan)
{
	ResetState();
	PendingSample = Sample;
	PendingLayoutPlan = LayoutPlan;
	bBegun = true;
	bComplete = false;
}

bool FGraphLayoutPreviewMaterializer::Tick(const float MaxMillisecondsPerFrame)
{
	if (!bBegun || bComplete)
	{
		return false;
	}

	if (!EnsureGameThread(TEXT("Tick")))
	{
		return false;
	}

	if (!Result.PreviewGraph.IsValid() && !InitializePreviewGraph())
	{
		return false;
	}

	const double StartedAtSeconds = FPlatformTime::Seconds();
	int32 ProcessedSteps = 0;
	const bool bUseOneStepFallback = MaxMillisecondsPerFrame <= 0.0f;
	auto HasBudgetRemaining = [&StartedAtSeconds, &ProcessedSteps, MaxMillisecondsPerFrame, bUseOneStepFallback]() -> bool
	{
		if (ProcessedSteps == 0)
		{
			return true;
		}
		if (bUseOneStepFallback)
		{
			return false;
		}
		return ((FPlatformTime::Seconds() - StartedAtSeconds) * 1000.0) < MaxMillisecondsPerFrame;
	};

	while (NextNodeIndex < PendingSample.Nodes.Num() && HasBudgetRemaining())
	{
		if (!MaterializeNextNode())
		{
			return false;
		}
		++ProcessedSteps;
	}

	while (NextLinkIndex < PendingSample.Links.Num() && HasBudgetRemaining())
	{
		if (!MaterializeNextLink())
		{
			return false;
		}
		++ProcessedSteps;
	}

	if (NextNodeIndex >= PendingSample.Nodes.Num() && NextLinkIndex >= PendingSample.Links.Num())
	{
		bComplete = true;
	}

	return !bComplete;
}

void FGraphLayoutPreviewMaterializer::Cancel()
{
	ResetState();
}

bool FGraphLayoutPreviewMaterializer::IsComplete() const
{
	return bComplete;
}

const FGraphLayoutPreviewMaterializerResult& FGraphLayoutPreviewMaterializer::GetResult() const
{
	return Result;
}

bool FGraphLayoutPreviewMaterializer::MaterializeForTest(
	const FGraphLayoutPreviewSample& Sample,
	const FLayoutPlan& LayoutPlan,
	FGraphLayoutPreviewMaterializerResult& OutResult)
{
	Begin(Sample, LayoutPlan);

	int32 TickGuard = 0;
	while (Tick(0.0f))
	{
		++TickGuard;
		if (TickGuard > (PendingSample.Nodes.Num() + PendingSample.Links.Num() + 8))
		{
			FinishWithError(TEXT("preview materializer exceeded synchronous tick guard"));
			break;
		}
	}

	OutResult = Result;
	return Result.Error.IsEmpty() && OutResult.PreviewBlueprint.IsValid() && OutResult.PreviewGraph.IsValid();
}

void FGraphLayoutPreviewMaterializer::ResetState()
{
	PendingSample = FGraphLayoutPreviewSample();
	PendingLayoutPlan = FLayoutPlan();
	Result = FGraphLayoutPreviewMaterializerResult();
	MaterializedNodesById.Reset();
	NextNodeIndex = 0;
	NextLinkIndex = 0;
	bBegun = false;
	bComplete = true;
}

bool FGraphLayoutPreviewMaterializer::EnsureGameThread(const TCHAR* Context)
{
	if (IsInGameThread())
	{
		return true;
	}

	FinishWithError(FString::Printf(TEXT("graph layout preview materializer requires the game thread: %s"), Context));
	return false;
}

bool FGraphLayoutPreviewMaterializer::InitializePreviewGraph()
{
	if (!EnsureGameThread(TEXT("InitializePreviewGraph")))
	{
		return false;
	}

	const FName BlueprintName = MakeUniqueObjectName(
		GetTransientPackage(),
		UBlueprint::StaticClass(),
		FName(TEXT("BlueprintHelperGraphLayoutPreviewBP")));
	UBlueprint* PreviewBlueprint = NewObject<UBlueprint>(
		GetTransientPackage(),
		BlueprintName,
		RF_Transient);
	if (!PreviewBlueprint)
	{
		FinishWithError(TEXT("failed to create transient preview blueprint"));
		return false;
	}

	PreviewBlueprint->ParentClass = AActor::StaticClass();
	Result.PreviewBlueprint = TStrongObjectPtr<UBlueprint>(PreviewBlueprint);

	const FName GraphName = PendingSample.Snapshot.GraphName.IsEmpty()
		? FName(TEXT("BlueprintHelperGraphLayoutPreviewGraph"))
		: FName(*PendingSample.Snapshot.GraphName);
	UEdGraph* PreviewGraph = NewObject<UEdGraph>(
		PreviewBlueprint,
		GraphName,
		RF_Transient);
	if (!PreviewGraph)
	{
		FinishWithError(TEXT("failed to create transient preview graph"));
		return false;
	}

	PreviewGraph->Schema = UEdGraphSchema_K2::StaticClass();
	PreviewGraph->bEditable = true;
	PreviewGraph->SetFlags(RF_Transient);
	PreviewBlueprint->UbergraphPages.Add(PreviewGraph);
	Result.PreviewGraph = TStrongObjectPtr<UEdGraph>(PreviewGraph);
	return true;
}

bool FGraphLayoutPreviewMaterializer::MaterializeNextNode()
{
	if (!Result.PreviewGraph.IsValid())
	{
		FinishWithError(TEXT("preview graph is not initialized"));
		return false;
	}

	if (!PendingSample.Nodes.IsValidIndex(NextNodeIndex))
	{
		FinishWithError(TEXT("preview node index is out of range"));
		return false;
	}

	const FGraphLayoutPreviewNodeSpec& NodeSpec = PendingSample.Nodes[NextNodeIndex];
	const FNodePlacement* Placement = FindPlacement(NodeSpec.NodeId);
	if (NodeSpec.bPreviewOverlay && !Placement)
	{
		++NextNodeIndex;
		return true;
	}

	UEdGraphNode* Node = CreateNodeForSpec(NodeSpec);
	if (!Node)
	{
		return false;
	}

	const FNodeSnapshot* SnapshotNode = FindSnapshotNode(NodeSpec.NodeId);
	if (!SnapshotNode && !NodeSpec.bPreviewSemanticLabel)
	{
		FinishWithError(FString::Printf(TEXT("preview sample snapshot is missing node \"%s\""), *NodeSpec.NodeId));
		return false;
	}

	const FVector2D TargetPosition = Placement
		? Placement->TargetPosition
		: (SnapshotNode ? SnapshotNode->Position : FVector2D::ZeroVector);
	const FVector2D TargetSize = Placement && !Placement->TargetSize.IsNearlyZero()
		? Placement->TargetSize
		: FVector2D::ZeroVector;
	Node->NodePosX = FMath::RoundToInt(TargetPosition.X);
	Node->NodePosY = FMath::RoundToInt(TargetPosition.Y);
	Node->NodeWidth = TargetSize.X > 0.0f
		? TargetSize.X
		: (NodeSpec.Size.X > 0.0f ? NodeSpec.Size.X : (SnapshotNode ? SnapshotNode->Size.X : 220.0f));
	Node->NodeHeight = TargetSize.Y > 0.0f
		? TargetSize.Y
		: (NodeSpec.Size.Y > 0.0f ? NodeSpec.Size.Y : (SnapshotNode ? SnapshotNode->Size.Y : 96.0f));
	Node->CreateNewGuid();
	Node->SetFlags(RF_Transient);

	if (SnapshotNode)
	{
		for (const FPinSnapshot& PinSnapshot : SnapshotNode->Pins)
		{
			if (!FindOrCreatePin(Node, PinSnapshot))
			{
				FinishWithError(FString::Printf(
					TEXT("failed to create preview pin \"%s\" on node \"%s\""),
					*PinSnapshot.Name,
					*NodeSpec.NodeId));
				return false;
			}
		}
	}

	MaterializedNodesById.Add(NodeSpec.NodeId, Node);
	Result.NodeGuidsById.Add(NodeSpec.NodeId, Node->NodeGuid);
	Result.NodeIdsByGuid.Add(Node->NodeGuid, NodeSpec.NodeId);
	Result.RolesByGuid.Add(Node->NodeGuid, NodeSpec.Role);
	Result.AnchorRolesByGuid.Add(
		Node->NodeGuid,
		NodeSpec.bUsePreviewRoleAnchor ? NodeSpec.PreviewAnchorRole : NodeSpec.Role);
	if (NodeSpec.bPreviewOverlay)
	{
		Result.PreviewOverlayGuids.Add(Node->NodeGuid);
	}
	++NextNodeIndex;
	return true;
}

bool FGraphLayoutPreviewMaterializer::MaterializeNextLink()
{
	if (!PendingSample.Links.IsValidIndex(NextLinkIndex))
	{
		FinishWithError(TEXT("preview link index is out of range"));
		return false;
	}

	const FGraphLayoutPreviewLinkSpec& Link = PendingSample.Links[NextLinkIndex];
	if (!ConnectLink(Link))
	{
		return false;
	}

	++NextLinkIndex;
	return true;
}

void FGraphLayoutPreviewMaterializer::FinishWithError(const FString& ErrorMessage)
{
	Result.Error = ErrorMessage;
	bComplete = true;
}

const FGraphLayoutPreviewNodeSpec* FGraphLayoutPreviewMaterializer::FindNodeSpec(const FString& NodeId) const
{
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : PendingSample.Nodes)
	{
		if (NodeSpec.NodeId == NodeId)
		{
			return &NodeSpec;
		}
	}
	return nullptr;
}

const FNodeSnapshot* FGraphLayoutPreviewMaterializer::FindSnapshotNode(const FString& NodeId) const
{
	for (const FNodeSnapshot& Node : PendingSample.Snapshot.Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}
	return nullptr;
}

const FNodePlacement* FGraphLayoutPreviewMaterializer::FindPlacement(const FString& NodeId) const
{
	for (const FNodePlacement& Placement : PendingLayoutPlan.Placements)
	{
		if (Placement.NodeId == NodeId)
		{
			return &Placement;
		}
	}
	return nullptr;
}

bool FGraphLayoutPreviewMaterializer::IsSkippedPreviewOverlay(const FString& NodeId) const
{
	const FGraphLayoutPreviewNodeSpec* NodeSpec = FindNodeSpec(NodeId);
	return NodeSpec && NodeSpec->bPreviewOverlay && !FindPlacement(NodeId);
}

UEdGraphNode* FGraphLayoutPreviewMaterializer::CreateNodeForSpec(const FGraphLayoutPreviewNodeSpec& NodeSpec)
{
	UEdGraph* Graph = Result.PreviewGraph.Get();
	if (!Graph)
	{
		FinishWithError(TEXT("preview graph is not available during node creation"));
		return nullptr;
	}

	const FString NodeName = FBlueprintHelperGraphLayoutPreviewMaterializerUtils::BuildNodeName(NodeSpec.NodeId, NodeSpec.Title);
	switch (NodeSpec.Factory)
	{
	case EGraphLayoutPreviewNodeFactory::CustomEvent:
	{
		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		Graph->AddNode(EventNode, true, false);
		EventNode->CustomFunctionName = FName(*NodeName);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
		return EventNode;
	}
	case EGraphLayoutPreviewNodeFactory::CallFunction:
	{
		UFunction* PrintStringFunction = UKismetSystemLibrary::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
		if (PrintStringFunction)
		{
			UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(Graph);
			Graph->AddNode(CallFunctionNode, true, false);
			CallFunctionNode->FunctionReference.SetExternalMember(
				GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString),
				UKismetSystemLibrary::StaticClass());
			CallFunctionNode->PostPlacedNewNode();
			CallFunctionNode->AllocateDefaultPins();
			return CallFunctionNode;
		}
		break;
	}
	case EGraphLayoutPreviewNodeFactory::IfThenElse:
	{
		UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
		Graph->AddNode(BranchNode, true, false);
		BranchNode->PostPlacedNewNode();
		BranchNode->AllocateDefaultPins();
		return BranchNode;
	}
	case EGraphLayoutPreviewNodeFactory::ExecutionSequence:
	{
		UK2Node_ExecutionSequence* SequenceNode = NewObject<UK2Node_ExecutionSequence>(Graph);
		Graph->AddNode(SequenceNode, true, false);
		SequenceNode->PostPlacedNewNode();
		SequenceNode->AllocateDefaultPins();
		return SequenceNode;
	}
	case EGraphLayoutPreviewNodeFactory::MakeArray:
	{
		UK2Node_MakeArray* MakeArrayNode = NewObject<UK2Node_MakeArray>(Graph);
		Graph->AddNode(MakeArrayNode, true, false);
		MakeArrayNode->PostPlacedNewNode();
		MakeArrayNode->AllocateDefaultPins();
		return MakeArrayNode;
	}
	case EGraphLayoutPreviewNodeFactory::Self:
	{
		UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
		Graph->AddNode(SelfNode, true, false);
		SelfNode->PostPlacedNewNode();
		SelfNode->AllocateDefaultPins();
		return SelfNode;
	}
	case EGraphLayoutPreviewNodeFactory::Comment:
	{
		UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(Graph);
		Graph->AddNode(CommentNode, true, false);
		CommentNode->NodeComment = NodeSpec.Title;
		CommentNode->CommentColor = NodeSpec.CommentColor;
		return CommentNode;
	}
	case EGraphLayoutPreviewNodeFactory::GenericK2:
	default:
		break;
	}

	UEdGraphNode* GenericNode = NewObject<UEdGraphNode>(Graph);
	Graph->AddNode(GenericNode, true, false);
	GenericNode->NodeComment = NodeSpec.Title;
	return GenericNode;
}

UEdGraphPin* FGraphLayoutPreviewMaterializer::FindOrCreatePin(UEdGraphNode* Node, const FPinSnapshot& PinSnapshot)
{
	if (!Node)
	{
		return nullptr;
	}

	const EEdGraphPinDirection Direction =
		FBlueprintHelperGraphLayoutPreviewMaterializerUtils::ToEdGraphDirection(PinSnapshot.Direction);
	if (UEdGraphPin* ExistingPin = FBlueprintHelperGraphLayoutPreviewMaterializerUtils::FindPin(Node, PinSnapshot.Name, Direction))
	{
		return ExistingPin;
	}

	const FEdGraphPinType PinType = FBlueprintHelperGraphLayoutPreviewMaterializerUtils::MakePinType(PinSnapshot);
	return Node->CreatePin(Direction, PinType, FName(*PinSnapshot.Name));
}

bool FGraphLayoutPreviewMaterializer::ConnectLink(const FGraphLayoutPreviewLinkSpec& Link)
{
	UEdGraphNode* const* FromNodePtr = MaterializedNodesById.Find(Link.FromNodeId);
	UEdGraphNode* const* ToNodePtr = MaterializedNodesById.Find(Link.ToNodeId);
	if (!FromNodePtr || !*FromNodePtr || !ToNodePtr || !*ToNodePtr)
	{
		const bool bFromMissing = !FromNodePtr || !*FromNodePtr;
		const bool bToMissing = !ToNodePtr || !*ToNodePtr;
		const bool bFromMissingSkippedOverlay = bFromMissing && IsSkippedPreviewOverlay(Link.FromNodeId);
		const bool bToMissingSkippedOverlay = bToMissing && IsSkippedPreviewOverlay(Link.ToNodeId);
		const bool bOnlySkippedOverlayEndpointsAreMissing =
			(!bFromMissing || bFromMissingSkippedOverlay) &&
			(!bToMissing || bToMissingSkippedOverlay) &&
			(bFromMissingSkippedOverlay || bToMissingSkippedOverlay);
		if (bOnlySkippedOverlayEndpointsAreMissing)
		{
			return true;
		}

		FinishWithError(FString::Printf(
			TEXT("preview link references unknown node: %s.%s -> %s.%s"),
			*Link.FromNodeId,
			*Link.FromPinName,
			*Link.ToNodeId,
			*Link.ToPinName));
		return false;
	}

	const FNodeSnapshot* FromSnapshot = FindSnapshotNode(Link.FromNodeId);
	const FNodeSnapshot* ToSnapshot = FindSnapshotNode(Link.ToNodeId);
	if (!FromSnapshot || !ToSnapshot)
	{
		FinishWithError(FString::Printf(
			TEXT("preview link snapshot nodes are missing: %s -> %s"),
			*Link.FromNodeId,
			*Link.ToNodeId));
		return false;
	}

	const FPinSnapshot* FromSnapshotPin = nullptr;
	for (const FPinSnapshot& Pin : FromSnapshot->Pins)
	{
		if (Pin.Name == Link.FromPinName && Pin.Direction == EPinDirection::Output)
		{
			FromSnapshotPin = &Pin;
			break;
		}
	}

	const FPinSnapshot* ToSnapshotPin = nullptr;
	for (const FPinSnapshot& Pin : ToSnapshot->Pins)
	{
		if (Pin.Name == Link.ToPinName && Pin.Direction == EPinDirection::Input)
		{
			ToSnapshotPin = &Pin;
			break;
		}
	}

	if (!FromSnapshotPin || !ToSnapshotPin)
	{
		FinishWithError(FString::Printf(
			TEXT("preview link pin metadata is missing: %s.%s -> %s.%s"),
			*Link.FromNodeId,
			*Link.FromPinName,
			*Link.ToNodeId,
			*Link.ToPinName));
		return false;
	}

	UEdGraphPin* FromPin = FindOrCreatePin(*FromNodePtr, *FromSnapshotPin);
	UEdGraphPin* ToPin = FindOrCreatePin(*ToNodePtr, *ToSnapshotPin);
	if (!FromPin || !ToPin)
	{
		FinishWithError(FString::Printf(
			TEXT("failed to create preview link pins: %s.%s -> %s.%s"),
			*Link.FromNodeId,
			*Link.FromPinName,
			*Link.ToNodeId,
			*Link.ToPinName));
		return false;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (Schema && Schema->TryCreateConnection(FromPin, ToPin))
	{
		return true;
	}

	if (!FromPin->LinkedTo.Contains(ToPin))
	{
		FBlueprintHelperVersionCompat::MakePinLinkTo(FromPin, ToPin, true);
	}
	return FromPin->LinkedTo.Contains(ToPin);
}
}
