// BlueprintHelper Review debug projection registry implementation.

#include "UI/Review/BlueprintHelperReviewDebugProjectionRegistry.h"
#include "UI/Review/BlueprintHelperReviewGenericDebugProjectionAdapter.h"

#include "Dom/JsonObject.h"

TSharedRef<FBlueprintHelperReviewDebugProjectionRegistry> FBlueprintHelperReviewDebugProjectionRegistry::CreateDefault()
{
	TSharedRef<FBlueprintHelperReviewDebugProjectionRegistry> Registry =
		MakeShared<FBlueprintHelperReviewDebugProjectionRegistry>();
	Registry->RegisterBuiltInAdapters();
	return Registry;
}

bool FBlueprintHelperReviewDebugProjectionRegistry::RegisterProjectionAdapter(
	const TSharedRef<IBlueprintHelperReviewDebugProjectionAdapter>& Adapter,
	FString& OutError)
{
	const FString Key = NormalizeEventType(Adapter->GetEventType());
	if (Key.IsEmpty())
	{
		OutError = TEXT("missing_debug_projection_event_type");
		return false;
	}
	if (AdaptersByEventType.Contains(Key))
	{
		OutError = FString::Printf(TEXT("duplicate_debug_projection_adapter:%s"), *Key);
		return false;
	}

	AdaptersByEventType.Add(Key, Adapter);
	return true;
}

FBlueprintHelperReviewDebugProjectionLookup FBlueprintHelperReviewDebugProjectionRegistry::FindProjectionAdapter(
	const FString& EventType) const
{
	FBlueprintHelperReviewDebugProjectionLookup Lookup;
	Lookup.EventType = NormalizeEventType(EventType);
	if (const TSharedPtr<IBlueprintHelperReviewDebugProjectionAdapter>* Adapter =
		AdaptersByEventType.Find(Lookup.EventType))
	{
		Lookup.bAvailable = Adapter->IsValid();
		Lookup.Adapter = *Adapter;
		Lookup.Message = Lookup.bAvailable ? TEXT("available") : TEXT("debug_projection_adapter_invalid");
		return Lookup;
	}

	Lookup.Message = FString::Printf(TEXT("debug_projection_adapter_unavailable:%s"), *Lookup.EventType);
	return Lookup;
}

TArray<FBlueprintHelperReviewDebugEventModel> FBlueprintHelperReviewDebugProjectionRegistry::ProjectEvent(
	const TSharedPtr<FJsonObject>& EventJson) const
{
	TArray<FBlueprintHelperReviewDebugEventModel> Events;
	if (!EventJson.IsValid())
	{
		return Events;
	}

	FString EventType;
	EventJson->TryGetStringField(TEXT("event_type"), EventType);
	const FBlueprintHelperReviewDebugProjectionLookup Lookup = FindProjectionAdapter(EventType);
	if (!Lookup.bAvailable || !Lookup.Adapter.IsValid())
	{
		return Events;
	}
	return Lookup.Adapter->Project(EventJson).Events;
}

void FBlueprintHelperReviewDebugProjectionRegistry::RegisterBuiltInAdapters()
{
	FString Error;
	const TCHAR* EventTypes[] =
	{
		TEXT("review.action.intent"),
		TEXT("review.action.result"),
		TEXT("review.restore.started"),
		TEXT("review.restore.finished"),
		TEXT("review.surface.projected"),
		TEXT("review.surface.hidden"),
		TEXT("review.focus.started"),
		TEXT("review.focus.result"),
		TEXT("review.store.changed"),
		TEXT("review.model.rebuilt"),
		TEXT("review.log"),
		TEXT("debug_log"),
		TEXT("focus_traversal"),
		TEXT("review_action_hash_guard"),
		TEXT("review_reject_timing"),
		TEXT("material_graph.preview"),
		TEXT("material_graph.mutation_plan"),
		TEXT("material_graph.connectivity_validation"),
		TEXT("material_graph.evidence.created"),
		TEXT("material_graph.restore.result"),
		TEXT("material_graph.surface.projected"),
		TEXT("material_instance.preview"),
		TEXT("material_instance.mutation_plan"),
		TEXT("material_instance.mutation_result"),
		TEXT("material_instance.readback"),
		TEXT("material_instance.evidence.created"),
		TEXT("material_instance.restore.result"),
		TEXT("material_instance.surface.projected")
	};
	for (const TCHAR* EventType : EventTypes)
	{
		RegisterProjectionAdapter(
			MakeShared<FBlueprintHelperReviewGenericDebugProjectionAdapter>(EventType),
			Error);
	}
}

FString FBlueprintHelperReviewDebugProjectionRegistry::NormalizeEventType(const FString& EventType)
{
	FString Normalized = EventType;
	Normalized.TrimStartAndEndInline();
	Normalized.ToLowerInline();
	return Normalized;
}
