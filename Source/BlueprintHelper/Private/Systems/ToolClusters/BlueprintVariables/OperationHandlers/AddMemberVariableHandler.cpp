#include "Systems/ToolClusters/BlueprintVariables/OperationHandlers/AddMemberVariableHandler.h"

#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"

bool FAddMemberVariableHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("add_member_variable"), ESearchCase::IgnoreCase);
}

bool FAddMemberVariableHandler::Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("add_member_variable 失败：蓝图无效。");
		return false;
	}

	FString VarName;
	if (!OpPayload->TryGetStringField(TEXT("name"), VarName) || VarName.IsEmpty())
	{
		OutError = TEXT("add_member_variable 失败：缺。name 字段。");
		return false;
	}

	// 检查变量是否已存在
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == FName(*VarName))
		{
			// 变量已存在，视为成功（幂等）
			return true;
		}
	}

	// 解析 pin_type
	const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
	FEdGraphPinType PinType;
	if (OpPayload->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject && PinTypeObject->IsValid())
	{
		FParsedPinType ParsedPinType;
		(*PinTypeObject)->TryGetStringField(TEXT("category"), ParsedPinType.Category);
		(*PinTypeObject)->TryGetStringField(TEXT("sub_category"), ParsedPinType.SubCategory);
		(*PinTypeObject)->TryGetStringField(TEXT("object_path"), ParsedPinType.SubCategoryObjectPath);
		(*PinTypeObject)->TryGetStringField(TEXT("container_type"), ParsedPinType.ContainerType);

		FString ConvertError;
		if (!TextToBlueprintGenerator::ConvertToEdGraphPinType(ParsedPinType, PinType, ConvertError))
		{
			OutError = FString::Printf(TEXT("add_member_variable '%s' 失败：类型转换错误%s"), *VarName, *ConvertError);
			return false;
		}
	}
	else
	{
		// 默认 bool 类型
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}

	// 创建变量
	const bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
	if (!bAdded)
	{
		OutError = FString::Printf(TEXT("add_member_variable '%s' 失败：FBlueprintEditorUtils::AddMemberVariable 返回 false。"), *VarName);
		return false;
	}

	// 设置默认值
	FString DefaultValue;
	if (OpPayload->TryGetStringField(TEXT("default_value"), DefaultValue) && !DefaultValue.IsEmpty())
	{
		for (FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarName == FName(*VarName))
			{
				Var.DefaultValue = DefaultValue;
				break;
			}
		}
	}

	// 设置分类
	FString Category;
	if (OpPayload->TryGetStringField(TEXT("category"), Category) && !Category.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
	}

	// 设置标记
	const TSharedPtr<FJsonObject>* FlagsObject = nullptr;
	if (OpPayload->TryGetObjectField(TEXT("flags"), FlagsObject) && FlagsObject && FlagsObject->IsValid())
	{
		bool bValue = false;
		if ((*FlagsObject)->TryGetBoolField(TEXT("blueprint_read_only"), bValue) && bValue)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*VarName), nullptr, FBlueprintMetadata::MD_Private, TEXT("true"));
		}

		if ((*FlagsObject)->TryGetBoolField(TEXT("expose_on_spawn"), bValue) && bValue)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*VarName), nullptr, FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
		}
	}

	return true;
}
