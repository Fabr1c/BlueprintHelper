#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraph;
class UEdGraphPin;
class UK2Node;
class UK2Node_CreateDelegate;

struct FBlueprintHelperDelegateLinkRequest
{
	FString FragmentId;
	FString HandlerName;
	FString HandlerFunctionPath;
	FString HandlerScopeClassPath;
	FString SignatureEvidenceId;
	FString DelegateInputPinName;
	FString DiagnosticPrefix = TEXT("delegate");
	FVector2D CreateDelegateLocation = FVector2D::ZeroVector;
};

class FBlueprintHelperDelegateLinkFragmentUtils
{
public:
	static bool AttachCreateDelegateToPrimary(
		UEdGraph* TargetGraph,
		UK2Node* PrimaryNode,
		const FBlueprintHelperDelegateLinkRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static UK2Node_CreateDelegate* SpawnCreateDelegateNode(
		UEdGraph* TargetGraph,
		const FBlueprintHelperDelegateLinkRequest& Request,
		FString& OutError);

	static UEdGraphPin* ResolveDelegateInputPin(
		UK2Node* PrimaryNode,
		const FString& DelegateInputPinName,
		FString& OutError);

	static UEdGraphPin* ResolveDelegateInputPin(
		UK2Node* PrimaryNode,
		const FString& DelegateInputPinName,
		const FString& DiagnosticPrefix,
		FString& OutError);

	static bool ConnectCreateDelegateToPin(
		UEdGraph* TargetGraph,
		UEdGraphPin* DelegateInPin,
		UK2Node_CreateDelegate* CreateDelegateNode,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
