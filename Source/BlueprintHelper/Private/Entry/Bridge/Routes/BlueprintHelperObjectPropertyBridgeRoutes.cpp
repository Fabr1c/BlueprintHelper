// BlueprintHelper Bridge Layer - ObjectProperty static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperObjectPropertyBridgeRoutes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"

class FBlueprintHelperObjectPropertyBridgeRoutesLocalUtils
{
public:
	static FString ReadObjectPropertyRouteStringField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
	{
		FString Value;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	static FBlueprintHelperBridgeResponse MakeInvalidObjectPropertyRequest(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Message)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			Message);
	}

};

FBlueprintHelperObjectPropertyBridgeRoutes::FBlueprintHelperObjectPropertyBridgeRoutes(
	const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService)
	: PropertyReflectionService(InPropertyReflectionService)
{
}

bool FBlueprintHelperObjectPropertyBridgeRoutes::IsObjectPropertyCommand(const FString& Command)
{
	return Command == TEXT("get_object_properties") ||
		Command == TEXT("set_object_property");
}

FBlueprintHelperBridgeResponse FBlueprintHelperObjectPropertyBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	const FString AssetPath = FBlueprintHelperObjectPropertyBridgeRoutesLocalUtils::ReadObjectPropertyRouteStringField(Request.Payload, TEXT("asset_path"));

	if (Request.Command == TEXT("get_object_properties"))
	{
		if (AssetPath.IsEmpty())
		{
			return FBlueprintHelperObjectPropertyBridgeRoutesLocalUtils::MakeInvalidObjectPropertyRequest(Request, TEXT("payload requires asset_path."));
		}

		const FBlueprintHelperObjectPropertiesResult Result =
			PropertyReflectionService.GetObjectProperties(AssetPath);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("class_name"), Result.ClassName);
		Response.Result->SetStringField(TEXT("asset_path"), Result.AssetPath);
		Response.Result->SetNumberField(TEXT("count"), Result.Properties.Num());

		TArray<TSharedPtr<FJsonValue>> Properties;
		for (const FBlueprintHelperObjectPropertyInfo& Info : Result.Properties)
		{
			TSharedPtr<FJsonObject> PropertyJson = MakeShared<FJsonObject>();
			PropertyJson->SetStringField(TEXT("name"), Info.Name);
			PropertyJson->SetStringField(TEXT("type"), Info.TypeName);
			PropertyJson->SetStringField(TEXT("value"), Info.Value);
			PropertyJson->SetStringField(TEXT("category"), Info.Category);
			PropertyJson->SetStringField(TEXT("flags"), Info.Flags);
			Properties.Add(MakeShared<FJsonValueObject>(PropertyJson));
		}
		Response.Result->SetArrayField(TEXT("properties"), Properties);
		return Response;
	}

	if (Request.Command == TEXT("set_object_property"))
	{
		const FString PropertyName = FBlueprintHelperObjectPropertyBridgeRoutesLocalUtils::ReadObjectPropertyRouteStringField(Request.Payload, TEXT("property_name"));
		if (AssetPath.IsEmpty() || PropertyName.IsEmpty())
		{
			return FBlueprintHelperObjectPropertyBridgeRoutesLocalUtils::MakeInvalidObjectPropertyRequest(Request, TEXT("payload requires asset_path and property_name."));
		}

		const FString Value = FBlueprintHelperObjectPropertyBridgeRoutesLocalUtils::ReadObjectPropertyRouteStringField(Request.Payload, TEXT("value"));
		const FBlueprintHelperSetPropertyResult Result =
			PropertyReflectionService.SetObjectProperty(AssetPath, PropertyName, Value);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("property"), Result.PropertyName);
		Response.Result->SetStringField(TEXT("old_value"), Result.OldValue);
		Response.Result->SetStringField(TEXT("new_value"), Result.NewValue);
		return Response;
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("Unknown ObjectProperty command: %s"), *Request.Command));
}
