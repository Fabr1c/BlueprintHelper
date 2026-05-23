#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionSettingsResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

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
	case EBlueprintHelperActionSemanticKind::Get: return TEXT("get");
	case EBlueprintHelperActionSemanticKind::Set: return TEXT("set");
	case EBlueprintHelperActionSemanticKind::GetProperty: return TEXT("get_property");
	case EBlueprintHelperActionSemanticKind::SetProperty: return TEXT("set_property");
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
	default: return TEXT("unknown");
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
