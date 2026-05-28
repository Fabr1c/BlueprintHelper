#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Serialization/JsonTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "GraphWritePipelineUtils.generated.h"

class UEdGraph;
class UK2Node_CustomEvent;
class UK2Node;
class FBlueprintHelperActionContextScope;

struct FSemanticStatementExecFlow
{
	TArray<UEdGraphPin*> Entries;
	TArray<UEdGraphPin*> Exits;
	bool bPreservePreviousExits = false;
};

UCLASS()
class BLUEPRINTHELPER_API UGraphWritePipelineUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========== From BlueprintGraphLocalVariableService.cpp ==========
	static void CopyParsedPinTypeToMapValueTerminal(const FEdGraphPinType& PinType, FEdGraphTerminalType& OutTerminalType);

	// ========== From BlueprintGraphGenerationPipeline.cpp ==========
	static bool TryReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue);
	static double GraphWriteElapsedMs(double StartSeconds);
	static bool ShouldReconstructExistingNodes(const TSharedPtr<FJsonObject>& Object);
	static bool IsDryRunPayload(const TSharedPtr<FJsonObject>& Object);
	static UK2Node_CustomEvent* FindExistingCustomEventNode(UEdGraph* Graph, const FString& EventName);
	static UK2Node_CustomEvent* CreateDryRunSignatureDependencyCustomEventNode(UEdGraph* Graph, const FString& EventName);
	static void ReadFragmentEndpointRef(const TSharedPtr<FJsonObject>& Object, FBlueprintHelperGraphFragmentEndpointRef& OutEndpoint);
	static void ReadFragmentDataEdgesFromObject(const TSharedPtr<FJsonObject>& Object, TArray<FBlueprintHelperGraphFragmentDataEdge>& OutDataEdges);
	static void AppendSemanticFragmentDataEdges(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Object, const TArray<FBlueprintHelperNodeFragment>& Fragments, TArray<FBlueprintHelperGraphFragmentDataEdge>& OutDataEdges);
	static TArray<FBlueprintHelperGraphFragmentDataEdge> CollectFragmentDataEdges(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& Object, const TArray<FBlueprintHelperNodeFragment>& Fragments);
	static void AddFragmentPinAlias(FBlueprintHelperNodeFragment& Fragment, TMap<FString, FBlueprintHelperFragmentPinRef>& DirectionMap, const FString& Alias, const FBlueprintHelperFragmentPinRef& SourcePinRef);
	static FBlueprintHelperNodeFragment BuildDataOnlyFragment(const FString& NodeId, UK2Node* Node);
	static FString GetSemanticStatementId(const FBlueprintHelperGraphStatementIR& Statement);
	static FString GetSemanticStatementContextId(const FBlueprintHelperGraphStatementIR& Statement);
	static FString GetSemanticExpressionId(const FBlueprintHelperGraphExpressionIR& Expression);
	static void AddSemanticUnresolved(TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes, const FString& DisplayText, const FString& Reason, const TArray<FBlueprintHelperCandidateFunctionGroup>& CandidateFunctions = TArray<FBlueprintHelperCandidateFunctionGroup>());
	static bool ConnectSemanticExecPins(UEdGraph* TargetGraph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics, int32& CreatedConnectionCount);
	static void AddSemanticFragment(const FBlueprintHelperNodeFragment& Fragment, TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, TSet<FString>& GeneratedFragmentIds, int32& GeneratedNodeCount);

	static void BuildSemanticExpressionMapFragments(UEdGraph* TargetGraph, const FBlueprintHelperActionContextScope* ActionContextScope, const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Expressions, TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, TSet<FString>& GeneratedFragmentIds, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes, int32& GeneratedNodeCount);
	static void BuildSemanticExpressionFragments(UEdGraph* TargetGraph, const FBlueprintHelperActionContextScope* ActionContextScope, const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression, TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, TSet<FString>& GeneratedFragmentIds, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes, int32& GeneratedNodeCount);

	static void FillCallArgsAsDefaultsAndTypes(const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args, TMap<FString, FString>& OutDefaultValues, TMap<FString, FString>& OutArgumentTypes);
	static FBlueprintHelperCallFunctionPinType MakeCallFunctionPinTypeFromEdGraphPin(const UEdGraphPin* Pin);
	static FBlueprintHelperCallFunctionPinType MakeCallFunctionPinTypeFromDagRef(const FBlueprintHelperGraphFragmentPinTypeRef& PinType);
	static UEdGraphPin* FindFragmentPinByKey(const TMap<FString, FBlueprintHelperFragmentPinRef>& Pins, const FString& Key);
	static const FBlueprintHelperNodeFragment* FindGeneratedFragment(const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, const FString& FragmentId);
	static FBlueprintHelperCallFunctionPinType ResolveSemanticDataEdgeSourcePinType(const FBlueprintHelperGraphFragmentDataEdge& DataEdge, const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments);
	static TMap<FString, FBlueprintHelperCallFunctionPinType> CollectSemanticArgumentPinTypes(const FBlueprintHelperGraphFragmentDag& FragmentDag, const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, const FString& ConsumerFragmentId);
	static bool SpawnSemanticStatementFragment(UEdGraph* TargetGraph, const FBlueprintHelperActionContextScope* ActionContextScope, const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement, FBlueprintHelperNodeFragment& OutFragment, FString& OutError, TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr, const TMap<FString, FBlueprintHelperCallFunctionPinType>* SemanticArgumentPinTypes = nullptr);

	static FSemanticStatementExecFlow BuildSemanticStatementArray(UEdGraph* TargetGraph, const FBlueprintHelperActionContextScope* ActionContextScope, const FBlueprintHelperGraphFragmentDag& FragmentDag, const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements, TArray<UEdGraphPin*> IncomingExits, TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, TSet<FString>& GeneratedFragmentIds, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics, int32& GeneratedNodeCount, int32& CreatedConnectionCount);
	static FSemanticStatementExecFlow BuildSemanticStatement(UEdGraph* TargetGraph, const FBlueprintHelperActionContextScope* ActionContextScope, const FBlueprintHelperGraphFragmentDag& FragmentDag, const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement, TArray<FBlueprintHelperNodeFragment>& GeneratedFragments, TSet<FString>& GeneratedFragmentIds, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes, TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics, int32& GeneratedNodeCount, int32& CreatedConnectionCount);
	static TArray<FBlueprintHelperGraphFragmentDataEdge> FilterSemanticDataEdges(const FBlueprintHelperGraphFragmentDag& FragmentDag, const TSet<FString>& GeneratedFragmentIds);
	static FBlueprintGenerateResult GenerateSemanticGraphFromJsonObject(UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& JsonObject, TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes);
};
