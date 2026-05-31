// BlueprintHelper Bridge Layer 。命令路由实现

#include "Entry/Bridge/BlueprintHelperBridgeRouter.h"
#include "Entry/Bridge/BlueprintHelperBridgeProtocol.h"
#include "Entry/Bridge/Utils/BlueprintHelperBridgeUtils.h"
#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "Entry/BlueprintHelper.h"
#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"
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
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"
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
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Shared/BlueprintVariables/BlueprintHelperBlueprintVariableTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Utils/BlueprintHelperToolTimingUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperBridgeRouterLocalUtils
{
public:
	template <typename TCallable>
	static FBlueprintHelperBridgeResponse ExecuteRouteWithTiming(
		const FBlueprintHelperBridgeRequest& Request,
		TCallable Callable)
	{
		FBlueprintHelperToolTimingUtils::FTimingTrace TimingTrace =
			FBlueprintHelperToolTimingUtils::StartTrace(
				Request.Command,
				ShouldIncludeBridgeTiming(Request.Command, Request.Payload),
				TEXT("ue_bridge_router"));
		const double RouteStageStart = FBlueprintHelperToolTimingUtils::StartStage(TimingTrace);
		FBlueprintHelperToolTimingUtils::FTimingTrace* PreviousTrace =
			FBlueprintHelperToolTimingUtils::GetCurrentTrace();
		FBlueprintHelperToolTimingUtils::SetCurrentTrace(&TimingTrace);
		FBlueprintHelperBridgeResponse Response = Callable();
		FBlueprintHelperToolTimingUtils::SetCurrentTrace(PreviousTrace);
		FBlueprintHelperToolTimingUtils::FinishStage(TimingTrace, TEXT("route_execute"), RouteStageStart);
		FBlueprintHelperToolTimingUtils::AttachTimingToBridgeResult(Response.Result, TimingTrace);
		ApplyGraphWriteValidationPolicy(Request.Command, Request.Payload, Response.Result);
		FBlueprintHelperReadContextOutputLimiter::ApplyToBridgeResult(Request.Command, Response.Result);
		return Response;
	}

	static bool ShouldIncludeBridgeTiming(
		const FString& Command,
		const TSharedPtr<FJsonObject>& Payload)
	{
		bool bIncludeTiming = false;
		if (!Payload.IsValid() || !Payload->TryGetBoolField(TEXT("include_timing"), bIncludeTiming) || !bIncludeTiming)
		{
			return false;
		}

		return IsReadTimingCommand(Command);
	}

	static bool IsReadTimingCommand(const FString& Command)
	{
		static const TSet<FString> ReadTimingCommands = {
			TEXT("get_editor_context"),
			TEXT("get_runtime_profile"),
			TEXT("diagnostics_runtime"),
			TEXT("get_debug_case"),
			TEXT("list_debug_cases"),
			TEXT("export_debug_bundle"),
			TEXT("read_reference_context"),
			TEXT("read_function_chain_context"),
			TEXT("read_blueprint_logic_md"),
			TEXT("read_blueprint_logic_json"),
			TEXT("export_to_json"),
			TEXT("export_logic"),
			TEXT("get_asset_info"),
			TEXT("list_assets"),
			TEXT("search_assets"),
			TEXT("list_graphs"),
			TEXT("list_variables"),
			TEXT("list_event_dispatchers"),
			TEXT("read_blueprint_member_variables"),
			TEXT("read_blueprint_member_defaults"),
			TEXT("read_blueprint_local_variables"),
			TEXT("get_widget_tree"),
			TEXT("get_widget_properties"),
			TEXT("get_datatable_rows"),
			TEXT("get_object_properties"),
			TEXT("read_components"),
			TEXT("read_class_settings"),
			TEXT("project_graphwrite_spawner_evidence")
		};
		return ReadTimingCommands.Contains(Command);
	}

	static double StartRouteStage(FBlueprintHelperToolTimingUtils::FTimingTrace* TimingTrace)
	{
		return TimingTrace ? FBlueprintHelperToolTimingUtils::StartStage(*TimingTrace) : 0.0;
	}

	static void FinishRouteStage(
		FBlueprintHelperToolTimingUtils::FTimingTrace* TimingTrace,
		const FString& Name,
		double StartedAtSeconds)
	{
		if (TimingTrace)
		{
			FBlueprintHelperToolTimingUtils::FinishStage(*TimingTrace, Name, StartedAtSeconds);
		}
	}

	static void AddRouteCounter(
		FBlueprintHelperToolTimingUtils::FTimingTrace* TimingTrace,
		const FString& Name,
		int64 Value)
	{
		if (TimingTrace)
		{
			FBlueprintHelperToolTimingUtils::AddCounter(*TimingTrace, Name, Value);
		}
	}

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

	static void SetStringDefaultIfMissing(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		const FString& Value)
	{
		if (Payload.IsValid() && !Value.IsEmpty() && !Payload->HasField(FieldName))
		{
			Payload->SetStringField(FieldName, Value);
		}
	}

	static void SetBoolDefaultIfMissing(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bValue)
	{
		if (Payload.IsValid() && !Payload->HasField(FieldName))
		{
			Payload->SetBoolField(FieldName, bValue);
		}
	}

	static TSharedPtr<FJsonObject> GetOrCreateOptionsObject(const TSharedPtr<FJsonObject>& Payload)
	{
		if (!Payload.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
		if (Payload->TryGetObjectField(TEXT("options"), OptionsObject) && OptionsObject && OptionsObject->IsValid())
		{
			return *OptionsObject;
		}

		TSharedRef<FJsonObject> NewOptions = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("options"), NewOptions);
		return NewOptions;
	}

	static void ApplyToolClusterSettingDefaults(
		EBlueprintHelperBridgeRouteCluster Cluster,
		const TSharedPtr<FJsonObject>& Payload)
	{
		if (!Payload.IsValid())
		{
			return;
		}

		switch (Cluster)
		{
		case EBlueprintHelperBridgeRouteCluster::AssetFactory:
		{
			const FBlueprintHelperAssetFactoryToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadAssetFactoryPolicy();
			SetStringDefaultIfMissing(Payload, TEXT("parent_class"), Policy.DefaultParentClass);
			SetStringDefaultIfMissing(Payload, TEXT("value_type"), Policy.DefaultValueType);
			SetStringDefaultIfMissing(Payload, TEXT("collision_policy"), Policy.DefaultCollisionPolicy);
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			break;
		}
		case EBlueprintHelperBridgeRouteCluster::Component:
		{
			const FBlueprintHelperComponentToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadComponentPolicy();
			SetStringDefaultIfMissing(Payload, TEXT("attach_rule"), Policy.DefaultAttachRule);
			SetStringDefaultIfMissing(Payload, TEXT("name_collision_policy"), Policy.DefaultNameCollisionPolicy);
			SetStringDefaultIfMissing(Payload, TEXT("property_mode"), Policy.DefaultPropertyMode);
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			break;
		}
		case EBlueprintHelperBridgeRouteCluster::ClassSettings:
		{
			const FBlueprintHelperClassSettingsToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadClassSettingsPolicy();
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			SetBoolDefaultIfMissing(Payload, TEXT("validation_should_compile"), Policy.bValidationShouldCompile);
			SetBoolDefaultIfMissing(Payload, TEXT("validation_should_save"), Policy.bValidationShouldSave);
			break;
		}
		case EBlueprintHelperBridgeRouteCluster::BlueprintVariables:
		{
			const FBlueprintHelperBlueprintVariablesToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadBlueprintVariablesPolicy();
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			break;
		}
		case EBlueprintHelperBridgeRouteCluster::ObjectProperty:
		{
			const FBlueprintHelperObjectPropertyToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadObjectPropertyPolicy();
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			break;
		}
		case EBlueprintHelperBridgeRouteCluster::DataTable:
		{
			const FBlueprintHelperDataTableToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadDataTablePolicy();
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			SetBoolDefaultIfMissing(Payload, TEXT("write_requires_row_struct"), Policy.bWriteRequiresRowStruct);
			break;
		}
		case EBlueprintHelperBridgeRouteCluster::UMGWidget:
		{
			const FBlueprintHelperUmgWidgetToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadUmgWidgetPolicy();
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			SetBoolDefaultIfMissing(Payload, TEXT("asset_path_required"), Policy.bAssetPathRequired);
			break;
		}
		case EBlueprintHelperBridgeRouteCluster::GraphWrite:
		{
			const FBlueprintHelperGraphWriteToolClusterPolicy Policy =
				FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy();
			SetBoolDefaultIfMissing(Payload, TEXT("dry_run"), Policy.bDryRun);
			TSharedPtr<FJsonObject> Options = GetOrCreateOptionsObject(Payload);
			SetBoolDefaultIfMissing(Options, TEXT("dry_run"), Policy.bDryRun);
			SetBoolDefaultIfMissing(Options, TEXT("strict"), Policy.bStrict);
			SetBoolDefaultIfMissing(Options, TEXT("create_missing_variables"), Policy.bCreateMissingVariables);
			SetBoolDefaultIfMissing(Options, TEXT("reconstruct_existing_nodes"), Policy.bReconstructExistingNodes);
			SetBoolDefaultIfMissing(Options, TEXT("compile"), Policy.bCompile);
			SetBoolDefaultIfMissing(Options, TEXT("save"), Policy.bSave);
			SetStringDefaultIfMissing(Options, TEXT("layout"), Policy.Layout);
			break;
		}
		default:
			break;
		}
	}

	static bool ReadGraphWriteBoolOption(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		bool bDefaultValue)
	{
		bool bValue = bDefaultValue;
		const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
		if (Payload.IsValid() &&
			Payload->TryGetObjectField(TEXT("options"), OptionsObject) &&
			OptionsObject &&
			OptionsObject->IsValid() &&
			(*OptionsObject)->HasField(FieldName))
		{
			(*OptionsObject)->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	static void ApplyGraphWriteValidationPolicy(
		const FString& Command,
		const TSharedPtr<FJsonObject>& Payload,
		const TSharedPtr<FJsonObject>& ResultJson)
	{
		if (!ResultJson.IsValid() || !FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(Command))
		{
			return;
		}

		const FBlueprintHelperGraphWriteToolClusterPolicy Policy =
			FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy();
		const bool bShouldCompile = ReadGraphWriteBoolOption(Payload, TEXT("compile"), Policy.bCompile);
		const bool bShouldSave = ReadGraphWriteBoolOption(Payload, TEXT("save"), Policy.bSave);

		TSharedPtr<FJsonObject> ValidationObject;
		const TSharedPtr<FJsonObject>* ExistingValidationObject = nullptr;
		if (ResultJson->TryGetObjectField(TEXT("validation"), ExistingValidationObject) &&
			ExistingValidationObject &&
			ExistingValidationObject->IsValid())
		{
			ValidationObject = *ExistingValidationObject;
		}
		else
		{
			ValidationObject = MakeShared<FJsonObject>();
			ResultJson->SetObjectField(TEXT("validation"), ValidationObject.ToSharedRef());
		}

		ValidationObject->SetBoolField(TEXT("should_compile"), bShouldCompile);
		ValidationObject->SetBoolField(TEXT("should_save"), bShouldSave);
	}

};

FBlueprintHelperBridgeRouter::FBlueprintHelperBridgeRouter(
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
	const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry,
	const FBlueprintHelperCompileAssetService& InCompileAssetService,
	const FBlueprintHelperBlueprintVariableService& InVariableService,
	const FBlueprintHelperReviewStoreService& InReviewStoreService)
	: ExportService(InExport)
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
	, GraphWriteRoutes(InGraphWriteRegistry)
	, VariableService(InVariableService)
	, BlueprintVariablesRoutes(InVariableService)
	, TaskRuntimeService(
		InGraphWriteRegistry,
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

	FBlueprintHelperBridgeRouterLocalUtils::ApplyToolClusterSettingDefaults(RoutePlan.Cluster, Request.Payload);

#define BLUEPRINTHELPER_ROUTE(CommandText, ClusterValue, Handler) \
	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::ClusterValue && Request.Command == TEXT(CommandText)) \
	{ \
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return Handler(Request); }); \
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
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return BlueprintVariablesRoutes.HandleRequest(Request); });
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::UMGWidget &&
		FBlueprintHelperUMGWidgetBridgeRoutes::IsUMGWidgetCommand(Request.Command))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return UMGWidgetRoutes.HandleRequest(Request); });
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::DataTable &&
		FBlueprintHelperDataTableBridgeRoutes::IsDataTableCommand(Request.Command))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return DataTableRoutes.HandleRequest(Request); });
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::ObjectProperty &&
		FBlueprintHelperObjectPropertyBridgeRoutes::IsObjectPropertyCommand(Request.Command))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return ObjectPropertyRoutes.HandleRequest(Request); });
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
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return AssetFactoryRoutes.HandleRequest(Request); });
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::Component &&
		FBlueprintHelperComponentBridgeRoutes::IsComponentCommand(Request.Command))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return ComponentRoutes.HandleRequest(Request); });
	}

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::ClassSettings &&
		FBlueprintHelperClassSettingsBridgeRoutes::IsClassSettingsCommand(Request.Command))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return ClassSettingsRoutes.HandleRequest(Request); });
	}

	BLUEPRINTHELPER_ROUTE("preview_task_plan", TaskRuntime, HandlePreviewTaskPlan)
	BLUEPRINTHELPER_ROUTE("execute_task_plan", TaskRuntime, HandleExecuteTaskPlan)
	BLUEPRINTHELPER_ROUTE("get_task_run_journal", TaskRuntime, HandleGetTaskRunJournal)

	if (RoutePlan.Cluster == EBlueprintHelperBridgeRouteCluster::GraphWrite &&
		FBlueprintHelperGraphWriteBridgeRoutes::IsGraphWriteCommand(Request.Command))
	{
		return FBlueprintHelperBridgeRouterLocalUtils::ExecuteRouteWithTiming(Request, [&]() { return GraphWriteRoutes.HandleRequest(Request); });
	}


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
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("get_rule_markdown"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("markdown"), FBlueprintHelperModule::Get().GetJsonToBlueprintRuleMarkdown());
	Result.Data = Data;
	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Result.ToJson();
	return Resp;
}


FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetEditorContext(
	const FBlueprintHelperBridgeRequest& Req) const
{
	const FBlueprintHelperEditorContext Context = ContextService.GetContext();
	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetStringField(TEXT("active_blueprint_path"), Context.ActiveBlueprintPath);
	Resp.Result->SetStringField(TEXT("active_graph_name"), Context.ActiveGraphName);
	Resp.Result->SetStringField(TEXT("blueprint_display_name"), Context.BlueprintDisplayName);
	Resp.Result->SetNumberField(TEXT("node_count"), Context.NodeCount);
	Resp.Result->SetBoolField(TEXT("is_compiled"), Context.bIsCompiled);
	Resp.Result->SetNumberField(TEXT("blueprint_status"), Context.BlueprintStatus);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRequestWriteSession(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperWriteSessionRequest Request;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("reason"), Request.Reason);
		Req.Payload->TryGetStringField(TEXT("scope"), Request.Scope);
		int32 TtlSeconds = Request.TtlSeconds;
		Req.Payload->TryGetNumberField(TEXT("ttl_seconds"), TtlSeconds);
		Request.TtlSeconds = TtlSeconds;
		Request.AssetPaths = UBlueprintHelperBridgeUtils::ReadStringArrayField(Req.Payload, TEXT("asset_paths"));
	}

	FString Error;
	const TOptional<FBlueprintHelperWriteSessionGrant> Grant =
		FBlueprintHelperWriteAuthorizationService::Get().RequestSession(Request, Error);
	if (!Grant.IsSet())
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::Unauthorized,
			Error.IsEmpty() ? TEXT("write session request denied") : Error);
	}

	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Grant->ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetRuntimeProfile(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = RuntimeProfileService.GetRuntimeProfile().ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleDiagnosticsRuntime(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = DiagnosticsService.RunRuntimeDiagnostics().ToJson();
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetDebugCase(
	const FBlueprintHelperBridgeRequest& Req) const
{
	return UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(Req, DebugEntryService.GetDebugCaseSummaryResult(Req.Payload));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListDebugCases(
	const FBlueprintHelperBridgeRequest& Req) const
{
	return UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(Req, DebugEntryService.GetDebugCaseListResult(Req.Payload));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExportDebugBundle(
	const FBlueprintHelperBridgeRequest& Req) const
{
	return UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(Req, DebugEntryService.ExportDebugBundleSummaryResult(Req.Payload));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadReferenceContext(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperDependencyAnalysisOptions Options;
	const FBlueprintHelperSignatureToolClusterPolicy SignaturePolicy =
		FBlueprintHelperToolClusterConfigResolver::LoadSignaturePolicy();
	Options.SearchScope = SignaturePolicy.ReferenceContextSearchScope;
	Options.ResolutionPolicy = SignaturePolicy.ReferenceContextResolutionPolicy;
	Options.Detail = SignaturePolicy.ReferenceContextDetail;
	Options.MaxResultCount = SignaturePolicy.ReferenceContextMaxResults;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("search_scope"), Options.SearchScope);
		Req.Payload->TryGetStringField(TEXT("resolution_policy"), Options.ResolutionPolicy);
		Req.Payload->TryGetStringField(TEXT("detail"), Options.Detail);
		Req.Payload->TryGetNumberField(TEXT("max_results"), Options.MaxResultCount);
	}

	FBlueprintHelperReferenceContextPack ContextPack;
	FString ErrorCode;
	FString ErrorMessage;
	if (!DependencyAnalysisService.TryBuildReferenceContext(
		FBlueprintHelperBridgeRouterLocalUtils::ReadReferenceContextTarget(Req.Payload),
		Options,
		ContextPack,
		ErrorCode,
		ErrorMessage))
	{
		FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMessage);
		Resp.Result = FBlueprintHelperBridgeRouterLocalUtils::MakeReferenceContextFailureResult(
			Req.Payload,
			ErrorCode.IsEmpty() ? TEXT("reference_context_failed") : ErrorCode,
			EBlueprintHelperToolStage::Execute,
			ErrorMessage).ToJson();
		return Resp;
	}

	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = ContextPack.ToJson();
	FBlueprintHelperReadContextOutputLimiter::ApplyToBridgeResult(Req.Command, Resp.Result);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadFunctionChainContext(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperFunctionChainContextRequest Request;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		Req.Payload->TryGetStringField(TEXT("target_type"), Request.TargetType);
		Req.Payload->TryGetStringField(TEXT("target_name"), Request.TargetName);
		Req.Payload->TryGetStringField(TEXT("graph_name"), Request.GraphName);
		Req.Payload->TryGetNumberField(TEXT("max_depth"), Request.MaxDepth);
		Req.Payload->TryGetBoolField(TEXT("include_data_dependencies"), Request.bIncludeDataDependencies);
		Req.Payload->TryGetBoolField(TEXT("expand_cross_asset"), Request.bExpandCrossAsset);
	}

	FBlueprintHelperFunctionChainContextPack ContextPack;
	FString ErrorCode;
	FString ErrorMessage;
	if (!FunctionChainContextService.TryBuildFunctionChainContext(Request, ContextPack, ErrorCode, ErrorMessage))
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::ExecutionFailed,
			ErrorMessage.IsEmpty() ? ErrorCode : ErrorMessage);
	}

	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = ContextPack.ToJson();
	FBlueprintHelperReadContextOutputLimiter::ApplyToBridgeResult(Req.Command, Resp.Result);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadBlueprintLogicMd(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperToolTimingUtils::FTimingTrace* TimingTrace =
		FBlueprintHelperToolTimingUtils::GetCurrentTrace();

	const double ParseStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	const FBlueprintHelperTargetRef Target = UBlueprintHelperBridgeUtils::ReadTargetRefFromPayload(Req.Payload);
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.read_request_parse"),
		ParseStageStart);

	FBlueprintHelperLogicReadRequestSnapshotCache RequestCache;
	FBlueprintHelperLogicReadSnapshot Snapshot;
	FString SnapshotError;
	const double SnapshotStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	const bool bSnapshotOk = LogicMdReadService.BuildSnapshot(Target, Snapshot, SnapshotError, &RequestCache);
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.snapshot_read"),
		SnapshotStageStart);
	FBlueprintHelperBridgeRouterLocalUtils::AddRouteCounter(
		TimingTrace,
		TEXT("ue.read_snapshot_cache_hit"),
		RequestCache.GetHitCount());
	FBlueprintHelperBridgeRouterLocalUtils::AddRouteCounter(
		TimingTrace,
		TEXT("ue.read_snapshot_cache_miss"),
		RequestCache.GetMissCount());

	const double FormatStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	FBlueprintHelperLogicMdData Data;
	if (bSnapshotOk)
	{
		Data = LogicMdReadService.FormatSnapshot(Snapshot);
	}
	else
	{
		Data.Scope = FBlueprintHelperLogicReadSnapshotService::TargetTypeToScope(Target.TargetType);
		Data.Markdown = TEXT("(导出失败)");
		Data.bImportable = false;
	}
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.format_output"),
		FormatStageStart);

	const double ResponseStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Data.ToJson();
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.response_wrap"),
		ResponseStageStart);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadBlueprintLogicJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperToolTimingUtils::FTimingTrace* TimingTrace =
		FBlueprintHelperToolTimingUtils::GetCurrentTrace();

	const double ParseStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	const FBlueprintHelperTargetRef Target = UBlueprintHelperBridgeUtils::ReadTargetRefFromPayload(Req.Payload);
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.read_request_parse"),
		ParseStageStart);

	FBlueprintHelperLogicReadRequestSnapshotCache RequestCache;
	FBlueprintHelperLogicReadSnapshot Snapshot;
	FString SnapshotError;
	const double SnapshotStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	const bool bSnapshotOk = LogicJsonReadService.BuildSnapshot(Target, Snapshot, SnapshotError, &RequestCache);
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.snapshot_read"),
		SnapshotStageStart);
	FBlueprintHelperBridgeRouterLocalUtils::AddRouteCounter(
		TimingTrace,
		TEXT("ue.read_snapshot_cache_hit"),
		RequestCache.GetHitCount());
	FBlueprintHelperBridgeRouterLocalUtils::AddRouteCounter(
		TimingTrace,
		TEXT("ue.read_snapshot_cache_miss"),
		RequestCache.GetMissCount());

	const double FormatStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	FBlueprintHelperLogicJsonData Data = bSnapshotOk
		? LogicJsonReadService.FormatSnapshot(Snapshot)
		: FBlueprintHelperLogicJsonData();
	if (!bSnapshotOk)
	{
		Data.Scope = FBlueprintHelperLogicReadSnapshotService::TargetTypeToScope(Target.TargetType);
	}
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.format_output"),
		FormatStageStart);

	const double ResponseStageStart = FBlueprintHelperBridgeRouterLocalUtils::StartRouteStage(TimingTrace);
	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = Data.ToJson();
	FBlueprintHelperBridgeRouterLocalUtils::FinishRouteStage(
		TimingTrace,
		TEXT("ue.route.response_wrap"),
		ResponseStageStart);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleValidateJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString JsonText;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("json"), JsonText);
		if (JsonText.IsEmpty())
		{
			Req.Payload->TryGetStringField(TEXT("content"), JsonText);
		}
	}
	const FBlueprintHelperValidationResult Result = ValidationService.Validate(JsonText);
	FBlueprintHelperBridgeResponse Resp = Result.bValid
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::InvalidRequest, TEXT("json validation failed"));
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetBoolField(TEXT("valid"), Result.bValid);
	Resp.Result->SetStringField(TEXT("detected_version"), Result.DetectedVersion);
	Resp.Result->SetNumberField(TEXT("error_count"), Result.Diagnostics.ErrorCount);
	Resp.Result->SetNumberField(TEXT("warning_count"), Result.Diagnostics.WarningCount);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExportToJson(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperExportRequest Request;
	Request.Target.BlueprintPath = UBlueprintHelperBridgeUtils::ReadTargetRefFromPayload(Req.Payload).BlueprintPath;
	if (Request.Target.BlueprintPath.IsEmpty())
	{
		Request.Target.BlueprintPath = UBlueprintHelperBridgeUtils::ReadTargetRefFromPayload(Req.Payload).AssetPath;
	}
	Request.Target.GraphName = UBlueprintHelperBridgeUtils::ReadTargetRefFromPayload(Req.Payload).Graph;
	FString Scope;
	if (Req.Payload.IsValid() && Req.Payload->TryGetStringField(TEXT("scope"), Scope))
	{
		if (Scope.Equals(TEXT("full_blueprint"), ESearchCase::IgnoreCase)) { Request.Scope = EBlueprintHelperExportScope::FullBlueprint; }
		else if (Scope.Equals(TEXT("selection"), ESearchCase::IgnoreCase)) { Request.Scope = EBlueprintHelperExportScope::Selection; }
	}

	const FBlueprintHelperExportResult Result = ExportService.Export(Request);
	FBlueprintHelperBridgeResponse Resp = Result.bSuccess
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, TEXT("export failed"));
	Resp.Result = MakeShared<FJsonObject>();
	Resp.Result->SetBoolField(TEXT("success"), Result.bSuccess);
	Resp.Result->SetStringField(TEXT("effective_scope"), Result.EffectiveScope);
	if (Result.JsonObject.IsValid())
	{
		Resp.Result->SetObjectField(TEXT("json"), Result.JsonObject.ToSharedRef());
	}
	Resp.Result->SetNumberField(TEXT("error_count"), Result.Diagnostics.ErrorCount);
	Resp.Result->SetNumberField(TEXT("warning_count"), Result.Diagnostics.WarningCount);
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExportLogic(
	const FBlueprintHelperBridgeRequest& Req) const
{
	return HandleReadBlueprintLogicJson(Req);
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandlePreviewTaskPlan(
	const FBlueprintHelperBridgeRequest& Req) const
{
	return UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(Req, TaskRuntimeService.PreviewTaskPlan(Req.Payload));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExecuteTaskPlan(
	const FBlueprintHelperBridgeRequest& Req) const
{
	return UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(Req, TaskRuntimeService.ExecuteTaskPlan(Req.Payload));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetTaskRunJournal(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString TaskRunId;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("task_run_id"), TaskRunId);
	}
	return UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(Req, TaskRuntimeService.GetTaskRunJournal(TaskRunId));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCompileBlueprintAsset(
	const FBlueprintHelperBridgeRequest& Req) const
{
	return UBlueprintHelperBridgeUtils::MakeToolResultBridgeResponse(Req, CompileAssetService.Execute(Req.Payload));
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleQueryReviewRecords(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FBlueprintHelperReviewRecordQuery Query;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("asset_path"), Query.AssetPathFilter);
		Req.Payload->TryGetStringField(TEXT("archive_session_id"), Query.ArchiveSessionIdFilter);
		Req.Payload->TryGetStringField(TEXT("task_run_id"), Query.TaskRunIdFilter);
	}
	const TArray<FBlueprintHelperReviewRecord> Records = ReviewStoreService.QueryReviewRecords(Query);
	FBlueprintHelperBridgeResponse Resp = FBlueprintHelperBridgeResponse::Success(Req.RequestId);
	Resp.Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		Items.Add(MakeShared<FJsonValueObject>(ReviewStoreService.BuildReviewRecordSummaryArtifact(Record)));
	}
	Resp.Result->SetArrayField(TEXT("records"), Items);
	Resp.Result->SetNumberField(TEXT("count"), Records.Num());
	return Resp;
}

FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleApplyReviewAction(
	const FBlueprintHelperBridgeRequest& Req) const
{
	FString Action;
	FString ChangeId;
	FString AssetPath;
	if (Req.Payload.IsValid())
	{
		Req.Payload->TryGetStringField(TEXT("action"), Action);
		Req.Payload->TryGetStringField(TEXT("change_id"), ChangeId);
		Req.Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
	}

	TArray<FBlueprintHelperReviewVisibleChange> Changes = ReviewStoreService.LoadPendingVisibleChanges(AssetPath);
	const FBlueprintHelperReviewVisibleChange* MatchedChange = nullptr;
	for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
	{
		if (Change.ChangeId == ChangeId)
		{
			MatchedChange = &Change;
			break;
		}
	}
	if (!MatchedChange)
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("review change not found"));
	}

	FBlueprintHelperReviewActionService ActionService(&DebugEntryService);
	FBlueprintHelperReviewActionResult Result;
	if (Action.Equals(TEXT("accept"), ESearchCase::IgnoreCase))
	{
		Result = ActionService.AcceptVisibleChange(*MatchedChange);
	}
	else if (Action.Equals(TEXT("reject"), ESearchCase::IgnoreCase))
	{
		Result = ActionService.RejectVisibleChange(*MatchedChange);
	}
	else
	{
		return FBlueprintHelperBridgeResponse::Error(
			Req.RequestId,
			EBlueprintHelperBridgeError::InvalidRequest,
			TEXT("unsupported review action"));
	}

	FBlueprintHelperBridgeResponse Resp = Result.bSucceeded
		? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
		: FBlueprintHelperBridgeResponse::Error(Req.RequestId, EBlueprintHelperBridgeError::ExecutionFailed, Result.Message);
	Resp.Result = UBlueprintHelperBridgeUtils::ReviewActionResultToJson(Result);
	return Resp;
}

// compile_blueprint

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
