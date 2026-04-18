#include "OperationHandlers/RemoveMemberVariableHandler.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"

bool FRemoveMemberVariableHandler::CanHandle(const FString& OpName) const
{
	return OpName.Equals(TEXT("remove_member_variable"), ESearchCase::IgnoreCase);
}

bool FRemoveMemberVariableHandler::Execute(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& OpPayload, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("remove_member_variable 失败：蓝图无效。");
		return false;
	}

	FString VarName;
	if (!OpPayload->TryGetStringField(TEXT("name"), VarName) || VarName.IsEmpty())
	{
		OutError = TEXT("remove_member_variable 失败：缺少 name 字段。");
		return false;
	}

	// 检查变量是否存在
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VarName));
	if (VarIndex == INDEX_NONE)
	{
		// 变量不存在，视为幂等成功
		return true;
	}

	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VarName));
	return true;
}
