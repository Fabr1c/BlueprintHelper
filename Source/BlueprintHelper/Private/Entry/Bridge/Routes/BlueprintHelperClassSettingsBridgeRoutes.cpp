// BlueprintHelper Bridge Layer - ClassSettings static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperClassSettingsBridgeRoutes.h"

#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"

namespace
{
	FBlueprintHelperBridgeResponse MakeClassSettingsResponse(
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

	TArray<FString> ReadClassSettingsRouteStringArrayField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
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

	TArray<FBlueprintHelperClassDefaultPropertySetting> ReadClassSettingsRouteDefaultSettings(
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
}

FBlueprintHelperClassSettingsBridgeRoutes::FBlueprintHelperClassSettingsBridgeRoutes(
	const FBlueprintHelperClassSettingsService& InClassSettingsService)
	: ClassSettingsService(InClassSettingsService)
{
}

bool FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(const FString& Command)
{
	return Command == TEXT("read_class_settings") ||
		Command == TEXT("add_implemented_interface") ||
		Command == TEXT("add_implemented_interfaces") ||
		Command == TEXT("remove_implemented_interface") ||
		Command == TEXT("remove_implemented_interfaces") ||
		Command == TEXT("set_class_default_property") ||
		Command == TEXT("set_class_default_properties");
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
		return MakeClassSettingsResponse(Request, ClassSettingsService.ReadClassSettings(AssetPath));
	}
	if (Request.Command == TEXT("add_implemented_interface"))
	{
		FString InterfacePath;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath);
		}
		return MakeClassSettingsResponse(
			Request,
			ClassSettingsService.AddImplementedInterface(AssetPath, InterfacePath));
	}
	if (Request.Command == TEXT("add_implemented_interfaces"))
	{
		return MakeClassSettingsResponse(
			Request,
			ClassSettingsService.AddImplementedInterfaces(
				AssetPath,
				ReadClassSettingsRouteStringArrayField(Request.Payload, TEXT("interface_paths"))));
	}
	if (Request.Command == TEXT("remove_implemented_interface"))
	{
		FString InterfacePath;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath);
		}
		return MakeClassSettingsResponse(
			Request,
			ClassSettingsService.RemoveImplementedInterface(AssetPath, InterfacePath));
	}
	if (Request.Command == TEXT("remove_implemented_interfaces"))
	{
		return MakeClassSettingsResponse(
			Request,
			ClassSettingsService.RemoveImplementedInterfaces(
				AssetPath,
				ReadClassSettingsRouteStringArrayField(Request.Payload, TEXT("interface_paths"))));
	}
	if (Request.Command == TEXT("set_class_default_property"))
	{
		FString PropertyPath;
		TSharedPtr<FJsonValue> Value;
		if (Request.Payload.IsValid())
		{
			Request.Payload->TryGetStringField(TEXT("property_path"), PropertyPath);
			const TSharedPtr<FJsonValue>* FoundValue = Request.Payload->Values.Find(TEXT("value"));
			if (FoundValue)
			{
				Value = *FoundValue;
			}
		}
		return MakeClassSettingsResponse(
			Request,
			ClassSettingsService.SetClassDefaultProperty(AssetPath, PropertyPath, Value));
	}
	if (Request.Command == TEXT("set_class_default_properties"))
	{
		return MakeClassSettingsResponse(
			Request,
			ClassSettingsService.SetClassDefaultProperties(
				AssetPath,
				ReadClassSettingsRouteDefaultSettings(Request.Payload)));
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 ClassSettings 命令: %s"), *Request.Command));
}
