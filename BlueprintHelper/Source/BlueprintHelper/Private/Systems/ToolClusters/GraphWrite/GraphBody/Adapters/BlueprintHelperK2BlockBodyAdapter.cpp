#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2BlockBodyAdapter.h"

#include "Dom/JsonValue.h"

static void BlueprintHelperReadGraphBodyStringArray(
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

static FBlueprintHelperGraphBodyAdapterDescriptor BlueprintHelperResolveDefaultK2BlockDescriptor()
{
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	if (FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(
		TEXT("merge_blueprint_graph"), Descriptor))
	{
		return Descriptor;
	}

	Descriptor.RuntimeAdapterId = TEXT("merge_blueprint_graph");
	Descriptor.TaskSpecStrategy = TEXT("merge_owned_graph");
	Descriptor.BodyKind = EBlueprintHelperGraphBodyKind::K2BlockImplementation;
	Descriptor.BoundarySource = TEXT("merge_service");
	Descriptor.bSupportsDryRunUnitOfWork = true;
	return Descriptor;
}

FBlueprintHelperK2BlockBodyAdapter::FBlueprintHelperK2BlockBodyAdapter()
	: Descriptor(BlueprintHelperResolveDefaultK2BlockDescriptor())
{
}

FBlueprintHelperK2BlockBodyAdapter::FBlueprintHelperK2BlockBodyAdapter(
	const FBlueprintHelperGraphBodyAdapterDescriptor& InDescriptor)
	: Descriptor(InDescriptor)
{
}

FString FBlueprintHelperK2BlockBodyAdapter::GetAdapterId() const
{
	return Descriptor.RuntimeAdapterId;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperK2BlockBodyAdapter::BuildBoundaryModel(
	const TSharedRef<FJsonObject>& Payload) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
	Boundary.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
	Boundary.BodyKind = Descriptor.BodyKind;

	const TSharedPtr<FJsonObject>* Target = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), Target) && Target && Target->IsValid())
	{
		(*Target)->TryGetStringField(TEXT("asset_path"), Boundary.TargetAssetPath);
		(*Target)->TryGetStringField(TEXT("graph"), Boundary.GraphName);
		(*Target)->TryGetStringField(TEXT("block_id"), Boundary.OwnedBlockId);
	}

	const TSharedPtr<FJsonObject>* PatchedRef = nullptr;
	if (Payload->TryGetObjectField(TEXT("patched_ref"), PatchedRef) && PatchedRef && PatchedRef->IsValid())
	{
		FString BlockId;
		if ((*PatchedRef)->TryGetStringField(TEXT("block_id"), BlockId) && Boundary.OwnedBlockId.IsEmpty())
		{
			Boundary.OwnedBlockId = BlockId;
		}

		FString NodeRef;
		if ((*PatchedRef)->TryGetStringField(TEXT("node_ref"), NodeRef) && !NodeRef.IsEmpty())
		{
			Boundary.ImportedBodyNodeRefs.Add(NodeRef);
		}
	}

	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("entry_node_refs"), Boundary.EntryNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("exit_node_refs"), Boundary.ExitNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("generated_node_refs"), Boundary.GeneratedNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("imported_body_node_refs"), Boundary.ImportedBodyNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("connectivity_exception_codes"), Boundary.ConnectivityExceptionCodes);
	return Boundary;
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperK2BlockBodyAdapter::SelectConnectivityPolicy(
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperK2BlockBodyAdapter::BuildMutationPlan(
	const TSharedRef<FJsonObject>& Payload) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = BuildBoundaryModel(Payload);
	Plan.ConnectivityPolicy = SelectConnectivityPolicy(Plan.BoundaryModel);
	Plan.Steps.Add({TEXT("resolve_owned_block_boundary"), TEXT("Resolve owned K2 block boundary from payload and read_context refs.")});
	Plan.Steps.Add({TEXT("delegate_existing_graphwrite_mutation"), TEXT("Delegate node creation and graph mutation to the existing service path.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}
