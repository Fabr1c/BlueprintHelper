// BlueprintHelper Bridge Layer - CleanupOwnership static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperCleanupOwnershipBridgeRoutes.h"

#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperRollbackCleanupTransactionService.h"

class FBlueprintHelperCleanupOwnershipBridgeRoutesLocalUtils
{
public:
	static FBlueprintHelperBridgeResponse MakeCleanupResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const FBlueprintHelperToolResultBase& Result,
		const TCHAR* FallbackFailureMessage)
	{
		FBlueprintHelperBridgeResponse Response = Result.bOk
			? FBlueprintHelperBridgeResponse::Success(Request.RequestId)
			: FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.Error.IsSet() ? Result.Error->Message : FString(FallbackFailureMessage));
		Response.Result = Result.ToJson();
		return Response;
	}

};

FBlueprintHelperCleanupOwnershipBridgeRoutes::FBlueprintHelperCleanupOwnershipBridgeRoutes(
	const FBlueprintHelperCleanupBlueprintHelperBlockService& InCleanupBlockService,
	const FBlueprintHelperRollbackCleanupTransactionService& InRollbackCleanupService,
	const FBlueprintHelperConvertBlockToUserOwnedService& InConvertBlockService)
	: CleanupBlockService(InCleanupBlockService)
	, RollbackCleanupService(InRollbackCleanupService)
	, ConvertBlockService(InConvertBlockService)
{
}

bool FBlueprintHelperCleanupOwnershipBridgeRoutes::IsCleanupOwnershipCommand(const FString& Command)
{
	return Command == TEXT("cleanup_blueprint_helper_block") ||
		Command == TEXT("rollback_cleanup_transaction") ||
		Command == TEXT("convert_blueprint_helper_block_to_user_owned");
}

FBlueprintHelperBridgeResponse FBlueprintHelperCleanupOwnershipBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	if (!Request.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload is required."));
	}

	if (Request.Command == TEXT("cleanup_blueprint_helper_block"))
	{
		return FBlueprintHelperCleanupOwnershipBridgeRoutesLocalUtils::MakeCleanupResponse(
			Request,
			CleanupBlockService.Execute(Request.Payload),
			TEXT("cleanup_blueprint_helper_block failed."));
	}

	if (Request.Command == TEXT("rollback_cleanup_transaction"))
	{
		return FBlueprintHelperCleanupOwnershipBridgeRoutesLocalUtils::MakeCleanupResponse(
			Request,
			RollbackCleanupService.Execute(Request.Payload),
			TEXT("rollback_cleanup_transaction failed."));
	}

	if (Request.Command == TEXT("convert_blueprint_helper_block_to_user_owned"))
	{
		return FBlueprintHelperCleanupOwnershipBridgeRoutesLocalUtils::MakeCleanupResponse(
			Request,
			ConvertBlockService.Execute(Request.Payload),
			TEXT("convert_blueprint_helper_block_to_user_owned failed."));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("Unknown CleanupOwnership command: %s"), *Request.Command));
}
