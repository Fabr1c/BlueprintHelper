#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

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

	if (!EffectiveRequest.TargetGraph)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EffectiveRequest.ClusterKind;
		Result.ErrorCode = TEXT("action_resolution_invalid_request");
		Result.Message = TEXT("action_resolution_invalid_request:missing_target_graph");
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

	return FBlueprintHelperSpawnerClusterResolver::Resolve(EffectiveRequest);
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
	case EBlueprintHelperActionSemanticKind::Bind: return TEXT("bind");
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
