// BlueprintHelper Review action service implementation.

#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewHashService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

class FBlueprintHelperReviewActionServiceLocalUtils
{
public:
	static bool ReviewTargetMatches(const FBlueprintHelperReviewAtomicTarget& Target, const TArray<FString>& TargetKeys)
	{
		return TargetKeys.Num() == 0 || TargetKeys.Contains(Target.TargetKey);
	}

	static EBlueprintHelperReviewChangeStatus CombineTargetStatuses(
		const TArray<FBlueprintHelperReviewAtomicTarget>& Targets)
	{
		if (Targets.Num() == 0)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}

		bool bAllAccepted = true;
		bool bAllRejected = true;
		bool bAnyNeedsAction = false;
		bool bAnyRejectFailed = false;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Targets)
		{
			bAllAccepted &= Target.Status == EBlueprintHelperReviewChangeStatus::Accepted;
			bAllRejected &= Target.Status == EBlueprintHelperReviewChangeStatus::Rejected;
			bAnyNeedsAction |= Target.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
			bAnyRejectFailed |= Target.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
		}

		if (bAnyRejectFailed)
		{
			return EBlueprintHelperReviewChangeStatus::RejectFailed;
		}
		if (bAnyNeedsAction)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}
		if (bAllAccepted)
		{
			return EBlueprintHelperReviewChangeStatus::Accepted;
		}
		if (bAllRejected)
		{
			return EBlueprintHelperReviewChangeStatus::Rejected;
		}
		return EBlueprintHelperReviewChangeStatus::Pending;
	}

	static EBlueprintHelperReviewChangeStatus CombineChangeStatuses(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		if (Changes.Num() == 0)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}

		bool bAllAccepted = true;
		bool bAllRejected = true;
		bool bAnyPending = false;
		bool bAnyNeedsAction = false;
		bool bAnyRejectFailed = false;
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			bAllAccepted &= Change.Status == EBlueprintHelperReviewChangeStatus::Accepted;
			bAllRejected &= Change.Status == EBlueprintHelperReviewChangeStatus::Rejected;
			bAnyPending |= Change.Status == EBlueprintHelperReviewChangeStatus::Pending;
			bAnyNeedsAction |= Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
			bAnyRejectFailed |= Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed;
		}

		if (bAnyRejectFailed)
		{
			return EBlueprintHelperReviewChangeStatus::RejectFailed;
		}
		if (bAnyNeedsAction)
		{
			return EBlueprintHelperReviewChangeStatus::NeedsAction;
		}
		if (bAnyPending)
		{
			return EBlueprintHelperReviewChangeStatus::Pending;
		}
		if (bAllAccepted)
		{
			return EBlueprintHelperReviewChangeStatus::Accepted;
		}
		if (bAllRejected)
		{
			return EBlueprintHelperReviewChangeStatus::Rejected;
		}
		return EBlueprintHelperReviewChangeStatus::Pending;
	}

	static void RefreshReviewRecordStatus(FBlueprintHelperReviewRecord& Record)
	{
		for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			Change.Status = CombineTargetStatuses(Change.AtomicTargets);
		}
		Record.Status = CombineChangeStatuses(Record.VisibleChanges);
		Record.SourceTransactionSummary.FinalReviewStatus = Record.Status;
	}

	static FBlueprintHelperReviewActionRecord MakeReviewActionRecord(
		const FString& Action,
		const TArray<FString>& TargetKeys,
		const FString& OwnershipPolicy,
		const FString& SourceTransactionId,
		const FString& Message)
	{
		FBlueprintHelperReviewActionRecord ActionRecord;
		ActionRecord.Action = Action;
		ActionRecord.TargetKeys = TargetKeys;
		ActionRecord.OwnershipPolicy = OwnershipPolicy;
		ActionRecord.SourceTransactionId = SourceTransactionId;
		ActionRecord.Message = Message;
		ActionRecord.CreatedAt = FDateTime::UtcNow().ToIso8601();
		return ActionRecord;
	}

	static TArray<FString> CollectPendingTargetKeys(const FBlueprintHelperReviewRecord& Record)
	{
		TArray<FString> TargetKeys;
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
			{
				if (Target.Status == EBlueprintHelperReviewChangeStatus::Pending)
				{
					TargetKeys.AddUnique(Target.TargetKey);
				}
			}
		}
		return TargetKeys;
	}

	static TArray<FString> CollectTargetKeysFromVisibleChange(const FBlueprintHelperReviewVisibleChange& Change)
	{
		TArray<FString> TargetKeys;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!Target.TargetKey.IsEmpty())
			{
				TargetKeys.AddUnique(Target.TargetKey);
			}
		}
		return TargetKeys;
	}

	static bool TryResolvePersistedReviewChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		FString& OutReviewRecordId,
		TArray<FString>& OutTargetKeys)
	{
		if (Change.AssetPath.IsEmpty())
		{
			return false;
		}

		FBlueprintHelperReviewRecordQuery Query;
		Query.AssetPathFilter = Change.AssetPath;
		Query.bPendingOnly = false;

		const TArray<FString> RequestedTargetKeys = CollectTargetKeysFromVisibleChange(Change);
		FBlueprintHelperReviewStoreService Store;
		const TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
		for (const FBlueprintHelperReviewRecord& Record : Records)
		{
			for (const FBlueprintHelperReviewVisibleChange& Candidate : Record.VisibleChanges)
			{
				const bool bSameChange =
					(!Change.ChangeId.IsEmpty() && Candidate.ChangeId == Change.ChangeId) ||
					(!Change.LocationKey.IsEmpty() && Candidate.LocationKey == Change.LocationKey) ||
					(!Change.LatestTransactionId.IsEmpty() && Candidate.LatestTransactionId == Change.LatestTransactionId);
				if (!bSameChange)
				{
					continue;
				}

				const TArray<FString> CandidateTargetKeys = CollectTargetKeysFromVisibleChange(Candidate);
				if (RequestedTargetKeys.Num() > 0)
				{
					bool bHasRequestedTarget = false;
					for (const FString& RequestedTargetKey : RequestedTargetKeys)
					{
						bHasRequestedTarget |= CandidateTargetKeys.Contains(RequestedTargetKey);
					}
					if (!bHasRequestedTarget)
					{
						continue;
					}
				}

				OutReviewRecordId = Record.ReviewRecordId;
				OutTargetKeys = RequestedTargetKeys.Num() > 0 ? RequestedTargetKeys : CandidateTargetKeys;
				return !OutReviewRecordId.IsEmpty() && OutTargetKeys.Num() > 0;
			}
		}

		return false;
	}

	static bool HasInjectedRejectOptions(const FBlueprintHelperReviewRejectOptions& Options)
	{
		return Options.CurrentHashesByTargetKey.Num() > 0
			|| Options.bRollbackExecutorAvailable
			|| Options.bRollbackSucceeded
			|| !Options.RollbackFailureMessage.IsEmpty();
	}

	static FBlueprintHelperReviewActionResult MakeRejectFailureResult(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewChangeStatus Status,
		const FString& Message)
	{
		FBlueprintHelperReviewActionResult Result;
		Result.TargetTransactionId = Change.LatestTransactionId;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.NewStatus = Status;
		Result.Message = Message;
		return Result;
	}

	static bool ExtractRollbackTransactionId(const FString& RollbackDataRef, FString& OutTransactionId)
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

	static bool LoadJournalRecordForReviewRollback(
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

	static FString ExtractReviewTargetTail(const FString& TargetKey, const FString& Marker)
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

	static bool TryFindReviewAtomicTarget(
		const FBlueprintHelperReviewRecord& Record,
		const FString& TargetKey,
		FBlueprintHelperReviewAtomicTarget& OutTarget)
	{
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
			{
				if (Target.TargetKey == TargetKey)
				{
					OutTarget = Target;
					return true;
				}
			}
		}
		return false;
	}

	static FString ResolveConversionTransactionId(
		const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
		const FBlueprintHelperTransactionJournalService& JournalService)
	{
		return Request.ConversionTransactionId.IsEmpty()
			? JournalService.GenerateTransactionId()
			: Request.ConversionTransactionId;
	}

	static FString MakeConvertBlockFailureMessage(const FBlueprintHelperToolResultBase& ToolResult)
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

	static bool ExecuteBhToUserOwnerBlockConversion(
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

	static UEdGraphNode* FindReviewNodeByAnchor(UEdGraph* Graph, const FString& Anchor)
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

			const FString NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
			if (Node->GetName() == NodeName || NodeGuid == NodeName)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static bool ExecuteUserToBhOwnerBlockConversion(
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

	static UEdGraph* FindReviewRollbackGraph(UBlueprint* Blueprint, const FString& GraphName)
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

	static void CollectRollbackNodesForTarget(
		UEdGraph* Graph,
		const FBlueprintHelperReviewAtomicTarget& Target,
		TArray<UEdGraphNode*>& OutNodes)
	{
		if (!Graph)
		{
			return;
		}

		if (Target.TargetKind == TEXT("graph_node") || Target.TargetKey.Contains(TEXT(":node:")))
		{
			const FString NodeName = Target.NodeGuid.IsEmpty()
				? ExtractReviewTargetTail(Target.TargetKey, TEXT("node"))
				: Target.NodeGuid;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->GetName() == NodeName)
				{
					OutNodes.AddUnique(Node);
					return;
				}
			}
			return;
		}

		if (Target.TargetKind == TEXT("graph_block") || Target.TargetKey.Contains(TEXT(":block:")))
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
				FMetaData& MetaData = Package->GetMetaData();
				if (MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")) == BlockId)
				{
					OutNodes.AddUnique(Node);
				}
			}
		}
	}

	static bool ExecuteGraphAppendRollback(
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
		if (Tool != TEXT("AppendBlueprintGraph"))
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
		if (NodesToRemove.Num() == 0)
		{
			OutError = FString::Printf(TEXT("anchor_not_found:%s"), *Target.TargetKey);
			return false;
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
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}
		return true;
	}

	static FBlueprintHelperReviewActionResult RejectVisibleChangeWithDefaultDispatcher(
		const FBlueprintHelperReviewVisibleChange& Change)
	{
		if (Change.AtomicTargets.Num() == 0)
		{
			return MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::NeedsAction,
				TEXT("missing_atomic_targets"));
		}

		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (Target.TargetKey.IsEmpty())
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_anchor"));
			}
			if (Target.RollbackDataRef.IsEmpty())
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_rollback_data_ref"));
			}
			if (Target.RecordedAfterHash.IsEmpty())
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_recorded_after_hash"));
			}

			FString CurrentHash;
			FString HashError;
			if (!FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(Target, CurrentHash, HashError))
			{
				return MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("current_hash_unavailable:%s"), *HashError));
			}
			if (CurrentHash != Target.RecordedAfterHash)
			{
				return MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("current_state_changed:%s"), *Target.TargetKey));
			}

			FString RollbackTransactionId;
			if (!ExtractRollbackTransactionId(Target.RollbackDataRef, RollbackTransactionId))
			{
				return MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("rollback_ref_unresolved:%s"), *Target.RollbackDataRef));
			}

			TSharedPtr<FJsonObject> JournalRecord;
			FString JournalError;
			if (!LoadJournalRecordForReviewRollback(RollbackTransactionId, JournalRecord, JournalError))
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, JournalError);
			}

			FString RollbackError;
			if (!ExecuteGraphAppendRollback(Target, JournalRecord, RollbackError))
			{
				return MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::RejectFailed, RollbackError);
			}
		}

		FBlueprintHelperReviewActionResult Result;
		Result.bSucceeded = true;
		Result.TargetTransactionId = Change.LatestTransactionId;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("rejected");
		Result.bSupersededDataCompactionEligible = true;
		return Result;
	}

};

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService() = default;

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService(
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: DebugEntryService(InDebugEntryService)
{
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::AcceptVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FString ReviewRecordId;
	TArray<FString> TargetKeys;
	if (FBlueprintHelperReviewActionServiceLocalUtils::TryResolvePersistedReviewChange(Change, ReviewRecordId, TargetKeys))
	{
		return AcceptReviewTargets(ReviewRecordId, TargetKeys);
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = true;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible =
		Change.SourceTransactionIds.Num() > FMath::Max(1, Change.LatestTransactionIds.Num());
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FString ReviewRecordId;
	TArray<FString> TargetKeys;
	if (FBlueprintHelperReviewActionServiceLocalUtils::TryResolvePersistedReviewChange(Change, ReviewRecordId, TargetKeys))
	{
		FBlueprintHelperReviewRejectOptions Options;
		return RejectReviewTargets(ReviewRecordId, TargetKeys, Options);
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = false;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = TEXT("Archive-baseline rollback backend is not wired in the first Review UI slice.");
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.RollbackMode = TEXT("archive_baseline");

	if (Change.AtomicTargets.Num() == 0)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = TEXT("missing_atomic_targets");
		return Result;
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.TargetKey.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_anchor");
			return Result;
		}
		if (Target.RollbackDataRef.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_rollback_data_ref");
			return Result;
		}
		if (Target.RecordedAfterHash.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_recorded_after_hash");
			return Result;
		}

		const FString* CurrentHash = Options.CurrentHashesByTargetKey.Find(Target.TargetKey);
		if (!CurrentHash)
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = FString::Printf(TEXT("missing_current_hash:%s"), *Target.TargetKey);
			return Result;
		}
		if (*CurrentHash != Target.RecordedAfterHash)
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = FString::Printf(TEXT("current_state_changed:%s"), *Target.TargetKey);
			return Result;
		}
	}

	if (!Options.bRollbackExecutorAvailable)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = TEXT("rollback_executor_unavailable");
		return Result;
	}

	if (!Options.bRollbackSucceeded)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = Options.RollbackFailureMessage.IsEmpty()
			? TEXT("rollback_failed")
			: Options.RollbackFailureMessage;
		return Result;
	}

	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	Result.Message = TEXT("rejected");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

void FBlueprintHelperReviewActionService::RecordRejectDebugCaseBestEffort(
	FBlueprintHelperReviewRecord& Record,
	const TArray<FString>& TargetKeys,
	const FString& SourceTransactionId,
	EBlueprintHelperReviewChangeStatus RejectStatus,
	const FString& RejectMessage) const
{
	if (!DebugEntryService ||
		(RejectStatus != EBlueprintHelperReviewChangeStatus::NeedsAction &&
		 RejectStatus != EBlueprintHelperReviewChangeStatus::RejectFailed))
	{
		return;
	}

	TSharedRef<FJsonObject> ToolSummary = MakeShared<FJsonObject>();
	ToolSummary->SetStringField(TEXT("review_record_id"), Record.ReviewRecordId);
	ToolSummary->SetStringField(TEXT("archive_session_id"), Record.ArchiveSessionId);
	ToolSummary->SetStringField(TEXT("asset_path"), Record.AssetPath);
	ToolSummary->SetStringField(TEXT("review_status"), BlueprintHelperReviewChangeStatusToString(RejectStatus));
	if (!SourceTransactionId.IsEmpty())
	{
		ToolSummary->SetStringField(TEXT("source_transaction_id"), SourceTransactionId);
	}
	if (!RejectMessage.IsEmpty())
	{
		ToolSummary->SetStringField(TEXT("message"), RejectMessage);
	}
	TArray<TSharedPtr<FJsonValue>> TargetKeyValues;
	for (const FString& TargetKey : TargetKeys)
	{
		if (!TargetKey.IsEmpty())
		{
			TargetKeyValues.Add(MakeShared<FJsonValueString>(TargetKey));
		}
	}
	if (TargetKeyValues.Num() > 0)
	{
		ToolSummary->SetArrayField(TEXT("target_keys"), TargetKeyValues);
	}

	FBlueprintHelperDebugEntryEventInput DebugInput;
	DebugInput.SourceLayer = TEXT("review");
	DebugInput.Source = RejectStatus == EBlueprintHelperReviewChangeStatus::RejectFailed
		? TEXT("review_reject_failed")
		: TEXT("review_reject_needs_action");
	DebugInput.Operation = TEXT("reject_review_targets");
	DebugInput.Stage = TEXT("reject");
	DebugInput.Severity = EBlueprintHelperDebugSeverity::Error;
	if (Record.SourceTaskRunIds.Num() > 0)
	{
		DebugInput.TaskRunId = Record.SourceTaskRunIds[0];
	}
	if (!Record.AssetPath.IsEmpty())
	{
		DebugInput.AssetPaths.Add(Record.AssetPath);
	}
	if (!Record.ReviewRecordId.IsEmpty())
	{
		DebugInput.ReviewRecordIds.Add(Record.ReviewRecordId);
	}
	if (!SourceTransactionId.IsEmpty())
	{
		FBlueprintHelperDebugTransactionLink TransactionLink;
		TransactionLink.TransactionId = SourceTransactionId;
		TransactionLink.Role = TEXT("review_reject_failed");
		TransactionLink.Source = TEXT("review");
		TransactionLink.Summary = TEXT("source transaction for review reject action");
		DebugInput.TransactionLinks.Add(TransactionLink);
	}
	DebugInput.Error.Code = DebugInput.Source;
	DebugInput.Error.Message = RejectMessage;
	DebugInput.RecommendedNext = TEXT("get_debug_case");
	DebugInput.ToolResultSummary = ToolSummary;

	const FBlueprintHelperDebugEntryRecordResult DebugResult =
		DebugEntryService->RecordEventBestEffort(DebugInput);
	if (DebugResult.bRecorded && !DebugResult.DebugCaseId.IsEmpty())
	{
		Record.DebugCaseIds.AddUnique(DebugResult.DebugCaseId);
	}
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::AcceptReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	bool bMatchedAny = false;
	FString SourceTransactionId;
	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!FBlueprintHelperReviewActionServiceLocalUtils::ReviewTargetMatches(Target, TargetKeys))
			{
				continue;
			}
			bMatchedAny = true;
			Target.Status = EBlueprintHelperReviewChangeStatus::Accepted;
			SourceTransactionId = Target.LatestTransactionId;
		}
	}

	if (!bMatchedAny)
	{
		Result.Message = TEXT("target_keys_not_found");
		return Result;
	}

	FBlueprintHelperReviewActionServiceLocalUtils::RefreshReviewRecordStatus(Record);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
		TEXT("accept"),
		TargetKeys,
		TEXT("keep_managed"),
		SourceTransactionId,
		TEXT("accepted")));

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.TargetTransactionId = SourceTransactionId;
	Result.NewStatus = Record.Status;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	bool bMatchedAny = false;
	bool bAllRejected = true;
	const bool bUseInjectedOptions = FBlueprintHelperReviewActionServiceLocalUtils::HasInjectedRejectOptions(Options);
	FString SourceTransactionId;
	FString LastMessage;
	EBlueprintHelperReviewChangeStatus LastStatus = EBlueprintHelperReviewChangeStatus::Rejected;

	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!FBlueprintHelperReviewActionServiceLocalUtils::ReviewTargetMatches(Target, TargetKeys))
			{
				continue;
			}

			bMatchedAny = true;
			FBlueprintHelperReviewVisibleChange TargetChange = Change;
			TargetChange.AtomicTargets.Reset();
			TargetChange.AtomicTargets.Add(Target);
			const FBlueprintHelperReviewActionResult TargetResult = bUseInjectedOptions
				? RejectVisibleChange(TargetChange, Options)
				: FBlueprintHelperReviewActionServiceLocalUtils::RejectVisibleChangeWithDefaultDispatcher(TargetChange);
			Target.Status = TargetResult.NewStatus;
			Change.NeedsActionReason = TargetResult.bSucceeded ? FString() : TargetResult.Message;
			SourceTransactionId = Target.LatestTransactionId;
			LastMessage = TargetResult.Message;
			LastStatus = TargetResult.NewStatus;
			bAllRejected &= TargetResult.bSucceeded;
		}
	}

	if (!bMatchedAny)
	{
		Result.Message = TEXT("target_keys_not_found");
		return Result;
	}

	FBlueprintHelperReviewActionServiceLocalUtils::RefreshReviewRecordStatus(Record);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
		TEXT("reject"),
		TargetKeys,
		TEXT("archive_baseline"),
		SourceTransactionId,
		LastMessage));
	RecordRejectDebugCaseBestEffort(
		Record,
		TargetKeys,
		SourceTransactionId,
		bAllRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus,
		LastMessage);

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = bAllRejected;
	Result.TargetTransactionId = SourceTransactionId;
	Result.NewStatus = bAllRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = LastMessage;
	Result.bSupersededDataCompactionEligible = bAllRejected;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectAll(
	const FBlueprintHelperReviewRecordQuery& Query,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
	if (Records.Num() == 0)
	{
		Result.bSucceeded = true;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("no_pending_review_targets");
		return Result;
	}

	bool bAllRejected = true;
	EBlueprintHelperReviewChangeStatus LastStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	FString LastMessage;
	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		const TArray<FString> TargetKeys = FBlueprintHelperReviewActionServiceLocalUtils::CollectPendingTargetKeys(Record);
		if (TargetKeys.Num() == 0)
		{
			continue;
		}

		const FBlueprintHelperReviewActionResult RecordResult = RejectReviewTargets(
			Record.ReviewRecordId,
			TargetKeys,
			Options);
		bAllRejected &= RecordResult.bSucceeded;
		LastStatus = RecordResult.NewStatus;
		LastMessage = RecordResult.Message;
	}

	Result.bSucceeded = bAllRejected;
	Result.NewStatus = bAllRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = LastMessage.IsEmpty() ? TEXT("reject_all") : LastMessage;
	Result.bSupersededDataCompactionEligible = bAllRejected;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::ConvertOwnerBlock(
	const FBlueprintHelperReviewConvertOwnerBlockRequest& Request) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(Request.ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	auto PersistFailure = [&Store, &Record, &Request, &Result](
		const FString& Message,
		const FString& SourceTransactionId) -> FBlueprintHelperReviewActionResult
	{
		TArray<FString> TargetKeys;
		if (!Request.BlockTargetKey.IsEmpty())
		{
			TargetKeys.Add(Request.BlockTargetKey);
		}
		Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
			TEXT("convert_owner_block"),
			TargetKeys,
			Request.Direction,
			SourceTransactionId,
			Message));

		FString SaveError;
		if (!Store.SaveReviewRecord(Record, SaveError))
		{
			Result.Message = SaveError;
			return Result;
		}

		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = Message;
		return Result;
	};

	if (!Request.bSettingProfileAllowsConversion)
	{
		return PersistFailure(TEXT("convert_owner_block_not_allowed_by_setting_profile"), FString());
	}
	if (Request.Direction != TEXT("bh_to_user") && Request.Direction != TEXT("user_to_bh"))
	{
		return PersistFailure(TEXT("invalid_convert_owner_block_direction"), FString());
	}
	if (Request.BlockTargetKey.IsEmpty() || Request.EntryAnchor.IsEmpty() || Request.DesiredBlockRef.IsEmpty())
	{
		return PersistFailure(TEXT("missing_convert_owner_block_anchor"), FString());
	}
	if (Request.NodeAnchors.Num() == 0)
	{
		return PersistFailure(TEXT("missing_convert_owner_block_node_anchors"), FString());
	}

	FBlueprintHelperReviewAtomicTarget MatchedTarget;
	if (!FBlueprintHelperReviewActionServiceLocalUtils::TryFindReviewAtomicTarget(Record, Request.BlockTargetKey, MatchedTarget))
	{
		return PersistFailure(TEXT("convert_owner_block_target_not_found"), FString());
	}
	if (MatchedTarget.TargetKind != TEXT("graph_block") && !MatchedTarget.TargetKey.Contains(TEXT(":block:")))
	{
		return PersistFailure(TEXT("convert_owner_block_requires_graph_block_target"), FString());
	}
	if (MatchedTarget.AssetPath.IsEmpty())
	{
		MatchedTarget.AssetPath = Record.AssetPath;
	}

	FBlueprintHelperTransactionJournalService JournalService;
	const FString ConversionTransactionId = FBlueprintHelperReviewActionServiceLocalUtils::ResolveConversionTransactionId(Request, JournalService);
	FString ConversionError;
	const bool bConverted = Request.Direction == TEXT("bh_to_user")
		? FBlueprintHelperReviewActionServiceLocalUtils::ExecuteBhToUserOwnerBlockConversion(
			MatchedTarget,
			Request,
			ConversionTransactionId,
			ConversionError)
		: FBlueprintHelperReviewActionServiceLocalUtils::ExecuteUserToBhOwnerBlockConversion(
			MatchedTarget,
			Request,
			ConversionTransactionId,
			ConversionError);
	if (!bConverted)
	{
		const FString FailureMessage = ConversionError.IsEmpty()
			? TEXT("convert_owner_block_failed")
			: ConversionError;
		return PersistFailure(FailureMessage, ConversionTransactionId);
	}

	bool bMatchedAny = false;
	const FString NewOwnership = Request.Direction == TEXT("bh_to_user")
		? TEXT("user_owned")
		: TEXT("blueprinthelper_owned");
	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (Target.TargetKey == Request.BlockTargetKey)
			{
				Target.Ownership = NewOwnership;
				bMatchedAny = true;
			}
		}
	}

	TArray<FString> ConvertedTargetKeys;
	ConvertedTargetKeys.Add(Request.BlockTargetKey);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionServiceLocalUtils::MakeReviewActionRecord(
		TEXT("convert_owner_block"),
		ConvertedTargetKeys,
		Request.Direction,
		ConversionTransactionId,
		TEXT("converted_owner_block")));
	Record.SourceTransactionSummary.TransactionIds.AddUnique(ConversionTransactionId);
	Record.SourceTransactionSummary.OperationKinds.AddUnique(TEXT("convert_owner_block"));
	if (!Record.AssetPath.IsEmpty())
	{
		Record.SourceTransactionSummary.AssetPaths.AddUnique(Record.AssetPath);
	}
	Record.SourceTransactionSummary.TransactionCount = Record.SourceTransactionSummary.TransactionIds.Num();

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.TargetTransactionId = ConversionTransactionId;
	Result.NewStatus = Record.Status;
	Result.Message = TEXT("converted_owner_block");
	return Result;
}
