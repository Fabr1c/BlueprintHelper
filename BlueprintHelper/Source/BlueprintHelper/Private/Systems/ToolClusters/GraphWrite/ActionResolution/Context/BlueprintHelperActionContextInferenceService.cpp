#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionContextUtils.h"

FBlueprintHelperResolvedActionContextBundle FBlueprintHelperActionContextInferenceService::Infer(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const TArray<FBlueprintHelperActionContextDemand>& Demands)
{
	FBlueprintHelperResolvedActionContextBundle Bundle;
	Bundle.Revision = Snapshot.Revision;
	Bundle.Contexts.Reserve(Demands.Num());

	for (const FBlueprintHelperActionContextDemand& Demand : Demands)
	{
		if (Demand.StatementId.IsEmpty())
		{
			continue;
		}

		Bundle.AddOrMerge(BuildContext(Snapshot, Demand));
	}

	return Bundle;
}

FBlueprintHelperResolvedActionContext FBlueprintHelperActionContextInferenceService::BuildContext(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	FBlueprintHelperResolvedActionContext Context;
	Context.StatementId = Demand.StatementId;
	Context.ClusterKind = Demand.ClusterKind;
	Context.GraphName = Snapshot.Graph.GraphName;
	Context.SourceThread = EBlueprintHelperActionContextSourceThread::WorkerInference;
	Context.Semantic.Kind = Demand.SemanticKind;
	Context.Semantic.SemanticFamily = Demand.SemanticFamily;
	Context.Semantic.TypeOperation = Demand.TypeOperation;
	Context.Semantic.Query = Demand.Query;
	Context.Semantic.TargetPath = Demand.TargetPath;
	Context.Semantic.PropertyPath = Demand.PropertyPath;
	Context.Semantic.FieldOperation = Demand.FieldOperation;
	Context.Semantic.FieldScope = Demand.FieldScope;
	Context.Semantic.CapabilityId = Demand.CapabilityId;
	Context.Semantic.CapabilityFacts = Demand.CapabilityFacts;
	Context.Semantic.FunctionOperation = Demand.FunctionOperation;
	Context.Semantic.TransformOperation = Demand.TransformOperation;
	Context.Semantic.ScheduleOperation = Demand.ScheduleOperation;
	Context.Semantic.CreateOperation = Demand.CreateOperation;
	Context.Semantic.ContainerKind = Demand.ContainerKind;
	Context.Semantic.ContainerOperation = Demand.ContainerOperation;
	Context.Semantic.ClassPath = Demand.ClassPath;
	Context.Semantic.AssetPath = Demand.AssetPath;
	Context.Semantic.ElementType = Demand.ElementType;
	Context.Semantic.KeyType = Demand.KeyType;
	Context.Semantic.ValueType = Demand.ValueType;
	Context.Semantic.TypeName = Demand.TypeName;
	Context.Semantic.StructPath = Demand.StructPath;
	Context.Semantic.TypeStructureId = Demand.TypeStructureId;
	Context.Semantic.SearchMode = Demand.SearchMode;
	Context.Semantic.AmbiguityPolicy = Demand.AmbiguityPolicy;
	Context.Semantic.CategoryPriority = Demand.CategoryPriority;
	Context.Semantic.DefaultValues = Demand.DefaultValues;
	Context.Semantic.ArgumentNames = Demand.ArgumentNames;
	Context.Semantic.ArgumentTypes = Demand.ArgumentTypes;
	Context.Semantic.ArgumentPinTypes = Demand.ArgumentPinTypes;
	Context.Semantic.TargetObjectType = Demand.TargetObjectType;
	Context.Semantic.TargetObjectPinType = Demand.TargetObjectPinType;
	Context.Semantic.ExpectedReturnType = Demand.ExpectedReturnType;
	Context.Semantic.ExpectedReturnPinType = Demand.ExpectedReturnPinType;
	Context.Semantic.ContainerElementPinType = Demand.ContainerElementPinType;
	Context.Semantic.ContainerKeyPinType = Demand.ContainerKeyPinType;
	Context.Semantic.ContainerValuePinType = Demand.ContainerValuePinType;

	if (!Demand.TypeName.IsEmpty())
	{
		if (Context.Semantic.ExpectedReturnType.IsEmpty())
		{
			Context.Semantic.ExpectedReturnType = Demand.TypeName;
		}
		Context.Evidence.Add(TEXT("demand_type_name"), Demand.TypeName);
	}
	if (Demand.SemanticFamily != EBlueprintHelperActionSemanticFamily::Unknown)
	{
		Context.Evidence.Add(
			TEXT("semantic_family"),
			FBlueprintHelperActionResolutionCore::SemanticFamilyToString(Demand.SemanticFamily));
	}
	if (Demand.TypeOperation != EBlueprintHelperTypeOperation::None)
	{
		Context.Evidence.Add(
			TEXT("type_operation"),
			FBlueprintHelperActionResolutionCore::TypeOperationToString(Demand.TypeOperation));
	}
	if (!Demand.StructPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("struct_path"), Demand.StructPath);
	}
	if (!Demand.TypeStructureId.IsEmpty())
	{
		Context.Evidence.Add(TEXT("type_structure_id"), Demand.TypeStructureId);
	}

	const FString ExplicitBindingKind = Demand.DefaultValues.FindRef(TEXT("event_delegate.binding_object_kind")).TrimStartAndEnd();
	const FString ExplicitBindingEvidenceId = Demand.DefaultValues.FindRef(TEXT("event_delegate.binding_object_evidence_id")).TrimStartAndEnd();
	const FString ExplicitBindingPath = Demand.DefaultValues.FindRef(TEXT("event_delegate.binding_object_path")).TrimStartAndEnd();
	const FString ExplicitBindingProducerStatementId = Demand.DefaultValues.FindRef(TEXT("event_delegate.binding_object_statement_id")).TrimStartAndEnd();
	if (!ExplicitBindingKind.IsEmpty() || !ExplicitBindingEvidenceId.IsEmpty() || !ExplicitBindingPath.IsEmpty())
	{
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.binding_object_kind"), ExplicitBindingKind);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.binding_object_evidence_id"), ExplicitBindingEvidenceId);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.binding_object_path"), ExplicitBindingPath);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.binding_object_statement_id"), ExplicitBindingProducerStatementId);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(
			Context,
			TEXT("event_delegate.binding_object_node_guid"),
			Demand.DefaultValues.FindRef(TEXT("event_delegate.binding_object_node_guid")));
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(
			Context,
			TEXT("event_delegate.binding_object_pin_name"),
			Demand.DefaultValues.FindRef(TEXT("event_delegate.binding_object_pin_name")));
		if (ExplicitBindingKind.Equals(TEXT("function_return_ref"), ESearchCase::IgnoreCase)
			&& !ExplicitBindingProducerStatementId.IsEmpty()
			&& !ExplicitBindingProducerStatementId.Equals(Demand.StatementId, ESearchCase::IgnoreCase))
		{
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(
				Context,
				TEXT("event_delegate.binding_object_error"),
				TEXT("binding_object_cross_statement_unsupported"));
		}
	}
	else if (!Demand.BindingObjectPath.IsEmpty())
	{
		const FString BindingKind = Demand.BindingObjectPath.Equals(TEXT("self"), ESearchCase::IgnoreCase)
			? TEXT("self")
			: TEXT("component_ref");
		Context.Evidence.Add(TEXT("event_delegate.binding_object_kind"), BindingKind);
		Context.Evidence.Add(TEXT("event_delegate.binding_object_evidence_id"), FString::Printf(TEXT("%s:%s"), *BindingKind, *Demand.BindingObjectPath));
		Context.Evidence.Add(TEXT("event_delegate.binding_object_path"), Demand.BindingObjectPath);
	}
	if (!Demand.ComponentPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("event_delegate.component_property_name"), Demand.ComponentPath);
		Context.Evidence.Add(TEXT("event_delegate.component_path"), Demand.ComponentPath);
	}
	if (!Demand.DelegateName.IsEmpty())
	{
		Context.Evidence.Add(TEXT("event_delegate.delegate_name"), Demand.DelegateName);
	}
	if (!Demand.DelegateOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("event_delegate.operation"), Demand.DelegateOperation);
	}
	if (!Demand.FieldOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("field_operation"), Demand.FieldOperation);
	}
	if (!Demand.FieldScope.IsEmpty())
	{
		Context.Evidence.Add(TEXT("field_scope"), Demand.FieldScope);
	}
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.capability_id"), Demand.CapabilityId);
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field_capability_id"), Demand.CapabilityId);
	for (const TPair<FString, FString>& FactPair : Demand.CapabilityFacts)
	{
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, FactPair.Key, FactPair.Value);
	}
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(
		Context,
		TEXT("field_owner_class"),
		Demand.CapabilityFacts.FindRef(TEXT("field.owner_class")));
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.blocking_reason"), Demand.BlockingReason);
	if (!Demand.FunctionOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("function_operation"), Demand.FunctionOperation);
	}
	if (!Demand.TransformOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("transform_operation"), Demand.TransformOperation);
	}
	if (!Demand.ScheduleOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("schedule_operation"), Demand.ScheduleOperation);
	}
	if (!Demand.CreateOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("create_operation"), Demand.CreateOperation);
	}
	if (!Demand.ContainerKind.IsEmpty())
	{
		Context.Evidence.Add(TEXT("container_kind"), Demand.ContainerKind);
	}
	if (!Demand.ContainerOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("container_operation"), Demand.ContainerOperation);
	}
	if (!Demand.ClassPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("class_path"), Demand.ClassPath);
	}
	if (!Demand.AssetPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("asset_path"), Demand.AssetPath);
	}
	if (!Demand.ElementType.IsEmpty())
	{
		Context.Evidence.Add(TEXT("element_type"), Demand.ElementType);
	}
	if (!Demand.KeyType.IsEmpty())
	{
		Context.Evidence.Add(TEXT("key_type"), Demand.KeyType);
	}
	if (!Demand.ValueType.IsEmpty())
	{
		Context.Evidence.Add(TEXT("value_type"), Demand.ValueType);
	}
	if (!Demand.GraphLatentAllowed.IsEmpty())
	{
		Context.Evidence.Add(TEXT("graph_latent_allowed"), Demand.GraphLatentAllowed);
	}
	if (!Demand.DelegateSignature.IsEmpty())
	{
		Context.Evidence.Add(TEXT("event_delegate.delegate_signature"), Demand.DelegateSignature);
	}
	if (!Demand.HandlerName.IsEmpty())
	{
		Context.Evidence.Add(TEXT("event_delegate.handler_name"), Demand.HandlerName);
		if (!Snapshot.Graph.BlueprintClassPath.IsEmpty())
		{
			Context.Evidence.Add(TEXT("event_delegate.handler_scope_class_path"), Snapshot.Graph.BlueprintClassPath);
		}
	}
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.handler_function_path"), Demand.HandlerFunctionPath);
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.handler_source_cluster"), Demand.HandlerSourceCluster);
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.signature_evidence_id"), Demand.SignatureEvidenceId);
	if (!Demand.UnbindMode.IsEmpty())
	{
		Context.Evidence.Add(TEXT("event_delegate.unbind_mode"), Demand.UnbindMode);
	}
	if (!Demand.TargetObjectType.IsEmpty())
	{
		Context.Evidence.Add(TEXT("target_object_type"), Demand.TargetObjectType);
	}
	if (Demand.DefaultValues.Num() > 0)
	{
		Context.Evidence.Add(TEXT("default_value_count"), LexToString(Demand.DefaultValues.Num()));
		for (const TPair<FString, FString>& DefaultPair : Demand.DefaultValues)
		{
			if (DefaultPair.Key.StartsWith(TEXT("op."), ESearchCase::IgnoreCase)
				|| DefaultPair.Key.StartsWith(TEXT("generic."), ESearchCase::IgnoreCase)
				|| DefaultPair.Key.StartsWith(TEXT("container."), ESearchCase::IgnoreCase)
				|| DefaultPair.Key.StartsWith(TEXT("event_delegate."), ESearchCase::IgnoreCase))
			{
				UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, DefaultPair.Key, DefaultPair.Value);
			}
		}
	}
	if (Demand.ArgumentTypes.Num() > 0)
	{
		Context.Evidence.Add(TEXT("argument_type_count"), LexToString(Demand.ArgumentTypes.Num()));
		for (const TPair<FString, FString>& ArgumentTypePair : Demand.ArgumentTypes)
		{
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(
				Context,
				FString::Printf(TEXT("event_delegate.call_arg.%s.pin_type"), *ArgumentTypePair.Key),
				ArgumentTypePair.Value);
		}
	}

	if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Field)
	{
		FBlueprintHelperCallFunctionPinType LinkedSourcePinType;
		if (!Context.Semantic.TargetObjectPinType.IsValid()
			&& UGraphWriteActionContextUtils::FindFirstLinkedPinType(Snapshot, Demand.SourceSymbolIds, LinkedSourcePinType))
		{
			Context.Semantic.TargetObjectPinType = LinkedSourcePinType;
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(
				Context,
				TEXT("linked_source_pin_type"),
				UGraphWriteActionContextUtils::DescribePinTypeEvidence(LinkedSourcePinType));
		}
		if (!Context.Semantic.TargetObjectPinType.IsValid()
			&& !Demand.TargetObjectType.TrimStartAndEnd().IsEmpty())
		{
			Context.Semantic.TargetObjectPinType.Category = TEXT("object");
			Context.Semantic.TargetObjectPinType.ObjectPath = Demand.TargetObjectType.TrimStartAndEnd();
		}
		else if (Context.Semantic.TargetObjectPinType.IsValid()
			&& Context.Semantic.TargetObjectPinType.ObjectPath.TrimStartAndEnd().IsEmpty()
			&& !Demand.TargetObjectType.TrimStartAndEnd().IsEmpty())
		{
			Context.Semantic.TargetObjectPinType.ObjectPath = Demand.TargetObjectType.TrimStartAndEnd();
		}
		if (Context.Semantic.TargetObjectPinType.IsValid()
			&& Context.Semantic.FieldScope.Equals(TEXT("field_access"), ESearchCase::IgnoreCase))
		{
			FString TargetPinRef = Context.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref")).TrimStartAndEnd();
			if (TargetPinRef.IsEmpty() && Demand.SourceSymbolIds.Num() > 0)
			{
				TargetPinRef = FString::Printf(TEXT("source_symbol:%s"), *Demand.SourceSymbolIds[0].TrimStartAndEnd());
			}
			if (TargetPinRef.IsEmpty() && !Demand.TargetPath.TrimStartAndEnd().IsEmpty())
			{
				TargetPinRef = FString::Printf(TEXT("target:%s"), *Demand.TargetPath.TrimStartAndEnd());
			}
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("target_pin_ref"), TargetPinRef);
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("linked_pin_type_category"), Context.Semantic.TargetObjectPinType.Category);
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("linked_pin_type_object_path"), Context.Semantic.TargetObjectPinType.ObjectPath);
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.target_pin_ref"), TargetPinRef);
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.target_pin_type"), Context.Semantic.TargetObjectPinType.Category);
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.target_pin_object_path"), Context.Semantic.TargetObjectPinType.ObjectPath);
			if (!TargetPinRef.IsEmpty())
			{
				Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_ref")) = TargetPinRef;
			}
			if (!Context.Semantic.TargetObjectPinType.Category.TrimStartAndEnd().IsEmpty())
			{
				Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_type")) = Context.Semantic.TargetObjectPinType.Category.TrimStartAndEnd();
			}
			if (!Context.Semantic.TargetObjectPinType.ObjectPath.TrimStartAndEnd().IsEmpty())
			{
				Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_object_path")) = Context.Semantic.TargetObjectPinType.ObjectPath.TrimStartAndEnd();
			}
		}

		FBlueprintHelperCallFunctionPinType LinkedConsumerPinType;
		if (!Context.Semantic.ExpectedReturnPinType.IsValid()
			&& UGraphWriteActionContextUtils::FindFirstLinkedPinType(Snapshot, Demand.ConsumerSymbolIds, LinkedConsumerPinType))
		{
			Context.Semantic.ExpectedReturnPinType = LinkedConsumerPinType;
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(
				Context,
				TEXT("linked_consumer_pin_type"),
				UGraphWriteActionContextUtils::DescribePinTypeEvidence(LinkedConsumerPinType));
		}
	}

	if (const FBlueprintHelperActionContextFieldSnapshot* Field =
		UGraphWriteActionContextUtils::FindField(Snapshot, Demand))
	{
		if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction)
		{
			const FBlueprintHelperCallFunctionPinType TargetPinType =
				UGraphWriteActionContextUtils::MakeFieldPinType(*Field);
			if (TargetPinType.IsValid())
			{
				Context.Semantic.ArgumentPinTypes.Add(TEXT("target"), TargetPinType);
				UGraphWriteActionContextUtils::AddEvidenceIfPresent(
					Context,
					TEXT("container_target_pin_type"),
					UGraphWriteActionContextUtils::DescribePinTypeEvidence(TargetPinType));
			}
		}
		else
		{
			Context.Semantic.TargetObjectType = Field->OwnerClassPath;
		}
		UGraphWriteActionContextUtils::ProjectFieldSnapshot(Context, Demand, *Field);
		Context.Evidence.Add(TEXT("field_name"), Field->Name);
		Context.Evidence.Add(TEXT("field_pin_category"), Field->PinCategory);
		Context.Evidence.Add(TEXT("field_pin_sub_category"), Field->PinSubCategory);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field_pin_container"), Field->PinContainerType);
		Context.Evidence.Add(TEXT("field_owner_class"), Field->OwnerClassPath);
	}

	if (const FBlueprintHelperActionContextFieldSnapshot* DelegateField =
		UGraphWriteActionContextUtils::FindDelegateField(Snapshot, Demand))
	{
		Context.Semantic.PropertyPath = DelegateField->Name;
		Context.Semantic.TargetObjectType = DelegateField->OwnerClassPath;
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.delegate_owner_class_path"), DelegateField->OwnerClassPath);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.delegate_property_name"), DelegateField->Name);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.delegate_property_path"), DelegateField->FieldPath);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.delegate_signature_function_path"), DelegateField->DelegateSignatureFunctionPath);
		if (Demand.DelegateSignature.IsEmpty())
		{
			UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.delegate_signature"), DelegateField->DelegateSignatureFunctionPath);
		}
		Context.Evidence.FindOrAdd(TEXT("event_delegate.delegate_blueprint_assignable")) = DelegateField->bBlueprintAssignable ? TEXT("true") : TEXT("false");
		Context.Evidence.FindOrAdd(TEXT("event_delegate.delegate_blueprint_callable")) = DelegateField->bBlueprintCallable ? TEXT("true") : TEXT("false");
	}

	if (const FBlueprintHelperActionContextFieldSnapshot* ComponentField =
		UGraphWriteActionContextUtils::FindComponentField(Snapshot, Demand))
	{
		Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.component_name"), !UGraphWriteActionContextUtils::SnapshotFactValue(*ComponentField, TEXT("field.component_name")).IsEmpty()
			? UGraphWriteActionContextUtils::SnapshotFactValue(*ComponentField, TEXT("field.component_name"))
			: ComponentField->Name);
		Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.component_owner_class"), !UGraphWriteActionContextUtils::SnapshotFactValue(*ComponentField, TEXT("field.component_owner_class")).IsEmpty()
			? UGraphWriteActionContextUtils::SnapshotFactValue(*ComponentField, TEXT("field.component_owner_class"))
			: ComponentField->OwnerClassPath);
		Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.component_kind"), UGraphWriteActionContextUtils::SnapshotFactValue(*ComponentField, TEXT("field.component_kind")));
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.component_binding_owner_class_path"), ComponentField->OwnerClassPath);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.component_property_name"), ComponentField->Name);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.component_binding_field_path"), ComponentField->FieldPath);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("event_delegate.component_class_path"), ComponentField->PinSubCategoryObjectPath);
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.component_name"), Context.Semantic.CapabilityFacts.FindRef(TEXT("field.component_name")));
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.component_owner_class"), Context.Semantic.CapabilityFacts.FindRef(TEXT("field.component_owner_class")));
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.component_kind"), Context.Semantic.CapabilityFacts.FindRef(TEXT("field.component_kind")));
	}

	if (!Snapshot.Graph.GraphName.IsEmpty())
	{
		Context.Evidence.Add(TEXT("graph_name"), Snapshot.Graph.GraphName);
		Context.Evidence.Add(TEXT("target_graph"), Snapshot.Graph.GraphName);
	}
	if (!Context.Evidence.Contains(TEXT("graph_latent_allowed")))
	{
		Context.Evidence.Add(TEXT("graph_latent_allowed"), Snapshot.Graph.bLatentAllowed ? TEXT("true") : TEXT("false"));
	}
	if (!Demand.TargetGraphName.IsEmpty())
	{
		Context.Evidence.Add(TEXT("target_graph"), Demand.TargetGraphName);
	}

	if (!Demand.SourcePath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("source_path"), Demand.SourcePath);
	}

	return Context;
}

#if WITH_DEV_AUTOMATION_TESTS
FBlueprintHelperResolvedActionContext FBlueprintHelperActionContextInferenceService::BuildContextForTest(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	return BuildContext(Snapshot, Demand);
}
#endif
