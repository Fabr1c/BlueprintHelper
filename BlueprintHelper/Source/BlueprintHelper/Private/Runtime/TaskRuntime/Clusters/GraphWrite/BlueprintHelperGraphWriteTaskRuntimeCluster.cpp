// BlueprintHelper TaskRuntime - GraphWrite static cluster

#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteOwnershipValidator.h"

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

	static FString TrimmedObjectStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(FieldName, Value);
			Value.TrimStartAndEndInline();
		}
		return Value;
	}

	static FString ReadSignatureEvidenceId(const TSharedPtr<FJsonObject>& Payload)
	{
		FString SignatureEvidenceId = TrimmedObjectStringField(Payload, TEXT("signature_evidence_id"));
		if (!SignatureEvidenceId.IsEmpty() || !Payload.IsValid())
		{
			return SignatureEvidenceId;
		}

		const TSharedPtr<FJsonObject>* LogicSpec = nullptr;
		if (Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpec) && LogicSpec && LogicSpec->IsValid())
		{
			SignatureEvidenceId = TrimmedObjectStringField(*LogicSpec, TEXT("signature_evidence_id"));
			if (!SignatureEvidenceId.IsEmpty())
			{
				return SignatureEvidenceId;
			}

			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if ((*LogicSpec)->TryGetObjectField(TEXT("entry"), Entry) && Entry && Entry->IsValid())
			{
				SignatureEvidenceId = TrimmedObjectStringField(*Entry, TEXT("signature_evidence_id"));
				if (!SignatureEvidenceId.IsEmpty())
				{
					return SignatureEvidenceId;
				}
			}
		}

		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (Payload->TryGetObjectField(TEXT("entry"), Entry) && Entry && Entry->IsValid())
		{
			SignatureEvidenceId = TrimmedObjectStringField(*Entry, TEXT("signature_evidence_id"));
		}
		return SignatureEvidenceId;
	}

	static void ApplySignatureDependencyMetadata(
		FBlueprintHelperReviewAtomicTarget& Target,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FString& SignatureEvidenceId)
	{
		if (SignatureEvidenceId.IsEmpty() || LoweredStep.DependsOn.Num() == 0)
		{
			return;
		}

		Target.SignatureRole = TEXT("dependency");
		Target.SignatureEvidenceId = SignatureEvidenceId;
		Target.DependencyOwnerStepId = LoweredStep.StepId;
		Target.DependentStepId = LoweredStep.DependsOn[0];
	}

	static void AppendStringArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		TArray<FString>& OutValues)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (!Value.IsValid())
			{
				continue;
			}

			FString Text = Value->AsString();
			Text.TrimStartAndEndInline();
			if (!Text.IsEmpty())
			{
				OutValues.AddUnique(Text);
			}
		}
	}

	static TArray<FString> ReadGraphBlockRefs(const FBlueprintHelperToolResultBase& StepResult)
	{
		TArray<FString> BlockRefs;
		if (!StepResult.Data.IsValid())
		{
			return BlockRefs;
		}

		const TSharedPtr<FJsonObject>* AppendResult = nullptr;
		if (StepResult.Data->TryGetObjectField(TEXT("append_result"), AppendResult) && AppendResult && AppendResult->IsValid())
		{
			AppendStringArrayField(*AppendResult, TEXT("block_refs"), BlockRefs);
		}
		AppendStringArrayField(StepResult.Data, TEXT("block_refs"), BlockRefs);

		return BlockRefs;
	}

	static FString MakeGraphBlockTargetKey(const FString& GraphName, const FString& BlockRef)
	{
		const FString FullBlockId = BlockRef.StartsWith(GraphName + TEXT("_"))
			? BlockRef
			: FString::Printf(TEXT("%s_%s"), *GraphName, *BlockRef);
		return FString::Printf(TEXT("graph:%s:block:%s"), *GraphName, *FullBlockId);
	}

};

FBlueprintHelperGraphWriteTaskRuntimeCluster::FBlueprintHelperGraphWriteTaskRuntimeCluster(
	const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry)
	: GraphWriteRegistry(InGraphWriteRegistry)
{
}

bool FBlueprintHelperGraphWriteTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(LoweredStep.AdapterOperation) ||
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

	const TArray<FString> BlockRefs = FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::ReadGraphBlockRefs(StepResult);
	TArray<FString> TargetKeys;
	for (const FString& BlockRef : BlockRefs)
	{
		TargetKeys.AddUnique(FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::MakeGraphBlockTargetKey(GraphName, BlockRef));
	}
	if (TargetKeys.Num() == 0)
	{
		TargetKeys.Add(FString::Printf(TEXT("graph_block:%s"), *GraphName));
	}

	const FString AnchorJson = FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::SerializePayloadForAnchor(LoweredStep.Payload);
	const FString SignatureEvidenceId =
		FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::ReadSignatureEvidenceId(LoweredStep.Payload);
	for (int32 TargetIndex = 0; TargetIndex < TargetKeys.Num(); ++TargetIndex)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = GraphName;
		Target.TargetKind = TEXT("graph_block");
		Target.TargetKey = TargetKeys[TargetIndex];
		Target.VisualGroupKey = FString::Printf(TEXT("graph_body|%s"), *GraphName);
		Target.DisplayLabel = FString::Printf(TEXT("%s %s"), *OperationKind, *GraphName);
		Target.LatestEvidenceId = OutEvidence.EvidenceId;
		Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
		Target.Ownership = TEXT("graph_write");
		Target.AnchorJson = AnchorJson;
		Target.ExecutionOrder = StepIndex;
		Target.TaskStepIndex = StepIndex;
		Target.AtomicIndex = TargetIndex;
		FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::ApplySignatureDependencyMetadata(
			Target,
			LoweredStep,
			SignatureEvidenceId);
		OutEvidence.AtomicTargets.Add(Target);
	}

	FBlueprintHelperGraphWriteOwnershipValidationInput OwnershipValidationInput;
	OwnershipValidationInput.GeneratedBlockRefs = BlockRefs;
	OwnershipValidationInput.AtomicTargets = OutEvidence.AtomicTargets;
	const FBlueprintHelperGraphWriteOwnershipValidationResult OwnershipValidation =
		FBlueprintHelperGraphWriteOwnershipValidator::Validate(OwnershipValidationInput);
	return OutEvidence.AtomicTargets.Num() > 0 && OwnershipValidation.bPassed;
}

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	if (FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(LoweredStep.AdapterOperation))
	{
		return GraphWriteRegistry.Execute(LoweredStep.AdapterOperation, LoweredStep.Payload.ToSharedRef());
	}

	return FBlueprintHelperToolResultBuilder::Failure(
		LoweredStep.RuntimeOperation.IsEmpty() ? TEXT("execute_task_plan") : LoweredStep.RuntimeOperation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		FBlueprintHelperGraphWriteTaskRuntimeClusterLocalUtils::MakeUnsupportedGraphWriteOperationError());
}
