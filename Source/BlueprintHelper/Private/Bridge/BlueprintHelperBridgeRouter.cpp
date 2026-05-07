// BlueprintHelper Bridge Layer 。命令路由实现

#include "Bridge/BlueprintHelperBridgeRouter.h"
#include "Bridge/BlueprintHelperBridgeProtocol.h"
#include "Bridge/BlueprintHelperRequestValidator.h"
#include "BlueprintHelper.h"
#include "Services/BlueprintHelperImportService.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "Services/BlueprintHelperExportService.h"
#include "Logic/BlueprintHelperLogicProcessor.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperCompileService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperValidationService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperContextService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperAssetBrowseService.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Services/UMGWidget/BlueprintHelperWidgetService.h"
#include "Services/DataAssetObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Services/DataTable/BlueprintHelperDataTableService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperEditorCommandService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperRuntimeProfileService.h"
#include "Structure/RuntimeDiagnostics/BlueprintHelperRuntimeProfileTypes.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperDiagnosticsService.h"
#include "Structure/RuntimeDiagnostics/BlueprintHelperDiagnosticsTypes.h"
#include "Structure/BlueprintHelperDependencyAnalysisTypes.h"
#include "Logic/BlueprintHelperLogicMdReadService.h"
#include "Structure/BlueprintHelperLogicMdTypes.h"
#include "Logic/BlueprintHelperLogicJsonReadService.h"
#include "Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Services/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Structure/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "Services/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Services/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Structure/BlueprintClassSettings/BlueprintHelperClassSettingsTypes.h"
#include "Services/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Structure/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Services/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Structure/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Services/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Structure/GraphWrite/BlueprintHelperPatchGraphTypes.h"
#include "Services/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Structure/GraphWrite/BlueprintHelperMergeGraphTypes.h"
#include "Services/CleanupOwnership/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Structure/CleanupOwnership/BlueprintHelperCleanupBlockTypes.h"
#include "Services/CleanupOwnership/BlueprintHelperRollbackCleanupTransactionService.h"
#include "Structure/CleanupOwnership/BlueprintHelperRollbackCleanupTypes.h"
#include "Services/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Structure/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedTypes.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperCompileAssetService.h"
#include "Structure/RuntimeDiagnostics/BlueprintHelperCompileAssetTypes.h"
#include "Structure/RuntimeDiagnostics/BlueprintHelperSaveAssetTypes.h"
#include "Transactions/Transactions/BlueprintHelperTransactionQueryService.h"
#include "Structure/Transactions/BlueprintHelperTransactionQueryTypes.h"
#include "Services/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Structure/BlueprintVariables/BlueprintHelperBlueprintVariableTypes.h"
#include "Structure/BlueprintHelperToolResultTypes.h"
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

	TSharedRef<FJsonObject> MakeRawJsonStatsObject(const TSharedPtr<FJsonObject>& RawJsonObject)
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

	TSharedRef<FJsonObject> MakeDiagnosticJsonArray(const FBlueprintHelperDiagnosticSet& Diagnostics)
	{
		TSharedRef<FJsonObject> Array = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> DiagValues;
		for (const FBlueprintHelperDiagnosticItem& Item : Diagnostics.Items)
		{
			TSharedRef<FJsonObject> DiagObj = MakeShared<FJsonObject>();
			DiagObj->SetStringField(TEXT("severity"),
				Item.Severity == EBlueprintHelperDiagnosticSeverity::Error ? TEXT("error") :
				Item.Severity == EBlueprintHelperDiagnosticSeverity::Warning ? TEXT("warning") : TEXT("info"));
			DiagObj->SetStringField(TEXT("message"), Item.Message);
			if (!Item.Code.IsEmpty()) DiagObj->SetStringField(TEXT("code"), Item.Code);
			if (!Item.Field.IsEmpty()) DiagObj->SetStringField(TEXT("field"), Item.Field);
			DiagValues.Add(MakeShared<FJsonValueObject>(DiagObj));
		}
		Array->SetArrayField(TEXT("diagnostics"), DiagValues);
		return Array;
	}

	/** 尝试从 JSON payload 读取 json 字段 — 支持 object 和 string */
	bool TryReadJsonObjectOrString(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		TSharedPtr<FJsonObject>& OutJsonObject,
		FString& OutJsonString,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		OutJsonObject.Reset();
		OutJsonString.Empty();

		if (!Payload.IsValid())
		{
			OutError.Code = TEXT("invalid_request");
			OutError.Field = FString(TEXT("payload.")) + FieldName;
			OutError.ExpectedType = TEXT("object 或 string");
			OutError.ActualType = TEXT("missing");
			OutError.Message = FString::Printf(TEXT("%s 缺失，需要 object 或 string。"), *OutError.Field);
			return false;
		}

		const TSharedPtr<FJsonValue>* FoundValue = Payload->Values.Find(FieldName);
		if (!FoundValue || !FoundValue->IsValid())
		{
			OutError.Code = TEXT("invalid_request");
			OutError.Field = FString(TEXT("payload.")) + FieldName;
			OutError.ExpectedType = TEXT("object 或 string");
			OutError.ActualType = TEXT("missing");
			OutError.Message = FString::Printf(TEXT("%s 缺失，需要 object 或 string。"), *OutError.Field);
			return false;
		}

		const TSharedPtr<FJsonValue> Value = *FoundValue;
		if (Value->Type == EJson::Object)
		{
			OutJsonObject = Value->AsObject();
			return true;
		}

		if (Value->Type == EJson::String)
		{
			OutJsonString = Value->AsString();
			return true;
		}

		OutError.Code = TEXT("invalid_request");
		OutError.Field = FString(TEXT("payload.")) + FieldName;
		OutError.ExpectedType = TEXT("object 或 string");
		OutError.ActualType = JsonValueTypeToString(Value);
		OutError.Message = FString::Printf(TEXT("%s 必须是 object 或 string，实际类型: %s。"), *OutError.Field, *OutError.ActualType);
		return false;
	}

	// ─── Asset Factory 辅助函数 ───

	/** 构建 AssetFactory 错误对象。*/
	FBlueprintHelperToolError MakeAssetFactoryError(
		const FString& Code, EBlueprintHelperToolStage Stage,
		const FString& Message, bool bRetryable)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = bRetryable;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return Error;
	}

	/** 填充 ToolResultBase 。Asset Factory target/data/validation。*/
	void BuildAssetFactoryResult(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperAssetFactoryData& Data,
		const FString& AssetPath,
		EBlueprintHelperAssetType AssetType)
	{
		FBlueprintHelperTargetRef Target;
		Target.AssetPath = AssetPath;
		Target.TargetType = EBlueprintHelperTargetType::Asset;
		// 根据 AssetType 设置 AssetClass
		switch (AssetType)
		{
		case EBlueprintHelperAssetType::BlueprintClass:     Target.AssetClass = TEXT("Blueprint"); break;
		case EBlueprintHelperAssetType::BlueprintInterface:  Target.AssetClass = TEXT("Blueprint"); break;
		case EBlueprintHelperAssetType::Structure:           Target.AssetClass = TEXT("UserDefinedStruct"); break;
		case EBlueprintHelperAssetType::InputAction:         Target.AssetClass = TEXT("InputAction"); break;
		case EBlueprintHelperAssetType::InputMappingContext: Target.AssetClass = TEXT("InputMappingContext"); break;
		case EBlueprintHelperAssetType::DataAsset:           Target.AssetClass = TEXT("DataAsset"); break;
		case EBlueprintHelperAssetType::DataTable:           Target.AssetClass = TEXT("DataTable"); break;
		default:                                             break;
		}
		Result.Target = Target;
		Result.Data = Data.ToJson();

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = FBlueprintHelperAssetFactoryService::ShouldCompile(AssetType);
		Validation.bShouldSave = FBlueprintHelperAssetFactoryService::ShouldSave(AssetType);
		Validation.bCompiled = false;
		Validation.bSaved = false;
		Result.Validation = Validation;
	}

	FBlueprintHelperToolError MakeReferenceContextError(
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

	TSharedRef<FJsonObject> MakeReferenceContextTargetJson(const TSharedPtr<FJsonObject>& Payload)
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

	FBlueprintHelperDependencyAnalysisTarget ReadReferenceContextTarget(const TSharedPtr<FJsonObject>& Payload)
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
		Payload->TryGetStringField(TEXT("row_name"), Target.RowName);
		Payload->TryGetStringField(TEXT("widget_name"), Target.WidgetName);
		Payload->TryGetStringField(TEXT("interface_path"), Target.InterfacePath);
		if (Target.TargetType.IsEmpty())
		{
			Target.TargetType = TEXT("asset");
		}
		return Target;
	}

	FBlueprintHelperToolResultBase MakeReferenceContextFailureResult(
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
		Result.CustomTargetJson = MakeReferenceContextTargetJson(Payload);
		return Result;
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
	const FBlueprintHelperEditorCommandService& InEditorCommand,
	const FBlueprintHelperRuntimeProfileService& InRuntimeProfile,
	const FBlueprintHelperDiagnosticsService& InDiagnostics,
	const FBlueprintHelperLogicMdReadService& InLogicMdRead,
	const FBlueprintHelperLogicJsonReadService& InLogicJsonRead,
	const FBlueprintHelperAssetFactoryService& InAssetFactory,
	const FBlueprintHelperComponentService& InComponentService,
	const FBlueprintHelperClassSettingsService& InClassSettings,
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
	const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
	const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
	const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService,
	const FBlueprintHelperCleanupBlueprintHelperBlockService& InCleanupBlockService,
	const FBlueprintHelperRollbackCleanupTransactionService& InRollbackCleanupService,
	const FBlueprintHelperConvertBlockToUserOwnedService& InConvertBlockService,
	const FBlueprintHelperCompileAssetService& InCompileAssetService,
	const FBlueprintHelperTransactionQueryService& InTransactionQueryService,
	const FBlueprintHelperBlueprintVariableService& InVariableService)
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
	, RuntimeProfileService(InRuntimeProfile)
	, DiagnosticsService(InDiagnostics)
	, LogicMdReadService(InLogicMdRead)
	, LogicJsonReadService(InLogicJsonRead)
	, AssetFactoryService(InAssetFactory)
	, ComponentService(InComponentService)
	, ClassSettingsService(InClassSettings)
	, AppendGraphService(InAppendGraphService)
	, ReplaceGraphService(InReplaceGraphService)
	, PatchGraphService(InPatchGraphService)
	, MergeGraphService(InMergeGraphService)
	, CleanupBlockService(InCleanupBlockService)
	, RollbackCleanupService(InRollbackCleanupService)
	, ConvertBlockService(InConvertBlockService)
	, VariableService(InVariableService)
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
		InCleanupBlockService,
		InRollbackCleanupService,
		InConvertBlockService,
		InCompileAssetService,
		InAssetBrowse)
	, CompileAssetService(InCompileAssetService)
	, TransactionQueryService(InTransactionQueryService)
{
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRequest(
	const FBlueprintHelperBridgeRequest& Request) const
{
	FBlueprintHelperBridgeValidationError ValidationError;
	if (!FBlueprintHelperRequestValidator::ValidatePayloadForCommand(Request.Command, Request.Payload, ValidationError))
	{
		FBlueprintHelperBridgeResponse Resp = ValidationErrorResponse(Request.RequestId, ValidationError);
		if (Request.Command == TEXT("read_reference_context"))
		{
			const FBlueprintHelperToolResultBase Result = MakeReferenceContextFailureResult(
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
	if (Request.Command == TEXT("get_runtime_profile"))
	{
		return HandleGetRuntimeProfile(Request);
	}
	if (Request.Command == TEXT("diagnostics_runtime"))
	{
		return HandleDiagnosticsRuntime(Request);
	}
	if (Request.Command == TEXT("read_reference_context"))
	{
		return HandleReadReferenceContext(Request);
	}
	if (Request.Command == TEXT("read_blueprint_logic_md"))
	{
		return HandleReadBlueprintLogicMd(Request);
	}
	if (Request.Command == TEXT("read_blueprint_logic_json"))
	{
		return HandleReadBlueprintLogicJson(Request);
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
	// ─── Blueprint Variable Service (new commands) ───
	if (Request.Command == TEXT("read_blueprint_member_variables"))
		return HandleReadMemberVariables(Request);
	if (Request.Command == TEXT("add_blueprint_member_variable"))
		return HandleAddMemberVariable(Request);
	if (Request.Command == TEXT("add_blueprint_member_variables"))
		return HandleAddMemberVariables(Request);
	if (Request.Command == TEXT("set_blueprint_member_variable_properties"))
		return HandleSetMemberVariableProperties(Request);
	if (Request.Command == TEXT("remove_blueprint_member_variable"))
		return HandleRemoveMemberVariable(Request);
	if (Request.Command == TEXT("remove_blueprint_member_variables"))
		return HandleRemoveMemberVariables(Request);
	if (Request.Command == TEXT("read_blueprint_member_defaults"))
		return HandleReadMemberDefaults(Request);
	if (Request.Command == TEXT("set_blueprint_member_default"))
		return HandleSetMemberDefault(Request);
	if (Request.Command == TEXT("set_blueprint_member_defaults"))
		return HandleSetMemberDefaults(Request);
	if (Request.Command == TEXT("read_blueprint_local_variables"))
		return HandleReadLocalVariables(Request);
	if (Request.Command == TEXT("add_blueprint_local_variable"))
		return HandleAddLocalVariable(Request);
	if (Request.Command == TEXT("add_blueprint_local_variables"))
		return HandleAddLocalVariables(Request);
	if (Request.Command == TEXT("set_blueprint_local_variable_properties"))
		return HandleSetLocalVariableProperties(Request);
	if (Request.Command == TEXT("remove_blueprint_local_variable"))
		return HandleRemoveLocalVariable(Request);
	if (Request.Command == TEXT("remove_blueprint_local_variables"))
		return HandleRemoveLocalVariables(Request);
	// ─── (legacy commands, migrated to new service) ───
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
	// ─── Phase 8: 编辑器命。───
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
	if (Request.Command == TEXT("create_asset"))
	{
		return HandleCreateAsset(Request);
	}
	if (Request.Command == TEXT("read_components"))
	{
		return HandleReadComponents(Request);
	}
	if (Request.Command == TEXT("add_component"))
	{
		return HandleAddComponent(Request);
	}
	if (Request.Command == TEXT("set_component_property"))
	{
		return HandleSetComponentProperty(Request);
	}
	if (Request.Command == TEXT("set_component_properties"))
	{
		return HandleSetComponentProperties(Request);
	}
	if (Request.Command == TEXT("remove_component"))
	{
		return HandleRemoveComponent(Request);
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

	// ─── Phase 9: Blueprint Class Settings ───
	if (Request.Command == TEXT("read_class_settings"))
	{
		return HandleReadClassSettings(Request);
	}
	if (Request.Command == TEXT("add_implemented_interface"))
	{
		return HandleAddImplementedInterface(Request);
	}
	if (Request.Command == TEXT("add_implemented_interfaces"))
	{
		return HandleAddImplementedInterfaces(Request);
	}
	if (Request.Command == TEXT("remove_implemented_interface"))
	{
		return HandleRemoveImplementedInterface(Request);
	}
	if (Request.Command == TEXT("remove_implemented_interfaces"))
	{
		return HandleRemoveImplementedInterfaces(Request);
	}
	if (Request.Command == TEXT("set_class_default_property"))
	{
		return HandleSetClassDefaultProperty(Request);
	}
	if (Request.Command == TEXT("set_class_default_properties"))
	{
		return HandleSetClassDefaultProperties(Request);
	}
	// ─── Task Runtime ───
	if (Request.Command == TEXT("preview_task_plan"))
	{
		return HandlePreviewTaskPlan(Request);
	}
	if (Request.Command == TEXT("execute_task_plan"))
	{
		return HandleExecuteTaskPlan(Request);
	}
	if (Request.Command == TEXT("get_task_run_journal"))
	{
		return HandleGetTaskRunJournal(Request);
	}
	// ─── AppendBlueprintGraph ───
	if (Request.Command == TEXT("append_blueprint_graph"))
	{
		return HandleAppendBlueprintGraph(Request);
	}
	// ─── ReplaceBlueprintGraph ───
	if (Request.Command == TEXT("replace_blueprint_graph"))
	{
		return HandleReplaceBlueprintGraph(Request);
	}
	// ─── PatchBlueprintGraph ───
	if (Request.Command == TEXT("patch_blueprint_graph"))
	{
		return HandlePatchBlueprintGraph(Request);
	}
	// ─── MergeBlueprintGraph ───
	if (Request.Command == TEXT("merge_blueprint_graph"))
	{
		return HandleMergeBlueprintGraph(Request);
	}
	// ─── CleanupBlueprintHelperBlock ───
	if (Request.Command == TEXT("cleanup_blueprint_helper_block"))
	{
		return HandleCleanupBlueprintHelperBlock(Request);
	}
	// ─── RollbackCleanupTransaction ───
	if (Request.Command == TEXT("rollback_cleanup_transaction"))
	{
		return HandleRollbackCleanupTransaction(Request);
	}
	// ─── ConvertBlockToUserOwned ───
	if (Request.Command == TEXT("convert_blueprint_helper_block_to_user_owned"))
	{
		return HandleConvertBlockToUserOwned(Request);
	}
	// ─── CompileBlueprintAsset ───
	if (Request.Command == TEXT("compile_blueprint_asset"))
	{
		return HandleCompileBlueprintAsset(Request);
	}
	// ─── Transaction Query ───
	if (Request.Command == TEXT("list_blueprint_helper_transactions"))
	{
		return HandleListTransactions(Request);
	}
	if (Request.Command == TEXT("read_blueprint_helper_transaction"))
	{
		return HandleReadTransaction(Request);
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

// ─── get_runtime_profile ───

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

// --- read_reference_context ---

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadReferenceContext(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> Payload = Req.Payload;
	const FBlueprintHelperDependencyAnalysisTarget Target = ReadReferenceContextTarget(Payload);

	FString Scope = TEXT("safety_context");
	if (Payload.IsValid())
	{
		Payload->TryGetStringField(TEXT("scope"), Scope);
	}

	bool bIncludeSamples = true;
	FBlueprintHelperBridgeValidationError ValidationError;
	if (!TryReadBoolOption(Payload, TEXT("include_samples"), bIncludeSamples, ValidationError))
	{
		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			ValidationError.Message);
		Resp.Result = MakeReferenceContextFailureResult(
			Payload,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			ValidationError.Message,
			ValidationError.Field).ToJson();
		return Resp;
	}

	double MaxResults = 50.0;
	if (!TryReadNumberField(Payload, TEXT("max_results"), false, MaxResults, ValidationError))
	{
		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			ValidationError.Message);
		Resp.Result = MakeReferenceContextFailureResult(
			Payload,
			TEXT("invalid_request"),
			EBlueprintHelperToolStage::ParseInput,
			ValidationError.Message,
			ValidationError.Field).ToJson();
		return Resp;
	}

	FBlueprintHelperDependencyAnalysisOptions Options;
	Options.MaxResultCount = FMath::RoundToInt(MaxResults);

	FBlueprintHelperReferenceContextPack ContextPack;
	FString ErrorCode;
	FString ErrorMessage;
	if (!DependencyAnalysisService.TryBuildReferenceContext(
		Target,
		Options,
		Scope,
		bIncludeSamples,
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
		Resp.Result = MakeReferenceContextFailureResult(
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
	Result.CustomTargetJson = MakeReferenceContextTargetJson(Payload);
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

		// 读取 include_json_text 选项
		bool bIncludeJsonText = false;
		Req.Payload->TryGetBoolField(TEXT("include_json_text"), bIncludeJsonText);
		ExportReq.bIncludeJsonText = bIncludeJsonText;
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
	Resp.Result->SetObjectField(TEXT("stats"), MakeRawJsonStatsObject(ExportResult.JsonObject));

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

	// json_text — 仅在请求时包含
	if (ExportReq.bIncludeJsonText && !ExportResult.JsonText.IsEmpty())
	{
		Resp.Result->SetStringField(TEXT("json_text"), ExportResult.JsonText);
	}

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
		Resp.Result->SetObjectField(TEXT("stats"), MakeLogicStatsObject(LogicResult));
		return Resp;
	}

	TSharedPtr<FJsonObject> LogicObject = ParseJsonObject(LogicResult.OutputText);
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
		Resp.Result->SetObjectField(TEXT("stats"), MakeLogicStatsObject(LogicResult));
	}

	return Resp;
}

// ─── import_json ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleImportJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	// 接受 object 或 string json
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
			else if (JsonVal->Type == EJson::String)
			{
				ImportReq.JsonText = JsonVal->AsString();
				bHasJson = true;
			}
		}
	}

	if (!bHasJson)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺少 json 字段，或类型不被支持（需要 object 或 string）。"));
	}

	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("target_blueprint"), ImportReq.Target.BlueprintPath);
		Req.Payload->TryGetStringField(TEXT("target_graph"), ImportReq.Target.GraphName);
	}
	if (!TryReadBoolOption(Req.Payload, TEXT("compile_after_import"), ImportReq.bAutoCompile, ParseError)
		|| !TryReadBoolOption(Req.Payload, TEXT("strict"), ImportReq.bStrict, ParseError)
		|| !TryReadBoolOption(Req.Payload, TEXT("allow_partial"), ImportReq.bAllowPartial, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	// ImportService.ResolveImportJsonText() 会处。JsonObject/JsonText 解析。schema/importable 守卫
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

// ─── append_blueprint_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAppendBlueprintGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺失。"));
	}

	const FBlueprintHelperToolResultBase Result = AppendGraphService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("append_blueprint_graph 执行失败。"));

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── replace_blueprint_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReplaceBlueprintGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺失。"));
	}

	const FBlueprintHelperToolResultBase Result = ReplaceGraphService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("replace_blueprint_graph 执行失败。"));

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── patch_blueprint_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandlePatchBlueprintGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺失。"));
	}

	const FBlueprintHelperToolResultBase Result = PatchGraphService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("patch_blueprint_graph 执行失败。"));

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── merge_blueprint_graph ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleMergeBlueprintGraph(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("payload 缺失。"));
	}

	const FBlueprintHelperToolResultBase Result = MergeGraphService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("merge_blueprint_graph 执行失败。"));

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── cleanup_blueprint_helper_block ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCleanupBlueprintHelperBlock(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest, TEXT("缺少 asset_path 参数。"));
	}

	const FBlueprintHelperToolResultBase Result = CleanupBlockService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("cleanup_blueprint_helper_block 执行失败。"));

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── rollback_cleanup_transaction ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRollbackCleanupTransaction(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
		return FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest, TEXT("payload 缺失。"));

	const FBlueprintHelperToolResultBase Result = RollbackCleanupService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("rollback_cleanup_transaction 执行失败。"));

	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── convert_blueprint_helper_block_to_user_owned ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleConvertBlockToUserOwned(
	const FBlueprintHelperBridgeRequest& Req) const
{
	if (!Req.Payload.IsValid())
		return FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest, TEXT("payload 缺失。"));

	const FBlueprintHelperToolResultBase Result = ConvertBlockService.Execute(Req.Payload);

	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("convert_blueprint_helper_block_to_user_owned 执行失败。"));

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
		auto Resp = FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, SaveResult.ErrorMessage);
		Resp.Result = Fail.ToJson();
		return Resp;
	}

	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
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

// ─── Blueprint Variable Service (new handlers) ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadMemberVariables(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.ReadMemberVariables(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddMemberVariable(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.AddMemberVariable(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddMemberVariables(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.AddMemberVariables(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetMemberVariableProperties(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.SetMemberVariableProperties(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveMemberVariable(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.RemoveMemberVariable(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveMemberVariables(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.RemoveMemberVariables(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadMemberDefaults(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.ReadMemberDefaults(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetMemberDefault(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.SetMemberDefault(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetMemberDefaults(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.SetMemberDefaults(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadLocalVariables(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.ReadLocalVariables(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddLocalVariable(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.AddLocalVariable(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddLocalVariables(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.AddLocalVariables(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetLocalVariableProperties(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.SetLocalVariableProperties(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveLocalVariable(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.RemoveLocalVariable(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveLocalVariables(const FBlueprintHelperBridgeRequest& Req) const
{ auto R = VariableService.RemoveLocalVariables(Req.Payload); auto Resp = R.bOk ? FBlueprintHelperBridgeResponse::Success(Req.RequestId) : FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, R.Error.IsSet() ? R.Error->Message : TEXT("failed")); Resp.Result = R.ToJson(); return Resp; }

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
			TEXT("payload 缺少 asset_path 。widget_name 字段。"));
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
			TEXT("payload 缺少 asset_path 。widget_name 字段。"));
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

	// value 可以为空字符串（合法值），所以不检。IsEmpty
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

// ══════════════════════════════════════════════════════════。// Phase 7 。DataAsset & DataTable 操作
// ══════════════════════════════════════════════════════════。
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
			TEXT("payload 。fields 对象为空，至少需要一个字段。"));
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

// ─── create_asset ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCreateAsset(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> Payload = Req.Payload;

	FString AssetPath;
	FString AssetTypeStr;
	FString ParentClass;
	FString ValueType;
	FString CollisionStr;

	FBlueprintHelperBridgeValidationError ParseError;
	if (!TryReadStringField(Payload, TEXT("asset_path"), true, AssetPath, ParseError)
		|| !TryReadStringField(Payload, TEXT("asset_type"), true, AssetTypeStr, ParseError))
	{
		return ValidationErrorResponse(Req.RequestId, ParseError);
	}

	TryReadStringField(Payload, TEXT("parent_class"), false, ParentClass, ParseError);
	TryReadStringField(Payload, TEXT("value_type"), false, ValueType, ParseError);
	TryReadStringField(Payload, TEXT("collision"), false, CollisionStr, ParseError);

	// 解析 asset_type
	EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
	if (!FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(AssetTypeStr, ParentClass, AssetType))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest,
			FString::Printf(TEXT("不支持的 asset_type: %s"), *AssetTypeStr));
	}

	// 解析 collision policy
	EBlueprintHelperAssetCollisionPolicy Collision = EBlueprintHelperAssetCollisionPolicy::FailIfExists;
	if (CollisionStr.Equals(TEXT("reuse_if_exists"), ESearchCase::IgnoreCase))
		Collision = EBlueprintHelperAssetCollisionPolicy::ReuseIfExists;

	// 调用 AssetFactoryService
	FBlueprintHelperAssetFactoryData FactoryData = AssetFactoryService.CreateAsset(
		AssetPath, AssetType, ParentClass, ValueType, Collision);

	// 构建 ToolResultBase
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();

	// 检查创建结果
	if (FactoryData.Asset.bAlreadyExisted)
	{
		if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists
			&& FactoryData.Collision.bHandled)
		{
			// reuse_if_exists 命中同类。
FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
				TEXT("create_asset"), TraceId);
			BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
			auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
			Resp.Result = Result.ToJson();
			return Resp;
		}

		if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::FailIfExists)
		{
			// fail_if_exists 冲突
			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("create_asset"), TraceId,
				MakeAssetFactoryError(TEXT("asset_already_exists"),
					EBlueprintHelperToolStage::Preflight,
					TEXT("Target asset already exists."), false));
			BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
			auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
			Resp.Result = Result.ToJson();
			return Resp;
		}

		// reuse_if_exists 但类型不匹配
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("create_asset"), TraceId,
			MakeAssetFactoryError(TEXT("asset_type_mismatch"),
				EBlueprintHelperToolStage::Preflight,
				TEXT("Existing asset type does not match requested asset type."), false));
		BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
		auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
		Resp.Result = Result.ToJson();
		return Resp;
	}

	if (!FactoryData.Asset.bCreated)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("create_asset"), TraceId,
			MakeAssetFactoryError(TEXT("creation_failed"),
				EBlueprintHelperToolStage::Execute,
				TEXT("Failed to create asset."), false));
		BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);
		auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
		Resp.Result = Result.ToJson();
		return Resp;
	}

	// 成功
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("create_asset"), TraceId);
	BuildAssetFactoryResult(Result, FactoryData, AssetPath, AssetType);

	auto Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── read_components ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadComponents(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperReadComponentsRequest Request;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
	}

	const FBlueprintHelperToolResultBase Result = ComponentService.ReadComponents(Request);

	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("read_components 执行失败。"));
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── add_component ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddComponent(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> P = Req.Payload;
	FBlueprintHelperAddComponentRequest Request;
	if (P.IsValid())
	{
		P->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		P->TryGetStringField(TEXT("component_name"), Request.ComponentName);
		P->TryGetStringField(TEXT("component_class"), Request.ComponentClass);
		P->TryGetStringField(TEXT("parent_component"), Request.ParentComponent);
		P->TryGetStringField(TEXT("socket_name"), Request.SocketName);

		FString AttachStr;
		if (P->TryGetStringField(TEXT("attach_rule"), AttachStr))
		{
			TryParseAttachRule(AttachStr, Request.AttachRule);
		}

		FString CollisionStr;
		if (P->TryGetStringField(TEXT("name_collision_policy"), CollisionStr))
		{
			TryParseNameCollisionPolicy(CollisionStr, Request.NameCollisionPolicy);
		}
	}

	const FBlueprintHelperToolResultBase Result = ComponentService.AddComponent(Request);

	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("add_component 执行失败。"));
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── set_component_property ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetComponentProperty(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> P = Req.Payload;
	FBlueprintHelperSetComponentPropertiesRequest Request;
	Request.Mode = EBlueprintHelperComponentPropertyMode::Single;

	if (P.IsValid())
	{
		P->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		P->TryGetStringField(TEXT("component_name"), Request.ComponentName);

		FBlueprintHelperComponentPropertySetting Setting;
		P->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);

		const TSharedPtr<FJsonValue>* Value = P->Values.Find(TEXT("value"));
		if (Value) Setting.Value = *Value;

		Request.Settings.Add(MoveTemp(Setting));
	}

	const FBlueprintHelperToolResultBase Result = ComponentService.SetComponentProperty(Request);

	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("set_component_property 执行失败。"));
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── set_component_properties ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetComponentProperties(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> P = Req.Payload;
	FBlueprintHelperSetComponentPropertiesRequest Request;
	Request.Mode = EBlueprintHelperComponentPropertyMode::Batch;

	if (P.IsValid())
	{
		P->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		P->TryGetStringField(TEXT("component_name"), Request.ComponentName);

		const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
		if (P->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
		{
			for (const TSharedPtr<FJsonValue>& ItemVal : *SettingsArray)
			{
				const TSharedPtr<FJsonObject>* ItemObj = nullptr;
				if (!ItemVal.IsValid() || !ItemVal->TryGetObject(ItemObj) || !ItemObj) continue;

				FBlueprintHelperComponentPropertySetting Setting;
				(*ItemObj)->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);

				const TSharedPtr<FJsonValue>* Val = (*ItemObj)->Values.Find(TEXT("value"));
				if (Val) Setting.Value = *Val;

				Request.Settings.Add(MoveTemp(Setting));
			}
		}
	}

	const FBlueprintHelperToolResultBase Result = ComponentService.SetComponentProperties(Request);

	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("set_component_properties 执行失败。"));
	Resp.Result = Result.ToJson();
	return Resp;
}

// ─── remove_component ───

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveComponent(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const TSharedPtr<FJsonObject> P = Req.Payload;
	FBlueprintHelperRemoveComponentRequest Request;
	if (P.IsValid())
	{
		P->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		P->TryGetStringField(TEXT("component_name"), Request.ComponentName);
	}

	const FBlueprintHelperToolResultBase Result = ComponentService.RemoveComponent(Request);

	auto Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("remove_component 执行失败。"));
	Resp.Result = Result.ToJson();
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

// ─── Phase 9: Blueprint Class Settings ───

static FBlueprintHelperBridgeResponse MakeToolResultResponse(
	const FBlueprintHelperBridgeRequest& Req,
	const FBlueprintHelperToolResultBase& Result)
{
	FBlueprintHelperBridgeResponse Resp = Result.bOk
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			Result.Error.IsSet() ? Result.Error->Message : TEXT("Blueprint Class Settings operation failed."));

	Resp.Result = Result.ToJson();
	return Resp;
}

static TArray<FString> ReadStringArrayField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName)
{
	TArray<FString> Result;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Payload.IsValid() && Payload->TryGetArrayField(FieldName, Array))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			FString Item;
			if (Value.IsValid() && Value->TryGetString(Item))
			{
				Result.Add(Item);
			}
		}
	}
	return Result;
}

static TArray<FBlueprintHelperClassDefaultPropertySetting> ReadClassDefaultSettings(
	const TSharedPtr<FJsonObject>& Payload)
{
	TArray<FBlueprintHelperClassDefaultPropertySetting> Settings;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("settings"), Array))
	{
		return Settings;
	}

	for (const TSharedPtr<FJsonValue>& ItemValue : *Array)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!ItemValue.IsValid() || !ItemValue->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			continue;
		}

		FBlueprintHelperClassDefaultPropertySetting Setting;
		(*Obj)->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
		const TSharedPtr<FJsonValue>* Value = (*Obj)->Values.Find(TEXT("value"));
		if (Value)
		{
			Setting.Value = *Value;
		}
		Settings.Add(MoveTemp(Setting));
	}

	return Settings;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadClassSettings(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString AssetPath;
	Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	return MakeToolResultResponse(Req, ClassSettingsService.ReadClassSettings(AssetPath));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddImplementedInterface(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString AssetPath;
	FString InterfacePath;
	Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	Req.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath);
	return MakeToolResultResponse(Req, ClassSettingsService.AddImplementedInterface(AssetPath, InterfacePath));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAddImplementedInterfaces(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString AssetPath;
	Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	const TArray<FString> InterfacePaths = ReadStringArrayField(Req.Payload, TEXT("interface_paths"));
	return MakeToolResultResponse(Req, ClassSettingsService.AddImplementedInterfaces(AssetPath, InterfacePaths));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveImplementedInterface(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString AssetPath;
	FString InterfacePath;
	Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	Req.Payload->TryGetStringField(TEXT("interface_path"), InterfacePath);
	return MakeToolResultResponse(Req, ClassSettingsService.RemoveImplementedInterface(AssetPath, InterfacePath));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRemoveImplementedInterfaces(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString AssetPath;
	Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	const TArray<FString> InterfacePaths = ReadStringArrayField(Req.Payload, TEXT("interface_paths"));
	return MakeToolResultResponse(Req, ClassSettingsService.RemoveImplementedInterfaces(AssetPath, InterfacePaths));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetClassDefaultProperty(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString AssetPath;
	FString PropertyPath;
	Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	Req.Payload->TryGetStringField(TEXT("property_path"), PropertyPath);

	TSharedPtr<FJsonValue> Value;
	const TSharedPtr<FJsonValue>* FoundValue = Req.Payload->Values.Find(TEXT("value"));
	if (FoundValue) { Value = *FoundValue; }

	return MakeToolResultResponse(Req, ClassSettingsService.SetClassDefaultProperty(AssetPath, PropertyPath, Value));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetClassDefaultProperties(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString AssetPath;
	Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	const TArray<FBlueprintHelperClassDefaultPropertySetting> Settings = ReadClassDefaultSettings(Req.Payload);
	return MakeToolResultResponse(Req, ClassSettingsService.SetClassDefaultProperties(AssetPath, Settings));
}
