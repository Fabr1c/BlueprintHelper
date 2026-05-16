#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace BlueprintHelper::GraphLayout
{
static bool TryReadPositiveNumber(const TSharedPtr<FJsonObject>& Json, const TCHAR* FieldName, float& OutValue, FValidationResult& Validation)
{
	if (!Json->HasField(FieldName))
	{
		return true;
	}

	double NumberValue = 0.0;
	if (!Json->TryGetNumberField(FieldName, NumberValue))
	{
		Validation.AddError(FString::Printf(TEXT("Field '%s' must be a number."), FieldName));
		return false;
	}
	if (NumberValue <= 0.0)
	{
		Validation.AddError(FString::Printf(TEXT("Field '%s' must be greater than zero."), FieldName));
		return false;
	}
	OutValue = static_cast<float>(NumberValue);
	return true;
}

static bool TryReadPositiveInt(const TSharedPtr<FJsonObject>& Json, const TCHAR* FieldName, int32& OutValue, FValidationResult& Validation)
{
	if (!Json->HasField(FieldName))
	{
		return true;
	}

	double NumberValue = 0.0;
	if (!Json->TryGetNumberField(FieldName, NumberValue))
	{
		Validation.AddError(FString::Printf(TEXT("Field '%s' must be a number."), FieldName));
		return false;
	}
	if (NumberValue <= 0.0)
	{
		Validation.AddError(FString::Printf(TEXT("Field '%s' must be greater than zero."), FieldName));
		return false;
	}
	OutValue = FMath::RoundToInt(NumberValue);
	return true;
}

static bool TryReadVector2D(const TSharedPtr<FJsonObject>& Json, FVector2D& OutValue)
{
	if (!Json.IsValid())
	{
		return false;
	}

	double X = 0.0;
	double Y = 0.0;
	if (!Json->TryGetNumberField(TEXT("x"), X) || !Json->TryGetNumberField(TEXT("y"), Y))
	{
		return false;
	}

	OutValue = FVector2D(static_cast<float>(X), static_cast<float>(Y));
	return true;
}

static bool IsDeprecatedIgnoredRoleText(const FString& Value)
{
	return Value.Equals(TEXT("Reroute"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("reroute"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("Knot"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("K2Node_Knot"), ESearchCase::IgnoreCase);
}

static void ReadStringArrayOrScalar(
	const TSharedPtr<FJsonObject>& Json,
	const TCHAR* FieldName,
	TArray<FString>& OutValues)
{
	if (!Json.IsValid())
	{
		return;
	}

	FString Scalar;
	if (Json->TryGetStringField(FieldName, Scalar) && !Scalar.IsEmpty())
	{
		OutValues.Add(Scalar);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Json->TryGetArrayField(FieldName, Array) && Array)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			if (Value.IsValid() && Value->Type == EJson::String)
			{
				OutValues.Add(Value->AsString());
			}
		}
	}
}

static const TArray<TSharedPtr<FJsonValue>>* FindRoleRulesArray(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid())
	{
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* RoleRules = nullptr;
	if (Json->TryGetArrayField(TEXT("role_rules"), RoleRules))
	{
		return RoleRules;
	}
	if (Json->TryGetArrayField(TEXT("node_roles"), RoleRules))
	{
		return RoleRules;
	}
	return nullptr;
}

static bool RuleObjectHasMatcher(const TSharedPtr<FJsonObject>& RuleObject)
{
	if (!RuleObject.IsValid())
	{
		return false;
	}

	FString Scalar;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if ((RuleObject->TryGetStringField(TEXT("match_class_contains"), Scalar) && !Scalar.IsEmpty()) ||
		(RuleObject->TryGetStringField(TEXT("match_title_contains"), Scalar) && !Scalar.IsEmpty()) ||
		RuleObject->TryGetArrayField(TEXT("match_class_contains"), Array) ||
		RuleObject->TryGetArrayField(TEXT("match_title_contains"), Array) ||
		RuleObject->HasField(TEXT("match_has_exec_pin")))
	{
		return true;
	}

	const TSharedPtr<FJsonObject>* MatchObject = nullptr;
	if (RuleObject->TryGetObjectField(TEXT("match"), MatchObject) && MatchObject && MatchObject->IsValid())
	{
		return (*MatchObject)->HasField(TEXT("node_classes")) ||
			(*MatchObject)->HasField(TEXT("title_contains")) ||
			(*MatchObject)->HasField(TEXT("has_exec_pin"));
	}
	return false;
}

static void ReadRoleRuleMatchers(const TSharedPtr<FJsonObject>& RuleObject, FRoleRule& OutRule)
{
	ReadStringArrayOrScalar(RuleObject, TEXT("match_class_contains"), OutRule.MatchClassContains);
	ReadStringArrayOrScalar(RuleObject, TEXT("match_title_contains"), OutRule.MatchTitleContains);
	if (RuleObject->HasField(TEXT("match_has_exec_pin")))
	{
		OutRule.bHasExecPinMatcher = true;
		RuleObject->TryGetBoolField(TEXT("match_has_exec_pin"), OutRule.bMatchHasExecPin);
	}

	const TSharedPtr<FJsonObject>* MatchObject = nullptr;
	if (RuleObject->TryGetObjectField(TEXT("match"), MatchObject) && MatchObject && MatchObject->IsValid())
	{
		ReadStringArrayOrScalar(*MatchObject, TEXT("node_classes"), OutRule.MatchClassContains);
		ReadStringArrayOrScalar(*MatchObject, TEXT("title_contains"), OutRule.MatchTitleContains);
		if ((*MatchObject)->HasField(TEXT("has_exec_pin")))
		{
			OutRule.bHasExecPinMatcher = true;
			(*MatchObject)->TryGetBoolField(TEXT("has_exec_pin"), OutRule.bMatchHasExecPin);
		}
	}
}

FValidationResult FRuleSetJson::Validate(const TSharedPtr<FJsonObject>& Json)
{
	FValidationResult Validation;
	if (!Json.IsValid())
	{
		Validation.AddError(TEXT("RuleSet JSON root must be an object."));
		return Validation;
	}

	FString Schema;
	if (!Json->TryGetStringField(TEXT("schema"), Schema))
	{
		Validation.AddError(TEXT("Field 'schema' is required."));
	}
	else if (Schema != RuleSetSchemaV1)
	{
		Validation.AddError(FString::Printf(TEXT("Unsupported schema '%s'. Expected '%s'."), *Schema, RuleSetSchemaV1));
	}

	FRuleSet Defaults;
	TryReadPositiveNumber(Json, TEXT("exec_column_spacing"), Defaults.ExecColumnSpacing, Validation);
	TryReadPositiveNumber(Json, TEXT("exec_row_spacing"), Defaults.ExecRowSpacing, Validation);
	TryReadPositiveNumber(Json, TEXT("branch_row_spacing"), Defaults.BranchRowSpacing, Validation);
	TryReadPositiveNumber(Json, TEXT("pure_input_offset_x"), Defaults.PureInputOffsetX, Validation);
	TryReadPositiveNumber(Json, TEXT("variable_input_offset_x"), Defaults.VariableInputOffsetX, Validation);
	TryReadPositiveNumber(Json, TEXT("input_pin_row_spacing"), Defaults.InputPinRowSpacing, Validation);
	TryReadPositiveInt(Json, TEXT("max_nodes_per_frame"), Defaults.MaxNodesPerFrame, Validation);
	TryReadPositiveNumber(Json, TEXT("max_ms_per_frame"), Defaults.MaxMillisecondsPerFrame, Validation);

	const TSharedPtr<FJsonObject>* SolverObject = nullptr;
	if (Json->TryGetObjectField(TEXT("solver"), SolverObject) && SolverObject && SolverObject->IsValid())
	{
		TryReadPositiveNumber(*SolverObject, TEXT("exec_horizontal_spacing"), Defaults.ExecColumnSpacing, Validation);
		TryReadPositiveNumber(*SolverObject, TEXT("lane_vertical_spacing"), Defaults.ExecRowSpacing, Validation);
		TryReadPositiveNumber(*SolverObject, TEXT("branch_vertical_spacing"), Defaults.BranchRowSpacing, Validation);
		TryReadPositiveNumber(*SolverObject, TEXT("data_horizontal_spacing"), Defaults.VariableInputOffsetX, Validation);
	}
	const TSharedPtr<FJsonObject>* ApplyObject = nullptr;
	if (Json->TryGetObjectField(TEXT("apply"), ApplyObject) && ApplyObject && ApplyObject->IsValid())
	{
		TryReadPositiveInt(*ApplyObject, TEXT("max_nodes_per_frame"), Defaults.MaxNodesPerFrame, Validation);
		TryReadPositiveNumber(*ApplyObject, TEXT("max_ms_per_frame"), Defaults.MaxMillisecondsPerFrame, Validation);
	}

	if (const TArray<TSharedPtr<FJsonValue>>* RoleRules = FindRoleRulesArray(Json))
	{
		for (int32 Index = 0; Index < RoleRules->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> RuleObject = (*RoleRules)[Index]->AsObject();
			if (!RuleObject.IsValid())
			{
				Validation.AddError(FString::Printf(TEXT("role_rules[%d] must be an object."), Index));
				continue;
			}

			FString RoleText;
			ENodeRole Role = ENodeRole::Unknown;
			if (!RuleObject->TryGetStringField(TEXT("role"), RoleText))
			{
				RuleObject->TryGetStringField(TEXT("id"), RoleText);
			}
			if (IsDeprecatedIgnoredRoleText(RoleText))
			{
				Validation.Warnings.Add(FString::Printf(TEXT("role_rules[%d] uses deprecated Reroute/Knot role and will be ignored."), Index));
				continue;
			}
			if (RoleText.IsEmpty() || !LexTryParseString(Role, RoleText))
			{
				Validation.AddError(FString::Printf(TEXT("role_rules[%d].role is missing or unsupported."), Index));
			}

			if (!RuleObjectHasMatcher(RuleObject))
			{
				Validation.AddError(FString::Printf(TEXT("role_rules[%d] must define a class, title, or exec-pin matcher."), Index));
			}
		}
	}
	return Validation;
}

bool FRuleSetJson::Import(const TSharedPtr<FJsonObject>& Json, FRuleSet& OutRuleSet, FValidationResult& OutValidation)
{
	OutValidation = Validate(Json);
	if (!OutValidation.bValid)
	{
		return false;
	}

	OutRuleSet = FRuleSet();
	Json->TryGetStringField(TEXT("schema"), OutRuleSet.Schema);
	Json->TryGetStringField(TEXT("id"), OutRuleSet.Id);
	Json->TryGetStringField(TEXT("display_name"), OutRuleSet.DisplayName);
	double Version = static_cast<double>(OutRuleSet.Version);
	if (Json->TryGetNumberField(TEXT("version"), Version))
	{
		OutRuleSet.Version = FMath::RoundToInt(Version);
	}
	TryReadPositiveNumber(Json, TEXT("exec_column_spacing"), OutRuleSet.ExecColumnSpacing, OutValidation);
	TryReadPositiveNumber(Json, TEXT("exec_row_spacing"), OutRuleSet.ExecRowSpacing, OutValidation);
	TryReadPositiveNumber(Json, TEXT("branch_row_spacing"), OutRuleSet.BranchRowSpacing, OutValidation);
	TryReadPositiveNumber(Json, TEXT("pure_input_offset_x"), OutRuleSet.PureInputOffsetX, OutValidation);
	TryReadPositiveNumber(Json, TEXT("variable_input_offset_x"), OutRuleSet.VariableInputOffsetX, OutValidation);
	TryReadPositiveNumber(Json, TEXT("input_pin_row_spacing"), OutRuleSet.InputPinRowSpacing, OutValidation);
	Json->TryGetBoolField(TEXT("target_pin_order_variable_input_alignment"), OutRuleSet.bUseTargetPinOrderForVariableInputs);
	Json->TryGetBoolField(TEXT("move_generated_nodes"), OutRuleSet.bMoveGeneratedNodes);
	Json->TryGetBoolField(TEXT("move_existing_nodes"), OutRuleSet.bMoveExistingNodes);
	TryReadPositiveInt(Json, TEXT("max_nodes_per_frame"), OutRuleSet.MaxNodesPerFrame, OutValidation);
	TryReadPositiveNumber(Json, TEXT("max_ms_per_frame"), OutRuleSet.MaxMillisecondsPerFrame, OutValidation);
	Json->TryGetBoolField(TEXT("mark_dirty_after_apply"), OutRuleSet.bMarkDirtyAfterApply);
	Json->TryGetBoolField(TEXT("save_after_apply"), OutRuleSet.bSaveAfterApply);

	const TSharedPtr<FJsonObject>* SolverObject = nullptr;
	if (Json->TryGetObjectField(TEXT("solver"), SolverObject) && SolverObject && SolverObject->IsValid())
	{
		TryReadPositiveNumber(*SolverObject, TEXT("exec_horizontal_spacing"), OutRuleSet.ExecColumnSpacing, OutValidation);
		TryReadPositiveNumber(*SolverObject, TEXT("lane_vertical_spacing"), OutRuleSet.ExecRowSpacing, OutValidation);
		TryReadPositiveNumber(*SolverObject, TEXT("branch_vertical_spacing"), OutRuleSet.BranchRowSpacing, OutValidation);
		TryReadPositiveNumber(*SolverObject, TEXT("data_horizontal_spacing"), OutRuleSet.VariableInputOffsetX, OutValidation);
		(*SolverObject)->TryGetBoolField(TEXT("move_user_nodes"), OutRuleSet.bMoveExistingNodes);
	}
	const TSharedPtr<FJsonObject>* ApplyObject = nullptr;
	if (Json->TryGetObjectField(TEXT("apply"), ApplyObject) && ApplyObject && ApplyObject->IsValid())
	{
		(*ApplyObject)->TryGetBoolField(TEXT("move_generated_nodes"), OutRuleSet.bMoveGeneratedNodes);
		TryReadPositiveInt(*ApplyObject, TEXT("max_nodes_per_frame"), OutRuleSet.MaxNodesPerFrame, OutValidation);
		TryReadPositiveNumber(*ApplyObject, TEXT("max_ms_per_frame"), OutRuleSet.MaxMillisecondsPerFrame, OutValidation);
	}
	const TSharedPtr<FJsonObject>* PersistenceObject = nullptr;
	if (Json->TryGetObjectField(TEXT("persistence"), PersistenceObject) && PersistenceObject && PersistenceObject->IsValid())
	{
		(*PersistenceObject)->TryGetBoolField(TEXT("mark_dirty_after_apply"), OutRuleSet.bMarkDirtyAfterApply);
		(*PersistenceObject)->TryGetBoolField(TEXT("save_after_apply"), OutRuleSet.bSaveAfterApply);
	}
	const TSharedPtr<FJsonObject>* EditorCanvasObject = nullptr;
	if (Json->TryGetObjectField(TEXT("editor_canvas"), EditorCanvasObject) && EditorCanvasObject && EditorCanvasObject->IsValid())
	{
		const TSharedPtr<FJsonObject>* RoleCentersObject = nullptr;
		if ((*EditorCanvasObject)->TryGetObjectField(TEXT("role_centers"), RoleCentersObject) && RoleCentersObject && RoleCentersObject->IsValid())
		{
			OutRuleSet.EditorCanvasRoleCenters.Reset();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*RoleCentersObject)->Values)
			{
				ENodeRole Role = ENodeRole::Unknown;
				if (IsDeprecatedIgnoredRoleText(Pair.Key) || !LexTryParseString(Role, Pair.Key) || Role == ENodeRole::Unknown)
				{
					continue;
				}

				FVector2D Center;
				if (TryReadVector2D(Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr, Center))
				{
					OutRuleSet.EditorCanvasRoleCenters.Add(Role, Center);
				}
			}
		}
	}

	if (const TArray<TSharedPtr<FJsonValue>>* RoleRules = FindRoleRulesArray(Json))
	{
		OutRuleSet.RoleRules.Reset();
		for (const TSharedPtr<FJsonValue>& RuleValue : *RoleRules)
		{
			const TSharedPtr<FJsonObject> RuleObject = RuleValue->AsObject();
			if (!RuleObject.IsValid())
			{
				continue;
			}

			FRoleRule Rule;
			RuleObject->TryGetStringField(TEXT("id"), Rule.Id);
			RuleObject->TryGetStringField(TEXT("color"), Rule.Color);
			double PriorityValue = 0.0;
			if (RuleObject->TryGetNumberField(TEXT("priority"), PriorityValue))
			{
				Rule.Priority = FMath::RoundToInt(PriorityValue);
			}
			ReadRoleRuleMatchers(RuleObject, Rule);
			FString RoleText;
			if (!RuleObject->TryGetStringField(TEXT("role"), RoleText))
			{
				RoleText = Rule.Id;
			}
			if (IsDeprecatedIgnoredRoleText(RoleText))
			{
				continue;
			}
			if (!RoleText.IsEmpty())
			{
				LexTryParseString(Rule.Role, RoleText);
			}
			OutRuleSet.RoleRules.Add(Rule);
		}
		OutRuleSet.RoleRules.Sort([](const FRoleRule& Left, const FRoleRule& Right)
		{
			return Left.Priority > Right.Priority;
		});
	}

	return OutValidation.bValid;
}

bool FRuleSetJson::ImportString(const FString& JsonText, FRuleSet& OutRuleSet, FValidationResult& OutValidation)
{
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		OutValidation = FValidationResult();
		OutValidation.AddError(TEXT("RuleSet JSON text could not be parsed."));
		return false;
	}
	return Import(Json, OutRuleSet, OutValidation);
}

TSharedRef<FJsonObject> FRuleSetJson::Export(const FRuleSet& RuleSet)
{
	return ToJson(RuleSet);
}

FString FRuleSetJson::ExportString(const FRuleSet& RuleSet)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Export(RuleSet), Writer);
	return Output;
}
}
