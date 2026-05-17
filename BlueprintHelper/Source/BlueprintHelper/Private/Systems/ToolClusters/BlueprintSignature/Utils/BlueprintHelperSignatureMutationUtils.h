// BlueprintHelper signature mutation helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintSignature/BlueprintHelperSignatureTypes.h"

class UBlueprint;
class UEdGraph;
class UK2Node_CustomEvent;
class UK2Node_Event;

class FBlueprintHelperSignatureMutationUtils
{
public:
	static UBlueprint* LoadSignatureBlueprint(const FString& AssetPath);

	static bool RemoveEventDispatcherSignatureDirect(
		UBlueprint* Blueprint,
		const FString& DispatcherName,
		bool& bOutRemoved,
		FString& OutError);

	static bool RemoveSignatureDirect(
		UBlueprint* Blueprint,
		const FBlueprintHelperRemoveSignatureRequest& Request,
		bool& bOutRemoved,
		FString& OutError);

private:
	static UEdGraph* FindGraphByName(const TArray<TObjectPtr<UEdGraph>>& Graphs, const FString& GraphName);
	static UEdGraph* FindGraphByName(const TArray<UEdGraph*>& Graphs, const FString& GraphName);
	static UK2Node_CustomEvent* FindCustomEventInGraph(UEdGraph* Graph, const FString& EventName);
	static FName ResolveNativeOrOverrideEventName(const FString& InEventName);
	static UFunction* ResolveNativeOrOverrideEventDeclarationFunction(UFunction* EventFunction);
	static UClass* ResolveNativeOrOverrideEventSignatureClass(UFunction* EventFunction, UClass* FallbackSignatureClass);
	static UK2Node_Event* FindOverrideEventInBlueprint(UBlueprint* Blueprint, const FString& EventName);
	static bool IsEventDispatcherVariable(UBlueprint* Blueprint, const FName DispatcherName);
};
