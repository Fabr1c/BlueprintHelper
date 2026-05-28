// ActionResolution 匿名命名空间函数提取 �?所有文件间共享的静态辅助函�?
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

#include "GraphWriteActionResolverUtils.generated.h"

enum class EBlueprintHelperFieldCapabilityRootKind : uint8;

class UEdGraphNode;
class UEdGraph;
class UBlueprint;
class UBlueprintNodeSpawner;
class UBlueprintFunctionNodeSpawner;
class UBlueprintFieldNodeSpawner;
class UK2Node_FunctionEntry;
class UFunction;
class UClass;
class UScriptStruct;
class UEnum;
class UActorComponent;
class UObject;
class FFieldVariant;

class FBlueprintHelperActionClusterContextView;
struct FBlueprintHelperCallFunctionCandidateInfo;
struct FBlueprintHelperActionResolutionResult;
struct FBlueprintHelperActionSemanticConstraints;
struct FBlueprintHelperGenericActionProviderBoundary;
struct FBlueprintHelperFieldCapabilitySpec;
struct FBlueprintHelperResolvedFieldPath;
struct FBlueprintHelperCallFunctionResolveRequest;
struct FBlueprintHelperCallFunctionResolveResult;
struct FBlueprintHelperSingletonControlFlowEvidence;
struct FBlueprintHelperProjectedScheduleActionEvidence;
struct FBlueprintHelperActionDatabaseProjectionEvidence;
struct FBlueprintHelperActionDatabaseProjectedCandidate;
struct FBlueprintHelperActionDatabaseProjectionResult;
struct FBlueprintHelperProjectedTypePromotionEvidence;
struct FEdGraphPinType;
struct FBPVariableDescription;

// ===== 从匿名命名空间提升的辅助结构�?=====

/** FieldVariableActionResolver 候�?*/
struct FBlueprintHelperVariableActionCandidate
{
    struct FBlueprintHelperCallFunctionCandidateInfo Info;
    TWeakObjectPtr<UBlueprintNodeSpawner> Spawner;
};

/** FieldVariableActionResolver 身份标识 */
struct FBlueprintHelperResolvedFieldIdentity
{
    FString CapabilityId;
    FString FieldKind;
    FString OwnerClassPath;
    FString MemberName;
    FGuid MemberGuid;
    FString LocalScopeName;
    FString FunctionName;
    FString TargetPinRef;
    FString TargetPinCategory;
    FString TargetPinObjectPath;
    FString ExpectedNodeFamily;
    FString ExpectedNodeClassPath;
    FString DiagnosticReason;
};

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteActionResolverUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ===== BlueprintHelperGenericActionProviderBoundary.cpp =====
    static FString BoundaryRequestEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key);
    static FString ControlOperation(const FBlueprintHelperActionResolutionRequest& Request);
    static bool IsSingletonControlOperation(const FString& Operation);
    static bool IsDedicatedControlFlowOperation(const FString& Operation);
    static bool IsStandardMacroControlOperation(const FString& Operation);
    static FBlueprintHelperGenericActionProviderBoundary MakeNeedsContext(
        const FString& RequiredBuilder, const FString& Reason);
    static FBlueprintHelperGenericActionProviderBoundary MakeDedicated(
        const FString& RequiredBuilder, const FString& Reason);

    // ===== BlueprintHelperFieldVariableActionResolver.cpp =====
    static FString NormalizeFieldVariableToken(const FString& Value);
    static FString NormalizeFieldBoundaryToken(const FString& Value);
    static FString FieldCapabilityRootKindToString(const EBlueprintHelperFieldCapabilityRootKind RootKind);
    static bool FieldCapabilityWrites(const struct FBlueprintHelperFieldCapabilitySpec& Spec);
    static const struct FBlueprintHelperFieldCapabilitySpec* ResolveFieldCapabilitySpecForRequest(
        const FBlueprintHelperActionResolutionRequest& Request);
    static bool IsFunctionScopedCapability(const struct FBlueprintHelperFieldCapabilitySpec& Spec);
    static FString DescribePinType(const FEdGraphPinType& PinType);
    static bool IsComponentRefFieldScope(const FString& FieldScope);
    static bool IsFieldAccessFieldScope(const FString& FieldScope);
    static FString GetEvidenceValue(const TMap<FString, FString>& Evidence, const TCHAR* Key);
    static void AddCapabilityFactIfPresent(
        FBlueprintHelperActionResolutionRequest& Request,
        const FString& Key, const FString& Value);
    static FString CapabilityFactOrEvidence(
        const FBlueprintHelperActionResolutionRequest& Request,
        const TMap<FString, FString>& Evidence,
        const FString& FactKey, const TCHAR* EvidenceKey);
    static FString CapabilityFactOrEvidence(
        const FBlueprintHelperActionResolutionRequest& Request,
        const TMap<FString, FString>& Evidence,
        const FString& FactKey,
        const TCHAR* FirstEvidenceKey, const TCHAR* SecondEvidenceKey);
    static void BackfillCapabilityFactsFromEvidence(
        FBlueprintHelperActionResolutionRequest& Request,
        const TMap<FString, FString>& Evidence);
    static FBlueprintHelperActionResolutionResult MakeFieldMissingEvidenceResult(
        const FString& Message, const FString& ErrorCode = TEXT("missing_required_evidence"));
    static FString FirstNonEmptyFieldValue(const FString& First, const FString& Second);
    static FString FirstNonEmptyFieldValue(const FString& First, const FString& Second, const FString& Third);
    static bool TypeMatches(
        const FString& ExpectedType,
        const struct FBlueprintHelperCallFunctionPinType& ExpectedPinType,
        const FEdGraphPinType& CandidateType);
    static UClass* ResolveOwnerClass(UBlueprint* Blueprint);
    static FString ResolveOwnerClassPath(const UBlueprint* Blueprint, const UClass* OwnerClass);
    static const FProperty* FindVariableProperty(UBlueprint* Blueprint, const FName VariableName);
    static UClass* FindClassByPath_FieldVarResolver(const FString& ClassPath);
    static const FProperty* FindPropertyOnClass(UClass* OwnerClass, const FName PropertyName);
    static const FProperty* ResolveFieldProperty(
        const FBlueprintHelperActionResolutionRequest& Request,
        const struct FBlueprintHelperFieldCapabilitySpec* CapabilitySpec,
        const FString& FieldName);
    static bool ResolveFunctionScope(
        const FBlueprintHelperActionResolutionRequest& Request,
        const TMap<FString, FString>& Evidence,
        const struct FBlueprintHelperFieldCapabilitySpec& Spec,
        FString& OutScopeName,
        FBlueprintHelperActionResolutionResult& OutResult);
    static FBPVariableDescription* FindLocalVariableDescription(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& FieldName,
        UK2Node_FunctionEntry** OutEntryNode = nullptr);
    static FProperty* ResolveLocalVariableProperty(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBPVariableDescription& LocalVariable);
    static UFunction* FindFunctionForGraph(UBlueprint* Blueprint, UEdGraph* FunctionGraph);
    static bool IsDisallowedFunctionParam(const FProperty* Param, const FString& ParamFlags);
    static FProperty* FindFunctionInputParameter(
        UFunction* Function,
        const FString& FieldName,
        const FString& ParamFlags,
        FString& OutErrorCode);
    static bool ConvertPropertyToPinType(const FProperty* Property, FEdGraphPinType& OutPinType);
    static int32 ScoreVariableCandidate(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBPVariableDescription& Variable);
    static FString MakeVariableStableId(
        const UBlueprint* Blueprint,
        const FString& FieldName,
        const FString& FieldOperation,
        const FString& FieldScope,
        const FString& Prefix = TEXT("field_variable"));
    static struct FBlueprintHelperCallFunctionCandidateInfo BuildCandidateInfo(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBPVariableDescription& Variable,
        const UClass* OwnerClass,
        const UClass* NodeClass,
        int32 Score);
    static void SortFieldVariableCandidates(TArray<struct FBlueprintHelperVariableActionCandidate>& Candidates);
    static TArray<struct FBlueprintHelperVariableActionCandidate> BuildProjectedFieldCandidates(
        const FBlueprintHelperActionResolutionRequest& Request,
        const UClass* OwnerClass,
        const UClass* NodeClass);
    static void SetFieldCandidateDiagnostics(
        FBlueprintHelperActionResolutionResult& Result,
        const TArray<struct FBlueprintHelperVariableActionCandidate>& Candidates,
        int32 MaxCandidates);
    static bool ResolveFieldIdentity(
        const FBlueprintHelperActionResolutionRequest& Request,
        const struct FBlueprintHelperFieldCapabilitySpec& Spec,
        const FString& FieldName,
        const FProperty* ResolvedProperty,
        const UClass* ResolvedOwnerClass,
        struct FBlueprintHelperResolvedFieldIdentity& OutIdentity);
    static void ApplyResolvedFieldIdentityToCandidate(
        const struct FBlueprintHelperResolvedFieldIdentity& Identity,
        struct FBlueprintHelperCallFunctionCandidateInfo& Candidate);

    // ===== BlueprintHelperFieldPathResolution.cpp =====
    static FString CleanLower(const FString& Value);
    static FString EvidenceValue(const TMap<FString, FString>& Evidence, const TCHAR* Key);
    static void SetInvalid(FBlueprintHelperResolvedFieldPath& Result, const FString& Code, const FString& Message);
    static void PopulateSegments(FBlueprintHelperResolvedFieldPath& Result);
    static FString ResolveOwnerEvidence(const FBlueprintHelperActionResolutionRequest& Request, const TMap<FString, FString>& Evidence);
    static FString DescribePinTypeEvidence(const struct FBlueprintHelperCallFunctionPinType& PinType);
    static FString ComposePropertyFullPath(const FBlueprintHelperActionResolutionRequest& Request, const TMap<FString, FString>& Evidence);

    // ===== BlueprintHelperGenericAssetStructControlActionResolver.cpp =====
    static bool IsGenericNodeSpawnerSemantic(EBlueprintHelperActionSemanticKind Kind);
    static FString ControlRequestEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key);
    static FString ResolveControlOperation(const FBlueprintHelperActionResolutionRequest& Request);
    static FBlueprintHelperActionResolutionResult MakeUnsupportedGenericSemanticResult(
        const FBlueprintHelperActionResolutionRequest& Request, const FString& Message);
    static FBlueprintHelperActionResolutionResult MakeInvalidGenericNodeSpawnerResult(
        const FString& ErrorCode, const FString& Message);
    static FBlueprintHelperActionResolutionResult MakeBlockedGenericNodeSpawnerResult(
        const FString& ErrorCode, const FString& Message);
    static TArray<FString> ParseDelimitedEvidenceList(const FString& Value);
    static void AddGenericOpsReadbackFacts(
        struct FBlueprintHelperCallFunctionCandidateInfo& Candidate,
        const FString& Family, const FString& Operation,
        const TMap<FString, FString>& ExtraFacts);
    static FBlueprintHelperActionResolutionResult MakeResolvedGenericNodeSpawnerResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& Family, const FString& Operation, const FString& StableEvidence,
        TSubclassOf<UEdGraphNode> NodeClass,
        UBlueprintNodeSpawner* Spawner,
        const FString& Category, const FString& MatchReason,
        const TMap<FString, FString>& ExtraFacts);
    static UBlueprintNodeSpawner* CreateSwitchEnumSpawner(UEnum* Enum);
    static UBlueprintNodeSpawner* CreateGenericControlSpawner(TSubclassOf<UEdGraphNode> ResolvedNodeClass);
    static UBlueprintNodeSpawner* CreateMacroInstanceSpawner(UEdGraph* MacroGraph);
    static UEnum* ResolveEnumEvidence(const FBlueprintHelperActionResolutionRequest& Request);
    static UEdGraph* ResolveMacroGraphEvidence(const FBlueprintHelperActionResolutionRequest& Request);
    static FBlueprintHelperActionResolutionResult ResolveDedicatedControlFlowNodeSpawner(
        const FBlueprintHelperActionResolutionRequest& Request);
    static bool IsStandardMacroOperation(const FString& Operation);
    static FBlueprintHelperActionResolutionResult ResolveStandardMacroNodeSpawner(
        const FBlueprintHelperActionResolutionRequest& Request);
    static FBlueprintHelperActionResolutionResult ResolveSingletonControlFlowNodeSpawner(
        const FBlueprintHelperActionResolutionRequest& Request);

    // ===== BlueprintHelperFunctionSemanticActionResolver.cpp =====
    static EBlueprintHelperActionResolutionStatus MapFunctionResolveStatus(EBlueprintHelperCallFunctionResolveStatus Status);
    static FString GetDefaultValue(const FBlueprintHelperActionSemanticConstraints& Semantic, const TCHAR* Key);
    static FString GetFunctionOperation(const FBlueprintHelperActionSemanticConstraints& Semantic);
    static bool HasTypedArgumentPinEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic);
    static bool IsTrueEvidence(const TMap<FString, FString>& Evidence, const TCHAR* Key);
    static void PopulateCallContext(
        struct FBlueprintHelperCallFunctionResolveRequest& CallRequest,
        const FBlueprintHelperActionResolutionRequest& Request);
    static FBlueprintHelperActionResolutionResult MakeInvalidRequestResult(
        const FString& ErrorCode, const FString& Message);
    static FBlueprintHelperActionResolutionResult ResolveViaCallFunctionResolver(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperActionSemanticConstraints& Semantic);

    // ===== BlueprintHelperGenericCreateActionResolver.cpp =====
    static FString NormalizeCreateOperation(const FString& Operation);
    static FString DescribePinType(const struct FBlueprintHelperCallFunctionPinType& PinType);
    static FString ResolveContainerElementEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic);
    static FString ResolveContainerKeyEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic);
    static FString ResolveContainerValueEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic);
    static bool IsFunctionBackedCreateOperation(const FString& Operation);
    static UClass* ResolveCreateWidgetNodeClass();
    static FBlueprintHelperActionResolutionResult MakeResolvedCreateResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& Operation,
        UClass* ResolvedNodeClass,
        const FString& Evidence,
        const FString& ReturnType);

    // ===== BlueprintHelperGenericTransformScheduleActionResolver.cpp =====
    static UClass* ResolveTargetClass(const FString& ClassEvidence);
    static FBlueprintHelperActionResolutionResult MakeScheduleInvalidResult(
        const TCHAR* ErrorCode, const FString& Message);
    static bool IsTimerOperation(const FString& Operation);
    static bool IsLatentOrAsyncOperation(const FString& Operation);
    static bool IsFunctionBackedTransformOperation(const FString& Operation);
    static bool IsFunctionBackedScheduleOperation(const FString& Operation);
    static struct FBlueprintHelperActionDatabaseProjectionEvidence ToProjectionEvidence(
        const struct FBlueprintHelperProjectedScheduleActionEvidence& Evidence);
    static struct FBlueprintHelperCallFunctionCandidateInfo MakeScheduleCandidateInfo(
        const struct FBlueprintHelperActionDatabaseProjectedCandidate& Match,
        const FString& Operation);
    static FBlueprintHelperActionResolutionResult MakeResolvedTransformResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& Operation, const FString& ClassEvidence,
        UClass* TargetClass, TSubclassOf<UEdGraphNode> ResolvedNodeClass);
    static FBlueprintHelperActionResolutionResult ResolveConvert(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperActionClusterContextView& Context);
    static FBlueprintHelperActionResolutionResult ResolveSchedule(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperActionClusterContextView& Context);

    // ===== BlueprintHelperSingletonControlFlowEvidenceProvider.cpp =====
    static const TCHAR* SingletonKindToStableName(EBlueprintHelperSingletonControlFlowKind Kind);
    static const TCHAR* SingletonKindToDisplayName(EBlueprintHelperSingletonControlFlowKind Kind);
    static FString SingletonKindToQuery(EBlueprintHelperSingletonControlFlowKind Kind);
    static EBlueprintHelperActionSemanticKind SingletonKindToSemanticKind(EBlueprintHelperSingletonControlFlowKind Kind);
    static void AppendObjectIdentity(FString& Stable, const TCHAR* Label, const UObject* Object);
    static FString BuildSingletonCanonicalStableFields(
        EBlueprintHelperSingletonControlFlowKind Kind,
        EBlueprintHelperActionSemanticKind SemanticKind,
        UBlueprint* Blueprint,
        UEdGraph* TargetGraph,
        const FString& StatementId,
        const FString& Query);
    static bool MakeEvidence(
        EBlueprintHelperSingletonControlFlowKind Kind,
        TSubclassOf<UEdGraphNode> NodeClass,
        struct FBlueprintHelperSingletonControlFlowEvidence& OutEvidence);

    // ===== BlueprintHelperStructTypeStructureActionResolver.cpp =====
    static bool IsStructTypeStructureRequest(const FBlueprintHelperActionResolutionRequest& Request);
    static bool IsConstructOperation(const FBlueprintHelperActionResolutionRequest& Request);
    static FString NormalizeNativeFunctionPath(const FString& RawPath);
    static FString NormalizeStructLookupText(const FString& TypeName);
    static UScriptStruct* ResolveKnownStructAlias(const FString& TypeName);
    static UScriptStruct* ResolveStructType(const FString& TypeName);
    static FString SemanticTypeFromProperty(const FProperty* Property);
    static void AddUniqueQuery(TArray<FString>& Queries, const FString& Query);
    static void AddUniqueCandidateInfo(
        TArray<struct FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
        const struct FBlueprintHelperCallFunctionCandidateInfo& Candidate);
    static void AppendCandidateDiagnostics(
        TArray<struct FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
        const FBlueprintHelperActionResolutionResult& FunctionResult,
        int32 MaxCandidates);
    static FProperty* FindStructPropertyBySemanticName(const UScriptStruct* TargetStruct, const FString& Name);
    static void PopulateFunctionArgumentConstraintsFromStruct(
        const UScriptStruct* TargetStruct,
        const FBlueprintHelperActionSemanticConstraints& SourceSemantic,
        FBlueprintHelperActionSemanticConstraints& FunctionSemantic);
    static struct FBlueprintHelperActionResolutionRequest MakeFunctionActionRequest(
        const FBlueprintHelperActionResolutionRequest& Request,
        UScriptStruct* TargetStruct,
        const FString& Query,
        const FString& SearchMode,
        bool bConstruct);
    static void PopulateStructTypeStructureEvidence(
        FBlueprintHelperActionResolutionResult& InOutResult,
        const FBlueprintHelperActionResolutionRequest& Request,
        const UScriptStruct* TargetStruct,
        const FString& MatchReason,
        const UClass* NodeClass);
    static bool TryResolveFunctionActionSpawner(
        const FBlueprintHelperActionResolutionRequest& Request,
        UScriptStruct* TargetStruct,
        const FString& Query,
        const FString& SearchMode,
        bool bConstruct,
        const FString& AttemptLabel,
        TArray<FString>& AttemptMessages,
        TArray<struct FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
        FBlueprintHelperActionResolutionResult& OutResult);
    static UFunction* ResolveNativeStructFunction(const FString& NativePath);
    static struct FBlueprintHelperCallFunctionCandidateInfo MakeNativeStructFunctionCandidateInfo(
        const UFunction* Function,
        bool bConstruct,
        const UScriptStruct* TargetStruct);
    static bool TryResolveNativeStructFunctionSpawner(
        const FString& NativeFunctionPath,
        const FBlueprintHelperActionResolutionRequest& Request,
        UScriptStruct* TargetStruct,
        bool bConstruct,
        TArray<struct FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
        FBlueprintHelperActionResolutionResult& OutResult);
    static void SetStructTypeOnNode(UEdGraphNode* NewNode, class FFieldVariant StructField, TWeakObjectPtr<UScriptStruct> StructPtr);
    static UBlueprintFieldNodeSpawner* CreateDirectStructSpawner(UClass* NodeClass, UScriptStruct* TargetStruct);
    static FString MakeDirectStructStableId(EBlueprintHelperTypeOperation Operation, const UScriptStruct* TargetStruct);
    static struct FBlueprintHelperCallFunctionCandidateInfo MakeDirectStructCandidateInfo(
        const FBlueprintHelperActionResolutionRequest& Request,
        const UScriptStruct* TargetStruct,
        const UClass* NodeClass);
    static FBlueprintHelperActionResolutionResult MakeDirectStructSpawnerResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        UScriptStruct* TargetStruct,
        const TArray<FString>& AttemptMessages,
        const TArray<struct FBlueprintHelperCallFunctionCandidateInfo>& FunctionCandidateActions);
    static FBlueprintHelperActionResolutionResult MakeNeedsContextResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& Message);
    static FBlueprintHelperActionResolutionResult MakeStructTypeNotFoundResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& Message);
    static FBlueprintHelperActionResolutionResult MakeInvalidSemanticResult(
        const FBlueprintHelperActionResolutionRequest& Request);

    // ===== BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp =====
    static FString NormalizeTypePromotionToken(const FString& Value);
    static bool TryBuildPrimitivePinType(const FString& TypeToken, FEdGraphPinType& OutPinType);
    static bool IsPromotionCompatible(const FEdGraphPinType& SourcePinType, const FEdGraphPinType& TargetPinType);
    static UBlueprintFunctionNodeSpawner* FindRegisteredTypePromotionSpawner(FName OperatorName);
    static FBlueprintHelperActionResolutionResult MakeNotFoundResult(
        const struct FBlueprintHelperProjectedTypePromotionEvidence& Evidence);
    static struct FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(
        const FString& StableId,
        const struct FBlueprintHelperProjectedTypePromotionEvidence& Evidence,
        UBlueprintFunctionNodeSpawner* Spawner);
};
