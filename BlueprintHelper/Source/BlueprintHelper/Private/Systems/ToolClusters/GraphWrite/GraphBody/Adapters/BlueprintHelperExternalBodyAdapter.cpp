#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperExternalBodyAdapter.h"

#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperK2GraphEntryIdentityResolver.h"

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

static bool BlueprintHelperExternalBodyIsFunctionScope(const FString& ReplaceScope)
{
	return ReplaceScope.Equals(TEXT("function"), ESearchCase::IgnoreCase)
		|| ReplaceScope.Equals(TEXT("function_body"), ESearchCase::IgnoreCase);
}

static bool BlueprintHelperExternalBodyIsEventScope(const FString& ReplaceScope)
{
	return ReplaceScope.Equals(TEXT("event"), ESearchCase::IgnoreCase)
		|| ReplaceScope.Equals(TEXT("event_body"), ESearchCase::IgnoreCase);
}

static bool BlueprintHelperExternalBodyIsCustomEventScope(const FString& ReplaceScope)
{
	return ReplaceScope.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase)
		|| ReplaceScope.Equals(TEXT("custom_event_body"), ESearchCase::IgnoreCase);
}

static bool BlueprintHelperExternalBodyIsReadEntryScope(const FString& ReplaceScope)
{
	return BlueprintHelperExternalBodyIsFunctionScope(ReplaceScope)
		|| BlueprintHelperExternalBodyIsEventScope(ReplaceScope)
		|| BlueprintHelperExternalBodyIsCustomEventScope(ReplaceScope);
}

static bool BlueprintHelperExternalBodyIsReadContextRequest(const FBlueprintHelperGraphBodyRequest& Request)
{
	return Request.OperationKind.Equals(TEXT("read_context"), ESearchCase::IgnoreCase)
		|| Request.TaskSpecStrategy.Equals(TEXT("read_context"), ESearchCase::IgnoreCase);
}

static UEdGraph* BlueprintHelperExternalBodyFindEntryGraph(
	const FBlueprintHelperGraphBodyRequest& Request)
{
	if (!Request.Blueprint)
	{
		return nullptr;
	}

	if (BlueprintHelperExternalBodyIsFunctionScope(Request.ReplaceScope))
	{
		UEdGraph* FunctionGraph = FBlueprintHelperK2GraphBodyAdapterUtils::FindGraphByName(
			Request.Blueprint->FunctionGraphs,
			Request.GraphName);
		if (!FunctionGraph && !Request.EntryName.IsEmpty())
		{
			FunctionGraph = FBlueprintHelperK2GraphBodyAdapterUtils::FindGraphByName(
				Request.Blueprint->FunctionGraphs,
				Request.EntryName);
		}
		return FunctionGraph;
	}

	UEdGraph* EventGraph = FBlueprintHelperK2GraphBodyAdapterUtils::FindGraphByName(
		Request.Blueprint->UbergraphPages,
		Request.GraphName.IsEmpty() ? FString(TEXT("EventGraph")) : Request.GraphName);
	if (!EventGraph && Request.Blueprint->UbergraphPages.Num() > 0)
	{
		EventGraph = Request.Blueprint->UbergraphPages[0];
	}
	return EventGraph;
}

static FString BlueprintHelperExternalBodyTargetTypeForScope(const FString& ReplaceScope)
{
	if (BlueprintHelperExternalBodyIsEventScope(ReplaceScope))
	{
		return TEXT("event");
	}
	if (BlueprintHelperExternalBodyIsCustomEventScope(ReplaceScope))
	{
		return TEXT("custom_event");
	}
	if (BlueprintHelperExternalBodyIsFunctionScope(ReplaceScope))
	{
		return TEXT("function");
	}
	return TEXT("");
}

static bool BlueprintHelperExternalBodyFindMismatchedEntryIdentity(
	UEdGraph* Graph,
	const FBlueprintHelperK2GraphEntryQuery& Query,
	FBlueprintHelperK2GraphEntryIdentity& OutIdentity)
{
	OutIdentity = FBlueprintHelperK2GraphEntryIdentity();
	if (!Graph || Query.TargetName.IsEmpty())
	{
		return false;
	}

	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		FBlueprintHelperK2GraphEntryIdentity Identity;
		if (!Resolver.TryResolveNodeIdentity(Node, Identity))
		{
			continue;
		}
		if (!Identity.StableName.Equals(Query.TargetName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Query.GraphName.IsEmpty() && !Identity.GraphName.Equals(Query.GraphName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (Query.RequiredRole != EBlueprintHelperK2GraphBoundaryRole::Unknown &&
			Identity.Role != Query.RequiredRole)
		{
			continue;
		}
		if (!Resolver.DoesIdentityMatchQuery(Identity, Query))
		{
			OutIdentity = Identity;
			return true;
		}
	}
	return false;
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

	if (Request.Blueprint && BlueprintHelperExternalBodyIsReadEntryScope(Request.ReplaceScope))
	{
		UEdGraph* EntryGraph = BlueprintHelperExternalBodyFindEntryGraph(Request);
		if (!EntryGraph)
		{
			OutError = FString::Printf(TEXT("External body graph %s was not found."), *Request.GraphName);
			return false;
		}

		OutTarget.Graph = EntryGraph;
		OutTarget.GraphName = EntryGraph->GetName();

		const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
		FBlueprintHelperK2GraphEntryQuery EntryQuery;
		EntryQuery.TargetType = BlueprintHelperExternalBodyTargetTypeForScope(Request.ReplaceScope);
		EntryQuery.TargetName = Request.EntryName;
		EntryQuery.GraphName = EntryGraph->GetName();
		EntryQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;

		UEdGraphNode* EntryNode = nullptr;
		FBlueprintHelperK2GraphEntryIdentity EntryIdentity;
		FString EntryError;
		if (Resolver.TryFindEntryNode(EntryGraph, EntryQuery, EntryNode, EntryIdentity, EntryError))
		{
			OutTarget.EntryBoundaryNodes.AddUnique(EntryNode);
			OutTarget.ProtectedNodes.AddUnique(EntryNode);
		}

		if (BlueprintHelperExternalBodyIsFunctionScope(Request.ReplaceScope))
		{
			FBlueprintHelperK2GraphEntryQuery ExitQuery = EntryQuery;
			ExitQuery.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyExit;
			UEdGraphNode* ExitNode = nullptr;
			FBlueprintHelperK2GraphEntryIdentity ExitIdentity;
			FString ExitError;
			if (Resolver.TryFindEntryNode(EntryGraph, ExitQuery, ExitNode, ExitIdentity, ExitError))
			{
				OutTarget.ExitBoundaryNodes.AddUnique(ExitNode);
				OutTarget.ProtectedNodes.AddUnique(ExitNode);
			}
		}

		if (OutTarget.EntryBoundaryNodes.Num() == 0)
		{
			OutTarget.BodyEvidenceStatus = TEXT("missing_entry");
			OutTarget.BodyEvidenceErrorCode = TEXT("k2_entry_identity_not_found");
			OutTarget.BodyEvidenceErrorMessage = FString::Printf(
				TEXT("ReadContext could not resolve a K2 %s entry for target %s."),
				*EntryQuery.TargetType,
				*Request.EntryName);
			FBlueprintHelperK2GraphEntryIdentity MismatchedIdentity;
			if (BlueprintHelperExternalBodyFindMismatchedEntryIdentity(EntryGraph, EntryQuery, MismatchedIdentity))
			{
				OutTarget.BodyEvidenceStatus = TEXT("target_type_mismatch");
				OutTarget.BodyEvidenceErrorCode = TEXT("k2_entry_identity_target_type_mismatch");
				OutTarget.BodyEvidenceErrorMessage = FString::Printf(
					TEXT("ReadContext resolved %s but it did not match requested target_type=%s."),
					*MismatchedIdentity.StableName,
					*EntryQuery.TargetType);
			}
			if (!BlueprintHelperExternalBodyIsReadContextRequest(Request))
			{
				OutError = FString::Printf(TEXT("External body entry %s was not found."), *Request.EntryName);
				return false;
			}
		}
	}

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
		*Descriptor.RuntimeAdapterId);
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
	for (UEdGraphNode* Node : Target.EntryBoundaryNodes)
	{
		if (!Node)
		{
			continue;
		}
		const FString Ref = FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(Node);
		if (!Ref.IsEmpty())
		{
			Boundary.EntryNodeRefs.AddUnique(Ref);
			Boundary.ProtectedNodeRefs.AddUnique(Ref);
			FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticSources(Node, Ref, Boundary.SemanticSourceRefs);
		}
	}
	for (UEdGraphNode* Node : Target.ExitBoundaryNodes)
	{
		if (!Node)
		{
			continue;
		}
		const FString Ref = FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(Node);
		if (!Ref.IsEmpty())
		{
			Boundary.ExitNodeRefs.AddUnique(Ref);
			Boundary.ProtectedNodeRefs.AddUnique(Ref);
			FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticSources(Node, Ref, Boundary.SemanticSourceRefs);
		}
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
	const FBlueprintHelperGraphBodyTarget& Target,
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
	Projection.BodyEvidenceStatus = Target.BodyEvidenceStatus;
	Projection.BodyEvidenceErrorCode = Target.BodyEvidenceErrorCode;
	Projection.BodyEvidenceErrorMessage = Target.BodyEvidenceErrorMessage;
	return Projection;
}
