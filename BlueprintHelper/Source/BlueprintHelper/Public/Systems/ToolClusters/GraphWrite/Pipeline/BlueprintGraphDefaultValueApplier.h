#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class BLUEPRINTHELPER_API FBlueprintGraphDefaultValueApplier
{
public:
	static bool ApplyPinDefaultValue( UEdGraphPin* TargetPin, const FString& InValue, FString& OutDiagnosticCode, FString& OutMessage);
	static TArray<FBlueprintGeneratorDiagnostic> ApplyDefaultValues( UK2Node* TargetNode, const TMap<FString, FString>& DefaultValues, const FString& NodeId = TEXT(""));
};
