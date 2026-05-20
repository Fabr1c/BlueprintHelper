#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class FBlueprintGraphWriteContext;

class BLUEPRINTHELPER_API FBlueprintGraphDefaultValueApplier
{
public:
	static bool ApplyPinDefaultValue( UEdGraphPin* TargetPin, const FString& InValue, FString& OutDiagnosticCode, FString& OutMessage);
	static TArray<FBlueprintGeneratorDiagnostic> ApplyDefaultValues(FBlueprintGraphWriteContext& Context, const FString& NodeId, const TMap<FString, FString>& DefaultValues);
	static TArray<FBlueprintGeneratorDiagnostic> ApplyDefaultValues( UK2Node* TargetNode, const TMap<FString, FString>& DefaultValues, const FString& NodeId = TEXT(""));
};
