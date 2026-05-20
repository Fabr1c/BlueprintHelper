// BlueprintHelper runtime typed settings resolver implementation.

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

#include "Dom/JsonValue.h"
#include "Math/UnrealMathUtility.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"

namespace
{
static void ResetDiagnostics(FString* OutDiagnostics)
{
	if (OutDiagnostics)
	{
		OutDiagnostics->Reset();
	}
}

static void SetDiagnostics(FString* OutDiagnostics, const FString& Message)
{
	if (OutDiagnostics)
	{
		*OutDiagnostics = Message;
	}
}

static FString MissingDiagnostic(const FString& DotPath, const FString& Error)
{
	if (!Error.IsEmpty())
	{
		return FString::Printf(TEXT("setting_resolve_failed:%s:%s"), *DotPath, *Error);
	}
	return FString::Printf(TEXT("setting_resolve_missing:%s"), *DotPath);
}

static FString TypeMismatchDiagnostic(const FString& DotPath)
{
	return FString::Printf(TEXT("setting_type_mismatch:%s"), *DotPath);
}

static bool ResolveValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString* OutDiagnostics)
{
	FString Error;
	if (!FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(DotPath, OutValue, Error))
	{
		SetDiagnostics(OutDiagnostics, MissingDiagnostic(DotPath, Error));
		return false;
	}

	ResetDiagnostics(OutDiagnostics);
	return OutValue.IsValid();
}

static bool TryParseBoolText(const FString& Text, bool& OutValue)
{
	if (Text.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		OutValue = true;
		return true;
	}
	if (Text.Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		OutValue = false;
		return true;
	}
	return false;
}

static bool TryParseIntText(const FString& Text, int32& OutValue)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	return !TrimmedText.IsEmpty() && LexTryParseString(OutValue, *TrimmedText);
}

static bool TryParseDoubleText(const FString& Text, double& OutValue)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	return !TrimmedText.IsEmpty() && LexTryParseString(OutValue, *TrimmedText);
}

static bool TryJsonValueToDouble(const TSharedPtr<FJsonValue>& Value, double& OutValue)
{
	if (!Value.IsValid())
	{
		return false;
	}

	if (Value->Type == EJson::Number)
	{
		OutValue = Value->AsNumber();
		return true;
	}

	if (Value->Type == EJson::String)
	{
		return TryParseDoubleText(Value->AsString(), OutValue);
	}

	return false;
}

static bool TryJsonArrayToDoubles(const TSharedPtr<FJsonValue>& Value, int32 ExpectedCount, TArray<double>& OutValues)
{
	OutValues.Reset();
	if (!Value.IsValid() || Value->Type != EJson::Array)
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>& ArrayValues = Value->AsArray();
	if (ArrayValues.Num() != ExpectedCount)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ArrayValue : ArrayValues)
	{
		double ParsedValue = 0.0;
		if (!TryJsonValueToDouble(ArrayValue, ParsedValue))
		{
			OutValues.Reset();
			return false;
		}
		OutValues.Add(ParsedValue);
	}

	return true;
}

static bool TryCommaTextToDoubles(const FString& Text, int32 ExpectedCount, TArray<double>& OutValues)
{
	OutValues.Reset();

	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() != ExpectedCount)
	{
		return false;
	}

	for (const FString& Part : Parts)
	{
		double ParsedValue = 0.0;
		if (!TryParseDoubleText(Part, ParsedValue))
		{
			OutValues.Reset();
			return false;
		}
		OutValues.Add(ParsedValue);
	}

	return true;
}
}

bool FBlueprintHelperRuntimeSettingResolver::GetBool(const FString& DotPath, bool DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	if (Value->Type == EJson::Boolean)
	{
		return Value->AsBool();
	}

	if (Value->Type == EJson::String)
	{
		bool ParsedValue = false;
		if (TryParseBoolText(Value->AsString(), ParsedValue))
		{
			return ParsedValue;
		}
	}

	SetDiagnostics(OutDiagnostics, TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

int32 FBlueprintHelperRuntimeSettingResolver::GetInt(const FString& DotPath, int32 DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	if (Value->Type == EJson::Number)
	{
		return FMath::RoundToInt(Value->AsNumber());
	}

	if (Value->Type == EJson::String)
	{
		int32 ParsedValue = 0;
		if (TryParseIntText(Value->AsString(), ParsedValue))
		{
			return ParsedValue;
		}
	}

	SetDiagnostics(OutDiagnostics, TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

double FBlueprintHelperRuntimeSettingResolver::GetDouble(const FString& DotPath, double DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	if (Value->Type == EJson::Number)
	{
		return Value->AsNumber();
	}

	if (Value->Type == EJson::String)
	{
		double ParsedValue = 0.0;
		if (TryParseDoubleText(Value->AsString(), ParsedValue))
		{
			return ParsedValue;
		}
	}

	SetDiagnostics(OutDiagnostics, TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

FString FBlueprintHelperRuntimeSettingResolver::GetString(const FString& DotPath, const FString& DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		return FString::SanitizeFloat(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	default:
		break;
	}

	SetDiagnostics(OutDiagnostics, TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

FVector2D FBlueprintHelperRuntimeSettingResolver::GetVector2(const FString& DotPath, const FVector2D& DefaultValue, FString* OutDiagnostics)
{
	return GetVector2D(DotPath, DefaultValue, OutDiagnostics);
}

FVector2D FBlueprintHelperRuntimeSettingResolver::GetVector2D(const FString& DotPath, const FVector2D& DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	TArray<double> ParsedValues;
	if (TryJsonArrayToDoubles(Value, 2, ParsedValues))
	{
		return FVector2D(ParsedValues[0], ParsedValues[1]);
	}

	if (Value->Type == EJson::String && TryCommaTextToDoubles(Value->AsString(), 2, ParsedValues))
	{
		return FVector2D(ParsedValues[0], ParsedValues[1]);
	}

	SetDiagnostics(OutDiagnostics, TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

FMargin FBlueprintHelperRuntimeSettingResolver::GetMargin(const FString& DotPath, const FMargin& DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	TArray<double> ParsedValues;
	if (TryJsonArrayToDoubles(Value, 4, ParsedValues))
	{
		return FMargin(ParsedValues[0], ParsedValues[1], ParsedValues[2], ParsedValues[3]);
	}

	if (Value->Type == EJson::String && TryCommaTextToDoubles(Value->AsString(), 4, ParsedValues))
	{
		return FMargin(ParsedValues[0], ParsedValues[1], ParsedValues[2], ParsedValues[3]);
	}

	SetDiagnostics(OutDiagnostics, TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

TSharedPtr<FJsonValue> FBlueprintHelperRuntimeSettingResolver::GetJsonValue(const FString& DotPath, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return nullptr;
	}

	return Value;
}
