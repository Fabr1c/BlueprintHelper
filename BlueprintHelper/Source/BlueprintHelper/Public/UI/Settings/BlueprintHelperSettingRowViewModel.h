#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperSettingChoiceViewModel
{
	FString Value;
	FText Label;
};

enum class EBlueprintHelperSettingValueType : uint8
{
	Number,
	Integer,
	Boolean,
	String,
	Choice,
	Vector2,
	Margin,
	ColorArray
};

struct FBlueprintHelperSettingRowViewModel
{
	FString DotPath;
	FText CategoryLabel;
	FText DisplayLabel;
	FText OverlapHint;
	EBlueprintHelperSettingValueType ValueType = EBlueprintHelperSettingValueType::String;
	FString CurrentValue;
	FString DefaultValue;
	FString ErrorText;
	FString AccessStatusText;
	FString ConsumerStatusText;
	TArray<FBlueprintHelperSettingChoiceViewModel> Choices;
	double MinValue = 0.0;
	double MaxValue = 0.0;
	bool bHasMinValue = false;
	bool bHasMaxValue = false;
	bool bModified = false;
	bool bEnabled = true;
	bool bDeveloperOnly = false;
	bool bRuntimeConsumed = false;
};

struct FBlueprintHelperSettingEditEvent
{
	FString DotPath;
	FString NewValue;
};
