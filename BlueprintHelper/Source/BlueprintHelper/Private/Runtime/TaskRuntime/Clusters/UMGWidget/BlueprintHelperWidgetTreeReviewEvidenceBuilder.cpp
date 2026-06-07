#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperWidgetTreeReviewEvidenceBuilder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/DateTime.h"
#include "Runtime/TaskRuntime/Review/BlueprintHelperWriteReviewEvidenceProjection.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

FString FBlueprintHelperWidgetTreeReviewEvidenceBuilder::ReadStringField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
		Value.TrimStartAndEndInline();
	}
	return Value;
}

TOptional<int32> FBlueprintHelperWidgetTreeReviewEvidenceBuilder::ReadOptionalIntField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return TOptional<int32>();
	}

	double Number = 0.0;
	if (!Object->TryGetNumberField(FieldName, Number))
	{
		return TOptional<int32>();
	}
	return FMath::RoundToInt(Number);
}

FString FBlueprintHelperWidgetTreeReviewEvidenceBuilder::ReadOperationKind(
	const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input)
{
	return Input.LoweredStep.AdapterOperation.IsEmpty()
		? Input.LoweredStep.RuntimeOperation
		: Input.LoweredStep.AdapterOperation;
}

FString FBlueprintHelperWidgetTreeReviewEvidenceBuilder::ReadTargetWidgetName(
	const FString& OperationKind,
	const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input)
{
	const FString ResultWidgetName = ReadStringField(Input.StepResult.Data, TEXT("widget_name"));
	if (!ResultWidgetName.IsEmpty())
	{
		return ResultWidgetName;
	}

	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent)
	{
		return ReadStringField(Input.LoweredStep.Payload, TEXT("widget_name"));
	}

	return ReadStringField(Input.LoweredStep.Payload, TEXT("widget_name"));
}

FString FBlueprintHelperWidgetTreeReviewEvidenceBuilder::SerializeJsonObject(
	const TSharedRef<FJsonObject>& Object)
{
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object, Writer);
	return Output;
}

TSharedRef<FJsonObject> FBlueprintHelperWidgetTreeReviewEvidenceBuilder::BuildAnchorJson(
	const FString& OperationKind,
	const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input)
{
	TSharedRef<FJsonObject> Anchor = MakeShared<FJsonObject>();
	Anchor->SetStringField(TEXT("operation"), OperationKind);
	if (!Input.LoweredStep.StepId.IsEmpty())
	{
		Anchor->SetStringField(TEXT("step_id"), Input.LoweredStep.StepId);
	}

	if (Input.LoweredStep.Payload.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Input.LoweredStep.Payload->Values)
		{
			Anchor->SetField(FBlueprintHelperVersionCompat::JsonKeyToString(Field.Key), Field.Value);
		}
	}

	if (Input.StepResult.Data.IsValid())
	{
		const TSharedPtr<FJsonObject>* ReadbackContext = nullptr;
		if (Input.StepResult.Data->TryGetObjectField(TEXT("readback_context"), ReadbackContext) &&
			ReadbackContext &&
			ReadbackContext->IsValid())
		{
			Anchor->SetObjectField(TEXT("readback_context"), (*ReadbackContext).ToSharedRef());
		}
	}
	return Anchor;
}

bool FBlueprintHelperWidgetTreeReviewEvidenceBuilder::Build(
	const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	if (!Input.StepResult.bOk || !Input.LoweredStep.Payload.IsValid())
	{
		return false;
	}

	const FString AssetPath = ReadStringField(Input.LoweredStep.Payload, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return false;
	}

	const FString OperationKind = ReadOperationKind(Input);
	const FString TargetWidgetName = ReadTargetWidgetName(OperationKind, Input);
	const FString HostWidgetName = ReadStringField(Input.LoweredStep.Payload, TEXT("host_widget_name"));
	const FString SlotName = ReadStringField(Input.LoweredStep.Payload, TEXT("slot_name"));
	const FString ParentName = ReadStringField(Input.LoweredStep.Payload, TEXT("parent_name"));
	const FString NewParentName = ReadStringField(Input.LoweredStep.Payload, TEXT("new_parent_name"));
	const FString PropertyName = ReadStringField(Input.LoweredStep.Payload, TEXT("property_name"));
	const TOptional<int32> VirtualIndex = ReadOptionalIntField(Input.LoweredStep.Payload, TEXT("virtual_index"));
	const TSharedRef<FJsonObject> Anchor = BuildAnchorJson(OperationKind, Input);
	const FString AnchorJson = SerializeJsonObject(Anchor);

	OutEvidence = FBlueprintHelperWriteReviewEvidence();
	OutEvidence.ArchiveSessionId = Input.ArchiveSessionId;
	OutEvidence.TaskRunId = Input.TaskRunId;
	OutEvidence.EvidenceId = FString::Printf(TEXT("task_step_%s_%d"), *Input.TaskRunId, Input.StepIndex);
	OutEvidence.CreatedAt = FDateTime::UtcNow().ToIso8601();
	OutEvidence.AssetPath = AssetPath;
	OutEvidence.OperationKind = OperationKind;
	OutEvidence.DisplayLabel = OperationKind;
	OutEvidence.TaskStepIndex = Input.StepIndex;

	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget)
	{
		OutEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	}
	else if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget)
	{
		OutEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Removed;
	}
	else
	{
		OutEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = AssetPath;
	Target.Surface = EBlueprintHelperReviewSurface::UMGWidgetTree;
	Target.TargetKind = TEXT("umg_widget_tree");
	Target.TargetSubKind = TEXT("widget_tree");
	Target.DisplayLabel = TargetWidgetName.IsEmpty() ? OperationKind : TargetWidgetName;
	Target.LatestEvidenceId = OutEvidence.EvidenceId;
	Target.SourceEvidenceIds.Add(OutEvidence.EvidenceId);
	Target.Ownership = TEXT("blueprinthelper_owned");
	Target.AnchorJson = AnchorJson;
	Target.ExecutionOrder = Input.StepIndex;
	Target.TaskStepIndex = Input.StepIndex;
	Target.AtomicIndex = 0;

	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent)
	{
		if (HostWidgetName.IsEmpty() || SlotName.IsEmpty())
		{
			return false;
		}
		Target.TargetSubKind = TEXT("named_slot_content");
		Target.TargetKey = FString::Printf(TEXT("umg_widget_tree:%s:slot:%s"), *HostWidgetName, *SlotName);
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = FString::Printf(TEXT("%s.%s"), *HostWidgetName, *SlotName);
		Target.LifecycleObjectKey = FString::Printf(TEXT("widget_slot:%s.%s"), *HostWidgetName.ToLower(), *SlotName.ToLower());
		Target.LifecycleParentKey = FString::Printf(TEXT("widget:%s"), *HostWidgetName.ToLower());
		Target.AfterParent = HostWidgetName;
		Target.PropertyPath = SlotName;
	}
	else
	{
		if (TargetWidgetName.IsEmpty())
		{
			return false;
		}
		Target.TargetKey = FString::Printf(TEXT("umg_widget:%s"), *TargetWidgetName);
		Target.VisualGroupKey = Target.TargetKey;
		Target.LifecycleObjectKey = FString::Printf(TEXT("widget:%s"), *TargetWidgetName.ToLower());
		const FString ParentForLifecycle = !NewParentName.IsEmpty() ? NewParentName : ParentName;
		if (!ParentForLifecycle.IsEmpty())
		{
			Target.AfterParent = ParentForLifecycle;
			Target.LifecycleParentKey = FString::Printf(TEXT("widget:%s"), *ParentForLifecycle.ToLower());
		}
		if (!PropertyName.IsEmpty())
		{
			Target.TargetKind = TEXT("umg_widget_property");
			Target.TargetSubKind = TEXT("widget_property");
			Target.PropertyPath = PropertyName;
			Target.TargetKey = FString::Printf(TEXT("umg_widget:%s.%s"), *TargetWidgetName, *PropertyName);
			Target.VisualGroupKey = Target.TargetKey;
			Target.DisplayLabel = FString::Printf(TEXT("%s.%s"), *TargetWidgetName, *PropertyName);
		}
	}
	if (VirtualIndex.IsSet())
	{
		Target.ReadbackFingerprintAfter = FString::Printf(TEXT("virtual_index:%d"), VirtualIndex.GetValue());
	}
	if (Target.ScopeIdentity.IsEmpty())
	{
		Target.ScopeIdentity = FString::Printf(TEXT("%s|%s"), *AssetPath, *Target.TargetKey);
	}
	FBlueprintHelperWriteReviewEvidenceProjection::ApplyBoundaryToAtomicTarget(
		FBlueprintHelperReviewBoundaryModelBuilder::FromAtomicTarget(Target),
		Target);

	TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
	BlueprintHelperReadDiagnosticArrayField(Input.StepResult.Data, TEXT("diagnostics"), Diagnostics);
	BlueprintHelperReadDiagnosticArrayField(Input.StepResult.Data, TEXT("compiler_results"), Diagnostics);
	const TSharedPtr<FJsonObject>* CompileResult = nullptr;
	if (Input.StepResult.Data.IsValid() &&
		Input.StepResult.Data->TryGetObjectField(TEXT("compile_result"), CompileResult) &&
		CompileResult &&
		CompileResult->IsValid())
	{
		BlueprintHelperReadDiagnosticArrayField(*CompileResult, TEXT("diagnostics"), Diagnostics);
		BlueprintHelperReadDiagnosticArrayField(*CompileResult, TEXT("compiler_results"), Diagnostics);
	}
	for (FBlueprintHelperDiagnosticItem& Diagnostic : Diagnostics)
	{
		if (Diagnostic.TargetKey.IsEmpty())
		{
			Diagnostic.TargetKey = Target.TargetKey;
		}
		if (Diagnostic.NodeName.IsEmpty())
		{
			Diagnostic.NodeName = TargetWidgetName;
		}
		if (Diagnostic.Field.IsEmpty())
		{
			Diagnostic.Field = Target.PropertyPath;
		}
	}
	OutEvidence.AtomicTargets.Add(Target);
	TArray<FBlueprintHelperDiagnosticProjection> DiagnosticProjections;
	for (const FBlueprintHelperDiagnosticItem& Diagnostic : Diagnostics)
	{
		DiagnosticProjections.Add(
			FBlueprintHelperDiagnosticProjectionUtils::FromDiagnosticItem(
				Diagnostic,
				TEXT("umg_widget.review_evidence"),
				AssetPath,
				Target.ScopeIdentity));
	}
	FBlueprintHelperWriteReviewEvidenceProjection::AttachDiagnostics(OutEvidence, DiagnosticProjections);
	return true;
}
