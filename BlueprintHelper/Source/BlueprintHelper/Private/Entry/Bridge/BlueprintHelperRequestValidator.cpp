// BlueprintHelper Bridge Layer — request validation helpers

#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"
#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Parse.h"

class FBlueprintHelperRequestValidatorLocalUtils
{
public:
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

	static FString ExpectedTypeToString(EBlueprintHelperJsonExpectedType Type)
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

	static FString ActualJsonTypeToString(const TSharedPtr<FJsonValue>& Value)
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

	static bool MatchesExpectedType(const TSharedPtr<FJsonValue>& Value, EBlueprintHelperJsonExpectedType Type)
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

	static void SetValidationError(
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

	static bool ValidateFieldRule(
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

	static bool ValidateRules(
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

	static bool RejectFields(
		const TSharedPtr<FJsonObject>& Payload,
		TArrayView<const TCHAR* const> FieldNames,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		for (const TCHAR* FieldName : FieldNames)
		{
			const FString Field(FieldName);
			if (Payload->HasField(Field))
			{
				OutError.Code = TEXT("invalid_request");
				OutError.Field = TEXT("payload.") + Field;
				OutError.ExpectedType = TEXT("absent");
				OutError.ActualType = TEXT("present");
				OutError.Message = FString::Printf(TEXT("%s is retired and is no longer accepted."), *OutError.Field);
				return false;
			}
		}
		return true;
	}

	static bool CommandEquals(const FString& Command, const TCHAR* Expected)
	{
		return Command.Equals(Expected, ESearchCase::IgnoreCase);
	}

	static bool ValidateOptionalStringEnum(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName,
		const TSet<FString>& AllowedValues,
		FBlueprintHelperBridgeValidationError& OutError)
	{
		FString Value;
		if (!Payload->TryGetStringField(FieldName, Value))
		{
			return true;
		}

		if (!AllowedValues.Contains(Value.ToLower()))
		{
			SetValidationError(
				OutError,
				TEXT("payload.") + FString(FieldName),
				FString::Printf(TEXT("one_of[%s]"), *FString::Join(AllowedValues.Array(), TEXT(","))),
				Value);
			return false;
		}
		return true;
	}

};

bool FBlueprintHelperRequestValidator::NormalizeExportScope(
	const FString& InScope,
	EBlueprintHelperExportScope& OutScope,
	FString& OutEffectiveScope,
	FString& OutError)
{
	const FString Scope = InScope.IsEmpty() ? TEXT("graph") : InScope.ToLower();
	if (Scope == TEXT("graph"))
	{
		OutScope = EBlueprintHelperExportScope::SingleGraph;
		OutEffectiveScope = TEXT("graph");
		return true;
	}
	if (Scope == TEXT("blueprint"))
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

	const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule TargetRules[] = {
		{TEXT("target_blueprint"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
		{TEXT("target_graph"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
	};
	if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, TargetRules, OutError))
	{
		return false;
	}

	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("export_to_json")))
	{
		const TCHAR* RetiredFields[] = {TEXT("include_json_text")};
		if (!FBlueprintHelperRequestValidatorLocalUtils::RejectFields(Payload, RetiredFields, OutError))
		{
			return false;
		}

		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("scope"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
		};
		const TSet<FString> AllowedScopes = {TEXT("graph"), TEXT("blueprint"), TEXT("selection")};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError)
			&& FBlueprintHelperRequestValidatorLocalUtils::ValidateOptionalStringEnum(
				Payload,
				TEXT("scope"),
				AllowedScopes,
				OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("export_logic")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("scope"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("format"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("detail"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("include_data_dependencies"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_orphans"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_node_ids"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_positions"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("include_raw_node_types"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		const TSet<FString> AllowedScopes = {TEXT("graph"), TEXT("blueprint")};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError)
			&& FBlueprintHelperRequestValidatorLocalUtils::ValidateOptionalStringEnum(
				Payload,
				TEXT("scope"),
				AllowedScopes,
				OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("validate_json")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("json"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("get_debug_case")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("debug_case_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("list_debug_cases")))
	{
		return true;
	}

	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("export_debug_bundle")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("debug_case_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("request_write_session")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("reason"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("scope"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("ttl_seconds"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Number, false},
			{TEXT("asset_paths"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("compile_blueprint")))
	{
		return true;
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("open_asset")) || FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("save_asset"))
		|| FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("get_asset_info")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("list_assets")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("class_filter"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("name_filter"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("recursive"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("max_results"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Number, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("search_assets")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("class_filter"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("query"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("max_results"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Number, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_variable")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("pin_type"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, false},
			{TEXT("default_value"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("category"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("flags"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_graph")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("graph_type"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("inputs"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
			{TEXT("outputs"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
			{TEXT("is_pure"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_event_dispatcher")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("params"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("remove_variable")) || FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("remove_graph")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("delete_nodes")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("node_ids"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, true},
			{TEXT("strict"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_widget")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_class"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("parent_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("widget_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("get_widget_tree")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("remove_widget")) || FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("get_widget_properties")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("move_widget")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("new_parent"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("insert_index"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Number, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("set_widget_property")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("widget_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("property_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("value"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("get_object_properties")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("set_object_property")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("property_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("value"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("get_datatable_rows")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("row_names"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_datatable_row")) || FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("update_datatable_row")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("row_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("fields"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("delete_datatable_row")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("row_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("create_blueprint")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("parent_class"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("exec_console_command")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("command"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("close_editor")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("save_all"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}

	// ─── Blueprint Component 命令校验 ───

	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("read_components")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_component")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("component_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("component_class"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("parent_component"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("socket_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("attach_rule"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("name_collision_policy"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("set_component_property")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("component_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("property_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError)) return false;
		if (!Payload->HasField(TEXT("value")))
		{
			OutError.Code = TEXT("invalid_request");
			OutError.Field = TEXT("payload.value");
			OutError.ExpectedType = TEXT("any");
			OutError.ActualType = TEXT("missing");
			OutError.Message = TEXT("value 缺失。");
			return false;
		}
		return true;
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("set_component_properties")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("component_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("settings"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("remove_component")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("component_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}

	// ─── Phase 9: Blueprint Class Settings ───
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("read_class_settings")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_implemented_interface")) ||
		FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("remove_implemented_interface")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("interface_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("add_implemented_interfaces")) ||
		FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("remove_implemented_interfaces")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("interface_paths"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("set_class_default_property")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("property_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError)) return false;
		if (!Payload->HasField(TEXT("value")))
		{
			OutError.Code = TEXT("invalid_request");
			OutError.Field = TEXT("payload.value");
			OutError.ExpectedType = TEXT("any");
			OutError.ActualType = TEXT("missing");
			OutError.Message = TEXT("value 缺失。");
			return false;
		}
		return true;
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("set_class_default_properties")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("settings"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("preview_task_plan")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("task_plan"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("preview_token_request"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("execute_task_plan")))
	{
		if (Payload->HasField(TEXT("task_plan")))
		{
			const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
				{TEXT("task_plan"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			};
			return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
		}

		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("preview_token"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("task_spec_hash"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("get_task_run_journal")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("task_run_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("read_reference_context")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("target_type"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("target_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("graph_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("declaring_class_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("block_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("widget_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("row_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("interface_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("search_scope"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("resolution_policy"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("detail"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("max_results"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Number, false},
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError))
		{
			return false;
		}

		if (Payload->HasField(TEXT("target_guid")))
		{
			FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
				OutError,
				TEXT("payload.target_guid"),
				TEXT("unsupported"),
				TEXT("present"));
			return false;
		}
		if (Payload->HasField(TEXT("scope")))
		{
			FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
				OutError,
				TEXT("payload.scope"),
				TEXT("unsupported"),
				TEXT("present"));
			return false;
		}
		if (Payload->HasField(TEXT("include_samples")))
		{
			FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
				OutError,
				TEXT("payload.include_samples"),
				TEXT("unsupported"),
				TEXT("present"));
			return false;
		}
		if (Payload->HasField(TEXT("max_result_count")))
		{
			FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
				OutError,
				TEXT("payload.max_result_count"),
				TEXT("unsupported; use max_results"),
				TEXT("present"));
			return false;
		}

		const TSet<FString> TargetTypes = {
			TEXT("asset"),
			TEXT("blueprint"),
			TEXT("graph"),
			TEXT("function"),
			TEXT("event"),
			TEXT("custom_event"),
			TEXT("member_variable"),
			TEXT("local_variable"),
			TEXT("event_dispatcher"),
			TEXT("block"),
			TEXT("widget"),
			TEXT("data_table_row"),
			TEXT("interface"),
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateOptionalStringEnum(Payload, TEXT("target_type"), TargetTypes, OutError))
		{
			return false;
		}

		FString TargetType;
		Payload->TryGetStringField(TEXT("target_type"), TargetType);
		TargetType = TargetType.IsEmpty() ? TEXT("asset") : TargetType.ToLower();
		if (TargetType == TEXT("function") ||
			TargetType == TEXT("event") ||
			TargetType == TEXT("custom_event") ||
			TargetType == TEXT("member_variable") ||
			TargetType == TEXT("local_variable") ||
			TargetType == TEXT("event_dispatcher"))
		{
			FString TargetName;
			if (!Payload->TryGetStringField(TEXT("target_name"), TargetName) || TargetName.IsEmpty())
			{
				FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
					OutError,
					TEXT("payload.target_name"),
					TEXT("string"),
					TEXT("missing"));
				return false;
			}
		}
		if (TargetType == TEXT("local_variable"))
		{
			FString GraphName;
			if (!Payload->TryGetStringField(TEXT("graph_name"), GraphName) || GraphName.IsEmpty())
			{
				FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
					OutError,
					TEXT("payload.graph_name"),
					TEXT("string"),
					TEXT("missing"));
				return false;
			}
		}

		const TSet<FString> SearchScopes = {
			TEXT("asset"),
			TEXT("project"),
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateOptionalStringEnum(Payload, TEXT("search_scope"), SearchScopes, OutError))
		{
			return false;
		}

		const TSet<FString> ResolutionPolicies = {
			TEXT("ue_then_name"),
			TEXT("ue_only"),
			TEXT("name_only"),
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateOptionalStringEnum(Payload, TEXT("resolution_policy"), ResolutionPolicies, OutError))
		{
			return false;
		}

		const TSet<FString> Details = {
			TEXT("summary"),
			TEXT("samples"),
			TEXT("full"),
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateOptionalStringEnum(Payload, TEXT("detail"), Details, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("read_function_chain_context")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("target_type"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("target_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("graph_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("max_depth"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Number, false},
			{TEXT("include_data_dependencies"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
			{TEXT("expand_cross_asset"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError))
		{
			return false;
		}

		const TSet<FString> TargetTypes = {
			TEXT("function"),
			TEXT("event"),
			TEXT("custom_event"),
		};
		if (!FBlueprintHelperRequestValidatorLocalUtils::ValidateOptionalStringEnum(Payload, TEXT("target_type"), TargetTypes, OutError))
		{
			return false;
		}

		if (Payload->HasField(TEXT("target_guid")))
		{
			FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
				OutError,
				TEXT("payload.target_guid"),
				TEXT("unsupported"),
				TEXT("present"));
			return false;
		}
		if (Payload->HasField(TEXT("entry")) ||
			Payload->HasField(TEXT("target")) ||
			Payload->HasField(TEXT("query")) ||
			Payload->HasField(TEXT("owner_asset_path")))
		{
			FBlueprintHelperRequestValidatorLocalUtils::SetValidationError(
				OutError,
				TEXT("payload"),
				TEXT("function_chain_minimal_fields"),
				TEXT("unsupported_echo_or_owner_field"));
			return false;
		}
		return true;
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("append_blueprint_graph")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("target"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("feature_name"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("nodes"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, true},
			{TEXT("links"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
			{TEXT("dry_run"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("replace_blueprint_graph")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("target"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("selector"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, false},
			{TEXT("replacement"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("options"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("patch_blueprint_graph")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("target"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("patch_type"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("patched_ref"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("patch"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("dry_run"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("merge_blueprint_graph")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("target"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("anchor"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("inserted"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Object, true},
			{TEXT("sequence_order"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
			{TEXT("dry_run"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("compile_blueprint_asset")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("query_review_records")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("archive_session_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("task_run_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("pending_only"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Bool, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	if (FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("apply_review_action")))
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("review_record_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("action"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("target_keys"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Array, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("query_scope"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("asset_path"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
			{TEXT("limit"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::Number, false},
			{TEXT("cursor"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
	}
	{
		const FBlueprintHelperRequestValidatorLocalUtils::FBlueprintHelperFieldRule Rules[] = {
			{TEXT("evidence_id"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, true},
			{TEXT("detail_level"), FBlueprintHelperRequestValidatorLocalUtils::EBlueprintHelperJsonExpectedType::String, false},
		};
		return FBlueprintHelperRequestValidatorLocalUtils::ValidateRules(Payload, Rules, OutError);
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

	if (IsWriteCommand(Request.Command) && FBlueprintHelperSafetyProfileResolver::IsAutoRepair())
	{
		return true;
	}

	return FBlueprintHelperWriteAuthorizationService::Get().ValidateSessionForCommand(
		Request.AuthSession,
		Request.Command,
		Request.Payload,
		OutError);

}

bool FBlueprintHelperRequestValidator::IsWriteCommand(const FString& Command)
{
	static const TSet<FString> WriteCommands = {
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
TEXT("create_blueprint"),
	TEXT("add_component"),
	TEXT("set_component_property"),
	TEXT("set_component_properties"),
	TEXT("remove_component"),
	TEXT("add_implemented_interface"),
	TEXT("add_implemented_interfaces"),
	TEXT("remove_implemented_interface"),
	TEXT("remove_implemented_interfaces"),
	TEXT("set_class_default_property"),
	TEXT("set_class_default_properties"),
	TEXT("execute_task_plan"),
	TEXT("append_blueprint_graph"),
	TEXT("replace_blueprint_graph"),
	TEXT("patch_blueprint_graph"),
	TEXT("merge_blueprint_graph"),
	TEXT("add_blueprint_member_variable"),
	TEXT("add_blueprint_member_variables"),
	TEXT("set_blueprint_member_variable_properties"),
	TEXT("remove_blueprint_member_variable"),
	TEXT("remove_blueprint_member_variables"),
	TEXT("set_blueprint_member_default"),
	TEXT("set_blueprint_member_defaults"),
	TEXT("add_blueprint_local_variable"),
	TEXT("add_blueprint_local_variables"),
	TEXT("set_blueprint_local_variable_properties"),
	TEXT("remove_blueprint_local_variable"),
	TEXT("remove_blueprint_local_variables"),
	};

	return WriteCommands.Contains(Command.ToLower());
}

bool FBlueprintHelperRequestValidator::IsHighRiskCommand(const FString& Command)
{
	return FBlueprintHelperRequestValidatorLocalUtils::CommandEquals(Command, TEXT("exec_console_command"));
}

bool FBlueprintHelperRequestValidator::IsHighRiskCommandEnabled()
{
	const FString Value = FPlatformMisc::GetEnvironmentVariable(TEXT("BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS")).ToLower();
	return Value == TEXT("1") || Value == TEXT("true") || Value == TEXT("yes");
}
