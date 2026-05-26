#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"

namespace
{
static void ApplyNamedPinTypePart(
	FBlueprintHelperCallFunctionPinType& PinType,
	const FString& Key,
	const FString& Value)
{
	const FString CleanKey = Key.TrimStartAndEnd().ToLower();
	const FString CleanValue = Value.TrimStartAndEnd();
	if (CleanValue.IsEmpty())
	{
		return;
	}

	if (CleanKey == TEXT("category") || CleanKey == TEXT("pin_category") || CleanKey == TEXT("type"))
	{
		PinType.Category = CleanValue;
	}
	else if (CleanKey == TEXT("subcategory") || CleanKey == TEXT("sub_category") || CleanKey == TEXT("pin_sub_category"))
	{
		PinType.SubCategory = CleanValue;
	}
	else if (CleanKey == TEXT("object") || CleanKey == TEXT("object_path") || CleanKey == TEXT("pin_sub_category_object"))
	{
		PinType.ObjectPath = CleanValue;
	}
	else if (CleanKey == TEXT("container") || CleanKey == TEXT("container_type"))
	{
		PinType.ContainerType = CleanValue;
	}
	else if (CleanKey == TEXT("ref") || CleanKey == TEXT("reference"))
	{
		PinType.bIsReference = CleanValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}
	else if (CleanKey == TEXT("const"))
	{
		PinType.bIsConst = CleanValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}
}
}

FBlueprintHelperCallFunctionPinType FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(const FString& Token)
{
	FBlueprintHelperCallFunctionPinType PinType;
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
			ApplyNamedPinTypePart(PinType, Key, Value);
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
