// BlueprintHelper Bridge Layer - UMGWidget static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperUMGWidgetBridgeRoutes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

namespace
{
	FString ReadStringField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
	{
		FString Value;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	FBlueprintHelperBridgeResponse MakeMissingWidgetFieldResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Message)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			Message);
	}
}

FBlueprintHelperUMGWidgetBridgeRoutes::FBlueprintHelperUMGWidgetBridgeRoutes(
	const FBlueprintHelperWidgetService& InWidgetService)
	: WidgetService(InWidgetService)
{
}

bool FBlueprintHelperUMGWidgetBridgeRoutes::IsUMGWidgetCommand(const FString& Command)
{
	return Command == TEXT("get_widget_tree") ||
		Command == TEXT("add_widget") ||
		Command == TEXT("remove_widget") ||
		Command == TEXT("move_widget") ||
		Command == TEXT("get_widget_properties") ||
		Command == TEXT("set_widget_property");
}

FBlueprintHelperBridgeResponse FBlueprintHelperUMGWidgetBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	const FString AssetPath = ReadStringField(Request.Payload, TEXT("asset_path"));

	if (Request.Command == TEXT("get_widget_tree"))
	{
		if (AssetPath.IsEmpty())
		{
			return MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 字段。"));
		}

		const FBlueprintHelperWidgetTreeResult Result = WidgetService.GetWidgetTree(AssetPath);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("root_widget"), Result.RootWidgetName);
		Response.Result->SetNumberField(TEXT("count"), Result.Widgets.Num());

		TArray<TSharedPtr<FJsonValue>> WidgetArray;
		for (const FBlueprintHelperWidgetInfo& Info : Result.Widgets)
		{
			TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("name"), Info.Name);
			Object->SetStringField(TEXT("class"), Info.WidgetClass);
			if (!Info.ParentName.IsEmpty())
			{
				Object->SetStringField(TEXT("parent"), Info.ParentName);
			}
			if (!Info.SlotClass.IsEmpty())
			{
				Object->SetStringField(TEXT("slot_class"), Info.SlotClass);
			}
			Object->SetNumberField(TEXT("child_count"), Info.ChildCount);
			Object->SetNumberField(TEXT("depth"), Info.Depth);
			WidgetArray.Add(MakeShared<FJsonValueObject>(Object));
		}
		Response.Result->SetArrayField(TEXT("widgets"), WidgetArray);
		return Response;
	}

	if (Request.Command == TEXT("add_widget"))
	{
		const FString WidgetClass = ReadStringField(Request.Payload, TEXT("widget_class"));
		const FString ParentName = ReadStringField(Request.Payload, TEXT("parent_name"));
		const FString WidgetName = ReadStringField(Request.Payload, TEXT("widget_name"));
		if (AssetPath.IsEmpty())
		{
			return MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 字段。"));
		}
		if (WidgetClass.IsEmpty())
		{
			return MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 widget_class 字段。"));
		}

		const FBlueprintHelperWidgetMutationResult Result =
			WidgetService.AddWidget(AssetPath, ParentName, WidgetClass, WidgetName);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("added_widget"), Result.AffectedWidget);
		return Response;
	}

	if (Request.Command == TEXT("remove_widget"))
	{
		const FString WidgetName = ReadStringField(Request.Payload, TEXT("widget_name"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
		{
			return MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 。widget_name 字段。"));
		}

		const FBlueprintHelperWidgetMutationResult Result = WidgetService.RemoveWidget(AssetPath, WidgetName);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("removed_widget"), Result.AffectedWidget);
		return Response;
	}

	if (Request.Command == TEXT("move_widget"))
	{
		const FString WidgetName = ReadStringField(Request.Payload, TEXT("widget_name"));
		const FString NewParent = ReadStringField(Request.Payload, TEXT("new_parent"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || NewParent.IsEmpty())
		{
			return MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path / widget_name / new_parent 字段。"));
		}

		int32 InsertIndex = -1;
		if (Request.Payload.IsValid() && Request.Payload->HasField(TEXT("insert_index")))
		{
			InsertIndex = static_cast<int32>(Request.Payload->GetNumberField(TEXT("insert_index")));
		}

		const FBlueprintHelperWidgetMutationResult Result =
			WidgetService.MoveWidget(AssetPath, WidgetName, NewParent, InsertIndex);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("moved_widget"), Result.AffectedWidget);
		Response.Result->SetStringField(TEXT("new_parent"), NewParent);
		return Response;
	}

	if (Request.Command == TEXT("get_widget_properties"))
	{
		const FString WidgetName = ReadStringField(Request.Payload, TEXT("widget_name"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
		{
			return MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 。widget_name 字段。"));
		}

		const FBlueprintHelperWidgetPropertyResult Result =
			WidgetService.GetWidgetProperties(AssetPath, WidgetName);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetNumberField(TEXT("count"), Result.Properties.Num());

		TArray<TSharedPtr<FJsonValue>> PropertyArray;
		for (const FBlueprintHelperWidgetPropertyInfo& Info : Result.Properties)
		{
			TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("name"), Info.Name);
			Object->SetStringField(TEXT("type"), Info.TypeName);
			Object->SetStringField(TEXT("value"), Info.Value);
			Object->SetStringField(TEXT("flags"), Info.Flags);
			PropertyArray.Add(MakeShared<FJsonValueObject>(Object));
		}
		Response.Result->SetArrayField(TEXT("properties"), PropertyArray);
		return Response;
	}

	if (Request.Command == TEXT("set_widget_property"))
	{
		const FString WidgetName = ReadStringField(Request.Payload, TEXT("widget_name"));
		const FString PropertyName = ReadStringField(Request.Payload, TEXT("property_name"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || PropertyName.IsEmpty())
		{
			return MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path / widget_name / property_name 字段。"));
		}

		const FString Value = ReadStringField(Request.Payload, TEXT("value"));
		const FBlueprintHelperWidgetMutationResult Result =
			WidgetService.SetWidgetProperty(AssetPath, WidgetName, PropertyName, Value);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("widget"), Result.AffectedWidget);
		Response.Result->SetStringField(TEXT("property"), PropertyName);
		Response.Result->SetStringField(TEXT("new_value"), Value);
		return Response;
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 UMGWidget 命令: %s"), *Request.Command));
}
