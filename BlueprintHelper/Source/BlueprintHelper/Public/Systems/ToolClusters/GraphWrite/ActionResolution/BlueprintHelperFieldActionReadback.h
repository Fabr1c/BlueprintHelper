#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraphNode;

struct BLUEPRINTHELPER_API FBlueprintHelperFieldCompileDiagnosticReadback
{
	FString CapabilityId;
	FString Code;
	FString Message;
	FString NodeGuid;
};

struct BLUEPRINTHELPER_API FBlueprintHelperFieldActionReadback
{
	FString CapabilityId;
	FString NodeGuid;
	FString NodeClassPath;
	FString NodeTitle;
	FString ExpectedNodeFamily;
	TMap<FString, FString> Facts;

	void AppendFlatFacts(TMap<FString, FString>& OutFacts) const;
};

class BLUEPRINTHELPER_API FBlueprintHelperFieldActionReadbackCollector
{
public:
	static FBlueprintHelperFieldActionReadback CollectFromNode(
		const FString& CapabilityId,
		UEdGraphNode* Node);

	static void CollectCompileDiagnostics(
		UBlueprint* Blueprint,
		const FString& CapabilityId,
		TArray<FBlueprintHelperFieldCompileDiagnosticReadback>& OutDiagnostics);
};
