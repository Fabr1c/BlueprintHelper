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
		TEXT("k2.block_implementation"), Descriptor))
	{
		return Descriptor;
	}

	Descriptor.RuntimeAdapterId = TEXT("k2.block_implementation");
	Descriptor.TaskSpecStrategy = TEXT("patch_owned_graph");
	Descriptor.BodyKind = EBlueprintHelperGraphBodyKind::K2BlockImplementation;
	Descriptor.BoundarySource = TEXT("block_body_adapter");
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

bool FBlueprintHelperK2BlockBodyAdapter::ResolveTarget(
	const FBlueprintHelperGraphBodyRequest& Request,
	FBlueprintHelperGraphBodyTarget& OutTarget,
	FString& OutError) const
{
	OutTarget.Blueprint = Request.Blueprint;
	OutTarget.AssetPath = Request.AssetPath;
	OutTarget.GraphName = Request.GraphName;
	OutTarget.EntryName = Request.EntryName;

	if (Request.Payload.IsValid())
	{
		const TSharedPtr<FJsonObject>* Target = nullptr;
		if (Request.Payload->TryGetObjectField(TEXT("target"), Target) && Target && Target->IsValid())
		{
			if (OutTarget.AssetPath.IsEmpty())
			{
				(*Target)->TryGetStringField(TEXT("asset_path"), OutTarget.AssetPath);
			}
			if (OutTarget.GraphName.IsEmpty())
			{
				(*Target)->TryGetStringField(TEXT("graph"), OutTarget.GraphName);
			}
		}
	}

	OutTarget.BodyIdentity = FString::Printf(
		TEXT("%s|%s|%s"),
		*OutTarget.AssetPath,
		*OutTarget.GraphName,
		*FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(Descriptor.BodyKind));
	OutError.Reset();
	return true;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperK2BlockBodyAdapter::BuildBoundaryModel(
	const TSharedRef<FJsonObject>& Payload) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
	Boundary.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
	Boundary.BodyKind = Descriptor.BodyKind;
	Boundary.GraphFamily = TEXT("k2");

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
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("protected_node_refs"), Boundary.ProtectedNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("deletable_node_refs"), Boundary.DeletableNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("generated_node_refs"), Boundary.GeneratedNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("imported_body_node_refs"), Boundary.ImportedBodyNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("reachable_body_flow_node_refs"), Boundary.ReachableBodyFlowNodeRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("semantic_source_refs"), Boundary.SemanticSourceRefs);
	BlueprintHelperReadGraphBodyStringArray(Payload, TEXT("connectivity_exception_codes"), Boundary.ConnectivityExceptionCodes);
	return Boundary;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperK2BlockBodyAdapter::BuildBoundaryModel(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyRequest& Request) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	if (Request.Payload.IsValid())
	{
		Boundary = BuildBoundaryModel(Request.Payload.ToSharedRef());
	}
	else
	{
		Boundary.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
		Boundary.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
		Boundary.BodyKind = Descriptor.BodyKind;
		Boundary.GraphFamily = TEXT("k2");
	}

	if (Boundary.TargetAssetPath.IsEmpty())
	{
		Boundary.TargetAssetPath = Target.AssetPath;
	}
	if (Boundary.GraphName.IsEmpty())
	{
		Boundary.GraphName = Target.GraphName;
	}
	return Boundary;
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperK2BlockBodyAdapter::SelectConnectivityPolicy(
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperK2BlockBodyAdapter::BuildConnectivityPolicy(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return SelectConnectivityPolicy(Boundary);
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperK2BlockBodyAdapter::BuildMutationPlan(
	const TSharedRef<FJsonObject>& Payload) const
{
	FBlueprintHelperGraphBodyRequest Request;
	Request.Payload = Payload;
	FBlueprintHelperGraphBodyTarget Target;
	FString Error;
	ResolveTarget(Request, Target, Error);
	const FBlueprintHelperGraphBodyBoundaryModel Boundary = BuildBoundaryModel(Target, Request);
	return BuildMutationPlan(Target, Boundary, Request);
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperK2BlockBodyAdapter::BuildMutationPlan(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyRequest&) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = Boundary;
	Plan.ConnectivityPolicy = BuildConnectivityPolicy(Target, Boundary);
	Plan.Steps.Add({TEXT("resolve_owned_block_boundary"), TEXT("Resolve owned K2 block boundary from payload and read_context refs.")});
	Plan.Steps.Add({TEXT("delegate_existing_graphwrite_mutation"), TEXT("Delegate node creation and graph mutation to the existing service path.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}

FBlueprintHelperGraphBodySemanticContext FBlueprintHelperK2BlockBodyAdapter::BuildSemanticContext(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodySemanticContext Context;
	Context.ContextId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	Context.GraphOwnedSymbolRefs = Boundary.SemanticSourceRefs;
	return Context;
}

FBlueprintHelperGraphBodyReconnectPlan FBlueprintHelperK2BlockBodyAdapter::BuildReconnectPlan(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReconnectPlan Plan;
	Plan.EntryBoundaryRefs = Boundary.EntryNodeRefs;
	Plan.ExitBoundaryRefs = Boundary.ExitNodeRefs;
	Plan.bReconnectImportedExecToExitBoundary = Boundary.ExitNodeRefs.Num() > 0;
	return Plan;
}

FBlueprintHelperGraphBodyReadbackProjection FBlueprintHelperK2BlockBodyAdapter::BuildReadbackProjection(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReadbackProjection Projection;
	Projection.ProjectionId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	if (Boundary.EntryNodeRefs.Num() > 0)
	{
		Projection.EntryNodeRef = Boundary.EntryNodeRefs[0];
	}
	Projection.ExitNodeRefs = Boundary.ExitNodeRefs;
	Projection.VisibleBoundaryNodeRefs.Append(Boundary.EntryNodeRefs);
	Projection.VisibleBoundaryNodeRefs.Append(Boundary.ExitNodeRefs);
	return Projection;
}
