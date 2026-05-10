// BlueprintHelper Bridge Layer - Material static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperMaterialBridgeRoutes.h"

bool FBlueprintHelperMaterialBridgeRoutes::IsMaterialCommand(const FString& Command)
{
	static_cast<void>(Command);
	return false;
}

FBlueprintHelperBridgeResponse FBlueprintHelperMaterialBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("Unknown Material command: %s"), *Request.Command));
}
