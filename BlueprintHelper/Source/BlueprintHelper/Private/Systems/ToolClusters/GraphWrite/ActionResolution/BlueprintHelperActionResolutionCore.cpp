#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSpawnerClusterResolver.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperActionResolutionCore::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	if (!Request.TargetGraph)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		Result.ErrorCode = TEXT("action_resolution_invalid_request");
		Result.Message = TEXT("action_resolution_invalid_request:missing_target_graph");
		return Result;
	}

	return FBlueprintHelperSpawnerClusterResolver::Resolve(Request);
}

FString FBlueprintHelperActionResolutionCore::IntentToString(EBlueprintHelperActionIntent Intent)
{
	switch (Intent)
	{
	case EBlueprintHelperActionIntent::Call: return TEXT("call");
	case EBlueprintHelperActionIntent::Get: return TEXT("get");
	case EBlueprintHelperActionIntent::Set: return TEXT("set");
	case EBlueprintHelperActionIntent::GetProperty: return TEXT("get_property");
	case EBlueprintHelperActionIntent::SetProperty: return TEXT("set_property");
	case EBlueprintHelperActionIntent::Op: return TEXT("op");
	case EBlueprintHelperActionIntent::Construct: return TEXT("construct");
	case EBlueprintHelperActionIntent::Deconstruct: return TEXT("deconstruct");
	case EBlueprintHelperActionIntent::Select: return TEXT("select");
	case EBlueprintHelperActionIntent::Event: return TEXT("event");
	case EBlueprintHelperActionIntent::ComponentBoundEvent: return TEXT("component_bound_event");
	case EBlueprintHelperActionIntent::Bind: return TEXT("bind");
	case EBlueprintHelperActionIntent::Control: return TEXT("control");
	case EBlueprintHelperActionIntent::Create: return TEXT("create");
	case EBlueprintHelperActionIntent::Convert: return TEXT("convert");
	case EBlueprintHelperActionIntent::Schedule: return TEXT("schedule");
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