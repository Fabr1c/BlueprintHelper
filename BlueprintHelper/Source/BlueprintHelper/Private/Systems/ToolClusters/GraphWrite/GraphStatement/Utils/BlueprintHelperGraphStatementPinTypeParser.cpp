#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"

#include "GraphWriteGraphStatementUtils.h"

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
