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
		TEXT("k2.external_body"), Descriptor))
	{
		return Descriptor;
	}

	Descriptor.RuntimeAdapterId = TEXT("k2.external_body");
	Descriptor.TaskSpecStrategy = TEXT("merge_external_flow");
	Descriptor.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;
	Descriptor.BoundarySource = TEXT("external_body_adapter");
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

bool FBlueprintHelperExternalBodyAdapter::ResolveTarget(
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

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperExternalBodyAdapter::BuildBoundaryModel(
	const TSharedRef<FJsonObject>& Payload) const
{
	FBlueprintHelperGraphBodyBoundaryModel Boundary;
	Boundary.RuntimeAdapterId = Descriptor.RuntimeAdapterId;
	Boundary.TaskSpecStrategy = Descriptor.TaskSpecStrategy;
	Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;
	Boundary.GraphFamily = TEXT("k2");

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
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("protected_node_refs"), Boundary.ProtectedNodeRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("deletable_node_refs"), Boundary.DeletableNodeRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("generated_node_refs"), Boundary.GeneratedNodeRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("imported_body_node_refs"), Boundary.ImportedBodyNodeRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("reachable_body_flow_node_refs"), Boundary.ReachableBodyFlowNodeRefs);
	BlueprintHelperReadExternalBodyStringArray(Payload, TEXT("semantic_source_refs"), Boundary.SemanticSourceRefs);
	return Boundary;
}

FBlueprintHelperGraphBodyBoundaryModel FBlueprintHelperExternalBodyAdapter::BuildBoundaryModel(
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
		Boundary.BodyKind = EBlueprintHelperGraphBodyKind::K2ExternalBody;
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

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperExternalBodyAdapter::SelectConnectivityPolicy(
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(Boundary);
}

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperExternalBodyAdapter::BuildConnectivityPolicy(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	return SelectConnectivityPolicy(Boundary);
}

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperExternalBodyAdapter::BuildMutationPlan(
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

FBlueprintHelperGraphBodyMutationPlan FBlueprintHelperExternalBodyAdapter::BuildMutationPlan(
	const FBlueprintHelperGraphBodyTarget& Target,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyRequest&) const
{
	FBlueprintHelperGraphBodyMutationPlan Plan;
	Plan.AdapterId = GetAdapterId();
	Plan.BoundaryModel = Boundary;
	Plan.ConnectivityPolicy = BuildConnectivityPolicy(Target, Boundary);
	Plan.Steps.Add({TEXT("resolve_external_anchor_boundary"), TEXT("Resolve external anchors as a distinct body boundary.")});
	Plan.Steps.Add({TEXT("delegate_existing_external_mutation"), TEXT("Delegate mutation to the existing external write service path.")});
	Plan.bCreatesNodesInsideAdapter = false;
	return Plan;
}

FBlueprintHelperGraphBodySemanticContext FBlueprintHelperExternalBodyAdapter::BuildSemanticContext(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodySemanticContext Context;
	Context.ContextId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(Boundary);
	Context.GraphOwnedSymbolRefs = Boundary.SemanticSourceRefs;
	return Context;
}

FBlueprintHelperGraphBodyReconnectPlan FBlueprintHelperExternalBodyAdapter::BuildReconnectPlan(
	const FBlueprintHelperGraphBodyTarget&,
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const
{
	FBlueprintHelperGraphBodyReconnectPlan Plan;
	Plan.EntryBoundaryRefs = Boundary.EntryNodeRefs;
	Plan.ExitBoundaryRefs = Boundary.ExitNodeRefs;
	Plan.RequiredLinkCodes.Append(Boundary.ExternalAnchorRefs);
	return Plan;
}

FBlueprintHelperGraphBodyReadbackProjection FBlueprintHelperExternalBodyAdapter::BuildReadbackProjection(
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
	Projection.VisibleBoundaryNodeRefs.Append(Boundary.ExternalAnchorRefs);
	Projection.VisibleBoundaryNodeRefs.Append(Boundary.EntryNodeRefs);
	Projection.VisibleBoundaryNodeRefs.Append(Boundary.ExitNodeRefs);
	return Projection;
}
