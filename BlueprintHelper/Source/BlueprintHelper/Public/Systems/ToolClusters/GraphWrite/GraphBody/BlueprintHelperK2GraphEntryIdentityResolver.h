#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;

enum class EBlueprintHelperK2GraphEntryKind : uint8
{
	Unknown,
	Event,
	CustomEvent,
	FunctionEntry,
	FunctionResult,
	MacroEntry,
	MacroExit,
};

enum class EBlueprintHelperK2GraphBoundaryRole : uint8
{
	Unknown,
	BodyEntry,
	BodyExit,
	ExecBoundary,
};

struct BLUEPRINTHELPER_API FBlueprintHelperK2GraphEntryIdentity
{
	EBlueprintHelperK2GraphEntryKind Kind = EBlueprintHelperK2GraphEntryKind::Unknown;
	EBlueprintHelperK2GraphBoundaryRole Role = EBlueprintHelperK2GraphBoundaryRole::Unknown;
	FString NodeGuid;
	FString NodeClass;
	FString StableName;
	FString DisplayName;
	FString GraphName;
	FString MemberName;
	FString FunctionName;
	bool bValid = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperK2GraphEntryQuery
{
	FString TargetType;
	FString TargetName;
	FString GraphName;
	EBlueprintHelperK2GraphEntryKind RequiredKind = EBlueprintHelperK2GraphEntryKind::Unknown;
	EBlueprintHelperK2GraphBoundaryRole RequiredRole = EBlueprintHelperK2GraphBoundaryRole::Unknown;
};

class BLUEPRINTHELPER_API FBlueprintHelperK2GraphEntryIdentityResolver
{
public:
	bool TryResolveNodeIdentity(
		const UEdGraphNode* Node,
		FBlueprintHelperK2GraphEntryIdentity& OutIdentity) const;

	bool DoesIdentityMatchQuery(
		const FBlueprintHelperK2GraphEntryIdentity& Identity,
		const FBlueprintHelperK2GraphEntryQuery& Query) const;

	bool TryFindEntryNode(
		UEdGraph* Graph,
		const FBlueprintHelperK2GraphEntryQuery& Query,
		UEdGraphNode*& OutNode,
		FBlueprintHelperK2GraphEntryIdentity& OutIdentity,
		FString& OutError) const;
};
