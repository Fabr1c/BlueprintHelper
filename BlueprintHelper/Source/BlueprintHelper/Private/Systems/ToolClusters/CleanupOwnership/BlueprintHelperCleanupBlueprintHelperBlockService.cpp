// BlueprintHelper Service Layer 。CleanupBlueprintHelperBlock 核心服务实现

#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FBlueprintHelperCleanupBlueprintHelperBlockService::FBlueprintHelperCleanupBlueprintHelperBlockService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperTransactionJournalService& InJournalService)
	: Resolver(InResolver), JournalService(InJournalService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperCleanupBlueprintHelperBlockService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FCleanupRequest Req = ParseRequest(Payload);
	if (Req.bDryRun) return ExecuteDryRun(Req);
	return ExecuteWrite(Req);
}

// ─── 解析 ───

FBlueprintHelperCleanupBlueprintHelperBlockService::FCleanupRequest
FBlueprintHelperCleanupBlueprintHelperBlockService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FCleanupRequest R;
	if (!Payload.IsValid()) return R;

	Payload->TryGetStringField(TEXT("asset_path"), R.AssetPath);
	Payload->TryGetStringField(TEXT("graph"), R.GraphName);
	Payload->TryGetStringField(TEXT("graph_id"), R.GraphName);
	if (R.GraphName.IsEmpty()) Payload->TryGetStringField(TEXT("graph_id"), R.GraphName);
	Payload->TryGetStringField(TEXT("block_ref"), R.BlockRef);
	Payload->TryGetStringField(TEXT("block_id"), R.BlockId);
	Payload->TryGetBoolField(TEXT("dry_run"), R.bDryRun);

	FString MP;
	if (Payload->TryGetStringField(TEXT("missing_policy"), MP))
		ParseMissingPolicy(MP, R.MissingPolicy);

	return R;
}

// ─── DryRun ───

FBlueprintHelperToolResultBase FBlueprintHelperCleanupBlueprintHelperBlockService::ExecuteDryRun(
	const FCleanupRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FResolvedBlockTarget Target;
	FCleanupPreflightResult Pre = Preflight(Request, Target);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("cleanup_blueprint_helper_block"), TraceId);
	MakeCleanupTargetJson(Request, Target, Result);

	if (Pre.bPassed)
	{
		FBlueprintHelperCleanupDryRunData Data;
		Data.DryRun.Result = TEXT("passed");
		Data.DryRun.bCanExecute = true;
		Result.Data = Data.ToJson();
	}
	else
	{
		FBlueprintHelperCleanupDryRunData Data;
		Data.DryRun.Result = TEXT("blocked");
		Data.DryRun.bCanExecute = false;
		Data.DryRun.BlockedBy = Pre.BlockedBy;
		for (const auto& C : Pre.Conflicts) Data.DryRun.Conflicts.Add(C);
		for (const auto& E : Pre.Errors) Data.DryRun.Errors.Add(E);
		Result.Data = Data.ToJson();
	}
	return Result;
}

// ─── Preflight ───

FBlueprintHelperCleanupBlueprintHelperBlockService::FCleanupPreflightResult
FBlueprintHelperCleanupBlueprintHelperBlockService::Preflight(
	const FCleanupRequest& Request, FResolvedBlockTarget& OutTarget) const
{
	FCleanupPreflightResult Result;

	if (Request.AssetPath.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		return Result;
	}

	FBlueprintHelperGraphTarget Tgt; Tgt.BlueprintPath = Request.AssetPath;
	FBlueprintHelperDiagnosticSet Diag;
	UBlueprint* BP = Resolver.ResolveBlueprint(Tgt, Diag);
	if (!BP)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_blueprint_not_found"));
		Result.Conflicts.Add({TEXT("target_blueprint_not_found"),
			FString::Printf(TEXT("蓝图 %s 未找到。"), *Request.AssetPath), Request.AssetPath, TEXT("asset_path")});
		return Result;
	}
	OutTarget.Blueprint = BP;

	FString ResolveErr;
	if (!ResolveBlock(Request, OutTarget, ResolveErr))
	{
		if (Request.MissingPolicy == EBlueprintHelperMissingPolicy::Ignore)
		{
			// 标记为缺失，不返回错误。这是合法 no_op 路径
			OutTarget.bFound = false;
			return Result;
		}
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("block_not_found"));
		Result.Conflicts.Add({TEXT("block_not_found"), ResolveErr, Request.GetEffectiveBlockId(), TEXT("block_id")});
		return Result;
	}

	if (!CheckOwnership(OutTarget, Result))
		return Result;

	if (!CheckDependencies(OutTarget, Result))
		return Result;

	return Result;
}

// ─── Block 解析 ───

bool FBlueprintHelperCleanupBlueprintHelperBlockService::ResolveBlock(
	const FCleanupRequest& Request, FResolvedBlockTarget& OutTarget, FString& OutError) const
{
	const FString EffectiveBlockId = Request.GetEffectiveBlockId();
	if (EffectiveBlockId.IsEmpty())
	{
		OutError = TEXT("无效请求：缺。block_id 。graph+block_ref。");
		return false;
	}

	// 遍历所。UbergraphPages 搜索 metadata
	UEdGraph* FoundGraph = nullptr;
	TArray<UEdGraphNode*> FoundNodes;

	auto ScanGraph = [&](UEdGraph* G)
	{
		if (!G) return;
		for (UEdGraphNode* Node : G->Nodes)
		{
			if (!Node) continue;
			UPackage* Pkg = Node->GetOutermost();
			if (!Pkg) continue;
			FBlueprintHelperPackageMetaData& Meta = FBlueprintHelperVersionCompat::GetPackageMetaData(Pkg);
			const FString Owned = Meta.GetValue(Node, TEXT("BlueprintHelperOwned"));
			const FString BlockId = Meta.GetValue(Node, TEXT("BlueprintHelperBlockId"));
			if (Owned == TEXT("true") && BlockId == EffectiveBlockId)
			{
				FoundNodes.Add(Node);
				FoundGraph = G;
			}
		}
	};

	if (!Request.GraphName.IsEmpty())
	{
		for (UEdGraph* Page : OutTarget.Blueprint->UbergraphPages)
		{
			if (Page && Page->GetName() == Request.GraphName) { ScanGraph(Page); break; }
		}
	}
	else
	{
		for (UEdGraph* Page : OutTarget.Blueprint->UbergraphPages)
			ScanGraph(Page);
	}

	if (FoundNodes.Num() == 0)
	{
		OutError = FString::Printf(TEXT("block_not_found: %s"), *EffectiveBlockId);
		return false;
	}

	OutTarget.Graph = FoundGraph;
	OutTarget.OwnedNodes = FoundNodes;
	OutTarget.BlockId = EffectiveBlockId;
	OutTarget.GraphName = FoundGraph ? FoundGraph->GetName() : FString();
	OutTarget.BlockRef = Request.BlockRef.IsEmpty()
		? (EffectiveBlockId.Contains(TEXT("_")) ? EffectiveBlockId.Mid(EffectiveBlockId.Find(TEXT("_")) + 1) : EffectiveBlockId)
		: Request.BlockRef;
	OutTarget.bFound = true;

	return true;
}

// ─── Ownership ───

bool FBlueprintHelperCleanupBlueprintHelperBlockService::CheckOwnership(
	const FResolvedBlockTarget& Target, FCleanupPreflightResult& OutResult) const
{
	for (UEdGraphNode* Node : Target.OwnedNodes)
	{
		if (!Node) continue;
		UPackage* Pkg = Node->GetOutermost();
		if (!Pkg) continue;
		FBlueprintHelperPackageMetaData& Meta = FBlueprintHelperVersionCompat::GetPackageMetaData(Pkg);
		const FString Owned = Meta.GetValue(Node, TEXT("BlueprintHelperOwned"));
		const FString BlockId = Meta.GetValue(Node, TEXT("BlueprintHelperBlockId"));

		if (Owned != TEXT("true") || BlockId != Target.BlockId)
		{
			OutResult.bPassed = false;
			OutResult.BlockedBy.Add(TEXT("ownership_mismatch"));
			OutResult.Conflicts.Add({TEXT("ownership_mismatch"),
				FString::Printf(TEXT("节点 %s 的 ownership 不匹配：owned=%s, block=%s"),
					*Node->GetName(), *Owned, *BlockId), Node->GetName(), TEXT("ownership")});
			return false;
		}
	}
	return true;
}

// ─── 依赖检。───

bool FBlueprintHelperCleanupBlueprintHelperBlockService::CheckDependencies(
	const FResolvedBlockTarget& Target, FCleanupPreflightResult& OutResult) const
{
	TSet<UEdGraphNode*> OwnedSet(Target.OwnedNodes);

	for (UEdGraphNode* Node : Target.OwnedNodes)
	{
		if (!Node) continue;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			// 外部 incoming link 。外部依赖。block
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && !OwnedSet.Contains(Linked->GetOwningNode()) && Pin->Direction == EGPD_Input)
				{
					OutResult.bPassed = false;
					OutResult.BlockedBy.Add(TEXT("external_dependents_exist"));
					OutResult.Conflicts.Add({TEXT("external_dependents_exist"),
						FString::Printf(TEXT("外部节点 %s 连接到此 block 。Pin %s。"),
							*Linked->GetOwningNode()->GetName(), *Pin->PinName.ToString()),
						Target.BlockId, TEXT("dependencies")});
					return false;
				}
			}
		}
	}
	return true;
}

// ─── 正式写入 ───

FBlueprintHelperToolResultBase FBlueprintHelperCleanupBlueprintHelperBlockService::ExecuteWrite(
	const FCleanupRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	FResolvedBlockTarget Target;
	FCleanupPreflightResult Pre = Preflight(Request, Target);

	// Missing + ignore -> no_op
	if (!Target.bFound && Request.MissingPolicy == EBlueprintHelperMissingPolicy::Ignore)
	{
		FBlueprintHelperToolResultBase NoOp = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("cleanup_blueprint_helper_block"), TraceId);

		FBlueprintHelperCleanupBlockResultData Data;
		Data.CleanupResult.CleanedRef.GraphId = Request.GraphName;
		Data.CleanupResult.CleanedRef.BlockRef = Request.BlockRef;
		Data.CleanupResult.MissingPolicy = TEXT("ignore");
		Data.CleanupResult.bMissing = true;
		// 不设。WriteRef
		NoOp.Data = Data.ToJson();

		FBlueprintHelperValidationSummary Val;
		Val.bShouldCompile = false;
		Val.bShouldSave = false;
		NoOp.Validation = Val;
		return NoOp;
	}

	if (!Pre.bPassed)
	{
		FBlueprintHelperToolError E;
		E.Code = Pre.BlockedBy.Num() > 0 ? Pre.BlockedBy[0] : TEXT("preflight_failed");
		E.Stage = EBlueprintHelperToolStage::Preflight;
		E.Message = Pre.Conflicts.Num() > 0 ? Pre.Conflicts[0].Message : TEXT("Preflight 未通过。");
		E.bRetryable = false;
		E.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("cleanup_blueprint_helper_block"), TraceId, E);
	}

	const FString TxId = JournalService.GenerateTransactionId();

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Cleanup Block")), Target.Blueprint);
	Mutation.Modify(Target.Graph);

	// 删除节点
	FString DelErr;
	if (!DeleteOwnedNodes(Target.Blueprint, Target.Graph, Target.OwnedNodes, DelErr))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("cleanup_blueprint_helper_block"), TraceId,
			{TEXT("node_delete_failed"), EBlueprintHelperToolStage::Execute,
			 DelErr, false, EBlueprintHelperRollbackResult::RolledBack});
	}

	// Journal
	FBlueprintHelperAppendJournalRecord JRec;
	JRec.TransactionId = TxId;
	JRec.Tool = TEXT("CleanupBlueprintHelperBlock");
	JRec.Status = TEXT("applied");
	JRec.TargetAssets.Add(Request.AssetPath);
	JRec.GraphId = Target.GraphName;
	JRec.GraphName = Target.GraphName;
	JRec.BlockIds.Add(Target.BlockId);

	FString JErr;
	if (!JournalService.WriteAppendJournal(JRec, JErr))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("cleanup_blueprint_helper_block"), TraceId,
			{TEXT("journal_write_failed"), EBlueprintHelperToolStage::Execute,
			 JErr, false, EBlueprintHelperRollbackResult::RolledBack});
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Target.Blueprint);
	if (Target.Blueprint->GetOutermost()) Target.Blueprint->GetOutermost()->MarkPackageDirty();
	Mutation.Commit();

	// Success
	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("cleanup_blueprint_helper_block"), TraceId);
	MakeCleanupTargetJson(Request, Target, Success);

	FBlueprintHelperCleanupBlockResultData Data;
	Data.CleanupResult.CleanedRef.GraphId = Target.GraphName;
	Data.CleanupResult.CleanedRef.BlockRef = Target.BlockRef;
	Data.WriteRef = FBlueprintHelperWriteRef{TxId, true};
	Success.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Val;
	Val.bShouldCompile = true;
	Val.bShouldSave = true;
	Success.Validation = Val;

	return Success;
}

// ─── 节点删除 ───

bool FBlueprintHelperCleanupBlueprintHelperBlockService::DeleteOwnedNodes(
	UBlueprint* BP, UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes, FString& OutError) const
{
	if (!BP || !Graph) { OutError = TEXT("蓝图或图表为空。"); return false; }

	// 先断所。Pin 链接
	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin) Pin->BreakAllPinLinks(true);
		}
	}

	// 删除节点
	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;
		FBlueprintEditorUtils::RemoveNode(BP, Node, true);
	}

	Graph->NotifyGraphChanged();
	return true;
}

// ─── target JSON ───

void FBlueprintHelperCleanupBlueprintHelperBlockService::MakeCleanupTargetJson(
	const FCleanupRequest& Req, const FResolvedBlockTarget& Tgt, FBlueprintHelperToolResultBase& Out) const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("asset_path"), Req.AssetPath);
	J->SetStringField(TEXT("graph"), Tgt.GraphName.IsEmpty() ? Req.GraphName : Tgt.GraphName);
	J->SetStringField(TEXT("cleanup_scope"), TEXT("block"));
	if (!Tgt.BlockRef.IsEmpty()) J->SetStringField(TEXT("block_ref"), Tgt.BlockRef);
	else if (!Req.BlockRef.IsEmpty()) J->SetStringField(TEXT("block_ref"), Req.BlockRef);
	Out.CustomTargetJson = J;
}
