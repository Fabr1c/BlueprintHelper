// BlueprintHelper Bridge Layer - GraphWrite static cluster routes.

#include "Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.h"

#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGraphWriteProjectedEvidenceQueryService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"

class FBlueprintHelperGraphWriteBridgeRoutesLocalUtils
{
public:
	static FBlueprintHelperBridgeResponse MakeGraphWritePayloadMissingResponse(
		const FBlueprintHelperBridgeRequest& Request)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload missing."));
	}

	static FBlueprintHelperBridgeResponse MakeGraphWriteExecutionResponse(
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

FBlueprintHelperGraphWriteBridgeRoutes::FBlueprintHelperGraphWriteBridgeRoutes(
	const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry)
	: GraphWriteRegistry(InGraphWriteRegistry)
{
}

bool FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(const FString& Command)
{
	return FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(Command) ||
		Command == TEXT("project_graphwrite_spawner_evidence");
}

bool FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteReadCommand(const FString& Command)
{
	return Command == TEXT("project_graphwrite_spawner_evidence");
}

FBlueprintHelperBridgeResponse FBlueprintHelperGraphWriteBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	if (!Request.Payload.IsValid())
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWritePayloadMissingResponse(Request);
	}

	if (FBlueprintHelperGraphWriteServiceRegistry::IsKnownOperation(Request.Command))
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWriteExecutionResponse(
			Request,
			GraphWriteRegistry.Execute(Request.Command, Request.Payload.ToSharedRef()),
			TEXT("GraphWrite operation failed."));
	}

	if (Request.Command == TEXT("project_graphwrite_spawner_evidence"))
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWriteExecutionResponse(
			Request,
			FBlueprintHelperGraphWriteProjectedEvidenceQueryService::Project(Request.Payload),
			TEXT("project_graphwrite_spawner_evidence failed."));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("Unknown GraphWrite command: %s"), *Request.Command));
}
