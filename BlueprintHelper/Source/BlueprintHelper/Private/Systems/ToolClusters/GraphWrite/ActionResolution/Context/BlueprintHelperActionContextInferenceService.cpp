#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

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
				|| MatchesToken(Field.FieldPath, Demand.TargetPath);
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
	Context.Semantic.Query = Demand.Query;
	Context.Semantic.StableId = Demand.StatementId;
	Context.Semantic.TargetPath = Demand.TargetPath;
	Context.Semantic.PropertyPath = Demand.PropertyPath;
	Context.Semantic.TypeName = Demand.TypeName;
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

	if (!Demand.TypeName.IsEmpty())
	{
		if (Context.Semantic.ExpectedReturnType.IsEmpty())
		{
			Context.Semantic.ExpectedReturnType = Demand.TypeName;
		}
		Context.Evidence.Add(TEXT("demand_type_name"), Demand.TypeName);
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
