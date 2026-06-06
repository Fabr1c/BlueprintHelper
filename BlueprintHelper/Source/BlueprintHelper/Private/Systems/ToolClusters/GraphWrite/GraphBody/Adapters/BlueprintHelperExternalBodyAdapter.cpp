#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperExternalBodyAdapter.h"

#include "Dom/JsonValue.h"

static void BlueprintHelperReadExternalBodyStringArray(
	const TSharedRef<FJsonObject>& Payload,
	const FString& FieldName,
	TArray<FString>& OutValues)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Payload->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString TextValue;
		if (Value.IsValid() && Value->TryGetString(TextValue) && !TextValue.IsEmpty())
		{
			OutValues.Add(TextValue);
		}
	}
}

static FBlueprintHelperGraphBodyAdapterDescriptor BlueprintHelperResolveDefaultExternalDescriptor()
{
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	if (FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(
		TEXT("merge_external_flow"), Descriptor))
	{
		return Descriptor;
	}

	Descriptor.RuntimeAdapterId = TEXT("merge_external_flow");
	Descriptor.TaskSpecStrategy = TEXT("merge_external_flow");
	Descriptor.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;
	Descriptor.BoundarySource = TEXT("external_anchor_resolver");
	Descriptor.bSupportsDryRunUnitOfWork = true;
	Descriptor.bSupportsExternalAnchors = true;
	return Descriptor;
}

FBlueprintHelperExternalBodyAdapter::FBlueprintHelperExternalBodyAdapter()
	: Descriptor(BlueprintHelperResolveDefaultExternalDescriptor())
{
}

FBlueprintHelperExternalBodyAdapter::FBlueprintHelperExternalBodyAdapter(
	const FBlueprintHelperGraphBodyAdapterDescriptor& InDescriptor)
	: Descriptor(InDescriptor)
{
}

FString FBlueprintHelperExternalBodyAdapter::GetAdapterId() const
{
	return Descriptor.RuntimeAdapterId;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperExternalBodyAdapter::BuildBoundaryModel(
	const TSharedRef<FJsonObject>& Payload) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
	Boundary.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;

	const TSharedPtr<FJsonObject>* Target = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), Target) && Target && Target->IsValid())
	{
		(*Target)->TryGetStringField(TEXT("asset_path"), Boundary.TargetAssetPath);
		(*Target)->TryGetStringField(TEXT("graph"), Boundary.GraphName);
		FString AnchorRef;
		if ((*Target)->TryGetStringField(TEXT("external_anchor_ref"), AnchorRef) && !AnchorRef.IsEmpty())
		{
			Boundary.ExternalAnchorRefs.Add(AnchorRef);
		}
	}

	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("external_anchor_refs"), Boundary.ExternalAnchorRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("entry_node_refs"), Boundary.EntryNodeRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("exit_node_refs"), Boundary.ExitNodeRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("generated_node_refs"), Boundary.GeneratedNodeRefs);
	return Boundary;
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperExternalBodyAdapter::SelectConnectivityPolicy(
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperExternalBodyAdapter::BuildMutationPlan(
	const TSharedRef<FJsonObject>& Payload) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = BuildBoundaryModel(Payload);
	Plan.ConnectivityPolicy = SelectConnectivityPolicy(Plan.BoundaryModel);
	Plan.Steps.Add({TEXT("resolve_external_anchor_boundary"), TEXT("Resolve external anchors as a distinct body boundary.")});
	Plan.Steps.Add({TEXT("delegate_existing_external_mutation"), TEXT("Delegate mutation to the existing external write service path.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}
