#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewInteractionModel.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"
#include "UObject/Object.h"

namespace BlueprintHelper::GraphLayout
{
bool FGraphLayoutPreviewInteractionModel::Initialize(
	const FGraphLayoutPreviewMaterializerResult& MaterializerResult,
	UEdGraph* PreviewGraph)
{
	Reset();
	if (!PreviewGraph)
	{
		return false;
	}

	InitialNodeCount = PreviewGraph->Nodes.Num();
	for (UEdGraphNode* Node : PreviewGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		const FString* NodeId = MaterializerResult.NodeIdsByGuid.Find(Node->NodeGuid);
		const ENodeRole* Role = MaterializerResult.RolesByGuid.Find(Node->NodeGuid);
		const ENodeRole* AnchorRole = MaterializerResult.AnchorRolesByGuid.Find(Node->NodeGuid);
		if (!NodeId || !Role || !AnchorRole)
		{
			continue;
		}

		FTrackedNode Tracked;
		Tracked.NodeId = *NodeId;
		Tracked.NodeGuid = Node->NodeGuid;
		Tracked.Role = *Role;
		Tracked.AnchorRole = *AnchorRole;
		Tracked.BeginTopLeft = FVector2D(static_cast<float>(Node->NodePosX), static_cast<float>(Node->NodePosY));
		Tracked.LastTopLeft = Tracked.BeginTopLeft;
		Tracked.Size = ResolveNodeSize(*Node);
		TrackedNodesByGuid.Add(Node->NodeGuid, Tracked);
	}

	if (!CaptureTopology(
		PreviewGraph,
		InitialNodeSignaturesByGuid,
		InitialLinkSignatures,
		InitialLinkEndpointCount))
	{
		Reset();
		return false;
	}

	return TrackedNodesByGuid.Num() > 0;
}

void FGraphLayoutPreviewInteractionModel::Reset()
{
	TrackedNodesByGuid.Reset();
	InitialNodeSignaturesByGuid.Reset();
	InitialLinkSignatures.Reset();
	InitialNodeCount = 0;
	InitialLinkEndpointCount = 0;
	bInteractionActive = false;
}

void FGraphLayoutPreviewInteractionModel::BeginInteraction(UEdGraph* PreviewGraph)
{
	TMap<FGuid, FVector2D> Positions;
	if (!CapturePositions(PreviewGraph, Positions))
	{
		bInteractionActive = false;
		return;
	}

	for (TPair<FGuid, FTrackedNode>& Pair : TrackedNodesByGuid)
	{
		if (const FVector2D* Position = Positions.Find(Pair.Key))
		{
			Pair.Value.BeginTopLeft = *Position;
			Pair.Value.LastTopLeft = *Position;
		}
	}
	bInteractionActive = true;
}

bool FGraphLayoutPreviewInteractionModel::EndInteraction(
	UEdGraph* PreviewGraph,
	FGraphLayoutPreviewInteractionCommit& OutCommit)
{
	OutCommit = FGraphLayoutPreviewInteractionCommit();
	if (!bInteractionActive)
	{
		return false;
	}
	bInteractionActive = false;

	FString RejectionReason;
	if (!ValidateMoveOnly(PreviewGraph, RejectionReason))
	{
		OutCommit.RejectionReason = RejectionReason;
		return false;
	}

	TMap<FGuid, FVector2D> Positions;
	if (!CapturePositions(PreviewGraph, Positions))
	{
		OutCommit.RejectionReason = TEXT("preview graph positions could not be sampled");
		return false;
	}

	for (const TPair<FGuid, FTrackedNode>& Pair : TrackedNodesByGuid)
	{
		const FVector2D* EndTopLeft = Positions.Find(Pair.Key);
		if (!EndTopLeft || EndTopLeft->Equals(Pair.Value.BeginTopLeft))
		{
			continue;
		}

		FGraphLayoutPreviewMovedNode& Moved = OutCommit.MovedNodes.AddDefaulted_GetRef();
		Moved.NodeId = Pair.Value.NodeId;
		Moved.NodeGuid = Pair.Value.NodeGuid;
		Moved.Role = Pair.Value.Role;
		Moved.AnchorRole = Pair.Value.AnchorRole;
		Moved.BeginTopLeft = Pair.Value.BeginTopLeft;
		Moved.EndTopLeft = *EndTopLeft;
		Moved.Size = Pair.Value.Size;
	}

	return OutCommit.MovedNodes.Num() > 0;
}

bool FGraphLayoutPreviewInteractionModel::HasActiveInteraction() const
{
	return bInteractionActive;
}

bool FGraphLayoutPreviewInteractionModel::BuildRuleSetJsonForCommit(
	const FString& InputRuleSetJson,
	const ESemanticScene Scene,
	const FGraphLayoutPreviewInteractionCommit& Commit,
	FString& OutRuleSetJson,
	FString& OutError)
{
	OutRuleSetJson.Reset();
	OutError.Reset();

	FRuleSet RuleSet;
	FValidationResult Validation;
	if (!FRuleSetJson::ImportString(InputRuleSetJson, RuleSet, Validation))
	{
		OutError = Validation.Errors.Num() > 0
			? FString::Join(Validation.Errors, TEXT(" "))
			: TEXT("RuleSet JSON is invalid.");
		return false;
	}

	FEditorCanvasSceneState SceneState = FSemanticSceneAdapter::ResolveSceneState(RuleSet, Scene);
	for (const FGraphLayoutPreviewMovedNode& Moved : Commit.MovedNodes)
	{
		if (Moved.AnchorRole == ENodeRole::Unknown)
		{
			continue;
		}

		const FVector2D Center = Moved.EndTopLeft + Moved.Size * 0.5;
		SceneState.RoleCenters.Add(Moved.AnchorRole, Center);
	}

	FSemanticSceneAdapter::ApplySceneStateToRuleSet(
		Scene,
		SceneState,
		RuleSet,
		1.0f);
	OutRuleSetJson = FRuleSetJson::ExportString(RuleSet);
	return true;
}

bool FGraphLayoutPreviewInteractionModel::CapturePositions(
	UEdGraph* PreviewGraph,
	TMap<FGuid, FVector2D>& OutPositions) const
{
	OutPositions.Reset();
	if (!PreviewGraph)
	{
		return false;
	}

	for (UEdGraphNode* Node : PreviewGraph->Nodes)
	{
		if (!Node || !TrackedNodesByGuid.Contains(Node->NodeGuid))
		{
			continue;
		}

		OutPositions.Add(
			Node->NodeGuid,
			FVector2D(static_cast<float>(Node->NodePosX), static_cast<float>(Node->NodePosY)));
	}

	return OutPositions.Num() == TrackedNodesByGuid.Num();
}

bool FGraphLayoutPreviewInteractionModel::CaptureTopology(
	UEdGraph* PreviewGraph,
	TMap<FGuid, FString>& OutNodeSignatures,
	TSet<FString>& OutLinkSignatures,
	int32& OutLinkEndpointCount) const
{
	OutNodeSignatures.Reset();
	OutLinkSignatures.Reset();
	OutLinkEndpointCount = 0;

	if (!PreviewGraph)
	{
		return false;
	}

	for (UEdGraphNode* Node : PreviewGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		OutNodeSignatures.Add(Node->NodeGuid, BuildNodeSignature(*Node));
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!LinkedPin || !LinkedNode)
				{
					continue;
				}

				++OutLinkEndpointCount;
				OutLinkSignatures.Add(BuildLinkSignature(*Node, *Pin, *LinkedNode, *LinkedPin));
			}
		}
	}

	return true;
}

bool FGraphLayoutPreviewInteractionModel::ValidateMoveOnly(UEdGraph* PreviewGraph, FString& OutReason) const
{
	if (!PreviewGraph)
	{
		OutReason = TEXT("preview graph is invalid");
		return false;
	}

	if (PreviewGraph->Nodes.Num() != InitialNodeCount)
	{
		OutReason = FString::Printf(
			TEXT("preview graph node count changed from %d to %d"),
			InitialNodeCount,
			PreviewGraph->Nodes.Num());
		return false;
	}

	TSet<FGuid> CurrentGuids;
	for (UEdGraphNode* Node : PreviewGraph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		CurrentGuids.Add(Node->NodeGuid);
	}

	for (const TPair<FGuid, FTrackedNode>& Pair : TrackedNodesByGuid)
	{
		if (!CurrentGuids.Contains(Pair.Key))
		{
			OutReason = FString::Printf(TEXT("preview graph tracked node disappeared: %s"), *Pair.Value.NodeId);
			return false;
		}
	}

	if (CurrentGuids.Num() != TrackedNodesByGuid.Num())
	{
		OutReason = TEXT("preview graph contains untracked nodes");
		return false;
	}

	TMap<FGuid, FString> CurrentNodeSignaturesByGuid;
	TSet<FString> CurrentLinkSignatures;
	int32 CurrentLinkEndpointCount = 0;
	if (!CaptureTopology(
		PreviewGraph,
		CurrentNodeSignaturesByGuid,
		CurrentLinkSignatures,
		CurrentLinkEndpointCount))
	{
		OutReason = TEXT("preview graph topology could not be sampled");
		return false;
	}

	if (CurrentNodeSignaturesByGuid.Num() != InitialNodeSignaturesByGuid.Num())
	{
		OutReason = TEXT("preview graph node/pin signature count changed");
		return false;
	}

	for (const TPair<FGuid, FString>& Pair : InitialNodeSignaturesByGuid)
	{
		const FString* CurrentSignature = CurrentNodeSignaturesByGuid.Find(Pair.Key);
		if (!CurrentSignature)
		{
			OutReason = TEXT("preview graph node/pin signature disappeared");
			return false;
		}

		if (*CurrentSignature != Pair.Value)
		{
			const FString NodeId = TrackedNodesByGuid.Contains(Pair.Key)
				? TrackedNodesByGuid.FindChecked(Pair.Key).NodeId
				: Pair.Key.ToString(EGuidFormats::DigitsWithHyphens);
			OutReason = FString::Printf(TEXT("preview graph node/pin signature changed: %s"), *NodeId);
			return false;
		}
	}

	if (CurrentLinkEndpointCount != InitialLinkEndpointCount)
	{
		OutReason = FString::Printf(
			TEXT("preview graph link endpoint count changed from %d to %d"),
			InitialLinkEndpointCount,
			CurrentLinkEndpointCount);
		return false;
	}

	if (CurrentLinkSignatures.Num() != InitialLinkSignatures.Num())
	{
		OutReason = TEXT("preview graph link signature count changed");
		return false;
	}

	for (const FString& LinkSignature : InitialLinkSignatures)
	{
		if (!CurrentLinkSignatures.Contains(LinkSignature))
		{
			OutReason = TEXT("preview graph link topology changed");
			return false;
		}
	}

	return true;
}

FVector2D FGraphLayoutPreviewInteractionModel::ResolveNodeSize(const UEdGraphNode& Node) const
{
	const float Width = Node.NodeWidth > 0 ? static_cast<float>(Node.NodeWidth) : 240.0f;
	const float Height = Node.NodeHeight > 0 ? static_cast<float>(Node.NodeHeight) : 120.0f;
	return FVector2D(Width, Height);
}

FString FGraphLayoutPreviewInteractionModel::BuildNodeSignature(const UEdGraphNode& Node) const
{
	TArray<FString> PinSignatures;
	PinSignatures.Reserve(Node.Pins.Num());
	for (const UEdGraphPin* Pin : Node.Pins)
	{
		if (Pin)
		{
			PinSignatures.Add(BuildPinSignature(Node, *Pin));
		}
	}
	PinSignatures.Sort();

	return FString::Printf(
		TEXT("node=%s|class=%s|title=%s|comment=%s|pins=[%s]"),
		*Node.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens),
		Node.GetClass() ? *Node.GetClass()->GetPathName() : TEXT(""),
		*Node.GetNodeTitle(ENodeTitleType::FullTitle).ToString(),
		*Node.NodeComment,
		*FString::Join(PinSignatures, TEXT(";")));
}

FString FGraphLayoutPreviewInteractionModel::BuildPinSignature(
	const UEdGraphNode& Node,
	const UEdGraphPin& Pin) const
{
	const UObject* const SubCategoryObject = Pin.PinType.PinSubCategoryObject.Get();
	const UObject* const DefaultObject = Pin.DefaultObject.Get();
	return FString::Printf(
		TEXT("owner=%s|pin=%s|name=%s|direction=%d|category=%s|subcategory=%s|subobject=%s|default=%s|autodefault=%s|defaulttext=%s|defaultobject=%s"),
		*Node.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens),
		*Pin.PinId.ToString(EGuidFormats::DigitsWithHyphens),
		*Pin.PinName.ToString(),
		static_cast<int32>(Pin.Direction),
		*Pin.PinType.PinCategory.ToString(),
		*Pin.PinType.PinSubCategory.ToString(),
		SubCategoryObject ? *SubCategoryObject->GetPathName() : TEXT(""),
		*Pin.DefaultValue,
		*Pin.AutogeneratedDefaultValue,
		*Pin.DefaultTextValue.ToString(),
		DefaultObject ? *DefaultObject->GetPathName() : TEXT(""));
}

FString FGraphLayoutPreviewInteractionModel::BuildLinkEndpointSignature(
	const UEdGraphNode& Node,
	const UEdGraphPin& Pin) const
{
	return FString::Printf(
		TEXT("node=%s|pin=%s|name=%s|direction=%d"),
		*Node.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens),
		*Pin.PinId.ToString(EGuidFormats::DigitsWithHyphens),
		*Pin.PinName.ToString(),
		static_cast<int32>(Pin.Direction));
}

FString FGraphLayoutPreviewInteractionModel::BuildLinkSignature(
	const UEdGraphNode& FromNode,
	const UEdGraphPin& FromPin,
	const UEdGraphNode& ToNode,
	const UEdGraphPin& ToPin) const
{
	const FString FromEndpoint = BuildLinkEndpointSignature(FromNode, FromPin);
	const FString ToEndpoint = BuildLinkEndpointSignature(ToNode, ToPin);
	return FromEndpoint <= ToEndpoint
		? FString::Printf(TEXT("%s -> %s"), *FromEndpoint, *ToEndpoint)
		: FString::Printf(TEXT("%s -> %s"), *ToEndpoint, *FromEndpoint);
}
}
