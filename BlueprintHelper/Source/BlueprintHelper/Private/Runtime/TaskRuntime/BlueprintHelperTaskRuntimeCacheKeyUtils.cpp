// BlueprintHelper TaskRuntime cache key utilities.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheKeyUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/BlueprintHelperVersionCompat.h"

class FBlueprintHelperTaskRuntimeCacheKeyLocalUtils
{
public:
	static FString EscapeStableString(const FString& Value)
	{
		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("|"), TEXT("\\p"));
		Escaped.ReplaceInline(TEXT("="), TEXT("\\e"));
		Escaped.ReplaceInline(TEXT("{"), TEXT("\\l"));
		Escaped.ReplaceInline(TEXT("}"), TEXT("\\r"));
		Escaped.ReplaceInline(TEXT("["), TEXT("\\a"));
		Escaped.ReplaceInline(TEXT("]"), TEXT("\\z"));
		return Escaped;
	}

	static FString SerializeJsonToString(const TSharedPtr<FJsonObject>& Object)
	{
		if (!Object.IsValid())
		{
			return TEXT("");
		}

		FString Serialized;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Serialized;
	}

	static TSharedPtr<FJsonObject> DeserializeJsonObject(const FString& Serialized)
	{
		if (Serialized.IsEmpty())
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Result;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
		if (!FJsonSerializer::Deserialize(Reader, Result))
		{
			return nullptr;
		}
		return Result;
	}

	static TSharedPtr<FJsonValue> DeserializeJsonValue(const FString& Serialized)
	{
		if (Serialized.IsEmpty())
		{
			return nullptr;
		}

		TSharedPtr<FJsonValue> Result;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
		if (!FJsonSerializer::Deserialize(Reader, Result))
		{
			return nullptr;
		}
		return Result;
	}

	static FString SerializeJsonValueToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("null");
		}

		FString Serialized;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
		return Serialized;
	}
};

FString FBlueprintHelperTaskRuntimeCacheKeyUtils::StableSerializeJsonObject(
	const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return TEXT("null");
	}

	TArray<FString> Keys;
	FBlueprintHelperVersionCompat::GetJsonObjectKeys(Object, Keys);
	Keys.Sort();

	TArray<FString> Parts;
	Parts.Reserve(Keys.Num());
	for (const FString& Key : Keys)
	{
		const TSharedPtr<FJsonValue> Value = FBlueprintHelperVersionCompat::FindJsonValue(Object, Key);
		Parts.Add(FString::Printf(
			TEXT("%s=%s"),
			*FBlueprintHelperTaskRuntimeCacheKeyLocalUtils::EscapeStableString(Key),
			Value.IsValid() ? *StableSerializeJsonValue(Value) : TEXT("null")));
	}
	return TEXT("{") + FString::Join(Parts, TEXT("|")) + TEXT("}");
}

FString FBlueprintHelperTaskRuntimeCacheKeyUtils::StableSerializeJsonValue(
	const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return TEXT("null");
	}

	switch (Value->Type)
	{
	case EJson::Object:
		return StableSerializeJsonObject(Value->AsObject());
	case EJson::Array:
		{
			TArray<FString> Parts;
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				Parts.Add(StableSerializeJsonValue(Item));
			}
			return TEXT("[") + FString::Join(Parts, TEXT("|")) + TEXT("]");
		}
	case EJson::String:
		return TEXT("s:") + FBlueprintHelperTaskRuntimeCacheKeyLocalUtils::EscapeStableString(Value->AsString());
	case EJson::Number:
		return TEXT("n:") + FString::SanitizeFloat(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("b:true") : TEXT("b:false");
	case EJson::Null:
		return TEXT("null");
	default:
		break;
	}
	return FBlueprintHelperTaskRuntimeCacheKeyLocalUtils::SerializeJsonValueToString(Value);
}

FString FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJson(
	const TSharedPtr<FJsonObject>& Object)
{
	return HashString(StableSerializeJsonObject(Object));
}

FString FBlueprintHelperTaskRuntimeCacheKeyUtils::HashStableJsonValue(
	const TSharedPtr<FJsonValue>& Value)
{
	return HashString(StableSerializeJsonValue(Value));
}

FString FBlueprintHelperTaskRuntimeCacheKeyUtils::HashString(const FString& Value)
{
	const FTCHARToUTF8 Utf8Value(*Value);
	return FMD5::HashBytes(
		reinterpret_cast<const uint8*>(Utf8Value.Get()),
		Utf8Value.Length());
}

int64 FBlueprintHelperTaskRuntimeCacheKeyUtils::EstimateJsonBytes(
	const TSharedPtr<FJsonObject>& Object)
{
	return FTCHARToUTF8(*StableSerializeJsonObject(Object)).Length();
}

int64 FBlueprintHelperTaskRuntimeCacheKeyUtils::EstimateJsonValueBytes(
	const TSharedPtr<FJsonValue>& Value)
{
	return FTCHARToUTF8(*StableSerializeJsonValue(Value)).Length();
}

int64 FBlueprintHelperTaskRuntimeCacheKeyUtils::EstimateToolResultBytes(
	const FBlueprintHelperToolResultBase& Result)
{
	return FTCHARToUTF8(*Result.ToJsonString()).Length();
}

TSharedPtr<FJsonObject> FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneJsonObject(
	const TSharedPtr<FJsonObject>& Source)
{
	return FBlueprintHelperTaskRuntimeCacheKeyLocalUtils::DeserializeJsonObject(
		FBlueprintHelperTaskRuntimeCacheKeyLocalUtils::SerializeJsonToString(Source));
}

TSharedPtr<FJsonValue> FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneJsonValue(
	const TSharedPtr<FJsonValue>& Source)
{
	return FBlueprintHelperTaskRuntimeCacheKeyLocalUtils::DeserializeJsonValue(
		FBlueprintHelperTaskRuntimeCacheKeyLocalUtils::SerializeJsonValueToString(Source));
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneToolResult(
	const FBlueprintHelperToolResultBase& Source)
{
	FBlueprintHelperToolResultBase Clone = Source;
	Clone.Data = CloneJsonObject(Source.Data);
	Clone.CustomTargetJson = CloneJsonObject(Source.CustomTargetJson);
	return Clone;
}
