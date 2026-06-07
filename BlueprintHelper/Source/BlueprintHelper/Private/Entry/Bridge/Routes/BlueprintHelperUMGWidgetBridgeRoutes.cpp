// BlueprintHelper Bridge Layer - UMGWidget static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperUMGWidgetBridgeRoutes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

class FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils
{
public:
	static FString ReadUMGWidgetRouteStringField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
	{
		FString Value;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	static TOptional<int32> ReadUMGWidgetRouteOptionalIntField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName)
	{
		if (!Payload.IsValid())
		{
			return TOptional<int32>();
		}

		double Number = 0.0;
		if (!Payload->TryGetNumberField(FieldName, Number))
		{
			return TOptional<int32>();
		}
		return FMath::RoundToInt(Number);
	}

	static bool ReadUMGWidgetRouteBoolField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bDefaultValue)
	{
		bool bValue = bDefaultValue;
		if (Payload.IsValid())
		{
			Payload->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	static FBlueprintHelperBridgeResponse MakeMissingWidgetFieldResponse(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Message)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			Message);
	}

};

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
		Command == TEXT("set_named_slot_content") ||
		Command == TEXT("get_widget_properties") ||
		Command == TEXT("set_widget_property") ||
		Command == TEXT("set_slot_property") ||
		Command == TEXT("set_widget_as_variable");
}

FBlueprintHelperBridgeResponse FBlueprintHelperUMGWidgetBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	const FString AssetPath = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("asset_path"));

	if (Request.Command == TEXT("get_widget_tree"))
	{
		if (AssetPath.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 字段。"));
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
		Response.Result = Result.Summary.ToJson();
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
		const FString WidgetClass = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_class"));
		const FString ParentName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("parent_name"));
		const FString SlotName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("slot_name"));
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		if (AssetPath.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 字段。"));
		}
		if (WidgetClass.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 widget_class 字段。"));
		}

		FBlueprintHelperAddWidgetRequest ServiceRequest;
		ServiceRequest.AssetPath = AssetPath;
		ServiceRequest.ParentName = ParentName;
		ServiceRequest.SlotName = SlotName;
		ServiceRequest.WidgetClass = WidgetClass;
		ServiceRequest.WidgetName = WidgetName;
		ServiceRequest.VirtualIndex = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteOptionalIntField(Request.Payload, TEXT("virtual_index"));
		ServiceRequest.ExpectedParentName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("expected_parent_name"));
		ServiceRequest.bDryRun = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteBoolField(Request.Payload, TEXT("dry_run"), false);

		const FBlueprintHelperWidgetMutationResult Result = WidgetService.AddWidget(ServiceRequest);
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
		if (Result.ReadbackContext.IsValid())
		{
			Response.Result->SetObjectField(TEXT("readback_context"), Result.ReadbackContext.ToSharedRef());
		}
		return Response;
	}

	if (Request.Command == TEXT("remove_widget"))
	{
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 。widget_name 字段。"));
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
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		const FString NewParent = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("new_parent_name"));
		const FString SlotName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("slot_name"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || NewParent.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload missing asset_path / widget_name / new_parent_name."));
		}

		FBlueprintHelperMoveWidgetRequest ServiceRequest;
		ServiceRequest.AssetPath = AssetPath;
		ServiceRequest.WidgetName = WidgetName;
		ServiceRequest.NewParentName = NewParent;
		ServiceRequest.SlotName = SlotName;
		ServiceRequest.VirtualIndex = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteOptionalIntField(Request.Payload, TEXT("virtual_index"));
		ServiceRequest.ExpectedParentName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("expected_parent_name"));
		ServiceRequest.ExpectedVirtualIndex = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteOptionalIntField(Request.Payload, TEXT("expected_virtual_index"));
		ServiceRequest.bDryRun = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteBoolField(Request.Payload, TEXT("dry_run"), false);

		const FBlueprintHelperWidgetMutationResult Result = WidgetService.MoveWidget(ServiceRequest);
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
		Response.Result->SetStringField(TEXT("new_parent_name"), NewParent);
		if (Result.ReadbackContext.IsValid())
		{
			Response.Result->SetObjectField(TEXT("readback_context"), Result.ReadbackContext.ToSharedRef());
		}
		return Response;
	}

	if (Request.Command == TEXT("set_named_slot_content"))
	{
		const FString HostWidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("host_widget_name"));
		const FString SlotName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("slot_name"));
		const FString WidgetClass = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_class"));
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		if (AssetPath.IsEmpty() || HostWidgetName.IsEmpty() || SlotName.IsEmpty() || WidgetClass.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload missing asset_path / host_widget_name / slot_name / widget_class."));
		}

		FBlueprintHelperSetNamedSlotContentRequest ServiceRequest;
		ServiceRequest.AssetPath = AssetPath;
		ServiceRequest.HostWidgetName = HostWidgetName;
		ServiceRequest.SlotName = SlotName;
		ServiceRequest.WidgetClass = WidgetClass;
		ServiceRequest.WidgetName = WidgetName;
		ServiceRequest.VirtualIndex = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteOptionalIntField(Request.Payload, TEXT("virtual_index"));
		ServiceRequest.ExpectedContentWidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("expected_content_widget_name"));
		ServiceRequest.bReplaceExisting = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteBoolField(Request.Payload, TEXT("replace_existing"), false);
		ServiceRequest.bDryRun = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteBoolField(Request.Payload, TEXT("dry_run"), false);

		const FBlueprintHelperWidgetMutationResult Result = WidgetService.SetNamedSlotContent(ServiceRequest);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperBridgeResponse::Error(
				Request.RequestId,
				EBlueprintHelperBridgeError::ExecutionFailed,
				Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("host_widget_name"), HostWidgetName);
		Response.Result->SetStringField(TEXT("slot_name"), SlotName);
		Response.Result->SetStringField(TEXT("content_widget_name"), Result.AffectedWidget);
		if (Result.ReadbackContext.IsValid())
		{
			Response.Result->SetObjectField(TEXT("readback_context"), Result.ReadbackContext.ToSharedRef());
		}
		return Response;
	}

	if (Request.Command == TEXT("get_widget_properties"))
	{
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path 。widget_name 字段。"));
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
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		const FString PropertyName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("property_name"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || PropertyName.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload 缺少 asset_path / widget_name / property_name 字段。"));
		}

		const FString Value = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("value"));
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

	if (Request.Command == TEXT("set_slot_property"))
	{
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		const FString PropertyPath = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("property_path"));
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || PropertyPath.IsEmpty())
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload missing asset_path / widget_name / property_path."));
		}

		FBlueprintHelperSetSlotPropertyRequest ServiceRequest;
		ServiceRequest.AssetPath = AssetPath;
		ServiceRequest.WidgetName = WidgetName;
		ServiceRequest.PropertyPath = PropertyPath;
		ServiceRequest.Value = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("value"));
		ServiceRequest.ExpectedSlotClassPath = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("expected_slot_class_path"));
		ServiceRequest.bDryRun = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteBoolField(Request.Payload, TEXT("dry_run"), false);

		const FBlueprintHelperWidgetMutationResult Result = WidgetService.SetSlotProperty(ServiceRequest);
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
		Response.Result->SetStringField(TEXT("property_path"), PropertyPath);
		Response.Result->SetStringField(TEXT("new_value"), ServiceRequest.Value);
		if (Result.ReadbackContext.IsValid())
		{
			Response.Result->SetObjectField(TEXT("readback_context"), Result.ReadbackContext.ToSharedRef());
		}
		return Response;
	}

	if (Request.Command == TEXT("set_widget_as_variable"))
	{
		const FString WidgetName = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("widget_name"));
		bool bIsVariable = false;
		if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || !Request.Payload.IsValid() || !Request.Payload->TryGetBoolField(TEXT("is_variable"), bIsVariable))
		{
			return FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::MakeMissingWidgetFieldResponse(Request, TEXT("payload missing asset_path / widget_name / boolean is_variable."));
		}

		FBlueprintHelperSetWidgetAsVariableRequest ServiceRequest;
		ServiceRequest.AssetPath = AssetPath;
		ServiceRequest.WidgetName = WidgetName;
		ServiceRequest.bIsVariable = bIsVariable;
		ServiceRequest.ExpectedWidgetClassPath = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteStringField(Request.Payload, TEXT("expected_widget_class_path"));
		ServiceRequest.bDryRun = FBlueprintHelperUMGWidgetBridgeRoutesLocalUtils::ReadUMGWidgetRouteBoolField(Request.Payload, TEXT("dry_run"), false);

		const FBlueprintHelperWidgetMutationResult Result = WidgetService.SetWidgetAsVariable(ServiceRequest);
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
		Response.Result->SetBoolField(TEXT("is_variable"), bIsVariable);
		if (Result.ReadbackContext.IsValid())
		{
			Response.Result->SetObjectField(TEXT("readback_context"), Result.ReadbackContext.ToSharedRef());
		}
		return Response;
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知 UMGWidget 命令: %s"), *Request.Command));
}
