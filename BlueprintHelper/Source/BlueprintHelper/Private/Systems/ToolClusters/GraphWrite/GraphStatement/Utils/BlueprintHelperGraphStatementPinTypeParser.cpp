#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"

#include "GraphWriteGraphStatementUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
static bool TryParseJsonPinTypeToken(const FString& Token, FBlueprintHelperCallFunctionPinType& OutPinType)
{
	const FString CleanToken = Token.TrimStartAndEnd();
	if (!CleanToken.StartsWith(TEXT("{")))
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanToken);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	FString Value;
	if (UGraphWriteGraphStatementUtils::TryReadStringField(JsonObject, TEXT("category"), Value))
	{
		OutPinType.Category = Value;
	}
	Value.Reset();
	if (!UGraphWriteGraphStatementUtils::TryReadStringField(JsonObject, TEXT("subcategory"), Value))
	{
		UGraphWriteGraphStatementUtils::TryReadStringField(JsonObject, TEXT("sub_category"), Value);
	}
	if (!Value.IsEmpty())
	{
		OutPinType.SubCategory = Value;
	}
	Value.Reset();
	if (!UGraphWriteGraphStatementUtils::TryReadStringField(JsonObject, TEXT("object_path"), Value))
	{
		UGraphWriteGraphStatementUtils::TryReadStringField(JsonObject, TEXT("sub_category_object_path"), Value);
	}
	if (!Value.IsEmpty())
	{
		OutPinType.ObjectPath = Value;
	}
	Value.Reset();
	if (!UGraphWriteGraphStatementUtils::TryReadStringField(JsonObject, TEXT("container_type"), Value))
	{
		UGraphWriteGraphStatementUtils::TryReadStringField(JsonObject, TEXT("container"), Value);
	}
	if (!Value.IsEmpty())
	{
		OutPinType.ContainerType = Value;
	}
	return OutPinType.IsValid();
}
}

FBlueprintHelperCallFunctionPinType FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(const FString& Token)
{
	FBlueprintHelperCallFunctionPinType PinType;
	if (TryParseJsonPinTypeToken(Token, PinType))
	{
		return PinType;
	}

	TArray<FString> Parts;
	Token.TrimStartAndEnd().ParseIntoArray(Parts, TEXT("|"), true);
	if (Parts.Num() == 0)
	{
		return PinType;
	}

	bool bUsedNamedParts = false;
	for (const FString& Part : Parts)
	{
		FString Key;
		FString Value;
		if (Part.Split(TEXT("="), &Key, &Value))
		{
			UGraphWriteGraphStatementUtils::ApplyNamedPinTypePart(PinType, Key, Value);
			bUsedNamedParts = true;
		}
	}

	if (bUsedNamedParts)
	{
		return PinType;
	}

	PinType.Category = Parts[0].TrimStartAndEnd();
	if (Parts.Num() > 1)
	{
		PinType.SubCategory = Parts[1].TrimStartAndEnd();
	}
	if (Parts.Num() > 2)
	{
		PinType.ObjectPath = Parts[2].TrimStartAndEnd();
	}
	else if (Parts.Num() == 2 && Parts[1].Contains(TEXT("/")))
	{
		PinType.ObjectPath = Parts[1].TrimStartAndEnd();
		PinType.SubCategory.Reset();
	}
	if (Parts.Num() > 3)
	{
		PinType.ContainerType = Parts[3].TrimStartAndEnd();
	}
	return PinType;
}
