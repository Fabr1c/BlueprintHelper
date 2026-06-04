// BlueprintHelper Bridge Layer - Component static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperComponentBridgeRoutes.h"

#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"

class FBlueprintHelperComponentBridgeRoutesLocalUtils
{
public:
	static FBlueprintHelperBridgeResponse MakeComponentResponse(
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

	static FBlueprintHelperBridgeResponse MakeInvalidComponentPolicyResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const TCHAR* FieldName,
		const FString& Value)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			FString::Printf(TEXT("Unsupported component %s value: %s"), FieldName, *Value));
	}

	static FBlueprintHelperSetComponentPropertiesRequest ReadComponentPropertiesRequest(
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
			const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(Payload, TEXT("value"));
			if (Value.IsValid())
			{
				Setting.Value = Value;
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
				const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(ItemObject, TEXT("value"));
				if (Value.IsValid())
				{
					Setting.Value = Value;
				}
				Request.Settings.Add(MoveTemp(Setting));
			}
		}
		return Request;
	}

};

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
		Command == TEXT("rename_component") ||
		Command == TEXT("reparent_component") ||
		Command == TEXT("attach_component") ||
		Command == TEXT("detach_component") ||
		Command == TEXT("set_root_component") ||
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
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
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
				if (!TryParseAttachRule(AttachRule, ServiceRequest.AttachRule))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("attach_rule"),
						AttachRule);
				}
			}
			FString NameCollisionPolicy;
			if (Request.Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy))
			{
				if (!TryParseNameCollisionPolicy(NameCollisionPolicy, ServiceRequest.NameCollisionPolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("name_collision_policy"),
						NameCollisionPolicy);
				}
			}
		}
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.AddComponent(ServiceRequest),
			TEXT("add_component 执行失败。"));
	}

	if (Request.Command == TEXT("set_component_property"))
	{
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.SetComponentProperty(
				FBlueprintHelperComponentBridgeRoutesLocalUtils::ReadComponentPropertiesRequest(Request.Payload, EBlueprintHelperComponentPropertyMode::Single)),
			TEXT("set_component_property 执行失败。"));
	}

	if (Request.Command == TEXT("set_component_properties"))
	{
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.SetComponentProperties(
				FBlueprintHelperComponentBridgeRoutesLocalUtils::ReadComponentPropertiesRequest(Request.Payload, EBlueprintHelperComponentPropertyMode::Batch)),
			TEXT("set_component_properties 执行失败。"));
	}

	if (Request.Command == TEXT("rename_component"))
	{
		FBlueprintHelperRenameComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetStringField(TEXT("new_component_name"), ServiceRequest.NewComponentName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);
		}
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.RenameComponent(ServiceRequest),
			TEXT("rename_component failed."));
	}

	if (Request.Command == TEXT("reparent_component"))
	{
		FBlueprintHelperReparentComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetStringField(TEXT("new_parent_component"), ServiceRequest.NewParentComponent);
			Request.Payload->TryGetStringField(TEXT("socket_name"), ServiceRequest.SocketName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);
			FString AttachRule;
			if (Request.Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
			{
				if (!TryParseAttachRule(AttachRule, ServiceRequest.AttachRule))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("attach_rule"),
						AttachRule);
				}
			}
			FString TransformPolicy;
			if (Request.Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
			{
				if (!TryParseComponentTransformPolicy(TransformPolicy, ServiceRequest.TransformPolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("transform_policy"),
						TransformPolicy);
				}
			}
		}
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.ReparentComponent(ServiceRequest),
			TEXT("reparent_component failed."));
	}

	if (Request.Command == TEXT("attach_component"))
	{
		FBlueprintHelperAttachComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetStringField(TEXT("parent_component"), ServiceRequest.ParentComponent);
			Request.Payload->TryGetStringField(TEXT("socket_name"), ServiceRequest.SocketName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);
			FString AttachRule;
			if (Request.Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
			{
				if (!TryParseAttachRule(AttachRule, ServiceRequest.AttachRule))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("attach_rule"),
						AttachRule);
				}
			}
			FString TransformPolicy;
			if (Request.Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
			{
				if (!TryParseComponentTransformPolicy(TransformPolicy, ServiceRequest.TransformPolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("transform_policy"),
						TransformPolicy);
				}
			}
		}
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.AttachComponent(ServiceRequest),
			TEXT("attach_component failed."));
	}

	if (Request.Command == TEXT("detach_component"))
	{
		FBlueprintHelperDetachComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);
			FString TransformPolicy;
			if (Request.Payload->TryGetStringField(TEXT("transform_policy"), TransformPolicy))
			{
				if (!TryParseComponentTransformPolicy(TransformPolicy, ServiceRequest.TransformPolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("transform_policy"),
						TransformPolicy);
				}
			}
			FString DefaultRootPolicy;
			if (Request.Payload->TryGetStringField(TEXT("default_root_policy"), DefaultRootPolicy))
			{
				if (!TryParseComponentDefaultRootPolicy(DefaultRootPolicy, ServiceRequest.DefaultRootPolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("default_root_policy"),
						DefaultRootPolicy);
				}
			}
		}
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.DetachComponent(ServiceRequest),
			TEXT("detach_component failed."));
	}

	if (Request.Command == TEXT("set_root_component"))
	{
		FBlueprintHelperSetRootComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);
			FString OldRootPolicy;
			if (Request.Payload->TryGetStringField(TEXT("old_root_policy"), OldRootPolicy))
			{
				if (!TryParseComponentOldRootPolicy(OldRootPolicy, ServiceRequest.OldRootPolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("old_root_policy"),
						OldRootPolicy);
				}
			}
			FString DefaultRootPolicy;
			if (Request.Payload->TryGetStringField(TEXT("default_root_policy"), DefaultRootPolicy))
			{
				if (!TryParseComponentDefaultRootPolicy(DefaultRootPolicy, ServiceRequest.DefaultRootPolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("default_root_policy"),
						DefaultRootPolicy);
				}
			}
		}
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.SetRootComponent(ServiceRequest),
			TEXT("set_root_component failed."));
	}

	if (Request.Command == TEXT("remove_component"))
	{
		FBlueprintHelperRemoveComponentRequest ServiceRequest;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("asset_path"), ServiceRequest.AssetPath);
			Request.Payload->TryGetStringField(TEXT("component_name"), ServiceRequest.ComponentName);
			Request.Payload->TryGetBoolField(TEXT("dry_run"), ServiceRequest.bDryRun);
			FString DeletePolicy;
			if (Request.Payload->TryGetStringField(TEXT("delete_policy"), DeletePolicy))
			{
				if (!TryParseComponentDeletePolicy(DeletePolicy, ServiceRequest.DeletePolicy))
				{
					return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeInvalidComponentPolicyResponse(
						Request,
						TEXT("delete_policy"),
						DeletePolicy);
				}
			}
		}
		return FBlueprintHelperComponentBridgeRoutesLocalUtils::MakeComponentResponse(
			Request,
			ComponentService.RemoveComponent(ServiceRequest),
			TEXT("remove_component 执行失败。"));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 Component 命令: %s"), *Request.Command));
}
