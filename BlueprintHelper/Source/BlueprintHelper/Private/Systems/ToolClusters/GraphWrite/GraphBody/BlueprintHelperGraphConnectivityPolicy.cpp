#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"

FBlueprintHelperGraphConnectivityPolicy FBlueprintHelperGraphConnectivityPolicyUtils::FromBoundaryModel(
	const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel)
{
	FBlueprintHelperGraphConnectivityPolicy Policy;
	Policy.BodyKind = BoundaryModel.BodyKind;
	Policy.PolicyId = FBlueprintHelperGraphBodyBoundaryModelUtils::MakeBodyIdentity(BoundaryModel);
	Policy.bAllowExitBoundaryReachability = BoundaryModel.ExitNodeRefs.Num() > 0;
	Policy.bAllowExternalAnchorBoundary =
		BoundaryModel.BodyKind == EBlueprintHelperGraphBodyKind::K2ExternalBody ||
		BoundaryModel.ExternalAnchorRefs.Num() > 0;
	Policy.bAllowOwnedBlockDisconnectedPreview =
		BoundaryModel.BodyKind == EBlueprintHelperGraphBodyKind::K2BlockImplementation ||
		!BoundaryModel.OwnedBlockId.IsEmpty();
	if (BoundaryModel.BodyKind == EBlueprintHelperGraphBodyKind::K2FunctionBody)
	{
		Policy.bRequireFunctionEntryReachability = BoundaryModel.EntryBoundaryRefs.Num() > 0 || BoundaryModel.EntryNodeRefs.Num() > 0;
		Policy.bRequireFunctionResultReachability = BoundaryModel.ExitBoundaryRefs.Num() > 0 || BoundaryModel.ExitNodeRefs.Num() > 0;
		Policy.bRequireReturnDataflowConsumption = BoundaryModel.ReturnDataPinRefs.Num() > 0;
		Policy.ReturnDataPinRefs = BoundaryModel.ReturnDataPinRefs;
		Policy.FunctionParamSourceRefs = BoundaryModel.SemanticSourceRefs;
		Policy.ViolationCodes.AddUnique(TEXT("function_result_exec_unreachable"));
		Policy.ViolationCodes.AddUnique(TEXT("function_result_output_unproduced"));
		Policy.ViolationCodes.AddUnique(TEXT("function_param_source_unresolved"));
		Policy.ViolationCodes.AddUnique(TEXT("unreachable_pure_data_chain"));
	}
	for (const FString& ConnectivityExceptionCode : BoundaryModel.ConnectivityExceptionCodes)
	{
		Policy.ViolationCodes.AddUnique(ConnectivityExceptionCode);
	}
	return Policy;
}
