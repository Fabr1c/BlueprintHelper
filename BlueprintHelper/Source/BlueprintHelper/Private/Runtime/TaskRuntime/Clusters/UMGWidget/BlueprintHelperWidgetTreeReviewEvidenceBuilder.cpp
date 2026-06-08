#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperWidgetTreeReviewEvidenceBuilder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/DateTime.h"
#include "Runtime/TaskRuntime/Review/BlueprintHelperWriteReviewEvidenceProjection.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Generated/BlueprintHelperUMGWidgetOperationManifest.generated.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperWidgetTreeReviewEvidenceDescriptorUtils
{
public:
	static FString ResolveReviewTargetSubKind(const FString& OperationKind)
	{
		for (const FBlueprintHelperGeneratedCommandDescriptor& Descriptor : GBlueprintHelperUMGWidgetOperationCommands)
		{
			if (OperationKind.Equals(Descriptor.TaskPlanOp, ESearchCase::IgnoreCase) ||
				OperationKind.Equals(Descriptor.Command, ESearchCase::IgnoreCase))
			{
				return Descriptor.ReviewTargetSubkind ? FString(Descriptor.ReviewTargetSubkind) : FString();
			}
		}
		return FString();
	}

	static FString ReadFirstNameMappingValue(const TSharedPtr<FJsonObject>& Payload)
	{
		const TSharedPtr<FJsonObject>* NameMapping = nullptr;
		if (!Payload.IsValid() ||
			!Payload->TryGetObjectField(TEXT("name_mapping"), NameMapping) ||
			!NameMapping ||
			!NameMapping->IsValid())
		{
			return FString();
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*NameMapping)->Values)
		{
			if (Field.Value.IsValid() && Field.Value->Type == EJson::String)
			{
				return Field.Value->AsString();
			}
		}
		return FString();
	}
};

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

bool FBlueprintHelperWidgetTreeReviewEvidenceBuilder::ReadBoolField(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	bool bDefaultValue)
{
	bool bValue = bDefaultValue;
	if (Object.IsValid())
	{
		Object->TryGetBoolField(FieldName, bValue);
	}
	return bValue;
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

TSharedPtr<FJsonObject> FBlueprintHelperWidgetTreeReviewEvidenceBuilder::ReadMutationContext(
	const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input)
{
	const TSharedPtr<FJsonObject>* ReadbackContext = nullptr;
	const TSharedPtr<FJsonObject>* MutationContext = nullptr;
	if (Input.StepResult.Data.IsValid() &&
		Input.StepResult.Data->TryGetObjectField(TEXT("readback_context"), ReadbackContext) &&
		ReadbackContext &&
		ReadbackContext->IsValid())
	{
		if ((*ReadbackContext)->TryGetObjectField(TEXT("mutation"), MutationContext) &&
			MutationContext &&
			MutationContext->IsValid())
		{
			return *MutationContext;
		}

		return *ReadbackContext;
	}
	return nullptr;
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
	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RenameWidget)
	{
		const FString OriginalWidgetName = ReadStringField(Input.LoweredStep.Payload, TEXT("widget_name"));
		if (!OriginalWidgetName.IsEmpty())
		{
			return OriginalWidgetName;
		}
	}
	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget)
	{
		const FString RootWidgetName = ReadStringField(Input.LoweredStep.Payload, TEXT("root_widget_name"));
		if (!RootWidgetName.IsEmpty())
		{
			return RootWidgetName;
		}
	}

	const FString ResultWidgetName = ReadStringField(Input.StepResult.Data, TEXT("widget_name"));
	if (!ResultWidgetName.IsEmpty())
	{
		return ResultWidgetName;
	}

	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent)
	{
		return ReadStringField(Input.LoweredStep.Payload, TEXT("widget_name"));
	}
	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RenameWidget)
	{
		const FString NewWidgetName = ReadStringField(Input.LoweredStep.Payload, TEXT("new_widget_name"));
		if (!NewWidgetName.IsEmpty())
		{
			return NewWidgetName;
		}
	}
	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReparentWidgetBlueprint)
	{
		return TEXT("widget_blueprint");
	}
	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::DuplicateWidgetSubtree)
	{
		const FString MappedName =
			FBlueprintHelperWidgetTreeReviewEvidenceDescriptorUtils::ReadFirstNameMappingValue(Input.LoweredStep.Payload);
		if (!MappedName.IsEmpty())
		{
			return MappedName;
		}
		return ReadStringField(Input.LoweredStep.Payload, TEXT("source_widget_name"));
	}
	if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::WrapWidget)
	{
		const FString WrapperName = ReadStringField(Input.LoweredStep.Payload, TEXT("wrapper_name"));
		if (!WrapperName.IsEmpty())
		{
			return WrapperName;
		}
	}

	return ReadStringField(Input.LoweredStep.Payload, TEXT("widget_name"));
}

FString FBlueprintHelperWidgetTreeReviewEvidenceBuilder::SerializeJsonObject(
	const TSharedRef<FJsonObject>& Object)
{
	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
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
	const FString DescriptorTargetSubKind =
		FBlueprintHelperWidgetTreeReviewEvidenceDescriptorUtils::ResolveReviewTargetSubKind(OperationKind);
	const FString TargetWidgetName = ReadTargetWidgetName(OperationKind, Input);
	const FString HostWidgetName = ReadStringField(Input.LoweredStep.Payload, TEXT("host_widget_name"));
	const FString SlotName = ReadStringField(Input.LoweredStep.Payload, TEXT("slot_name"));
	const FString ParentName = ReadStringField(Input.LoweredStep.Payload, TEXT("parent_name"));
	const FString NewParentName = ReadStringField(Input.LoweredStep.Payload, TEXT("new_parent_name"));
	const FString PropertyName = ReadStringField(Input.LoweredStep.Payload, TEXT("property_name"));
	const FString PayloadPropertyPath = ReadStringField(Input.LoweredStep.Payload, TEXT("property_path"));
	const FString RenameAfterWidgetName = ReadStringField(Input.LoweredStep.Payload, TEXT("new_widget_name"));
	const FString RootReplacementPolicy = ReadStringField(Input.LoweredStep.Payload, TEXT("replacement_policy"));
	const FString RootReplacementWidgetClass = ReadStringField(Input.LoweredStep.Payload, TEXT("replacement_widget_class"));
	const FString RootReplacementWidgetName = ReadStringField(Input.LoweredStep.Payload, TEXT("replacement_widget_name"));
	const FString RootAfterWidgetName = ReadStringField(Input.StepResult.Data, TEXT("widget_name"));
	const TSharedPtr<FJsonObject> MutationContext = ReadMutationContext(Input);
	const FString MutationTargetKind = ReadStringField(MutationContext, TEXT("target_kind"));
	const FString MutationPropertyPath = ReadStringField(MutationContext, TEXT("property_path"));
	const FString SlotPropertyPath = MutationPropertyPath.IsEmpty() ? PayloadPropertyPath : MutationPropertyPath;
	const FString SlotClassPath = ReadStringField(MutationContext, TEXT("slot_class_path"));
	const FString BeforeValue = ReadStringField(MutationContext, TEXT("before_value"));
	const FString AfterValue = ReadStringField(MutationContext, TEXT("after_value"));
	const bool bBeforeIsVariable = ReadBoolField(MutationContext, TEXT("before_is_variable"), false);
	const bool bAfterIsVariable = ReadBoolField(MutationContext, TEXT("after_is_variable"), false);
	const FString VariableGuidState = ReadStringField(MutationContext, TEXT("variable_guid_state"));
	const TOptional<int32> VirtualIndex = ReadOptionalIntField(Input.LoweredStep.Payload, TEXT("virtual_index"));
	const TSharedRef<FJsonObject> Anchor = BuildAnchorJson(OperationKind, Input);
	const FString AnchorJson = SerializeJsonObject(Anchor);
	const FString MutationJson = MutationContext.IsValid()
		? SerializeJsonObject(MutationContext.ToSharedRef())
		: FString();

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
	else if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::DuplicateWidgetSubtree)
	{
		OutEvidence.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
	}
	else if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget ||
		OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget)
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
	Target.TargetSubKind = DescriptorTargetSubKind.IsEmpty() ? TEXT("widget_tree") : DescriptorTargetSubKind;
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
	else if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetSlotProperty)
	{
		if (TargetWidgetName.IsEmpty() || SlotPropertyPath.IsEmpty())
		{
			return false;
		}
		Target.TargetKind = MutationTargetKind.IsEmpty() ? TEXT("slot_property") : MutationTargetKind;
		Target.TargetSubKind = TEXT("slot_property");
		Target.TargetKey = FString::Printf(TEXT("slot_property:%s.%s"), *TargetWidgetName, *SlotPropertyPath);
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = FString::Printf(TEXT("%s.%s"), *TargetWidgetName, *SlotPropertyPath);
		Target.LifecycleObjectKey = FString::Printf(TEXT("slot_property:%s.%s"), *TargetWidgetName.ToLower(), *SlotPropertyPath.ToLower());
		Target.LifecycleParentKey = FString::Printf(TEXT("widget:%s"), *TargetWidgetName.ToLower());
		Target.PropertyPath = SlotPropertyPath;
		Target.ComponentPath = SlotClassPath;
		Target.ReadbackFingerprintBefore = BeforeValue.IsEmpty() ? FString() : FString::Printf(TEXT("slot_property:%s"), *BeforeValue);
		Target.ReadbackFingerprintAfter = AfterValue.IsEmpty() ? FString() : FString::Printf(TEXT("slot_property:%s"), *AfterValue);
		Target.ChangedPropertiesJson = MutationJson;
	}
	else if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetAsVariable)
	{
		if (TargetWidgetName.IsEmpty())
		{
			return false;
		}
		Target.TargetKind = MutationTargetKind.IsEmpty() ? TEXT("widget_variable") : MutationTargetKind;
		Target.TargetSubKind = TEXT("widget_variable");
		Target.TargetKey = FString::Printf(TEXT("widget_variable:%s"), *TargetWidgetName);
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = FString::Printf(TEXT("%s.is_variable"), *TargetWidgetName);
		Target.LifecycleObjectKey = FString::Printf(TEXT("widget:%s"), *TargetWidgetName.ToLower());
		Target.PropertyPath = TEXT("is_variable");
		Target.ReadbackFingerprintBefore = FString::Printf(TEXT("is_variable:%s"), bBeforeIsVariable ? TEXT("true") : TEXT("false"));
		Target.ReadbackFingerprintAfter = FString::Printf(TEXT("is_variable:%s"), bAfterIsVariable ? TEXT("true") : TEXT("false"));
		Target.ChangedPropertiesJson = MutationJson;
		if (!VariableGuidState.IsEmpty())
		{
			Target.ComponentOrigin = VariableGuidState;
		}
	}
	else
	{
		if (TargetWidgetName.IsEmpty())
		{
			return false;
		}
		if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReparentWidgetBlueprint)
		{
			Target.TargetKind = TEXT("umg_widget_blueprint");
			Target.TargetKey = FString::Printf(TEXT("umg_widget_blueprint:%s"), *AssetPath);
			Target.VisualGroupKey = Target.TargetKey;
			Target.DisplayLabel = TEXT("WidgetBlueprint ParentClass");
			Target.LifecycleObjectKey = FString::Printf(TEXT("umg_widget_blueprint:%s"), *AssetPath.ToLower());
			Target.PropertyPath = TEXT("ParentClass");
			Target.ReadbackFingerprintAfter = ReadStringField(Input.LoweredStep.Payload, TEXT("new_parent_class"));
		}
		else
		{
			Target.TargetKey = FString::Printf(TEXT("umg_widget:%s"), *TargetWidgetName);
			Target.VisualGroupKey = Target.TargetKey;
			Target.LifecycleObjectKey = FString::Printf(TEXT("widget:%s"), *TargetWidgetName.ToLower());
			if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RenameWidget)
			{
				Target.TargetSubKind = TEXT("widget_rename");
				if (!RenameAfterWidgetName.IsEmpty())
				{
					Target.DisplayLabel = FString::Printf(TEXT("%s -> %s"), *TargetWidgetName, *RenameAfterWidgetName);
					Target.ReadbackFingerprintBefore = FString::Printf(TEXT("widget_name:%s"), *TargetWidgetName);
					Target.ReadbackFingerprintAfter = FString::Printf(TEXT("widget_name:%s"), *RenameAfterWidgetName);
					TSharedRef<FJsonObject> RenameProperties = MakeShared<FJsonObject>();
					RenameProperties->SetStringField(TEXT("before_widget_name"), TargetWidgetName);
					RenameProperties->SetStringField(TEXT("after_widget_name"), RenameAfterWidgetName);
					Target.ChangedPropertiesJson = SerializeJsonObject(RenameProperties);
				}
			}
			else if (OperationKind == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget)
			{
				Target.TargetSubKind = TEXT("root_widget_removal");
				Target.DisplayLabel = TargetWidgetName;
				Target.ReadbackFingerprintBefore = FString::Printf(TEXT("root_widget:%s"), *TargetWidgetName);
				if (!RootAfterWidgetName.IsEmpty())
				{
					Target.ReadbackFingerprintAfter = FString::Printf(TEXT("root_widget:%s"), *RootAfterWidgetName);
				}
				TSharedRef<FJsonObject> RootRemovalProperties = MakeShared<FJsonObject>();
				RootRemovalProperties->SetStringField(TEXT("before_root_widget_name"), TargetWidgetName);
				if (!RootAfterWidgetName.IsEmpty())
				{
					RootRemovalProperties->SetStringField(TEXT("after_root_widget_name"), RootAfterWidgetName);
				}
				if (!RootReplacementPolicy.IsEmpty())
				{
					RootRemovalProperties->SetStringField(TEXT("replacement_policy"), RootReplacementPolicy);
				}
				if (!RootReplacementWidgetClass.IsEmpty())
				{
					RootRemovalProperties->SetStringField(TEXT("replacement_widget_class"), RootReplacementWidgetClass);
				}
				if (!RootReplacementWidgetName.IsEmpty())
				{
					RootRemovalProperties->SetStringField(TEXT("replacement_widget_name"), RootReplacementWidgetName);
				}
				Target.ChangedPropertiesJson = SerializeJsonObject(RootRemovalProperties);
			}
		}
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
