// BlueprintHelper Utils -- Blueprint Class Settings 工具函数库实现

#include "Systems/ToolClusters/BlueprintClassSettings/Utils/BlueprintHelperClassSettingsUtils.h"

#include "Shared/BlueprintHelperServiceTypes.h"

FString UBlueprintHelperClassSettingsUtils::BlueprintClassSettingsDescribeInvalidInterface(const FBlueprintHelperInvalidInterface& Invalid)
{
	FString Message = FString::Printf(
		TEXT("Invalid interface '%s': %s"),
		*Invalid.InterfacePath,
		Invalid.Code.IsEmpty() ? TEXT("unknown_error") : *Invalid.Code);
	if (!Invalid.Message.IsEmpty())
	{
		Message += FString::Printf(TEXT(" (%s)"), *Invalid.Message);
	}
	return Message;
}

FString UBlueprintHelperClassSettingsUtils::BlueprintClassSettingsDescribeInvalidDefaultSetting(const FBlueprintHelperInvalidClassDefaultSetting& Invalid)
{
	FString Message = FString::Printf(
		TEXT("Invalid class default property '%s': %s"),
		*Invalid.PropertyPath,
		Invalid.Code.IsEmpty() ? TEXT("unknown_error") : *Invalid.Code);
	if (!Invalid.ExpectedType.IsEmpty())
	{
		Message += FString::Printf(TEXT(", expected_type=%s"), *Invalid.ExpectedType);
	}
	if (!Invalid.ActualType.IsEmpty())
	{
		Message += FString::Printf(TEXT(", actual_type=%s"), *Invalid.ActualType);
	}
	if (!Invalid.ValueSummary.IsEmpty())
	{
		Message += FString::Printf(TEXT(", detail=%s"), *Invalid.ValueSummary);
	}
	return Message;
}
