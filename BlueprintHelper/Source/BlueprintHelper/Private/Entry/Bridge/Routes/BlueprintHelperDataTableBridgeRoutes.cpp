// BlueprintHelper Bridge Layer - DataTable static cluster routes

#include "Entry/Bridge/Routes/BlueprintHelperDataTableBridgeRoutes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"

class FBlueprintHelperDataTableBridgeRoutesLocalUtils
{
public:
	static FString ReadDataTableRouteStringField(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
	{
		FString Value;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	static TArray<FString> ReadDataTableRouteRowNameFilter(const TSharedPtr<FJsonObject>& Payload)
	{
		TArray<FString> RowNames;
		const TArray<TSharedPtr<FJsonValue>>* RowNameValues = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("row_names"), RowNameValues) || !RowNameValues)
		{
			return RowNames;
		}

		for (const TSharedPtr<FJsonValue>& Value : *RowNameValues)
		{
			FString RowName;
			if (Value.IsValid() && Value->TryGetString(RowName))
			{
				RowNames.Add(MoveTemp(RowName));
			}
		}
		return RowNames;
	}

	static TMap<FString, FString> ReadDataTableRouteFieldsObject(const TSharedPtr<FJsonObject>& Payload)
	{
		TMap<FString, FString> Fields;
		const TSharedPtr<FJsonObject>* FieldsObject = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetObjectField(TEXT("fields"), FieldsObject) ||
			!FieldsObject || !FieldsObject->IsValid())
		{
			return Fields;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*FieldsObject)->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
			{
				Fields.Add(Pair.Key, MoveTemp(Value));
			}
		}
		return Fields;
	}

	static FBlueprintHelperBridgeResponse MakeInvalidDataTableRequest(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Message)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			Message);
	}

	static FBlueprintHelperBridgeResponse MakeDataTableExecutionFailure(
		const FBlueprintHelperBridgeRequest& Request,
		const FString& Message)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Message);
	}

	static TSharedPtr<FJsonObject> MakeDataTableMutationJson(const FBlueprintHelperDataTableMutationResult& Result)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("row_name"), Result.AffectedRow.ToString());
		return Json;
	}

};

FBlueprintHelperDataTableBridgeRoutes::FBlueprintHelperDataTableBridgeRoutes(
	const FBlueprintHelperDataTableService& InDataTableService)
	: DataTableService(InDataTableService)
{
}

bool FBlueprintHelperDataTableBridgeRoutes::IsDataTableCommand(const FString& Command)
{
	return Command == TEXT("get_datatable_rows") ||
		Command == TEXT("add_datatable_row") ||
		Command == TEXT("update_datatable_row") ||
		Command == TEXT("delete_datatable_row");
}

FBlueprintHelperBridgeResponse FBlueprintHelperDataTableBridgeRoutes::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	const FString AssetPath = FBlueprintHelperDataTableBridgeRoutesLocalUtils::ReadDataTableRouteStringField(Request.Payload, TEXT("asset_path"));

	if (Request.Command == TEXT("get_datatable_rows"))
	{
		if (AssetPath.IsEmpty())
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeInvalidDataTableRequest(Request, TEXT("payload requires asset_path."));
		}

		const FBlueprintHelperDataTableRowsResult Result =
			DataTableService.GetDataTableRows(AssetPath, FBlueprintHelperDataTableBridgeRoutesLocalUtils::ReadDataTableRouteRowNameFilter(Request.Payload));
		if (!Result.bSuccess)
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeDataTableExecutionFailure(Request, Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = MakeShared<FJsonObject>();
		Response.Result->SetStringField(TEXT("row_struct"), Result.RowStructName);
		Response.Result->SetNumberField(TEXT("row_count"), Result.Rows.Num());

		TArray<TSharedPtr<FJsonValue>> Columns;
		for (const FBlueprintHelperDataTableColumnInfo& Column : Result.Columns)
		{
			TSharedPtr<FJsonObject> ColumnJson = MakeShared<FJsonObject>();
			ColumnJson->SetStringField(TEXT("name"), Column.Name);
			ColumnJson->SetStringField(TEXT("type"), Column.TypeName);
			Columns.Add(MakeShared<FJsonValueObject>(ColumnJson));
		}
		Response.Result->SetArrayField(TEXT("columns"), Columns);

		TArray<TSharedPtr<FJsonValue>> Rows;
		for (const FBlueprintHelperDataTableRowInfo& Row : Result.Rows)
		{
			TSharedPtr<FJsonObject> RowJson = MakeShared<FJsonObject>();
			RowJson->SetStringField(TEXT("row_name"), Row.RowName.ToString());

			TSharedPtr<FJsonObject> FieldsJson = MakeShared<FJsonObject>();
			for (const TPair<FString, FString>& Pair : Row.Fields)
			{
				FieldsJson->SetStringField(Pair.Key, Pair.Value);
			}
			RowJson->SetObjectField(TEXT("fields"), FieldsJson);
			Rows.Add(MakeShared<FJsonValueObject>(RowJson));
		}
		Response.Result->SetArrayField(TEXT("rows"), Rows);
		return Response;
	}

	const FString RowName = FBlueprintHelperDataTableBridgeRoutesLocalUtils::ReadDataTableRouteStringField(Request.Payload, TEXT("row_name"));
	if (Request.Command == TEXT("add_datatable_row"))
	{
		if (AssetPath.IsEmpty() || RowName.IsEmpty())
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeInvalidDataTableRequest(Request, TEXT("payload requires asset_path and row_name."));
		}

		const FBlueprintHelperDataTableMutationResult Result =
			DataTableService.AddDataTableRow(AssetPath, RowName, FBlueprintHelperDataTableBridgeRoutesLocalUtils::ReadDataTableRouteFieldsObject(Request.Payload));
		if (!Result.bSuccess)
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeDataTableExecutionFailure(Request, Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeDataTableMutationJson(Result);
		return Response;
	}

	if (Request.Command == TEXT("update_datatable_row"))
	{
		if (AssetPath.IsEmpty() || RowName.IsEmpty())
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeInvalidDataTableRequest(Request, TEXT("payload requires asset_path and row_name."));
		}

		const TMap<FString, FString> Fields = FBlueprintHelperDataTableBridgeRoutesLocalUtils::ReadDataTableRouteFieldsObject(Request.Payload);
		if (Fields.Num() == 0)
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeInvalidDataTableRequest(Request, TEXT("payload.fields must contain at least one field."));
		}

		const FBlueprintHelperDataTableMutationResult Result =
			DataTableService.UpdateDataTableRow(AssetPath, RowName, Fields);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeDataTableExecutionFailure(Request, Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeDataTableMutationJson(Result);
		return Response;
	}

	if (Request.Command == TEXT("delete_datatable_row"))
	{
		if (AssetPath.IsEmpty() || RowName.IsEmpty())
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeInvalidDataTableRequest(Request, TEXT("payload requires asset_path and row_name."));
		}

		const FBlueprintHelperDataTableMutationResult Result =
			DataTableService.DeleteDataTableRow(AssetPath, RowName);
		if (!Result.bSuccess)
		{
			return FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeDataTableExecutionFailure(Request, Result.ErrorMessage);
		}

		FBlueprintHelperBridgeResponse Response = FBlueprintHelperBridgeResponse::Success(Request.RequestId);
		Response.Result = FBlueprintHelperDataTableBridgeRoutesLocalUtils::MakeDataTableMutationJson(Result);
		return Response;
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("Unknown DataTable command: %s"), *Request.Command));
}
