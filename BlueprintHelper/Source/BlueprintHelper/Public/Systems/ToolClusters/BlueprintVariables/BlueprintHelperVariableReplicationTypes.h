#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "Shared/BlueprintVariables/BlueprintHelperBlueprintVariableTypes.h"

enum class EBlueprintHelperVariableReplicationMode : uint8
{
	None,
	Replicated,
	RepNotify
};

struct FBlueprintHelperVariableReplicationRequest
{
	EBlueprintHelperVariableReplicationMode Mode = EBlueprintHelperVariableReplicationMode::None;
	ELifetimeCondition Condition = COND_None;
	FName NotifyFunctionName = NAME_None;
	bool bCreateNotifyFunction = true;
	bool bReuseExistingNotifyFunction = false;
};

struct FBlueprintHelperVariableReplicationError
{
	FString Code;
	FString Message;
	FString Field;

	void Set(const FString& InCode, const FString& InMessage, const FString& InField)
	{
		Code = InCode;
		Message = InMessage;
		Field = InField;
	}

	bool HasError() const
	{
		return !Code.IsEmpty();
	}
};
