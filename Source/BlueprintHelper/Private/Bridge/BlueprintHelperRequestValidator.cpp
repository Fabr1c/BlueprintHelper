// BlueprintHelper Bridge Layer — request validation helpers

#include "Bridge/BlueprintHelperRequestValidator.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Parse.h"

namespace
{
	enum class EBlueprintHelperJsonExpectedType : uint8
	{
		String,
		Bool,
		Number,
		Object,
		Array
	};

	struct FBlueprintHelperFieldRule
	{
		const TCHAR* FieldName;
		EBlueprintHelperJsonExpectedType Type;
		bool bRequired = false;
	};

	FString ExpectedTypeToString(EBlueprintHelperJsonExpectedType Type)
	{
		switch (Type)
		{
		case EBlueprintHelperJsonExpectedType::String: return TEXT("string");
		case EBlueprintHelperJsonExpectedType::Bool: return TEXT("bool");
		case EBlueprintHelperJsonExpectedType::Number: return TEXT("number");
		case EBlueprintHelperJsonExpectedType::Object: return TEXT("object");
		case EBlueprintHelperJsonExpectedType::Array: return TEXT("array");
		default: return TEXT("unknown");
		}
	}

	FString ActualJsonTypeToString(const TSharedPtr<FJsonValue>& Value)
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

	bool MatchesExpectedType(const TSharedPtr<FJsonValue>& Value, EBlueprintHelperJsonExpectedType Type)
	{
		if (!Value.IsValid())
		{
			return false;
		}

		switch (Type)
		{
		case EBlueprintHelperJsonExpectedType::String: return Value->Type == EJson::String;
		case EBlueprintHelperJsonExpectedType::Bool: return Value->Type == EJson::Boolean;
		case EBlueprintHelperJsonExpectedType::Number: return Value->Type == EJson::Number;
		case EBlueprintHelperJsonExpectedType::Object: return Value->Type == EJson::Object;
		case EBlueprintHelperJsonExpectedType::Array: return Value->Type == EJson::Array;
		default: return false;
		}
	}

	void SetValidationError(
		FBlueprintHelperBridgeValidationError& OutError,
		const FString& Field,
		const FString& Expected,
		const FString& Actual)
	{
		OutError.Code = TEXT("invalid_request");
		OutError.Field = Field;
		OutError.ExpectedType = Expected;
		OutError.ActualType = Actual;
		OutError.Message = FString::Printf(TEXT("%s 必须是 %s，实际为 %s。"),
			*Field, *Expected, *Actual);
	}

	bool ValidateFieldRule(
		const TSharedPtr<FJsonObject>& Payload,
		const FBlueprintHelperFieldRule& Rule,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		const FString FieldName(Rule.FieldName);
		const TSharedPtr<FJsonValue>* FoundValue = Payload->Values.Find(FieldName);
		if (!FoundValue)
		{
			if (Rule.bRequired)
			{
				SetValidationError(OutError, TEXT("payload.") + FieldName, ExpectedTypeToString(Rule.Type), TEXT("missing"));
				return false;
			}
			return true;
		}

		if (!MatchesExpectedType(*FoundValue, Rule.Type))
		{
			SetValidationError(OutError, TEXT("payload.") + FieldName,
				ExpectedTypeToString(Rule.Type), ActualJsonTypeToString(*FoundValue));
			return false;
		}
		return true;
	}

	bool ValidateRules(
		const TSharedPtr<FJsonObject>& Payload,
		TArrayView<const FBlueprintHelperFieldRule> Rules,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		for (const FBlueprintHelperFieldRule& Rule : Rules)
		{
			if (!ValidateFieldRule(Payload, Rule, OutError))
			{
				return false;
			}
		}
		return true;
	}

	bool CommandEquals(const FString& Command, const TCHAR* Expected)
	{
		return Command.Equals(Expected, ESearchCase::IgnoreCase);
	}
}

bool FBlueprintHelperRequestValidator::NormalizeExportScope(
	const FString& InScope,
	EBlueprintHelperExportScope& OutScope,
	FString& OutEffectiveScope,
	FString& OutError)
{
	const FString Scope = InScope.IsEmpty() ? TEXT("graph") : InScope.ToLower();
	if (Scope == TEXT("graph") || Scope == TEXT("full_graph") || Scope == TEXT("single_graph"))
	{
		OutScope = EBlueprintHelperExportScope::SingleGraph;
		OutEffectiveScope = TEXT("graph");
		return true;
	}
	if (Scope == TEXT("blueprint") || Scope == TEXT("full_blueprint"))
	{
		OutScope = EBlueprintHelperExportScope::FullBlueprint;
		OutEffectiveScope = TEXT("blueprint");
		return true;
	}
	if (Scope == TEXT("selection"))
	{
		OutScope = EBlueprintHelperExportScope::Selection;
		OutEffectiveScope = TEXT("selection");
		return true;
	}

	OutError = FString::Printf(TEXT("不支持的 scope: %s。有效值为 graph、blueprint、selection。"), *InScope);
	return false;
}

bool FBlueprintHelperRequestValidator::ValidatePayloadForCommand(
	const FString& Command,
	const TSharedPtr<FJsonObject>& Payload,
	FBlueprintHelperBridgeValidationError& OutError)
{
	if (!Payload.IsValid())
	{
		OutError.Code = TEXT("invalid_request");
		OutError.Field = TEXT("payload");
		OutError.ExpectedType = TEXT("object");
		OutError.ActualType = TEXT("missing");
		OutError.Message = TEXT("payload 必须是对象。");
		return false;
	}

	const FBlueprintHelperFieldRule TargetRules[] = {
		{TEXT("target_blueprint"), EBlueprintHelperJsonExpectedType::String, false},
		{TEXT("target_graph"), EBlueprintHelperJsonExpectedType::String, false},
	};
	if (!ValidateRules(Payload, TargetRules, OutError))
	{
		return false;
	}

	if (CommandEquals(Command, TEXT("export_to_json")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("scope"), EBlueprintHelperJsonExpectedType::String, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("export_logic")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("scope"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("format"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("detail"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("include_data_dependencies"), EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_orphans"), EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_node_ids"), EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_positions"), EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_raw_node_types"), EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("validate_json")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("json"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("import_json")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("json"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("compile_after_import"), EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("strict"), EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("allow_partial"), EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("import_agent_graph")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("schema"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("version"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("target_blueprint"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("target_graph"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("mode"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("layout"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("declarations"), EBlueprintHelperJsonExpectedType::Object, false},
			{TEXT("nodes"), EBlueprintHelperJsonExpectedType::Array, true},
			{TEXT("links"), EBlueprintHelperJsonExpectedType::Array, false},
			{TEXT("options"), EBlueprintHelperJsonExpectedType::Object, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("compile_blueprint")))
	{
		return true;
	}
	if (CommandEquals(Command, TEXT("open_asset")) || CommandEquals(Command, TEXT("save_asset"))
		|| CommandEquals(Command, TEXT("get_asset_info")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("list_assets")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("path"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("class_filter"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("name_filter"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("recursive"), EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("max_results"), EBlueprintHelperJsonExpectedType::Number, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("search_assets")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("path"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("class_filter"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("query"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("max_results"), EBlueprintHelperJsonExpectedType::Number, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("add_variable")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("pin_type"), EBlueprintHelperJsonExpectedType::Object, false},
			{TEXT("default_value"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("category"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("flags"), EBlueprintHelperJsonExpectedType::Object, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("add_graph")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("graph_type"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("inputs"), EBlueprintHelperJsonExpectedType::Array, false},
			{TEXT("outputs"), EBlueprintHelperJsonExpectedType::Array, false},
			{TEXT("is_pure"), EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("add_event_dispatcher")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("params"), EBlueprintHelperJsonExpectedType::Array, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("remove_variable")) || CommandEquals(Command, TEXT("remove_graph")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("delete_nodes")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("node_ids"), EBlueprintHelperJsonExpectedType::Array, true},
			{TEXT("strict"), EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("add_widget")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_class"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("parent_name"), EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("widget_name"), EBlueprintHelperJsonExpectedType::String, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("get_widget_tree")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("remove_widget")) || CommandEquals(Command, TEXT("get_widget_properties")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_name"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("move_widget")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("new_parent"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("insert_index"), EBlueprintHelperJsonExpectedType::Number, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("set_widget_property")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("property_name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("value"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("get_object_properties")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("set_object_property")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("property_name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("value"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("get_datatable_rows")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("row_names"), EBlueprintHelperJsonExpectedType::Array, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("add_datatable_row")) || CommandEquals(Command, TEXT("update_datatable_row")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("row_name"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("fields"), EBlueprintHelperJsonExpectedType::Object, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("delete_datatable_row")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("row_name"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("create_blueprint")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("parent_class"), EBlueprintHelperJsonExpectedType::String, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("exec_console_command")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("command"), EBlueprintHelperJsonExpectedType::String, true},
		};
		return ValidateRules(Payload, Rules, OutError);
	}
	if (CommandEquals(Command, TEXT("close_editor")))
	{
		const FBlueprintHelperFieldRule Rules[] = {
			{TEXT("save_all"), EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return ValidateRules(Payload, Rules, OutError);
	}

	return true;
}

bool FBlueprintHelperRequestValidator::ValidateAuthorization(
	const FBlueprintHelperBridgeRequest& Request,
	FBlueprintHelperBridgeValidationError& OutError)
{
	if (IsHighRiskCommand(Request.Command) && !IsHighRiskCommandEnabled())
	{
		OutError.Code = TEXT("command_disabled");
		OutError.Field = TEXT("command");
		OutError.Message = FString::Printf(TEXT("命令 %s 默认禁用。设置 BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS=1 后才能启用。"),
			*Request.Command);
		return false;
	}

	if (!IsWriteCommand(Request.Command) && !IsHighRiskCommand(Request.Command))
	{
		return true;
	}

	const FString ExpectedToken = GetConfiguredToken();
	if (ExpectedToken.IsEmpty())
	{
		OutError.Code = TEXT("unauthorized");
		OutError.Field = TEXT("auth_token");
		OutError.Message = TEXT("写命令需要配置 BLUEPRINTHELPER_BRIDGE_TOKEN 并携带匹配 auth_token。");
		return false;
	}

	if (Request.AuthToken.IsEmpty() || Request.AuthToken != ExpectedToken)
	{
		OutError.Code = TEXT("unauthorized");
		OutError.Field = TEXT("auth_token");
		OutError.Message = TEXT("auth_token 缺失或不匹配。");
		return false;
	}

	return true;
}

bool FBlueprintHelperRequestValidator::IsWriteCommand(const FString& Command)
{
	static const TSet<FString> WriteCommands = {
		TEXT("import_json"),
		TEXT("import_agent_graph"),
		TEXT("compile_blueprint"),
		TEXT("save_asset"),
		TEXT("add_variable"),
		TEXT("remove_variable"),
		TEXT("add_graph"),
		TEXT("remove_graph"),
		TEXT("add_event_dispatcher"),
		TEXT("delete_nodes"),
		TEXT("add_widget"),
		TEXT("remove_widget"),
		TEXT("move_widget"),
		TEXT("set_widget_property"),
		TEXT("set_object_property"),
		TEXT("add_datatable_row"),
		TEXT("update_datatable_row"),
		TEXT("delete_datatable_row"),
		TEXT("undo"),
		TEXT("redo"),
		TEXT("play_in_editor"),
		TEXT("stop_pie"),
		TEXT("create_blueprint")
	};

	return WriteCommands.Contains(Command.ToLower());
}

bool FBlueprintHelperRequestValidator::IsHighRiskCommand(const FString& Command)
{
	return CommandEquals(Command, TEXT("exec_console_command"))
		|| CommandEquals(Command, TEXT("close_editor"));
}

bool FBlueprintHelperRequestValidator::IsHighRiskCommandEnabled()
{
	const FString Value = FPlatformMisc::GetEnvironmentVariable(TEXT("BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS")).ToLower();
	return Value == TEXT("1") || Value == TEXT("true") || Value == TEXT("yes");
}

FString FBlueprintHelperRequestValidator::GetConfiguredToken()
{
	return FPlatformMisc::GetEnvironmentVariable(TEXT("BLUEPRINTHELPER_BRIDGE_TOKEN"));
}
