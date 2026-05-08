#include "Systems/ToolClusters/BlueprintVariables/OperationHandlers/BlueprintMemberVariableMutationHandler.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
bool TryJsonValueToBool(const TSharedPtr<FJsonValue>& Value, bool& OutValue)
{
	if (!Value.IsValid())
	{
		return false;
	}

	if (Value->Type == EJson::Boolean)
	{
		OutValue = Value->AsBool();
		return true;
	}

	if (Value->Type == EJson::String)
	{
		const FString LowerValue = Value->AsString().ToLower();
		if (LowerValue == TEXT("true"))
		{
			OutValue = true;
			return true;
		}
		if (LowerValue == TEXT("false"))
		{
			OutValue = false;
			return true;
		}
	}

	return false;
}

bool ApplyVariableMetadataSetting(
	UBlueprint* Blueprint,
	const FName VariableName,
	FBPVariableDescription& Variable,
	const FBlueprintHelperMemberPropertyMutation& Setting,
	bool& bOutChanged,
	FString& OutError)
{
	bOutChanged = false;
	const FString NormalizedPath = Setting.PropertyPath.ToLower();

	if (NormalizedPath == TEXT("category"))
	{
		FString NewCategory;
		if (!FBlueprintHelperMemberVariableMutationHandler::TryScalarJsonToBlueprintDefaultString(Setting.Value, NewCategory))
		{
			OutError = TEXT("category must be a scalar value.");
			return false;
		}

		const FString OldCategory = FBlueprintEditorUtils::GetBlueprintVariableCategory(
			Blueprint,
			VariableName,
			nullptr).ToString();
		if (OldCategory == NewCategory)
		{
			return true;
		}

		Blueprint->Modify();
		FBlueprintEditorUtils::SetBlueprintVariableCategory(
			Blueprint,
			VariableName,
			nullptr,
			FText::FromString(NewCategory));
		bOutChanged = true;
		return true;
	}

	if (NormalizedPath == TEXT("tooltip"))
	{
		FString NewTooltip;
		if (!FBlueprintHelperMemberVariableMutationHandler::TryScalarJsonToBlueprintDefaultString(Setting.Value, NewTooltip))
		{
			OutError = TEXT("tooltip must be a scalar value.");
			return false;
		}

		FString OldTooltip;
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(
			Blueprint,
			VariableName,
			nullptr,
			FBlueprintMetadata::MD_Tooltip,
			OldTooltip);
		if (OldTooltip == NewTooltip)
		{
			return true;
		}

		Blueprint->Modify();
		if (NewTooltip.IsEmpty())
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(
				Blueprint,
				VariableName,
				nullptr,
				FBlueprintMetadata::MD_Tooltip);
		}
		else
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(
				Blueprint,
				VariableName,
				nullptr,
				FBlueprintMetadata::MD_Tooltip,
				NewTooltip);
		}
		bOutChanged = true;
		return true;
	}

	if (NormalizedPath == TEXT("instance_editable"))
	{
		bool bInstanceEditable = false;
		if (!TryJsonValueToBool(Setting.Value, bInstanceEditable))
		{
			OutError = TEXT("instance_editable must be boolean.");
			return false;
		}

		const bool bWasInstanceEditable = !(Variable.PropertyFlags & CPF_DisableEditOnInstance);
		if (bWasInstanceEditable == bInstanceEditable)
		{
			return true;
		}

		Blueprint->Modify();
		if (bInstanceEditable)
		{
			Variable.PropertyFlags &= ~CPF_DisableEditOnInstance;
		}
		else
		{
			Variable.PropertyFlags |= CPF_DisableEditOnInstance;
		}
		bOutChanged = true;
		return true;
	}

	if (NormalizedPath == TEXT("expose_on_spawn"))
	{
		bool bExposeOnSpawn = false;
		if (!TryJsonValueToBool(Setting.Value, bExposeOnSpawn))
		{
			OutError = TEXT("expose_on_spawn must be boolean.");
			return false;
		}

		FString OldExposeOnSpawn;
		const bool bHadExposeOnSpawn = FBlueprintEditorUtils::GetBlueprintVariableMetaData(
			Blueprint,
			VariableName,
			nullptr,
			FBlueprintMetadata::MD_ExposeOnSpawn,
			OldExposeOnSpawn);
		const bool bWasExposeOnSpawn = bHadExposeOnSpawn && OldExposeOnSpawn == TEXT("true");
		if (bWasExposeOnSpawn == bExposeOnSpawn)
		{
			return true;
		}

		Blueprint->Modify();
		if (bExposeOnSpawn)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(
				Blueprint,
				VariableName,
				nullptr,
				FBlueprintMetadata::MD_ExposeOnSpawn,
				TEXT("true"));
		}
		else
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(
				Blueprint,
				VariableName,
				nullptr,
				FBlueprintMetadata::MD_ExposeOnSpawn);
		}
		bOutChanged = true;
		return true;
	}

	OutError = FString::Printf(TEXT("Unsupported member variable property: %s."), *Setting.PropertyPath);
	return false;
}
}

bool FBlueprintHelperMemberVariableMutationHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("set_member_default"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("set_member_defaults"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("set_member_variable_properties"), ESearchCase::IgnoreCase);
}

bool FBlueprintHelperMemberVariableMutationHandler::Execute(
	UBlueprint* Blueprint,
	const TSharedPtr<FJsonObject>& OpPayload,
	FString& OutError)
{
	if (!Blueprint || !OpPayload.IsValid())
	{
		OutError = TEXT("member variable mutation failed: invalid Blueprint or payload.");
		return false;
	}

	FString OpName;
	OpPayload->TryGetStringField(TEXT("op"), OpName);
	if (OpName.Equals(TEXT("set_member_default"), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperMemberDefaultMutation Change;
		if (!TryReadVariableName(OpPayload, Change.VariableName) || !TryReadDefaultValue(OpPayload, Change.DefaultValue))
		{
			OutError = TEXT("set_member_default requires name and scalar value/default_value.");
			return false;
		}

		FBlueprintHelperVariableMutationCounts Counts;
		TArray<FBlueprintHelperMemberDefaultMutation> Changes;
		Changes.Add(MoveTemp(Change));
		return ApplyDefaultChanges(Blueprint, Changes, Counts, OutError);
	}

	if (OpName.Equals(TEXT("set_member_defaults"), ESearchCase::IgnoreCase))
	{
		TArray<FBlueprintHelperMemberDefaultMutation> Changes;
		const TArray<TSharedPtr<FJsonValue>>* DefaultsArray = nullptr;
		if (OpPayload->TryGetArrayField(TEXT("defaults"), DefaultsArray) && DefaultsArray)
		{
			for (const TSharedPtr<FJsonValue>& Value : *DefaultsArray)
			{
				FBlueprintHelperMemberDefaultMutation Change;
				const TSharedPtr<FJsonObject> DefaultObject = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!TryReadVariableName(DefaultObject, Change.VariableName) ||
					!TryReadDefaultValue(DefaultObject, Change.DefaultValue))
				{
					OutError = TEXT("set_member_defaults entries require name and scalar value/default_value.");
					return false;
				}
				Changes.Add(MoveTemp(Change));
			}
		}
		else
		{
			const TSharedPtr<FJsonObject>* ValuesObject = nullptr;
			if (OpPayload->TryGetObjectField(TEXT("values"), ValuesObject) && ValuesObject && ValuesObject->IsValid())
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ValuesObject)->Values)
				{
					FBlueprintHelperMemberDefaultMutation Change;
					Change.VariableName = Pair.Key;
					if (!TryScalarJsonToBlueprintDefaultString(Pair.Value, Change.DefaultValue))
					{
						OutError = TEXT("set_member_defaults values entries must be scalar JSON values.");
						return false;
					}
					Changes.Add(MoveTemp(Change));
				}
			}
		}

		if (Changes.Num() == 0)
		{
			OutError = TEXT("set_member_defaults requires defaults array or values object.");
			return false;
		}

		FBlueprintHelperVariableMutationCounts Counts;
		return ApplyDefaultChanges(Blueprint, Changes, Counts, OutError);
	}

	if (OpName.Equals(TEXT("set_member_variable_properties"), ESearchCase::IgnoreCase))
	{
		FString VariableName;
		if (!TryReadVariableName(OpPayload, VariableName))
		{
			OutError = TEXT("set_member_variable_properties requires name or variable_name.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
		if (!OpPayload->TryGetArrayField(TEXT("settings"), SettingsArray) || !SettingsArray)
		{
			OutError = TEXT("set_member_variable_properties requires settings array.");
			return false;
		}

		TArray<FBlueprintHelperMemberPropertyMutation> Settings;
		for (const TSharedPtr<FJsonValue>& Value : *SettingsArray)
		{
			FBlueprintHelperMemberPropertyMutation Setting;
			if (!TryReadPropertySetting(Value.IsValid() ? Value->AsObject() : nullptr, Setting))
			{
				OutError = TEXT("set_member_variable_properties settings require property_path and value.");
				return false;
			}
			Settings.Add(MoveTemp(Setting));
		}

		FBlueprintHelperVariableMutationCounts Counts;
		return ApplyPropertySettings(Blueprint, VariableName, Settings, Counts, OutError);
	}

	OutError = FString::Printf(TEXT("unsupported member variable mutation op: %s"), *OpName);
	return false;
}

bool FBlueprintHelperMemberVariableMutationHandler::TryReadVariableName(
	const TSharedPtr<FJsonObject>& Payload,
	FString& OutVariableName)
{
	if (!Payload.IsValid())
	{
		return false;
	}

	Payload->TryGetStringField(TEXT("name"), OutVariableName);
	if (OutVariableName.IsEmpty())
	{
		Payload->TryGetStringField(TEXT("variable_name"), OutVariableName);
	}
	return !OutVariableName.IsEmpty();
}

bool FBlueprintHelperMemberVariableMutationHandler::TryReadDefaultValue(
	const TSharedPtr<FJsonObject>& Payload,
	FString& OutDefaultValue)
{
	if (!Payload.IsValid())
	{
		return false;
	}

	TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		Value = Payload->TryGetField(TEXT("default_value"));
	}
	return TryScalarJsonToBlueprintDefaultString(Value, OutDefaultValue);
}

bool FBlueprintHelperMemberVariableMutationHandler::TryReadPropertySetting(
	const TSharedPtr<FJsonObject>& SettingObject,
	FBlueprintHelperMemberPropertyMutation& OutSetting)
{
	if (!SettingObject.IsValid())
	{
		return false;
	}

	SettingObject->TryGetStringField(TEXT("property_path"), OutSetting.PropertyPath);
	if (OutSetting.PropertyPath.IsEmpty())
	{
		SettingObject->TryGetStringField(TEXT("field"), OutSetting.PropertyPath);
	}
	OutSetting.Value = SettingObject->TryGetField(TEXT("value"));
	return !OutSetting.PropertyPath.IsEmpty() && OutSetting.Value.IsValid();
}

bool FBlueprintHelperMemberVariableMutationHandler::TryScalarJsonToBlueprintDefaultString(
	const TSharedPtr<FJsonValue>& Value,
	FString& OutDefaultValue)
{
	if (!Value.IsValid())
	{
		return false;
	}

	switch (Value->Type)
	{
	case EJson::String:
		OutDefaultValue = Value->AsString();
		return true;
	case EJson::Number:
	{
		const double Number = Value->AsNumber();
		const double Rounded = FMath::RoundToDouble(Number);
		OutDefaultValue = FMath::IsNearlyEqual(Number, Rounded)
			? FString::Printf(TEXT("%.0f"), Number)
			: FString::SanitizeFloat(Number);
		return true;
	}
	case EJson::Boolean:
		OutDefaultValue = Value->AsBool() ? TEXT("true") : TEXT("false");
		return true;
	default:
		return false;
	}
}

bool FBlueprintHelperMemberVariableMutationHandler::ApplyDefaultChanges(
	UBlueprint* Blueprint,
	const TArray<FBlueprintHelperMemberDefaultMutation>& Changes,
	FBlueprintHelperVariableMutationCounts& OutCounts,
	FString& OutError,
	FString* OutField)
{
	OutCounts = {};
	OutCounts.RequestedCount = Changes.Num();
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint asset could not be resolved.");
		if (OutField) *OutField = TEXT("asset_path");
		return false;
	}

	for (int32 Index = 0; Index < Changes.Num(); ++Index)
	{
		const FBlueprintHelperMemberDefaultMutation& Change = Changes[Index];
		if (Change.VariableName.IsEmpty())
		{
			OutError = TEXT("Member default change requires name or variable_name.");
			if (OutField) *OutField = FString::Printf(TEXT("defaults[%d].name"), Index);
			return false;
		}

		if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Change.VariableName)) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Member variable '%s' was not found."), *Change.VariableName);
			if (OutField) *OutField = FString::Printf(TEXT("defaults[%d].name"), Index);
			return false;
		}
	}

	bool bWillChange = false;
	for (const FBlueprintHelperMemberDefaultMutation& Change : Changes)
	{
		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Change.VariableName));
		if (VariableIndex != INDEX_NONE && Blueprint->NewVariables[VariableIndex].DefaultValue != Change.DefaultValue)
		{
			bWillChange = true;
			break;
		}
	}

	if (bWillChange)
	{
		Blueprint->Modify();
	}

	for (const FBlueprintHelperMemberDefaultMutation& Change : Changes)
	{
		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Change.VariableName));
		if (VariableIndex == INDEX_NONE)
		{
			continue;
		}

		FBPVariableDescription& Variable = Blueprint->NewVariables[VariableIndex];
		if (Variable.DefaultValue == Change.DefaultValue)
		{
			++OutCounts.NoOpCount;
			continue;
		}

		Variable.DefaultValue = Change.DefaultValue;
		++OutCounts.ChangedCount;
	}

	if (OutCounts.ChangedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	return true;
}

bool FBlueprintHelperMemberVariableMutationHandler::ApplyPropertySettings(
	UBlueprint* Blueprint,
	const FString& VariableName,
	const TArray<FBlueprintHelperMemberPropertyMutation>& Settings,
	FBlueprintHelperVariableMutationCounts& OutCounts,
	FString& OutError,
	FString* OutField)
{
	OutCounts = {};
	OutCounts.RequestedCount = Settings.Num();
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint asset could not be resolved.");
		if (OutField) *OutField = TEXT("asset_path");
		return false;
	}

	const FName VariableFName(*VariableName);
	const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableFName);
	if (VariableIndex == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("Member variable '%s' was not found."), *VariableName);
		if (OutField) *OutField = TEXT("name");
		return false;
	}

	for (int32 Index = 0; Index < Settings.Num(); ++Index)
	{
		bool bChanged = false;
		FString SettingError;
		FBPVariableDescription& Variable = Blueprint->NewVariables[VariableIndex];
		if (!ApplyVariableMetadataSetting(
			Blueprint,
			VariableFName,
			Variable,
			Settings[Index],
			bChanged,
			SettingError))
		{
			OutError = SettingError;
			if (OutField) *OutField = FString::Printf(TEXT("settings[%d].property_path"), Index);
			return false;
		}

		if (bChanged)
		{
			++OutCounts.ChangedCount;
		}
		else
		{
			++OutCounts.NoOpCount;
		}
	}

	if (OutCounts.ChangedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	return true;
}
