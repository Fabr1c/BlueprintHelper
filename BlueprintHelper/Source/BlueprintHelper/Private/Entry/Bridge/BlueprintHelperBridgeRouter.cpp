// BlueprintHelper Bridge Layer 。命令路由实现

#include "Entry/Bridge/BlueprintHelperBridgeRouter.h"
#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "Entry/BlueprintHelper.h"
#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"
#include "Shared/Services/BlueprintHelperImportService.h"
#include "Shared/Services/BlueprintHelperAgentImportService.h"
#include "Shared/Services/BlueprintHelperExportService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicProcessor.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/Debug/BlueprintHelperValidationService.h"
#include "Systems/Debug/BlueprintHelperContextService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/Debug/BlueprintHelperEditorCommandService.h"
#include "Systems/Debug/BlueprintHelperRuntimeProfileService.h"
#include "Shared/Debug/BlueprintHelperRuntimeProfileTypes.h"
#include "Systems/Debug/BlueprintHelperDiagnosticsService.h"
#include "Shared/Debug/BlueprintHelperDiagnosticsTypes.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Shared/BlueprintHelperDependencyAnalysisTypes.h"
#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.h"
#include "Shared/BlueprintHelperLogicMdTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Shared/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Shared/BlueprintClassSettings/BlueprintHelperClassSettingsTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Shared/GraphWrite/BlueprintHelperMergeGraphTypes.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Shared/Debug/BlueprintHelperCompileAssetTypes.h"
#include "Shared/Debug/BlueprintHelperSaveAssetTypes.h"
#include "Systems/Transactions/BlueprintHelperTransactionQueryService.h"
#include "Shared/Transactions/BlueprintHelperTransactionQueryTypes.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Shared/BlueprintVariables/BlueprintHelperBlueprintVariableTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperBridgeRouterLocalUtils
{
public:
	static TSharedRef<FJsonObject> MakeLogicStatsObject(const FBlueprintHelperLogicResult& Result)
	{
		TSharedRef<FJsonObject> StatsObject = MakeShared<FJsonObject>();
		StatsObject->SetNumberField(TEXT("nodes"), Result.NodeCount);
		StatsObject->SetNumberField(TEXT("exec_links"), Result.ExecLinkCount);
		StatsObject->SetNumberField(TEXT("data_links"), Result.DataLinkCount);
		StatsObject->SetNumberField(TEXT("entry_points"), Result.EntryPointCount);
		StatsObject->SetNumberField(TEXT("orphans"), Result.OrphanNodeCount);
		return StatsObject;
	}

	static FString JsonValueTypeToString(const TSharedPtr<FJsonValue>& Value)
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

	static FBlueprintHelperBridgeValidationError MakePayloadFieldError(
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

	static bool TryReadStringField(
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

	static bool TryReadStringOption(
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

	static bool TryReadBoolField(
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

	static bool TryReadNumberField(
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

	static bool TryReadBoolOption(
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

	static EBlueprintHelperBridgeError ValidationCodeToBridgeError(const FString& Code)
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

	static FBlueprintHelperBridgeResponse ValidationErrorResponse(
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

	static EBlueprintHelperBridgeError DiagnosticSetToBridgeError(const FBlueprintHelperDiagnosticSet& Diagnostics)
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

	static TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonText)
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			return nullptr;
		}
		return JsonObject;
	}

	static FString AgentImportSeverityToString(EBlueprintHelperAgentImportDiagnosticSeverity Severity)
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

	static TSharedRef<FJsonObject> AgentImportResultToJson(const FBlueprintHelperAgentImportResult& Result)
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

	static TSharedRef<FJsonObject> MakeRawJsonStatsObject(const TSharedPtr<FJsonObject>& RawJsonObject)
	{
		TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
		if (!RawJsonObject.IsValid())
		{
			Stats->SetNumberField(TEXT("nodes"), 0);
			Stats->SetNumberField(TEXT("links"), 0);
			return Stats;
		}

		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		if (RawJsonObject->TryGetArrayField(TEXT("nodes"), NodesArray) && NodesArray)
		{
			Stats->SetNumberField(TEXT("nodes"), NodesArray->Num());
		}
		else
		{
			Stats->SetNumberField(TEXT("nodes"), 0);
		}

		const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
		if (RawJsonObject->TryGetArrayField(TEXT("links"), LinksArray) && LinksArray)
		{
			Stats->SetNumberField(TEXT("links"), LinksArray->Num());
		}
		else
		{
			Stats->SetNumberField(TEXT("links"), 0);
		}

		return Stats;
	}

	// ─── reference context helpers ───
	static FBlueprintHelperToolError MakeReferenceContextError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = FString())
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	static TSharedRef<FJsonObject> MakeReferenceContextTargetJson(const TSharedPtr<FJsonObject>& Payload)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();

		FString Value;
		if (Payload.IsValid() && Payload->TryGetStringField(TEXT("target_type"), Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TEXT("target_type"), Value.ToLower());
		}
		else
		{
			Target->SetStringField(TEXT("target_type"), TEXT("asset"));
		}
		if (Payload.IsValid() && Payload->TryGetStringField(TEXT("asset_path"), Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TEXT("asset_path"), Value);
		}
		if (Payload.IsValid() && Payload->TryGetStringField(TEXT("graph_name"), Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TEXT("graph_name"), Value);
		}
		if (Payload.IsValid() && Payload->TryGetStringField(TEXT("target_name"), Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TEXT("member_name"), Value);
		}
		if (Payload.IsValid() && Payload->TryGetStringField(TEXT("block_id"), Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TEXT("block_id"), Value);
		}
		if (Payload.IsValid() && Payload->TryGetStringField(TEXT("widget_name"), Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TEXT("widget_name"), Value);
		}
		if (Payload.IsValid() && Payload->TryGetStringField(TEXT("row_name"), Value) && !Value.IsEmpty())
		{
			Target->SetStringField(TEXT("row_name"), Value);
		}
		return Target;
	}

	static FBlueprintHelperDependencyAnalysisTarget ReadReferenceContextTarget(const TSharedPtr<FJsonObject>& Payload)
	{
		FBlueprintHelperDependencyAnalysisTarget Target;
		if (!Payload.IsValid())
		{
			return Target;
		}
		Payload->TryGetStringField(TEXT("asset_path"), Target.AssetPath);
		Payload->TryGetStringField(TEXT("target_type"), Target.TargetType);
		Payload->TryGetStringField(TEXT("target_name"), Target.TargetName);
		Payload->TryGetStringField(TEXT("block_id"), Target.BlockId);
		Payload->TryGetStringField(TEXT("graph_name"), Target.GraphName);
		Payload->TryGetStringField(TEXT("declaring_class_path"), Target.DeclaringClassPath);
		Payload->TryGetStringField(TEXT("row_name"), Target.RowName);
		Payload->TryGetStringField(TEXT("widget_name"), Target.WidgetName);
		Payload->TryGetStringField(TEXT("interface_path"), Target.InterfacePath);
		if (Target.TargetType.IsEmpty())
		{
			Target.TargetType = TEXT("asset");
		}
		return Target;
	}

	static FBlueprintHelperToolResultBase MakeReferenceContextFailureResult(
		const TSharedPtr<FJsonObject>& Payload,
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = FString())
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("read_reference_context"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeReferenceContextError(Code, Stage, Message, Field));
		return Result;
	}

};

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
	const FBlueprintHelperEditorCommandService& InEditorCommand,
	const FBlueprintHelperRuntimeProfileService& InRuntimeProfile,
	const FBlueprintHelperDiagnosticsService& InDiagnostics,
	const FBlueprintHelperDebugEntryService& InDebugEntryService,
	const FBlueprintHelperLogicMdReadService& InLogicMdRead,
	const FBlueprintHelperLogicJsonReadService& InLogicJsonRead,
	const FBlueprintHelperAssetFactoryService& InAssetFactory,
	const FBlueprintHelperComponentService& InComponentService,
	const FBlueprintHelperClassSettingsService& InClassSettings,
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
	const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
	const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
	const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService,
	const FBlueprintHelperCompileAssetService& InCompileAssetService,
	const FBlueprintHelperTransactionQueryService& InTransactionQueryService,
	const FBlueprintHelperBlueprintVariableService& InVariableService,
	const FBlueprintHelperReviewStoreService& InReviewStoreService)
	: ImportService(InImport)
	, AgentImportService(InAgentImport)
	, ExportService(InExport)
	, CompileService(InCompile)
	, ValidationService(InValidation)
	, ContextService(InContext)
	, AssetBrowseService(InAssetBrowse)
	, StructureService(InStructure)
	, UMGWidgetRoutes(InWidget)
	, ObjectPropertyRoutes(InPropertyReflection)
	, DataTableRoutes(InDataTable)
	, EditorCommandService(InEditorCommand)
	, RuntimeProfileService(InRuntimeProfile)
	, DiagnosticsService(InDiagnostics)
	, DebugEntryService(InDebugEntryService)
	, LogicMdReadService(InLogicMdRead)
	, LogicJsonReadService(InLogicJsonRead)
	, AssetFactoryRoutes(InAssetFactory)
	, ComponentRoutes(InComponentService)
	, ClassSettingsRoutes(InClassSettings)
	, GraphWriteRoutes(
		InAppendGraphService,
		InReplaceGraphService,
		InPatchGraphService,
		InMergeGraphService)
	, VariableService(InVariableService)
	, BlueprintVariablesRoutes(InVariableService)
	, TaskRuntimeService(
		InAppendGraphService,
		InReplaceGraphService,
		InPatchGraphService,
		InMergeGraphService,
		InVariableService,
		InStructure,
		InAssetFactory,
		InComponentService,
		InClassSettings,
		InWidget,
		InDataTable,
		InPropertyReflection,
		InCompileAssetService,
		InAssetBrowse,
		&InDebugEntryService)
	, CompileAssetService(InCompileAssetService)
	, TransactionQueryService(InTransactionQueryService)
	, ReviewStoreService(InReviewStoreService)
{
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	return HandleRequestWithPlan(Request, FBlueprintHelperBridgeRoutePlanner::BuildPlan(Request.Command));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRequestWithPlan(
	const FBlueprintHelperBridgeRequest& Request,
	const FBlueprintHelperBridgeRoutePlan& RoutePlan) const
{
	FBlueprintHelperBridgeValidationError ValidationError;
	if (!FBlueprintHelperRequestValidator::ValidatePayloadForCommand(Request.Command, Request.Payload, ValidationError))
	{
		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Request.RequestId, ValidationError);
		if (Request.Command == TEXT("read_reference_context"))
		{
			const FBlueprintHelperToolResultBase Result = FBlueprintHelperBridgeRouterLocalUtils::MakeReferenceContextFailureResult(
				Request.Payload,
				TEXT("invalid_request"),
				EBlueprintHelperToolStage::ParseInput,
				ValidationError.Message,
				ValidationError.Field);
			Resp.Result = Result.ToJson();
		}
		return Resp;
	}
	if (!FBlueprintHelperRequestValidator::ValidateAuthorization(Request, ValidationError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Request.RequestId, ValidationError);
	}

	if (!RoutePlan.bKnownCommand)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Request.RequestId,
			EBlueprintHelperBridgeError::UnknownCommand,
			FString::Printf(TEXT("鏈煡鍛戒护: %s"), *Request.Command));
	}

#define BLUEPRINTHELPER_ROUTE(CommandText, ClusterValue, Handler) \
	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::ClusterValue && Request.Command == TEXT(CommandText)) \
	{ \
		return Handler(Request); \
	}

	BLUEPRINTHELPER_ROUTE("get_rule_markdown", Core, HandleGetRuleMarkdown)
	BLUEPRINTHELPER_ROUTE("get_editor_context", Core, HandleGetEditorContext)
	BLUEPRINTHELPER_ROUTE("request_write_session", Core, HandleRequestWriteSession)

	BLUEPRINTHELPER_ROUTE("get_runtime_profile", Debug, HandleGetRuntimeProfile)
	BLUEPRINTHELPER_ROUTE("diagnostics_runtime", Debug, HandleDiagnosticsRuntime)
	BLUEPRINTHELPER_ROUTE("get_debug_case", Debug, HandleGetDebugCase)
	BLUEPRINTHELPER_ROUTE("list_debug_cases", Debug, HandleListDebugCases)
	BLUEPRINTHELPER_ROUTE("export_debug_bundle", Debug, HandleExportDebugBundle)
	BLUEPRINTHELPER_ROUTE("compile_blueprint", Debug, HandleCompileBlueprint)
	BLUEPRINTHELPER_ROUTE("compile_blueprint_asset", Debug, HandleCompileBlueprintAsset)
	BLUEPRINTHELPER_ROUTE("query_review_records", Review, HandleQueryReviewRecords)
	BLUEPRINTHELPER_ROUTE("apply_review_action", Review, HandleApplyReviewAction)

	BLUEPRINTHELPER_ROUTE("read_reference_context", SharedServices, HandleReadReferenceContext)
	BLUEPRINTHELPER_ROUTE("read_function_chain_context", SharedServices, HandleReadFunctionChainContext)
	BLUEPRINTHELPER_ROUTE("read_blueprint_logic_md", SharedServices, HandleReadBlueprintLogicMd)
	BLUEPRINTHELPER_ROUTE("read_blueprint_logic_json", SharedServices, HandleReadBlueprintLogicJson)
	BLUEPRINTHELPER_ROUTE("validate_json", SharedServices, HandleValidateJson)
	BLUEPRINTHELPER_ROUTE("export_to_json", SharedServices, HandleExportToJson)
	BLUEPRINTHELPER_ROUTE("export_logic", SharedServices, HandleExportLogic)
	BLUEPRINTHELPER_ROUTE("import_json", SharedServices, HandleImportJson)
	BLUEPRINTHELPER_ROUTE("import_agent_graph", SharedServices, HandleImportAgentGraph)

	BLUEPRINTHELPER_ROUTE("open_asset", AssetBrowser, HandleOpenAsset)
	BLUEPRINTHELPER_ROUTE("list_assets", AssetBrowser, HandleListAssets)
	BLUEPRINTHELPER_ROUTE("search_assets", AssetBrowser, HandleSearchAssets)
	BLUEPRINTHELPER_ROUTE("save_asset", AssetBrowser, HandleSaveAsset)
	BLUEPRINTHELPER_ROUTE("get_asset_info", AssetBrowser, HandleGetAssetInfo)

	BLUEPRINTHELPER_ROUTE("list_graphs", BlueprintStructure, HandleListGraphs)
	BLUEPRINTHELPER_ROUTE("list_variables", BlueprintStructure, HandleListVariables)
	BLUEPRINTHELPER_ROUTE("list_event_dispatchers", BlueprintStructure, HandleListEventDispatchers)
	BLUEPRINTHELPER_ROUTE("add_variable", BlueprintStructure, HandleAddVariable)
	BLUEPRINTHELPER_ROUTE("remove_variable", BlueprintStructure, HandleRemoveVariable)
	BLUEPRINTHELPER_ROUTE("add_graph", BlueprintStructure, HandleAddGraph)
	BLUEPRINTHELPER_ROUTE("remove_graph", BlueprintStructure, HandleRemoveGraph)
	BLUEPRINTHELPER_ROUTE("add_event_dispatcher", BlueprintStructure, HandleAddEventDispatcher)
	BLUEPRINTHELPER_ROUTE("delete_nodes", BlueprintStructure, HandleDeleteNodes)

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::BlueprintVariables &&
		FBlueprintHelperBlueprintVariablesBridgeRoutes::IsBlueprintVariablesCommand(Request.Command))
	{
		return BlueprintVariablesRoutes.HandleRequest(Request);
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::UMGWidget &&
		FBlueprintHelperUMGWidgetBridgeRoutes::IsUMGWidgetCommand(Request.Command))
	{
		return UMGWidgetRoutes.HandleRequest(Request);
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::DataTable &&
		FBlueprintHelperDataTableBridgeRoutes::IsDataTableCommand(Request.Command))
	{
		return DataTableRoutes.HandleRequest(Request);
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::ObjectProperty &&
		FBlueprintHelperObjectPropertyBridgeRoutes::IsObjectPropertyCommand(Request.Command))
	{
		return ObjectPropertyRoutes.HandleRequest(Request);
	}

	BLUEPRINTHELPER_ROUTE("undo", EditorCommand, HandleUndo)
	BLUEPRINTHELPER_ROUTE("redo", EditorCommand, HandleRedo)
	BLUEPRINTHELPER_ROUTE("play_in_editor", EditorCommand, HandlePlayInEditor)
	BLUEPRINTHELPER_ROUTE("stop_pie", EditorCommand, HandleStopPIE)
	BLUEPRINTHELPER_ROUTE("create_blueprint", EditorCommand, HandleCreateBlueprint)
	BLUEPRINTHELPER_ROUTE("exec_console_command", EditorCommand, HandleExecConsoleCommand)
	BLUEPRINTHELPER_ROUTE("close_editor", EditorCommand, HandleCloseEditor)

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::AssetFactory &&
		FBlueprintHelperAssetFactoryBridgeRoutes::IsAssetFactoryCommand(Request.Command))
	{
		return AssetFactoryRoutes.HandleRequest(Request);
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::Component &&
		FBlueprintHelperComponentBridgeRoutes::IsComponentCommand(Request.Command))
	{
		return ComponentRoutes.HandleRequest(Request);
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::ClassSettings &&
		FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(Request.Command))
	{
		return ClassSettingsRoutes.HandleRequest(Request);
	}

	BLUEPRINTHELPER_ROUTE("preview_task_plan", TaskRuntime, HandlePreviewTaskPlan)
	BLUEPRINTHELPER_ROUTE("execute_task_plan", TaskRuntime, HandleExecuteTaskPlan)
	BLUEPRINTHELPER_ROUTE("get_task_run_journal", TaskRuntime, HandleGetTaskRunJournal)

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::GraphWrite &&
		FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(Request.Command))
	{
		return GraphWriteRoutes.HandleRequest(Request);
	}

	BLUEPRINTHELPER_ROUTE("list_blueprint_helper_transactions", Transactions, HandleListTransactions)
	BLUEPRINTHELPER_ROUTE("read_blueprint_helper_transaction", Transactions, HandleReadTransaction)

#undef BLUEPRINTHELPER_ROUTE

	return FBlueprintHelperBridgeResponse::Error(
		Request.RequestId,
		EBlueprintHelperBridgeError::UnknownCommand,
		FString::Printf(TEXT("RoutePlan cluster %s did not handle command: %s"),
			FBlueprintHelperBridgeRoutePlanner::GetClusterName(RoutePlan.Cluster),
			*Request.Command));
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

// ─── get_runtime_profile ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRequestWriteSession(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperWriteSessionRequest SessionRequest;
	Req.Payload->TryGetStringField(TEXT("reason"), SessionRequest.Reason);
	Req.Payload->TryGetStringField(TEXT("scope"), SessionRequest.Scope);

	double TtlSeconds = 0.0;
	if (Req.Payload->TryGetNumberField(TEXT("ttl_seconds"), TtlSeconds))
	{
		SessionRequest.TtlSeconds = FMath::RoundToInt(TtlSeconds);
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetPathValues = nullptr;
	if (Req.Payload->TryGetArrayField(TEXT("asset_paths"), AssetPathValues) && AssetPathValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *AssetPathValues)
		{
			FString AssetPath;
			if (Value.IsValid() && Value->TryGetString(AssetPath))
			{
				SessionRequest.AssetPaths.Add(AssetPath);
			}
		}
	}

	FString Error;
	const TOptional<FBlueprintHelperWriteSessionGrant> Grant =
		FBlueprintHelperWriteAuthorizationService::Get().RequestSession(SessionRequest, Error);
	if (!Grant.IsSet())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::Unauthorized,
			Error.IsEmpty() ? TEXT("Write session request was denied.") : Error);
	}

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetObjectField(TEXT("write_session"), Grant.GetValue().ToJson());
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetRuntimeProfile(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperRuntimeProfileData ProfileData = RuntimeProfileService.GetRuntimeProfile();

	// 使用 ToolResultBuilder 构建标准化的返回。
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("get_runtime_profile"), TraceId);
	Result.Data = ProfileData.ToJson();

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── diagnostics_runtime ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleDiagnosticsRuntime(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperDiagnosticsData DiagnosticsData = DiagnosticsService.RunRuntimeDiagnostics();

	// 使用 ToolResultBuilder 构建标准化的返回。
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("diagnostics_runtime"), TraceId);
	Result.Data = DiagnosticsData.ToJson();

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── read_blueprint_logic_md ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetDebugCase(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperToolResultBase Result = DebugEntryService.GetDebugCaseSummaryResult(Req.Payload);
	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("get_debug_case failed."));
	Resp.Result = Result.ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListDebugCases(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperToolResultBase Result = DebugEntryService.GetDebugCaseListResult(Req.Payload);
	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("list_debug_cases failed."));
	Resp.Result = Result.ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExportDebugBundle(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperToolResultBase Result = DebugEntryService.ExportDebugBundleSummaryResult(Req.Payload);
	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("export_debug_bundle failed."));
	Resp.Result = Result.ToJson();
	return Resp;
}

// --- read_reference_context ---

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadReferenceContext(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> Payload = Req.Payload;
	const FBlueprintHelperDependencyAnalysisTarget Target = FBlueprintHelperBridgeRouterLocalUtils::ReadReferenceContextTarget(Payload);

	FString SearchScope = TEXT("project");
	FString ResolutionPolicy = TEXT("ue_then_name");
	FString Detail = TEXT("samples");
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("search_scope"), SearchScope);
		Payload->TryGetStringField(TEXT("resolution_policy"), ResolutionPolicy);
		Payload->TryGetStringField(TEXT("detail"), Detail);
	}

	FBlueprintHelperBridgeValidationError ValidationError;
	double MaxResults = 50.0;
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadNumberField(Payload, TEXT("max_results"), false, MaxResults, ValidationError))
	{
		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			ValidationError.Message);
		Resp.Result = FBlueprintHelperBridgeRouterLocalUtils::MakeReferenceContextFailureResult(
			Payload,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			ValidationError.Message,
			ValidationError.Field).ToJson();
		return Resp;
	}

	FBlueprintHelperDependencyAnalysisOptions Options;
	Options.MaxResultCount = FMath::RoundToInt(MaxResults);
	Options.SearchScope = SearchScope;
	Options.ResolutionPolicy = ResolutionPolicy;
	Options.Detail = Detail;

	FBlueprintHelperReferenceContextPack ContextPack;
	FString ErrorCode;
	FString ErrorMessage;
	if (!DependencyAnalysisService.TryBuildReferenceContext(
		Target,
		Options,
		ContextPack,
		ErrorCode,
		ErrorMessage))
	{
		const EBlueprintHelperBridgeError BridgeError =
			ErrorCode == TEXT("asset_not_found")
				? EBlueprintHelperBridgeError::AssetNotFound
				: EBlueprintHelperBridgeError::ExecutionFailed;
		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			BridgeError,
			ErrorMessage);
		Resp.Result = FBlueprintHelperBridgeRouterLocalUtils::MakeReferenceContextFailureResult(
			Payload,
			ErrorCode.IsEmpty() ? TEXT("reference_context_failed") : ErrorCode,
			EBlueprintHelperToolStage::ResolveTarget,
			ErrorMessage,
			TEXT("payload.asset_path")).ToJson();
		return Resp;
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_reference_context"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Data = ContextPack.ToJson();

	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// --- read_function_chain_context ---

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadFunctionChainContext(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> Payload = Req.Payload;

	FBlueprintHelperFunctionChainContextRequest Request;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		Payload->TryGetStringField(TEXT("target_type"), Request.TargetType);
		Payload->TryGetStringField(TEXT("target_name"), Request.TargetName);
		Payload->TryGetStringField(TEXT("graph_name"), Request.GraphName);

		double MaxDepth = Request.MaxDepth;
		if (Payload->TryGetNumberField(TEXT("max_depth"), MaxDepth))
		{
			Request.MaxDepth = FMath::Clamp(FMath::RoundToInt(MaxDepth), 0, 12);
		}
		Payload->TryGetBoolField(TEXT("include_data_dependencies"), Request.bIncludeDataDependencies);
		Payload->TryGetBoolField(TEXT("expand_cross_asset"), Request.bExpandCrossAsset);
	}

	FBlueprintHelperFunctionChainContextPack ContextPack;
	FString ErrorCode;
	FString ErrorMessage;
	if (!FunctionChainContextService.TryBuildFunctionChainContext(Request, ContextPack, ErrorCode, ErrorMessage))
	{
		FBlueprintHelperToolError Error;
		Error.Code = ErrorCode.IsEmpty() ? TEXT("function_chain_context_failed") : ErrorCode;
		Error.Stage = Error.Code == TEXT("asset_not_found") || Error.Code == TEXT("target_entry_not_found")
			? EBlueprintHelperToolStage::ResolveTarget
			: EBlueprintHelperToolStage::ParseInput;
		Error.Message = ErrorMessage.IsEmpty() ? TEXT("read_function_chain_context failed.") : ErrorMessage;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Error.Code == TEXT("asset_path_required") || Error.Code == TEXT("asset_not_found")
			? TEXT("payload.asset_path")
			: TEXT("payload.target_name");

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("read_function_chain_context"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);

		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			Error.Code == TEXT("asset_not_found")
				? EBlueprintHelperBridgeError::AssetNotFound
				: EBlueprintHelperBridgeError::ExecutionFailed,
			Error.Message);
		Resp.Result = Result.ToJson();
		return Resp;
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_function_chain_context"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Data = ContextPack.ToJson();

	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// --- read_blueprint_logic_md ---

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadBlueprintLogicMd(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> Payload = Req.Payload;

	// 构建 TargetRef
	FBlueprintHelperTargetRef Target;
	Target.TargetType = EBlueprintHelperTargetType::Graph; // 默认

	if (Payload.IsValid())
	{
		FString AssetPath;
		if (Payload->TryGetStringField(TEXT("asset_path"), AssetPath))
		{
			Target.AssetPath = AssetPath;
		}

		FString GraphName;
		if (Payload->TryGetStringField(TEXT("graph"), GraphName))
		{
			Target.Graph = GraphName;
		}

		FString FunctionName;
		if (Payload->TryGetStringField(TEXT("function"), FunctionName))
		{
			Target.Function = FunctionName;
			Target.TargetType = EBlueprintHelperTargetType::Function;
		}

		FString EventName;
		if (Payload->TryGetStringField(TEXT("event"), EventName))
		{
			Target.Event = EventName;
			Target.TargetType = EBlueprintHelperTargetType::Event;
		}

		FString BlockId;
		if (Payload->TryGetStringField(TEXT("block_id"), BlockId))
		{
			Target.BlockId = BlockId;
			Target.TargetType = EBlueprintHelperTargetType::Block;
		}

		// scope override
		FString ScopeStr;
		if (Payload->TryGetStringField(TEXT("scope"), ScopeStr))
		{
			if (ScopeStr.Equals(TEXT("blueprint"), ESearchCase::IgnoreCase))
			{
				Target.TargetType = EBlueprintHelperTargetType::Blueprint;
			}
			else if (ScopeStr.Equals(TEXT("target_graph"), ESearchCase::IgnoreCase))
			{
				Target.TargetType = EBlueprintHelperTargetType::Graph;
			}
			else if (ScopeStr.Equals(TEXT("target_function"), ESearchCase::IgnoreCase))
			{
				Target.TargetType = EBlueprintHelperTargetType::Function;
			}
			else if (ScopeStr.Equals(TEXT("target_event"), ESearchCase::IgnoreCase))
			{
				Target.TargetType = EBlueprintHelperTargetType::Event;
			}
		}
	}

	if (Target.AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("缺少 asset_path 参数。"));
	}

	// 调用 LogicMdReadService
	FBlueprintHelperLogicMdData LogicMdData = LogicMdReadService.ReadLogicMd(Target);

	// 使用 ToolResultBuilder 构建标准化的返回。
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_blueprint_logic_md_by_target"), TraceId);
	Result.Target = Target;
	Result.Data = LogicMdData.ToJson();

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── read_blueprint_logic_json ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadBlueprintLogicJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> Payload = Req.Payload;

	FBlueprintHelperTargetRef Target;
	Target.TargetType = EBlueprintHelperTargetType::Graph;

	if (Payload.IsValid())
	{
		FString AssetPath;
		if (Payload->TryGetStringField(TEXT("asset_path"), AssetPath)) { Target.AssetPath = AssetPath; }

		FString GraphName;
		if (Payload->TryGetStringField(TEXT("graph"), GraphName)) { Target.Graph = GraphName; }

		FString FunctionName;
		if (Payload->TryGetStringField(TEXT("function"), FunctionName)) { Target.Function = FunctionName; Target.TargetType = EBlueprintHelperTargetType::Function; }

		FString EventName;
		if (Payload->TryGetStringField(TEXT("event"), EventName)) { Target.Event = EventName; Target.TargetType = EBlueprintHelperTargetType::Event; }

		FString BlockId;
		if (Payload->TryGetStringField(TEXT("block_id"), BlockId)) { Target.BlockId = BlockId; Target.TargetType = EBlueprintHelperTargetType::Block; }

		FString ScopeStr;
		if (Payload->TryGetStringField(TEXT("scope"), ScopeStr))
		{
			if (ScopeStr.Equals(TEXT("blueprint"), ESearchCase::IgnoreCase)) { Target.TargetType = EBlueprintHelperTargetType::Blueprint; }
			else if (ScopeStr.Equals(TEXT("target_graph"), ESearchCase::IgnoreCase)) { Target.TargetType = EBlueprintHelperTargetType::Graph; }
			else if (ScopeStr.Equals(TEXT("target_function"), ESearchCase::IgnoreCase)) { Target.TargetType = EBlueprintHelperTargetType::Function; }
			else if (ScopeStr.Equals(TEXT("target_event"), ESearchCase::IgnoreCase)) { Target.TargetType = EBlueprintHelperTargetType::Event; }
			else if (ScopeStr.Equals(TEXT("target_custom_event"), ESearchCase::IgnoreCase)) { Target.TargetType = EBlueprintHelperTargetType::CustomEvent; }
		}
	}

	if (Target.AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest, TEXT("缺少 asset_path 参数。"));
	}

	FBlueprintHelperLogicJsonData LogicJsonData = LogicJsonReadService.ReadLogicJson(Target);

	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("read_blueprint_logic_json_by_target"), TraceId);
	Result.Target = Target;
	Result.Data = LogicJsonData.ToJson();

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── validate_json ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleValidateJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString JsonText;
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("json"), true, JsonText, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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

	// 构建目标协议响应
	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();

	// 元信息
	Resp.Result->SetStringField(TEXT("format"), TEXT("raw_json"));
	Resp.Result->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.JsonToBlueprint.v2.2"));
	Resp.Result->SetStringField(TEXT("assetPath"), ExportReq.Target.BlueprintPath);
	if (!ExportReq.Target.GraphName.IsEmpty())
	{
		Resp.Result->SetStringField(TEXT("graph"), ExportReq.Target.GraphName);
	}
	Resp.Result->SetStringField(TEXT("effective_scope"), ExportResult.EffectiveScope);
	Resp.Result->SetBoolField(TEXT("importable"), true);

	// payload（对象，主要字段）
	if (ExportResult.JsonObject.IsValid())
	{
		Resp.Result->SetObjectField(TEXT("payload"), ExportResult.JsonObject.ToSharedRef());
	}

	// stats
	Resp.Result->SetObjectField(TEXT("stats"), FBlueprintHelperBridgeRouterLocalUtils::MakeRawJsonStatsObject(ExportResult.JsonObject));

	// diagnostics
	TArray<TSharedPtr<FJsonValue>> DiagArray;
	for (const FBlueprintHelperDiagnosticItem& Item : ExportResult.Diagnostics.Items)
	{
		TSharedRef<FJsonObject> DiagObj = MakeShared<FJsonObject>();
		DiagObj->SetStringField(TEXT("severity"),
			Item.Severity == EBlueprintHelperDiagnosticSeverity::Error ? TEXT("error") :
			Item.Severity == EBlueprintHelperDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
		DiagObj->SetStringField(TEXT("message"), Item.Message);
		if (!Item.Code.IsEmpty()) DiagObj->SetStringField(TEXT("code"), Item.Code);
		DiagArray.Add(MakeShared<FJsonValueObject>(DiagObj));
	}
	Resp.Result->SetArrayField(TEXT("diagnostics"), DiagArray);

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
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringOption(Payload, TEXT("scope"), TEXT("graph"), ScopeStr, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadStringOption(Payload, TEXT("format"), TEXT("logic_md"), FormatStr, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadStringOption(Payload, TEXT("detail"), TEXT("normal"), DetailStr, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
	}

	FBlueprintHelperExportRequest ExportReq;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("target_blueprint"), ExportReq.Target.BlueprintPath);
		Payload->TryGetStringField(TEXT("target_graph"), ExportReq.Target.GraphName);
	}

	if (ScopeStr == TEXT("graph"))
	{
		ExportReq.Scope = EBlueprintHelperExportScope::SingleGraph;
	}
	else if (ScopeStr == TEXT("blueprint"))
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

	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Payload, TEXT("include_data_dependencies"), LogicOptions.bIncludeDataDependencies, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Payload, TEXT("include_orphans"), LogicOptions.bIncludeOrphanNodes, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Payload, TEXT("include_node_ids"), LogicOptions.bIncludeNodeIds, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Payload, TEXT("include_positions"), LogicOptions.bIncludePositions, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Payload, TEXT("include_raw_node_types"), LogicOptions.bIncludeRawNodeTypes, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
	}

	const FBlueprintHelperExportResult ExportResult = ExportService.Export(ExportReq);
	if (!ExportResult.bSuccess)
	{
		const FString ErrorMsg = ExportResult.Diagnostics.Items.Num() > 0
			? ExportResult.Diagnostics.Items[0].Message
			: TEXT("导出失败。");
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			FBlueprintHelperBridgeRouterLocalUtils::DiagnosticSetToBridgeError(ExportResult.Diagnostics),
			ErrorMsg);
	}

	const FBlueprintHelperLogicResult LogicResult =
		FBlueprintHelperLogicProcessor::ProcessRawJsonObject(ExportResult.JsonObject, LogicOptions);
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
		Resp.Result->SetObjectField(TEXT("stats"), FBlueprintHelperBridgeRouterLocalUtils::MakeLogicStatsObject(LogicResult));
		return Resp;
	}

	TSharedPtr<FJsonObject> LogicObject = FBlueprintHelperBridgeRouterLocalUtils::ParseJsonObject(LogicResult.OutputText);
	if (!LogicObject.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::JsonParseFailed,
			TEXT("LogicProcessor 返回。logic_json 无法解析。"));
	}

	Resp.Result->SetObjectField(TEXT("logic"), LogicObject);

	const TSharedPtr<FJsonObject>* StatsObject = nullptr;
	if (LogicObject->TryGetObjectField(TEXT("stats"), StatsObject) && StatsObject && StatsObject->IsValid())
	{
		Resp.Result->SetObjectField(TEXT("stats"), *StatsObject);
	}
	else
	{
		Resp.Result->SetObjectField(TEXT("stats"), FBlueprintHelperBridgeRouterLocalUtils::MakeLogicStatsObject(LogicResult));
	}

	return Resp;
}

// ─── import_json ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleImportJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	// import_json now accepts object-first payload.json only.
	FBlueprintHelperImportRequest ImportReq;
	FBlueprintHelperBridgeValidationError ParseError;

	bool bHasJson = false;
	if (Req.Payload.IsValid())
	{
		const TSharedPtr<FJsonValue>* FoundValue = Req.Payload->Values.Find(TEXT("json"));
		if (FoundValue && FoundValue->IsValid())
		{
			const TSharedPtr<FJsonValue> JsonVal = *FoundValue;
			if (JsonVal->Type == EJson::Object)
			{
				ImportReq.JsonObject = JsonVal->AsObject();
				bHasJson = true;
			}
		}
	}

	if (!bHasJson)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 json 字段，或类型不被支持（需要 object）。"));
	}

	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("target_blueprint"), ImportReq.Target.BlueprintPath);
		Req.Payload->TryGetStringField(TEXT("target_graph"), ImportReq.Target.GraphName);
	}
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Req.Payload, TEXT("compile_after_import"), ImportReq.bAutoCompile, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Req.Payload, TEXT("strict"), ImportReq.bStrict, ParseError)
		|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Req.Payload, TEXT("allow_partial"), ImportReq.bAllowPartial, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
	}

	// ImportService.ResolveImportJsonText() serializes object-first RawJson before schema/importable guards.
	FBlueprintHelperImportResult ImportResult = ImportService.Import(ImportReq);

	if (!ImportResult.bSuccess && ImportResult.Diagnostics.HasErrors())
	{
		const FString ErrorMsg = ImportResult.GetSummaryText();
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			FBlueprintHelperBridgeRouterLocalUtils::DiagnosticSetToBridgeError(ImportResult.Diagnostics),
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
	Resp.Result = FBlueprintHelperBridgeRouterLocalUtils::AgentImportResultToJson(ImportResult);
	return Resp;
}

// ─── Task Runtime ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandlePreviewTaskPlan(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperToolResultBase Result = TaskRuntimeService.PreviewTaskPlan(Req.Payload);
	const FString ErrorMessage = Result.Error.IsSet() && !Result.Error->Message.IsEmpty()
		? Result.Error->Message
		: TEXT("preview_task_plan 执行失败。");

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMessage);

	Resp.Result = Result.ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExecuteTaskPlan(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperToolResultBase Result = TaskRuntimeService.ExecuteTaskPlan(Req.Payload);
	FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts();
	ReviewStoreService.NotifyPendingReviewChanged();
	const FString ErrorMessage = Result.Error.IsSet() && !Result.Error->Message.IsEmpty()
		? Result.Error->Message
		: TEXT("execute_task_plan 执行失败。");

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMessage);

	Resp.Result = Result.ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetTaskRunJournal(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString TaskRunId;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("task_run_id"), TaskRunId);
	}

	const FBlueprintHelperToolResultBase Result = TaskRuntimeService.GetTaskRunJournal(TaskRunId);
	const FString ErrorMessage = Result.Error.IsSet() && !Result.Error->Message.IsEmpty()
		? Result.Error->Message
		: TEXT("get_task_run_journal 执行失败。");
	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMessage);

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── compile_blueprint_asset ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCompileBlueprintAsset(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
		return FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest, TEXT("payload 缺失。"));

	const FBlueprintHelperToolResultBase Result = CompileAssetService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("compile_blueprint_asset 执行失败。"));

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── list_blueprint_helper_transactions ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleQueryReviewRecords(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperReviewRecordQuery Query;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("archive_session_id"), Query.ArchiveSessionIdFilter);
		Req.Payload->TryGetStringField(TEXT("asset_path"), Query.AssetPathFilter);
		Req.Payload->TryGetStringField(TEXT("task_run_id"), Query.TaskRunIdFilter);
		Req.Payload->TryGetBoolField(TEXT("pending_only"), Query.bPendingOnly);
	}

	FBlueprintHelperReviewStoreService ReviewStore;
	const TArray<FBlueprintHelperReviewRecord> Records = ReviewStore.QueryReviewRecords(Query);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewRecordQueryResult.v1"));
	Data->SetNumberField(TEXT("record_count"), Records.Num());

	TSharedRef<FJsonObject> QueryJson = MakeShared<FJsonObject>();
	if (!Query.ArchiveSessionIdFilter.IsEmpty())
	{
		QueryJson->SetStringField(TEXT("archive_session_id"), Query.ArchiveSessionIdFilter);
	}
	if (!Query.AssetPathFilter.IsEmpty())
	{
		QueryJson->SetStringField(TEXT("asset_path"), Query.AssetPathFilter);
	}
	if (!Query.TaskRunIdFilter.IsEmpty())
	{
		QueryJson->SetStringField(TEXT("task_run_id"), Query.TaskRunIdFilter);
	}
	QueryJson->SetBoolField(TEXT("pending_only"), Query.bPendingOnly);
	Data->SetObjectField(TEXT("query"), QueryJson);

	TArray<TSharedPtr<FJsonValue>> RecordValues;
	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		RecordValues.Add(MakeShared<FJsonValueObject>(ReviewStore.BuildReviewRecordSummaryArtifact(Record)));
	}
	Data->SetArrayField(TEXT("records"), RecordValues);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("query_review_records"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Data = Data;

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleApplyReviewAction(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload is required."));
	}

	FString ReviewRecordId;
	FString Action;
	Req.Payload->TryGetStringField(TEXT("review_record_id"), ReviewRecordId);
	Req.Payload->TryGetStringField(TEXT("action"), Action);

	TArray<FString> TargetKeys;
	const TArray<TSharedPtr<FJsonValue>>* TargetKeyValues = nullptr;
	if (Req.Payload->TryGetArrayField(TEXT("target_keys"), TargetKeyValues) && TargetKeyValues)
	{
		for (const TSharedPtr<FJsonValue>& TargetKeyValue : *TargetKeyValues)
		{
			FString TargetKey;
			if (TargetKeyValue.IsValid() && TargetKeyValue->TryGetString(TargetKey) && !TargetKey.IsEmpty())
			{
				TargetKeys.Add(TargetKey);
			}
		}
	}

	FBlueprintHelperReviewActionService ActionService(&DebugEntryService);
	FBlueprintHelperReviewActionResult ActionResult;
	if (Action.Equals(TEXT("accept"), ESearchCase::IgnoreCase))
	{
		ActionResult = ActionService.AcceptReviewTargets(ReviewRecordId, TargetKeys);
	}
	else if (Action.Equals(TEXT("reject"), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperReviewRejectOptions Options;
		ActionResult = ActionService.RejectReviewTargets(ReviewRecordId, TargetKeys, Options);
	}
	else
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("action must be accept or reject."));
	}

	ReviewStoreService.NotifyPendingReviewChanged();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewActionResult.v1"));
	Data->SetStringField(TEXT("review_record_id"), ReviewRecordId);
	Data->SetStringField(TEXT("action"), Action.ToLower());
	Data->SetBoolField(TEXT("succeeded"), ActionResult.bSucceeded);
	Data->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(ActionResult.NewStatus));
	Data->SetStringField(TEXT("message"), ActionResult.Message);
	if (!ActionResult.TargetTransactionId.IsEmpty())
	{
		Data->SetStringField(TEXT("target_transaction_id"), ActionResult.TargetTransactionId);
	}
	if (!ActionResult.RollbackMode.IsEmpty())
	{
		Data->SetStringField(TEXT("rollback_mode"), ActionResult.RollbackMode);
	}
	TArray<TSharedPtr<FJsonValue>> TargetKeyJson;
	for (const FString& TargetKey : TargetKeys)
	{
		TargetKeyJson.Add(MakeShared<FJsonValueString>(TargetKey));
	}
	Data->SetArrayField(TEXT("target_keys"), TargetKeyJson);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("apply_review_action"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Data = Data;
	Result.bModified = ActionResult.bSucceeded;

	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListTransactions(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperToolResultBase Result = TransactionQueryService.List(Req.Payload);
	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("list transactions failed"));
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── read_blueprint_helper_transaction ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadTransaction(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperToolResultBase Result = TransactionQueryService.Read(Req.Payload);
	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("read transaction failed"));
	Resp.Result = Result.ToJson();
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

// ══════════════════════════════════════════════════════════。// Phase 4 。资产浏览命令
// ══════════════════════════════════════════════════════════。
// ─── open_asset ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleOpenAsset(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString AssetPath;
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("asset_path"), true, AssetPath, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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
		if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("path"), false, ListReq.Path, ParseError)
			|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("class_filter"), false, ListReq.ClassFilter, ParseError)
			|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("name_filter"), false, ListReq.NameFilter, ParseError)
			|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolOption(Req.Payload, TEXT("recursive"), ListReq.bRecursive, ParseError))
		{
			return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
		}
		if (Req.Payload->HasField(TEXT("max_results")))
		{
			double MaxResults = 0.0;
			if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadNumberField(Req.Payload, TEXT("max_results"), false, MaxResults, ParseError))
			{
				return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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
		if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("path"), false, SearchReq.Path, ParseError)
			|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("class_filter"), false, SearchReq.ClassFilter, ParseError)
			|| !FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("query"), true, SearchReq.NameFilter, ParseError))
		{
			return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
		}
		if (Req.Payload->HasField(TEXT("max_results")))
		{
			double MaxResults = 0.0;
			if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadNumberField(Req.Payload, TEXT("max_results"), false, MaxResults, ParseError))
			{
				return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("asset_path"), true, AssetPath, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
	}

	if (AssetPath.IsEmpty())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 asset_path 字段。"));
	}

	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	TSharedRef<FJsonObject> Tgt = MakeShared<FJsonObject>();
	Tgt->SetStringField(TEXT("asset_path"), AssetPath);

	const FBlueprintHelperSaveResult SaveResult = AssetBrowseService.SaveAsset(AssetPath);
	if (!SaveResult.bSuccess)
	{
		FBlueprintHelperToolError Err;
		Err.Code = TEXT("save_failed");
		Err.Stage = EBlueprintHelperToolStage::Execute;
		Err.Message = SaveResult.ErrorMessage;
		Err.bRetryable = true;
		FBlueprintHelperToolResultBase Fail = FBlueprintHelperToolResultBuilder::Failure(TEXT("save_asset"), TraceId, Err);
		Fail.CustomTargetJson = Tgt;
		FBlueprintHelperDebugEntryEventInput DebugInput;
		DebugInput.SourceLayer = TEXT("debug");
		DebugInput.Source = TEXT("save_failure");
		DebugInput.Operation = TEXT("save_asset");
		DebugInput.Stage = TEXT("execute");
		DebugInput.AssetPaths.Add(AssetPath);
		DebugInput.Error.Code = Err.Code;
		DebugInput.Error.Message = Err.Message;
		DebugInput.RecommendedNext = TEXT("verify_asset_checkout_or_path");
		DebugEntryService.AttachDebugCaseToFailureBestEffort(Fail, DebugInput);
		auto Resp = FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, SaveResult.ErrorMessage);
		Resp.Result = Fail.ToJson();
		return Resp;
	}

	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = FBlueprintHelperProtocol::ToolResultSchema;
	Result.Operation = TEXT("save_asset");
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Completed;
	Result.bModified = false;
	Result.CustomTargetJson = Tgt;

	FBlueprintHelperSaveAssetResultData Data;
	Data.SaveResult.bSaved = true;
	Data.SaveResult.bWasDirty = true;
	Result.Data = Data.ToJson();

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── get_asset_info ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetAssetInfo(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString AssetPath;
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("asset_path"), true, AssetPath, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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

// ══════════════════════════════════════════════════════════。// Phase 5 。蓝图结构查询与操。// ══════════════════════════════════════════════════════════。
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
	// Migrated to VariableService
	const FBlueprintHelperToolResultBase Result = VariableService.ReadMemberVariables(Req.Payload);
	auto Resp = Result.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("list_variables failed"));
	Resp.Result = Result.ToJson();
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
	// Migrated to VariableService
	const FBlueprintHelperToolResultBase Result = VariableService.AddMemberVariable(Req.Payload);
	auto Resp = Result.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("add_variable failed"));
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── remove_variable ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveVariable(
	const FBlueprintHelperBridgeRequest& Req) const
{
	// Migrated to VariableService
	const FBlueprintHelperToolResultBase Result = VariableService.RemoveMemberVariable(Req.Payload);
	auto Resp = Result.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("remove_variable failed"));
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── add_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeValidationError ParseError;
	FString GraphName;
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("name"), true, GraphName, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("name"), true, GraphName, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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
	if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("name"), true, DispatcherName, ParseError))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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

// ══════════════════════════════════════════════════════════。// Phase 6 。UMG Widget 操作
// ══════════════════════════════════════════════════════════。
static FString GetRequiredStringField(const TSharedPtr<FJsonObject>& Payload, const FString& Field)
{
	FString Value;
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(Field, Value);
	}
	return Value;
}

// ══════════════════════════════════════════════════════════。// Phase 8: 编辑器命。// ══════════════════════════════════════════════════════════。
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
		if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadStringField(Req.Payload, TEXT("parent_class"), false, ParentClass, ParseError))
		{
			return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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
		if (!FBlueprintHelperBridgeRouterLocalUtils::TryReadBoolField(Req.Payload, TEXT("save_all"), false, bSaveAll, ParseError))
		{
			return FBlueprintHelperBridgeRouterLocalUtils::ValidationErrorResponse(Req.RequestId, ParseError);
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
