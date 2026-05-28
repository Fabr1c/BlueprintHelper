// GraphWriteActionResolverUtils 实现 �?所有文件间共享的匿名命名空间函数提�?
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

// === 核心 ActionResolution 类型 ===
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformSpawnerFactory.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/BlueprintHelperGraphActionUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h"

// === FunctionResolution ===
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

// === GraphStatement/Utils ===
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEvidenceWrappers.h"

// === UE 引擎包含 ===
#include "BlueprintActionDatabase.h"
#include "BlueprintFieldNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "BlueprintVariableNodeSpawner.h"
#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/MemberReference.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_GenericCreateObject.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_MultiGate.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Select.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_StructOperation.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchString.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"

// ============================================================================
// BlueprintHelperGenericActionProviderBoundary.cpp
// ============================================================================

FString UGraphWriteActionResolverUtils::BoundaryRequestEvidenceValue(
    const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key)
{
    if (const FString* Value = Request.ContextEvidence.Find(Key))
    {
        return Clean(*Value);
    }
    if (const FString* Value = Request.Semantic.DefaultValues.Find(Key))
    {
        return Clean(*Value);
    }
    return FString();
}

FString UGraphWriteActionResolverUtils::ControlOperation(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    return NormalizeOperation(
        !BoundaryRequestEvidenceValue(Request, TEXT("generic.control.operation")).IsEmpty()
            ? BoundaryRequestEvidenceValue(Request, TEXT("generic.control.operation"))
            : Request.Semantic.Query);
}

bool UGraphWriteActionResolverUtils::IsSingletonControlOperation(const FString& Operation)
{
    return Operation == TEXT("branch")
        || Operation == TEXT("sequence")
        || Operation == TEXT("return");
}

bool UGraphWriteActionResolverUtils::IsDedicatedControlFlowOperation(const FString& Operation)
{
    return Operation == TEXT("switch_int")
        || Operation == TEXT("switch_string")
        || Operation == TEXT("switch_name")
        || Operation == TEXT("switch_enum")
        || Operation == TEXT("multi_gate");
}

bool UGraphWriteActionResolverUtils::IsStandardMacroControlOperation(const FString& Operation)
{
    return Operation == TEXT("do_once")
        || Operation == TEXT("do_n")
        || Operation == TEXT("gate")
        || Operation == TEXT("flip_flop")
        || Operation == TEXT("for_loop")
        || Operation == TEXT("for_loop_with_break")
        || Operation == TEXT("foreach_loop")
        || Operation == TEXT("foreach_loop_with_break")
        || Operation == TEXT("while_loop");
}

FBlueprintHelperGenericActionProviderBoundary UGraphWriteActionResolverUtils::MakeNeedsContext(
    const FString& RequiredBuilder,
    const FString& Reason)
{
    FBlueprintHelperGenericActionProviderBoundary Boundary;
    Boundary.Mode = EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext;
    Boundary.RequiredBuilder = RequiredBuilder;
    Boundary.Reason = Reason;
    return Boundary;
}

FBlueprintHelperGenericActionProviderBoundary UGraphWriteActionResolverUtils::MakeDedicated(
    const FString& RequiredBuilder,
    const FString& Reason)
{
    FBlueprintHelperGenericActionProviderBoundary Boundary;
    Boundary.Mode = EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired;
    Boundary.RequiredBuilder = RequiredBuilder;
    Boundary.Reason = Reason;
    return Boundary;
}

// ============================================================================
// BlueprintHelperFieldVariableActionResolver.cpp
// ============================================================================

FString UGraphWriteActionResolverUtils::NormalizeFieldVariableToken(const FString& Value)
{
    FString Normalized = Value.TrimStartAndEnd().ToLower();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    return Normalized;
}

FString UGraphWriteActionResolverUtils::NormalizeFieldBoundaryToken(const FString& Value)
{
    return Value.TrimStartAndEnd().ToLower();
}

FString UGraphWriteActionResolverUtils::FieldCapabilityRootKindToString(
    const EBlueprintHelperFieldCapabilityRootKind RootKind)
{
    switch (RootKind)
    {
    case EBlueprintHelperFieldCapabilityRootKind::Member:
        return TEXT("member");
    case EBlueprintHelperFieldCapabilityRootKind::InheritedMember:
        return TEXT("inherited_member");
    case EBlueprintHelperFieldCapabilityRootKind::SparseData:
        return TEXT("sparse_data");
    case EBlueprintHelperFieldCapabilityRootKind::FunctionParam:
        return TEXT("function_param");
    case EBlueprintHelperFieldCapabilityRootKind::Local:
        return TEXT("local");
    case EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember:
        return TEXT("object_pin_member");
    case EBlueprintHelperFieldCapabilityRootKind::ComponentRef:
        return TEXT("component_ref");
    case EBlueprintHelperFieldCapabilityRootKind::ComponentProperty:
        return TEXT("component_property");
    case EBlueprintHelperFieldCapabilityRootKind::StructMember:
        return TEXT("struct_member");
    case EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath:
        return TEXT("nested_property_path");
    default:
        return TEXT("unsupported");
    }
}

bool UGraphWriteActionResolverUtils::FieldCapabilityWrites(
    const FBlueprintHelperFieldCapabilitySpec& Spec)
{
    return Spec.AccessMode == EBlueprintHelperFieldCapabilityAccessMode::Set;
}

const FBlueprintHelperFieldCapabilitySpec* UGraphWriteActionResolverUtils::ResolveFieldCapabilitySpecForRequest(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    if (!Request.Semantic.CapabilityId.TrimStartAndEnd().IsEmpty())
    {
        return FBlueprintHelperFieldCapabilityRegistry::FindById(Request.Semantic.CapabilityId);
    }

    if (const FBlueprintHelperFieldCapabilitySpec* InferredSpec =
        FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(
            Request.Semantic.FieldOperation,
            Request.Semantic.FieldScope))
    {
        if (!InferredSpec->bRequiresTargetPin
            || !Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref")).TrimStartAndEnd().IsEmpty())
        {
            return InferredSpec;
        }
    }

    const FString Operation = NormalizeFieldBoundaryToken(Request.Semantic.FieldOperation);
    const FString Scope = NormalizeFieldBoundaryToken(Request.Semantic.FieldScope);
    if (Operation == TEXT("get") && (Scope == TEXT("variable") || Scope == TEXT("property_path")))
    {
        return FBlueprintHelperFieldCapabilityRegistry::FindById(TEXT("field.member_get"));
    }
    if (Operation == TEXT("set") && (Scope == TEXT("variable") || Scope == TEXT("property_path")))
    {
        return FBlueprintHelperFieldCapabilityRegistry::FindById(TEXT("field.member_set"));
    }
    return nullptr;
}

bool UGraphWriteActionResolverUtils::IsFunctionScopedCapability(
    const FBlueprintHelperFieldCapabilitySpec& Spec)
{
    return Spec.RootKind == EBlueprintHelperFieldCapabilityRootKind::Local
        || Spec.RootKind == EBlueprintHelperFieldCapabilityRootKind::FunctionParam
        || Spec.bRequiresFunctionScope;
}

FString UGraphWriteActionResolverUtils::DescribePinType(const FEdGraphPinType& PinType)
{
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    {
        return TEXT("bool");
    }
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
    {
        return TEXT("int");
    }
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
    {
        return TEXT("string");
    }
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
    {
        return TEXT("name");
    }
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
    {
        return TEXT("text");
    }
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real && PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
    {
        return TEXT("float");
    }
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real && PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
    {
        return TEXT("double");
    }

    FString Result = PinType.PinCategory.ToString();
    if (!PinType.PinSubCategory.IsNone())
    {
        Result += TEXT(".");
        Result += PinType.PinSubCategory.ToString();
    }
    if (PinType.PinSubCategoryObject.IsValid())
    {
        Result += TEXT(":");
        Result += PinType.PinSubCategoryObject->GetPathName();
    }
    return Result;
}

bool UGraphWriteActionResolverUtils::IsComponentRefFieldScope(const FString& FieldScope)
{
    return NormalizeFieldBoundaryToken(FieldScope) == TEXT("component_ref");
}

bool UGraphWriteActionResolverUtils::IsFieldAccessFieldScope(const FString& FieldScope)
{
    return NormalizeFieldBoundaryToken(FieldScope) == TEXT("field_access");
}

FString UGraphWriteActionResolverUtils::GetEvidenceValue(
    const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
    if (const FString* Value = Evidence.Find(Key))
    {
        return Value->TrimStartAndEnd();
    }
    return FString();
}

void UGraphWriteActionResolverUtils::AddCapabilityFactIfPresent(
    FBlueprintHelperActionResolutionRequest& Request,
    const FString& Key,
    const FString& Value)
{
    const FString CleanValue = Value.TrimStartAndEnd();
    if (!Key.IsEmpty() && !CleanValue.IsEmpty())
    {
        Request.Semantic.CapabilityFacts.FindOrAdd(Key, CleanValue);
    }
}

FString UGraphWriteActionResolverUtils::CapabilityFactOrEvidence(
    const FBlueprintHelperActionResolutionRequest& Request,
    const TMap<FString, FString>& Evidence,
    const FString& FactKey,
    const TCHAR* EvidenceKey)
{
    const FString FactValue = Request.Semantic.CapabilityFacts.FindRef(FactKey).TrimStartAndEnd();
    return !FactValue.IsEmpty() ? FactValue : GetEvidenceValue(Evidence, EvidenceKey);
}

FString UGraphWriteActionResolverUtils::CapabilityFactOrEvidence(
    const FBlueprintHelperActionResolutionRequest& Request,
    const TMap<FString, FString>& Evidence,
    const FString& FactKey,
    const TCHAR* FirstEvidenceKey,
    const TCHAR* SecondEvidenceKey)
{
    const FString FactValue = Request.Semantic.CapabilityFacts.FindRef(FactKey).TrimStartAndEnd();
    if (!FactValue.IsEmpty())
    {
        return FactValue;
    }
    const FString FirstEvidenceValue = GetEvidenceValue(Evidence, FirstEvidenceKey);
    return !FirstEvidenceValue.IsEmpty() ? FirstEvidenceValue : GetEvidenceValue(Evidence, SecondEvidenceKey);
}

void UGraphWriteActionResolverUtils::BackfillCapabilityFactsFromEvidence(
    FBlueprintHelperActionResolutionRequest& Request,
    const TMap<FString, FString>& Evidence)
{
    AddCapabilityFactIfPresent(Request, TEXT("field.member_name"), GetEvidenceValue(Evidence, TEXT("field_name")));
    AddCapabilityFactIfPresent(Request, TEXT("field.owner_class"), GetEvidenceValue(Evidence, TEXT("field_owner_class")));
    AddCapabilityFactIfPresent(Request, TEXT("field.property_path"), GetEvidenceValue(Evidence, TEXT("property_path")));
    AddCapabilityFactIfPresent(Request, TEXT("field.component_name"), GetEvidenceValue(Evidence, TEXT("component_name")));
    AddCapabilityFactIfPresent(Request, TEXT("field.component_name"), GetEvidenceValue(Evidence, TEXT("component_property_name")));
    AddCapabilityFactIfPresent(Request, TEXT("field.component_owner_class"), GetEvidenceValue(Evidence, TEXT("component_owner_class")));
    AddCapabilityFactIfPresent(Request, TEXT("field.component_owner_class"), GetEvidenceValue(Evidence, TEXT("component_binding_owner_class_path")));
    AddCapabilityFactIfPresent(Request, TEXT("field.component_kind"), GetEvidenceValue(Evidence, TEXT("component_kind")));
    AddCapabilityFactIfPresent(Request, TEXT("field.target_pin_ref"), GetEvidenceValue(Evidence, TEXT("target_pin_ref")));
    AddCapabilityFactIfPresent(Request, TEXT("field.target_pin_type"), GetEvidenceValue(Evidence, TEXT("linked_pin_type_category")));
    AddCapabilityFactIfPresent(Request, TEXT("field.target_pin_object_path"), GetEvidenceValue(Evidence, TEXT("linked_pin_type_object_path")));

    const FString TargetPinCategory = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type")).TrimStartAndEnd();
    const FString TargetPinObjectPath = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")).TrimStartAndEnd();
    if (!Request.Semantic.TargetObjectPinType.IsValid() && (!TargetPinCategory.IsEmpty() || !TargetPinObjectPath.IsEmpty()))
    {
        Request.Semantic.TargetObjectPinType.Category = TargetPinCategory;
        Request.Semantic.TargetObjectPinType.ObjectPath = TargetPinObjectPath;
    }
    if (Request.Semantic.TargetObjectType.TrimStartAndEnd().IsEmpty() && !TargetPinObjectPath.IsEmpty())
    {
        Request.Semantic.TargetObjectType = TargetPinObjectPath;
    }
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeFieldMissingEvidenceResult(
    const FString& Message, const FString& ErrorCode)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
    Result.ErrorCode = ErrorCode;
    Result.Message = Message;
    return Result;
}

FString UGraphWriteActionResolverUtils::FirstNonEmptyFieldValue(const FString& First, const FString& Second)
{
    return !First.TrimStartAndEnd().IsEmpty() ? First.TrimStartAndEnd() : Second.TrimStartAndEnd();
}

FString UGraphWriteActionResolverUtils::FirstNonEmptyFieldValue(const FString& First, const FString& Second, const FString& Third)
{
    return FirstNonEmptyFieldValue(FirstNonEmptyFieldValue(First, Second), Third);
}

bool UGraphWriteActionResolverUtils::TypeMatches(
    const FString& ExpectedType,
    const FBlueprintHelperCallFunctionPinType& ExpectedPinType,
    const FEdGraphPinType& CandidateType)
{
    if (!ExpectedType.TrimStartAndEnd().IsEmpty())
    {
        const FString CandidateTypeText = DescribePinType(CandidateType);
        const FString Expected = NormalizeFieldVariableToken(ExpectedType);
        const FString Candidate = NormalizeFieldVariableToken(CandidateTypeText);
        return Candidate.Contains(Expected)
            || Expected.Contains(Candidate)
            || NormalizeFieldVariableToken(CandidateType.PinCategory.ToString()) == Expected;
    }

    if (ExpectedPinType.IsValid())
    {
        if (!ExpectedPinType.Category.IsEmpty()
            && !CandidateType.PinCategory.ToString().Equals(ExpectedPinType.Category, ESearchCase::IgnoreCase))
        {
            return false;
        }
        if (!ExpectedPinType.SubCategory.IsEmpty()
            && !CandidateType.PinSubCategory.ToString().Equals(ExpectedPinType.SubCategory, ESearchCase::IgnoreCase))
        {
            return false;
        }
    }

    return true;
}

UClass* UGraphWriteActionResolverUtils::ResolveOwnerClass(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    if (Blueprint->SkeletonGeneratedClass)
    {
        return Blueprint->SkeletonGeneratedClass;
    }
    if (Blueprint->GeneratedClass)
    {
        return Blueprint->GeneratedClass;
    }
    return Blueprint->ParentClass;
}

FString UGraphWriteActionResolverUtils::ResolveOwnerClassPath(const UBlueprint* Blueprint, const UClass* OwnerClass)
{
    if (OwnerClass)
    {
        return OwnerClass->GetPathName();
    }
    if (Blueprint && Blueprint->GeneratedClass)
    {
        return Blueprint->GeneratedClass->GetPathName();
    }
    if (Blueprint && Blueprint->SkeletonGeneratedClass)
    {
        return Blueprint->SkeletonGeneratedClass->GetPathName();
    }
    if (Blueprint && Blueprint->ParentClass)
    {
        return Blueprint->ParentClass->GetPathName();
    }
    return FString();
}

const FProperty* UGraphWriteActionResolverUtils::FindVariableProperty(UBlueprint* Blueprint, const FName VariableName)
{
    if (!Blueprint || VariableName.IsNone())
    {
        return nullptr;
    }

    if (Blueprint->SkeletonGeneratedClass)
    {
        if (const FProperty* Property = FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, VariableName))
        {
            return Property;
        }
    }
    if (Blueprint->GeneratedClass)
    {
        if (const FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VariableName))
        {
            return Property;
        }
    }
    return Blueprint->ParentClass ? FindFProperty<FProperty>(Blueprint->ParentClass, VariableName) : nullptr;
}

UClass* UGraphWriteActionResolverUtils::FindClassByPath_FieldVarResolver(const FString& ClassPath)
{
    const FString CleanPath = ClassPath.TrimStartAndEnd();
    return CleanPath.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *CleanPath);
}

const FProperty* UGraphWriteActionResolverUtils::FindPropertyOnClass(UClass* OwnerClass, const FName PropertyName)
{
    if (!OwnerClass || PropertyName.IsNone())
    {
        return nullptr;
    }

    for (UClass* Class = OwnerClass; Class; Class = Class->GetSuperClass())
    {
        if (const FProperty* Property = FindFProperty<FProperty>(Class, PropertyName))
        {
            return Property;
        }
    }
    return nullptr;
}

const FProperty* UGraphWriteActionResolverUtils::ResolveFieldProperty(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperFieldCapabilitySpec* CapabilitySpec,
    const FString& FieldName)
{
    const FName FieldFName(*FieldName.TrimStartAndEnd());
    if (FieldFName.IsNone())
    {
        return nullptr;
    }

    if (CapabilitySpec && CapabilitySpec->bRequiresTargetPin)
    {
        const FString OwnerClassPath = FirstNonEmptyFieldValue(
            Request.Semantic.CapabilityFacts.FindRef(TEXT("field.owner_class")),
            Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")));
        if (const FProperty* TargetProperty = FindPropertyOnClass(FindClassByPath_FieldVarResolver(OwnerClassPath), FieldFName))
        {
            return TargetProperty;
        }
    }

    return FindVariableProperty(Request.Blueprint, FieldFName);
}

bool UGraphWriteActionResolverUtils::ResolveFunctionScope(
    const FBlueprintHelperActionResolutionRequest& Request,
    const TMap<FString, FString>& Evidence,
    const FBlueprintHelperFieldCapabilitySpec& Spec,
    FString& OutScopeName,
    FBlueprintHelperActionResolutionResult& OutResult)
{
    if (!IsFunctionScopedCapability(Spec))
    {
        return true;
    }

    OutScopeName = FirstNonEmptyFieldValue(
        Request.Semantic.CapabilityFacts.FindRef(TEXT("field.function_name")),
        Request.Semantic.CapabilityFacts.FindRef(TEXT("field.local_scope")),
        GetEvidenceValue(Evidence, TEXT("local_scope")));
    if (OutScopeName.IsEmpty() && Request.TargetGraph)
    {
        OutScopeName = Request.TargetGraph->GetName();
    }

    if (!Request.TargetGraph || !FBlueprintEditorUtils::DoesSupportLocalVariables(Request.TargetGraph))
    {
        OutResult.Status = EBlueprintHelperActionResolutionStatus::NotFound;
        OutResult.ErrorCode = TEXT("missing_or_mismatched_function_scope");
        OutResult.Message = TEXT("Field capability requires a function graph scope.");
        return false;
    }

    if (!OutScopeName.IsEmpty() && !Request.TargetGraph->GetName().Equals(OutScopeName, ESearchCase::IgnoreCase))
    {
        OutResult.Status = EBlueprintHelperActionResolutionStatus::NotFound;
        OutResult.ErrorCode = TEXT("missing_or_mismatched_function_scope");
        OutResult.Message = FString::Printf(
            TEXT("Field capability scope mismatch: expected=%s target_graph=%s."),
            *OutScopeName,
            *Request.TargetGraph->GetName());
        return false;
    }
    return true;
}

FBPVariableDescription* UGraphWriteActionResolverUtils::FindLocalVariableDescription(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& FieldName,
    UK2Node_FunctionEntry** OutEntryNode)
{
    if (!Request.Blueprint || !Request.TargetGraph || FieldName.TrimStartAndEnd().IsEmpty())
    {
        return nullptr;
    }
    return FBlueprintEditorUtils::FindLocalVariable(
        Request.Blueprint,
        Request.TargetGraph,
        FName(*FieldName.TrimStartAndEnd()),
        OutEntryNode);
}

FProperty* UGraphWriteActionResolverUtils::ResolveLocalVariableProperty(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBPVariableDescription& LocalVariable)
{
    if (!Request.Blueprint || !Request.Blueprint->SkeletonGeneratedClass || !Request.TargetGraph)
    {
        return nullptr;
    }

    FMemberReference Reference;
    Reference.SetLocalMember(LocalVariable.VarName, Request.TargetGraph->GetName(), LocalVariable.VarGuid);
    return Reference.ResolveMember<FProperty>(Request.Blueprint->SkeletonGeneratedClass);
}

UFunction* UGraphWriteActionResolverUtils::FindFunctionForGraph(UBlueprint* Blueprint, UEdGraph* FunctionGraph)
{
    if (!Blueprint || !FunctionGraph)
    {
        return nullptr;
    }

    if (Blueprint->SkeletonGeneratedClass)
    {
        if (UFunction* Function = FindUField<UFunction>(Blueprint->SkeletonGeneratedClass, FunctionGraph->GetFName()))
        {
            return Function;
        }
    }
    if (Blueprint->GeneratedClass)
    {
        if (UFunction* Function = FindUField<UFunction>(Blueprint->GeneratedClass, FunctionGraph->GetFName()))
        {
            return Function;
        }
    }
    return Blueprint->ParentClass ? FindUField<UFunction>(Blueprint->ParentClass, FunctionGraph->GetFName()) : nullptr;
}

bool UGraphWriteActionResolverUtils::IsDisallowedFunctionParam(const FProperty* Param, const FString& ParamFlags)
{
    return !Param
        || Param->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
        || ParamFlags.Contains(TEXT("ReturnParm"))
        || ParamFlags.Contains(TEXT("OutParm"))
        || ParamFlags.Contains(TEXT("ReferenceParm"));
}

FProperty* UGraphWriteActionResolverUtils::FindFunctionInputParameter(
    UFunction* Function,
    const FString& FieldName,
    const FString& ParamFlags,
    FString& OutErrorCode)
{
    if (!Function || FieldName.TrimStartAndEnd().IsEmpty())
    {
        OutErrorCode = TEXT("function_param_not_found");
        return nullptr;
    }

    const FName ParamName(*FieldName.TrimStartAndEnd());
    for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
    {
        FProperty* Param = *ParamIt;
        if (!Param || Param->GetFName() != ParamName)
        {
            continue;
        }

        if (IsDisallowedFunctionParam(Param, ParamFlags))
        {
            OutErrorCode = TEXT("function_output_param_belongs_to_control_return");
            return nullptr;
        }
        return Param;
    }

    OutErrorCode = TEXT("function_param_not_found");
    return nullptr;
}

bool UGraphWriteActionResolverUtils::ConvertPropertyToPinType(const FProperty* Property, FEdGraphPinType& OutPinType)
{
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    return Schema && Schema->ConvertPropertyToPinType(Property, OutPinType);
}

int32 UGraphWriteActionResolverUtils::ScoreVariableCandidate(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBPVariableDescription& Variable)
{
    const FString VariableName = Variable.VarName.ToString();
    const FString Query = Request.Semantic.Query.TrimStartAndEnd();
    const FString TargetPath = Request.Semantic.TargetPath.TrimStartAndEnd();
    const FString Wanted = !Query.IsEmpty() ? Query : TargetPath;
    const FString NormalizedVariable = NormalizeFieldVariableToken(VariableName);
    const FString NormalizedWanted = NormalizeFieldVariableToken(Wanted);

    int32 Score = 0;
    if (NormalizedWanted.IsEmpty())
    {
        Score += 10;
    }
    else if (NormalizedVariable == NormalizedWanted)
    {
        Score += 100;
    }
    else if (Request.bAllowFuzzyUnique && NormalizedVariable.Contains(NormalizedWanted))
    {
        Score += 55;
    }
    else
    {
        return INDEX_NONE;
    }

    if (!TargetPath.IsEmpty() && NormalizeFieldVariableToken(TargetPath) == NormalizedVariable)
    {
        Score += 20;
    }

    if (!TypeMatches(Request.Semantic.ExpectedReturnType, Request.Semantic.ExpectedReturnPinType, Variable.VarType))
    {
        return INDEX_NONE;
    }

    Score += 15;
    return Score;
}

FString UGraphWriteActionResolverUtils::MakeVariableStableId(
    const UBlueprint* Blueprint,
    const FString& FieldName,
    const FString& FieldOperation,
    const FString& FieldScope,
    const FString& Prefix)
{
    return FString::Printf(
        TEXT("%s:%s:%s:field:%s:%s"),
        *Prefix,
        Blueprint ? *Blueprint->GetPathName() : TEXT("unknown_blueprint"),
        *FieldName,
        *NormalizeFieldBoundaryToken(FieldOperation),
        *NormalizeFieldBoundaryToken(FieldScope));
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionResolverUtils::BuildCandidateInfo(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBPVariableDescription& Variable,
    const UClass* OwnerClass,
    const UClass* NodeClass,
    const int32 Score)
{
    FBlueprintHelperCallFunctionCandidateInfo Info;
    Info.StableId = MakeVariableStableId(
        Request.Blueprint,
        Variable.VarName.ToString(),
        Request.Semantic.FieldOperation,
        Request.Semantic.FieldScope);
    Info.DisplayName = Variable.VarName.ToString();
    Info.OwnerClassPath = OwnerClass ? OwnerClass->GetPathName() : FString();
    Info.NativeFunctionName = Variable.VarName.ToString();
    Info.Category = TEXT("field_variable");
    Info.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
    Info.MatchReason = FString::Printf(
        TEXT("semantic=%s,field_operation=%s,field_scope=%s,score=%d,type=%s"),
        *FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
        *Request.Semantic.FieldOperation,
        *Request.Semantic.FieldScope,
        Score,
        *DescribePinType(Variable.VarType));
    Info.ReturnType = DescribePinType(Variable.VarType);
    Info.TargetObjectPin = TEXT("self");
    Info.Score = Score;
    Info.bGraphCompatible = Request.TargetGraph != nullptr;
    return Info;
}

void UGraphWriteActionResolverUtils::SortFieldVariableCandidates(
    TArray<FBlueprintHelperVariableActionCandidate>& Candidates)
{
    Candidates.Sort([](
        const FBlueprintHelperVariableActionCandidate& Left,
        const FBlueprintHelperVariableActionCandidate& Right)
    {
        if (Left.Info.Score != Right.Info.Score)
        {
            return Left.Info.Score > Right.Info.Score;
        }
        return Left.Info.DisplayName < Right.Info.DisplayName;
    });
}

TArray<FBlueprintHelperVariableActionCandidate> UGraphWriteActionResolverUtils::BuildProjectedFieldCandidates(
    const FBlueprintHelperActionResolutionRequest& Request,
    const UClass* OwnerClass,
    const UClass* NodeClass)
{
    TArray<FBlueprintHelperVariableActionCandidate> Candidates;
    if (!Request.Blueprint)
    {
        return Candidates;
    }

    for (const FBPVariableDescription& Variable : Request.Blueprint->NewVariables)
    {
        const int32 Score = ScoreVariableCandidate(Request, Variable);
        if (Score == INDEX_NONE)
        {
            continue;
        }

        FBlueprintHelperVariableActionCandidate Candidate;
        Candidate.Info = BuildCandidateInfo(Request, Variable, OwnerClass, NodeClass, Score);
        Candidates.Add(MoveTemp(Candidate));
    }

    SortFieldVariableCandidates(Candidates);
    return Candidates;
}

void UGraphWriteActionResolverUtils::SetFieldCandidateDiagnostics(
    FBlueprintHelperActionResolutionResult& Result,
    const TArray<FBlueprintHelperVariableActionCandidate>& Candidates,
    const int32 MaxCandidates)
{
    const int32 Limit = FMath::Max(1, MaxCandidates);
    for (const FBlueprintHelperVariableActionCandidate& Candidate : Candidates)
    {
        if (Result.CandidateActions.Num() >= Limit)
        {
            break;
        }
        Result.CandidateActions.Add(Candidate.Info);
    }
}

bool UGraphWriteActionResolverUtils::ResolveFieldIdentity(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperFieldCapabilitySpec& Spec,
    const FString& FieldName,
    const FProperty* ResolvedProperty,
    const UClass* ResolvedOwnerClass,
    FBlueprintHelperResolvedFieldIdentity& OutIdentity)
{
    const FString ResolvedFieldName = FirstNonEmptyFieldValue(
        FieldName,
        Request.Semantic.CapabilityFacts.FindRef(TEXT("field.member_name")),
        Request.Semantic.Query);
    if (ResolvedFieldName.IsEmpty())
    {
        OutIdentity.DiagnosticReason = TEXT("missing_resolved_field_name");
        return false;
    }

    OutIdentity.CapabilityId = Spec.Id;
    OutIdentity.FieldKind = FieldCapabilityRootKindToString(Spec.RootKind);
    const FString RequestedOwnerClass = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.owner_class"));
    const FString TargetPinObjectPath = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path"));
    if (Spec.bRequiresTargetPin)
    {
        OutIdentity.OwnerClassPath = FirstNonEmptyFieldValue(RequestedOwnerClass, TargetPinObjectPath);
    }
    if (OutIdentity.OwnerClassPath.IsEmpty())
    {
        OutIdentity.OwnerClassPath = ResolveOwnerClassPath(Request.Blueprint, ResolvedOwnerClass);
    }
    if (OutIdentity.OwnerClassPath.IsEmpty())
    {
        OutIdentity.OwnerClassPath = RequestedOwnerClass;
    }
    if (OutIdentity.OwnerClassPath.IsEmpty() && ResolvedProperty)
    {
        if (const UStruct* OwnerStruct = ResolvedProperty->GetOwnerStruct())
        {
            OutIdentity.OwnerClassPath = OwnerStruct->GetPathName();
        }
    }
    OutIdentity.MemberName = ResolvedProperty ? ResolvedProperty->GetName() : ResolvedFieldName;
    FGuid::Parse(Request.Semantic.CapabilityFacts.FindRef(TEXT("field.member_guid")), OutIdentity.MemberGuid);
    OutIdentity.LocalScopeName = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.local_scope"));
    OutIdentity.FunctionName = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.function_name"));
    if (OutIdentity.LocalScopeName.IsEmpty() && Request.TargetGraph && IsFunctionScopedCapability(Spec))
    {
        OutIdentity.LocalScopeName = Request.TargetGraph->GetName();
    }
    if (OutIdentity.FunctionName.IsEmpty() && Request.TargetGraph && IsFunctionScopedCapability(Spec))
    {
        OutIdentity.FunctionName = Request.TargetGraph->GetName();
    }
    OutIdentity.TargetPinRef = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref"));
    OutIdentity.TargetPinCategory = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type"));
    OutIdentity.TargetPinObjectPath = TargetPinObjectPath;
    OutIdentity.ExpectedNodeFamily = Spec.ExpectedNodeFamily;
    OutIdentity.ExpectedNodeClassPath = Spec.ExpectedNodeClass;
    return true;
}

void UGraphWriteActionResolverUtils::ApplyResolvedFieldIdentityToCandidate(
    const FBlueprintHelperResolvedFieldIdentity& Identity,
    FBlueprintHelperCallFunctionCandidateInfo& Candidate)
{
    Candidate.CapabilityId = Identity.CapabilityId;
    Candidate.ExpectedNodeFamily = Identity.ExpectedNodeFamily;
    Candidate.ExpectedNodeClassPath = Identity.ExpectedNodeClassPath;

    if (!Identity.CapabilityId.IsEmpty())
    {
        Candidate.ReadbackFacts.Add(TEXT("capability_id"), Identity.CapabilityId);
    }
    if (!Identity.FieldKind.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.kind"), Identity.FieldKind);
        Candidate.ReadbackFacts.Add(TEXT("field_kind"), Identity.FieldKind);
    }
    if (!Identity.OwnerClassPath.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.owner_class"), Identity.OwnerClassPath);
        Candidate.ReadbackFacts.Add(TEXT("owner_class"), Identity.OwnerClassPath);
    }
    if (!Identity.MemberName.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.member_name"), Identity.MemberName);
        Candidate.ReadbackFacts.Add(TEXT("member_name"), Identity.MemberName);
    }
    if (!Identity.LocalScopeName.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.local_scope"), Identity.LocalScopeName);
        Candidate.ReadbackFacts.Add(TEXT("local_scope"), Identity.LocalScopeName);
    }
    if (!Identity.FunctionName.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.function_name"), Identity.FunctionName);
        Candidate.ReadbackFacts.Add(TEXT("function_name"), Identity.FunctionName);
    }
    if (!Identity.TargetPinRef.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.target_pin_ref"), Identity.TargetPinRef);
        Candidate.ReadbackFacts.Add(TEXT("target_pin_ref"), Identity.TargetPinRef);
    }
    if (!Identity.TargetPinCategory.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.target_pin_type"), Identity.TargetPinCategory);
        Candidate.ReadbackFacts.Add(TEXT("target_pin_category"), Identity.TargetPinCategory);
    }
    if (!Identity.TargetPinObjectPath.IsEmpty())
    {
        Candidate.CapabilityFacts.Add(TEXT("field.target_pin_object_path"), Identity.TargetPinObjectPath);
        Candidate.ReadbackFacts.Add(TEXT("target_pin_object_path"), Identity.TargetPinObjectPath);
        if (!Identity.OwnerClassPath.Equals(Identity.TargetPinObjectPath, ESearchCase::IgnoreCase))
        {
            Candidate.ReadbackFacts.Add(TEXT("owner_projected_from_target_pin"), Identity.TargetPinObjectPath);
        }
    }

    Candidate.StableId = FString::Printf(
        TEXT("%s:%s:%s:%s:%s"),
        *Candidate.StableId,
        *Identity.CapabilityId,
        *Identity.OwnerClassPath,
        *Identity.MemberName,
        *Identity.TargetPinObjectPath);
}

// ============================================================================
// BlueprintHelperFieldPathResolution.cpp
// ============================================================================

FString UGraphWriteActionResolverUtils::CleanLower(const FString& Value)
{
    return Clean(Value).ToLower();
}

FString UGraphWriteActionResolverUtils::EvidenceValue(
    const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
    if (const FString* Value = Evidence.Find(Key))
    {
        return Clean(*Value);
    }
    return FString();
}

void UGraphWriteActionResolverUtils::SetInvalid(
    FBlueprintHelperResolvedFieldPath& Result, const FString& Code, const FString& Message)
{
    Result.bIsValid = false;
    Result.ErrorCode = Code;
    Result.Message = Message;
}

void UGraphWriteActionResolverUtils::PopulateSegments(FBlueprintHelperResolvedFieldPath& Result)
{
    Result.Segments.Reset();
    Result.FullPath.ParseIntoArray(Result.Segments, TEXT("."), true);
    for (FString& Segment : Result.Segments)
    {
        Segment = Clean(Segment);
    }
    Result.Segments.RemoveAll([](const FString& Segment)
    {
        return Segment.IsEmpty();
    });

    if (Result.Segments.Num() > 0)
    {
        Result.RootName = Result.Segments[0];
        Result.LeafName = Result.Segments.Last();
    }
    else
    {
        Result.RootName = Result.FullPath;
        Result.LeafName = Result.FullPath;
    }
}

FString UGraphWriteActionResolverUtils::ResolveOwnerEvidence(
    const FBlueprintHelperActionResolutionRequest& Request,
    const TMap<FString, FString>& Evidence)
{
    return FirstNonEmpty(
        EvidenceValue(Evidence, TEXT("field_owner_class")),
        EvidenceValue(Evidence, TEXT("property_owner")),
        EvidenceValue(Evidence, TEXT("target_object_type")),
        Request.Semantic.TargetObjectType);
}

FString UGraphWriteActionResolverUtils::DescribePinTypeEvidence(
    const FBlueprintHelperCallFunctionPinType& PinType)
{
    if (!PinType.IsValid())
    {
        return FString();
    }
    return FirstNonEmpty(PinType.Category, PinType.ObjectPath);
}

FString UGraphWriteActionResolverUtils::ComposePropertyFullPath(
    const FBlueprintHelperActionResolutionRequest& Request,
    const TMap<FString, FString>& Evidence)
{
    const FString PropertyPath = FirstNonEmpty(
        Request.Semantic.PropertyPath,
        EvidenceValue(Evidence, TEXT("property_path")));
    const FString TargetPath = Clean(Request.Semantic.TargetPath);
    if (PropertyPath.IsEmpty())
    {
        return FString();
    }
    if (!TargetPath.IsEmpty()
        && !TargetPath.Contains(TEXT("."))
        && !PropertyPath.StartsWith(TargetPath + TEXT("."), ESearchCase::CaseSensitive))
    {
        return TargetPath + TEXT(".") + PropertyPath;
    }
    return PropertyPath;
}

// ============================================================================
// BlueprintHelperGenericAssetStructControlActionResolver.cpp
// ============================================================================

bool UGraphWriteActionResolverUtils::IsGenericNodeSpawnerSemantic(
    const EBlueprintHelperActionSemanticKind Kind)
{
    return Kind == EBlueprintHelperActionSemanticKind::Select
        || Kind == EBlueprintHelperActionSemanticKind::Control;
}

FString UGraphWriteActionResolverUtils::ControlRequestEvidenceValue(
    const FBlueprintHelperActionResolutionRequest& Request, const TCHAR* Key)
{
    if (const FString* Value = Request.ContextEvidence.Find(Key))
    {
        return Clean(*Value);
    }
    if (const FString* Value = Request.Semantic.DefaultValues.Find(Key))
    {
        return Clean(*Value);
    }
    return FString();
}

FString UGraphWriteActionResolverUtils::ResolveControlOperation(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const FString EvidenceOperation = ControlRequestEvidenceValue(Request, TEXT("generic.control.operation"));
    return NormalizeOperation(EvidenceOperation.IsEmpty() ? Request.Semantic.Query : EvidenceOperation);
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeUnsupportedGenericSemanticResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("unsupported_generic_node_spawner_candidate_semantic");
    Result.Message = Message;
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeInvalidGenericNodeSpawnerResult(
    const FString& ErrorCode,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = ErrorCode;
    Result.Message = Message;
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeBlockedGenericNodeSpawnerResult(
    const FString& ErrorCode,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = ErrorCode;
    Result.Message = Message;
    return Result;
}

TArray<FString> UGraphWriteActionResolverUtils::ParseDelimitedEvidenceList(const FString& Value)
{
    TArray<FString> Result;
    Value.ParseIntoArray(Result, TEXT(","), true);
    for (FString& Entry : Result)
    {
        Entry = Entry.TrimStartAndEnd();
    }
    Result.RemoveAll([](const FString& Entry)
    {
        return Entry.IsEmpty();
    });
    return Result;
}

void UGraphWriteActionResolverUtils::AddGenericOpsReadbackFacts(
    FBlueprintHelperCallFunctionCandidateInfo& Candidate,
    const FString& Family,
    const FString& Operation,
    const TMap<FString, FString>& ExtraFacts)
{
    Candidate.ReadbackFacts.Add(TEXT("generic.family"), Family);
    Candidate.ReadbackFacts.Add(TEXT("generic.operation_id"), FString::Printf(TEXT("generic_ops.%s.%s"), *Family, *Operation));
    Candidate.ReadbackFacts.Add(TEXT("generic.operation"), Operation);
    Candidate.ReadbackFacts.Add(TEXT("generic.wildcard_residual"), TEXT("false"));
    for (const TPair<FString, FString>& Fact : ExtraFacts)
    {
        if (!Fact.Key.IsEmpty() && !Fact.Value.TrimStartAndEnd().IsEmpty())
        {
            Candidate.ReadbackFacts.Add(Fact.Key, Fact.Value.TrimStartAndEnd());
        }
    }
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeResolvedGenericNodeSpawnerResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& Family,
    const FString& Operation,
    const FString& StableEvidence,
    TSubclassOf<UEdGraphNode> NodeClass,
    UBlueprintNodeSpawner* Spawner,
    const FString& Category,
    const FString& MatchReason,
    const TMap<FString, FString>& ExtraFacts)
{
    if (!NodeClass)
    {
        return MakeBlockedGenericNodeSpawnerResult(
            TEXT("generic_node_class_unavailable"),
            FString::Printf(TEXT("GenericOps operation '%s' does not have a loadable node class."), *Operation));
    }
    if (!Spawner)
    {
        return MakeBlockedGenericNodeSpawnerResult(
            TEXT("generic_node_spawner_unavailable"),
            FString::Printf(TEXT("GenericOps operation '%s' could not create a UBlueprintNodeSpawner."), *Operation));
    }

    const FString StableId = FString::Printf(
        TEXT("generic_ops:%s:%s:%s"),
        *Family,
        *Operation,
        StableEvidence.IsEmpty() ? TEXT("default") : *StableEvidence);

    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = StableId;
    Candidate.DisplayName = FString::Printf(TEXT("GenericOps %s"), *Operation);
    Candidate.Category = Category;
    Candidate.NodeClassPath = NodeClass->GetPathName();
    Candidate.MatchReason = MatchReason;
    Candidate.Score = 100;
    Candidate.bGraphCompatible = Request.TargetGraph != nullptr;
    Candidate.bFromActionDatabase = false;
    Candidate.bBlueprintCallable = true;
    AddGenericOpsReadbackFacts(Candidate, Family, Operation, ExtraFacts);

    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.SelectedStableId = StableId;
    Result.SelectedSpawner = Spawner;
    Result.CandidateActions.Add(MoveTemp(Candidate));
    Result.SpawnerClass = Spawner->GetClass()->GetPathName();
    Result.NodeClass = NodeClass->GetPathName();
    Result.MatchReason = MatchReason;
    return Result;
}

UBlueprintNodeSpawner* UGraphWriteActionResolverUtils::CreateSwitchEnumSpawner(UEnum* Enum)
{
    if (!Enum)
    {
        return nullptr;
    }

    TWeakObjectPtr<UEnum> EnumPtr = Enum;
    UBlueprintNodeSpawner::FCustomizeNodeDelegate CustomizeSwitchEnum =
        UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda(
            [EnumPtr](UEdGraphNode* NewNode, bool)
            {
                UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(NewNode);
                if (SwitchNode && EnumPtr.IsValid())
                {
                    SwitchNode->SetEnum(EnumPtr.Get());
                }
            });
    return UBlueprintNodeSpawner::Create(UK2Node_SwitchEnum::StaticClass(), nullptr, CustomizeSwitchEnum);
}

UBlueprintNodeSpawner* UGraphWriteActionResolverUtils::CreateGenericControlSpawner(
    TSubclassOf<UEdGraphNode> ResolvedNodeClass)
{
    return ResolvedNodeClass ? UBlueprintNodeSpawner::Create(ResolvedNodeClass) : nullptr;
}

UBlueprintNodeSpawner* UGraphWriteActionResolverUtils::CreateMacroInstanceSpawner(UEdGraph* MacroGraph)
{
    if (!MacroGraph)
    {
        return nullptr;
    }

    TWeakObjectPtr<UEdGraph> MacroGraphPtr = MacroGraph;
    UBlueprintNodeSpawner::FCustomizeNodeDelegate CustomizeMacro =
        UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda(
            [MacroGraphPtr](UEdGraphNode* NewNode, bool)
            {
                UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(NewNode);
                if (MacroNode && MacroGraphPtr.IsValid())
                {
                    MacroNode->SetMacroGraph(MacroGraphPtr.Get());
                }
            });
    return UBlueprintNodeSpawner::Create(UK2Node_MacroInstance::StaticClass(), nullptr, CustomizeMacro);
}

UEnum* UGraphWriteActionResolverUtils::ResolveEnumEvidence(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const FString EnumPath = ControlRequestEvidenceValue(Request, TEXT("generic.control.enum_path"));
    if (EnumPath.IsEmpty())
    {
        return nullptr;
    }
    if (UEnum* ExistingEnum = FindObject<UEnum>(nullptr, *EnumPath))
    {
        return ExistingEnum;
    }
    return LoadObject<UEnum>(nullptr, *EnumPath);
}

UEdGraph* UGraphWriteActionResolverUtils::ResolveMacroGraphEvidence(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const FString GraphPath = ControlRequestEvidenceValue(Request, TEXT("generic.macro.graph_path"));
    if (GraphPath.IsEmpty())
    {
        return nullptr;
    }
    if (UEdGraph* ExistingGraph = FindObject<UEdGraph>(nullptr, *GraphPath))
    {
        return ExistingGraph;
    }
    return LoadObject<UEdGraph>(nullptr, *GraphPath);
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::ResolveDedicatedControlFlowNodeSpawner(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const FString Operation = ResolveControlOperation(Request);
    const FString CaseValues = ControlRequestEvidenceValue(Request, TEXT("generic.control.case_values"));
    const FString DynamicOutputCount = ControlRequestEvidenceValue(Request, TEXT("generic.control.dynamic_output_count"));

    TSubclassOf<UEdGraphNode> NodeClass = nullptr;
    UBlueprintNodeSpawner* Spawner = nullptr;
    TMap<FString, FString> Facts;
    Facts.Add(TEXT("generic.control.operation"), Operation);

    if (Operation == TEXT("switch_int"))
    {
        if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
        {
            return MakeInvalidGenericNodeSpawnerResult(
                TEXT("missing_required_evidence"),
                TEXT("switch_int requires generic.control.case_values evidence."));
        }
        NodeClass = UK2Node_SwitchInteger::StaticClass();
        Spawner = CreateGenericControlSpawner(NodeClass);
        Facts.Add(TEXT("generic.control.case_values"), CaseValues);
    }
    else if (Operation == TEXT("switch_string"))
    {
        if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
        {
            return MakeInvalidGenericNodeSpawnerResult(
                TEXT("missing_required_evidence"),
                TEXT("switch_string requires generic.control.case_values evidence."));
        }
        NodeClass = UK2Node_SwitchString::StaticClass();
        Spawner = CreateGenericControlSpawner(NodeClass);
        Facts.Add(TEXT("generic.control.case_values"), CaseValues);
    }
    else if (Operation == TEXT("switch_name"))
    {
        if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
        {
            return MakeInvalidGenericNodeSpawnerResult(
                TEXT("missing_required_evidence"),
                TEXT("switch_name requires generic.control.case_values evidence."));
        }
        NodeClass = UK2Node_SwitchName::StaticClass();
        Spawner = CreateGenericControlSpawner(NodeClass);
        Facts.Add(TEXT("generic.control.case_values"), CaseValues);
    }
    else if (Operation == TEXT("switch_enum"))
    {
        if (ParseDelimitedEvidenceList(CaseValues).Num() == 0)
        {
            return MakeInvalidGenericNodeSpawnerResult(
                TEXT("missing_required_evidence"),
                TEXT("switch_enum requires generic.control.case_values evidence."));
        }
        UEnum* Enum = ResolveEnumEvidence(Request);
        if (!Enum)
        {
            return MakeInvalidGenericNodeSpawnerResult(
                TEXT("missing_required_evidence"),
                TEXT("switch_enum requires loadable generic.control.enum_path evidence."));
        }
        NodeClass = UK2Node_SwitchEnum::StaticClass();
        Spawner = CreateSwitchEnumSpawner(Enum);
        Facts.Add(TEXT("generic.control.case_values"), CaseValues);
        Facts.Add(TEXT("generic.control.enum_path"), Enum->GetPathName());
    }
    else if (Operation == TEXT("multi_gate"))
    {
        int32 OutputCount = 0;
        if (!LexTryParseString(OutputCount, *DynamicOutputCount) || OutputCount <= 0)
        {
            return MakeInvalidGenericNodeSpawnerResult(
                TEXT("missing_required_evidence"),
                TEXT("multi_gate requires positive generic.control.dynamic_output_count evidence."));
        }
        NodeClass = UK2Node_MultiGate::StaticClass();
        Spawner = CreateGenericControlSpawner(NodeClass);
        Facts.Add(TEXT("generic.control.dynamic_output_count"), FString::FromInt(OutputCount));
    }
    else
    {
        return MakeUnsupportedGenericSemanticResult(
            Request,
            FString::Printf(TEXT("Unsupported dedicated GenericOps control operation '%s'."), *Operation));
    }

    return MakeResolvedGenericNodeSpawnerResult(
        Request,
        TEXT("control"),
        Operation,
        Operation == TEXT("multi_gate") ? DynamicOutputCount : CaseValues,
        NodeClass,
        Spawner,
        TEXT("GenericOps.Control"),
        FString::Printf(TEXT("GenericOps dedicated control operation=%s"), *Operation),
        Facts);
}

bool UGraphWriteActionResolverUtils::IsStandardMacroOperation(const FString& Operation)
{
    return Operation == TEXT("do_once")
        || Operation == TEXT("do_n")
        || Operation == TEXT("gate")
        || Operation == TEXT("flip_flop")
        || Operation == TEXT("for_loop")
        || Operation == TEXT("for_loop_with_break")
        || Operation == TEXT("foreach_loop")
        || Operation == TEXT("foreach_loop_with_break")
        || Operation == TEXT("while_loop");
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::ResolveStandardMacroNodeSpawner(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const FString Operation = ResolveControlOperation(Request);
    if (!IsStandardMacroOperation(Operation))
    {
        return MakeUnsupportedGenericSemanticResult(
            Request,
            FString::Printf(TEXT("Unsupported StandardMacros GenericOps operation '%s'."), *Operation));
    }

    const FString GraphPath = ControlRequestEvidenceValue(Request, TEXT("generic.macro.graph_path"));
    const FString PinShapeSnapshot = ControlRequestEvidenceValue(Request, TEXT("generic.macro.pin_shape_snapshot"));
    if (GraphPath.IsEmpty() || PinShapeSnapshot.IsEmpty())
    {
        return MakeInvalidGenericNodeSpawnerResult(
            TEXT("missing_required_evidence"),
            TEXT("StandardMacros control requires generic.macro.graph_path and generic.macro.pin_shape_snapshot evidence."));
    }

    UEdGraph* MacroGraph = ResolveMacroGraphEvidence(Request);
    if (!MacroGraph)
    {
        return MakeInvalidGenericNodeSpawnerResult(
            TEXT("macro_graph_not_found"),
            FString::Printf(TEXT("StandardMacros control macro graph could not be loaded: %s."), *GraphPath));
    }
    if (!MacroGraph->GetSchema() || MacroGraph->GetSchema()->GetGraphType(MacroGraph) != GT_Macro)
    {
        return MakeInvalidGenericNodeSpawnerResult(
            TEXT("macro_graph_not_macro"),
            FString::Printf(TEXT("StandardMacros control graph is not a macro graph: %s."), *MacroGraph->GetPathName()));
    }

    UBlueprintNodeSpawner* Spawner = CreateMacroInstanceSpawner(MacroGraph);
    TMap<FString, FString> Facts;
    Facts.Add(TEXT("generic.control.operation"), Operation);
    Facts.Add(TEXT("generic.macro.graph_path"), MacroGraph->GetPathName());
    Facts.Add(TEXT("generic.macro.pin_shape_snapshot"), PinShapeSnapshot);

    return MakeResolvedGenericNodeSpawnerResult(
        Request,
        TEXT("control"),
        Operation,
        MacroGraph->GetPathName(),
        UK2Node_MacroInstance::StaticClass(),
        Spawner,
        TEXT("GenericOps.StandardMacros"),
        FString::Printf(TEXT("GenericOps StandardMacros operation=%s macro=%s"), *Operation, *MacroGraph->GetPathName()),
        Facts);
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::ResolveSingletonControlFlowNodeSpawner(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    FBlueprintHelperSingletonControlFlowEvidence Evidence;
    if (FBlueprintHelperSingletonControlFlowEvidenceProvider::TryResolve(Request, Evidence))
    {
        return FBlueprintHelperSingletonControlFlowEvidenceProvider::MakeResolvedResult(Request, Evidence);
    }

    const FString Operation = ResolveControlOperation(Request);
    if (Operation.StartsWith(TEXT("switch_")) || Operation == TEXT("multi_gate"))
    {
        return ResolveDedicatedControlFlowNodeSpawner(Request);
    }
    if (IsStandardMacroOperation(Operation))
    {
        return ResolveStandardMacroNodeSpawner(Request);
    }

    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("unsupported_singleton_control_flow_semantic");
    Result.Message = FString::Printf(
        TEXT("Unsupported singleton control-flow semantic '%s' with query '%s'."),
        *FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
        *Request.Semantic.Query);
    return Result;
}

// ============================================================================
// BlueprintHelperFunctionSemanticActionResolver.cpp
// ============================================================================

EBlueprintHelperActionResolutionStatus UGraphWriteActionResolverUtils::MapFunctionResolveStatus(
    EBlueprintHelperCallFunctionResolveStatus Status)
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

FString UGraphWriteActionResolverUtils::GetDefaultValue(
    const FBlueprintHelperActionSemanticConstraints& Semantic,
    const TCHAR* Key)
{
    return Semantic.DefaultValues.FindRef(Key).TrimStartAndEnd();
}

FString UGraphWriteActionResolverUtils::GetFunctionOperation(
    const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    const FString SemanticOperation = Semantic.FunctionOperation.TrimStartAndEnd();
    return SemanticOperation.IsEmpty()
        ? GetDefaultValue(Semantic, TEXT("function_operation"))
        : SemanticOperation;
}

bool UGraphWriteActionResolverUtils::HasTypedArgumentPinEvidence(
    const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    for (const TPair<FString, FBlueprintHelperCallFunctionPinType>& Pair : Semantic.ArgumentPinTypes)
    {
        if (Pair.Value.IsValid())
        {
            return true;
        }
    }
    return false;
}

bool UGraphWriteActionResolverUtils::IsTrueEvidence(
    const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
    const FString Value = Evidence.FindRef(Key).TrimStartAndEnd();
    return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
        || Value == TEXT("1")
        || Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
}

void UGraphWriteActionResolverUtils::PopulateCallContext(
    FBlueprintHelperCallFunctionResolveRequest& CallRequest,
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const FBlueprintHelperActionSemanticConstraints& Semantic = Request.Semantic;
    CallRequest.Context.Blueprint = Request.Blueprint;
    CallRequest.Context.Graph = Request.TargetGraph;
    CallRequest.Context.Schema = Request.TargetGraph ? Request.TargetGraph->GetSchema() : nullptr;
    CallRequest.Context.SelfClass = Request.Blueprint
        ? (Request.Blueprint->GeneratedClass ? Request.Blueprint->GeneratedClass.Get() : Request.Blueprint->SkeletonGeneratedClass.Get())
        : nullptr;
    CallRequest.Context.GraphKind = Request.TargetGraph && Request.TargetGraph->GetClass() ? Request.TargetGraph->GetClass()->GetName() : FString();
    CallRequest.Context.ArgumentNames = Semantic.ArgumentNames;
    CallRequest.Context.ArgumentTypes = Semantic.ArgumentTypes;
    CallRequest.Context.ArgumentPinTypes = Semantic.ArgumentPinTypes;
    CallRequest.Context.TargetObjectType = Semantic.TargetObjectType;
    CallRequest.Context.TargetObjectPinType = Semantic.TargetObjectPinType;
    CallRequest.Context.ExpectedReturnType = Semantic.ExpectedReturnType;
    CallRequest.Context.ExpectedReturnPinType = Semantic.ExpectedReturnPinType;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeInvalidRequestResult(
    const FString& ErrorCode,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
    Result.ErrorCode = ErrorCode;
    Result.Message = Message;
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::ResolveViaCallFunctionResolver(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    FBlueprintHelperCallFunctionResolveRequest CallRequest;
    CallRequest.Blueprint = Request.Blueprint;
    CallRequest.Graph = Request.TargetGraph;
    CallRequest.Query = Semantic.Query;
    CallRequest.SearchMode = Semantic.SearchMode;
    CallRequest.AmbiguityPolicy = Semantic.AmbiguityPolicy;
    CallRequest.CategoryPriority = Semantic.CategoryPriority;
    CallRequest.ArgumentNames = Semantic.ArgumentNames;
    CallRequest.ArgumentTypes = Semantic.ArgumentTypes;
    CallRequest.ArgumentPinTypes = Semantic.ArgumentPinTypes;
    CallRequest.TargetObjectType = Semantic.TargetObjectType;
    CallRequest.TargetObjectPinType = Semantic.TargetObjectPinType;
    CallRequest.ExpectedReturnType = Semantic.ExpectedReturnType;
    CallRequest.ExpectedReturnPinType = Semantic.ExpectedReturnPinType;
    CallRequest.bAllowFuzzyUnique = Request.bAllowFuzzyUnique;
    CallRequest.MaxCandidates = Request.MaxCandidates;
    if (!Semantic.StableId.TrimStartAndEnd().IsEmpty())
    {
        CallRequest.CandidatePolicy.RequiredStableCallableIds.Add(Semantic.StableId.TrimStartAndEnd());
    }
    PopulateCallContext(CallRequest, Request);

    const FBlueprintHelperCallFunctionResolveResult CallResult = FBlueprintHelperCallFunctionResolver::Resolve(CallRequest);

    FBlueprintHelperActionResolutionResult Result;
    Result.Status = MapFunctionResolveStatus(CallResult.Status);
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
    Result.ErrorCode = CallResult.ErrorCode;
    Result.Message = CallResult.Message;
    Result.SelectedStableId = CallResult.Selected.StableId;
    Result.SelectedSpawner = CallResult.Selected.NodeSpawner;
    Result.SelectedFunction = CallResult.Selected.Function;
    Result.CandidateActions = CallResult.CandidateFunctions;
    Result.FunctionCandidate = CallResult.Selected;
    return Result;
}

// ============================================================================
// BlueprintHelperGenericCreateActionResolver.cpp
// ============================================================================

FString UGraphWriteActionResolverUtils::NormalizeCreateOperation(const FString& Operation)
{
    return Operation.TrimStartAndEnd().ToLower();
}

FString UGraphWriteActionResolverUtils::DescribePinType(const FBlueprintHelperCallFunctionPinType& PinType)
{
    if (!PinType.IsValid())
    {
        return FString();
    }

    TArray<FString> Parts;
    if (!PinType.Category.IsEmpty())
    {
        Parts.Add(PinType.Category);
    }
    if (!PinType.SubCategory.IsEmpty())
    {
        Parts.Add(PinType.SubCategory);
    }
    if (!PinType.ObjectPath.IsEmpty())
    {
        Parts.Add(PinType.ObjectPath);
    }
    if (!PinType.ContainerType.IsEmpty())
    {
        Parts.Add(PinType.ContainerType);
    }
    return FString::Join(Parts, TEXT("|"));
}

FString UGraphWriteActionResolverUtils::ResolveContainerElementEvidence(
    const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    return FirstNonEmpty(
        Semantic.ArgumentTypes.FindRef(TEXT("element")),
        Semantic.ArgumentTypes.FindRef(TEXT("value")),
        DescribePinType(Semantic.ContainerElementPinType),
        Semantic.ExpectedReturnType);
}

FString UGraphWriteActionResolverUtils::ResolveContainerKeyEvidence(
    const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    return FirstNonEmpty(
        Semantic.ArgumentTypes.FindRef(TEXT("key")),
        DescribePinType(Semantic.ContainerKeyPinType));
}

FString UGraphWriteActionResolverUtils::ResolveContainerValueEvidence(
    const FBlueprintHelperActionSemanticConstraints& Semantic)
{
    return FirstNonEmpty(
        Semantic.ArgumentTypes.FindRef(TEXT("value")),
        Semantic.ArgumentTypes.FindRef(TEXT("element")),
        DescribePinType(Semantic.ContainerValuePinType));
}

bool UGraphWriteActionResolverUtils::IsFunctionBackedCreateOperation(const FString& Operation)
{
    const FString Normalized = NormalizeCreateOperation(Operation);
    return Normalized == TEXT("async_action")
        || Normalized == TEXT("function_backed_create")
        || Normalized == TEXT("function_backed_spawn")
        || Normalized == TEXT("function_backed_construct");
}

UClass* UGraphWriteActionResolverUtils::ResolveCreateWidgetNodeClass()
{
    FModuleManager::Get().LoadModule(FName(TEXT("UMGEditor")));
    return LoadObject<UClass>(nullptr, TEXT("/Script/UMGEditor.K2Node_CreateWidget"));
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeResolvedCreateResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& Operation,
    UClass* ResolvedNodeClass,
    const FString& Evidence,
    const FString& ReturnType)
{
    if (!ResolvedNodeClass || !ResolvedNodeClass->IsChildOf(UEdGraphNode::StaticClass()))
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("unsupported_create_node_class"),
            FString::Printf(TEXT("Create operation '%s' does not have a loadable UE graph node class."), *Operation));
    }

    UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(ResolvedNodeClass);
    if (!Spawner)
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("unsupported_create_node_spawner"),
            FString::Printf(TEXT("Create operation '%s' could not create a UBlueprintNodeSpawner."), *Operation));
    }

    const FString CleanEvidence = Evidence.TrimStartAndEnd().IsEmpty() ? TEXT("default") : Evidence.TrimStartAndEnd();
    const FString StableId = FString::Printf(TEXT("generic_create:%s:%s"), *Operation, *CleanEvidence);

    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = StableId;
    Candidate.DisplayName = FString::Printf(TEXT("Create %s"), *Operation);
    Candidate.Category = TEXT("GenericCreate");
    Candidate.NodeClassPath = ResolvedNodeClass->GetPathName();
    Candidate.MatchReason = FString::Printf(TEXT("create_operation=%s evidence=%s"), *Operation, *CleanEvidence);
    Candidate.ReturnType = ReturnType;
    Candidate.Score = 100;
    Candidate.bGraphCompatible = Request.TargetGraph != nullptr;
    Candidate.bFromActionDatabase = true;

    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.SelectedStableId = StableId;
    Result.SelectedSpawner = Spawner;
    Result.CandidateActions.Add(MoveTemp(Candidate));
    Result.SpawnerClass = Spawner->GetClass()->GetPathName();
    Result.NodeClass = ResolvedNodeClass->GetPathName();
    Result.MatchReason = FString::Printf(TEXT("create_operation=%s"), *Operation);
    return Result;
}

// ============================================================================
// BlueprintHelperGenericTransformScheduleActionResolver.cpp
// ============================================================================

UClass* UGraphWriteActionResolverUtils::ResolveTargetClass(const FString& ClassEvidence)
{
    const FString CleanEvidence = ClassEvidence.TrimStartAndEnd();
    if (CleanEvidence.IsEmpty())
    {
        return nullptr;
    }

    if (UClass* ExistingClass = FindObject<UClass>(nullptr, *CleanEvidence))
    {
        return ExistingClass;
    }
    return LoadObject<UClass>(nullptr, *CleanEvidence);
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeScheduleInvalidResult(
    const TCHAR* ErrorCode,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = ErrorCode;
    Result.Message = Message;
    return Result;
}

bool UGraphWriteActionResolverUtils::IsTimerOperation(const FString& Operation)
{
    return Operation.Equals(TEXT("timer_delegate_node"), ESearchCase::IgnoreCase);
}

bool UGraphWriteActionResolverUtils::IsLatentOrAsyncOperation(const FString& Operation)
{
    return Operation.Equals(TEXT("latent_or_async_node"), ESearchCase::IgnoreCase);
}

bool UGraphWriteActionResolverUtils::IsFunctionBackedTransformOperation(const FString& Operation)
{
    const FString Normalized = NormalizeOperation(Operation);
    return Normalized == TEXT("function_conversion")
        || Normalized == TEXT("blueprint_autocast")
        || Normalized == TEXT("numeric_conversion")
        || Normalized == TEXT("string_name_text_conversion")
        || Normalized == TEXT("enum_conversion")
        || Normalized == TEXT("object_to_soft_object")
        || Normalized == TEXT("class_to_soft_class");
}

bool UGraphWriteActionResolverUtils::IsFunctionBackedScheduleOperation(const FString& Operation)
{
    const FString Normalized = NormalizeOperation(Operation);
    return Normalized == TEXT("timer_by_function_name")
        || Normalized == TEXT("timer_by_handle")
        || Normalized == TEXT("timer_clear_by_handle")
        || Normalized == TEXT("timer_clear_by_function_name")
        || Normalized == TEXT("timer_pause_by_handle")
        || Normalized == TEXT("timer_pause_by_function_name")
        || Normalized == TEXT("timer_unpause_by_handle")
        || Normalized == TEXT("timer_unpause_by_function_name")
        || Normalized == TEXT("delay")
        || Normalized == TEXT("retriggerable_delay")
        || Normalized == TEXT("delay_until_next_tick")
        || Normalized == TEXT("generic_latent_function_call")
        || Normalized == TEXT("async_proxy_output_delegate_connection");
}

FBlueprintHelperActionDatabaseProjectionEvidence UGraphWriteActionResolverUtils::ToProjectionEvidence(
    const FBlueprintHelperProjectedScheduleActionEvidence& Evidence)
{
    FBlueprintHelperActionDatabaseProjectionEvidence Result;
    Result.StableId = Evidence.StableId;
    Result.NodeClassPath = Evidence.NodeClassPath;
    Result.SpawnerSignature = Evidence.SpawnerSignature;
    Result.OwnerPath = Evidence.OwnerPath;
    Result.Query = Evidence.Query;
    Result.MenuName = Evidence.MenuName;
    Result.Category = Evidence.Category;
    return Result;
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionResolverUtils::MakeScheduleCandidateInfo(
    const FBlueprintHelperActionDatabaseProjectedCandidate& Match,
    const FString& Operation)
{
    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = Match.StableId;
    Candidate.DisplayName = Match.MenuName.IsEmpty() ? TEXT("generic_schedule") : Match.MenuName;
    Candidate.Category = Match.Category;
    Candidate.NodeClassPath = Match.NodeClassPath;
    Candidate.MatchReason = FString::Printf(
        TEXT("generic_schedule operation=%s owner=%s node=%s menu=%s"),
        *Operation,
        Match.OwnerPath.IsEmpty() ? TEXT("none") : *Match.OwnerPath,
        Match.NodeClassPath.IsEmpty() ? TEXT("none") : *Match.NodeClassPath,
        Match.MenuName.IsEmpty() ? TEXT("none") : *Match.MenuName);
    Candidate.Score = 100;
    Candidate.bGraphCompatible = Match.Spawner != nullptr;
    Candidate.bFromActionDatabase = true;
    return Candidate;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeResolvedTransformResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& Operation,
    const FString& ClassEvidence,
    UClass* TargetClass,
    TSubclassOf<UEdGraphNode> ResolvedNodeClass)
{
    if (!TargetClass)
    {
        return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
            TEXT("needs_more_semantic_context"),
            TEXT("Generic cast requires target class evidence."));
    }

    if (!ResolvedNodeClass || !ResolvedNodeClass->IsChildOf(UEdGraphNode::StaticClass()))
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("unsupported_generic_transform_node_class"),
            FString::Printf(TEXT("Generic transform operation '%s' does not have a loadable UE graph node class."), *Operation));
    }

    UBlueprintNodeSpawner* Spawner =
        FBlueprintHelperGenericTransformSpawnerFactory::CreateCastSpawner(ResolvedNodeClass, TargetClass);
    if (!Spawner)
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("unsupported_generic_transform_spawner"),
            FString::Printf(TEXT("Generic transform operation '%s' could not create a UBlueprintNodeSpawner."), *Operation));
    }

    const FString StableId = FString::Printf(TEXT("generic_transform:%s:%s"), *Operation, *ClassEvidence);

    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = StableId;
    Candidate.DisplayName = FString::Printf(TEXT("Generic %s"), *Operation);
    Candidate.Category = TEXT("GenericTransform");
    Candidate.NodeClassPath = ResolvedNodeClass->GetPathName();
    Candidate.MatchReason = FString::Printf(TEXT("generic_transform operation=%s target=%s"), *Operation, *ClassEvidence);
    Candidate.ReturnType = ClassEvidence;
    Candidate.Score = 100;
    Candidate.bGraphCompatible = Request.TargetGraph != nullptr;
    Candidate.bFromActionDatabase = true;

    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.SelectedStableId = StableId;
    Result.SelectedSpawner = Spawner;
    Result.CandidateActions.Add(MoveTemp(Candidate));
    Result.SpawnerClass = Spawner->GetClass()->GetPathName();
    Result.NodeClass = ResolvedNodeClass->GetPathName();
    Result.MatchReason = FString::Printf(TEXT("generic_transform operation=%s"), *Operation);
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::ResolveConvert(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperActionClusterContextView& Context)
{
    const FString Operation = NormalizeOperation(Context.GetSemantic().TransformOperation);
    if (Operation.IsEmpty())
    {
        return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
            TEXT("needs_more_semantic_context"),
            TEXT("Generic Convert requires transform_operation."));
    }

    if (FBlueprintHelperGraphActionUtils::HasFunctionBackedOperationEvidence(Context.GetSemantic())
        || IsFunctionBackedTransformOperation(Operation))
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("function_backed_operation_wrong_owner"),
            TEXT("Function-backed convert operations must route through FunctionActionCluster."));
    }

    if (!FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedTransformOperation(Operation))
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("unsupported_generic_transform_operation"),
            FString::Printf(TEXT("Unsupported generic transform_operation '%s'."), *Operation));
    }

    if (Operation == TEXT("type_promotion"))
    {
        return FBlueprintHelperTypePromotionSpawnerEvidenceResolver::Resolve(Request, Context);
    }

    const FString ClassEvidence = FBlueprintHelperGraphActionUtils::ResolveClassEvidence(Context.GetSemantic());
    if (ClassEvidence.IsEmpty())
    {
        return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
            TEXT("needs_more_semantic_context"),
            TEXT("Generic cast requires target class evidence."));
    }

    UClass* TargetClass = ResolveTargetClass(ClassEvidence);
    if (!TargetClass)
    {
        return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
            TEXT("needs_more_semantic_context"),
            TEXT("Generic cast requires loadable target class evidence."));
    }

    return MakeResolvedTransformResult(
        Request,
        Operation,
        ClassEvidence,
        TargetClass,
        Operation == TEXT("class_cast")
            ? UK2Node_ClassDynamicCast::StaticClass()
            : UK2Node_DynamicCast::StaticClass());
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::ResolveSchedule(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperActionClusterContextView& Context)
{
    const FString Operation = NormalizeOperation(Context.GetSemantic().ScheduleOperation);
    if (Operation.IsEmpty())
    {
        return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
            TEXT("needs_more_semantic_context"),
            TEXT("Generic Schedule requires schedule_operation."));
    }

    if (FBlueprintHelperGraphActionUtils::HasFunctionBackedOperationEvidence(Context.GetSemantic())
        || IsFunctionBackedScheduleOperation(Operation))
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("function_backed_operation_wrong_owner"),
            TEXT("Function-backed schedule operations must route through FunctionActionCluster."));
    }

    if (!FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedScheduleOperation(Operation))
    {
        return FBlueprintHelperGraphActionUtils::MakeUnsupportedResult(
            TEXT("unsupported_generic_schedule_operation"),
            FString::Printf(TEXT("Unsupported generic schedule_operation '%s'."), *Operation));
    }

    const FBlueprintHelperProjectedScheduleActionEvidence Evidence =
        FBlueprintHelperProjectedSpawnerEvidence::ReadScheduleActionEvidence(Context.GetRequest());
    if (!Evidence.HasProjectedIdentity())
    {
        return MakeScheduleInvalidResult(
            TEXT("schedule_spawner_evidence_missing"),
            TEXT("Generic schedule requires projected ActionDatabase spawner identity evidence."));
    }

    if (IsTimerOperation(Operation) && !Evidence.HasTimerHandlerEvidence())
    {
        return MakeScheduleInvalidResult(
            TEXT("handler_evidence_missing"),
            TEXT("timer_delegate_node requires projected handler and signature evidence from BlueprintSignature or ActionContext."));
    }

    if (IsLatentOrAsyncOperation(Operation) && !Evidence.IsGraphLatentAllowed())
    {
        return MakeScheduleInvalidResult(
            TEXT("latent_function_not_allowed_in_graph"),
            TEXT("latent_or_async_node requires graph_latent_allowed=true evidence."));
    }

    FBlueprintHelperActionDatabaseProjectionRequest ProjectionRequest;
    ProjectionRequest.Blueprint = Context.GetRequest().Blueprint;
    ProjectionRequest.TargetGraph = Context.GetRequest().TargetGraph;
    ProjectionRequest.RequiredEvidence = ToProjectionEvidence(Evidence);
    ProjectionRequest.Query = Evidence.Query;
    ProjectionRequest.ErrorPrefix = TEXT("schedule");

    const FBlueprintHelperActionDatabaseProjectionResult Projection =
        FBlueprintHelperActionDatabaseProjectionService::Project(ProjectionRequest);
    if (Projection.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest)
    {
        return MakeScheduleInvalidResult(
            Projection.ErrorCode.IsEmpty() ? TEXT("schedule_spawner_evidence_missing") : *Projection.ErrorCode,
            Projection.Message.IsEmpty()
                ? TEXT("Generic schedule requires projected ActionDatabase spawner evidence.")
                : Projection.Message);
    }
    if (Projection.Status == EBlueprintHelperActionResolutionStatus::NotFound || Projection.Candidates.Num() == 0)
    {
        FBlueprintHelperActionResolutionResult Result;
        Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
        Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
        Result.ErrorCode = TEXT("schedule_spawner_not_found");
        Result.Message = TEXT("Generic schedule projected evidence did not match any current ActionDatabase spawner.");
        return Result;
    }
    if (Projection.Status == EBlueprintHelperActionResolutionStatus::Ambiguous || Projection.Candidates.Num() > 1)
    {
        FBlueprintHelperActionResolutionResult Result;
        Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
        Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
        Result.ErrorCode = TEXT("schedule_spawner_ambiguous");
        Result.Message = Projection.Message;
        return Result;
    }

    const FBlueprintHelperActionDatabaseProjectedCandidate& Match = Projection.Candidates[0];
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.SelectedStableId = Match.StableId;
    Result.SelectedSpawner = Match.Spawner;
    Result.SpawnerClass = Match.Spawner ? Match.Spawner->GetClass()->GetPathName() : FString();
    Result.NodeClass = Match.NodeClassPath;
    Result.MatchReason = FString::Printf(TEXT("generic_schedule operation=%s"), *Operation);
    Result.CandidateActions.Add(MakeScheduleCandidateInfo(Match, Operation));
    Result.Message = FString::Printf(
        TEXT("Resolved generic schedule operation '%s' from ActionDatabase spawner '%s'."),
        *Operation,
        *Match.StableId);
    return Result;
}

// ============================================================================
// BlueprintHelperSingletonControlFlowEvidenceProvider.cpp
// ============================================================================

const TCHAR* UGraphWriteActionResolverUtils::SingletonKindToStableName(
    const EBlueprintHelperSingletonControlFlowKind Kind)
{
    switch (Kind)
    {
    case EBlueprintHelperSingletonControlFlowKind::Branch:
        return TEXT("branch");
    case EBlueprintHelperSingletonControlFlowKind::Sequence:
        return TEXT("sequence");
    case EBlueprintHelperSingletonControlFlowKind::Return:
        return TEXT("return");
    case EBlueprintHelperSingletonControlFlowKind::Select:
        return TEXT("select");
    default:
        return TEXT("unknown");
    }
}

const TCHAR* UGraphWriteActionResolverUtils::SingletonKindToDisplayName(
    const EBlueprintHelperSingletonControlFlowKind Kind)
{
    switch (Kind)
    {
    case EBlueprintHelperSingletonControlFlowKind::Branch:
        return TEXT("Branch");
    case EBlueprintHelperSingletonControlFlowKind::Sequence:
        return TEXT("Sequence");
    case EBlueprintHelperSingletonControlFlowKind::Return:
        return TEXT("Return");
    case EBlueprintHelperSingletonControlFlowKind::Select:
        return TEXT("Select");
    default:
        return TEXT("Unknown");
    }
}

FString UGraphWriteActionResolverUtils::SingletonKindToQuery(
    const EBlueprintHelperSingletonControlFlowKind Kind)
{
    switch (Kind)
    {
    case EBlueprintHelperSingletonControlFlowKind::Branch:
        return TEXT("branch");
    case EBlueprintHelperSingletonControlFlowKind::Sequence:
        return TEXT("sequence");
    case EBlueprintHelperSingletonControlFlowKind::Return:
        return TEXT("return");
    case EBlueprintHelperSingletonControlFlowKind::Select:
        return FString();
    default:
        return FString();
    }
}

EBlueprintHelperActionSemanticKind UGraphWriteActionResolverUtils::SingletonKindToSemanticKind(
    const EBlueprintHelperSingletonControlFlowKind Kind)
{
    return Kind == EBlueprintHelperSingletonControlFlowKind::Select
        ? EBlueprintHelperActionSemanticKind::Select
        : EBlueprintHelperActionSemanticKind::Control;
}

void UGraphWriteActionResolverUtils::AppendObjectIdentity(
    FString& Stable, const TCHAR* Label, const UObject* Object)
{
    Stable += Label;
    Stable += TEXT("_path=");
    Stable += Object ? Object->GetPathName() : FString();
    Stable += TEXT("|");
    Stable += Label;
    Stable += TEXT("_name=");
    Stable += Object ? Object->GetName() : FString();
}

FString UGraphWriteActionResolverUtils::BuildSingletonCanonicalStableFields(
    const EBlueprintHelperSingletonControlFlowKind Kind,
    const EBlueprintHelperActionSemanticKind SemanticKind,
    UBlueprint* Blueprint,
    UEdGraph* TargetGraph,
    const FString& StatementId,
    const FString& Query)
{
    FString Stable;
    Stable += TEXT("singleton_control_flow|kind=");
    Stable += SingletonKindToStableName(Kind);
    Stable += TEXT("|semantic=");
    Stable += FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind);
    Stable += TEXT("|query=");
    Stable += Query;
    Stable += TEXT("|");
    AppendObjectIdentity(Stable, TEXT("blueprint"), Blueprint);
    Stable += TEXT("|");
    AppendObjectIdentity(Stable, TEXT("target_graph"), TargetGraph);
    Stable += TEXT("|statement=");
    Stable += StatementId;
    return Stable;
}

bool UGraphWriteActionResolverUtils::MakeEvidence(
    const EBlueprintHelperSingletonControlFlowKind Kind,
    TSubclassOf<UEdGraphNode> NodeClass,
    FBlueprintHelperSingletonControlFlowEvidence& OutEvidence)
{
    OutEvidence = FBlueprintHelperSingletonControlFlowEvidence();
    if (!NodeClass)
    {
        return false;
    }

    const FString KindName = SingletonKindToStableName(Kind);
    const FString NodeClassPath = NodeClass->GetPathName();
    OutEvidence.SingletonKind = Kind;
    OutEvidence.NodeClass = NodeClass;
    OutEvidence.StableId = FString::Printf(
        TEXT("singleton_control_flow:%s:%s"),
        *KindName,
        *NodeClassPath);
    OutEvidence.Reason = FString::Printf(
        TEXT("GenericAssetStructControl singleton control-flow provider resolved canonical %s node class %s."),
        *KindName,
        *NodeClassPath);
    return true;
}

// ============================================================================
// BlueprintHelperStructTypeStructureActionResolver.cpp
// ============================================================================

bool UGraphWriteActionResolverUtils::IsStructTypeStructureRequest(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    const bool bStructFamily = Request.Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::Struct
        || Request.Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::TypeStructure;
    const bool bTypeOperation = Request.Semantic.TypeOperation == EBlueprintHelperTypeOperation::Construct
        || Request.Semantic.TypeOperation == EBlueprintHelperTypeOperation::Deconstruct;
    return bStructFamily && bTypeOperation;
}

bool UGraphWriteActionResolverUtils::IsConstructOperation(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    return Request.Semantic.TypeOperation == EBlueprintHelperTypeOperation::Construct;
}

FString UGraphWriteActionResolverUtils::NormalizeNativeFunctionPath(const FString& RawPath)
{
    FString Path = RawPath.TrimStartAndEnd();
    if (Path.IsEmpty() || Path.Contains(TEXT(":")))
    {
        return Path;
    }

    int32 LastDotIndex = INDEX_NONE;
    if (Path.FindLastChar(TEXT('.'), LastDotIndex) && LastDotIndex > 0 && LastDotIndex < Path.Len() - 1)
    {
        Path = Path.Left(LastDotIndex) + TEXT(":") + Path.Mid(LastDotIndex + 1);
    }
    return Path;
}

FString UGraphWriteActionResolverUtils::NormalizeStructLookupText(const FString& TypeName)
{
    FString Normalized = TypeName.TrimStartAndEnd();
    Normalized.ToLowerInline();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    if (Normalized.StartsWith(TEXT("struct")))
    {
        Normalized.RightChopInline(6);
    }
    if (Normalized.StartsWith(TEXT("f")))
    {
        Normalized.RightChopInline(1);
    }
    return Normalized;
}

UScriptStruct* UGraphWriteActionResolverUtils::ResolveKnownStructAlias(const FString& TypeName)
{
    const FString Normalized = NormalizeStructLookupText(TypeName);
    if (Normalized == TEXT("vector"))
    {
        return TBaseStructure<FVector>::Get();
    }
    if (Normalized == TEXT("vector2d"))
    {
        return TBaseStructure<FVector2D>::Get();
    }
    if (Normalized == TEXT("rotator"))
    {
        return TBaseStructure<FRotator>::Get();
    }
    if (Normalized == TEXT("transform"))
    {
        return TBaseStructure<FTransform>::Get();
    }
    if (Normalized == TEXT("linearcolor") || Normalized == TEXT("color"))
    {
        return TBaseStructure<FLinearColor>::Get();
    }
    return nullptr;
}

UScriptStruct* UGraphWriteActionResolverUtils::ResolveStructType(const FString& TypeName)
{
    const FString Query = TypeName.TrimStartAndEnd();
    if (Query.IsEmpty())
    {
        return nullptr;
    }

    if (UScriptStruct* DirectStruct = FindObject<UScriptStruct>(nullptr, *Query))
    {
        return DirectStruct;
    }
    if (UScriptStruct* LoadedStruct = LoadObject<UScriptStruct>(nullptr, *Query))
    {
        return LoadedStruct;
    }
    if (UScriptStruct* KnownAlias = ResolveKnownStructAlias(Query))
    {
        return KnownAlias;
    }

    return nullptr;
}

FString UGraphWriteActionResolverUtils::SemanticTypeFromProperty(const FProperty* Property)
{
    if (!Property)
    {
        return FString();
    }
    if (Property->IsA<FBoolProperty>())
    {
        return TEXT("bool");
    }
    if (Property->IsA<FIntProperty>() || Property->IsA<FUInt32Property>() || Property->IsA<FByteProperty>())
    {
        return TEXT("int");
    }
    if (Property->IsA<FInt64Property>())
    {
        return TEXT("int64");
    }
    if (Property->IsA<FFloatProperty>())
    {
        return TEXT("float");
    }
    if (Property->IsA<FDoubleProperty>())
    {
        return TEXT("double");
    }
    if (Property->IsA<FStrProperty>())
    {
        return TEXT("string");
    }
    if (Property->IsA<FNameProperty>())
    {
        return TEXT("name");
    }
    if (Property->IsA<FTextProperty>())
    {
        return TEXT("text");
    }
    if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        return StructProperty->Struct ? StructProperty->Struct->GetPathName() : FString();
    }
    if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        return ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetPathName() : FString();
    }
    if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
    {
        return ClassProperty->MetaClass ? ClassProperty->MetaClass->GetPathName() : FString(TEXT("class"));
    }
    return FString();
}

void UGraphWriteActionResolverUtils::AddUniqueQuery(TArray<FString>& Queries, const FString& Query)
{
    const FString Trimmed = Query.TrimStartAndEnd();
    if (!Trimmed.IsEmpty() && !Queries.Contains(Trimmed))
    {
        Queries.Add(Trimmed);
    }
}

void UGraphWriteActionResolverUtils::AddUniqueCandidateInfo(
    TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
    const FBlueprintHelperCallFunctionCandidateInfo& Candidate)
{
    if (Candidate.StableId.IsEmpty())
    {
        CandidateActions.Add(Candidate);
        return;
    }

    for (const FBlueprintHelperCallFunctionCandidateInfo& Existing : CandidateActions)
    {
        if (Existing.StableId == Candidate.StableId)
        {
            return;
        }
    }
    CandidateActions.Add(Candidate);
}

void UGraphWriteActionResolverUtils::AppendCandidateDiagnostics(
    TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
    const FBlueprintHelperActionResolutionResult& FunctionResult,
    const int32 MaxCandidates)
{
    for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : FunctionResult.CandidateActions)
    {
        if (MaxCandidates > 0 && CandidateActions.Num() >= MaxCandidates)
        {
            return;
        }
        AddUniqueCandidateInfo(CandidateActions, Candidate);
    }
}

FProperty* UGraphWriteActionResolverUtils::FindStructPropertyBySemanticName(
    const UScriptStruct* TargetStruct, const FString& Name)
{
    if (!TargetStruct)
    {
        return nullptr;
    }

    if (FProperty* DirectProperty = FindFProperty<FProperty>(TargetStruct, *Name))
    {
        return DirectProperty;
    }

    for (TFieldIterator<FProperty> It(TargetStruct); It; ++It)
    {
        FProperty* Property = *It;
        if (Property
            && (Property->GetName().Equals(Name, ESearchCase::IgnoreCase)
                || Property->GetDisplayNameText().ToString().Equals(Name, ESearchCase::IgnoreCase)))
        {
            return Property;
        }
    }
    return nullptr;
}

void UGraphWriteActionResolverUtils::PopulateFunctionArgumentConstraintsFromStruct(
    const UScriptStruct* TargetStruct,
    const FBlueprintHelperActionSemanticConstraints& SourceSemantic,
    FBlueprintHelperActionSemanticConstraints& FunctionSemantic)
{
    FunctionSemantic.ArgumentNames = SourceSemantic.ArgumentNames;
    FunctionSemantic.ArgumentTypes = SourceSemantic.ArgumentTypes;
    FunctionSemantic.ArgumentPinTypes = SourceSemantic.ArgumentPinTypes;

    TArray<FString> ArgumentTypeNames;
    SourceSemantic.ArgumentTypes.GetKeys(ArgumentTypeNames);
    for (const FString& ArgumentName : ArgumentTypeNames)
    {
        FunctionSemantic.ArgumentNames.AddUnique(ArgumentName);
    }

    TArray<FString> ArgumentPinTypeNames;
    SourceSemantic.ArgumentPinTypes.GetKeys(ArgumentPinTypeNames);
    for (const FString& ArgumentName : ArgumentPinTypeNames)
    {
        FunctionSemantic.ArgumentNames.AddUnique(ArgumentName);
    }

    for (const TPair<FString, FString>& DefaultPair : SourceSemantic.DefaultValues)
    {
        FunctionSemantic.ArgumentNames.AddUnique(DefaultPair.Key);
        if (!FunctionSemantic.ArgumentTypes.Contains(DefaultPair.Key))
        {
            const FString SemanticType = SemanticTypeFromProperty(FindStructPropertyBySemanticName(TargetStruct, DefaultPair.Key));
            if (!SemanticType.IsEmpty())
            {
                FunctionSemantic.ArgumentTypes.Add(DefaultPair.Key, SemanticType);
            }
        }
    }
}

FBlueprintHelperActionResolutionRequest UGraphWriteActionResolverUtils::MakeFunctionActionRequest(
    const FBlueprintHelperActionResolutionRequest& Request,
    UScriptStruct* TargetStruct,
    const FString& Query,
    const FString& SearchMode,
    const bool bConstruct)
{
    FBlueprintHelperActionResolutionRequest FunctionRequest;
    FunctionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
    FunctionRequest.Blueprint = Request.Blueprint ? Request.Blueprint : FBlueprintEditorUtils::FindBlueprintForGraph(Request.TargetGraph);
    FunctionRequest.TargetGraph = Request.TargetGraph;
    FunctionRequest.bAllowFuzzyUnique = Request.bAllowFuzzyUnique;
    FunctionRequest.MaxCandidates = Request.MaxCandidates;
    FunctionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Call;
    FunctionRequest.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
    FunctionRequest.Semantic.Query = Query;
    FunctionRequest.Semantic.SearchMode = SearchMode;
    FunctionRequest.Semantic.AmbiguityPolicy = TEXT("pick_best");
    FunctionRequest.Semantic.CategoryPriority = Request.Semantic.CategoryPriority;
    FunctionRequest.Semantic.TargetObjectType = Request.Semantic.TargetObjectType;
    FunctionRequest.Semantic.TargetObjectPinType = Request.Semantic.TargetObjectPinType;

    PopulateFunctionArgumentConstraintsFromStruct(TargetStruct, Request.Semantic, FunctionRequest.Semantic);

    if (bConstruct)
    {
        FunctionRequest.Semantic.ExpectedReturnType = TargetStruct ? TargetStruct->GetPathName() : FString();
        FunctionRequest.Semantic.ExpectedReturnPinType = Request.Semantic.ExpectedReturnPinType;
    }

    return FunctionRequest;
}

void UGraphWriteActionResolverUtils::PopulateStructTypeStructureEvidence(
    FBlueprintHelperActionResolutionResult& InOutResult,
    const FBlueprintHelperActionResolutionRequest& Request,
    const UScriptStruct* TargetStruct,
    const FString& MatchReason,
    const UClass* NodeClass)
{
    InOutResult.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    InOutResult.StructPath = TargetStruct ? TargetStruct->GetPathName() : Request.Semantic.StructPath;
    InOutResult.TypeStructureId = Request.Semantic.TypeStructureId.IsEmpty()
        ? InOutResult.StructPath
        : Request.Semantic.TypeStructureId;
    InOutResult.TypeOperation = FBlueprintHelperActionResolutionCore::TypeOperationToString(Request.Semantic.TypeOperation);
    InOutResult.SpawnerClass = InOutResult.SelectedSpawner.IsValid()
        ? InOutResult.SelectedSpawner->GetClass()->GetPathName()
        : FString();
    InOutResult.NodeClass = NodeClass ? NodeClass->GetPathName() : FString();
    InOutResult.MatchReason = MatchReason;
}

bool UGraphWriteActionResolverUtils::TryResolveFunctionActionSpawner(
    const FBlueprintHelperActionResolutionRequest& Request,
    UScriptStruct* TargetStruct,
    const FString& Query,
    const FString& SearchMode,
    const bool bConstruct,
    const FString& AttemptLabel,
    TArray<FString>& AttemptMessages,
    TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
    FBlueprintHelperActionResolutionResult& OutResult)
{
    const FString TrimmedQuery = Query.TrimStartAndEnd();
    if (TrimmedQuery.IsEmpty())
    {
        return false;
    }

    FBlueprintHelperActionResolutionRequest FunctionRequest =
        MakeFunctionActionRequest(Request, TargetStruct, TrimmedQuery, SearchMode, bConstruct);
    FBlueprintHelperActionResolutionResult FunctionResult = FBlueprintHelperActionResolutionCore::Resolve(FunctionRequest);
    AppendCandidateDiagnostics(CandidateActions, FunctionResult, Request.MaxCandidates);

    if (FunctionResult.IsResolved() && FunctionResult.SelectedSpawner.IsValid())
    {
        OutResult = FunctionResult;
        OutResult.CandidateActions = CandidateActions;
        OutResult.Message = FString::Printf(
            TEXT("Resolved %s for struct '%s' via %s FunctionAction query '%s': %s"),
            *FBlueprintHelperActionResolutionCore::TypeOperationToString(Request.Semantic.TypeOperation),
            TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"),
            *AttemptLabel,
            *TrimmedQuery,
            *FunctionResult.Message);
        PopulateStructTypeStructureEvidence(
            OutResult,
            Request,
            TargetStruct,
            bConstruct ? TEXT("type_operation_construct_native_function_action") : TEXT("type_operation_deconstruct_native_function_action"),
            nullptr);
        return true;
    }

    AttemptMessages.Add(FString::Printf(
        TEXT("%s query '%s' status=%d error=%s message=%s"),
        *AttemptLabel,
        *TrimmedQuery,
        static_cast<int32>(FunctionResult.Status),
        FunctionResult.ErrorCode.IsEmpty() ? TEXT("<none>") : *FunctionResult.ErrorCode,
        FunctionResult.Message.IsEmpty() ? TEXT("<none>") : *FunctionResult.Message));
    return false;
}

UFunction* UGraphWriteActionResolverUtils::ResolveNativeStructFunction(const FString& NativePath)
{
    const FString Trimmed = NativePath.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return nullptr;
    }
    if (UFunction* DirectFunction = FindObject<UFunction>(nullptr, *Trimmed))
    {
        return DirectFunction;
    }
    const FString DotPath = Trimmed.Replace(TEXT(":"), TEXT("."));
    if (DotPath != Trimmed)
    {
        return FindObject<UFunction>(nullptr, *DotPath);
    }
    return nullptr;
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionResolverUtils::MakeNativeStructFunctionCandidateInfo(
    const UFunction* Function,
    const bool bConstruct,
    const UScriptStruct* TargetStruct)
{
    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = FString::Printf(
        TEXT("type_operation:%s:function:%s:%s"),
        bConstruct ? TEXT("construct") : TEXT("deconstruct"),
        Function && Function->GetOwnerClass() ? *Function->GetOwnerClass()->GetPathName() : TEXT("<owner>"),
        Function ? *Function->GetName() : TEXT("<function>"));
    Candidate.DisplayName = Function ? Function->GetName() : FString();
    Candidate.OwnerClassPath = Function && Function->GetOwnerClass() ? Function->GetOwnerClass()->GetPathName() : FString();
    Candidate.NativeFunctionName = Function ? Function->GetName() : FString();
    Candidate.Category = TEXT("Struct");
    Candidate.NodeClassPath = UBlueprintFunctionNodeSpawner::StaticClass()->GetPathName();
    Candidate.MatchReason = bConstruct ? TEXT("type_operation_construct_native_function_spawner") : TEXT("type_operation_deconstruct_native_function_spawner");
    Candidate.ReturnType = bConstruct && TargetStruct ? TargetStruct->GetPathName() : FString();
    Candidate.Score = 100;
    Candidate.bGraphCompatible = true;
    Candidate.bFromActionDatabase = false;
    Candidate.bBlueprintCallable = true;
    Candidate.bBlueprintPure = Function ? Function->HasAnyFunctionFlags(FUNC_BlueprintPure) : true;
    return Candidate;
}

bool UGraphWriteActionResolverUtils::TryResolveNativeStructFunctionSpawner(
    const FString& NativeFunctionPath,
    const FBlueprintHelperActionResolutionRequest& Request,
    UScriptStruct* TargetStruct,
    const bool bConstruct,
    TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
    FBlueprintHelperActionResolutionResult& OutResult)
{
    UFunction* NativeFunction = ResolveNativeStructFunction(NativeFunctionPath);
    if (!NativeFunction)
    {
        return false;
    }

    UBlueprintFunctionNodeSpawner* NativeSpawner = UBlueprintFunctionNodeSpawner::Create(NativeFunction);
    if (!NativeSpawner)
    {
        return false;
    }

    FBlueprintHelperCallFunctionCandidateInfo Candidate =
        MakeNativeStructFunctionCandidateInfo(NativeFunction, bConstruct, TargetStruct);
    AddUniqueCandidateInfo(CandidateActions, Candidate);

    OutResult = FBlueprintHelperActionResolutionResult();
    OutResult.Status = EBlueprintHelperActionResolutionStatus::Resolved;
    OutResult.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    OutResult.SelectedStableId = Candidate.StableId;
    OutResult.SelectedSpawner = NativeSpawner;
    OutResult.SelectedFunction = NativeFunction;
    OutResult.CandidateActions = CandidateActions;
    OutResult.Message = FString::Printf(
        TEXT("Resolved %s for struct '%s' via native function spawner '%s'."),
        *FBlueprintHelperActionResolutionCore::TypeOperationToString(Request.Semantic.TypeOperation),
        TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"),
        *Candidate.StableId);
    PopulateStructTypeStructureEvidence(
        OutResult,
        Request,
        TargetStruct,
        Candidate.MatchReason,
        nullptr);
    return true;
}

void UGraphWriteActionResolverUtils::SetStructTypeOnNode(
    UEdGraphNode* NewNode, FFieldVariant /*StructField*/, TWeakObjectPtr<UScriptStruct> StructPtr)
{
    if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(NewNode))
    {
        StructNode->StructType = StructPtr.Get();
    }
}

UBlueprintFieldNodeSpawner* UGraphWriteActionResolverUtils::CreateDirectStructSpawner(
    UClass* NodeClass, UScriptStruct* TargetStruct)
{
    if (!NodeClass || !TargetStruct)
    {
        return nullptr;
    }

    UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(NodeClass, TargetStruct);
    if (!NodeSpawner)
    {
        return nullptr;
    }

    NodeSpawner->SetNodeFieldDelegate = UBlueprintFieldNodeSpawner::FSetNodeFieldDelegate::CreateStatic(
        &SetStructTypeOnNode,
        MakeWeakObjectPtr(TargetStruct));
    return NodeSpawner;
}

FString UGraphWriteActionResolverUtils::MakeDirectStructStableId(
    const EBlueprintHelperTypeOperation Operation, const UScriptStruct* TargetStruct)
{
    return FString::Printf(
        TEXT("type_operation:%s:struct:%s"),
        *FBlueprintHelperActionResolutionCore::TypeOperationToString(Operation),
        TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"));
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionResolverUtils::MakeDirectStructCandidateInfo(
    const FBlueprintHelperActionResolutionRequest& Request,
    const UScriptStruct* TargetStruct,
    const UClass* NodeClass)
{
    const bool bConstruct = IsConstructOperation(Request);
    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = MakeDirectStructStableId(Request.Semantic.TypeOperation, TargetStruct);
    Candidate.DisplayName = FString::Printf(
        TEXT("%s %s"),
        bConstruct ? TEXT("Construct") : TEXT("Deconstruct"),
        TargetStruct ? *TargetStruct->GetDisplayNameText().ToString() : TEXT("<null>"));
    Candidate.Category = TEXT("Struct");
    Candidate.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
    Candidate.MatchReason = bConstruct
        ? TEXT("type_operation_construct_struct_field_spawner_boundary")
        : TEXT("type_operation_deconstruct_struct_field_spawner_boundary");
    Candidate.ReturnType = bConstruct && TargetStruct ? TargetStruct->GetPathName() : FString();
    Candidate.Score = 100;
    Candidate.bGraphCompatible = true;
    Candidate.bFromActionDatabase = false;
    Candidate.bBlueprintCallable = true;
    Candidate.bBlueprintPure = true;
    if (!bConstruct && TargetStruct)
    {
        Candidate.InputPins.Add(TargetStruct->GetName());
        Candidate.InputPinTypes.Add(TargetStruct->GetName(), TargetStruct->GetPathName());
    }
    return Candidate;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeDirectStructSpawnerResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    UScriptStruct* TargetStruct,
    const TArray<FString>& AttemptMessages,
    const TArray<FBlueprintHelperCallFunctionCandidateInfo>& FunctionCandidateActions)
{
    const bool bConstruct = IsConstructOperation(Request);
    UClass* NodeClass = bConstruct ? UK2Node_MakeStruct::StaticClass() : UK2Node_BreakStruct::StaticClass();
    UBlueprintFieldNodeSpawner* DirectSpawner = CreateDirectStructSpawner(NodeClass, TargetStruct);

    FBlueprintHelperActionResolutionResult Result;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.CandidateActions = FunctionCandidateActions;
    const FBlueprintHelperCallFunctionCandidateInfo Candidate =
        MakeDirectStructCandidateInfo(Request, TargetStruct, NodeClass);
    AddUniqueCandidateInfo(Result.CandidateActions, Candidate);

    if (!DirectSpawner)
    {
        Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
        Result.ErrorCode = TEXT("struct_type_operation_spawner_unavailable");
        Result.Message = FString::Printf(
            TEXT("Could not create direct struct type-operation UBlueprintFieldNodeSpawner for '%s'."),
            TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"));
        return Result;
    }

    Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
    Result.Message = FString::Printf(
        TEXT("Resolved %s for struct '%s' through the GenericAssetStructControl UBlueprintFieldNodeSpawner boundary after UE FunctionAction attempts were not applicable%s%s"),
        *FBlueprintHelperActionResolutionCore::TypeOperationToString(Request.Semantic.TypeOperation),
        TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"),
        AttemptMessages.Num() > 0 ? TEXT(": ") : TEXT("."),
        AttemptMessages.Num() > 0 ? *FString::Join(AttemptMessages, TEXT(" | ")) : TEXT(""));
    Result.SelectedStableId = Candidate.StableId;
    Result.SelectedSpawner = DirectSpawner;
    PopulateStructTypeStructureEvidence(Result, Request, TargetStruct, Candidate.MatchReason, NodeClass);
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeNeedsContextResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("needs_more_semantic_context");
    Result.Message = Message;
    Result.TypeOperation = FBlueprintHelperActionResolutionCore::TypeOperationToString(Request.Semantic.TypeOperation);
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeStructTypeNotFoundResult(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FString& Message)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("not_found");
    Result.Message = Message;
    Result.TypeOperation = FBlueprintHelperActionResolutionCore::TypeOperationToString(Request.Semantic.TypeOperation);
    return Result;
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeInvalidSemanticResult(
    const FBlueprintHelperActionResolutionRequest& Request)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("invalid_struct_type_operation_semantic");
    Result.Message = FString::Printf(
        TEXT("Struct/TypeStructure resolver requires SemanticFamily=Struct|TypeStructure and TypeOperation=Construct|Deconstruct. family=%s operation=%s kind=%s"),
        *FBlueprintHelperActionResolutionCore::SemanticFamilyToString(Request.Semantic.SemanticFamily),
        *FBlueprintHelperActionResolutionCore::TypeOperationToString(Request.Semantic.TypeOperation),
        *FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
    return Result;
}

// ============================================================================
// BlueprintHelperTypePromotionSpawnerEvidenceResolver.cpp
// ============================================================================

FString UGraphWriteActionResolverUtils::NormalizeTypePromotionToken(const FString& Value)
{
    FString Result = Value.TrimStartAndEnd().ToLower();
    Result.ReplaceInline(TEXT(" "), TEXT(""));
    Result.ReplaceInline(TEXT("_"), TEXT(""));
    Result.ReplaceInline(TEXT("-"), TEXT(""));
    return Result;
}

bool UGraphWriteActionResolverUtils::TryBuildPrimitivePinType(
    const FString& TypeToken, FEdGraphPinType& OutPinType)
{
    OutPinType = FEdGraphPinType();

    const FString Normalized = NormalizeTypePromotionToken(TypeToken);
    if (Normalized.IsEmpty())
    {
        return false;
    }

    if (Normalized == TEXT("int") || Normalized == TEXT("int32") || Normalized == TEXT("integer"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        return true;
    }
    if (Normalized == TEXT("int64") || Normalized == TEXT("integer64"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
        return true;
    }
    if (Normalized == TEXT("byte"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
        return true;
    }
    if (Normalized == TEXT("bool") || Normalized == TEXT("boolean"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        return true;
    }
    if (Normalized == TEXT("float") || Normalized == TEXT("single"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        return true;
    }
    if (Normalized == TEXT("double") || Normalized == TEXT("real"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
        return true;
    }
    if (Normalized == TEXT("wildcard"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        return true;
    }

    return false;
}

bool UGraphWriteActionResolverUtils::IsPromotionCompatible(
    const FEdGraphPinType& SourcePinType, const FEdGraphPinType& TargetPinType)
{
    return SourcePinType == TargetPinType
        || FTypePromotion::IsValidPromotion(SourcePinType, TargetPinType);
}

UBlueprintFunctionNodeSpawner* UGraphWriteActionResolverUtils::FindRegisteredTypePromotionSpawner(
    FName OperatorName)
{
    if (OperatorName.IsNone())
    {
        return nullptr;
    }

    FTypePromotion::Get();
    if (UBlueprintFunctionNodeSpawner* Spawner = FTypePromotion::GetOperatorSpawner(OperatorName))
    {
        return Spawner;
    }

    FBlueprintActionDatabase::Get().RefreshAll();
    return FTypePromotion::GetOperatorSpawner(OperatorName);
}

FBlueprintHelperActionResolutionResult UGraphWriteActionResolverUtils::MakeNotFoundResult(
    const FBlueprintHelperProjectedTypePromotionEvidence& Evidence)
{
    FBlueprintHelperActionResolutionResult Result;
    Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
    Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
    Result.ErrorCode = TEXT("type_promotion_spawner_not_found");
    Result.Message = FString::Printf(
        TEXT("FTypePromotion did not expose a registered operator spawner for '%s'."),
        Evidence.OperatorName.IsEmpty() ? TEXT("none") : *Evidence.OperatorName);
    Result.MatchReason = FString::Printf(
        TEXT("type_promotion operator=%s source=%s target=%s provider=FTypePromotion spawner=not_found"),
        Evidence.OperatorName.IsEmpty() ? TEXT("none") : *Evidence.OperatorName,
        Evidence.SourcePinType.IsEmpty() ? TEXT("none") : *Evidence.SourcePinType,
        Evidence.TargetPinType.IsEmpty() ? TEXT("none") : *Evidence.TargetPinType);
    return Result;
}

FBlueprintHelperCallFunctionCandidateInfo UGraphWriteActionResolverUtils::MakeCandidateInfo(
    const FString& StableId,
    const FBlueprintHelperProjectedTypePromotionEvidence& Evidence,
    UBlueprintFunctionNodeSpawner* Spawner)
{
    FBlueprintHelperCallFunctionCandidateInfo Candidate;
    Candidate.StableId = StableId;
    Candidate.DisplayName = Evidence.OperatorName;
    Candidate.Category = TEXT("Utilities|Operators");
    Candidate.NodeClassPath = UK2Node_PromotableOperator::StaticClass()->GetPathName();
    Candidate.MatchReason = FString::Printf(
        TEXT("type_promotion operator=%s source=%s target=%s provider=FTypePromotion"),
        *Evidence.OperatorName,
        *Evidence.SourcePinType,
        *Evidence.TargetPinType);
    Candidate.ReturnType = Evidence.ResultPinType.IsEmpty() ? Evidence.TargetPinType : Evidence.ResultPinType;
    Candidate.Score = 100;
    Candidate.bGraphCompatible = Spawner != nullptr;
    Candidate.bFromActionDatabase = true;
    Candidate.bBlueprintCallable = true;
    Candidate.bBlueprintPure = true;
    return Candidate;
}
