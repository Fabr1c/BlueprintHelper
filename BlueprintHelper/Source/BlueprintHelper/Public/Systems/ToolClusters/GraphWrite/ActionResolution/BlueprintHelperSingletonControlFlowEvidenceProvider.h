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
	static bool TryBuildCanonicalRequest(
		EBlueprintHelperSingletonControlFlowKind Kind,
		UBlueprint* Blueprint,
		UEdGraph* TargetGraph,
		const FString& StatementId,
		const FString& Reason,
		FBlueprintHelperActionResolutionRequest& OutRequest);

	static FBlueprintHelperActionResolutionResult ResolveCanonical(
		EBlueprintHelperSingletonControlFlowKind Kind,
		UBlueprint* Blueprint,
		UEdGraph* TargetGraph,
		const FString& StatementId,
		const FString& Reason);

	static bool TryResolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperSingletonControlFlowEvidence& OutEvidence);

	static FBlueprintHelperActionResolutionResult MakeResolvedResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperSingletonControlFlowEvidence& Evidence);
};
