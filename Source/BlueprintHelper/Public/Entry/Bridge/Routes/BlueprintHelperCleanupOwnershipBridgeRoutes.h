// BlueprintHelper Bridge Layer - CleanupOwnership static cluster routes

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperCleanupBlueprintHelperBlockService;
class FBlueprintHelperRollbackCleanupTransactionService;
class FBlueprintHelperConvertBlockToUserOwnedService;

class BLUEPRINTHELPER_API FBlueprintHelperCleanupOwnershipBridgeRoutes
{
public:
	FBlueprintHelperCleanupOwnershipBridgeRoutes(
		const FBlueprintHelperCleanupBlueprintHelperBlockService& InCleanupBlockService,
		const FBlueprintHelperRollbackCleanupTransactionService& InRollbackCleanupService,
		const FBlueprintHelperConvertBlockToUserOwnedService& InConvertBlockService);

	static bool IsCleanupOwnershipCommand(const FString& Command);

	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	const FBlueprintHelperCleanupBlueprintHelperBlockService& CleanupBlockService;
	const FBlueprintHelperRollbackCleanupTransactionService& RollbackCleanupService;
	const FBlueprintHelperConvertBlockToUserOwnedService& ConvertBlockService;
};
