#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"

class UEdGraph;
class UEdGraphNode;

struct BLUEPRINTHELPER_API FBlueprintGraphWriteConnectivityValidationInput
{
	UEdGraph* TargetGraph = nullptr;
	TArray<UEdGraphNode*> GeneratedNodes;
	FBlueprintHelperGraphBodyBoundaryModel BoundaryModel;
	FBlueprintHelperGraphConnectivityPolicy ConnectivityPolicy;
	TMap<FString, UEdGraphNode*> NodeRefs;
	TSet<UEdGraphNode*> AllowedTerminalPureDataNodes;
	int32 RequestedConnectionCount = 0;
	int32 CreatedConnectionCount = 0;
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
