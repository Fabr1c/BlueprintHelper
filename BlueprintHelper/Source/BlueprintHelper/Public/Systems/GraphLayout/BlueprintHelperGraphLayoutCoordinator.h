#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;

class BLUEPRINTHELPER_API FBlueprintHelperGraphLayoutCoordinator
{
public:
	static void Startup();
	static void RecordGeneratedNodes(UEdGraph* Graph, const TArray<UEdGraphNode*>& GeneratedNodes);
	static void FlushPendingTaskLayouts();
	static void DiscardPendingTaskLayouts();
	static void Shutdown();

	static FString GetDefaultRuleSetJson();
	static FString LoadConfiguredRuleSetJson();
	static bool SaveConfiguredRuleSetJson(const FString& JsonText);
	static bool ValidateRuleSetJson(const FString& JsonText, FString& OutMessage);
};
