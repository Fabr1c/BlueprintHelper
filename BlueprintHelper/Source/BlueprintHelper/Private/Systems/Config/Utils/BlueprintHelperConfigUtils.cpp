// BlueprintHelper Config utility functions.
// Extracted from anonymous namespaces in Systems/Config/*.cpp.

#include "Systems/Config/Utils/BlueprintHelperConfigUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"

// ─── Diagnostics helpers (from BlueprintHelperRuntimeSettingResolver) ───

void UBlueprintHelperConfigUtils::ResetDiagnostics(FString* OutDiagnostics)
{
	if (OutDiagnostics)
	{
		OutDiagnostics->Reset();
	}
}

void UBlueprintHelperConfigUtils::SetDiagnostics(FString* OutDiagnostics, const FString& Message)
{
	if (OutDiagnostics)
	{
		*OutDiagnostics = Message;
	}
}

FString UBlueprintHelperConfigUtils::MissingDiagnostic(const FString& DotPath, const FString& Error)
{
	if (!Error.IsEmpty())
	{
		return FString::Printf(TEXT("setting_resolve_failed:%s:%s"), *DotPath, *Error);
	}
	return FString::Printf(TEXT("setting_resolve_missing:%s"), *DotPath);
}

FString UBlueprintHelperConfigUtils::TypeMismatchDiagnostic(const FString& DotPath)
{
	return FString::Printf(TEXT("setting_type_mismatch:%s"), *DotPath);
}

bool UBlueprintHelperConfigUtils::ResolveValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString* OutDiagnostics)
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

bool UBlueprintHelperConfigUtils::TryParseBoolText(const FString& Text, bool& OutValue)
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

bool UBlueprintHelperConfigUtils::TryParseIntText(const FString& Text, int32& OutValue)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	return !TrimmedText.IsEmpty() && LexTryParseString(OutValue, *TrimmedText);
}

bool UBlueprintHelperConfigUtils::TryParseDoubleText(const FString& Text, double& OutValue)
{
	const FString TrimmedText = Text.TrimStartAndEnd();
	return !TrimmedText.IsEmpty() && LexTryParseString(OutValue, *TrimmedText);
}

bool UBlueprintHelperConfigUtils::TryJsonValueToDouble(const TSharedPtr<FJsonValue>& Value, double& OutValue)
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

bool UBlueprintHelperConfigUtils::TryJsonArrayToDoubles(const TSharedPtr<FJsonValue>& Value, int32 ExpectedCount, TArray<double>& OutValues)
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

bool UBlueprintHelperConfigUtils::TryCommaTextToDoubles(const FString& Text, int32 ExpectedCount, TArray<double>& OutValues)
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

// ─── Safety profile helpers (from BlueprintHelperSafetyProfileResolver) ───

FString UBlueprintHelperConfigUtils::BuildActiveProfileSafetyPath(const FString& ActiveProfile)
{
	const FString SanitizedProfile = ActiveProfile.IsEmpty() ? FString(TEXT("default")) : ActiveProfile;
	return FString::Printf(TEXT("profiles.%s.safety_profile"), *SanitizedProfile);
}

EBlueprintHelperSafetyProfile UBlueprintHelperConfigUtils::ParseSafetyProfile(const FString& Profile)
{
	if (Profile.Equals(TEXT("readonly"), ESearchCase::IgnoreCase)
		|| Profile.Equals(TEXT("read_only"), ESearchCase::IgnoreCase)
		|| Profile.Equals(TEXT("read-only"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::ReadOnly;
	}
	if (Profile.Equals(TEXT("standard"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::Standard;
	}
	if (Profile.Equals(TEXT("autorepair"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::AutoRepair;
	}
	return EBlueprintHelperSafetyProfile::Conservative;
}

// ─── Setting path / JSON helpers (from BlueprintHelperSettingStore) ───

const TCHAR* UBlueprintHelperConfigUtils::GetSettingSchema()
{
	return TEXT("BlueprintHelper.Setting.v1");
}

const TCHAR* UBlueprintHelperConfigUtils::GetSettingVersion()
{
	return TEXT("0.5.7");
}

bool UBlueprintHelperConfigUtils::SplitDotPath(const FString& DotPath, TArray<FString>& OutParts, FString& OutError)
{
	DotPath.ParseIntoArray(OutParts, TEXT("."), true);
	if (OutParts.Num() == 0)
	{
		OutError = TEXT("setting_path_empty");
		return false;
	}

	for (const FString& Part : OutParts)
	{
		if (Part.IsEmpty())
		{
			OutError = FString::Printf(TEXT("setting_path_invalid:%s"), *DotPath);
			return false;
		}
	}
	return true;
}

bool UBlueprintHelperConfigUtils::ParseJsonPathSegment(const FString& DotPath, const FString& Part, FBlueprintHelperSettingPathSegment& OutSegment, FString& OutError)
{
	int32 BracketIndex = INDEX_NONE;
	if (!Part.FindChar(TEXT('['), BracketIndex))
	{
		if (Part.IsEmpty())
		{
			OutError = FString::Printf(TEXT("setting_path_invalid:%s"), *DotPath);
			return false;
		}

		OutSegment.FieldName = Part;
		return true;
	}

	OutSegment.FieldName = Part.Left(BracketIndex);
	if (OutSegment.FieldName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("setting_path_invalid:%s"), *DotPath);
		return false;
	}

	int32 Cursor = BracketIndex;
	while (Cursor < Part.Len())
	{
		if (Part[Cursor] != TEXT('['))
		{
			OutError = FString::Printf(TEXT("setting_path_invalid:%s"), *DotPath);
			return false;
		}

		const int32 CloseIndex = Part.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 1);
		if (CloseIndex == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("setting_path_invalid:%s"), *DotPath);
			return false;
		}

		const FString IndexText = Part.Mid(Cursor + 1, CloseIndex - Cursor - 1);
		int32 ParsedIndex = INDEX_NONE;
		if (IndexText.IsEmpty() || !LexTryParseString(ParsedIndex, *IndexText) || ParsedIndex < 0)
		{
			OutError = FString::Printf(TEXT("setting_path_invalid_array_index:%s"), *DotPath);
			return false;
		}

		OutSegment.ArrayIndices.Add(ParsedIndex);
		Cursor = CloseIndex + 1;
	}

	return true;
}

bool UBlueprintHelperConfigUtils::ParseJsonPath(const FString& DotPath, TArray<FBlueprintHelperSettingPathSegment>& OutSegments, FString& OutError)
{
	TArray<FString> Parts;
	if (!SplitDotPath(DotPath, Parts, OutError))
	{
		return false;
	}

	for (const FString& Part : Parts)
	{
		FBlueprintHelperSettingPathSegment Segment;
		if (!ParseJsonPathSegment(DotPath, Part, Segment, OutError))
		{
			return false;
		}
		OutSegments.Add(MoveTemp(Segment));
	}

	return true;
}

bool UBlueprintHelperConfigUtils::ParseJsonObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		OutError = TEXT("setting_json_parse_failed");
		return false;
	}
	return true;
}

bool UBlueprintHelperConfigUtils::TryGetObjectFieldSafe(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, TSharedPtr<FJsonObject>& OutChild)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ExistingChild = nullptr;
	if (Object->TryGetObjectField(FieldName, ExistingChild) && ExistingChild && ExistingChild->IsValid())
	{
		OutChild = *ExistingChild;
		return true;
	}
	return false;
}

bool UBlueprintHelperConfigUtils::ApplyJsonPathArrayIndices(const FString& DotPath, const FBlueprintHelperSettingPathSegment& Segment, TSharedPtr<FJsonValue>& InOutValue, FString& OutError)
{
	for (const int32 ArrayIndex : Segment.ArrayIndices)
	{
		if (!InOutValue.IsValid() || InOutValue->Type != EJson::Array)
		{
			OutError = FString::Printf(TEXT("setting_path_array_expected:%s"), *DotPath);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>& ArrayValues = InOutValue->AsArray();
		if (!ArrayValues.IsValidIndex(ArrayIndex))
		{
			OutError = FString::Printf(TEXT("setting_path_array_index_out_of_range:%s"), *DotPath);
			return false;
		}

		InOutValue = ArrayValues[ArrayIndex];
	}

	return true;
}

bool UBlueprintHelperConfigUtils::TryGetValueAtPath(const TSharedPtr<FJsonObject>& RootObject, const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
	TArray<FBlueprintHelperSettingPathSegment> Segments;
	if (!ParseJsonPath(DotPath, Segments, OutError))
	{
		return false;
	}

	if (!RootObject.IsValid())
	{
		return false;
	}

	TSharedPtr<FJsonValue> CursorValue;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FBlueprintHelperSettingPathSegment& Segment = Segments[Index];
		TSharedPtr<FJsonObject> CursorObject;
		if (Index == 0)
		{
			CursorObject = RootObject;
		}
		else if (CursorValue.IsValid() && CursorValue->Type == EJson::Object)
		{
			CursorObject = CursorValue->AsObject();
		}

		if (!CursorObject.IsValid())
		{
			return false;
		}

		CursorValue = CursorObject->TryGetField(Segment.FieldName);
		if (!CursorValue.IsValid())
		{
			return false;
		}

		if (!ApplyJsonPathArrayIndices(DotPath, Segment, CursorValue, OutError))
		{
			return false;
		}
	}

	OutValue = CursorValue;
	if (!OutValue.IsValid())
	{
		return false;
	}
	return true;
}

void UBlueprintHelperConfigUtils::MergeJsonObjectInto(TSharedPtr<FJsonObject> Target, const TSharedPtr<FJsonObject>& Source)
{
	if (!Target.IsValid() || !Source.IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Source->Values)
	{
		const TSharedPtr<FJsonValue>* ExistingValue = Target->Values.Find(Pair.Key);
		if (ExistingValue
			&& ExistingValue->IsValid()
			&& Pair.Value.IsValid()
			&& (*ExistingValue)->Type == EJson::Object
			&& Pair.Value->Type == EJson::Object)
		{
			MergeJsonObjectInto((*ExistingValue)->AsObject(), Pair.Value->AsObject());
			continue;
		}

		Target->SetField(Pair.Key, Pair.Value);
	}
}

bool UBlueprintHelperConfigUtils::MergeJsonFileIfExists(const FString& Path, TSharedPtr<FJsonObject> Target, FString& OutError)
{
	FString JsonText;
	if (!FPaths::FileExists(Path))
	{
		return true;
	}

	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		return true;
	}

	TSharedPtr<FJsonObject> SourceObject;
	if (!ParseJsonObject(JsonText, SourceObject, OutError))
	{
		OutError = FString::Printf(TEXT("setting_json_parse_failed:%s"), *Path);
		return false;
	}

	MergeJsonObjectInto(Target, SourceObject);
	return true;
}

FString UBlueprintHelperConfigUtils::JsonValueToSettingString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		return FString::SanitizeFloat(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	case EJson::Array:
	{
		TArray<FString> Parts;
		for (const TSharedPtr<FJsonValue>& ArrayValue : Value->AsArray())
		{
			Parts.Add(JsonValueToSettingString(ArrayValue));
		}
		return FString::Join(Parts, TEXT(", "));
	}
	default:
		break;
	}

	FString SerializedValue;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedValue);
	FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
	return SerializedValue;
}

TSharedPtr<FJsonValue> UBlueprintHelperConfigUtils::TryParseJsonLiteralValue(const FString& NewValue)
{
	const FString TrimmedValue = NewValue.TrimStartAndEnd();
	if (!TrimmedValue.StartsWith(TEXT("[")) && !TrimmedValue.StartsWith(TEXT("{")))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> WrappedObject;
	FString Error;
	const FString WrappedJson = FString::Printf(TEXT("{\"value\":%s}"), *TrimmedValue);
	if (!ParseJsonObject(WrappedJson, WrappedObject, Error))
	{
		return nullptr;
	}

	const TSharedPtr<FJsonValue> ParsedValue = WrappedObject->TryGetField(TEXT("value"));
	if (ParsedValue.IsValid())
	{
		return ParsedValue;
	}
	return nullptr;
}

TSharedPtr<FJsonValue> UBlueprintHelperConfigUtils::ConvertSettingStringToJsonValue(const FString& NewValue)
{
	if (TSharedPtr<FJsonValue> ParsedLiteral = TryParseJsonLiteralValue(NewValue))
	{
		return ParsedLiteral;
	}

	if (NewValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		return MakeShared<FJsonValueBoolean>(true);
	}
	if (NewValue.Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		return MakeShared<FJsonValueBoolean>(false);
	}

	if (NewValue.IsNumeric())
	{
		double NumberValue = 0.0;
		LexTryParseString(NumberValue, *NewValue);
		return MakeShared<FJsonValueNumber>(NumberValue);
	}

	return MakeShared<FJsonValueString>(NewValue);
}
