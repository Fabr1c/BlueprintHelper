#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

class UBlueprint;
class UBlueprintNodeSpawner;
class UEdGraph;

/**
 * UE NodeSpawner-family-oriented cluster boundary. This is the first-level
 * ActionResolution request type.
 */
enum class EBlueprintHelperSpawnerClusterKind : uint8
{
	FunctionAction,
	FieldVariableAction,
	EventDelegateAction,
	GenericAssetStructControlAction,
	Unknown
};

/**
 * AgentFace semantic operation kind. This is a constraint consumed inside the
 * selected spawner cluster, not a first-level ActionResolution request type.
 */
enum class EBlueprintHelperActionSemanticKind : uint8
{
	Call,
	Get,
	Set,
	GetProperty,
	SetProperty,
	Op,
	Construct,
	Deconstruct,
	Select,
	Event,
	ComponentBoundEvent,
	Bind,
	Control,
	Create,
	Convert,
	Schedule,
	Unknown
};

enum class EBlueprintHelperActionResolutionStatus : uint8
{
	Resolved,
	Ambiguous,
	UnsupportedIntent,
	NotFound,
	Blocked,
	InvalidRequest
};

struct FBlueprintHelperActionSemanticConstraints
{
	EBlueprintHelperActionSemanticKind Kind = EBlueprintHelperActionSemanticKind::Unknown;
	FString Query;
	FString StableId;
	FString TargetPath;
	FString PropertyPath;
	FString TypeName;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	TMap<FString, FString> DefaultValues;
	TArray<FString> ArgumentNames;
	TMap<FString, FString> ArgumentTypes;
	TMap<FString, FBlueprintHelperCallFunctionPinType> ArgumentPinTypes;
	FString TargetObjectType;
	FBlueprintHelperCallFunctionPinType TargetObjectPinType;
	FString ExpectedReturnType;
	FBlueprintHelperCallFunctionPinType ExpectedReturnPinType;
};

struct FBlueprintHelperActionResolutionRequest
{
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	UBlueprint* Blueprint = nullptr;
	UEdGraph* TargetGraph = nullptr;
	FString StatementId;
	FString ProjectedContextHash;
	FString SemanticConstraintsHash;
	TMap<FString, FString> ContextEvidence;
	FBlueprintHelperActionSemanticConstraints Semantic;
	bool bAllowFuzzyUnique = true;
	int32 MaxCandidates = 0;
};

struct FBlueprintHelperActionResolutionResult
{
	EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	FString ErrorCode;
	FString Message;
	FString SelectedStableId;
	TWeakObjectPtr<UBlueprintNodeSpawner> SelectedSpawner;
	TWeakObjectPtr<UFunction> SelectedFunction;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateActions;
	FBlueprintHelperCallFunctionCandidate FunctionCandidate;

	bool IsResolved() const
	{
		return Status == EBlueprintHelperActionResolutionStatus::Resolved;
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperActionResolutionCore
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);
	static FString SemanticKindToString(EBlueprintHelperActionSemanticKind Kind);
	static FString ClusterKindToString(EBlueprintHelperSpawnerClusterKind ClusterKind);
};
