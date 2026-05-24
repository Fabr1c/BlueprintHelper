#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;
class FBlueprintHelperActionContextScope;
struct FBlueprintHelperGraphExpressionIR;

struct BLUEPRINTHELPER_API FBlueprintHelperFragmentPinRef
{
	FString NodeId;
	FString PinName;
	FString Type;
	UEdGraphPin* Pin = nullptr;
};

struct BLUEPRINTHELPER_API FBlueprintHelperFragmentLink
{
	FBlueprintHelperFragmentPinRef From;
	FBlueprintHelperFragmentPinRef To;
};

struct BLUEPRINTHELPER_API FBlueprintHelperNodeFragment
{
	FString FragmentId;
	FString SourceStatementId;
	UK2Node* PrimaryNode = nullptr;
	TArray<UEdGraphNode*> Nodes;
	TArray<FBlueprintHelperFragmentLink> InternalLinks;
	UEdGraphPin* ExecEntryPin = nullptr;
	UEdGraphPin* ExecExitPin = nullptr;
	TMap<FString, FBlueprintHelperFragmentPinRef> DataInputs;
	TMap<FString, FBlueprintHelperFragmentPinRef> DataOutputs;
	TMap<FString, FBlueprintHelperFragmentPinRef> PinBindings;
	// DEPRECATED_LAYOUT: legacy hint bag. GraphWrite must not be the source of layout rules.
	// New layout behavior belongs to the UE-side GraphLayout system rule set.
	TMap<FString, FString> LayoutHints;
	TMap<FString, FString> OwnershipTags;
	TArray<FString> ReviewTargets;
	TArray<FString> Diagnostics;

	bool IsValid() const
	{
		return PrimaryNode != nullptr;
	}
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphStatementBuilder
{
public:
	static bool BuildCallFunctionFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr,
		const FBlueprintHelperActionContextScope* ActionContextScope = nullptr);

	static bool BuildVariableSetFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		const FBlueprintHelperActionContextScope* ActionContextScope = nullptr);

	static bool BuildSetPropertyFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		const FBlueprintHelperActionContextScope* ActionContextScope = nullptr);

	static bool BuildCreateFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		const FBlueprintHelperActionContextScope* ActionContextScope = nullptr);

	static bool BuildActionProviderFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		EBlueprintHelperActionSemanticKind SemanticKind,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		const FBlueprintHelperActionContextScope* ActionContextScope = nullptr);

	static bool BuildSequenceFragment(
		UEdGraph* TargetGraph,
		const FString& FragmentId,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildExpressionFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphExpressionIR& Expression,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr,
		const FBlueprintHelperActionContextScope* ActionContextScope = nullptr);
};
