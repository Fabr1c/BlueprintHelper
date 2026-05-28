// BlueprintHelper runtime typed settings resolver implementation.

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

#include "Dom/JsonValue.h"
#include "Math/UnrealMathUtility.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"
#include "Systems/Config/Utils/BlueprintHelperConfigUtils.h"

bool FBlueprintHelperRuntimeSettingResolver::GetBool(const FString& DotPath, bool DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!UBlueprintHelperConfigUtils::ResolveValue(DotPath, Value, OutDiagnostics))
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
		if (UBlueprintHelperConfigUtils::TryParseBoolText(Value->AsString(), ParsedValue))
		{
			return ParsedValue;
		}
	}

	UBlueprintHelperConfigUtils::SetDiagnostics(OutDiagnostics, UBlueprintHelperConfigUtils::TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

int32 FBlueprintHelperRuntimeSettingResolver::GetInt(const FString& DotPath, int32 DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!UBlueprintHelperConfigUtils::ResolveValue(DotPath, Value, OutDiagnostics))
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
		if (UBlueprintHelperConfigUtils::TryParseIntText(Value->AsString(), ParsedValue))
		{
			return ParsedValue;
		}
	}

	UBlueprintHelperConfigUtils::SetDiagnostics(OutDiagnostics, UBlueprintHelperConfigUtils::TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

double FBlueprintHelperRuntimeSettingResolver::GetDouble(const FString& DotPath, double DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!UBlueprintHelperConfigUtils::ResolveValue(DotPath, Value, OutDiagnostics))
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
		if (UBlueprintHelperConfigUtils::TryParseDoubleText(Value->AsString(), ParsedValue))
		{
			return ParsedValue;
		}
	}

	UBlueprintHelperConfigUtils::SetDiagnostics(OutDiagnostics, UBlueprintHelperConfigUtils::TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

FString FBlueprintHelperRuntimeSettingResolver::GetString(const FString& DotPath, const FString& DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!UBlueprintHelperConfigUtils::ResolveValue(DotPath, Value, OutDiagnostics))
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

	UBlueprintHelperConfigUtils::SetDiagnostics(OutDiagnostics, UBlueprintHelperConfigUtils::TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

FVector2D FBlueprintHelperRuntimeSettingResolver::GetVector2(const FString& DotPath, const FVector2D& DefaultValue, FString* OutDiagnostics)
{
	return GetVector2D(DotPath, DefaultValue, OutDiagnostics);
}

FVector2D FBlueprintHelperRuntimeSettingResolver::GetVector2D(const FString& DotPath, const FVector2D& DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!UBlueprintHelperConfigUtils::ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	TArray<double> ParsedValues;
	if (UBlueprintHelperConfigUtils::TryJsonArrayToDoubles(Value, 2, ParsedValues))
	{
		return FVector2D(ParsedValues[0], ParsedValues[1]);
	}

	if (Value->Type == EJson::String && UBlueprintHelperConfigUtils::TryCommaTextToDoubles(Value->AsString(), 2, ParsedValues))
	{
		return FVector2D(ParsedValues[0], ParsedValues[1]);
	}

	UBlueprintHelperConfigUtils::SetDiagnostics(OutDiagnostics, UBlueprintHelperConfigUtils::TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

FMargin FBlueprintHelperRuntimeSettingResolver::GetMargin(const FString& DotPath, const FMargin& DefaultValue, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!UBlueprintHelperConfigUtils::ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return DefaultValue;
	}

	TArray<double> ParsedValues;
	if (UBlueprintHelperConfigUtils::TryJsonArrayToDoubles(Value, 4, ParsedValues))
	{
		return FMargin(ParsedValues[0], ParsedValues[1], ParsedValues[2], ParsedValues[3]);
	}

	if (Value->Type == EJson::String && UBlueprintHelperConfigUtils::TryCommaTextToDoubles(Value->AsString(), 4, ParsedValues))
	{
		return FMargin(ParsedValues[0], ParsedValues[1], ParsedValues[2], ParsedValues[3]);
	}

	UBlueprintHelperConfigUtils::SetDiagnostics(OutDiagnostics, UBlueprintHelperConfigUtils::TypeMismatchDiagnostic(DotPath));
	return DefaultValue;
}

TSharedPtr<FJsonValue> FBlueprintHelperRuntimeSettingResolver::GetJsonValue(const FString& DotPath, FString* OutDiagnostics)
{
	TSharedPtr<FJsonValue> Value;
	if (!UBlueprintHelperConfigUtils::ResolveValue(DotPath, Value, OutDiagnostics))
	{
		return nullptr;
	}

	return Value;
}
