#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static FBlueprintHelperGraphBodyAdapterDescriptor BlueprintHelperMakeGraphBodyDescriptor(
	const TCHAR* RuntimeAdapterId,
	const TCHAR* TaskSpecStrategy,
	EBlueprintHelperGraphBodyKind BodyKind,
	const TCHAR* BoundarySource,
	bool bSupportsDryRunUnitOfWork,
	bool bSupportsExternalAnchors,
	bool bReservedOnly = false)
{
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	Descriptor.RuntimeAdapterId = RuntimeAdapterId;
	Descriptor.TaskSpecStrategy = TaskSpecStrategy;
	Descriptor.BodyKind = BodyKind;
	Descriptor.BoundarySource = BoundarySource;
	Descriptor.bSupportsDryRunUnitOfWork = bSupportsDryRunUnitOfWork;
	Descriptor.bSupportsExternalAnchors = bSupportsExternalAnchors;
	Descriptor.bReservedOnly = bReservedOnly;
	return Descriptor;
}

TArray<FBlueprintHelperGraphBodyAdapterDescriptor> FBlueprintHelperGraphBodyAdapterRegistry::GetKnownDescriptors()
{
	TArray<FBlueprintHelperGraphBodyAdapterDescriptor> Descriptors;
	Descriptors.Reserve(19);
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.custom_event_body"),
		TEXT("append_new_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2CustomEventBody,
		TEXT("custom_event_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.event_body"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2EventBody,
		TEXT("event_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.function_body"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2FunctionBody,
		TEXT("function_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.macro_body"),
		TEXT("replace_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2MacroBody,
		TEXT("macro_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.block_implementation"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::K2BlockImplementation,
		TEXT("block_body_adapter"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_body"),
		TEXT("merge_external_flow"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_body_adapter"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("material.function_body"),
		TEXT(""),
		EBlueprintHelperGraphBodyKind::ReservedMaterialFunctionBody,
		TEXT("reserved"),
		false,
		false,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("animation.graph_body"),
		TEXT(""),
		EBlueprintHelperGraphBodyKind::ReservedAnimationGraphBody,
		TEXT("reserved"),
		false,
		false,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.ensure_entry"),
		TEXT("append_new_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_append_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.insert_flow"),
		TEXT("merge_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_merge_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_graph.insert_external_flow"),
		TEXT("merge_external_flow"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_graph_merge_service"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_graph.replace_body"),
		TEXT("replace_external_body"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_graph_replace_body_service"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_graph.patch_links.connect_pins"),
		TEXT("patch_external_links"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_graph_patch_links_service"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_graph.patch_links.disconnect_link"),
		TEXT("patch_external_links"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_graph_patch_links_service"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_graph.patch_links.replace_link"),
		TEXT("patch_external_links"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_graph_patch_links_service"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_graph.patch.node_property"),
		TEXT("patch_external_graph"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_graph_patch_service"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.external_graph.patch.pin_default"),
		TEXT("patch_external_graph"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		TEXT("external_graph_patch_service"),
		true,
		true));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.patch.connect_pins"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_patch_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.patch.node_delete"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_patch_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.patch.disconnect_link"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_patch_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.patch.node_comment"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_patch_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.patch.pin_default"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_patch_service"),
		true,
		false));
	Descriptors.Add(BlueprintHelperMakeGraphBodyDescriptor(
		TEXT("k2.owned_graph.patch.replace_link"),
		TEXT("patch_owned_graph"),
		EBlueprintHelperGraphBodyKind::Unknown,
		TEXT("owned_graph_patch_service"),
		true,
		false));
	return Descriptors;
}

bool FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(
	const FString& RuntimeAdapterId,
	FBlueprintHelperGraphBodyAdapterDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperGraphBodyAdapterDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (Descriptor.RuntimeAdapterId.Equals(RuntimeAdapterId, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}

static FString BlueprintHelperGraphWriteRouteSyncArtifactPath()
{
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Generated/BlueprintHelperGraphWriteRouteAdapterSync.generated.json"));
}

static void BlueprintHelperAddGraphWriteRouteSyncIssue(
	TArray<FBlueprintHelperGraphWriteRouteSyncValidationIssue>& Issues,
	const FString& RouteId,
	const FString& RuntimeAdapterId,
	const FString& Status,
	const FString& Code,
	const FString& Message)
{
	FBlueprintHelperGraphWriteRouteSyncValidationIssue Issue;
	Issue.RouteId = RouteId;
	Issue.RuntimeAdapterId = RuntimeAdapterId;
	Issue.Status = Status;
	Issue.Code = Code;
	Issue.Message = Message;
	Issues.Add(MoveTemp(Issue));
}

TArray<FBlueprintHelperGraphWriteRouteSyncValidationIssue>
FBlueprintHelperGraphBodyAdapterRegistry::ValidateGeneratedRouteSync()
{
	TArray<FBlueprintHelperGraphWriteRouteSyncValidationIssue> Issues;

	const FString ArtifactPath = BlueprintHelperGraphWriteRouteSyncArtifactPath();
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ArtifactPath))
	{
		BlueprintHelperAddGraphWriteRouteSyncIssue(
			Issues,
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("artifact_load_failed"),
			FString::Printf(TEXT("Unable to load generated route sync artifact: %s"), *ArtifactPath));
		return Issues;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		BlueprintHelperAddGraphWriteRouteSyncIssue(
			Issues,
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("artifact_parse_failed"),
			FString::Printf(TEXT("Unable to parse generated route sync artifact: %s"), *ArtifactPath));
		return Issues;
	}

	const TArray<TSharedPtr<FJsonValue>>* Routes = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("routes"), Routes))
	{
		BlueprintHelperAddGraphWriteRouteSyncIssue(
			Issues,
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("routes_missing"),
			TEXT("Generated route sync artifact does not contain a routes array."));
		return Issues;
	}

	for (int32 RouteIndex = 0; RouteIndex < Routes->Num(); ++RouteIndex)
	{
		const TSharedPtr<FJsonValue>& RouteValue = (*Routes)[RouteIndex];
		if (!RouteValue.IsValid() || RouteValue->Type != EJson::Object)
		{
			BlueprintHelperAddGraphWriteRouteSyncIssue(
				Issues,
				FString::Printf(TEXT("<route[%d]>"), RouteIndex),
				TEXT(""),
				TEXT(""),
				TEXT("route_object_invalid"),
				TEXT("Generated route entry is not an object."));
			continue;
		}

		const TSharedPtr<FJsonObject> RouteObject = RouteValue->AsObject();
		FString RouteId;
		FString RuntimeAdapterId;
		FString Status;
		FString AdapterSync;
		RouteObject->TryGetStringField(TEXT("route_id"), RouteId);
		RouteObject->TryGetStringField(TEXT("runtime_adapter_id"), RuntimeAdapterId);
		RouteObject->TryGetStringField(TEXT("status"), Status);
		RouteObject->TryGetStringField(TEXT("adapter_sync"), AdapterSync);

		if (Status.Equals(TEXT("active"), ESearchCase::IgnoreCase) &&
			!AdapterSync.Equals(TEXT("active_requires_registered_non_reserved_adapter"), ESearchCase::IgnoreCase))
		{
			BlueprintHelperAddGraphWriteRouteSyncIssue(
				Issues,
				RouteId,
				RuntimeAdapterId,
				Status,
				TEXT("active_adapter_sync_semantics_invalid"),
				TEXT("Active generated route must declare active_requires_registered_non_reserved_adapter."));
			continue;
		}

		if (!Status.Equals(TEXT("active"), ESearchCase::IgnoreCase) &&
			!AdapterSync.Equals(TEXT("reserved_hidden_from_agent"), ESearchCase::IgnoreCase))
		{
			BlueprintHelperAddGraphWriteRouteSyncIssue(
				Issues,
				RouteId,
				RuntimeAdapterId,
				Status,
				TEXT("reserved_adapter_sync_semantics_invalid"),
				TEXT("Reserved generated route must declare reserved_hidden_from_agent."));
			continue;
		}

		if (!Status.Equals(TEXT("active"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (RuntimeAdapterId.IsEmpty())
		{
			BlueprintHelperAddGraphWriteRouteSyncIssue(
				Issues,
				RouteId,
				RuntimeAdapterId,
				Status,
				TEXT("active_runtime_adapter_missing"),
				TEXT("Active generated route is missing runtime_adapter_id."));
			continue;
		}

		FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
		if (!TryFindByRuntimeAdapterId(RuntimeAdapterId, Descriptor))
		{
			BlueprintHelperAddGraphWriteRouteSyncIssue(
				Issues,
				RouteId,
				RuntimeAdapterId,
				Status,
				TEXT("active_runtime_adapter_unregistered"),
				TEXT("Active generated route runtime_adapter_id does not resolve to a UE adapter descriptor."));
			continue;
		}

		if (Descriptor.bReservedOnly)
		{
			BlueprintHelperAddGraphWriteRouteSyncIssue(
				Issues,
				RouteId,
				RuntimeAdapterId,
				Status,
				TEXT("active_runtime_adapter_reserved"),
				TEXT("Active generated route resolves to a reserved-only UE adapter descriptor."));
		}
	}

	return Issues;
}

bool FBlueprintHelperGraphBodyAdapterRegistry::TryFindByTaskSpecStrategy(
	const FString& TaskSpecStrategy,
	FBlueprintHelperGraphBodyAdapterDescriptor& OutDescriptor)
{
	if (TaskSpecStrategy.IsEmpty())
	{
		return false;
	}
	for (const FBlueprintHelperGraphBodyAdapterDescriptor& Descriptor : GetKnownDescriptors())
	{
		if (!Descriptor.TaskSpecStrategy.IsEmpty() &&
			Descriptor.TaskSpecStrategy.Equals(TaskSpecStrategy, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}
	return false;
}
