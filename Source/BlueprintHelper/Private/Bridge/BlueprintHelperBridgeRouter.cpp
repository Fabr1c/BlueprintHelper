// BlueprintHelper Bridge Layer — 命令路由实现

#include "Bridge/BlueprintHelperBridgeRouter.h"
#include "Bridge/BlueprintHelperBridgeProtocol.h"
#include "Bridge/BlueprintHelperRequestValidator.h"
#include "BlueprintHelper.h"
#include "Services/BlueprintHelperImportService.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "Services/BlueprintHelperExportService.h"
#include "Services/BlueprintHelperLogicProcessor.h"
#include "Services/BlueprintHelperCompileService.h"
#include "Services/BlueprintHelperValidationService.h"
#include "Services/BlueprintHelperContextService.h"
#include "Services/BlueprintHelperAssetBrowseService.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Services/BlueprintHelperWidgetService.h"
#include "Services/BlueprintHelperPropertyReflectionService.h"
#include "Services/BlueprintHelperDataTableService.h"
#include "Services/BlueprintHelperEditorCommandService.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TSharedRef<FJsonObject> MakeLogicStatsObject(const FBlueprintHelperLogicResult& Result)
	{
		TSharedRef<FJsonObject> StatsObject = MakeShared<FJsonObject>();
		StatsObject->SetNumberField(TEXT("nodes"), Result.NodeCount);
		StatsObject->SetNumberField(TEXT("exec_links"), Result.ExecLinkCount);
		StatsObject->SetNumberField(TEXT("data_links"), Result.DataLinkCount);
		StatsObject->SetNumberField(TEXT("entry_points"), Result.EntryPointCount);
		StatsObject->SetNumberField(TEXT("orphans"), Result.OrphanNodeCount);
		return StatsObject;
	}

	FString JsonValueTypeToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("missing");
		}

		switch (Value->Type)
		{
		case EJson::None: return TEXT("missing");
		case EJson::Null: return TEXT("null");
		case EJson::String: return TEXT("string");
		case EJson::Number: return TEXT("number");
		case EJson::Boolean: return TEXT("bool");
		case EJson::Array: return TEXT("array");
		case EJson::Object: return TEXT("object");
		default: return TEXT("unknown");
		}
	}

	FBlueprintHelperBridgeValidationError MakePayloadFieldError(
		const TCHAR* FieldName,
		const FString& ExpectedType,
		const FString& ActualType)
	{
		FBlueprintHelperBridgeValidationError Error;
		Error.Code = TEXT("invalid_request");
		Error.Field = TEXT("payload.") + FString(FieldName);
		Error.ExpectedType = ExpectedType;
		Error.ActualType = ActualType;
		Error.Message = FString::Printf(TEXT("%s must be %s; actual type is %s."),
			*Error.Field, *ExpectedType, *ActualType);
		return Error;
	}

	bool TryReadStringField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bRequired,
		FString& OutValue,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		OutValue.Empty();
		if (!Payload.IsValid())
		{
			if (bRequired)
			{
				OutError = MakePayloadFieldError(FieldName, TEXT("string"), TEXT("missing"));
				return false;
			}
			return true;
		}

		const TSharedPtr<FJsonValue>* FoundValue = Payload->Values.Find(FieldName);
		if (!FoundValue)
		{
			if (bRequired)
			{
				OutError = MakePayloadFieldError(FieldName, TEXT("string"), TEXT("missing"));
				return false;
			}
			return true;
		}

		if (!(*FoundValue).IsValid() || !(*FoundValue)->TryGetString(OutValue))
		{
			OutError = MakePayloadFieldError(FieldName, TEXT("string"), JsonValueTypeToString(*FoundValue));
			return false;
		}

		return true;
	}

	bool TryReadStringOption(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		const FString& DefaultValue,
		FString& OutValue,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		OutValue = DefaultValue;
		if (!Payload.IsValid() || !Payload->HasField(FieldName))
		{
			return true;
		}

		FString Value;
		if (!TryReadStringField(Payload, FieldName, false, Value, OutError))
		{
			return false;
		}
		if (Value.IsEmpty())
		{
			OutError = MakePayloadFieldError(FieldName, TEXT("non_empty_string"), TEXT("empty_string"));
			return false;
		}

		OutValue = Value;
		return true;
	}

	bool TryReadBoolField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bRequired,
		bool& OutValue,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		if (!Payload.IsValid())
		{
			if (bRequired)
			{
				OutError = MakePayloadFieldError(FieldName, TEXT("bool"), TEXT("missing"));
				return false;
			}
			return true;
		}

		const TSharedPtr<FJsonValue>* FoundValue = Payload->Values.Find(FieldName);
		if (!FoundValue)
		{
			if (bRequired)
			{
				OutError = MakePayloadFieldError(FieldName, TEXT("bool"), TEXT("missing"));
				return false;
			}
			return true;
		}

		if (!(*FoundValue).IsValid() || !(*FoundValue)->TryGetBool(OutValue))
		{
			OutError = MakePayloadFieldError(FieldName, TEXT("bool"), JsonValueTypeToString(*FoundValue));
			return false;
		}

		return true;
	}

	bool TryReadNumberField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bRequired,
		double& OutValue,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		if (!Payload.IsValid())
		{
			if (bRequired)
			{
				OutError = MakePayloadFieldError(FieldName, TEXT("number"), TEXT("missing"));
				return false;
			}
			return true;
		}

		const TSharedPtr<FJsonValue>* FoundValue = Payload->Values.Find(FieldName);
		if (!FoundValue)
		{
			if (bRequired)
			{
				OutError = MakePayloadFieldError(FieldName, TEXT("number"), TEXT("missing"));
				return false;
			}
			return true;
		}

		if (!(*FoundValue).IsValid() || !(*FoundValue)->TryGetNumber(OutValue))
		{
			OutError = MakePayloadFieldError(FieldName, TEXT("number"), JsonValueTypeToString(*FoundValue));
			return false;
		}

		return true;
	}

	bool TryReadBoolOption(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool& InOutValue,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		if (!Payload.IsValid() || !Payload->HasField(FieldName))
		{
			return true;
		}

		bool bValue = false;
		if (!TryReadBoolField(Payload, FieldName, false, bValue, OutError))
		{
			return false;
		}

		InOutValue = bValue;
		return true;
	}

	bool TryReadStringOption(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		const FString& DefaultValue,
		FString& OutValue,
		FString& OutError)
	{
		OutValue = DefaultValue;
		if (!Payload.IsValid() || !Payload->HasField(FieldName))
		{
			return true;
		}

		if (!Payload->TryGetStringField(FieldName, OutValue) || OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("payload.%s 必须是非空字符串。"), FieldName);
			return false;
		}

		return true;
	}

	bool TryReadBoolOption(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool& InOutValue,
		FString& OutError)
	{
		if (!Payload.IsValid() || !Payload->HasField(FieldName))
		{
			return true;
		}

		bool bValue = false;
		if (!Payload->TryGetBoolField(FieldName, bValue))
		{
			OutError = FString::Printf(TEXT("payload.%s 必须是布尔值。"), FieldName);
			return false;
		}

		InOutValue = bValue;
		return true;
	}

	EBlueprintHelperBridgeError ValidationCodeToBridgeError(const FString& Code)
	{
		if (Code == TEXT("unauthorized"))
		{
			return EBlueprintHelperBridgeError::Unauthorized;
		}
		if (Code == TEXT("command_disabled"))
		{
			return EBlueprintHelperBridgeError::CommandDisabled;
		}
		return EBlueprintHelperBridgeError::InvalidRequest;
	}

	FBlueprintHelperBridgeResponse ValidationErrorResponse(
		const FString& RequestId,
		const FBlueprintHelperBridgeValidationError& Error)
	{
		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Error(
			RequestId,
			ValidationCodeToBridgeError(Error.Code),
			Error.Message.IsEmpty() ? TEXT("请求校验失败。") : Error.Message);

		Resp.Result = MakeShared<FJsonObject>();
		Resp.Result->SetStringField(TEXT("field"), Error.Field);
		if (!Error.ExpectedType.IsEmpty())
		{
			Resp.Result->SetStringField(TEXT("expected_type"), Error.ExpectedType);
		}
		if (!Error.ActualType.IsEmpty())
		{
			Resp.Result->SetStringField(TEXT("actual_type"), Error.ActualType);
		}
		return Resp;
	}

	EBlueprintHelperBridgeError DiagnosticSetToBridgeError(const FBlueprintHelperDiagnosticSet& Diagnostics)
	{
		for (const FBlueprintHelperDiagnosticItem& Item : Diagnostics.Items)
		{
			if (Item.Code == TEXT("graph_not_found"))
			{
				return EBlueprintHelperBridgeError::GraphNotFound;
			}
			if (Item.Code == TEXT("asset_not_found"))
			{
				return EBlueprintHelperBridgeError::AssetNotFound;
			}
		}
		return EBlueprintHelperBridgeError::ExecutionFailed;
	}

	TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonText)
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			return nullptr;
		}
		return JsonObject;
	}

	FString AgentImportSeverityToString(EBlueprintHelperAgentImportDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EBlueprintHelperAgentImportDiagnosticSeverity::Error:
			return TEXT("error");
		case EBlueprintHelperAgentImportDiagnosticSeverity::Warning:
			return TEXT("warning");
		default:
			return TEXT("info");
		}
	}

	TSharedRef<FJsonObject> AgentImportResultToJson(const FBlueprintHelperAgentImportResult& Result)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.AgentImportResult"));
		Object->SetBoolField(TEXT("success"), Result.bSuccess);
		Object->SetStringField(TEXT("status"), Result.Status);
		Object->SetStringField(TEXT("error_code"), Result.ErrorCode);
		Object->SetStringField(TEXT("message"), Result.GetSummaryText());
		Object->SetNumberField(TEXT("created_nodes"), Result.CreatedNodeCount);
		Object->SetNumberField(TEXT("created_links"), Result.CreatedLinkCount);
		Object->SetNumberField(TEXT("created_variables"), Result.CreatedVariableCount);
		Object->SetNumberField(TEXT("warning_count"), Result.WarningCount);
		Object->SetNumberField(TEXT("error_count"), Result.ErrorCount);
		Object->SetBoolField(TEXT("rolled_back"), Result.bRolledBack);
		Object->SetNumberField(TEXT("rollback_count"), Result.RollbackCount);
		Object->SetBoolField(TEXT("compiled"), Result.bCompiled);
		Object->SetBoolField(TEXT("saved"), Result.bSaved);
		Object->SetBoolField(TEXT("dry_run"), Result.bDryRun);

		TArray<TSharedPtr<FJsonValue>> WarningArray;
		for (const FString& Warning : Result.Warnings)
		{
			WarningArray.Add(MakeShared<FJsonValueString>(Warning));
		}
		Object->SetArrayField(TEXT("warnings"), WarningArray);

		TArray<TSharedPtr<FJsonValue>> DiagnosticArray;
		for (const FBlueprintHelperAgentImportDiagnostic& Diagnostic : Result.Diagnostics)
		{
			TSharedPtr<FJsonObject> DiagnosticObject = MakeShared<FJsonObject>();
			DiagnosticObject->SetStringField(TEXT("severity"), AgentImportSeverityToString(Diagnostic.Severity));
			DiagnosticObject->SetStringField(TEXT("code"), Diagnostic.Code);
			DiagnosticObject->SetStringField(TEXT("path"), Diagnostic.Path);
			DiagnosticObject->SetStringField(TEXT("message"), Diagnostic.Message);
			DiagnosticObject->SetStringField(TEXT("suggestion"), Diagnostic.Suggestion);
			DiagnosticArray.Add(MakeShared<FJsonValueObject>(DiagnosticObject));
		}
		Object->SetArrayField(TEXT("diagnostics"), DiagnosticArray);

		return Object;
	}
}

FBlueprintHelperBridgeRouter::FBlueprintHelperBridgeRouter(
	const FBlueprintHelperImportService& InImport,
	const FBlueprintHelperAgentImportService& InAgentImport,
	const FBlueprintHelperExportService& InExport,
	const FBlueprintHelperCompileService& InCompile,
	const FBlueprintHelperValidationService& InValidation,
	const FBlueprintHelperContextService& InContext,
	const FBlueprintHelperAssetBrowseService& InAssetBrowse,
	const FBlueprintHelperBlueprintStructureService& InStructure,
	const FBlueprintHelperWidgetService& InWidget,
	const FBlueprintHelperPropertyReflectionService& InPropertyReflection,
	const FBlueprintHelperDataTableService& InDataTable,
	const FBlueprintHelperEditorCommandService& InEditorCommand)
	: ImportService(InImport)
	, AgentImportService(InAgentImport)
	, ExportService(InExport)
	, CompileService(InCompile)
	, ValidationService(InValidation)
	, ContextService(InContext)
	, AssetBrowseService(InAssetBrowse)
	, StructureService(InStructure)
	, WidgetService(InWidget)
	, PropertyReflectionService(InPropertyReflection)
	, DataTableService(InDataTable)
	, EditorCommandService(InEditorCommand)
{
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	FBlueprintHelperBridgeValidationError ValidationError;
	if (!FBlueprintHelperRequestValidator::ValidatePayloadForCommand(Request.Command, Request.Payload, ValidationError))
	{
		return ValidationErrorResponse(Request.RequestId, ValidationError);
	}
	if (!FBlueprintHelperRequestValidator::ValidateAuthorization(Request, ValidationError))
	{
		return ValidationErrorResponse(Request.RequestId, ValidationError);
	}

	if (Request.Command == TEXT("get_rule_markdown"))
	{
		return HandleGetRuleMarkdown(Request);
	}
	if (Request.Command == TEXT("get_editor_context"))
	{
		return HandleGetEditorContext(Request);
	}
	if (Request.Command == TEXT("validate_json"))
	{
		return HandleValidateJson(Request);
	}
	if (Request.Command == TEXT("export_to_json"))
	{
		return HandleExportToJson(Request);
	}
	if (Request.Command == TEXT("export_logic"))
	{
		return HandleExportLogic(Request);
	}
	if (Request.Command == TEXT("import_json"))
	{
		return HandleImportJson(Request);
	}
	if (Request.Command == TEXT("import_agent_graph"))
	{
		return HandleImportAgentGraph(Request);
	}
	if (Request.Command == TEXT("compile_blueprint"))
	{
		return HandleCompileBlueprint(Request);
	}
	if (Request.Command == TEXT("open_asset"))
	{
		return HandleOpenAsset(Request);
	}
	if (Request.Command == TEXT("list_assets"))
	{
		return HandleListAssets(Request);
	}
	if (Request.Command == TEXT("search_assets"))
	{
		return HandleSearchAssets(Request);
	}
	if (Request.Command == TEXT("save_asset"))
	{
		return HandleSaveAsset(Request);
	}
	if (Request.Command == TEXT("get_asset_info"))
	{
		return HandleGetAssetInfo(Request);
	}
	// ─── Phase 5: 蓝图结构操作 ───
	if (Request.Command == TEXT("list_graphs"))
	{
		return HandleListGraphs(Request);
	}
	if (Request.Command == TEXT("list_variables"))
	{
		return HandleListVariables(Request);
	}
	if (Request.Command == TEXT("list_event_dispatchers"))
	{
		return HandleListEventDispatchers(Request);
	}
	if (Request.Command == TEXT("add_variable"))
	{
		return HandleAddVariable(Request);
	}
	if (Request.Command == TEXT("remove_variable"))
	{
		return HandleRemoveVariable(Request);
	}
	if (Request.Command == TEXT("add_graph"))
	{
		return HandleAddGraph(Request);
	}
	if (Request.Command == TEXT("remove_graph"))
	{
		return HandleRemoveGraph(Request);
	}
	if (Request.Command == TEXT("add_event_dispatcher"))
	{
		return HandleAddEventDispatcher(Request);
	}
	if (Request.Command == TEXT("delete_nodes"))
	{
		return HandleDeleteNodes(Request);
	}
	// ─── Phase 6: UMG Widget 操作 ───
	if (Request.Command == TEXT("get_widget_tree"))
	{
		return HandleGetWidgetTree(Request);
	}
	if (Request.Command == TEXT("add_widget"))
	{
		return HandleAddWidget(Request);
	}
	if (Request.Command == TEXT("remove_widget"))
	{
		return HandleRemoveWidget(Request);
	}
	if (Request.Command == TEXT("move_widget"))
	{
		return HandleMoveWidget(Request);
	}
	if (Request.Command == TEXT("get_widget_properties"))
	{
		return HandleGetWidgetProperties(Request);
	}
	if (Request.Command == TEXT("set_widget_property"))
	{
		return HandleSetWidgetProperty(Request);
	}
	// ─── Phase 7: DataAsset & DataTable 操作 ───
	if (Request.Command == TEXT("get_object_properties"))
	{
		return HandleGetObjectProperties(Request);
	}
	if (Request.Command == TEXT("set_object_property"))
	{
		return HandleSetObjectProperty(Request);
	}
	if (Request.Command == TEXT("get_datatable_rows"))
	{
		return HandleGetDataTableRows(Request);
	}
	if (Request.Command == TEXT("add_datatable_row"))
	{
		return HandleAddDataTableRow(Request);
	}
	if (Request.Command == TEXT("update_datatable_row"))
	{
		return HandleUpdateDataTableRow(Request);
	}
	if (Request.Command == TEXT("delete_datatable_row"))
	{
		return HandleDeleteDataTableRow(Request);
	}
	// ─── Phase 8: 编辑器命令 ───
	if (Request.Command == TEXT("undo"))
	{
		return HandleUndo(Request);
	}
	if (Request.Command == TEXT("redo"))
	{
		return HandleRedo(Request);
	}
	if (Request.Command == TEXT("play_in_editor"))
	{
		return HandlePlayInEditor(Request);
	}
	if (Request.Command == TEXT("stop_pie"))
	{
		return HandleStopPIE(Request);
	}
	if (Request.Command == TEXT("create_blueprint"))
	{
		return HandleCreateBlueprint(Request);
	}
	if (Request.Command == TEXT("exec_console_command"))
	{
		return HandleExecConsoleCommand(Request);
	}
	if (Request.Command == TEXT("close_editor"))
	{
		return HandleCloseEditor(Request);
	}

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("未知命令: %s"), *Request.Command));
}

// ─── get_rule_markdown ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetRuleMarkdown(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString Markdown = FBlueprintHelperModule::Get().GetJsonToBlueprintRuleMarkdown();
	if (Markdown.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InternalError,
			TEXT("未找到规则文档。"));
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("markdown"), Markdown);
	return Resp;
}

// ─── get_editor_context ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetEditorContext(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperEditorContext Ctx = ContextService.GetContext();

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = FBlueprintHelperBridgeProtocol::ContextToJson(Ctx);
	return Resp;
}

// ─── validate_json ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleValidateJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString JsonText;
	if (!TryReadStringField(Req.Payload, TEXT("json"), true, JsonText, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (JsonText.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 json 字段。"));
	}

	FBlueprintHelperValidationResult ValResult = ValidationService.Validate(JsonText);

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetBoolField(TEXT("is_valid"), ValResult.bValid);
	Resp.Result->SetStringField(TEXT("detected_version"), ValResult.DetectedVersion);
	Resp.Result->SetNumberField(TEXT("error_count"), ValResult.Diagnostics.ErrorCount);
	Resp.Result->SetNumberField(TEXT("warning_count"), ValResult.Diagnostics.WarningCount);

	TArray<TSharedPtr<FJsonValue>> DiagArray;
	for (const auto& Item : ValResult.Diagnostics.Items)
	{
		TSharedPtr<FJsonObject> DiagObj = MakeShared<FJsonObject>();
		DiagObj->SetStringField(TEXT("severity"),
			Item.Severity == EBlueprintHelperDiagnosticSeverity::Error ? TEXT("error") :
			Item.Severity == EBlueprintHelperDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
		DiagObj->SetStringField(TEXT("message"), Item.Message);
		DiagArray.Add(MakeShared<FJsonValueObject>(DiagObj));
	}
	Resp.Result->SetArrayField(TEXT("diagnostics"), DiagArray);

	return Resp;
}

// ─── export_to_json ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExportToJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperExportRequest ExportReq;

	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("target_blueprint"), ExportReq.Target.BlueprintPath);
		Req.Payload->TryGetStringField(TEXT("target_graph"), ExportReq.Target.GraphName);

		FString ScopeStr;
		Req.Payload->TryGetStringField(TEXT("scope"), ScopeStr);
		FString EffectiveScope;
		FString ScopeError;
		if (!FBlueprintHelperRequestValidator::NormalizeExportScope(ScopeStr, ExportReq.Scope, EffectiveScope, ScopeError))
		{
			return FBlueprintHelperBridgeResponse::Error(
				Req.RequestId,
				EBlueprintHelperBridgeError::InvalidRequest,
				ScopeError);
		}
	}
	else
	{
		ExportReq.Scope = EBlueprintHelperExportScope::SingleGraph;
	}

	FBlueprintHelperExportResult ExportResult = ExportService.Export(ExportReq);

	if (!ExportResult.bSuccess)
	{
		const FString ErrorMsg = ExportResult.Diagnostics.Items.Num() > 0
			? ExportResult.Diagnostics.Items[0].Message
			: TEXT("导出失败。");
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMsg);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("json"), ExportResult.JsonText);
	Resp.Result->SetStringField(TEXT("effective_scope"), ExportResult.EffectiveScope);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExportLogic(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> Payload = Req.Payload;

	FString ScopeStr;
	FString FormatStr;
	FString DetailStr;
	FBlueprintHelperBridgeValidationError ParseError;
	if (!TryReadStringOption(Payload, TEXT("scope"), TEXT("single_graph"), ScopeStr, ParseError)
		|| !TryReadStringOption(Payload, TEXT("format"), TEXT("logic_md"), FormatStr, ParseError)
		|| !TryReadStringOption(Payload, TEXT("detail"), TEXT("normal"), DetailStr, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	FBlueprintHelperExportRequest ExportReq;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("target_blueprint"), ExportReq.Target.BlueprintPath);
		Payload->TryGetStringField(TEXT("target_graph"), ExportReq.Target.GraphName);
	}

	if (ScopeStr == TEXT("single_graph"))
	{
		ExportReq.Scope = EBlueprintHelperExportScope::SingleGraph;
	}
	else if (ScopeStr == TEXT("full_blueprint"))
	{
		ExportReq.Scope = EBlueprintHelperExportScope::FullBlueprint;
	}
	else
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			FString::Printf(TEXT("不支持的 scope: %s"), *ScopeStr));
	}

	FBlueprintHelperLogicOptions LogicOptions;
	if (FormatStr == TEXT("logic_json"))
	{
		LogicOptions.Format = EBlueprintHelperLogicOutputFormat::LogicJson;
	}
	else if (FormatStr == TEXT("logic_md"))
	{
		LogicOptions.Format = EBlueprintHelperLogicOutputFormat::Markdown;
	}
	else
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			FString::Printf(TEXT("不支持的 format: %s"), *FormatStr));
	}

	if (DetailStr == TEXT("brief"))
	{
		LogicOptions.DetailLevel = EBlueprintHelperLogicDetailLevel::Brief;
	}
	else if (DetailStr == TEXT("normal"))
	{
		LogicOptions.DetailLevel = EBlueprintHelperLogicDetailLevel::Normal;
	}
	else if (DetailStr == TEXT("debug"))
	{
		LogicOptions.DetailLevel = EBlueprintHelperLogicDetailLevel::Debug;
	}
	else
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			FString::Printf(TEXT("不支持的 detail: %s"), *DetailStr));
	}

	if (!TryReadBoolOption(Payload, TEXT("include_data_dependencies"), LogicOptions.bIncludeDataDependencies, ParseError)
		|| !TryReadBoolOption(Payload, TEXT("include_orphans"), LogicOptions.bIncludeOrphanNodes, ParseError)
		|| !TryReadBoolOption(Payload, TEXT("include_node_ids"), LogicOptions.bIncludeNodeIds, ParseError)
		|| !TryReadBoolOption(Payload, TEXT("include_positions"), LogicOptions.bIncludePositions, ParseError)
		|| !TryReadBoolOption(Payload, TEXT("include_raw_node_types"), LogicOptions.bIncludeRawNodeTypes, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	const FBlueprintHelperExportResult ExportResult = ExportService.Export(ExportReq);
	if (!ExportResult.bSuccess)
	{
		const FString ErrorMsg = ExportResult.Diagnostics.Items.Num() > 0
			? ExportResult.Diagnostics.Items[0].Message
			: TEXT("导出失败。");
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			DiagnosticSetToBridgeError(ExportResult.Diagnostics),
			ErrorMsg);
	}

	const FBlueprintHelperLogicResult LogicResult =
		FBlueprintHelperLogicProcessor::ProcessRawJson(ExportResult.JsonText, LogicOptions);
	if (!LogicResult.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::JsonParseFailed,
			LogicResult.ErrorMessage.IsEmpty() ? TEXT("raw JSON 转逻辑视图失败。") : LogicResult.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("format"), FormatStr);
	Resp.Result->SetStringField(TEXT("schema"),
		LogicOptions.Format == EBlueprintHelperLogicOutputFormat::Markdown
			? TEXT("BlueprintHelper.LogicMarkdown")
			: TEXT("BlueprintHelper.LogicGraph"));
	Resp.Result->SetBoolField(TEXT("importable"), false);

	if (LogicOptions.Format == EBlueprintHelperLogicOutputFormat::Markdown)
	{
		Resp.Result->SetStringField(TEXT("markdown"), LogicResult.OutputText);
		Resp.Result->SetObjectField(TEXT("stats"), MakeLogicStatsObject(LogicResult));
		return Resp;
	}

	TSharedPtr<FJsonObject> LogicObject = ParseJsonObject(LogicResult.OutputText);
	if (!LogicObject.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::JsonParseFailed,
			TEXT("LogicProcessor 返回的 logic_json 无法解析。"));
	}

	Resp.Result->SetObjectField(TEXT("logic"), LogicObject);

	const TSharedPtr<FJsonObject>* StatsObject = nullptr;
	if (LogicObject->TryGetObjectField(TEXT("stats"), StatsObject) && StatsObject && StatsObject->IsValid())
	{
		Resp.Result->SetObjectField(TEXT("stats"), *StatsObject);
	}
	else
	{
		Resp.Result->SetObjectField(TEXT("stats"), MakeLogicStatsObject(LogicResult));
	}

	return Resp;
}

// ─── import_json ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleImportJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid() || !Req.Payload->HasField(TEXT("json")))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 json 字段。"));
	}

	FBlueprintHelperImportRequest ImportReq;
	FBlueprintHelperBridgeValidationError ParseError;
	if (!TryReadStringField(Req.Payload, TEXT("json"), true, ImportReq.JsonText, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}
	Req.Payload->TryGetStringField(TEXT("target_blueprint"), ImportReq.Target.BlueprintPath);
	Req.Payload->TryGetStringField(TEXT("target_graph"), ImportReq.Target.GraphName);
	if (!TryReadBoolOption(Req.Payload, TEXT("compile_after_import"), ImportReq.bAutoCompile, ParseError)
		|| !TryReadBoolOption(Req.Payload, TEXT("strict"), ImportReq.bStrict, ParseError)
		|| !TryReadBoolOption(Req.Payload, TEXT("allow_partial"), ImportReq.bAllowPartial, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	FBlueprintHelperImportResult ImportResult = ImportService.Import(ImportReq);

	if (!ImportResult.bSuccess && ImportResult.Diagnostics.HasErrors())
	{
		const FString ErrorMsg = ImportResult.GetSummaryText();
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			DiagnosticSetToBridgeError(ImportResult.Diagnostics),
			ErrorMsg);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId, ImportResult.GetSummaryText());
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("status"), ImportResult.Status);
	Resp.Result->SetNumberField(TEXT("generated_node_count"), ImportResult.GeneratedNodeCount);
	Resp.Result->SetNumberField(TEXT("nodes_created"), ImportResult.GeneratedNodeCount);
	Resp.Result->SetNumberField(TEXT("links_connected"), ImportResult.LinksConnected);
	Resp.Result->SetNumberField(TEXT("operations_applied"), ImportResult.OperationsApplied);
	Resp.Result->SetNumberField(TEXT("unresolved_node_count"), ImportResult.UnresolvedNodeCount);
	Resp.Result->SetBoolField(TEXT("rolled_back"), ImportResult.bRolledBack);

	TArray<TSharedPtr<FJsonValue>> UnresolvedArray;
	for (const FString& Summary : ImportResult.UnresolvedNodeSummaries)
	{
		UnresolvedArray.Add(MakeShared<FJsonValueString>(Summary));
	}
	Resp.Result->SetArrayField(TEXT("unresolved"), UnresolvedArray);

	TArray<TSharedPtr<FJsonValue>> WarningArray;
	TArray<TSharedPtr<FJsonValue>> ErrorArray;
	for (const FBlueprintHelperDiagnosticItem& Item : ImportResult.Diagnostics.Items)
	{
		TSharedPtr<FJsonObject> DiagObj = MakeShared<FJsonObject>();
		DiagObj->SetStringField(TEXT("code"), Item.Code);
		DiagObj->SetStringField(TEXT("message"), Item.Message);
		if (!Item.Field.IsEmpty())
		{
			DiagObj->SetStringField(TEXT("field"), Item.Field);
		}
		if (!Item.NodeId.IsEmpty())
		{
			DiagObj->SetStringField(TEXT("node_id"), Item.NodeId);
		}
		if (!Item.PinName.IsEmpty())
		{
			DiagObj->SetStringField(TEXT("pin_name"), Item.PinName);
		}
		if (Item.Severity == EBlueprintHelperDiagnosticSeverity::Error)
		{
			ErrorArray.Add(MakeShared<FJsonValueObject>(DiagObj));
		}
		else
		{
			WarningArray.Add(MakeShared<FJsonValueObject>(DiagObj));
		}
	}
	Resp.Result->SetArrayField(TEXT("warnings"), WarningArray);
	Resp.Result->SetArrayField(TEXT("errors"), ErrorArray);

	return Resp;
}

// ─── import_agent_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleImportAgentGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺失。"));
	}

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Req.Payload.ToSharedRef(), Writer))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::JsonParseFailed,
			TEXT("import_agent_graph payload 无法序列化为 JSON。"));
	}

	FBlueprintHelperAgentImportRequest ImportReq;
	ImportReq.JsonText = JsonText;
	const FBlueprintHelperAgentImportResult ImportResult = AgentImportService.Import(ImportReq);

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId, ImportResult.GetSummaryText());
	Resp.Result = AgentImportResultToJson(ImportResult);
	return Resp;
}

// ─── compile_blueprint ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCompileBlueprint(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperGraphTarget Target;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("target_blueprint"), Target.BlueprintPath);
	}

	FBlueprintHelperCompileResult CompileResult = CompileService.Compile(Target);

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetBoolField(TEXT("compile_success"), CompileResult.bSuccess);
	Resp.Result->SetNumberField(TEXT("blueprint_status"), CompileResult.BlueprintStatus);
	Resp.Result->SetNumberField(TEXT("error_count"), CompileResult.Diagnostics.ErrorCount);
	Resp.Result->SetNumberField(TEXT("warning_count"), CompileResult.Diagnostics.WarningCount);

	TArray<TSharedPtr<FJsonValue>> DiagArray;
	for (const auto& Item : CompileResult.Diagnostics.Items)
	{
		TSharedPtr<FJsonObject> DiagObj = MakeShared<FJsonObject>();
		DiagObj->SetStringField(TEXT("severity"),
			Item.Severity == EBlueprintHelperDiagnosticSeverity::Error ? TEXT("error") :
			Item.Severity == EBlueprintHelperDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
		DiagObj->SetStringField(TEXT("message"), Item.Message);
		if (!Item.NodeName.IsEmpty())
		{
			DiagObj->SetStringField(TEXT("node_name"), Item.NodeName);
		}
		DiagArray.Add(MakeShared<FJsonValueObject>(DiagObj));
	}
	Resp.Result->SetArrayField(TEXT("diagnostics"), DiagArray);

	return Resp;
}

// ═══════════════════════════════════════════════════════════
// Phase 4 — 资产浏览命令
// ═══════════════════════════════════════════════════════════

// ─── open_asset ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleOpenAsset(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString AssetPath;
	if (!TryReadStringField(Req.Payload, TEXT("asset_path"), true, AssetPath, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	FString ErrorMsg;
	const bool bOk = AssetBrowseService.OpenAsset(AssetPath, ErrorMsg);
	if (!bOk)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMsg);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("opened"), AssetPath);
	return Resp;
}

// ─── list_assets ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListAssets(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperListAssetsRequest ListReq;
	if (Req.Payload.IsValid())
	{
		FBlueprintHelperBridgeValidationError ParseError;
		if (!TryReadStringField(Req.Payload, TEXT("path"), false, ListReq.Path, ParseError)
			|| !TryReadStringField(Req.Payload, TEXT("class_filter"), false, ListReq.ClassFilter, ParseError)
			|| !TryReadStringField(Req.Payload, TEXT("name_filter"), false, ListReq.NameFilter, ParseError)
			|| !TryReadBoolOption(Req.Payload, TEXT("recursive"), ListReq.bRecursive, ParseError))
		{
			return ValidationErrorResponse(Req.RequestId, ParseError);
		}
		if (Req.Payload->HasField(TEXT("max_results")))
		{
			double MaxResults = 0.0;
			if (!TryReadNumberField(Req.Payload, TEXT("max_results"), false, MaxResults, ParseError))
			{
				return ValidationErrorResponse(Req.RequestId, ParseError);
			}
			ListReq.MaxResults = static_cast<int32>(MaxResults);
		}
	}

	FBlueprintHelperListAssetsResult ListResult = AssetBrowseService.ListAssets(ListReq);

	if (!ListResult.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ListResult.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetNumberField(TEXT("total_count"), ListResult.TotalCount);
	Resp.Result->SetNumberField(TEXT("returned_count"), ListResult.Assets.Num());

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FBlueprintHelperAssetInfo& Info : ListResult.Assets)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Info.AssetPath);
		Obj->SetStringField(TEXT("name"), Info.AssetName);
		Obj->SetStringField(TEXT("class"), Info.AssetClass);
		if (!Info.ParentClass.IsEmpty())
		{
			Obj->SetStringField(TEXT("parent_class"), Info.ParentClass);
		}
		AssetArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("assets"), AssetArray);

	return Resp;
}

// ─── search_assets ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSearchAssets(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperListAssetsRequest SearchReq;
	if (Req.Payload.IsValid())
	{
		FBlueprintHelperBridgeValidationError ParseError;
		if (!TryReadStringField(Req.Payload, TEXT("path"), false, SearchReq.Path, ParseError)
			|| !TryReadStringField(Req.Payload, TEXT("class_filter"), false, SearchReq.ClassFilter, ParseError)
			|| !TryReadStringField(Req.Payload, TEXT("query"), true, SearchReq.NameFilter, ParseError))
		{
			return ValidationErrorResponse(Req.RequestId, ParseError);
		}
		if (Req.Payload->HasField(TEXT("max_results")))
		{
			double MaxResults = 0.0;
			if (!TryReadNumberField(Req.Payload, TEXT("max_results"), false, MaxResults, ParseError))
			{
				return ValidationErrorResponse(Req.RequestId, ParseError);
			}
			SearchReq.MaxResults = static_cast<int32>(MaxResults);
		}
	}

	if (SearchReq.NameFilter.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 query 字段。"));
	}

	FBlueprintHelperListAssetsResult SearchResult = AssetBrowseService.SearchAssets(SearchReq);

	if (!SearchResult.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			SearchResult.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetNumberField(TEXT("total_count"), SearchResult.TotalCount);
	Resp.Result->SetNumberField(TEXT("returned_count"), SearchResult.Assets.Num());

	TArray<TSharedPtr<FJsonValue>> AssetArray;
	for (const FBlueprintHelperAssetInfo& Info : SearchResult.Assets)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("path"), Info.AssetPath);
		Obj->SetStringField(TEXT("name"), Info.AssetName);
		Obj->SetStringField(TEXT("class"), Info.AssetClass);
		if (!Info.ParentClass.IsEmpty())
		{
			Obj->SetStringField(TEXT("parent_class"), Info.ParentClass);
		}
		AssetArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("assets"), AssetArray);

	return Resp;
}

// ─── save_asset ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSaveAsset(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString AssetPath;
	if (!TryReadStringField(Req.Payload, TEXT("asset_path"), true, AssetPath, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	FBlueprintHelperSaveResult SaveResult = AssetBrowseService.SaveAsset(AssetPath);
	if (!SaveResult.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			SaveResult.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("saved"), AssetPath);
	return Resp;
}

// ─── get_asset_info ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetAssetInfo(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString AssetPath;
	if (!TryReadStringField(Req.Payload, TEXT("asset_path"), true, AssetPath, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	bool bSuccess = false;
	FString ErrorMsg;
	FBlueprintHelperAssetInfo Info = AssetBrowseService.GetAssetInfo(AssetPath, bSuccess, ErrorMsg);

	if (!bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMsg);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("path"), Info.AssetPath);
	Resp.Result->SetStringField(TEXT("name"), Info.AssetName);
	Resp.Result->SetStringField(TEXT("class"), Info.AssetClass);
	if (!Info.ParentClass.IsEmpty())
	{
		Resp.Result->SetStringField(TEXT("parent_class"), Info.ParentClass);
	}
	if (Info.DiskSize >= 0)
	{
		Resp.Result->SetNumberField(TEXT("disk_size"), static_cast<double>(Info.DiskSize));
	}
	return Resp;
}

// ═══════════════════════════════════════════════════════════
// Phase 5 — 蓝图结构查询与操作
// ═══════════════════════════════════════════════════════════

static FBlueprintHelperGraphTarget ParseTargetFromPayload(const TSharedPtr<FJsonObject>& Payload)
{
	FBlueprintHelperGraphTarget Target;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("target_blueprint"), Target.BlueprintPath);
		Payload->TryGetStringField(TEXT("target_graph"), Target.GraphName);
	}
	return Target;
}

// ─── list_graphs ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListGraphs(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FBlueprintHelperListGraphsResult Result = StructureService.ListGraphs(Target);

	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> GraphArray;
	for (const FBlueprintHelperGraphInfo& Info : Result.Graphs)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Info.Name);
		Obj->SetStringField(TEXT("type"), Info.GraphType);
		Obj->SetNumberField(TEXT("node_count"), Info.NodeCount);
		Obj->SetBoolField(TEXT("is_pure"), Info.bIsPure);
		GraphArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("graphs"), GraphArray);
	Resp.Result->SetNumberField(TEXT("count"), Result.Graphs.Num());
	return Resp;
}

// ─── list_variables ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListVariables(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FBlueprintHelperListVariablesResult Result = StructureService.ListVariables(Target);

	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> VarArray;
	for (const FBlueprintHelperVariableInfo& Info : Result.Variables)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Info.Name);
		Obj->SetStringField(TEXT("type_category"), Info.TypeCategory);
		if (!Info.SubCategoryObject.IsEmpty())
		{
			Obj->SetStringField(TEXT("sub_category_object"), Info.SubCategoryObject);
		}
		Obj->SetStringField(TEXT("container_type"), Info.ContainerType);
		if (!Info.DefaultValue.IsEmpty())
		{
			Obj->SetStringField(TEXT("default_value"), Info.DefaultValue);
		}
		if (!Info.Category.IsEmpty())
		{
			Obj->SetStringField(TEXT("category"), Info.Category);
		}
		VarArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("variables"), VarArray);
	Resp.Result->SetNumberField(TEXT("count"), Result.Variables.Num());
	return Resp;
}

// ─── list_event_dispatchers ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListEventDispatchers(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FBlueprintHelperListDispatchersResult Result = StructureService.ListEventDispatchers(Target);

	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> DispArray;
	for (const FBlueprintHelperEventDispatcherInfo& Info : Result.Dispatchers)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Info.Name);

		TArray<TSharedPtr<FJsonValue>> ParamsArr;
		for (const FString& P : Info.Params)
		{
			ParamsArr.Add(MakeShared<FJsonValueString>(P));
		}
		Obj->SetArrayField(TEXT("params"), ParamsArr);
		DispArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("event_dispatchers"), DispArray);
	Resp.Result->SetNumberField(TEXT("count"), Result.Dispatchers.Num());
	return Resp;
}

// ─── add_variable ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddVariable(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString VarName;
	if (!TryReadStringField(Req.Payload, TEXT("name"), true, VarName, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (!Req.Payload.IsValid() || !Req.Payload->HasField(TEXT("name")))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 name 字段。"));
	}

	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FString Error;
	const bool bOk = StructureService.AddVariable(Target, Req.Payload, Error);
	if (!bOk)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Error);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("added_variable"), VarName);
	return Resp;
}

// ─── remove_variable ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveVariable(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString VarName;
	if (!TryReadStringField(Req.Payload, TEXT("name"), true, VarName, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (VarName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 name 字段。"));
	}

	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FString Error;
	const bool bOk = StructureService.RemoveVariable(Target, VarName, Error);
	if (!bOk)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Error);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("removed_variable"), VarName);
	return Resp;
}

// ─── add_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString GraphName;
	if (!TryReadStringField(Req.Payload, TEXT("name"), true, GraphName, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (!Req.Payload.IsValid() || !Req.Payload->HasField(TEXT("name")))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 name 字段。"));
	}

	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FString Error;
	const bool bOk = StructureService.AddGraph(Target, Req.Payload, Error);
	if (!bOk)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Error);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("added_graph"), GraphName);
	return Resp;
}

// ─── remove_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString GraphName;
	if (!TryReadStringField(Req.Payload, TEXT("name"), true, GraphName, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (GraphName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 name 字段。"));
	}

	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FString Error;
	const bool bOk = StructureService.RemoveGraph(Target, GraphName, Error);
	if (!bOk)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Error);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("removed_graph"), GraphName);
	return Resp;
}

// ─── add_event_dispatcher ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddEventDispatcher(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString DispatcherName;
	if (!TryReadStringField(Req.Payload, TEXT("name"), true, DispatcherName, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (!Req.Payload.IsValid() || !Req.Payload->HasField(TEXT("name")))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 name 字段。"));
	}

	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FString Error;
	const bool bOk = StructureService.AddEventDispatcher(Target, Req.Payload, Error);
	if (!bOk)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Error);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("added_dispatcher"), DispatcherName);
	return Resp;
}

// ─── delete_nodes ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleDeleteNodes(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid() || !Req.Payload->HasField(TEXT("node_ids")))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 node_ids 字段。"));
	}

	const TArray<TSharedPtr<FJsonValue>>* NodeIdsArray = nullptr;
	if (!Req.Payload->TryGetArrayField(TEXT("node_ids"), NodeIdsArray) || !NodeIdsArray)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("node_ids 必须是字符串数组。"));
	}

	TArray<FString> NodeIds;
	for (const TSharedPtr<FJsonValue>& Val : *NodeIdsArray)
	{
		FString Id;
		if (Val->TryGetString(Id))
		{
			NodeIds.Add(Id);
		}
	}

	const FBlueprintHelperGraphTarget Target = ParseTargetFromPayload(Req.Payload);
	FString Error;
	int32 DeletedCount = 0;
	const bool bOk = StructureService.DeleteNodes(Target, NodeIds, DeletedCount, Error);
	if (!bOk)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Error);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetNumberField(TEXT("deleted_count"), DeletedCount);
	Resp.Result->SetNumberField(TEXT("requested_count"), NodeIds.Num());
	return Resp;
}

// ═══════════════════════════════════════════════════════════
// Phase 6 — UMG Widget 操作
// ═══════════════════════════════════════════════════════════

static FString GetRequiredStringField(const TSharedPtr<FJsonObject>& Payload, const FString& Field)
{
	FString Value;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(Field, Value);
	}
	return Value;
}

// ─── get_widget_tree ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetWidgetTree(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	FBlueprintHelperWidgetTreeResult Result = WidgetService.GetWidgetTree(AssetPath);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("root_widget"), Result.RootWidgetName);
	Resp.Result->SetNumberField(TEXT("count"), Result.Widgets.Num());

	TArray<TSharedPtr<FJsonValue>> WidgetArray;
	for (const FBlueprintHelperWidgetInfo& Info : Result.Widgets)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Info.Name);
		Obj->SetStringField(TEXT("class"), Info.WidgetClass);
		if (!Info.ParentName.IsEmpty())
		{
			Obj->SetStringField(TEXT("parent"), Info.ParentName);
		}
		if (!Info.SlotClass.IsEmpty())
		{
			Obj->SetStringField(TEXT("slot_class"), Info.SlotClass);
		}
		Obj->SetNumberField(TEXT("child_count"), Info.ChildCount);
		Obj->SetNumberField(TEXT("depth"), Info.Depth);
		WidgetArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("widgets"), WidgetArray);
	return Resp;
}

// ─── add_widget ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddWidget(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString AssetPath;
	FString WidgetClass;
	FString ParentName;
	FString WidgetName;
	if (!TryReadStringField(Req.Payload, TEXT("asset_path"), true, AssetPath, ParseError)
		|| !TryReadStringField(Req.Payload, TEXT("widget_class"), true, WidgetClass, ParseError)
		|| !TryReadStringField(Req.Payload, TEXT("parent_name"), false, ParentName, ParseError)
		|| !TryReadStringField(Req.Payload, TEXT("widget_name"), false, WidgetName, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (!Req.Payload.IsValid() || !Req.Payload->HasField(TEXT("asset_path")))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	if (WidgetClass.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 widget_class 字段。"));
	}

	FBlueprintHelperWidgetMutationResult Result = WidgetService.AddWidget(AssetPath, ParentName, WidgetClass, WidgetName);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("added_widget"), Result.AffectedWidget);
	return Resp;
}

// ─── remove_widget ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveWidget(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString WidgetName = GetRequiredStringField(Req.Payload, TEXT("widget_name"));

	if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 或 widget_name 字段。"));
	}

	FBlueprintHelperWidgetMutationResult Result = WidgetService.RemoveWidget(AssetPath, WidgetName);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("removed_widget"), Result.AffectedWidget);
	return Resp;
}

// ─── move_widget ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleMoveWidget(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString WidgetName = GetRequiredStringField(Req.Payload, TEXT("widget_name"));
	const FString NewParent = GetRequiredStringField(Req.Payload, TEXT("new_parent"));

	if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || NewParent.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path / widget_name / new_parent 字段。"));
	}

	int32 InsertIndex = -1;
	if (Req.Payload->HasField(TEXT("insert_index")))
	{
		FBlueprintHelperBridgeValidationError ParseError;
		double InsertIndexValue = -1.0;
		if (!TryReadNumberField(Req.Payload, TEXT("insert_index"), false, InsertIndexValue, ParseError))
		{
			return ValidationErrorResponse(Req.RequestId, ParseError);
		}
		InsertIndex = static_cast<int32>(InsertIndexValue);
	}

	FBlueprintHelperWidgetMutationResult Result = WidgetService.MoveWidget(AssetPath, WidgetName, NewParent, InsertIndex);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("moved_widget"), Result.AffectedWidget);
	Resp.Result->SetStringField(TEXT("new_parent"), NewParent);
	return Resp;
}

// ─── get_widget_properties ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetWidgetProperties(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString WidgetName = GetRequiredStringField(Req.Payload, TEXT("widget_name"));

	if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 或 widget_name 字段。"));
	}

	FBlueprintHelperWidgetPropertyResult Result = WidgetService.GetWidgetProperties(AssetPath, WidgetName);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetNumberField(TEXT("count"), Result.Properties.Num());

	TArray<TSharedPtr<FJsonValue>> PropArray;
	for (const FBlueprintHelperWidgetPropertyInfo& Info : Result.Properties)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Info.Name);
		Obj->SetStringField(TEXT("type"), Info.TypeName);
		Obj->SetStringField(TEXT("value"), Info.Value);
		Obj->SetStringField(TEXT("flags"), Info.Flags);
		PropArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("properties"), PropArray);
	return Resp;
}

// ─── set_widget_property ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetWidgetProperty(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString WidgetName = GetRequiredStringField(Req.Payload, TEXT("widget_name"));
	const FString PropertyName = GetRequiredStringField(Req.Payload, TEXT("property_name"));

	if (AssetPath.IsEmpty() || WidgetName.IsEmpty() || PropertyName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path / widget_name / property_name 字段。"));
	}

	// value 可以为空字符串（合法值），所以不检查 IsEmpty
	const FString Value = GetRequiredStringField(Req.Payload, TEXT("value"));

	FBlueprintHelperWidgetMutationResult Result = WidgetService.SetWidgetProperty(AssetPath, WidgetName, PropertyName, Value);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("widget"), Result.AffectedWidget);
	Resp.Result->SetStringField(TEXT("property"), PropertyName);
	Resp.Result->SetStringField(TEXT("new_value"), Value);
	return Resp;
}

// ═══════════════════════════════════════════════════════════
// Phase 7 — DataAsset & DataTable 操作
// ═══════════════════════════════════════════════════════════

// ─── get_object_properties ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetObjectProperties(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	FBlueprintHelperObjectPropertiesResult Result = PropertyReflectionService.GetObjectProperties(AssetPath);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("class_name"), Result.ClassName);
	Resp.Result->SetStringField(TEXT("asset_path"), Result.AssetPath);
	Resp.Result->SetNumberField(TEXT("count"), Result.Properties.Num());

	TArray<TSharedPtr<FJsonValue>> PropArray;
	for (const FBlueprintHelperObjectPropertyInfo& Info : Result.Properties)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Info.Name);
		Obj->SetStringField(TEXT("type"), Info.TypeName);
		Obj->SetStringField(TEXT("value"), Info.Value);
		Obj->SetStringField(TEXT("category"), Info.Category);
		Obj->SetStringField(TEXT("flags"), Info.Flags);
		PropArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("properties"), PropArray);
	return Resp;
}

// ─── set_object_property ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetObjectProperty(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString PropertyName = GetRequiredStringField(Req.Payload, TEXT("property_name"));

	if (AssetPath.IsEmpty() || PropertyName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path / property_name 字段。"));
	}

	const FString Value = GetRequiredStringField(Req.Payload, TEXT("value"));

	FBlueprintHelperSetPropertyResult Result = PropertyReflectionService.SetObjectProperty(AssetPath, PropertyName, Value);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("property"), Result.PropertyName);
	Resp.Result->SetStringField(TEXT("old_value"), Result.OldValue);
	Resp.Result->SetStringField(TEXT("new_value"), Result.NewValue);
	return Resp;
}

// ─── get_datatable_rows ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetDataTableRows(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	// 可选的行名过滤
	TArray<FString> FilterRowNames;
	if (Req.Payload.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* RowNamesArray = nullptr;
		if (Req.Payload->TryGetArrayField(TEXT("row_names"), RowNamesArray) && RowNamesArray)
		{
			for (const auto& Val : *RowNamesArray)
			{
				FString S;
				if (Val->TryGetString(S))
				{
					FilterRowNames.Add(MoveTemp(S));
				}
			}
		}
	}

	FBlueprintHelperDataTableRowsResult Result = DataTableService.GetDataTableRows(AssetPath, FilterRowNames);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("row_struct"), Result.RowStructName);
	Resp.Result->SetNumberField(TEXT("row_count"), Result.Rows.Num());

	// 列信息
	TArray<TSharedPtr<FJsonValue>> ColArray;
	for (const FBlueprintHelperDataTableColumnInfo& Col : Result.Columns)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Col.Name);
		Obj->SetStringField(TEXT("type"), Col.TypeName);
		ColArray.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Resp.Result->SetArrayField(TEXT("columns"), ColArray);

	// 行数据
	TArray<TSharedPtr<FJsonValue>> RowArray;
	for (const FBlueprintHelperDataTableRowInfo& Row : Result.Rows)
	{
		TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
		RowObj->SetStringField(TEXT("row_name"), Row.RowName.ToString());

		TSharedPtr<FJsonObject> FieldsObj = MakeShared<FJsonObject>();
		for (const auto& Pair : Row.Fields)
		{
			FieldsObj->SetStringField(Pair.Key, Pair.Value);
		}
		RowObj->SetObjectField(TEXT("fields"), FieldsObj);
		RowArray.Add(MakeShared<FJsonValueObject>(RowObj));
	}
	Resp.Result->SetArrayField(TEXT("rows"), RowArray);
	return Resp;
}

// ─── add_datatable_row ───

static TMap<FString, FString> ParseFieldsFromPayload(const TSharedPtr<FJsonObject>& Payload)
{
	TMap<FString, FString> Fields;
	if (!Payload.IsValid()) return Fields;

	const TSharedPtr<FJsonObject>* FieldsObj = nullptr;
	if (Payload->TryGetObjectField(TEXT("fields"), FieldsObj) && FieldsObj && FieldsObj->IsValid())
	{
		for (const auto& Pair : (*FieldsObj)->Values)
		{
			FString ValueStr;
			if (Pair.Value->TryGetString(ValueStr))
			{
				Fields.Add(Pair.Key, MoveTemp(ValueStr));
			}
		}
	}
	return Fields;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddDataTableRow(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString RowName = GetRequiredStringField(Req.Payload, TEXT("row_name"));

	if (AssetPath.IsEmpty() || RowName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path / row_name 字段。"));
	}

	TMap<FString, FString> Fields = ParseFieldsFromPayload(Req.Payload);

	FBlueprintHelperDataTableMutationResult Result = DataTableService.AddDataTableRow(AssetPath, RowName, Fields);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("row_name"), Result.AffectedRow.ToString());
	return Resp;
}

// ─── update_datatable_row ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleUpdateDataTableRow(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString RowName = GetRequiredStringField(Req.Payload, TEXT("row_name"));

	if (AssetPath.IsEmpty() || RowName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path / row_name 字段。"));
	}

	TMap<FString, FString> Fields = ParseFieldsFromPayload(Req.Payload);
	if (Fields.Num() == 0)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 中 fields 对象为空，至少需要一个字段。"));
	}

	FBlueprintHelperDataTableMutationResult Result = DataTableService.UpdateDataTableRow(AssetPath, RowName, Fields);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("row_name"), Result.AffectedRow.ToString());
	return Resp;
}

// ─── delete_datatable_row ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleDeleteDataTableRow(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	const FString RowName = GetRequiredStringField(Req.Payload, TEXT("row_name"));

	if (AssetPath.IsEmpty() || RowName.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path / row_name 字段。"));
	}

	FBlueprintHelperDataTableMutationResult Result = DataTableService.DeleteDataTableRow(AssetPath, RowName);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("row_name"), Result.AffectedRow.ToString());
	return Resp;
}

// ═══════════════════════════════════════════════════════════
// Phase 8: 编辑器命令
// ═══════════════════════════════════════════════════════════

// ─── undo ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleUndo(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperCommandResult Result = EditorCommandService.Undo();
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("message"), Result.Message);
	return Resp;
}

// ─── redo ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRedo(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperCommandResult Result = EditorCommandService.Redo();
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("message"), Result.Message);
	return Resp;
}

// ─── play_in_editor ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandlePlayInEditor(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperCommandResult Result = EditorCommandService.PlayInEditor();
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("message"), Result.Message);
	return Resp;
}

// ─── stop_pie ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleStopPIE(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperCommandResult Result = EditorCommandService.StopPIE();
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("message"), Result.Message);
	return Resp;
}

// ─── create_blueprint ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCreateBlueprint(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString AssetPath = GetRequiredStringField(Req.Payload, TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	FString ParentClass = TEXT("Actor");
	if (Req.Payload.IsValid() && Req.Payload->HasField(TEXT("parent_class")))
	{
		FBlueprintHelperBridgeValidationError ParseError;
		if (!TryReadStringField(Req.Payload, TEXT("parent_class"), false, ParentClass, ParseError))
		{
			return ValidationErrorResponse(Req.RequestId, ParseError);
		}
	}

	FBlueprintHelperCreateBlueprintResult Result = EditorCommandService.CreateBlueprint(AssetPath, ParentClass);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("asset_path"), Result.AssetPath);
	Resp.Result->SetStringField(TEXT("blueprint_name"), Result.BlueprintName);
	Resp.Result->SetStringField(TEXT("parent_class"), Result.ParentClassName);
	return Resp;
}

// ─── exec_console_command ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExecConsoleCommand(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FString Command = GetRequiredStringField(Req.Payload, TEXT("command"));
	if (Command.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 command 字段。"));
	}

	FBlueprintHelperCommandResult Result = EditorCommandService.ExecConsoleCommand(Command);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("output"), Result.Message);
	return Resp;
}

// ─── close_editor ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCloseEditor(
	const FBlueprintHelperBridgeRequest& Req) const
{
	bool bSaveAll = true;
	if (Req.Payload.IsValid() && Req.Payload->HasField(TEXT("save_all")))
	{
		FBlueprintHelperBridgeValidationError ParseError;
		if (!TryReadBoolField(Req.Payload, TEXT("save_all"), false, bSaveAll, ParseError))
		{
			return ValidationErrorResponse(Req.RequestId, ParseError);
		}
	}

	FBlueprintHelperCommandResult Result = EditorCommandService.CloseEditor(bSaveAll);
	if (!Result.bSuccess)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.ErrorMessage);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("message"), Result.Message);
	return Resp;
}
