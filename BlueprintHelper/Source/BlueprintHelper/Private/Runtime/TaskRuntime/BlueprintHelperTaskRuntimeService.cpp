// BlueprintHelper Service Layer - TaskPlan runtime executor

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Shared/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetTreePositionPolicy.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetTreeProjectionService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/SharedServices/Utils/BlueprintHelperBlueprintStructureUtils.h"
#include "Shared/Debug/BlueprintHelperSaveAssetTypes.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/AssetFactory/BlueprintHelperAssetFactoryTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintClassSettings/BlueprintHelperClassSettingsTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/DataAssetObjectProperty/BlueprintHelperObjectPropertyTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/DataTable/BlueprintHelperDataTableTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/MaterialInstance/BlueprintHelperMaterialInstanceTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteRuntimeDispatcher.h"
#include "Runtime/TaskRuntime/BlueprintHelperGraphWritePlanCache.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCallFunctionResolutionCache.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheDiagnostics.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheKeyUtils.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeContextRevisionManifest.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeDryRunPolicy.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyClassifier.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyDebugEvidenceProjection.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyEvidenceProvider.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeSettingsResolver.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRunJournalStoreService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskPartialPreviewCache.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePrepareService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeReviewIoBatch.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskPreviewStore.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipelineExecutors.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationTypes.h"
#include "Runtime/TaskRuntime/Projection/BlueprintHelperTaskRuntimeResultProjection.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeTimingUtils.h"
#include "Generated/BlueprintHelperUMGWidgetOperationManifest.generated.h"
#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteAutoSearchPolicyResolver.h"
#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteCandidateArtifactStore.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"

#include "Components/NamedSlotInterface.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperTaskRuntimeServiceLocalUtils
{
public:
	using FPlannedMemberVariableByName = TMap<FString, TSharedPtr<FJsonObject>>;
	using FPlannedMemberVariablesByAsset = TMap<FString, FPlannedMemberVariableByName>;

	static FBlueprintHelperToolError MakeTaskRuntimeError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	static FBlueprintHelperToolResultBase MakeFailure(
		const FString& Operation,
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			Operation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeTaskRuntimeError(Code, Stage, Message, Field));
	}

	static FBlueprintHelperToolResultBase MakeComponentPolicyParseFailure(
		const TCHAR* FieldName,
		const FString& Value)
	{
		return MakeFailure(
			TEXT("blueprint_component"),
			TEXT("unsupported_blueprint_component_policy"),
			EBlueprintHelperToolStage::ParseInput,
			FString::Printf(TEXT("Unsupported blueprint component %s value: %s."), FieldName, *Value),
			TEXT("payload.") + FString(FieldName));
	}

	struct FScopedBlueprintHelperReviewContext
	{
		FScopedBlueprintHelperReviewContext(
			bool bInActive,
			const FString& ArchiveSessionId,
			const FString& TaskRunId)
			: bActive(bInActive)
		{
			(void)ArchiveSessionId;
			(void)TaskRunId;
		}

		~FScopedBlueprintHelperReviewContext()
		{
			if (bActive)
			{
			}
		}

		bool bActive = false;
	};

	struct FScopedBlueprintHelperGraphLayoutTask
	{
		explicit FScopedBlueprintHelperGraphLayoutTask(bool bInActive)
			: bActive(bInActive)
		{
		}

		~FScopedBlueprintHelperGraphLayoutTask()
		{
			if (bActive && !bCompleted)
			{
				FBlueprintHelperGraphLayoutCoordinator::DiscardPendingTaskLayouts();
			}
		}

		bool FlushAndComplete()
		{
			if (!bActive || bCompleted)
			{
				return true;
			}

			const bool bFlushSucceeded = FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts();
			bCompleted = bFlushSucceeded;
			return bFlushSucceeded;
		}

		bool bActive = false;
		bool bCompleted = false;
	};

	enum class EBlueprintHelperReviewBaselineDirtyAssetPolicy : uint8
	{
		Block,
		SaveBeforeArchive,
		AllowStaleDiskSnapshot
	};

	struct FBlueprintHelperReviewBaselinePolicyEvaluation
	{
		EBlueprintHelperReviewBaselineDirtyAssetPolicy Policy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block;
		FString PolicyString = TEXT("block");
		FString SnapshotTrust = TEXT("fresh_disk_copy");
		TArray<FString> DirtyTargetAssets;
		TArray<FString> SavedBeforeArchiveAssets;
		TArray<FString> Warnings;
		TArray<FBlueprintHelperTaskRuntimePostOperationRecord> PreArchiveSaveOperations;
		FBlueprintHelperReviewBaselineDirtyDecision DirtyDecision;
	};

	struct FBlueprintHelperReviewTargetSnapshotCacheValue
	{
		FString SnapshotJson;
		FString SnapshotHash;
	};

	static FString MakeTaskRuntimeReviewRefSegment(const FString& RawValue)
	{
		FString Segment;
		Segment.Reserve(RawValue.Len());
		for (const TCHAR Ch : RawValue)
		{
			Segment.AppendChar(FChar::IsAlnum(Ch) || Ch == TCHAR('_') ? Ch : TCHAR('_'));
		}
		while (Segment.Contains(TEXT("__")))
		{
			Segment.ReplaceInline(TEXT("__"), TEXT("_"));
		}
		return Segment.IsEmpty() ? FString(TEXT("target")) : Segment;
	}

	static TArray<FString> CaptureReviewBaselineSnapshots(
		const FString& ArchiveSessionId,
		const TArray<FString>& AssetPaths)
	{
		TArray<FString> SnapshotRefs;
		if (ArchiveSessionId.IsEmpty())
		{
			return SnapshotRefs;
		}

		const FString SnapshotDir = FBlueprintHelperReviewConfigResolver::Load().Artifact.SnapshotRoot
			/ ArchiveSessionId;
		IFileManager::Get().MakeDirectory(*SnapshotDir, true);

		for (const FString& AssetPath : AssetPaths)
		{
			if (AssetPath.IsEmpty())
			{
				continue;
			}

			const FString PackageFilePath = FPackageName::LongPackageNameToFilename(
				AssetPath,
				FPackageName::GetAssetPackageExtension());
			if (!IFileManager::Get().FileExists(*PackageFilePath))
			{
				continue;
			}

			const FString SnapshotFileName = MakeTaskRuntimeReviewRefSegment(AssetPath) + TEXT(".uasset");
			const FString SnapshotPath = SnapshotDir / SnapshotFileName;
			if (IFileManager::Get().Copy(*SnapshotPath, *PackageFilePath, true, true) == COPY_OK)
			{
				SnapshotRefs.Add(FString::Printf(
					TEXT("review://archive/%s/baseline/%s"),
					*ArchiveSessionId,
					*SnapshotFileName));
			}
		}

		return SnapshotRefs;
	}

	static void PopulateTaskRuntimeReviewTargetSnapshots(
		FBlueprintHelperWriteReviewEvidence& Evidence,
		bool bBeforeSnapshot,
		TMap<FString, FBlueprintHelperReviewTargetSnapshotCacheValue>* SnapshotCache = nullptr)
	{
		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
		for (FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			const FString CacheKey = FString::Printf(
				TEXT("%s|%s|%s|%s"),
				*Target.AssetPath,
				*Target.GraphName,
				*Target.TargetKind,
				*Target.TargetKey);
			if (SnapshotCache)
			{
				if (const FBlueprintHelperReviewTargetSnapshotCacheValue* CachedSnapshot = SnapshotCache->Find(CacheKey))
				{
					if (bBeforeSnapshot)
					{
						Target.BeforeSnapshotJson = CachedSnapshot->SnapshotJson;
						Target.BaselineHash = CachedSnapshot->SnapshotHash;
					}
					else
					{
						Target.AfterSnapshotJson = CachedSnapshot->SnapshotJson;
						Target.RecordedAfterHash = CachedSnapshot->SnapshotHash;
					}
					continue;
				}
			}

			FString SnapshotJson;
			FString SnapshotHash;
			FString SnapshotError;
			if (!SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError))
			{
				continue;
			}

			if (bBeforeSnapshot)
			{
				Target.BeforeSnapshotJson = SnapshotJson;
				Target.BaselineHash = SnapshotHash;
			}
			else
			{
				Target.AfterSnapshotJson = SnapshotJson;
				Target.RecordedAfterHash = SnapshotHash;
			}

			if (SnapshotCache)
			{
				FBlueprintHelperReviewTargetSnapshotCacheValue CacheValue;
				CacheValue.SnapshotJson = SnapshotJson;
				CacheValue.SnapshotHash = SnapshotHash;
				SnapshotCache->Add(CacheKey, CacheValue);
			}
		}
	}

	static TArray<TSharedPtr<FJsonValue>> CopyArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(FieldName, Array) && Array)
		{
			return *Array;
		}
		return {};
	}

	static void CopyObjectFields(
		const TSharedPtr<FJsonObject>& Source,
		const TSharedRef<FJsonObject>& Destination)
	{
		if (!Source.IsValid())
		{
			return;
		}

		for (const auto& Field : Source->Values)
		{
			Destination->SetField(FBlueprintHelperVersionCompat::JsonKeyToString(Field.Key), Field.Value);
		}
	}

	static void RemoveJsonFieldRecursive(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
	{
		if (!Object.IsValid())
		{
			return;
		}

		Object->RemoveField(FieldName);
		for (const auto& Field : Object->Values)
		{
			if (!Field.Value.IsValid())
			{
				continue;
			}

			if (Field.Value->Type == EJson::Object)
			{
				RemoveJsonFieldRecursive(Field.Value->AsObject(), FieldName);
			}
			else if (Field.Value->Type == EJson::Array)
			{
				for (const TSharedPtr<FJsonValue>& ArrayValue : Field.Value->AsArray())
				{
					if (ArrayValue.IsValid() && ArrayValue->Type == EJson::Object)
					{
						RemoveJsonFieldRecursive(ArrayValue->AsObject(), FieldName);
					}
				}
			}
		}
	}

	static void ApplyCachedTaskRuntimeReviewTargetSnapshots(
		FBlueprintHelperWriteReviewEvidence& Evidence,
		const TMap<FString, FBlueprintHelperReviewTargetSnapshotCacheValue>& SnapshotCache)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			const FString CacheKey = FString::Printf(
				TEXT("%s|%s|%s|%s"),
				*Target.AssetPath,
				*Target.GraphName,
				*Target.TargetKind,
				*Target.TargetKey);
			const FBlueprintHelperReviewTargetSnapshotCacheValue* CachedSnapshot = SnapshotCache.Find(CacheKey);
			if (!CachedSnapshot)
			{
				continue;
			}

			Target.BeforeSnapshotJson = CachedSnapshot->SnapshotJson;
			Target.BaselineHash = CachedSnapshot->SnapshotHash;
		}
	}

	static bool IsUmgWidgetReviewPreStepCandidate(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		return LoweredStep.Capability.Equals(TEXT("umg_widget"), ESearchCase::IgnoreCase)
			|| LoweredStep.RuntimeOperation.Equals(TEXT("umg_widget"), ESearchCase::IgnoreCase)
			|| LoweredStep.AdapterOperation.Equals(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetSlotProperty, ESearchCase::IgnoreCase)
			|| LoweredStep.AdapterOperation.Equals(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetAsVariable, ESearchCase::IgnoreCase)
			|| LoweredStep.AdapterOperation.Equals(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent, ESearchCase::IgnoreCase);
	}

	static FBlueprintHelperToolResultBase MakeTaskRuntimePreStepReviewResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		const FString StepOperation = LoweredStep.AdapterOperation.IsEmpty()
			? LoweredStep.RuntimeOperation
			: LoweredStep.AdapterOperation;
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
			StepOperation.IsEmpty() ? TEXT("task_runtime_pre_step_review") : StepOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		Result.bModified = false;

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("TaskRuntimePreStepReview.v1"));
		TSharedRef<FJsonObject> ReadbackContext = MakeShared<FJsonObject>();
		if (LoweredStep.AdapterOperation.Equals(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetSlotProperty, ESearchCase::IgnoreCase))
		{
			ReadbackContext->SetStringField(TEXT("target_kind"), TEXT("slot_property"));
		}
		else if (LoweredStep.AdapterOperation.Equals(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetAsVariable, ESearchCase::IgnoreCase))
		{
			ReadbackContext->SetStringField(TEXT("target_kind"), TEXT("widget_variable"));
		}
		else if (LoweredStep.AdapterOperation.Equals(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent, ESearchCase::IgnoreCase))
		{
			ReadbackContext->SetStringField(TEXT("target_kind"), TEXT("named_slot_content"));
		}
		if (LoweredStep.Payload.IsValid())
		{
			FString WidgetName;
			if (LoweredStep.Payload->TryGetStringField(TEXT("widget_name"), WidgetName) && !WidgetName.IsEmpty())
			{
				ReadbackContext->SetStringField(TEXT("widget_name"), WidgetName);
			}
			FString PropertyPath;
			if (LoweredStep.Payload->TryGetStringField(TEXT("property_path"), PropertyPath) && !PropertyPath.IsEmpty())
			{
				ReadbackContext->SetStringField(TEXT("property_path"), PropertyPath);
			}
			FString SlotName;
			if (LoweredStep.Payload->TryGetStringField(TEXT("slot_name"), SlotName) && !SlotName.IsEmpty())
			{
				ReadbackContext->SetStringField(TEXT("slot_name"), SlotName);
			}
		}
		Data->SetObjectField(TEXT("readback_context"), ReadbackContext);
		Result.Data = Data;
		return Result;
	}

	static bool IsGraphWriteTaskPlanOperation(const FString& Operation)
	{
		return Operation == TEXT("append_blueprint_graph") ||
			Operation == TEXT("replace_blueprint_graph") ||
			Operation == TEXT("patch_blueprint_graph") ||
			Operation == TEXT("merge_blueprint_graph");
	}

	static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("");
		}

		switch (Value->Type)
		{
		case EJson::String:
			return Value->AsString();
		case EJson::Number:
			{
				const double NumberValue = Value->AsNumber();
				const double RoundedValue = FMath::RoundToDouble(NumberValue);
				if (FMath::IsNearlyEqual(NumberValue, RoundedValue))
				{
					return FString::Printf(TEXT("%.0f"), RoundedValue);
				}
				return FString::SanitizeFloat(NumberValue);
			}
		case EJson::Boolean:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		case EJson::Null:
			return TEXT("");
		default:
			break;
		}

		FString Serialized;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
		return Serialized;
	}

	static TSharedPtr<FJsonValue> GetLiteralJsonValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return MakeShared<FJsonValueNull>();
		}

		if (Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> ObjectValue = AsJsonObjectIfObject(Value);
			FString Kind;
			if (ObjectValue.IsValid() &&
				ObjectValue->TryGetStringField(TEXT("kind"), Kind) &&
				Kind == TEXT("literal"))
			{
				const TSharedPtr<FJsonValue> LiteralValue = ObjectValue->TryGetField(TEXT("value"));
				return LiteralValue.IsValid() ? LiteralValue : MakeShared<FJsonValueNull>();
			}
		}

		return Value;
	}

	static TSharedPtr<FJsonObject> AsJsonObjectIfObject(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			return nullptr;
		}
		return Value->AsObject();
	}

	static void CopyLiteralArgsToInputs(
		const TSharedPtr<FJsonObject>& ArgsObject,
		const TSharedRef<FJsonObject>& InputsObject)
	{
		if (!ArgsObject.IsValid())
		{
			return;
		}

		for (const auto& Arg : ArgsObject->Values)
		{
			InputsObject->SetField(FBlueprintHelperVersionCompat::JsonKeyToString(Arg.Key), GetLiteralJsonValue(Arg.Value));
		}
	}

	static TSharedRef<FJsonObject> MakeExecLink(
		const FString& FromNodeId,
		const FString& ToNodeId)
	{
		TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
		Link->SetStringField(TEXT("kind"), TEXT("exec"));
		Link->SetStringField(TEXT("from"), FString::Printf(TEXT("%s.then"), *FromNodeId));
		Link->SetStringField(TEXT("to"), FString::Printf(TEXT("%s.execute"), *ToNodeId));
		return Link;
	}

	static TSharedRef<FJsonObject> MakeSyntheticDryRunData(
		const FString& Strategy = TEXT("full"),
		const FString& ValidatedScope = TEXT("taskplan_lowering_only"))
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("strategy"), Strategy);
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("synthetic"));
		DryRun->SetStringField(TEXT("validated_scope"), ValidatedScope);
		DryRun->SetStringField(TEXT("limitation"), TEXT("Adapter service dry-run is not implemented; target asset state and service preflight were not validated."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueString>(
			TEXT("Synthetic preview only validates TaskPlan lowering. Execute may still fail in the underlying service preflight.")));
		DryRun->SetArrayField(TEXT("warnings"), MoveTemp(Warnings));
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		return Data;
	}

	static void AttachCallFunctionResolutionCacheStats(
		TSharedPtr<FJsonObject> Data,
		const FBlueprintHelperTaskRuntimeCallFunctionResolutionCache& ResolutionCache)
	{
		if (!Data.IsValid())
		{
			return;
		}

		const FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats Stats = ResolutionCache.GetStats();
		TSharedRef<FJsonObject> StatsJson = MakeShared<FJsonObject>();
		StatsJson->SetNumberField(TEXT("hits"), Stats.Hits);
		StatsJson->SetNumberField(TEXT("misses"), Stats.Misses);
		StatsJson->SetNumberField(TEXT("entries"), Stats.Entries);
		Data->SetObjectField(TEXT("call_function_resolution_cache"), StatsJson);
	}

	static void AttachGraphWriteExecutionStatsFromSteps(
		TSharedPtr<FJsonObject> Data,
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords)
	{
		if (!Data.IsValid())
		{
			return;
		}

		TArray<TSharedPtr<FJsonValue>> StepStatsValues;
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			const TSharedPtr<FJsonObject>* StepStats = nullptr;
			if (!StepRecord.Result.Data.IsValid() ||
				!StepRecord.Result.Data->TryGetObjectField(TEXT("graph_write_execution_stats"), StepStats) ||
				!StepStats || !StepStats->IsValid())
			{
				continue;
			}

			TSharedRef<FJsonObject> StepStatsObject = MakeShared<FJsonObject>();
			CopyObjectFields(*StepStats, StepStatsObject);
			StepStatsObject->SetStringField(TEXT("step_id"), StepRecord.Step.StepId);
			if (!StepRecord.Step.AdapterOperation.IsEmpty())
			{
				StepStatsObject->SetStringField(TEXT("adapter_operation"), StepRecord.Step.AdapterOperation);
			}
			StepStatsValues.Add(MakeShared<FJsonValueObject>(StepStatsObject));
		}

		if (StepStatsValues.Num() == 0)
		{
			return;
		}

		TSharedRef<FJsonObject> AggregatedStats = MakeShared<FJsonObject>();
		AggregatedStats->SetArrayField(TEXT("steps"), MoveTemp(StepStatsValues));
		Data->SetObjectField(TEXT("graph_write_execution_stats"), AggregatedStats);
	}

	static void AttachDryRunStrategy(
		TSharedPtr<FJsonObject> Data,
		const FBlueprintHelperTaskRuntimeDryRunPolicy& DryRunPolicy)
	{
		if (!Data.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonObject>* DryRunObjectPtr = nullptr;
		if (Data->TryGetObjectField(TEXT("dry_run"), DryRunObjectPtr) &&
			DryRunObjectPtr && DryRunObjectPtr->IsValid())
		{
			(*DryRunObjectPtr)->SetStringField(TEXT("strategy"), DryRunPolicy.ToDiagnosticString());
			return;
		}

		TSharedRef<FJsonObject> DryRunObject = MakeShared<FJsonObject>();
		DryRunObject->SetStringField(TEXT("strategy"), DryRunPolicy.ToDiagnosticString());
		Data->SetObjectField(TEXT("dry_run"), DryRunObject);
	}

	static void AttachDryRunOutcomeFields(
		TSharedPtr<FJsonObject> Data,
		bool bPassed)
	{
		if (!Data.IsValid())
		{
			return;
		}

		Data->SetBoolField(TEXT("passed"), bPassed);
		Data->SetBoolField(TEXT("blocked"), !bPassed);

		TSharedPtr<FJsonObject> DryRunObject;
		const TSharedPtr<FJsonObject>* DryRunObjectPtr = nullptr;
		if (Data->TryGetObjectField(TEXT("dry_run"), DryRunObjectPtr) &&
			DryRunObjectPtr && DryRunObjectPtr->IsValid())
		{
			DryRunObject = *DryRunObjectPtr;
		}
		else
		{
			DryRunObject = MakeShared<FJsonObject>();
			Data->SetObjectField(TEXT("dry_run"), DryRunObject.ToSharedRef());
		}

		if (!bPassed)
		{
			DryRunObject->SetBoolField(TEXT("can_execute"), false);
			DryRunObject->SetStringField(TEXT("result"), TEXT("blocked"));
			return;
		}

		if (!DryRunObject->HasField(TEXT("can_execute")))
		{
			DryRunObject->SetBoolField(TEXT("can_execute"), true);
		}
		if (!DryRunObject->HasField(TEXT("result")))
		{
			DryRunObject->SetStringField(TEXT("result"), TEXT("passed"));
		}
	}

	static bool IsPreviewResultExecutable(const FBlueprintHelperToolResultBase& Result)
	{
		if (!Result.bOk)
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* DryRunObjectPtr = nullptr;
		if (Result.Data.IsValid() &&
			Result.Data->TryGetObjectField(TEXT("dry_run"), DryRunObjectPtr) &&
			DryRunObjectPtr && DryRunObjectPtr->IsValid())
		{
			bool bCanExecute = true;
			if ((*DryRunObjectPtr)->TryGetBoolField(TEXT("can_execute"), bCanExecute))
			{
				return bCanExecute;
			}

			FString DryRunResult;
			if ((*DryRunObjectPtr)->TryGetStringField(TEXT("result"), DryRunResult) &&
				DryRunResult == TEXT("blocked"))
			{
				return false;
			}
		}

		return true;
	}

	static FString NormalizePackageNameFromAssetPath(const FString& AssetPath)
	{
		FString PackageName = AssetPath;
		int32 DotIndex = INDEX_NONE;
		if (PackageName.FindChar(TEXT('.'), DotIndex))
		{
			PackageName.LeftInline(DotIndex);
		}
		return PackageName;
	}

	static FString BuildTargetAssetStateHash(const TSharedPtr<FJsonObject>& TaskPlan)
	{
		TArray<FString> TargetAssets = ReadTargetAssets(TaskPlan);
		TargetAssets.Sort();

		TArray<FString> Parts;
		for (const FString& TargetAsset : TargetAssets)
		{
			const FString PackageName = NormalizePackageNameFromAssetPath(TargetAsset);
			const FString PackageFilename =
				FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			const bool bFileExists = IFileManager::Get().FileExists(*PackageFilename);
			const FDateTime FileTimestamp = bFileExists
				? IFileManager::Get().GetTimeStamp(*PackageFilename)
				: FDateTime();
			UPackage* Package = FindPackage(nullptr, *PackageName);
			const bool bLoaded = Package != nullptr;
			const bool bDirty = Package ? Package->IsDirty() : false;

			Parts.Add(FString::Printf(
				TEXT("%s|package=%s|file=%d|timestamp=%s|loaded=%d|dirty=%d"),
				*TargetAsset,
				*PackageName,
				bFileExists ? 1 : 0,
				*FileTimestamp.ToIso8601(),
				bLoaded ? 1 : 0,
				bDirty ? 1 : 0));
		}

		return FString::Join(Parts, TEXT("\n"));
	}

	struct FResolvedCallFunctionRuntimeFact
	{
		FString StepId;
		FString StatementPath;
		FString Query;
		FString StableId;
		FString NativeName;
		FString DisplayName;
		FString OwnerClassPath;
	};

	struct FCallFunctionStatementRef
	{
		TSharedPtr<FJsonObject> StatementObject;
		FString StatementPath;
		FString SemanticPath;
		FString NamePath;
		FString Query;
		bool bExpression = false;
	};

	struct FCallFunctionLogicSpecRef
	{
		TSharedPtr<FJsonObject> LogicSpec;
		FString LogicSpecPath;
	};

	static UBlueprint* ResolveTaskRuntimeBlueprint(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		if (UBlueprint* Existing = FindObject<UBlueprint>(nullptr, *AssetPath))
		{
			return Existing;
		}
		return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
	}

	static UEdGraph* ResolveTaskRuntimeGraph(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		auto FindInGraphs = [&GraphName](const TArray<TObjectPtr<UEdGraph>>& Graphs) -> UEdGraph*
		{
			for (UEdGraph* Graph : Graphs)
			{
				if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
				{
					return Graph;
				}
			}
			return nullptr;
		};

		if (UEdGraph* Graph = FindInGraphs(Blueprint->UbergraphPages))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindInGraphs(Blueprint->FunctionGraphs))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindInGraphs(Blueprint->MacroGraphs))
		{
			return Graph;
		}
		return Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	}

	static FString GetSemanticStatementId(const FBlueprintHelperGraphStatementIR& Statement)
	{
		return FBlueprintHelperGraphStatementTypeUtils::MakeStatementFragmentId(Statement);
	}

	static FString GetSemanticExpressionId(const FBlueprintHelperGraphExpressionIR& Expression)
	{
		return FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	}

	static void AddUniqueString(TArray<FString>& Values, const FString& Value)
	{
		if (!Value.IsEmpty() && !Values.Contains(Value))
		{
			Values.Add(Value);
		}
	}

	static FBlueprintHelperCallFunctionPinType MakeCallFunctionPinTypeFromDagRef(
		const FBlueprintHelperGraphFragmentPinTypeRef& PinType)
	{
		FBlueprintHelperCallFunctionPinType Result;
		Result.Category = PinType.Category;
		Result.SubCategory = PinType.SubCategory;
		Result.ObjectPath = PinType.ObjectPath;
		Result.ContainerType = PinType.ContainerType;
		Result.bIsReference = PinType.bIsReference;
		Result.bIsConst = PinType.bIsConst;
		return Result;
	}

	static void NormalizeObjectPathPinType(FBlueprintHelperCallFunctionPinType& InOutPinType)
	{
		const FString Category = InOutPinType.Category.TrimStartAndEnd();
		if (InOutPinType.ObjectPath.IsEmpty()
			&& (Category.StartsWith(TEXT("/")) || Category.StartsWith(TEXT("Class'"))))
		{
			InOutPinType.Category = TEXT("object");
			InOutPinType.ObjectPath = Category;
		}
	}

	static FBlueprintHelperCallFunctionPinType MakeTargetObjectPinTypeFromExpression(
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression)
	{
		FBlueprintHelperCallFunctionPinType Result;
		if (!Expression.IsValid())
		{
			return Result;
		}

		if (!Expression->PinType.TrimStartAndEnd().IsEmpty())
		{
			Result = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Expression->PinType);
			NormalizeObjectPathPinType(Result);
			if (Result.IsValid())
			{
				return Result;
			}
		}

		const FString Type = !Expression->Type.TrimStartAndEnd().IsEmpty()
			? Expression->Type.TrimStartAndEnd()
			: Expression->ResolvedTarget.Type.TrimStartAndEnd();
		if (!Type.IsEmpty())
		{
			Result.Category = Type;
			NormalizeObjectPathPinType(Result);
		}
		return Result;
	}

	static bool TryCollectSemanticTargetObjectPinTypeFromDag(
		const FBlueprintHelperGraphFragmentDag& FragmentDag,
		const FString& ConsumerFragmentId,
		FBlueprintHelperCallFunctionPinType& OutPinType)
	{
		for (const FBlueprintHelperGraphFragmentDataEdge& DataEdge : FragmentDag.DataEdges)
		{
			if (!DataEdge.To.FragmentId.Equals(ConsumerFragmentId, ESearchCase::CaseSensitive))
			{
				continue;
			}

			const FString PortName = !DataEdge.To.PinName.IsEmpty() ? DataEdge.To.PinName : DataEdge.To.PortId;
			if (!PortName.Equals(TEXT("target_object"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			FBlueprintHelperCallFunctionPinType PinType = MakeCallFunctionPinTypeFromDagRef(DataEdge.From.PinType);
			NormalizeObjectPathPinType(PinType);
			if (PinType.IsValid())
			{
				OutPinType = PinType;
				return true;
			}
		}
		return false;
	}

	static TMap<FString, FBlueprintHelperCallFunctionPinType> CollectSemanticArgumentPinTypesFromDag(
		const FBlueprintHelperGraphFragmentDag& FragmentDag,
		const FString& ConsumerFragmentId)
	{
		TMap<FString, FBlueprintHelperCallFunctionPinType> Result;
		for (const FBlueprintHelperGraphFragmentDataEdge& DataEdge : FragmentDag.DataEdges)
		{
			if (!DataEdge.To.FragmentId.Equals(ConsumerFragmentId, ESearchCase::CaseSensitive))
			{
				continue;
			}

			const FString ArgumentName = !DataEdge.To.PinName.IsEmpty() ? DataEdge.To.PinName : DataEdge.To.PortId;
			if (ArgumentName.IsEmpty())
			{
				continue;
			}
			if (ArgumentName.Equals(TEXT("target_object"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FBlueprintHelperCallFunctionPinType PinType = MakeCallFunctionPinTypeFromDagRef(DataEdge.From.PinType);
			if (PinType.IsValid())
			{
				Result.Add(ArgumentName, PinType);
			}
		}
		return Result;
	}

	static void IndexSemanticStatements(
		const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
		TMap<FString, TSharedPtr<FBlueprintHelperGraphStatementIR>>& OutStatementsByPath)
	{
		for (const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement : Statements)
		{
			if (!Statement.IsValid())
			{
				continue;
			}

			OutStatementsByPath.Add(Statement->Path, Statement);
			IndexSemanticStatements(Statement->ThenStatements, OutStatementsByPath);
			IndexSemanticStatements(Statement->ElseStatements, OutStatementsByPath);
		}
	}

	static void IndexSemanticExpression(
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
		TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& OutExpressionsByPath)
	{
		if (!Expression.IsValid())
		{
			return;
		}

		OutExpressionsByPath.Add(Expression->Path, Expression);
		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression->Args)
		{
			IndexSemanticExpression(ArgPair.Value, OutExpressionsByPath);
		}
		for (const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option : Expression->Options)
		{
			IndexSemanticExpression(Option, OutExpressionsByPath);
		}
		IndexSemanticExpression(Expression->TargetObject, OutExpressionsByPath);
		IndexSemanticExpression(Expression->Left, OutExpressionsByPath);
		IndexSemanticExpression(Expression->Right, OutExpressionsByPath);
	}

	static void IndexSemanticExpressions(
		const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
		TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& OutExpressionsByPath)
	{
		for (const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement : Statements)
		{
			if (!Statement.IsValid())
			{
				continue;
			}

			for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Statement->Args)
			{
				IndexSemanticExpression(ArgPair.Value, OutExpressionsByPath);
			}
			IndexSemanticExpression(Statement->Value, OutExpressionsByPath);
			IndexSemanticExpression(Statement->Condition, OutExpressionsByPath);
			IndexSemanticExpression(Statement->TargetObject, OutExpressionsByPath);
			IndexSemanticExpressions(Statement->ThenStatements, OutExpressionsByPath);
			IndexSemanticExpressions(Statement->ElseStatements, OutExpressionsByPath);
		}
	}

	static void ApplySemanticStatementContext(
		const FCallFunctionStatementRef& CallStatement,
		const FBlueprintHelperGraphSemanticIR& SemanticIR,
		const FBlueprintHelperGraphFragmentDag& FragmentDag,
		FBlueprintHelperCallFunctionResolveRequest& InOutRequest)
	{
		TMap<FString, TSharedPtr<FBlueprintHelperGraphStatementIR>> StatementsByPath;
		IndexSemanticStatements(SemanticIR.Statements, StatementsByPath);
		const TSharedPtr<FBlueprintHelperGraphStatementIR>* StatementPtr = StatementsByPath.Find(CallStatement.SemanticPath);
		if (!StatementPtr || !StatementPtr->IsValid())
		{
			return;
		}

		const FBlueprintHelperGraphStatementIR& Statement = *(*StatementPtr);
		if (Statement.Kind != EBlueprintHelperGraphStatementKind::Call)
		{
			return;
		}

		if (Statement.ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::ComponentMemberFunction)
		{
			InOutRequest.TargetObjectType = Statement.ResolvedTarget.Type;
		}
		if (Statement.TargetObject.IsValid())
		{
			if (!Statement.TargetObject->Type.IsEmpty())
			{
				InOutRequest.TargetObjectType = Statement.TargetObject->Type;
			}
			const FBlueprintHelperCallFunctionPinType TargetObjectPinType =
				MakeTargetObjectPinTypeFromExpression(Statement.TargetObject);
			if (TargetObjectPinType.IsValid())
			{
				InOutRequest.TargetObjectPinType = TargetObjectPinType;
			}
		}

		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Statement.Args)
		{
			AddUniqueString(InOutRequest.ArgumentNames, ArgPair.Key);
			if (ArgPair.Value.IsValid() && !ArgPair.Value->Type.IsEmpty())
			{
				InOutRequest.ArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type);
			}
		}

		const FString StatementId = GetSemanticStatementId(Statement);
		InOutRequest.ArgumentPinTypes.Append(CollectSemanticArgumentPinTypesFromDag(FragmentDag, StatementId));
		if (!InOutRequest.TargetObjectPinType.IsValid())
		{
			FBlueprintHelperCallFunctionPinType TargetObjectPinType;
			if (TryCollectSemanticTargetObjectPinTypeFromDag(FragmentDag, StatementId, TargetObjectPinType))
			{
				InOutRequest.TargetObjectPinType = TargetObjectPinType;
			}
		}
	}

	static void ApplySemanticExpressionContext(
		const FCallFunctionStatementRef& CallStatement,
		const FBlueprintHelperGraphSemanticIR& SemanticIR,
		const FBlueprintHelperGraphFragmentDag& FragmentDag,
		FBlueprintHelperCallFunctionResolveRequest& InOutRequest)
	{
		TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>> ExpressionsByPath;
		IndexSemanticExpressions(SemanticIR.Statements, ExpressionsByPath);
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>* ExpressionPtr = ExpressionsByPath.Find(CallStatement.SemanticPath);
		if (!ExpressionPtr || !ExpressionPtr->IsValid())
		{
			return;
		}

		const FBlueprintHelperGraphExpressionIR& Expression = *(*ExpressionPtr);
		if (Expression.Kind != EBlueprintHelperGraphExpressionKind::Call)
		{
			return;
		}

		if (Expression.ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::ComponentMemberFunction)
		{
			InOutRequest.TargetObjectType = Expression.ResolvedTarget.Type;
		}
		if (Expression.TargetObject.IsValid() && !Expression.TargetObject->Type.IsEmpty())
		{
			InOutRequest.TargetObjectType = Expression.TargetObject->Type;
		}
		if (Expression.TargetObject.IsValid())
		{
			const FBlueprintHelperCallFunctionPinType TargetObjectPinType =
				MakeTargetObjectPinTypeFromExpression(Expression.TargetObject);
			if (TargetObjectPinType.IsValid())
			{
				InOutRequest.TargetObjectPinType = TargetObjectPinType;
			}
		}

		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			AddUniqueString(InOutRequest.ArgumentNames, ArgPair.Key);
			if (ArgPair.Value.IsValid() && !ArgPair.Value->Type.IsEmpty())
			{
				InOutRequest.ArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type);
			}
		}

		const FString ExpressionId = GetSemanticExpressionId(Expression);
		InOutRequest.ArgumentPinTypes.Append(CollectSemanticArgumentPinTypesFromDag(FragmentDag, ExpressionId));
		if (!InOutRequest.TargetObjectPinType.IsValid())
		{
			FBlueprintHelperCallFunctionPinType TargetObjectPinType;
			if (TryCollectSemanticTargetObjectPinTypeFromDag(FragmentDag, ExpressionId, TargetObjectPinType))
			{
				InOutRequest.TargetObjectPinType = TargetObjectPinType;
			}
		}
	}

	static void CollectCallFunctionExpressionValue(
		const TSharedPtr<FJsonValue>& ExpressionValue,
		const FString& ExpressionPath,
		const FString& SemanticExpressionPath,
		TArray<FCallFunctionStatementRef>& OutStatements)
	{
		const TSharedPtr<FJsonObject> ExpressionObject = AsJsonObjectIfObject(ExpressionValue);
		if (!ExpressionObject.IsValid())
		{
			return;
		}

		FString Kind;
		ExpressionObject->TryGetStringField(TEXT("kind"), Kind);
		if (Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase))
		{
			FString Query;
			ExpressionObject->TryGetStringField(TEXT("target"), Query);

			Query.TrimStartAndEndInline();
			if (!Query.IsEmpty())
			{
				FCallFunctionStatementRef Ref;
				Ref.StatementObject = ExpressionObject;
				Ref.StatementPath = ExpressionPath;
				Ref.SemanticPath = SemanticExpressionPath;
				Ref.NamePath = ExpressionPath + TEXT(".target");
				Ref.Query = Query;
				Ref.bExpression = true;
				OutStatements.Add(MoveTemp(Ref));
			}
		}

		const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
		if (ExpressionObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) &&
			ArgsObjectPtr && ArgsObjectPtr->IsValid())
		{
			for (const auto& Pair : (*ArgsObjectPtr)->Values)
			{
				const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Pair.Key);
				CollectCallFunctionExpressionValue(
					Pair.Value,
					ExpressionPath + TEXT(".args.") + Key,
					SemanticExpressionPath + TEXT(".args.") + Key,
					OutStatements);
			}
		}

		const TCHAR* ExpressionFieldNames[] =
		{
			TEXT("target_object"),
			TEXT("value"),
			TEXT("condition"),
			TEXT("index"),
			TEXT("left"),
			TEXT("right"),
		};
		for (const TCHAR* FieldName : ExpressionFieldNames)
		{
			const TSharedPtr<FJsonValue> FieldValue = FBlueprintHelperVersionCompat::FindJsonValue(ExpressionObject, FieldName);
			if (FieldValue.IsValid())
			{
				CollectCallFunctionExpressionValue(
					FieldValue,
					ExpressionPath + TEXT(".") + FieldName,
					SemanticExpressionPath + TEXT(".") + FieldName,
					OutStatements);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* OptionValues = nullptr;
		if (ExpressionObject->TryGetArrayField(TEXT("options"), OptionValues) && OptionValues)
		{
			for (int32 OptionIndex = 0; OptionIndex < OptionValues->Num(); ++OptionIndex)
			{
				CollectCallFunctionExpressionValue(
					(*OptionValues)[OptionIndex],
					FString::Printf(TEXT("%s.options[%d]"), *ExpressionPath, OptionIndex),
					FString::Printf(TEXT("%s.options[%d]"), *SemanticExpressionPath, OptionIndex),
					OutStatements);
			}
		}
	}

	static void CollectCallFunctionExpressionMap(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldName,
		const FString& ExpressionPath,
		const FString& SemanticExpressionPath,
		TArray<FCallFunctionStatementRef>& OutStatements)
	{
		const TSharedPtr<FJsonObject>* MapObjectPtr = nullptr;
		if (!Object.IsValid() ||
			!Object->TryGetObjectField(FieldName, MapObjectPtr) ||
			!MapObjectPtr || !MapObjectPtr->IsValid())
		{
			return;
		}

		for (const auto& Pair : (*MapObjectPtr)->Values)
		{
			const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Pair.Key);
			CollectCallFunctionExpressionValue(
				Pair.Value,
				ExpressionPath + TEXT(".") + FieldName + TEXT(".") + Key,
				SemanticExpressionPath + TEXT(".") + FieldName + TEXT(".") + Key,
				OutStatements);
		}
	}

	static void CollectCallFunctionExpressionField(
		const TSharedPtr<FJsonObject>& Object,
		const FString& FieldName,
		const FString& ExpressionPath,
		const FString& SemanticExpressionPath,
		TArray<FCallFunctionStatementRef>& OutStatements)
	{
		if (!Object.IsValid())
		{
			return;
		}

		const TSharedPtr<FJsonValue> FieldValue = FBlueprintHelperVersionCompat::FindJsonValue(Object, FieldName);
		if (FieldValue.IsValid())
		{
			CollectCallFunctionExpressionValue(
				FieldValue,
				ExpressionPath + TEXT(".") + FieldName,
				SemanticExpressionPath + TEXT(".") + FieldName,
				OutStatements);
		}
	}

	static void CollectCallFunctionStatements(
		const TArray<TSharedPtr<FJsonValue>>& StatementValues,
		const FString& StatementsPath,
		const FString& SemanticStatementsPath,
		TArray<FCallFunctionStatementRef>& OutStatements)
	{
		for (int32 StatementIndex = 0; StatementIndex < StatementValues.Num(); ++StatementIndex)
		{
			const TSharedPtr<FJsonObject> StatementObject = AsJsonObjectIfObject(StatementValues[StatementIndex]);
			if (!StatementObject.IsValid())
			{
				continue;
			}

			const FString StatementPath = FString::Printf(TEXT("%s[%d]"), *StatementsPath, StatementIndex);
			const FString SemanticPath = FString::Printf(TEXT("%s[%d]"), *SemanticStatementsPath, StatementIndex);
			FString Kind;
			StatementObject->TryGetStringField(TEXT("kind"), Kind);
			if (Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase))
			{
				FString Query;
				StatementObject->TryGetStringField(TEXT("target"), Query);

				Query.TrimStartAndEndInline();
				if (!Query.IsEmpty())
				{
					FCallFunctionStatementRef Ref;
					Ref.StatementObject = StatementObject;
					Ref.StatementPath = StatementPath;
					Ref.SemanticPath = SemanticPath;
					Ref.NamePath = StatementPath + TEXT(".target");
					Ref.Query = Query;
					OutStatements.Add(MoveTemp(Ref));
				}
			}

			CollectCallFunctionExpressionMap(StatementObject, TEXT("args"), StatementPath, SemanticPath, OutStatements);
			CollectCallFunctionExpressionField(StatementObject, TEXT("target_object"), StatementPath, SemanticPath, OutStatements);
			CollectCallFunctionExpressionField(StatementObject, TEXT("value"), StatementPath, SemanticPath, OutStatements);
			CollectCallFunctionExpressionField(StatementObject, TEXT("condition"), StatementPath, SemanticPath, OutStatements);

			const TArray<TSharedPtr<FJsonValue>>* ThenStatements = nullptr;
			if (StatementObject->TryGetArrayField(TEXT("then"), ThenStatements) && ThenStatements)
			{
				CollectCallFunctionStatements(*ThenStatements, StatementPath + TEXT(".then"), SemanticPath + TEXT(".then"), OutStatements);
			}

			const TArray<TSharedPtr<FJsonValue>>* ElseStatements = nullptr;
			if (StatementObject->TryGetArrayField(TEXT("else"), ElseStatements) && ElseStatements)
			{
				CollectCallFunctionStatements(*ElseStatements, StatementPath + TEXT(".else"), SemanticPath + TEXT(".else"), OutStatements);
			}
		}
	}

	static TArray<TSharedPtr<FJsonValue>> MakeResolvedCallFunctionFactArray(
		const TArray<FResolvedCallFunctionRuntimeFact>& Facts)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FResolvedCallFunctionRuntimeFact& Fact : Facts)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			if (!Fact.StepId.IsEmpty())
			{
				Json->SetStringField(TEXT("step_id"), Fact.StepId);
			}
			Json->SetStringField(TEXT("statement_path"), Fact.StatementPath);
			Json->SetStringField(TEXT("query"), Fact.Query);
			Json->SetStringField(TEXT("stable_id"), Fact.StableId);
			Json->SetStringField(TEXT("native_name"), Fact.NativeName);
			Json->SetStringField(TEXT("display_name"), Fact.DisplayName);
			if (!Fact.OwnerClassPath.IsEmpty())
			{
				Json->SetStringField(TEXT("owner_class"), Fact.OwnerClassPath);
			}
			Values.Add(MakeShared<FJsonValueObject>(Json));
		}
		return Values;
	}

	static void AttachRuntimeFacts(
		TSharedPtr<FJsonObject> Data,
		const TArray<FResolvedCallFunctionRuntimeFact>& ResolvedCallFunctionFacts)
	{
		if (!Data.IsValid() || ResolvedCallFunctionFacts.Num() == 0)
		{
			return;
		}

		TSharedRef<FJsonObject> RuntimeFacts = MakeShared<FJsonObject>();
		RuntimeFacts->SetArrayField(
			TEXT("resolved_call_functions"),
			MakeResolvedCallFunctionFactArray(ResolvedCallFunctionFacts));
		Data->SetObjectField(TEXT("runtime_facts"), RuntimeFacts);
	}

	static void AttachRuntimeFactJsonValues(
		TSharedPtr<FJsonObject> Data,
		const TArray<TSharedPtr<FJsonValue>>& FactValues)
	{
		if (!Data.IsValid() || FactValues.Num() == 0)
		{
			return;
		}

		const TSharedPtr<FJsonObject>* RuntimeFactsPtr = nullptr;
		TSharedPtr<FJsonObject> RuntimeFacts;
		if (Data->TryGetObjectField(TEXT("runtime_facts"), RuntimeFactsPtr) &&
			RuntimeFactsPtr && RuntimeFactsPtr->IsValid())
		{
			RuntimeFacts = *RuntimeFactsPtr;
		}
		else
		{
			RuntimeFacts = MakeShared<FJsonObject>();
			Data->SetObjectField(TEXT("runtime_facts"), RuntimeFacts.ToSharedRef());
		}

		TArray<TSharedPtr<FJsonValue>> Values;
		const TArray<TSharedPtr<FJsonValue>>* ExistingValues = nullptr;
		if (RuntimeFacts->TryGetArrayField(TEXT("resolved_call_functions"), ExistingValues) && ExistingValues)
		{
			Values = *ExistingValues;
		}
		for (const TSharedPtr<FJsonValue>& FactValue : FactValues)
		{
			Values.Add(FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneJsonValue(FactValue));
		}
		RuntimeFacts->SetArrayField(TEXT("resolved_call_functions"), Values);
	}

	static TSharedRef<FJsonObject> MakeCompactCallFunctionCandidateJson(
		const FBlueprintHelperCallFunctionCandidateInfo& Candidate)
	{
		TSharedRef<FJsonObject> CandidateJson = MakeShared<FJsonObject>();
		CandidateJson->SetStringField(TEXT("stable_id"), Candidate.StableId);
		CandidateJson->SetStringField(TEXT("display_name"), Candidate.DisplayName);
		CandidateJson->SetStringField(TEXT("owner_class"), Candidate.OwnerClassPath);
		CandidateJson->SetStringField(TEXT("native_name"), Candidate.NativeFunctionName);
		return CandidateJson;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeCandidateFunctionGroupsJson(
		const FString& Query,
		const TArray<FBlueprintHelperCallFunctionCandidateInfo>& Candidates)
	{
		TArray<TSharedPtr<FJsonValue>> Groups;
		if (Candidates.Num() == 0)
		{
			return Groups;
		}

		TSharedRef<FJsonObject> Group = MakeShared<FJsonObject>();
		Group->SetStringField(TEXT("query"), Query);

		TArray<TSharedPtr<FJsonValue>> CandidateValues;
		for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : Candidates)
		{
			CandidateValues.Add(MakeShared<FJsonValueObject>(MakeCompactCallFunctionCandidateJson(Candidate)));
		}
		Group->SetArrayField(TEXT("candidates"), CandidateValues);
		Groups.Add(MakeShared<FJsonValueObject>(Group));
		return Groups;
	}

	static FString SanitizeGraphWriteAutoSearchIdSegment(const FString& Raw)
	{
		FString Sanitized;
		for (const TCHAR Ch : Raw)
		{
			const bool bAllowed =
				(Ch >= TCHAR('A') && Ch <= TCHAR('Z')) ||
				(Ch >= TCHAR('a') && Ch <= TCHAR('z')) ||
				(Ch >= TCHAR('0') && Ch <= TCHAR('9')) ||
				Ch == TCHAR('_') ||
				Ch == TCHAR('-');
			Sanitized.AppendChar(bAllowed ? Ch : TCHAR('_'));
		}
		Sanitized.TrimStartAndEndInline();
		return Sanitized.IsEmpty() ? TEXT("statement") : Sanitized;
	}

	static FString ReadGraphWriteAutoSearchStatementId(const FCallFunctionStatementRef& CallStatement)
	{
		FString StatementId;
		if (CallStatement.StatementObject.IsValid())
		{
			CallStatement.StatementObject->TryGetStringField(TEXT("statement_id"), StatementId);
		}
		if (StatementId.TrimStartAndEnd().IsEmpty())
		{
			StatementId = CallStatement.StatementPath;
		}
		return SanitizeGraphWriteAutoSearchIdSegment(StatementId);
	}

	static FString MakeGraphWriteAutoSearchPreviewScope(
		const FString& StepId,
		const FString& ContextRevisionManifestHash)
	{
		const FString StableText = StepId + TEXT("|") + ContextRevisionManifestHash;
		return FString::Printf(TEXT("gw_%08x"), FCrc::StrCrc32(*StableText));
	}

	static FString MakeGraphWriteAutoSearchCandidateId(
		const FString& PreviewScope,
		const FString& StatementId,
		int32 CandidateIndex)
	{
		return FString::Printf(
			TEXT("preview:%s:%s:%03d"),
			*PreviewScope,
			*SanitizeGraphWriteAutoSearchIdSegment(StatementId),
			CandidateIndex + 1);
	}

	static FString ShortClassName(const FString& Path)
	{
		int32 DotIndex = INDEX_NONE;
		int32 SlashIndex = INDEX_NONE;
		Path.FindLastChar(TEXT('.'), DotIndex);
		Path.FindLastChar(TEXT('/'), SlashIndex);
		const int32 CutIndex = FMath::Max(DotIndex, SlashIndex);
		return CutIndex != INDEX_NONE ? Path.Mid(CutIndex + 1) : Path;
	}

	static FString HashGraphWriteAutoSearchEvidence(const FString& StableText)
	{
		return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*StableText));
	}

	static TSharedRef<FJsonObject> MakeGraphWriteAutoSearchCandidateJson(
		const FString& CandidateId,
		const FBlueprintHelperCallFunctionCandidateInfo& Candidate)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("candidate_id"), CandidateId);
		Json->SetStringField(TEXT("suggested_kind"), TEXT("call"));
		Json->SetStringField(TEXT("display_name"), Candidate.DisplayName);
		Json->SetStringField(TEXT("owner_short"), ShortClassName(Candidate.OwnerClassPath));
		Json->SetStringField(TEXT("node_class"), ShortClassName(Candidate.NodeClassPath.IsEmpty() ? TEXT("K2Node_CallFunction") : Candidate.NodeClassPath));
		Json->SetStringField(TEXT("match_reason"), Candidate.MatchReason.IsEmpty() ? TEXT("target text + graph context compatible") : Candidate.MatchReason);
		return Json;
	}

	static TSharedRef<FJsonObject> MakeGraphWriteAutoSearchArtifactCandidateJson(
		const FString& CandidateId,
		const FString& StatementId,
		const FString& SnapshotGeneration,
		const FBlueprintHelperCallFunctionCandidateInfo& Candidate)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("candidate_id"), CandidateId);
		Json->SetStringField(TEXT("statement_id"), StatementId);
		Json->SetStringField(TEXT("stable_id"), Candidate.StableId);
		Json->SetStringField(TEXT("owner_path"), Candidate.OwnerClassPath);
		Json->SetStringField(TEXT("node_class"), Candidate.NodeClassPath.IsEmpty() ? TEXT("K2Node_CallFunction") : Candidate.NodeClassPath);
		Json->SetStringField(TEXT("spawner_signature_hash"), HashGraphWriteAutoSearchEvidence(Candidate.StableId + TEXT("|") + Candidate.NodeClassPath));
		Json->SetStringField(TEXT("snapshot_generation"), SnapshotGeneration);
		Json->SetStringField(TEXT("pin_shape_hash"), HashGraphWriteAutoSearchEvidence(FString::Join(Candidate.InputPins, TEXT("|"))));
		return Json;
	}

	static bool TryParseGraphWriteAutoSearchCandidateId(
		const FString& CandidateId,
		FString& OutPreviewScope,
		FString& OutStatementId,
		int32& OutCandidateIndex)
	{
		OutPreviewScope.Reset();
		OutStatementId.Reset();
		OutCandidateIndex = INDEX_NONE;

		TArray<FString> Parts;
		CandidateId.ParseIntoArray(Parts, TEXT(":"), false);
		if (Parts.Num() != 4 || Parts[0] != TEXT("preview") || Parts[1].IsEmpty() || Parts[2].IsEmpty())
		{
			return false;
		}
		int32 CandidateOrdinal = 0;
		if (!LexTryParseString(CandidateOrdinal, *Parts[3]) || CandidateOrdinal <= 0)
		{
			return false;
		}
		OutPreviewScope = Parts[1];
		OutStatementId = Parts[2];
		OutCandidateIndex = CandidateOrdinal - 1;
		return true;
	}

	static bool TryFindGraphWriteAutoSearchCandidateInArtifact(
		const TSharedPtr<FJsonObject>& ArtifactJson,
		const FString& CandidateId,
		FString& OutStableId)
	{
		OutStableId.Reset();
		if (!ArtifactJson.IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* CandidateValues = nullptr;
		if (!ArtifactJson->TryGetArrayField(TEXT("candidates"), CandidateValues) || !CandidateValues)
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& CandidateValue : *CandidateValues)
		{
			const TSharedPtr<FJsonObject> CandidateObject = AsJsonObjectIfObject(CandidateValue);
			if (!CandidateObject.IsValid())
			{
				continue;
			}
			FString ExistingCandidateId;
			if (CandidateObject->TryGetStringField(TEXT("candidate_id"), ExistingCandidateId) &&
				ExistingCandidateId == CandidateId &&
				CandidateObject->TryGetStringField(TEXT("stable_id"), OutStableId) &&
				!OutStableId.IsEmpty())
			{
				return true;
			}
		}
		return false;
	}

	static bool TryResolveGraphWriteAutoSearchCurrentCandidateStableId(
		const FString& CandidatePreviewScope,
		const FString& CandidateStatementId,
		int32 CandidateIndex,
		const FString& CurrentPreviewScope,
		const FString& CurrentStatementId,
		const FBlueprintHelperCallFunctionResolveRequest& ResolveRequest,
		FString& OutStableId)
	{
		OutStableId.Reset();
		if (CandidateIndex < 0 ||
			CandidatePreviewScope != CurrentPreviewScope ||
			CandidateStatementId != CurrentStatementId)
		{
			return false;
		}

		FBlueprintHelperCallFunctionResolveRequest DiscoveryRequest = ResolveRequest;
		DiscoveryRequest.SelectedCandidateId.Reset();
		DiscoveryRequest.CandidatePolicy.RequiredStableCallableIds.Reset();

		const FBlueprintHelperCallFunctionResolveResult DiscoveryResult =
			FBlueprintHelperCallFunctionResolver::Resolve(DiscoveryRequest);
		if (DiscoveryResult.CandidateFunctions.IsValidIndex(CandidateIndex) &&
			!DiscoveryResult.CandidateFunctions[CandidateIndex].StableId.IsEmpty())
		{
			OutStableId = DiscoveryResult.CandidateFunctions[CandidateIndex].StableId;
			return true;
		}
		if (CandidateIndex == 0 &&
			DiscoveryResult.IsResolved() &&
			!DiscoveryResult.Selected.StableId.IsEmpty())
		{
			OutStableId = DiscoveryResult.Selected.StableId;
			return true;
		}
		return false;
	}

	static bool GraphWriteAutoSearchStatementMatchesSelection(
		const TSharedPtr<FJsonObject>& StatementObject,
		const FString& StatementId,
		const FString& CandidateId)
	{
		if (!StatementObject.IsValid())
		{
			return false;
		}

		FString RawStatementId;
		if (StatementObject->TryGetStringField(TEXT("statement_id"), RawStatementId) &&
			SanitizeGraphWriteAutoSearchIdSegment(RawStatementId) == StatementId)
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* ActionSelectionObjectPtr = nullptr;
		FString ExistingCandidateId;
		return StatementObject->TryGetObjectField(TEXT("action_selection"), ActionSelectionObjectPtr) &&
			ActionSelectionObjectPtr && ActionSelectionObjectPtr->IsValid() &&
			(*ActionSelectionObjectPtr)->TryGetStringField(TEXT("candidate_id"), ExistingCandidateId) &&
			ExistingCandidateId == CandidateId;
	}

	static void ApplyGraphWriteAutoSearchResolvedStableIdToStatements(
		const TArray<TSharedPtr<FJsonValue>>& StatementValues,
		const FString& StatementId,
		const FString& CandidateId,
		const FString& StableId)
	{
		if (StableId.IsEmpty())
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& StatementValue : StatementValues)
		{
			const TSharedPtr<FJsonObject> StatementObject = AsJsonObjectIfObject(StatementValue);
			if (!StatementObject.IsValid())
			{
				continue;
			}
			if (GraphWriteAutoSearchStatementMatchesSelection(StatementObject, StatementId, CandidateId))
			{
				StatementObject->SetStringField(TEXT("resolved_stable_id"), StableId);
			}

			const TArray<TSharedPtr<FJsonValue>>* ThenStatements = nullptr;
			if (StatementObject->TryGetArrayField(TEXT("then"), ThenStatements) && ThenStatements)
			{
				ApplyGraphWriteAutoSearchResolvedStableIdToStatements(*ThenStatements, StatementId, CandidateId, StableId);
			}
			const TArray<TSharedPtr<FJsonValue>>* ElseStatements = nullptr;
			if (StatementObject->TryGetArrayField(TEXT("else"), ElseStatements) && ElseStatements)
			{
				ApplyGraphWriteAutoSearchResolvedStableIdToStatements(*ElseStatements, StatementId, CandidateId, StableId);
			}
		}
	}

	static void ApplyGraphWriteAutoSearchResolvedStableIdToLogicSpec(
		const TSharedPtr<FJsonObject>& LogicSpec,
		const FString& StatementId,
		const FString& CandidateId,
		const FString& StableId)
	{
		const TArray<TSharedPtr<FJsonValue>>* StatementValues = nullptr;
		if (!LogicSpec.IsValid() ||
			!LogicSpec->TryGetArrayField(TEXT("statements"), StatementValues) ||
			!StatementValues)
		{
			return;
		}
		ApplyGraphWriteAutoSearchResolvedStableIdToStatements(
			*StatementValues,
			StatementId,
			CandidateId,
			StableId);
	}

	static void ApplyGraphWriteAutoSearchResolvedStableIdToPayloads(
		const TSharedPtr<FJsonObject>& StepObject,
		const TSharedPtr<FJsonObject>& LoweredPayload,
		const FString& StatementId,
		const FString& CandidateId,
		const FString& StableId)
	{
		TArray<FCallFunctionLogicSpecRef> StepLogicSpecs;
		CollectCallFunctionLogicSpecs(StepObject, nullptr, StepLogicSpecs);
		for (const FCallFunctionLogicSpecRef& LogicSpecRef : StepLogicSpecs)
		{
			ApplyGraphWriteAutoSearchResolvedStableIdToLogicSpec(
				LogicSpecRef.LogicSpec,
				StatementId,
				CandidateId,
				StableId);
		}

		TArray<FCallFunctionLogicSpecRef> LoweredLogicSpecs;
		CollectCallFunctionLogicSpecs(nullptr, LoweredPayload, LoweredLogicSpecs);
		for (const FCallFunctionLogicSpecRef& LogicSpecRef : LoweredLogicSpecs)
		{
			ApplyGraphWriteAutoSearchResolvedStableIdToLogicSpec(
				LogicSpecRef.LogicSpec,
				StatementId,
				CandidateId,
				StableId);
		}
	}

	static TSharedRef<FJsonObject> MakeGraphWriteAutoSearchCandidateRequiredData(
		const FString& Message,
		const FString& Query,
		const FString& Path,
		const TArray<TSharedPtr<FJsonValue>>& CandidateJsonValues,
		int32 CandidateCount,
		bool bTruncated)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("resolution_status"), TEXT("candidate_required"));
		Data->SetStringField(TEXT("error_code"), TEXT("graphwrite_autosearch_candidate_required"));
		Data->SetArrayField(TEXT("candidates"), CandidateJsonValues);
		Data->SetNumberField(TEXT("candidate_count"), CandidateCount);
		Data->SetBoolField(TEXT("truncated"), bTruncated);
		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), false);
		DryRun->SetArrayField(TEXT("conflicts"), {});

		TSharedRef<FJsonObject> Issue = MakeShared<FJsonObject>();
		Issue->SetStringField(TEXT("code"), TEXT("graphwrite_autosearch_candidate_required"));
		Issue->SetStringField(TEXT("stage"), ToolStageToString(EBlueprintHelperToolStage::DryRun));
		Issue->SetStringField(TEXT("path"), Path);
		Issue->SetStringField(TEXT("source"), Path);
		Issue->SetStringField(TEXT("target"), Query);
		Issue->SetStringField(TEXT("message"), Message);
		Issue->SetStringField(TEXT("resolution_status"), TEXT("candidate_required"));
		Issue->SetArrayField(TEXT("candidates"), CandidateJsonValues);
		Issue->SetNumberField(TEXT("candidate_count"), CandidateCount);
		Issue->SetBoolField(TEXT("truncated"), bTruncated);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueObject>(Issue));
		DryRun->SetArrayField(TEXT("errors"), Errors);
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		return Data;
	}

	static TSharedRef<FJsonObject> MakeCallFunctionResolutionBlockedData(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Query,
		const FString& Path,
		const TArray<FBlueprintHelperCallFunctionCandidateInfo>& Candidates)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), false);
		DryRun->SetArrayField(TEXT("conflicts"), {});

		TSharedRef<FJsonObject> Issue = MakeShared<FJsonObject>();
		Issue->SetStringField(TEXT("code"), Code);
		Issue->SetStringField(TEXT("stage"), ToolStageToString(Stage));
		Issue->SetStringField(TEXT("path"), Path);
		Issue->SetStringField(TEXT("source"), Path);
		Issue->SetStringField(TEXT("target"), Query);
		if (!Message.IsEmpty())
		{
			Issue->SetStringField(TEXT("message"), Message);
		}
		const TArray<TSharedPtr<FJsonValue>> CandidateGroups = MakeCandidateFunctionGroupsJson(Query, Candidates);
		if (CandidateGroups.Num() > 0)
		{
			Issue->SetArrayField(TEXT("candidate_functions"), CandidateGroups);
		}

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueObject>(Issue));
		DryRun->SetArrayField(TEXT("errors"), Errors);
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		return Data;
	}

	static void PopulateCallFunctionResolveContext(
		FBlueprintHelperCallFunctionResolveRequest& Request,
		UBlueprint* Blueprint,
		UEdGraph* Graph)
	{
		Request.Blueprint = Blueprint;
		Request.Graph = Graph;
		Request.Context.Blueprint = Blueprint;
		Request.Context.Graph = Graph;
		Request.Context.Schema = Graph ? Graph->GetSchema() : nullptr;
		Request.Context.SelfClass = Blueprint
			? (Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->SkeletonGeneratedClass.Get())
			: nullptr;
		Request.Context.GraphKind = Graph && Graph->GetClass() ? Graph->GetClass()->GetName() : FString();
		Request.Context.ArgumentNames = Request.ArgumentNames;
		Request.Context.ArgumentTypes = Request.ArgumentTypes;
		Request.Context.ArgumentPinTypes = Request.ArgumentPinTypes;
		Request.Context.TargetObjectType = Request.TargetObjectType;
		Request.Context.TargetObjectPinType = Request.TargetObjectPinType;
		Request.Context.ExpectedReturnType = Request.ExpectedReturnType;
		Request.Context.ExpectedReturnPinType = Request.ExpectedReturnPinType;
	}

	static bool IsCachedCallFunctionResolutionStillAvailable(
		const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& CachedResolution)
	{
		if (CachedResolution.StableId.IsEmpty() ||
			CachedResolution.NativeName.IsEmpty() ||
			CachedResolution.OwnerClassPath.IsEmpty())
		{
			return false;
		}

		UClass* OwnerClass = FindObject<UClass>(nullptr, *CachedResolution.OwnerClassPath);
		if (!OwnerClass)
		{
			OwnerClass = LoadObject<UClass>(nullptr, *CachedResolution.OwnerClassPath);
		}
		return OwnerClass && OwnerClass->FindFunctionByName(*CachedResolution.NativeName) != nullptr;
	}

	static void CollectCallFunctionLogicSpecs(
		const TSharedPtr<FJsonObject>& StepObject,
		const TSharedPtr<FJsonObject>& LoweredPayload,
		TArray<FCallFunctionLogicSpecRef>& OutLogicSpecs)
	{
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (StepObject.IsValid() &&
			StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) &&
			WriteObjectPtr && WriteObjectPtr->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
			if ((*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) && OpsArray)
			{
				for (int32 OpIndex = 0; OpIndex < OpsArray->Num(); ++OpIndex)
				{
					const TSharedPtr<FJsonObject> OpObject = AsJsonObjectIfObject((*OpsArray)[OpIndex]);
					if (!OpObject.IsValid())
					{
						continue;
					}

					const TSharedPtr<FJsonObject>* BodyObjectPtr = nullptr;
					FCallFunctionLogicSpecRef Ref;
					if (OpObject->TryGetObjectField(TEXT("body"), BodyObjectPtr) &&
						BodyObjectPtr && BodyObjectPtr->IsValid())
					{
						Ref.LogicSpec = *BodyObjectPtr;
						Ref.LogicSpecPath = FString::Printf(TEXT("write.ops[%d].body"), OpIndex);
						OutLogicSpecs.Add(MoveTemp(Ref));
					}
					else if (OpObject->TryGetObjectField(TEXT("logic_spec"), BodyObjectPtr) &&
						BodyObjectPtr && BodyObjectPtr->IsValid())
					{
						Ref.LogicSpec = *BodyObjectPtr;
						Ref.LogicSpecPath = FString::Printf(TEXT("write.ops[%d].logic_spec"), OpIndex);
						OutLogicSpecs.Add(MoveTemp(Ref));
					}
				}
			}
		}

		if (!OutLogicSpecs.IsEmpty())
		{
			return;
		}

		const TSharedPtr<FJsonObject>* LoweredLogicSpecPtr = nullptr;
		if (LoweredPayload.IsValid() &&
			LoweredPayload->TryGetObjectField(TEXT("logic_spec"), LoweredLogicSpecPtr) &&
			LoweredLogicSpecPtr && LoweredLogicSpecPtr->IsValid())
		{
			FCallFunctionLogicSpecRef Ref;
			Ref.LogicSpec = *LoweredLogicSpecPtr;
			Ref.LogicSpecPath = TEXT("payload.logic_spec");
			OutLogicSpecs.Add(MoveTemp(Ref));
		}
	}

	static void ReadCallFunctionResolutionTarget(
		const TSharedPtr<FJsonObject>& StepObject,
		const TSharedPtr<FJsonObject>& LoweredPayload,
		FString& OutAssetPath,
		FString& OutGraphName)
	{
		OutAssetPath.Reset();
		OutGraphName.Reset();

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (LoweredPayload.IsValid() &&
			LoweredPayload->TryGetObjectField(TEXT("target"), TargetObjectPtr) &&
			TargetObjectPtr && TargetObjectPtr->IsValid())
		{
			(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath);
			(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), OutGraphName);
			if (!OutAssetPath.IsEmpty())
			{
				return;
			}
		}

		if (StepObject.IsValid() &&
			StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) &&
			TargetObjectPtr && TargetObjectPtr->IsValid())
		{
			(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath);
			(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), OutGraphName);
		}
	}

	static bool TryResolveTaskRuntimeCallFunctions(
		const TSharedPtr<FJsonObject>& StepObject,
		const TSharedPtr<FJsonObject>& LoweredPayload,
		int32 StepIndex,
		const FString& StepId,
		bool bDryRun,
		const FString& AssetStateHash,
		const FString& ContextRevisionManifestHash,
		const TSharedPtr<FJsonObject>& PreviewArtifactJson,
		FBlueprintHelperGraphWriteCandidateArtifactStore* CandidateArtifactStore,
		FBlueprintHelperTaskRuntimeCallFunctionResolutionCache& ResolutionCache,
		TArray<FResolvedCallFunctionRuntimeFact>& OutResolvedFacts,
		FBlueprintHelperToolError& OutError,
		TSharedPtr<FJsonObject>& OutBlockedStepData,
		TSharedPtr<FJsonObject>& OutCandidateArtifactJson)
	{
		FString AssetPath;
		FString GraphName;
		ReadCallFunctionResolutionTarget(StepObject, LoweredPayload, AssetPath, GraphName);

		TArray<FCallFunctionLogicSpecRef> LogicSpecs;
		CollectCallFunctionLogicSpecs(StepObject, LoweredPayload, LogicSpecs);
		if (LogicSpecs.IsEmpty())
		{
			return true;
		}

		FBlueprintHelperGraphWriteAutoSearchPolicy AutoSearchPolicy;
		FString AutoSearchPolicyError;
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		const TSharedPtr<FJsonObject> WriteObject =
			StepObject.IsValid() &&
			StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) &&
			WriteObjectPtr && WriteObjectPtr->IsValid()
				? *WriteObjectPtr
				: nullptr;
		if (!FBlueprintHelperGraphWriteAutoSearchPolicyResolver::TryParseFromWriteObject(
			WriteObject,
			AutoSearchPolicy,
			AutoSearchPolicyError))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graphwrite_autosearch_policy"),
				bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Execute,
				AutoSearchPolicyError,
				FString::Printf(TEXT("task_plan.steps[%d].write.auto_search_policy"), StepIndex));
			return false;
		}
		int32 AutoSearchStatementCount = 0;
		const double AutoSearchStartSeconds = FPlatformTime::Seconds();

		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		bool bTriedResolveBlueprint = false;

		for (const FCallFunctionLogicSpecRef& LogicSpecRef : LogicSpecs)
		{
			const TArray<TSharedPtr<FJsonValue>>* StatementValues = nullptr;
			if (!LogicSpecRef.LogicSpec.IsValid() ||
				!LogicSpecRef.LogicSpec->TryGetArrayField(TEXT("statements"), StatementValues) ||
				!StatementValues)
			{
				continue;
			}

			TArray<FCallFunctionStatementRef> CallStatements;
			CollectCallFunctionStatements(
				*StatementValues,
				LogicSpecRef.LogicSpecPath + TEXT(".statements"),
				TEXT("$.statements"),
				CallStatements);
			if (CallStatements.IsEmpty())
			{
				continue;
			}

			if (!bTriedResolveBlueprint)
			{
				Blueprint = ResolveTaskRuntimeBlueprint(AssetPath);
				Graph = ResolveTaskRuntimeGraph(Blueprint, GraphName);
				bTriedResolveBlueprint = true;
			}
			if (!Blueprint)
			{
				const EBlueprintHelperToolStage Stage = bDryRun
					? EBlueprintHelperToolStage::DryRun
					: EBlueprintHelperToolStage::Execute;
				const FString Code = TEXT("call_function_resolution_context_missing");
				const FString Message = FString::Printf(
					TEXT("call_function resolve failed: target Blueprint could not be loaded for asset '%s'."),
					*AssetPath);
				OutError = MakeTaskRuntimeError(
					Code,
					Stage,
					Message,
					FString::Printf(TEXT("task_plan.steps[%d].target.asset_path"), StepIndex));
				OutBlockedStepData = MakeCallFunctionResolutionBlockedData(
					Code,
					Stage,
					Message,
					CallStatements[0].Query,
					CallStatements[0].NamePath,
					TArray<FBlueprintHelperCallFunctionCandidateInfo>());
				return false;
			}

			FBlueprintHelperGraphSemanticIR SemanticIR;
			FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpecRef.LogicSpec, Blueprint, SemanticIR);
			FBlueprintHelperGraphFragmentDag FragmentDag;
			FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(SemanticIR, FragmentDag);

			for (const FCallFunctionStatementRef& CallStatement : CallStatements)
			{
				FBlueprintHelperCallFunctionResolveRequest ResolveRequest;
				ResolveRequest.Query = CallStatement.Query;
				CallStatement.StatementObject->TryGetStringField(TEXT("search_mode"), ResolveRequest.SearchMode);
				CallStatement.StatementObject->TryGetStringField(TEXT("ambiguity"), ResolveRequest.AmbiguityPolicy);
				CallStatement.StatementObject->TryGetStringField(TEXT("ambiguity_policy"), ResolveRequest.AmbiguityPolicy);
				CallStatement.StatementObject->TryGetStringField(TEXT("resolution_policy"), ResolveRequest.ResolutionPolicy);
				const TSharedPtr<FJsonObject>* ActionSelectionObjectPtr = nullptr;
				if (CallStatement.StatementObject->TryGetObjectField(TEXT("action_selection"), ActionSelectionObjectPtr) &&
					ActionSelectionObjectPtr && ActionSelectionObjectPtr->IsValid())
				{
					(*ActionSelectionObjectPtr)->TryGetStringField(TEXT("candidate_id"), ResolveRequest.SelectedCandidateId);
				}
				const FString AutoSearchStatementId = ReadGraphWriteAutoSearchStatementId(CallStatement);
				const bool bStatementAutoSearch =
					ResolveRequest.ResolutionPolicy.Equals(TEXT("auto_search"), ESearchCase::IgnoreCase);
				FString SelectedAutoSearchStableId;
				FString SelectedCandidatePreviewScope;
				FString SelectedCandidateStatementId;
				int32 SelectedCandidateIndex = INDEX_NONE;
				if (bStatementAutoSearch || AutoSearchPolicy.bEnablePreviewRecovery)
				{
					ResolveRequest.MaxCandidates = AutoSearchPolicy.MaxCandidatesPerStatement;
					if (bDryRun)
					{
						++AutoSearchStatementCount;
						const int32 ElapsedMs = static_cast<int32>((FPlatformTime::Seconds() - AutoSearchStartSeconds) * 1000.0);
						if (AutoSearchStatementCount > AutoSearchPolicy.MaxAutoSearchStatements ||
							ElapsedMs > AutoSearchPolicy.MaxTotalSearchMs)
						{
							OutError = MakeTaskRuntimeError(
								TEXT("graphwrite_autosearch_budget_exceeded"),
								EBlueprintHelperToolStage::DryRun,
								TEXT("GraphWrite AutoSearch preview budget was exceeded."),
								FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex));
							return false;
						}
					}
				}
				if (!ResolveRequest.SelectedCandidateId.TrimStartAndEnd().IsEmpty())
				{
					if (!TryParseGraphWriteAutoSearchCandidateId(
						ResolveRequest.SelectedCandidateId,
						SelectedCandidatePreviewScope,
						SelectedCandidateStatementId,
						SelectedCandidateIndex))
					{
						OutError = MakeTaskRuntimeError(
							TEXT("invalid_graphwrite_candidate_selection"),
							bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Execute,
							TEXT("action_selection.candidate_id must be a preview-scoped candidate id."),
							FString::Printf(TEXT("task_plan.steps[%d].%s.action_selection.candidate_id"), StepIndex, *CallStatement.NamePath));
						return false;
					}
					const FString CurrentPreviewScope = MakeGraphWriteAutoSearchPreviewScope(StepId, ContextRevisionManifestHash);

					FBlueprintHelperGraphWriteCandidateArtifactRecord ResolvedArtifact;
					if (CandidateArtifactStore &&
						CandidateArtifactStore->TryResolve(
							SelectedCandidatePreviewScope,
							SelectedCandidateStatementId,
							ResolveRequest.SelectedCandidateId,
							ResolvedArtifact))
					{
						SelectedAutoSearchStableId = ResolvedArtifact.StableId;
					}
					if (SelectedAutoSearchStableId.IsEmpty())
					{
						TryFindGraphWriteAutoSearchCandidateInArtifact(
							PreviewArtifactJson,
							ResolveRequest.SelectedCandidateId,
							SelectedAutoSearchStableId);
					}
					if (SelectedAutoSearchStableId.IsEmpty())
					{
						if (SelectedCandidatePreviewScope != CurrentPreviewScope ||
							SelectedCandidateStatementId != AutoSearchStatementId)
						{
							OutError = MakeTaskRuntimeError(
								TEXT("graphwrite_autosearch_candidate_stale"),
								bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Execute,
								TEXT("Selected AutoSearch candidate does not match the current preview scope or statement."),
								FString::Printf(TEXT("task_plan.steps[%d].%s.action_selection.candidate_id"), StepIndex, *CallStatement.NamePath));
							return false;
						}
					}
				}
				const TArray<TSharedPtr<FJsonValue>>* CategoryPriorityValues = nullptr;
				if (CallStatement.StatementObject->TryGetArrayField(TEXT("category_priority"), CategoryPriorityValues) &&
					CategoryPriorityValues)
				{
					for (const TSharedPtr<FJsonValue>& Value : *CategoryPriorityValues)
					{
						if (Value.IsValid())
						{
							ResolveRequest.CategoryPriority.Add(Value->AsString());
						}
					}
				}
				const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
				if (CallStatement.StatementObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) &&
					ArgsObjectPtr && ArgsObjectPtr->IsValid())
				{
					FBlueprintHelperVersionCompat::GetJsonObjectKeys(*ArgsObjectPtr, ResolveRequest.ArgumentNames);
				}
				if (CallStatement.bExpression)
				{
					ApplySemanticExpressionContext(CallStatement, SemanticIR, FragmentDag, ResolveRequest);
				}
				else
				{
					ApplySemanticStatementContext(CallStatement, SemanticIR, FragmentDag, ResolveRequest);
				}
				PopulateCallFunctionResolveContext(ResolveRequest, Blueprint, Graph);

				if (!ResolveRequest.SelectedCandidateId.TrimStartAndEnd().IsEmpty() &&
					ResolveRequest.CandidatePolicy.RequiredStableCallableIds.IsEmpty())
				{
					FString SelectedStableId;
					if (!SelectedAutoSearchStableId.IsEmpty())
					{
						SelectedStableId = SelectedAutoSearchStableId;
					}
					else
					{
						const FString CurrentPreviewScope = MakeGraphWriteAutoSearchPreviewScope(StepId, ContextRevisionManifestHash);
						if (!TryResolveGraphWriteAutoSearchCurrentCandidateStableId(
							SelectedCandidatePreviewScope,
							SelectedCandidateStatementId,
							SelectedCandidateIndex,
							CurrentPreviewScope,
							AutoSearchStatementId,
							ResolveRequest,
							SelectedStableId))
						{
							OutError = MakeTaskRuntimeError(
								TEXT("graphwrite_autosearch_candidate_expired"),
								bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Execute,
								TEXT("Selected AutoSearch candidate is not present in the current ActionDatabase projection."),
								FString::Printf(TEXT("task_plan.steps[%d].%s.action_selection.candidate_id"), StepIndex, *CallStatement.NamePath));
							return false;
						}
					}
					ResolveRequest.CandidatePolicy.RequiredStableCallableIds.Add(SelectedStableId);
					ApplyGraphWriteAutoSearchResolvedStableIdToPayloads(
						StepObject,
						LoweredPayload,
						AutoSearchStatementId,
						ResolveRequest.SelectedCandidateId,
						SelectedStableId);
				}

				const FString ResolutionKey =
					FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::MakeKey(ResolveRequest, AssetPath, GraphName);
				FBlueprintHelperTaskRuntimeCachedCallFunctionResolution CachedResolution;
				const bool bCacheHit = ResolutionCache.TryGet(
					ResolutionKey,
					AssetStateHash,
					ContextRevisionManifestHash,
					FDateTime::UtcNow(),
					CachedResolution);
				if (!bCacheHit || !IsCachedCallFunctionResolutionStillAvailable(CachedResolution))
				{
					const FBlueprintHelperCallFunctionResolveResult ResolveResult =
						FBlueprintHelperCallFunctionResolver::Resolve(ResolveRequest);
					CachedResolution.bResolved = ResolveResult.IsResolved();
					CachedResolution.ErrorCode = ResolveResult.ErrorCode;
					CachedResolution.Message = ResolveResult.Message;
					CachedResolution.CandidateFunctions = ResolveResult.CandidateFunctions;
					CachedResolution.AssetStateHash = AssetStateHash;
					CachedResolution.ContextRevisionManifestHash = ContextRevisionManifestHash;
					CachedResolution.ResolverVersion =
						FBlueprintHelperTaskRuntimeCallFunctionResolutionCache::CurrentResolverVersion();
					if (ResolveResult.IsResolved())
					{
						CachedResolution.StableId = ResolveResult.Selected.StableId;
						CachedResolution.NativeName = ResolveResult.Selected.NativeFunctionName;
						CachedResolution.DisplayName = ResolveResult.Selected.DisplayName;
						CachedResolution.OwnerClassPath = ResolveResult.Selected.OwnerClassPath;
					}
					ResolutionCache.Store(ResolutionKey, CachedResolution, FDateTime::UtcNow());
				}

				if (!CachedResolution.bResolved)
				{
					const FString Code = CachedResolution.ErrorCode.IsEmpty()
						? TEXT("function_call_not_found")
						: CachedResolution.ErrorCode;
					const FString Message = CachedResolution.Message.IsEmpty()
						? FString::Printf(TEXT("call_function resolve failed: %s"), *CallStatement.Query)
						: CachedResolution.Message;
					const EBlueprintHelperToolStage Stage = bDryRun
						? EBlueprintHelperToolStage::DryRun
						: EBlueprintHelperToolStage::Execute;
					const bool bCanProduceAutoSearchCandidates =
						bDryRun &&
						(bStatementAutoSearch || AutoSearchPolicy.bEnablePreviewRecovery) &&
						(Code == TEXT("ambiguous_function_call") || Code == TEXT("function_call_not_found")) &&
						CachedResolution.CandidateFunctions.Num() > 0;
					if (bCanProduceAutoSearchCandidates)
					{
						const FString PreviewScope = MakeGraphWriteAutoSearchPreviewScope(StepId, ContextRevisionManifestHash);
						const FString SnapshotGeneration = PreviewScope;
						TArray<TSharedPtr<FJsonValue>> CandidateJsonValues;
						TArray<TSharedPtr<FJsonValue>> ArtifactCandidateValues;
						const int32 CandidateLimit = FMath::Min(
							CachedResolution.CandidateFunctions.Num(),
							AutoSearchPolicy.MaxCandidatesPerStatement);
						for (int32 CandidateIndex = 0; CandidateIndex < CandidateLimit; ++CandidateIndex)
						{
							const FBlueprintHelperCallFunctionCandidateInfo& Candidate =
								CachedResolution.CandidateFunctions[CandidateIndex];
							const FString CandidateId = MakeGraphWriteAutoSearchCandidateId(
								PreviewScope,
								AutoSearchStatementId,
								CandidateIndex);
							CandidateJsonValues.Add(MakeShared<FJsonValueObject>(
								MakeGraphWriteAutoSearchCandidateJson(CandidateId, Candidate)));

							TSharedRef<FJsonObject> ArtifactCandidateJson =
								MakeGraphWriteAutoSearchArtifactCandidateJson(
									CandidateId,
									AutoSearchStatementId,
									SnapshotGeneration,
									Candidate);
							ArtifactCandidateValues.Add(MakeShared<FJsonValueObject>(ArtifactCandidateJson));

							if (CandidateArtifactStore)
							{
								FBlueprintHelperGraphWriteCandidateArtifactRecord Artifact;
								Artifact.PreviewToken = PreviewScope;
								Artifact.StatementId = AutoSearchStatementId;
								Artifact.CandidateId = CandidateId;
								Artifact.CandidateHash = HashGraphWriteAutoSearchEvidence(Candidate.StableId);
								Artifact.StableId = Candidate.StableId;
								Artifact.SnapshotGeneration = SnapshotGeneration;
								Artifact.NodeClassPath = Candidate.NodeClassPath;
								Artifact.OwnerPath = Candidate.OwnerClassPath;
								Artifact.SpawnerSignatureHash = HashGraphWriteAutoSearchEvidence(Candidate.StableId + TEXT("|") + Candidate.NodeClassPath);
								Artifact.ArgumentNames = Candidate.InputPins;
								Artifact.ArgumentPinTypeSummaries = Candidate.InputPinTypes;
								Artifact.EvidenceJson = ArtifactCandidateJson;
								CandidateArtifactStore->Store(Artifact);
							}
						}

						TSharedRef<FJsonObject> ArtifactJson = MakeShared<FJsonObject>();
						ArtifactJson->SetStringField(TEXT("snapshot_generation"), SnapshotGeneration);
						ArtifactJson->SetStringField(TEXT("action_context_revision_manifest_hash"), ContextRevisionManifestHash);
						ArtifactJson->SetArrayField(TEXT("candidates"), ArtifactCandidateValues);
						OutCandidateArtifactJson = ArtifactJson;

						OutError = MakeTaskRuntimeError(
							TEXT("graphwrite_autosearch_candidate_required"),
							EBlueprintHelperToolStage::DryRun,
							TEXT("GraphWrite AutoSearch requires a candidate selection before execute."),
							FString::Printf(TEXT("task_plan.steps[%d].%s"), StepIndex, *CallStatement.NamePath));
						OutBlockedStepData = MakeGraphWriteAutoSearchCandidateRequiredData(
							TEXT("GraphWrite AutoSearch requires a candidate selection before execute."),
							CallStatement.Query,
							CallStatement.NamePath,
							CandidateJsonValues,
							CachedResolution.CandidateFunctions.Num(),
							CachedResolution.CandidateFunctions.Num() > CandidateLimit);
						return false;
					}
					OutError = MakeTaskRuntimeError(
						Code,
						Stage,
						Message,
						FString::Printf(TEXT("task_plan.steps[%d].%s"), StepIndex, *CallStatement.NamePath));
					OutBlockedStepData = MakeCallFunctionResolutionBlockedData(
						Code,
						Stage,
						Message,
						CallStatement.Query,
						CallStatement.NamePath,
						CachedResolution.CandidateFunctions);
					return false;
				}

				FResolvedCallFunctionRuntimeFact Fact;
				Fact.StepId = StepId;
				Fact.StatementPath = CallStatement.StatementPath;
				Fact.Query = CallStatement.Query;
				Fact.StableId = CachedResolution.StableId;
				Fact.NativeName = CachedResolution.NativeName;
				Fact.DisplayName = CachedResolution.DisplayName;
				Fact.OwnerClassPath = CachedResolution.OwnerClassPath;
				OutResolvedFacts.Add(MoveTemp(Fact));
				CallStatement.StatementObject->SetStringField(TEXT("resolved_stable_id"), CachedResolution.StableId);
				ApplyGraphWriteAutoSearchResolvedStableIdToPayloads(
					StepObject,
					LoweredPayload,
					AutoSearchStatementId,
					ResolveRequest.SelectedCandidateId,
					CachedResolution.StableId);
			}
		}

		return true;
	}

	struct FTaskPlanStepPayloadParts
	{
		TSharedPtr<FJsonObject> TargetObject;
		TSharedPtr<FJsonObject> ArgsObject;
		FString AssetPath;
		FString GraphName;
	};

	static bool TryReadStepPayloadParts(
		const TSharedPtr<FJsonObject>& StepObject,
		FTaskPlanStepPayloadParts& OutParts,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutErrorCode = TEXT("invalid_taskplan_step");
			OutErrorMessage = TEXT("TaskPlan step 缺少 target 对象。");
			OutErrorField = TEXT("task_plan.steps[0].target");
			return false;
		}
		if (!StepObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) ||
			!ArgsObjectPtr || !ArgsObjectPtr->IsValid())
		{
			OutErrorCode = TEXT("invalid_taskplan_step");
			OutErrorMessage = TEXT("TaskPlan step 缺少 args 对象。");
			OutErrorField = TEXT("task_plan.steps[0].args");
			return false;
		}

		(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutParts.AssetPath);
		(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), OutParts.GraphName);
		if (OutParts.AssetPath.IsEmpty() || OutParts.GraphName.IsEmpty())
		{
			OutErrorCode = TEXT("invalid_taskplan_step_target");
			OutErrorMessage = TEXT("TaskPlan step target 需。asset_path 。graph。");
			OutErrorField = TEXT("task_plan.steps[0].target");
			return false;
		}

		OutParts.TargetObject = *TargetObjectPtr;
		OutParts.ArgsObject = *ArgsObjectPtr;
		return true;
	}

	static TSharedRef<FJsonObject> BuildBridgeTargetPayload(const FTaskPlanStepPayloadParts& Parts)
	{
		TSharedRef<FJsonObject> BridgeTarget = MakeShared<FJsonObject>();
		CopyObjectFields(Parts.TargetObject, BridgeTarget);
		BridgeTarget->SetStringField(TEXT("asset_path"), Parts.AssetPath);
		BridgeTarget->SetStringField(TEXT("graph"), Parts.GraphName);
		return BridgeTarget;
	}

	static bool HasExecutionPolicyValidationFields(const TSharedPtr<FJsonObject>& TaskPlan)
	{
		const FBlueprintHelperTaskRuntimeExecutionPolicySettings SettingsPolicy =
			FBlueprintHelperTaskRuntimeSettingsResolver::LoadExecutionPolicy();
		if (SettingsPolicy.bShouldCompile || SettingsPolicy.bShouldSave)
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		if (!TaskPlan.IsValid() ||
			!TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) ||
			!ExecutionPolicyPtr || !ExecutionPolicyPtr->IsValid())
		{
			return false;
		}

		bool bIgnored = false;
		return (*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_compile"), bIgnored) ||
			(*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_save"), bIgnored);
	}

	static bool TryReadExecutionPolicyBool(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TCHAR* FieldName,
		bool& OutValue)
	{
		OutValue = false;
		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		return TaskPlan.IsValid() &&
			TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) &&
			ExecutionPolicyPtr && ExecutionPolicyPtr->IsValid() &&
			(*ExecutionPolicyPtr)->TryGetBoolField(FieldName, OutValue);
	}

	static bool TryReadReviewBaselineDirtyAssetPolicy(
		const TSharedPtr<FJsonObject>& TaskPlan,
		EBlueprintHelperReviewBaselineDirtyAssetPolicy& OutPolicy,
		FString& OutPolicyString,
		FBlueprintHelperToolError& OutError)
	{
		OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block;
		OutPolicyString = TEXT("block");

		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		if (!TaskPlan.IsValid()
			|| !TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr)
			|| !ExecutionPolicyPtr
			|| !ExecutionPolicyPtr->IsValid())
		{
			return true;
		}

		FString RawPolicy;
		if (!(*ExecutionPolicyPtr)->TryGetStringField(TEXT("review_baseline_dirty_asset_policy"), RawPolicy)
			|| RawPolicy.IsEmpty())
		{
			return true;
		}

		RawPolicy.TrimStartAndEndInline();
		if (RawPolicy.Equals(TEXT("block"), ESearchCase::IgnoreCase))
		{
			OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block;
			OutPolicyString = TEXT("block");
			return true;
		}
		if (RawPolicy.Equals(TEXT("save_before_archive"), ESearchCase::IgnoreCase))
		{
			OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::SaveBeforeArchive;
			OutPolicyString = TEXT("save_before_archive");
			return true;
		}
		if (RawPolicy.Equals(TEXT("allow_stale_disk_snapshot"), ESearchCase::IgnoreCase))
		{
			OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::AllowStaleDiskSnapshot;
			OutPolicyString = TEXT("allow_stale_disk_snapshot");
			return true;
		}

		OutError = MakeTaskRuntimeError(
			TEXT("invalid_taskplan_execution_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("execution_policy.review_baseline_dirty_asset_policy must be block, save_before_archive, or allow_stale_disk_snapshot."),
			TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
		OutError.Actual = RawPolicy;
		return false;
	}

	static TArray<FString> ReadTargetAssets(const TSharedPtr<FJsonObject>& TaskPlan)
	{
		TArray<FString> Assets;
		const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
		if (!TaskPlan.IsValid() ||
			!TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) ||
			!TargetAssets)
		{
			return Assets;
		}

		for (const TSharedPtr<FJsonValue>& AssetValue : *TargetAssets)
		{
			if (!AssetValue.IsValid() || AssetValue->Type != EJson::String)
			{
				continue;
			}
			const FString AssetPath = AssetValue->AsString();
			if (!AssetPath.IsEmpty())
			{
				Assets.AddUnique(AssetPath);
			}
		}
		return Assets;
	}

	static TArray<FString> FindDirtyTargetAssets(const TArray<FString>& TargetAssets)
	{
		TArray<FString> DirtyAssets;
		for (const FString& AssetPath : TargetAssets)
		{
			if (AssetPath.IsEmpty())
			{
				continue;
			}

			UObject* Asset = StaticFindObject(UObject::StaticClass(), nullptr, *AssetPath);
			if (!Asset)
			{
				Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
			}
			if (!Asset)
			{
				continue;
			}

			UPackage* Package = Asset->GetOutermost();
			if (Package && Package->IsDirty())
			{
				DirtyAssets.AddUnique(AssetPath);
			}
		}
		return DirtyAssets;
	}

	static void ApplyReviewBaselineDirtyDecisionToError(
		const FBlueprintHelperReviewBaselineDirtyDecision& Decision,
		FBlueprintHelperToolError& Error)
	{
		Error.Category = Decision.Category;
		Error.SafeNextAction = Decision.SafeNextAction;
		Error.DirtyState = ToString(Decision.State);
		Error.DirtyAssets = Decision.DirtyAssets;
		Error.AllowedRecoveryActions = Decision.AllowedRecoveryActions;
		Error.RiskyRecoveryActions = Decision.RiskyRecoveryActions;
		Error.EvidenceRefs = Decision.EvidenceRefs;
	}

	static bool EvaluateReviewBaselinePolicy(
		const TSharedPtr<FJsonObject>& TaskPlan,
		bool bDryRun,
		const FBlueprintHelperAssetBrowseService& AssetBrowseService,
		const TMap<FString, TSharedPtr<FJsonObject>>& TaskRunJournals,
		FBlueprintHelperReviewBaselinePolicyEvaluation& OutEvaluation,
		FBlueprintHelperToolError& OutError)
	{
		OutEvaluation = FBlueprintHelperReviewBaselinePolicyEvaluation();
		if (!TryReadReviewBaselineDirtyAssetPolicy(
			TaskPlan,
			OutEvaluation.Policy,
			OutEvaluation.PolicyString,
			OutError))
		{
			return false;
		}

		const TArray<FString> TargetAssets = ReadTargetAssets(TaskPlan);
		OutEvaluation.DirtyTargetAssets = FindDirtyTargetAssets(TargetAssets);
		const FBlueprintHelperReviewBaselineDirtyClassifyRequest DirtyRequest =
			FBlueprintHelperReviewBaselineDirtyEvidenceProvider().BuildClassifyRequest(
				TargetAssets,
				OutEvaluation.DirtyTargetAssets,
				TaskRunJournals);
		OutEvaluation.DirtyDecision = FBlueprintHelperReviewBaselineDirtyClassifier().Classify(DirtyRequest);
		if (OutEvaluation.DirtyTargetAssets.Num() == 0)
		{
			OutEvaluation.SnapshotTrust = TEXT("fresh_disk_copy");
			return true;
		}

		if (OutEvaluation.Policy == EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block)
		{
			OutError = MakeTaskRuntimeError(
				TEXT("review_baseline_dirty_target_assets"),
				EBlueprintHelperToolStage::Preflight,
				TEXT("Review baseline requires saved target assets or review_baseline_dirty_asset_policy=save_before_archive."),
				TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
			OutError.Actual = FString::Join(OutEvaluation.DirtyTargetAssets, TEXT(","));
			ApplyReviewBaselineDirtyDecisionToError(OutEvaluation.DirtyDecision, OutError);
			return false;
		}

		if (OutEvaluation.Policy == EBlueprintHelperReviewBaselineDirtyAssetPolicy::SaveBeforeArchive)
		{
			OutEvaluation.SnapshotTrust = TEXT("saved_before_archive");
			const TArray<FString> OriginallyDirtyTargetAssets = OutEvaluation.DirtyTargetAssets;
			if (bDryRun)
			{
				OutEvaluation.Warnings.Add(TEXT("review_baseline_dirty_targets_would_save_before_archive"));
				return true;
			}

			for (const FString& DirtyAsset : OutEvaluation.DirtyTargetAssets)
			{
				const FBlueprintHelperToolResultBase SaveResult = MakeSaveAssetToolResult(AssetBrowseService, DirtyAsset);
				OutEvaluation.PreArchiveSaveOperations.Add({TEXT("save_before_archive"), SaveResult});
				if (!SaveResult.bOk)
				{
					OutError = SaveResult.Error.IsSet()
						? *SaveResult.Error
						: MakeTaskRuntimeError(
							TEXT("review_baseline_save_before_archive_failed"),
							EBlueprintHelperToolStage::Preflight,
							FString::Printf(TEXT("Failed to save dirty target before archive: %s"), *DirtyAsset),
							TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
					OutError.Code = TEXT("review_baseline_save_before_archive_failed");
					OutError.Stage = EBlueprintHelperToolStage::Preflight;
					OutError.Field = TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy");
					ApplyReviewBaselineDirtyDecisionToError(OutEvaluation.DirtyDecision, OutError);
					return false;
				}
				OutEvaluation.SavedBeforeArchiveAssets.Add(DirtyAsset);
			}
			const TArray<FString> RemainingDirtyTargetAssets = FindDirtyTargetAssets(TargetAssets);
			if (RemainingDirtyTargetAssets.Num() > 0)
			{
				OutError = MakeTaskRuntimeError(
					TEXT("review_baseline_save_before_archive_failed"),
					EBlueprintHelperToolStage::Preflight,
					TEXT("One or more target assets remained dirty after save_before_archive."),
					TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
				OutError.Actual = FString::Join(RemainingDirtyTargetAssets, TEXT(","));
				ApplyReviewBaselineDirtyDecisionToError(OutEvaluation.DirtyDecision, OutError);
				return false;
			}
			OutEvaluation.DirtyTargetAssets = OriginallyDirtyTargetAssets;
			return true;
		}

		OutEvaluation.SnapshotTrust = TEXT("stale_disk_copy");
		OutEvaluation.Warnings.Add(TEXT("Review baseline snapshot copied from disk while target asset was dirty in editor. Snapshot is diagnostic evidence only and must not be used as authoritative rollback baseline."));
		return true;
	}

	static FBlueprintHelperToolResultBase MakeSaveAssetToolResult(
		const FBlueprintHelperAssetBrowseService& Service,
		const FString& AssetPath)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);

		const FBlueprintHelperSaveResult SaveResult = Service.SaveAsset(AssetPath);
		if (!SaveResult.bSuccess)
		{
			FBlueprintHelperToolError Error;
			Error.Code = TEXT("save_failed");
			Error.Stage = EBlueprintHelperToolStage::Execute;
			Error.Message = SaveResult.ErrorMessage;
			Error.bRetryable = true;

			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("save_asset"),
				TraceId,
				Error);
			Result.CustomTargetJson = Target;
			return Result;
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
			TEXT("save_asset"),
			TraceId);
		Result.bModified = false;
		Result.CustomTargetJson = Target;

		FBlueprintHelperSaveAssetResultData Data;
		Data.SaveResult.bSaved = true;
		Data.SaveResult.bWasDirty = true;
		Result.Data = Data.ToJson();
		return Result;
	}

	static TSharedRef<FJsonObject> BuildAppendPayload(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		FTaskPlanStepPayloadParts Parts;
		if (!TryReadStepPayloadParts(StepObject, Parts, OutErrorCode, OutErrorMessage, OutErrorField))
		{
			return MakeShared<FJsonObject>();
		}

		TSharedRef<FJsonObject> AppendPayload = MakeShared<FJsonObject>();
		AppendPayload->SetObjectField(TEXT("target"), BuildBridgeTargetPayload(Parts));
		AppendPayload->SetBoolField(TEXT("dry_run"), bDryRun);

		FString FeatureName;
		if (Parts.ArgsObject->TryGetStringField(TEXT("feature_name"), FeatureName) && !FeatureName.IsEmpty())
		{
			AppendPayload->SetStringField(TEXT("feature_name"), FeatureName);
		}

		AppendPayload->SetArrayField(TEXT("nodes"), CopyArrayField(Parts.ArgsObject, TEXT("nodes")));
		AppendPayload->SetArrayField(TEXT("links"), CopyArrayField(Parts.ArgsObject, TEXT("links")));
		return AppendPayload;
	}

	static TSharedRef<FJsonObject> BuildGraphWritePayload(
		const FString& StepOperation,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		if (StepOperation == TEXT("append_blueprint_graph"))
		{
			return BuildAppendPayload(StepObject, bDryRun, OutErrorCode, OutErrorMessage, OutErrorField);
		}

		FTaskPlanStepPayloadParts Parts;
		if (!TryReadStepPayloadParts(StepObject, Parts, OutErrorCode, OutErrorMessage, OutErrorField))
		{
			return MakeShared<FJsonObject>();
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		CopyObjectFields(Parts.ArgsObject, Payload);
		Payload->SetObjectField(TEXT("target"), BuildBridgeTargetPayload(Parts));

		if (StepOperation == TEXT("replace_blueprint_graph"))
		{
			TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
			const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
			if (Payload->TryGetObjectField(TEXT("options"), OptionsObject) &&
				OptionsObject && OptionsObject->IsValid())
			{
				CopyObjectFields(*OptionsObject, Options);
			}
			Options->SetBoolField(TEXT("dry_run"), bDryRun);
			Payload->SetObjectField(TEXT("options"), Options);
		}
		else
		{
			Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		}

		return Payload;
	}

	static TSharedRef<FJsonObject> MakePostOperationResultJson(
		const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation)
	{
		TSharedRef<FJsonObject> PostJson = MakeShared<FJsonObject>();
		PostJson->SetStringField(TEXT("operation"), PostOperation.Operation);
		PostJson->SetStringField(TEXT("status"), ToolStatusToString(PostOperation.Result.Status));
		if (!PostOperation.AssetPath.IsEmpty())
		{
			PostJson->SetStringField(TEXT("asset_path"), PostOperation.AssetPath);
		}
		if (!PostOperation.Status.IsEmpty())
		{
			PostJson->SetStringField(TEXT("post_status"), PostOperation.Status);
		}
		if (!PostOperation.Reason.IsEmpty())
		{
			PostJson->SetStringField(TEXT("reason"), PostOperation.Reason);
		}
		PostJson->SetNumberField(TEXT("duration_ms"), PostOperation.DurationMs);
		PostJson->SetObjectField(TEXT("result"), PostOperation.Result.ToJson());
		return PostJson;
	}

	static bool HasFailedStep(
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords)
	{
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			if (!StepRecord.Result.bOk)
			{
				return true;
			}
		}
		return false;
	}

	static bool HasFailedPostOperation(
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords)
	{
		for (const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation : PostOperationRecords)
		{
			if (!PostOperation.Result.bOk)
			{
				return true;
			}
		}
		return false;
	}

	enum class EBlueprintHelperTaskJournalStepStatus : uint8
	{
		Completed,
		Failed,
		Blocked,
		Skipped
	};

	static const TCHAR* TaskJournalStepStatusToString(EBlueprintHelperTaskJournalStepStatus Status)
	{
		switch (Status)
		{
		case EBlueprintHelperTaskJournalStepStatus::Completed:
			return TEXT("completed");
		case EBlueprintHelperTaskJournalStepStatus::Failed:
			return TEXT("failed");
		case EBlueprintHelperTaskJournalStepStatus::Blocked:
			return TEXT("blocked");
		case EBlueprintHelperTaskJournalStepStatus::Skipped:
			return TEXT("skipped");
		default:
			return TEXT("unknown");
		}
	}

	static TArray<FString> ReadStepDependsOn(const TSharedPtr<FJsonObject>& StepObject)
	{
		TArray<FString> DependsOn;
		const TArray<TSharedPtr<FJsonValue>>* DependsOnValues = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetArrayField(TEXT("depends_on"), DependsOnValues) ||
			!DependsOnValues)
		{
			return DependsOn;
		}

		for (const TSharedPtr<FJsonValue>& DependsOnValue : *DependsOnValues)
		{
			if (!DependsOnValue.IsValid())
			{
				continue;
			}

			const FString StepId = DependsOnValue->AsString();
			if (!StepId.IsEmpty())
			{
				DependsOn.Add(StepId);
			}
		}

		return DependsOn;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}

	static TSharedPtr<FJsonObject> MakeReviewBaselinePolicyJson(
		const FBlueprintHelperReviewBaselinePolicyEvaluation& BaselinePolicy)
	{
		TSharedPtr<FJsonObject> Baseline = MakeShared<FJsonObject>();
		Baseline->SetStringField(TEXT("dirty_asset_policy"), BaselinePolicy.PolicyString);
		Baseline->SetStringField(TEXT("snapshot_trust"), BaselinePolicy.SnapshotTrust);
		Baseline->SetStringField(TEXT("dirty_state"), ToString(BaselinePolicy.DirtyDecision.State));
		if (!BaselinePolicy.DirtyDecision.SafeNextAction.IsEmpty())
		{
			Baseline->SetStringField(TEXT("safe_next_action"), BaselinePolicy.DirtyDecision.SafeNextAction);
		}
		Baseline->SetArrayField(TEXT("dirty_target_assets"), MakeStringArray(BaselinePolicy.DirtyTargetAssets));
		Baseline->SetArrayField(TEXT("saved_before_archive_assets"), MakeStringArray(BaselinePolicy.SavedBeforeArchiveAssets));
		Baseline->SetArrayField(TEXT("allowed_recovery_actions"), MakeStringArray(BaselinePolicy.DirtyDecision.AllowedRecoveryActions));
		Baseline->SetArrayField(TEXT("risky_recovery_actions"), MakeStringArray(BaselinePolicy.DirtyDecision.RiskyRecoveryActions));
		Baseline->SetArrayField(TEXT("dirty_evidence_refs"), MakeStringArray(BaselinePolicy.DirtyDecision.EvidenceRefs));
		Baseline->SetArrayField(TEXT("warnings"), MakeStringArray(BaselinePolicy.Warnings));

		TArray<TSharedPtr<FJsonValue>> PreArchiveSaveOperations;
		for (const FBlueprintHelperTaskRuntimePostOperationRecord& Operation : BaselinePolicy.PreArchiveSaveOperations)
		{
			TSharedRef<FJsonObject> OperationJson = MakeShared<FJsonObject>();
			OperationJson->SetStringField(TEXT("operation"), Operation.Operation);
			OperationJson->SetStringField(TEXT("status"), Operation.Result.bOk ? TEXT("ok") : TEXT("failed"));
			if (Operation.Result.Target.IsSet())
			{
				OperationJson->SetObjectField(TEXT("target"), Operation.Result.Target->ToJson());
			}
			if (Operation.Result.Error.IsSet())
			{
				OperationJson->SetStringField(TEXT("error_code"), Operation.Result.Error->Code);
				OperationJson->SetStringField(TEXT("error_message"), Operation.Result.Error->Message);
			}
			PreArchiveSaveOperations.Add(MakeShared<FJsonValueObject>(OperationJson));
		}
		Baseline->SetArrayField(TEXT("pre_archive_save_operations"), PreArchiveSaveOperations);
		return Baseline;
	}

	static FString GetTaskPlanStepId(const TSharedPtr<FJsonObject>& StepObject, int32 StepIndex)
	{
		FString StepId;
		if (StepObject.IsValid() && StepObject->TryGetStringField(TEXT("step_id"), StepId) && !StepId.IsEmpty())
		{
			return StepId;
		}

		return FString::Printf(TEXT("step_%03d"), StepIndex + 1);
	}

	static FString GetTaskPlanStepOperation(const TSharedPtr<FJsonObject>& StepObject)
	{
		FString Capability;
		if (StepObject.IsValid() &&
			StepObject->TryGetStringField(TEXT("capability"), Capability) &&
			!Capability.IsEmpty())
		{
			return Capability;
		}

		FString Operation;
		if (StepObject.IsValid() &&
			StepObject->TryGetStringField(TEXT("operation"), Operation) &&
			!Operation.IsEmpty())
		{
			return Operation;
		}

		return TEXT("unknown");
	}

	static TSharedRef<FJsonObject> MakeTaskJournalRecoveryJson()
	{
		TSharedRef<FJsonObject> Recovery = MakeShared<FJsonObject>();
		Recovery->SetStringField(
			TEXT("recommended_action"),
			TEXT("inspect_task_result_then_submit_followup_taskspec"));
		Recovery->SetBoolField(TEXT("safe_to_retry"), false);
		Recovery->SetBoolField(TEXT("rollback_available"), false);
		TArray<TSharedPtr<FJsonValue>> Notes;
		Notes.Add(MakeShared<FJsonValueString>(
			TEXT("TaskRuntime does not perform global rollback after partial failure.")));
		Notes.Add(MakeShared<FJsonValueString>(
			TEXT("Review failed and blocked steps, then submit a follow-up TaskSpec for the remaining intended changes.")));
		Recovery->SetArrayField(TEXT("notes"), Notes);
		return Recovery;
	}

	static TSharedRef<FJsonObject> MakeTaskJournalStepJson(
		const TSharedPtr<FJsonObject>& PlannedStep,
		const FBlueprintHelperTaskRuntimeStepRecord* ExecutedStep,
		const FString& StepId,
		EBlueprintHelperTaskJournalStepStatus StepStatus,
		const TArray<FString>& DependsOn,
		const TArray<FString>& BlockedByStepIds,
		const FString& BlockedReason)
	{
		TSharedRef<FJsonObject> StepJson = MakeShared<FJsonObject>();
		StepJson->SetStringField(TEXT("step_id"), StepId);

		FString Capability;
		if (ExecutedStep && !ExecutedStep->Step.Capability.IsEmpty())
		{
			Capability = ExecutedStep->Step.Capability;
		}
		else if (PlannedStep.IsValid())
		{
			PlannedStep->TryGetStringField(TEXT("capability"), Capability);
		}
		if (!Capability.IsEmpty())
		{
			StepJson->SetStringField(TEXT("capability"), Capability);
		}

		const FString Operation = ExecutedStep
			? (ExecutedStep->Step.RuntimeOperation.IsEmpty() ? GetTaskPlanStepOperation(PlannedStep) : ExecutedStep->Step.RuntimeOperation)
			: GetTaskPlanStepOperation(PlannedStep);
		StepJson->SetStringField(TEXT("operation"), Operation);

		if (ExecutedStep && !ExecutedStep->Step.AdapterOperation.IsEmpty())
		{
			StepJson->SetStringField(TEXT("adapter_operation"), ExecutedStep->Step.AdapterOperation);
		}

		if (DependsOn.Num() > 0)
		{
			StepJson->SetArrayField(TEXT("depends_on"), MakeStringArray(DependsOn));
		}

		StepJson->SetStringField(TEXT("status"), TaskJournalStepStatusToString(StepStatus));

		if (ExecutedStep)
		{
			StepJson->SetObjectField(TEXT("result"), ExecutedStep->Result.ToJson());
		}
		else
		{
			StepJson->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
		}

		if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Blocked)
		{
			StepJson->SetArrayField(TEXT("blocked_by_step_ids"), MakeStringArray(BlockedByStepIds));
			StepJson->SetStringField(TEXT("blocked_reason"), BlockedReason);
		}

		return StepJson;
	}

	static TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
		bool bRuntimeFailed = false,
		const TSharedPtr<FJsonObject>& ReviewBaseline = nullptr)
	{
		TSharedRef<FJsonObject> Journal = MakeShared<FJsonObject>();
		Journal->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRunJournal.v1"));
		Journal->SetStringField(TEXT("task_run_id"), TaskRunId);

		if (TaskPlan.IsValid())
		{
			FString TaskType;
			if (TaskPlan->TryGetStringField(TEXT("task_type"), TaskType))
			{
				Journal->SetStringField(TEXT("task_type"), TaskType);
			}
			FString FeatureName;
			if (TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName))
			{
				Journal->SetStringField(TEXT("feature_name"), FeatureName);
			}
			const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
			if (TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) && TargetAssets)
			{
				Journal->SetArrayField(TEXT("target_assets"), *TargetAssets);
			}
		}

		if (ReviewBaseline.IsValid())
		{
			Journal->SetObjectField(TEXT("review_baseline"), ReviewBaseline);
		}

		TMap<FString, const FBlueprintHelperTaskRuntimeStepRecord*> StepRecordsById;
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			StepRecordsById.Add(StepRecord.Step.StepId, &StepRecord);
		}

		TArray<TSharedPtr<FJsonValue>> Steps;
		TSet<FString> ConsumedStepIds;
		TMap<FString, EBlueprintHelperTaskJournalStepStatus> JournalStepStatuses;
		bool bHasBlockedJournalStep = false;
		bool bHasFailedJournalStep = false;
		const TArray<TSharedPtr<FJsonValue>>* PlannedSteps = nullptr;
		if (TaskPlan.IsValid() &&
			TaskPlan->TryGetArrayField(TEXT("steps"), PlannedSteps) &&
			PlannedSteps)
		{
			for (int32 StepIndex = 0; StepIndex < PlannedSteps->Num(); ++StepIndex)
			{
				const TSharedPtr<FJsonObject> PlannedStep = AsJsonObjectIfObject((*PlannedSteps)[StepIndex]);
				const FString StepId = GetTaskPlanStepId(PlannedStep, StepIndex);
				const TArray<FString> DependsOn = ReadStepDependsOn(PlannedStep);

				const FBlueprintHelperTaskRuntimeStepRecord* const* ExecutedStepPtr = StepRecordsById.Find(StepId);
				const FBlueprintHelperTaskRuntimeStepRecord* ExecutedStep = ExecutedStepPtr ? *ExecutedStepPtr : nullptr;

				EBlueprintHelperTaskJournalStepStatus StepStatus = EBlueprintHelperTaskJournalStepStatus::Skipped;
				TArray<FString> BlockedByStepIds;
				FString BlockedReason;
				if (ExecutedStep)
				{
					StepStatus = ExecutedStep->Result.bOk
						? EBlueprintHelperTaskJournalStepStatus::Completed
						: EBlueprintHelperTaskJournalStepStatus::Failed;
					ConsumedStepIds.Add(StepId);
				}
				else
				{
					bool bBlockedByFailedDependency = false;
					bool bBlockedByBlockedDependency = false;
					for (const FString& DependsOnStepId : DependsOn)
					{
						const EBlueprintHelperTaskJournalStepStatus* DependsOnStatus = JournalStepStatuses.Find(DependsOnStepId);
						if (!DependsOnStatus)
						{
							continue;
						}

						if (*DependsOnStatus == EBlueprintHelperTaskJournalStepStatus::Failed)
						{
							bBlockedByFailedDependency = true;
							BlockedByStepIds.Add(DependsOnStepId);
						}
						else if (*DependsOnStatus == EBlueprintHelperTaskJournalStepStatus::Blocked)
						{
							bBlockedByBlockedDependency = true;
							BlockedByStepIds.Add(DependsOnStepId);
						}
					}

					if (BlockedByStepIds.Num() > 0)
					{
						StepStatus = EBlueprintHelperTaskJournalStepStatus::Blocked;
						BlockedReason = bBlockedByFailedDependency
							? TEXT("dependency_failed")
							: TEXT("dependency_blocked");
					}
				}

				if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Failed)
				{
					bHasFailedJournalStep = true;
				}
				else if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Blocked)
				{
					bHasBlockedJournalStep = true;
				}

				JournalStepStatuses.Add(StepId, StepStatus);
				Steps.Add(MakeShared<FJsonValueObject>(MakeTaskJournalStepJson(
					PlannedStep,
					ExecutedStep,
					StepId,
					StepStatus,
					DependsOn,
					BlockedByStepIds,
					BlockedReason)));
			}
		}

		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			if (ConsumedStepIds.Contains(StepRecord.Step.StepId))
			{
				continue;
			}

			const EBlueprintHelperTaskJournalStepStatus StepStatus = StepRecord.Result.bOk
				? EBlueprintHelperTaskJournalStepStatus::Completed
				: EBlueprintHelperTaskJournalStepStatus::Failed;
			if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Failed)
			{
				bHasFailedJournalStep = true;
			}

			Steps.Add(MakeShared<FJsonValueObject>(MakeTaskJournalStepJson(
				nullptr,
				&StepRecord,
				StepRecord.Step.StepId,
				StepStatus,
				{},
				{},
				TEXT(""))));
		}
		Journal->SetArrayField(TEXT("steps"), Steps);

		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			const TSharedPtr<FJsonObject>* ContextRevisionPtr = nullptr;
			if (StepRecord.Result.Data.IsValid() &&
				StepRecord.Result.Data->TryGetObjectField(TEXT("current_context_revision"), ContextRevisionPtr) &&
				ContextRevisionPtr && ContextRevisionPtr->IsValid())
			{
				Journal->SetObjectField(TEXT("context_revision"), *ContextRevisionPtr);
				break;
			}
		}

		const bool bHasPartialFailure =
			bRuntimeFailed ||
			bHasFailedJournalStep ||
			bHasBlockedJournalStep ||
			HasFailedPostOperation(PostOperationRecords);
		Journal->SetStringField(
			TEXT("status"),
			bHasPartialFailure ? TEXT("partial_failure") : TEXT("completed"));
		if (bHasPartialFailure)
		{
			Journal->SetObjectField(TEXT("recovery"), MakeTaskJournalRecoveryJson());
		}

		if (PostOperationRecords.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> PostOperations;
			for (const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation : PostOperationRecords)
			{
				PostOperations.Add(MakeShared<FJsonValueObject>(MakePostOperationResultJson(PostOperation)));
			}
			Journal->SetArrayField(TEXT("post_operations"), PostOperations);
		}
		return Journal;
	}

	static TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult)
	{
		TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
		StepRecords.Add({LoweredStep, StepResult});
		return MakeTaskRunJournal(
			TaskRunId,
			TaskPlan,
			StepRecords,
			{});
	}

	static TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& StepId,
		const FString& StepOperation,
		const FBlueprintHelperToolResultBase& StepResult)
	{
		FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
		LoweredStep.StepId = StepId;
		LoweredStep.RuntimeOperation = StepOperation;
		LoweredStep.AdapterOperation = StepOperation;
		return MakeTaskRunJournal(TaskRunId, TaskPlan, LoweredStep, StepResult);
	}

	static TSharedRef<FJsonObject> MakeRuntimeTarget(
		const FString& AssetPath,
		const FString& TargetType,
		const FString& MemberName = TEXT(""),
		const FString& RowName = TEXT(""),
		const FString& PropertyPath = TEXT(""))
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		if (!AssetPath.IsEmpty())
		{
			Target->SetStringField(TEXT("asset_path"), AssetPath);
		}
		Target->SetStringField(TEXT("target_type"), TargetType);
		if (!MemberName.IsEmpty())
		{
			Target->SetStringField(TEXT("member_name"), MemberName);
		}
		if (!RowName.IsEmpty())
		{
			Target->SetStringField(TEXT("row_name"), RowName);
		}
		if (!PropertyPath.IsEmpty())
		{
			Target->SetStringField(TEXT("property_path"), PropertyPath);
		}
		return Target;
	}

	static FString MakePlannedComponentKey(
		const FString& AssetPath,
		const FString& ComponentName)
	{
		return FString::Printf(TEXT("%s\n%s"), *AssetPath, *ComponentName);
	}

	static FString MakePlannedWidgetKey(
		const FString& AssetPath,
		const FString& WidgetName)
	{
		return FString::Printf(TEXT("%s\n%s"), *AssetPath, *WidgetName);
	}

	struct FPlannedWidgetTreeState
	{
		FBlueprintHelperWidgetTreeSummary Summary;
		TMap<FString, FBlueprintHelperWidgetTreeItem> NamedSlotContentItemsByName;
		bool bInitialized = false;

		static FBlueprintHelperWidgetTreeItem MakeItem(
			const FString& WidgetName,
			const FString& WidgetClass,
			const FString& ParentName,
			const FString& SlotName,
			int32 VirtualIndex)
		{
			FBlueprintHelperWidgetTreeItem Item;
			Item.WidgetName = WidgetName;
			Item.WidgetClass = WidgetClass;
			Item.WidgetClassPath = ResolveWidgetClassPath(WidgetClass, FString());
			Item.ParentName = ParentName;
			Item.SlotName = SlotName;
			Item.VirtualIndex = VirtualIndex;
			Item.bIsVariable = true;
			Item.bIsInherited = false;
			return Item;
		}

		static UClass* ResolveWidgetClass(const FString& WidgetClass, const FString& WidgetClassPath)
		{
			if (!WidgetClassPath.IsEmpty())
			{
				if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *WidgetClassPath))
				{
					return LoadedClass;
				}
			}

			FString FullName = WidgetClass;
			if (!FullName.StartsWith(TEXT("U")))
			{
				FullName = TEXT("U") + FullName;
			}

			for (TObjectIterator<UClass> It; It; ++It)
			{
				UClass* Class = *It;
				if (Class &&
					Class->IsChildOf(UWidget::StaticClass()) &&
					!Class->HasAnyClassFlags(CLASS_Abstract) &&
					(Class->GetName() == FullName || Class->GetName() == WidgetClass))
				{
					return Class;
				}
			}
			return nullptr;
		}

		static FString ResolveWidgetClassPath(const FString& WidgetClass, const FString& WidgetClassPath)
		{
			if (UClass* Class = ResolveWidgetClass(WidgetClass, WidgetClassPath))
			{
				return Class->GetPathName();
			}
			return WidgetClassPath;
		}

		static FString NormalizeWidgetClassName(const FString& WidgetClass, const FString& WidgetClassPath)
		{
			if (UClass* Class = ResolveWidgetClass(WidgetClass, WidgetClassPath))
			{
				FString ClassName = Class->GetName();
				if (ClassName.StartsWith(TEXT("U")))
				{
					ClassName.RemoveFromStart(TEXT("U"));
				}
				return ClassName;
			}
			return WidgetClass;
		}

		static bool ClassSupportsNamedSlot(
			const FBlueprintHelperWidgetTreeItem& HostItem,
			const FString& SlotName)
		{
			UClass* Class = ResolveWidgetClass(HostItem.WidgetClass, HostItem.WidgetClassPath);
			if (!Class)
			{
				return false;
			}

			UWidget* ProbeWidget = NewObject<UWidget>(GetTransientPackage(), Class);
			INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(ProbeWidget);
			if (!NamedSlotHost)
			{
				return false;
			}

			TArray<FName> SlotNames;
			NamedSlotHost->GetSlotNames(SlotNames);
			return SlotNames.Contains(FName(*SlotName));
		}

		static bool ClassSupportsPanelChildren(const FBlueprintHelperWidgetTreeItem& Item)
		{
			UClass* Class = ResolveWidgetClass(Item.WidgetClass, Item.WidgetClassPath);
			return Class && Class->IsChildOf(UPanelWidget::StaticClass());
		}

		static FBlueprintHelperWidgetTreeItem* FindInChildren(
			TArray<FBlueprintHelperWidgetTreeItem>& Children,
			const FString& WidgetName)
		{
			for (FBlueprintHelperWidgetTreeItem& Child : Children)
			{
				if (Child.WidgetName == WidgetName)
				{
					return &Child;
				}
				if (FBlueprintHelperWidgetTreeItem* Found = FindInChildren(Child.Children, WidgetName))
				{
					return Found;
				}
			}
			return nullptr;
		}

		FBlueprintHelperWidgetTreeItem* FindItem(const FString& WidgetName)
		{
			if (!Summary.Root.WidgetName.IsEmpty() && Summary.Root.WidgetName == WidgetName)
			{
				return &Summary.Root;
			}
			if (FBlueprintHelperWidgetTreeItem* Found = FindInChildren(Summary.Root.Children, WidgetName))
			{
				return Found;
			}
			if (FBlueprintHelperWidgetTreeItem* NamedSlotContent = NamedSlotContentItemsByName.Find(WidgetName))
			{
				return NamedSlotContent;
			}
			for (TPair<FString, FBlueprintHelperWidgetTreeItem>& Pair : NamedSlotContentItemsByName)
			{
				if (FBlueprintHelperWidgetTreeItem* Found = FindInChildren(Pair.Value.Children, WidgetName))
				{
					return Found;
				}
			}
			return nullptr;
		}

		const FBlueprintHelperWidgetTreeItem* FindItem(const FString& WidgetName) const
		{
			return const_cast<FPlannedWidgetTreeState*>(this)->FindItem(WidgetName);
		}

		bool ContainsWidget(const FString& WidgetName) const
		{
			return FindItem(WidgetName) != nullptr;
		}

		static bool ContainsDescendant(
			const FBlueprintHelperWidgetTreeItem& Item,
			const FString& WidgetName)
		{
			for (const FBlueprintHelperWidgetTreeItem& Child : Item.Children)
			{
				if (Child.WidgetName == WidgetName || ContainsDescendant(Child, WidgetName))
				{
					return true;
				}
			}
			return false;
		}

		static bool ContainsSelfOrDescendant(
			const FBlueprintHelperWidgetTreeItem& Item,
			const FString& WidgetName)
		{
			return Item.WidgetName == WidgetName || ContainsDescendant(Item, WidgetName);
		}

		static void ReindexChildren(TArray<FBlueprintHelperWidgetTreeItem>& Children, const FString& ParentName)
		{
			for (int32 Index = 0; Index < Children.Num(); ++Index)
			{
				Children[Index].ParentName = ParentName;
				Children[Index].SlotName.Empty();
				Children[Index].VirtualIndex = Index;
				ReindexChildren(Children[Index].Children, Children[Index].WidgetName);
			}
		}

		static void BuildFlatIndex(
			const FBlueprintHelperWidgetTreeItem& Item,
			TMap<FString, FBlueprintHelperWidgetTreeItem>& OutIndex)
		{
			if (!Item.WidgetName.IsEmpty())
			{
				OutIndex.Add(Item.WidgetName, Item);
			}
			for (const FBlueprintHelperWidgetTreeItem& Child : Item.Children)
			{
				BuildFlatIndex(Child, OutIndex);
			}
		}

		void RebuildIndex()
		{
			Summary.Index.Reset();
			if (!Summary.Root.WidgetName.IsEmpty())
			{
				ReindexChildren(Summary.Root.Children, Summary.Root.WidgetName);
				BuildFlatIndex(Summary.Root, Summary.Index);
			}
			for (const TPair<FString, FBlueprintHelperWidgetTreeItem>& Pair : NamedSlotContentItemsByName)
			{
				BuildFlatIndex(Pair.Value, Summary.Index);
			}
		}

		static bool RemoveFromChildren(
			TArray<FBlueprintHelperWidgetTreeItem>& Children,
			const FString& WidgetName,
			FBlueprintHelperWidgetTreeItem& OutItem)
		{
			for (int32 Index = 0; Index < Children.Num(); ++Index)
			{
				if (Children[Index].WidgetName == WidgetName)
				{
					OutItem = Children[Index];
					Children.RemoveAt(Index);
					return true;
				}
				if (RemoveFromChildren(Children[Index].Children, WidgetName, OutItem))
				{
					return true;
				}
			}
			return false;
		}

		bool RemoveWidget(const FString& WidgetName, FBlueprintHelperWidgetTreeItem& OutItem)
		{
			if (NamedSlotContentItemsByName.RemoveAndCopyValue(WidgetName, OutItem))
			{
				Summary.NamedSlots.RemoveAll([&WidgetName](const FBlueprintHelperNamedSlotEntry& Entry)
				{
					return Entry.ContentWidgetName == WidgetName;
				});
				RebuildIndex();
				return true;
			}

			if (Summary.Root.WidgetName == WidgetName)
			{
				return false;
			}

			if (RemoveFromChildren(Summary.Root.Children, WidgetName, OutItem))
			{
				RebuildIndex();
				return true;
			}
			return false;
		}

		static bool RenameInChildren(
			TArray<FBlueprintHelperWidgetTreeItem>& Children,
			const FString& WidgetName,
			const FString& NewWidgetName)
		{
			for (FBlueprintHelperWidgetTreeItem& Child : Children)
			{
				if (Child.WidgetName == WidgetName)
				{
					Child.WidgetName = NewWidgetName;
					ReindexChildren(Child.Children, NewWidgetName);
					return true;
				}
				if (RenameInChildren(Child.Children, WidgetName, NewWidgetName))
				{
					return true;
				}
			}
			return false;
		}

		static FString ResolveMappedWidgetName(
			const FString& SourceName,
			const TSharedPtr<FJsonObject>& NameMapping)
		{
			FString MappedName;
			if (NameMapping.IsValid() &&
				NameMapping->TryGetStringField(SourceName, MappedName) &&
				!MappedName.IsEmpty())
			{
				return MappedName;
			}
			return SourceName + TEXT("_Copy");
		}

		FBlueprintHelperWidgetTreeItem CloneItemWithMapping(
			const FBlueprintHelperWidgetTreeItem& SourceItem,
			const TSharedPtr<FJsonObject>& NameMapping,
			const FString& ParentName,
			int32 VirtualIndex,
			bool& bOutValid,
			FString& OutError) const
		{
			bOutValid = false;
			FBlueprintHelperWidgetTreeItem Clone = SourceItem;
			Clone.WidgetName = ResolveMappedWidgetName(SourceItem.WidgetName, NameMapping);
			if (Clone.WidgetName.IsEmpty())
			{
				OutError = TEXT("clone_widget_name_empty");
				return Clone;
			}
			if (ContainsWidget(Clone.WidgetName))
			{
				OutError = FString::Printf(TEXT("widget_name_mapping_conflict:%s"), *Clone.WidgetName);
				return Clone;
			}
			Clone.ParentName = ParentName;
			Clone.SlotName.Empty();
			Clone.VirtualIndex = VirtualIndex;
			for (int32 ChildIndex = 0; ChildIndex < Clone.Children.Num(); ++ChildIndex)
			{
				bool bChildValid = false;
				Clone.Children[ChildIndex] = CloneItemWithMapping(
					Clone.Children[ChildIndex],
					NameMapping,
					Clone.WidgetName,
					ChildIndex,
					bChildValid,
					OutError);
				if (!bChildValid)
				{
					return Clone;
				}
			}
			bOutValid = true;
			return Clone;
		}

		bool AttachItemToTarget(
			FBlueprintHelperWidgetTreeItem&& Item,
			const FString& TargetParentName,
			const FString& SlotName,
			TOptional<int32> VirtualIndexValue,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			if (TargetParentName.IsEmpty())
			{
				OutError = TEXT("target_parent_name_required");
				return false;
			}

			FBlueprintHelperWidgetTreeItem* ParentItem = FindItem(TargetParentName);
			if (!ParentItem)
			{
				OutError = FString::Printf(TEXT("target_parent_not_found:%s"), *TargetParentName);
				return false;
			}

			if (!SlotName.IsEmpty())
			{
				if (!ClassSupportsNamedSlot(*ParentItem, SlotName))
				{
					OutError = TEXT("named_slot_not_found");
					return false;
				}
				for (const FBlueprintHelperNamedSlotEntry& Entry : Summary.NamedSlots)
				{
					if (Entry.HostWidgetName == TargetParentName &&
						Entry.SlotName == SlotName &&
						!Entry.ContentWidgetName.IsEmpty())
					{
						OutError = TEXT("named_slot_content_exists");
						return false;
					}
				}
				Item.ParentName = TargetParentName;
				Item.SlotName = SlotName;
				Item.VirtualIndex = 0;
				const FString ItemName = Item.WidgetName;
				NamedSlotContentItemsByName.Add(ItemName, MoveTemp(Item));
				FBlueprintHelperNamedSlotEntry Entry;
				Entry.HostWidgetName = TargetParentName;
				Entry.SlotName = SlotName;
				Entry.ContentWidgetName = ItemName;
				Entry.VirtualIndex = 0;
				Summary.NamedSlots.Add(Entry);
				OutAffectedWidget = ItemName;
				RebuildIndex();
				return true;
			}

			if (!ClassSupportsPanelChildren(*ParentItem))
			{
				OutError = TEXT("target_parent_is_not_panel");
				return false;
			}
			const int32 VirtualIndex = FBlueprintHelperWidgetTreePositionPolicy::NormalizePanelVirtualIndex(VirtualIndexValue);
			if (VirtualIndex < 0 || VirtualIndex > ParentItem->Children.Num())
			{
				OutError = TEXT("invalid_virtual_index");
				return false;
			}
			Item.ParentName = ParentItem->WidgetName;
			Item.SlotName.Empty();
			Item.VirtualIndex = VirtualIndex;
			const FString ItemName = Item.WidgetName;
			ParentItem->Children.Insert(MoveTemp(Item), VirtualIndex);
			OutAffectedWidget = ItemName;
			RebuildIndex();
			return true;
		}

		bool ApplyAdd(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString ParentName;
			FString SlotName;
			FString WidgetClass;
			FString WidgetName;
			FString ExpectedParentName;
			TOptional<int32> VirtualIndexValue;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("parent_name"), ParentName);
				Payload->TryGetStringField(TEXT("slot_name"), SlotName);
				Payload->TryGetStringField(TEXT("widget_class"), WidgetClass);
				Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
				Payload->TryGetStringField(TEXT("expected_parent_name"), ExpectedParentName);
				double NumberValue = 0.0;
				if (Payload->TryGetNumberField(TEXT("virtual_index"), NumberValue))
				{
					VirtualIndexValue = FMath::RoundToInt(NumberValue);
				}
			}

			if (!ExpectedParentName.IsEmpty() && ExpectedParentName != ParentName)
			{
				OutError = TEXT("widget_expected_parent_mismatch");
				return false;
			}
			if (WidgetName.IsEmpty() || WidgetClass.IsEmpty())
			{
				OutError = TEXT("invalid_widget_add_payload");
				return false;
			}
			if (ContainsWidget(WidgetName))
			{
				OutError = TEXT("widget_name_already_exists");
				return false;
			}
			if (!SlotName.IsEmpty())
			{
				return ApplySetNamedSlotContent(Payload, OutAffectedWidget, OutError, false);
			}

			const int32 VirtualIndex = FBlueprintHelperWidgetTreePositionPolicy::NormalizePanelVirtualIndex(VirtualIndexValue);
			FBlueprintHelperWidgetTreeItem NewItem = MakeItem(
				WidgetName,
				NormalizeWidgetClassName(WidgetClass, FString()),
				ParentName,
				FString(),
				VirtualIndex);

			if (ParentName.IsEmpty() && Summary.Root.WidgetName.IsEmpty())
			{
				NewItem.VirtualIndex = 0;
				Summary.Root = MoveTemp(NewItem);
				OutAffectedWidget = WidgetName;
				RebuildIndex();
				return true;
			}

			FBlueprintHelperWidgetTreeItem* ParentItem = ParentName.IsEmpty()
				? (!Summary.Root.WidgetName.IsEmpty() ? &Summary.Root : nullptr)
				: FindItem(ParentName);
			if (!ParentItem)
			{
				OutError = FString::Printf(TEXT("Parent widget '%s' was not found."), *ParentName);
				return false;
			}
			if (ParentName.IsEmpty() && !ClassSupportsPanelChildren(*ParentItem))
			{
				OutError = TEXT("Root widget is not a panel widget; specify an explicit parent_name.");
				return false;
			}

			if (VirtualIndex < 0 || VirtualIndex > ParentItem->Children.Num())
			{
				OutError = TEXT("invalid_virtual_index");
				return false;
			}

			NewItem.ParentName = ParentItem->WidgetName;
			ParentItem->Children.Insert(MoveTemp(NewItem), VirtualIndex);
			OutAffectedWidget = WidgetName;
			RebuildIndex();
			return true;
		}

		bool ApplyMove(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString WidgetName;
			FString NewParentName;
			FString SlotName;
			FString ExpectedParentName;
			TOptional<int32> VirtualIndexValue;
			TOptional<int32> ExpectedVirtualIndex;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
				Payload->TryGetStringField(TEXT("new_parent_name"), NewParentName);
				Payload->TryGetStringField(TEXT("slot_name"), SlotName);
				Payload->TryGetStringField(TEXT("expected_parent_name"), ExpectedParentName);
				double NumberValue = 0.0;
				if (Payload->TryGetNumberField(TEXT("virtual_index"), NumberValue))
				{
					VirtualIndexValue = FMath::RoundToInt(NumberValue);
				}
				if (Payload->TryGetNumberField(TEXT("expected_virtual_index"), NumberValue))
				{
					ExpectedVirtualIndex = FMath::RoundToInt(NumberValue);
				}
			}

			FString ErrorCode;
			FString ErrorMessage;
			if (!FBlueprintHelperWidgetTreePositionPolicy::ValidateExpectedPosition(
				Summary,
				WidgetName,
				ExpectedParentName,
				ExpectedVirtualIndex,
				ErrorCode,
				ErrorMessage))
			{
				OutError = ErrorCode.IsEmpty() ? ErrorMessage : ErrorCode;
				return false;
			}

			if (!SlotName.IsEmpty())
			{
				return ApplyMoveToNamedSlot(
					WidgetName,
					NewParentName,
					SlotName,
					VirtualIndexValue,
					OutAffectedWidget,
					OutError);
			}

			FBlueprintHelperWidgetTreeItem* MovingItemPtr = FindItem(WidgetName);
			FBlueprintHelperWidgetTreeItem* NewParentItem = FindItem(NewParentName);
			if (!MovingItemPtr)
			{
				OutError = TEXT("widget_not_found");
				return false;
			}
			if (!NewParentItem)
			{
				OutError = FString::Printf(TEXT("Widget '%s' does not exist in the planned WidgetTree."), *NewParentName);
				return false;
			}
			if (ContainsDescendant(*MovingItemPtr, NewParentName))
			{
				OutError = TEXT("Cannot move widget into its own subtree.");
				return false;
			}

			const int32 VirtualIndex = FBlueprintHelperWidgetTreePositionPolicy::NormalizePanelVirtualIndex(VirtualIndexValue);
			if (VirtualIndex < 0 || VirtualIndex > NewParentItem->Children.Num())
			{
				OutError = TEXT("invalid_virtual_index");
				return false;
			}

			FBlueprintHelperWidgetTreeItem MovingItem;
			if (!RemoveWidget(WidgetName, MovingItem))
			{
				OutError = TEXT("widget_not_found");
				return false;
			}

			NewParentItem = FindItem(NewParentName);
			if (!NewParentItem)
			{
				OutError = TEXT("planned_parent_lost_after_move");
				return false;
			}

			MovingItem.ParentName = NewParentItem->WidgetName;
			MovingItem.SlotName.Empty();
			MovingItem.VirtualIndex = VirtualIndex;
			NewParentItem->Children.Insert(MoveTemp(MovingItem), VirtualIndex);
			OutAffectedWidget = WidgetName;
			RebuildIndex();
			return true;
		}

		bool ApplyMoveToNamedSlot(
			const FString& WidgetName,
			const FString& HostWidgetName,
			const FString& SlotName,
			TOptional<int32> VirtualIndexValue,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			if (WidgetName.IsEmpty() || HostWidgetName.IsEmpty() || SlotName.IsEmpty())
			{
				OutError = TEXT("invalid_named_slot_move_payload");
				return false;
			}

			FBlueprintHelperWidgetTreeItem* MovingItemPtr = FindItem(WidgetName);
			FBlueprintHelperWidgetTreeItem* HostItem = FindItem(HostWidgetName);
			if (!MovingItemPtr)
			{
				OutError = TEXT("widget_not_found");
				return false;
			}
			if (!HostItem)
			{
				OutError = FString::Printf(TEXT("Widget '%s' does not exist in the planned WidgetTree."), *HostWidgetName);
				return false;
			}
			if (WidgetName == HostWidgetName || ContainsDescendant(*MovingItemPtr, HostWidgetName))
			{
				OutError = TEXT("Cannot move widget into its own subtree.");
				return false;
			}
			if (!ClassSupportsNamedSlot(*HostItem, SlotName))
			{
				OutError = TEXT("named_slot_not_found");
				return false;
			}

			const int32 VirtualIndex = FBlueprintHelperWidgetTreePositionPolicy::NormalizeNamedSlotVirtualIndex(VirtualIndexValue);
			if (VirtualIndex != 0)
			{
				OutError = TEXT("invalid_virtual_index");
				return false;
			}

			FString ExistingContentName;
			for (const FBlueprintHelperNamedSlotEntry& Entry : Summary.NamedSlots)
			{
				if (Entry.HostWidgetName == HostWidgetName && Entry.SlotName == SlotName)
				{
					ExistingContentName = Entry.ContentWidgetName;
					break;
				}
			}
			if (!ExistingContentName.IsEmpty() && ExistingContentName != WidgetName)
			{
				OutError = TEXT("named_slot_content_exists");
				return false;
			}

			FBlueprintHelperWidgetTreeItem MovingItem;
			if (!RemoveWidget(WidgetName, MovingItem))
			{
				OutError = TEXT("widget_not_found");
				return false;
			}

			HostItem = FindItem(HostWidgetName);
			if (!HostItem)
			{
				OutError = TEXT("planned_parent_lost_after_move");
				return false;
			}

			MovingItem.ParentName = HostItem->WidgetName;
			MovingItem.SlotName = SlotName;
			MovingItem.VirtualIndex = 0;
			NamedSlotContentItemsByName.Add(WidgetName, MovingItem);

			FBlueprintHelperNamedSlotEntry* TargetSlot = nullptr;
			for (FBlueprintHelperNamedSlotEntry& Entry : Summary.NamedSlots)
			{
				if (Entry.HostWidgetName == HostWidgetName && Entry.SlotName == SlotName)
				{
					TargetSlot = &Entry;
					break;
				}
			}
			if (TargetSlot)
			{
				TargetSlot->ContentWidgetName = WidgetName;
				TargetSlot->VirtualIndex = 0;
			}
			else
			{
				FBlueprintHelperNamedSlotEntry Entry;
				Entry.HostWidgetName = HostWidgetName;
				Entry.SlotName = SlotName;
				Entry.ContentWidgetName = WidgetName;
				Entry.VirtualIndex = 0;
				Summary.NamedSlots.Add(Entry);
			}

			OutAffectedWidget = WidgetName;
			RebuildIndex();
			return true;
		}

		bool ApplySetNamedSlotContent(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError,
			bool bUsePayloadReplaceExisting = true)
		{
			FString HostWidgetName;
			FString SlotName;
			FString WidgetClass;
			FString WidgetName;
			FString ExpectedContentWidgetName;
			bool bReplaceExisting = false;
			TOptional<int32> VirtualIndexValue;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("host_widget_name"), HostWidgetName);
				Payload->TryGetStringField(TEXT("slot_name"), SlotName);
				Payload->TryGetStringField(TEXT("widget_class"), WidgetClass);
				Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
				Payload->TryGetStringField(TEXT("expected_content_widget_name"), ExpectedContentWidgetName);
				if (bUsePayloadReplaceExisting)
				{
					Payload->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);
				}
				double NumberValue = 0.0;
				if (Payload->TryGetNumberField(TEXT("virtual_index"), NumberValue))
				{
					VirtualIndexValue = FMath::RoundToInt(NumberValue);
				}
			}

			if (HostWidgetName.IsEmpty() || SlotName.IsEmpty() || WidgetName.IsEmpty() || WidgetClass.IsEmpty())
			{
				OutError = TEXT("invalid_named_slot_payload");
				return false;
			}

			FBlueprintHelperWidgetTreeItem* HostItem = FindItem(HostWidgetName);
			if (!HostItem)
			{
				OutError = FString::Printf(TEXT("Widget '%s' does not exist in the planned WidgetTree."), *HostWidgetName);
				return false;
			}
			if (!ClassSupportsNamedSlot(*HostItem, SlotName))
			{
				OutError = TEXT("named_slot_not_found");
				return false;
			}

			const int32 VirtualIndex = FBlueprintHelperWidgetTreePositionPolicy::NormalizeNamedSlotVirtualIndex(VirtualIndexValue);
			if (VirtualIndex != 0)
			{
				OutError = TEXT("invalid_virtual_index");
				return false;
			}

			FBlueprintHelperNamedSlotEntry* ExistingSlot = nullptr;
			for (FBlueprintHelperNamedSlotEntry& Entry : Summary.NamedSlots)
			{
				if (Entry.HostWidgetName == HostWidgetName && Entry.SlotName == SlotName)
				{
					ExistingSlot = &Entry;
					break;
				}
			}

			const FString ExistingContentName = ExistingSlot ? ExistingSlot->ContentWidgetName : FString();
			const FBlueprintHelperWidgetTreeItem* ExistingContentItem = ExistingContentName.IsEmpty()
				? nullptr
				: FindItem(ExistingContentName);
			if (!ExpectedContentWidgetName.IsEmpty() && ExistingContentName != ExpectedContentWidgetName)
			{
				OutError = TEXT("named_slot_expected_content_mismatch");
				return false;
			}
			if (!ExistingContentName.IsEmpty() && !bReplaceExisting)
			{
				OutError = TEXT("named_slot_content_exists");
				return false;
			}
			const bool bNameCollisionRetiresWithExistingContent =
				ExistingContentItem && ContainsSelfOrDescendant(*ExistingContentItem, WidgetName);
			if (ContainsWidget(WidgetName) && !bNameCollisionRetiresWithExistingContent)
			{
				OutError = TEXT("widget_name_already_exists");
				return false;
			}

			if (!ExistingContentName.IsEmpty())
			{
				FBlueprintHelperWidgetTreeItem RemovedItem;
				RemoveWidget(ExistingContentName, RemovedItem);
			}

			FBlueprintHelperWidgetTreeItem ContentItem = MakeItem(
				WidgetName,
				NormalizeWidgetClassName(WidgetClass, FString()),
				HostWidgetName,
				SlotName,
				0);
			NamedSlotContentItemsByName.Add(WidgetName, ContentItem);

			FBlueprintHelperNamedSlotEntry* TargetSlot = nullptr;
			for (FBlueprintHelperNamedSlotEntry& Entry : Summary.NamedSlots)
			{
				if (Entry.HostWidgetName == HostWidgetName && Entry.SlotName == SlotName)
				{
					TargetSlot = &Entry;
					break;
				}
			}

			if (TargetSlot)
			{
				TargetSlot->ContentWidgetName = WidgetName;
				TargetSlot->VirtualIndex = 0;
			}
			else
			{
				FBlueprintHelperNamedSlotEntry Entry;
				Entry.HostWidgetName = HostWidgetName;
				Entry.SlotName = SlotName;
				Entry.ContentWidgetName = WidgetName;
				Entry.VirtualIndex = 0;
				Summary.NamedSlots.Add(Entry);
			}

			OutAffectedWidget = WidgetName;
			RebuildIndex();
			return true;
		}

		bool ApplyRemove(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString WidgetName;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
			}
			if (WidgetName.IsEmpty())
			{
				OutError = TEXT("invalid_widget_remove_payload");
				return false;
			}

			FBlueprintHelperWidgetTreeItem RemovedItem;
			if (!RemoveWidget(WidgetName, RemovedItem))
			{
				OutError = Summary.Root.WidgetName == WidgetName
					? TEXT("root_widget_requires_remove_root_widget")
					: TEXT("widget_not_found");
				return false;
			}
			OutAffectedWidget = WidgetName;
			RebuildIndex();
			return true;
		}

		bool ApplyRename(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString WidgetName;
			FString NewWidgetName;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
				Payload->TryGetStringField(TEXT("new_widget_name"), NewWidgetName);
			}
			if (WidgetName.IsEmpty() || NewWidgetName.IsEmpty())
			{
				OutError = TEXT("invalid_widget_rename_payload");
				return false;
			}
			if (!ContainsWidget(WidgetName))
			{
				OutError = TEXT("widget_not_found");
				return false;
			}
			if (WidgetName != NewWidgetName && ContainsWidget(NewWidgetName))
			{
				OutError = TEXT("widget_name_already_exists");
				return false;
			}

			if (Summary.Root.WidgetName == WidgetName)
			{
				Summary.Root.WidgetName = NewWidgetName;
				ReindexChildren(Summary.Root.Children, NewWidgetName);
			}
			else if (FBlueprintHelperWidgetTreeItem* NamedSlotItem = NamedSlotContentItemsByName.Find(WidgetName))
			{
				FBlueprintHelperWidgetTreeItem RenamedItem = *NamedSlotItem;
				NamedSlotContentItemsByName.Remove(WidgetName);
				RenamedItem.WidgetName = NewWidgetName;
				ReindexChildren(RenamedItem.Children, NewWidgetName);
				NamedSlotContentItemsByName.Add(NewWidgetName, MoveTemp(RenamedItem));
			}
			else if (!RenameInChildren(Summary.Root.Children, WidgetName, NewWidgetName))
			{
				OutError = TEXT("widget_not_found");
				return false;
			}

			for (FBlueprintHelperNamedSlotEntry& Entry : Summary.NamedSlots)
			{
				if (Entry.ContentWidgetName == WidgetName)
				{
					Entry.ContentWidgetName = NewWidgetName;
				}
				if (Entry.HostWidgetName == WidgetName)
				{
					Entry.HostWidgetName = NewWidgetName;
				}
			}
			OutAffectedWidget = NewWidgetName;
			RebuildIndex();
			return true;
		}

		bool ApplyRemoveRoot(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString RootWidgetName;
			FString ReplacementPolicy;
			FString ReplacementWidgetClass;
			FString ReplacementWidgetName;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("root_widget_name"), RootWidgetName);
				Payload->TryGetStringField(TEXT("replacement_policy"), ReplacementPolicy);
				Payload->TryGetStringField(TEXT("replacement_widget_class"), ReplacementWidgetClass);
				Payload->TryGetStringField(TEXT("replacement_widget_name"), ReplacementWidgetName);
			}
			if (Summary.Root.WidgetName.IsEmpty())
			{
				OutError = TEXT("root_widget_missing");
				return false;
			}
			if (!RootWidgetName.IsEmpty() && Summary.Root.WidgetName != RootWidgetName)
			{
				OutError = TEXT("root_widget_name_mismatch");
				return false;
			}
			const FString Policy = ReplacementPolicy.ToLower();
			if (Policy == TEXT("promote_single_child"))
			{
				if (Summary.Root.Children.Num() != 1)
				{
					OutError = TEXT("root_promote_single_child_requires_exactly_one_child");
					return false;
				}
				FBlueprintHelperWidgetTreeItem PromotedChild = Summary.Root.Children[0];
				PromotedChild.ParentName.Empty();
				PromotedChild.SlotName.Empty();
				PromotedChild.VirtualIndex = 0;
				Summary.Root = MoveTemp(PromotedChild);
				OutAffectedWidget = Summary.Root.WidgetName;
				RebuildIndex();
				return true;
			}
			if (Policy == TEXT("replace_with_empty_root"))
			{
				if (ReplacementWidgetClass.IsEmpty())
				{
					OutError = TEXT("replacement_widget_class_required");
					return false;
				}
				const FString NewRootName = ReplacementWidgetName.IsEmpty()
					? Summary.Root.WidgetName
					: ReplacementWidgetName;
				if (NewRootName != Summary.Root.WidgetName && ContainsWidget(NewRootName))
				{
					OutError = TEXT("replacement_widget_name_already_exists");
					return false;
				}
				Summary.Root = MakeItem(
					NewRootName,
					NormalizeWidgetClassName(ReplacementWidgetClass, FString()),
					FString(),
					FString(),
					0);
				OutAffectedWidget = NewRootName;
				RebuildIndex();
				return true;
			}
			if (Policy == TEXT("remove_empty_root"))
			{
				if (Summary.Root.Children.Num() > 0)
				{
					OutError = TEXT("root_remove_empty_requires_no_children");
					return false;
				}
				OutAffectedWidget = Summary.Root.WidgetName;
				Summary.Root = FBlueprintHelperWidgetTreeItem();
				Summary.NamedSlots.Reset();
				NamedSlotContentItemsByName.Reset();
				RebuildIndex();
				return true;
			}
			OutError = TEXT("unsupported_root_removal_policy");
			return false;
		}

		bool ApplyDuplicateSubtree(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString SourceWidgetName;
			FString TargetParentName;
			FString SlotName;
			TOptional<int32> VirtualIndexValue;
			const TSharedPtr<FJsonObject>* NameMappingPtr = nullptr;
			TSharedPtr<FJsonObject> NameMapping;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("source_widget_name"), SourceWidgetName);
				Payload->TryGetStringField(TEXT("target_parent_name"), TargetParentName);
				Payload->TryGetStringField(TEXT("slot_name"), SlotName);
				Payload->TryGetObjectField(TEXT("name_mapping"), NameMappingPtr);
				if (NameMappingPtr && NameMappingPtr->IsValid())
				{
					NameMapping = *NameMappingPtr;
				}
				double NumberValue = 0.0;
				if (Payload->TryGetNumberField(TEXT("virtual_index"), NumberValue))
				{
					VirtualIndexValue = FMath::RoundToInt(NumberValue);
				}
			}
			const FBlueprintHelperWidgetTreeItem* SourceItem = FindItem(SourceWidgetName);
			if (!SourceItem)
			{
				OutError = TEXT("widget_not_found");
				return false;
			}
			bool bCloneValid = false;
			FBlueprintHelperWidgetTreeItem Clone = CloneItemWithMapping(
				*SourceItem,
				NameMapping,
				TargetParentName,
				VirtualIndexValue.IsSet() ? VirtualIndexValue.GetValue() : 0,
				bCloneValid,
				OutError);
			if (!bCloneValid)
			{
				return false;
			}
			return AttachItemToTarget(MoveTemp(Clone), TargetParentName, SlotName, VirtualIndexValue, OutAffectedWidget, OutError);
		}

		bool ApplyWrap(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString WidgetName;
			FString WrapperClass;
			FString WrapperName;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
				Payload->TryGetStringField(TEXT("wrapper_class"), WrapperClass);
				Payload->TryGetStringField(TEXT("wrapper_name"), WrapperName);
			}
			if (WidgetName.IsEmpty() || WrapperClass.IsEmpty() || WrapperName.IsEmpty())
			{
				OutError = TEXT("invalid_wrap_widget_payload");
				return false;
			}
			if (ContainsWidget(WrapperName))
			{
				OutError = TEXT("wrapper_name_already_exists");
				return false;
			}
			const FBlueprintHelperWidgetTreeItem* Widget = FindItem(WidgetName);
			if (!Widget)
			{
				OutError = TEXT("widget_not_found");
				return false;
			}
			const FString OldParentName = Widget->ParentName;
			const FString OldSlotName = Widget->SlotName;
			const int32 OldVirtualIndex = Widget->VirtualIndex;
			const bool bWasRoot = Summary.Root.WidgetName == WidgetName;
			FBlueprintHelperWidgetTreeItem WrappedItem;
			if (bWasRoot)
			{
				WrappedItem = Summary.Root;
				Summary.Root = FBlueprintHelperWidgetTreeItem();
			}
			else if (!RemoveWidget(WidgetName, WrappedItem))
			{
				OutError = TEXT("widget_not_found");
				return false;
			}

			FBlueprintHelperWidgetTreeItem Wrapper = MakeItem(
				WrapperName,
				NormalizeWidgetClassName(WrapperClass, FString()),
				OldParentName,
				OldSlotName,
				OldVirtualIndex);
			WrappedItem.ParentName = WrapperName;
			WrappedItem.SlotName.Empty();
			WrappedItem.VirtualIndex = 0;
			Wrapper.Children.Add(MoveTemp(WrappedItem));

			if (bWasRoot)
			{
				Wrapper.ParentName.Empty();
				Wrapper.SlotName.Empty();
				Wrapper.VirtualIndex = 0;
				Summary.Root = MoveTemp(Wrapper);
				OutAffectedWidget = WrapperName;
				RebuildIndex();
				return true;
			}
			return AttachItemToTarget(MoveTemp(Wrapper), OldParentName, OldSlotName, OldVirtualIndex, OutAffectedWidget, OutError);
		}

		bool ApplyReplaceWidgetClass(
			const TSharedPtr<FJsonObject>& Payload,
			FString& OutAffectedWidget,
			FString& OutError)
		{
			FString WidgetName;
			FString NewWidgetClass;
			bool bPreserveChildren = true;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
				Payload->TryGetStringField(TEXT("new_widget_class"), NewWidgetClass);
				Payload->TryGetBoolField(TEXT("preserve_children"), bPreserveChildren);
			}
			FBlueprintHelperWidgetTreeItem* Item = FindItem(WidgetName);
			if (!Item)
			{
				OutError = TEXT("widget_not_found");
				return false;
			}
			if (NewWidgetClass.IsEmpty())
			{
				OutError = TEXT("new_widget_class_required");
				return false;
			}
			Item->WidgetClass = NormalizeWidgetClassName(NewWidgetClass, FString());
			Item->WidgetClassPath = ResolveWidgetClassPath(NewWidgetClass, FString());
			if (!bPreserveChildren)
			{
				Item->Children.Reset();
			}
			OutAffectedWidget = WidgetName;
			RebuildIndex();
			return true;
		}
	};

	using FPlannedWidgetTreesByAsset = TMap<FString, FPlannedWidgetTreeState>;

	static bool IsPlannedWidgetTreeStructuralDryRunStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		if (LoweredStep.Capability != FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget)
		{
			return false;
		}
		for (const FBlueprintHelperGeneratedCommandDescriptor& Descriptor : GBlueprintHelperUMGWidgetOperationCommands)
		{
			if (LoweredStep.AdapterOperation.Equals(Descriptor.TaskPlanOp, ESearchCase::IgnoreCase) ||
				LoweredStep.AdapterOperation.Equals(Descriptor.Command, ESearchCase::IgnoreCase))
			{
				return FString(Descriptor.PlannedPreviewEffect).Equals(TEXT("widget_tree_structural"), ESearchCase::IgnoreCase);
			}
		}
		return false;
	}

	static UWidgetBlueprint* ResolvePlannedWidgetBlueprint(
		const FString& AssetPath,
		FString& OutError)
	{
		OutError.Reset();
		if (AssetPath.IsEmpty())
		{
			OutError = TEXT("asset_path is required for planned WidgetTree preview.");
			return nullptr;
		}

		UObject* Object = StaticFindObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath);
		if (!Object)
		{
			Object = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath);
		}

		UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Object);
		if (!WidgetBlueprint)
		{
			OutError = FString::Printf(TEXT("WidgetBlueprint was not found for planned preview: %s"), *AssetPath);
			return nullptr;
		}
		return WidgetBlueprint;
	}

	static void PopulateNamedSlotContentItems(FPlannedWidgetTreeState& State)
	{
		State.NamedSlotContentItemsByName.Reset();
		for (const FBlueprintHelperNamedSlotEntry& Entry : State.Summary.NamedSlots)
		{
			if (const FBlueprintHelperWidgetTreeItem* Item = State.Summary.Index.Find(Entry.ContentWidgetName))
			{
				State.NamedSlotContentItemsByName.Add(Entry.ContentWidgetName, *Item);
			}
		}
	}

	static bool TryInitializePlannedWidgetTreeState(
		const FString& AssetPath,
		FPlannedWidgetTreeState& State,
		FString& OutError)
	{
		OutError.Reset();
		if (State.bInitialized)
		{
			return true;
		}

		UWidgetBlueprint* WidgetBlueprint = ResolvePlannedWidgetBlueprint(AssetPath, OutError);
		if (!WidgetBlueprint)
		{
			return false;
		}

		FString ErrorCode;
		FString ErrorMessage;
		if (FBlueprintHelperWidgetTreeProjectionService::BuildWidgetTreeSummary(
			WidgetBlueprint,
			State.Summary,
			ErrorCode,
			ErrorMessage))
		{
			PopulateNamedSlotContentItems(State);
			State.bInitialized = true;
			return true;
		}

		if (ErrorCode != TEXT("widget_tree_root_not_found"))
		{
			OutError = ErrorCode.IsEmpty() ? ErrorMessage : ErrorCode;
			return false;
		}

		State.Summary = FBlueprintHelperWidgetTreeSummary();
		State.Summary.AssetClass = WidgetBlueprint->GetClass() ? WidgetBlueprint->GetClass()->GetPathName() : FString();
		State.Summary.ParentClass = WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetPathName() : FString();
		State.bInitialized = true;
		return true;
	}

	static FPlannedWidgetTreeState* FindOrCreatePlannedWidgetTreeState(
		const FString& AssetPath,
		FPlannedWidgetTreesByAsset& PlannedWidgetTreesByAsset,
		FString& OutError)
	{
		const FString StateKey = NormalizePlannedStateAssetPath(AssetPath);
		FPlannedWidgetTreeState& State = PlannedWidgetTreesByAsset.FindOrAdd(StateKey);
		return TryInitializePlannedWidgetTreeState(AssetPath, State, OutError) ? &State : nullptr;
	}

	static FBlueprintHelperToolResultBase MakePlannedWidgetTreeDryRunResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FPlannedWidgetTreeState& State,
		const FString& AffectedWidget)
	{
		FString AssetPath;
		FString WidgetName;
		TryReadWidgetPayloadIdentity(LoweredStep, AssetPath, WidgetName);
		const FString TargetWidgetName = !AffectedWidget.IsEmpty() ? AffectedWidget : WidgetName;

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("widget"), TargetWidgetName);

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		const bool bWouldCreate =
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget ||
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent ||
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::DuplicateWidgetSubtree ||
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::WrapWidget;
		const bool bWouldRemove =
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget ||
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget;
		const bool bWouldUpdate = !bWouldCreate && !bWouldRemove;
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("task_runtime_planned_widget_tree"));
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("result"), TEXT("passed"));
		DryRun->SetNumberField(TEXT("would_change_count"), 1);
		DryRun->SetNumberField(TEXT("would_create_count"), bWouldCreate ? 1 : 0);
		DryRun->SetNumberField(TEXT("would_update_count"), bWouldUpdate ? 1 : 0);
		DryRun->SetNumberField(TEXT("would_remove_count"), bWouldRemove ? 1 : 0);
		DryRun->SetNumberField(TEXT("would_no_op_count"), 0);
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("WidgetMutation.v1"));
		Data->SetBoolField(TEXT("dry_run"), true);
		if (!TargetWidgetName.IsEmpty())
		{
			Data->SetStringField(TEXT("widget_name"), TargetWidgetName);
		}
		Data->SetObjectField(TEXT("readback_context"), State.Summary.ToJson());
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		Result.Data = Data;

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = false;
		Validation.bShouldSave = false;
		Result.Validation = Validation;
		return Result;
	}

	static FBlueprintHelperToolResultBase ExecutePlannedWidgetTreeDryRunStep(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FPlannedWidgetTreesByAsset& PlannedWidgetTreesByAsset)
	{
		const FString AssetPath = ReadPayloadAssetPath(LoweredStep.Payload);
		FString ErrorMessage;
		FPlannedWidgetTreeState* State = FindOrCreatePlannedWidgetTreeState(
			AssetPath,
			PlannedWidgetTreesByAsset,
			ErrorMessage);
		if (!State)
		{
			return MakeFailure(
				LoweredStep.AdapterOperation,
				TEXT("widget_tree_planned_state_init_failed"),
				EBlueprintHelperToolStage::DryRun,
				ErrorMessage.IsEmpty() ? TEXT("Failed to initialize planned WidgetTree preview state.") : ErrorMessage,
				TEXT("asset_path"));
		}

		FString AffectedWidget;
		FString OperationError;
		bool bApplied = false;
		if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget)
		{
			bApplied = State->ApplyAdd(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::MoveWidget)
		{
			bApplied = State->ApplyMove(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent)
		{
			bApplied = State->ApplySetNamedSlotContent(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget)
		{
			bApplied = State->ApplyRemove(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RenameWidget)
		{
			bApplied = State->ApplyRename(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget)
		{
			bApplied = State->ApplyRemoveRoot(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::DuplicateWidgetSubtree)
		{
			bApplied = State->ApplyDuplicateSubtree(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::WrapWidget)
		{
			bApplied = State->ApplyWrap(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReplaceWidgetClass)
		{
			bApplied = State->ApplyReplaceWidgetClass(LoweredStep.Payload, AffectedWidget, OperationError);
		}
		else
		{
			return MakeFailure(
				LoweredStep.AdapterOperation,
				TEXT("unsupported_planned_widget_tree_operation"),
				EBlueprintHelperToolStage::DryRun,
				TEXT("Unsupported planned WidgetTree dry-run operation."));
		}

		if (!bApplied)
		{
			return MakeFailure(
				LoweredStep.AdapterOperation,
				TEXT("widget_operation_failed"),
				EBlueprintHelperToolStage::DryRun,
				OperationError.IsEmpty() ? TEXT("Planned WidgetTree dry-run operation failed.") : OperationError);
		}
		return MakePlannedWidgetTreeDryRunResult(LoweredStep, *State, AffectedWidget);
	}

	static FString MakePlannedDataTableRowKey(
		const FString& AssetPath,
		const FString& RowName)
	{
		return FString::Printf(TEXT("%s\n%s"), *AssetPath, *RowName);
	}

	static bool TryReadComponentPayloadIdentity(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FString& OutAssetPath,
		FString& OutComponentName)
	{
		OutAssetPath.Empty();
		OutComponentName.Empty();
		if (!LoweredStep.Payload.IsValid())
		{
			return false;
		}

		LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		LoweredStep.Payload->TryGetStringField(TEXT("component_name"), OutComponentName);
		return !OutAssetPath.IsEmpty() && !OutComponentName.IsEmpty();
	}

	static bool TryReadWidgetPayloadIdentity(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FString& OutAssetPath,
		FString& OutWidgetName)
	{
		OutAssetPath.Empty();
		OutWidgetName.Empty();
		if (!LoweredStep.Payload.IsValid())
		{
			return false;
		}

		LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		LoweredStep.Payload->TryGetStringField(TEXT("widget_name"), OutWidgetName);
		return !OutAssetPath.IsEmpty() && !OutWidgetName.IsEmpty();
	}

	static bool TryReadDataTablePayloadIdentity(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FString& OutAssetPath,
		FString& OutRowName)
	{
		OutAssetPath.Empty();
		OutRowName.Empty();
		if (!LoweredStep.Payload.IsValid())
		{
			return false;
		}

		LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		LoweredStep.Payload->TryGetStringField(TEXT("row_name"), OutRowName);
		return !OutAssetPath.IsEmpty() && !OutRowName.IsEmpty();
	}

	static FString NormalizePlannedStateAssetPath(const FString& AssetPath)
	{
		FString Key = AssetPath;
		Key.TrimStartAndEndInline();
		Key.ToLowerInline();
		return Key;
	}

	static FString ReadPayloadAssetPath(const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		if (!Payload.IsValid())
		{
			return AssetPath;
		}

		Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		if (AssetPath.IsEmpty())
		{
			Payload->TryGetStringField(TEXT("blueprint_path"), AssetPath);
		}
		if (AssetPath.IsEmpty())
		{
			const TSharedPtr<FJsonObject>* TargetObject = nullptr;
			if (Payload->TryGetObjectField(TEXT("target"), TargetObject) &&
				TargetObject && TargetObject->IsValid())
			{
				(*TargetObject)->TryGetStringField(TEXT("asset_path"), AssetPath);
				if (AssetPath.IsEmpty())
				{
					(*TargetObject)->TryGetStringField(TEXT("blueprint_path"), AssetPath);
				}
			}
		}
		AssetPath.TrimStartAndEndInline();
		return AssetPath;
	}

	static bool TryReadVariableNameField(
		const TSharedPtr<FJsonObject>& VariableObject,
		FString& OutVariableName)
	{
		OutVariableName.Reset();
		if (!VariableObject.IsValid())
		{
			return false;
		}

		VariableObject->TryGetStringField(TEXT("name"), OutVariableName);
		if (OutVariableName.IsEmpty())
		{
			VariableObject->TryGetStringField(TEXT("variable_name"), OutVariableName);
		}
		OutVariableName.TrimStartAndEndInline();
		return !OutVariableName.IsEmpty();
	}

	static bool BlueprintHasMemberVariable(
		const UBlueprint* Blueprint,
		const FString& VariableName)
	{
		if (!Blueprint || VariableName.TrimStartAndEnd().IsEmpty())
		{
			return false;
		}

		const FName VariableFName(*VariableName.TrimStartAndEnd());
		return Blueprint->NewVariables.ContainsByPredicate(
			[VariableFName](const FBPVariableDescription& Variable)
			{
				return Variable.VarName == VariableFName;
			});
	}

	static void AddDryRunPlannedMemberVariable(
		FPlannedMemberVariablesByAsset& PlannedVariablesByAsset,
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& VariableObject)
	{
		const FString AssetKey = NormalizePlannedStateAssetPath(AssetPath);
		FString VariableName;
		if (AssetKey.IsEmpty() ||
			!TryReadVariableNameField(VariableObject, VariableName))
		{
			return;
		}

		TSharedPtr<FJsonObject> StoredVariable =
			FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneJsonObject(VariableObject);
		if (!StoredVariable.IsValid())
		{
			StoredVariable = MakeShared<FJsonObject>();
			for (const auto& Pair : VariableObject->Values)
			{
				StoredVariable->SetField(FBlueprintHelperVersionCompat::JsonKeyToString(Pair.Key), Pair.Value);
			}
		}
		StoredVariable->SetStringField(TEXT("name"), VariableName);

		FPlannedMemberVariableByName& VariablesByName = PlannedVariablesByAsset.FindOrAdd(AssetKey);
		VariablesByName.Add(VariableName, StoredVariable);
	}

	static void RemoveDryRunPlannedMemberVariable(
		FPlannedMemberVariablesByAsset& PlannedVariablesByAsset,
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& VariableObject)
	{
		const FString AssetKey = NormalizePlannedStateAssetPath(AssetPath);
		FString VariableName;
		if (AssetKey.IsEmpty() ||
			!TryReadVariableNameField(VariableObject, VariableName))
		{
			return;
		}

		if (FPlannedMemberVariableByName* VariablesByName = PlannedVariablesByAsset.Find(AssetKey))
		{
			VariablesByName->Remove(VariableName);
			if (VariablesByName->Num() == 0)
			{
				PlannedVariablesByAsset.Remove(AssetKey);
			}
		}
	}

	static void TrackDryRunPlannedMemberVariables(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FPlannedMemberVariablesByAsset& PlannedVariablesByAsset)
	{
		const FString AssetPath = ReadPayloadAssetPath(LoweredStep.Payload);
		if (AssetPath.IsEmpty() || !LoweredStep.Payload.IsValid())
		{
			return;
		}

		if (LoweredStep.AdapterOperation == FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationAddMemberVariables)
		{
			const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
			if (!LoweredStep.Payload->TryGetArrayField(TEXT("variables"), Variables) || !Variables)
			{
				return;
			}

			for (const TSharedPtr<FJsonValue>& VariableValue : *Variables)
			{
				AddDryRunPlannedMemberVariable(
					PlannedVariablesByAsset,
					AssetPath,
					VariableValue.IsValid() ? VariableValue->AsObject() : nullptr);
			}
			return;
		}

		if (LoweredStep.AdapterOperation != FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch)
		{
			return;
		}

		FString Strategy;
		LoweredStep.Payload->TryGetStringField(TEXT("strategy"), Strategy);
		if (!Strategy.Equals(FBlueprintHelperBlueprintVariableTaskPlanAdapter::StrategyMemberVariables, ESearchCase::IgnoreCase))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (!LoweredStep.Payload->TryGetArrayField(TEXT("ops"), Ops) || !Ops)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& OpValue : *Ops)
		{
			const TSharedPtr<FJsonObject> OpObject = OpValue.IsValid()
				? OpValue->AsObject()
				: nullptr;
			if (!OpObject.IsValid())
			{
				continue;
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureMemberVariable)
			{
				AddDryRunPlannedMemberVariable(PlannedVariablesByAsset, AssetPath, OpObject);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveMemberVariable)
			{
				RemoveDryRunPlannedMemberVariable(PlannedVariablesByAsset, AssetPath, OpObject);
			}
		}
	}

	static FString BuildDryRunPlannedMemberVariablesStateHash(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FPlannedMemberVariablesByAsset& PlannedVariablesByAsset)
	{
		const FString AssetPath = ReadPayloadAssetPath(LoweredStep.Payload);
		TArray<FString> AssetKeys;
		if (!AssetPath.IsEmpty())
		{
			AssetKeys.Add(NormalizePlannedStateAssetPath(AssetPath));
		}
		else
		{
			PlannedVariablesByAsset.GetKeys(AssetKeys);
		}
		AssetKeys.Sort();

		TArray<TSharedPtr<FJsonValue>> AssetValues;
		for (const FString& AssetKey : AssetKeys)
		{
			const FPlannedMemberVariableByName* VariablesByName = PlannedVariablesByAsset.Find(AssetKey);
			if (!VariablesByName || VariablesByName->Num() == 0)
			{
				continue;
			}

			TArray<FString> VariableNames;
			VariablesByName->GetKeys(VariableNames);
			VariableNames.Sort();

			TArray<TSharedPtr<FJsonValue>> VariableValues;
			for (const FString& VariableName : VariableNames)
			{
				const TSharedPtr<FJsonObject>* VariableObject = VariablesByName->Find(VariableName);
				if (!VariableObject || !VariableObject->IsValid())
				{
					continue;
				}

				TSharedRef<FJsonObject> VariableEntry = MakeShared<FJsonObject>();
				VariableEntry->SetStringField(TEXT("name"), VariableName);
				VariableEntry->SetStringField(
					TEXT("payload_hash"),
					FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(*VariableObject));
				VariableValues.Add(MakeShared<FJsonValueObject>(VariableEntry));
			}

			if (VariableValues.Num() == 0)
			{
				continue;
			}

			TSharedRef<FJsonObject> AssetEntry = MakeShared<FJsonObject>();
			AssetEntry->SetStringField(TEXT("asset_key"), AssetKey);
			AssetEntry->SetArrayField(TEXT("variables"), VariableValues);
			AssetValues.Add(MakeShared<FJsonValueObject>(AssetEntry));
		}

		if (AssetValues.Num() == 0)
		{
			return TEXT("none");
		}

		TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
		State->SetArrayField(TEXT("planned_member_variables"), AssetValues);
		return FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(State);
	}

	struct FScopedDryRunPlannedMemberVariableOverlay
	{
		UBlueprint* Blueprint = nullptr;
		UPackage* Package = nullptr;
		bool bHadPackageDirty = false;
		TArray<FString> AddedVariableNames;

		FScopedDryRunPlannedMemberVariableOverlay(
			const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
			const FPlannedMemberVariablesByAsset& PlannedVariablesByAsset)
		{
			if (LoweredStep.Capability != TEXT("graph_write"))
			{
				return;
			}

			const FString AssetPath = ReadPayloadAssetPath(LoweredStep.Payload);
			const FPlannedMemberVariableByName* VariablesByName =
				PlannedVariablesByAsset.Find(NormalizePlannedStateAssetPath(AssetPath));
			if (!VariablesByName || VariablesByName->Num() == 0)
			{
				return;
			}

			Blueprint = ResolveTaskRuntimeBlueprint(AssetPath);
			if (!Blueprint)
			{
				return;
			}

			Package = Blueprint->GetOutermost();
			bHadPackageDirty = Package && Package->IsDirty();

			TArray<FString> VariableNames;
			VariablesByName->GetKeys(VariableNames);
			VariableNames.Sort();
			for (const FString& VariableName : VariableNames)
			{
				if (BlueprintHasMemberVariable(Blueprint, VariableName))
				{
					continue;
				}

				const TSharedPtr<FJsonObject>* VariableObject = VariablesByName->Find(VariableName);
				if (!VariableObject || !VariableObject->IsValid())
				{
					continue;
				}

				FString AddError;
				if (UBlueprintHelperBlueprintStructureUtils::AddMemberVariableDirect(
					Blueprint,
					*VariableObject,
					AddError))
				{
					AddedVariableNames.Add(VariableName);
				}
			}
		}

		~FScopedDryRunPlannedMemberVariableOverlay()
		{
			if (!Blueprint)
			{
				return;
			}

			for (int32 Index = AddedVariableNames.Num() - 1; Index >= 0; --Index)
			{
				FString RemoveError;
				UBlueprintHelperBlueprintStructureUtils::RemoveMemberVariableDirect(
					Blueprint,
					AddedVariableNames[Index],
					RemoveError);
			}

			if (Package)
			{
				Package->SetDirtyFlag(bHadPackageDirty);
			}
		}
	};

	static bool IsPlannedComponentPropertyDryRun(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const TSet<FString>& PlannedComponentKeys)
	{
		if (LoweredStep.AdapterOperation != FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties ||
			StepResult.bOk ||
			!StepResult.Error.IsSet() ||
			StepResult.Error->Code != TEXT("component_not_found"))
		{
			return false;
		}

		bool bDryRun = false;
		if (!LoweredStep.Payload.IsValid() ||
			!LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun) ||
			!bDryRun)
		{
			return false;
		}

		FString AssetPath;
		FString ComponentName;
		return TryReadComponentPayloadIdentity(LoweredStep, AssetPath, ComponentName) &&
			PlannedComponentKeys.Contains(MakePlannedComponentKey(AssetPath, ComponentName));
	}

	static bool IsPlannedWidgetPropertyDryRun(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const TSet<FString>& PlannedWidgetKeys)
	{
		if (LoweredStep.AdapterOperation != FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty ||
			StepResult.bOk ||
			!StepResult.Error.IsSet() ||
			StepResult.Error->Code != TEXT("widget_operation_failed"))
		{
			return false;
		}

		bool bDryRun = false;
		if (!LoweredStep.Payload.IsValid() ||
			!LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun) ||
			!bDryRun)
		{
			return false;
		}

		FString AssetPath;
		FString WidgetName;
		return TryReadWidgetPayloadIdentity(LoweredStep, AssetPath, WidgetName) &&
			PlannedWidgetKeys.Contains(MakePlannedWidgetKey(AssetPath, WidgetName));
	}

	static bool IsPlannedDataTableRowUpdateDryRun(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const TSet<FString>& PlannedDataTableRowKeys)
	{
		if (LoweredStep.AdapterOperation != FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationUpdateRow ||
			StepResult.bOk ||
			!StepResult.Error.IsSet() ||
			StepResult.Error->Code != TEXT("data_table_operation_failed"))
		{
			return false;
		}

		bool bDryRun = false;
		if (!LoweredStep.Payload.IsValid() ||
			!LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun) ||
			!bDryRun)
		{
			return false;
		}

		FString AssetPath;
		FString RowName;
		return TryReadDataTablePayloadIdentity(LoweredStep, AssetPath, RowName) &&
			PlannedDataTableRowKeys.Contains(MakePlannedDataTableRowKey(AssetPath, RowName));
	}

	static FBlueprintHelperToolResultBase MakePlannedComponentPropertyDryRunResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		FString AssetPath;
		FString ComponentName;
		TryReadComponentPayloadIdentity(LoweredStep, AssetPath, ComponentName);

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		if (!AssetPath.IsEmpty())
		{
			Target->SetStringField(TEXT("asset_path"), AssetPath);
		}
		Target->SetStringField(TEXT("target_type"), TEXT("component"));
		if (!ComponentName.IsEmpty())
		{
			Target->SetStringField(TEXT("component_name"), ComponentName);
		}
		Result.CustomTargetJson = Target;

		int32 SettingsCount = 0;
		const TArray<TSharedPtr<FJsonValue>>* Settings = nullptr;
		if (LoweredStep.Payload.IsValid() &&
			LoweredStep.Payload->TryGetArrayField(TEXT("settings"), Settings) &&
			Settings)
		{
			SettingsCount = Settings->Num();
		}

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("task_runtime_planned_component"));
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("result"), TEXT("passed"));
		DryRun->SetNumberField(TEXT("would_change_count"), SettingsCount);
		DryRun->SetNumberField(TEXT("would_create_count"), 0);
		DryRun->SetNumberField(TEXT("would_update_count"), SettingsCount);
		DryRun->SetNumberField(TEXT("would_remove_count"), 0);
		DryRun->SetNumberField(TEXT("would_no_op_count"), 0);
		DryRun->SetStringField(
			TEXT("limitation"),
			TEXT("Component property validation is deferred because the component is created by an earlier dry-run step."));

		TSharedRef<FJsonObject> Warning = MakeShared<FJsonObject>();
		Warning->SetStringField(TEXT("code"), TEXT("planned_component_property_validation_deferred"));
		Warning->SetStringField(TEXT("target"), ComponentName);
		Warning->SetStringField(
			TEXT("message"),
			TEXT("The component does not exist in the asset during preview, but a prior TaskPlan step plans to create it."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueObject>(Warning));
		DryRun->SetArrayField(TEXT("warnings"), Warnings);
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintComponent.v1"));
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		Result.Data = Data;

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = false;
		Validation.bShouldSave = false;
		Result.Validation = Validation;
		return Result;
	}

	static FBlueprintHelperToolResultBase MakePlannedWidgetPropertyDryRunResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		FString AssetPath;
		FString WidgetName;
		TryReadWidgetPayloadIdentity(LoweredStep, AssetPath, WidgetName);

		FString PropertyName;
		if (LoweredStep.Payload.IsValid())
		{
			LoweredStep.Payload->TryGetStringField(TEXT("property_name"), PropertyName);
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("widget"), WidgetName, TEXT(""), PropertyName);

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("task_runtime_planned_widget"));
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("result"), TEXT("passed"));
		DryRun->SetNumberField(TEXT("would_change_count"), 1);
		DryRun->SetNumberField(TEXT("would_create_count"), 0);
		DryRun->SetNumberField(TEXT("would_update_count"), 1);
		DryRun->SetNumberField(TEXT("would_remove_count"), 0);
		DryRun->SetNumberField(TEXT("would_no_op_count"), 0);
		DryRun->SetStringField(
			TEXT("limitation"),
			TEXT("Widget property validation is deferred because the widget is created by an earlier dry-run step."));

		TSharedRef<FJsonObject> Warning = MakeShared<FJsonObject>();
		Warning->SetStringField(TEXT("code"), TEXT("planned_widget_property_validation_deferred"));
		Warning->SetStringField(TEXT("target"), WidgetName);
		Warning->SetStringField(
			TEXT("message"),
			TEXT("The widget does not exist in the asset during preview, but a prior TaskPlan step plans to create it."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueObject>(Warning));
		DryRun->SetArrayField(TEXT("warnings"), Warnings);
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("WidgetMutation.v1"));
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		if (!WidgetName.IsEmpty())
		{
			Data->SetStringField(TEXT("widget_name"), WidgetName);
		}
		if (!PropertyName.IsEmpty())
		{
			Data->SetStringField(TEXT("property_name"), PropertyName);
		}
		Result.Data = Data;

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = false;
		Validation.bShouldSave = false;
		Result.Validation = Validation;
		return Result;
	}

	static FBlueprintHelperToolResultBase MakePlannedDataTableRowUpdateDryRunResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		FString AssetPath;
		FString RowName;
		TryReadDataTablePayloadIdentity(LoweredStep, AssetPath, RowName);

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("data_table_row"), TEXT(""), RowName);

		int32 FieldCount = 0;
		const TSharedPtr<FJsonObject>* Fields = nullptr;
		if (LoweredStep.Payload.IsValid() &&
			LoweredStep.Payload->TryGetObjectField(TEXT("fields"), Fields) &&
			Fields && Fields->IsValid())
		{
			FieldCount = (*Fields)->Values.Num();
		}

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("task_runtime_planned_data_table_row"));
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("result"), TEXT("passed"));
		DryRun->SetNumberField(TEXT("would_change_count"), FieldCount);
		DryRun->SetNumberField(TEXT("would_create_count"), 0);
		DryRun->SetNumberField(TEXT("would_update_count"), FieldCount);
		DryRun->SetNumberField(TEXT("would_remove_count"), 0);
		DryRun->SetNumberField(TEXT("would_no_op_count"), 0);
		DryRun->SetStringField(
			TEXT("limitation"),
			TEXT("DataTable row update validation is deferred because the row is created by an earlier dry-run step."));

		TSharedRef<FJsonObject> Warning = MakeShared<FJsonObject>();
		Warning->SetStringField(TEXT("code"), TEXT("planned_datatable_row_update_validation_deferred"));
		Warning->SetStringField(TEXT("target"), RowName);
		Warning->SetStringField(
			TEXT("message"),
			TEXT("The DataTable row does not exist during preview, but a prior TaskPlan step plans to create it."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueObject>(Warning));
		DryRun->SetArrayField(TEXT("warnings"), Warnings);
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("DataTableMutation.v1"));
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		Data->SetStringField(TEXT("row_name"), RowName);
		Result.Data = Data;

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = false;
		Validation.bShouldSave = false;
		Result.Validation = Validation;
		return Result;
	}

	static EBlueprintHelperAssetCollisionPolicy ParseAssetFactoryCollision(const FString& CollisionText)
	{
		return CollisionText.Equals(TEXT("reuse_if_exists"), ESearchCase::IgnoreCase)
			? EBlueprintHelperAssetCollisionPolicy::ReuseIfExists
			: EBlueprintHelperAssetCollisionPolicy::FailIfExists;
	}

	static void ApplyAssetFactoryResultData(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperAssetFactoryData& FactoryData,
		const FString& AssetPath,
		EBlueprintHelperAssetType AssetType)
	{
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("asset"));
		Result.Data = FactoryData.ToJson();

		if (Result.bOk && Result.Status == EBlueprintHelperToolStatus::Applied)
		{
			FBlueprintHelperValidationSummary Validation;
			Validation.bShouldCompile = FBlueprintHelperAssetFactoryService::ShouldCompile(AssetType);
			Validation.bShouldSave = FBlueprintHelperAssetFactoryService::ShouldSave(AssetType);
			Result.Validation = Validation;
		}
	}

	static TArray<FBlueprintHelperAssetFactoryFieldSpec> ReadAssetFactoryFieldsArray(
		const TSharedPtr<FJsonObject>& Payload)
	{
		TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("fields"), Values) || !Values)
		{
			return Fields;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> FieldObject = AsJsonObjectIfObject(Value);
			if (!FieldObject.IsValid())
			{
				continue;
			}

			FString Name;
			FString Type;
			if (!FieldObject->TryGetStringField(TEXT("name"), Name) ||
				!FieldObject->TryGetStringField(TEXT("type"), Type))
			{
				continue;
			}

			FBlueprintHelperAssetFactoryFieldSpec Field(Name, Type);
			if (FieldObject->HasField(TEXT("default_value")))
			{
				FString DefaultValue;
				if (!FieldObject->TryGetStringField(TEXT("default_value"), DefaultValue))
				{
					DefaultValue = JsonValueToString(FieldObject->TryGetField(TEXT("default_value")));
				}
				Field.DefaultValue = DefaultValue;
				Field.bHasDefaultValue = true;
			}
			Fields.Add(Field);
		}
		return Fields;
	}

	static FBlueprintHelperToolResultBase ExecuteAssetFactoryTaskPlanStep(
		const FBlueprintHelperAssetFactoryService& Service,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString AssetTypeText;
		FString ParentClass;
		FString ValueType;
		FString RowStruct;
		FString DataAssetClass;
		FString CollisionText;
		TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("asset_type"), AssetTypeText);
			Payload->TryGetStringField(TEXT("parent_class"), ParentClass);
			Payload->TryGetStringField(TEXT("value_type"), ValueType);
			Payload->TryGetStringField(TEXT("row_struct"), RowStruct);
			Payload->TryGetStringField(TEXT("data_asset_class"), DataAssetClass);
			Payload->TryGetStringField(TEXT("collision"), CollisionText);
			Fields = ReadAssetFactoryFieldsArray(Payload);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
		if (!FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(AssetTypeText, ParentClass, AssetType))
		{
			return MakeFailure(
				TEXT("create_asset"),
				TEXT("unsupported_asset_type"),
				EBlueprintHelperToolStage::ParseInput,
				FString::Printf(TEXT("Unsupported asset_type: %s"), *AssetTypeText),
				TEXT("task_plan.steps[0].write.ops[0].asset_type"));
		}

		if (AssetType == EBlueprintHelperAssetType::DataTable && RowStruct.TrimStartAndEnd().IsEmpty())
		{
			return MakeFailure(
				TEXT("create_asset"),
				TEXT("missing_row_struct"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("asset_type=data_table requires row_struct."),
				TEXT("task_plan.steps[0].write.ops[0].row_struct"));
		}

		if (AssetType == EBlueprintHelperAssetType::BlueprintClass)
		{
			FString BlueprintParentError;
			if (!FBlueprintHelperAssetFactoryService::TryValidateBlueprintParentClass(ParentClass, BlueprintParentError))
			{
				return MakeFailure(
					TEXT("create_asset"),
					TEXT("invalid_blueprint_parent_class"),
					EBlueprintHelperToolStage::Preflight,
					BlueprintParentError,
					TEXT("task_plan.steps[0].write.ops[0].parent_class"));
			}
		}

		const EBlueprintHelperAssetCollisionPolicy Collision = ParseAssetFactoryCollision(CollisionText);
		const FBlueprintHelperAssetFactoryData FactoryData = Service.CreateAsset(
			AssetPath,
			AssetType,
			ParentClass,
			ValueType,
			RowStruct,
			DataAssetClass,
			Fields,
			Collision,
			bDryRun);

		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result;
		if (FactoryData.Asset.bAlreadyExisted)
		{
			if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists &&
				FactoryData.Collision.bHandled)
			{
				Result = bDryRun
					? FBlueprintHelperToolResultBuilder::DryRun(TEXT("create_asset"), TraceId)
					: FBlueprintHelperToolResultBuilder::NoOp(TEXT("create_asset"), TraceId);
			}
			else
			{
				Result = FBlueprintHelperToolResultBuilder::Failure(
					TEXT("create_asset"),
					TraceId,
					MakeTaskRuntimeError(
						FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::FailIfExists
							? TEXT("asset_already_exists")
							: TEXT("asset_type_mismatch"),
						EBlueprintHelperToolStage::Preflight,
						TEXT("Asset Factory could not create the requested asset.")));
			}
		}
		else if (!FactoryData.Asset.bCreated)
		{
			Result = bDryRun
				? FBlueprintHelperToolResultBuilder::DryRun(TEXT("create_asset"), TraceId)
				: FBlueprintHelperToolResultBuilder::Failure(
					TEXT("create_asset"),
					TraceId,
					MakeTaskRuntimeError(
						TEXT("creation_failed"),
						EBlueprintHelperToolStage::Execute,
						TEXT("Failed to create asset.")));
		}
		else
		{
			Result = FBlueprintHelperToolResultBuilder::Applied(TEXT("create_asset"), TraceId);
		}

		ApplyAssetFactoryResultData(Result, FactoryData, AssetPath, AssetType);
		if (bDryRun && Result.Data.IsValid())
		{
			Result.Data->SetBoolField(TEXT("dry_run"), true);
		}
		return Result;
	}

	static TArray<FString> ReadTaskRuntimeStringArrayField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(FieldName, Values) || !Values)
		{
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Item;
			if (Value.IsValid() && Value->TryGetString(Item))
			{
				Result.Add(Item);
			}
		}
		return Result;
	}

	static TMap<FString, FString> ReadTaskRuntimeStringFieldsObject(const TSharedPtr<FJsonObject>& Payload)
	{
		TMap<FString, FString> Fields;
		const TSharedPtr<FJsonObject>* FieldsObject = nullptr;
		if (!Payload.IsValid() ||
			!Payload->TryGetObjectField(TEXT("fields"), FieldsObject) ||
			!FieldsObject || !FieldsObject->IsValid())
		{
			return Fields;
		}

		for (const auto& Field : (*FieldsObject)->Values)
		{
			Fields.Add(FBlueprintHelperVersionCompat::JsonKeyToString(Field.Key), JsonValueToString(Field.Value));
		}
		return Fields;
	}

	static TArray<FBlueprintHelperClassDefaultPropertySetting> ReadTaskRuntimeClassDefaultSettings(
		const TSharedPtr<FJsonObject>& Payload)
	{
		TArray<FBlueprintHelperClassDefaultPropertySetting> Settings;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("settings"), Values) || !Values)
		{
			return Settings;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Object = AsJsonObjectIfObject(Value);
			if (!Object.IsValid())
			{
				continue;
			}

			FBlueprintHelperClassDefaultPropertySetting Setting;
			Object->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
			Setting.Value = Object->TryGetField(TEXT("value"));
			Settings.Add(MoveTemp(Setting));
		}
		return Settings;
	}

	static FBlueprintHelperSetComponentPropertiesRequest ReadComponentPropertiesRequest(
		const TSharedPtr<FJsonObject>& Payload)
	{
		FBlueprintHelperSetComponentPropertiesRequest Request;
		Request.Mode = EBlueprintHelperComponentPropertyMode::Batch;
		if (!Payload.IsValid())
		{
			return Request;
		}

		Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
		Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

		const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
		if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *SettingsArray)
			{
				const TSharedPtr<FJsonObject> ItemObject = AsJsonObjectIfObject(ItemValue);
				if (!ItemObject.IsValid())
				{
					continue;
				}

				FBlueprintHelperComponentPropertySetting Setting;
				ItemObject->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
				Setting.Value = ItemObject->TryGetField(TEXT("value"));
				Request.Settings.Add(MoveTemp(Setting));
			}
		}
		return Request;
	}

	static TSharedRef<FJsonObject> MakeBlueprintVariableOpPayload(
		const FString& AssetPath,
		const FString& FunctionName,
		const TSharedPtr<FJsonObject>& OpObject)
	{
		TSharedRef<FJsonObject> OpPayload = MakeShared<FJsonObject>();
		if (OpObject.IsValid())
		{
			for (const auto& Field : OpObject->Values)
			{
				const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Field.Key);
				if (Key != TEXT("op"))
				{
					OpPayload->SetField(Key, Field.Value);
				}
			}
		}
		OpPayload->SetStringField(TEXT("asset_path"), AssetPath);
		if (!FunctionName.IsEmpty())
		{
			OpPayload->SetStringField(TEXT("function_name"), FunctionName);
		}
		return OpPayload;
	}

	static FBlueprintHelperToolResultBase ExecuteBlueprintVariableBatchTaskPlanStep(
		const FBlueprintHelperBlueprintVariableService& Service,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString FunctionName;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("function_name"), FunctionName);
		}
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (AssetPath.IsEmpty() ||
			!Payload.IsValid() ||
			!Payload->TryGetArrayField(TEXT("ops"), Ops) ||
			!Ops || Ops->Num() == 0)
		{
			return MakeFailure(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TEXT("invalid_variable_batch_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Blueprint variable batch payload requires asset_path and ops."),
				TEXT("task_plan.steps[0]"));
		}

		int32 AppliedCount = 0;
		int32 DryRunCount = 0;
		int32 NoOpCount = 0;
		for (int32 OpIndex = 0; OpIndex < Ops->Num(); ++OpIndex)
		{
			const TSharedPtr<FJsonObject> OpObject = AsJsonObjectIfObject((*Ops)[OpIndex]);
			if (!OpObject.IsValid())
			{
				return MakeFailure(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TEXT("invalid_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("Blueprint variable batch op must be an object."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex));
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			const TSharedRef<FJsonObject> OpPayload = MakeBlueprintVariableOpPayload(AssetPath, FunctionName, OpObject);
			OpPayload->SetBoolField(TEXT("dry_run"), bDryRun);

			FBlueprintHelperToolResultBase OpResult;
			if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureMemberVariable)
			{
				OpResult = Service.AddMemberVariable(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberVariableProperties)
			{
				OpResult = Service.SetMemberVariableProperties(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveMemberVariable)
			{
				OpResult = Service.RemoveMemberVariable(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberDefault)
			{
				OpResult = Service.SetMemberDefault(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureLocalVariable)
			{
				OpResult = Service.AddLocalVariable(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetLocalVariableProperties)
			{
				OpResult = Service.SetLocalVariableProperties(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveLocalVariable)
			{
				OpResult = Service.RemoveLocalVariable(OpPayload);
			}
			else
			{
				return MakeFailure(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TEXT("unsupported_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					FString::Printf(TEXT("Unsupported blueprint variable op: %s."), *OpName),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].op"), OpIndex));
			}

			if (!OpResult.bOk)
			{
				return OpResult;
			}

			if (OpResult.Status == EBlueprintHelperToolStatus::Applied)
			{
				++AppliedCount;
			}
			else if (OpResult.Status == EBlueprintHelperToolStatus::DryRun)
			{
				++DryRunCount;
			}
			else
			{
				++NoOpCount;
			}
		}

		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = (bDryRun || DryRunCount > 0)
			? FBlueprintHelperToolResultBuilder::DryRun(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TraceId)
			: (AppliedCount > 0
				? FBlueprintHelperToolResultBuilder::Applied(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TraceId)
				: FBlueprintHelperToolResultBuilder::NoOp(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TraceId));

		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("blueprint"));
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.BlueprintVariableBatchResult.v1"));
		Data->SetNumberField(TEXT("requested_count"), Ops->Num());
		Data->SetNumberField(TEXT("applied_count"), AppliedCount);
		Data->SetNumberField(TEXT("dry_run_count"), DryRunCount);
		Data->SetNumberField(TEXT("no_op_count"), NoOpCount);
		Result.Data = Data;

		if (!bDryRun && AppliedCount > 0)
		{
			FBlueprintHelperValidationSummary Validation;
			Validation.bShouldCompile = true;
			Validation.bShouldSave = true;
			Result.Validation = Validation;
		}
		return Result;
	}

	static FBlueprintHelperToolResultBase ExecuteComponentTaskPlanStep(
		const FBlueprintHelperComponentService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent)
		{
			FBlueprintHelperAddComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("component_class"), Request.ComponentClass);
				Payload->TryGetStringField(TEXT("parent_component"), Request.ParentComponent);
				Payload->TryGetStringField(TEXT("socket_name"), Request.SocketName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

				FString AttachRule;
				if (Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
				{
					if (!TryParseAttachRule(AttachRule, Request.AttachRule))
					{
						return MakeComponentPolicyParseFailure(TEXT("attach_rule"), AttachRule);
					}
				}
				FString NameCollisionPolicy;
				if (Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy))
				{
					if (!TryParseNameCollisionPolicy(NameCollisionPolicy, Request.NameCollisionPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("name_collision_policy"), NameCollisionPolicy);
					}
				}
			}
			return Service.AddComponent(Request);
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties)
		{
			return Service.SetComponentProperties(ReadComponentPropertiesRequest(Payload));
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRenameComponent)
		{
			FBlueprintHelperRenameComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("new_component_name"), Request.NewComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
			}
			return Service.RenameComponent(Request);
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationReparentComponent)
		{
			FBlueprintHelperReparentComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("new_parent_component"), Request.NewParentComponent);
				Payload->TryGetStringField(TEXT("socket_name"), Request.SocketName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString AttachRule;
				if (Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
				{
					if (!TryParseAttachRule(AttachRule, Request.AttachRule))
					{
						return MakeComponentPolicyParseFailure(TEXT("attach_rule"), AttachRule);
					}
				}
				FString TransformPolicy;
				if (Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
				{
					if (!TryParseComponentTransformPolicy(TransformPolicy, Request.TransformPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("transform_policy"), TransformPolicy);
					}
				}
			}
			return Service.ReparentComponent(Request);
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAttachComponent)
		{
			FBlueprintHelperAttachComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("parent_component"), Request.ParentComponent);
				Payload->TryGetStringField(TEXT("socket_name"), Request.SocketName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString AttachRule;
				if (Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
				{
					if (!TryParseAttachRule(AttachRule, Request.AttachRule))
					{
						return MakeComponentPolicyParseFailure(TEXT("attach_rule"), AttachRule);
					}
				}
				FString TransformPolicy;
				if (Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
				{
					if (!TryParseComponentTransformPolicy(TransformPolicy, Request.TransformPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("transform_policy"), TransformPolicy);
					}
				}
			}
			return Service.AttachComponent(Request);
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationDetachComponent)
		{
			FBlueprintHelperDetachComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString TransformPolicy;
				if (Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
				{
					if (!TryParseComponentTransformPolicy(TransformPolicy, Request.TransformPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("transform_policy"), TransformPolicy);
					}
				}
				FString DefaultRootPolicy;
				if (Payload->TryGetStringField(TEXT("default_root_policy"), DefaultRootPolicy))
				{
					if (!TryParseComponentDefaultRootPolicy(DefaultRootPolicy, Request.DefaultRootPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("default_root_policy"), DefaultRootPolicy);
					}
				}
			}
			return Service.DetachComponent(Request);
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetRootComponent)
		{
			FBlueprintHelperSetRootComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString OldRootPolicy;
				if (Payload->TryGetStringField(TEXT("old_root_policy"), OldRootPolicy))
				{
					if (!TryParseComponentOldRootPolicy(OldRootPolicy, Request.OldRootPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("old_root_policy"), OldRootPolicy);
					}
				}
				FString DefaultRootPolicy;
				if (Payload->TryGetStringField(TEXT("default_root_policy"), DefaultRootPolicy))
				{
					if (!TryParseComponentDefaultRootPolicy(DefaultRootPolicy, Request.DefaultRootPolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("default_root_policy"), DefaultRootPolicy);
					}
				}
			}
			return Service.SetRootComponent(Request);
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRemoveComponent)
		{
			FBlueprintHelperRemoveComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
				FString DeletePolicy;
				if (Payload->TryGetStringField(TEXT("delete_policy"), DeletePolicy))
				{
					if (!TryParseComponentDeletePolicy(DeletePolicy, Request.DeletePolicy))
					{
						return MakeComponentPolicyParseFailure(TEXT("delete_policy"), DeletePolicy);
					}
				}
			}
			return Service.RemoveComponent(Request);
		}

		return MakeFailure(
			TEXT("blueprint_component"),
			TEXT("unsupported_component_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported component adapter operation."));
	}

	static FBlueprintHelperToolResultBase ExecuteClassSettingsTaskPlanStep(
		const FBlueprintHelperClassSettingsService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::AddImplementedInterfacesOp)
		{
			return Service.AddImplementedInterfaces(AssetPath, ReadTaskRuntimeStringArrayField(Payload, TEXT("interface_paths")), bDryRun);
		}
		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::RemoveImplementedInterfacesOp)
		{
			return Service.RemoveImplementedInterfaces(AssetPath, ReadTaskRuntimeStringArrayField(Payload, TEXT("interface_paths")), bDryRun);
		}
		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::SetClassDefaultPropertiesOp)
		{
			return Service.SetClassDefaultProperties(AssetPath, ReadTaskRuntimeClassDefaultSettings(Payload), bDryRun);
		}
		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::ReparentBlueprintOp)
		{
			FString NewParentClass;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("new_parent_class"), NewParentClass);
			}
			return Service.ReparentBlueprint(AssetPath, NewParentClass, bDryRun);
		}

		return MakeFailure(
			TEXT("blueprint_class_settings"),
			TEXT("unsupported_class_settings_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported class settings adapter operation."));
	}

	static FBlueprintHelperToolResultBase MakeDataTableMutationResult(
		const FString& Operation,
		const TSharedPtr<FJsonObject>& Payload,
		const FBlueprintHelperDataTableMutationResult& MutationResult)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = MutationResult.bSuccess
			? (MutationResult.bDryRun
				? FBlueprintHelperToolResultBuilder::DryRun(Operation, TraceId)
				: FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId))
			: FBlueprintHelperToolResultBuilder::Failure(
				Operation,
				TraceId,
				MakeTaskRuntimeError(
					TEXT("data_table_operation_failed"),
					EBlueprintHelperToolStage::Execute,
					MutationResult.ErrorMessage));

		FString AssetPath;
		FString RowName;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("row_name"), RowName);
		}
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("data_table_row"), TEXT(""), RowName);

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("DataTableMutation.v1"));
		Data->SetBoolField(TEXT("dry_run"), MutationResult.bDryRun);
		Data->SetStringField(TEXT("row_name"), MutationResult.AffectedRow.ToString());
		Result.Data = Data;
		return Result;
	}

	static FBlueprintHelperToolResultBase ExecuteDataTableTaskPlanStep(
		const FBlueprintHelperDataTableService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString RowName;
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("row_name"), RowName);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationAddRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.AddDataTableRow(AssetPath, RowName, ReadTaskRuntimeStringFieldsObject(Payload), bDryRun));
		}
		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationUpdateRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.UpdateDataTableRow(AssetPath, RowName, ReadTaskRuntimeStringFieldsObject(Payload), bDryRun));
		}
		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationDeleteRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.DeleteDataTableRow(AssetPath, RowName, bDryRun));
		}

		return MakeFailure(
			TEXT("data_table"),
			TEXT("unsupported_data_table_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported DataTable adapter operation."));
	}

	static bool TryBuildObjectPropertyRequest(
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperSetObjectPropertiesRequest& OutRequest,
		FString& OutError)
	{
		OutRequest = FBlueprintHelperSetObjectPropertiesRequest();
		OutError.Reset();
		if (!Payload.IsValid())
		{
			OutError = TEXT("object_property adapter payload is required.");
			return false;
		}

		Payload->TryGetStringField(TEXT("asset_path"), OutRequest.AssetPath);
		Payload->TryGetBoolField(TEXT("dry_run"), OutRequest.bDryRun);
		if (OutRequest.AssetPath.IsEmpty())
		{
			OutError = TEXT("object_property adapter payload requires asset_path.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
		if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
		{
			for (const TSharedPtr<FJsonValue>& SettingValue : *SettingsArray)
			{
				const TSharedPtr<FJsonObject> SettingObject = AsJsonObjectIfObject(SettingValue);
				if (!SettingObject.IsValid())
				{
					OutError = TEXT("object_property settings entries must be objects.");
					return false;
				}

				FBlueprintHelperObjectPropertySetting Setting;
				SettingObject->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
				if (Setting.PropertyPath.IsEmpty())
				{
					OutError = TEXT("object_property settings entries require property_path.");
					return false;
				}

				const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(SettingObject, TEXT("value"));
				if (!Value.IsValid())
				{
					OutError = TEXT("object_property settings entries require value.");
					return false;
				}

				Setting.Value = Value;
				OutRequest.Settings.Add(MoveTemp(Setting));
			}
			if (OutRequest.Settings.Num() == 0)
			{
				OutError = TEXT("object_property settings cannot be empty.");
				return false;
			}
			return true;
		}

		FBlueprintHelperObjectPropertySetting Setting;
		Payload->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
		if (Setting.PropertyPath.IsEmpty())
		{
			OutError = TEXT("object_property adapter payload requires property_path.");
			return false;
		}

		const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(Payload, TEXT("value"));
		if (!Value.IsValid())
		{
			OutError = TEXT("object_property adapter payload requires value.");
			return false;
		}

		Setting.Value = Value;
		OutRequest.Settings.Add(MoveTemp(Setting));
		return true;
	}

	static FBlueprintHelperToolResultBase ExecuteObjectPropertyTaskPlanStep(
		const FBlueprintHelperPropertyReflectionService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FBlueprintHelperSetObjectPropertiesRequest Request;
		FString Error;
		if (!TryBuildObjectPropertyRequest(Payload, Request, Error))
		{
			return MakeFailure(
				TEXT("object_property"),
				TEXT("invalid_object_property_adapter_payload"),
				EBlueprintHelperToolStage::ParseInput,
				Error);
		}

		if (AdapterOperation == FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperty)
		{
			return Service.SetObjectProperty(Request);
		}
		if (AdapterOperation == FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperties)
		{
			return Service.SetObjectProperties(Request);
		}

		return MakeFailure(
			TEXT("object_property"),
			TEXT("unsupported_object_property_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported object_property adapter operation."));
	}

	static FBlueprintHelperToolResultBase ExecuteSignatureTaskPlanStep(
		const FBlueprintHelperBlueprintStructureService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		const bool bEnsureFunction = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureFunction;
		const bool bEnsureCustomEvent = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureCustomEvent;
		const bool bRemoveSignature = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationRemoveSignature;
		const bool bEnsureEventDispatcher = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureEventDispatcher;
		const bool bEnsureOverrideEvent = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureOverrideEvent;
		if (!bEnsureFunction && !bEnsureCustomEvent && !bRemoveSignature && !bEnsureEventDispatcher && !bEnsureOverrideEvent)
		{
			return MakeFailure(
				TEXT("blueprint_signature"),
				TEXT("unsupported_signature_adapter_operation"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Unsupported signature adapter operation."));
		}

		FString AssetPath;
		FString FunctionName;
		FString EventName;
		FString GraphName;
		FString DispatcherName;
		FString EventKind;
		FString SignatureKind;
		FString SignatureName;
		FString NameCollisionPolicy = TEXT("reuse_if_exists");
		FString InterfaceEntryKind;
		FString SignatureMismatchPolicy;
		FString ExecutePolicy;
		bool bIsPure = false;
		bool bDryRun = false;
		bool bRequireReferenceContext = true;
		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("function_name"), FunctionName);
			Payload->TryGetStringField(TEXT("event_name"), EventName);
			Payload->TryGetStringField(TEXT("graph_name"), GraphName);
			Payload->TryGetStringField(TEXT("dispatcher_name"), DispatcherName);
			Payload->TryGetStringField(TEXT("event_kind"), EventKind);
			Payload->TryGetStringField(TEXT("signature_kind"), SignatureKind);
			Payload->TryGetStringField(TEXT("signature_name"), SignatureName);
			Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy);
			Payload->TryGetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind);
			Payload->TryGetStringField(TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
			Payload->TryGetStringField(TEXT("execute_policy"), ExecutePolicy);
			Payload->TryGetBoolField(TEXT("is_pure"), bIsPure);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
			Payload->TryGetBoolField(TEXT("require_reference_context"), bRequireReferenceContext);
			Payload->TryGetArrayField(TEXT("inputs"), Inputs);
			Payload->TryGetArrayField(TEXT("outputs"), Outputs);
		}

		FBlueprintHelperSignatureService SignatureService(Service);

		if (bEnsureFunction)
		{
			FBlueprintHelperEnsureFunctionSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.FunctionName = FunctionName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			if (!SignatureMismatchPolicy.IsEmpty())
			{
				Request.SignatureMismatchPolicy = SignatureMismatchPolicy;
			}
			Request.bDryRun = bDryRun;
			Request.bIsPure = bIsPure;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("interface_path"), Request.InterfacePath);
			}
			Request.InterfaceEntryKind = InterfaceEntryKind;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			if (Outputs)
			{
				Request.Outputs = *Outputs;
			}
			return SignatureService.EnsureFunction(Request);
		}

		if (bEnsureCustomEvent)
		{
			FBlueprintHelperEnsureCustomEventSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.GraphName = GraphName;
			Request.EventName = EventName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			Request.bDryRun = bDryRun;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("interface_path"), Request.InterfacePath);
			}
			Request.InterfaceEntryKind = InterfaceEntryKind;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureCustomEvent(Request);
		}

		if (bEnsureEventDispatcher)
		{
			FBlueprintHelperEnsureEventDispatcherSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.DispatcherName = DispatcherName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			if (!SignatureMismatchPolicy.IsEmpty())
			{
				Request.SignatureMismatchPolicy = SignatureMismatchPolicy;
			}
			Request.bDryRun = bDryRun;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureEventDispatcher(Request);
		}

		if (bEnsureOverrideEvent)
		{
			FBlueprintHelperEnsureOverrideEventSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.GraphName = GraphName;
			Request.EventName = EventName;
			Request.EventKind = EventKind;
			if (!ExecutePolicy.IsEmpty())
			{
				Request.ExecutePolicy = ExecutePolicy;
			}
			Request.bDryRun = bDryRun;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureOverrideEvent(Request);
		}

		FBlueprintHelperRemoveSignatureRequest Request;
		Request.AssetPath = AssetPath;
		Request.GraphName = GraphName;
		Request.SignatureKind = SignatureKind;
		Request.SignatureName = SignatureName;
		if (!ExecutePolicy.IsEmpty())
		{
			Request.ExecutePolicy = ExecutePolicy;
		}
		Request.bDryRun = bDryRun;
		Request.bRequireReferenceContext = bRequireReferenceContext;
		return SignatureService.RemoveSignature(Request);
	}

};

FBlueprintHelperTaskRuntimeService::FBlueprintHelperTaskRuntimeService(
	const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry,
	const FBlueprintHelperBlueprintVariableService& InVariableService,
	const FBlueprintHelperBlueprintStructureService& InStructureService,
	const FBlueprintHelperAssetFactoryService& InAssetFactoryService,
	const FBlueprintHelperComponentService& InComponentService,
	const FBlueprintHelperClassSettingsService& InClassSettingsService,
	const FBlueprintHelperWidgetService& InWidgetService,
	const FBlueprintHelperDataTableService& InDataTableService,
	const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService,
	const FBlueprintHelperCompileAssetService& InCompileAssetService,
	const FBlueprintHelperAssetBrowseService& InAssetBrowseService,
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: ClusterHub(MakeUnique<FBlueprintHelperTaskRuntimeClusterHub>(
		InGraphWriteRegistry,
		InVariableService,
		InStructureService,
		InAssetFactoryService,
		InComponentService,
		InClassSettingsService,
		InWidgetService,
		InDataTableService,
		InPropertyReflectionService))
	, PreviewStore(MakeUnique<FBlueprintHelperTaskPreviewStore>())
	, PartialPreviewCache(MakeUnique<FBlueprintHelperTaskPartialPreviewCache>())
	, CallFunctionResolutionCache(MakeUnique<FBlueprintHelperTaskRuntimeCallFunctionResolutionCache>())
	, GraphWritePlanCache(MakeUnique<FBlueprintHelperGraphWritePlanCache>())
	, GraphWriteCandidateArtifactStore(MakeUnique<FBlueprintHelperGraphWriteCandidateArtifactStore>())
	, CompileAssetService(InCompileAssetService)
	, AssetBrowseService(InAssetBrowseService)
	, DebugEntryService(InDebugEntryService)
{
}

FBlueprintHelperTaskRuntimeService::~FBlueprintHelperTaskRuntimeService() = default;

FBlueprintHelperValidationSummary FBlueprintHelperTaskRuntimeService::BuildRuntimeValidation(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FBlueprintHelperValidationSummary& BaseValidation)
{
	FBlueprintHelperValidationSummary RuntimeValidation = BaseValidation;

	const FBlueprintHelperTaskRuntimeExecutionPolicySettings ExecutionPolicy =
		FBlueprintHelperTaskRuntimeSettingsResolver::ResolveExecutionPolicy(TaskPlan);
	RuntimeValidation.bShouldCompile = ExecutionPolicy.bShouldCompile;
	RuntimeValidation.bShouldSave = ExecutionPolicy.bShouldSave;

	return RuntimeValidation;
}

bool FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
			TEXT("invalid_taskplan_step"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("TaskPlan step must be an object."),
			TEXT("task_plan.steps[0]"));
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutLoweredStep.StepId);
	if (OutLoweredStep.StepId.IsEmpty())
	{
		OutLoweredStep.StepId = TEXT("step_001");
	}

	FString Capability;
	StepObject->TryGetStringField(TEXT("capability"), Capability);
	if (Capability == TEXT("graph_write"))
	{
		FString OperationField;
		if (StepObject->TryGetStringField(TEXT("operation"), OperationField))
		{
			OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_operation_field"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				TEXT("task_plan.steps[0].operation"));
			return false;
		}

		const FString StepId = OutLoweredStep.StepId;
		FBlueprintHelperGraphWriteLoweringRequest GraphWriteRequest;
		GraphWriteRequest.TaskPlan = TaskPlan;
		GraphWriteRequest.StepObject = StepObject;
		GraphWriteRequest.bDryRun = bDryRun;

		FBlueprintHelperGraphWriteLoweringResult GraphWriteResult;
		if (!FBlueprintHelperGraphWriteRuntimeDispatcher::TryLower(
			GraphWriteRequest,
			GraphWriteResult,
			OutError))
		{
			return false;
		}

		OutLoweredStep = GraphWriteResult.LoweredStep;
		OutLoweredStep.StepId = StepId;
		return true;
	}

	if (Capability == TEXT("blueprint_variable"))
	{
		FBlueprintHelperBlueprintVariableTaskPlanPayload BuiltPayload;
		if (!FBlueprintHelperBlueprintVariableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			StepObject,
			bDryRun,
			BuiltPayload,
			OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = BuiltPayload.Capability;
		OutLoweredStep.RuntimeOperation = BuiltPayload.RuntimeOperation;
		OutLoweredStep.AdapterOperation = BuiltPayload.AdapterOperation;
		OutLoweredStep.Payload = BuiltPayload.Payload;
		OutLoweredStep.bAdapterDryRunSupported = BuiltPayload.bAdapterDryRunSupported;
		return true;
	}

	if (Capability == FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability)
	{
		FString AdapterOperation;
		if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperation))
		{
			OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
				TEXT("unsupported_asset_factory_operation_field"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("asset_factory IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				TEXT("task_plan.steps[0].operation"));
			return false;
		}

		TSharedPtr<FJsonObject> Payload;
		if (!FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			TaskPlan, StepObject, bDryRun, Payload, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability;
		OutLoweredStep.RuntimeOperation = FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability;
		OutLoweredStep.AdapterOperation = FBlueprintHelperAssetFactoryTaskPlanAdapter::AdapterOperation;
		OutLoweredStep.Payload = Payload;
		OutLoweredStep.bAdapterDryRunSupported = true;
		return true;
	}

	if (Capability == FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent)
	{
		FBlueprintHelperComponentTaskPlanPayload Payload;
		if (!FBlueprintHelperComponentTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			StepObject, bDryRun, Payload, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = Payload.Capability;
		OutLoweredStep.RuntimeOperation = Payload.RuntimeOperation;
		OutLoweredStep.AdapterOperation = Payload.AdapterOperation;
		OutLoweredStep.Payload = Payload.Payload;
		OutLoweredStep.bAdapterDryRunSupported = Payload.bAdapterDryRunSupported;
		return true;
	}

	if (Capability == FBlueprintHelperClassSettingsTaskPlanAdapter::CapabilityName)
	{
		return FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (Capability == FBlueprintHelperSignatureTaskPlanAdapter::CapabilityName)
	{
		return FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (Capability == FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget)
	{
		FBlueprintHelperWidgetTaskPlanPayload Payload;
		if (!FBlueprintHelperWidgetTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			TaskPlan, StepObject, bDryRun, Payload, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = Payload.Capability;
		OutLoweredStep.RuntimeOperation = Payload.RuntimeOperation;
		OutLoweredStep.AdapterOperation = Payload.AdapterOperation;
		OutLoweredStep.Payload = Payload.Payload;
		OutLoweredStep.bAdapterDryRunSupported = Payload.bAdapterDryRunSupported;
		return true;
	}

	if (Capability == FBlueprintHelperDataTableTaskPlanAdapter::CapabilityDataTable)
	{
		return FBlueprintHelperDataTableTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (Capability == FBlueprintHelperObjectPropertyTaskPlanAdapter::CapabilityObjectProperty)
	{
		return FBlueprintHelperObjectPropertyTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (Capability == FBlueprintHelperMaterialInstanceTaskPlanAdapter::CapabilityMaterialInstance)
	{
		return FBlueprintHelperMaterialInstanceTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (!Capability.IsEmpty())
	{
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
			TEXT("unsupported_taskplan_capability"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Task Runtime does not support this TaskPlan capability yet."),
			TEXT("task_plan.steps[0].capability"));
		return false;
	}

	FString StepOperation;
	StepObject->TryGetStringField(TEXT("operation"), StepOperation);
	if (!FBlueprintHelperTaskRuntimeServiceLocalUtils::IsGraphWriteTaskPlanOperation(StepOperation))
	{
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
			TEXT("unsupported_taskplan_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Task Runtime currently supports graph write TaskPlan steps only."),
			TEXT("task_plan.steps[0].operation"));
		return false;
	}

	FString StepPayloadErrorCode;
	FString StepPayloadErrorMessage;
	FString StepPayloadErrorField;
	TSharedRef<FJsonObject> StepPayload = FBlueprintHelperTaskRuntimeServiceLocalUtils::BuildGraphWritePayload(
		StepOperation,
		StepObject,
		bDryRun,
		StepPayloadErrorCode,
		StepPayloadErrorMessage,
		StepPayloadErrorField);
	if (!StepPayloadErrorCode.IsEmpty())
	{
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
			StepPayloadErrorCode,
			EBlueprintHelperToolStage::ParseInput,
			StepPayloadErrorMessage,
			StepPayloadErrorField);
		return false;
	}

	OutLoweredStep.RuntimeOperation = StepOperation;
	OutLoweredStep.AdapterOperation = StepOperation;
	OutLoweredStep.Payload = StepPayload;
	return true;
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForStep(
	const FString& TaskRunId,
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult)
{
	return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(TaskRunId, TaskPlan, LoweredStep, StepResult);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForSteps(
	const FString& TaskRunId,
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
	const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords)
{
	return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(TaskRunId, TaskPlan, StepRecords, PostOperationRecords);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::PreviewTaskPlan(
	const TSharedPtr<FJsonObject>& Payload) const
{
	FBlueprintHelperToolResultBase Result = RunTaskPlan(Payload, true);
	AttachPreviewToken(Payload, Result);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::ExecuteTaskPlan(
	const TSharedPtr<FJsonObject>& Payload) const
{
	if (Payload.IsValid() && Payload->HasTypedField<EJson::String>(TEXT("preview_token")))
	{
		return ExecutePreviewTokenTaskPlan(Payload);
	}
	return RunTaskPlan(Payload, false);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::GetTaskRunJournal(
	const FString& TaskRunId) const
{
	const TSharedPtr<FJsonObject>* FoundJournal = TaskRunJournals.Find(TaskRunId);
	if (!FoundJournal || !FoundJournal->IsValid())
	{
		TSharedPtr<FJsonObject> LoadedJournal;
		FString LoadError;
		if (!FBlueprintHelperTaskRunJournalStoreService().LoadTaskRunJournal(
			TaskRunId,
			LoadedJournal,
			LoadError))
		{
			return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
				TEXT("get_task_run_journal"),
				TEXT("task_run_journal_not_found"),
				EBlueprintHelperToolStage::ResolveTarget,
				FString::Printf(TEXT("TaskRunJournal not found: %s. %s"), *TaskRunId, *LoadError),
				TEXT("task_run_id"));
		}
		TaskRunJournals.Add(TaskRunId, LoadedJournal);
		FoundJournal = TaskRunJournals.Find(TaskRunId);
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("get_task_run_journal"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Data = *FoundJournal;
	return Result;
}

void FBlueprintHelperTaskRuntimeService::AttachPreviewToken(
	const TSharedPtr<FJsonObject>& Payload,
	FBlueprintHelperToolResultBase& Result) const
{
	auto StripGraphWriteCandidateArtifact = [&Result]()
	{
		FBlueprintHelperTaskRuntimeServiceLocalUtils::RemoveJsonFieldRecursive(
			Result.Data,
			TEXT("graph_write_candidate_artifact"));
	};

	if (!Payload.IsValid() || !PreviewStore.IsValid())
	{
		StripGraphWriteCandidateArtifact();
		return;
	}

	const TSharedPtr<FJsonObject>* TaskPlanPtr = nullptr;
	const TSharedPtr<FJsonObject>* TokenRequestPtr = nullptr;
	if (!Payload->TryGetObjectField(TEXT("task_plan"), TaskPlanPtr) ||
		!TaskPlanPtr || !TaskPlanPtr->IsValid() ||
		!Payload->TryGetObjectField(TEXT("preview_token_request"), TokenRequestPtr) ||
		!TokenRequestPtr || !TokenRequestPtr->IsValid())
	{
		StripGraphWriteCandidateArtifact();
		return;
	}

	FString TaskSpecHash;
	FString TaskPlanHash;
	FString ExecutionPolicyHash;
	if (!(*TokenRequestPtr)->TryGetStringField(TEXT("task_spec_hash"), TaskSpecHash) ||
		!(*TokenRequestPtr)->TryGetStringField(TEXT("task_plan_hash"), TaskPlanHash) ||
		!(*TokenRequestPtr)->TryGetStringField(TEXT("execution_policy_hash"), ExecutionPolicyHash))
	{
		StripGraphWriteCandidateArtifact();
		return;
	}

	FBlueprintHelperTaskPreviewStoreCreateRequest StoreRequest;
	StoreRequest.TaskPlan = *TaskPlanPtr;
	StoreRequest.TaskSpecHash = TaskSpecHash;
	StoreRequest.TaskPlanHash = TaskPlanHash;
	StoreRequest.ExecutionPolicyHash = ExecutionPolicyHash;
	StoreRequest.AssetStateHash = FBlueprintHelperTaskRuntimeServiceLocalUtils::BuildTargetAssetStateHash(*TaskPlanPtr);
	const FBlueprintHelperTaskRuntimeContextRevisionManifest ContextRevisionManifest =
		FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder::BuildFromTaskPlan(*TaskPlanPtr);
	StoreRequest.ActionContextRevisionManifestHash = ContextRevisionManifest.ManifestHash;
	StoreRequest.ActionContextRevisionManifestJson = ContextRevisionManifest.ToJson();
	StoreRequest.bPassed = FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPreviewResultExecutable(Result);
	if (Result.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* CandidateArtifactPtr = nullptr;
		if (Result.Data->TryGetObjectField(TEXT("graph_write_candidate_artifact"), CandidateArtifactPtr) &&
			CandidateArtifactPtr && CandidateArtifactPtr->IsValid())
		{
			StoreRequest.GraphWriteCandidateArtifactJson = *CandidateArtifactPtr;
			StoreRequest.GraphWriteCandidateArtifactHash =
				FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(*CandidateArtifactPtr);
		}
		StripGraphWriteCandidateArtifact();
	}

	const FString Token = PreviewStore->Store(StoreRequest);
	if (!Result.Data.IsValid())
	{
		Result.Data = MakeShared<FJsonObject>();
	}
	Result.Data->SetStringField(TEXT("preview_token"), Token);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::ExecutePreviewTokenTaskPlan(
	const TSharedPtr<FJsonObject>& Payload) const
{
	if (!Payload.IsValid() || !PreviewStore.IsValid())
	{
		return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
			TEXT("execute_task_plan"),
			TEXT("invalid_preview_token_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload with preview_token is required."),
			TEXT("payload"));
	}

	FString PreviewToken;
	FString TaskSpecHash;
	if (!Payload->TryGetStringField(TEXT("preview_token"), PreviewToken) ||
		!Payload->TryGetStringField(TEXT("task_spec_hash"), TaskSpecHash))
	{
		return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
			TEXT("execute_task_plan"),
			TEXT("invalid_preview_token_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("payload.preview_token and payload.task_spec_hash are required for token execute."),
			TEXT("payload.preview_token"));
	}

	const FBlueprintHelperTaskPreviewStoreResolveResult ResolveResult =
		PreviewStore->Resolve(PreviewToken, TaskSpecHash);
	if (!ResolveResult.bOk)
	{
		return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
			TEXT("execute_task_plan"),
			ResolveResult.ErrorCode,
			EBlueprintHelperToolStage::Preflight,
			ResolveResult.ErrorMessage,
			ResolveResult.ErrorField);
	}

	if (!ResolveResult.bPassed)
	{
		return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
			TEXT("execute_task_plan"),
			TEXT("task_preview_blocked"),
			EBlueprintHelperToolStage::DryRun,
			TEXT("Stored preview was blocked; execute_task_plan did not write assets."),
			TEXT("preview_token"));
	}

	if (!ResolveResult.TaskPlan.IsValid())
	{
		return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
			TEXT("execute_task_plan"),
			TEXT("preview_token_task_plan_missing"),
			EBlueprintHelperToolStage::Preflight,
			TEXT("Stored preview token did not contain a valid TaskPlan."),
			TEXT("preview_token"));
	}

	const FBlueprintHelperTaskRuntimeContextRevisionManifest ExpectedContextRevisionManifest =
		FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder::BuildFromJson(
			ResolveResult.ActionContextRevisionManifestJson);
	const FBlueprintHelperTaskRuntimeContextRevisionManifest CurrentContextRevisionManifest =
		FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder::BuildFromTaskPlan(ResolveResult.TaskPlan);
	FBlueprintHelperTaskRuntimeContextRevisionMismatch RevisionMismatch;
	if (!FBlueprintHelperTaskRuntimeContextRevisionManifest::Compare(
		ExpectedContextRevisionManifest,
		CurrentContextRevisionManifest,
		RevisionMismatch))
	{
		FBlueprintHelperToolError Error = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
			RevisionMismatch.Code,
			EBlueprintHelperToolStage::Preflight,
			RevisionMismatch.Message,
			RevisionMismatch.Field);
		Error.bRetryable = true;
		FBlueprintHelperToolResultBase Failure = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("execute_task_plan"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
		Failure.Data = MakeShared<FJsonObject>();
		Failure.Data->SetStringField(TEXT("detail_code"), RevisionMismatch.DetailCode);
		if (RevisionMismatch.Expected.IsValid())
		{
			Failure.Data->SetObjectField(TEXT("expected_context_revision"), RevisionMismatch.Expected);
		}
		if (RevisionMismatch.Current.IsValid())
		{
			Failure.Data->SetObjectField(TEXT("current_context_revision"), RevisionMismatch.Current);
		}
		Failure.Data->SetStringField(TEXT("refresh_hint"), TEXT("run preview_task again"));
		Failure.Data->SetStringField(TEXT("agent_action"), TEXT("refresh_context_and_preview"));
		return Failure;
	}

	const FString CurrentAssetStateHash =
		FBlueprintHelperTaskRuntimeServiceLocalUtils::BuildTargetAssetStateHash(ResolveResult.TaskPlan);
	if (CurrentAssetStateHash != ResolveResult.AssetStateHash)
	{
		return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
			TEXT("execute_task_plan"),
			TEXT("preview_token_mismatch"),
			EBlueprintHelperToolStage::Preflight,
			TEXT("Target asset state changed after preview; run preview_task again."),
			TEXT("preview_token.asset_state"));
	}

	TSharedRef<FJsonObject> ResolvedPayload = MakeShared<FJsonObject>();
	ResolvedPayload->SetObjectField(TEXT("task_plan"), ResolveResult.TaskPlan.ToSharedRef());
	if (ResolveResult.GraphWriteCandidateArtifactJson.IsValid())
	{
		ResolvedPayload->SetObjectField(
			TEXT("graph_write_candidate_artifact"),
			ResolveResult.GraphWriteCandidateArtifactJson.ToSharedRef());
	}
	bool bIncludeTiming = false;
	if (Payload->TryGetBoolField(TEXT("include_timing"), bIncludeTiming))
	{
		ResolvedPayload->SetBoolField(TEXT("include_timing"), bIncludeTiming);
	}
	return RunTaskPlan(ResolvedPayload, false);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::RunTaskPlan(
	const TSharedPtr<FJsonObject>& Payload,
	bool bDryRun) const
{
	const FString RuntimeOperation = bDryRun ? TEXT("preview_task_plan") : TEXT("execute_task_plan");
	FBlueprintHelperTaskRuntimeReviewIoBatch PostIoBatch;
	if (!bDryRun)
	{
		PostIoBatch.EnablePendingReviewNotification();
	}
	bool bIncludeTiming = false;
	if (Payload.IsValid())
	{
		Payload->TryGetBoolField(TEXT("include_timing"), bIncludeTiming);
	}
	FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace TimingTrace =
		FBlueprintHelperTaskRuntimeTimingUtils::StartTrace(RuntimeOperation, bIncludeTiming);
	auto AttachTimingToResult = [&TimingTrace](FBlueprintHelperToolResultBase& Result)
	{
		if (!TimingTrace.bEnabled)
		{
			return;
		}

		if (!Result.Data.IsValid())
		{
			Result.Data = MakeShared<FJsonObject>();
		}
		FBlueprintHelperTaskRuntimeTimingUtils::AttachTiming(Result.Data, TimingTrace);
	};

	FBlueprintHelperTaskRuntimePreparedTaskRun PreparedRun;
	FBlueprintHelperToolError PrepareError;
	const double PurePrepareStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
	const FBlueprintHelperTaskRuntimePrepareService PrepareService;
	if (!PrepareService.Prepare(Payload, bDryRun, PreparedRun, PrepareError))
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("pure_prepare"), PurePrepareStageStart);
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			PrepareError);
		if (bDryRun &&
			PrepareError.Code == TEXT("dry_run_mode_none_requires_preview_token") &&
			PreparedRun.TaskPlan.IsValid())
		{
			if (!Result.Data.IsValid())
			{
				Result.Data = MakeShared<FJsonObject>();
			}
			FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachDryRunStrategy(
				Result.Data,
				PreparedRun.DryRunPolicy);
		}
		AttachTimingToResult(Result);
		return Result;
	}
	FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("pure_prepare"), PurePrepareStageStart);

	const TSharedPtr<FJsonObject>* TaskPlanPtr = &PreparedRun.TaskPlan;
	TSharedPtr<FJsonObject> PreviewCandidateArtifactJson;
	const TSharedPtr<FJsonObject>* PreviewCandidateArtifactPtr = nullptr;
	if (Payload.IsValid() &&
		Payload->TryGetObjectField(TEXT("graph_write_candidate_artifact"), PreviewCandidateArtifactPtr) &&
		PreviewCandidateArtifactPtr && PreviewCandidateArtifactPtr->IsValid())
	{
		PreviewCandidateArtifactJson = *PreviewCandidateArtifactPtr;
	}
	const FBlueprintHelperTaskRuntimeDryRunPolicy& DryRunPolicy = PreparedRun.DryRunPolicy;
	const bool bQuickDryRun = PreparedRun.bQuickDryRun;
	const FString& TaskRunId = PreparedRun.TaskRunId;
	const FString& ArchiveSessionId = PreparedRun.ArchiveSessionId;
	FBlueprintHelperTaskRuntimePipelineRunner PipelineRunner(*TaskPlanPtr, TaskRunId, bDryRun);
	FBlueprintHelperTaskRuntimePipelineExecutionContext PipelineContext =
		PipelineRunner.MakeExecutionContext();
	auto MakePipelineStageExecutor = [](
		EBlueprintHelperTaskRuntimePipelineStage Stage,
		FBlueprintHelperTaskRuntimeCallbackStageExecutor::FCallback Callback =
			FBlueprintHelperTaskRuntimeCallbackStageExecutor::FCallback())
		-> TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>
	{
		return MakeUnique<FBlueprintHelperTaskRuntimeCallbackStageExecutor>(Stage, MoveTemp(Callback));
	};
	auto ExecutePipelineBatch = [&PipelineRunner, &PipelineContext](
		TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> Executors)
	{
		FBlueprintHelperTaskRuntimePipeline Pipeline(MoveTemp(Executors));
		return Pipeline.Execute(PipelineRunner, PipelineContext);
	};
	auto ExecutePipelineStage = [&MakePipelineStageExecutor, &ExecutePipelineBatch](
		EBlueprintHelperTaskRuntimePipelineStage Stage,
		FBlueprintHelperTaskRuntimeCallbackStageExecutor::FCallback Callback =
			FBlueprintHelperTaskRuntimeCallbackStageExecutor::FCallback())
	{
		TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> Executors;
		Executors.Add(MakePipelineStageExecutor(Stage, MoveTemp(Callback)));
		return ExecutePipelineBatch(MoveTemp(Executors));
	};
	auto AttachPipelineToResult = [&PipelineRunner](FBlueprintHelperToolResultBase& RuntimeResult)
	{
		PipelineRunner.AttachToResult(RuntimeResult);
	};
	TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> InitialPipelineStages;
	InitialPipelineStages.Add(MakePipelineStageExecutor(
		EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract));
	InitialPipelineStages.Add(MakePipelineStageExecutor(
		EBlueprintHelperTaskRuntimePipelineStage::ResolveBridgeRoute));
	ExecutePipelineBatch(MoveTemp(InitialPipelineStages));
	const FBlueprintHelperTaskRuntimeCacheConfig CacheConfig = FBlueprintHelperTaskRuntimeCacheConfig::Default();
	if (PartialPreviewCache.IsValid())
	{
		PartialPreviewCache->ResetRequestStats();
	}
	if (CallFunctionResolutionCache.IsValid())
	{
		CallFunctionResolutionCache->ResetRequestStats();
	}
	if (GraphWritePlanCache.IsValid())
	{
		GraphWritePlanCache->ResetRequestStats();
	}
	const FString TargetAssetStateHash =
		FBlueprintHelperTaskRuntimeServiceLocalUtils::BuildTargetAssetStateHash(*TaskPlanPtr);
	const FBlueprintHelperTaskRuntimeContextRevisionManifest TargetContextRevisionManifest =
		FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder::BuildFromTaskPlan(*TaskPlanPtr);
	const FString TargetContextRevisionManifestHash = TargetContextRevisionManifest.ManifestHash;

	auto BuildExecutionPolicyHash = [&]() -> FString
	{
		const FBlueprintHelperTaskRuntimeExecutionPolicySettings ExecutionPolicy =
			FBlueprintHelperTaskRuntimeSettingsResolver::ResolveExecutionPolicy(*TaskPlanPtr);
		return FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(
			FBlueprintHelperTaskRuntimeSettingsResolver::MakeExecutionPolicyJson(ExecutionPolicy));
	};

	auto BuildTaskSpecGroupHash = [&]() -> FString
	{
		TSharedRef<FJsonObject> GroupObject = MakeShared<FJsonObject>();
		TArray<FString> TargetAssets =
			FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadTargetAssets(*TaskPlanPtr);
		TargetAssets.Sort();
		TArray<TSharedPtr<FJsonValue>> TargetAssetValues;
		for (const FString& TargetAsset : TargetAssets)
		{
			TargetAssetValues.Add(MakeShared<FJsonValueString>(TargetAsset));
		}
		GroupObject->SetArrayField(TEXT("target_assets"), TargetAssetValues);
		FString TaskName;
		(*TaskPlanPtr)->TryGetStringField(TEXT("task_name"), TaskName);
		GroupObject->SetStringField(TEXT("task_name"), TaskName);
		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		if ((*TaskPlanPtr)->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) &&
			ExecutionPolicyPtr && ExecutionPolicyPtr->IsValid())
		{
			TSharedPtr<FJsonObject> ClonedExecutionPolicy =
				FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneJsonObject(*ExecutionPolicyPtr);
			if (ClonedExecutionPolicy.IsValid())
			{
				GroupObject->SetObjectField(TEXT("execution_policy"), ClonedExecutionPolicy.ToSharedRef());
			}
		}
		return FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(GroupObject);
	};

	const FString ExecutionPolicyHash = BuildExecutionPolicyHash();
	const FString TaskSpecGroupHash = BuildTaskSpecGroupHash();
	TMap<FString, FString> StepPayloadHashes;
	TMap<FString, TArray<FString>> StepDependencyIds;
	for (const FBlueprintHelperTaskRuntimePreparedStep& PreparedStep : PreparedRun.Steps)
	{
		StepPayloadHashes.Add(
			PreparedStep.StepId,
			FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(PreparedStep.LoweredStep.Payload));
		StepDependencyIds.Add(PreparedStep.StepId, PreparedStep.DependsOn);
	}

	TFunction<void(const FString&, TSet<FString>&)> CollectDependencyClosure =
		[&](const FString& StepId, TSet<FString>& InOutClosure)
	{
		const TArray<FString>* Dependencies = StepDependencyIds.Find(StepId);
		if (!Dependencies)
		{
			return;
		}
		for (const FString& DependencyId : *Dependencies)
		{
			if (InOutClosure.Contains(DependencyId))
			{
				continue;
			}
			InOutClosure.Add(DependencyId);
			CollectDependencyClosure(DependencyId, InOutClosure);
		}
	};

	auto BuildDependencyClosureHash = [&](const FBlueprintHelperTaskRuntimePreparedStep& PreparedStep) -> FString
	{
		TSet<FString> Closure;
		CollectDependencyClosure(PreparedStep.StepId, Closure);
		TArray<FString> ClosureIds = Closure.Array();
		ClosureIds.Sort();

		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& DependencyId : ClosureIds)
		{
			TSharedRef<FJsonObject> Dependency = MakeShared<FJsonObject>();
			Dependency->SetStringField(TEXT("step_id"), DependencyId);
			if (const FString* PayloadHash = StepPayloadHashes.Find(DependencyId))
			{
				Dependency->SetStringField(TEXT("payload_hash"), *PayloadHash);
			}
			Values.Add(MakeShared<FJsonValueObject>(Dependency));
		}

		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetArrayField(TEXT("dependency_closure"), Values);
		return FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(Json);
	};

	FBlueprintHelperTaskRuntimeServiceLocalUtils::FBlueprintHelperReviewBaselinePolicyEvaluation BaselinePolicy;
	FBlueprintHelperToolError BaselinePolicyError;
	const double BaselinePolicyStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
	if (!FBlueprintHelperTaskRuntimeServiceLocalUtils::EvaluateReviewBaselinePolicy(
		*TaskPlanPtr,
		bDryRun,
		AssetBrowseService,
		TaskRunJournals,
		BaselinePolicy,
		BaselinePolicyError))
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_baseline_policy"), BaselinePolicyStageStart);
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			BaselinePolicyError);
		AttachPipelineToResult(Result);
		AttachTimingToResult(Result);
		return Result;
	}
	FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_baseline_policy"), BaselinePolicyStageStart);

	if (!bDryRun)
	{
		const double BaselineCaptureStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		FBlueprintHelperReviewArchiveSession ArchiveSession;
		ArchiveSession.Schema = FBlueprintHelperReviewConfigResolver::Load().MakeArchiveSessionSchema();
		ArchiveSession.ArchiveSessionId = ArchiveSessionId;
		ArchiveSession.TaskRunId = TaskRunId;
		ArchiveSession.AllowedTargetAssets = FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadTargetAssets(*TaskPlanPtr);
		ArchiveSession.BaselineDirtyAssetPolicy = BaselinePolicy.PolicyString;
		ArchiveSession.BaselineSnapshotTrust = BaselinePolicy.SnapshotTrust;
		ArchiveSession.DirtyTargetAssets = BaselinePolicy.DirtyTargetAssets;
		ArchiveSession.BaselineWarnings = BaselinePolicy.Warnings;
		ArchiveSession.DirtyState = ToString(BaselinePolicy.DirtyDecision.State);
		ArchiveSession.SafeNextAction = BaselinePolicy.DirtyDecision.SafeNextAction;
		ArchiveSession.AllowedRecoveryActions = BaselinePolicy.DirtyDecision.AllowedRecoveryActions;
		ArchiveSession.RiskyRecoveryActions = BaselinePolicy.DirtyDecision.RiskyRecoveryActions;
		ArchiveSession.DirtyEvidenceRefs = BaselinePolicy.DirtyDecision.EvidenceRefs;
		ArchiveSession.BaselineSnapshotRefs = FBlueprintHelperTaskRuntimeServiceLocalUtils::CaptureReviewBaselineSnapshots(
			ArchiveSessionId,
			ArchiveSession.AllowedTargetAssets);
		FBlueprintHelperReviewBaselineSnapshotService BaselineSnapshotService;
		ArchiveSession.BaselineSemanticSnapshotRefs = BaselineSnapshotService.CaptureSemanticBaselineSnapshots(
			ArchiveSessionId,
			ArchiveSession.AllowedTargetAssets,
			&ArchiveSession.BaselineWarnings);
		BaselinePolicy.Warnings = ArchiveSession.BaselineWarnings;
		ArchiveSession.CreatedAt = FDateTime::UtcNow().ToIso8601();
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_baseline_capture"), BaselineCaptureStageStart);
		PostIoBatch.SetArchiveSession(ArchiveSession);
	}
	FBlueprintHelperTaskRuntimeServiceLocalUtils::FScopedBlueprintHelperReviewContext ReviewContext(!bDryRun, ArchiveSessionId, TaskRunId);
	FBlueprintHelperTaskRuntimeServiceLocalUtils::FScopedBlueprintHelperGraphLayoutTask GraphLayoutTask(!bDryRun);

	TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords = PipelineContext.StepRecords;
	TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords =
		PipelineContext.PostOperationRecords;
	TArray<FBlueprintHelperTaskRuntimeServiceLocalUtils::FResolvedCallFunctionRuntimeFact> ResolvedCallFunctionFacts;
	TArray<TSharedPtr<FJsonValue>> CachedRuntimeFactValues;
	FBlueprintHelperValidationSummary BaseValidation;
	bool bSawStepValidation = false;
	TMap<FString, FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus> StepExecutionStatuses;
	TSet<FString> DryRunPlannedComponentKeys;
	TSet<FString> DryRunPlannedWidgetKeys;
	TSet<FString> DryRunPlannedDataTableRowKeys;
	FBlueprintHelperTaskRuntimeServiceLocalUtils::FPlannedMemberVariablesByAsset DryRunPlannedMemberVariablesByAsset;
	FBlueprintHelperTaskRuntimeServiceLocalUtils::FPlannedWidgetTreesByAsset DryRunPlannedWidgetTreesByAsset;
	TMap<FString, FBlueprintHelperTaskRuntimeServiceLocalUtils::FBlueprintHelperReviewTargetSnapshotCacheValue> ReviewBeforeSnapshotCache;
	bool bSawExecutionFailure = false;
	bool bHasFirstExecutionError = false;
	FBlueprintHelperToolError FirstExecutionError;
	TSharedPtr<FJsonObject> GraphWriteCandidateArtifactJson;
	double MainThreadCommitStageStart = 0.0;
	bool bMainThreadCommitStageOpen = false;

	auto FinishMainThreadCommitStage = [&]()
	{
		if (!bMainThreadCommitStageOpen)
		{
			return;
		}

		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
			TimingTrace,
			TEXT("main_thread_commit"),
			MainThreadCommitStageStart);
		bMainThreadCommitStageOpen = false;
	};

	auto FlushPostIo = [&](FBlueprintHelperToolResultBase& RuntimeResult)
	{
		const double PostIoStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		const FBlueprintHelperTaskRuntimePostIoService PostIoService;
		FBlueprintHelperTaskRuntimePostIoFlushResult PostIoResult = PostIoService.Flush(
			PostIoBatch.GetBatch(),
			TaskRunJournals,
			DebugEntryService,
			&RuntimeResult,
			&TimingTrace);
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("post_io"), PostIoStageStart);

		if (PostIoBatch.HasWork())
		{
			if (!RuntimeResult.Data.IsValid())
			{
				RuntimeResult.Data = MakeShared<FJsonObject>();
			}
			RuntimeResult.Data->SetObjectField(TEXT("post_io"), PostIoResult.ToJson());
		}
	};

	auto BuildCacheDiagnostics = [&]() -> FBlueprintHelperTaskRuntimeCacheDiagnostics
	{
		FBlueprintHelperTaskRuntimeCacheDiagnostics Diagnostics;
		Diagnostics.PartialPreviewTtlSeconds = CacheConfig.PartialPreviewTtl.GetTotalSeconds();
		Diagnostics.CallFunctionFactTtlSeconds = CacheConfig.CallFunctionFactTtl.GetTotalSeconds();
		Diagnostics.GraphWritePlanTtlSeconds = CacheConfig.GraphWritePlanTtl.GetTotalSeconds();
		if (PartialPreviewCache.IsValid())
		{
			const FBlueprintHelperPartialPreviewCacheStats Stats = PartialPreviewCache->GetStats();
			Diagnostics.PartialPreviewHits = Stats.Hits;
			Diagnostics.PartialPreviewMisses = Stats.Misses;
			Diagnostics.PartialPreviewReusedSteps = Stats.ReusedSteps;
			Diagnostics.PrunedExpiredEntries += Stats.PrunedExpiredEntries;
			Diagnostics.PrunedCapacityEntries += Stats.PrunedCapacityEntries;
			Diagnostics.CurrentBytes += Stats.CurrentBytes;
		}
		if (CallFunctionResolutionCache.IsValid())
		{
			const FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats Stats =
				CallFunctionResolutionCache->GetStats();
			Diagnostics.CallFunctionFactHits = Stats.Hits;
			Diagnostics.CallFunctionFactMisses = Stats.Misses;
			Diagnostics.PrunedExpiredEntries += Stats.PrunedExpiredEntries;
			Diagnostics.PrunedCapacityEntries += Stats.PrunedCapacityEntries;
			Diagnostics.CurrentBytes += Stats.CurrentBytes;
		}
		if (GraphWritePlanCache.IsValid())
		{
			const FBlueprintHelperGraphWritePlanCacheStats Stats = GraphWritePlanCache->GetStats();
			Diagnostics.GraphWritePlanHits = Stats.Hits;
			Diagnostics.GraphWritePlanMisses = Stats.Misses;
			Diagnostics.PrunedExpiredEntries += Stats.PrunedExpiredEntries;
			Diagnostics.PrunedCapacityEntries += Stats.PrunedCapacityEntries;
			Diagnostics.CurrentBytes += Stats.CurrentBytes;
		}
		return Diagnostics;
	};

	auto AttachCacheDiagnostics = [&](FBlueprintHelperToolResultBase& RuntimeResult)
	{
		if (!TimingTrace.bEnabled)
		{
			return;
		}
		if (!RuntimeResult.Data.IsValid())
		{
			RuntimeResult.Data = MakeShared<FJsonObject>();
		}
		RuntimeResult.Data->SetObjectField(TEXT("cache_diagnostics"), BuildCacheDiagnostics().ToJson());
	};

	auto BuildFailureResult = [&](const FBlueprintHelperToolError& Error) -> FBlueprintHelperToolResultBase
	{
		FBlueprintHelperToolResultBase RuntimeResult = (bDryRun && StepRecords.Num() > 0)
			? FBlueprintHelperToolResultBuilder::DryRun(
				RuntimeOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId())
			: FBlueprintHelperToolResultBuilder::Failure(
				RuntimeOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId(),
				Error);

		if (bSawStepValidation || FBlueprintHelperTaskRuntimeServiceLocalUtils::HasExecutionPolicyValidationFields(*TaskPlanPtr))
		{
			RuntimeResult.Validation = BuildRuntimeValidation(*TaskPlanPtr, BaseValidation);
		}

		if (StepRecords.Num() > 0 || PostOperationRecords.Num() > 0)
		{
			RuntimeResult.Data = PipelineRunner.BuildRuntimeData(
				StepRecords,
				PostOperationRecords);
		}

		if (bDryRun && StepRecords.Num() > 0 && RuntimeResult.Data.IsValid())
		{
			const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
			const bool bHasDryRunObject =
				RuntimeResult.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) &&
				DryRunObject && DryRunObject->IsValid();
			if (!bHasDryRunObject)
			{
				TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
				DryRun->SetStringField(TEXT("result"), TEXT("blocked"));
				DryRun->SetBoolField(TEXT("can_execute"), false);
				DryRun->SetArrayField(TEXT("conflicts"), {});

				TSharedRef<FJsonObject> Issue = MakeShared<FJsonObject>();
				Issue->SetStringField(TEXT("code"), Error.Code.IsEmpty() ? TEXT("task_preview_blocked") : Error.Code);
				if (!Error.Message.IsEmpty())
				{
					Issue->SetStringField(TEXT("message"), Error.Message);
				}
				if (!Error.Field.IsEmpty())
				{
					Issue->SetStringField(TEXT("target"), Error.Field);
				}
				Issue->SetStringField(TEXT("source"), TEXT("task_runtime"));

				TArray<TSharedPtr<FJsonValue>> Errors;
				Errors.Add(MakeShared<FJsonValueObject>(Issue));
				DryRun->SetArrayField(TEXT("errors"), Errors);
				RuntimeResult.Data->SetObjectField(TEXT("dry_run"), DryRun);
			}
		}
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
			RuntimeResult.Data,
			ResolvedCallFunctionFacts);
		if (bDryRun)
		{
			FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachDryRunStrategy(RuntimeResult.Data, DryRunPolicy);
			FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachDryRunOutcomeFields(RuntimeResult.Data, false);
		}
		if (GraphWriteCandidateArtifactJson.IsValid())
		{
			if (!RuntimeResult.Data.IsValid())
			{
				RuntimeResult.Data = MakeShared<FJsonObject>();
			}
			RuntimeResult.Data->SetObjectField(
				TEXT("graph_write_candidate_artifact"),
				GraphWriteCandidateArtifactJson.ToSharedRef());
		}
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachCallFunctionResolutionCacheStats(
			RuntimeResult.Data,
			*CallFunctionResolutionCache);
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFactJsonValues(
			RuntimeResult.Data,
			CachedRuntimeFactValues);
		AttachCacheDiagnostics(RuntimeResult);
		PipelineRunner.SetStepRecords(StepRecords);
		ExecutePipelineStage(EBlueprintHelperTaskRuntimePipelineStage::ProjectMetricsAndResult);

		if (!bDryRun && !TaskRunId.IsEmpty() && (StepRecords.Num() > 0 || PostOperationRecords.Num() > 0))
		{
			ExecutePipelineStage(
				EBlueprintHelperTaskRuntimePipelineStage::BuildJournal,
				[&](FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
				{
					TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(
						TaskRunId,
						*TaskPlanPtr,
						Context.StepRecords,
						Context.PostOperationRecords,
						true,
						FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeReviewBaselinePolicyJson(BaselinePolicy));
					FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
						Journal,
						ResolvedCallFunctionFacts);
					FBlueprintHelperTaskRuntimeTimingUtils::AttachTiming(Journal, TimingTrace);
					PostIoBatch.SetTaskRunJournal(TaskRunId, Journal);
					return FBlueprintHelperToolResultBuilder::Applied(
						TEXT("task_runtime_build_journal"),
						FBlueprintHelperToolResultBuilder::GenerateTraceId());
				});
		}
		ExecutePipelineStage(
			EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse,
			[&](FBlueprintHelperTaskRuntimePipelineExecutionContext&)
			{
				AttachPipelineToResult(RuntimeResult);
				return FBlueprintHelperToolResultBuilder::Applied(
					TEXT("task_runtime_finalize_response"),
					FBlueprintHelperToolResultBuilder::GenerateTraceId());
			});

		if (DebugEntryService)
		{
			const FString ErrorCodeLower = Error.Code.ToLower();
			FBlueprintHelperDebugEntryEventInput DebugInput;
			DebugInput.SourceLayer = TEXT("task_runtime");
			DebugInput.Source = bDryRun
				? TEXT("task_preview_blocker")
				: ((StepRecords.Num() > 0 || PostOperationRecords.Num() > 0)
					? TEXT("task_partial_failure")
					: TEXT("task_execute_failure"));
			if (ErrorCodeLower.Contains(TEXT("compile")))
			{
				DebugInput.Source = TEXT("compile_failure");
			}
			else if (ErrorCodeLower.Contains(TEXT("save")))
			{
				DebugInput.Source = TEXT("save_failure");
			}
			else if (ErrorCodeLower.Contains(TEXT("rollback")) || Error.RollbackResult == EBlueprintHelperRollbackResult::Failed)
			{
				DebugInput.Source = TEXT("review_snapshot_restore_failure");
			}
			DebugInput.Operation = RuntimeOperation;
			DebugInput.Stage = ToolStageToString(Error.Stage);
			DebugInput.TraceId = RuntimeResult.TraceId;
			DebugInput.TaskRunId = TaskRunId;
			DebugInput.AssetPaths = FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadTargetAssets(*TaskPlanPtr);
			DebugInput.Error.Code = Error.Code;
			DebugInput.Error.Message = Error.Message;
			DebugInput.EvidenceLinks = FBlueprintHelperReviewBaselineDirtyDebugEvidenceProjection::MakeEvidenceLinksFromRefs(
				Error.EvidenceRefs);
			DebugInput.RecommendedNext = TEXT("get_debug_case");
			PostIoBatch.SetDebugEvent(DebugInput, !RuntimeResult.bOk);
		}

		FinishMainThreadCommitStage();
		FlushPostIo(RuntimeResult);
		AttachTimingToResult(RuntimeResult);
		return RuntimeResult;
	};

	const FBlueprintHelperTaskRuntimeCommitService CommitService(
		*ClusterHub,
		CompileAssetService,
		AssetBrowseService);

	auto ExecuteLoweredStep = [&](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) -> FBlueprintHelperToolResultBase
	{
		TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> ClusterStages;
		ClusterStages.Add(MakePipelineStageExecutor(
			EBlueprintHelperTaskRuntimePipelineStage::ResolveClusterFamilyAdapter));
		ClusterStages.Add(MakeUnique<FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor>(
			CommitService,
			LoweredStep,
			bDryRun));
		return ExecutePipelineBatch(MoveTemp(ClusterStages));
	};

	auto TrackDryRunPlannedState = [&](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep, const FBlueprintHelperToolResultBase& StepResult)
	{
		if (!bDryRun || !StepResult.bOk)
		{
			return;
		}

		if (LoweredStep.AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent)
		{
			FString PlannedAssetPath;
			FString PlannedComponentName;
			if (FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadComponentPayloadIdentity(
				LoweredStep,
				PlannedAssetPath,
				PlannedComponentName))
			{
				DryRunPlannedComponentKeys.Add(
					FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedComponentKey(PlannedAssetPath, PlannedComponentName));
			}
			return;
		}

		if (LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget ||
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent)
		{
			FString PlannedAssetPath;
			FString PlannedWidgetName;
			if (FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadWidgetPayloadIdentity(
				LoweredStep,
				PlannedAssetPath,
				PlannedWidgetName))
			{
				DryRunPlannedWidgetKeys.Add(
					FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedWidgetKey(PlannedAssetPath, PlannedWidgetName));
			}
			return;
		}

		if (LoweredStep.AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationAddRow)
		{
			FString PlannedAssetPath;
			FString PlannedRowName;
			if (FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadDataTablePayloadIdentity(
				LoweredStep,
				PlannedAssetPath,
				PlannedRowName))
			{
				DryRunPlannedDataTableRowKeys.Add(
					FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedDataTableRowKey(PlannedAssetPath, PlannedRowName));
			}
		}

		if (LoweredStep.Capability == FBlueprintHelperBlueprintVariableTaskPlanAdapter::CapabilityBlueprintVariable)
		{
			FBlueprintHelperTaskRuntimeServiceLocalUtils::TrackDryRunPlannedMemberVariables(
				LoweredStep,
				DryRunPlannedMemberVariablesByAsset);
		}
	};

	auto BuildPartialPreviewCacheKey = [&](const FBlueprintHelperTaskRuntimePreparedStep& PreparedStep)
	{
		FBlueprintHelperPartialPreviewCacheKey Key;
		Key.TaskSpecGroupHash = TaskSpecGroupHash;
		Key.StepId = PreparedStep.StepId;
		Key.StepPayloadHash = StepPayloadHashes.FindRef(PreparedStep.StepId);
		Key.DependencyClosureHash = BuildDependencyClosureHash(PreparedStep);
		Key.ExecutionPolicyHash = ExecutionPolicyHash;
		Key.AssetStateHash = TargetAssetStateHash;
		Key.ContextRevisionManifestHash = TargetContextRevisionManifestHash;
		Key.DryRunPlannedStateHash =
			FBlueprintHelperTaskRuntimeServiceLocalUtils::BuildDryRunPlannedMemberVariablesStateHash(
				PreparedStep.LoweredStep,
				DryRunPlannedMemberVariablesByAsset);
		return Key;
	};

	MainThreadCommitStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
	bMainThreadCommitStageOpen = true;

	for (const FBlueprintHelperTaskRuntimePreparedStep& PreparedStep : PreparedRun.Steps)
	{
		const int32 StepIndex = PreparedStep.StepIndex;
		const TSharedPtr<FJsonObject> StepObject = PreparedStep.StepObject;
		const FString& PlannedStepId = PreparedStep.StepId;
		if (!bDryRun)
		{
			const TArray<FString>& DependsOn = PreparedStep.DependsOn;
			bool bBlockedByDependency = false;
			for (const FString& DependsOnStepId : DependsOn)
			{
				const FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus* DependencyStatus = StepExecutionStatuses.Find(DependsOnStepId);
				if (DependencyStatus &&
					(*DependencyStatus == FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Failed ||
					 *DependencyStatus == FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Blocked))
				{
					bBlockedByDependency = true;
					break;
				}
			}

			if (bBlockedByDependency)
			{
				StepExecutionStatuses.Add(PlannedStepId, FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Blocked);
				bSawExecutionFailure = true;
				continue;
			}
		}

		FBlueprintHelperTaskRuntimeLoweredStep LoweredStep = PreparedStep.LoweredStep;
		if (!LoweredStep.Payload.IsValid())
		{
			return BuildFailureResult(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
				TEXT("invalid_taskplan_lowered_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Task Runtime lowering did not produce a payload."),
				FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex)));
		}
		if (TimingTrace.bEnabled)
		{
			LoweredStep.Payload->SetBoolField(TEXT("include_timing"), true);
		}

		const FBlueprintHelperPartialPreviewCacheKey PartialCacheKey = BuildPartialPreviewCacheKey(PreparedStep);
		const bool bUsePartialPreviewCache =
			!FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedWidgetTreeStructuralDryRunStep(LoweredStep);
		if (bDryRun && bUsePartialPreviewCache && PartialPreviewCache.IsValid())
		{
			const double PartialPreviewCacheLookupStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			FBlueprintHelperPartialPreviewCacheEntry CachedPartialPreview;
			const bool bPartialPreviewCacheHit = PartialPreviewCache->TryGet(
				PartialCacheKey,
				FDateTime::UtcNow(),
				CachedPartialPreview);
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.partial_preview_cache_lookup"), *LoweredStep.StepId),
				PartialPreviewCacheLookupStart);
			if (bPartialPreviewCacheHit && CachedPartialPreview.bPassed)
			{
				StepRecords.Add({LoweredStep, CachedPartialPreview.Result});
				CachedRuntimeFactValues.Append(CachedPartialPreview.RuntimeFactValues);
				StepExecutionStatuses.Add(
					LoweredStep.StepId,
					FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Completed);
				TrackDryRunPlannedState(LoweredStep, CachedPartialPreview.Result);
				if (CachedPartialPreview.Result.Validation.IsSet())
				{
					BaseValidation.bShouldCompile =
						BaseValidation.bShouldCompile || CachedPartialPreview.Result.Validation->bShouldCompile;
					BaseValidation.bShouldSave =
						BaseValidation.bShouldSave || CachedPartialPreview.Result.Validation->bShouldSave;
					bSawStepValidation = true;
				}
				continue;
			}
		}

		TArray<FBlueprintHelperTaskRuntimeServiceLocalUtils::FResolvedCallFunctionRuntimeFact> StepResolvedCallFunctionFacts;
		FBlueprintHelperToolError CallFunctionResolutionError;
		TSharedPtr<FJsonObject> CallFunctionBlockedData;
		const double CallFunctionStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		if (!FBlueprintHelperTaskRuntimeServiceLocalUtils::TryResolveTaskRuntimeCallFunctions(
			StepObject,
			LoweredStep.Payload,
			StepIndex,
			LoweredStep.StepId,
			bDryRun,
			TargetAssetStateHash,
			TargetContextRevisionManifestHash,
			PreviewCandidateArtifactJson,
			GraphWriteCandidateArtifactStore.Get(),
			*CallFunctionResolutionCache,
			StepResolvedCallFunctionFacts,
			CallFunctionResolutionError,
			CallFunctionBlockedData,
			GraphWriteCandidateArtifactJson))
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.call_function_resolution"), *LoweredStep.StepId),
				CallFunctionStageStart);
			const FString StepOperation = LoweredStep.AdapterOperation.IsEmpty()
				? LoweredStep.RuntimeOperation
				: LoweredStep.AdapterOperation;
			FBlueprintHelperToolResultBase StepResult = FBlueprintHelperToolResultBuilder::Failure(
				StepOperation.IsEmpty() ? TEXT("graph_write") : StepOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId(),
				CallFunctionResolutionError);
			StepResult.Data = CallFunctionBlockedData;
			StepRecords.Add({LoweredStep, StepResult});
			StepExecutionStatuses.Add(
				LoweredStep.StepId,
				FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Failed);
			return BuildFailureResult(CallFunctionResolutionError);
		}
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
			TimingTrace,
			FString::Printf(TEXT("step.%s.call_function_resolution"), *LoweredStep.StepId),
			CallFunctionStageStart);
		ResolvedCallFunctionFacts.Append(StepResolvedCallFunctionFacts);

		if (GraphWritePlanCache.IsValid() && LoweredStep.Capability == TEXT("graph_write"))
		{
			FString GraphWriteAssetPath;
			FString GraphWriteGraphName;
			FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadCallFunctionResolutionTarget(
				StepObject,
				LoweredStep.Payload,
				GraphWriteAssetPath,
				GraphWriteGraphName);
			FBlueprintHelperGraphWritePlanCacheKey GraphWriteCacheKey;
			GraphWriteCacheKey.PayloadHash =
				FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(LoweredStep.Payload);
			GraphWriteCacheKey.GraphSchemaHash =
				FBlueprintHelperTaskRuntimeCacheKeyUtils::HashString(GraphWriteAssetPath + TEXT("|") + GraphWriteGraphName);
			GraphWriteCacheKey.AssetStateHash = TargetAssetStateHash;
			GraphWriteCacheKey.ContextRevisionManifestHash = TargetContextRevisionManifestHash;
			GraphWriteCacheKey.DryRunPlannedStateHash =
				FBlueprintHelperTaskRuntimeServiceLocalUtils::BuildDryRunPlannedMemberVariablesStateHash(
					LoweredStep,
					DryRunPlannedMemberVariablesByAsset);

			const double GraphWriteCacheLookupStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			FBlueprintHelperGraphWritePlanCacheEntry GraphWriteCachedPlan;
			const bool bGraphWritePlanCacheHit = GraphWritePlanCache->TryGet(
				GraphWriteCacheKey,
				FDateTime::UtcNow(),
				GraphWriteCachedPlan);
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.graph_write_plan_cache_lookup"), *LoweredStep.StepId),
				GraphWriteCacheLookupStart);

			if (!bGraphWritePlanCacheHit)
			{
				TArray<FString> StableIds;
				for (const FBlueprintHelperTaskRuntimeServiceLocalUtils::FResolvedCallFunctionRuntimeFact& Fact : StepResolvedCallFunctionFacts)
				{
					if (!Fact.StableId.IsEmpty())
					{
						StableIds.Add(Fact.StableId);
					}
				}

				const double GraphWritePlanPrepareStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
				const FBlueprintHelperGraphWritePlanCacheEntry GraphWritePlan =
					FBlueprintHelperGraphWritePlanCache::MakeEntryFromPayload(
						GraphWriteGraphName,
						LoweredStep.Payload,
						StableIds);
				FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
					TimingTrace,
					FString::Printf(TEXT("step.%s.graph_write_plan_prepare"), *LoweredStep.StepId),
					GraphWritePlanPrepareStart);

				const double GraphWriteCacheStoreStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
				GraphWritePlanCache->Store(GraphWriteCacheKey, GraphWritePlan, FDateTime::UtcNow());
				FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
					TimingTrace,
					FString::Printf(TEXT("step.%s.graph_write_plan_cache_store"), *LoweredStep.StepId),
					GraphWriteCacheStoreStart);
			}
		}

		FBlueprintHelperWriteReviewEvidence PreStepReviewEvidence;
		bool bHasPreStepReviewEvidence = false;
		if (!bDryRun)
		{
			const double ReviewBeforeStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			if (FBlueprintHelperTaskRuntimeServiceLocalUtils::IsUmgWidgetReviewPreStepCandidate(LoweredStep))
			{
				const FBlueprintHelperToolResultBase PreStepResult =
					FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimePreStepReviewResult(LoweredStep);
				bHasPreStepReviewEvidence = PipelineRunner.BuildReviewEvidence(
					*ClusterHub,
					LoweredStep,
					PreStepResult,
					ArchiveSessionId,
					TaskRunId,
					StepIndex,
					PreStepReviewEvidence);
			}
			if (!bHasPreStepReviewEvidence)
			{
				bHasPreStepReviewEvidence = FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
					LoweredStep,
					ArchiveSessionId,
					TaskRunId,
					StepIndex,
					PreStepReviewEvidence);
			}
			if (bHasPreStepReviewEvidence)
			{
				FBlueprintHelperTaskRuntimeServiceLocalUtils::PopulateTaskRuntimeReviewTargetSnapshots(
					PreStepReviewEvidence,
					true,
					&ReviewBeforeSnapshotCache);
			}
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.review_before_snapshot"), *LoweredStep.StepId),
				ReviewBeforeStageStart);
		}

		FBlueprintHelperToolResultBase StepResult;
		if (bQuickDryRun)
		{
			const double QuickPreviewStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			const FString StepOperation = LoweredStep.AdapterOperation.IsEmpty()
				? LoweredStep.RuntimeOperation
				: LoweredStep.AdapterOperation;
			StepResult = FBlueprintHelperToolResultBuilder::DryRun(
				StepOperation.IsEmpty() ? TEXT("task_runtime_quick_preview") : StepOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
			StepResult.Data = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeSyntheticDryRunData(
				TEXT("quick"),
				TEXT("lowering_and_call_function_resolution"));
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.quick_preview_validate"), *LoweredStep.StepId),
				QuickPreviewStageStart);
		}
		else
		{
			const double ExecuteStepStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			if (bDryRun && FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedWidgetTreeStructuralDryRunStep(LoweredStep))
			{
				StepResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::ExecutePlannedWidgetTreeDryRunStep(
					LoweredStep,
					DryRunPlannedWidgetTreesByAsset);
			}
			else
			{
				TUniquePtr<FBlueprintHelperTaskRuntimeServiceLocalUtils::FScopedDryRunPlannedMemberVariableOverlay> PlannedMemberVariableOverlay;
				if (bDryRun && LoweredStep.Capability == TEXT("graph_write"))
				{
					PlannedMemberVariableOverlay =
						MakeUnique<FBlueprintHelperTaskRuntimeServiceLocalUtils::FScopedDryRunPlannedMemberVariableOverlay>(
							LoweredStep,
							DryRunPlannedMemberVariablesByAsset);
				}
				StepResult = ExecuteLoweredStep(LoweredStep);
				PlannedMemberVariableOverlay.Reset();
			}
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.cluster_execute"), *LoweredStep.StepId),
				ExecuteStepStageStart);
		}
		if (bDryRun && FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedComponentPropertyDryRun(LoweredStep, StepResult, DryRunPlannedComponentKeys))
		{
			StepResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedComponentPropertyDryRunResult(LoweredStep);
		}
		else if (bDryRun && FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedWidgetPropertyDryRun(LoweredStep, StepResult, DryRunPlannedWidgetKeys))
		{
			StepResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedWidgetPropertyDryRunResult(LoweredStep);
		}
		else if (bDryRun && FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedDataTableRowUpdateDryRun(LoweredStep, StepResult, DryRunPlannedDataTableRowKeys))
		{
			StepResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedDataTableRowUpdateDryRunResult(LoweredStep);
		}
		StepRecords.Add({LoweredStep, StepResult});
		if (bDryRun && bUsePartialPreviewCache && StepResult.bOk && PartialPreviewCache.IsValid())
		{
			FBlueprintHelperPartialPreviewCacheEntry PartialEntry;
			PartialEntry.StepId = LoweredStep.StepId;
			PartialEntry.bPassed = true;
			PartialEntry.Result = StepResult;
			PartialEntry.RuntimeFactValues =
				FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeResolvedCallFunctionFactArray(
					StepResolvedCallFunctionFacts);
			const double PartialPreviewCacheStoreStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			PartialPreviewCache->Store(PartialCacheKey, PartialEntry, FDateTime::UtcNow());
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.partial_preview_cache_store"), *LoweredStep.StepId),
				PartialPreviewCacheStoreStart);
		}
		if (!bDryRun && StepResult.bOk)
		{
			const double ReviewAfterStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			FBlueprintHelperWriteReviewEvidence RuntimeEvidence;
			const bool bHasRuntimeReviewEvidence = PipelineRunner.BuildReviewEvidence(
				*ClusterHub,
				LoweredStep,
				StepResult,
				ArchiveSessionId,
				TaskRunId,
				StepIndex,
				RuntimeEvidence);
			if (bHasRuntimeReviewEvidence)
			{
				if (bHasPreStepReviewEvidence)
				{
					FBlueprintHelperTaskRuntimeServiceLocalUtils::ApplyCachedTaskRuntimeReviewTargetSnapshots(
						RuntimeEvidence,
						ReviewBeforeSnapshotCache);
				}
				FBlueprintHelperTaskRuntimeServiceLocalUtils::PopulateTaskRuntimeReviewTargetSnapshots(
					RuntimeEvidence,
					false);
				PostIoBatch.AddReviewEvidence(RuntimeEvidence);
			}
			else if (bHasPreStepReviewEvidence)
			{
				FBlueprintHelperWriteReviewEvidence FallbackEvidence = PreStepReviewEvidence;
				FBlueprintHelperTaskRuntimeServiceLocalUtils::PopulateTaskRuntimeReviewTargetSnapshots(
					FallbackEvidence,
					false);
				PostIoBatch.AddReviewEvidence(FallbackEvidence);
			}
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.review_after_snapshot"), *LoweredStep.StepId),
				ReviewAfterStageStart);
		}
		StepExecutionStatuses.Add(
			LoweredStep.StepId,
			StepResult.bOk
				? FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Completed
				: FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Failed);
		TrackDryRunPlannedState(LoweredStep, StepResult);

		if (StepResult.Validation.IsSet())
		{
			BaseValidation.bShouldCompile = BaseValidation.bShouldCompile || StepResult.Validation->bShouldCompile;
			BaseValidation.bShouldSave = BaseValidation.bShouldSave || StepResult.Validation->bShouldSave;
			bSawStepValidation = true;
		}

		if (!StepResult.bOk)
		{
			if (!bHasFirstExecutionError)
			{
				FirstExecutionError = StepResult.Error.IsSet()
					? *StepResult.Error
					: FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(TEXT("task_step_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan step failed."));
				bHasFirstExecutionError = true;
			}
			bSawExecutionFailure = true;
		}
	}

	if (bSawExecutionFailure)
	{
		return BuildFailureResult(
			bHasFirstExecutionError
				? FirstExecutionError
				: FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(TEXT("task_step_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan step failed.")));
	}

	if (!bDryRun)
	{
		const double GraphLayoutStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		const bool bGraphLayoutFlushed = CommitService.FlushGraphLayout();
		GraphLayoutTask.bCompleted = bGraphLayoutFlushed;
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("graph_layout_flush"), GraphLayoutStageStart);
		if (!bGraphLayoutFlushed)
		{
			return BuildFailureResult(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
				TEXT("graph_layout_flush_failed"),
				EBlueprintHelperToolStage::Execute,
				TEXT("GraphLayout flush did not complete before compile/save post-operations.")));
		}
	}

	{
		TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> PostOperationStages;
		TUniquePtr<FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor> PostOperationExecutor =
			MakeUnique<FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor>(
				&CommitService,
				&TimingTrace);
		FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor* PostOperationExecutorPtr =
			PostOperationExecutor.Get();
		PostOperationStages.Add(MoveTemp(PostOperationExecutor));
		FBlueprintHelperTaskRuntimePipeline PostOperationPipeline(MoveTemp(PostOperationStages));
		const FBlueprintHelperToolResultBase PostOperationStageResult =
			PostOperationPipeline.Execute(PipelineRunner, PipelineContext);
		if (!PostOperationStageResult.bOk)
		{
			const FBlueprintHelperTaskRuntimePostOperationExecutionResult& ExecutionResult =
				PostOperationExecutorPtr->GetLastExecutionResult();
			FBlueprintHelperToolError Error = PostOperationStageResult.Error.IsSet()
				? *PostOperationStageResult.Error
				: FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
					TEXT("task_post_operation_failed"),
					EBlueprintHelperToolStage::Execute,
					TEXT("TaskPlan post operation failed."));
			if (ExecutionResult.FirstError.IsSet())
			{
				Error = *ExecutionResult.FirstError;
			}
			FBlueprintHelperToolResultBase FailureResult = BuildFailureResult(Error);
			if (!FailureResult.Data.IsValid())
			{
				FailureResult.Data = MakeShared<FJsonObject>();
			}
			if (ExecutionResult.Records.Num() > 0)
			{
				FailureResult.Data->SetObjectField(
					TEXT("post_operation_failure"),
					FBlueprintHelperTaskRuntimePostOperationJson::RecordToJson(ExecutionResult.Records.Last()));
			}
			return FailureResult;
		}
	}

	FinishMainThreadCommitStage();
	const double ResultWrapStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
	FBlueprintHelperToolResultBase RuntimeResult = bDryRun
		? FBlueprintHelperToolResultBuilder::DryRun(RuntimeOperation, FBlueprintHelperToolResultBuilder::GenerateTraceId())
		: FBlueprintHelperToolResultBuilder::Applied(RuntimeOperation, FBlueprintHelperToolResultBuilder::GenerateTraceId());

	for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
	{
		if (StepRecord.Result.Target.IsSet())
		{
			RuntimeResult.Target = StepRecord.Result.Target;
			break;
		}
	}

	if (bSawStepValidation || FBlueprintHelperTaskRuntimeServiceLocalUtils::HasExecutionPolicyValidationFields(*TaskPlanPtr))
	{
		RuntimeResult.Validation = BuildRuntimeValidation(*TaskPlanPtr, BaseValidation);
	}

	RuntimeResult.Data = PipelineRunner.BuildRuntimeData(
		StepRecords,
		PostOperationRecords);
	FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
		RuntimeResult.Data,
		ResolvedCallFunctionFacts);
	FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFactJsonValues(
		RuntimeResult.Data,
		CachedRuntimeFactValues);
	if (bDryRun)
	{
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachDryRunStrategy(RuntimeResult.Data, DryRunPolicy);
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachDryRunOutcomeFields(RuntimeResult.Data, true);
	}
	FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachCallFunctionResolutionCacheStats(
		RuntimeResult.Data,
		*CallFunctionResolutionCache);
	AttachCacheDiagnostics(RuntimeResult);
	if (TimingTrace.bEnabled)
	{
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachGraphWriteExecutionStatsFromSteps(
			RuntimeResult.Data,
			StepRecords);
	}

	if (!bDryRun && !TaskRunId.IsEmpty())
	{
		ExecutePipelineStage(
			EBlueprintHelperTaskRuntimePipelineStage::BuildJournal,
			[&](FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
			{
				TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(
					TaskRunId,
					*TaskPlanPtr,
					Context.StepRecords,
					Context.PostOperationRecords,
					false,
					FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeReviewBaselinePolicyJson(BaselinePolicy));
				FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
					Journal,
					ResolvedCallFunctionFacts);
				FBlueprintHelperTaskRuntimeTimingUtils::AttachTiming(Journal, TimingTrace);
				PostIoBatch.SetTaskRunJournal(TaskRunId, Journal);
				return FBlueprintHelperToolResultBuilder::Applied(
					TEXT("task_runtime_build_journal"),
					FBlueprintHelperToolResultBuilder::GenerateTraceId());
			});
	}
	ExecutePipelineStage(
		EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse,
		[&](FBlueprintHelperTaskRuntimePipelineExecutionContext&)
		{
			AttachPipelineToResult(RuntimeResult);
			return FBlueprintHelperToolResultBuilder::Applied(
				TEXT("task_runtime_finalize_response"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
		});
	FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("result_wrap"), ResultWrapStageStart);

	FlushPostIo(RuntimeResult);
	AttachTimingToResult(RuntimeResult);

	return RuntimeResult;
}
