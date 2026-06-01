#include "Systems/Debug/BlueprintHelperEditorFocusService.h"

#include "BlueprintEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteBlockScopedResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

class FBlueprintHelperEditorFocusServiceLocalUtils
{
public:
	static FBlueprintHelperEditorFocusResult MakeBaseResult(
		const FBlueprintHelperEditorFocusRequest& Request)
	{
		FBlueprintHelperEditorFocusResult Result;
		Result.AssetPath = Request.AssetPath;
		Result.GraphName = Request.GraphName;
		Result.BlockRef = Request.BlockRef;
		Result.NodeRef = Request.NodeRef;
		return Result;
	}

	static FBlueprintHelperEditorFocusResult Failure(
		const FBlueprintHelperEditorFocusRequest& Request,
		const FString& Code,
		const FString& Message)
	{
		FBlueprintHelperEditorFocusResult Result = MakeBaseResult(Request);
		Result.bSuccess = false;
		Result.ErrorCode = Code;
		Result.Message = Message;
		return Result;
	}

	static FBlueprintHelperEditorFocusResult Success(
		const FBlueprintHelperEditorFocusRequest& Request,
		const FString& FocusedObjectName,
		const FString& Message)
	{
		FBlueprintHelperEditorFocusResult Result = MakeBaseResult(Request);
		Result.bSuccess = true;
		Result.FocusedObjectName = FocusedObjectName;
		Result.Message = Message;
		return Result;
	}

	static FString FirstDiagnosticMessage(const FBlueprintHelperDiagnosticSet& Diagnostics)
	{
		return Diagnostics.Items.Num() > 0
			? Diagnostics.Items[0].Message
			: FString(TEXT("Blueprint editor target resolution failed."));
	}

	static bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	static bool AddUniqueNode(TArray<UEdGraphNode*>& Nodes, UEdGraphNode* Node)
	{
		if (!Node || Nodes.Contains(Node))
		{
			return false;
		}
		Nodes.Add(Node);
		return true;
	}

	static bool IsNodeOwnedByBlock(UEdGraphNode* Node, const FString& BlockId)
	{
		if (!Node || BlockId.IsEmpty())
		{
			return false;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			return false;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		const FString NodeBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		const FString OwnedValue = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
		return NodeBlockId == BlockId && OwnedValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	static void SortNodesByGraphOrder(UEdGraph* Graph, TArray<UEdGraphNode*>& Nodes)
	{
		if (!Graph)
		{
			return;
		}
		Nodes.Sort([Graph](const UEdGraphNode& Left, const UEdGraphNode& Right)
		{
			return Graph->Nodes.IndexOfByKey(&Left) < Graph->Nodes.IndexOfByKey(&Right);
		});
	}
};

FBlueprintHelperEditorFocusService::FBlueprintHelperEditorFocusService(
	const FBlueprintHelperAssetBrowseService& InAssetBrowseService,
	const FBlueprintHelperGraphResolver& InGraphResolver,
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperLogicJsonPathService& InPathService)
	: AssetBrowseService(InAssetBrowseService)
	, GraphResolver(InGraphResolver)
	, BlockIdService(InBlockIdService)
	, PathService(InPathService)
{
}

FBlueprintHelperEditorFocusResult FBlueprintHelperEditorFocusService::FocusBlueprintEditorTarget(
	const FBlueprintHelperEditorFocusRequest& Request) const
{
	ClearLastFocusedGraphSelection();
	if (Request.AssetPath.IsEmpty())
	{
		return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(
			Request,
			TEXT("asset_path_required"),
			TEXT("asset_path is required."));
	}

	FString OpenError;
	if (!AssetBrowseService.OpenAsset(Request.AssetPath, OpenError))
	{
		return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(
			Request,
			TEXT("asset_open_failed"),
			OpenError);
	}

	if (Request.GraphName.IsEmpty() && Request.BlockRef.IsEmpty() && Request.NodeRef.IsEmpty())
	{
		return FBlueprintHelperEditorFocusServiceLocalUtils::Success(
			Request,
			Request.AssetPath,
			TEXT("Asset editor opened."));
	}

	return ResolveAndFocusGraph(Request);
}

FBlueprintHelperEditorFocusResult FBlueprintHelperEditorFocusService::ResolveAndFocusGraph(
	const FBlueprintHelperEditorFocusRequest& Request) const
{
	if (Request.GraphName.IsEmpty())
	{
		return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(
			Request,
			TEXT("graph_name_required"),
			TEXT("graph_name is required when block_ref or node_ref is provided."));
	}

	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = Request.AssetPath;
	Target.GraphName = Request.GraphName;
	FBlueprintHelperDiagnosticSet Diagnostics;
	UEdGraph* Graph = GraphResolver.ResolveGraph(Target, Diagnostics);
	if (!Graph)
	{
		return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(
			Request,
			TEXT("graph_not_found"),
			FBlueprintHelperEditorFocusServiceLocalUtils::FirstDiagnosticMessage(Diagnostics));
	}

	UObject* ObjectToFocus = Graph;
	UEdGraphNode* NodeToFocus = nullptr;
	TArray<UEdGraphNode*> NodesToSelect;
	if (!Request.BlockRef.IsEmpty() || !Request.NodeRef.IsEmpty())
	{
		const FString FullBlockId = !Request.BlockRef.IsEmpty()
			? BlockIdService.MakeFullBlockId(Request.GraphName, Request.BlockRef)
			: FString();
		if (!Request.NodeRef.IsEmpty())
		{
			UEdGraphNode* Node = nullptr;
			FString ErrorCode;
			FString ErrorMessage;
			if (!TryResolveNode(Graph, Request, Node, ErrorCode, ErrorMessage))
			{
				return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(Request, ErrorCode, ErrorMessage);
			}
			ObjectToFocus = Node;
			NodeToFocus = Node;
			CollectEventLogicNodes(Node, NodesToSelect);
			if (!FullBlockId.IsEmpty())
			{
				NodesToSelect.RemoveAll([&FullBlockId](UEdGraphNode* Candidate)
				{
					return !FBlueprintHelperEditorFocusServiceLocalUtils::IsNodeOwnedByBlock(Candidate, FullBlockId);
				});
				FBlueprintHelperEditorFocusServiceLocalUtils::AddUniqueNode(NodesToSelect, Node);
			}
		}
		else if (!CollectBlockNodes(Graph, FullBlockId, NodesToSelect))
		{
			return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(
				Request,
				TEXT("block_nodes_not_found"),
				FString::Printf(TEXT("No BlueprintHelper-owned nodes were found for block_ref %s."), *Request.BlockRef));
		}
		FBlueprintHelperEditorFocusServiceLocalUtils::SortNodesByGraphOrder(Graph, NodesToSelect);
	}
	else
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			FBlueprintHelperEditorFocusServiceLocalUtils::AddUniqueNode(NodesToSelect, Node);
		}
	}

	if (!ObjectToFocus)
	{
		return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(
			Request,
			TEXT("focus_target_not_found"),
			TEXT("Resolved focus target is null."));
	}

	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ObjectToFocus, false);
	if (NodesToSelect.Num() > 0 && !FocusNodeSetInGraphEditor(Graph, NodesToSelect))
	{
		return FBlueprintHelperEditorFocusServiceLocalUtils::Failure(
			Request,
			TEXT("graph_editor_focus_failed"),
			TEXT("Resolved Graph nodes could not be selected and zoomed in the Blueprint graph editor."));
	}
	StoreLastFocusedGraphSelection(Graph, NodesToSelect);
	return FBlueprintHelperEditorFocusServiceLocalUtils::Success(
		Request,
		NodeToFocus ? NodeToFocus->GetPathName() : ObjectToFocus->GetPathName(),
		TEXT("Blueprint editor target focused."));
}

bool FBlueprintHelperEditorFocusService::TryGetLastFocusedGraphSelection(
	FBlueprintHelperEditorFocusedGraphSelection& OutSelection) const
{
	OutSelection = LastFocusedGraphSelection;
	if (!OutSelection.bHasSelection || !OutSelection.Graph)
	{
		return false;
	}
	OutSelection.Nodes.RemoveAll([](UEdGraphNode* Node)
	{
		return Node == nullptr;
	});
	return OutSelection.Nodes.Num() > 0;
}

bool FBlueprintHelperEditorFocusService::TryResolveNode(
	UEdGraph* Graph,
	const FBlueprintHelperEditorFocusRequest& Request,
	UEdGraphNode*& OutNode,
	FString& OutErrorCode,
	FString& OutErrorMessage) const
{
	OutNode = nullptr;
	OutErrorCode.Empty();
	OutErrorMessage.Empty();

	FBlueprintHelperPatchResolveError ResolveError;
	if (!Request.BlockRef.IsEmpty())
	{
		FBlueprintHelperGraphWriteAnchorRef Anchor;
		Anchor.BlockId = BlockIdService.MakeFullBlockId(Request.GraphName, Request.BlockRef);
		Anchor.GroupEntryNodePath = TEXT("nodes[0]");
		Anchor.NodeRef = Request.NodeRef;
		if (FBlueprintHelperGraphWriteBlockScopedResolver::ResolveNode(PathService, Graph, Anchor, OutNode, ResolveError))
		{
			return true;
		}
		OutErrorCode = ResolveError.Code.IsEmpty() ? TEXT("block_node_not_found") : ResolveError.Code;
		OutErrorMessage = ResolveError.Message;
		return false;
	}

	if (PathService.ResolveNode(Graph, Request.NodeRef, FString(), OutNode, ResolveError))
	{
		return true;
	}
	OutErrorCode = ResolveError.Code.IsEmpty() ? TEXT("node_not_found") : ResolveError.Code;
	OutErrorMessage = ResolveError.Message;
	return false;
}

bool FBlueprintHelperEditorFocusService::FocusResolvedNodeInGraphEditor(
	UEdGraph* Graph,
	UEdGraphNode* Node) const
{
	TArray<UEdGraphNode*> Nodes;
	FBlueprintHelperEditorFocusServiceLocalUtils::AddUniqueNode(Nodes, Node);
	return FocusNodeSetInGraphEditor(Graph, Nodes);
}

bool FBlueprintHelperEditorFocusService::FocusNodeSetInGraphEditor(
	UEdGraph* Graph,
	const TArray<UEdGraphNode*>& Nodes) const
{
	if (!Graph || Nodes.Num() == 0)
	{
		return false;
	}

	TSharedPtr<IBlueprintEditor> BlueprintEditor =
		FKismetEditorUtilities::GetIBlueprintEditorForObject(Nodes[0], true);
	if (!BlueprintEditor.IsValid())
	{
		BlueprintEditor = FKismetEditorUtilities::GetIBlueprintEditorForObject(Graph, true);
	}
	if (!BlueprintEditor.IsValid())
	{
		return false;
	}

	TSharedPtr<SGraphEditor> GraphEditor = BlueprintEditor->OpenGraphAndBringToFront(Graph, true);
	if (!GraphEditor.IsValid())
	{
		return false;
	}

	GraphEditor->ClearSelectionSet();
	for (UEdGraphNode* Node : Nodes)
	{
		if (Node)
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
	}
	GraphEditor->ZoomToFit(true);
	return true;
}

bool FBlueprintHelperEditorFocusService::CollectBlockNodes(
	UEdGraph* Graph,
	const FString& BlockId,
	TArray<UEdGraphNode*>& OutNodes) const
{
	OutNodes.Reset();
	if (!Graph || BlockId.IsEmpty())
	{
		return false;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (FBlueprintHelperEditorFocusServiceLocalUtils::IsNodeOwnedByBlock(Node, BlockId))
		{
			FBlueprintHelperEditorFocusServiceLocalUtils::AddUniqueNode(OutNodes, Node);
		}
	}
	return OutNodes.Num() > 0;
}

void FBlueprintHelperEditorFocusService::CollectEventLogicNodes(
	UEdGraphNode* EntryNode,
	TArray<UEdGraphNode*>& OutNodes) const
{
	OutNodes.Reset();
	if (!EntryNode)
	{
		return;
	}

	TArray<UEdGraphNode*> ExecQueue;
	FBlueprintHelperEditorFocusServiceLocalUtils::AddUniqueNode(OutNodes, EntryNode);
	ExecQueue.Add(EntryNode);

	for (int32 QueueIndex = 0; QueueIndex < ExecQueue.Num(); ++QueueIndex)
	{
		UEdGraphNode* CurrentNode = ExecQueue[QueueIndex];
		if (!CurrentNode)
		{
			continue;
		}

		for (UEdGraphPin* Pin : CurrentNode->Pins)
		{
			if (!FBlueprintHelperEditorFocusServiceLocalUtils::IsExecPin(Pin) ||
				Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (FBlueprintHelperEditorFocusServiceLocalUtils::AddUniqueNode(OutNodes, LinkedNode))
				{
					ExecQueue.Add(LinkedNode);
				}
			}
		}
	}

	const int32 ExecReachableNodeCount = OutNodes.Num();
	for (int32 NodeIndex = 0; NodeIndex < ExecReachableNodeCount; ++NodeIndex)
	{
		UEdGraphNode* CurrentNode = OutNodes[NodeIndex];
		if (!CurrentNode)
		{
			continue;
		}

		for (UEdGraphPin* Pin : CurrentNode->Pins)
		{
			if (!Pin || FBlueprintHelperEditorFocusServiceLocalUtils::IsExecPin(Pin))
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				FBlueprintHelperEditorFocusServiceLocalUtils::AddUniqueNode(OutNodes, LinkedNode);
			}
		}
	}

	if (UEdGraph* Graph = EntryNode->GetGraph())
	{
		FBlueprintHelperEditorFocusServiceLocalUtils::SortNodesByGraphOrder(Graph, OutNodes);
	}
}

void FBlueprintHelperEditorFocusService::StoreLastFocusedGraphSelection(
	UEdGraph* Graph,
	const TArray<UEdGraphNode*>& Nodes) const
{
	LastFocusedGraphSelection = FBlueprintHelperEditorFocusedGraphSelection();
	LastFocusedGraphSelection.Graph = Graph;
	LastFocusedGraphSelection.GraphName = Graph ? Graph->GetName() : FString();
	for (UEdGraphNode* Node : Nodes)
	{
		if (Node)
		{
			LastFocusedGraphSelection.Nodes.Add(Node);
		}
	}
	LastFocusedGraphSelection.bHasSelection =
		LastFocusedGraphSelection.Graph && LastFocusedGraphSelection.Nodes.Num() > 0;
}

void FBlueprintHelperEditorFocusService::ClearLastFocusedGraphSelection() const
{
	LastFocusedGraphSelection = FBlueprintHelperEditorFocusedGraphSelection();
}
