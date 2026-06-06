#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterResolver.h"

#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperExternalBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2BlockBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2CustomEventBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2EventBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2FunctionBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2MacroBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"

EBlueprintHelperGraphBodyKind FBlueprintHelperGraphBodyAdapterResolver::BodyKindForReplaceScope(
	EBlueprintHelperReplaceScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperReplaceScope::CustomEventBody:
		return EBlueprintHelperGraphBodyKind::K2CustomEventBody;
	case EBlueprintHelperReplaceScope::EventBody:
		return EBlueprintHelperGraphBodyKind::K2EventBody;
	case EBlueprintHelperReplaceScope::FunctionBody:
		return EBlueprintHelperGraphBodyKind::K2FunctionBody;
	case EBlueprintHelperReplaceScope::MacroBody:
		return EBlueprintHelperGraphBodyKind::K2MacroBody;
	case EBlueprintHelperReplaceScope::BlockImplementation:
		return EBlueprintHelperGraphBodyKind::K2BlockImplementation;
	default:
		return EBlueprintHelperGraphBodyKind::Unknown;
	}
}

FString FBlueprintHelperGraphBodyAdapterResolver::RuntimeAdapterIdForReplaceScope(
	EBlueprintHelperReplaceScope Scope)
{
	return FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(BodyKindForReplaceScope(Scope));
}

bool FBlueprintHelperGraphBodyAdapterResolver::TryCreateByRuntimeAdapterId(
	const FString& RuntimeAdapterId,
	TUniquePtr<IBlueprintHelperGraphBodyAdapter>& OutAdapter,
	FString& OutError)
{
	OutAdapter.Reset();
	OutError.Reset();

	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
	if (!FBlueprintHelperGraphBodyAdapterRegistry::TryFindByRuntimeAdapterId(RuntimeAdapterId, Descriptor))
	{
		OutError = FString::Printf(TEXT("GraphBody runtime adapter is not registered: %s."), *RuntimeAdapterId);
		return false;
	}
	if (Descriptor.bReservedOnly)
	{
		OutError = FString::Printf(TEXT("GraphBody runtime adapter is reserved and not executable: %s."), *RuntimeAdapterId);
		return false;
	}

	switch (Descriptor.BodyKind)
	{
	case EBlueprintHelperGraphBodyKind::K2CustomEventBody:
		OutAdapter = MakeUnique<FBlueprintHelperK2CustomEventBodyAdapter>();
		break;
	case EBlueprintHelperGraphBodyKind::K2EventBody:
		OutAdapter = MakeUnique<FBlueprintHelperK2EventBodyAdapter>();
		break;
	case EBlueprintHelperGraphBodyKind::K2FunctionBody:
		OutAdapter = MakeUnique<FBlueprintHelperK2FunctionBodyAdapter>();
		break;
	case EBlueprintHelperGraphBodyKind::K2MacroBody:
		OutAdapter = MakeUnique<FBlueprintHelperK2MacroBodyAdapter>();
		break;
	case EBlueprintHelperGraphBodyKind::K2BlockImplementation:
		OutAdapter = MakeUnique<FBlueprintHelperK2BlockBodyAdapter>(Descriptor);
		break;
	case EBlueprintHelperGraphBodyKind::K2ExternalBody:
		OutAdapter = MakeUnique<FBlueprintHelperExternalBodyAdapter>(Descriptor);
		break;
	default:
		OutError = FString::Printf(TEXT("GraphBody runtime adapter kind is not executable: %s."), *RuntimeAdapterId);
		return false;
	}
	return OutAdapter.IsValid();
}

bool FBlueprintHelperGraphBodyAdapterResolver::TryCreateForReplaceScope(
	EBlueprintHelperReplaceScope Scope,
	TUniquePtr<IBlueprintHelperGraphBodyAdapter>& OutAdapter,
	FString& OutError)
{
	const FString RuntimeAdapterId = RuntimeAdapterIdForReplaceScope(Scope);
	if (RuntimeAdapterId == TEXT("unknown"))
	{
		OutError = FString::Printf(TEXT("No GraphBody runtime adapter maps replace scope: %s."), ReplaceScopeToString(Scope));
		OutAdapter.Reset();
		return false;
	}
	return TryCreateByRuntimeAdapterId(RuntimeAdapterId, OutAdapter, OutError);
}

bool FBlueprintHelperGraphBodyAdapterResolver::TryCreateForReadTarget(
	const FBlueprintHelperTargetRef& Target,
	TUniquePtr<IBlueprintHelperGraphBodyAdapter>& OutAdapter,
	FString& OutError)
{
	FString RuntimeAdapterId;
	switch (Target.TargetType)
	{
	case EBlueprintHelperTargetType::Function:
		RuntimeAdapterId = TEXT("k2.function_body");
		break;
	case EBlueprintHelperTargetType::CustomEvent:
		RuntimeAdapterId = TEXT("k2.custom_event_body");
		break;
	case EBlueprintHelperTargetType::Event:
		RuntimeAdapterId = TEXT("k2.event_body");
		break;
	case EBlueprintHelperTargetType::Graph:
		RuntimeAdapterId = TEXT("k2.macro_body");
		break;
	default:
		OutAdapter.Reset();
		OutError = FString::Printf(TEXT("No GraphBody read adapter maps target type: %s."), TargetTypeToString(Target.TargetType));
		return false;
	}
	return TryCreateByRuntimeAdapterId(RuntimeAdapterId, OutAdapter, OutError);
}

FBlueprintHelperGraphBodyRequest FBlueprintHelperGraphBodyAdapterResolver::MakeReadRequestForTarget(
	const FBlueprintHelperTargetRef& Target,
	UBlueprint* Blueprint)
{
	FBlueprintHelperGraphBodyRequest Request;
	Request.OperationKind = TEXT("read_context");
	Request.TaskSpecStrategy = TEXT("read_context");
	Request.AssetPath = Target.AssetPath;
	Request.GraphName = Target.Graph;
	Request.Blueprint = Blueprint;

	switch (Target.TargetType)
	{
	case EBlueprintHelperTargetType::Function:
		Request.ReplaceScope = TEXT("function");
		if (Request.GraphName.IsEmpty())
		{
			Request.GraphName = Target.Function;
		}
		Request.EntryName = Target.Function;
		break;
	case EBlueprintHelperTargetType::CustomEvent:
		Request.ReplaceScope = TEXT("custom_event");
		Request.EntryName = Target.Event;
		break;
	case EBlueprintHelperTargetType::Event:
		Request.ReplaceScope = TEXT("event");
		Request.EntryName = Target.Event;
		break;
	case EBlueprintHelperTargetType::Graph:
		Request.ReplaceScope = TEXT("graph");
		break;
	default:
		break;
	}

	if (Request.GraphName.IsEmpty())
	{
		Request.GraphName = TEXT("EventGraph");
	}
	return Request;
}
