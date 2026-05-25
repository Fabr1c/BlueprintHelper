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

enum class EBlueprintHelperActionSemanticFamily : uint8
{
	Callable,
	Field,
	Operator,
	Struct,
	TypeStructure,
	Event,
	Delegate,
	Control,
	Create,
	Convert,
	Schedule,
	Unknown
};

enum class EBlueprintHelperTypeOperation : uint8
{
	None,
	Construct,
	Deconstruct
};

/**
 * AgentFace semantic operation kind. This is a constraint consumed inside the
 * selected spawner cluster, not a first-level ActionResolution request type.
 */
enum class EBlueprintHelperActionSemanticKind : uint8
{
	Call,
	Field,
	Op,
	Construct,
	Deconstruct,
	Select,
	Event,
	ComponentBoundEvent,
	Delegate,
	Control,
	Create,
	Convert,
	Schedule,
	ContainerAction,
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
	EBlueprintHelperActionSemanticFamily SemanticFamily = EBlueprintHelperActionSemanticFamily::Unknown;
	EBlueprintHelperTypeOperation TypeOperation = EBlueprintHelperTypeOperation::None;
	FString Query;
	FString StableId;
	FString TargetPath;
	FString PropertyPath;
	FString FieldOperation;
	FString FieldScope;
	FString FunctionOperation;
	FString TransformOperation;
	FString ScheduleOperation;
	FString CreateOperation;
	FString ContainerKind;
	FString ContainerOperation;
	FString ClassPath;
	FString AssetPath;
	FString ElementType;
	FString KeyType;
	FString ValueType;
	FString TypeName;
	FString StructPath;
	FString TypeStructureId;
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
	FBlueprintHelperCallFunctionPinType ContainerElementPinType;
	FBlueprintHelperCallFunctionPinType ContainerKeyPinType;
	FBlueprintHelperCallFunctionPinType ContainerValuePinType;
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
	FString StructPath;
	FString TypeStructureId;
	FString TypeOperation;
	FString SpawnerClass;
	FString NodeClass;
	FString MatchReason;
	bool bRequiresDedicatedFragmentBuilder = false;

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
	static FString SemanticFamilyToString(EBlueprintHelperActionSemanticFamily Family);
	static FString TypeOperationToString(EBlueprintHelperTypeOperation Operation);
	static FString ClusterKindToString(EBlueprintHelperSpawnerClusterKind ClusterKind);
};
