#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteConnectivityValidator.h"

class UEdGraph;
class UEdGraphNode;
class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteConnectivityContextInput
{
	FString RuntimeAdapterId;
	FString TaskSpecStrategy;
	FString TargetAssetPath;
	FString GraphName;
	FString GraphFamily = TEXT("k2");
	EBlueprintHelperGraphBodyKind BodyKind = EBlueprintHelperGraphBodyKind::Unknown;
	TArray<FString> EntryNodeRefs;
	TArray<UEdGraphNode*> EntryNodes;
	TArray<FString> ExitNodeRefs;
	TArray<UEdGraphNode*> ExitNodes;
	TArray<FString> ProtectedNodeRefs;
	TArray<UEdGraphNode*> ProtectedNodes;
	EBlueprintHelperGraphBodyPureDataPolicy PureDataPolicy =
		EBlueprintHelperGraphBodyPureDataPolicy::RequireReachableExecConsumer;
	EBlueprintHelperGraphBodyIsolatedNodePolicy IsolatedNodePolicy =
		EBlueprintHelperGraphBodyIsolatedNodePolicy::CommentsAndReroutesOnly;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteConnectivityContextBuilder
{
public:
	static FBlueprintGraphWriteConnectivityValidationInput Build(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphWriteConnectivityContextInput& Input);
	static FBlueprintGraphWriteConnectivityValidationInput BuildSemanticGenerationContext(
		UEdGraph* TargetGraph,
		const FString& RuntimeAdapterId,
		const FString& TaskSpecStrategy,
		const FString& GraphWriteJsonText,
		const FString& TargetAssetPath = TEXT(""));
	static FString MakeSemanticEntryRefFromLogicSpec(const TSharedPtr<FJsonObject>& LogicSpecObject);
	static FString MakeSemanticEntryRefFromGraphWriteJsonText(const FString& GraphWriteJsonText);

private:
	static void AddContextNodeRefs(
		FBlueprintGraphWriteConnectivityValidationInput& Out,
		const TArray<FString>& Refs,
		const TArray<UEdGraphNode*>& Nodes);
};
