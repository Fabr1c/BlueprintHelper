#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;
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
		const FParsedNode& NodeData,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildVariableSetFragment(
		UEdGraph* TargetGraph,
		const FParsedNode& NodeData,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildExpressionFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphExpressionIR& Expression,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
