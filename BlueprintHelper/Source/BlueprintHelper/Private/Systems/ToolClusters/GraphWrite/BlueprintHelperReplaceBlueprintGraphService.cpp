// BlueprintHelper Service Layer 鈥?ReplaceBlueprintGraph 鏍稿績鏈嶅姟瀹炵幇

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceEntryResolver.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityDiagnosticsJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionResult.h"
#include "HAL/PlatformTime.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils
{
public:
	struct FReplaceRollbackExecBoundary
	{
		FGuid EntryNodeGuid;
		FGuid BodyNodeGuid;
		bool bValid = false;
	};

	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static bool TryReadBlueprintHelperBlockId(UEdGraphNode* Node, FString& OutBlockId)
	{
		OutBlockId.Reset();
		if (!Node)
		{
			return false;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			return false;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		if (MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) != TEXT("true"))
		{
			return false;
		}

		OutBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		return !OutBlockId.IsEmpty();
	}

	static bool HasInboundExecLinkFromImportedNode(UEdGraphPin* ExecInputPin, const TSet<UEdGraphNode*>& ImportedNodes)
	{
		if (!ExecInputPin)
		{
			return false;
		}

		for (UEdGraphPin* LinkedPin : ExecInputPin->LinkedTo)
		{
			if (!LinkedPin ||
				LinkedPin->Direction != EGPD_Output ||
				LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}

			if (ImportedNodes.Contains(LinkedPin->GetOwningNode()))
			{
				return true;
			}
		}

		return false;
	}

	static UEdGraphNode* FindFirstImportedExecutableBodyNode(
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport)
	{
		TArray<UEdGraphNode*> ImportedExecutableNodes;
		TSet<UEdGraphNode*> ImportedNodes;

		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || NodesBeforeImport.Contains(Node))
			{
				continue;
			}

			ImportedNodes.Add(Node);
			if (FindFirstExecPin(Node, EGPD_Input))
			{
				ImportedExecutableNodes.Add(Node);
			}
		}

		for (UEdGraphNode* Node : ImportedExecutableNodes)
		{
			if (!HasInboundExecLinkFromImportedNode(FindFirstExecPin(Node, EGPD_Input), ImportedNodes))
			{
				return Node;
			}
		}

		return ImportedExecutableNodes.Num() > 0 ? ImportedExecutableNodes[0] : nullptr;
	}

	static bool PinsHaveSingleConnectionToEachOther(UEdGraphPin* FirstPin, UEdGraphPin* SecondPin)
	{
		return FirstPin &&
			SecondPin &&
			FirstPin->LinkedTo.Num() == 1 &&
			SecondPin->LinkedTo.Num() == 1 &&
			FirstPin->LinkedTo[0] == SecondPin &&
			SecondPin->LinkedTo[0] == FirstPin;
	}

	static bool HasLinkedExecBody(UEdGraphNode* EntryNode)
	{
		UEdGraphPin* EntryExecOut = FindFirstExecPin(EntryNode, EGPD_Output);
		return EntryExecOut && EntryExecOut->LinkedTo.Num() > 0;
	}

	static bool HasExecPin(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return true;
			}
		}
		return false;
	}

	static bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	static TArray<UEdGraphNode*> CollectImportedNodes(
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport)
	{
		TArray<UEdGraphNode*> ImportedNodes;
		if (!Graph)
		{
			return ImportedNodes;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !NodesBeforeImport.Contains(Node))
			{
				ImportedNodes.Add(Node);
			}
		}
		return ImportedNodes;
	}

	static void CollectExecReachableFromBodyEntry(
		UEdGraphPin* BodyEntryPin,
		const TSet<UEdGraphNode*>& ImportedNodeSet,
		TSet<UEdGraphNode*>& OutReachable)
	{
		OutReachable.Empty();
		UEdGraphNode* BodyEntryNode = BodyEntryPin ? BodyEntryPin->GetOwningNode() : nullptr;
		if (!BodyEntryNode || !ImportedNodeSet.Contains(BodyEntryNode))
		{
			return;
		}

		TArray<UEdGraphNode*> PendingNodes;
		OutReachable.Add(BodyEntryNode);
		PendingNodes.Add(BodyEntryNode);

		while (PendingNodes.Num() > 0)
		{
			UEdGraphNode* Node = FBlueprintHelperVersionCompat::PopNoShrink(PendingNodes);
			if (!Node)
			{
				continue;
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin))
				{
					continue;
				}

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode && ImportedNodeSet.Contains(LinkedNode) && HasExecPin(LinkedNode) && !OutReachable.Contains(LinkedNode))
					{
						OutReachable.Add(LinkedNode);
						PendingNodes.Add(LinkedNode);
					}
				}
			}
		}
	}

	static void AddFunctionResultsReachedByImportedExecFlow(
		UEdGraph* Graph,
		TSet<UEdGraphNode*>& InOutBodyFlowNodes)
	{
		if (!Graph)
		{
			return;
		}

		bool bAddedNode = true;
		while (bAddedNode)
		{
			bAddedNode = false;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node);
				if (!ResultNode || InOutBodyFlowNodes.Contains(ResultNode))
				{
					continue;
				}

				UEdGraphPin* ResultExecInput = FindFirstExecPin(ResultNode, EGPD_Input);
				if (!ResultExecInput)
				{
					continue;
				}

				for (UEdGraphPin* LinkedPin : ResultExecInput->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode && InOutBodyFlowNodes.Contains(LinkedNode))
					{
						InOutBodyFlowNodes.Add(ResultNode);
						bAddedNode = true;
						break;
					}
				}
			}
		}
	}

	static bool DataChainReachesReachableExecConsumer(
		const UEdGraphNode* Node,
		const TSet<UEdGraphNode*>& ImportedNodeSet,
		const TSet<UEdGraphNode*>& ReachableExecNodes,
		TSet<const UEdGraphNode*>& VisitedNodes)
	{
		if (!Node || VisitedNodes.Contains(Node))
		{
			return false;
		}
		VisitedNodes.Add(Node);

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin))
			{
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || LinkedPin->Direction != EGPD_Input || IsExecPin(LinkedPin))
				{
					continue;
				}

				UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
				if (!LinkedNode || !ImportedNodeSet.Contains(LinkedNode))
				{
					continue;
				}

				if (HasExecPin(LinkedNode))
				{
					if (ReachableExecNodes.Contains(LinkedNode))
					{
						return true;
					}
					continue;
				}

				if (DataChainReachesReachableExecConsumer(LinkedNode, ImportedNodeSet, ReachableExecNodes, VisitedNodes))
				{
					return true;
				}
			}
		}

		return false;
	}

	static bool GeneratedPureDataChainsReachBodyEntryExecFlow(
		const TArray<UEdGraphNode*>& ImportedNodes,
		const TSet<UEdGraphNode*>& ImportedNodeSet,
		const TSet<UEdGraphNode*>& ReachableExecNodes)
	{
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (!Node || HasExecPin(Node))
			{
				continue;
			}

			TSet<const UEdGraphNode*> VisitedNodes;
			if (!DataChainReachesReachableExecConsumer(Node, ImportedNodeSet, ReachableExecNodes, VisitedNodes))
			{
				return false;
			}
		}
		return true;
	}

	static bool ImportedExecNodesReachBodyEntryExecFlow(
		const TArray<UEdGraphNode*>& ImportedNodes,
		const TSet<UEdGraphNode*>& ReachableExecNodes)
	{
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (HasExecPin(Node) && !ReachableExecNodes.Contains(Node))
			{
				return false;
			}
		}
		return true;
	}

	static bool CanDeferEntryResolvedConnectivityFailure(
		const EBlueprintHelperReplaceScope Scope,
		const FBlueprintGenerateResult& GenerateResult,
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport)
	{
		if (Scope != EBlueprintHelperReplaceScope::FunctionBody)
		{
			return false;
		}
		if (GenerateResult.bSucceed ||
			GenerateResult.UnresolvedNodeCount != 0 ||
			GenerateResult.ConnectivityDiagnostics.Num() == 0)
		{
			return false;
		}

		for (const FBlueprintGeneratorDiagnostic& Diagnostic : GenerateResult.ConnectivityDiagnostics)
		{
			if (Diagnostic.Code != TEXT("unreachable_exec_node") &&
				Diagnostic.Code != TEXT("unreachable_pure_data_chain") &&
				Diagnostic.Code != TEXT("missing_expected_link") &&
				Diagnostic.Code != TEXT("unconsumed_pure_data_node"))
			{
				return false;
			}
		}

		const TArray<UEdGraphNode*> ImportedNodes = CollectImportedNodes(Graph, NodesBeforeImport);
		TSet<UEdGraphNode*> ImportedNodeSet;
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (Node)
			{
				ImportedNodeSet.Add(Node);
			}
		}
		if (ImportedNodeSet.Num() == 0)
		{
			return false;
		}

		TSet<UEdGraphNode*> BodyFlowNodeSet = ImportedNodeSet;
		AddFunctionResultsReachedByImportedExecFlow(Graph, BodyFlowNodeSet);

		UEdGraphNode* BodyEntryNode = FindFirstImportedExecutableBodyNode(Graph, NodesBeforeImport);
		UEdGraphPin* BodyEntryPin = FindFirstExecPin(BodyEntryNode, EGPD_Input);
		if (!BodyEntryPin || !BodyFlowNodeSet.Contains(BodyEntryNode))
		{
			return false;
		}

		TSet<UEdGraphNode*> ReachableExecNodes;
		CollectExecReachableFromBodyEntry(BodyEntryPin, BodyFlowNodeSet, ReachableExecNodes);
		if (ReachableExecNodes.Num() == 0)
		{
			return false;
		}

		return ImportedExecNodesReachBodyEntryExecFlow(ImportedNodes, ReachableExecNodes) &&
			GeneratedPureDataChainsReachBodyEntryExecFlow(ImportedNodes, BodyFlowNodeSet, ReachableExecNodes);
	}

	static void BreakAllPinLinksWithModify(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return;
		}

		Pin->Modify();
		Pin->BreakAllPinLinks(true);
	}

	static FBlueprintHelperGraphReviewNodeAnchor MakeReviewNodeAnchor(const UEdGraphNode* Node)
	{
		FBlueprintHelperGraphReviewNodeAnchor Anchor;
		if (!Node)
		{
			return Anchor;
		}

		Anchor.NodePath = Node->GetPathName();
		Anchor.NodeGuid = Node->NodeGuid.IsValid()
			? Node->NodeGuid.ToString(EGuidFormats::Digits)
			: FString();
		Anchor.DisplayLabel = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		if (Anchor.DisplayLabel.IsEmpty())
		{
			Anchor.DisplayLabel = Node->GetName();
		}
		Anchor.GraphPosition = FVector2D(
			static_cast<float>(Node->NodePosX),
			static_cast<float>(Node->NodePosY));
		Anchor.GraphSize = FVector2D(
			Node->NodeWidth > 0 ? static_cast<float>(Node->NodeWidth) : 360.0f,
			Node->NodeHeight > 0 ? static_cast<float>(Node->NodeHeight) : 180.0f);
		Anchor.bHasGraphBounds = true;
		return Anchor;
	}

	static void AttachGraphWriteExecutionStats(
		TSharedPtr<FJsonObject> Data,
		const FBlueprintGraphWriteExecutionStats& Stats)
	{
		if (!Data.IsValid())
		{
			return;
		}

		Data->SetObjectField(
			TEXT("graph_write_execution_stats"),
			FBlueprintGraphWriteExecutionStatsSerializer::ToJson(Stats));
	}

	static void AddGuidVariants(TSet<FString>& OutGuids, const FString& GuidText)
	{
		if (GuidText.IsEmpty())
		{
			return;
		}

		OutGuids.Add(GuidText);
		FString Compact = GuidText;
		Compact.ReplaceInline(TEXT("-"), TEXT(""));
		Compact.ReplaceInline(TEXT("{"), TEXT(""));
		Compact.ReplaceInline(TEXT("}"), TEXT(""));
		if (!Compact.IsEmpty())
		{
			OutGuids.Add(Compact);
		}
	}

	static bool SnapshotContainsNodeGuid(
		const FBlueprintHelperGraphSnapshot& Snapshot,
		const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}

		TSet<FString> SnapshotGuids;
		for (const FString& NodeGuid : Snapshot.NodeGuids)
		{
			AddGuidVariants(SnapshotGuids, NodeGuid);
		}

		TSet<FString> CurrentGuids;
		AddGuidVariants(CurrentGuids, Node->NodeGuid.ToString());
		AddGuidVariants(CurrentGuids, Node->NodeGuid.ToString(EGuidFormats::Digits));
		for (const FString& CurrentGuid : CurrentGuids)
		{
			if (SnapshotGuids.Contains(CurrentGuid))
			{
				return true;
			}
		}
		return false;
	}

	static const FBlueprintHelperGraphSnapshotOwnershipEntry* FindSnapshotOwnershipEntry(
		const FBlueprintHelperGraphSnapshot& Snapshot,
		const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return nullptr;
		}

		TSet<FString> CurrentGuids;
		AddGuidVariants(CurrentGuids, Node->NodeGuid.ToString());
		AddGuidVariants(CurrentGuids, Node->NodeGuid.ToString(EGuidFormats::Digits));

		for (const FBlueprintHelperGraphSnapshotOwnershipEntry& Entry : Snapshot.OwnershipEntries)
		{
			TSet<FString> EntryGuids;
			AddGuidVariants(EntryGuids, Entry.NodeGuid);
			for (const FString& CurrentGuid : CurrentGuids)
			{
				if (EntryGuids.Contains(CurrentGuid))
				{
					return &Entry;
				}
			}
		}
		return nullptr;
	}

	static void RestoreSnapshotOwnershipMetadata(
		const FBlueprintHelperGraphSnapshot& Snapshot,
		const TSet<UEdGraphNode*>& ImportedNodes)
	{
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (!Node)
			{
				continue;
			}

			const FBlueprintHelperGraphSnapshotOwnershipEntry* Entry = FindSnapshotOwnershipEntry(Snapshot, Node);
			if (!Entry)
			{
				continue;
			}

			UPackage* Package = Node->GetOutermost();
			if (!Package)
			{
				continue;
			}

			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			if (!Entry->Owned.IsEmpty())
			{
				MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), *Entry->Owned);
			}
			else
			{
				MetaData.RemoveValue(Node, TEXT("BlueprintHelperOwned"));
			}

			if (!Entry->BlockId.IsEmpty())
			{
				MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *Entry->BlockId);
			}
			else
			{
				MetaData.RemoveValue(Node, TEXT("BlueprintHelperBlockId"));
			}

			if (!Entry->FeatureName.IsEmpty())
			{
				MetaData.SetValue(Node, TEXT("BlueprintHelperFeatureName"), *Entry->FeatureName);
			}
			else
			{
				MetaData.RemoveValue(Node, TEXT("BlueprintHelperFeatureName"));
			}

			if (!Entry->Tool.IsEmpty())
			{
				MetaData.SetValue(Node, TEXT("BlueprintHelperTool"), *Entry->Tool);
			}
			else
			{
				MetaData.RemoveValue(Node, TEXT("BlueprintHelperTool"));
			}
		}
	}

	static void RemoveNodesNotInSnapshot(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesToKeep)
	{
		if (!Blueprint || !Graph)
		{
			return;
		}

		TArray<UEdGraphNode*> NodesToRemove;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && !NodesToKeep.Contains(Node))
			{
				NodesToRemove.Add(Node);
			}
		}
		for (UEdGraphNode* Node : NodesToRemove)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}

	static bool RestoreSnapshotNodes(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperGraphSnapshot& Snapshot,
		FString& OutError)
	{
		if (!Blueprint || !Graph)
		{
			OutError = TEXT("graph_snapshot_restore_target_missing");
			return false;
		}
		if (Snapshot.ExportedText.IsEmpty())
		{
			return true;
		}

		TArray<UEdGraphNode*> ExistingSnapshotNodes;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (SnapshotContainsNodeGuid(Snapshot, Node))
			{
				ExistingSnapshotNodes.Add(Node);
			}
		}
		for (UEdGraphNode* Node : ExistingSnapshotNodes)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}

		if (!FEdGraphUtilities::CanImportNodesFromText(Graph, Snapshot.ExportedText))
		{
			OutError = TEXT("graph_snapshot_restore_text_not_importable");
			return false;
		}

		TSet<UEdGraphNode*> ImportedNodes;
		FEdGraphUtilities::ImportNodesFromText(Graph, Snapshot.ExportedText, ImportedNodes);
		if (ImportedNodes.Num() == 0)
		{
			OutError = TEXT("graph_snapshot_restore_imported_no_nodes");
			return false;
		}

		RestoreSnapshotOwnershipMetadata(Snapshot, ImportedNodes);
		Graph->NotifyGraphChanged();
		return true;
	}

	static FReplaceRollbackExecBoundary CaptureRollbackExecBoundary(
		const TArray<UEdGraphNode*>& EntryCandidates,
		const TArray<UEdGraphNode*>& BodyNodes)
	{
		FReplaceRollbackExecBoundary Boundary;
		TSet<UEdGraphNode*> BodyNodeSet;
		for (UEdGraphNode* BodyNode : BodyNodes)
		{
			if (BodyNode)
			{
				BodyNodeSet.Add(BodyNode);
			}
		}

		for (UEdGraphNode* EntryCandidate : EntryCandidates)
		{
			if (!EntryCandidate)
			{
				continue;
			}
			for (UEdGraphPin* Pin : EntryCandidate->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					continue;
				}
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					if (LinkedNode && BodyNodeSet.Contains(LinkedNode))
					{
						Boundary.EntryNodeGuid = EntryCandidate->NodeGuid;
						Boundary.BodyNodeGuid = LinkedNode->NodeGuid;
						Boundary.bValid = true;
						return Boundary;
					}
				}
			}
		}
		return Boundary;
	}

	static UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FGuid& NodeGuid)
	{
		if (!Graph || !NodeGuid.IsValid())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == NodeGuid)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static bool RestoreRollbackExecBoundary(
		UEdGraph* Graph,
		const FReplaceRollbackExecBoundary& Boundary,
		FString& OutError)
	{
		if (!Boundary.bValid)
		{
			return true;
		}

		UEdGraphNode* EntryNode = FindNodeByGuid(Graph, Boundary.EntryNodeGuid);
		UEdGraphNode* BodyNode = FindNodeByGuid(Graph, Boundary.BodyNodeGuid);
		UEdGraphPin* EntryExecOut = FindFirstExecPin(EntryNode, EGPD_Output);
		UEdGraphPin* BodyExecIn = FindFirstExecPin(BodyNode, EGPD_Input);
		if (!EntryExecOut || !BodyExecIn)
		{
			OutError = TEXT("graph_snapshot_restore_exec_boundary_missing");
			return false;
		}
		if (EntryExecOut->LinkedTo.Contains(BodyExecIn))
		{
			return true;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (!Schema || !Schema->TryCreateConnection(EntryExecOut, BodyExecIn))
		{
			OutError = TEXT("graph_snapshot_restore_exec_boundary_connect_failed");
			return false;
		}
		if (Graph)
		{
			Graph->NotifyGraphChanged();
		}
		return true;
	}

	static bool RestoreOwnedEntryBodyBoundary(
		UEdGraph* Graph,
		const EBlueprintHelperReplaceScope Scope,
		const FString& EntryName,
		const FString& EventTaxonomy,
		const FString& SignatureEvidenceId,
		const FString& BlockId,
		FString& OutError)
	{
		if (!Graph || BlockId.IsEmpty())
		{
			return true;
		}

		FBlueprintHelperReplaceEntryResolveRequest EntryResolveRequest;
		EntryResolveRequest.Scope = Scope;
		EntryResolveRequest.EntryName = EntryName;
		EntryResolveRequest.EventTaxonomy = EventTaxonomy;
		EntryResolveRequest.SignatureEvidenceId = SignatureEvidenceId;

		UEdGraphNode* EntryNode = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && FBlueprintHelperReplaceEntryResolver::NodeMatchesEntry(EntryResolveRequest, Node))
			{
				EntryNode = Node;
				break;
			}
		}
		if (!EntryNode)
		{
			OutError = TEXT("graph_snapshot_restore_owned_entry_missing");
			return false;
		}

		UEdGraphNode* BodyNode = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || Node == EntryNode || !FindFirstExecPin(Node, EGPD_Input))
			{
				continue;
			}

			FString NodeBlockId;
			if (TryReadBlueprintHelperBlockId(Node, NodeBlockId) &&
				NodeBlockId.Equals(BlockId, ESearchCase::IgnoreCase))
			{
				BodyNode = Node;
				break;
			}
		}
		if (!BodyNode)
		{
			OutError = TEXT("graph_snapshot_restore_owned_body_missing");
			return false;
		}

		UEdGraphPin* EntryExecOut = FindFirstExecPin(EntryNode, EGPD_Output);
		UEdGraphPin* BodyExecIn = FindFirstExecPin(BodyNode, EGPD_Input);
		if (!EntryExecOut || !BodyExecIn)
		{
			OutError = TEXT("graph_snapshot_restore_owned_boundary_missing");
			return false;
		}
		if (EntryExecOut->LinkedTo.Contains(BodyExecIn))
		{
			return true;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (!Schema || !Schema->TryCreateConnection(EntryExecOut, BodyExecIn))
		{
			OutError = TEXT("graph_snapshot_restore_owned_boundary_connect_failed");
			return false;
		}
		Graph->NotifyGraphChanged();
		return true;
	}

	static bool RestoreReplacementFailureSnapshot(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TSet<UEdGraphNode*>& NodesBeforeImport,
		FBlueprintHelperScopedAssetMutation& Mutation,
		const FBlueprintHelperGraphSnapshot& RollbackSnapshot,
		const FReplaceRollbackExecBoundary& RollbackExecBoundary,
		const EBlueprintHelperReplaceScope Scope,
		const FString& EntryName,
		const FString& EventTaxonomy,
		const FString& SignatureEvidenceId,
		const FString& OriginalBlockId,
		const bool bRequireOwnedEntryBodyBoundaryFallback,
		FString& OutError)
	{
		RemoveNodesNotInSnapshot(Blueprint, Graph, NodesBeforeImport);
		Mutation.Rollback();
		bool bRestored = RestoreSnapshotNodes(Blueprint, Graph, RollbackSnapshot, OutError);
		if (!bRestored)
		{
			return false;
		}

		bool bBoundaryRestored = RestoreRollbackExecBoundary(Graph, RollbackExecBoundary, OutError);
		if (!bBoundaryRestored || (!RollbackExecBoundary.bValid && bRequireOwnedEntryBodyBoundaryFallback))
		{
			bBoundaryRestored = RestoreOwnedEntryBodyBoundary(
				Graph,
				Scope,
				EntryName,
				EventTaxonomy,
				SignatureEvidenceId,
				OriginalBlockId,
				OutError);
		}
		return bBoundaryRestored;
	}

	static TSharedPtr<FJsonObject> BuildLogicSpecWithCustomEventEntryReference(
		const TSharedPtr<FJsonObject>& LogicSpec,
		const EBlueprintHelperReplaceScope Scope,
		const FString& GraphName,
		const FString& EntryName,
		const FString& EventTaxonomy,
		const FString& SignatureEvidenceId)
	{
		if (!LogicSpec.IsValid() ||
			Scope != EBlueprintHelperReplaceScope::CustomEventBody ||
			EntryName.TrimStartAndEnd().IsEmpty())
		{
			return LogicSpec;
		}

		const TSharedPtr<FJsonObject>* ExistingEntryObject = nullptr;
		if (LogicSpec->TryGetObjectField(TEXT("entry"), ExistingEntryObject) &&
			ExistingEntryObject &&
			ExistingEntryObject->IsValid())
		{
			return LogicSpec;
		}

		TSharedRef<FJsonObject> AugmentedLogicSpec = MakeShared<FJsonObject>();
		AugmentedLogicSpec->Values = LogicSpec->Values;

		TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("id"), EntryName + TEXT("_entry"));
		EntryObject->SetStringField(TEXT("kind"), TEXT("custom_event"));
		EntryObject->SetStringField(TEXT("name"), EntryName);
		EntryObject->SetStringField(TEXT("graph"), GraphName);
		EntryObject->SetStringField(
			TEXT("event_taxonomy"),
			EventTaxonomy.IsEmpty() ? TEXT("custom_event") : EventTaxonomy);
		EntryObject->SetStringField(TEXT("source_cluster"), TEXT("blueprint_signature"));
		EntryObject->SetStringField(
			TEXT("signature_evidence_id"),
			SignatureEvidenceId.IsEmpty() ? EntryName + TEXT("_signature_evidence") : SignatureEvidenceId);
		AugmentedLogicSpec->SetObjectField(TEXT("entry"), EntryObject);
		return AugmentedLogicSpec;
	}

};

// 鈹€鈹€鈹€ 鏋勯€?鈹€鈹€鈹€

FBlueprintHelperReplaceBlueprintGraphService::FBlueprintHelperReplaceBlueprintGraphService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperBlockIdService& InBlockIdService,
	const FBlueprintHelperOwnershipService& InOwnershipService,
	const FBlueprintHelperGraphSnapshotService& InSnapshotService)
	: Resolver(InResolver)
	, BlockIdService(InBlockIdService)
	, OwnershipService(InOwnershipService)
	, SnapshotService(InSnapshotService)
{
}

// 鈹€鈹€鈹€ 鍏叡鍏ュ彛 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FReplaceRequest Request = ParseRequest(Payload);

	return FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
		Request.bDryRun
			? EBlueprintHelperGraphWriteUnitOfWorkMode::Preview
			: EBlueprintHelperGraphWriteUnitOfWorkMode::Execute,
		TEXT("replace_blueprint_graph"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2FunctionBody,
		[this, &Request]()
		{
			return Request.bDryRun ? ExecuteDryRun(Request) : ExecuteWrite(Request);
		});
}

// 鈹€鈹€鈹€ 瑙ｆ瀽 鈹€鈹€鈹€

FBlueprintHelperReplaceBlueprintGraphService::FReplaceRequest
FBlueprintHelperReplaceBlueprintGraphService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FReplaceRequest Request;
	const FBlueprintHelperGraphWriteToolClusterPolicy Policy =
		FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy();
	Request.bDryRun = Policy.bDryRun;
	Request.bStrict = Policy.bStrict;

	if (!Payload.IsValid())
	{
		return Request;
	}

	// target
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName);

		FString ScopeStr;
		if ((*TargetObject)->TryGetStringField(TEXT("replace_scope"), ScopeStr))
		{
			ParseReplaceScope(ScopeStr, Request.Scope);
		}
	}

	// selector
	const TSharedPtr<FJsonObject>* SelectorObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("selector"), SelectorObject) && SelectorObject->IsValid())
	{
		(*SelectorObject)->TryGetStringField(TEXT("block_id"), Request.BlockId);
		(*SelectorObject)->TryGetStringField(TEXT("target_ref"), Request.TargetRef);
		(*SelectorObject)->TryGetStringField(TEXT("entry_name"), Request.EntryName);
		if (Request.EntryName.IsEmpty())
		{
			(*SelectorObject)->TryGetStringField(TEXT("function_name"), Request.EntryName);
		}
		(*SelectorObject)->TryGetStringField(TEXT("event_taxonomy"), Request.EventTaxonomy);
		(*SelectorObject)->TryGetStringField(TEXT("signature_evidence_id"), Request.SignatureEvidenceId);
		(*SelectorObject)->TryGetStringField(TEXT("node_path"), Request.NodePath);
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject && LogicSpecObject->IsValid())
	{
		Request.LogicSpec = *LogicSpecObject;
	}

	// options
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("options"), OptionsObject) && OptionsObject->IsValid())
	{
		(*OptionsObject)->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
		(*OptionsObject)->TryGetBoolField(TEXT("strict"), Request.bStrict);
		(*OptionsObject)->TryGetBoolField(TEXT("preserve_layout"), Request.bPreserveLayout);
		(*OptionsObject)->TryGetBoolField(TEXT("include_timing"), Request.bIncludeTiming);
	}
	Payload->TryGetBoolField(TEXT("include_timing"), Request.bIncludeTiming);

	return Request;
}

// 鈹€鈹€鈹€ Preflight 鈹€鈹€鈹€

FBlueprintHelperReplaceBlueprintGraphService::FReplacePreflightResult
FBlueprintHelperReplaceBlueprintGraphService::Preflight(const FReplaceRequest& Request) const
{
	FReplacePreflightResult Result;

	// 1. asset_path
	if (Request.AssetPath.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		Result.Conflicts.Add({TEXT("target_blueprint_not_found"),
			TEXT("Missing target.asset_path."), TEXT("target.asset_path"), TEXT("payload")});
		return Result;
	}

	// 2. graph
	if (Request.GraphName.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_graph_not_found"));
		Result.Conflicts.Add({TEXT("target_graph_not_found"),
			TEXT("Missing target.graph."), TEXT("target.graph"), TEXT("payload")});
		return Result;
	}

	// 3. replace_scope
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionDefinition ||
		Request.Scope == EBlueprintHelperReplaceScope::EventDefinition)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("replace_scope_unsupported"));
		Result.Conflicts.Add({TEXT("replace_scope_unsupported"),
			TEXT("function_definition / event_definition write is not supported; use dry_run."),
			TEXT("target.replace_scope"), TEXT("payload")});
		return Result;
	}


	// 5. 钃濆浘鏍￠獙
	if (Request.Scope == EBlueprintHelperReplaceScope::Graph && !Request.EntryName.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("graph_scope_entry_selector_unsupported"));
		Result.Conflicts.Add({TEXT("graph_scope_entry_selector_unsupported"),
			TEXT("replace_scope=graph does not accept selector.entry_name; use custom_event_body or event_body for entry-body replacement."),
			Request.EntryName, TEXT("selector.entry_name")});
		return Result;
	}

	if ((Request.Scope == EBlueprintHelperReplaceScope::CustomEventBody ||
		Request.Scope == EBlueprintHelperReplaceScope::EventBody) &&
		Request.EntryName.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_entry_not_found"));
		Result.Conflicts.Add({TEXT("target_entry_not_found"),
			TEXT("event/custom_event body replacement requires selector.entry_name."),
			TEXT("selector.entry_name"), TEXT("payload")});
		return Result;
	}

	UBlueprint* Blueprint = nullptr;
	if (!PreflightBlueprint(Request.AssetPath, Blueprint, Result))
	{
		return Result;
	}

	if (!PreflightLogicSpec(Request, Blueprint, Result))
	{
		return Result;
	}

	// 6. 鍥捐〃鏍￠獙
	UEdGraph* Graph = nullptr;
	if (!PreflightGraphTarget(Blueprint, Request.GraphName, Request.Scope, Graph, Result))
	{
		return Result;
	}

	// 7. scope 鏍￠獙
	if (!PreflightReplaceScope(Request.Scope, Result))
	{
		return Result;
	}

	// 8. selector 瀛樺湪鎬ф鏌?
	if (Request.Scope == EBlueprintHelperReplaceScope::BlockImplementation)
	{
		if (Request.BlockId.IsEmpty() && Request.TargetRef.IsEmpty() && Request.NodePath.IsEmpty())
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("target_block_not_found"));
			Result.Conflicts.Add({TEXT("target_block_not_found"),
				TEXT("block_implementation requires selector.block_id, selector.target_ref, or selector.node_path."),
				TEXT("selector"), TEXT("payload")});
		}
	}

	return Result;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightBlueprint(
	const FString& AssetPath, UBlueprint*& OutBlueprint, FReplacePreflightResult& OutResult) const
{
	FBlueprintHelperGraphTarget Target;
	Target.BlueprintPath = AssetPath;

	FBlueprintHelperDiagnosticSet Diag;
	OutBlueprint = Resolver.ResolveBlueprint(Target, Diag, FBlueprintHelperResolvePolicy::Mutation());

	if (!OutBlueprint || Diag.HasErrors())
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		OutResult.Conflicts.Add({TEXT("target_blueprint_not_found"),
			FString::Printf(TEXT("钃濆浘璧勪骇鏈壘鍒帮細%s"), *AssetPath),
			AssetPath, TEXT("target.asset_path")});
		return false;
	}

	return true;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightLogicSpec(
	const FReplaceRequest& Request,
	UBlueprint* Blueprint,
	FReplacePreflightResult& OutResult) const
{
	if (!Request.LogicSpec.IsValid())
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("logic_spec_required"));
		OutResult.Conflicts.Add({TEXT("logic_spec_required"),
			TEXT("replace_blueprint_graph requires logic_spec/SemanticIR input."), TEXT("logic_spec"), TEXT("payload")});
		return false;
	}

	OutResult.FragmentDebugData = FBlueprintHelperGraphFragmentDebugData::BuildFromLogicSpec(Request.LogicSpec, Blueprint);

	UEdGraph* SemanticContextGraph = nullptr;
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
		{
			if (FunctionGraph && FunctionGraph->GetName() == Request.GraphName)
			{
				SemanticContextGraph = FunctionGraph;
				break;
			}
		}
	}
	else
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph && Graph->GetName() == Request.GraphName)
			{
				SemanticContextGraph = Graph;
				break;
			}
		}
	}

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		Request.LogicSpec,
		FBlueprintHelperGraphSemanticContext::FromBlueprintAndGraph(Blueprint, SemanticContextGraph),
		SemanticIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : SemanticIR.Diagnostics)
	{
		if (Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(Diagnostic.Code);
			OutResult.Errors.Add({Diagnostic.Code, Diagnostic.Message, Diagnostic.Path, TEXT("logic_spec")});
		}
	}
	return OutResult.bPassed;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightGraphTarget(
	UBlueprint* Blueprint, const FString& GraphName, EBlueprintHelperReplaceScope Scope,
	UEdGraph*& OutGraph, FReplacePreflightResult& OutResult) const
{
	// 鍑芥暟鍥?(function_body)
	if (Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		for (UEdGraph* FnGraph : Blueprint->FunctionGraphs)
		{
			if (FnGraph && FnGraph->GetName() == GraphName)
			{
				OutGraph = FnGraph;
				return true;
			}
		}
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("target_function_not_found"));
		OutResult.Conflicts.Add({TEXT("target_function_not_found"),
			FString::Printf(TEXT("Function graph %s was not found."), *GraphName), GraphName, TEXT("target.graph")});
		return false;
	}

	// 浜嬩欢鍥?(block_implementation / event_body / custom_event_body / graph)
	for (UEdGraph* UbergraphPage : Blueprint->UbergraphPages)
	{
		if (UbergraphPage && UbergraphPage->GetName() == GraphName)
		{
			OutGraph = UbergraphPage;
			return true;
		}
	}

	OutResult.bPassed = false;
	OutResult.BlockedBy.Add(TEXT("target_graph_not_found"));
	OutResult.Conflicts.Add({TEXT("target_graph_not_found"),
		FString::Printf(TEXT("Graph %s was not found."), *GraphName), GraphName, TEXT("target.graph")});
	return false;
}

bool FBlueprintHelperReplaceBlueprintGraphService::PreflightReplaceScope(
	EBlueprintHelperReplaceScope Scope, FReplacePreflightResult& OutResult) const
{
	if (Scope == EBlueprintHelperReplaceScope::FunctionDefinition ||
		Scope == EBlueprintHelperReplaceScope::EventDefinition)
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("replace_scope_unsupported"));
		OutResult.Conflicts.Add({TEXT("replace_scope_unsupported"),
			TEXT("This replace_scope write path is not implemented."), TEXT("target.replace_scope"), TEXT("payload")});
		return false;
	}
	return true;
}

// 鈹€鈹€鈹€ DryRun 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::ExecuteDryRun(
	const FReplaceRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FReplacePreflightResult PreflightResult = Preflight(Request);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("replace_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef TargetRef;
	TargetRef.AssetPath = Request.AssetPath;
	TargetRef.TargetType = EBlueprintHelperTargetType::Graph;
	TargetRef.Graph = Request.GraphName;
	Result.Target = TargetRef;

	// 鐗规畩锛歊eplace dry_run target 涓嶈緭鍑?target_type锛屼絾 include replace_scope
	// 閫氳繃鐩存帴璁剧疆 JSON 瀛楁瀹炵幇
	TargetRef.TargetType = EBlueprintHelperTargetType::None;

	if (PreflightResult.bPassed)
	{
		FBlueprintHelperReplaceDryRunData DryRunData;
		DryRunData.DryRun.Result = TEXT("passed");
		DryRunData.DryRun.bCanExecute = true;
		Result.Data = DryRunData.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, PreflightResult.FragmentDebugData);
	}
	else
	{
		FBlueprintHelperReplaceDryRunData DryRunData;
		DryRunData.DryRun.Result = TEXT("blocked");
		DryRunData.DryRun.bCanExecute = false;
		DryRunData.DryRun.BlockedBy = PreflightResult.BlockedBy;
		DryRunData.DryRun.Conflicts = PreflightResult.Conflicts;
		DryRunData.DryRun.Errors = PreflightResult.Errors;

		const FBlueprintHelperGraphWriteIssue* FirstIssue = PreflightResult.Conflicts.Num() > 0
			? &PreflightResult.Conflicts[0]
			: (PreflightResult.Errors.Num() > 0 ? &PreflightResult.Errors[0] : nullptr);

		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = FirstIssue && !FirstIssue->Message.IsEmpty()
			? FirstIssue->Message
			: TEXT("Replace dry-run preflight blocked execution.");
		Error.Field = FirstIssue && !FirstIssue->Source.IsEmpty()
			? FirstIssue->Source
			: TEXT("target.graph");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

		Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("replace_blueprint_graph"), TraceId, Error);
		Result.Target = TargetRef;
		Result.Data = DryRunData.ToJson();
		FBlueprintHelperGraphFragmentDebugData::AttachToData(Result.Data, PreflightResult.FragmentDebugData);
	}

	return Result;
}

// 鈹€鈹€鈹€ 姝ｅ紡鍐欏叆 鈹€鈹€鈹€

FBlueprintHelperToolResultBase FBlueprintHelperReplaceBlueprintGraphService::ExecuteWrite(
	const FReplaceRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	// 1. Preflight
	FReplacePreflightResult PreflightResult = Preflight(Request);
	if (!PreflightResult.bPassed)
	{
		FBlueprintHelperToolError Error;
		Error.Code = PreflightResult.BlockedBy.Num() > 0 ? PreflightResult.BlockedBy[0] : TEXT("preflight_failed");
		Error.Stage = EBlueprintHelperToolStage::Preflight;
		Error.Message = PreflightResult.Conflicts.Num() > 0
			? PreflightResult.Conflicts[0].Message : TEXT("Preflight failed.");
		Error.Field = PreflightResult.Conflicts.Num() > 0
			? PreflightResult.Conflicts[0].Source : FString();
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
		FBlueprintHelperGraphFragmentDebugData::AttachToData(FailResult.Data, PreflightResult.FragmentDebugData);
		return FailResult;
	}

	// 2. 瑙ｆ瀽钃濆浘
	FBlueprintHelperGraphTarget BPTarget;
	BPTarget.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* Blueprint = Resolver.ResolveBlueprint(BPTarget, Diag, FBlueprintHelperResolvePolicy::Mutation());
	if (!Blueprint)
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("target_blueprint_not_found");
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = FString::Printf(TEXT("Blueprint %s was not found."), *Request.AssetPath);
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 3. 瑙ｆ瀽鏇挎崲鐩爣
	FResolvedReplaceTarget Resolved;
	FString ResolveError;
	if (!ResolveReplaceTarget(Request, Blueprint, Resolved, ResolveError))
	{
		FBlueprintHelperToolError Error;
		if (ResolveError.Contains(TEXT("owned_replace_target_not_blueprinthelper_owned")))
		{
			Error.Code = TEXT("owned_replace_target_not_blueprinthelper_owned");
		}
		else
		{
			Error.Code = ResolveError.Contains(TEXT("block")) ? TEXT("target_block_not_found")
				: (ResolveError.Contains(TEXT("function")) ? TEXT("target_function_not_found") : TEXT("target_not_found"));
		}
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = ResolveError;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 4. 鎹曡幏 before snapshot
	FBlueprintHelperGraphSnapshot BeforeSnapshot = SnapshotService.CaptureNodeSnapshot(
		Resolved.Graph, Resolved.NodesToDelete);
	const FBlueprintHelperGraphSnapshot RollbackSnapshot = SnapshotService.CaptureNodeSnapshot(
		Resolved.Graph, Resolved.NodesToDelete);
	BeforeSnapshot.OwnerBlockId = Resolved.OriginalBlockId;
	BeforeSnapshot.EntryIdentity = Request.EntryName.IsEmpty() ? Resolved.TargetRef : Request.EntryName;
	BeforeSnapshot.ReplaceScope = ReplaceScopeToString(Request.Scope);
	const FString ReviewBlockTargetKey = !Resolved.OriginalBlockId.IsEmpty()
		? FString::Printf(TEXT("graph:%s:block:%s"), *Request.GraphName, *Resolved.OriginalBlockId)
		: FString();
	FString BeforeBlockSnapshotJson;
	if (!ReviewBlockTargetKey.IsEmpty())
	{
		FBlueprintHelperReviewAtomicTarget BeforeBlockTarget;
		BeforeBlockTarget.AssetPath = Request.AssetPath;
		BeforeBlockTarget.GraphName = Request.GraphName;
		BeforeBlockTarget.TargetKind = TEXT("graph_block");
		BeforeBlockTarget.TargetKey = ReviewBlockTargetKey;
		FString BeforeBlockHash;
		FString SnapshotError;
		FBlueprintHelperReviewBaselineSnapshotService SemanticSnapshotService;
		SemanticSnapshotService.CaptureTargetSnapshot(
			BeforeBlockTarget,
			BeforeBlockSnapshotJson,
			BeforeBlockHash,
			SnapshotError);
	}

	// 5. 寮€濮嬩慨鏀?
	const bool bPackageWasDirtyBeforeWrite = Blueprint->GetOutermost()
		? Blueprint->GetOutermost()->IsDirty()
		: false;
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Replace Blueprint Graph")), Blueprint);
	Mutation.Modify(Resolved.Graph);
	const FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FReplaceRollbackExecBoundary RollbackExecBoundary =
		FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::CaptureRollbackExecBoundary(
			Resolved.NodesToPreserve,
			Resolved.NodesToDelete);

	// 6. 鍒犻櫎鏃у疄鐜?
	if (!DeleteOldImplementation(Blueprint, Resolved.Graph, Resolved.NodesToDelete))
	{
		Mutation.Rollback();

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("node_create_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = TEXT("Failed to delete old implementation.");
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	TSet<UEdGraphNode*> NodesBeforeImport;
	for (UEdGraphNode* Node : Resolved.Graph->Nodes)
	{
		if (Node)
		{
			NodesBeforeImport.Add(Node);
		}
	}

	// 7. 閫氳繃 AgentImportService 鍒涘缓鏂拌妭鐐?杩炵嚎
	const FString GraphWritePayload = BuildSemanticGraphWritePayload(Request);
	TArray<TSharedPtr<FUnresolvedNodeItem>> UnresolvedNodes;
	const FBlueprintGenerateResult GenerateResult =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(Resolved.Graph, GraphWritePayload, UnresolvedNodes);
	FBlueprintGraphWriteExecutionStats ExecutionStats = GenerateResult.ExecutionStats;
	const bool bDeferredEntryResolvedConnectivityFailure =
		FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::CanDeferEntryResolvedConnectivityFailure(
			Request.Scope,
			GenerateResult,
			Resolved.Graph,
			NodesBeforeImport);

	if (!GenerateResult.bSucceed && !bDeferredEntryResolvedConnectivityFailure)
	{
		FString RestoreError;
		const bool bRestored = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::RestoreReplacementFailureSnapshot(
			Blueprint,
			Resolved.Graph,
			NodesBeforeImport,
			Mutation,
			RollbackSnapshot,
			RollbackExecBoundary,
			Request.Scope,
			Request.EntryName,
			Request.EventTaxonomy,
			Request.SignatureEvidenceId,
			Resolved.OriginalBlockId,
			Resolved.NodesToDelete.Num() > 0,
			RestoreError);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->SetDirtyFlag(bPackageWasDirtyBeforeWrite);
		}

		FString ErrorMessage = GenerateResult.Message.IsEmpty()
			? TEXT("Failed to create replacement implementation through SemanticIR.")
			: GenerateResult.Message;
		if (UnresolvedNodes.Num() > 0 && UnresolvedNodes[0].IsValid())
		{
			ErrorMessage += FString::Printf(
				TEXT(" First unresolved: %s - %s"),
				*UnresolvedNodes[0]->DisplayText,
				*UnresolvedNodes[0]->Reason);
		}
		if (!bRestored && !RestoreError.IsEmpty())
		{
			ErrorMessage += FString::Printf(TEXT(" Rollback restore failed: %s"), *RestoreError);
		}

		FBlueprintHelperToolError Error;
		Error.Code = GenerateResult.ConnectivityViolationCount > 0
			? TEXT("graphwrite_connectivity_failed")
			: TEXT("semantic_graph_write_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ErrorMessage;
		Error.bRetryable = false;
		Error.RollbackResult = bRestored
			? EBlueprintHelperRollbackResult::RolledBack
			: EBlueprintHelperRollbackResult::Failed;

		FBlueprintHelperToolResultBase FailResult = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("replace_blueprint_graph"), TraceId, Error);
		FailResult.Data = MakeShared<FJsonObject>();
		FBlueprintHelperGraphWriteConnectivityDiagnosticsJson::Attach(
			FailResult.Data,
			GenerateResult.ConnectivityDiagnostics);
		if (Request.bIncludeTiming)
		{
			FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::AttachGraphWriteExecutionStats(
				FailResult.Data,
				ExecutionStats);
		}
		return FailResult;
	}
	if (bDeferredEntryResolvedConnectivityFailure)
	{
		ExecutionStats.ConnectivityViolationCount = 0;
	}
	FString ReconnectError;
	if (!ReconnectPreservedEntryToNewBody(Request, Resolved, NodesBeforeImport, ReconnectError))
	{
		FString RestoreError;
		const bool bRestored = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::RestoreReplacementFailureSnapshot(
			Blueprint,
			Resolved.Graph,
			NodesBeforeImport,
			Mutation,
			RollbackSnapshot,
			RollbackExecBoundary,
			Request.Scope,
			Request.EntryName,
			Request.EventTaxonomy,
			Request.SignatureEvidenceId,
			Resolved.OriginalBlockId,
			Resolved.NodesToDelete.Num() > 0,
			RestoreError);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->SetDirtyFlag(bPackageWasDirtyBeforeWrite);
		}

		FBlueprintHelperToolError Error;
		Error.Code = TEXT("entry_reconnect_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ReconnectError.IsEmpty()
			? TEXT("Failed to rebuild entry exec link after replacement.") : ReconnectError;
		if (!bRestored && !RestoreError.IsEmpty())
		{
			Error.Message += FString::Printf(TEXT(" Rollback restore failed: %s"), *RestoreError);
		}
		Error.bRetryable = false;
		Error.RollbackResult = bRestored
			? EBlueprintHelperRollbackResult::RolledBack
			: EBlueprintHelperRollbackResult::Failed;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
	}

	// 8. 鍐欏叆 ownership锛堝鏋滅洰鏍囦负 owned block锛?
	TArray<UEdGraphNode*> NewNodes;
	for (UEdGraphNode* Node : Resolved.Graph->Nodes)
	{
		if (Node && !NodesBeforeImport.Contains(Node))
		{
			NewNodes.Add(Node);
		}
	}

	if (Resolved.bIsBlueprintHelperOwned)
	{
		TArray<UEdGraphNode*> OwnershipNodes = NewNodes;
		for (UEdGraphNode* NodeToAdopt : Resolved.NodesToAdoptOwnership)
		{
			OwnershipNodes.AddUnique(NodeToAdopt);
		}

		if (OwnershipNodes.Num() > 0)
		{
			FString OwnershipError;
			if (!OwnershipService.WriteBlockOwnership(
				Blueprint, OwnershipNodes, Resolved.OriginalBlockId, TEXT("Replace"), OwnershipError))
			{
				Mutation.Rollback();

				FBlueprintHelperToolError Error;
				Error.Code = TEXT("ownership_write_failed");
				Error.Stage = EBlueprintHelperToolStage::Execute;
				Error.Message = OwnershipError;
				Error.bRetryable = false;
				Error.RollbackResult = EBlueprintHelperRollbackResult::RolledBack;
				return FBlueprintHelperToolResultBuilder::Failure(TEXT("replace_blueprint_graph"), TraceId, Error);
			}
		}
	}

	// 10. 鏍囪淇敼
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (Blueprint->GetOutermost())
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}
	Mutation.Commit();

	// 11. 鎴愬姛缁撴灉
	FBlueprintHelperToolResultBase SuccessResult = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("replace_blueprint_graph"), TraceId);

	FBlueprintHelperTargetRef SuccessTarget;
	SuccessTarget.AssetPath = Request.AssetPath;
	SuccessTarget.TargetType = EBlueprintHelperTargetType::None; // 涓嶈緭鍑?target_type
	SuccessTarget.Graph = Request.GraphName;
	SuccessResult.Target = SuccessTarget;

	FBlueprintHelperReplaceGraphResultData Data;
	Data.ReplaceResult.ReplacedRef.GraphId = Resolved.GraphId.IsEmpty() ? Request.GraphName : Resolved.GraphId;
	Data.ReplaceResult.ReplacedRef.TargetRef = Resolved.TargetRef;
	if (!Resolved.OriginalBlockId.IsEmpty())
	{
		Data.BlockRefs.Add(Resolved.OriginalBlockId);
	}
	SuccessResult.Data = Data.ToJson();
	FBlueprintHelperGraphFragmentDebugData::AttachToData(SuccessResult.Data, PreflightResult.FragmentDebugData);

	FBlueprintHelperValidationSummary Validation;
	const FBlueprintHelperGraphWriteToolClusterPolicy Policy =
		FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy();
	Validation.bShouldCompile = Policy.bCompile;
	Validation.bShouldSave = Policy.bSave;
	SuccessResult.Validation = Validation;

	const double LayoutStart = FPlatformTime::Seconds();
	FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Resolved.Graph, NewNodes);
	ExecutionStats.RecordLayoutMs = (FPlatformTime::Seconds() - LayoutStart) * 1000.0;
	ExecutionStats.LayoutRecordNodeCount = NewNodes.Num();
	if (Request.bIncludeTiming)
	{
		FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::AttachGraphWriteExecutionStats(
			SuccessResult.Data,
			ExecutionStats);
	}

	return SuccessResult;
}

// 鈹€鈹€鈹€ 鐩爣瑙ｆ瀽 鈹€鈹€鈹€

bool FBlueprintHelperReplaceBlueprintGraphService::ResolveReplaceTarget(
	const FReplaceRequest& Request, UBlueprint* Blueprint, FResolvedReplaceTarget& OutTarget, FString& OutError) const
{
	// 鏌ユ壘鍥捐〃
	UEdGraph* Graph = nullptr;
	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		for (UEdGraph* FnGraph : Blueprint->FunctionGraphs)
		{
			if (FnGraph && FnGraph->GetName() == Request.GraphName)
			{
				Graph = FnGraph;
				break;
			}
		}
	}
	else
	{
		for (UEdGraph* Page : Blueprint->UbergraphPages)
		{
			if (Page && Page->GetName() == Request.GraphName)
			{
				Graph = Page;
				break;
			}
		}
	}

	if (!Graph)
	{
		OutError = FString::Printf(TEXT("Graph %s was not found."), *Request.GraphName);
		return false;
	}

	OutTarget.Blueprint = Blueprint;
	OutTarget.Graph = Graph;
	OutTarget.Scope = Request.Scope;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = Request.GraphName;
	OutTarget.GraphId = Request.GraphName;

	if (Request.Scope == EBlueprintHelperReplaceScope::BlockImplementation)
	{
		return ResolveBlockImplementation(Graph, Request, OutTarget, OutError);
	}

	if (Request.Scope == EBlueprintHelperReplaceScope::FunctionBody)
	{
		OutTarget.TargetRef = Request.GraphName;
		FBlueprintHelperReplaceEntryResolveRequest EntryResolveRequest;
		EntryResolveRequest.Scope = Request.Scope;
		EntryResolveRequest.EntryName = Request.EntryName;
		EntryResolveRequest.EventTaxonomy = Request.EventTaxonomy;
		EntryResolveRequest.SignatureEvidenceId = Request.SignatureEvidenceId;
		// 鏀堕泦 body 鑺傜偣锛堜繚鐣?FunctionEntry/Result锛?
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			if (FBlueprintHelperReplaceEntryResolver::ShouldPreserveEntryNode(EntryResolveRequest, Node->GetClass()))
			{
				OutTarget.NodesToPreserve.Add(Node);
			}
			else
			{
				OutTarget.NodesToDelete.Add(Node);
			}
		}
		OutTarget.bExternalDependentsMayBreak = false;
		return true;
	}

	// event_body / custom_event_body / graph: 鍥為€€鍒?block_implementation 璇箟
	if (Request.Scope == EBlueprintHelperReplaceScope::Graph)
	{
		// 绠€鍖栵細鍒犻櫎鎵€鏈夐潪 entry 鑺傜偣
		OutTarget.TargetRef = Request.GraphName;
		FBlueprintHelperReplaceEntryResolveRequest EntryResolveRequest;
		EntryResolveRequest.Scope = Request.Scope;
		EntryResolveRequest.EntryName = Request.EntryName;
		EntryResolveRequest.EventTaxonomy = Request.EventTaxonomy;
		EntryResolveRequest.SignatureEvidenceId = Request.SignatureEvidenceId;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				if (FBlueprintHelperReplaceEntryResolver::ShouldPreserveEntryNode(EntryResolveRequest, Node->GetClass()))
				{
					OutTarget.NodesToPreserve.Add(Node);
				}
				else
				{
					OutTarget.NodesToDelete.Add(Node);
				}
			}
		}
		return true;
	}

	if (Request.Scope == EBlueprintHelperReplaceScope::CustomEventBody ||
		Request.Scope == EBlueprintHelperReplaceScope::EventBody)
	{
		OutTarget.TargetRef = Request.EntryName;
		FBlueprintHelperReplaceEntryResolveRequest EntryResolveRequest;
		EntryResolveRequest.Scope = Request.Scope;
		EntryResolveRequest.EntryName = Request.EntryName;
		EntryResolveRequest.EventTaxonomy = Request.EventTaxonomy;
		EntryResolveRequest.SignatureEvidenceId = Request.SignatureEvidenceId;

		UEdGraphNode* EntryNode = nullptr;
		FString EntryBlockId;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || !FBlueprintHelperReplaceEntryResolver::NodeMatchesEntry(EntryResolveRequest, Node))
			{
				continue;
			}

			EntryNode = Node;
			OutTarget.NodesToPreserve.Add(Node);
			FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::TryReadBlueprintHelperBlockId(Node, EntryBlockId);
			break;
		}

		if (!EntryNode)
		{
			OutError = FString::Printf(TEXT("Entry %s was not found."), *Request.EntryName);
			return false;
		}

		if (EntryBlockId.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("owned_replace_target_not_blueprinthelper_owned: Entry %s does not have BlueprintHelper ownership metadata; owned replace cannot adopt user-authored nodes."),
				*Request.EntryName);
			return false;
		}

		OutTarget.OriginalBlockId = EntryBlockId;
		OutTarget.OriginalBlockRef = Request.EntryName;
		OutTarget.TargetRef = Request.EntryName;
		OutTarget.bIsBlueprintHelperOwned = true;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || Node == EntryNode)
			{
				continue;
			}

			FString NodeBlockId;
			if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::TryReadBlueprintHelperBlockId(Node, NodeBlockId) &&
				NodeBlockId.Equals(EntryBlockId, ESearchCase::IgnoreCase))
			{
				OutTarget.NodesToDelete.Add(Node);
				OutTarget.ExistingOwnedNodes.Add(Node);
			}
		}
		return true;
	}

	OutError = TEXT("Unsupported replace_scope.");
	return false;
}

bool FBlueprintHelperReplaceBlueprintGraphService::ResolveBlockImplementation(
	UEdGraph* Graph, const FReplaceRequest& Request, FResolvedReplaceTarget& OutTarget, FString& OutError) const
{
	// 鎵弿鍥捐〃涓殑 BlueprintHelper-owned 鑺傜偣
	FString SearchBlockId = Request.BlockId;
	if (SearchBlockId.IsEmpty() && !Request.TargetRef.IsEmpty())
	{
		SearchBlockId = FString::Printf(TEXT("%s_%s"), *Request.GraphName, *Request.TargetRef);
	}

	TArray<UEdGraphNode*> OwnedNodes;
	FString FoundBlockRef;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		UPackage* Package = Node->GetOutermost();
		if (!Package) continue;

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		const FString OwnedStr = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned"));
		if (OwnedStr != TEXT("true")) continue;

		const FString NodeBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		if (!SearchBlockId.IsEmpty() && NodeBlockId != SearchBlockId) continue;

		OwnedNodes.Add(Node);
		if (FoundBlockRef.IsEmpty())
		{
			// 浠?block_id 鎻愬彇 block_ref
			const FString Prefix = Request.GraphName + TEXT("_");
			if (NodeBlockId.StartsWith(Prefix))
			{
				FoundBlockRef = NodeBlockId.Mid(Prefix.Len());
			}
		}
	}

	if (OwnedNodes.Num() == 0)
	{
		if (SearchBlockId.IsEmpty())
		{
			OutError = TEXT("No BlueprintHelper-owned node found. Provide selector.block_id or selector.target_ref.");
		}
		else
		{
			OutError = FString::Printf(TEXT("Target block %s was not found or is not owned by BlueprintHelper."), *SearchBlockId);
		}
		return false;
	}

	OutTarget.NodesToDelete = OwnedNodes;
	OutTarget.ExistingOwnedNodes = OwnedNodes;
	OutTarget.OriginalBlockId = SearchBlockId;
	OutTarget.OriginalBlockRef = FoundBlockRef.IsEmpty() ? Request.TargetRef : FoundBlockRef;
	OutTarget.TargetRef = FoundBlockRef.IsEmpty() ? Request.TargetRef : FoundBlockRef;
	OutTarget.bIsBlueprintHelperOwned = true;

	return true;
}

// 鈹€鈹€鈹€ 鍒犻櫎鏃у疄鐜?鈹€鈹€鈹€

bool FBlueprintHelperReplaceBlueprintGraphService::DeleteOldImplementation(
	UBlueprint* Blueprint, UEdGraph* Graph, const TArray<UEdGraphNode*>& NodesToDelete) const
{
	if (!Blueprint || !Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : NodesToDelete)
	{
		if (Node)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}

	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool FBlueprintHelperReplaceBlueprintGraphService::ReconnectPreservedEntryToNewBody(
	const FReplaceRequest& Request,
	const FResolvedReplaceTarget& Resolved,
	const TSet<UEdGraphNode*>& NodesBeforeImport,
	FString& OutError) const
{
	if (Request.Scope != EBlueprintHelperReplaceScope::FunctionBody &&
		Request.Scope != EBlueprintHelperReplaceScope::EventBody &&
		Request.Scope != EBlueprintHelperReplaceScope::CustomEventBody)
	{
		return true;
	}

	if (!Resolved.Graph)
	{
		OutError = TEXT("Replacement graph is null; cannot rebuild entry exec link.");
		return false;
	}

	UEdGraphNode* EntryNode = nullptr;
	FBlueprintHelperReplaceEntryResolveRequest EntryResolveRequest;
	EntryResolveRequest.Scope = Request.Scope;
	EntryResolveRequest.EntryName = Request.EntryName;
	EntryResolveRequest.EventTaxonomy = Request.EventTaxonomy;
	EntryResolveRequest.SignatureEvidenceId = Request.SignatureEvidenceId;
	for (UEdGraphNode* Node : Resolved.NodesToPreserve)
	{
		if (!Node)
		{
			continue;
		}

		if (FBlueprintHelperReplaceEntryResolver::NodeMatchesEntry(EntryResolveRequest, Node))
		{
			EntryNode = Node;
			break;
		}
	}

	if (!EntryNode)
	{
		OutError = Request.EntryName.IsEmpty()
			? TEXT("No preserved entry node was found for reconnection.")
			: FString::Printf(TEXT("No preserved entry node was found for reconnection: %s."), *Request.EntryName);
		return false;
	}

	UEdGraphPin* EntryExecOut = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(EntryNode, EGPD_Output);
	UEdGraphNode* FirstBodyNode = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstImportedExecutableBodyNode(Resolved.Graph, NodesBeforeImport);
	if (!EntryExecOut)
	{
		OutError = TEXT("Entry node or replacement body first node is missing an Exec pin.");
		return false;
	}

	if (!FirstBodyNode)
	{
		if (EntryExecOut->LinkedTo.Num() > 0)
		{
			FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(EntryExecOut);
			Resolved.Graph->NotifyGraphChanged();
		}
		return true;
	}

	UEdGraphPin* BodyExecIn = FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::FindFirstExecPin(FirstBodyNode, EGPD_Input);
	if (!BodyExecIn)
	{
		OutError = TEXT("Replacement body first node is missing an Exec input pin.");
		return false;
	}

	if (FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
	{
		return true;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutError = TEXT("K2 schema is unavailable; cannot rebuild entry exec link.");
		return false;
	}

	FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(EntryExecOut);
	FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BreakAllPinLinksWithModify(BodyExecIn);
	if (!Schema->TryCreateConnection(EntryExecOut, BodyExecIn) ||
		!FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
	{
		OutError = FString::Printf(TEXT("Cannot connect entry %s to replacement body first node %s."),
			*EntryNode->GetName(), *FirstBodyNode->GetName());
		return false;
	}

	Resolved.Graph->NotifyGraphChanged();
	return true;
}

// GraphWrite SemanticIR payload

FString FBlueprintHelperReplaceBlueprintGraphService::BuildSemanticGraphWritePayload(
	const FReplaceRequest& Request) const
{
	const FBlueprintHelperGraphWriteToolClusterPolicy Policy =
		FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy();
	FBlueprintHelperGraphWriteSemanticPayload Payload;
	Payload.TargetAssetPath = Request.AssetPath;
	Payload.TargetGraph = Request.GraphName;
	Payload.Mode = TEXT("replace");
	Payload.bCompile = Policy.bCompile;
	Payload.bSave = Policy.bSave;
	Payload.bStrict = Request.bStrict;
	Payload.bDryRun = Request.bDryRun;
	Payload.bCreateMissingVariables = Policy.bCreateMissingVariables;
	Payload.bReconstructExistingNodes =
		(Request.Scope == EBlueprintHelperReplaceScope::CustomEventBody && !Request.EntryName.TrimStartAndEnd().IsEmpty())
			? true
			: Policy.bReconstructExistingNodes;
	Payload.LogicSpec =
		FBlueprintHelperReplaceBlueprintGraphServiceLocalUtils::BuildLogicSpecWithCustomEventEntryReference(
			Request.LogicSpec,
			Request.Scope,
			Request.GraphName,
			Request.EntryName,
			Request.EventTaxonomy,
			Request.SignatureEvidenceId);
	return Payload.ToJsonString();
}
