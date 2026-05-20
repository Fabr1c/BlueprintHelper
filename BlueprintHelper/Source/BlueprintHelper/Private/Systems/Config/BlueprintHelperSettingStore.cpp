// BlueprintHelper settings store implementation.

#include "Systems/Config/BlueprintHelperSettingStore.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"

namespace
{
constexpr const TCHAR* BlueprintHelperSettingSchema = TEXT("BlueprintHelper.Setting.v1");
constexpr const TCHAR* BlueprintHelperSettingVersion = TEXT("0.5.0");

struct FBlueprintHelperSettingPathSegment
{
	FString FieldName;
	TArray<int32> ArrayIndices;
};

static bool SplitDotPath(const FString& DotPath, TArray<FString>& OutParts, FString& OutError)
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

static bool ParseJsonPathSegment(const FString& DotPath, const FString& Part, FBlueprintHelperSettingPathSegment& OutSegment, FString& OutError)
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

static bool ParseJsonPath(const FString& DotPath, TArray<FBlueprintHelperSettingPathSegment>& OutSegments, FString& OutError)
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

static bool ParseJsonObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		OutError = TEXT("setting_json_parse_failed");
		return false;
	}
	return true;
}

static bool TryGetObjectFieldSafe(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, TSharedPtr<FJsonObject>& OutChild)
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

static bool ApplyJsonPathArrayIndices(const FString& DotPath, const FBlueprintHelperSettingPathSegment& Segment, TSharedPtr<FJsonValue>& InOutValue, FString& OutError)
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

static bool TryGetValueAtPath(const TSharedPtr<FJsonObject>& RootObject, const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
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

static void MergeJsonObjectInto(TSharedPtr<FJsonObject> Target, const TSharedPtr<FJsonObject>& Source)
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

static bool MergeJsonFileIfExists(const FString& Path, TSharedPtr<FJsonObject> Target, FString& OutError)
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

static FString JsonValueToSettingString(const TSharedPtr<FJsonValue>& Value)
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

static TSharedPtr<FJsonValue> TryParseJsonLiteralValue(const FString& NewValue)
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

static TSharedPtr<FJsonValue> ConvertSettingStringToJsonValue(const FString& NewValue)
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
}

FBlueprintHelperSettingView FBlueprintHelperSettingStore::Load()
{
	FBlueprintHelperSettingView View;
	View.Schema = BlueprintHelperSettingSchema;
	View.Version = BlueprintHelperSettingVersion;
	View.DefaultSettingPath = GetDefaultSettingPath();
	View.ProjectSettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	View.UserSettingOverridePath = FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath();
	View.bProjectSettingExists = FPaths::FileExists(View.ProjectSettingPath);
	View.bUserOverrideExists = FPaths::FileExists(View.UserSettingOverridePath);

	FString EffectiveJson;
	if (LoadEffectiveSettingJson(EffectiveJson, View.ErrorText))
	{
		View.EffectiveSourcePath = TEXT("built-in + default + project + user");
		View.EffectiveJson = PrettyPrintJsonOrOriginal(EffectiveJson);
		View.bLoaded = !View.EffectiveJson.IsEmpty();
		View.StatusText = FString::Printf(TEXT("Loaded %s"), *View.EffectiveSourcePath);
	}
	else
	{
		View.EffectiveSourcePath = TEXT("built-in + default + project + user");
		View.StatusText = TEXT("Settings failed to load");
	}

	return View;
}

bool FBlueprintHelperSettingStore::EnsureProjectSetting(FString& OutPath, FString& OutError)
{
	OutPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	OutError.Reset();
	if (FPaths::FileExists(OutPath))
	{
		return true;
	}

	FString JsonText;
	if (!LoadFileIfExists(GetDefaultSettingPath(), JsonText))
	{
		JsonText = GetBuiltInDefaultSettingJson();
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
	if (!FFileHelper::SaveStringToFile(JsonText, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Failed to write %s"), *OutPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperSettingStore::LoadEffectiveSettingObject(TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
	OutObject.Reset();
	OutError.Reset();

	TSharedPtr<FJsonObject> EffectiveObject;
	if (!ParseJsonObject(GetBuiltInDefaultSettingJson(), EffectiveObject, OutError))
	{
		return false;
	}

	if (!MergeJsonFileIfExists(GetDefaultSettingPath(), EffectiveObject, OutError))
	{
		return false;
	}

	if (!MergeJsonFileIfExists(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath(), EffectiveObject, OutError))
	{
		return false;
	}

	if (!MergeJsonFileIfExists(FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath(), EffectiveObject, OutError))
	{
		return false;
	}

	OutObject = EffectiveObject;
	return OutObject.IsValid();
}

bool FBlueprintHelperSettingStore::LoadEffectiveSettingJson(FString& OutJson, FString& OutError)
{
	OutJson.Reset();

	TSharedPtr<FJsonObject> EffectiveObject;
	if (!LoadEffectiveSettingObject(EffectiveObject, OutError))
	{
		return false;
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(EffectiveObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
		return false;
	}

	return true;
}

bool FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
	OutValue.Reset();

	TSharedPtr<FJsonObject> EffectiveObject;
	if (!LoadEffectiveSettingObject(EffectiveObject, OutError))
	{
		return false;
	}

	return TryGetValueAtPath(EffectiveObject, DotPath, OutValue, OutError);
}

bool FBlueprintHelperSettingStore::TryGetProjectJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
	OutValue.Reset();
	OutError.Reset();

	FString ProjectJson;
	if (!LoadFileIfExists(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath(), ProjectJson))
	{
		return false;
	}

	TSharedPtr<FJsonObject> ProjectObject;
	if (!ParseJsonObject(ProjectJson, ProjectObject, OutError))
	{
		return false;
	}

	return TryGetValueAtPath(ProjectObject, DotPath, OutValue, OutError);
}

bool FBlueprintHelperSettingStore::UpdateProjectSettingValue(const FString& DotPath, const FString& NewValue, FString& OutError)
{
	const FString SettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	FString InputJson;
	if (!LoadFileIfExists(SettingPath, InputJson))
	{
		InputJson = TEXT("{}\n");
	}

	FString OutputJson;
	if (!UpdateSettingJsonText(InputJson, DotPath, NewValue, OutputJson, OutError))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SettingPath), true);
	if (!FFileHelper::SaveStringToFile(OutputJson, *SettingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("setting_write_failed:%s"), *SettingPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperSettingStore::ResetProjectSettingValue(const FString& DotPath, FString& OutError)
{
	const FString SettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	FString InputJson;
	if (!LoadFileIfExists(SettingPath, InputJson))
	{
		InputJson = TEXT("{}\n");
	}

	FString OutputJson;
	if (!RemoveSettingJsonPath(InputJson, DotPath, OutputJson, OutError))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SettingPath), true);
	if (!FFileHelper::SaveStringToFile(OutputJson, *SettingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("setting_write_failed:%s"), *SettingPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperSettingStore::GetSettingValue(const FString& DotPath, FString& OutCurrentValue, FString& OutDefaultValue, bool& bOutHasProjectOverride, FString& OutError)
{
	OutCurrentValue.Reset();
	OutDefaultValue.Reset();
	bOutHasProjectOverride = false;
	OutError.Reset();

	TSharedPtr<FJsonObject> DefaultObject;
	if (!ParseJsonObject(GetBuiltInDefaultSettingJson(), DefaultObject, OutError))
	{
		return false;
	}
	if (!MergeJsonFileIfExists(GetDefaultSettingPath(), DefaultObject, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonValue> DefaultValue;
	if (TryGetValueAtPath(DefaultObject, DotPath, DefaultValue, OutError))
	{
		OutDefaultValue = JsonValueToSettingString(DefaultValue);
		OutCurrentValue = OutDefaultValue;
	}

	const FString ProjectSettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	FString ProjectJson;
	if (LoadFileIfExists(ProjectSettingPath, ProjectJson))
	{
		TSharedPtr<FJsonObject> ProjectObject;
		FString ProjectError;
		if (ParseJsonObject(ProjectJson, ProjectObject, ProjectError))
		{
			TSharedPtr<FJsonValue> ProjectValue;
			if (TryGetValueAtPath(ProjectObject, DotPath, ProjectValue, ProjectError))
			{
				OutCurrentValue = JsonValueToSettingString(ProjectValue);
				bOutHasProjectOverride = true;
			}
		}
	}

	const FString UserOverridePath = FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath();
	FString UserJson;
	if (LoadFileIfExists(UserOverridePath, UserJson))
	{
		TSharedPtr<FJsonObject> UserObject;
		FString UserError;
		if (ParseJsonObject(UserJson, UserObject, UserError))
		{
			TSharedPtr<FJsonValue> UserValue;
			if (TryGetValueAtPath(UserObject, DotPath, UserValue, UserError))
			{
				OutCurrentValue = JsonValueToSettingString(UserValue);
			}
		}
	}

	return !OutCurrentValue.IsEmpty() || !OutDefaultValue.IsEmpty();
}

bool FBlueprintHelperSettingStore::UpdateSettingJsonText(const FString& InputJson, const FString& DotPath, const FString& NewValue, FString& OutJson, FString& OutError)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!ParseJsonObject(InputJson, RootObject, OutError))
	{
		return false;
	}

	TArray<FString> Parts;
	if (!SplitDotPath(DotPath, Parts, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Cursor = RootObject;
	for (int32 Index = 0; Index < Parts.Num() - 1; ++Index)
	{
		const FString& Part = Parts[Index];
		TSharedPtr<FJsonObject> Child;
		if (!TryGetObjectFieldSafe(Cursor, Part, Child))
		{
			Child = MakeShared<FJsonObject>();
			Cursor->SetObjectField(Part, Child);
		}
		Cursor = Child;
	}

	Cursor->SetField(Parts.Last(), ConvertSettingStringToJsonValue(NewValue));
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
		return false;
	}
	return true;
}

bool FBlueprintHelperSettingStore::RemoveSettingJsonPath(const FString& InputJson, const FString& DotPath, FString& OutJson, FString& OutError)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!ParseJsonObject(InputJson, RootObject, OutError))
	{
		return false;
	}

	TArray<FString> Parts;
	if (!SplitDotPath(DotPath, Parts, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Cursor = RootObject;
	for (int32 Index = 0; Index < Parts.Num() - 1; ++Index)
	{
		TSharedPtr<FJsonObject> Child;
		if (!TryGetObjectFieldSafe(Cursor, Parts[Index], Child))
		{
			OutJson = InputJson;
			return true;
		}
		Cursor = Child;
	}

	Cursor->RemoveField(Parts.Last());
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
		return false;
	}
	return true;
}

FString FBlueprintHelperSettingStore::GetDefaultSettingPath()
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper")))
	{
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("DefaultSetting.json"));
	}
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Config"),
		TEXT("DefaultSetting.json"));
}

FString FBlueprintHelperSettingStore::GetBuiltInDefaultSettingJson()
{
	return TEXT("{\n")
		TEXT("  \"schema\": \"BlueprintHelper.Setting.v1\",\n")
		TEXT("  \"version\": \"0.5.0\",\n")
		TEXT("  \"active_profile\": \"default\",\n")
		TEXT("  \"ui\": { \"main_window\": { \"default_tab\": \"tools\" } },\n")
		TEXT("  \"runtime\": { \"bridge\": { \"port\": 54321 } },\n")
		TEXT("  \"tool_clusters\": {},\n")
		TEXT("  \"debug\": {}\n")
		TEXT("}\n");
}

bool FBlueprintHelperSettingStore::LoadFileIfExists(const FString& Path, FString& OutText)
{
	OutText.Reset();
	return FPaths::FileExists(Path) && FFileHelper::LoadFileToString(OutText, *Path);
}

FString FBlueprintHelperSettingStore::PrettyPrintJsonOrOriginal(const FString& JsonText)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return JsonText;
	}

	FString PrettyText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PrettyText);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return PrettyText;
}
