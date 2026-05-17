// BlueprintHelper Service Layer 。Transaction Journal 服务实现

#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/DateTime.h"

class FBlueprintHelperTransactionJournalServiceLocalUtils
{
public:
	inline static FString GBlueprintHelperRuntimeReviewArchiveSessionId;
	inline static FString GBlueprintHelperRuntimeReviewTaskRunId;

	static FString ExtractJournalTargetName(const FString& RawPath)
	{
		FString Name = RawPath;
		int32 DotIndex = INDEX_NONE;
		if (Name.FindLastChar(TEXT('.'), DotIndex))
		{
			Name = Name.Mid(DotIndex + 1);
		}
		int32 SlashIndex = INDEX_NONE;
		if (Name.FindLastChar(TEXT('/'), SlashIndex))
		{
			Name = Name.Mid(SlashIndex + 1);
		}
		return Name;
	}

	static EBlueprintHelperReviewChangeKind DeriveReviewChangeKindFromTool(const FString& Tool)
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

	static bool TryNormalizeGuidString(const FString& Value, FString& OutGuid)
	{
		OutGuid.Reset();
		FString Candidate = Value;
		Candidate.TrimStartAndEndInline();
		if (Candidate.IsEmpty())
		{
			return false;
		}

		FGuid ParsedGuid;
		if (!FGuid::Parse(Candidate, ParsedGuid))
		{
			return false;
		}

		OutGuid = ParsedGuid.ToString(EGuidFormats::Digits);
		return true;
	}

	static TSharedRef<FJsonObject> MakeVectorJson(const FVector2D& Value)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("x"), Value.X);
		Json->SetNumberField(TEXT("y"), Value.Y);
		return Json;
	}

	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Json)
	{
		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Json, Writer);
		return JsonText;
	}

	static FString MakeStructuredAnchorJson(const FBlueprintHelperGraphReviewNodeAnchor& Anchor)
	{
		TSharedRef<FJsonObject> Json = Anchor.ToJson();
		Json->SetStringField(TEXT("anchor_source"), TEXT("structured"));
		return SerializeJsonObject(Json);
	}

	static FString MakeLegacyNodeGuidAnchorJson(const FString& NodeGuid)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("anchor_source"), TEXT("legacy"));
		Json->SetStringField(TEXT("node_guid"), NodeGuid);
		return SerializeJsonObject(Json);
	}

	static FString MakeAggregateAnchorJson(
		const FVector2D& Position,
		const FVector2D& Size,
		const FString& Source)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("anchor_source"), Source);
		Json->SetObjectField(TEXT("graph_position"), MakeVectorJson(Position));
		Json->SetObjectField(TEXT("graph_size"), MakeVectorJson(Size));
		Json->SetBoolField(TEXT("has_graph_bounds"), true);
		return SerializeJsonObject(Json);
	}

	static bool TryBuildAggregateRecordedBounds(
		const TArray<FBlueprintHelperGraphReviewNodeAnchor>& Anchors,
		FVector2D& OutPosition,
		FVector2D& OutSize)
	{
		FBox2D Bounds(ForceInit);
		bool bHasBounds = false;
		for (const FBlueprintHelperGraphReviewNodeAnchor& Anchor : Anchors)
		{
			if (!Anchor.bHasGraphBounds)
			{
				continue;
			}

			Bounds += Anchor.GraphPosition;
			Bounds += Anchor.GraphPosition + Anchor.GraphSize;
			bHasBounds = true;
		}

		if (!bHasBounds)
		{
			return false;
		}

		OutPosition = Bounds.Min;
		OutSize = Bounds.Max - Bounds.Min;
		return true;
	}

	static void ApplyRecordedBounds(
		FBlueprintHelperReviewAtomicTarget& Target,
		const FVector2D& Position,
		const FVector2D& Size,
		const FString& AnchorJson)
	{
		Target.bHasGraphBounds = true;
		Target.GraphPosition = Position;
		Target.GraphSize = Size;
		Target.AnchorJson = AnchorJson;
	}

	static FBlueprintHelperReviewAtomicTarget& AddJournalAtomicTarget(
		FBlueprintHelperWriteReviewEvidence& Evidence,
		const FString& GraphName,
		const FString& TargetKey,
		const FString& TargetKind,
		const FString& VisualGroupKey,
		const FString& DisplayLabel,
		const FString& RollbackDataRef,
		const FString& NodeGuid = FString(),
		bool bHasGraphBounds = false,
		FVector2D GraphPosition = FVector2D::ZeroVector,
		FVector2D GraphSize = FVector2D(360.0f, 180.0f),
		const FString& AnchorJson = FString(),
		const FString& ExplicitBeforeSnapshotJson = FString(),
		const FString& ExplicitAfterSnapshotJson = FString())
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
		Target.NodeGuid = NodeGuid;
		Target.bHasGraphBounds = bHasGraphBounds;
		Target.GraphPosition = GraphPosition;
		Target.GraphSize = GraphSize;
		Target.AnchorJson = AnchorJson;

		if (!ExplicitBeforeSnapshotJson.IsEmpty())
		{
			Target.BeforeSnapshotJson = ExplicitBeforeSnapshotJson;
			Target.BaselineHash =
				FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(ExplicitBeforeSnapshotJson);
		}
		else if (Evidence.ChangeKind == EBlueprintHelperReviewChangeKind::Added
			&& !FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind))
		{
			FBlueprintHelperReviewBaselineSnapshotService::MakeMissingTargetSnapshot(
				Target,
				true,
				Target.BeforeSnapshotJson,
				Target.BaselineHash);
		}

		if (!ExplicitAfterSnapshotJson.IsEmpty())
		{
			Target.AfterSnapshotJson = ExplicitAfterSnapshotJson;
			Target.RecordedAfterHash =
				FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(ExplicitAfterSnapshotJson);
		}
		else
		{
			FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
			FString SnapshotJson;
			FString SnapshotHash;
			FString SnapshotError;
			if (SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError))
			{
				Target.AfterSnapshotJson = SnapshotJson;
				Target.RecordedAfterHash = SnapshotHash;
			}
		}
		Target.Ownership = TEXT("blueprinthelper_owned");
		Evidence.AtomicTargets.Add(Target);
		return Evidence.AtomicTargets.Last();
	}

	static TArray<FBlueprintHelperWriteReviewEvidence> BuildReviewEvidencesFromJournal(
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
			Evidence.CreatedAt = Record.CreatedAt;
			Evidence.AssetPath = AssetPath;
			Evidence.OperationKind = Record.Tool;
			Evidence.ChangeKind = DeriveReviewChangeKindFromTool(Record.Tool);
			Evidence.DisplayLabel = Record.Tool.IsEmpty() ? Record.TransactionId : Record.Tool;
			Evidence.AfterSummary = Record.DiffSummary;

			FVector2D AggregateGraphPosition = FVector2D::ZeroVector;
			FVector2D AggregateGraphSize = FVector2D::ZeroVector;
			const bool bHasAggregateRecordedBounds = TryBuildAggregateRecordedBounds(
				Record.CreatedNodeAnchors,
				AggregateGraphPosition,
				AggregateGraphSize);

			for (const FString& BlockId : Record.BlockIds)
			{
				const FString NormalizedBlockId =
					FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(GraphName, BlockId);
				const FString VisualGroupKey = FString::Printf(
					TEXT("graph:%s:block:%s"),
					*GraphName,
					*NormalizedBlockId);
				FBlueprintHelperReviewAtomicTarget& BlockTarget = AddJournalAtomicTarget(
					Evidence,
					GraphName,
					VisualGroupKey,
					TEXT("graph_block"),
					VisualGroupKey,
					BlockId,
					RollbackDataRef,
					FString(),
					false,
					FVector2D::ZeroVector,
					FVector2D(360.0f, 180.0f),
					FString(),
					Record.BaselineSnapshotsByTargetKey.FindRef(VisualGroupKey),
					Record.RecordedAfterSnapshotsByTargetKey.FindRef(VisualGroupKey));
				if (bHasAggregateRecordedBounds)
				{
					ApplyRecordedBounds(
						BlockTarget,
						AggregateGraphPosition,
						AggregateGraphSize,
						MakeAggregateAnchorJson(AggregateGraphPosition, AggregateGraphSize, TEXT("structured")));
				}
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

			if (Record.CreatedNodeAnchors.Num() > 0)
			{
				for (const FBlueprintHelperGraphReviewNodeAnchor& Anchor : Record.CreatedNodeAnchors)
				{
					const FString NodeId = Anchor.NodeGuid.IsEmpty()
						? ExtractJournalTargetName(Anchor.NodePath)
						: Anchor.NodeGuid;
					if (NodeId.IsEmpty())
					{
						continue;
					}

					const FString DisplayLabel = Anchor.DisplayLabel.IsEmpty()
						? ExtractJournalTargetName(Anchor.NodePath)
						: Anchor.DisplayLabel;
					AddJournalAtomicTarget(
						Evidence,
						GraphName,
						FString::Printf(TEXT("graph:%s:node:%s"), *GraphName, *NodeId),
						TEXT("graph_node"),
						DefaultVisualGroupKey,
						DisplayLabel.IsEmpty() ? NodeId : DisplayLabel,
						RollbackDataRef,
						Anchor.NodeGuid,
						Anchor.bHasGraphBounds,
						Anchor.GraphPosition,
						Anchor.GraphSize,
						MakeStructuredAnchorJson(Anchor),
						Record.BaselineSnapshotsByTargetKey.FindRef(FString::Printf(TEXT("graph:%s:node:%s"), *GraphName, *NodeId)),
						Record.RecordedAfterSnapshotsByTargetKey.FindRef(FString::Printf(TEXT("graph:%s:node:%s"), *GraphName, *NodeId)));
				}
			}
			else
			{
				for (const FString& CreatedNodePath : Record.CreatedNodePaths)
				{
					FString LegacyNodeGuid;
					const bool bGuidNodePath = TryNormalizeGuidString(CreatedNodePath, LegacyNodeGuid);
					const FString NodeName = bGuidNodePath
						? LegacyNodeGuid
						: ExtractJournalTargetName(CreatedNodePath);
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
						RollbackDataRef,
						bGuidNodePath ? LegacyNodeGuid : FString(),
						false,
						FVector2D::ZeroVector,
						FVector2D(360.0f, 180.0f),
						bGuidNodePath ? MakeLegacyNodeGuidAnchorJson(LegacyNodeGuid) : FString());
				}
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

};

void FBlueprintHelperTransactionJournalService::SetRuntimeReviewContext(
	const FString& ArchiveSessionId,
	const FString& TaskRunId)
{
	FBlueprintHelperTransactionJournalServiceLocalUtils::GBlueprintHelperRuntimeReviewArchiveSessionId = ArchiveSessionId;
	FBlueprintHelperTransactionJournalServiceLocalUtils::GBlueprintHelperRuntimeReviewTaskRunId = TaskRunId;
}

void FBlueprintHelperTransactionJournalService::ClearRuntimeReviewContext()
{
	FBlueprintHelperTransactionJournalServiceLocalUtils::GBlueprintHelperRuntimeReviewArchiveSessionId.Empty();
	FBlueprintHelperTransactionJournalServiceLocalUtils::GBlueprintHelperRuntimeReviewTaskRunId.Empty();
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
	FBlueprintHelperAppendJournalRecord RecordToWrite = Record;
	if (RecordToWrite.CreatedAt.IsEmpty())
	{
		RecordToWrite.CreatedAt = FDateTime::UtcNow().ToIso8601();
	}

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
		if (!FJsonSerializer::Serialize(RecordToWrite.ToJson(), Writer))
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
		if (!FJsonSerializer::Serialize(RecordToWrite.ToJson(), Writer))
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

	const TArray<FBlueprintHelperWriteReviewEvidence> ReviewEvidences = FBlueprintHelperTransactionJournalServiceLocalUtils::BuildReviewEvidencesFromJournal(RecordToWrite);
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
