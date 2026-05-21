#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

namespace BlueprintHelperActionContextInference
{
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

	if (!Snapshot.Graph.GraphName.IsEmpty())
	{
		Context.Evidence.Add(TEXT("graph_name"), Snapshot.Graph.GraphName);
	}

	if (!Demand.SourcePath.IsEmpty())
	{
		Context.Evidence.Add(TEXT("source_path"), Demand.SourcePath);
	}

	return Context;
}
