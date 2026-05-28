// ActionResolution 集群匿名命名空间函数提取 �?GraphWrite 集群相关的静态辅助函�?// �?8 �?ActionResolution/*.cpp 文件中提取的 anonymous namespace 函数

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"

#include "GraphWriteActionClusterUtils.generated.h"

class UBlueprintNodeSpawner;
class UBlueprintFunctionNodeSpawner;
class UK2Node_BaseMCDelegate;
class UClass;

struct FBlueprintHelperGenericActionProviderBoundary;
struct FBlueprintHelperEventDelegatePolicyDecision;
struct FBlueprintHelperEventDelegateUseSiteEvidence;
struct FBlueprintHelperOpCallableEvidence;
struct FBlueprintHelperProjectedAssetActionEvidence;
struct FBlueprintHelperAssetActionProjectedCandidate;
class FBlueprintHelperActionClusterContextView;

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteActionClusterUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ===== BlueprintHelperGenericAssetStructControlActionCluster.cpp =====
    static FBlueprintHelperActionResolutionResult MakeNeedsMoreSemanticContextResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperGenericActionProviderBoundary& Boundary);

    static FBlueprintHelperActionResolutionResult MakeUnsupportedProviderBoundaryResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperGenericActionProviderBoundary& Boundary);

    static bool IsStructTypeStructureOperation(const FBlueprintHelperActionSemanticConstraints& Semantic);

    // ===== BlueprintHelperEventDelegateActionCluster.cpp =====
    static FBlueprintHelperActionResolutionResult MakeMissingEvidenceResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& MissingDetail,
        const FString& Message);

    static FBlueprintHelperActionResolutionResult MakeEventDelegateBlockedResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FString& ErrorCode,
        const FString& Message);

    static FBlueprintHelperActionResolutionResult MakePolicyResult(
        const FBlueprintHelperEventDelegatePolicyDecision& Decision);

    static bool DelegateOperationRequiresHandler(const FString& Operation);

    static TSubclassOf<UK2Node_BaseMCDelegate> DelegateNodeClassForOperation(const FString& Operation);

    static FString MakeComponentBoundEventStableId(const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence);

    static FString MakeDelegateStableId(const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence);

    static FBlueprintHelperCallFunctionCandidateInfo MakeEventDelegateCandidateInfo(
        const FString& StableId,
        const FString& DisplayName,
        UClass* NodeClass,
        const FString& MatchReason);

    static FBlueprintHelperActionResolutionResult MakeResolvedEventDelegateResult(
        const FString& StableId,
        UBlueprintNodeSpawner* Spawner,
        UClass* NodeClass,
        const FString& DisplayName,
        const FString& MatchReason,
        const FString& Message);

    static FString EventDelegateEvidenceValue(
        const FBlueprintHelperActionResolutionRequest& Request,
        const TCHAR* Key);

    static bool ShouldReturnExistingBinding(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
        FString& OutExistingBindingId);

    // ===== BlueprintHelperContainerActionResolver.cpp =====
    static FBlueprintHelperActionResolutionResult MakeInvalid(const FString& Code, const FString& Message);

    static FString NormalizeRole(const FString& Role);

    static FString ExtractStableFunctionName(const FString& StableFunctionPath);

    static FString ResultKindToString(const EBlueprintHelperContainerActionResultKind ResultKind);

    static FString ContainerActionPermittedNodeClassPaths();

    static bool HasArgumentName(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role);

    static bool HasRoleTypeEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role);

    static bool HasRoleEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role);

    static bool HasContainerTargetTypeEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic);

    static bool HasTypedRoleEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role);

    static bool TryValidateRequiredRoles(
        const FBlueprintHelperContainerActionSpec& Spec,
        const FBlueprintHelperActionSemanticConstraints& Semantic,
        FString& OutMissingRole);

    static bool TryValidateTypedRoleEvidence(
        const FBlueprintHelperContainerActionSpec& Spec,
        const FBlueprintHelperActionSemanticConstraints& Semantic,
        FString& OutMissingRole);

    static FString RoleType(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role);

    static FBlueprintHelperCallFunctionPinType RolePinType(
        const FBlueprintHelperActionSemanticConstraints& Semantic,
        const FString& Role);

    static bool IsWildcardTypedRole(const FBlueprintHelperContainerActionSpec& Spec, const FString& Role);

    static void ProjectRoleConstraintsToFunctionPins(
        const FBlueprintHelperContainerActionSpec& Spec,
        const FBlueprintHelperActionSemanticConstraints& SourceSemantic,
        FBlueprintHelperActionSemanticConstraints& InOutFunctionSemantic);

    static FBlueprintHelperActionResolutionResult ResolveInternal(
        const FBlueprintHelperActionResolutionRequest& Request);

    // ===== BlueprintHelperGenericAssetActionResolver.cpp =====
    static FBlueprintHelperActionResolutionResult MakeClusterInvalidResult(const FString& Message);

    static FBlueprintHelperActionResolutionResult MakeNotFoundResult(
        const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
        const FString& Message);

    static FBlueprintHelperActionResolutionResult MakeAmbiguousResult(
        const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
        int32 MatchCount);

    static FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(
        const FBlueprintHelperAssetActionProjectedCandidate& Match);

    // ===== BlueprintHelperOperatorActionResolver.cpp =====
    static FString MakePromotableOperatorStableId(FName OpName);

    static FString GetOperatorTokenFromContext(const FBlueprintHelperActionClusterContextView& Context);

    static FString GetRequestedOpOperationId(const FBlueprintHelperActionResolutionRequest& Request);

    static EBlueprintHelperActionResolutionStatus MapOperatorFunctionResolveStatus(EBlueprintHelperCallFunctionResolveStatus Status);

    static void ApplyArrayIdenticalEvidence(
        const FBlueprintHelperOpCallableEvidence& Evidence,
        FBlueprintHelperCallFunctionResolveRequest& CallRequest);

    static FBlueprintHelperActionResolutionResult MakeCallableOpResult(
        const FBlueprintHelperActionResolutionRequest& Request,
        const FBlueprintHelperOpCallableEvidence& Evidence,
        const FBlueprintHelperCallFunctionResolveResult& CallResult);

    static FBlueprintHelperCallFunctionCandidateInfo MakePromotableOperatorCandidateInfo(
        FName OpName,
        UBlueprintFunctionNodeSpawner* Spawner);

    // ===== BlueprintHelperFieldActionReadback.cpp =====
    static void AddFactIfPresent(TMap<FString, FString>& OutFacts, const FString& Key, const FString& Value);

    // ===== BlueprintHelperActionResolutionCore.cpp =====
    static FString NormalizeOperationToken(const FString& Operation);

    static bool IsGenericTransformOperation(const FString& Operation);

    static bool IsGenericCreateOperation(const FString& Operation);

    static bool IsGenericScheduleOperation(const FString& Operation);

    static bool HasAmbiguousGenericFunctionOwner(const FBlueprintHelperActionSemanticConstraints& Semantic);

    // ===== BlueprintHelperOpCallableCatalog.cpp =====
    static FString StableCallableId(const TCHAR* OwnerClassPath, const TCHAR* FunctionName);

    static FBlueprintHelperOpCallableSpec MakeSpec(
        const TCHAR* OperationId,
        const TCHAR* SpawnFamily,
        const TCHAR* OwnerClassPath,
        const TCHAR* FunctionName,
        const TCHAR* RequiredNodeClassPath = TEXT(""));

    static const TCHAR* CommutativeOperatorNodeClassPath();

    static FBlueprintHelperOpCallableSpec MakeArrayIdenticalSpec();

    static FBlueprintHelperOpCallableSpec MakeRejectedSpec(const TCHAR* OperationId);

    static const TArray<FBlueprintHelperOpCallableSpec>& SupportedSpecs();

    static const TArray<FBlueprintHelperOpCallableSpec>& ExcludedSpecs();
};
