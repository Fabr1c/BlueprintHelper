#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "GraphWriteGraphStatementUtils.generated.h"

class UBlueprint;
enum class EBlueprintHelperGraphFragmentPortDirection : uint8;

class UEdGraph;
class UEdGraphSchema;
class UK2Node_AssignDelegate;
class UK2Node_Select;
class UK2Node_IfThenElse;
class UK2Node_ExecutionSequence;
class UK2Node_FunctionResult;
class UK2Node_BaseMCDelegate;
class UK2Node_CreateDelegate;
class UK2Node_SetFieldsInStruct;
class UK2Node_BreakStruct;
class UK2Node_CallFunction;
struct FBlueprintHelperGraphSemanticIR;
struct FBlueprintHelperGraphStatementIR;
struct FBlueprintHelperGraphExpressionIR;
struct FBlueprintHelperGraphResolvedTarget;
struct FBlueprintHelperGraphSemanticContext;
struct FBlueprintHelperGraphSymbol;
struct FBlueprintHelperActionResolutionResult;
struct FBlueprintHelperNodeFragment;
struct FBlueprintHelperFragmentPinRef;
struct FBlueprintHelperFragmentLink;
struct FBlueprintHelperGraphFragmentBuildRequest;
struct FBlueprintHelperFieldFragmentPlan;
struct FBlueprintHelperActionNodeSpawnOptions;
struct FBlueprintHelperActionNodeSpawnContext;
struct FBlueprintHelperContainerActionSpec;
struct FBlueprintHelperEventDelegateUseSiteEvidence;
class FBlueprintHelperActionContextScope;
struct FBlueprintHelperActionResolutionRequest;
struct FBlueprintHelperEventDelegateBindingObjectResolution;
struct FBlueprintHelperDelegateLinkRequest;
struct FBlueprintHelperCallFunctionPinType;
struct FBlueprintHelperGraphComposeResult;
struct FBlueprintHelperGraphFragmentDataEdge;
struct FBlueprintHelperGraphFragmentEndpointRef;
struct FBlueprintHelperCandidateFunctionGroup;
struct FEdGraphPinType;
struct FBlueprintHelperGraphEventReference;

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteGraphStatementUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // -- From BlueprintHelperActionFragmentSpawnCoordinator.cpp --
    static void AddOwnershipTagIfPresent(
        FBlueprintHelperNodeFragment& Fragment,
        const FString& Key,
        const FString& Value);
    static void AppendResolvedActionCandidateFacts(
        const FBlueprintHelperActionResolutionResult& ActionResult,
        FBlueprintHelperNodeFragment& OutFragment);

    // -- From BlueprintHelperGraphSemanticIR.cpp --
    static bool IsBoolProducingOperator(const FString& Operator);
    static TSharedPtr<FBlueprintHelperGraphExpressionIR> FindFirstExpression(
        const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Expressions);
    static bool ContainerActionHasRole(
        const FBlueprintHelperContainerActionSpec& Spec,
        const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
        const TSharedPtr<FBlueprintHelperGraphExpressionIR>& TargetObject,
        const FString& Target,
        const FString& Role);
    static void ValidateContainerActionContract(
        FBlueprintHelperGraphSemanticIR& OutIR,
        const FString& Path,
        const FString& ContainerKind,
        const FString& ContainerOperation,
        const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
        const TSharedPtr<FBlueprintHelperGraphExpressionIR>& TargetObject,
        const FString& Target,
        const FString& ResultSymbolName,
        bool bExpression);
    static bool IsSupportedDelegateOperation(const FString& Operation);
    static bool DelegateOperationRequiresHandler(const FString& Operation);
    static bool StatementKindCanReturnGraphLocalValue(const EBlueprintHelperGraphStatementKind Kind);
    static bool StatementResultSymbolRequiresTypeEvidence(const EBlueprintHelperGraphStatementKind Kind);
    static FString ResolveStatementResultTypeToken(const FBlueprintHelperGraphStatementIR& Statement);
    static TSharedPtr<FBlueprintHelperGraphExpressionIR> MakeStatementResultSymbolExpression(
        const FBlueprintHelperGraphStatementIR& Statement,
        const FString& ResultType);
    static void AddCanonicalOpEvidenceAlias(
        TMap<FString, FString>& Evidence,
        const FString& CanonicalKey,
        const FString& LegacyKey);
    static FString ResolveCanonicalOpOperationId(
        const FString& FunctionOperation,
        const TMap<FString, FString>& Evidence,
        const FString& Operator);
    static void CanonicalizeOpExpressionEvidence(FBlueprintHelperGraphExpressionIR& Expression);
    static bool IsSupportedFieldOperation(const FString& Operation);
    static bool IsFieldSetOperation(const FString& Operation);
    static bool IsFieldReadOperation(const FString& Operation);
    static bool IsSupportedFieldScope(const FString& Scope);
    static bool IsPropertyFieldScope(const FString& Scope);
    static bool HasCreateTargetEvidence(
        const FString& CreateOperation,
        const FString& Target,
        const FString& ClassPath,
        const FString& AssetPath,
        const FString& PinType,
        const FString& KeyPinType,
        const FString& ValuePinType);
    static FString ReadOptionalJsonValueAsString(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName);
    static FString ReadFirstOptionalJsonValueAsString(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FirstFieldName,
        const TCHAR* SecondFieldName = nullptr,
        const TCHAR* ThirdFieldName = nullptr);
    static FGuid ReadOptionalGuidField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FirstFieldName,
        const TCHAR* SecondFieldName = nullptr);
    static void AddCapabilityFactIfPresent(
        TMap<FString, FString>& OutFacts,
        const FString& FactKey,
        const FString& Value);
    static void AddCapabilityFactIfPresent(
        TMap<FString, FString>& OutFacts,
        const FString& FactKey,
        const FGuid& Value);
    static void ReadOptionalStringMapField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName,
        TMap<FString, FString>& OutMap);
    static void ReadOptionalCapabilityFacts(
        const TSharedPtr<FJsonObject>& Object,
        TMap<FString, FString>& OutFacts);
    static void ParseLogicSpecEntry(
        const TSharedPtr<FJsonObject>& LogicSpecObject,
        FBlueprintHelperGraphSemanticIR& OutIR);

    // -- From BlueprintHelperEventDelegateFragmentBuilder.cpp --
    static FString GetEventDelegateStatementId(const FString& StatementId, const FString& Path);
    static EBlueprintHelperActionSemanticKind ToEventDelegateActionSemanticKind(
        const EBlueprintHelperGraphStatementKind StatementKind);
    static bool IsDelegateReferenceOperation(const FString& Operation);
    static void AddPinRefEventDelegate(
        FBlueprintHelperNodeFragment& Fragment,
        const FString& NodeId,
        const FString& Key,
        UEdGraphPin* Pin);
    static void PopulatePrimaryPins(UK2Node* PrimaryNode, FBlueprintHelperNodeFragment& OutFragment);
    static void PopulateCommonFragmentMetadata(
        const FString& StatementId,
        const FString& SemanticKind,
        const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
        FBlueprintHelperNodeFragment& OutFragment);
    static void CollectLiteralDefaultValuesEventDelegate(
        const FBlueprintHelperGraphStatementIR& Statement,
        TMap<FString, FString>& OutDefaultValues);
    static bool BuildActionRequestEventDelegate(
        UEdGraph* TargetGraph,
        const FBlueprintHelperActionContextScope* ActionContextScope,
        const FString& StatementId,
        FBlueprintHelperActionResolutionRequest& OutRequest,
        FString& OutError);
    static bool ResolveEventDelegateAction(
        const FBlueprintHelperActionResolutionRequest& Request,
        FBlueprintHelperActionResolutionResult& OutResult,
        FString& OutError);
    static UK2Node_AssignDelegate* SpawnAssignDelegateNodeManually(
        UEdGraph* TargetGraph,
        const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
        const FVector2D& Location,
        FString& OutError);
    static bool ConnectProjectedBindingObjectToPrimaryTarget(
        UEdGraph* TargetGraph,
        const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
        UK2Node* PrimaryNode,
        FBlueprintHelperNodeFragment& OutFragment,
        FString& OutError);
    static FString DescribePinCategory(const UEdGraphPin* Pin);
    static bool ValidateAndRecordDelegateCallArgs(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperGraphStatementIR& Statement,
        UK2Node* PrimaryNode,
        FBlueprintHelperNodeFragment& OutFragment,
        FString& OutError);
    static UK2Node* SpawnResolvedPrimaryNode(
        UEdGraph* TargetGraph,
        const FBlueprintHelperActionResolutionResult& ActionResult,
        const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
        const FBlueprintHelperGraphStatementIR& Statement,
        const FString& StatementId,
        FString& OutError);

    // -- From BlueprintHelperFieldFragmentBuilder.cpp --
    static bool BuildVariableFragment(
        UEdGraph* TargetGraph,
        const FBlueprintHelperGraphFragmentBuildRequest& Request,
        const FBlueprintHelperActionResolutionResult& ActionResult,
        FBlueprintHelperNodeFragment& OutFragment,
        FString& OutError);
    static FString FirstPlanFact(
        const FBlueprintHelperFieldFragmentPlan& Plan,
        const TArray<const TCHAR*>& Keys);
    static UScriptStruct* ResolveStructType(
        const FBlueprintHelperFieldFragmentPlan& Plan,
        FString& OutError);
    static void PopulateFieldPlanTags(
        const FBlueprintHelperFieldFragmentPlan& Plan,
        const FString& FragmentKind,
        FBlueprintHelperNodeFragment& OutFragment);
    static TArray<FString> GetPropertyPathSegments(const FBlueprintHelperFieldFragmentPlan& Plan);
    static FBlueprintHelperFragmentPinRef MakeFragmentPinRef(
        UEdGraphNode* Node,
        UEdGraphPin* Pin);
    static UEdGraphPin* FindDirectionalPin(
        UEdGraphNode* Node,
        const FString& PinName,
        const EEdGraphPinDirection Direction);
    static UEdGraphPin* FindStructInputPin(UEdGraphNode* Node);
    static void RestoreSetFieldsPinsIfNeeded(UEdGraphNode* Node);
    static void RestoreSetFieldsPinsIfNeeded(UK2Node_SetFieldsInStruct* Node);
    static bool BuildStructNodeFragment(
        UEdGraph* TargetGraph,
        const FBlueprintHelperFieldFragmentPlan& Plan,
        const FString& FragmentKind,
        bool bWrite,
        FBlueprintHelperNodeFragment& OutFragment,
        FString& OutError);
    static bool BuildNestedStructBreakPathFragment(
        UEdGraph* TargetGraph,
        const FBlueprintHelperFieldFragmentPlan& Plan,
        FBlueprintHelperNodeFragment& OutFragment,
        FString& OutError);

    // -- From BlueprintHelperSelectFragmentBuilder.cpp --
    static void SelectAddPinAlias(
        TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
        const FString& Alias,
        const FBlueprintHelperFragmentPinRef& PinRef);
    static bool TryBuildSelectPinType(const FString& TypeName, FEdGraphPinType& OutPinType);
    static FString GetExpressionEvidenceValue(
        const FBlueprintHelperGraphExpressionIR& Expression,
        const TCHAR* Key);
    static FString ResolveSelectResultTypeProof(const FBlueprintHelperGraphExpressionIR& Expression);
    static bool IsWildcardSelectTypeToken(const FString& TypeName);
    static bool ValidateSelectResultTypeProof(
        const FBlueprintHelperGraphExpressionIR& Expression,
        FEdGraphPinType& OutResultPinType,
        FString& OutError);
    static bool TryGetExpressionLiteral(
        const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
        FString& OutLiteral);
    static void ApplyIndexPinType(UK2Node_Select* SelectNode, const FBlueprintHelperGraphExpressionIR& Expression);
    static void ApplyResultPinType(UK2Node_Select* SelectNode, const FEdGraphPinType& PinType);
    static int32 GetDesiredOptionCount(const FBlueprintHelperGraphExpressionIR& Expression);
    static bool EnsureOptionPinCount(UK2Node_Select* SelectNode, int32 DesiredOptionCount, FString& OutError);
    static void CollectLiteralDefaultsSelect(
        UK2Node_Select* SelectNode,
        const FBlueprintHelperGraphExpressionIR& Expression,
        TMap<FString, FString>& InOutDefaults);
    static void PopulateSelectPins(UK2Node_Select* SelectNode, FBlueprintHelperNodeFragment& OutFragment);

    // -- From BlueprintHelperGraphFragmentBuilderRegistry.cpp --
    static FString GetStatementId(const FBlueprintHelperGraphStatementIR& Statement);
    static FString GetStatementContextId(const FBlueprintHelperGraphStatementIR& Statement);
    static void FillLiteralArgsAsDefaultsAndTypes(
        const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
        TMap<FString, FString>& OutDefaultValues,
        TMap<FString, FString>& OutArgumentTypes);

    // -- From BlueprintHelperGraphFragmentDag.cpp --
    static bool TryReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue);
    static bool TryReadBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool& OutValue);
    static EBlueprintHelperGraphFragmentPortDirection ParseFragmentPortDirection(const FString& Direction);

    // -- From BlueprintHelperDelegateLinkFragmentUtils.cpp --
    static FString MakeDelegatePinDiagnostic(
        const FString& DiagnosticPrefix,
        const TCHAR* CodeSuffix,
        const FString& Message);

    // -- From BlueprintHelperEventDelegateBindingObjectResolver.cpp --
    static FBlueprintHelperEventDelegateBindingObjectResolution Fail(const FString& Code);
    static UEdGraphPin* FindProjectedPin(
        const FBlueprintHelperNodeFragment& Fragment,
        const FString& EvidenceId);

    // -- From BlueprintHelperControlFragmentBuilder.cpp --
    static FString SanitizeFragmentIdPart(const FString& Value);
    static FString StatementKindName(const EBlueprintHelperGraphStatementKind Kind);
    static FString ResolveStatementFragmentId(const FBlueprintHelperGraphStatementIR& Statement);
    static void ControlAddPinAlias(
        TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
        const FString& Alias,
        UEdGraphPin* Pin);
    static void ControlAddExecPinAlias(
        FBlueprintHelperNodeFragment& Fragment,
        const FString& Alias,
        UEdGraphPin* Pin);
    static void ControlAddDataInputAlias(
        FBlueprintHelperNodeFragment& Fragment,
        const FString& Alias,
        UEdGraphPin* Pin);
    static UEdGraphPin* FindFirstExecPin(UK2Node* Node, const EEdGraphPinDirection Direction);
    static UEdGraphPin* FindFirstDataInputPin(UK2Node* Node);
    static void CollectExecOutputPins(UK2Node* Node, TArray<UEdGraphPin*>& OutPins);
    static bool ResolveControlActionProvider(
        UEdGraph* TargetGraph,
        const FBlueprintHelperActionContextScope* ActionContextScope,
        const FString& StatementContextId,
        const FString& ControlKind,
        const FString& FragmentId,
        FBlueprintHelperActionResolutionResult& OutResult,
        FString& OutError);
    static UK2Node* SpawnControlNodeThroughSpawner(
        UEdGraph* TargetGraph,
        const FBlueprintHelperActionResolutionResult& ActionResult,
        const FVector2D& Location,
        const FBlueprintHelperActionNodeSpawnOptions& SpawnOptions,
        FString& OutError);
    template <typename TNode>
    static TNode* SpawnTypedControlNode(
        UEdGraph* TargetGraph,
        const FBlueprintHelperActionResolutionResult& ActionResult,
        const FString& ControlKind,
        const FString& FragmentId,
        const FBlueprintHelperActionNodeSpawnOptions& SpawnOptions,
        FString& OutError)
    {
        UK2Node* SpawnedNode = SpawnControlNodeThroughSpawner(
            TargetGraph,
            ActionResult,
            FVector2D::ZeroVector,
            SpawnOptions,
            OutError);
        TNode* TypedNode = Cast<TNode>(SpawnedNode);
        if (!TypedNode && SpawnedNode)
        {
            OutError = FString::Printf(
                TEXT("control fragment build failed: spawner for '%s' created unexpected node class '%s'."),
                *ControlKind,
                *SpawnedNode->GetClass()->GetName());
        }
        return TypedNode;
    }
    template <typename TNode>
    static TNode* SpawnTypedControlNode(
        UEdGraph* TargetGraph,
        const FBlueprintHelperActionResolutionResult& ActionResult,
        const FString& ControlKind,
        const FString& FragmentId,
        FString& OutError)
    {
        FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
        SpawnOptions.NodeId = FragmentId;
        return SpawnTypedControlNode<TNode>(TargetGraph, ActionResult, ControlKind, FragmentId, SpawnOptions, OutError);
    }
    static void PopulateCommonControlMetadata(
        const FString& FragmentId,
        const FString& SourceStatementId,
        const FString& ControlKind,
        UK2Node* Node,
        FBlueprintHelperNodeFragment& OutFragment);
    static bool EnsureSequenceOutputCount(
        UK2Node_ExecutionSequence* SequenceNode,
        int32 DesiredOutputCount,
        TArray<UEdGraphPin*>& OutOutputPins,
        FString& OutError);
    static void PopulateSequencePins(
        UK2Node_ExecutionSequence* SequenceNode,
        const TArray<UEdGraphPin*>& OutputPins,
        FBlueprintHelperNodeFragment& OutFragment);
    static void PopulateBranchPins(
        UK2Node_IfThenElse* BranchNode,
        FBlueprintHelperNodeFragment& OutFragment);
    static void PopulateReturnPins(
        UK2Node_FunctionResult* ReturnNode,
        FBlueprintHelperNodeFragment& OutFragment);
    static void CollectBranchLiteralDefaults(
        const FBlueprintHelperGraphStatementIR& Statement,
        TMap<FString, FString>& OutDefaults);
    static void CollectReturnLiteralDefault(
        UK2Node_FunctionResult* ReturnNode,
        const FBlueprintHelperGraphStatementIR& Statement,
        TMap<FString, FString>& InOutDefaults);

    // -- From BlueprintHelperActionFragmentBuildUtils.cpp --
    static bool IsCallableTargetObjectPin(UK2Node* Node, UEdGraphPin* Pin);
    static void ActionBuildUtilsAddDataInputAlias(
        FBlueprintHelperNodeFragment& OutFragment,
        const FString& Alias,
        const FBlueprintHelperFragmentPinRef& PinRef);

    // -- From BlueprintHelperGraphComposer.cpp --
    static bool NodeHasWildcardPins(const UEdGraphNode* Node);
    static void AddNodeForDeferredReconstruct(UEdGraphPin* Pin, TSet<UEdGraphNode*>& NodesToReconstruct);
    static void ReconstructWildcardNodes(const TSet<UEdGraphNode*>& NodesToReconstruct);

    // -- From BlueprintHelperGraphStatementPinTypeParser.cpp --
    static void ApplyNamedPinTypePart(
        FBlueprintHelperCallFunctionPinType& PinType,
        const FString& Key,
        const FString& Value);

    // -- From BlueprintHelperGraphEventReferenceUtils.cpp --
    static FString ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName);
    static void ReadStringMapField(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName,
        TMap<FString, FString>& OutMap);
    static void AddMetadataIfPresent(
        TMap<FString, FString>& OutMetadata,
        const FString& Key,
        const FString& Value);
};

template <typename TNodeType>
static TNodeType* GraphWriteAddStructNode(UEdGraph* TargetGraph, UScriptStruct* StructType)
{
	if (!TargetGraph || !StructType)
	{
		return nullptr;
	}

	TNodeType* StructNode = NewObject<TNodeType>(TargetGraph);
	TargetGraph->AddNode(StructNode, true, false);
	StructNode->CreateNewGuid();
	StructNode->StructType = StructType;
	StructNode->PostPlacedNewNode();
	StructNode->AllocateDefaultPins();
	UGraphWriteGraphStatementUtils::RestoreSetFieldsPinsIfNeeded(StructNode);
	return StructNode;
}
