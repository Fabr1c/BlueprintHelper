// BlueprintHelper Bridge Layer - Component static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperComponentBridgeRoutes.h"

#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"

namespace
{
	FBlueprintHelperBridgeResponse MakeComponentResponse(
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

	FBlueprintHelperSetComponentPropertiesRequest ReadComponentPropertiesRequest(
		const TSharedPtr<FJsonObject>& Payload,
		EBlueprintHelperComponentPropertyMode Mode)
	{
		FBlueprintHelperSetComponentPropertiesRequest Request;
		Request.Mode = Mode;
		if (!Payload.IsValid())
		{
			return Request;
		}

		Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
		Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

		if (Mode == EBlueprintHelperComponentPropertyMode::Single)
		{
			FBlueprintHelperComponentPropertySetting Setting;
			Payload->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
			const TSharedPtr<FJsonValue>* Value = Payload->Values.Find(TEXT("value"));
			if (Value)
			{
				Setting.Value = *Value;
			}
			Request.Settings.Add(MoveTemp(Setting));
			return Request;
		}

		const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
		if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *SettingsArray)
			{
				const TSharedPtr<FJsonObject> ItemObject = ItemValue.IsValid()
					? ItemValue->AsObject()
					: nullptr;
				if (!ItemObject.IsValid())
				{
					continue;
				}

				FBlueprintHelperComponentPropertySetting Setting;
				ItemObject->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
				const TSharedPtr<FJsonValue>* Value = ItemObject->Values.Find(TEXT("value"));
				if (Value)
				{
					Setting.Value = *Value;
				}
				Request.Settings.Add(MoveTemp(Setting));
			}
		}
		return Request;
	}
}

FBlueprintHelperComponentBridgeRoutes::FBlueprintHelperComponentBridgeRoutes(
	const FBlueprintHelperComponentService& InComponentService)
	: ComponentService(InComponentService)
{
}

bool FBlueprintHelperComponentBridgeRoutes::IsComponentCommand(const FString& Command)
{
	return Command == TEXT("read_components") ||
		Command == TEXT("add_component") ||
		Command == TEXT("set_component_property") ||
		Command == TEXT("set_component_properties") ||
		Command == TEXT("remove_component");
}

FBlueprintHelperBridgeResponse FBlueprintHelperComponentBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	if (Request.Command == TEXT("read_components"))
	{
		FBlueprintHelperReadComponentsRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
		}
		return MakeComponentResponse(
			Request,
			ComponentService.ReadComponents(ServiceRequest),
			TEXT("read_components 执行失败。"));
	}

	if (Request.Command == TEXT("add_component"))
	{
		FBlueprintHelperAddComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetStringField(TEXT("component_class"), ServiceRequest.ComponentClass);
			Request.Payload->TryGetStringField(TEXT("parent_component"), ServiceRequest.ParentComponent);
			Request.Payload->TryGetStringField(TEXT("socket_name"), ServiceRequest.SocketName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);

			FString AttachRule;
			if (Request.Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
			{
				TryParseAttachRule(AttachRule, ServiceRequest.AttachRule);
			}
			FString NameCollisionPolicy;
			if (Request.Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy))
			{
				TryParseNameCollisionPolicy(NameCollisionPolicy, ServiceRequest.NameCollisionPolicy);
			}
		}
		return MakeComponentResponse(
			Request,
			ComponentService.AddComponent(ServiceRequest),
			TEXT("add_component 执行失败。"));
	}

	if (Request.Command == TEXT("set_component_property"))
	{
		return MakeComponentResponse(
			Request,
			ComponentService.SetComponentProperty(
				ReadComponentPropertiesRequest(Request.Payload, EBlueprintHelperComponentPropertyMode::Single)),
			TEXT("set_component_property 执行失败。"));
	}

	if (Request.Command == TEXT("set_component_properties"))
	{
		return MakeComponentResponse(
			Request,
			ComponentService.SetComponentProperties(
				ReadComponentPropertiesRequest(Request.Payload, EBlueprintHelperComponentPropertyMode::Batch)),
			TEXT("set_component_properties 执行失败。"));
	}

	if (Request.Command == TEXT("remove_component"))
	{
		FBlueprintHelperRemoveComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);
		}
		return MakeComponentResponse(
			Request,
			ComponentService.RemoveComponent(ServiceRequest),
			TEXT("remove_component 执行失败。"));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 Component 命令: %s"), *Request.Command));
}
