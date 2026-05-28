#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

#include "BlueprintBoundEventNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_RemoveDelegate.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/BlueprintHelperGraphActionUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"

// ============================================================================
// BlueprintHelperGenericAssetStructControlActionCluster.cpp
// ============================================================================

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeNeedsMoreSemanticContextResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperGenericActionProviderBoundary& Boundary)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode =
        (Request.Semantic.Kind == EBlueprintHelperActionSemanticKind::Construct
            || Request.Semantic.Kind == EBlueprintHelperActionSemanticKind::Deconstruct)
        ? TEXT("missing_required_evidence")
        : TEXT("needs_more_semantic_context");
    Result.Message = Boundary.Reason;
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeUnsupportedProviderBoundaryResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperGenericActionProviderBoundary& Boundary)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("unsupported_generic_action_provider_boundary");
    Result.Message = FString::Printf(
        TEXT("%s Semantic kind: '%s'."),
        *Boundary.Reason,
        *FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
    return Result;
}

bool UGraphWriteActionClusterUtils::IsStructTypeStructureOperation(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    const bool bStructFamily = Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::Struct
        || Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::TypeStructure;
    const bool bTypeOperation = Semantic.TypeOperation == EBlueprintHelperTypeOperation::Construct
        || Semantic.TypeOperation == EBlueprintHelperTypeOperation::Deconstruct;
    return bStructFamily && bTypeOperation;
}

// ============================================================================
// BlueprintHelperEventDelegateActionCluster.cpp
// ============================================================================

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeMissingEvidenceResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& MissingDetail,
    const FString& Message)
{
    const FString MissingRequiredEvidenceBoundary = TEXT("missing_required_evidence");
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
    Result.ErrorCode = MissingDetail;
    Result.Message = FString::Printf(TEXT("%s: %s: %s"), *MissingRequiredEvidenceBoundary, *MissingDetail, *Message);
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeEventDelegateBlockedResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& ErrorCode,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
    Result.ErrorCode = ErrorCode;
    Result.Message = Message;
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakePolicyResult(
    const FBlueprintHelperEventDelegatePolicyDecision& Decision)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = Decision.Status;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
    Result.ErrorCode = Decision.ErrorCode;
    Result.Message = Decision.Message;
    return Result;
}

bool UGraphWriteActionClusterUtils::DelegateOperationRequiresHandler(const FString& Operation)
{
    return Operation.Equals(TEXT("bind"), ESearchCase::IgnoreCase)
        || Operation.Equals(TEXT("assign"), ESearchCase::IgnoreCase)
        || Operation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase);
}

TSubclassOf<UK2Node_BaseMCDelegate> UGraphWriteActionClusterUtils::DelegateNodeClassForOperation(const FString& Operation)
{
    if (Operation.Equals(TEXT("bind"), ESearchCase::IgnoreCase))
    {
        return UK2Node_AddDelegate::StaticClass();
    }
    if (Operation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
    {
        return UK2Node_AssignDelegate::StaticClass();
    }
    if (Operation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase))
    {
        return UK2Node_RemoveDelegate::StaticClass();
    }
    if (Operation.Equals(TEXT("call"), ESearchCase::IgnoreCase))
    {
        return UK2Node_CallDelegate::StaticClass();
    }
    if (Operation.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
    {
        return UK2Node_ClearDelegate::StaticClass();
    }
    return nullptr;
}

FString UGraphWriteActionClusterUtils::MakeComponentBoundEventStableId(const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
    return FString::Printf(
        TEXT("component_bound_event:%s:%s"),
        *Evidence.DelegatePropertyPath,
        *Evidence.ComponentBindingFieldPath);
}

FString UGraphWriteActionClusterUtils::MakeDelegateStableId(const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
    FString StableId = FString::Printf(
        TEXT("delegate:%s:%s"),
        *Evidence.DelegateOperation,
        *Evidence.DelegatePropertyPath);
    if (DelegateOperationRequiresHandler(Evidence.DelegateOperation))
    {
        StableId += FString::Printf(TEXT(":%s"), *Evidence.HandlerName);
    }
    return StableId;
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionClusterUtils::MakeEventDelegateCandidateInfo(
    const FString& StableId,
    const FString& DisplayName,
    UClass* NodeClass,
    const FString& MatchReason)
{
    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = StableId;
    Candidate.DisplayName = DisplayName;
    Candidate.Category = TEXT("EventDelegate");
    Candidate.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
    Candidate.MatchReason = MatchReason;
    Candidate.Score = 100;
    Candidate.bGraphCompatible = NodeClass != nullptr;
    Candidate.bFromActionDatabase = true;
    Candidate.bBlueprintCallable = true;
    Candidate.bBlueprintPure = false;
    return Candidate;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeResolvedEventDelegateResult(
    const FString& StableId,
    UBlueprintNodeSpawner* Spawner,
    UClass* NodeClass,
    const FString& DisplayName,
    const FString& MatchReason,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
    Result.Message = Message;
    Result.SelectedStableId = StableId;
    Result.SelectedSpawner = Spawner;
    Result.CandidateActions.Add(MakeEventDelegateCandidateInfo(StableId, DisplayName, NodeClass, MatchReason));
    return Result;
}

FString UGraphWriteActionClusterUtils::EventDelegateEvidenceValue(
    const FBlueprintHelperActionResolutionRequest& Request,
    const TCHAR* Key)
{
    if (const FString* Value = Request.ContextEvidence.Find(Key))
    {
        return Value->TrimStartAndEnd();
    }
    return FString();
}

bool UGraphWriteActionClusterUtils::ShouldReturnExistingBinding(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
    FString& OutExistingBindingId)
{
    if (!Evidence.DuplicatePolicy.Equals(TEXT("return_existing"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    OutExistingBindingId = EventDelegateEvidenceValue(Request, TEXT("event_delegate.existing_binding_evidence_id"));
    return !OutExistingBindingId.IsEmpty();
}

// ============================================================================
// BlueprintHelperContainerActionResolver.cpp
// ============================================================================

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeInvalid(const FString& Code, const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
    Result.ErrorCode = Code;
    Result.Message = Message;
    return Result;
}

FString UGraphWriteActionClusterUtils::NormalizeRole(const FString& Role)
{
    return Role.TrimStartAndEnd().ToLower();
}

FString UGraphWriteActionClusterUtils::ExtractStableFunctionName(const FString& StableFunctionPath)
{
    int32 ColonIndex = INDEX_NONE;
    if (StableFunctionPath.FindLastChar(TEXT(':'), ColonIndex)
        && ColonIndex > 0
        && ColonIndex < StableFunctionPath.Len() - 1)
    {
        return StableFunctionPath.Mid(ColonIndex + 1).TrimStartAndEnd();
    }
    return StableFunctionPath.TrimStartAndEnd();
}

FString UGraphWriteActionClusterUtils::ResultKindToString(const EBlueprintHelperContainerActionResultKind ResultKind)
{
    switch (ResultKind)
    {
    case EBlueprintHelperContainerActionResultKind::ReturnValue:
        return TEXT("return_value");
    case EBlueprintHelperContainerActionResultKind::OutputPins:
        return TEXT("output_pins");
    case EBlueprintHelperContainerActionResultKind::None:
    default:
        return TEXT("none");
    }
}

FString UGraphWriteActionClusterUtils::ContainerActionPermittedNodeClassPaths()
{
    return TEXT("/Script/BlueprintGraph.K2Node_CallFunction;/Script/BlueprintGraph.K2Node_CallArrayFunction");
}

bool UGraphWriteActionClusterUtils::HasArgumentName(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
    return Semantic.ArgumentNames.ContainsByPredicate(
        [&Role](const FString& Candidate)
        {
            return Candidate.Equals(Role, ESearchCase::IgnoreCase);
        });
}

bool UGraphWriteActionClusterUtils::HasRoleTypeEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
    if (Semantic.ArgumentTypes.Contains(Role) || Semantic.ArgumentPinTypes.Contains(Role))
    {
        return true;
    }

    return false;
}

bool UGraphWriteActionClusterUtils::HasRoleEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
    const FString NormalizedRole = NormalizeRole(Role);
    if (NormalizedRole == TEXT("target"))
    {
        return !Semantic.TargetPath.IsEmpty()
            || !Semantic.TargetObjectType.IsEmpty()
            || Semantic.TargetObjectPinType.IsValid()
            || HasArgumentName(Semantic, Role)
            || Semantic.DefaultValues.Contains(Role);
    }

    return HasArgumentName(Semantic, Role)
        || Semantic.DefaultValues.Contains(Role)
        || HasRoleTypeEvidence(Semantic, Role);
}

bool UGraphWriteActionClusterUtils::HasContainerTargetTypeEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    const FString Kind = NormalizeRole(Semantic.ContainerKind);
    if (Kind == TEXT("map"))
    {
        return (!Semantic.KeyType.IsEmpty() && !Semantic.ValueType.IsEmpty())
            || (Semantic.ContainerKeyPinType.IsValid() && Semantic.ContainerValuePinType.IsValid())
            || Semantic.ArgumentPinTypes.Contains(TEXT("target"));
    }

    return !Semantic.ElementType.IsEmpty()
        || Semantic.ContainerElementPinType.IsValid()
        || Semantic.ArgumentPinTypes.Contains(TEXT("target"));
}

bool UGraphWriteActionClusterUtils::HasTypedRoleEvidence(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
    const FString NormalizedRole = NormalizeRole(Role);
    if (NormalizedRole == TEXT("target"))
    {
        return HasContainerTargetTypeEvidence(Semantic);
    }
    if (NormalizedRole == TEXT("item") || NormalizedRole == TEXT("items") || NormalizedRole == TEXT("other") || NormalizedRole == TEXT("result"))
    {
        return !Semantic.ElementType.IsEmpty()
            || Semantic.ContainerElementPinType.IsValid()
            || Semantic.ArgumentTypes.Contains(TEXT("element"))
            || Semantic.ArgumentPinTypes.Contains(TEXT("element"))
            || Semantic.ArgumentTypes.Contains(Role)
            || Semantic.ArgumentPinTypes.Contains(Role);
    }
    if (NormalizedRole == TEXT("key"))
    {
        return !Semantic.KeyType.IsEmpty()
            || Semantic.ContainerKeyPinType.IsValid()
            || Semantic.ArgumentTypes.Contains(Role)
            || Semantic.ArgumentPinTypes.Contains(Role);
    }
    if (NormalizedRole == TEXT("value"))
    {
        return !Semantic.ValueType.IsEmpty()
            || Semantic.ContainerValuePinType.IsValid()
            || Semantic.ArgumentTypes.Contains(Role)
            || Semantic.ArgumentPinTypes.Contains(Role);
    }

    return true;
}

bool UGraphWriteActionClusterUtils::TryValidateRequiredRoles(
    const FBlueprintHelperContainerActionSpec& Spec,
    const FBlueprintHelperActionSemanticConstraints& Semantic,
    FString& OutMissingRole)
{
    OutMissingRole.Reset();
    for (const FString& RequiredRole : Spec.RequiredRoles)
    {
        if (!HasRoleEvidence(Semantic, RequiredRole))
        {
            OutMissingRole = RequiredRole;
            return false;
        }
    }
    return true;
}

bool UGraphWriteActionClusterUtils::TryValidateTypedRoleEvidence(
    const FBlueprintHelperContainerActionSpec& Spec,
    const FBlueprintHelperActionSemanticConstraints& Semantic,
    FString& OutMissingRole)
{
    OutMissingRole.Reset();
    for (const FString& TypedRole : Spec.WildcardPolicy.TypedRoles)
    {
        if (!HasTypedRoleEvidence(Semantic, TypedRole))
        {
            OutMissingRole = TypedRole;
            return false;
        }
    }
    return true;
}

FString UGraphWriteActionClusterUtils::RoleType(const FBlueprintHelperActionSemanticConstraints& Semantic, const FString& Role)
{
    const FString NormalizedRole = NormalizeRole(Role);
    if (NormalizedRole == TEXT("item") || NormalizedRole == TEXT("items") || NormalizedRole == TEXT("other") || NormalizedRole == TEXT("result"))
    {
        if (!Semantic.ElementType.IsEmpty())
        {
            return Semantic.ElementType;
        }
        if (const FString* ElementType = Semantic.ArgumentTypes.Find(TEXT("element")))
        {
            return *ElementType;
        }
        if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
        {
            return *Type;
        }
        return FString();
    }
    if (NormalizedRole == TEXT("key"))
    {
        if (!Semantic.KeyType.IsEmpty())
        {
            return Semantic.KeyType;
        }
        if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
        {
            return *Type;
        }
        return FString();
    }
    if (NormalizedRole == TEXT("value"))
    {
        if (!Semantic.ValueType.IsEmpty())
        {
            return Semantic.ValueType;
        }
        if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
        {
            return *Type;
        }
        return FString();
    }
    if (const FString* Type = Semantic.ArgumentTypes.Find(Role))
    {
        return *Type;
    }
    return FString();
}

FBlueprintHelperCallFunctionPinType UGraphWriteActionClusterUtils::RolePinType(
    const FBlueprintHelperActionSemanticConstraints& Semantic,
    const FString& Role)
{
    const FString NormalizedRole = NormalizeRole(Role);
    if (NormalizedRole == TEXT("target"))
    {
        return FBlueprintHelperCallFunctionPinType();
    }

    if (const FBlueprintHelperCallFunctionPinType* PinType = Semantic.ArgumentPinTypes.Find(Role))
    {
        return *PinType;
    }
    if (NormalizedRole == TEXT("item") || NormalizedRole == TEXT("items"))
    {
        if (const FBlueprintHelperCallFunctionPinType* ElementPinType = Semantic.ArgumentPinTypes.Find(TEXT("element")))
        {
            return *ElementPinType;
        }
        return Semantic.ContainerElementPinType;
    }
    if (NormalizedRole == TEXT("key"))
    {
        return Semantic.ContainerKeyPinType;
    }
    if (NormalizedRole == TEXT("value"))
    {
        return Semantic.ContainerValuePinType;
    }
    return FBlueprintHelperCallFunctionPinType();
}

bool UGraphWriteActionClusterUtils::IsWildcardTypedRole(const FBlueprintHelperContainerActionSpec& Spec, const FString& Role)
{
    return Spec.WildcardPolicy.TypedRoles.ContainsByPredicate(
        [&Role](const FString& TypedRole)
        {
            return NormalizeRole(TypedRole).Equals(NormalizeRole(Role), ESearchCase::IgnoreCase);
        });
}

void UGraphWriteActionClusterUtils::ProjectRoleConstraintsToFunctionPins(
    const FBlueprintHelperContainerActionSpec& Spec,
    const FBlueprintHelperActionSemanticConstraints& SourceSemantic,
    FBlueprintHelperActionSemanticConstraints& InOutFunctionSemantic)
{
    InOutFunctionSemantic.ArgumentNames.Reset();
    InOutFunctionSemantic.ArgumentTypes.Reset();
    InOutFunctionSemantic.ArgumentPinTypes.Reset();

    for (const FBlueprintHelperContainerActionRoleBinding& Binding : Spec.RoleBindings)
    {
        if (!Binding.bProjectToCallableRequest
            || Binding.FunctionPinName.IsEmpty()
            || !HasRoleEvidence(SourceSemantic, Binding.RoleName))
        {
            continue;
        }

        InOutFunctionSemantic.ArgumentNames.AddUnique(Binding.FunctionPinName);
        if (IsWildcardTypedRole(Spec, Binding.RoleName))
        {
            continue;
        }

        const FString Type = RoleType(SourceSemantic, Binding.RoleName);
        if (!Type.IsEmpty())
        {
            InOutFunctionSemantic.ArgumentTypes.Add(Binding.FunctionPinName, Type);
        }
        const FBlueprintHelperCallFunctionPinType PinType = RolePinType(SourceSemantic, Binding.RoleName);
        if (PinType.IsValid())
        {
            InOutFunctionSemantic.ArgumentPinTypes.Add(Binding.FunctionPinName, PinType);
        }
    }
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::ResolveInternal(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const FBlueprintHelperContainerActionSpec* Spec =
        FBlueprintHelperContainerActionVocabulary::Find(Request.Semantic.ContainerKind, Request.Semantic.ContainerOperation);
    if (!Spec)
    {
        return MakeInvalid(TEXT("unsupported_container_operation"), TEXT("Unsupported container_action operation."));
    }

    FString MissingRole;
    if (!TryValidateRequiredRoles(*Spec, Request.Semantic, MissingRole))
    {
        return MakeInvalid(
            TEXT("container_action_role_missing"),
            FString::Printf(TEXT("container_action %s requires role '%s'."), *Spec->OperationId, *MissingRole));
    }
    if (!TryValidateTypedRoleEvidence(*Spec, Request.Semantic, MissingRole))
    {
        return MakeInvalid(
            TEXT("container_action_type_evidence_missing"),
            FString::Printf(TEXT("container_action %s requires typed callable evidence for role '%s'."), *Spec->OperationId, *MissingRole));
    }

    FBlueprintHelperActionResolutionRequest FunctionRequest = Request;
    FunctionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
    FunctionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Call;
    FunctionRequest.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
    FunctionRequest.Semantic.FunctionOperation = TEXT("container_action");
    FunctionRequest.Semantic.Query = ExtractStableFunctionName(Spec->StableUFunctionPath);
    FunctionRequest.Semantic.StableId = Spec->StableUFunctionPath;
    FunctionRequest.Semantic.CapabilityFacts.FindOrAdd(TEXT("container.stable_ufunction_path")) = Spec->StableUFunctionPath;
    FunctionRequest.Semantic.CapabilityFacts.FindOrAdd(TEXT("function.permitted_node_class_paths")) = ContainerActionPermittedNodeClassPaths();
    if (FunctionRequest.Semantic.SearchMode.IsEmpty())
    {
        FunctionRequest.Semantic.SearchMode = TEXT("exact");
    }
    FunctionRequest.Semantic.DefaultValues.Add(TEXT("container.stable_ufunction_path"), Spec->StableUFunctionPath);
    FunctionRequest.Semantic.DefaultValues.Add(TEXT("function.permitted_node_class_paths"), ContainerActionPermittedNodeClassPaths());
    ProjectRoleConstraintsToFunctionPins(*Spec, Request.Semantic, FunctionRequest.Semantic);
    FunctionRequest.Semantic.CategoryPriority.AddUnique(TEXT("Utilities|Array"));
    FunctionRequest.Semantic.CategoryPriority.AddUnique(TEXT("Utilities|Map"));
    FunctionRequest.Semantic.CategoryPriority.AddUnique(TEXT("Utilities|Set"));
    FunctionRequest.Semantic.TargetPath.Reset();
    FunctionRequest.Semantic.TargetObjectType.Reset();
    FunctionRequest.Semantic.TargetObjectPinType = FBlueprintHelperCallFunctionPinType();
    FunctionRequest.Semantic.DefaultValues.Add(TEXT("container.result_kind"), ResultKindToString(Spec->ResultKind));
    FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation_id"), Spec->OperationId);
    FunctionRequest.ContextEvidence.Add(TEXT("container_action_kind"), Spec->ContainerKind);
    FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation"), Spec->ContainerOperation);
    FunctionRequest.ContextEvidence.Add(TEXT("container.stable_ufunction_path"), Spec->StableUFunctionPath);

    const FBlueprintHelperActionClusterContextView FunctionContext(FunctionRequest);
    FString ContextErrorCode;
    FString ContextMessage;
    if (!FunctionContext.IsCompleteForCluster(EBlueprintHelperSpawnerClusterKind::FunctionAction, ContextErrorCode, ContextMessage))
    {
        FBlueprintHelperActionResolutionResult Result = MakeInvalid(ContextErrorCode, ContextMessage);
        Result.MatchReason = Spec->OperationId;
        return Result;
    }

    FBlueprintHelperActionResolutionResult Result =
        FBlueprintHelperFunctionActionCluster::Resolve(FunctionRequest, FunctionContext);
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
    if (Result.IsResolved())
    {
        Result.MatchReason = Spec->OperationId + TEXT(" via ") + Result.MatchReason;
    }
    return Result;
}

// ============================================================================
// BlueprintHelperGenericAssetActionResolver.cpp
// ============================================================================

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeClusterInvalidResult(const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("needs_more_semantic_context");
    Result.Message = Message;
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeNotFoundResult(
    const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("asset_action_spawner_not_found");
    Result.Message = Message;
    Result.MatchReason = FString::Printf(
        TEXT("asset_action_spawner_not_found stable_id=%s owner=%s node=%s signature=%s query=%s menu=%s category=%s"),
        Evidence.StableId.IsEmpty() ? TEXT("none") : *Evidence.StableId,
        Evidence.OwnerPath.IsEmpty() ? TEXT("none") : *Evidence.OwnerPath,
        Evidence.NodeClassPath.IsEmpty() ? TEXT("none") : *Evidence.NodeClassPath,
        Evidence.SpawnerSignature.IsEmpty() ? TEXT("none") : *Evidence.SpawnerSignature,
        Evidence.Query.IsEmpty() ? TEXT("none") : *Evidence.Query,
        Evidence.MenuName.IsEmpty() ? TEXT("none") : *Evidence.MenuName,
        Evidence.Category.IsEmpty() ? TEXT("none") : *Evidence.Category);
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeAmbiguousResult(
    const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
    int32 MatchCount)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("asset_action_spawner_ambiguous");
    Result.Message = FString::Printf(
        TEXT("asset_action projected evidence matched %d ActionDatabase spawners. Provide asset_action_stable_id or stronger projected evidence. query=%s node=%s owner=%s"),
        MatchCount,
        Evidence.Query.IsEmpty() ? TEXT("none") : *Evidence.Query,
        Evidence.NodeClassPath.IsEmpty() ? TEXT("none") : *Evidence.NodeClassPath,
        Evidence.OwnerPath.IsEmpty() ? TEXT("none") : *Evidence.OwnerPath);
    Result.MatchReason = FString::Printf(
        TEXT("asset_action_spawner_ambiguous count=%d query=%s node=%s owner=%s"),
        MatchCount,
        Evidence.Query.IsEmpty() ? TEXT("none") : *Evidence.Query,
        Evidence.NodeClassPath.IsEmpty() ? TEXT("none") : *Evidence.NodeClassPath,
        Evidence.OwnerPath.IsEmpty() ? TEXT("none") : *Evidence.OwnerPath);
    return Result;
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionClusterUtils::MakeCandidateInfo(
    const FBlueprintHelperAssetActionProjectedCandidate& Match)
{
    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = Match.StableId;
    Candidate.DisplayName = Match.MenuName.IsEmpty() ? TEXT("asset_action") : Match.MenuName;
    Candidate.Category = Match.Category;
    Candidate.NodeClassPath = Match.NodeClassPath;
    Candidate.MatchReason = FString::Printf(
        TEXT("action_database owner=%s node=%s menu=%s"),
        Match.OwnerPath.IsEmpty() ? TEXT("none") : *Match.OwnerPath,
        Match.NodeClassPath.IsEmpty() ? TEXT("none") : *Match.NodeClassPath,
        Match.MenuName.IsEmpty() ? TEXT("none") : *Match.MenuName);
    Candidate.Score = 100;
    Candidate.bGraphCompatible = Match.Spawner != nullptr;
    Candidate.bFromActionDatabase = true;
    return Candidate;
}

// ============================================================================
// BlueprintHelperOperatorActionResolver.cpp
// ============================================================================

FString UGraphWriteActionClusterUtils::MakePromotableOperatorStableId(FName OpName)
{
    return FString::Printf(TEXT("promotable_operator:%s"), *OpName.ToString());
}

FString UGraphWriteActionClusterUtils::GetOperatorTokenFromContext(const FBlueprintHelperActionClusterContextView& Context)
{
    const FString SemanticQuery = Context.GetSemantic().Query.TrimStartAndEnd();
    if (!SemanticQuery.IsEmpty())
    {
        return SemanticQuery;
    }

    static const TCHAR* EvidenceKeys[] =
    {
        TEXT("operator_token"),
        TEXT("operator"),
        TEXT("op"),
        TEXT("op_name")
    };

    for (const TCHAR* EvidenceKey : EvidenceKeys)
    {
        if (const FString* EvidenceValue = Context.GetRequest().ContextEvidence.Find(EvidenceKey))
        {
            const FString Trimmed = EvidenceValue->TrimStartAndEnd();
            if (!Trimmed.IsEmpty())
            {
                return Trimmed;
            }
        }
    }

    return FString();
}

FString UGraphWriteActionClusterUtils::GetRequestedOpOperationId(const FBlueprintHelperActionResolutionRequest& Request)
{
    const FString FunctionOperation = Request.Semantic.FunctionOperation.TrimStartAndEnd();
    if (FunctionOperation.StartsWith(TEXT("op."), ESearchCase::IgnoreCase))
    {
        return FBlueprintHelperOpCallableCatalog::NormalizeOperationId(FunctionOperation);
    }

    if (const FString* OperationId = Request.ContextEvidence.Find(TEXT("op.operation_id")))
    {
        return FBlueprintHelperOpCallableCatalog::NormalizeOperationId(*OperationId);
    }

    return FString();
}

EBlueprintHelperActionResolutionStatus UGraphWriteActionClusterUtils::MapOperatorFunctionResolveStatus(EBlueprintHelperCallFunctionResolveStatus Status)
{
    switch (Status)
    {
    case EBlueprintHelperCallFunctionResolveStatus::Resolved:
        return EBlueprintHelperActionResolutionStatus::Resolved;
    case EBlueprintHelperCallFunctionResolveStatus::Ambiguous:
        return EBlueprintHelperActionResolutionStatus::Ambiguous;
    case EBlueprintHelperCallFunctionResolveStatus::Blocked:
        return EBlueprintHelperActionResolutionStatus::Blocked;
    case EBlueprintHelperCallFunctionResolveStatus::NotFound:
    default:
        return EBlueprintHelperActionResolutionStatus::NotFound;
    }
}

void UGraphWriteActionClusterUtils::ApplyArrayIdenticalEvidence(
    const FBlueprintHelperOpCallableEvidence& Evidence,
    FBlueprintHelperCallFunctionResolveRequest& CallRequest)
{
    if (!Evidence.OperationId.Equals(TEXT("array_identical"), ESearchCase::IgnoreCase))
    {
        return;
    }

    FBlueprintHelperCallFunctionPinType LhsPinType =
        FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Evidence.Facts.FindRef(TEXT("op.array_lhs_pin_type")));
    FBlueprintHelperCallFunctionPinType RhsPinType =
        FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Evidence.Facts.FindRef(TEXT("op.array_rhs_pin_type")));
    if (LhsPinType.IsValid() && LhsPinType.ContainerType.IsEmpty())
    {
        LhsPinType.ContainerType = TEXT("array");
    }
    if (RhsPinType.IsValid() && RhsPinType.ContainerType.IsEmpty())
    {
        RhsPinType.ContainerType = TEXT("array");
    }

    CallRequest.ArgumentNames = { TEXT("ArrayA"), TEXT("ArrayB") };
    CallRequest.ArgumentPinTypes.Add(TEXT("ArrayA"), LhsPinType);
    CallRequest.ArgumentPinTypes.Add(TEXT("ArrayB"), RhsPinType);
    CallRequest.Context.ArgumentNames = CallRequest.ArgumentNames;
    CallRequest.Context.ArgumentPinTypes = CallRequest.ArgumentPinTypes;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionClusterUtils::MakeCallableOpResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperOpCallableEvidence& Evidence,
    const FBlueprintHelperCallFunctionResolveResult& CallResult)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = MapOperatorFunctionResolveStatus(CallResult.Status);
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
    Result.ErrorCode = CallResult.ErrorCode;
    Result.Message = CallResult.Message;
    Result.SelectedStableId = CallResult.Selected.StableId;
    Result.SelectedSpawner = CallResult.Selected.NodeSpawner;
    Result.SelectedFunction = CallResult.Selected.Function;
    Result.CandidateActions = CallResult.CandidateFunctions;
    Result.FunctionCandidate = CallResult.Selected;
    for (FBlueprintHelperCallFunctionCandidateInfo& CandidateInfo : Result.CandidateActions)
    {
        CandidateInfo.CapabilityId = TEXT("op_coverage");
        CandidateInfo.ExpectedNodeFamily = Evidence.Spec.SpawnFamily;
        CandidateInfo.ExpectedNodeClassPath = Evidence.Spec.RequiredNodeClassPath;
        for (const TPair<FString, FString>& FactPair : Evidence.Facts)
        {
            CandidateInfo.CapabilityFacts.FindOrAdd(FactPair.Key, FactPair.Value);
            CandidateInfo.ReadbackFacts.FindOrAdd(FactPair.Key, FactPair.Value);
        }
        CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.operation_id"), Evidence.OperationId);
        CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.source_function_path"), Evidence.Spec.StableCallableId);
        CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.node_class_path"), CandidateInfo.NodeClassPath);
        CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.wildcard_residual"), TEXT("false"));
        if (Result.SelectedSpawner.IsValid())
        {
            CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.spawner_class"), Result.SelectedSpawner->GetClass()->GetPathName());
        }
    }
    if (Result.IsResolved())
    {
        Result.Message = FString::Printf(
            TEXT("Resolved op.%s to callable %s through FunctionActionCluster policy."),
            *Evidence.OperationId,
            *Evidence.Spec.StableCallableId);
    }
    return Result;
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionClusterUtils::MakePromotableOperatorCandidateInfo(
    FName OpName,
    UBlueprintFunctionNodeSpawner* Spawner)
{
    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = MakePromotableOperatorStableId(OpName);
    Candidate.DisplayName = OpName.ToString();
    Candidate.Category = TEXT("Utilities|Operators");
    Candidate.NodeClassPath = UK2Node_PromotableOperator::StaticClass()->GetPathName();
    Candidate.MatchReason = TEXT("ue_promotable_operator_spawner");
    Candidate.CapabilityId = TEXT("op_coverage");
    Candidate.ExpectedNodeFamily = TEXT("type_promotion");
    Candidate.ExpectedNodeClassPath = UK2Node_PromotableOperator::StaticClass()->GetPathName();
    Candidate.ReadbackFacts.Add(TEXT("op.type_promotion_operator"), OpName.ToString());
    Candidate.ReadbackFacts.Add(TEXT("op.node_class_path"), Candidate.NodeClassPath);
    Candidate.ReadbackFacts.Add(TEXT("op.wildcard_residual"), TEXT("false"));
    if (Spawner)
    {
        Candidate.ReadbackFacts.Add(TEXT("op.spawner_class"), Spawner->GetClass()->GetPathName());
    }
    Candidate.Score = 100;
    Candidate.bGraphCompatible = Spawner != nullptr;
    Candidate.bFromActionDatabase = true;
    Candidate.bBlueprintCallable = true;
    Candidate.bBlueprintPure = true;
    return Candidate;
}

// ============================================================================
// BlueprintHelperFieldActionReadback.cpp
// ============================================================================

void UGraphWriteActionClusterUtils::AddFactIfPresent(
    TMap<FString, FString>& OutFacts,
    const FString& Key,
    const FString& Value)
{
    const FString CleanValue = Value.TrimStartAndEnd();
    if (!Key.IsEmpty() && !CleanValue.IsEmpty())
    {
        OutFacts.Add(Key, CleanValue);
    }
}

// ============================================================================
// BlueprintHelperActionResolutionCore.cpp
// ============================================================================

FString UGraphWriteActionClusterUtils::NormalizeOperationToken(const FString& Operation)
{
    return Operation.TrimStartAndEnd().ToLower();
}

bool UGraphWriteActionClusterUtils::IsGenericTransformOperation(const FString& Operation)
{
    const FString Normalized = NormalizeOperationToken(Operation);
    return Normalized == TEXT("dynamic_cast")
        || Normalized == TEXT("class_cast")
        || Normalized == TEXT("type_promotion");
}

bool UGraphWriteActionClusterUtils::IsGenericCreateOperation(const FString& Operation)
{
    const FString Normalized = NormalizeOperationToken(Operation);
    return Normalized == TEXT("spawn_actor")
        || Normalized == TEXT("create_widget")
        || Normalized == TEXT("construct_object")
        || Normalized == TEXT("make_array")
        || Normalized == TEXT("make_map")
        || Normalized == TEXT("make_set")
        || Normalized == TEXT("asset_action");
}

bool UGraphWriteActionClusterUtils::IsGenericScheduleOperation(const FString& Operation)
{
    const FString Normalized = NormalizeOperationToken(Operation);
    return Normalized == TEXT("timer_delegate_node")
        || Normalized == TEXT("latent_or_async_node");
}

bool UGraphWriteActionClusterUtils::HasAmbiguousGenericFunctionOwner(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    if (Semantic.FunctionOperation.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }

    if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Convert)
    {
        return IsGenericTransformOperation(Semantic.TransformOperation);
    }

    if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Schedule)
    {
        return IsGenericScheduleOperation(Semantic.ScheduleOperation);
    }

    if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Create)
    {
        return IsGenericCreateOperation(Semantic.CreateOperation);
    }

    return false;
}

// ============================================================================
// BlueprintHelperOpCallableCatalog.cpp
// ============================================================================

FString UGraphWriteActionClusterUtils::StableCallableId(const TCHAR* OwnerClassPath, const TCHAR* FunctionName)
{
    return FString::Printf(TEXT("%s:%s"), OwnerClassPath, FunctionName);
}

FBlueprintHelperOpCallableSpec UGraphWriteActionClusterUtils::MakeSpec(
    const TCHAR* OperationId,
    const TCHAR* SpawnFamily,
    const TCHAR* OwnerClassPath,
    const TCHAR* FunctionName,
    const TCHAR* RequiredNodeClassPath)
{
    FBlueprintHelperOpCallableSpec Spec;
    Spec.OperationId = OperationId;
    Spec.SpawnFamily = SpawnFamily;
    Spec.StableCallableId = StableCallableId(OwnerClassPath, FunctionName);
    Spec.RequiredNodeClassPath = RequiredNodeClassPath;
    return Spec;
}

const TCHAR* UGraphWriteActionClusterUtils::CommutativeOperatorNodeClassPath()
{
    return TEXT("/Script/BlueprintGraph.K2Node_CommutativeAssociativeBinaryOperator");
}

FBlueprintHelperOpCallableSpec UGraphWriteActionClusterUtils::MakeArrayIdenticalSpec()
{
    FBlueprintHelperOpCallableSpec Spec = MakeSpec(
        TEXT("array_identical"),
        TEXT("special_node"),
        TEXT("/Script/Engine.KismetArrayLibrary"),
        TEXT("Array_Identical"),
        TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction"));
    Spec.RequiredEvidenceKeys = { TEXT("op.array_lhs_pin_type"), TEXT("op.array_rhs_pin_type") };
    return Spec;
}

FBlueprintHelperOpCallableSpec UGraphWriteActionClusterUtils::MakeRejectedSpec(const TCHAR* OperationId)
{
    FBlueprintHelperOpCallableSpec Spec;
    Spec.OperationId = OperationId;
    Spec.RejectionCode = TEXT("excluded_op_operation");
    return Spec;
}

const TArray<FBlueprintHelperOpCallableSpec>& UGraphWriteActionClusterUtils::SupportedSpecs()
{
    static const TArray<FBlueprintHelperOpCallableSpec> Specs = {
        MakeSpec(TEXT("bitwise_and"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("And_IntInt"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("bitwise_or"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Or_IntInt"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("boolean_and"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanAND"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("boolean_or"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanOR"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("boolean_nand"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanNAND"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("max"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("FMax"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("min"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("FMin"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("string_append"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetStringLibrary"), TEXT("Concat_StrStr"), CommutativeOperatorNodeClassPath()),
        MakeSpec(TEXT("boolean_not"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Not_PreBool")),
        MakeSpec(TEXT("boolean_xor"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanXOR")),
        MakeSpec(TEXT("boolean_nor"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanNOR")),
        MakeSpec(TEXT("bitwise_not"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Not_Int")),
        MakeSpec(TEXT("bitwise_xor"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Xor_IntInt")),
        MakeSpec(TEXT("abs"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Abs")),
        MakeSpec(TEXT("modulo"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Percent_FloatFloat")),
        MakeSpec(TEXT("negate"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NegateVector")),
        MakeSpec(TEXT("dot"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("DotProduct2D")),
        MakeSpec(TEXT("dot3"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Dot_VectorVector")),
        MakeSpec(TEXT("cross"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("CrossProduct2D")),
        MakeSpec(TEXT("cross3"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Cross_VectorVector")),
        MakeSpec(TEXT("near_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NearlyEqual_FloatFloat")),
        MakeSpec(TEXT("intpoint_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Equal_IntPointIntPoint")),
        MakeSpec(TEXT("transform_compose"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("ComposeTransforms")),
        MakeSpec(TEXT("equal_exact"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("EqualExactly_VectorVector")),
        MakeSpec(TEXT("not_equal_exact"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NotEqualExactly_VectorVector")),
        MakeSpec(TEXT("equal_ignore_case"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetStringLibrary"), TEXT("EqualEqual_StriStri")),
        MakeSpec(TEXT("not_equal_ignore_case"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetStringLibrary"), TEXT("NotEqual_StriStri")),
        MakeSpec(TEXT("datetime_add_datetime"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Add_DateTimeDateTime")),
        MakeSpec(TEXT("datetime_add_timespan"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Add_DateTimeTimespan")),
        MakeSpec(TEXT("datetime_subtract_datetime"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Subtract_DateTimeDateTime")),
        MakeSpec(TEXT("datetime_subtract_timespan"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Subtract_DateTimeTimespan")),
        MakeSpec(TEXT("datetime_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("EqualEqual_DateTimeDateTime")),
        MakeSpec(TEXT("datetime_not_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NotEqual_DateTimeDateTime")),
        MakeSpec(TEXT("datetime_greater"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Greater_DateTimeDateTime")),
        MakeSpec(TEXT("datetime_greater_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("GreaterEqual_DateTimeDateTime")),
        MakeSpec(TEXT("datetime_less"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Less_DateTimeDateTime")),
        MakeSpec(TEXT("datetime_less_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("LessEqual_DateTimeDateTime")),
        MakeArrayIdenticalSpec()
    };
    return Specs;
}

const TArray<FBlueprintHelperOpCallableSpec>& UGraphWriteActionClusterUtils::ExcludedSpecs()
{
    static const TArray<FBlueprintHelperOpCallableSpec> Specs = {
        MakeRejectedSpec(TEXT("enum_equal")),
        MakeRejectedSpec(TEXT("enum_not_equal")),
        MakeRejectedSpec(TEXT("slate_brush_equal")),
        MakeRejectedSpec(TEXT("slate_brush_not_equal")),
        MakeRejectedSpec(TEXT("convert_numeric")),
        MakeRejectedSpec(TEXT("convert_string_text_name")),
        MakeRejectedSpec(TEXT("array_map_set_mutation")),
        MakeRejectedSpec(TEXT("validity_predicate"))
    };
    return Specs;
}
