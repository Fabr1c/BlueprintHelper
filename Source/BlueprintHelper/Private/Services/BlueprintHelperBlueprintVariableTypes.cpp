// BlueprintHelper Service Layer — BlueprintVariableTypes ToJson 实现

#include "Services/BlueprintHelperBlueprintVariableTypes.h"

TSharedRef<FJsonObject> FBlueprintHelperVariableType::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("category"), Category);
	if (Subtype.IsSet()) J->SetStringField(TEXT("subtype"), *Subtype);
	J->SetStringField(TEXT("container"), Container);
	if (KeyType.IsSet() && (*KeyType).IsValid()) J->SetObjectField(TEXT("key_type"), (*KeyType)->ToJson());
	if (ValueType.IsSet() && (*ValueType).IsValid()) J->SetObjectField(TEXT("value_type"), (*ValueType)->ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperMemberVariableItem::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("variable_name"), VariableName);
	J->SetObjectField(TEXT("variable_type"), VariableType.ToJson());
	if (Category.IsSet()) J->SetStringField(TEXT("category"), *Category);
	if (Tooltip.IsSet()) J->SetStringField(TEXT("tooltip"), *Tooltip);
	J->SetBoolField(TEXT("instance_editable"), bInstanceEditable);
	J->SetBoolField(TEXT("expose_on_spawn"), bExposeOnSpawn);
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperReadMemberVariablesResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	TArray<TSharedPtr<FJsonValue>> A; for (const auto& V : MemberVariables) A.Add(MakeShared<FJsonValueObject>(V.ToJson()));
	J->SetArrayField(TEXT("member_variables"), A);
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperReadMemberDefaultsResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	if (Values.IsValid()) J->SetObjectField(TEXT("values"), Values);
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperAddMemberVariableResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("add_result"), AddResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperAddMemberVariablesResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("add_result"), AddResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperSetMemberDefaultsResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetNumberField(TEXT("applied_count"), AppliedCount);
	J->SetNumberField(TEXT("changed_count"), ChangedCount);
	J->SetNumberField(TEXT("no_op_count"), NoOpCount);
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperSetMemberDefaultsBatchResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("defaults_result"), DefaultsResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperRemoveMemberVariableResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("remove_result"), RemoveResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperRemoveMemberVariablesResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("remove_result"), RemoveResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperReadLocalVariablesResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetStringField(TEXT("function_name"), FunctionName);
	TArray<TSharedPtr<FJsonValue>> A; for (const auto& V : LocalVariables) A.Add(MakeShared<FJsonValueObject>(V.ToJson()));
	J->SetArrayField(TEXT("local_variables"), A);
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperAddLocalVariableResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("add_result"), AddResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperAddLocalVariablesResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("add_result"), AddResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperSetLocalVariablePropertiesResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("properties_result"), PropertiesResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperRemoveLocalVariableResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("remove_result"), RemoveResult.ToJson());
	return J;
}

TSharedRef<FJsonObject> FBlueprintHelperRemoveLocalVariablesResultData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), Schema);
	J->SetObjectField(TEXT("remove_result"), RemoveResult.ToJson());
	return J;
}
