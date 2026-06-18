// BlueprintHelper MaterialInstance parameter JSON helpers.

#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceParameterJsonUtils.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"

class FBlueprintHelperMaterialInstanceParameterJsonLocalUtils
{
public:
	static TSharedRef<FJsonObject> MakeStructuredValueJson(
		const FBlueprintHelperMaterialInstanceParameterValue& Value,
		EBlueprintHelperMaterialInstanceParameterType Type)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("has_value"), Value.bHasValue);
		if (!Value.bHasValue)
		{
			return Json;
		}

		switch (Type)
		{
		case EBlueprintHelperMaterialInstanceParameterType::Scalar:
			Json->SetNumberField(TEXT("scalar"), Value.Scalar);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Vector:
		{
			TSharedRef<FJsonObject> VectorJson = MakeShared<FJsonObject>();
			VectorJson->SetNumberField(TEXT("r"), Value.Vector.R);
			VectorJson->SetNumberField(TEXT("g"), Value.Vector.G);
			VectorJson->SetNumberField(TEXT("b"), Value.Vector.B);
			VectorJson->SetNumberField(TEXT("a"), Value.Vector.A);
			Json->SetObjectField(TEXT("vector"), VectorJson);
			break;
		}
		case EBlueprintHelperMaterialInstanceParameterType::Texture:
			Json->SetStringField(TEXT("texture_asset"), Value.TexturePath);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
			Json->SetBoolField(TEXT("static_switch"), Value.bStaticSwitch);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Unknown:
		default:
			break;
		}
		return Json;
	}

	static void AddFlatValueFields(
		const TSharedRef<FJsonObject>& Json,
		const FBlueprintHelperMaterialInstanceParameterValue& Value,
		EBlueprintHelperMaterialInstanceParameterType Type)
	{
		if (!Value.bHasValue)
		{
			return;
		}

		switch (Type)
		{
		case EBlueprintHelperMaterialInstanceParameterType::Scalar:
			Json->SetNumberField(TEXT("scalar"), Value.Scalar);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Vector:
		{
			TSharedRef<FJsonObject> VectorJson = MakeShared<FJsonObject>();
			VectorJson->SetNumberField(TEXT("r"), Value.Vector.R);
			VectorJson->SetNumberField(TEXT("g"), Value.Vector.G);
			VectorJson->SetNumberField(TEXT("b"), Value.Vector.B);
			VectorJson->SetNumberField(TEXT("a"), Value.Vector.A);
			Json->SetObjectField(TEXT("vector"), VectorJson);
			break;
		}
		case EBlueprintHelperMaterialInstanceParameterType::Texture:
			Json->SetStringField(TEXT("texture_asset"), Value.TexturePath);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
			Json->SetBoolField(TEXT("static_switch"), Value.bStaticSwitch);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Unknown:
		default:
			break;
		}
	}

	static bool TryReadVectorObject(
		const TSharedPtr<FJsonObject>& Json,
		FLinearColor& OutVector)
	{
		const TSharedPtr<FJsonObject>* VectorJson = nullptr;
		if (!Json.IsValid() ||
			!Json->TryGetObjectField(TEXT("vector"), VectorJson) ||
			!VectorJson ||
			!VectorJson->IsValid())
		{
			return false;
		}

		double R = 0.0;
		double G = 0.0;
		double B = 0.0;
		double A = 1.0;
		(*VectorJson)->TryGetNumberField(TEXT("r"), R);
		(*VectorJson)->TryGetNumberField(TEXT("g"), G);
		(*VectorJson)->TryGetNumberField(TEXT("b"), B);
		(*VectorJson)->TryGetNumberField(TEXT("a"), A);
		OutVector = FLinearColor(
			static_cast<float>(R),
			static_cast<float>(G),
			static_cast<float>(B),
			static_cast<float>(A));
		return true;
	}

	static bool TryParseScalarString(const FString& Input, double& OutValue)
	{
		if (Input.IsEmpty() || Input == TEXT("<unset>") || Input == TEXT("<unknown>"))
		{
			return false;
		}
		OutValue = FCString::Atod(*Input);
		return true;
	}

	static bool TryParseBoolString(const FString& Input, bool& bOutValue)
	{
		const FString Normalized = Input.TrimStartAndEnd().ToLower();
		if (Normalized == TEXT("true") || Normalized == TEXT("1") || Normalized == TEXT("yes"))
		{
			bOutValue = true;
			return true;
		}
		if (Normalized == TEXT("false") || Normalized == TEXT("0") || Normalized == TEXT("no"))
		{
			bOutValue = false;
			return true;
		}
		return false;
	}
};

TSharedRef<FJsonObject> FBlueprintHelperMaterialInstanceParameterJsonUtils::MakeParameterValueJson(
	const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter,
	const FString& AssetPath)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!AssetPath.IsEmpty())
	{
		Json->SetStringField(TEXT("asset_path"), AssetPath);
	}
	Json->SetStringField(TEXT("target_kind"), TEXT("material_instance_parameter"));
	Json->SetStringField(TEXT("parameter_name"), Parameter.ParameterInfo.Name.ToString());
	Json->SetStringField(TEXT("parameter_type"), BlueprintHelperMaterialInstanceParameterTypeToString(Parameter.Type));
	Json->SetBoolField(TEXT("has_override"), Parameter.bHasOverride);
	Json->SetStringField(TEXT("source"), BlueprintHelperMaterialInstanceParameterSourceToString(Parameter.Source));
	Json->SetStringField(
		TEXT("override_state"),
		MakeOverrideState(
			Parameter.bHasOverride,
			BlueprintHelperMaterialInstanceParameterSourceToString(Parameter.Source)));
	Json->SetStringField(TEXT("effective_value"), Parameter.EffectiveValue.ToDebugString(Parameter.Type));
	Json->SetStringField(TEXT("override_value"), Parameter.OverrideValue.ToDebugString(Parameter.Type));
	Json->SetObjectField(
		TEXT("effective"),
		FBlueprintHelperMaterialInstanceParameterJsonLocalUtils::MakeStructuredValueJson(
			Parameter.EffectiveValue,
			Parameter.Type));
	Json->SetObjectField(
		TEXT("override"),
		FBlueprintHelperMaterialInstanceParameterJsonLocalUtils::MakeStructuredValueJson(
			Parameter.OverrideValue,
			Parameter.Type));

	const FBlueprintHelperMaterialInstanceParameterValue& PreferredValue =
		Parameter.bHasOverride ? Parameter.OverrideValue : Parameter.EffectiveValue;
	FBlueprintHelperMaterialInstanceParameterJsonLocalUtils::AddFlatValueFields(
		Json,
		PreferredValue,
		Parameter.Type);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperMaterialInstanceParameterJsonUtils::MakeParameterSnapshotJson(
	const FString& AssetPath,
	const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter)
{
	TSharedRef<FJsonObject> Json = MakeParameterValueJson(Parameter, AssetPath);
	Json->SetBoolField(TEXT("exists"), true);
	return Json;
}

bool FBlueprintHelperMaterialInstanceParameterJsonUtils::TryReadParameterType(
	const TSharedPtr<FJsonObject>& Json,
	EBlueprintHelperMaterialInstanceParameterType& OutType)
{
	FString TypeString;
	if (!Json.IsValid() || !Json->TryGetStringField(TEXT("parameter_type"), TypeString))
	{
		OutType = EBlueprintHelperMaterialInstanceParameterType::Unknown;
		return false;
	}
	return FBlueprintHelperMaterialInstanceResolver::TryParseParameterType(TypeString, OutType);
}

bool FBlueprintHelperMaterialInstanceParameterJsonUtils::TryReadParameterValue(
	const TSharedPtr<FJsonObject>& Json,
	EBlueprintHelperMaterialInstanceParameterType Type,
	FBlueprintHelperMaterialInstanceParameterValue& OutValue,
	FString& OutError)
{
	OutValue = FBlueprintHelperMaterialInstanceParameterValue();
	if (!Json.IsValid())
	{
		OutError = TEXT("invalid_parameter_snapshot");
		return false;
	}

	switch (Type)
	{
	case EBlueprintHelperMaterialInstanceParameterType::Scalar:
	{
		double Scalar = 0.0;
		if (!Json->TryGetNumberField(TEXT("scalar"), Scalar))
		{
			FString OverrideValue;
			Json->TryGetStringField(TEXT("override_value"), OverrideValue);
			if (!FBlueprintHelperMaterialInstanceParameterJsonLocalUtils::TryParseScalarString(OverrideValue, Scalar))
			{
				OutError = TEXT("material_instance_snapshot_scalar_missing");
				return false;
			}
		}
		OutValue.bHasValue = true;
		OutValue.Scalar = static_cast<float>(Scalar);
		return true;
	}
	case EBlueprintHelperMaterialInstanceParameterType::Vector:
	{
		FLinearColor VectorValue = FLinearColor::Transparent;
		if (!FBlueprintHelperMaterialInstanceParameterJsonLocalUtils::TryReadVectorObject(Json, VectorValue))
		{
			OutError = TEXT("material_instance_snapshot_vector_missing");
			return false;
		}
		OutValue.bHasValue = true;
		OutValue.Vector = VectorValue;
		return true;
	}
	case EBlueprintHelperMaterialInstanceParameterType::Texture:
	{
		FString TexturePath;
		Json->TryGetStringField(TEXT("texture_asset"), TexturePath);
		if (TexturePath.IsEmpty())
		{
			Json->TryGetStringField(TEXT("override_value"), TexturePath);
		}
		OutValue.bHasValue = true;
		OutValue.TexturePath = TexturePath == TEXT("<unset>") ? FString() : TexturePath;
		return true;
	}
	case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
	{
		bool bValue = false;
		if (!Json->TryGetBoolField(TEXT("static_switch"), bValue))
		{
			FString OverrideValue;
			Json->TryGetStringField(TEXT("override_value"), OverrideValue);
			if (!FBlueprintHelperMaterialInstanceParameterJsonLocalUtils::TryParseBoolString(OverrideValue, bValue))
			{
				OutError = TEXT("material_instance_snapshot_static_switch_missing");
				return false;
			}
		}
		OutValue.bHasValue = true;
		OutValue.bStaticSwitch = bValue;
		return true;
	}
	case EBlueprintHelperMaterialInstanceParameterType::Unknown:
	default:
		OutError = TEXT("material_instance_snapshot_parameter_type_unknown");
		return false;
	}
}

FString FBlueprintHelperMaterialInstanceParameterJsonUtils::ReadParameterValueString(
	const TSharedPtr<FJsonObject>& Json,
	const FString& FallbackField)
{
	if (!Json.IsValid())
	{
		return FString();
	}

	FString Value;
	if (Json->TryGetStringField(FallbackField, Value) && !Value.IsEmpty())
	{
		return Value;
	}
	if (Json->TryGetStringField(TEXT("override_value"), Value) && !Value.IsEmpty())
	{
		return Value;
	}

	double Scalar = 0.0;
	if (Json->TryGetNumberField(TEXT("scalar"), Scalar))
	{
		return FString::SanitizeFloat(Scalar);
	}

	bool bSwitch = false;
	if (Json->TryGetBoolField(TEXT("static_switch"), bSwitch))
	{
		return bSwitch ? TEXT("true") : TEXT("false");
	}

	if (Json->TryGetStringField(TEXT("texture_asset"), Value))
	{
		return Value;
	}

	const TSharedPtr<FJsonObject>* VectorJson = nullptr;
	if (Json->TryGetObjectField(TEXT("vector"), VectorJson) && VectorJson && VectorJson->IsValid())
	{
		double R = 0.0;
		double G = 0.0;
		double B = 0.0;
		double A = 1.0;
		(*VectorJson)->TryGetNumberField(TEXT("r"), R);
		(*VectorJson)->TryGetNumberField(TEXT("g"), G);
		(*VectorJson)->TryGetNumberField(TEXT("b"), B);
		(*VectorJson)->TryGetNumberField(TEXT("a"), A);
		return FLinearColor(
			static_cast<float>(R),
			static_cast<float>(G),
			static_cast<float>(B),
			static_cast<float>(A)).ToString();
	}
	return FString();
}

FString FBlueprintHelperMaterialInstanceParameterJsonUtils::MakeOverrideState(
	bool bHasOverride,
	const FString& Source)
{
	if (bHasOverride)
	{
		return TEXT("override");
	}
	return Source.Equals(TEXT("none"), ESearchCase::IgnoreCase) ? TEXT("none") : TEXT("inherited");
}

bool FBlueprintHelperMaterialInstanceParameterJsonUtils::SerializeJsonObject(
	const TSharedPtr<FJsonObject>& Json,
	FString& OutJsonText)
{
	OutJsonText.Reset();
	if (!Json.IsValid())
	{
		return false;
	}

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonText);
	return FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);
}

TSharedPtr<FJsonObject> FBlueprintHelperMaterialInstanceParameterJsonUtils::ParseJsonObject(
	const FString& JsonText)
{
	TSharedPtr<FJsonObject> Json;
	if (JsonText.IsEmpty())
	{
		return Json;
	}
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	FJsonSerializer::Deserialize(Reader, Json);
	return Json;
}
