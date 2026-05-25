#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

namespace BlueprintHelperActionContextInference
{
static bool MatchesToken(const FString& Value, const FString& Token)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	const FString CleanToken = Token.TrimStartAndEnd();
	return !CleanValue.IsEmpty()
		&& !CleanToken.IsEmpty()
		&& (CleanValue.Equals(CleanToken, ESearchCase::IgnoreCase)
			|| CleanValue.EndsWith(FString(TEXT(".")) + CleanToken, ESearchCase::IgnoreCase)
			|| CleanToken.EndsWith(FString(TEXT(".")) + CleanValue, ESearchCase::IgnoreCase));
}

static void AddEvidenceIfPresent(
	FBlueprintHelperResolvedActionContext& Context,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		Context.Evidence.FindOrAdd(Key) = CleanValue;
	}
}

static FString DescribePinTypeEvidence(const FBlueprintHelperCallFunctionPinType& PinType)
{
	if (!PinType.IsValid())
	{
		return FString();
	}

	TArray<FString> Parts;
	if (!PinType.Category.IsEmpty())
	{
		Parts.Add(PinType.Category);
	}
	if (!PinType.SubCategory.IsEmpty())
	{
		Parts.Add(PinType.SubCategory);
	}
	if (!PinType.ObjectPath.IsEmpty())
	{
		Parts.Add(PinType.ObjectPath);
	}
	if (!PinType.ContainerType.IsEmpty())
	{
		Parts.Add(PinType.ContainerType);
	}
	return FString::Join(Parts, TEXT("|"));
}

static bool FindFirstLinkedPinType(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const TArray<FString>& SymbolIds,
	FBlueprintHelperCallFunctionPinType& OutPinType)
{
	for (const FString& SymbolId : SymbolIds)
	{
		if (const FBlueprintHelperCallFunctionPinType* PinType = Snapshot.SymbolPinTypes.Find(SymbolId))
		{
			if (PinType->IsValid())
			{
				OutPinType = *PinType;
				return true;
			}
		}
	}
	return false;
}

static const FBlueprintHelperActionContextFieldSnapshot* FindField(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	return Snapshot.Fields.FindByPredicate(
		[&Demand](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			return Field.Name == Demand.TargetPath
				|| Field.Name == Demand.PropertyPath
				|| (!Demand.TargetPath.IsEmpty() && Demand.TargetPath.EndsWith(FString(TEXT(".")) + Field.Name));
		});
}

static const FBlueprintHelperActionContextFieldSnapshot* FindDelegateField(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	return Snapshot.Fields.FindByPredicate(
		[&Demand](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			if (!Field.bMulticastDelegate)
			{
				return false;
			}
			return MatchesToken(Field.Name, Demand.DelegateName)
				|| MatchesToken(Field.Name, Demand.PropertyPath)
				|| MatchesToken(Field.Name, Demand.Query)
				|| MatchesToken(Field.FieldPath, Demand.PropertyPath);
		});
}

static const FBlueprintHelperActionContextFieldSnapshot* FindComponentField(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	return Snapshot.Fields.FindByPredicate(
		[&Demand](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			if (!Field.bComponent)
			{
				return false;
			}
			return MatchesToken(Field.Name, Demand.ComponentPath)
				|| MatchesToken(Field.FieldPath, Demand.ComponentPath)
				|| MatchesToken(Field.Name, Demand.TargetPath)
				|| MatchesToken(Field.FieldPath, Demand.TargetPath)
				|| MatchesToken(Field.Name, Demand.BindingObjectPath)
				|| MatchesToken(Field.FieldPath, Demand.BindingObjectPath);
		});
}
}

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
	Context.Semantic.StableId = Demand.StatementId;
	Context.Semantic.TargetPath = Demand.TargetPath;
	Context.Semantic.PropertyPath = Demand.PropertyPath;
	Context.Semantic.FieldOperation = Demand.FieldOperation;
	Context.Semantic.FieldScope = Demand.FieldScope;
	Context.Semantic.FunctionOperation = Demand.FunctionOperation;
	Context.Semantic.TransformOperation = Demand.TransformOperation;
	Context.Semantic.ScheduleOperation = Demand.ScheduleOperation;
	Context.Semantic.CreateOperation = Demand.CreateOperation;
	Context.Semantic.ClassPath = Demand.ClassPath;
	Context.Semantic.AssetPath = Demand.AssetPath;
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

	if (!Demand.BindingObjectPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("binding_object_path"), Demand.BindingObjectPath);
	}
	if (!Demand.ComponentPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("component_path"), Demand.ComponentPath);
	}
	if (!Demand.DelegateName.IsEmpty())
	{
		Context.Evidence.Add(TEXT("delegate_name"), Demand.DelegateName);
	}
	if (!Demand.DelegateOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("delegate_operation"), Demand.DelegateOperation);
	}
	if (!Demand.FieldOperation.IsEmpty())
	{
		Context.Evidence.Add(TEXT("field_operation"), Demand.FieldOperation);
	}
	if (!Demand.FieldScope.IsEmpty())
	{
		Context.Evidence.Add(TEXT("field_scope"), Demand.FieldScope);
	}
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
	if (!Demand.ClassPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("class_path"), Demand.ClassPath);
	}
	if (!Demand.AssetPath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("asset_path"), Demand.AssetPath);
	}
	if (!Demand.GraphLatentAllowed.IsEmpty())
	{
		Context.Evidence.Add(TEXT("graph_latent_allowed"), Demand.GraphLatentAllowed);
	}
	if (!Demand.DelegateSignature.IsEmpty())
	{
		Context.Evidence.Add(TEXT("delegate_signature"), Demand.DelegateSignature);
	}
	if (!Demand.HandlerName.IsEmpty())
	{
		Context.Evidence.Add(TEXT("handler_name"), Demand.HandlerName);
		if (!Snapshot.Graph.BlueprintClassPath.IsEmpty())
		{
			Context.Evidence.Add(TEXT("handler_scope_class_path"), Snapshot.Graph.BlueprintClassPath);
		}
	}
	BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("handler_function_path"), Demand.HandlerFunctionPath);
	BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("handler_source_cluster"), Demand.HandlerSourceCluster);
	BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("signature_evidence_id"), Demand.SignatureEvidenceId);
	if (!Demand.UnbindMode.IsEmpty())
	{
		Context.Evidence.Add(TEXT("unbind_mode"), Demand.UnbindMode);
	}
	if (!Demand.TargetObjectType.IsEmpty())
	{
		Context.Evidence.Add(TEXT("target_object_type"), Demand.TargetObjectType);
	}
	if (Demand.DefaultValues.Num() > 0)
	{
		Context.Evidence.Add(TEXT("default_value_count"), LexToString(Demand.DefaultValues.Num()));
	}
	if (Demand.ArgumentTypes.Num() > 0)
	{
		Context.Evidence.Add(TEXT("argument_type_count"), LexToString(Demand.ArgumentTypes.Num()));
	}

	if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Field)
	{
		FBlueprintHelperCallFunctionPinType LinkedSourcePinType;
		if (!Context.Semantic.TargetObjectPinType.IsValid()
			&& BlueprintHelperActionContextInference::FindFirstLinkedPinType(Snapshot, Demand.SourceSymbolIds, LinkedSourcePinType))
		{
			Context.Semantic.TargetObjectPinType = LinkedSourcePinType;
			BlueprintHelperActionContextInference::AddEvidenceIfPresent(
				Context,
				TEXT("linked_source_pin_type"),
				BlueprintHelperActionContextInference::DescribePinTypeEvidence(LinkedSourcePinType));
		}

		FBlueprintHelperCallFunctionPinType LinkedConsumerPinType;
		if (!Context.Semantic.ExpectedReturnPinType.IsValid()
			&& BlueprintHelperActionContextInference::FindFirstLinkedPinType(Snapshot, Demand.ConsumerSymbolIds, LinkedConsumerPinType))
		{
			Context.Semantic.ExpectedReturnPinType = LinkedConsumerPinType;
			BlueprintHelperActionContextInference::AddEvidenceIfPresent(
				Context,
				TEXT("linked_consumer_pin_type"),
				BlueprintHelperActionContextInference::DescribePinTypeEvidence(LinkedConsumerPinType));
		}
	}

	if (const FBlueprintHelperActionContextFieldSnapshot* Field =
		BlueprintHelperActionContextInference::FindField(Snapshot, Demand))
	{
		Context.Semantic.TargetObjectType = Field->OwnerClassPath;
		Context.Evidence.Add(TEXT("field_name"), Field->Name);
		Context.Evidence.Add(TEXT("field_pin_category"), Field->PinCategory);
		Context.Evidence.Add(TEXT("field_pin_sub_category"), Field->PinSubCategory);
		Context.Evidence.Add(TEXT("field_owner_class"), Field->OwnerClassPath);
	}

	if (const FBlueprintHelperActionContextFieldSnapshot* DelegateField =
		BlueprintHelperActionContextInference::FindDelegateField(Snapshot, Demand))
	{
		Context.Semantic.PropertyPath = DelegateField->Name;
		Context.Semantic.TargetObjectType = DelegateField->OwnerClassPath;
		BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("delegate_owner_class_path"), DelegateField->OwnerClassPath);
		BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("delegate_property_name"), DelegateField->Name);
		BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("delegate_property_path"), DelegateField->FieldPath);
		BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("delegate_signature_function_path"), DelegateField->DelegateSignatureFunctionPath);
		if (Demand.DelegateSignature.IsEmpty())
		{
			BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("delegate_signature"), DelegateField->DelegateSignatureFunctionPath);
		}
		Context.Evidence.FindOrAdd(TEXT("delegate_blueprint_assignable")) = DelegateField->bBlueprintAssignable ? TEXT("true") : TEXT("false");
		Context.Evidence.FindOrAdd(TEXT("delegate_blueprint_callable")) = DelegateField->bBlueprintCallable ? TEXT("true") : TEXT("false");
	}

	if (const FBlueprintHelperActionContextFieldSnapshot* ComponentField =
		BlueprintHelperActionContextInference::FindComponentField(Snapshot, Demand))
	{
		BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("component_binding_owner_class_path"), ComponentField->OwnerClassPath);
		BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("component_property_name"), ComponentField->Name);
		BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("component_binding_field_path"), ComponentField->FieldPath);
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
