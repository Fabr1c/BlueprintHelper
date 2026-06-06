#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EBlueprintHelperGraphBodyKind : uint8
{
	Unknown,
	K2CustomEventBody,
	K2EventBody,
	K2FunctionBody,
	K2MacroBody,
	K2BlockImplementation,
	K2ExternalBody,
	ReservedMaterialFunctionBody,
	ReservedAnimationGraphBody
};

enum class EBlueprintHelperGraphBodyPureDataPolicy : uint8
{
	RequireReachableExecConsumer,
	AllowTerminalPureDataOutput,
	PureDataGraphOutput
};

enum class EBlueprintHelperGraphBodyIsolatedNodePolicy : uint8
{
	CommentsAndReroutesOnly,
	AdapterDeclaredOnly
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyBoundaryModel
{
	FString RuntimeAdapterId;
	FString TaskSpecStrategy;
	FString TargetAssetPath;
	FString GraphName;
	FString GraphFamily;
	FString OwnedBlockId;
	EBlueprintHelperGraphBodyKind BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
	TArray<FString> EntryNodeRefs;
	TArray<FString> ExitNodeRefs;
	TArray<FString> ProtectedNodeRefs;
	TArray<FString> DeletableNodeRefs;
	TArray<FString> GeneratedNodeRefs;
	TArray<FString> ImportedBodyNodeRefs;
	TArray<FString> ReachableBodyFlowNodeRefs;
	TArray<FString> ExternalAnchorRefs;
	TArray<FString> SemanticSourceRefs;
	TArray<FString> ConnectivityExceptionCodes;
	EBlueprintHelperGraphBodyPureDataPolicy PureDataConsumptionPolicy =
		EBlueprintHelperGraphBodyPureDataPolicy::RequireReachableExecConsumer;
	EBlueprintHelperGraphBodyIsolatedNodePolicy AllowedIsolatedNodePolicy =
		EBlueprintHelperGraphBodyIsolatedNodePolicy::CommentsAndReroutesOnly;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphBodyBoundaryModelUtils
{
public:
	static FString BodyKindToString(EBlueprintHelperGraphBodyKind Kind);
	static EBlueprintHelperGraphBodyKind BodyKindFromString(const FString& Value);
	static FString MakeBodyIdentity(const FBlueprintHelperGraphBodyBoundaryModel& Model);
	static TSharedRef<FJsonObject> ToJsonObject(const FBlueprintHelperGraphBodyBoundaryModel& Model);
};
