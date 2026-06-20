// BlueprintHelper Bridge Layer - ClassSettings static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperClassSettingsBridgeRoutes.h"

#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"

class FBlueprintHelperClassSettingsBridgeRoutesLocalUtils
{
public:
	static FBlueprintHelperBridgeResponse MakeClassSettingsResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const FBlueprintHelperToolResultBase& Result)
	{
		FBlueprintHelperBridgeResponse Response = Result.bOk
			? FBlueprintHelperBridgeResponse::Success(Request.RequestId)
			: FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.Error.IsSet() ? Result.Error->Message : TEXT("ClassSettings command failed."));
		Response.Result = Result.ToJson();
		return Response;
	}

	static TArray<FString> ReadClassSettingsRouteStringArrayField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(FieldName, Values) || !Values)
		{
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Item;
			if (Value.IsValid() && Value->TryGetString(Item))
			{
				Result.Add(Item);
			}
		}
		return Result;
	}

	static TArray<FBlueprintHelperClassDefaultPropertySetting> ReadClassSettingsRouteDefaultSettings(
		const TSharedPtr<FJsonObject>& Payload)
	{
		TArray<FBlueprintHelperClassDefaultPropertySetting> Settings;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("settings"), Values) || !Values)
		{
			return Settings;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}

			FBlueprintHelperClassDefaultPropertySetting Setting;
			Object->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
			Setting.Value = Object->TryGetField(TEXT("value"));
			Settings.Add(MoveTemp(Setting));
		}
		return Settings;
	}

};

FBlueprintHelperClassSettingsBridgeRoutes::FBlueprintHelperClassSettingsBridgeRoutes(
	const FBlueprintHelperClassSettingsService& InClassSettingsService)
	: ClassSettingsService(InClassSettingsService)
{
}

bool FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(const FString& Command)
{
	return Command == TEXT("read_class_settings") ||
		Command == TEXT("read_blueprint_class_default_property") ||
		Command == TEXT("add_implemented_interface") ||
		Command == TEXT("add_implemented_interfaces") ||
		Command == TEXT("remove_implemented_interface") ||
		Command == TEXT("remove_implemented_interfaces") ||
		Command == TEXT("set_class_default_property") ||
		Command == TEXT("set_class_default_properties") ||
		Command == TEXT("reparent_blueprint");
}

FBlueprintHelperBridgeResponse FBlueprintHelperClassSettingsBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	FString AssetPath;
	if (Request.Payload.IsValid())
	{
		Request.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	}

	if (Request.Command == TEXT("read_class_settings"))
	{
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(Request, ClassSettingsService.ReadClassSettings(AssetPath));
	}
	if (Request.Command == TEXT("read_blueprint_class_default_property"))
	{
		FString PropertyPath;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("property_path"), PropertyPath);
			if (PropertyPath.IsEmpty())
			{
				Request.Payload->TryGetStringField(TEXT("target_name"), PropertyPath);
			}
		}
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.ReadClassDefaultProperty(AssetPath, PropertyPath));
	}
	if (Request.Command == TEXT("add_implemented_interface"))
	{
		FString InterfacePath;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath);
		}
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.AddImplementedInterface(AssetPath, InterfacePath));
	}
	if (Request.Command == TEXT("add_implemented_interfaces"))
	{
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.AddImplementedInterfaces(
				AssetPath,
				FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::ReadClassSettingsRouteStringArrayField(Request.Payload, TEXT("interface_paths"))));
	}
	if (Request.Command == TEXT("remove_implemented_interface"))
	{
		FString InterfacePath;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath);
		}
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.RemoveImplementedInterface(AssetPath, InterfacePath));
	}
	if (Request.Command == TEXT("remove_implemented_interfaces"))
	{
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.RemoveImplementedInterfaces(
				AssetPath,
				FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::ReadClassSettingsRouteStringArrayField(Request.Payload, TEXT("interface_paths"))));
	}
	if (Request.Command == TEXT("set_class_default_property"))
	{
		FString PropertyPath;
		TSharedPtr<FJsonValue> Value;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("property_path"), PropertyPath);
			const TSharedPtr<FJsonValue> FoundValue = FBlueprintHelperVersionCompat::FindJsonValue(Request.Payload, TEXT("value"));
			if (FoundValue.IsValid())
			{
				Value = FoundValue;
			}
		}
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.SetClassDefaultProperty(AssetPath, PropertyPath, Value));
	}
	if (Request.Command == TEXT("set_class_default_properties"))
	{
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.SetClassDefaultProperties(
				AssetPath,
				FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::ReadClassSettingsRouteDefaultSettings(Request.Payload)));
	}
	if (Request.Command == TEXT("reparent_blueprint"))
	{
		FString NewParentClass;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("new_parent_class"), NewParentClass);
		}
		return FBlueprintHelperClassSettingsBridgeRoutesLocalUtils::MakeClassSettingsResponse(
			Request,
			ClassSettingsService.ReparentBlueprint(AssetPath, NewParentClass));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 ClassSettings 命令: %s"), *Request.Command));
}
