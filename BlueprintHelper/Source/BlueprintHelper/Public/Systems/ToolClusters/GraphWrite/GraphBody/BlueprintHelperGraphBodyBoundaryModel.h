#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperGraphBodyKind : uint8
{
	Unknown,
	K2CustomEventBody,
	K2EventBody,
	K2FunctionBody,
	K2BlockImplementation,
	K2ExternalBody,
	ReservedMacroBody
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyBoundaryModel
{
	FString RuntimeAdapterId;
	FString TaskSpecStrategy;
	FString TargetAssetPath;
	FString GraphName;
	FString OwnedBlockId;
	EBlueprintHelperGraphBodyKind BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
	TArray<FString> EntryNodeRefs;
	TArray<FString> ExitNodeRefs;
	TArray<FString> ProtectedNodeRefs;
	TArray<FString> GeneratedNodeRefs;
	TArray<FString> ImportedBodyNodeRefs;
	TArray<FString> ExternalAnchorRefs;
	TArray<FString> ConnectivityExceptionCodes;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphBodyBoundaryModelUtils
{
public:
	static FString BodyKindToString(EBlueprintHelperGraphBodyKind Kind);
	static EBlueprintHelperGraphBodyKind BodyKindFromString(const FString& Value);
	static FString MakeBodyIdentity(const FBlueprintHelperGraphBodyBoundaryModel& Model);
};
