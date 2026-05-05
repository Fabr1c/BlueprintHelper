// BlueprintHelper Service Layer — ConvertBlockToUserOwned 核心服务实现

#include "Services/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "GraphSupport/BlueprintHelperOwnershipService.h"
#include "Transactions/BlueprintHelperTransactionJournalService.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/BlueprintHelperAppendGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"

FBlueprintHelperConvertBlockToUserOwnedService::FBlueprintHelperConvertBlockToUserOwnedService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperOwnershipService& InOwnershipService,
	const FBlueprintHelperTransactionJournalService& InJournalService)
	: Resolver(InResolver), OwnershipService(InOwnershipService), JournalService(InJournalService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperConvertBlockToUserOwnedService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FConvertRequest Req = ParseRequest(Payload);
	if (Req.bDryRun) return ExecuteDryRun(Req);
	return ExecuteWrite(Req);
}

FBlueprintHelperConvertBlockToUserOwnedService::FConvertRequest
FBlueprintHelperConvertBlockToUserOwnedService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FConvertRequest R;
	if (!Payload.IsValid()) return R;

	Payload->TryGetStringField(TEXT("asset_path"), R.AssetPath);
	Payload->TryGetStringField(TEXT("graph"), R.GraphName);
	Payload->TryGetStringField(TEXT("graph_id"), R.GraphId);
	if (R.GraphId.IsEmpty()) Payload->TryGetStringField(TEXT("graph_id"), R.GraphId);
	else if (R.GraphName.IsEmpty()) R.GraphName = R.GraphId;
	Payload->TryGetStringField(TEXT("block_ref"), R.BlockRef);
	Payload->TryGetStringField(TEXT("block_id"), R.BlockId);
	Payload->TryGetBoolField(TEXT("dry_run"), R.bDryRun);

	FString S;
	if (Payload->TryGetStringField(TEXT("ownership_scope"), S))
		ParseOwnershipScope(S, R.OwnershipScope);
	if (Payload->TryGetStringField(TEXT("already_user_owned_policy"), S))
		ParseAlreadyUserOwnedPolicy(S, R.AlreadyUserOwnedPolicy);

	return R;
}

FBlueprintHelperConvertBlockToUserOwnedService::FConvertPreflightResult
FBlueprintHelperConvertBlockToUserOwnedService::Preflight(const FConvertRequest& Request, FResolvedBlock& OutTarget) const
{
	FConvertPreflightResult Result;

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
		return Result;
	}
	OutTarget.Blueprint = BP;

	FString ResolveErr;
	if (!ResolveBlock(Request, OutTarget, ResolveErr))
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("block_not_found"));
		Result.Conflicts.Add({TEXT("block_not_found"), ResolveErr, Request.GetEffectiveBlockId(), TEXT("block_id")});
		return Result;
	}

	if (!OutTarget.bIsOwned)
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("target_not_owned"));
		Result.Conflicts.Add({TEXT("target_not_owned"),
			FString::Printf(TEXT("Block %s 不属于 BlueprintHelper。"), *OutTarget.BlockId),
			OutTarget.BlockId, TEXT("ownership")});
		return Result;
	}

	if (OutTarget.bAlreadyUserOwned)
	{
		if (Request.AlreadyUserOwnedPolicy == EBlueprintHelperAlreadyUserOwnedPolicy::Error)
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("already_user_owned"));
			Result.Conflicts.Add({TEXT("already_user_owned"),
				FString::Printf(TEXT("Block %s 已经是 user-owned。"), *OutTarget.BlockId),
				OutTarget.BlockId, TEXT("ownership")});
		}
	}

	return Result;
}

bool FBlueprintHelperConvertBlockToUserOwnedService::ResolveBlock(
	const FConvertRequest& Request, FResolvedBlock& OutTarget, FString& OutError) const
{
	const FString EffectiveBlockId = Request.GetEffectiveBlockId();
	if (EffectiveBlockId.IsEmpty())
	{
		OutError = TEXT("无效请求：需要 block_id 或 graph_id+block_ref。");
		return false;
	}

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
			FMetaData& Meta = Pkg->GetMetaData();
			const FString Owned = Meta.GetValue(Node, TEXT("BlueprintHelperOwned"));
			const FString BId = Meta.GetValue(Node, TEXT("BlueprintHelperBlockId"));
			if (BId == EffectiveBlockId)
			{
				FoundNodes.Add(Node);
				FoundGraph = G;
				if (Owned != TEXT("true")) OutTarget.bAlreadyUserOwned = true;
				else OutTarget.bIsOwned = true;
			}
		}
	};

	const FString SearchGraph = Request.GraphName.IsEmpty() ? Request.GraphId : Request.GraphName;
	if (!SearchGraph.IsEmpty())
	{
		for (UEdGraph* P : OutTarget.Blueprint->UbergraphPages)
			if (P && P->GetName() == SearchGraph) { ScanGraph(P); break; }
	}
	else
	{
		for (UEdGraph* P : OutTarget.Blueprint->UbergraphPages) ScanGraph(P);
	}

	if (FoundNodes.Num() == 0)
	{
		OutError = FString::Printf(TEXT("block_not_found: %s"), *EffectiveBlockId);
		return false;
	}

	OutTarget.Graph = FoundGraph;
	OutTarget.BlockNodes = FoundNodes;
	OutTarget.BlockId = EffectiveBlockId;
	OutTarget.GraphName = FoundGraph ? FoundGraph->GetName() : FString();
	OutTarget.bFound = true;
	return true;
}

// ─── DryRun ───

FBlueprintHelperToolResultBase FBlueprintHelperConvertBlockToUserOwnedService::ExecuteDryRun(
	const FConvertRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FResolvedBlock Target;
	FConvertPreflightResult Pre = Preflight(Request, Target);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("convert_blueprint_helper_block_to_user_owned"), TraceId);
	MakeConvertTargetJson(Request, Result);

	if (Pre.bPassed)
	{
		FBlueprintHelperConvertBlockToUserOwnedDryRunData Data;
		Data.DryRun.Result = TEXT("passed");
		Data.DryRun.bCanExecute = true;
		Result.Data = Data.ToJson();
	}
	else
	{
		FBlueprintHelperConvertBlockToUserOwnedDryRunData Data;
		Data.DryRun.Result = TEXT("blocked");
		Data.DryRun.bCanExecute = false;
		Data.DryRun.BlockedBy = Pre.BlockedBy;
		for (const auto& C : Pre.Conflicts) Data.DryRun.Conflicts.Add(C);
		for (const auto& E : Pre.Errors) Data.DryRun.Errors.Add(E);
		Result.Data = Data.ToJson();
	}
	return Result;
}

// ─── Write ───

FBlueprintHelperToolResultBase FBlueprintHelperConvertBlockToUserOwnedService::ExecuteWrite(
	const FConvertRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FResolvedBlock Target;
	FConvertPreflightResult Pre = Preflight(Request, Target);

	// already_user_owned + ignore → no_op
	if (Target.bAlreadyUserOwned && Request.AlreadyUserOwnedPolicy == EBlueprintHelperAlreadyUserOwnedPolicy::Ignore)
	{
		FBlueprintHelperToolResultBase NoOp = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("convert_blueprint_helper_block_to_user_owned"), TraceId);
		MakeConvertTargetJson(Request, NoOp);

		FBlueprintHelperConvertBlockToUserOwnedResultData Data;
		Data.ConversionResult.ConvertedCount = 0;
		Data.ConversionResult.ConversionStatus = TEXT("already_user_owned");
		Data.ConversionResult.AlreadyUserOwnedPolicy = TEXT("ignore");
		NoOp.Data = Data.ToJson();

		FBlueprintHelperValidationSummary Val; Val.bShouldCompile = false; Val.bShouldSave = false;
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
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("convert_blueprint_helper_block_to_user_owned"), TraceId, E);
	}

	const FString TxId = JournalService.GenerateTransactionId();

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Convert Block To User Owned")), Target.Blueprint);

	// 转换 metadata
	FString ConvErr;
	if (!ConvertOwnershipMetadata(Target.BlockNodes, Target.BlockId, ConvErr))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("convert_blueprint_helper_block_to_user_owned"), TraceId,
			{TEXT("ownership_metadata_write_failed"), EBlueprintHelperToolStage::Execute,
			 ConvErr, false, EBlueprintHelperRollbackResult::RolledBack});
	}

	// Journal (内部记录，不暴露给 Agent)
	FBlueprintHelperAppendJournalRecord JRec;
	JRec.TransactionId = TxId;
	JRec.Tool = TEXT("ConvertBlueprintHelperBlockToUserOwned");
	JRec.Status = TEXT("applied");
	JRec.TargetAssets.Add(Request.AssetPath);
	JRec.GraphId = Target.GraphName;
	JRec.GraphName = Target.GraphName;
	JRec.BlockIds.Add(Target.BlockId);

	FString JErr;
	if (!JournalService.WriteAppendJournal(JRec, JErr))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("convert_blueprint_helper_block_to_user_owned"), TraceId,
			{TEXT("journal_write_failed"), EBlueprintHelperToolStage::Execute,
			 JErr, false, EBlueprintHelperRollbackResult::RolledBack});
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Target.Blueprint);
	if (Target.Blueprint->GetOutermost()) Target.Blueprint->GetOutermost()->MarkPackageDirty();
	Mutation.Commit();

	// Success (不含 write_ref)
	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("convert_blueprint_helper_block_to_user_owned"), TraceId);
	MakeConvertTargetJson(Request, Success);

	FBlueprintHelperConvertBlockToUserOwnedResultData Data;
	Data.ConversionResult.ConvertedCount = Target.BlockNodes.Num();
	Success.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Val;
	Val.bShouldCompile = false;
	Val.bShouldSave = true;
	Success.Validation = Val;

	return Success;
}

// ─── Metadata 转换 ───

bool FBlueprintHelperConvertBlockToUserOwnedService::ConvertOwnershipMetadata(
	const TArray<UEdGraphNode*>& Nodes, const FString& BlockId, FString& OutError) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;

		UPackage* Pkg = Node->GetOutermost();
		if (!Pkg) { OutError = TEXT("无法获取 Package。"); return false; }
		FMetaData& Meta = Pkg->GetMetaData();

		// 清除 BlueprintHelper ownership metadata
		Meta.RemoveValue(Node, TEXT("BlueprintHelperOwned"));
		Meta.RemoveValue(Node, TEXT("BlueprintHelperBlockId"));
		Meta.RemoveValue(Node, TEXT("BlueprintHelperTransactionId"));
		Meta.RemoveValue(Node, TEXT("BlueprintHelperTool"));
		Meta.RemoveValue(Node, TEXT("BlueprintHelperFeatureName"));

		// 清理 NodeComment 中的 [BlueprintHelper] managed block
		StripManagedNodeComment(Node);
	}
	return true;
}

void FBlueprintHelperConvertBlockToUserOwnedService::StripManagedNodeComment(UEdGraphNode* Node) const
{
	if (!Node) return;

	FString Comment = Node->NodeComment;
	if (Comment.IsEmpty()) return;

	// 查找并移除 [BlueprintHelper] block
	const FString Marker = TEXT("[BlueprintHelper]");
	int32 MarkerPos = Comment.Find(Marker, ESearchCase::CaseSensitive);
	if (MarkerPos == INDEX_NONE) return;

	// 找到下一行或 block 结尾
	int32 EndPos = Comment.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, MarkerPos + Marker.Len());
	if (EndPos == INDEX_NONE) EndPos = Comment.Len();

	FString Before = Comment.Left(MarkerPos).TrimEnd();
	FString After = (EndPos < Comment.Len()) ? Comment.Mid(EndPos + 1).TrimStart() : TEXT("");

	FString NewComment;
	if (!Before.IsEmpty()) NewComment = Before;
	if (!After.IsEmpty())
	{
		if (!NewComment.IsEmpty()) NewComment += TEXT("\n");
		NewComment += After;
	}

	// 去除多余的 "\n" 开头的 managed 行
	while (NewComment.StartsWith(TEXT("\n"))) NewComment = NewComment.Mid(1);
	while (NewComment.EndsWith(TEXT("\n"))) NewComment = NewComment.LeftChop(1);

	Node->Modify();
	Node->NodeComment = NewComment;
}

// ─── target JSON ───

void FBlueprintHelperConvertBlockToUserOwnedService::MakeConvertTargetJson(
	const FConvertRequest& Req, FBlueprintHelperToolResultBase& Out) const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("asset_path"), Req.AssetPath);
	J->SetStringField(TEXT("graph"), Req.GraphName.IsEmpty() ? Req.GraphId : Req.GraphName);
	J->SetStringField(TEXT("ownership_scope"), TEXT("block"));
	Out.CustomTargetJson = J;
}
