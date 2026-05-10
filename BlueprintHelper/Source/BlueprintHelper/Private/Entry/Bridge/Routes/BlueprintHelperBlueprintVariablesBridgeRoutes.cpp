// BlueprintHelper Bridge Layer - BlueprintVariables static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperBlueprintVariablesBridgeRoutes.h"

#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"

class FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils
{
public:
	static FBlueprintHelperBridgeResponse MakeBlueprintVariablesResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const FBlueprintHelperToolResultBase& Result)
	{
		FBlueprintHelperBridgeResponse Response = Result.bOk
			? FBlueprintHelperBridgeResponse::Success(Request.RequestId)
			: FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.Error.IsSet() ? Result.Error->Message : TEXT("failed"));
		Response.Result = Result.ToJson();
		return Response;
	}

};

FBlueprintHelperBlueprintVariablesBridgeRoutes::FBlueprintHelperBlueprintVariablesBridgeRoutes(
	const FBlueprintHelperBlueprintVariableService& InVariableService)
	: VariableService(InVariableService)
{
}

bool FBlueprintHelperBlueprintVariablesBridgeRoutes::IsBlueprintVariablesCommand(const FString& Command)
{
	return Command == TEXT("read_blueprint_member_variables") ||
		Command == TEXT("add_blueprint_member_variable") ||
		Command == TEXT("add_blueprint_member_variables") ||
		Command == TEXT("set_blueprint_member_variable_properties") ||
		Command == TEXT("remove_blueprint_member_variable") ||
		Command == TEXT("remove_blueprint_member_variables") ||
		Command == TEXT("read_blueprint_member_defaults") ||
		Command == TEXT("set_blueprint_member_default") ||
		Command == TEXT("set_blueprint_member_defaults") ||
		Command == TEXT("read_blueprint_local_variables") ||
		Command == TEXT("add_blueprint_local_variable") ||
		Command == TEXT("add_blueprint_local_variables") ||
		Command == TEXT("set_blueprint_local_variable_properties") ||
		Command == TEXT("remove_blueprint_local_variable") ||
		Command == TEXT("remove_blueprint_local_variables");
}

FBlueprintHelperBridgeResponse FBlueprintHelperBlueprintVariablesBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	if (Request.Command == TEXT("read_blueprint_member_variables"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.ReadMemberVariables(Request.Payload));
	}
	if (Request.Command == TEXT("add_blueprint_member_variable"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.AddMemberVariable(Request.Payload));
	}
	if (Request.Command == TEXT("add_blueprint_member_variables"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.AddMemberVariables(Request.Payload));
	}
	if (Request.Command == TEXT("set_blueprint_member_variable_properties"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.SetMemberVariableProperties(Request.Payload));
	}
	if (Request.Command == TEXT("remove_blueprint_member_variable"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.RemoveMemberVariable(Request.Payload));
	}
	if (Request.Command == TEXT("remove_blueprint_member_variables"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.RemoveMemberVariables(Request.Payload));
	}
	if (Request.Command == TEXT("read_blueprint_member_defaults"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.ReadMemberDefaults(Request.Payload));
	}
	if (Request.Command == TEXT("set_blueprint_member_default"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.SetMemberDefault(Request.Payload));
	}
	if (Request.Command == TEXT("set_blueprint_member_defaults"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.SetMemberDefaults(Request.Payload));
	}
	if (Request.Command == TEXT("read_blueprint_local_variables"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.ReadLocalVariables(Request.Payload));
	}
	if (Request.Command == TEXT("add_blueprint_local_variable"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.AddLocalVariable(Request.Payload));
	}
	if (Request.Command == TEXT("add_blueprint_local_variables"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.AddLocalVariables(Request.Payload));
	}
	if (Request.Command == TEXT("set_blueprint_local_variable_properties"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.SetLocalVariableProperties(Request.Payload));
	}
	if (Request.Command == TEXT("remove_blueprint_local_variable"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.RemoveLocalVariable(Request.Payload));
	}
	if (Request.Command == TEXT("remove_blueprint_local_variables"))
	{
		return FBlueprintHelperBlueprintVariablesBridgeRoutesLocalUtils::MakeBlueprintVariablesResponse(Request, VariableService.RemoveLocalVariables(Request.Payload));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 BlueprintVariables 命令: %s"), *Request.Command));
}
