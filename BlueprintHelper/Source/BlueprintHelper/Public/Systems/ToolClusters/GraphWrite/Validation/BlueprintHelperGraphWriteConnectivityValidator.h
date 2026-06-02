#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"

class UEdGraph;
class UEdGraphNode;

struct BLUEPRINTHELPER_API FBlueprintGraphWriteConnectivityValidationInput
{
	UEdGraph* TargetGraph = nullptr;
	TArray<UEdGraphNode*> GeneratedNodes;
	TSet<UEdGraphNode*> EntryRootNodes;
	int32 RequestedConnectionCount = 0;
	int32 CreatedConnectionCount = 0;
	bool bRequirePureDataReachableToExec = true;
};

struct BLUEPRINTHELPER_API FBlueprintGraphWriteConnectivityValidationResult
{
	bool bPassed = true;
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteConnectivityValidator
{
public:
	static FBlueprintGraphWriteConnectivityValidationResult Validate(
		const FBlueprintGraphWriteConnectivityValidationInput& Input);
};
