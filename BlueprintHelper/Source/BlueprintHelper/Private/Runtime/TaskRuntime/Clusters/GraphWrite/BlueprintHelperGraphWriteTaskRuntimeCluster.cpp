// BlueprintHelper TaskRuntime - GraphWrite static cluster

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"

#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"

class FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils
{
public:
	static FBlueprintHelperToolError MakeUnsupportedGraphWriteOperationError()
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("unsupported_taskplan_adapter_operation");
		Error.Stage = EBlueprintHelperToolStage::ParseInput;
		Error.Message = TEXT("Task Runtime GraphWrite cluster received an unsupported adapter operation.");
		Error.Field = TEXT("task_plan.steps[0]");
		return Error;
	}

	static FString TrimmedPayloadString(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
	{
		FString Value;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(FieldName, Value);
			Value.TrimStartAndEndInline();
		}
		return Value;
	}

	static FString ReadTargetStringField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* PrimaryFieldName,
		const TCHAR* AlternateFieldName)
	{
		FString Value = TrimmedPayloadString(Payload, PrimaryFieldName);
		if (Value.IsEmpty() && AlternateFieldName)
		{
			Value = TrimmedPayloadString(Payload, AlternateFieldName);
		}
		if (!Value.IsEmpty() || !Payload.IsValid())
		{
			return Value;
		}

		const TSharedPtr<FJsonObject>* TargetObject = nullptr;
		if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
		{
			Value = TrimmedPayloadString(*TargetObject, PrimaryFieldName);
			if (Value.IsEmpty() && AlternateFieldName)
			{
				Value = TrimmedPayloadString(*TargetObject, AlternateFieldName);
			}
		}
		return Value;
	}

	static FString ReadAssetPath(const TSharedPtr<FJsonObject>& Payload)
	{
		return ReadTargetStringField(Payload, TEXT("asset_path"), TEXT("blueprint_path"));
	}

	static FString ReadGraphName(const TSharedPtr<FJsonObject>& Payload)
	{
		return ReadTargetStringField(Payload, TEXT("graph_name"), TEXT("graph"));
	}

	static FString SerializePayloadForAnchor(const TSharedPtr<FJsonObject>& Payload)
	{
		if (!Payload.IsValid())
		{
			return FString();
		}

		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
		return Output;
	}

};

FBlueprintHelperGraphWriteTaskRuntimeCluster::FBlueprintHelperGraphWriteTaskRuntimeCluster(
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
	const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
	const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
	const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService)
	: AppendGraphService(InAppendGraphService)
	, ReplaceGraphService(InReplaceGraphService)
	, PatchGraphService(InPatchGraphService)
	, MergeGraphService(InMergeGraphService)
{
}

bool FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.AdapterOperation == TEXT("append_blueprint_graph") ||
		LoweredStep.AdapterOperation == TEXT("replace_blueprint_graph") ||
		LoweredStep.AdapterOperation == TEXT("patch_blueprint_graph") ||
		LoweredStep.AdapterOperation == TEXT("merge_blueprint_graph") ||
		LoweredStep.Capability == TEXT("graph_write");
}

bool FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	if (!StepResult.bOk || !LoweredStep.Payload.IsValid())
	{
		return false;
	}

	const FString AssetPath = FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::ReadAssetPath(LoweredStep.Payload);
	const FString GraphName = FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::ReadGraphName(LoweredStep.Payload);
	if (AssetPath.IsEmpty() || GraphName.IsEmpty())
	{
		return false;
	}

	const FString OperationKind = LoweredStep.AdapterOperation.IsEmpty()
		? LoweredStep.RuntimeOperation
		: LoweredStep.AdapterOperation;
	const FString TargetKey = FString::Printf(TEXT("graph_block:%s"), *GraphName);

	OutEvidence = FBlueprintHelperWriteReviewEvidence();
	OutEvidence.ArchiveSessionId = ArchiveSessionId;
	OutEvidence.TaskRunId = TaskRunId;
	OutEvidence.EvidenceId = FString::Printf(TEXT("task_step_%s_%d"), *TaskRunId, StepIndex);
	OutEvidence.CreatedAt = FDateTime::UtcNow().ToIso8601();
	OutEvidence.AssetPath = AssetPath;
	OutEvidence.OperationKind = OperationKind;
	OutEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	OutEvidence.DisplayLabel = OperationKind;
	OutEvidence.TaskStepIndex = StepIndex;

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::Graph;
	Target.GraphName = GraphName;
	Target.TargetKind = TEXT("graph_block");
	Target.TargetKey = TargetKey;
	Target.VisualGroupKey = FString::Printf(TEXT("graph_body|%s"), *GraphName);
	Target.DisplayLabel = FString::Printf(TEXT("%s %s"), *OperationKind, *GraphName);
	Target.LatestEvidenceId = OutEvidence.EvidenceId;
	Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
	Target.Ownership = TEXT("graph_write");
	Target.AnchorJson = FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::SerializePayloadForAnchor(LoweredStep.Payload);
	Target.ExecutionOrder = StepIndex;
	Target.TaskStepIndex = StepIndex;
	Target.AtomicIndex = 0;
	OutEvidence.AtomicTargets.Add(Target);
	return true;
}

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	if (LoweredStep.AdapterOperation == TEXT("append_blueprint_graph"))
	{
		return AppendGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}
	if (LoweredStep.AdapterOperation == TEXT("replace_blueprint_graph"))
	{
		return ReplaceGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}
	if (LoweredStep.AdapterOperation == TEXT("patch_blueprint_graph"))
	{
		return PatchGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}
	if (LoweredStep.AdapterOperation == TEXT("merge_blueprint_graph"))
	{
		return MergeGraphService.Execute(LoweredStep.Payload.ToSharedRef());
	}

	return FBlueprintHelperToolResultBuilder::Failure(
		LoweredStep.RuntimeOperation.IsEmpty() ? TEXT("execute_task_plan") : LoweredStep.RuntimeOperation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::MakeUnsupportedGraphWriteOperationError());
}
