#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionSettingsResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

namespace
{
static FString NormalizeOperationToken(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

static bool IsGenericTransformOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperationToken(Operation);
	return Normalized == TEXT("dynamic_cast")
		|| Normalized == TEXT("class_cast")
		|| Normalized == TEXT("type_promotion");
}

static bool IsGenericCreateOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperationToken(Operation);
	return Normalized == TEXT("spawn_actor")
		|| Normalized == TEXT("create_widget")
		|| Normalized == TEXT("construct_object")
		|| Normalized == TEXT("make_array")
		|| Normalized == TEXT("make_map")
		|| Normalized == TEXT("make_set")
		|| Normalized == TEXT("asset_action")
		|| Normalized == TEXT("asset_backed_graph_node");
}

static bool IsGenericScheduleOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperationToken(Operation);
	return Normalized == TEXT("timer_delegate_node")
		|| Normalized == TEXT("latent_or_async_node");
}

static bool HasAmbiguousGenericFunctionOwner(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	if (Semantic.FunctionOperation.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Convert)
	{
		return IsGenericTransformOperation(Semantic.TransformOperation);
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Schedule)
	{
		return IsGenericScheduleOperation(Semantic.ScheduleOperation);
	}

	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Create)
	{
		return IsGenericCreateOperation(Semantic.CreateOperation);
	}

	return false;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperActionResolutionCore::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionRequest EffectiveRequest = Request;
	const FBlueprintHelperActionResolutionSettings Settings =
		FBlueprintHelperActionResolutionSettingsResolver::Load();
	if (EffectiveRequest.MaxCandidates <= 0)
	{
		EffectiveRequest.MaxCandidates = Settings.CandidateCount;
	}
	if (EffectiveRequest.Semantic.SearchMode.IsEmpty())
	{
		EffectiveRequest.Semantic.SearchMode = Settings.DefaultSearchMode;
	}
	if (EffectiveRequest.Semantic.AmbiguityPolicy.IsEmpty())
	{
		EffectiveRequest.Semantic.AmbiguityPolicy = Settings.DefaultAmbiguityPolicy;
	}

	const FBlueprintHelperActionClusterContextView Context(EffectiveRequest);
	FString ContextErrorCode;
	FString ContextMessage;
	if (!Context.HasGraphContext())
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EffectiveRequest.ClusterKind;
		Result.ErrorCode = TEXT("action_context_graph_missing");
		Result.Message = TEXT("Projected action context is missing Blueprint or target graph.");
		return Result;
	}

	if (!Context.HasSemanticKind())
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EffectiveRequest.ClusterKind;
		Result.ErrorCode = TEXT("semantic_kind_missing");
		Result.Message = TEXT("Projected action context is missing semantic kind.");
		return Result;
	}

	if (!Context.HasStableIdentity())
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EffectiveRequest.ClusterKind;
		Result.ErrorCode = TEXT("action_context_identity_missing");
		Result.Message = TEXT("Projected action context is missing statement/context identity.");
		return Result;
	}

	if (HasAmbiguousGenericFunctionOwner(EffectiveRequest.Semantic))
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EffectiveRequest.ClusterKind;
		Result.ErrorCode = TEXT("ambiguous_generic_function_owner");
		Result.Message = TEXT("GenericOps request contains both function_operation and generic node/spawner operation evidence.");
		return Result;
	}

	if (EffectiveRequest.ClusterKind == EBlueprintHelperSpawnerClusterKind::Unknown)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		Result.ErrorCode = TEXT("action_resolution_invalid_cluster");
		Result.Message = FString::Printf(
			TEXT("action_resolution_invalid_cluster: semantic=%s"),
			*SemanticKindToString(EffectiveRequest.Semantic.Kind));
		return Result;
	}

	if (!Context.IsCompleteForCluster(EffectiveRequest.ClusterKind, ContextErrorCode, ContextMessage))
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EffectiveRequest.ClusterKind;
		Result.ErrorCode = ContextErrorCode;
		Result.Message = ContextMessage;
		return Result;
	}

	return FBlueprintHelperSpawnerClusterResolver::Resolve(EffectiveRequest, Context);
}

FString FBlueprintHelperActionResolutionCore::SemanticKindToString(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Call: return TEXT("call");
	case EBlueprintHelperActionSemanticKind::Field: return TEXT("field");
	case EBlueprintHelperActionSemanticKind::Op: return TEXT("op");
	case EBlueprintHelperActionSemanticKind::Construct: return TEXT("construct");
	case EBlueprintHelperActionSemanticKind::Deconstruct: return TEXT("deconstruct");
	case EBlueprintHelperActionSemanticKind::Select: return TEXT("select");
	case EBlueprintHelperActionSemanticKind::Event: return TEXT("event");
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent: return TEXT("component_bound_event");
	case EBlueprintHelperActionSemanticKind::Delegate: return TEXT("delegate");
	case EBlueprintHelperActionSemanticKind::Control: return TEXT("control");
	case EBlueprintHelperActionSemanticKind::Create: return TEXT("create");
	case EBlueprintHelperActionSemanticKind::Convert: return TEXT("convert");
	case EBlueprintHelperActionSemanticKind::Schedule: return TEXT("schedule");
	case EBlueprintHelperActionSemanticKind::ContainerAction: return TEXT("container_action");
	default: return TEXT("unknown");
	}
}

FString FBlueprintHelperActionResolutionCore::SemanticFamilyToString(EBlueprintHelperActionSemanticFamily Family)
{
	switch (Family)
	{
	case EBlueprintHelperActionSemanticFamily::Callable: return TEXT("callable");
	case EBlueprintHelperActionSemanticFamily::Field: return TEXT("field");
	case EBlueprintHelperActionSemanticFamily::Operator: return TEXT("operator");
	case EBlueprintHelperActionSemanticFamily::Struct: return TEXT("struct");
	case EBlueprintHelperActionSemanticFamily::TypeStructure: return TEXT("type_structure");
	case EBlueprintHelperActionSemanticFamily::Event: return TEXT("event");
	case EBlueprintHelperActionSemanticFamily::Delegate: return TEXT("delegate");
	case EBlueprintHelperActionSemanticFamily::Control: return TEXT("control");
	case EBlueprintHelperActionSemanticFamily::Create: return TEXT("create");
	case EBlueprintHelperActionSemanticFamily::Convert: return TEXT("convert");
	case EBlueprintHelperActionSemanticFamily::Schedule: return TEXT("schedule");
	default: return TEXT("unknown");
	}
}

FString FBlueprintHelperActionResolutionCore::TypeOperationToString(EBlueprintHelperTypeOperation Operation)
{
	switch (Operation)
	{
	case EBlueprintHelperTypeOperation::Construct: return TEXT("construct");
	case EBlueprintHelperTypeOperation::Deconstruct: return TEXT("deconstruct");
	default: return TEXT("none");
	}
}

FString FBlueprintHelperActionResolutionCore::ClusterKindToString(EBlueprintHelperSpawnerClusterKind ClusterKind)
{
	switch (ClusterKind)
	{
	case EBlueprintHelperSpawnerClusterKind::FunctionAction: return TEXT("FunctionActionCluster");
	case EBlueprintHelperSpawnerClusterKind::FieldVariableAction: return TEXT("FieldVariableActionCluster");
	case EBlueprintHelperSpawnerClusterKind::EventDelegateAction: return TEXT("EventDelegateActionCluster");
	case EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction: return TEXT("GenericAssetStructControlActionCluster");
	default: return TEXT("UnknownCluster");
	}
}
