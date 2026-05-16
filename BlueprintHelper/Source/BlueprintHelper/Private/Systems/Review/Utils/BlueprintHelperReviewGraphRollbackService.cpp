// BlueprintHelper Review BlueprintHelperReviewGraphRollbackService implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewGraphRollbackService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewHashService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"

bool FBlueprintHelperReviewGraphRollbackService::ExtractRollbackTransactionId(const FString& RollbackDataRef, FString& OutTransactionId)
	{
		const FString Prefix = TEXT("transaction://");
		const FString Suffix = TEXT("/rollback_data");
		if (!RollbackDataRef.StartsWith(Prefix) || !RollbackDataRef.EndsWith(Suffix))
		{
			return false;
		}

		OutTransactionId = RollbackDataRef.Mid(Prefix.Len());
		OutTransactionId.LeftChopInline(Suffix.Len());
		return !OutTransactionId.IsEmpty();
	}
bool FBlueprintHelperReviewGraphRollbackService::LoadJournalRecordForReviewRollback(
		const FString& TransactionId,
		TSharedPtr<FJsonObject>& OutRecord,
		FString& OutError)
	{
		const FString ActivePath = FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Transactions")
			/ TEXT("Active")
			/ FString::Printf(TEXT("%s.json"), *TransactionId);
		const FString ReviewPath = FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Review")
			/ FString::Printf(TEXT("%s.json"), *TransactionId);

		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *ActivePath)
			&& !FFileHelper::LoadFileToString(Content, *ReviewPath))
		{
			OutError = FString::Printf(TEXT("rollback_ref_not_found:%s"), *TransactionId);
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, OutRecord) || !OutRecord.IsValid())
		{
			OutError = FString::Printf(TEXT("rollback_ref_parse_failed:%s"), *TransactionId);
			return false;
		}

		FString RollbackDataString;
		const TSharedPtr<FJsonObject>* RollbackDataObject = nullptr;
		const bool bHasRollbackData =
			(OutRecord->TryGetStringField(TEXT("rollback_data"), RollbackDataString) && !RollbackDataString.IsEmpty()) ||
			(OutRecord->TryGetObjectField(TEXT("rollback_data"), RollbackDataObject) && RollbackDataObject && RollbackDataObject->IsValid());
		if (!bHasRollbackData)
		{
			OutError = FString::Printf(TEXT("rollback_data_missing:%s"), *TransactionId);
			return false;
		}

		return true;
	}
FString FBlueprintHelperReviewGraphRollbackService::ExtractReviewTargetTail(const FString& TargetKey, const FString& Marker)
	{
		const FString Token = Marker + TEXT(":");
		const int32 Pos = TargetKey.Find(Token, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Pos != INDEX_NONE)
		{
			return TargetKey.Mid(Pos + Token.Len());
		}

		int32 LastColon = INDEX_NONE;
		if (TargetKey.FindLastChar(TEXT(':'), LastColon))
		{
			return TargetKey.Mid(LastColon + 1);
		}
		return TargetKey;
	}
FString FBlueprintHelperReviewGraphRollbackService::NormalizeReviewGuidCandidate(const FString& Candidate)
	{
		FString Trimmed = Candidate;
		Trimmed.TrimStartAndEndInline();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}

		FGuid ParsedGuid;
		if (FGuid::Parse(Trimmed, ParsedGuid))
		{
			return ParsedGuid.ToString(EGuidFormats::Digits);
		}

		FString HexDigits;
		HexDigits.Reserve(Trimmed.Len());
		for (const TCHAR Ch : Trimmed)
		{
			if (FChar::IsHexDigit(Ch))
			{
				HexDigits.AppendChar(Ch);
			}
		}
		return HexDigits.Len() == 32 ? HexDigits : Trimmed;
	}
bool FBlueprintHelperReviewGraphRollbackService::DoesReviewNodeMatchStableId(const UEdGraphNode* Node, const FString& Candidate)
	{
		if (!Node || Candidate.IsEmpty())
		{
			return false;
		}

		if (Node->GetName().Equals(Candidate, ESearchCase::IgnoreCase))
		{
			return true;
		}

		const FString NodeGuidDigits = Node->NodeGuid.ToString(EGuidFormats::Digits);
		const FString CandidateGuidDigits = NormalizeReviewGuidCandidate(Candidate);
		if (!NodeGuidDigits.IsEmpty() && NodeGuidDigits.Equals(CandidateGuidDigits, ESearchCase::IgnoreCase))
		{
			return true;
		}

		if (UPackage* Package = Node->GetOutermost())
		{
			FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
			return MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")).Equals(Candidate, ESearchCase::IgnoreCase)
				|| MetaData.GetValue(Node, TEXT("BlueprintHelperTransactionId")).Equals(Candidate, ESearchCase::IgnoreCase)
				|| MetaData.GetValue(Node, TEXT("BlueprintHelperFeatureName")).Equals(Candidate, ESearchCase::IgnoreCase);
		}
		return false;
	}
UEdGraphPin* FBlueprintHelperReviewGraphRollbackService::FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
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
bool FBlueprintHelperReviewGraphRollbackService::NodeMatchesEntryName(UEdGraphNode* Node, const FString& EntryName)
	{
		if (!Node)
		{
			return false;
		}
		if (EntryName.IsEmpty())
		{
			return true;
		}

		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
		{
			if (CustomEvent->CustomFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			if (EventNode->GetFunctionName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				EventNode->EventReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
		{
			if (FunctionEntry->FunctionReference.GetMemberName().ToString().Equals(EntryName, ESearchCase::IgnoreCase) ||
				FunctionEntry->CustomGeneratedFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		const FString NodeName = Node->GetName();
		const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		return NodeName.Equals(EntryName, ESearchCase::IgnoreCase) ||
			NodeTitle.Equals(EntryName, ESearchCase::IgnoreCase);
	}
bool FBlueprintHelperReviewGraphRollbackService::HasInboundExecLinkFromImportedNode(UEdGraphPin* ExecInputPin, const TSet<UEdGraphNode*>& ImportedNodes)
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
UEdGraphNode* FBlueprintHelperReviewGraphRollbackService::FindFirstExecutableBodyNode(const TSet<UEdGraphNode*>& ImportedNodes)
	{
		TArray<UEdGraphNode*> ImportedExecutableNodes;
		for (UEdGraphNode* Node : ImportedNodes)
		{
			if (Node && FindFirstExecPin(Node, EGPD_Input))
			{
				ImportedExecutableNodes.Add(Node);
			}
		}

		ImportedExecutableNodes.Sort(
			[](const UEdGraphNode& Left, const UEdGraphNode& Right)
			{
				return Left.NodePosX == Right.NodePosX
					? Left.NodePosY < Right.NodePosY
					: Left.NodePosX < Right.NodePosX;
			});

		for (UEdGraphNode* Node : ImportedExecutableNodes)
		{
			if (!HasInboundExecLinkFromImportedNode(FindFirstExecPin(Node, EGPD_Input), ImportedNodes))
			{
				return Node;
			}
		}

		return ImportedExecutableNodes.Num() > 0 ? ImportedExecutableNodes[0] : nullptr;
	}
bool FBlueprintHelperReviewGraphRollbackService::PinsHaveSingleConnectionToEachOther(UEdGraphPin* FirstPin, UEdGraphPin* SecondPin)
	{
		return FirstPin &&
			SecondPin &&
			FirstPin->LinkedTo.Num() == 1 &&
			SecondPin->LinkedTo.Num() == 1 &&
			FirstPin->LinkedTo[0] == SecondPin &&
			SecondPin->LinkedTo[0] == FirstPin;
	}
void FBlueprintHelperReviewGraphRollbackService::BreakAllPinLinksWithModify(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return;
		}

		Pin->Modify();
		Pin->BreakAllPinLinks(true);
	}
bool FBlueprintHelperReviewGraphRollbackService::TryGetRollbackDataObject(
		const TSharedPtr<FJsonObject>& JournalRecord,
		TSharedPtr<FJsonObject>& OutRollbackData,
		FString& OutError)
	{
		OutRollbackData.Reset();
		if (!JournalRecord.IsValid())
		{
			OutError = TEXT("rollback_journal_missing");
			return false;
		}

		const TSharedPtr<FJsonObject>* RollbackDataObject = nullptr;
		if (JournalRecord->TryGetObjectField(TEXT("rollback_data"), RollbackDataObject) &&
			RollbackDataObject &&
			RollbackDataObject->IsValid())
		{
			OutRollbackData = *RollbackDataObject;
			return true;
		}

		FString RollbackDataString;
		if (JournalRecord->TryGetStringField(TEXT("rollback_data"), RollbackDataString) &&
			!RollbackDataString.IsEmpty())
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RollbackDataString);
			if (FJsonSerializer::Deserialize(Reader, OutRollbackData) && OutRollbackData.IsValid())
			{
				return true;
			}
			OutError = TEXT("rollback_data_parse_failed");
			return false;
		}

		OutError = TEXT("rollback_data_missing");
		return false;
	}
FString FBlueprintHelperReviewGraphRollbackService::ResolveConversionTransactionId(
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
		const FBlueprintHelperTransactionJournalService& JournalService)
	{
		return Request.ConversionTransactionId.IsEmpty()
			? JournalService.GenerateTransactionId()
			: Request.ConversionTransactionId;
	}
FString FBlueprintHelperReviewGraphRollbackService::MakeConvertBlockFailureMessage(const FBlueprintHelperToolResultBase& ToolResult)
	{
		if (ToolResult.Error.IsSet())
		{
			if (!ToolResult.Error->Code.IsEmpty() && !ToolResult.Error->Message.IsEmpty())
			{
				return FString::Printf(TEXT("%s:%s"), *ToolResult.Error->Code, *ToolResult.Error->Message);
			}
			if (!ToolResult.Error->Code.IsEmpty())
			{
				return ToolResult.Error->Code;
			}
			if (!ToolResult.Error->Message.IsEmpty())
			{
				return ToolResult.Error->Message;
			}
		}
		return TEXT("convert_owner_block_failed");
	}
bool FBlueprintHelperReviewGraphRollbackService::ExecuteBhToUserOwnerBlockConversion(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
		const FString& ConversionTransactionId,
		FString& OutError)
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperOwnershipService OwnershipService;
		FBlueprintHelperTransactionJournalService JournalService;
		FBlueprintHelperConvertBlockToUserOwnedService ConvertService(
			Resolver,
			OwnershipService,
			JournalService);

		const FString BlockRef = Request.DesiredBlockRef.IsEmpty()
			? ExtractReviewTargetTail(Target.TargetKey, TEXT("block"))
			: Request.DesiredBlockRef;
		const FString BlockId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(
			Target.GraphName,
			BlockRef);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), Target.AssetPath);
		Payload->SetStringField(TEXT("graph"), Target.GraphName);
		Payload->SetStringField(TEXT("block_ref"), BlockRef);
		Payload->SetStringField(TEXT("block_id"), BlockId);
		Payload->SetStringField(TEXT("ownership_scope"), TEXT("block"));
		Payload->SetStringField(TEXT("already_user_owned_policy"), TEXT("error"));
		Payload->SetStringField(TEXT("transaction_id"), ConversionTransactionId);
		Payload->SetBoolField(TEXT("dry_run"), false);

		const FBlueprintHelperToolResultBase ToolResult = ConvertService.Execute(Payload);
		if (!ToolResult.bOk)
		{
			OutError = MakeConvertBlockFailureMessage(ToolResult);
			return false;
		}
		return true;
	}
UEdGraphNode* FBlueprintHelperReviewGraphRollbackService::FindReviewNodeByAnchor(UEdGraph* Graph, const FString& Anchor)
	{
		if (!Graph || Anchor.IsEmpty())
		{
			return nullptr;
		}

		FString NodeName = Anchor.Contains(TEXT(":entry:"))
			? ExtractReviewTargetTail(Anchor, TEXT("entry"))
			: ExtractReviewTargetTail(Anchor, TEXT("node"));
		if (NodeName.IsEmpty())
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (DoesReviewNodeMatchStableId(Node, NodeName))
			{
				return Node;
			}
		}
		return nullptr;
	}
bool FBlueprintHelperReviewGraphRollbackService::ExecuteUserToBhOwnerBlockConversion(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
		const FString& ConversionTransactionId,
		FString& OutError)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		UEdGraph* Graph = FindReviewRollbackGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		TArray<UEdGraphNode*> Nodes;
		if (UEdGraphNode* EntryNode = FindReviewNodeByAnchor(Graph, Request.EntryAnchor))
		{
			Nodes.AddUnique(EntryNode);
		}
		for (const FString& NodeAnchor : Request.NodeAnchors)
		{
			if (UEdGraphNode* Node = FindReviewNodeByAnchor(Graph, NodeAnchor))
			{
				Nodes.AddUnique(Node);
			}
			else
			{
				OutError = FString::Printf(TEXT("node_anchor_not_found:%s"), *NodeAnchor);
				return false;
			}
		}

		if (Nodes.Num() == 0)
		{
			OutError = TEXT("missing_convert_owner_block_nodes");
			return false;
		}

		const FString BlockId = FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(
			Target.GraphName,
			Request.DesiredBlockRef);

		FBlueprintHelperScopedAssetMutation Mutation(
			FText::FromString(TEXT("BlueprintHelper Review Convert Owner Block")),
			Blueprint);
		Mutation.Modify(Graph);
		for (UEdGraphNode* Node : Nodes)
		{
			if (Node)
			{
				Mutation.Modify(Node);
			}
		}

		FBlueprintHelperOwnershipService OwnershipService;
		FString OwnershipError;
		if (!OwnershipService.WriteBlockOwnership(
			Blueprint,
			Nodes,
			BlockId,
			ConversionTransactionId,
			Request.DesiredBlockRef,
			OwnershipError))
		{
			Mutation.Rollback();
			OutError = OwnershipError;
			return false;
		}

		FBlueprintHelperAppendJournalRecord JournalRecord;
		JournalRecord.TransactionId = ConversionTransactionId;
		JournalRecord.Tool = TEXT("ConvertOwnerBlock");
		JournalRecord.Status = TEXT("applied");
		JournalRecord.TargetAssets.Add(Target.AssetPath);
		JournalRecord.GraphId = Target.GraphName;
		JournalRecord.GraphName = Target.GraphName;
		JournalRecord.BlockIds.Add(BlockId);
		JournalRecord.RollbackData = FString::Printf(
			TEXT("{\"direction\":\"user_to_bh\",\"block_id\":\"%s\"}"),
			*BlockId);

		FBlueprintHelperTransactionJournalService JournalService;
		FString JournalError;
		if (!JournalService.WriteAppendJournal(JournalRecord, JournalError))
		{
			Mutation.Rollback();
			OutError = JournalError;
			return false;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}
		Mutation.Commit();
		return true;
	}
UEdGraph* FBlueprintHelperReviewGraphRollbackService::FindReviewRollbackGraph(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		auto FindIn = [&GraphName](const TArray<UEdGraph*>& Graphs) -> UEdGraph*
		{
			for (UEdGraph* Graph : Graphs)
			{
				if (Graph && (GraphName.IsEmpty() || Graph->GetName() == GraphName))
				{
					return Graph;
				}
			}
			return nullptr;
		};

		if (UEdGraph* Graph = FindIn(Blueprint->UbergraphPages))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindIn(Blueprint->FunctionGraphs))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindIn(Blueprint->MacroGraphs))
		{
			return Graph;
		}
		return nullptr;
	}
void FBlueprintHelperReviewGraphRollbackService::CollectRollbackNodesForTarget(
		UEdGraph* Graph,
		const FBlueprintHelperReviewAtomicTarget& Target,
		TArray<UEdGraphNode*>& OutNodes)
	{
		if (!Graph)
		{
			return;
		}

		if (FBlueprintHelperReviewTargetKindRegistry::IsGraphNodeTarget(Target.TargetKind, Target.TargetKey))
		{
			const FString NodeName = Target.NodeGuid.IsEmpty()
				? ExtractReviewTargetTail(Target.TargetKey, TEXT("node"))
				: Target.NodeGuid;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (DoesReviewNodeMatchStableId(Node, NodeName))
				{
					OutNodes.AddUnique(Node);
					return;
				}
			}
			return;
		}

		if (FBlueprintHelperReviewTargetKindRegistry::IsGraphBlockTarget(Target.TargetKind, Target.TargetKey))
		{
			const FString BlockId = ExtractReviewTargetTail(Target.TargetKey, TEXT("block"));
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					continue;
				}
				UPackage* Package = Node->GetOutermost();
				if (!Package)
				{
					continue;
				}
				FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
				if (MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")) == BlockId)
				{
					OutNodes.AddUnique(Node);
				}
			}
		}
	}
bool FBlueprintHelperReviewGraphRollbackService::ExecuteGraphAppendRollback(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& JournalRecord,
		FString& OutError)
	{
		FString Tool;
		if (!JournalRecord.IsValid() || !JournalRecord->TryGetStringField(TEXT("tool"), Tool))
		{
			OutError = TEXT("rollback_journal_tool_missing");
			return false;
		}
		const bool bAppendRollback = Tool == TEXT("AppendBlueprintGraph");
		const bool bReplaceRollback = Tool == TEXT("ReplaceBlueprintGraph");
		if (!bAppendRollback && !bReplaceRollback)
		{
			OutError = FString::Printf(TEXT("rollback_executor_unimplemented:%s"), *Tool);
			return false;
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		UEdGraph* Graph = FindReviewRollbackGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		TArray<UEdGraphNode*> NodesToRemove;
		CollectRollbackNodesForTarget(Graph, Target, NodesToRemove);
		if (!bReplaceRollback && NodesToRemove.Num() == 0)
		{
			OutError = FString::Printf(TEXT("anchor_not_found:%s"), *Target.TargetKey);
			return false;
		}

		TSharedPtr<FJsonObject> RollbackData;
		FString ExportedText;
		FString EntryIdentity;
		FString ReplaceScope;
		FString OwnerBlockId;
		bool bNeedsEntryReconnect = false;
		if (bReplaceRollback)
		{
			if (!TryGetRollbackDataObject(JournalRecord, RollbackData, OutError))
			{
				return false;
			}
			RollbackData->TryGetStringField(TEXT("exported_text"), ExportedText);
			RollbackData->TryGetStringField(TEXT("entry_identity"), EntryIdentity);
			RollbackData->TryGetStringField(TEXT("replace_scope"), ReplaceScope);
			RollbackData->TryGetStringField(TEXT("owner_block_id"), OwnerBlockId);
			bNeedsEntryReconnect =
				ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase) ||
				ReplaceScope.Equals(TEXT("event_body"), ESearchCase::IgnoreCase) ||
				ReplaceScope.Equals(TEXT("custom_event_body"), ESearchCase::IgnoreCase);
			if (ExportedText.IsEmpty())
			{
				OutError = TEXT("replace_rollback_exported_text_missing");
				return false;
			}
			if (!FEdGraphUtilities::CanImportNodesFromText(Graph, ExportedText))
			{
				OutError = TEXT("replace_rollback_exported_text_not_importable");
				return false;
			}
			if (bNeedsEntryReconnect)
			{
				NodesToRemove.RemoveAll(
					[](UEdGraphNode* Node)
					{
						return Node && (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_CustomEvent>() || Node->IsA<UK2Node_Event>());
					});
			}
			if (NodesToRemove.Num() == 0)
			{
				OutError = FString::Printf(TEXT("anchor_not_found:%s"), *Target.TargetKey);
				return false;
			}
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Reject")));
		Blueprint->Modify();
		Graph->Modify();
		for (UEdGraphNode* Node : NodesToRemove)
		{
			if (Node)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}
		if (bReplaceRollback)
		{
			TSet<UEdGraphNode*> ImportedNodes;
			FEdGraphUtilities::ImportNodesFromText(Graph, ExportedText, ImportedNodes);
			if (ImportedNodes.Num() == 0)
			{
				OutError = TEXT("replace_rollback_imported_no_nodes");
				return false;
			}

			if (!OwnerBlockId.IsEmpty())
			{
				TArray<UEdGraphNode*> ImportedNodeArray;
				for (UEdGraphNode* Node : ImportedNodes)
				{
					if (Node)
					{
						ImportedNodeArray.Add(Node);
					}
				}

				FBlueprintHelperOwnershipService OwnershipService;
				FString OwnershipError;
				const FString RollbackTransactionId = Target.LatestTransactionId.IsEmpty()
					? TEXT("review_reject")
					: FString::Printf(TEXT("%s_reject"), *Target.LatestTransactionId);
				if (!OwnershipService.WriteBlockOwnership(
					Blueprint,
					ImportedNodeArray,
					OwnerBlockId,
					RollbackTransactionId,
					TEXT("ReplaceRollback"),
					OwnershipError))
				{
					OutError = OwnershipError.IsEmpty() ? TEXT("replace_rollback_ownership_write_failed") : OwnershipError;
					return false;
				}
			}

			if (bNeedsEntryReconnect)
			{
				UEdGraphNode* EntryNode = nullptr;
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (!Node || ImportedNodes.Contains(Node))
					{
						continue;
					}

					const bool bMatchesScope =
						(ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase) && Node->IsA<UK2Node_FunctionEntry>()) ||
						(!ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase) && FindFirstExecPin(Node, EGPD_Output) != nullptr);
					if (bMatchesScope && NodeMatchesEntryName(Node, EntryIdentity))
					{
						EntryNode = Node;
						break;
					}
				}

				if (!EntryNode)
				{
					OutError = EntryIdentity.IsEmpty()
						? TEXT("replace_rollback_entry_not_found")
						: FString::Printf(TEXT("replace_rollback_entry_not_found:%s"), *EntryIdentity);
					return false;
				}

				UEdGraphPin* EntryExecOut = FindFirstExecPin(EntryNode, EGPD_Output);
				if (!EntryExecOut)
				{
					OutError = TEXT("replace_rollback_entry_exec_missing");
					return false;
				}

				if (UEdGraphNode* FirstBodyNode = FindFirstExecutableBodyNode(ImportedNodes))
				{
					UEdGraphPin* BodyExecIn = FindFirstExecPin(FirstBodyNode, EGPD_Input);
					if (!BodyExecIn)
					{
						OutError = TEXT("replace_rollback_body_exec_missing");
						return false;
					}

					if (!PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
					{
						const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
						if (!Schema)
						{
							OutError = TEXT("replace_rollback_k2_schema_missing");
							return false;
						}
						BreakAllPinLinksWithModify(EntryExecOut);
						BreakAllPinLinksWithModify(BodyExecIn);
						if (!Schema->TryCreateConnection(EntryExecOut, BodyExecIn) ||
							!PinsHaveSingleConnectionToEachOther(EntryExecOut, BodyExecIn))
						{
							OutError = TEXT("replace_rollback_entry_reconnect_failed");
							return false;
						}
					}
				}
				else
				{
					BreakAllPinLinksWithModify(EntryExecOut);
				}
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}
		Graph->NotifyGraphChanged();
		return true;
	}
