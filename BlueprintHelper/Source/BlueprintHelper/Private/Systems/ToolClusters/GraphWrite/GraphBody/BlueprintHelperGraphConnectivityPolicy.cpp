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
	Policy.ViolationCodes = BoundaryModel.ConnectivityExceptionCodes;
	return Policy;
}
