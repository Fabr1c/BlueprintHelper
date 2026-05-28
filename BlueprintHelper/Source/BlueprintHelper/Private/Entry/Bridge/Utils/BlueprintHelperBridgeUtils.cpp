// BlueprintHelper Bridge 层 —— 通用命令路由与服务器辅助函数实现

#include "Entry/Bridge/Utils/BlueprintHelperBridgeUtils.h"
#include "Entry/Bridge/BlueprintHelperBridgeRuntimeConfigResolver.h"
#include "Shared/Bridge/BlueprintHelperBridgeTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Dom/JsonObject.h"

FBlueprintHelperBridgeRuntimeConfig UBlueprintHelperBridgeUtils::BridgeConfigWithPort(int32 InPort)
{
	FBlueprintHelperBridgeRuntimeConfig Config = FBlueprintHelperBridgeRuntimeConfigResolver::Load();
	Config.Port = InPort;
	return Config;
}

FBlueprintHelperBridgeResponse UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(
	const FBlueprintHelperBridgeRequest& Req,
	const FBlueprintHelperToolResultBase& Result)
{
	const FString ErrorMessage = Result.Error.IsSet() && !Result.Error.GetValue().Message.IsEmpty()
		? Result.Error.GetValue().Message
		: FString(ToolStatusToString(Result.Status));
	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMessage);
	Resp.Result = Result.ToJson();
	FBlueprintHelperReadContextOutputLimiter::ApplyToBridgeResult(Req.Command, Resp.Result);
	return Resp;
}

EBlueprintHelperTargetType UBlueprintHelperBridgeUtils::ParseBridgeTargetType(const FString& Type)
{
	if (Type.Equals(TEXT("asset"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Asset; }
	if (Type.Equals(TEXT("blueprint"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Blueprint; }
	if (Type.Equals(TEXT("graph"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Graph; }
	if (Type.Equals(TEXT("function"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Function; }
	if (Type.Equals(TEXT("event"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Event; }
	if (Type.Equals(TEXT("custom_event"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::CustomEvent; }
	if (Type.Equals(TEXT("block"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Block; }
	if (Type.Equals(TEXT("node"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Node; }
	if (Type.Equals(TEXT("pin"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Pin; }
	if (Type.Equals(TEXT("link"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Link; }
	if (Type.Equals(TEXT("component"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Component; }
	if (Type.Equals(TEXT("property"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Property; }
	if (Type.Equals(TEXT("data_table"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::DataTable; }
	if (Type.Equals(TEXT("data_table_row"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::DataTableRow; }
	if (Type.Equals(TEXT("widget"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Widget; }
	return EBlueprintHelperTargetType::None;
}

EBlueprintHelperTargetType UBlueprintHelperBridgeUtils::ParseLogicScopeTargetType(const FString& Scope)
{
	if (Scope.Equals(TEXT("blueprint"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Blueprint; }
	if (Scope.Equals(TEXT("target_graph"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Graph; }
	if (Scope.Equals(TEXT("target_function"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Function; }
	if (Scope.Equals(TEXT("target_event"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Event; }
	if (Scope.Equals(TEXT("target_custom_event"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::CustomEvent; }
	if (Scope.Equals(TEXT("target_block"), ESearchCase::IgnoreCase)) { return EBlueprintHelperTargetType::Block; }
	return EBlueprintHelperTargetType::None;
}

EBlueprintHelperTargetType UBlueprintHelperBridgeUtils::InferTargetTypeFromReadFields(const FBlueprintHelperTargetRef& Target)
{
	if (!Target.Function.IsEmpty()) { return EBlueprintHelperTargetType::Function; }
	if (!Target.Event.IsEmpty()) { return EBlueprintHelperTargetType::Event; }
	if (!Target.BlockId.IsEmpty()) { return EBlueprintHelperTargetType::Block; }
	if (!Target.Graph.IsEmpty()) { return EBlueprintHelperTargetType::Graph; }
	if (!Target.AssetPath.IsEmpty() || !Target.BlueprintPath.IsEmpty()) { return EBlueprintHelperTargetType::Blueprint; }
	return EBlueprintHelperTargetType::None;
}

void UBlueprintHelperBridgeUtils::ApplyTargetNameToTypedField(FBlueprintHelperTargetRef& Target, const FString& TargetName)
{
	if (TargetName.IsEmpty())
	{
		return;
	}

	switch (Target.TargetType)
	{
	case EBlueprintHelperTargetType::Function:
		if (Target.Function.IsEmpty()) { Target.Function = TargetName; }
		break;
	case EBlueprintHelperTargetType::Event:
	case EBlueprintHelperTargetType::CustomEvent:
		if (Target.Event.IsEmpty()) { Target.Event = TargetName; }
		break;
	case EBlueprintHelperTargetType::Graph:
		if (Target.Graph.IsEmpty()) { Target.Graph = TargetName; }
		break;
	case EBlueprintHelperTargetType::Block:
		if (Target.BlockId.IsEmpty()) { Target.BlockId = TargetName; }
		break;
	default:
		break;
	}
}

FBlueprintHelperTargetRef UBlueprintHelperBridgeUtils::ReadTargetRefFromPayload(const TSharedPtr<FJsonObject>& Payload)
{
	FBlueprintHelperTargetRef Target;
	if (!Payload.IsValid())
	{
		return Target;
	}

	FString Type;
	Payload->TryGetStringField(TEXT("asset_path"), Target.AssetPath);
	Payload->TryGetStringField(TEXT("blueprint_path"), Target.BlueprintPath);
	Payload->TryGetStringField(TEXT("graph"), Target.Graph);
	if (Target.Graph.IsEmpty())
	{
		Payload->TryGetStringField(TEXT("graph_name"), Target.Graph);
	}
	Payload->TryGetStringField(TEXT("function"), Target.Function);
	Payload->TryGetStringField(TEXT("event"), Target.Event);
	Payload->TryGetStringField(TEXT("block_id"), Target.BlockId);
	Payload->TryGetStringField(TEXT("node_path"), Target.NodePath);
	Payload->TryGetStringField(TEXT("pin_path"), Target.PinPath);
	Payload->TryGetStringField(TEXT("link_path"), Target.LinkPath);
	Payload->TryGetStringField(TEXT("component_name"), Target.ComponentName);
	Payload->TryGetStringField(TEXT("property_path"), Target.PropertyPath);
	Payload->TryGetStringField(TEXT("widget_path"), Target.WidgetPath);
	Payload->TryGetStringField(TEXT("row_name"), Target.RowName);
	FString TargetName;
	Payload->TryGetStringField(TEXT("target_name"), TargetName);
	if (Payload->TryGetStringField(TEXT("target_type"), Type))
	{
		Target.TargetType = ParseBridgeTargetType(Type);
	}
	if (Target.TargetType == EBlueprintHelperTargetType::None && Payload->TryGetStringField(TEXT("scope"), Type))
	{
		Target.TargetType = ParseLogicScopeTargetType(Type);
	}
	if (Target.TargetType == EBlueprintHelperTargetType::None)
	{
		Target.TargetType = InferTargetTypeFromReadFields(Target);
	}
	ApplyTargetNameToTypedField(Target, TargetName);
	return Target;
}

TArray<FString> UBlueprintHelperBridgeUtils::ReadStringArrayField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
{
	TArray<FString> Result;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Payload.IsValid() && Payload->TryGetArrayField(FieldName, Values) && Values)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (Value.IsValid() && Value->TryGetString(Text))
			{
				Result.Add(Text);
			}
		}
	}
	return Result;
}

TSharedRef<FJsonObject> UBlueprintHelperBridgeUtils::ReviewActionResultToJson(const FBlueprintHelperReviewActionResult& Result)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("success"), Result.bSucceeded);
	Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Result.NewStatus));
	if (!Result.TargetEvidenceId.IsEmpty()) { Json->SetStringField(TEXT("target_evidence_id"), Result.TargetEvidenceId); }
	if (!Result.Message.IsEmpty()) { Json->SetStringField(TEXT("message"), Result.Message); }
	if (!Result.HashGuardTargetKey.IsEmpty()) { Json->SetStringField(TEXT("hash_guard_target_key"), Result.HashGuardTargetKey); }
	return Json;
}
