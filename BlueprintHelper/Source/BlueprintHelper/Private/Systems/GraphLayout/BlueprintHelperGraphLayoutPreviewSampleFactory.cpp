#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.h"

#include <initializer_list>

namespace BlueprintHelper::GraphLayout
{
static FPinSnapshot MakePreviewPin(
	const FString& Name,
	const EPinDirection Direction,
	const bool bExec,
	const FString& Category = TEXT(""))
{
	FPinSnapshot Pin;
	Pin.PinId = Name;
	Pin.Name = Name;
	Pin.Direction = Direction;
	Pin.bExec = bExec;
	Pin.Category = Category.IsEmpty()
		? (bExec ? TEXT("exec") : TEXT("object"))
		: Category;
	return Pin;
}

static void AddPreviewNode(
	FGraphLayoutPreviewSample& Sample,
	const FString& NodeId,
	const FString& ClassPath,
	const FString& Title,
	const EGraphLayoutPreviewNodeFactory Factory,
	const ENodeRole Role,
	const FVector2D& Position,
	const FVector2D& Size,
	const bool bExisting,
	std::initializer_list<FPinSnapshot> Pins,
	const ENodeRole PreviewAnchorRole = ENodeRole::Unknown)
{
	FNodeSnapshot SnapshotNode;
	SnapshotNode.NodeId = NodeId;
	SnapshotNode.StableName = NodeId;
	SnapshotNode.ClassPath = ClassPath;
	SnapshotNode.Title = Title;
	SnapshotNode.Position = Position;
	SnapshotNode.Size = Size;
	SnapshotNode.bExisting = bExisting;
	for (const FPinSnapshot& Pin : Pins)
	{
		SnapshotNode.Pins.Add(Pin);
	}
	Sample.Snapshot.Nodes.Add(SnapshotNode);

	FGraphLayoutPreviewNodeSpec NodeSpec;
	NodeSpec.NodeId = NodeId;
	NodeSpec.Title = Title;
	NodeSpec.Factory = Factory;
	NodeSpec.Role = Role;
	NodeSpec.PreviewAnchorRole = PreviewAnchorRole;
	NodeSpec.bUsePreviewRoleAnchor = PreviewAnchorRole != ENodeRole::Unknown;
	NodeSpec.Size = Size;
	Sample.Nodes.Add(NodeSpec);
}

static FNodeSnapshot* FindPreviewNode(FGraphLayoutPreviewSample& Sample, const FString& NodeId)
{
	for (FNodeSnapshot& Node : Sample.Snapshot.Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}
	return nullptr;
}

static FPinSnapshot* FindPreviewPin(FNodeSnapshot& Node, const FString& PinName, const EPinDirection Direction)
{
	for (FPinSnapshot& Pin : Node.Pins)
	{
		if (Pin.Name == PinName && Pin.Direction == Direction)
		{
			return &Pin;
		}
	}
	return nullptr;
}

static bool AddPreviewLink(
	FGraphLayoutPreviewSample& Sample,
	const FString& FromNodeId,
	const FString& FromPinName,
	const FString& ToNodeId,
	const FString& ToPinName,
	const bool bExec,
	FString& OutError)
{
	FNodeSnapshot* FromNode = FindPreviewNode(Sample, FromNodeId);
	FNodeSnapshot* ToNode = FindPreviewNode(Sample, ToNodeId);
	if (!FromNode || !ToNode)
	{
		OutError = FString::Printf(TEXT("preview sample link references missing node: %s -> %s"), *FromNodeId, *ToNodeId);
		return false;
	}

	FPinSnapshot* FromPin = FindPreviewPin(*FromNode, FromPinName, EPinDirection::Output);
	FPinSnapshot* ToPin = FindPreviewPin(*ToNode, ToPinName, EPinDirection::Input);
	if (!FromPin || !ToPin)
	{
		OutError = FString::Printf(TEXT("preview sample link references missing pin: %s.%s -> %s.%s"), *FromNodeId, *FromPinName, *ToNodeId, *ToPinName);
		return false;
	}

	if (bExec)
	{
		FromPin->LinkedNodeIds.Add(ToNodeId);
		ToPin->LinkedNodeIds.Add(FromNodeId);
	}
	else
	{
		ToPin->LinkedNodeIds.Add(FromNodeId);
	}

	FGraphLayoutPreviewLinkSpec Link;
	Link.FromNodeId = FromNodeId;
	Link.FromPinName = FromPinName;
	Link.ToNodeId = ToNodeId;
	Link.ToPinName = ToPinName;
	Link.bExec = bExec;
	Sample.Links.Add(Link);
	return true;
}

static void ResetSample(const ESemanticScene Scene, FGraphLayoutPreviewSample& OutSample)
{
	OutSample = FGraphLayoutPreviewSample();
	OutSample.Scene = Scene;
	OutSample.Snapshot.GraphName = FString::Printf(TEXT("Preview_%s"), ToString(Scene));
}

static bool BuildLinearExecSample(FGraphLayoutPreviewSample& OutSample, FString& OutError)
{
	ResetSample(ESemanticScene::LinearExecChain, OutSample);

	AddPreviewNode(
		OutSample,
		TEXT("EventStart"),
		TEXT("K2Node_CustomEvent"),
		TEXT("On Preview Trigger"),
		EGraphLayoutPreviewNodeFactory::CustomEvent,
		ENodeRole::EventEntry,
		FVector2D(0.0f, 0.0f),
		FVector2D(220.0f, 88.0f),
		false,
		{MakePreviewPin(TEXT("then"), EPinDirection::Output, true)},
		ENodeRole::EventEntry);
	AddPreviewNode(
		OutSample,
		TEXT("ResetState"),
		TEXT("K2Node_CallFunction"),
		TEXT("Reset State"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(80.0f, 24.0f),
		FVector2D(228.0f, 96.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		},
		ENodeRole::ExecNode);
	AddPreviewNode(
		OutSample,
		TEXT("SetCounter"),
		TEXT("K2Node_VariableSet"),
		TEXT("Set Counter"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::ExecNode,
		FVector2D(120.0f, 24.0f),
		FVector2D(236.0f, 104.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("In"), EPinDirection::Input, false, TEXT("int")),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		});
	AddPreviewNode(
		OutSample,
		TEXT("PrintLabel"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print String"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(160.0f, 24.0f),
		FVector2D(232.0f, 96.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("In"), EPinDirection::Input, false, TEXT("string")),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		});
	AddPreviewNode(
		OutSample,
		TEXT("DelayAsync"),
		TEXT("K2Node_AsyncAction"),
		TEXT("Delay"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::AsyncNode,
		FVector2D(200.0f, 24.0f),
		FVector2D(244.0f, 104.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("Completed"), EPinDirection::Output, true)
		});

	return AddPreviewLink(OutSample, TEXT("EventStart"), TEXT("then"), TEXT("ResetState"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("ResetState"), TEXT("then"), TEXT("SetCounter"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("SetCounter"), TEXT("then"), TEXT("PrintLabel"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("PrintLabel"), TEXT("then"), TEXT("DelayAsync"), TEXT("execute"), true, OutError);
}

static bool BuildPureDataSample(FGraphLayoutPreviewSample& OutSample, FString& OutError)
{
	ResetSample(ESemanticScene::PureDataSubgraph, OutSample);

	AddPreviewNode(
		OutSample,
		TEXT("EventStart"),
		TEXT("K2Node_CustomEvent"),
		TEXT("On Preview Trigger"),
		EGraphLayoutPreviewNodeFactory::CustomEvent,
		ENodeRole::EventEntry,
		FVector2D(0.0f, 0.0f),
		FVector2D(220.0f, 88.0f),
		false,
		{MakePreviewPin(TEXT("then"), EPinDirection::Output, true)});
	AddPreviewNode(
		OutSample,
		TEXT("SelfRef"),
		TEXT("K2Node_VariableGet"),
		TEXT("Self"),
		EGraphLayoutPreviewNodeFactory::Self,
		ENodeRole::VariableInput,
		FVector2D(24.0f, 200.0f),
		FVector2D(168.0f, 72.0f),
		false,
		{MakePreviewPin(TEXT("Value"), EPinDirection::Output, false)},
		ENodeRole::VariableInput);
	AddPreviewNode(
		OutSample,
		TEXT("ItemName"),
		TEXT("K2Node_VariableGet"),
		TEXT("Item Name"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::VariableInput,
		FVector2D(24.0f, 320.0f),
		FVector2D(184.0f, 72.0f),
		false,
		{MakePreviewPin(TEXT("Value"), EPinDirection::Output, false)});
	AddPreviewNode(
		OutSample,
		TEXT("ComposeKey"),
		TEXT("K2Node_CommutativeAssociativeBinaryOperator"),
		TEXT("+"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::OperatorOrCompare,
		FVector2D(240.0f, 252.0f),
		FVector2D(204.0f, 84.0f),
		false,
		{
			MakePreviewPin(TEXT("A"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("B"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("ReturnValue"), EPinDirection::Output, false)
		},
		ENodeRole::OperatorOrCompare);
	AddPreviewNode(
		OutSample,
		TEXT("BuildArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		EGraphLayoutPreviewNodeFactory::MakeArray,
		ENodeRole::PureFunction,
		FVector2D(500.0f, 220.0f),
		FVector2D(228.0f, 104.0f),
		false,
		{
			MakePreviewPin(TEXT("In0"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("In1"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("Array"), EPinDirection::Output, false, TEXT("array"))
		},
		ENodeRole::PureFunction);
	AddPreviewNode(
		OutSample,
		TEXT("ConsumeArray"),
		TEXT("K2Node_CallFunction"),
		TEXT("Submit Preview"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(820.0f, 180.0f),
		FVector2D(244.0f, 104.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("Array"), EPinDirection::Input, false, TEXT("array")),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		});

	return AddPreviewLink(OutSample, TEXT("EventStart"), TEXT("then"), TEXT("ConsumeArray"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("SelfRef"), TEXT("Value"), TEXT("ComposeKey"), TEXT("A"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("ItemName"), TEXT("Value"), TEXT("ComposeKey"), TEXT("B"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("ComposeKey"), TEXT("ReturnValue"), TEXT("BuildArray"), TEXT("In0"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("SelfRef"), TEXT("Value"), TEXT("BuildArray"), TEXT("In1"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("BuildArray"), TEXT("Array"), TEXT("ConsumeArray"), TEXT("Array"), false, OutError);
}

static bool BuildNodeInputClusterSample(FGraphLayoutPreviewSample& OutSample, FString& OutError)
{
	ResetSample(ESemanticScene::NodeInputCluster, OutSample);

	AddPreviewNode(
		OutSample,
		TEXT("EventStart"),
		TEXT("K2Node_CustomEvent"),
		TEXT("On Preview Trigger"),
		EGraphLayoutPreviewNodeFactory::CustomEvent,
		ENodeRole::EventEntry,
		FVector2D(0.0f, 0.0f),
		FVector2D(220.0f, 88.0f),
		false,
		{MakePreviewPin(TEXT("then"), EPinDirection::Output, true)});
	AddPreviewNode(
		OutSample,
		TEXT("Consumer"),
		TEXT("K2Node_CallFunction"),
		TEXT("Apply Material"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(760.0f, 140.0f),
		FVector2D(256.0f, 112.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("Context"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("Condition"), EPinDirection::Input, false, TEXT("bool")),
			MakePreviewPin(TEXT("Payload"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		},
		ENodeRole::ExecNode);
	AddPreviewNode(
		OutSample,
		TEXT("ContextGet"),
		TEXT("K2Node_VariableGet"),
		TEXT("Context Object"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::VariableInput,
		FVector2D(120.0f, 320.0f),
		FVector2D(188.0f, 72.0f),
		false,
		{MakePreviewPin(TEXT("Value"), EPinDirection::Output, false)},
		ENodeRole::VariableInput);
	AddPreviewNode(
		OutSample,
		TEXT("FlagGet"),
		TEXT("K2Node_VariableGet"),
		TEXT("Has Authority"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::VariableInput,
		FVector2D(120.0f, 440.0f),
		FVector2D(188.0f, 72.0f),
		false,
		{MakePreviewPin(TEXT("Value"), EPinDirection::Output, false, TEXT("bool"))});
	AddPreviewNode(
		OutSample,
		TEXT("IsValidGate"),
		TEXT("K2Node_PromotableOperator"),
		TEXT("Is Valid"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::OperatorOrCompare,
		FVector2D(320.0f, 420.0f),
		FVector2D(204.0f, 84.0f),
		false,
		{
			MakePreviewPin(TEXT("Value"), EPinDirection::Input, false, TEXT("bool")),
			MakePreviewPin(TEXT("ReturnValue"), EPinDirection::Output, false, TEXT("bool"))
		},
		ENodeRole::OperatorOrCompare);
	AddPreviewNode(
		OutSample,
		TEXT("ValueGet"),
		TEXT("K2Node_VariableGet"),
		TEXT("Preview Label"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::VariableInput,
		FVector2D(120.0f, 560.0f),
		FVector2D(192.0f, 72.0f),
		false,
		{MakePreviewPin(TEXT("Value"), EPinDirection::Output, false)});
	AddPreviewNode(
		OutSample,
		TEXT("NormalizeValue"),
		TEXT("K2Node_CallFunction"),
		TEXT("Normalize Label"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::PureFunction,
		FVector2D(360.0f, 540.0f),
		FVector2D(220.0f, 92.0f),
		false,
		{
			MakePreviewPin(TEXT("In"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("ReturnValue"), EPinDirection::Output, false)
		});
	AddPreviewNode(
		OutSample,
		TEXT("ComposePayload"),
		TEXT("K2Node_CallFunction"),
		TEXT("Compose Payload"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::PureFunction,
		FVector2D(600.0f, 520.0f),
		FVector2D(232.0f, 100.0f),
		false,
		{
			MakePreviewPin(TEXT("In"), EPinDirection::Input, false),
			MakePreviewPin(TEXT("ReturnValue"), EPinDirection::Output, false)
		},
		ENodeRole::PureFunction);

	return AddPreviewLink(OutSample, TEXT("EventStart"), TEXT("then"), TEXT("Consumer"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("ContextGet"), TEXT("Value"), TEXT("Consumer"), TEXT("Context"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("FlagGet"), TEXT("Value"), TEXT("IsValidGate"), TEXT("Value"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("IsValidGate"), TEXT("ReturnValue"), TEXT("Consumer"), TEXT("Condition"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("ValueGet"), TEXT("Value"), TEXT("NormalizeValue"), TEXT("In"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("NormalizeValue"), TEXT("ReturnValue"), TEXT("ComposePayload"), TEXT("In"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("ComposePayload"), TEXT("ReturnValue"), TEXT("Consumer"), TEXT("Payload"), false, OutError);
}

static bool BuildMultiExecSample(FGraphLayoutPreviewSample& OutSample, FString& OutError)
{
	ResetSample(ESemanticScene::MultiExecOutput, OutSample);

	AddPreviewNode(
		OutSample,
		TEXT("EventStart"),
		TEXT("K2Node_CustomEvent"),
		TEXT("On Preview Trigger"),
		EGraphLayoutPreviewNodeFactory::CustomEvent,
		ENodeRole::EventEntry,
		FVector2D(0.0f, 0.0f),
		FVector2D(220.0f, 88.0f),
		false,
		{MakePreviewPin(TEXT("then"), EPinDirection::Output, true)},
		ENodeRole::EventEntry);
	AddPreviewNode(
		OutSample,
		TEXT("Sequence"),
		TEXT("K2Node_ExecutionSequence"),
		TEXT("Sequence"),
		EGraphLayoutPreviewNodeFactory::ExecutionSequence,
		ENodeRole::BranchControl,
		FVector2D(180.0f, 32.0f),
		FVector2D(236.0f, 104.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("Then_0"), EPinDirection::Output, true),
			MakePreviewPin(TEXT("Then_1"), EPinDirection::Output, true)
		});
	AddPreviewNode(
		OutSample,
		TEXT("PrimaryPrint"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print Primary"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(420.0f, 0.0f),
		FVector2D(224.0f, 92.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		},
		ENodeRole::ExecNode);
	AddPreviewNode(
		OutSample,
		TEXT("Branch"),
		TEXT("K2Node_IfThenElse"),
		TEXT("Branch"),
		EGraphLayoutPreviewNodeFactory::IfThenElse,
		ENodeRole::BranchControl,
		FVector2D(420.0f, 220.0f),
		FVector2D(228.0f, 104.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("Condition"), EPinDirection::Input, false, TEXT("bool")),
			MakePreviewPin(TEXT("Then"), EPinDirection::Output, true),
			MakePreviewPin(TEXT("Else"), EPinDirection::Output, true)
		},
		ENodeRole::BranchControl);
	AddPreviewNode(
		OutSample,
		TEXT("BranchCondition"),
		TEXT("K2Node_VariableGet"),
		TEXT("Should Branch"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::VariableInput,
		FVector2D(180.0f, 300.0f),
		FVector2D(188.0f, 72.0f),
		false,
		{MakePreviewPin(TEXT("Value"), EPinDirection::Output, false, TEXT("bool"))});
	AddPreviewNode(
		OutSample,
		TEXT("BranchPrint"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print Branch"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(700.0f, 220.0f),
		FVector2D(220.0f, 92.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		});
	AddPreviewNode(
		OutSample,
		TEXT("CompletedPrint"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print Completed"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(700.0f, 20.0f),
		FVector2D(236.0f, 92.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		});

	return AddPreviewLink(OutSample, TEXT("EventStart"), TEXT("then"), TEXT("Sequence"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("Sequence"), TEXT("Then_0"), TEXT("PrimaryPrint"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("Sequence"), TEXT("Then_1"), TEXT("Branch"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("BranchCondition"), TEXT("Value"), TEXT("Branch"), TEXT("Condition"), false, OutError) &&
		AddPreviewLink(OutSample, TEXT("Branch"), TEXT("Then"), TEXT("BranchPrint"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("PrimaryPrint"), TEXT("then"), TEXT("CompletedPrint"), TEXT("execute"), true, OutError);
}

static bool BuildOccupancySample(FGraphLayoutPreviewSample& OutSample, FString& OutError)
{
	ResetSample(ESemanticScene::Occupancy, OutSample);

	AddPreviewNode(
		OutSample,
		TEXT("EventStart"),
		TEXT("K2Node_CustomEvent"),
		TEXT("On Preview Trigger"),
		EGraphLayoutPreviewNodeFactory::CustomEvent,
		ENodeRole::EventEntry,
		FVector2D(0.0f, 0.0f),
		FVector2D(220.0f, 88.0f),
		false,
		{MakePreviewPin(TEXT("then"), EPinDirection::Output, true)});
	AddPreviewNode(
		OutSample,
		TEXT("CandidateExec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Candidate Step"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(80.0f, 0.0f),
		FVector2D(228.0f, 96.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		},
		ENodeRole::ExecNode);
	AddPreviewNode(
		OutSample,
		TEXT("FallbackExec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Fallback Step"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::ExecNode,
		FVector2D(120.0f, 0.0f),
		FVector2D(228.0f, 96.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		});
	AddPreviewNode(
		OutSample,
		TEXT("DelayAsync"),
		TEXT("K2Node_AsyncAction"),
		TEXT("Delay"),
		EGraphLayoutPreviewNodeFactory::GenericK2,
		ENodeRole::AsyncNode,
		FVector2D(160.0f, 0.0f),
		FVector2D(236.0f, 100.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("Completed"), EPinDirection::Output, true)
		},
		ENodeRole::AsyncNode);
	AddPreviewNode(
		OutSample,
		TEXT("CommentBlocker"),
		TEXT("EdGraphNode_Comment"),
		TEXT("Existing Comment"),
		EGraphLayoutPreviewNodeFactory::Comment,
		ENodeRole::Comment,
		FVector2D(360.0f, 0.0f),
		FVector2D(340.0f, 120.0f),
		true,
		{},
		ENodeRole::Comment);
	AddPreviewNode(
		OutSample,
		TEXT("ExistingGuard"),
		TEXT("K2Node_CallFunction"),
		TEXT("Existing Guard"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(720.0f, 0.0f),
		FVector2D(236.0f, 96.0f),
		true,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		});

	return AddPreviewLink(OutSample, TEXT("EventStart"), TEXT("then"), TEXT("CandidateExec"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("CandidateExec"), TEXT("then"), TEXT("FallbackExec"), TEXT("execute"), true, OutError) &&
		AddPreviewLink(OutSample, TEXT("FallbackExec"), TEXT("then"), TEXT("DelayAsync"), TEXT("execute"), true, OutError);
}

bool FGraphLayoutPreviewSampleFactory::BuildSample(
	const ESemanticScene Scene,
	FGraphLayoutPreviewSample& OutSample,
	FString& OutError)
{
	OutError.Reset();

	switch (Scene)
	{
	case ESemanticScene::LinearExecChain:
		return BuildLinearExecSample(OutSample, OutError);
	case ESemanticScene::PureDataSubgraph:
		return BuildPureDataSample(OutSample, OutError);
	case ESemanticScene::NodeInputCluster:
		return BuildNodeInputClusterSample(OutSample, OutError);
	case ESemanticScene::MultiExecOutput:
		return BuildMultiExecSample(OutSample, OutError);
	case ESemanticScene::Occupancy:
		return BuildOccupancySample(OutSample, OutError);
	default:
		OutError = FString::Printf(TEXT("unsupported preview semantic scene: %s"), ToString(Scene));
		OutSample = FGraphLayoutPreviewSample();
		return false;
	}
}
}
