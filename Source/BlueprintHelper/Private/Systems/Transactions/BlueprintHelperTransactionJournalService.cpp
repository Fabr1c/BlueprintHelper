// BlueprintHelper Service Layer 。Transaction Journal 服务实现

#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Systems/Review/BlueprintHelperReviewHashService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/DateTime.h"

namespace
{
	FString GBlueprintHelperRuntimeReviewArchiveSessionId;
	FString GBlueprintHelperRuntimeReviewTaskRunId;

	FString ExtractJournalTargetName(const FString& RawPath)
	{
		FString Name = RawPath;
		int32 DotIndex = INDEX_NONE;
		if (Name.FindLastChar(TEXT('.'), DotIndex))
		{
			Name = Name.Mid(DotIndex + 1);
		}
		return Name;
	}

	EBlueprintHelperReviewChangeKind DeriveReviewChangeKindFromTool(const FString& Tool)
	{
		if (Tool.Contains(TEXT("Cleanup"), ESearchCase::IgnoreCase)
			|| Tool.Contains(TEXT("Remove"), ESearchCase::IgnoreCase)
			|| Tool.Contains(TEXT("Delete"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeKind::Removed;
		}
		if (Tool.Contains(TEXT("Append"), ESearchCase::IgnoreCase)
			|| Tool.Contains(TEXT("Create"), ESearchCase::IgnoreCase)
			|| Tool.Contains(TEXT("Factory"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperReviewChangeKind::Added;
		}
		return EBlueprintHelperReviewChangeKind::Modified;
	}

	FString MakeStableReviewHash(const FString& Payload)
	{
		return FBlueprintHelperReviewHashService::MakeStableHash(Payload);
	}

	FString MakeReviewTargetHashPayload(
		const FString& Phase,
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		const FString& GraphName,
		const FString& TargetKind,
		const FString& TargetKey)
	{
		return FString::Printf(
			TEXT("%s|%s|%s|%s|%s|%s"),
			*Phase,
			*Evidence.ArchiveSessionId,
			*Evidence.AssetPath,
			*GraphName,
			*TargetKind,
			*TargetKey);
	}

	void AddJournalAtomicTarget(
		FBlueprintHelperWriteReviewEvidence& Evidence,
		const FString& GraphName,
		const FString& TargetKey,
		const FString& TargetKind,
		const FString& VisualGroupKey,
		const FString& DisplayLabel,
		const FString& RollbackDataRef)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = Evidence.AssetPath;
		Target.GraphName = GraphName;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.TargetKey = TargetKey;
		Target.TargetKind = TargetKind;
		Target.VisualGroupKey = VisualGroupKey;
		Target.DisplayLabel = DisplayLabel;
		Target.LatestTransactionId = Evidence.TransactionId;
		Target.SourceTransactionIds.Add(Evidence.TransactionId);
		Target.RollbackDataRef = RollbackDataRef;
		Target.BaselineHash = MakeStableReviewHash(MakeReviewTargetHashPayload(
			TEXT("baseline"),
			Evidence,
			GraphName,
			TargetKind,
			TargetKey));
		Target.RecordedAfterHash = MakeStableReviewHash(MakeReviewTargetHashPayload(
			Evidence.TransactionId,
			Evidence,
			GraphName,
			TargetKind,
			TargetKey));
		FString CurrentTargetHash;
		FString CurrentTargetHashError;
		if (FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(
			Target,
			CurrentTargetHash,
			CurrentTargetHashError))
		{
			Target.RecordedAfterHash = CurrentTargetHash;
		}
		Target.Ownership = TEXT("blueprinthelper_owned");
		Evidence.AtomicTargets.Add(Target);
	}

	TArray<FBlueprintHelperWriteReviewEvidence> BuildReviewEvidencesFromJournal(
		const FBlueprintHelperAppendJournalRecord& Record)
	{
		TArray<FBlueprintHelperWriteReviewEvidence> Evidences;
		if (Record.TransactionId.IsEmpty())
		{
			return Evidences;
		}

		const FString ArchiveSessionId = Record.ArchiveSessionId.IsEmpty()
			? (GBlueprintHelperRuntimeReviewArchiveSessionId.IsEmpty()
				? FString::Printf(TEXT("archive_%s"), *Record.TransactionId)
				: GBlueprintHelperRuntimeReviewArchiveSessionId)
			: Record.ArchiveSessionId;
		const FString TaskRunId = Record.TaskRunId.IsEmpty()
			? GBlueprintHelperRuntimeReviewTaskRunId
			: Record.TaskRunId;
		const FString GraphName = Record.GraphName.IsEmpty() ? Record.GraphId : Record.GraphName;
		const FString RollbackDataRef = Record.RollbackData.IsEmpty()
			? FString()
			: FString::Printf(TEXT("transaction://%s/rollback_data"), *Record.TransactionId);

		for (const FString& AssetPath : Record.TargetAssets)
		{
			FBlueprintHelperWriteReviewEvidence Evidence;
			Evidence.ArchiveSessionId = ArchiveSessionId;
			Evidence.TaskRunId = TaskRunId;
			Evidence.TransactionId = Record.TransactionId;
			Evidence.AssetPath = AssetPath;
			Evidence.OperationKind = Record.Tool;
			Evidence.ChangeKind = DeriveReviewChangeKindFromTool(Record.Tool);
			Evidence.DisplayLabel = Record.Tool.IsEmpty() ? Record.TransactionId : Record.Tool;
			Evidence.AfterSummary = Record.DiffSummary;

			for (const FString& BlockId : Record.BlockIds)
			{
				const FString NormalizedBlockId =
					FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(GraphName, BlockId);
				const FString VisualGroupKey = FString::Printf(
					TEXT("graph:%s:block:%s"),
					*GraphName,
					*NormalizedBlockId);
				AddJournalAtomicTarget(
					Evidence,
					GraphName,
					VisualGroupKey,
					TEXT("graph_block"),
					VisualGroupKey,
					BlockId,
					RollbackDataRef);
			}

			FString DefaultVisualGroupKey = FString::Printf(
				TEXT("graph:%s:transaction:%s"),
				*GraphName,
				*Record.TransactionId);
			if (Record.BlockIds.Num() > 0)
			{
				const FString FirstBlockId =
					FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(GraphName, Record.BlockIds[0]);
				DefaultVisualGroupKey = FString::Printf(
					TEXT("graph:%s:block:%s"),
					*GraphName,
					*FirstBlockId);
			}

			for (const FString& CreatedNodePath : Record.CreatedNodePaths)
			{
				const FString NodeName = ExtractJournalTargetName(CreatedNodePath);
				if (NodeName.IsEmpty())
				{
					continue;
				}
				AddJournalAtomicTarget(
					Evidence,
					GraphName,
					FString::Printf(TEXT("graph:%s:node:%s"), *GraphName, *NodeName),
					TEXT("graph_node"),
					DefaultVisualGroupKey,
					NodeName,
					RollbackDataRef);
			}

			for (const FString& CreatedLinkPath : Record.CreatedLinkPaths)
			{
				const FString LinkName = ExtractJournalTargetName(CreatedLinkPath);
				if (LinkName.IsEmpty())
				{
					continue;
				}
				AddJournalAtomicTarget(
					Evidence,
					GraphName,
					FString::Printf(TEXT("graph:%s:link:%s"), *GraphName, *LinkName),
					TEXT("graph_link"),
					DefaultVisualGroupKey,
					LinkName,
					RollbackDataRef);
			}

			Evidences.Add(Evidence);
		}

		return Evidences;
	}
}

void FBlueprintHelperTransactionJournalService::SetRuntimeReviewContext(
	const FString& ArchiveSessionId,
	const FString& TaskRunId)
{
	GBlueprintHelperRuntimeReviewArchiveSessionId = ArchiveSessionId;
	GBlueprintHelperRuntimeReviewTaskRunId = TaskRunId;
}

void FBlueprintHelperTransactionJournalService::ClearRuntimeReviewContext()
{
	GBlueprintHelperRuntimeReviewArchiveSessionId.Empty();
	GBlueprintHelperRuntimeReviewTaskRunId.Empty();
}

FString FBlueprintHelperTransactionJournalService::GenerateTransactionId() const
{
	const FDateTime Now = FDateTime::UtcNow();
	return FString::Printf(TEXT("tx_%lld%03d"),
		static_cast<int64>(Now.ToUnixTimestamp()),
		Now.GetMillisecond());
}

FString FBlueprintHelperTransactionJournalService::GetJournalRootPath() const
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Transactions") / TEXT("Active");
}

FString FBlueprintHelperTransactionJournalService::GetReviewRootPath() const
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Review");
}

bool FBlueprintHelperTransactionJournalService::WriteAppendJournal(
	const FBlueprintHelperAppendJournalRecord& Record,
	FString& OutError) const
{
	const FString JournalDir = GetJournalRootPath();
	if (!IFileManager::Get().DirectoryExists(*JournalDir))
	{
		IFileManager::Get().MakeDirectory(*JournalDir, true);
	}

	const FString ReviewDir = GetReviewRootPath();
	if (!IFileManager::Get().DirectoryExists(*ReviewDir))
	{
		IFileManager::Get().MakeDirectory(*ReviewDir, true);
	}

	// 写入 Active Journal
	const FString JournalPath = JournalDir / FString::Printf(TEXT("%s.json"), *Record.TransactionId);
	{
		FString JournalJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JournalJson);
		if (!FJsonSerializer::Serialize(Record.ToJson(), Writer))
		{
			OutError = TEXT("Journal 记录 JSON 序列化失败。");
			return false;
		}

		if (!FFileHelper::SaveStringToFile(JournalJson, *JournalPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("无法写入 Journal 文件：%s"), *JournalPath);
			return false;
		}
	}

	// 写入 Review Store 副本
	const FString ReviewPath = ReviewDir / FString::Printf(TEXT("%s.json"), *Record.TransactionId);
	{
		FString ReviewJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReviewJson);
		if (!FJsonSerializer::Serialize(Record.ToJson(), Writer))
		{
			OutError = TEXT("Review 记录 JSON 序列化失败。");
			return false;
		}

		if (!FFileHelper::SaveStringToFile(ReviewJson, *ReviewPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("无法写入 Review 文件：%s"), *ReviewPath);
			return false;
		}
	}

	const TArray<FBlueprintHelperWriteReviewEvidence> ReviewEvidences = BuildReviewEvidencesFromJournal(Record);
	if (ReviewEvidences.Num() > 0)
	{
		FBlueprintHelperReviewStoreService ReviewStore;
		const TArray<FBlueprintHelperReviewRecord> ReviewRecords =
			ReviewStore.BuildReviewRecordsFromEvidence(ReviewEvidences);
		FString ReviewRecordError;
		if (!ReviewStore.SaveReviewRecords(ReviewRecords, ReviewRecordError))
		{
			OutError = ReviewRecordError;
			return false;
		}
	}

	return true;
}
