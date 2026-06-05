#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

struct FBlueprintHelperGraphWriteRollbackInput
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* TargetGraph = nullptr;
	bool bGraphExistedBeforeWrite = true;
	bool bPackageWasDirtyBeforeWrite = false;
	TSet<UEdGraphNode*> NodeSnapshot;
	TArray<UEdGraphNode*> ImportedNodes;
	FString ReasonCode;
};

struct FBlueprintHelperGraphWriteRollbackResult
{
	bool bRolledBack = false;
	FString ErrorMessage;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteRollbackFinalizer
{
public:
	FBlueprintHelperGraphWriteRollbackResult RollbackPostImportFailure(
		const FBlueprintHelperGraphWriteRollbackInput& Input) const;
};
