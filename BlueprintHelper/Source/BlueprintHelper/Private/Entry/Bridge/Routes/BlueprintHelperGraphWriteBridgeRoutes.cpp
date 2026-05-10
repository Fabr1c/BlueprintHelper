// BlueprintHelper Bridge Layer - GraphWrite static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.h"

#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"

class FBlueprintHelperGraphWriteBridgeRoutesLocalUtils
{
public:
	static FBlueprintHelperBridgeResponse MakeGraphWritePayloadMissingResponse(const FBlueprintHelperBridgeRequest& Request)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺失。"));
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
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
	const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
	const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
	const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService)
	: AppendGraphService(InAppendGraphService)
	, ReplaceGraphService(InReplaceGraphService)
	, PatchGraphService(InPatchGraphService)
	, MergeGraphService(InMergeGraphService)
{
}

bool FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(const FString& Command)
{
	return Command == TEXT("append_blueprint_graph") ||
		Command == TEXT("replace_blueprint_graph") ||
		Command == TEXT("patch_blueprint_graph") ||
		Command == TEXT("merge_blueprint_graph");
}

FBlueprintHelperBridgeResponse FBlueprintHelperGraphWriteBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	if (!Request.Payload.IsValid())
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWritePayloadMissingResponse(Request);
	}

	if (Request.Command == TEXT("append_blueprint_graph"))
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWriteExecutionResponse(
			Request,
			AppendGraphService.Execute(Request.Payload),
			TEXT("append_blueprint_graph 执行失败。"));
	}
	if (Request.Command == TEXT("replace_blueprint_graph"))
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWriteExecutionResponse(
			Request,
			ReplaceGraphService.Execute(Request.Payload),
			TEXT("replace_blueprint_graph 执行失败。"));
	}
	if (Request.Command == TEXT("patch_blueprint_graph"))
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWriteExecutionResponse(
			Request,
			PatchGraphService.Execute(Request.Payload),
			TEXT("patch_blueprint_graph 执行失败。"));
	}
	if (Request.Command == TEXT("merge_blueprint_graph"))
	{
		return FBlueprintHelperGraphWriteBridgeRoutesLocalUtils::MakeGraphWriteExecutionResponse(
			Request,
			MergeGraphService.Execute(Request.Payload),
			TEXT("merge_blueprint_graph 执行失败。"));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 GraphWrite 命令: %s"), *Request.Command));
}
