#include "Systems/ToolClusters/BlueprintVariables/OperationHandlers/BlueprintLocalVariableMutationHandler.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/Class.h"

class FBlueprintLocalVariableMutationHandlerLocalUtils
{
public:
inline static const FName BlueprintHelperLocalDescriptionMetaKey = FName(TEXT("Description"));
inline static const FName BlueprintHelperLocalCategoryMetaKey = FName(TEXT("Category"));

static void SetOptionalField(FString* OutField, const FString& Field)
{
	if (OutField)
	{
		*OutField = Field;
	}
}

static FString NormalizePropertyPath(const FString& PropertyPath)
{
	FString Result = PropertyPath;
	Result.TrimStartAndEndInline();
	Result.ReplaceInline(TEXT("-"), TEXT("_"));
	Result.ReplaceInline(TEXT(" "), TEXT("_"));
	return Result.ToLower();
}

static FString NormalizeContainerName(const FString& Container)
{
	FString Result = Container;
	Result.TrimStartAndEndInline();
	Result = Result.ToLower();
	if (Result.IsEmpty() || Result == TEXT("none"))
	{
		return TEXT("single");
	}
	return Result;
}

static bool TryGetOptionalScalarStringField(
	const TSharedPtr<FJsonObject>& Payload,
	const TCHAR* FieldName,
	TOptional<FString>& OutValue,
	FString& OutError,
	FString* OutField)
{
	if (!Payload.IsValid() || !Payload->HasField(FieldName))
	{
		return true;
	}

	FString ConvertedValue;
	if (!FBlueprintHelperLocalVariableMutationHandler::TryScalarJsonToBlueprintDefaultString(
		Payload->TryGetField(FieldName),
		ConvertedValue))
	{
		OutError = FString::Printf(TEXT("%s must be a scalar JSON value."), FieldName);
		SetOptionalField(OutField, FieldName);
		return false;
	}

	OutValue = ConvertedValue;
	return true;
}

static bool RejectUnsupportedLocalFields(
	const TSharedPtr<FJsonObject>& Payload,
	const TArray<FString>& FieldNames,
	FString& OutError,
	FString* OutField,
	FString* OutErrorCode = nullptr)
{
	if (!Payload.IsValid())
	{
		return true;
	}

	for (const FString& FieldName : FieldNames)
	{
		if (Payload->HasField(FieldName))
		{
			OutError = FString::Printf(TEXT("Local variables do not support '%s'."), *FieldName);
			SetOptionalField(OutField, FieldName);
			if (FieldName.Equals(TEXT("replication"), ESearchCase::IgnoreCase))
			{
				SetOptionalField(OutErrorCode, TEXT("local_variable_replication_unsupported"));
			}
			return false;
		}
	}

	return true;
}

static UK2Node_FunctionEntry* FindFunctionEntryNode(UEdGraph* FunctionGraph)
{
	if (!FunctionGraph)
	{
		return nullptr;
	}

	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FunctionGraph->GetNodesOfClass<UK2Node_FunctionEntry>(EntryNodes);
	return EntryNodes.Num() > 0 ? EntryNodes[0] : nullptr;
}

static const UK2Node_FunctionEntry* FindFunctionEntryNode(const UEdGraph* FunctionGraph)
{
	return FindFunctionEntryNode(const_cast<UEdGraph*>(FunctionGraph));
}

static FBPVariableDescription* FindLocalVariableOnEntry(UK2Node_FunctionEntry* EntryNode, const FName VariableName)
{
	if (!EntryNode)
	{
		return nullptr;
	}

	for (FBPVariableDescription& LocalVariable : EntryNode->LocalVariables)
	{
		if (LocalVariable.VarName == VariableName)
		{
			return &LocalVariable;
		}
	}

	return nullptr;
}

static const FBPVariableDescription* FindLocalVariableOnEntry(const UK2Node_FunctionEntry* EntryNode, const FName VariableName)
{
	return FindLocalVariableOnEntry(const_cast<UK2Node_FunctionEntry*>(EntryNode), VariableName);
}

static bool FunctionGraphMatchesName(const UEdGraph* Graph, const FString& FunctionName)
{
	if (!Graph || FunctionName.IsEmpty())
	{
		return false;
	}

	if (Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase) ||
		Graph->GetFName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	const UK2Node_FunctionEntry* EntryNode = FindFunctionEntryNode(Graph);
	if (!EntryNode)
	{
		return false;
	}

	if (!EntryNode->CustomGeneratedFunctionName.IsNone() &&
		EntryNode->CustomGeneratedFunctionName.ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	return EntryNode->FunctionReference.GetMemberName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase);
}

static UFunction* FindFunctionScopeByName(UBlueprint* Blueprint, const FName ScopeName)
{
	if (!Blueprint || ScopeName.IsNone())
	{
		return nullptr;
	}

	if (Blueprint->SkeletonGeneratedClass)
	{
		if (UFunction* Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(ScopeName))
		{
			return Function;
		}
	}

	if (Blueprint->GeneratedClass)
	{
		if (UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(ScopeName))
		{
			return Function;
		}
	}

	return nullptr;
}

static bool IsLocalVariablePropertyUnsupportedRenameOrType(const FString& NormalizedPath, FString& OutError)
{
	if (NormalizedPath == TEXT("name") || NormalizedPath == TEXT("variable_name"))
	{
		OutError = TEXT("Local variable rename is unsupported.");
		return true;
	}

	if (NormalizedPath == TEXT("type") ||
		NormalizedPath == TEXT("pin_type") ||
		NormalizedPath == TEXT("variable_type"))
	{
		OutError = TEXT("Local variable type changes are unsupported.");
		return true;
	}

	return false;
}

static bool IsLocalVariablePropertyUnsupportedMemberOnly(const FString& NormalizedPath, FString& OutError)
{
	if (NormalizedPath == TEXT("instance_editable") ||
		NormalizedPath == TEXT("expose_on_spawn") ||
		NormalizedPath == TEXT("replication") ||
		NormalizedPath == TEXT("class_default"))
	{
		OutError = FString::Printf(TEXT("Local variables do not support '%s'."), *NormalizedPath);
		return true;
	}

	return false;
}

static bool ValidateLocalVariablePropertyPath(const FString& PropertyPath, FString& OutError, FString* OutErrorCode = nullptr)
{
	const FString NormalizedPath = NormalizePropertyPath(PropertyPath);
	if (IsLocalVariablePropertyUnsupportedRenameOrType(NormalizedPath, OutError) ||
		IsLocalVariablePropertyUnsupportedMemberOnly(NormalizedPath, OutError))
	{
		if (NormalizedPath == TEXT("replication"))
		{
			SetOptionalField(OutErrorCode, TEXT("local_variable_replication_unsupported"));
		}
		return false;
	}

	if (NormalizedPath == TEXT("category") ||
		NormalizedPath == TEXT("tooltip") ||
		NormalizedPath == TEXT("description") ||
		NormalizedPath == TEXT("default_value"))
	{
		return true;
	}

	OutError = FString::Printf(TEXT("Unsupported local variable property: %s."), *PropertyPath);
	return false;
}

static bool TryReadVariableTypeObject(
	const TSharedPtr<FJsonObject>& TypeObject,
	FBlueprintHelperVariableType& OutVariableType,
	FString& OutError,
	FString* OutField,
	const FString& FieldPrefix)
{
	if (!TypeObject.IsValid())
	{
		OutError = TEXT("variable_type or pin_type object is required.");
		SetOptionalField(OutField, FieldPrefix.IsEmpty() ? FString(TEXT("variable_type")) : FieldPrefix);
		return false;
	}

	TypeObject->TryGetStringField(TEXT("category"), OutVariableType.Category);
	if (OutVariableType.Category.IsEmpty())
	{
		OutError = TEXT("variable_type.category is required.");
		SetOptionalField(OutField, FieldPrefix.IsEmpty() ? FString(TEXT("variable_type.category")) : FieldPrefix + TEXT(".category"));
		return false;
	}

	FString Subtype;
	TypeObject->TryGetStringField(TEXT("subtype"), Subtype);
	if (Subtype.IsEmpty())
	{
		TypeObject->TryGetStringField(TEXT("object_path"), Subtype);
	}
	if (Subtype.IsEmpty())
	{
		TypeObject->TryGetStringField(TEXT("sub_category_object_path"), Subtype);
	}
	if (!Subtype.IsEmpty())
	{
		OutVariableType.Subtype = Subtype;
	}

	FString Container;
	TypeObject->TryGetStringField(TEXT("container"), Container);
	if (Container.IsEmpty())
	{
		TypeObject->TryGetStringField(TEXT("container_type"), Container);
	}
	OutVariableType.Container = NormalizeContainerName(Container);

	const TSharedPtr<FJsonObject>* KeyTypeObject = nullptr;
	if (TypeObject->TryGetObjectField(TEXT("key_type"), KeyTypeObject) && KeyTypeObject && KeyTypeObject->IsValid())
	{
		TSharedPtr<FBlueprintHelperVariableType> KeyType = MakeShared<FBlueprintHelperVariableType>();
		if (!TryReadVariableTypeObject(*KeyTypeObject, *KeyType, OutError, OutField, FieldPrefix + TEXT(".key_type")))
		{
			return false;
		}
		OutVariableType.KeyType = KeyType;
	}

	const TSharedPtr<FJsonObject>* ValueTypeObject = nullptr;
	if (TypeObject->TryGetObjectField(TEXT("value_type"), ValueTypeObject) && ValueTypeObject && ValueTypeObject->IsValid())
	{
		TSharedPtr<FBlueprintHelperVariableType> ValueType = MakeShared<FBlueprintHelperVariableType>();
		if (!TryReadVariableTypeObject(*ValueTypeObject, *ValueType, OutError, OutField, FieldPrefix + TEXT(".value_type")))
		{
			return false;
		}
		OutVariableType.ValueType = ValueType;
	}

	return true;
}

static bool TryLoadSubtypeObject(const FString& Subtype, UObject*& OutObject, FString& OutError)
{
	OutObject = nullptr;
	if (Subtype.IsEmpty())
	{
		return true;
	}

	OutObject = LoadObject<UObject>(nullptr, *Subtype);
	if (!OutObject)
	{
		OutError = FString::Printf(TEXT("Unable to load variable subtype object: %s."), *Subtype);
		return false;
	}

	return true;
}

static FString GetVariableMetaDataValue(const FBPVariableDescription& Variable, const FName Key)
{
	return Variable.FindMetaDataEntryIndexForKey(Key) != INDEX_NONE
		? Variable.GetMetaData(Key)
		: FString();
}

static bool SetVariableMetaDataValue(
	FBPVariableDescription& Variable,
	const FName Key,
	const FString& NewValue,
	bool& bOutChanged)
{
	bOutChanged = false;
	const FString OldValue = GetVariableMetaDataValue(Variable, Key);
	if (OldValue == NewValue)
	{
		return true;
	}

	if (NewValue.IsEmpty())
	{
		Variable.RemoveMetaData(Key);
	}
	else
	{
		Variable.SetMetaData(Key, NewValue);
	}
	bOutChanged = true;
	return true;
}

static bool ApplyValidatedPropertySetting(
	FBPVariableDescription& Variable,
	const FString& NormalizedPath,
	const FString& NewValue,
	bool& bOutChanged)
{
	bOutChanged = false;

	if (NormalizedPath == TEXT("category"))
	{
		const FString OldCategory = Variable.Category.ToString();
		if (OldCategory == NewValue)
		{
			return true;
		}

		Variable.Category = FText::FromString(NewValue);
		if (NewValue.IsEmpty())
		{
			Variable.RemoveMetaData(BlueprintHelperLocalCategoryMetaKey);
		}
		else
		{
			Variable.SetMetaData(BlueprintHelperLocalCategoryMetaKey, NewValue);
		}
		bOutChanged = true;
		return true;
	}

	if (NormalizedPath == TEXT("tooltip"))
	{
		return SetVariableMetaDataValue(Variable, FBlueprintMetadata::MD_Tooltip, NewValue, bOutChanged);
	}

	if (NormalizedPath == TEXT("description"))
	{
		return SetVariableMetaDataValue(Variable, BlueprintHelperLocalDescriptionMetaKey, NewValue, bOutChanged);
	}

	if (NormalizedPath == TEXT("default_value"))
	{
		if (Variable.DefaultValue == NewValue)
		{
			return true;
		}

		Variable.DefaultValue = NewValue;
		bOutChanged = true;
		return true;
	}

	return false;
}

static bool WouldChangeValidatedPropertySetting(
	const FBPVariableDescription& Variable,
	const FString& NormalizedPath,
	const FString& NewValue)
{
	if (NormalizedPath == TEXT("category"))
	{
		return Variable.Category.ToString() != NewValue;
	}

	if (NormalizedPath == TEXT("tooltip"))
	{
		return GetVariableMetaDataValue(Variable, FBlueprintMetadata::MD_Tooltip) != NewValue;
	}

	if (NormalizedPath == TEXT("description"))
	{
		return GetVariableMetaDataValue(Variable, BlueprintHelperLocalDescriptionMetaKey) != NewValue;
	}

	if (NormalizedPath == TEXT("default_value"))
	{
		return Variable.DefaultValue != NewValue;
	}

	return false;
}

static bool ReadAddRequestsArray(
	const TSharedPtr<FJsonObject>& Payload,
	TArray<FBlueprintHelperLocalVariableAddRequest>& OutRequests,
	FString& OutError,
	FString* OutField)
{
	OutRequests.Reset();
	const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("variables"), VariablesArray) || !VariablesArray)
	{
		FBlueprintHelperLocalVariableAddRequest Request;
		if (!FBlueprintHelperLocalVariableMutationHandler::TryReadAddRequest(Payload, Request, OutError, OutField))
		{
			return false;
		}
		OutRequests.Add(MoveTemp(Request));
		return true;
	}

	if (VariablesArray->Num() == 0)
	{
		OutError = TEXT("variables array must not be empty.");
		SetOptionalField(OutField, TEXT("variables"));
		return false;
	}

	for (int32 Index = 0; Index < VariablesArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> VariableObject =
			(*VariablesArray)[Index].IsValid()
				? (*VariablesArray)[Index]->AsObject()
				: nullptr;
		FBlueprintHelperLocalVariableAddRequest Request;
		if (!FBlueprintHelperLocalVariableMutationHandler::TryReadAddRequest(VariableObject, Request, OutError, OutField))
		{
			if (OutField && !OutField->IsEmpty())
			{
				*OutField = FString::Printf(TEXT("variables[%d].%s"), Index, **OutField);
			}
			else
			{
				SetOptionalField(OutField, FString::Printf(TEXT("variables[%d]"), Index));
			}
			return false;
		}
		OutRequests.Add(MoveTemp(Request));
	}

	return true;
}

static bool ReadRemoveRequestsArray(
	const TSharedPtr<FJsonObject>& Payload,
	TArray<FBlueprintHelperLocalVariableRemoveRequest>& OutRequests,
	FString& OutError,
	FString* OutField)
{
	OutRequests.Reset();
	bool bRootDryRun = false;
	if (Payload.IsValid())
	{
		Payload->TryGetBoolField(TEXT("dry_run"), bRootDryRun);
	}

	const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
	if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("variables"), VariablesArray) || !VariablesArray)
	{
		FBlueprintHelperLocalVariableRemoveRequest Request;
		if (!FBlueprintHelperLocalVariableMutationHandler::TryReadRemoveRequest(Payload, Request, OutError, OutField))
		{
			return false;
		}
		OutRequests.Add(MoveTemp(Request));
		return true;
	}

	if (VariablesArray->Num() == 0)
	{
		OutError = TEXT("variables array must not be empty.");
		SetOptionalField(OutField, TEXT("variables"));
		return false;
	}

	for (int32 Index = 0; Index < VariablesArray->Num(); ++Index)
	{
		FBlueprintHelperLocalVariableRemoveRequest Request;
		Request.bDryRun = bRootDryRun;
		const TSharedPtr<FJsonValue>& Value = (*VariablesArray)[Index];
		if (Value.IsValid() && Value->Type == EJson::String)
		{
			Request.VariableName = Value->AsString();
		}
		else
		{
			const TSharedPtr<FJsonObject> VariableObject = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!FBlueprintHelperLocalVariableMutationHandler::TryReadRemoveRequest(VariableObject, Request, OutError, OutField))
			{
				if (OutField && !OutField->IsEmpty())
				{
					*OutField = FString::Printf(TEXT("variables[%d].%s"), Index, **OutField);
				}
				else
				{
					SetOptionalField(OutField, FString::Printf(TEXT("variables[%d]"), Index));
				}
				return false;
			}
		}

		if (Request.VariableName.IsEmpty())
		{
			OutError = TEXT("remove local variable entries require name or variable_name.");
			SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}
		OutRequests.Add(MoveTemp(Request));
	}

	return true;
}

};

bool FBlueprintHelperLocalVariableMutationHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("read_local_variables"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("read_blueprint_local_variables"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("add_local_variable"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("add_blueprint_local_variable"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("add_local_variables"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("add_blueprint_local_variables"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("ensure_local_variable"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("set_local_variable_properties"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("set_blueprint_local_variable_properties"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("remove_local_variable"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("remove_blueprint_local_variable"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("remove_local_variables"), ESearchCase::IgnoreCase)
		|| OpName.Equals(TEXT("remove_blueprint_local_variables"), ESearchCase::IgnoreCase);
}

bool FBlueprintHelperLocalVariableMutationHandler::Execute(
	UBlueprint* Blueprint,
	const TSharedPtr<FJsonObject>& OpPayload,
	FString& OutError)
{
	if (!Blueprint || !OpPayload.IsValid())
	{
		OutError = TEXT("local variable mutation failed: invalid Blueprint or payload.");
		return false;
	}

	FString OpName;
	OpPayload->TryGetStringField(TEXT("op"), OpName);
	if (OpName.IsEmpty())
	{
		OpPayload->TryGetStringField(TEXT("operation"), OpName);
	}

	FString FunctionName;
	if (!TryReadFunctionName(OpPayload, FunctionName))
	{
		OutError = TEXT("local variable operations require function_name.");
		return false;
	}

	FString Field;
	if (OpName.Equals(TEXT("read_local_variables"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("read_blueprint_local_variables"), ESearchCase::IgnoreCase))
	{
		TArray<FBlueprintHelperLocalVariableItem> LocalVariables;
		return ReadLocalVariables(Blueprint, FunctionName, LocalVariables, OutError, &Field);
	}

	if (OpName.Equals(TEXT("add_local_variable"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("add_blueprint_local_variable"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("add_local_variables"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("add_blueprint_local_variables"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("ensure_local_variable"), ESearchCase::IgnoreCase))
	{
		TArray<FBlueprintHelperLocalVariableAddRequest> Requests;
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::ReadAddRequestsArray(OpPayload, Requests, OutError, &Field))
		{
			return false;
		}

		FBlueprintHelperLocalVariableMutationCounts Counts;
		return ApplyAddLocalVariables(Blueprint, FunctionName, Requests, Counts, OutError, &Field);
	}

	if (OpName.Equals(TEXT("set_local_variable_properties"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("set_blueprint_local_variable_properties"), ESearchCase::IgnoreCase))
	{
		FString VariableName;
		if (!TryReadVariableName(OpPayload, VariableName))
		{
			OutError = TEXT("set local variable properties requires name or variable_name.");
			return false;
		}

		TArray<FBlueprintHelperLocalVariablePropertyMutation> Settings;
		if (!TryReadPropertySettings(OpPayload, Settings, OutError, &Field))
		{
			return false;
		}

		FBlueprintHelperLocalVariableMutationCounts Counts;
		return ApplyPropertySettings(Blueprint, FunctionName, VariableName, Settings, Counts, OutError, &Field);
	}

	if (OpName.Equals(TEXT("remove_local_variable"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("remove_blueprint_local_variable"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("remove_local_variables"), ESearchCase::IgnoreCase) ||
		OpName.Equals(TEXT("remove_blueprint_local_variables"), ESearchCase::IgnoreCase))
	{
		TArray<FBlueprintHelperLocalVariableRemoveRequest> Requests;
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::ReadRemoveRequestsArray(OpPayload, Requests, OutError, &Field))
		{
			return false;
		}

		FBlueprintHelperLocalVariableMutationCounts Counts;
		return ApplyRemoveLocalVariables(Blueprint, FunctionName, Requests, Counts, OutError, &Field);
	}

	OutError = FString::Printf(TEXT("unsupported local variable mutation op: %s"), *OpName);
	return false;
}

bool FBlueprintHelperLocalVariableMutationHandler::TryReadFunctionName(
	const TSharedPtr<FJsonObject>& Payload,
	FString& OutFunctionName)
{
	if (!Payload.IsValid())
	{
		return false;
	}

	Payload->TryGetStringField(TEXT("function_name"), OutFunctionName);
	return !OutFunctionName.IsEmpty();
}

bool FBlueprintHelperLocalVariableMutationHandler::TryReadVariableName(
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

bool FBlueprintHelperLocalVariableMutationHandler::TryReadDefaultValue(
	const TSharedPtr<FJsonObject>& Payload,
	TOptional<FString>& OutDefaultValue,
	FString& OutError,
	FString* OutField)
{
	OutDefaultValue.Reset();
	if (!Payload.IsValid())
	{
		OutError = TEXT("payload is required.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("payload"));
		return false;
	}

	TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("default_value"));
	FString FieldName = TEXT("default_value");
	if (!Value.IsValid())
	{
		Value = Payload->TryGetField(TEXT("value"));
		FieldName = TEXT("value");
	}

	if (!Value.IsValid())
	{
		return true;
	}

	FString ConvertedValue;
	if (!TryScalarJsonToBlueprintDefaultString(Value, ConvertedValue))
	{
		OutError = FString::Printf(TEXT("%s must be a scalar JSON value."), *FieldName);
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FieldName);
		return false;
	}

	OutDefaultValue = ConvertedValue;
	return true;
}

bool FBlueprintHelperLocalVariableMutationHandler::TryReadPropertySetting(
	const TSharedPtr<FJsonObject>& SettingObject,
	FBlueprintHelperLocalVariablePropertyMutation& OutSetting)
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

bool FBlueprintHelperLocalVariableMutationHandler::TryReadNameCollisionPolicy(
	const TSharedPtr<FJsonObject>& Payload,
	EBlueprintHelperVariableNameCollisionPolicy& OutPolicy,
	FString& OutError,
	FString* OutField)
{
	OutPolicy = EBlueprintHelperVariableNameCollisionPolicy::FailIfExists;
	if (!Payload.IsValid())
	{
		return true;
	}

	FString PolicyString;
	Payload->TryGetStringField(TEXT("name_collision"), PolicyString);
	if (PolicyString.IsEmpty())
	{
		Payload->TryGetStringField(TEXT("name_collision_policy"), PolicyString);
	}
	if (PolicyString.IsEmpty())
	{
		return true;
	}

	if (PolicyString.Equals(TEXT("fail_if_exists"), ESearchCase::IgnoreCase))
	{
		OutPolicy = EBlueprintHelperVariableNameCollisionPolicy::FailIfExists;
		return true;
	}

	if (PolicyString.Equals(TEXT("reuse_if_exists"), ESearchCase::IgnoreCase))
	{
		OutPolicy = EBlueprintHelperVariableNameCollisionPolicy::ReuseIfExists;
		return true;
	}

	OutError = FString::Printf(TEXT("Unsupported local variable name_collision policy: %s."), *PolicyString);
	FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, Payload->HasField(TEXT("name_collision")) ? TEXT("name_collision") : TEXT("name_collision_policy"));
	return false;
}

bool FBlueprintHelperLocalVariableMutationHandler::TryReadVariableType(
	const TSharedPtr<FJsonObject>& Payload,
	FBlueprintHelperVariableType& OutVariableType,
	FString& OutError,
	FString* OutField)
{
	if (!Payload.IsValid())
	{
		OutError = TEXT("payload is required.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("payload"));
		return false;
	}

	const TSharedPtr<FJsonObject>* TypeObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("variable_type"), TypeObject) && TypeObject && TypeObject->IsValid())
	{
		return FBlueprintLocalVariableMutationHandlerLocalUtils::TryReadVariableTypeObject(*TypeObject, OutVariableType, OutError, OutField, TEXT("variable_type"));
	}

	if (Payload->TryGetObjectField(TEXT("pin_type"), TypeObject) && TypeObject && TypeObject->IsValid())
	{
		return FBlueprintLocalVariableMutationHandlerLocalUtils::TryReadVariableTypeObject(*TypeObject, OutVariableType, OutError, OutField, TEXT("pin_type"));
	}

	const bool bLooksLikeStandaloneType =
		Payload->HasField(TEXT("category")) &&
		!Payload->HasField(TEXT("name")) &&
		!Payload->HasField(TEXT("variable_name")) &&
		!Payload->HasField(TEXT("function_name"));
	if (bLooksLikeStandaloneType)
	{
		return FBlueprintLocalVariableMutationHandlerLocalUtils::TryReadVariableTypeObject(Payload, OutVariableType, OutError, OutField, TEXT("variable_type"));
	}

	OutError = TEXT("variable_type or pin_type object is required.");
	FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("variable_type"));
	return false;
}

bool FBlueprintHelperLocalVariableMutationHandler::TryReadAddRequest(
	const TSharedPtr<FJsonObject>& Payload,
	FBlueprintHelperLocalVariableAddRequest& OutRequest,
	FString& OutError,
	FString* OutField)
{
	if (!Payload.IsValid())
	{
		OutError = TEXT("local variable add request must be an object.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("variable"));
		return false;
	}

	if (!FBlueprintLocalVariableMutationHandlerLocalUtils::RejectUnsupportedLocalFields(
		Payload,
		{
			TEXT("instance_editable"),
			TEXT("expose_on_spawn"),
			TEXT("replication"),
			TEXT("class_default")
		},
		OutError,
		OutField))
	{
		return false;
	}

	if (!TryReadVariableName(Payload, OutRequest.VariableName))
	{
		OutError = TEXT("local variable add request requires name or variable_name.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("name"));
		return false;
	}

	if (!TryReadVariableType(Payload, OutRequest.VariableType, OutError, OutField))
	{
		return false;
	}

	if (!TryReadDefaultValue(Payload, OutRequest.DefaultValue, OutError, OutField))
	{
		return false;
	}

	if (!TryReadNameCollisionPolicy(Payload, OutRequest.NameCollisionPolicy, OutError, OutField))
	{
		return false;
	}

	if (!FBlueprintLocalVariableMutationHandlerLocalUtils::TryGetOptionalScalarStringField(Payload, TEXT("category"), OutRequest.Category, OutError, OutField) ||
		!FBlueprintLocalVariableMutationHandlerLocalUtils::TryGetOptionalScalarStringField(Payload, TEXT("tooltip"), OutRequest.Tooltip, OutError, OutField) ||
		!FBlueprintLocalVariableMutationHandlerLocalUtils::TryGetOptionalScalarStringField(Payload, TEXT("description"), OutRequest.Description, OutError, OutField))
	{
		return false;
	}

	return true;
}

bool FBlueprintHelperLocalVariableMutationHandler::TryReadPropertySettings(
	const TSharedPtr<FJsonObject>& Payload,
	TArray<FBlueprintHelperLocalVariablePropertyMutation>& OutSettings,
	FString& OutError,
	FString* OutField,
	FString* OutErrorCode)
{
	OutSettings.Reset();
	FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutErrorCode, TEXT(""));
	if (!Payload.IsValid())
	{
		OutError = TEXT("payload is required.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("payload"));
		return false;
	}

	if (!FBlueprintLocalVariableMutationHandlerLocalUtils::RejectUnsupportedLocalFields(
		Payload,
		{
			TEXT("variable_type"),
			TEXT("pin_type"),
			TEXT("type"),
			TEXT("instance_editable"),
			TEXT("expose_on_spawn"),
			TEXT("replication"),
			TEXT("class_default")
		},
		OutError,
		OutField,
		OutErrorCode))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
	{
		for (int32 Index = 0; Index < SettingsArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> SettingObject =
				(*SettingsArray)[Index].IsValid()
					? (*SettingsArray)[Index]->AsObject()
					: nullptr;

			FBlueprintHelperLocalVariablePropertyMutation Setting;
			if (!TryReadPropertySetting(SettingObject, Setting))
			{
				OutError = TEXT("settings entries require property_path and value.");
				FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("settings[%d]"), Index));
				return false;
			}

			FString ValidationError;
			if (!FBlueprintLocalVariableMutationHandlerLocalUtils::ValidateLocalVariablePropertyPath(Setting.PropertyPath, ValidationError, OutErrorCode))
			{
				OutError = ValidationError;
				FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("settings[%d].property_path"), Index));
				return false;
			}

			OutSettings.Add(MoveTemp(Setting));
		}
	}
	else
	{
		const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
		if (!Payload->TryGetObjectField(TEXT("properties"), PropertiesObject) || !PropertiesObject || !PropertiesObject->IsValid())
		{
			OutError = TEXT("settings array or properties object is required.");
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("settings"));
			return false;
		}

		for (const auto& Pair : (*PropertiesObject)->Values)
		{
			const FString Key = FBlueprintHelperVersionCompat::JsonKeyToString(Pair.Key);
			FString ValidationError;
			if (!FBlueprintLocalVariableMutationHandlerLocalUtils::ValidateLocalVariablePropertyPath(Key, ValidationError, OutErrorCode))
			{
				OutError = ValidationError;
				FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("properties.%s"), *Key));
				return false;
			}

			FBlueprintHelperLocalVariablePropertyMutation Setting;
			Setting.PropertyPath = Key;
			Setting.Value = Pair.Value;
			OutSettings.Add(MoveTemp(Setting));
		}
	}

	if (OutSettings.Num() == 0)
	{
		OutError = TEXT("at least one local variable property setting is required.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("settings"));
		return false;
	}

	return true;
}

bool FBlueprintHelperLocalVariableMutationHandler::TryReadRemoveRequest(
	const TSharedPtr<FJsonObject>& Payload,
	FBlueprintHelperLocalVariableRemoveRequest& OutRequest,
	FString& OutError,
	FString* OutField)
{
	if (!Payload.IsValid())
	{
		OutError = TEXT("local variable remove request must be an object.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("variable"));
		return false;
	}

	if (!TryReadVariableName(Payload, OutRequest.VariableName))
	{
		OutError = TEXT("local variable remove request requires name or variable_name.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("name"));
		return false;
	}

	Payload->TryGetBoolField(TEXT("dry_run"), OutRequest.bDryRun);
	return true;
}

bool FBlueprintHelperLocalVariableMutationHandler::TryScalarJsonToBlueprintDefaultString(
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

bool FBlueprintHelperLocalVariableMutationHandler::TryBuildPinType(
	const FBlueprintHelperVariableType& VariableType,
	FEdGraphPinType& OutPinType,
	FString& OutError)
{
	OutPinType = FEdGraphPinType();

	const FString Category = VariableType.Category.ToLower();
	if (Category.IsEmpty())
	{
		OutError = TEXT("variable_type.category is required.");
		return false;
	}

	const FString Container = FBlueprintLocalVariableMutationHandlerLocalUtils::NormalizeContainerName(VariableType.Container);
	if (Container == TEXT("array"))
	{
		OutPinType.ContainerType = EPinContainerType::Array;
	}
	else if (Container == TEXT("single"))
	{
		OutPinType.ContainerType = EPinContainerType::None;
	}
	else
	{
		OutError = FString::Printf(TEXT("Unsupported local variable container: %s."), *VariableType.Container);
		return false;
	}

	if (Category == TEXT("bool") || Category == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Category == TEXT("int") || Category == TEXT("int32") || Category == TEXT("integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Category == TEXT("int64") || Category == TEXT("integer64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (Category == TEXT("float"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (Category == TEXT("double") || Category == TEXT("real"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = Category == TEXT("double")
			? UEdGraphSchema_K2::PC_Double
			: UEdGraphSchema_K2::PC_Float;
	}
	else if (Category == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Category == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Category == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (Category == TEXT("object"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		UObject* SubtypeObject = nullptr;
		const FString Subtype = VariableType.Subtype.IsSet() ? VariableType.Subtype.GetValue() : FString();
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::TryLoadSubtypeObject(Subtype, SubtypeObject, OutError))
		{
			return false;
		}
		OutPinType.PinSubCategoryObject = SubtypeObject ? SubtypeObject : UObject::StaticClass();
	}
	else if (Category == TEXT("class"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		UObject* SubtypeObject = nullptr;
		const FString Subtype = VariableType.Subtype.IsSet() ? VariableType.Subtype.GetValue() : FString();
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::TryLoadSubtypeObject(Subtype, SubtypeObject, OutError))
		{
			return false;
		}
		OutPinType.PinSubCategoryObject = SubtypeObject ? SubtypeObject : UObject::StaticClass();
	}
	else if (Category == TEXT("struct"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		if (!VariableType.Subtype.IsSet() || VariableType.Subtype.GetValue().IsEmpty())
		{
			OutError = TEXT("struct local variable type requires subtype.");
			return false;
		}

		UObject* SubtypeObject = nullptr;
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::TryLoadSubtypeObject(VariableType.Subtype.GetValue(), SubtypeObject, OutError))
		{
			return false;
		}
		if (!SubtypeObject->IsA<UScriptStruct>())
		{
			OutError = FString::Printf(TEXT("Struct subtype is not a UScriptStruct: %s."), *VariableType.Subtype.GetValue());
			return false;
		}
		OutPinType.PinSubCategoryObject = SubtypeObject;
	}
	else if (Category == TEXT("enum"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
		if (!VariableType.Subtype.IsSet() || VariableType.Subtype.GetValue().IsEmpty())
		{
			OutError = TEXT("enum local variable type requires subtype.");
			return false;
		}

		UObject* SubtypeObject = nullptr;
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::TryLoadSubtypeObject(VariableType.Subtype.GetValue(), SubtypeObject, OutError))
		{
			return false;
		}
		if (!SubtypeObject->IsA<UEnum>())
		{
			OutError = FString::Printf(TEXT("Enum subtype is not a UEnum: %s."), *VariableType.Subtype.GetValue());
			return false;
		}
		OutPinType.PinSubCategoryObject = SubtypeObject;
	}
	else
	{
		OutError = FString::Printf(TEXT("Unsupported local variable type category: %s."), *VariableType.Category);
		return false;
	}

	return true;
}

FBlueprintHelperVariableType FBlueprintHelperLocalVariableMutationHandler::ConvertPinTypeToVariableType(
	const FEdGraphPinType& PinType)
{
	FBlueprintHelperVariableType VariableType;

	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		VariableType.Category = TEXT("bool");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		VariableType.Category = TEXT("int");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		VariableType.Category = TEXT("int64");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		VariableType.Category = PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double
			? TEXT("double")
			: TEXT("float");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		VariableType.Category = TEXT("string");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		VariableType.Category = TEXT("name");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		VariableType.Category = TEXT("text");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		VariableType.Category = TEXT("object");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		VariableType.Category = TEXT("class");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		VariableType.Category = TEXT("struct");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
	{
		VariableType.Category = TEXT("enum");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte && PinType.PinSubCategoryObject.IsValid())
	{
		VariableType.Category = TEXT("enum");
	}
	else
	{
		VariableType.Category = PinType.PinCategory.ToString();
	}

	if (PinType.PinSubCategoryObject.IsValid())
	{
		VariableType.Subtype = PinType.PinSubCategoryObject->GetPathName();
	}

	if (PinType.IsArray())
	{
		VariableType.Container = TEXT("array");
	}
	else if (PinType.IsSet())
	{
		VariableType.Container = TEXT("set");
	}
	else if (PinType.IsMap())
	{
		VariableType.Container = TEXT("map");
	}
	else
	{
		VariableType.Container = TEXT("single");
	}

	return VariableType;
}

bool FBlueprintHelperLocalVariableMutationHandler::ResolveFunctionGraph(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	UEdGraph*& OutFunctionGraph,
	FString& OutError,
	FString* OutField)
{
	OutFunctionGraph = nullptr;
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint asset could not be resolved.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("asset_path"));
		return false;
	}

	if (FunctionName.IsEmpty())
	{
		OutError = TEXT("function_name is required for local variable operations.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("function_name"));
		return false;
	}

	auto TryUseGraph = [&OutFunctionGraph, &FunctionName](UEdGraph* Graph)
	{
		if (!Graph || !FBlueprintLocalVariableMutationHandlerLocalUtils::FunctionGraphMatchesName(Graph, FunctionName))
		{
			return false;
		}

		if (!FBlueprintEditorUtils::DoesSupportLocalVariables(Graph))
		{
			return false;
		}

		OutFunctionGraph = FBlueprintEditorUtils::GetTopLevelGraph(Graph);
		return OutFunctionGraph != nullptr;
	};

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (TryUseGraph(Graph))
		{
			return true;
		}
	}

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		if (TryUseGraph(Graph))
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Function graph '%s' was not found or does not support local variables."), *FunctionName);
	FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("function_name"));
	return false;
}

bool FBlueprintHelperLocalVariableMutationHandler::ResolveFunctionScope(
	UBlueprint* Blueprint,
	UEdGraph* FunctionGraph,
	UStruct*& OutScope,
	FString& OutError,
	FString* OutField)
{
	OutScope = nullptr;
	if (!Blueprint || !FunctionGraph)
	{
		OutError = TEXT("Blueprint and function graph are required to resolve local variable scope.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, !Blueprint ? TEXT("asset_path") : TEXT("function_name"));
		return false;
	}

	if (UFunction* ScopeFunction = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionScopeByName(Blueprint, FunctionGraph->GetFName()))
	{
		OutScope = ScopeFunction;
		return true;
	}

	const UK2Node_FunctionEntry* EntryNode = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionEntryNode(FunctionGraph);
	if (EntryNode)
	{
		if (!EntryNode->CustomGeneratedFunctionName.IsNone())
		{
			if (UFunction* ScopeFunction = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionScopeByName(Blueprint, EntryNode->CustomGeneratedFunctionName))
			{
				OutScope = ScopeFunction;
				return true;
			}
		}

		const FName MemberName = EntryNode->FunctionReference.GetMemberName();
		if (!MemberName.IsNone())
		{
			if (UFunction* ScopeFunction = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionScopeByName(Blueprint, MemberName))
			{
				OutScope = ScopeFunction;
				return true;
			}
		}
	}

	OutError = FString::Printf(
		TEXT("Function scope for '%s' could not be resolved. Compile the Blueprint before removing local variables."),
		*FunctionGraph->GetName());
	FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("function_name"));
	return false;
}

bool FBlueprintHelperLocalVariableMutationHandler::ReadLocalVariables(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	TArray<FBlueprintHelperLocalVariableItem>& OutLocalVariables,
	FString& OutError,
	FString* OutField)
{
	OutLocalVariables.Reset();

	UEdGraph* FunctionGraph = nullptr;
	if (!ResolveFunctionGraph(Blueprint, FunctionName, FunctionGraph, OutError, OutField))
	{
		return false;
	}

	const UK2Node_FunctionEntry* EntryNode = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionEntryNode(FunctionGraph);
	if (!EntryNode)
	{
		OutError = FString::Printf(TEXT("Function graph '%s' has no function entry node."), *FunctionName);
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("function_name"));
		return false;
	}

	for (const FBPVariableDescription& LocalVariable : EntryNode->LocalVariables)
	{
		FBlueprintHelperLocalVariableItem Item;
		Item.VariableName = LocalVariable.VarName.ToString();
		Item.VariableType = ConvertPinTypeToVariableType(LocalVariable.VarType);
		if (!LocalVariable.DefaultValue.IsEmpty())
		{
			Item.DefaultValue = LocalVariable.DefaultValue;
		}
		OutLocalVariables.Add(MoveTemp(Item));
	}

	return true;
}

bool FBlueprintHelperLocalVariableMutationHandler::ApplyAddLocalVariables(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	const TArray<FBlueprintHelperLocalVariableAddRequest>& Requests,
	FBlueprintHelperLocalVariableMutationCounts& OutCounts,
	FString& OutError,
	FString* OutField)
{
	OutCounts = {};
	OutCounts.RequestedCount = Requests.Num();

	UEdGraph* FunctionGraph = nullptr;
	if (!ResolveFunctionGraph(Blueprint, FunctionName, FunctionGraph, OutError, OutField))
	{
		return false;
	}

	UK2Node_FunctionEntry* EntryNode = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionEntryNode(FunctionGraph);
	if (!EntryNode)
	{
		OutError = FString::Printf(TEXT("Function graph '%s' has no function entry node."), *FunctionName);
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("function_name"));
		return false;
	}

	if (Requests.Num() == 0)
	{
		OutError = TEXT("at least one local variable add request is required.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("variables"));
		return false;
	}

	TArray<FEdGraphPinType> PinTypes;
	PinTypes.SetNum(Requests.Num());
	TSet<FName> PlannedAddNames;

	for (int32 Index = 0; Index < Requests.Num(); ++Index)
	{
		const FBlueprintHelperLocalVariableAddRequest& Request = Requests[Index];
		if (Request.VariableName.IsEmpty())
		{
			OutError = TEXT("Local variable add request requires name or variable_name.");
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}

		const FName VariableFName(*Request.VariableName);
		const bool bExists = FBlueprintLocalVariableMutationHandlerLocalUtils::FindLocalVariableOnEntry(EntryNode, VariableFName) != nullptr;
		const bool bPlanned = PlannedAddNames.Contains(VariableFName);
		if ((bExists || bPlanned) &&
			Request.NameCollisionPolicy == EBlueprintHelperVariableNameCollisionPolicy::FailIfExists)
		{
			OutError = FString::Printf(TEXT("Local variable '%s' already exists."), *Request.VariableName);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}

		if (!TryBuildPinType(Request.VariableType, PinTypes[Index], OutError))
		{
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].variable_type"), Index));
			return false;
		}

		if (!bExists && !bPlanned)
		{
			PlannedAddNames.Add(VariableFName);
		}
	}

	const TArray<FBPVariableDescription> OriginalLocalVariables = EntryNode->LocalVariables;

	for (int32 Index = 0; Index < Requests.Num(); ++Index)
	{
		const FBlueprintHelperLocalVariableAddRequest& Request = Requests[Index];
		const FName VariableFName(*Request.VariableName);
		if (FBlueprintLocalVariableMutationHandlerLocalUtils::FindLocalVariableOnEntry(EntryNode, VariableFName))
		{
			++OutCounts.NoOpCount;
			continue;
		}

		const FString DefaultValue = Request.DefaultValue.IsSet() ? Request.DefaultValue.GetValue() : FString();
		if (!FBlueprintEditorUtils::AddLocalVariable(Blueprint, FunctionGraph, VariableFName, PinTypes[Index], DefaultValue))
		{
			EntryNode->LocalVariables = OriginalLocalVariables;
			OutError = FString::Printf(TEXT("Failed to add local variable '%s'."), *Request.VariableName);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}

		EntryNode = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionEntryNode(FunctionGraph);
		FBPVariableDescription* AddedVariable = FBlueprintLocalVariableMutationHandlerLocalUtils::FindLocalVariableOnEntry(EntryNode, VariableFName);
		if (AddedVariable)
		{
			if (Request.Category.IsSet())
			{
				AddedVariable->Category = FText::FromString(*Request.Category);
				AddedVariable->SetMetaData(FBlueprintLocalVariableMutationHandlerLocalUtils::BlueprintHelperLocalCategoryMetaKey, *Request.Category);
			}
			if (Request.Tooltip.IsSet())
			{
				AddedVariable->SetMetaData(FBlueprintMetadata::MD_Tooltip, *Request.Tooltip);
			}
			if (Request.Description.IsSet())
			{
				AddedVariable->SetMetaData(FBlueprintLocalVariableMutationHandlerLocalUtils::BlueprintHelperLocalDescriptionMetaKey, *Request.Description);
			}
		}

		++OutCounts.AddedCount;
	}

	if (OutCounts.AddedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	return true;
}

bool FBlueprintHelperLocalVariableMutationHandler::ApplyPropertySettings(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	const FString& VariableName,
	const TArray<FBlueprintHelperLocalVariablePropertyMutation>& Settings,
	FBlueprintHelperLocalVariableMutationCounts& OutCounts,
	FString& OutError,
	FString* OutField)
{
	OutCounts = {};
	OutCounts.RequestedCount = Settings.Num();

	UEdGraph* FunctionGraph = nullptr;
	if (!ResolveFunctionGraph(Blueprint, FunctionName, FunctionGraph, OutError, OutField))
	{
		return false;
	}

	UK2Node_FunctionEntry* EntryNode = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionEntryNode(FunctionGraph);
	if (!EntryNode)
	{
		OutError = FString::Printf(TEXT("Function graph '%s' has no function entry node."), *FunctionName);
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("function_name"));
		return false;
	}

	if (VariableName.IsEmpty())
	{
		OutError = TEXT("Local variable property settings require name or variable_name.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("name"));
		return false;
	}

	FBPVariableDescription* LocalVariable = FBlueprintLocalVariableMutationHandlerLocalUtils::FindLocalVariableOnEntry(EntryNode, FName(*VariableName));
	if (!LocalVariable)
	{
		OutError = FString::Printf(TEXT("Local variable '%s' was not found."), *VariableName);
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("name"));
		return false;
	}

	if (Settings.Num() == 0)
	{
		OutError = TEXT("at least one local variable property setting is required.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("settings"));
		return false;
	}

	struct FValidatedSetting
	{
		FString NormalizedPath;
		FString NewValue;
	};

	TArray<FValidatedSetting> ValidatedSettings;
	ValidatedSettings.Reserve(Settings.Num());
	for (int32 Index = 0; Index < Settings.Num(); ++Index)
	{
		const FBlueprintHelperLocalVariablePropertyMutation& Setting = Settings[Index];
		FString ValidationError;
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::ValidateLocalVariablePropertyPath(Setting.PropertyPath, ValidationError))
		{
			OutError = ValidationError;
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("settings[%d].property_path"), Index));
			return false;
		}

		FString NewValue;
		if (!TryScalarJsonToBlueprintDefaultString(Setting.Value, NewValue))
		{
			OutError = FString::Printf(TEXT("%s must be a scalar JSON value."), *Setting.PropertyPath);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("settings[%d].value"), Index));
			return false;
		}

		FValidatedSetting ValidatedSetting;
		ValidatedSetting.NormalizedPath = FBlueprintLocalVariableMutationHandlerLocalUtils::NormalizePropertyPath(Setting.PropertyPath);
		ValidatedSetting.NewValue = NewValue;
		ValidatedSettings.Add(MoveTemp(ValidatedSetting));
	}

	bool bModified = false;
	for (int32 Index = 0; Index < ValidatedSettings.Num(); ++Index)
	{
		if (!bModified &&
			FBlueprintLocalVariableMutationHandlerLocalUtils::WouldChangeValidatedPropertySetting(
				*LocalVariable,
				ValidatedSettings[Index].NormalizedPath,
				ValidatedSettings[Index].NewValue))
		{
			Blueprint->Modify();
			EntryNode->Modify();
			bModified = true;
		}

		bool bChanged = false;
		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::ApplyValidatedPropertySetting(
			*LocalVariable,
			ValidatedSettings[Index].NormalizedPath,
			ValidatedSettings[Index].NewValue,
			bChanged))
		{
			OutError = FString::Printf(TEXT("Failed to apply local variable property setting '%s'."), *Settings[Index].PropertyPath);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("settings[%d].property_path"), Index));
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
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	return true;
}

bool FBlueprintHelperLocalVariableMutationHandler::ApplyRemoveLocalVariables(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	const TArray<FBlueprintHelperLocalVariableRemoveRequest>& Requests,
	FBlueprintHelperLocalVariableMutationCounts& OutCounts,
	FString& OutError,
	FString* OutField)
{
	OutCounts = {};
	OutCounts.RequestedCount = Requests.Num();

	UEdGraph* FunctionGraph = nullptr;
	if (!ResolveFunctionGraph(Blueprint, FunctionName, FunctionGraph, OutError, OutField))
	{
		return false;
	}

	UK2Node_FunctionEntry* EntryNode = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionEntryNode(FunctionGraph);
	if (!EntryNode)
	{
		OutError = FString::Printf(TEXT("Function graph '%s' has no function entry node."), *FunctionName);
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("function_name"));
		return false;
	}

	if (Requests.Num() == 0)
	{
		OutError = TEXT("at least one local variable remove request is required.");
		FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, TEXT("variables"));
		return false;
	}

	TSet<FName> SeenNames;
	for (int32 Index = 0; Index < Requests.Num(); ++Index)
	{
		const FBlueprintHelperLocalVariableRemoveRequest& Request = Requests[Index];
		if (Request.VariableName.IsEmpty())
		{
			OutError = TEXT("Local variable remove request requires name or variable_name.");
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}

		const FName VariableFName(*Request.VariableName);
		if (SeenNames.Contains(VariableFName))
		{
			OutError = FString::Printf(TEXT("Duplicate local variable remove request: %s."), *Request.VariableName);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}
		SeenNames.Add(VariableFName);

		if (!FBlueprintLocalVariableMutationHandlerLocalUtils::FindLocalVariableOnEntry(EntryNode, VariableFName))
		{
			OutError = FString::Printf(TEXT("Local variable '%s' was not found."), *Request.VariableName);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}

		const int32 ReferenceCount = CountLocalVariableReferences(Blueprint, FunctionGraph, Request.VariableName);
		OutCounts.ReferenceCount += ReferenceCount;
		if (ReferenceCount > 0)
		{
			OutError = FString::Printf(
				TEXT("Local variable '%s' has %d function graph reference(s)."),
				*Request.VariableName,
				ReferenceCount);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}
	}

	for (int32 Index = 0; Index < Requests.Num(); ++Index)
	{
		const FBlueprintHelperLocalVariableRemoveRequest& Request = Requests[Index];
		if (Request.bDryRun)
		{
			++OutCounts.NoOpCount;
			continue;
		}

		const FName VariableFName(*Request.VariableName);
		Blueprint->Modify();
		EntryNode->Modify();

		bool bRemoved = false;
		for (int32 VariableIndex = 0; VariableIndex < EntryNode->LocalVariables.Num(); ++VariableIndex)
		{
			if (EntryNode->LocalVariables[VariableIndex].VarName == VariableFName)
			{
				EntryNode->LocalVariables.RemoveAt(VariableIndex);
				FBlueprintEditorUtils::RemoveVariableNodes(Blueprint, VariableFName, true, FunctionGraph);
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
				bRemoved = true;
				break;
			}
		}

		if (!bRemoved)
		{
			OutError = FString::Printf(TEXT("Failed to remove local variable '%s'."), *Request.VariableName);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}

		EntryNode = FBlueprintLocalVariableMutationHandlerLocalUtils::FindFunctionEntryNode(FunctionGraph);
		if (FBlueprintLocalVariableMutationHandlerLocalUtils::FindLocalVariableOnEntry(EntryNode, VariableFName))
		{
			OutError = FString::Printf(TEXT("Failed to remove local variable '%s'."), *Request.VariableName);
			FBlueprintLocalVariableMutationHandlerLocalUtils::SetOptionalField(OutField, FString::Printf(TEXT("variables[%d].name"), Index));
			return false;
		}

		++OutCounts.RemovedCount;
	}

	return true;
}

int32 FBlueprintHelperLocalVariableMutationHandler::CountLocalVariableReferences(
	UBlueprint* Blueprint,
	UEdGraph* FunctionGraph,
	const FString& VariableName)
{
	(void)Blueprint;
	if (!FunctionGraph || VariableName.IsEmpty())
	{
		return 0;
	}

	const FName VariableFName(*VariableName);
	const FString FunctionScopeName = FunctionGraph->GetName();
	int32 ReferenceCount = 0;

	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
		if (!VariableNode)
		{
			continue;
		}

		const FMemberReference& VariableReference = VariableNode->VariableReference;
		if (VariableReference.GetMemberName() != VariableFName)
		{
			continue;
		}

		if (VariableReference.IsLocalScope())
		{
			const FString MemberScopeName = VariableReference.GetMemberScopeName();
			if (MemberScopeName.IsEmpty() ||
				MemberScopeName.Equals(FunctionScopeName, ESearchCase::IgnoreCase) ||
				MemberScopeName.Equals(FunctionGraph->GetFName().ToString(), ESearchCase::IgnoreCase))
			{
				++ReferenceCount;
			}
			continue;
		}

		++ReferenceCount;
	}

	return ReferenceCount;
}
