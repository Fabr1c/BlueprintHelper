// BlueprintHelper Utils -- 蓝图结构查询与操作函数库

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "BlueprintHelperBlueprintStructureUtils.generated.h"

class UBlueprint;
struct FParsedPinType;
struct FEdGraphPinType;

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperBlueprintStructureUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Parse a stable node GUID from string (Digits or DigitsWithHyphens format) */
	static bool ParseStableNodeGuid(const FString& NodeId, FGuid& OutGuid);

	static void ReadParsedPinType(const TSharedPtr<FJsonObject>& PinTypeObject, FParsedPinType& OutParsedPinType);

	static FParsedPinType ParsedPinTypeFromJson(const TSharedPtr<FJsonObject>& PinTypeObject);

	static bool TryConvertPinTypeObject(const TSharedPtr<FJsonObject>& PinTypeObject, FEdGraphPinType& OutPinType, FString& OutError);

	static void ReadOptionalPinTypeOrDefault(const TSharedPtr<FJsonObject>& Payload, FName DefaultCategory, FEdGraphPinType& OutPinType);

	static bool AddMemberVariableDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError);

	static bool RemoveMemberVariableDirect(UBlueprint* Blueprint, const FString& VarName, FString& OutError);

	static bool AddFunctionGraphDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError);

	static bool AddMacroGraphDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError);

	static bool RemoveGraphDirect(UBlueprint* Blueprint, const FString& GraphName, FString& OutError);

	static bool AddEventDispatcherDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError);
};
