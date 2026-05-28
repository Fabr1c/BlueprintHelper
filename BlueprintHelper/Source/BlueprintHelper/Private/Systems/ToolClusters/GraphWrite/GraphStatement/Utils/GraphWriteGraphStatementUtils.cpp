#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

#include "BlueprintNodeSpawner.h"
#include "BlueprintNodeBinder.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Select.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/FieldIterator.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphComposerUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEvidenceWrappers.h"

// ============================================================================
// BlueprintHelperActionFragmentSpawnCoordinator.cpp
// ============================================================================
void UGraphWriteGraphStatementUtils::AddOwnershipTagIfPresent(
    FBlueprintHelperNodeFragment& Fragment,
    const FString& Key,
    const FString& Value)
{
    const FString CleanValue = Value.TrimStartAndEnd();
    if (!Key.IsEmpty() && !CleanValue.IsEmpty())
    {
        Fragment.OwnershipTags.FindOrAdd(Key, CleanValue);
    }
}

void UGraphWriteGraphStatementUtils::AppendResolvedActionCandidateFacts(
    const FBlueprintHelperActionResolutionResult& ActionResult,
    FBlueprintHelperNodeFragment& OutFragment)
{
    if (ActionResult.CandidateActions.Num() == 0)
    {
        return;
    }

    const FBlueprintHelperActionCandidate& Candidate = ActionResult.CandidateActions[0];
    AddOwnershipTagIfPresent(OutFragment, TEXT("capability_id"), Candidate.CapabilityId);
    AddOwnershipTagIfPresent(OutFragment, TEXT("expected_node_family"), Candidate.ExpectedNodeFamily);
    AddOwnershipTagIfPresent(OutFragment, TEXT("expected_node_class"), Candidate.ExpectedNodeClassPath);
    AddOwnershipTagIfPresent(OutFragment, TEXT("node_class"), Candidate.NodeClassPath);

    for (const TPair<FString, FString>& FactPair : Candidate.CapabilityFacts)
    {
        AddOwnershipTagIfPresent(OutFragment, TEXT("capability.") + FactPair.Key, FactPair.Value);
    }
    for (const TPair<FString, FString>& FactPair : Candidate.ReadbackFacts)
    {
        AddOwnershipTagIfPresent(OutFragment, TEXT("readback.") + FactPair.Key, FactPair.Value);
    }
}

// ============================================================================
// BlueprintHelperGraphSemanticIR.cpp
// ============================================================================
bool UGraphWriteGraphStatementUtils::IsBoolProducingOperator(const FString& Operator)
{
    const FString Token = Operator.TrimStartAndEnd().ToLower();
    return Token == TEXT(">")
        || Token == TEXT(">=")
        || Token == TEXT("<")
        || Token == TEXT("<=")
        || Token == TEXT("==")
        || Token == TEXT("=")
        || Token == TEXT("!=")
        || Token == TEXT("<>")
        || Token == TEXT("gt")
        || Token == TEXT("gte")
        || Token == TEXT("lt")
        || Token == TEXT("lte")
        || Token == TEXT("eq")
        || Token == TEXT("ne")
        || Token == TEXT("equal")
        || Token == TEXT("equals")
        || Token == TEXT("not_equal")
        || Token == TEXT("notequal")
        || Token == TEXT("and")
        || Token == TEXT("or")
        || Token == TEXT("&&")
        || Token == TEXT("||")
        || Token == TEXT("boolean_and")
        || Token == TEXT("boolean_or")
        || Token == TEXT("booleanand")
        || Token == TEXT("booleanor");
}

TSharedPtr<FBlueprintHelperGraphExpressionIR> UGraphWriteGraphStatementUtils::FindFirstExpression(
    const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Expressions)
{
    TArray<FString> Keys;
    Expressions.GetKeys(Keys);
    Keys.Sort();
    for (const FString& Key : Keys)
    {
        const TSharedPtr<FBlueprintHelperGraphExpressionIR>* Expression = Expressions.Find(Key);
        if (Expression && Expression->IsValid())
        {
            return *Expression;
        }
    }
    return nullptr;
}

bool UGraphWriteGraphStatementUtils::ContainerActionHasRole(
    const FBlueprintHelperContainerActionSpec& Spec,
    const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
    const TSharedPtr<FBlueprintHelperGraphExpressionIR>& TargetObject,
    const FString& Target,
    const FString& Role)
{
    if (Role.Equals(TEXT("target"), ESearchCase::IgnoreCase))
    {
        return TargetObject.IsValid() || !Target.TrimStartAndEnd().IsEmpty();
    }
    return Args.Contains(Role);
}

void UGraphWriteGraphStatementUtils::ValidateContainerActionContract(
    FBlueprintHelperGraphSemanticIR& OutIR,
    const FString& Path,
    const FString& ContainerKind,
    const FString& ContainerOperation,
    const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
    const TSharedPtr<FBlueprintHelperGraphExpressionIR>& TargetObject,
    const FString& Target,
    const FString& ResultSymbolName,
    const bool bExpression)
{
    const FString Kind = ContainerKind.TrimStartAndEnd();
    const FString Operation = ContainerOperation.TrimStartAndEnd();
    if (Kind.IsEmpty() || Operation.IsEmpty())
    {
        return;
    }

    const FBlueprintHelperContainerActionSpec* Spec = FBlueprintHelperContainerActionVocabulary::Find(Kind, Operation);
    if (!Spec)
    {
        FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
            OutIR,
            TEXT("unsupported_container_operation"),
            Path + TEXT(".container_operation"),
            FString::Printf(TEXT("Unsupported container_action operation: %s.%s."), *Kind, *Operation));
        return;
    }

    for (const FString& Role : Spec->RequiredRoles)
    {
        if (!ContainerActionHasRole(*Spec, Args, TargetObject, Target, Role))
        {
            FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
                OutIR,
                TEXT("container_role_missing"),
                Path + TEXT(".") + Role,
                FString::Printf(TEXT("container_action %s requires role %s."), *Spec->OperationId, *Role));
        }
    }

    if (!ResultSymbolName.TrimStartAndEnd().IsEmpty() && (!Spec->bReturnsValue || Spec->bMutatesTarget))
    {
        FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
            OutIR,
            TEXT("container_result_symbol_invalid"),
            Path + TEXT(".result_symbol"),
            FString::Printf(TEXT("container_action %s cannot bind result_symbol because it is not a pure query operation."), *Spec->OperationId));
    }

    if (bExpression && (!Spec->bReturnsValue || Spec->bMutatesTarget))
    {
        FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
            OutIR,
            TEXT("container_expression_operation_invalid"),
            Path + TEXT(".container_operation"),
            FString::Printf(TEXT("container_action expression requires a pure query operation, got %s."), *Spec->OperationId));
    }
}

bool UGraphWriteGraphStatementUtils::IsSupportedDelegateOperation(const FString& Operation)
{
    const FString Normalized = NormalizeDelegateOperation(Operation);
    return Normalized == TEXT("bind")
        || Normalized == TEXT("assign")
        || Normalized == TEXT("unbind")
        || Normalized == TEXT("call")
        || Normalized == TEXT("clear");
}

bool UGraphWriteGraphStatementUtils::DelegateOperationRequiresHandler(const FString& Operation)
{
    const FString Normalized = NormalizeDelegateOperation(Operation);
    return Normalized == TEXT("bind")
        || Normalized == TEXT("assign")
        || Normalized == TEXT("unbind");
}

bool UGraphWriteGraphStatementUtils::StatementKindCanReturnGraphLocalValue(const EBlueprintHelperGraphStatementKind Kind)
{
    switch (Kind)
    {
    case EBlueprintHelperGraphStatementKind::Call:
    case EBlueprintHelperGraphStatementKind::Create:
    case EBlueprintHelperGraphStatementKind::Convert:
    case EBlueprintHelperGraphStatementKind::Schedule:
    case EBlueprintHelperGraphStatementKind::ContainerAction:
        return true;
    default:
        return false;
    }
}

bool UGraphWriteGraphStatementUtils::StatementResultSymbolRequiresTypeEvidence(const EBlueprintHelperGraphStatementKind Kind)
{
    return Kind == EBlueprintHelperGraphStatementKind::Call
        || Kind == EBlueprintHelperGraphStatementKind::Schedule;
}

FString UGraphWriteGraphStatementUtils::ResolveStatementResultTypeToken(const FBlueprintHelperGraphStatementIR& Statement)
{
    const FString ContainerResultType = FBlueprintHelperGraphStatementTypeUtils::ResolveContainerActionResultTypeToken(
        Statement.ContainerKind,
        Statement.ContainerOperation,
        Statement.ElementType,
        Statement.KeyType,
        Statement.ValueType,
        Statement.PinType,
        Statement.KeyPinType,
        Statement.ValuePinType);
    if (!ContainerResultType.IsEmpty())
    {
        return ContainerResultType;
    }
    if (!Statement.ValueType.IsEmpty())
    {
        return Statement.ValueType;
    }
    if (!Statement.ElementType.IsEmpty())
    {
        return Statement.ElementType;
    }
    if (!Statement.PinType.IsEmpty())
    {
        return Statement.PinType;
    }
    if (!Statement.ResolvedTarget.Type.IsEmpty())
    {
        return Statement.ResolvedTarget.Type;
    }
    return FString();
}

TSharedPtr<FBlueprintHelperGraphExpressionIR> UGraphWriteGraphStatementUtils::MakeStatementResultSymbolExpression(
    const FBlueprintHelperGraphStatementIR& Statement,
    const FString& ResultType)
{
    TSharedPtr<FBlueprintHelperGraphExpressionIR> ResultExpression = MakeShared<FBlueprintHelperGraphExpressionIR>();
    ResultExpression->ExpressionId = Statement.StatementId + TEXT("_result");
    ResultExpression->Path = Statement.Path + TEXT(".result_symbol");
    ResultExpression->Kind = EBlueprintHelperGraphExpressionKind::Field;
    ResultExpression->Target = Statement.ResultSymbolName;
    ResultExpression->Name = Statement.ResultSymbolName;
    ResultExpression->FieldOperation = TEXT("get");
    ResultExpression->FieldScope = TEXT("variable");
    ResultExpression->Type = ResultType;
    ResultExpression->ResolvedTarget.Kind = EBlueprintHelperGraphTargetKind::Temporary;
    ResultExpression->ResolvedTarget.Raw = Statement.ResultSymbolName;
    ResultExpression->ResolvedTarget.Member = Statement.ResultSymbolName;
    ResultExpression->ResolvedTarget.Type = ResultType;
    ResultExpression->ResolvedTarget.bVerifiedByContext = true;
    return ResultExpression;
}

void UGraphWriteGraphStatementUtils::AddCanonicalOpEvidenceAlias(
    TMap<FString, FString>& Evidence,
    const FString& CanonicalKey,
    const FString& LegacyKey)
{
    if (Evidence.Contains(CanonicalKey))
    {
        return;
    }

    const FString LegacyValue = ContextEvidenceValue(Evidence, LegacyKey);
    if (!LegacyValue.IsEmpty())
    {
        Evidence.Add(CanonicalKey, LegacyValue);
    }
}

FString UGraphWriteGraphStatementUtils::ResolveCanonicalOpOperationId(
    const FString& FunctionOperation,
    const TMap<FString, FString>& Evidence,
    const FString& Operator)
{
    const FString FunctionOperationId = NormalizeOpOperationToken(FunctionOperation);
    if (!FunctionOperationId.IsEmpty() && FunctionOperationId != TEXT("operator_function"))
    {
        return FunctionOperationId;
    }

    const FString CanonicalEvidenceOperationId = NormalizeOpOperationToken(ContextEvidenceValue(Evidence, TEXT("op.operation_id")));
    if (!CanonicalEvidenceOperationId.IsEmpty())
    {
        return CanonicalEvidenceOperationId;
    }

    FString LegacyEvidenceOperationId = NormalizeOpOperationToken(ContextEvidenceValue(Evidence, TEXT("op")));
    if (LegacyEvidenceOperationId.IsEmpty())
    {
        LegacyEvidenceOperationId = NormalizeOpOperationToken(ContextEvidenceValue(Evidence, TEXT("op_name")));
    }
    if (LegacyEvidenceOperationId.IsEmpty())
    {
        LegacyEvidenceOperationId = NormalizeOpOperationToken(ContextEvidenceValue(Evidence, TEXT("operator")));
    }
    if (!LegacyEvidenceOperationId.IsEmpty())
    {
        return LegacyEvidenceOperationId;
    }

    return NormalizeOpOperationToken(Operator);
}

void UGraphWriteGraphStatementUtils::CanonicalizeOpExpressionEvidence(FBlueprintHelperGraphExpressionIR& Expression)
{
    if (Expression.Kind != EBlueprintHelperGraphExpressionKind::Op)
    {
        return;
    }

    AddCanonicalOpEvidenceAlias(Expression.ContextEvidence, TEXT("op.argument_pin_type.0"), TEXT("argument_pin_type.0"));
    AddCanonicalOpEvidenceAlias(Expression.ContextEvidence, TEXT("op.argument_pin_type.0"), TEXT("argument_pin_type_0"));
    AddCanonicalOpEvidenceAlias(Expression.ContextEvidence, TEXT("op.argument_pin_type.1"), TEXT("argument_pin_type.1"));
    AddCanonicalOpEvidenceAlias(Expression.ContextEvidence, TEXT("op.argument_pin_type.1"), TEXT("argument_pin_type_1"));
    AddCanonicalOpEvidenceAlias(Expression.ContextEvidence, TEXT("op.expected_return_pin_type"), TEXT("expected_return_pin_type"));
    AddCanonicalOpEvidenceAlias(Expression.ContextEvidence, TEXT("op.array_lhs_pin_type"), TEXT("array_lhs_pin_type"));
    AddCanonicalOpEvidenceAlias(Expression.ContextEvidence, TEXT("op.array_rhs_pin_type"), TEXT("array_rhs_pin_type"));

    const FString OperationId = ResolveCanonicalOpOperationId(
        Expression.FunctionOperation,
        Expression.ContextEvidence,
        Expression.Operator);
    if (!OperationId.IsEmpty())
    {
        Expression.ContextEvidence.FindOrAdd(TEXT("op.operation_id")) = OperationId;
        Expression.FunctionOperation = FString::Printf(TEXT("op.%s"), *OperationId);
    }
}

bool UGraphWriteGraphStatementUtils::IsSupportedFieldOperation(const FString& Operation)
{
    const FString Normalized = NormalizeFieldToken(Operation);
    return Normalized == TEXT("get")
        || Normalized == TEXT("set")
        || Normalized == TEXT("get_property")
        || Normalized == TEXT("set_property");
}

bool UGraphWriteGraphStatementUtils::IsFieldSetOperation(const FString& Operation)
{
    const FString Normalized = NormalizeFieldToken(Operation);
    return Normalized == TEXT("set") || Normalized == TEXT("set_property");
}

bool UGraphWriteGraphStatementUtils::IsFieldReadOperation(const FString& Operation)
{
    const FString Normalized = NormalizeFieldToken(Operation);
    return Normalized == TEXT("get") || Normalized == TEXT("get_property");
}

bool UGraphWriteGraphStatementUtils::IsSupportedFieldScope(const FString& Scope)
{
    const FString Normalized = NormalizeFieldToken(Scope);
    return Normalized == TEXT("variable")
        || Normalized == TEXT("property_path")
        || Normalized == TEXT("component_ref")
        || Normalized == TEXT("field_access");
}

bool UGraphWriteGraphStatementUtils::IsPropertyFieldScope(const FString& Scope)
{
    return NormalizeFieldToken(Scope) == TEXT("property_path");
}

bool UGraphWriteGraphStatementUtils::HasCreateTargetEvidence(
    const FString& CreateOperation,
    const FString& Target,
    const FString& ClassPath,
    const FString& AssetPath,
    const FString& PinType,
    const FString& KeyPinType,
    const FString& ValuePinType)
{
    if (!Target.TrimStartAndEnd().IsEmpty()
        || !ClassPath.TrimStartAndEnd().IsEmpty()
        || !AssetPath.TrimStartAndEnd().IsEmpty()
        || !PinType.TrimStartAndEnd().IsEmpty())
    {
        return true;
    }

    if (NormalizeFieldToken(CreateOperation) == TEXT("make_map"))
    {
        return !KeyPinType.TrimStartAndEnd().IsEmpty()
            && !ValuePinType.TrimStartAndEnd().IsEmpty();
    }

    return false;
}

FString UGraphWriteGraphStatementUtils::ReadOptionalJsonValueAsString(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
    if (!Object.IsValid() || !FieldName)
    {
        return FString();
    }

    if (const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName))
    {
        return FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(*Value).TrimStartAndEnd();
    }

    return FString();
}

FString UGraphWriteGraphStatementUtils::ReadFirstOptionalJsonValueAsString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FirstFieldName,
    const TCHAR* SecondFieldName,
    const TCHAR* ThirdFieldName)
{
    FString Value = ReadOptionalJsonValueAsString(Object, FirstFieldName);
    if (Value.IsEmpty() && SecondFieldName)
    {
        Value = ReadOptionalJsonValueAsString(Object, SecondFieldName);
    }
    if (Value.IsEmpty() && ThirdFieldName)
    {
        Value = ReadOptionalJsonValueAsString(Object, ThirdFieldName);
    }
    return Value;
}

FGuid UGraphWriteGraphStatementUtils::ReadOptionalGuidField(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FirstFieldName,
    const TCHAR* SecondFieldName)
{
    FGuid ParsedGuid;
    const FString GuidText = ReadFirstOptionalJsonValueAsString(Object, FirstFieldName, SecondFieldName);
    if (!GuidText.IsEmpty())
    {
        FGuid::Parse(GuidText, ParsedGuid);
    }
    return ParsedGuid;
}

void UGraphWriteGraphStatementUtils::AddCapabilityFactIfPresent(
    TMap<FString, FString>& OutFacts,
    const FString& FactKey,
    const FString& Value)
{
    const FString CleanValue = Value.TrimStartAndEnd();
    if (!FactKey.IsEmpty() && !CleanValue.IsEmpty())
    {
        OutFacts.FindOrAdd(FactKey, CleanValue);
    }
}

void UGraphWriteGraphStatementUtils::AddCapabilityFactIfPresent(
    TMap<FString, FString>& OutFacts,
    const FString& FactKey,
    const FGuid& Value)
{
    if (Value.IsValid())
    {
        OutFacts.FindOrAdd(FactKey, Value.ToString(EGuidFormats::DigitsWithHyphens));
    }
}

void UGraphWriteGraphStatementUtils::ReadOptionalStringMapField(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FieldName,
    TMap<FString, FString>& OutMap)
{
    OutMap.Reset();

    const TSharedPtr<FJsonObject>* MapObject = nullptr;
    if (!Object.IsValid()
        || !Object->TryGetObjectField(FieldName, MapObject)
        || !MapObject
        || !MapObject->IsValid())
    {
        return;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*MapObject)->Values)
    {
        OutMap.Add(Pair.Key, FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(Pair.Value));
    }
}

void UGraphWriteGraphStatementUtils::ReadOptionalCapabilityFacts(
    const TSharedPtr<FJsonObject>& Object,
    TMap<FString, FString>& OutFacts)
{
    ReadOptionalStringMapField(Object, TEXT("capability_facts"), OutFacts);

    AddCapabilityFactIfPresent(OutFacts, TEXT("field.owner_class"), ReadFirstOptionalJsonValueAsString(Object, TEXT("owner_class_path"), TEXT("owner_class"), TEXT("field_owner_class")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.member_guid"), ReadOptionalGuidField(Object, TEXT("member_guid")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.local_scope"), ReadFirstOptionalJsonValueAsString(Object, TEXT("local_scope"), TEXT("scope_name")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.function_name"), ReadFirstOptionalJsonValueAsString(Object, TEXT("function_name")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.param_flags"), ReadFirstOptionalJsonValueAsString(Object, TEXT("param_flags")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.target_pin_ref"), ReadFirstOptionalJsonValueAsString(Object, TEXT("target_pin_ref"), TEXT("target_pin")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.target_pin_type"), ReadFirstOptionalJsonValueAsString(Object, TEXT("target_pin_type_category"), TEXT("target_pin_type")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.target_pin_object_path"), ReadFirstOptionalJsonValueAsString(Object, TEXT("target_pin_type_object_path"), TEXT("target_pin_object_path")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.component_name"), ReadFirstOptionalJsonValueAsString(Object, TEXT("component_name")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.component_owner_class"), ReadFirstOptionalJsonValueAsString(Object, TEXT("component_owner_class")));
    AddCapabilityFactIfPresent(OutFacts, TEXT("field.component_kind"), ReadFirstOptionalJsonValueAsString(Object, TEXT("component_kind")));

    const TArray<TSharedPtr<FJsonValue>>* SegmentValues = nullptr;
    if (Object.IsValid()
        && (Object->TryGetArrayField(TEXT("field_path_segments"), SegmentValues)
            || Object->TryGetArrayField(TEXT("field_path"), SegmentValues))
        && SegmentValues)
    {
        TArray<FString> Segments;
        for (const TSharedPtr<FJsonValue>& SegmentValue : *SegmentValues)
        {
            if (SegmentValue.IsValid())
            {
                Segments.Add(FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(SegmentValue).TrimStartAndEnd());
            }
        }
        Segments.RemoveAll([](const FString& Segment)
        {
            return Segment.IsEmpty();
        });
        if (Segments.Num() > 0)
        {
            OutFacts.FindOrAdd(TEXT("field.property_path"), FString::Join(Segments, TEXT(".")));
        }
    }
}

void UGraphWriteGraphStatementUtils::ParseLogicSpecEntry(
    const TSharedPtr<FJsonObject>& LogicSpecObject,
    FBlueprintHelperGraphSemanticIR& OutIR)
{
    const TSharedPtr<FJsonObject>* EntryObject = nullptr;
    if (!LogicSpecObject.IsValid()
        || !LogicSpecObject->TryGetObjectField(TEXT("entry"), EntryObject)
        || !EntryObject
        || !EntryObject->IsValid())
    {
        return;
    }

    FBlueprintHelperGraphEventReference EntryReference;
    if (!FBlueprintHelperGraphEventReferenceUtils::TryReadEntryReference(*EntryObject, EntryReference))
    {
        return;
    }

    OutIR.Entry.Kind = EntryReference.Kind;
    OutIR.Entry.Name = EntryReference.Name;
    OutIR.Entry.GraphName = EntryReference.GraphName;
    OutIR.Entry.EventTaxonomy = FBlueprintHelperGraphEventReferenceUtils::TaxonomyToString(EntryReference.Taxonomy);
    OutIR.Entry.SourceCluster = EntryReference.SourceCluster;
    OutIR.Entry.SignatureEvidenceId = EntryReference.SignatureEvidenceId;
    OutIR.Entry.ContextEvidence = EntryReference.Metadata;

    if (FBlueprintHelperGraphEventReferenceUtils::IsSignatureOwnedTaxonomy(EntryReference.Taxonomy))
    {
        if (EntryReference.SignatureEvidenceId.TrimStartAndEnd().IsEmpty())
        {
            const FString DiagnosticCode = EntryReference.Taxonomy == EBlueprintHelperGraphEventTaxonomy::CustomEvent
                ? TEXT("custom_event_signature_evidence_missing")
                : TEXT("event_signature_evidence_missing");
            FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
                OutIR,
                DiagnosticCode,
                TEXT("$.entry.signature_evidence_id"),
                TEXT("Signature-owned event entry requires BlueprintSignature signature_evidence_id; GraphWrite only writes the body/use-site."));
        }
        if (EntryReference.SourceCluster.TrimStartAndEnd().IsEmpty())
        {
            FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
                OutIR,
                TEXT("event_source_cluster_missing"),
                TEXT("$.entry.source_cluster"),
                TEXT("Signature-owned event entry requires source_cluster evidence."));
        }
    }
}

// ============================================================================
// BlueprintHelperEventDelegateFragmentBuilder.cpp
// ============================================================================
FString UGraphWriteGraphStatementUtils::GetEventDelegateStatementId(const FString& StatementId, const FString& Path)
{
    if (!StatementId.IsEmpty())
    {
        return StatementId;
    }
    if (!Path.IsEmpty())
    {
        return Path;
    }
    return TEXT("event_delegate_statement");
}

EBlueprintHelperActionSemanticKind UGraphWriteGraphStatementUtils::ToEventDelegateActionSemanticKind(
    const EBlueprintHelperGraphStatementKind StatementKind)
{
    switch (StatementKind)
    {
    case EBlueprintHelperGraphStatementKind::ComponentBoundEvent:
        return EBlueprintHelperActionSemanticKind::ComponentBoundEvent;
    case EBlueprintHelperGraphStatementKind::Delegate:
        return EBlueprintHelperActionSemanticKind::Delegate;
    default:
        return EBlueprintHelperActionSemanticKind::Unknown;
    }
}

bool UGraphWriteGraphStatementUtils::IsDelegateReferenceOperation(const FString& Operation)
{
    return Operation.Equals(TEXT("bind"), ESearchCase::IgnoreCase)
        || Operation.Equals(TEXT("assign"), ESearchCase::IgnoreCase)
        || Operation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase);
}

void UGraphWriteGraphStatementUtils::AddPinRefEventDelegate(
    FBlueprintHelperNodeFragment& Fragment,
    const FString& NodeId,
    const FString& Key,
    UEdGraphPin* Pin)
{
    if (!Pin || Key.IsEmpty())
    {
        return;
    }

    const FString Type = Pin->PinType.PinCategory.ToString();
    FBlueprintHelperFragmentPinRef PinRef{ NodeId, Pin->PinName.ToString(), Type, Pin };
    Fragment.PinBindings.Add(Key, PinRef);
    if (!Fragment.PinBindings.Contains(Key.ToLower()))
    {
        Fragment.PinBindings.Add(Key.ToLower(), PinRef);
    }

    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
    {
        return;
    }
    if (Pin->Direction == EGPD_Input)
    {
        Fragment.DataInputs.Add(Key, PinRef);
    }
    else if (Pin->Direction == EGPD_Output)
    {
        Fragment.DataOutputs.Add(Key, PinRef);
    }
}

void UGraphWriteGraphStatementUtils::PopulatePrimaryPins(UK2Node* PrimaryNode, FBlueprintHelperNodeFragment& OutFragment)
{
    OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, TEXT("execute"));
    OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, TEXT("then"));
    AddPinRefEventDelegate(OutFragment, TEXT("primary"), TEXT("execute"), OutFragment.ExecEntryPin);
    AddPinRefEventDelegate(OutFragment, TEXT("primary"), TEXT("then"), OutFragment.ExecExitPin);

    if (!PrimaryNode)
    {
        return;
    }

    for (UEdGraphPin* Pin : PrimaryNode->Pins)
    {
        if (!Pin)
        {
            continue;
        }
        AddPinRefEventDelegate(OutFragment, TEXT("primary"), Pin->PinName.ToString(), Pin);
    }
}

void UGraphWriteGraphStatementUtils::PopulateCommonFragmentMetadata(
    const FString& StatementId,
    const FString& SemanticKind,
    const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
    FBlueprintHelperNodeFragment& OutFragment)
{
    OutFragment.OwnershipTags.Add(TEXT("statement_id"), StatementId);
    OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), SemanticKind);
    OutFragment.OwnershipTags.Add(TEXT("delegate_name"), Evidence.DelegateName);
    if (!Evidence.DelegateOperation.IsEmpty())
    {
        OutFragment.OwnershipTags.Add(TEXT("delegate_operation"), Evidence.DelegateOperation);
    }
    if (!Evidence.HandlerName.IsEmpty())
    {
        OutFragment.OwnershipTags.Add(TEXT("handler_name"), Evidence.HandlerName);
    }
    if (!Evidence.BindingObjectPath.IsEmpty())
    {
        OutFragment.OwnershipTags.Add(TEXT("binding_object_path"), Evidence.BindingObjectPath);
    }
    if (!Evidence.ComponentPath.IsEmpty())
    {
        OutFragment.OwnershipTags.Add(TEXT("component_path"), Evidence.ComponentPath);
    }
    OutFragment.LayoutHints.Add(TEXT("x"), TEXT("0"));
    OutFragment.LayoutHints.Add(TEXT("y"), TEXT("0"));
    OutFragment.ReviewTargets.Add(StatementId);
}

void UGraphWriteGraphStatementUtils::CollectLiteralDefaultValuesEventDelegate(
    const FBlueprintHelperGraphStatementIR& Statement,
    TMap<FString, FString>& OutDefaultValues)
{
    for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Statement.Args)
    {
        if (ArgPair.Value.IsValid() && ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
        {
            OutDefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
        }
    }
}

bool UGraphWriteGraphStatementUtils::BuildActionRequestEventDelegate(
    UEdGraph* TargetGraph,
    const FBlueprintHelperActionContextScope* ActionContextScope,
    const FString& StatementId,
    FBlueprintHelperActionResolutionRequest& OutRequest,
    FString& OutError)
{
    if (!TargetGraph)
    {
        OutError = TEXT("event_delegate fragment build failed: target graph is invalid.");
        return false;
    }
    if (!ActionContextScope)
    {
        OutError = TEXT("event_delegate fragment build failed: action context scope is required.");
        return false;
    }

    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
    return ActionContextScope->TryBuildRequest(StatementId, Blueprint, TargetGraph, OutRequest, OutError);
}

bool UGraphWriteGraphStatementUtils::ResolveEventDelegateAction(
    const FBlueprintHelperActionResolutionRequest& Request,
    FBlueprintHelperActionResolutionResult& OutResult,
    FString& OutError)
{
    OutResult = FBlueprintGraphWriteFacade::ResolveActionForGraph(Request);
    if (OutResult.IsResolved())
    {
        return true;
    }

    OutError = OutResult.Message.IsEmpty()
        ? FString::Printf(
            TEXT("event_delegate action resolve failed: semantic=%s cluster=%s"),
            *FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
            *FBlueprintHelperActionResolutionCore::ClusterKindToString(OutResult.ClusterKind))
        : OutResult.Message;
    return false;
}

UK2Node_AssignDelegate* UGraphWriteGraphStatementUtils::SpawnAssignDelegateNodeManually(
    UEdGraph* TargetGraph,
    const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
    const FVector2D& Location,
    FString& OutError)
{
    if (!TargetGraph || !Evidence.DelegateProperty)
    {
        OutError = TEXT("delegate.assign manual spawn failed: target graph or delegate evidence is invalid.");
        return nullptr;
    }

    UK2Node_AssignDelegate* AssignNode = NewObject<UK2Node_AssignDelegate>(TargetGraph);
    if (!AssignNode)
    {
        OutError = TEXT("delegate.assign manual spawn failed: could not allocate UK2Node_AssignDelegate.");
        return nullptr;
    }

    AssignNode->CreateNewGuid();
    AssignNode->NodePosX = static_cast<int32>(Location.X);
    AssignNode->NodePosY = static_cast<int32>(Location.Y);
    AssignNode->SetFromProperty(Evidence.DelegateProperty, false, Evidence.DelegateProperty->GetOwnerClass());
    AssignNode->SetFlags(RF_Transactional);
    AssignNode->AllocateDefaultPins();
    TargetGraph->Modify();
    TargetGraph->AddNode(AssignNode, /*bFromUI=*/true, /*bSelectNewNode=*/false);
    return AssignNode;
}

bool UGraphWriteGraphStatementUtils::ConnectProjectedBindingObjectToPrimaryTarget(
    UEdGraph* TargetGraph,
    const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
    UK2Node* PrimaryNode,
    FBlueprintHelperNodeFragment& OutFragment,
    FString& OutError)
{
    if (!TargetGraph || !TargetGraph->GetSchema() || !PrimaryNode)
    {
        return true;
    }

    const FBlueprintHelperEventDelegateBindingObjectResolution BindingResolution =
        FBlueprintHelperEventDelegateBindingObjectResolver::Resolve(Evidence, OutFragment);
    if (!BindingResolution.bResolved)
    {
        OutError = FString::Printf(TEXT("%s: event_delegate binding object could not be resolved."), *BindingResolution.ErrorCode);
        return false;
    }

    if (!BindingResolution.ObjectEvidenceId.IsEmpty())
    {
        OutFragment.OwnershipTags.Add(TEXT("binding_object_evidence_id"), BindingResolution.ObjectEvidenceId);
    }
    if (!Evidence.BindingObjectKind.IsEmpty())
    {
        OutFragment.OwnershipTags.Add(TEXT("binding_object_kind"), Evidence.BindingObjectKind);
    }
    if (!BindingResolution.ObjectPin)
    {
        return true;
    }

    UEdGraphPin* TargetPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, TEXT("target"));
    if (!TargetPin)
    {
        TargetPin = PrimaryNode->FindPin(UEdGraphSchema_K2::PN_Self);
    }
    if (!TargetPin || TargetPin->LinkedTo.Num() > 0)
    {
        return true;
    }

    if (!TargetGraph->GetSchema()->TryCreateConnection(BindingResolution.ObjectPin, TargetPin))
    {
        OutError = TEXT("delegate target binding failed: schema rejected projected binding-object target connection.");
        return false;
    }

    AddPinRefEventDelegate(OutFragment, TEXT("binding_object"), Evidence.BindingObjectEvidenceId, BindingResolution.ObjectPin);
    AddPinRefEventDelegate(OutFragment, TEXT("delegate"), TEXT("target"), TargetPin);

    FBlueprintHelperFragmentLink Link;
    Link.From = FBlueprintHelperFragmentPinRef{ TEXT("binding_object"), BindingResolution.ObjectPin->PinName.ToString(), BindingResolution.ObjectPin->PinType.PinCategory.ToString(), BindingResolution.ObjectPin };
    Link.To = FBlueprintHelperFragmentPinRef{ TEXT("delegate"), TargetPin->PinName.ToString(), TargetPin->PinType.PinCategory.ToString(), TargetPin };
    OutFragment.InternalLinks.Add(Link);
    OutFragment.PinBindings.Add(TEXT("binding_object.value"), Link.From);
    OutFragment.PinBindings.Add(TEXT("delegate.target"), Link.To);
    return true;
}

FString UGraphWriteGraphStatementUtils::DescribePinCategory(const UEdGraphPin* Pin)
{
    return Pin ? Pin->PinType.PinCategory.ToString() : FString();
}

bool UGraphWriteGraphStatementUtils::ValidateAndRecordDelegateCallArgs(
    const FBlueprintHelperActionResolutionRequest& Request,
    const FBlueprintHelperGraphStatementIR& Statement,
    UK2Node* PrimaryNode,
    FBlueprintHelperNodeFragment& OutFragment,
    FString& OutError)
{
    if (!PrimaryNode || Statement.Args.Num() == 0)
    {
        return true;
    }

    for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Statement.Args)
    {
        const FString ArgName = ArgPair.Key.TrimStartAndEnd();
        if (ArgName.IsEmpty())
        {
            continue;
        }

        UEdGraphPin* ArgPin = FBlueprintGraphWriteFacade::FindPinByAlias(PrimaryNode, ArgName);
        if (!ArgPin)
        {
            OutError = FString::Printf(TEXT("missing_delegate_call_arg_pin:%s"), *ArgName);
            return false;
        }

        const FString ExpectedPinType =
            Request.ContextEvidence.FindRef(FString::Printf(TEXT("event_delegate.call_arg.%s.pin_type"), *ArgName)).TrimStartAndEnd();
        if (!ExpectedPinType.IsEmpty()
            && !ExpectedPinType.Equals(DescribePinCategory(ArgPin), ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(
                TEXT("delegate_call_arg_pin_type_mismatch:%s expected=%s actual=%s"),
                *ArgName,
                *ExpectedPinType,
                *DescribePinCategory(ArgPin));
            return false;
        }

        AddPinRefEventDelegate(OutFragment, TEXT("call_arg"), FString::Printf(TEXT("call_arg.%s"), *ArgName), ArgPin);
    }
    return true;
}

UK2Node* UGraphWriteGraphStatementUtils::SpawnResolvedPrimaryNode(
    UEdGraph* TargetGraph,
    const FBlueprintHelperActionResolutionResult& ActionResult,
    const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
    const FBlueprintHelperGraphStatementIR& Statement,
    const FString& StatementId,
    FString& OutError)
{
    const FVector2D PrimaryLocation(0.0, 0.0);
    if (Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::Delegate
        && Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
    {
        return SpawnAssignDelegateNodeManually(TargetGraph, Evidence, PrimaryLocation, OutError);
    }

    FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
    SpawnOptions.NodeId = StatementId;
    CollectLiteralDefaultValuesEventDelegate(Statement, SpawnOptions.DefaultValues);
    if ((Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
        || Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::Delegate)
        && Evidence.ComponentBindingProperty)
    {
        SpawnOptions.Bindings.Add(FBindingObject(Evidence.ComponentBindingProperty));
    }

    return FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
        TargetGraph,
        ActionResult,
        PrimaryLocation,
        SpawnOptions,
        OutError);
}

// ============================================================================
// BlueprintHelperFieldFragmentBuilder.cpp
// ============================================================================
bool UGraphWriteGraphStatementUtils::BuildVariableFragment(
    UEdGraph* TargetGraph,
    const FBlueprintHelperGraphFragmentBuildRequest& Request,
    const FBlueprintHelperActionResolutionResult& ActionResult,
    FBlueprintHelperNodeFragment& OutFragment,
    FString& OutError)
{
    OutFragment = FBlueprintHelperNodeFragment();
    if (!TargetGraph)
    {
        OutError = TEXT("field fragment build failed: target graph is null.");
        return false;
    }
    if (!ActionResult.IsResolved())
    {
        OutError = ActionResult.Message.IsEmpty()
            ? FString(TEXT("field fragment build failed: action result is not resolved."))
            : ActionResult.Message;
        return false;
    }

    FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
    SpawnOptions.NodeId = Request.FragmentId;
    SpawnOptions.DefaultValues = Request.DefaultValues;
    UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
        TargetGraph,
        ActionResult,
        FVector2D(Request.Location.X, Request.Location.Y),
        SpawnOptions,
        OutError);
    if (!SpawnedNode)
    {
        return false;
    }

    OutFragment.FragmentId = Request.FragmentId;
    OutFragment.SourceStatementId = Request.SourceStatementId.IsEmpty() ? Request.FragmentId : Request.SourceStatementId;
    OutFragment.PrimaryNode = SpawnedNode;
    OutFragment.Nodes.Add(SpawnedNode);
    FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
    FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(Request, OutFragment);
    if (ActionResult.CandidateActions.Num() > 0)
    {
        const FBlueprintHelperActionCandidate& Candidate = ActionResult.CandidateActions[0];
        FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.capability_id"), Candidate.CapabilityId);
        FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.expected_node_family"), Candidate.ExpectedNodeFamily);
        FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.expected_node_class"), Candidate.ExpectedNodeClassPath);
        for (const TPair<FString, FString>& FactPair : Candidate.ReadbackFacts)
        {
            FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.readback.") + FactPair.Key, FactPair.Value);
        }
    }
    return true;
}

FString UGraphWriteGraphStatementUtils::FirstPlanFact(
    const FBlueprintHelperFieldFragmentPlan& Plan,
    const TArray<const TCHAR*>& Keys)
{
    for (const TCHAR* Key : Keys)
    {
        if (const FString* Value = Plan.CapabilityFacts.Find(Key))
        {
            const FString CleanValue = Value->TrimStartAndEnd();
            if (!CleanValue.IsEmpty())
            {
                return CleanValue;
            }
        }
    }
    return FString();
}

UScriptStruct* UGraphWriteGraphStatementUtils::ResolveStructType(
    const FBlueprintHelperFieldFragmentPlan& Plan,
    FString& OutError)
{
    const FString StructTypePath = FirstPlanFact(
        Plan,
        {
            TEXT("field.struct_type"),
            TEXT("field.root_struct_type"),
            TEXT("field.owner_type"),
            TEXT("field.target_pin_object_path"),
            TEXT("field.target_pin_type")
        });
    if (StructTypePath.IsEmpty())
    {
        OutError = TEXT("field_struct_type_missing");
        return nullptr;
    }

    UScriptStruct* StructType = FindObject<UScriptStruct>(nullptr, *StructTypePath);
    if (!StructType)
    {
        StructType = LoadObject<UScriptStruct>(nullptr, *StructTypePath);
    }
    if (!StructType)
    {
        OutError = FString::Printf(TEXT("field_struct_type_not_found: %s"), *StructTypePath);
    }
    return StructType;
}

void UGraphWriteGraphStatementUtils::PopulateFieldPlanTags(
    const FBlueprintHelperFieldFragmentPlan& Plan,
    const FString& FragmentKind,
    FBlueprintHelperNodeFragment& OutFragment)
{
    OutFragment.OwnershipTags.Add(TEXT("field.capability_id"), Plan.CapabilityId);
    OutFragment.OwnershipTags.Add(TEXT("field.fragment_kind"), FragmentKind);
    OutFragment.OwnershipTags.Add(TEXT("field.expected_node_family"), FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(Plan.CapabilityId));

    const FString PropertyPath = FirstPlanFact(Plan, {TEXT("field.property_path"), TEXT("property_path")});
    FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.property_path"), PropertyPath);
    if (!PropertyPath.IsEmpty())
    {
        TArray<FString> Segments;
        PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
        OutFragment.OwnershipTags.Add(TEXT("field.property_path.segment_count"), FString::FromInt(Segments.Num()));
        if (Segments.Num() > 0)
        {
            FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.property_path.root"), Segments[0]);
            FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.property_path.leaf"), Segments.Last());
        }
    }
}

TArray<FString> UGraphWriteGraphStatementUtils::GetPropertyPathSegments(const FBlueprintHelperFieldFragmentPlan& Plan)
{
    const FString PropertyPath = FirstPlanFact(Plan, {TEXT("field.property_path"), TEXT("property_path")});
    TArray<FString> Segments;
    PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
    for (FString& Segment : Segments)
    {
        Segment = Segment.TrimStartAndEnd();
    }
    Segments.RemoveAll([](const FString& Segment)
    {
        return Segment.IsEmpty();
    });

    const FString RootName = FirstPlanFact(Plan, {TEXT("field.root_name"), TEXT("field.root"), TEXT("field.member_name")});
    if (Segments.Num() > 1)
    {
        const FString FirstSegment = Segments[0];
        if ((!Plan.FieldName.IsEmpty() && FirstSegment.Equals(Plan.FieldName, ESearchCase::IgnoreCase))
            || (!RootName.IsEmpty() && FirstSegment.Equals(RootName, ESearchCase::IgnoreCase)))
        {
            Segments.RemoveAt(0);
        }
    }
    return Segments;
}

FBlueprintHelperFragmentPinRef UGraphWriteGraphStatementUtils::MakeFragmentPinRef(
    UEdGraphNode* Node,
    UEdGraphPin* Pin)
{
    FBlueprintHelperFragmentPinRef Ref;
    Ref.NodeId = Node ? Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString();
    Ref.PinName = Pin ? Pin->PinName.ToString() : FString();
    Ref.Type = Pin ? Pin->PinType.PinCategory.ToString() : FString();
    Ref.Pin = Pin;
    return Ref;
}

UEdGraphPin* UGraphWriteGraphStatementUtils::FindDirectionalPin(
    UEdGraphNode* Node,
    const FString& PinName,
    const EEdGraphPinDirection Direction)
{
    if (!Node || PinName.IsEmpty())
    {
        return nullptr;
    }
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
        {
            return Pin;
        }
    }
    return nullptr;
}

UEdGraphPin* UGraphWriteGraphStatementUtils::FindStructInputPin(UEdGraphNode* Node)
{
    if (!Node)
    {
        return nullptr;
    }
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
        {
            return Pin;
        }
    }
    return nullptr;
}

void UGraphWriteGraphStatementUtils::RestoreSetFieldsPinsIfNeeded(UEdGraphNode* Node)
{
}

void UGraphWriteGraphStatementUtils::RestoreSetFieldsPinsIfNeeded(UK2Node_SetFieldsInStruct* Node)
{
    if (Node)
    {
        Node->RestoreAllPins();
    }
}

bool UGraphWriteGraphStatementUtils::BuildStructNodeFragment(
    UEdGraph* TargetGraph,
    const FBlueprintHelperFieldFragmentPlan& Plan,
    const FString& FragmentKind,
    const bool bWrite,
    FBlueprintHelperNodeFragment& OutFragment,
    FString& OutError)
{
    OutFragment = FBlueprintHelperNodeFragment();
    if (!TargetGraph)
    {
        OutError = FString::Printf(TEXT("%s requires a target graph."), *FragmentKind);
        return false;
    }
    if (!Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema()) || !FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph))
    {
        OutError = FString::Printf(TEXT("%s requires a Blueprint-owned K2 graph."), *FragmentKind);
        return false;
    }

    UScriptStruct* StructType = ResolveStructType(Plan, OutError);
    if (!StructType)
    {
        return false;
    }

    UK2Node* StructNode = bWrite
        ? Cast<UK2Node>(GraphWriteAddStructNode<UK2Node_SetFieldsInStruct>(TargetGraph, StructType))
        : Cast<UK2Node>(GraphWriteAddStructNode<UK2Node_BreakStruct>(TargetGraph, StructType));
    if (!StructNode)
    {
        OutError = FString::Printf(TEXT("%s failed to create struct node."), *FragmentKind);
        return false;
    }

    OutFragment.FragmentId = Plan.CapabilityId;
    OutFragment.SourceStatementId = Plan.CapabilityId;
    OutFragment.PrimaryNode = StructNode;
    OutFragment.Nodes.Add(StructNode);
    FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(StructNode, OutFragment);
    PopulateFieldPlanTags(Plan, FragmentKind, OutFragment);
    OutFragment.OwnershipTags.Add(TEXT("field.struct_type"), StructType->GetPathName());
    const TArray<FString> Segments = GetPropertyPathSegments(Plan);
    if (Segments.Num() > 0)
    {
        const EEdGraphPinDirection LeafDirection = bWrite ? EGPD_Input : EGPD_Output;
        UEdGraphPin* LeafPin = FindDirectionalPin(StructNode, Segments.Last(), LeafDirection);
        if (LeafPin)
        {
            FBlueprintHelperFragmentPinRef LeafRef = MakeFragmentPinRef(StructNode, LeafPin);
            const FString LeafKey = Segments.Last();
            if (bWrite)
            {
                OutFragment.DataInputs.Add(LeafKey, LeafRef);
            }
            else
            {
                OutFragment.DataOutputs.Add(LeafKey, LeafRef);
            }
            OutFragment.PinBindings.Add(TEXT("field.leaf"), LeafRef);
            FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.leaf_pin"), LeafPin->PinName.ToString());
        }
    }
    return true;
}

bool UGraphWriteGraphStatementUtils::BuildNestedStructBreakPathFragment(
    UEdGraph* TargetGraph,
    const FBlueprintHelperFieldFragmentPlan& Plan,
    FBlueprintHelperNodeFragment& OutFragment,
    FString& OutError)
{
    OutFragment = FBlueprintHelperNodeFragment();
    if (!TargetGraph)
    {
        OutError = TEXT("nested_property_path requires a target graph.");
        return false;
    }
    const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
    if (!K2Schema || !FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph))
    {
        OutError = TEXT("nested_property_path requires a Blueprint-owned K2 graph.");
        return false;
    }

    UScriptStruct* RootStructType = ResolveStructType(Plan, OutError);
    if (!RootStructType)
    {
        return false;
    }

    const TArray<FString> Segments = GetPropertyPathSegments(Plan);
    if (Segments.Num() < 2)
    {
        OutError = TEXT("field_nested_property_path_requires_multiple_segments");
        return false;
    }

    UK2Node_BreakStruct* RootNode = GraphWriteAddStructNode<UK2Node_BreakStruct>(TargetGraph, RootStructType);
    if (!RootNode)
    {
        OutError = TEXT("nested_property_path failed to create root break struct node.");
        return false;
    }

    OutFragment.FragmentId = Plan.CapabilityId;
    OutFragment.SourceStatementId = Plan.CapabilityId;
    OutFragment.PrimaryNode = RootNode;
    OutFragment.Nodes.Add(RootNode);
    FBlueprintHelperActionFragmentBuildUtils::PopulateActionProviderFragmentPins(RootNode, OutFragment);
    PopulateFieldPlanTags(Plan, TEXT("nested_property_path"), OutFragment);
    OutFragment.OwnershipTags.Add(TEXT("field.struct_type"), RootStructType->GetPathName());

    UEdGraphNode* CurrentNode = RootNode;
    for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
    {
        const FString& Segment = Segments[SegmentIndex];
        UEdGraphPin* SegmentPin = FindDirectionalPin(CurrentNode, Segment, EGPD_Output);
        if (!SegmentPin)
        {
            OutError = FString::Printf(TEXT("unknown_struct_property_path: %s"), *Segment);
            return false;
        }

        if (SegmentIndex == Segments.Num() - 1)
        {
            FBlueprintHelperFragmentPinRef LeafRef = MakeFragmentPinRef(CurrentNode, SegmentPin);
            OutFragment.DataOutputs.Add(Segment, LeafRef);
            OutFragment.DataOutputs.Add(TEXT("field.path.leaf"), LeafRef);
            OutFragment.PinBindings.Add(TEXT("field.leaf"), LeafRef);
            FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(OutFragment, TEXT("field.leaf_pin"), SegmentPin->PinName.ToString());
            OutFragment.OwnershipTags.Add(TEXT("field.path.break_node_count"), FString::FromInt(OutFragment.Nodes.Num()));
            OutFragment.OwnershipTags.Add(TEXT("field.path.link_count"), FString::FromInt(OutFragment.InternalLinks.Num()));
            return true;
        }

        UScriptStruct* NextStructType = Cast<UScriptStruct>(SegmentPin->PinType.PinSubCategoryObject.Get());
        if (!NextStructType)
        {
            OutError = FString::Printf(TEXT("unknown_struct_property_path: %s is not a struct segment"), *Segment);
            return false;
        }

        UK2Node_BreakStruct* NextNode = GraphWriteAddStructNode<UK2Node_BreakStruct>(TargetGraph, NextStructType);
        UEdGraphPin* NextInputPin = FindStructInputPin(NextNode);
        if (!NextNode || !NextInputPin)
        {
            OutError = FString::Printf(TEXT("nested_property_path failed to create break node for segment: %s"), *Segment);
            return false;
        }
        if (!K2Schema->TryCreateConnection(SegmentPin, NextInputPin))
        {
            OutError = FString::Printf(TEXT("nested_property_path failed to link segment: %s"), *Segment);
            return false;
        }

        FBlueprintHelperFragmentLink Link;
        Link.From = MakeFragmentPinRef(CurrentNode, SegmentPin);
        Link.To = MakeFragmentPinRef(NextNode, NextInputPin);
        OutFragment.InternalLinks.Add(Link);
        OutFragment.Nodes.Add(NextNode);
        CurrentNode = NextNode;
    }

    OutError = TEXT("nested_property_path did not resolve a leaf segment.");
    return false;
}

// ============================================================================
// BlueprintHelperSelectFragmentBuilder.cpp
// ============================================================================
void UGraphWriteGraphStatementUtils::SelectAddPinAlias(
    TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
    const FString& Alias,
    const FBlueprintHelperFragmentPinRef& PinRef)
{
    if (Alias.IsEmpty() || !PinRef.Pin)
    {
        return;
    }

    PinMap.Add(Alias, PinRef);
    const FString LowerAlias = Alias.ToLower();
    if (!PinMap.Contains(LowerAlias))
    {
        PinMap.Add(LowerAlias, FBlueprintHelperFragmentPinRef{ PinRef.NodeId, LowerAlias, PinRef.Type, PinRef.Pin });
    }
}

bool UGraphWriteGraphStatementUtils::TryBuildSelectPinType(const FString& TypeName, FEdGraphPinType& OutPinType)
{
    FString Raw = TypeName.TrimStartAndEnd();
    const FBlueprintHelperCallFunctionPinType ParsedPinType =
        FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Raw);
    FString Category = ParsedPinType.Category.TrimStartAndEnd();
    FString ObjectPath = ParsedPinType.ObjectPath.TrimStartAndEnd();
    if (ObjectPath.IsEmpty() && !ParsedPinType.SubCategory.TrimStartAndEnd().IsEmpty())
    {
        ObjectPath = ParsedPinType.SubCategory.TrimStartAndEnd();
    }

    const FString Prefixes[] = {
        TEXT("object"),
        TEXT("class"),
        TEXT("soft_object"),
        TEXT("softobject"),
        TEXT("soft_class"),
        TEXT("softclass"),
        TEXT("interface"),
        TEXT("struct"),
        TEXT("enum")
    };
    for (const FString& Prefix : Prefixes)
    {
        if (Raw.StartsWith(Prefix + TEXT(":"), ESearchCase::IgnoreCase))
        {
            Category = Prefix;
            ObjectPath = Raw.Mid(Prefix.Len() + 1).TrimStartAndEnd();
            break;
        }
    }

    const FString Normalized = Raw.ToLower();
    if (Normalized.IsEmpty())
    {
        return false;
    }

    if (Normalized == TEXT("bool") || Normalized == TEXT("boolean"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        return true;
    }
    if (Normalized == TEXT("int") || Normalized == TEXT("integer"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        return true;
    }
    if (Normalized == TEXT("int64") || Normalized == TEXT("long"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
        return true;
    }
    if (Normalized == TEXT("float"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        return true;
    }
    if (Normalized == TEXT("double") || Normalized == TEXT("real") || Normalized == TEXT("number"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
        return true;
    }
    if (Normalized == TEXT("string"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
        return true;
    }
    if (Normalized == TEXT("name"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        return true;
    }
    if (Normalized == TEXT("text"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
        return true;
    }

    auto ResolveTypeObject = [](const FString& Path) -> UObject*
    {
        const FString CleanPath = Path.TrimStartAndEnd();
        if (CleanPath.IsEmpty())
        {
            return nullptr;
        }
        if (UObject* Existing = FindObject<UObject>(nullptr, *CleanPath))
        {
            return Existing;
        }
        return LoadObject<UObject>(nullptr, *CleanPath);
    };

    UObject* TypeObject = ResolveTypeObject(ObjectPath);
    if (!TypeObject && Raw.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
    {
        TypeObject = ResolveTypeObject(Raw);
        if (TypeObject)
        {
            if (TypeObject->IsA<UEnum>())
            {
                Category = TEXT("enum");
            }
            else if (TypeObject->IsA<UScriptStruct>())
            {
                Category = TEXT("struct");
            }
            else if (TypeObject->IsA<UClass>())
            {
                Category = TEXT("object");
            }
        }
    }

    const FString EffectiveCategory = Category.TrimStartAndEnd().ToLower();
    if (EffectiveCategory == TEXT("object"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
        return true;
    }
    if (EffectiveCategory == TEXT("class"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
        OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
        return true;
    }
    if (EffectiveCategory == TEXT("soft_object") || EffectiveCategory == TEXT("softobject"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
        return true;
    }
    if (EffectiveCategory == TEXT("soft_class") || EffectiveCategory == TEXT("softclass"))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        OutPinType.PinSubCategoryObject = TypeObject ? TypeObject : UObject::StaticClass();
        return true;
    }
    if (EffectiveCategory == TEXT("interface"))
    {
        UClass* InterfaceClass = Cast<UClass>(TypeObject);
        if (!InterfaceClass)
        {
            return false;
        }
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
        OutPinType.PinSubCategoryObject = InterfaceClass;
        return true;
    }
    if (EffectiveCategory == TEXT("struct"))
    {
        UScriptStruct* StructType = Cast<UScriptStruct>(TypeObject);
        if (!StructType)
        {
            return false;
        }
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPinType.PinSubCategoryObject = StructType;
        return true;
    }
    if (EffectiveCategory == TEXT("enum"))
    {
        UEnum* EnumType = Cast<UEnum>(TypeObject);
        if (!EnumType)
        {
            return false;
        }
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
        OutPinType.PinSubCategoryObject = EnumType;
        return true;
    }
    return false;
}

FString UGraphWriteGraphStatementUtils::GetExpressionEvidenceValue(
    const FBlueprintHelperGraphExpressionIR& Expression,
    const TCHAR* Key)
{
    if (const FString* Value = Expression.ContextEvidence.Find(Key))
    {
        return Value->TrimStartAndEnd();
    }
    return FString();
}

FString UGraphWriteGraphStatementUtils::ResolveSelectResultTypeProof(const FBlueprintHelperGraphExpressionIR& Expression)
{
    const FString EvidenceProof = GetExpressionEvidenceValue(Expression, TEXT("generic.select.result_type_proof"));
    if (!EvidenceProof.IsEmpty())
    {
        return EvidenceProof;
    }
    return Expression.Type.TrimStartAndEnd();
}

bool UGraphWriteGraphStatementUtils::IsWildcardSelectTypeToken(const FString& TypeName)
{
    const FString Normalized = TypeName.TrimStartAndEnd().ToLower();
    return Normalized == TEXT("wildcard")
        || Normalized == TEXT("wildcard_pin")
        || Normalized == TEXT("wildcardpin")
        || Normalized == TEXT("any")
        || Normalized == TEXT("unknown");
}

bool UGraphWriteGraphStatementUtils::ValidateSelectResultTypeProof(
    const FBlueprintHelperGraphExpressionIR& Expression,
    FEdGraphPinType& OutResultPinType,
    FString& OutError)
{
    const FString ResultTypeProof = ResolveSelectResultTypeProof(Expression);
    if (ResultTypeProof.IsEmpty())
    {
        OutError = TEXT("select_result_type_unresolved: select expression requires generic.select.result_type_proof or a resolved result type.");
        return false;
    }
    if (IsWildcardSelectTypeToken(ResultTypeProof))
    {
        OutError = TEXT("wildcard_residual: select result type proof is still wildcard.");
        return false;
    }
    if (!TryBuildSelectPinType(ResultTypeProof, OutResultPinType))
    {
        OutError = FString::Printf(
            TEXT("select_result_type_unresolved: unsupported select result type proof '%s'."),
            *ResultTypeProof);
        return false;
    }
    return true;
}

bool UGraphWriteGraphStatementUtils::TryGetExpressionLiteral(
    const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
    FString& OutLiteral)
{
    if (!Expression.IsValid() || Expression->Kind != EBlueprintHelperGraphExpressionKind::Literal)
    {
        return false;
    }

    OutLiteral = Expression->LiteralValue;
    return !OutLiteral.IsEmpty();
}

void UGraphWriteGraphStatementUtils::ApplyIndexPinType(UK2Node_Select* SelectNode, const FBlueprintHelperGraphExpressionIR& Expression)
{
    if (!SelectNode || !Expression.Condition.IsValid())
    {
        return;
    }

    FEdGraphPinType PinType;
    if (!TryBuildSelectPinType(Expression.Condition->Type, PinType))
    {
        return;
    }

    UEdGraphPin* IndexPin = SelectNode->GetIndexPin();
    if (!IndexPin || !SelectNode->CanChangePinType(IndexPin))
    {
        return;
    }

    IndexPin->PinType = PinType;
    SelectNode->ChangePinType(IndexPin);
}

void UGraphWriteGraphStatementUtils::ApplyResultPinType(UK2Node_Select* SelectNode, const FEdGraphPinType& PinType)
{
    if (!SelectNode)
    {
        return;
    }

    UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();
    if (!ReturnPin || !SelectNode->CanChangePinType(ReturnPin))
    {
        return;
    }

    ReturnPin->PinType = PinType;
    SelectNode->ChangePinType(ReturnPin);
}

int32 UGraphWriteGraphStatementUtils::GetDesiredOptionCount(const FBlueprintHelperGraphExpressionIR& Expression)
{
    if (Expression.ThenValue.IsValid() || Expression.ElseValue.IsValid())
    {
        return 2;
    }
    return FMath::Max(2, Expression.Options.Num());
}

bool UGraphWriteGraphStatementUtils::EnsureOptionPinCount(
    UK2Node_Select* SelectNode,
    const int32 DesiredOptionCount,
    FString& OutError)
{
    if (!SelectNode)
    {
        OutError = TEXT("select fragment build failed: select node is invalid.");
        return false;
    }

    TArray<UEdGraphPin*> OptionPins;
    SelectNode->GetOptionPins(OptionPins);
    while (OptionPins.Num() < DesiredOptionCount)
    {
        if (!SelectNode->CanAddPin())
        {
            OutError = FString::Printf(
                TEXT("select fragment build failed: cannot add option pin %d."),
                OptionPins.Num());
            return false;
        }
        SelectNode->AddInputPin();
        OptionPins.Reset();
        SelectNode->GetOptionPins(OptionPins);
    }

    return true;
}

void UGraphWriteGraphStatementUtils::CollectLiteralDefaultsSelect(
    UK2Node_Select* SelectNode,
    const FBlueprintHelperGraphExpressionIR& Expression,
    TMap<FString, FString>& InOutDefaults)
{
    if (!SelectNode)
    {
        return;
    }

    FString Literal;
    if (TryGetExpressionLiteral(Expression.Condition, Literal))
    {
        InOutDefaults.Add(TEXT("Index"), Literal);
        InOutDefaults.Add(TEXT("condition"), Literal);
    }

    TArray<UEdGraphPin*> OptionPins;
    SelectNode->GetOptionPins(OptionPins);
    if (Expression.ThenValue.IsValid() || Expression.ElseValue.IsValid())
    {
        if (OptionPins.IsValidIndex(0) && TryGetExpressionLiteral(Expression.ElseValue, Literal))
        {
            InOutDefaults.Add(OptionPins[0]->PinName.ToString(), Literal);
            InOutDefaults.Add(TEXT("else"), Literal);
        }
        if (OptionPins.IsValidIndex(1) && TryGetExpressionLiteral(Expression.ThenValue, Literal))
        {
            InOutDefaults.Add(OptionPins[1]->PinName.ToString(), Literal);
            InOutDefaults.Add(TEXT("then"), Literal);
        }
    }
    else
    {
        for (int32 OptionIndex = 0; OptionIndex < Expression.Options.Num() && OptionIndex < OptionPins.Num(); ++OptionIndex)
        {
            if (TryGetExpressionLiteral(Expression.Options[OptionIndex], Literal))
            {
                InOutDefaults.Add(OptionPins[OptionIndex]->PinName.ToString(), Literal);
                InOutDefaults.Add(FString::Printf(TEXT("option_%d"), OptionIndex), Literal);
            }
        }
    }
}

void UGraphWriteGraphStatementUtils::PopulateSelectPins(
    UK2Node_Select* SelectNode,
    FBlueprintHelperNodeFragment& OutFragment)
{
    if (!SelectNode)
    {
        return;
    }

    if (UEdGraphPin* IndexPin = SelectNode->GetIndexPin())
    {
        const FString Type = IndexPin->PinType.PinCategory.ToString();
        const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), IndexPin->PinName.ToString(), Type, IndexPin };
        SelectAddPinAlias(OutFragment.PinBindings, IndexPin->PinName.ToString(), PinRef);
        SelectAddPinAlias(OutFragment.DataInputs, IndexPin->PinName.ToString(), PinRef);
        SelectAddPinAlias(OutFragment.PinBindings, TEXT("condition"), PinRef);
        SelectAddPinAlias(OutFragment.DataInputs, TEXT("condition"), PinRef);
        SelectAddPinAlias(OutFragment.PinBindings, TEXT("index"), PinRef);
        SelectAddPinAlias(OutFragment.DataInputs, TEXT("index"), PinRef);
    }

    TArray<UEdGraphPin*> OptionPins;
    SelectNode->GetOptionPins(OptionPins);
    for (int32 OptionIndex = 0; OptionIndex < OptionPins.Num(); ++OptionIndex)
    {
        UEdGraphPin* OptionPin = OptionPins[OptionIndex];
        if (!OptionPin)
        {
            continue;
        }

        const FString Type = OptionPin->PinType.PinCategory.ToString();
        const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), OptionPin->PinName.ToString(), Type, OptionPin };
        SelectAddPinAlias(OutFragment.PinBindings, OptionPin->PinName.ToString(), PinRef);
        SelectAddPinAlias(OutFragment.DataInputs, OptionPin->PinName.ToString(), PinRef);
        SelectAddPinAlias(OutFragment.PinBindings, FString::Printf(TEXT("option_%d"), OptionIndex), PinRef);
        SelectAddPinAlias(OutFragment.DataInputs, FString::Printf(TEXT("option_%d"), OptionIndex), PinRef);
    }

    if (OptionPins.IsValidIndex(0))
    {
        const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), TEXT("else"), OptionPins[0]->PinType.PinCategory.ToString(), OptionPins[0] };
        SelectAddPinAlias(OutFragment.PinBindings, TEXT("else"), PinRef);
        SelectAddPinAlias(OutFragment.DataInputs, TEXT("else"), PinRef);
    }
    if (OptionPins.IsValidIndex(1))
    {
        const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), TEXT("then"), OptionPins[1]->PinType.PinCategory.ToString(), OptionPins[1] };
        SelectAddPinAlias(OutFragment.PinBindings, TEXT("then"), PinRef);
        SelectAddPinAlias(OutFragment.DataInputs, TEXT("then"), PinRef);
    }

    if (UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin())
    {
        const FString Type = ReturnPin->PinType.PinCategory.ToString();
        const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), ReturnPin->PinName.ToString(), Type, ReturnPin };
        SelectAddPinAlias(OutFragment.PinBindings, ReturnPin->PinName.ToString(), PinRef);
        SelectAddPinAlias(OutFragment.DataOutputs, ReturnPin->PinName.ToString(), PinRef);
        SelectAddPinAlias(OutFragment.PinBindings, TEXT("result"), PinRef);
        SelectAddPinAlias(OutFragment.DataOutputs, TEXT("result"), PinRef);
        SelectAddPinAlias(OutFragment.PinBindings, TEXT("value"), PinRef);
        SelectAddPinAlias(OutFragment.DataOutputs, TEXT("value"), PinRef);
        SelectAddPinAlias(OutFragment.PinBindings, TEXT("return"), PinRef);
        SelectAddPinAlias(OutFragment.DataOutputs, TEXT("return"), PinRef);
    }
}

// ============================================================================
// BlueprintHelperGraphFragmentBuilderRegistry.cpp
// ============================================================================
FString UGraphWriteGraphStatementUtils::GetStatementId(const FBlueprintHelperGraphStatementIR& Statement)
{
    return FBlueprintHelperGraphStatementTypeUtils::MakeStatementFragmentId(Statement);
}

FString UGraphWriteGraphStatementUtils::GetStatementContextId(const FBlueprintHelperGraphStatementIR& Statement)
{
    return !Statement.StatementId.IsEmpty() ? Statement.StatementId : GetStatementId(Statement);
}

void UGraphWriteGraphStatementUtils::FillLiteralArgsAsDefaultsAndTypes(
    const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
    TMap<FString, FString>& OutDefaultValues,
    TMap<FString, FString>& OutArgumentTypes)
{
    for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Args)
    {
        if (!ArgPair.Value.IsValid())
        {
            continue;
        }
        if (ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
        {
            OutDefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
        }
        if (!ArgPair.Value->Type.TrimStartAndEnd().IsEmpty())
        {
            OutArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type);
        }
    }
}

// ============================================================================
// BlueprintHelperGraphFragmentDag.cpp
// ============================================================================
bool UGraphWriteGraphStatementUtils::TryReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue)
{
    return Object.IsValid() && Object->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
}

bool UGraphWriteGraphStatementUtils::TryReadBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool& OutValue)
{
    return Object.IsValid() && Object->TryGetBoolField(FieldName, OutValue);
}

EBlueprintHelperGraphFragmentPortDirection UGraphWriteGraphStatementUtils::ParseFragmentPortDirection(const FString& Direction)
{
    const FString Normalized = Direction.TrimStartAndEnd().ToLower();
    if (Normalized == TEXT("exec_input"))
    {
        return EBlueprintHelperGraphFragmentPortDirection::ExecInput;
    }
    if (Normalized == TEXT("exec_output"))
    {
        return EBlueprintHelperGraphFragmentPortDirection::ExecOutput;
    }
    if (Normalized == TEXT("data_input"))
    {
        return EBlueprintHelperGraphFragmentPortDirection::DataInput;
    }
    if (Normalized == TEXT("data_output"))
    {
        return EBlueprintHelperGraphFragmentPortDirection::DataOutput;
    }
    return EBlueprintHelperGraphFragmentPortDirection::Unknown;
}

// ============================================================================
// BlueprintHelperDelegateLinkFragmentUtils.cpp
// ============================================================================
FString UGraphWriteGraphStatementUtils::MakeDelegatePinDiagnostic(
    const FString& DiagnosticPrefix,
    const TCHAR* CodeSuffix,
    const FString& Message)
{
    const FString Prefix = DiagnosticPrefix.TrimStartAndEnd().IsEmpty()
        ? TEXT("delegate")
        : DiagnosticPrefix.TrimStartAndEnd();
    return FString::Printf(TEXT("%s_%s: %s"), *Prefix, CodeSuffix, *Message);
}

// ============================================================================
// BlueprintHelperEventDelegateBindingObjectResolver.cpp
// ============================================================================
FBlueprintHelperEventDelegateBindingObjectResolution UGraphWriteGraphStatementUtils::Fail(const FString& Code)
{
    FBlueprintHelperEventDelegateBindingObjectResolution Resolution;
    Resolution.bResolved = false;
    Resolution.ErrorCode = Code;
    return Resolution;
}

UEdGraphPin* UGraphWriteGraphStatementUtils::FindProjectedPin(
    const FBlueprintHelperNodeFragment& Fragment,
    const FString& EvidenceId)
{
    if (EvidenceId.TrimStartAndEnd().IsEmpty())
    {
        return nullptr;
    }
    if (const FBlueprintHelperFragmentPinRef* PinRef = Fragment.PinBindings.Find(EvidenceId))
    {
        return PinRef->Pin;
    }
    if (const FBlueprintHelperFragmentPinRef* PinRef = Fragment.DataOutputs.Find(EvidenceId))
    {
        return PinRef->Pin;
    }
    return nullptr;
}

// ============================================================================
// BlueprintHelperControlFragmentBuilder.cpp
// ============================================================================
FString UGraphWriteGraphStatementUtils::SanitizeFragmentIdPart(const FString& Value)
{
    FString Clean = Value.TrimStartAndEnd();
    if (Clean.IsEmpty())
    {
        return TEXT("unnamed");
    }

    FString Result;
    Result.Reserve(Clean.Len());
    for (int32 Index = 0; Index < Clean.Len(); ++Index)
    {
        const TCHAR Character = Clean[Index];
        Result.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
    }
    return Result.IsEmpty() ? TEXT("unnamed") : Result;
}

FString UGraphWriteGraphStatementUtils::StatementKindName(const EBlueprintHelperGraphStatementKind Kind)
{
    switch (Kind)
    {
    case EBlueprintHelperGraphStatementKind::Branch:
        return TEXT("branch");
    case EBlueprintHelperGraphStatementKind::Sequence:
        return TEXT("sequence");
    case EBlueprintHelperGraphStatementKind::Return:
        return TEXT("return");
    default:
        return TEXT("control");
    }
}

FString UGraphWriteGraphStatementUtils::ResolveStatementFragmentId(const FBlueprintHelperGraphStatementIR& Statement)
{
    const FString SourceId = !Statement.StatementId.IsEmpty() ? Statement.StatementId : Statement.Path;
    if (!SourceId.Contains(TEXT("$")) && !SourceId.Contains(TEXT(".")) && !SourceId.Contains(TEXT("["))
        && !SourceId.Contains(TEXT("]")))
    {
        return SanitizeFragmentIdPart(SourceId);
    }

    const FString KindName = StatementKindName(Statement.Kind);
    return SanitizeFragmentIdPart(TEXT("stmt_") + KindName + TEXT("_") + SourceId + TEXT("_") + KindName);
}

void UGraphWriteGraphStatementUtils::ControlAddPinAlias(
    TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
    const FString& Alias,
    UEdGraphPin* Pin)
{
    if (Alias.IsEmpty() || !Pin)
    {
        return;
    }

    const FString Type = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
        ? FString(TEXT("exec"))
        : Pin->PinType.PinCategory.ToString();
    const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), Alias, Type, Pin };
    PinMap.Add(Alias, PinRef);

    const FString LowerAlias = Alias.ToLower();
    if (!PinMap.Contains(LowerAlias))
    {
        PinMap.Add(LowerAlias, FBlueprintHelperFragmentPinRef{ TEXT("primary"), LowerAlias, Type, Pin });
    }
}

void UGraphWriteGraphStatementUtils::ControlAddExecPinAlias(
    FBlueprintHelperNodeFragment& Fragment,
    const FString& Alias,
    UEdGraphPin* Pin)
{
    ControlAddPinAlias(Fragment.PinBindings, Alias, Pin);
}

void UGraphWriteGraphStatementUtils::ControlAddDataInputAlias(
    FBlueprintHelperNodeFragment& Fragment,
    const FString& Alias,
    UEdGraphPin* Pin)
{
    ControlAddPinAlias(Fragment.PinBindings, Alias, Pin);
    ControlAddPinAlias(Fragment.DataInputs, Alias, Pin);
}

UEdGraphPin* UGraphWriteGraphStatementUtils::FindFirstExecPin(UK2Node* Node, const EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin
            && Pin->Direction == Direction
            && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        {
            return Pin;
        }
    }
    return nullptr;
}

UEdGraphPin* UGraphWriteGraphStatementUtils::FindFirstDataInputPin(UK2Node* Node)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin
            && Pin->Direction == EGPD_Input
            && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            return Pin;
        }
    }
    return nullptr;
}

void UGraphWriteGraphStatementUtils::CollectExecOutputPins(UK2Node* Node, TArray<UEdGraphPin*>& OutPins)
{
    OutPins.Reset();
    if (!Node)
    {
        return;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin
            && Pin->Direction == EGPD_Output
            && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        {
            OutPins.Add(Pin);
        }
    }
}

bool UGraphWriteGraphStatementUtils::ResolveControlActionProvider(
    UEdGraph* TargetGraph,
    const FBlueprintHelperActionContextScope* ActionContextScope,
    const FString& StatementContextId,
    const FString& ControlKind,
    const FString& FragmentId,
    FBlueprintHelperActionResolutionResult& OutResult,
    FString& OutError)
{
    if (!TargetGraph)
    {
        OutError = TEXT("control fragment build failed: target graph is invalid.");
        return false;
    }
    if (StatementContextId.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("control fragment build failed: statement context id is required.");
        return false;
    }

    FBlueprintHelperActionResolutionRequest ActionRequest;
    if (!ActionContextScope)
    {
        OutError = FString::Printf(
            TEXT("missing_required_evidence: action_context_scope_required for control '%s' statement '%s'."),
            *ControlKind,
            *StatementContextId);
        return false;
    }

    if (!ActionContextScope->TryBuildRequest(
        StatementContextId,
        FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph),
        TargetGraph,
        ActionRequest,
        OutError))
    {
        return false;
    }

    if (ActionRequest.Semantic.Query.IsEmpty())
    {
        ActionRequest.Semantic.Query = ControlKind;
    }
    if (ActionRequest.Semantic.TargetPath.IsEmpty())
    {
        ActionRequest.Semantic.TargetPath = FragmentId;
    }

    const FBlueprintHelperActionResolutionResult ActionResult =
        FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);
    if (ActionResult.IsResolved())
    {
        OutResult = ActionResult;
        return true;
    }

    OutError = ActionResult.Message.IsEmpty()
        ? FString::Printf(
            TEXT("control fragment build failed: ActionResolution did not resolve Generic control spawner for '%s'."),
            *ControlKind)
        : ActionResult.Message;
    return false;
}

UK2Node* UGraphWriteGraphStatementUtils::SpawnControlNodeThroughSpawner(
    UEdGraph* TargetGraph,
    const FBlueprintHelperActionResolutionResult& ActionResult,
    const FVector2D& Location,
    const FBlueprintHelperActionNodeSpawnOptions& SpawnOptions,
    FString& OutError)
{
    return FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
        TargetGraph,
        ActionResult,
        Location,
        SpawnOptions,
        OutError);
}

void UGraphWriteGraphStatementUtils::PopulateCommonControlMetadata(
    const FString& FragmentId,
    const FString& SourceStatementId,
    const FString& ControlKind,
    UK2Node* Node,
    FBlueprintHelperNodeFragment& OutFragment)
{
    OutFragment.FragmentId = FragmentId;
    OutFragment.SourceStatementId = SourceStatementId;
    OutFragment.PrimaryNode = Node;
    OutFragment.Nodes.Add(Node);
    OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("control"));
    OutFragment.OwnershipTags.Add(TEXT("control_kind"), ControlKind);
    if (!SourceStatementId.IsEmpty())
    {
        OutFragment.OwnershipTags.Add(TEXT("statement_id"), SourceStatementId);
        OutFragment.ReviewTargets.Add(SourceStatementId);
    }
    else
    {
        OutFragment.ReviewTargets.Add(FragmentId);
    }
}

bool UGraphWriteGraphStatementUtils::EnsureSequenceOutputCount(
    UK2Node_ExecutionSequence* SequenceNode,
    const int32 DesiredOutputCount,
    TArray<UEdGraphPin*>& OutOutputPins,
    FString& OutError)
{
    CollectExecOutputPins(SequenceNode, OutOutputPins);
    while (OutOutputPins.Num() < DesiredOutputCount)
    {
        SequenceNode->AddInputPin();
        CollectExecOutputPins(SequenceNode, OutOutputPins);
    }

    if (OutOutputPins.Num() < DesiredOutputCount)
    {
        OutError = FString::Printf(
            TEXT("sequence fragment build failed: expected at least %d exec outputs, got %d."),
            DesiredOutputCount,
            OutOutputPins.Num());
        return false;
    }
    return true;
}

void UGraphWriteGraphStatementUtils::PopulateSequencePins(
    UK2Node_ExecutionSequence* SequenceNode,
    const TArray<UEdGraphPin*>& OutputPins,
    FBlueprintHelperNodeFragment& OutFragment)
{
    OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(SequenceNode, TEXT("execute"));
    if (!OutFragment.ExecEntryPin)
    {
        OutFragment.ExecEntryPin = FindFirstExecPin(SequenceNode, EGPD_Input);
    }
    ControlAddExecPinAlias(OutFragment, TEXT("execute"), OutFragment.ExecEntryPin);
    ControlAddExecPinAlias(OutFragment, TEXT("exec"), OutFragment.ExecEntryPin);

    if (OutputPins.Num() > 0)
    {
        OutFragment.ExecExitPin = OutputPins[0];
        ControlAddExecPinAlias(OutFragment, TEXT("then"), OutputPins[0]);
    }

    for (int32 OutputIndex = 0; OutputIndex < OutputPins.Num(); ++OutputIndex)
    {
        UEdGraphPin* Pin = OutputPins[OutputIndex];
        if (!Pin)
        {
            continue;
        }

        ControlAddExecPinAlias(OutFragment, Pin->PinName.ToString(), Pin);
        ControlAddExecPinAlias(OutFragment, FString::Printf(TEXT("then_%d"), OutputIndex), Pin);
        ControlAddExecPinAlias(OutFragment, FString::Printf(TEXT("then%d"), OutputIndex), Pin);
    }
}

void UGraphWriteGraphStatementUtils::PopulateBranchPins(
    UK2Node_IfThenElse* BranchNode,
    FBlueprintHelperNodeFragment& OutFragment)
{
    OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("execute"));
    if (!OutFragment.ExecEntryPin)
    {
        OutFragment.ExecEntryPin = FindFirstExecPin(BranchNode, EGPD_Input);
    }
    ControlAddExecPinAlias(OutFragment, TEXT("execute"), OutFragment.ExecEntryPin);
    ControlAddExecPinAlias(OutFragment, TEXT("exec"), OutFragment.ExecEntryPin);

    UEdGraphPin* ThenPin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("then"));
    UEdGraphPin* ElsePin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("else"));
    OutFragment.ExecExitPin = ThenPin;
    ControlAddExecPinAlias(OutFragment, TEXT("then"), ThenPin);
    ControlAddExecPinAlias(OutFragment, TEXT("true"), ThenPin);
    ControlAddExecPinAlias(OutFragment, TEXT("else"), ElsePin);
    ControlAddExecPinAlias(OutFragment, TEXT("false"), ElsePin);

    if (UEdGraphPin* ConditionPin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("condition")))
    {
        ControlAddDataInputAlias(OutFragment, TEXT("condition"), ConditionPin);
        ControlAddDataInputAlias(OutFragment, ConditionPin->PinName.ToString(), ConditionPin);
    }
}

void UGraphWriteGraphStatementUtils::PopulateReturnPins(
    UK2Node_FunctionResult* ReturnNode,
    FBlueprintHelperNodeFragment& OutFragment)
{
    OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(ReturnNode, TEXT("execute"));
    if (!OutFragment.ExecEntryPin)
    {
        OutFragment.ExecEntryPin = FindFirstExecPin(ReturnNode, EGPD_Input);
    }
    OutFragment.ExecExitPin = nullptr;
    ControlAddExecPinAlias(OutFragment, TEXT("execute"), OutFragment.ExecEntryPin);
    ControlAddExecPinAlias(OutFragment, TEXT("exec"), OutFragment.ExecEntryPin);

    UEdGraphPin* FirstDataPin = nullptr;
    for (UEdGraphPin* Pin : ReturnNode->Pins)
    {
        if (!Pin
            || Pin->Direction != EGPD_Input
            || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        {
            continue;
        }

        if (!FirstDataPin)
        {
            FirstDataPin = Pin;
        }
        ControlAddDataInputAlias(OutFragment, Pin->PinName.ToString(), Pin);
    }

    ControlAddDataInputAlias(OutFragment, TEXT("value"), FirstDataPin);
    ControlAddDataInputAlias(OutFragment, TEXT("return"), FirstDataPin);
    ControlAddDataInputAlias(OutFragment, TEXT("result"), FirstDataPin);
}

void UGraphWriteGraphStatementUtils::CollectBranchLiteralDefaults(
    const FBlueprintHelperGraphStatementIR& Statement,
    TMap<FString, FString>& OutDefaults)
{
    OutDefaults.Reset();
    if (!Statement.Condition.IsValid()
        || Statement.Condition->Kind != EBlueprintHelperGraphExpressionKind::Literal
        || Statement.Condition->LiteralValue.IsEmpty())
    {
        return;
    }

    OutDefaults.Add(TEXT("Condition"), Statement.Condition->LiteralValue);
    OutDefaults.Add(TEXT("condition"), Statement.Condition->LiteralValue);
}

void UGraphWriteGraphStatementUtils::CollectReturnLiteralDefault(
    UK2Node_FunctionResult* ReturnNode,
    const FBlueprintHelperGraphStatementIR& Statement,
    TMap<FString, FString>& InOutDefaults)
{
    if (!ReturnNode
        || !Statement.Value.IsValid()
        || Statement.Value->Kind != EBlueprintHelperGraphExpressionKind::Literal
        || Statement.Value->LiteralValue.IsEmpty())
    {
        return;
    }

    UEdGraphPin* ValuePin = FindFirstDataInputPin(ReturnNode);
    if (!ValuePin)
    {
        return;
    }

    InOutDefaults.Add(ValuePin->PinName.ToString(), Statement.Value->LiteralValue);
    InOutDefaults.Add(TEXT("value"), Statement.Value->LiteralValue);
    InOutDefaults.Add(TEXT("return"), Statement.Value->LiteralValue);
}

// ============================================================================
// BlueprintHelperActionFragmentBuildUtils.cpp
// ============================================================================
bool UGraphWriteGraphStatementUtils::IsCallableTargetObjectPin(UK2Node* Node, UEdGraphPin* Pin)
{
    if (!Pin || Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
    {
        return false;
    }

    const FString PinName = Pin->PinName.ToString();
    if (PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase))
    {
        return true;
    }

    const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
    const UFunction* Function = CallNode ? CallNode->GetTargetFunction() : nullptr;
    if (!Function || !Function->HasAnyFunctionFlags(FUNC_Static))
    {
        return false;
    }

    const FString DefaultToSelfPinName = Function->GetMetaData(TEXT("DefaultToSelf"));
    return !DefaultToSelfPinName.IsEmpty()
        && DefaultToSelfPinName.Equals(PinName, ESearchCase::IgnoreCase);
}

void UGraphWriteGraphStatementUtils::ActionBuildUtilsAddDataInputAlias(
    FBlueprintHelperNodeFragment& OutFragment,
    const FString& Alias,
    const FBlueprintHelperFragmentPinRef& PinRef)
{
    if (!OutFragment.DataInputs.Contains(Alias))
    {
        OutFragment.DataInputs.Add(Alias, PinRef);
    }
    if (!OutFragment.PinBindings.Contains(Alias))
    {
        OutFragment.PinBindings.Add(Alias, PinRef);
    }
}

// ============================================================================
// BlueprintHelperGraphComposer.cpp
// ============================================================================
bool UGraphWriteGraphStatementUtils::NodeHasWildcardPins(const UEdGraphNode* Node)
{
    if (!Node)
    {
        return false;
    }
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            return true;
        }
    }
    return false;
}

void UGraphWriteGraphStatementUtils::AddNodeForDeferredReconstruct(UEdGraphPin* Pin, TSet<UEdGraphNode*>& NodesToReconstruct)
{
    UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
    if (Node && NodeHasWildcardPins(Node))
    {
        NodesToReconstruct.Add(Node);
    }
}

void UGraphWriteGraphStatementUtils::ReconstructWildcardNodes(const TSet<UEdGraphNode*>& NodesToReconstruct)
{
    for (UEdGraphNode* Node : NodesToReconstruct)
    {
        if (UK2Node* K2Node = Cast<UK2Node>(Node))
        {
            K2Node->ReconstructNode();
        }
        Node->NodeConnectionListChanged();
    }
}

// ============================================================================
// BlueprintHelperGraphStatementPinTypeParser.cpp
// ============================================================================
void UGraphWriteGraphStatementUtils::ApplyNamedPinTypePart(
    FBlueprintHelperCallFunctionPinType& PinType,
    const FString& Key,
    const FString& Value)
{
    const FString CleanKey = Key.TrimStartAndEnd().ToLower();
    const FString CleanValue = Value.TrimStartAndEnd();
    if (CleanValue.IsEmpty())
    {
        return;
    }

    if (CleanKey == TEXT("category") || CleanKey == TEXT("pin_category") || CleanKey == TEXT("type"))
    {
        PinType.Category = CleanValue;
    }
    else if (CleanKey == TEXT("subcategory") || CleanKey == TEXT("sub_category") || CleanKey == TEXT("pin_sub_category"))
    {
        PinType.SubCategory = CleanValue;
    }
    else if (CleanKey == TEXT("object") || CleanKey == TEXT("object_path") || CleanKey == TEXT("pin_sub_category_object"))
    {
        PinType.ObjectPath = CleanValue;
    }
    else if (CleanKey == TEXT("container") || CleanKey == TEXT("container_type"))
    {
        PinType.ContainerType = CleanValue;
    }
    else if (CleanKey == TEXT("reference"))
    {
        PinType.bIsReference = CleanValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
    }
    else if (CleanKey == TEXT("const"))
    {
        PinType.bIsConst = CleanValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
    }
}

// ============================================================================
// BlueprintHelperGraphEventReferenceUtils.cpp
// ============================================================================
FString UGraphWriteGraphStatementUtils::ReadStringField(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FieldName)
{
    FString Value;
    if (Object.IsValid() && FieldName)
    {
        Object->TryGetStringField(FieldName, Value);
    }
    return Value.TrimStartAndEnd();
}

void UGraphWriteGraphStatementUtils::ReadStringMapField(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* FieldName,
    TMap<FString, FString>& OutMap)
{
    const TSharedPtr<FJsonObject>* MapObject = nullptr;
    if (!Object.IsValid()
        || !FieldName
        || !Object->TryGetObjectField(FieldName, MapObject)
        || !MapObject
        || !MapObject->IsValid())
    {
        return;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*MapObject)->Values)
    {
        if (!Pair.Key.IsEmpty())
        {
            OutMap.Add(Pair.Key, FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(Pair.Value).TrimStartAndEnd());
        }
    }
}

void UGraphWriteGraphStatementUtils::AddMetadataIfPresent(
    TMap<FString, FString>& OutMetadata,
    const FString& Key,
    const FString& Value)
{
    const FString CleanValue = Value.TrimStartAndEnd();
    if (!Key.IsEmpty() && !CleanValue.IsEmpty())
    {
        OutMetadata.Add(Key, CleanValue);
    }
}
