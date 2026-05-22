#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UEdGraphNode;

enum class EBlueprintHelperSingletonControlFlowKind : uint8
{
	Branch,
	Sequence,
	Return,
	Select,
	Unknown
};

struct FBlueprintHelperSingletonControlFlowEvidence
{
	EBlueprintHelperSingletonControlFlowKind SingletonKind = EBlueprintHelperSingletonControlFlowKind::Unknown;
	TSubclassOf<UEdGraphNode> NodeClass = nullptr;
	FString StableId;
	FString Reason;
};

class BLUEPRINTHELPER_API FBlueprintHelperSingletonControlFlowEvidenceProvider
{
public:
	static bool TryResolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperSingletonControlFlowEvidence& OutEvidence);

	static FBlueprintHelperActionResolutionResult MakeResolvedResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperSingletonControlFlowEvidence& Evidence);
};
