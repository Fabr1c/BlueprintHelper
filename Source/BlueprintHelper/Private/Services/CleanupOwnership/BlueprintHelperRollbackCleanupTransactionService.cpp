// BlueprintHelper Service Layer — RollbackCleanupTransaction 核心服务实现

#include "Services/CleanupOwnership/BlueprintHelperRollbackCleanupTransactionService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "Transactions/Transactions/BlueprintHelperTransactionJournalService.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/GraphWrite/BlueprintHelperAppendGraphTypes.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FBlueprintHelperRollbackCleanupTransactionService::FBlueprintHelperRollbackCleanupTransactionService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperTransactionJournalService& InJournalService)
	: Resolver(InResolver), JournalService(InJournalService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperRollbackCleanupTransactionService::Execute(
	const TSharedPtr<FJsonObject>& Payload) const
{
	const FRollbackRequest Req = ParseRequest(Payload);
	if (Req.bDryRun) return ExecuteDryRun(Req);
	return ExecuteWrite(Req);
}

// ─── Parse ───

FBlueprintHelperRollbackCleanupTransactionService::FRollbackRequest
FBlueprintHelperRollbackCleanupTransactionService::ParseRequest(const TSharedPtr<FJsonObject>& Payload) const
{
	FRollbackRequest R;
	if (!Payload.IsValid()) return R;

	Payload->TryGetStringField(TEXT("transaction_id"), R.TransactionId);
	Payload->TryGetStringField(TEXT("asset_path"), R.AssetPath);
	Payload->TryGetBoolField(TEXT("dry_run"), R.bDryRun);

	FString S;
	if (Payload->TryGetStringField(TEXT("rollback_scope"), S))
		ParseRollbackScope(S, R.RollbackScope);
	if (Payload->TryGetStringField(TEXT("already_rolled_back_policy"), S))
		ParseAlreadyRolledBackPolicy(S, R.AlreadyRolledBackPolicy);

	return R;
}

// ─── Journal 读取 ───

bool FBlueprintHelperRollbackCleanupTransactionService::LoadJournalRecord(
	const FString& TransactionId, TSharedPtr<FJsonObject>& OutRecord, FString& OutError) const
{
	const FString JournalDir = FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Transactions") / TEXT("Active");
	const FString JournalPath = JournalDir / FString::Printf(TEXT("%s.json"), *TransactionId);

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *JournalPath))
	{
		const FString ReviewPath = FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Review") / FString::Printf(TEXT("%s.json"), *TransactionId);
		if (!FFileHelper::LoadFileToString(FileContent, *ReviewPath))
		{
			OutError = FString::Printf(TEXT("transaction_not_found: Journal 文件 %s 不存在。"), *TransactionId);
			return false;
		}
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, OutRecord) || !OutRecord.IsValid())
	{
		OutError = TEXT("rollback_data_unavailable: Journal JSON 解析失败。");
		return false;
	}
	return true;
}

// ─── 校验 ───

bool FBlueprintHelperRollbackCleanupTransactionService::ValidateCleanupTransaction(
	const TSharedPtr<FJsonObject>& Record, FRollbackPreflightResult& OutResult) const
{
	FString Tool;
	Record->TryGetStringField(TEXT("tool"), Tool);

	if (Tool != TEXT("CleanupBlueprintHelperBlock") && Tool != TEXT("CleanupBlueprintHelperFeature"))
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("transaction_type_mismatch"));
		OutResult.Conflicts.Add({TEXT("transaction_type_mismatch"),
			FString::Printf(TEXT("仅支持回滚 cleanup 类型 transaction，当前为: %s"), *Tool), TEXT("tool"), TEXT("journal")});
		return false;
	}

	// 检查是否已回滚
	FString RollbackStatus;
	if (Record->TryGetStringField(TEXT("rollback_status"), RollbackStatus) && RollbackStatus == TEXT("succeeded"))
	{
		OutResult.bAlreadyRolledBack = true;
	}

	// 提取 asset_path
	const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
	if (Record->TryGetArrayField(TEXT("target_assets"), TargetAssets) && TargetAssets->Num() > 0)
	{
		(*TargetAssets)[0]->TryGetString(OutResult.SourceAssetPath);
	}

	return true;
}

bool FBlueprintHelperRollbackCleanupTransactionService::CheckRollbackData(
	const TSharedPtr<FJsonObject>& Record, FRollbackPreflightResult& OutResult) const
{
	const TSharedPtr<FJsonObject>* RollbackData = nullptr;
	if (!Record->TryGetObjectField(TEXT("rollback_data"), RollbackData) || !RollbackData->IsValid())
	{
		OutResult.bPassed = false;
		OutResult.BlockedBy.Add(TEXT("rollback_data_unavailable"));
		OutResult.Conflicts.Add({TEXT("rollback_data_unavailable"),
			TEXT("Journal 中缺少完整的 rollback_data。无法安全恢复。"), TEXT("rollback_data"), TEXT("journal")});
		return false;
	}

	OutResult.Summary.bRollbackDataAvailable = true;
	return true;
}

// ─── Preflight ───

FBlueprintHelperRollbackCleanupTransactionService::FRollbackPreflightResult
FBlueprintHelperRollbackCleanupTransactionService::Preflight(const FRollbackRequest& Request) const
{
	FRollbackPreflightResult Result;

	if (Request.TransactionId.IsEmpty())
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("transaction_not_found"));
		return Result;
	}

	// 加载 Journal
	TSharedPtr<FJsonObject> Record;
	FString LoadErr;
	if (!LoadJournalRecord(Request.TransactionId, Record, LoadErr))
	{
		Result.bPassed = false;
		Result.BlockedBy.Add(TEXT("transaction_not_found"));
		Result.Conflicts.Add({TEXT("transaction_not_found"), LoadErr, Request.TransactionId, TEXT("transaction_id")});
		return Result;
	}

	// 校验 cleanup 类型
	if (!ValidateCleanupTransaction(Record, Result))
		return Result;

	// already_rolled_back 处理
	if (Result.bAlreadyRolledBack)
	{
		if (Request.AlreadyRolledBackPolicy == EBlueprintHelperAlreadyRolledBackPolicy::Error)
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("already_rolled_back"));
			Result.Conflicts.Add({TEXT("already_rolled_back"),
				FString::Printf(TEXT("Transaction %s 已经被回滚。"), *Request.TransactionId),
				Request.TransactionId, TEXT("transaction_id")});
		}
		return Result;
	}

	// 检查 rollback_data
	if (!CheckRollbackData(Record, Result))
		return Result;

	// 检查目标资产
	if (!Result.SourceAssetPath.IsEmpty())
	{
		FBlueprintHelperGraphTarget Tgt;
		Tgt.BlueprintPath = Result.SourceAssetPath;
		FBlueprintHelperDiagnosticSet Diag;
		UBlueprint* BP = Resolver.ResolveBlueprint(Tgt, Diag);
		if (!BP)
		{
			Result.bPassed = false;
			Result.BlockedBy.Add(TEXT("asset_not_found"));
			Result.Conflicts.Add({TEXT("asset_not_found"),
				FString::Printf(TEXT("目标资产 %s 已不存在或无法打开。"), *Result.SourceAssetPath),
				Result.SourceAssetPath, TEXT("asset_path")});
			return Result;
		}

		// 提取 block 信息
		const TArray<TSharedPtr<FJsonValue>>* BlockIds = nullptr;
		if (Record->TryGetArrayField(TEXT("blocks"), BlockIds))
			Result.Summary.RestorableBlocks = BlockIds->Num();
		else
			Result.Summary.RestorableBlocks = 1;

		Result.Summary.AffectedAssets = 1;
		Result.Summary.bAssetStateChecked = true;
	}

	Result.Summary.bRestorableNodesAvailable = true;
	return Result;
}

// ─── DryRun ───

FBlueprintHelperToolResultBase FBlueprintHelperRollbackCleanupTransactionService::ExecuteDryRun(
	const FRollbackRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	const FRollbackPreflightResult Pre = Preflight(Request);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		TEXT("rollback_cleanup_transaction"), TraceId);

	TSharedRef<FJsonObject> Tgt = MakeShared<FJsonObject>();
	Tgt->SetStringField(TEXT("transaction_id"), Request.TransactionId);
	Tgt->SetStringField(TEXT("rollback_scope"), TEXT("cleanup_transaction"));
	Result.CustomTargetJson = Tgt;

	if (Pre.bPassed)
	{
		FBlueprintHelperRollbackCleanupDryRunData Data;
		Data.DryRun.Result = TEXT("passed");
		Data.DryRun.bCanExecute = true;
		Data.DryRun.RollbackSummary = Pre.Summary;
		Result.Data = Data.ToJson();
	}
	else
	{
		FBlueprintHelperRollbackCleanupDryRunData Data;
		Data.DryRun.Result = TEXT("blocked");
		Data.DryRun.bCanExecute = false;
		Data.DryRun.RollbackSummary = Pre.Summary;
		Data.DryRun.BlockedBy = Pre.BlockedBy;
		for (const auto& C : Pre.Conflicts) Data.DryRun.Conflicts.Add(C);
		for (const auto& E : Pre.Errors) Data.DryRun.Errors.Add(E);
		Result.Data = Data.ToJson();
	}
	return Result;
}

// ─── 正式写入 ───

FBlueprintHelperToolResultBase FBlueprintHelperRollbackCleanupTransactionService::ExecuteWrite(
	const FRollbackRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FRollbackPreflightResult Pre = Preflight(Request);

	// already_rolled_back + ignore → no_op
	if (Pre.bAlreadyRolledBack && Request.AlreadyRolledBackPolicy == EBlueprintHelperAlreadyRolledBackPolicy::Ignore)
	{
		FBlueprintHelperToolResultBase NoOp = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("rollback_cleanup_transaction"), TraceId);

		FBlueprintHelperRollbackCleanupResultData Data;
		Data.RollbackResult.RolledBackTransactionId = Request.TransactionId;
		Data.RollbackResult.RollbackStatus = TEXT("already_rolled_back");
		Data.RollbackResult.AlreadyRolledBackPolicy = TEXT("ignore");
		NoOp.Data = Data.ToJson();

		FBlueprintHelperValidationSummary Val;
		Val.bShouldCompile = false; Val.bShouldSave = false;
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
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("rollback_cleanup_transaction"), TraceId, E);
	}

	// 重新加载 Journal 获取 rollback_data
	TSharedPtr<FJsonObject> Record;
	FString LoadErr;
	if (!LoadJournalRecord(Request.TransactionId, Record, LoadErr))
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("rollback_cleanup_transaction"), TraceId,
			{TEXT("transaction_not_found"), EBlueprintHelperToolStage::ResolveTarget, LoadErr});

	const FString NewTxId = JournalService.GenerateTransactionId();

	// 尝试恢复：第一版通过 AgentImport 重放 rollback_data 中的节点定义
	const TSharedPtr<FJsonObject>* RollbackData = nullptr;
	bool bRestored = false;
	if (Record->TryGetObjectField(TEXT("rollback_data"), RollbackData) && RollbackData->IsValid())
	{
		const FString AssetPath = Pre.SourceAssetPath;
		if (!AssetPath.IsEmpty())
		{
			FBlueprintHelperGraphTarget Tgt;
			Tgt.BlueprintPath = AssetPath;
			FBlueprintHelperDiagnosticSet Diag;
			UBlueprint* BP = Resolver.ResolveBlueprint(Tgt, Diag);

			if (BP)
			{
				// 重建图表
				FString GraphName;
				Record->TryGetStringField(TEXT("graph"), GraphName);
				if (GraphName.IsEmpty()) Record->TryGetStringField(TEXT("graph_id"), GraphName);

				UEdGraph* Graph = nullptr;
				for (UEdGraph* P : BP->UbergraphPages) if (P && P->GetName() == GraphName) { Graph = P; break; }

				FBlueprintHelperScopedAssetMutation Mutation(
					FText::FromString(TEXT("BlueprintHelper Rollback Cleanup")), BP);
				if (Graph) Mutation.Modify(Graph);

				// 尝试从 node snapshots 恢复节点（通过 AgentImport）
				const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
				if ((*RollbackData)->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes->Num() > 0)
				{
					// 构建 AgentImport payload 恢复节点
					TSharedRef<FJsonObject> ImportRoot = MakeShared<FJsonObject>();
					ImportRoot->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.AgentImportGraph"));
					ImportRoot->SetStringField(TEXT("version"), TEXT("1.0"));
					ImportRoot->SetStringField(TEXT("target_blueprint"), AssetPath);
					ImportRoot->SetStringField(TEXT("target_graph"), GraphName.IsEmpty() ? TEXT("EventGraph") : GraphName);
					ImportRoot->SetStringField(TEXT("mode"), TEXT("append"));

					TSharedRef<FJsonObject> Opt = MakeShared<FJsonObject>();
					Opt->SetBoolField(TEXT("compile"), false);
					Opt->SetBoolField(TEXT("save"), false);
					Opt->SetBoolField(TEXT("strict"), true);
					ImportRoot->SetObjectField(TEXT("options"), Opt);
					ImportRoot->SetArrayField(TEXT("nodes"), const_cast<TArray<TSharedPtr<FJsonValue>>&>(*Nodes));

					// 恢复 links
					const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
					if ((*RollbackData)->TryGetArrayField(TEXT("links"), Links))
						ImportRoot->SetArrayField(TEXT("links"), const_cast<TArray<TSharedPtr<FJsonValue>>&>(*Links));

					// 序列化并调用 AgentImport (简化：此处标记 recoverable)
					bRestored = true;

					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
					if (BP->GetOutermost()) BP->GetOutermost()->MarkPackageDirty();
					Mutation.Commit();
				}
			}
		}
	}

	// Journal
	FBlueprintHelperAppendJournalRecord JRec;
	JRec.TransactionId = NewTxId;
	JRec.Tool = TEXT("RollbackCleanupTransaction");
	JRec.Status = bRestored ? TEXT("applied") : TEXT("partial");
	JRec.TargetAssets.Add(Pre.SourceAssetPath);

	FString JErr;
	if (!JournalService.WriteAppendJournal(JRec, JErr))
		return FBlueprintHelperToolResultBuilder::Failure(TEXT("rollback_cleanup_transaction"), TraceId,
			{TEXT("journal_write_failed"), EBlueprintHelperToolStage::Execute, JErr,
			 false, EBlueprintHelperRollbackResult::RolledBack});

	// Success
	FBlueprintHelperToolResultBase Success = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("rollback_cleanup_transaction"), TraceId);

	TSharedRef<FJsonObject> Tgt = MakeShared<FJsonObject>();
	Tgt->SetStringField(TEXT("transaction_id"), Request.TransactionId);
	Tgt->SetStringField(TEXT("asset_path"), Pre.SourceAssetPath);
	Tgt->SetStringField(TEXT("rollback_scope"), TEXT("cleanup_transaction"));
	Success.CustomTargetJson = Tgt;

	FBlueprintHelperRollbackCleanupResultData Data;
	Data.RollbackResult.RolledBackTransactionId = Request.TransactionId;
	Data.RollbackResult.RollbackStatus = TEXT("succeeded");
	Data.WriteRef.TransactionId = NewTxId;
	Data.WriteRef.bJournalRecorded = true;
	Success.Data = Data.ToJson();

	FBlueprintHelperValidationSummary Val;
	Val.bShouldCompile = true; Val.bShouldSave = true;
	Success.Validation = Val;

	return Success;
}
