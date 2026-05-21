#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperActionResolutionCore::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	if (!Request.TargetGraph)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = Request.ClusterKind;
		Result.ErrorCode = TEXT("action_resolution_invalid_request");
		Result.Message = TEXT("action_resolution_invalid_request:missing_target_graph");
		return Result;
	}

	if (Request.ClusterKind == EBlueprintHelperSpawnerClusterKind::Unknown)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		Result.ErrorCode = TEXT("action_resolution_invalid_cluster");
		Result.Message = FString::Printf(
			TEXT("action_resolution_invalid_cluster: semantic=%s"),
			*SemanticKindToString(Request.Semantic.Kind));
		return Result;
	}

	return FBlueprintHelperSpawnerClusterResolver::Resolve(Request);
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