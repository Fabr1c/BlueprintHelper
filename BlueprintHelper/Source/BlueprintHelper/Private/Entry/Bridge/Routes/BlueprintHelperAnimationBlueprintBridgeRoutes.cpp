// BlueprintHelper Bridge Layer - AnimationBlueprint static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperAnimationBlueprintBridgeRoutes.h"

bool FBlueprintHelperAnimationBlueprintBridgeRoutes::IsAnimationBlueprintCommand(const FString& Command)
{
	static_cast<void>(Command);
	return false;
}

FBlueprintHelperBridgeResponse FBlueprintHelperAnimationBlueprintBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("Unknown AnimationBlueprint command: %s"), *Request.Command));
}
