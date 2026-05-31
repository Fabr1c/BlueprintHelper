#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionContextUtils.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEvidenceWrappers.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Shared/BlueprintHelperVersionCompat.h"

namespace
{
static FString ReadFieldOwnerClassEvidence(const TMap<FString, FString>& Evidence)
{
	return FirstNonEmpty(
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("field_owner_class")),
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("owner_class_path")),
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("owner_class")),
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("field.owner_class"))).TrimStartAndEnd();
}

static void AddFieldOwnerClassCapabilityFact(
	FBlueprintHelperActionContextDemand& Demand,
	const TMap<FString, FString>& Evidence)
{
	const FString ExistingOwnerClass = Demand.CapabilityFacts.FindRef(TEXT("field.owner_class")).TrimStartAndEnd();
	if (!ExistingOwnerClass.IsEmpty())
	{
		return;
	}

	const FString OwnerClass = ReadFieldOwnerClassEvidence(Evidence);
	if (!OwnerClass.IsEmpty())
	{
		Demand.CapabilityFacts.Add(TEXT("field.owner_class"), OwnerClass);
	}
}
}

// ============================================================================
// From BlueprintHelperActionContextDemandCollector (original named namespace)
// ============================================================================

FString UGraphWriteActionContextUtils::EvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

void UGraphWriteActionContextUtils::AddDefaultIfPresent(
	FBlueprintHelperActionContextDemand& Demand,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		Demand.DefaultValues.FindOrAdd(Key) = CleanValue;
	}
}

void UGraphWriteActionContextUtils::RemoveEmptyFacts(TMap<FString, FString>& Facts)
{
	TArray<FString> EmptyKeys;
	for (const TPair<FString, FString>& Pair : Facts)
	{
		if (Pair.Value.TrimStartAndEnd().IsEmpty())
		{
			EmptyKeys.Add(Pair.Key);
		}
	}
	for (const FString& EmptyKey : EmptyKeys)
	{
		Facts.Remove(EmptyKey);
	}
}

void UGraphWriteActionContextUtils::AddFieldDemandFacts(FBlueprintHelperActionContextDemand& Demand)
{
	if (Demand.SemanticKind != EBlueprintHelperActionSemanticKind::Field)
	{
		return;
	}

	Demand.RequiredFacts.Add(TEXT("field.capability_id"));
	Demand.RequiredFacts.Add(TEXT("field.owner_class"));
	Demand.RequiredFacts.Add(TEXT("field.member_name"));
	Demand.RequiredFacts.Add(TEXT("field.member_guid"));
	Demand.RequiredFacts.Add(TEXT("field.local_scope"));
	Demand.RequiredFacts.Add(TEXT("field.function_name"));
	Demand.RequiredFacts.Add(TEXT("field.target_pin_ref"));
	Demand.RequiredFacts.Add(TEXT("field.target_pin_type"));
	Demand.RequiredFacts.Add(TEXT("field.component_name"));
	Demand.RequiredFacts.Add(TEXT("field.component_guid"));
	Demand.RequiredFacts.Add(TEXT("field.struct_type"));
	Demand.RequiredFacts.Add(TEXT("field.property_path"));

	UGraphWriteActionContextUtils::AddDefaultIfPresent(Demand, TEXT("field.capability_id"), Demand.CapabilityId);
	UGraphWriteActionContextUtils::AddDefaultIfPresent(Demand, TEXT("field_capability_id"), Demand.CapabilityId);
	for (const TPair<FString, FString>& FactPair : Demand.CapabilityFacts)
	{
		UGraphWriteActionContextUtils::AddDefaultIfPresent(Demand, FactPair.Key, FactPair.Value);
	}
}

void UGraphWriteActionContextUtils::ApplyStatementCapabilityFacts(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextDemand& Demand)
{
	if (Demand.SemanticKind != EBlueprintHelperActionSemanticKind::Field)
	{
		return;
	}

	Demand.CapabilityId = Statement.CapabilityId;
	Demand.CapabilityFacts = Statement.CapabilityFacts;
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.member_name"), FirstNonEmpty(Statement.Name, Statement.Property, Statement.ResolvedTarget.Member, Statement.Target));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_ref"), UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("target_pin_ref")));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_type"), UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("linked_pin_type_category")));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_object_path"), UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("linked_pin_type_object_path")));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.component_name"), FirstNonEmpty(Statement.ComponentName, UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("component_name"))));
	AddFieldOwnerClassCapabilityFact(Demand, Statement.ContextEvidence);
	if (!Demand.PropertyPath.IsEmpty())
	{
		Demand.CapabilityFacts.FindOrAdd(TEXT("field.property_path"), Demand.PropertyPath);
	}
	UGraphWriteActionContextUtils::RemoveEmptyFacts(Demand.CapabilityFacts);
	UGraphWriteActionContextUtils::AddFieldDemandFacts(Demand);
}

void UGraphWriteActionContextUtils::ApplyExpressionCapabilityFacts(
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperActionContextDemand& Demand)
{
	if (Demand.SemanticKind != EBlueprintHelperActionSemanticKind::Field)
	{
		return;
	}

	Demand.CapabilityId = Expression.CapabilityId;
	Demand.CapabilityFacts = Expression.CapabilityFacts;
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.member_name"), FirstNonEmpty(Expression.Name, Expression.Property, Expression.ResolvedTarget.Member, Expression.Target));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_ref"), UGraphWriteActionContextUtils::EvidenceValue(Expression.ContextEvidence, TEXT("target_pin_ref")));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_type"), UGraphWriteActionContextUtils::EvidenceValue(Expression.ContextEvidence, TEXT("linked_pin_type_category")));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.target_pin_object_path"), UGraphWriteActionContextUtils::EvidenceValue(Expression.ContextEvidence, TEXT("linked_pin_type_object_path")));
	Demand.CapabilityFacts.FindOrAdd(TEXT("field.component_name"), UGraphWriteActionContextUtils::EvidenceValue(Expression.ContextEvidence, TEXT("component_name")));
	AddFieldOwnerClassCapabilityFact(Demand, Expression.ContextEvidence);
	if (!Demand.PropertyPath.IsEmpty())
	{
		Demand.CapabilityFacts.FindOrAdd(TEXT("field.property_path"), Demand.PropertyPath);
	}
	UGraphWriteActionContextUtils::RemoveEmptyFacts(Demand.CapabilityFacts);
	UGraphWriteActionContextUtils::AddFieldDemandFacts(Demand);
}

bool UGraphWriteActionContextUtils::IsEventDelegateSemantic(const EBlueprintHelperActionSemanticKind SemanticKind)
{
	return SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		|| SemanticKind == EBlueprintHelperActionSemanticKind::Delegate;
}

FString UGraphWriteActionContextUtils::BuildStatementQuery(
	const FBlueprintHelperGraphStatementIR& Statement,
	const EBlueprintHelperActionSemanticKind SemanticKind)
{
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Field)
	{
		return FirstNonEmpty(
			Statement.ResolvedTarget.Raw,
			Statement.Property,
			Statement.Target,
			Statement.Name,
			Statement.ResolvedTarget.Member,
			Statement.PatternName);
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Call)
	{
		return FirstNonEmpty(
			Statement.ResolvedCallFunctionStableId,
			Statement.Target,
			Statement.Name,
			Statement.PatternName);
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		const FString ContainerQuery = Statement.ContainerKind.TrimStartAndEnd().ToLower()
			+ TEXT(".")
			+ Statement.ContainerOperation.TrimStartAndEnd().ToLower();
		return ContainerQuery == TEXT(".") ? Statement.PatternName : ContainerQuery;
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Control)
	{
		return FirstNonEmpty(
			Statement.ControlOperation,
			UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("generic.control.operation")),
			Statement.PatternName);
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Create
		&& Statement.FunctionOperation.Equals(TEXT("create_function"), ESearchCase::IgnoreCase))
	{
		return FirstNonEmpty(
			Statement.Target,
			Statement.Name,
			Statement.ResolvedCallFunctionStableId);
	}

	if (UGraphWriteActionContextUtils::IsEventDelegateSemantic(SemanticKind))
	{
		return FirstNonEmpty(
			Statement.DelegateName,
			Statement.Property,
			Statement.Name,
			Statement.ResolvedTarget.Member,
			Statement.PatternName);
	}

	return FirstNonEmpty(
		Statement.ResolvedCallFunctionStableId,
		Statement.Name,
		Statement.PatternName);
}

FString UGraphWriteActionContextUtils::BuildStatementTargetPath(
	const FBlueprintHelperGraphStatementIR& Statement,
	const EBlueprintHelperActionSemanticKind SemanticKind)
{
	if (SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction
		&& Statement.TargetObject.IsValid())
	{
		return FirstNonEmpty(
			Statement.TargetObject->ResolvedTarget.Raw,
			Statement.TargetObject->Target,
			Statement.TargetObject->Name,
			Statement.TargetObject->ResolvedTarget.Member);
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Field
		&& Statement.FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase))
	{
		return FirstNonEmpty(
			Statement.ResolvedTarget.Owner,
			Statement.Target,
			Statement.Name);
	}

	return FirstNonEmpty(
		Statement.ResolvedTarget.Raw,
		Statement.Target,
		Statement.Name);
}

FString UGraphWriteActionContextUtils::BuildExpressionQuery(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const EBlueprintHelperActionSemanticKind SemanticKind)
{
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Field)
	{
		return FirstNonEmpty(
			Expression.ResolvedTarget.Raw,
			Expression.Property,
			Expression.Target,
			Expression.Name,
			Expression.ResolvedTarget.Member,
			Expression.PatternName);
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Op)
	{
		return Expression.Operator;
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::Call)
	{
		return FirstNonEmpty(
			Expression.ResolvedCallFunctionStableId,
			Expression.Target,
			Expression.Name,
			Expression.PatternName);
	}

	if (SemanticKind == EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		const FString ContainerQuery = Expression.ContainerKind.TrimStartAndEnd().ToLower()
			+ TEXT(".")
			+ Expression.ContainerOperation.TrimStartAndEnd().ToLower();
		return ContainerQuery == TEXT(".") ? Expression.PatternName : ContainerQuery;
	}

	return FirstNonEmpty(
		Expression.ResolvedCallFunctionStableId,
		Expression.Name,
		Expression.PatternName);
}

FString UGraphWriteActionContextUtils::DemandIdFromPath(const FString& Prefix, const FString& Path)
{
	return FString::Printf(TEXT("%s:%s"), *Prefix, *Path);
}

TArray<FString> UGraphWriteActionContextUtils::SortedArgumentNames(
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args)
{
	TArray<FString> Names;
	Args.GetKeys(Names);
	Names.Sort();
	return Names;
}

void UGraphWriteActionContextUtils::CopyExpressionMapContext(
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Expressions,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Pair : Expressions)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}
		if (!Pair.Value->Type.TrimStartAndEnd().IsEmpty())
		{
			InOutDemand.ArgumentTypes.Add(Pair.Key, Pair.Value->Type.TrimStartAndEnd());
		}
		if (Pair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			InOutDemand.DefaultValues.Add(Pair.Key, Pair.Value->LiteralValue);
		}
	}
}

void UGraphWriteActionContextUtils::CopyNamedExpressionContext(
	const FString& Name,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (Name.IsEmpty() || !Expression.IsValid())
	{
		return;
	}
	InOutDemand.ArgumentNames.AddUnique(Name);
	if (!Expression->Type.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(Name, Expression->Type.TrimStartAndEnd());
	}
	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Literal)
	{
		InOutDemand.DefaultValues.Add(Name, Expression->LiteralValue);
	}
}

FString UGraphWriteActionContextUtils::ResolveComponentPathFromTarget(const FBlueprintHelperGraphResolvedTarget& Target)
{
	if (Target.Kind == EBlueprintHelperGraphTargetKind::Component)
	{
		return Target.Raw.TrimStartAndEnd();
	}
	if (Target.Kind == EBlueprintHelperGraphTargetKind::ComponentMemberFunction)
	{
		return !Target.Owner.TrimStartAndEnd().IsEmpty()
			? Target.Owner.TrimStartAndEnd()
			: Target.Raw.TrimStartAndEnd();
	}
	return FString();
}

EBlueprintHelperActionSemanticFamily UGraphWriteActionContextUtils::ResolveSemanticFamily(
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& FieldScope)
{
	switch (SemanticKind)
	{
	case EBlueprintHelperActionSemanticKind::Call:
	case EBlueprintHelperActionSemanticKind::ContainerAction:
		return EBlueprintHelperActionSemanticFamily::Callable;
	case EBlueprintHelperActionSemanticKind::Field:
		return EBlueprintHelperActionSemanticFamily::Field;
	case EBlueprintHelperActionSemanticKind::Op:
		return EBlueprintHelperActionSemanticFamily::Operator;
	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
		return FieldScope.Equals(TEXT("type_structure"), ESearchCase::IgnoreCase)
			? EBlueprintHelperActionSemanticFamily::TypeStructure
			: EBlueprintHelperActionSemanticFamily::Struct;
	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
		return EBlueprintHelperActionSemanticFamily::Event;
	case EBlueprintHelperActionSemanticKind::Delegate:
		return EBlueprintHelperActionSemanticFamily::Delegate;
	case EBlueprintHelperActionSemanticKind::Control:
	case EBlueprintHelperActionSemanticKind::Select:
		return EBlueprintHelperActionSemanticFamily::Control;
	case EBlueprintHelperActionSemanticKind::Create:
		return EBlueprintHelperActionSemanticFamily::Create;
	case EBlueprintHelperActionSemanticKind::Convert:
		return EBlueprintHelperActionSemanticFamily::Convert;
	case EBlueprintHelperActionSemanticKind::Schedule:
		return EBlueprintHelperActionSemanticFamily::Schedule;
	default:
		return EBlueprintHelperActionSemanticFamily::Unknown;
	}
}

EBlueprintHelperTypeOperation UGraphWriteActionContextUtils::ResolveTypeOperation(const EBlueprintHelperActionSemanticKind SemanticKind)
{
	switch (SemanticKind)
	{
	case EBlueprintHelperActionSemanticKind::Construct:
		return EBlueprintHelperTypeOperation::Construct;
	case EBlueprintHelperActionSemanticKind::Deconstruct:
		return EBlueprintHelperTypeOperation::Deconstruct;
	default:
		return EBlueprintHelperTypeOperation::None;
	}
}

FString UGraphWriteActionContextUtils::GetDefaultValue(
	const FBlueprintHelperActionContextDemand& Demand,
	const TCHAR* Key)
{
	return Demand.DefaultValues.FindRef(Key).TrimStartAndEnd();
}

FString UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

FString UGraphWriteActionContextUtils::NormalizeOpOperationId(const FString& Operation)
{
	FString Clean = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(Operation);
	if (Clean.StartsWith(TEXT("op."), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperVersionCompat::RightChopInlineNoShrink(Clean, 3);
	}

	if (Clean == TEXT("+")) return TEXT("add");
	if (Clean == TEXT("-")) return TEXT("subtract");
	if (Clean == TEXT("*")) return TEXT("multiply");
	if (Clean == TEXT("/")) return TEXT("divide");
	if (Clean == TEXT(">")) return TEXT("greater");
	if (Clean == TEXT(">=")) return TEXT("greater_equal");
	if (Clean == TEXT("<")) return TEXT("less");
	if (Clean == TEXT("<=")) return TEXT("less_equal");
	if (Clean == TEXT("==") || Clean == TEXT("=")) return TEXT("equal");
	if (Clean == TEXT("!=") || Clean == TEXT("<>")) return TEXT("not_equal");
	if (Clean == TEXT("&&") || Clean == TEXT("and")) return TEXT("boolean_and");
	if (Clean == TEXT("||") || Clean == TEXT("or")) return TEXT("boolean_or");
	if (Clean == TEXT("!") || Clean == TEXT("not")) return TEXT("boolean_not");
	return Clean;
}

void UGraphWriteActionContextUtils::AddOpEvidenceIfPresent(
	FBlueprintHelperActionContextDemand& Demand,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (Key.StartsWith(TEXT("op."), ESearchCase::IgnoreCase) && !CleanValue.IsEmpty())
	{
		Demand.DefaultValues.FindOrAdd(Key) = CleanValue;
	}
}

bool UGraphWriteActionContextUtils::IsFocusedContextEvidenceKey(const FString& Key)
{
	return Key.StartsWith(TEXT("op."), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("generic."), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("container."), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("event_delegate."), ESearchCase::IgnoreCase);
}

void UGraphWriteActionContextUtils::AddFocusedContextEvidenceIfPresent(
	FBlueprintHelperActionContextDemand& Demand,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (UGraphWriteActionContextUtils::IsFocusedContextEvidenceKey(Key) && !CleanValue.IsEmpty())
	{
		Demand.DefaultValues.FindOrAdd(Key) = CleanValue;
	}
}

void UGraphWriteActionContextUtils::CopyFocusedContextEvidence(
	const TMap<FString, FString>& Evidence,
	FBlueprintHelperActionContextDemand& Demand)
{
	for (const TPair<FString, FString>& EvidencePair : Evidence)
	{
		UGraphWriteActionContextUtils::AddFocusedContextEvidenceIfPresent(Demand, EvidencePair.Key, EvidencePair.Value);
	}
}

void UGraphWriteActionContextUtils::CopyOpContextEvidence(
	const TMap<FString, FString>& Evidence,
	FBlueprintHelperActionContextDemand& Demand)
{
	for (const TPair<FString, FString>& EvidencePair : Evidence)
	{
		UGraphWriteActionContextUtils::AddFocusedContextEvidenceIfPresent(Demand, EvidencePair.Key, EvidencePair.Value);
	}
}

FString UGraphWriteActionContextUtils::ResolveOpOperationId(
	const FString& ExplicitFunctionOperation,
	const TMap<FString, FString>& Evidence,
	const FString& Operator,
	const FString& Query)
{
	const FString FunctionOperation = UGraphWriteActionContextUtils::NormalizeOpOperationId(ExplicitFunctionOperation);
	if (!FunctionOperation.IsEmpty() && !FunctionOperation.Equals(TEXT("operator_function"), ESearchCase::IgnoreCase))
	{
		return FunctionOperation;
	}

	const FString EvidenceOperation = UGraphWriteActionContextUtils::NormalizeOpOperationId(FirstNonEmpty(
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("op.operation_id")),
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("op")),
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("op_name")),
		UGraphWriteActionContextUtils::EvidenceValue(Evidence, TEXT("operator"))));
	if (!EvidenceOperation.IsEmpty())
	{
		return EvidenceOperation;
	}

	return UGraphWriteActionContextUtils::NormalizeOpOperationId(FirstNonEmpty(Operator, Query));
}

void UGraphWriteActionContextUtils::ApplyOpEvidence(
	const FString& ExplicitFunctionOperation,
	const FString& Operator,
	const TMap<FString, FString>& Evidence,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Left,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Right,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::Op)
	{
		return;
	}

	UGraphWriteActionContextUtils::CopyOpContextEvidence(Evidence, InOutDemand);
	const FString OperationId = UGraphWriteActionContextUtils::ResolveOpOperationId(ExplicitFunctionOperation, Evidence, Operator, InOutDemand.Query);
	if (!OperationId.IsEmpty())
	{
		InOutDemand.FunctionOperation = FString::Printf(TEXT("op.%s"), *OperationId);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("op.operation_id")) = OperationId;
	}
	if (Left.IsValid() && !Left->Type.TrimStartAndEnd().IsEmpty())
	{
		UGraphWriteActionContextUtils::AddOpEvidenceIfPresent(InOutDemand, TEXT("op.argument_pin_type.0"), Left->Type);
	}
	if (Right.IsValid() && !Right->Type.TrimStartAndEnd().IsEmpty())
	{
		UGraphWriteActionContextUtils::AddOpEvidenceIfPresent(InOutDemand, TEXT("op.argument_pin_type.1"), Right->Type);
	}
	if (!InOutDemand.ExpectedReturnType.TrimStartAndEnd().IsEmpty())
	{
		UGraphWriteActionContextUtils::AddOpEvidenceIfPresent(InOutDemand, TEXT("op.expected_return_pin_type"), InOutDemand.ExpectedReturnType);
	}
}

bool UGraphWriteActionContextUtils::ShouldRouteConvertToGeneric(const FString& ExplicitFunctionOperation, const FString& ExplicitTransformOperation)
{
	const FString FunctionOperation = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(ExplicitFunctionOperation);
	const FString TransformOperation = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(ExplicitTransformOperation);
	return FunctionOperation.IsEmpty()
		&& !TransformOperation.IsEmpty()
		&& TransformOperation != TEXT("convert");
}

bool UGraphWriteActionContextUtils::ShouldRouteScheduleToGeneric(const FString& ExplicitFunctionOperation, const FString& ExplicitScheduleOperation)
{
	const FString FunctionOperation = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(ExplicitFunctionOperation);
	const FString ScheduleOperation = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(ExplicitScheduleOperation);
	return FunctionOperation.IsEmpty()
		&& !ScheduleOperation.IsEmpty()
		&& ScheduleOperation != TEXT("latent_or_async");
}

void UGraphWriteActionContextUtils::ApplyFunctionSemanticOperations(FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.ClusterKind != EBlueprintHelperSpawnerClusterKind::FunctionAction)
	{
		return;
	}

	switch (InOutDemand.SemanticKind)
	{
	case EBlueprintHelperActionSemanticKind::Call:
		if (InOutDemand.FunctionOperation.IsEmpty())
		{
			InOutDemand.FunctionOperation = TEXT("function_call");
		}
		break;
	case EBlueprintHelperActionSemanticKind::Op:
		if (InOutDemand.FunctionOperation.IsEmpty())
		{
			InOutDemand.FunctionOperation = TEXT("operator_function");
		}
		break;
	case EBlueprintHelperActionSemanticKind::Convert:
		if (InOutDemand.FunctionOperation.IsEmpty())
		{
			InOutDemand.FunctionOperation = TEXT("convert_function");
		}
		if (InOutDemand.TransformOperation.IsEmpty())
		{
			InOutDemand.TransformOperation = TEXT("convert");
		}
		break;
	case EBlueprintHelperActionSemanticKind::Schedule:
		if (InOutDemand.FunctionOperation.IsEmpty())
		{
			InOutDemand.FunctionOperation = UGraphWriteActionContextUtils::GetDefaultValue(InOutDemand, TEXT("function_operation"));
		}
		if (InOutDemand.FunctionOperation.IsEmpty())
		{
			InOutDemand.FunctionOperation = TEXT("schedule_function");
		}
		if (InOutDemand.ScheduleOperation.IsEmpty())
		{
			InOutDemand.ScheduleOperation = UGraphWriteActionContextUtils::GetDefaultValue(InOutDemand, TEXT("schedule_operation"));
		}
		if (InOutDemand.ScheduleOperation.IsEmpty())
		{
			InOutDemand.ScheduleOperation = TEXT("latent_or_async");
		}
		break;
	case EBlueprintHelperActionSemanticKind::ContainerAction:
		if (InOutDemand.FunctionOperation.IsEmpty())
		{
			InOutDemand.FunctionOperation = TEXT("container_action");
		}
		break;
	default:
		break;
	}
}

void UGraphWriteActionContextUtils::RouteConvertScheduleDemandToGeneric(FBlueprintHelperActionContextDemand& InOutDemand)
{
	InOutDemand.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	InOutDemand.FunctionOperation.Reset();
	InOutDemand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
	InOutDemand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
}

void UGraphWriteActionContextUtils::ApplyConvertScheduleEvidence(
	const FString& ExplicitFunctionOperation,
	const FString& ExplicitTransformOperation,
	const FString& ExplicitScheduleOperation,
	const FString& ExplicitClassPath,
	const FString& ExplicitTarget,
	const FString& ExplicitGraphLatentAllowed,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::Convert
		&& InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::Schedule)
	{
		return;
	}

	const FString FunctionOperation = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(ExplicitFunctionOperation);
	const FString TransformOperation = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(ExplicitTransformOperation);
	const FString ScheduleOperation = UGraphWriteActionContextUtils::NormalizeSemanticOperationToken(ExplicitScheduleOperation);
	if (!FunctionOperation.IsEmpty())
	{
		InOutDemand.FunctionOperation = FunctionOperation;
		InOutDemand.DefaultValues.Add(TEXT("function_operation"), FunctionOperation);
	}
	if (!TransformOperation.IsEmpty())
	{
		InOutDemand.TransformOperation = TransformOperation;
		InOutDemand.DefaultValues.Add(TEXT("transform_operation"), TransformOperation);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("generic.transform.operation")) = TransformOperation;
	}
	if (!ScheduleOperation.IsEmpty())
	{
		InOutDemand.ScheduleOperation = ScheduleOperation;
		InOutDemand.DefaultValues.Add(TEXT("schedule_operation"), ScheduleOperation);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("generic.schedule.operation")) = ScheduleOperation;
	}

	const FString ClassPath = FirstNonEmpty(ExplicitClassPath, ExplicitTarget).TrimStartAndEnd();
	if (!ClassPath.IsEmpty())
	{
		InOutDemand.ClassPath = ClassPath;
		if (InOutDemand.TargetPath.IsEmpty())
		{
			InOutDemand.TargetPath = ClassPath;
		}
		InOutDemand.DefaultValues.Add(TEXT("target_class_path"), ClassPath);
		if (InOutDemand.SemanticKind == EBlueprintHelperActionSemanticKind::Convert)
		{
			InOutDemand.DefaultValues.FindOrAdd(TEXT("generic.transform.target_pin_type")) = ClassPath;
		}
		else if (InOutDemand.SemanticKind == EBlueprintHelperActionSemanticKind::Schedule)
		{
			InOutDemand.DefaultValues.FindOrAdd(TEXT("generic.schedule.target_class_path")) = ClassPath;
		}
	}

	const FString GraphLatentAllowed = ExplicitGraphLatentAllowed.TrimStartAndEnd().ToLower();
	if (!GraphLatentAllowed.IsEmpty())
	{
		InOutDemand.GraphLatentAllowed = GraphLatentAllowed;
		InOutDemand.DefaultValues.Add(TEXT("graph_latent_allowed"), GraphLatentAllowed);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("generic.schedule.graph_latent_allowed")) = GraphLatentAllowed;
	}

	if (InOutDemand.SemanticKind == EBlueprintHelperActionSemanticKind::Convert
		&& UGraphWriteActionContextUtils::ShouldRouteConvertToGeneric(FunctionOperation, TransformOperation))
	{
		UGraphWriteActionContextUtils::RouteConvertScheduleDemandToGeneric(InOutDemand);
	}
	else if (InOutDemand.SemanticKind == EBlueprintHelperActionSemanticKind::Schedule
		&& UGraphWriteActionContextUtils::ShouldRouteScheduleToGeneric(FunctionOperation, ScheduleOperation))
	{
		UGraphWriteActionContextUtils::RouteConvertScheduleDemandToGeneric(InOutDemand);
	}
}

void UGraphWriteActionContextUtils::ApplyCreateStatementEvidence(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::Create)
	{
		return;
	}

	InOutDemand.CreateOperation = Statement.CreateOperation.TrimStartAndEnd().ToLower();
	InOutDemand.ClassPath = FirstNonEmpty(Statement.ClassPath, Statement.Target, Statement.Name);
	InOutDemand.AssetPath = Statement.AssetPath.TrimStartAndEnd();
	if (Statement.FunctionOperation.Equals(TEXT("create_function"), ESearchCase::IgnoreCase))
	{
		InOutDemand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		InOutDemand.FunctionOperation = TEXT("create_function");
		InOutDemand.Query = FirstNonEmpty(Statement.Target, Statement.Name, Statement.ResolvedCallFunctionStableId);
		InOutDemand.DefaultValues.Add(TEXT("function_operation"), TEXT("create_function"));
	}
	else
	{
		InOutDemand.Query = InOutDemand.CreateOperation;
	}
	UGraphWriteActionContextUtils::AddDefaultIfPresent(InOutDemand, TEXT("generic.create.operation"), InOutDemand.CreateOperation);
	UGraphWriteActionContextUtils::AddDefaultIfPresent(InOutDemand, TEXT("generic.create.class_path"), InOutDemand.ClassPath);
	UGraphWriteActionContextUtils::AddDefaultIfPresent(InOutDemand, TEXT("generic.create.asset_path"), InOutDemand.AssetPath);
	if (!InOutDemand.ClassPath.IsEmpty())
	{
		InOutDemand.TargetPath = InOutDemand.ClassPath;
	}
	if (!Statement.PinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("element"), Statement.PinType.TrimStartAndEnd());
		InOutDemand.ContainerElementPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Statement.PinType);
	}
	if (!Statement.KeyPinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("key"), Statement.KeyPinType.TrimStartAndEnd());
		InOutDemand.ContainerKeyPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Statement.KeyPinType);
	}
	if (!Statement.ValuePinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("value"), Statement.ValuePinType.TrimStartAndEnd());
		InOutDemand.ContainerValuePinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Statement.ValuePinType);
	}
}

void UGraphWriteActionContextUtils::ApplyCreateExpressionEvidence(
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::Create)
	{
		return;
	}

	InOutDemand.CreateOperation = Expression.CreateOperation.TrimStartAndEnd().ToLower();
	InOutDemand.ClassPath = FirstNonEmpty(Expression.ClassPath, Expression.Target, Expression.Name);
	InOutDemand.AssetPath = Expression.AssetPath.TrimStartAndEnd();
	if (Expression.FunctionOperation.Equals(TEXT("create_function"), ESearchCase::IgnoreCase))
	{
		InOutDemand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		InOutDemand.FunctionOperation = TEXT("create_function");
		InOutDemand.Query = FirstNonEmpty(Expression.Target, Expression.Name);
		InOutDemand.DefaultValues.Add(TEXT("function_operation"), TEXT("create_function"));
	}
	else
	{
		InOutDemand.Query = InOutDemand.CreateOperation;
	}
	UGraphWriteActionContextUtils::AddDefaultIfPresent(InOutDemand, TEXT("generic.create.operation"), InOutDemand.CreateOperation);
	UGraphWriteActionContextUtils::AddDefaultIfPresent(InOutDemand, TEXT("generic.create.class_path"), InOutDemand.ClassPath);
	UGraphWriteActionContextUtils::AddDefaultIfPresent(InOutDemand, TEXT("generic.create.asset_path"), InOutDemand.AssetPath);
	if (!InOutDemand.ClassPath.IsEmpty())
	{
		InOutDemand.TargetPath = InOutDemand.ClassPath;
	}
	if (!Expression.PinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("element"), Expression.PinType.TrimStartAndEnd());
		InOutDemand.ContainerElementPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Expression.PinType);
	}
	if (!Expression.KeyPinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("key"), Expression.KeyPinType.TrimStartAndEnd());
		InOutDemand.ContainerKeyPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Expression.KeyPinType);
	}
	if (!Expression.ValuePinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("value"), Expression.ValuePinType.TrimStartAndEnd());
		InOutDemand.ContainerValuePinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Expression.ValuePinType);
	}
}

FString UGraphWriteActionContextUtils::ResolveExpressionTargetPath(const FBlueprintHelperGraphExpressionIR& Expression)
{
	return FirstNonEmpty(
		Expression.ResolvedTarget.Raw,
		Expression.Target,
		Expression.Name,
		Expression.ResolvedTarget.Member);
}

void UGraphWriteActionContextUtils::ApplyContainerTypeEvidence(
	const FString& ExplicitElementType,
	const FString& ExplicitKeyType,
	const FString& ExplicitValueType,
	const FString& ExplicitPinType,
	const FString& ExplicitKeyPinType,
	const FString& ExplicitValuePinType,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	const FString ElementType = FirstNonEmpty(ExplicitElementType, ExplicitPinType).TrimStartAndEnd();
	const FString KeyType = FirstNonEmpty(ExplicitKeyType, ExplicitKeyPinType).TrimStartAndEnd();
	const FString ValueType = FirstNonEmpty(ExplicitValueType, ExplicitValuePinType).TrimStartAndEnd();

	if (!ElementType.IsEmpty())
	{
		InOutDemand.ElementType = ElementType;
		InOutDemand.ArgumentTypes.Add(TEXT("element"), ElementType);
		InOutDemand.ContainerElementPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(ElementType);
		InOutDemand.ArgumentPinTypes.Add(TEXT("element"), InOutDemand.ContainerElementPinType);
		InOutDemand.DefaultValues.Add(TEXT("element_type"), ElementType);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("container.element_pin_type")) = ElementType;
	}
	if (!KeyType.IsEmpty())
	{
		InOutDemand.KeyType = KeyType;
		InOutDemand.ArgumentTypes.Add(TEXT("key"), KeyType);
		InOutDemand.ContainerKeyPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(KeyType);
		InOutDemand.ArgumentPinTypes.Add(TEXT("key"), InOutDemand.ContainerKeyPinType);
		InOutDemand.DefaultValues.Add(TEXT("key_type"), KeyType);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("container.key_pin_type")) = KeyType;
	}
	if (!ValueType.IsEmpty())
	{
		InOutDemand.ValueType = ValueType;
		InOutDemand.ArgumentTypes.Add(TEXT("value"), ValueType);
		InOutDemand.ContainerValuePinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(ValueType);
		InOutDemand.ArgumentPinTypes.Add(TEXT("value"), InOutDemand.ContainerValuePinType);
		InOutDemand.DefaultValues.Add(TEXT("value_type"), ValueType);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("container.value_pin_type")) = ValueType;
	}
}

void UGraphWriteActionContextUtils::ApplyContainerActionStatementEvidence(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		return;
	}

	InOutDemand.ContainerKind = Statement.ContainerKind.TrimStartAndEnd().ToLower();
	InOutDemand.ContainerOperation = Statement.ContainerOperation.TrimStartAndEnd().ToLower();
	if (!InOutDemand.ContainerKind.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("container_kind"), InOutDemand.ContainerKind);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("container.kind")) = InOutDemand.ContainerKind;
	}
	if (!InOutDemand.ContainerOperation.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("container_operation"), InOutDemand.ContainerOperation);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("container.operation")) = InOutDemand.ContainerOperation;
	}
	if (!InOutDemand.ContainerKind.IsEmpty() || !InOutDemand.ContainerOperation.IsEmpty())
	{
		InOutDemand.Query = InOutDemand.ContainerKind + TEXT(".") + InOutDemand.ContainerOperation;
	}
	if (InOutDemand.FunctionOperation.IsEmpty())
	{
		InOutDemand.FunctionOperation = TEXT("container_action");
	}
	if (Statement.TargetObject.IsValid())
	{
		UGraphWriteActionContextUtils::CopyNamedExpressionContext(TEXT("target"), Statement.TargetObject, InOutDemand);
		const FString TargetPath = UGraphWriteActionContextUtils::ResolveExpressionTargetPath(*Statement.TargetObject);
		if (!TargetPath.IsEmpty())
		{
			InOutDemand.TargetPath = TargetPath;
			InOutDemand.BindingObjectPath = TargetPath;
		}
	}
	UGraphWriteActionContextUtils::ApplyContainerTypeEvidence(
		Statement.ElementType,
		Statement.KeyType,
		Statement.ValueType,
		Statement.PinType,
		Statement.KeyPinType,
		Statement.ValuePinType,
		InOutDemand);
}

void UGraphWriteActionContextUtils::ApplyContainerActionExpressionEvidence(
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		return;
	}

	InOutDemand.ContainerKind = Expression.ContainerKind.TrimStartAndEnd().ToLower();
	InOutDemand.ContainerOperation = Expression.ContainerOperation.TrimStartAndEnd().ToLower();
	if (!InOutDemand.ContainerKind.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("container_kind"), InOutDemand.ContainerKind);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("container.kind")) = InOutDemand.ContainerKind;
	}
	if (!InOutDemand.ContainerOperation.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("container_operation"), InOutDemand.ContainerOperation);
		InOutDemand.DefaultValues.FindOrAdd(TEXT("container.operation")) = InOutDemand.ContainerOperation;
	}
	if (!InOutDemand.ContainerKind.IsEmpty() || !InOutDemand.ContainerOperation.IsEmpty())
	{
		InOutDemand.Query = InOutDemand.ContainerKind + TEXT(".") + InOutDemand.ContainerOperation;
	}
	if (InOutDemand.FunctionOperation.IsEmpty())
	{
		InOutDemand.FunctionOperation = TEXT("container_action");
	}
	if (Expression.TargetObject.IsValid())
	{
		UGraphWriteActionContextUtils::CopyNamedExpressionContext(TEXT("target"), Expression.TargetObject, InOutDemand);
		const FString TargetPath = UGraphWriteActionContextUtils::ResolveExpressionTargetPath(*Expression.TargetObject);
		if (!TargetPath.IsEmpty())
		{
			InOutDemand.TargetPath = TargetPath;
			InOutDemand.BindingObjectPath = TargetPath;
		}
	}
	UGraphWriteActionContextUtils::ApplyContainerTypeEvidence(
		Expression.ElementType,
		Expression.KeyType,
		Expression.ValueType,
		Expression.PinType,
		Expression.KeyPinType,
		Expression.ValuePinType,
		InOutDemand);
}

void UGraphWriteActionContextUtils::ApplyEventDelegateStatementEvidence(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (!UGraphWriteActionContextUtils::IsEventDelegateSemantic(InOutDemand.SemanticKind))
	{
		return;
	}

	if (InOutDemand.ComponentPath.IsEmpty())
	{
		InOutDemand.ComponentPath = FirstNonEmpty(
			Statement.ComponentName,
			UGraphWriteActionContextUtils::ResolveComponentPathFromTarget(Statement.ResolvedTarget));
	}
	if (InOutDemand.DelegateName.IsEmpty())
	{
		InOutDemand.DelegateName = FirstNonEmpty(
			Statement.DelegateName,
			Statement.Property,
			Statement.Name,
			Statement.ResolvedTarget.Member);
	}
	if (InOutDemand.DelegateOperation.IsEmpty())
	{
		InOutDemand.DelegateOperation = Statement.DelegateOperation.TrimStartAndEnd();
	}
	if (InOutDemand.BindingObjectPath.IsEmpty()
		&& InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		InOutDemand.BindingObjectPath = FirstNonEmpty(Statement.Target, Statement.ResolvedTarget.Raw);
	}
	if (InOutDemand.HandlerName.IsEmpty())
	{
		InOutDemand.HandlerName = Statement.HandlerName.TrimStartAndEnd();
	}
	if (InOutDemand.HandlerFunctionPath.IsEmpty())
	{
		InOutDemand.HandlerFunctionPath = UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("event_delegate.handler_function_path"));
	}
	if (InOutDemand.HandlerSourceCluster.IsEmpty())
	{
		InOutDemand.HandlerSourceCluster = UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("event_delegate.handler_source_cluster"));
	}
	if (InOutDemand.SignatureEvidenceId.IsEmpty())
	{
		InOutDemand.SignatureEvidenceId = UGraphWriteActionContextUtils::EvidenceValue(Statement.ContextEvidence, TEXT("event_delegate.signature_evidence_id"));
	}
	if (InOutDemand.UnbindMode.IsEmpty())
	{
		InOutDemand.UnbindMode = Statement.UnbindMode.TrimStartAndEnd();
	}
	if (!InOutDemand.HandlerName.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("event_delegate.handler_name"), InOutDemand.HandlerName);
	}
	if (!InOutDemand.HandlerFunctionPath.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("event_delegate.handler_function_path"), InOutDemand.HandlerFunctionPath);
	}
	if (!InOutDemand.HandlerSourceCluster.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("event_delegate.handler_source_cluster"), InOutDemand.HandlerSourceCluster);
	}
	if (!InOutDemand.SignatureEvidenceId.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("event_delegate.signature_evidence_id"), InOutDemand.SignatureEvidenceId);
	}
	if (!InOutDemand.DelegateOperation.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("event_delegate.operation"), InOutDemand.DelegateOperation);
	}
	if (!InOutDemand.UnbindMode.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("event_delegate.unbind_mode"), InOutDemand.UnbindMode);
	}
}

void UGraphWriteActionContextUtils::ApplyEventDelegateExpressionEvidence(
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (!UGraphWriteActionContextUtils::IsEventDelegateSemantic(InOutDemand.SemanticKind))
	{
		return;
	}

	if (InOutDemand.ComponentPath.IsEmpty())
	{
		InOutDemand.ComponentPath = UGraphWriteActionContextUtils::ResolveComponentPathFromTarget(Expression.ResolvedTarget);
	}
	if (InOutDemand.DelegateName.IsEmpty())
	{
		InOutDemand.DelegateName = FirstNonEmpty(Expression.Property, Expression.Name, Expression.ResolvedTarget.Member);
	}
	if (InOutDemand.DelegateSignature.IsEmpty() && !Expression.Type.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.DelegateSignature = Expression.Type.TrimStartAndEnd();
	}
	if (InOutDemand.HandlerFunctionPath.IsEmpty())
	{
		InOutDemand.HandlerFunctionPath = UGraphWriteActionContextUtils::EvidenceValue(Expression.ContextEvidence, TEXT("event_delegate.handler_function_path"));
	}
	if (InOutDemand.HandlerSourceCluster.IsEmpty())
	{
		InOutDemand.HandlerSourceCluster = UGraphWriteActionContextUtils::EvidenceValue(Expression.ContextEvidence, TEXT("event_delegate.handler_source_cluster"));
	}
	if (InOutDemand.SignatureEvidenceId.IsEmpty())
	{
		InOutDemand.SignatureEvidenceId = UGraphWriteActionContextUtils::EvidenceValue(Expression.ContextEvidence, TEXT("event_delegate.signature_evidence_id"));
	}
}

// ============================================================================
// From BlueprintHelperActionContextBundleProjector (anonymous namespace)
// ============================================================================

void UGraphWriteActionContextUtils::AppendPinType(FString& Stable, const FBlueprintHelperCallFunctionPinType& PinType)
{
	Stable += PinType.Category;
	Stable += TEXT(".");
	Stable += PinType.SubCategory;
	Stable += TEXT(".");
	Stable += PinType.ObjectPath;
	Stable += TEXT(".");
	Stable += PinType.ContainerType;
	Stable += TEXT(".");
	Stable += PinType.bIsReference ? TEXT("ref") : TEXT("value");
	Stable += TEXT(".");
	Stable += PinType.bIsConst ? TEXT("const") : TEXT("mutable");
}

FString UGraphWriteActionContextUtils::BuildSemanticConstraintsHash(
	const FBlueprintHelperActionSemanticConstraints& Semantic,
	const TMap<FString, FString>& Evidence)
{
	FString Stable;
	Stable += FBlueprintHelperActionResolutionCore::SemanticKindToString(Semantic.Kind);
	Stable += TEXT("|");
	Stable += FBlueprintHelperActionResolutionCore::SemanticFamilyToString(Semantic.SemanticFamily);
	Stable += TEXT("|");
	Stable += FBlueprintHelperActionResolutionCore::TypeOperationToString(Semantic.TypeOperation);
	Stable += TEXT("|");
	Stable += Semantic.Query;
	Stable += TEXT("|");
	Stable += Semantic.StableId;
	Stable += TEXT("|");
	Stable += Semantic.TargetPath;
	Stable += TEXT("|");
	Stable += Semantic.PropertyPath;
	Stable += TEXT("|");
	Stable += Semantic.FieldOperation;
	Stable += TEXT("|");
	Stable += Semantic.FieldScope;
	Stable += TEXT("|");
	Stable += Semantic.CapabilityId;
	Stable += TEXT("|");
	Stable += Semantic.FunctionOperation;
	Stable += TEXT("|");
	Stable += Semantic.TransformOperation;
	Stable += TEXT("|");
	Stable += Semantic.ScheduleOperation;
	Stable += TEXT("|");
	Stable += Semantic.CreateOperation;
	Stable += TEXT("|");
	Stable += Semantic.ContainerKind;
	Stable += TEXT("|");
	Stable += Semantic.ContainerOperation;
	Stable += TEXT("|");
	Stable += Semantic.ClassPath;
	Stable += TEXT("|");
	Stable += Semantic.AssetPath;
	Stable += TEXT("|");
	Stable += Semantic.ElementType;
	Stable += TEXT("|");
	Stable += Semantic.KeyType;
	Stable += TEXT("|");
	Stable += Semantic.ValueType;
	Stable += TEXT("|");
	Stable += Semantic.TypeName;
	Stable += TEXT("|");
	Stable += Semantic.StructPath;
	Stable += TEXT("|");
	Stable += Semantic.TypeStructureId;
	Stable += TEXT("|");
	Stable += Semantic.SearchMode;
	Stable += TEXT("|");
	Stable += Semantic.AmbiguityPolicy;
	Stable += TEXT("|");
	Stable += Semantic.TargetObjectType;
	Stable += TEXT("|");
	Stable += Semantic.ExpectedReturnType;
	Stable += TEXT("|");
	UGraphWriteActionContextUtils::AppendPinType(Stable, Semantic.TargetObjectPinType);
	Stable += TEXT("|");
	UGraphWriteActionContextUtils::AppendPinType(Stable, Semantic.ExpectedReturnPinType);
	Stable += TEXT("|");
	UGraphWriteActionContextUtils::AppendPinType(Stable, Semantic.ContainerElementPinType);
	Stable += TEXT("|");
	UGraphWriteActionContextUtils::AppendPinType(Stable, Semantic.ContainerKeyPinType);
	Stable += TEXT("|");
	UGraphWriteActionContextUtils::AppendPinType(Stable, Semantic.ContainerValuePinType);

	TArray<FString> ArgumentTypeKeys;
	Semantic.ArgumentTypes.GetKeys(ArgumentTypeKeys);
	ArgumentTypeKeys.Sort();
	for (const FString& Key : ArgumentTypeKeys)
	{
		Stable += TEXT("|arg:");
		Stable += Key;
		Stable += TEXT("=");
		Stable += Semantic.ArgumentTypes.FindRef(Key);
	}

	TArray<FString> ArgumentPinTypeKeys;
	Semantic.ArgumentPinTypes.GetKeys(ArgumentPinTypeKeys);
	ArgumentPinTypeKeys.Sort();
	for (const FString& Key : ArgumentPinTypeKeys)
	{
		Stable += TEXT("|pin:");
		Stable += Key;
		Stable += TEXT("=");
		UGraphWriteActionContextUtils::AppendPinType(Stable, Semantic.ArgumentPinTypes.FindRef(Key));
	}

	TArray<FString> CapabilityFactKeys;
	Semantic.CapabilityFacts.GetKeys(CapabilityFactKeys);
	CapabilityFactKeys.Sort();
	for (const FString& Key : CapabilityFactKeys)
	{
		Stable += TEXT("|cap:");
		Stable += Key;
		Stable += TEXT("=");
		Stable += Semantic.CapabilityFacts.FindRef(Key);
	}

	TArray<FString> EvidenceKeys;
	Evidence.GetKeys(EvidenceKeys);
	EvidenceKeys.Sort();
	for (const FString& Key : EvidenceKeys)
	{
		Stable += TEXT("|ev:");
		Stable += Key;
		Stable += TEXT("=");
		Stable += Evidence.FindRef(Key);
	}

	return StableHashString(Stable);
}

FString UGraphWriteActionContextUtils::BuildProjectedContextHash(
	const FBlueprintHelperResolvedActionContextBundle& Bundle,
	const FBlueprintHelperResolvedActionContext& Context)
{
	FString Stable;
	Stable += Bundle.Revision.AssetPath;
	Stable += TEXT("|");
	Stable += Bundle.Revision.GraphName;
	Stable += TEXT("|");
	Stable += Bundle.Revision.TaskRunId;
	Stable += TEXT("|");
	Stable += Bundle.Revision.PlanHash;
	Stable += TEXT("|");
	Stable += LexToString(Bundle.Revision.BlueprintRevision);
	Stable += TEXT("|");
	Stable += LexToString(Bundle.Revision.GraphRevision);
	Stable += TEXT("|");
	Stable += Context.StatementId;
	Stable += TEXT("|");
	Stable += FBlueprintHelperActionResolutionCore::ClusterKindToString(Context.ClusterKind);
	Stable += TEXT("|");
	Stable += Context.GraphName;

	TArray<FString> EvidenceKeys;
	Context.Evidence.GetKeys(EvidenceKeys);
	EvidenceKeys.Sort();
	for (const FString& Key : EvidenceKeys)
	{
		Stable += TEXT("|ev:");
		Stable += Key;
		Stable += TEXT("=");
		Stable += Context.Evidence.FindRef(Key);
	}

	return StableHashString(Stable);
}

// ============================================================================
// From BlueprintHelperActionContextInferenceService (original named namespace)
// ============================================================================

bool UGraphWriteActionContextUtils::MatchesToken(const FString& Value, const FString& Token)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	const FString CleanToken = Token.TrimStartAndEnd();
	return !CleanValue.IsEmpty()
		&& !CleanToken.IsEmpty()
		&& (CleanValue.Equals(CleanToken, ESearchCase::IgnoreCase)
			|| CleanValue.EndsWith(FString(TEXT(".")) + CleanToken, ESearchCase::IgnoreCase)
			|| CleanToken.EndsWith(FString(TEXT(".")) + CleanValue, ESearchCase::IgnoreCase));
}

void UGraphWriteActionContextUtils::AddEvidenceIfPresent(
	FBlueprintHelperResolvedActionContext& Context,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		Context.Evidence.FindOrAdd(Key) = CleanValue;
	}
}

void UGraphWriteActionContextUtils::AddGuidEvidenceIfPresent(
	FBlueprintHelperResolvedActionContext& Context,
	const FString& Key,
	const FGuid& Value)
{
	if (Value.IsValid())
	{
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, Key, Value.ToString(EGuidFormats::Digits));
	}
}

FString UGraphWriteActionContextUtils::SnapshotFactValue(
	const FBlueprintHelperActionContextFieldSnapshot& Field,
	const FString& Key)
{
	if (const FString* Value = Field.CapabilityFacts.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

FString UGraphWriteActionContextUtils::DemandFactValue(
	const FBlueprintHelperActionContextDemand& Demand,
	const FString& Key)
{
	if (const FString* Value = Demand.CapabilityFacts.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

FString UGraphWriteActionContextUtils::DescribePinTypeEvidence(const FBlueprintHelperCallFunctionPinType& PinType)
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

FBlueprintHelperCallFunctionPinType UGraphWriteActionContextUtils::MakeFieldPinType(
	const FBlueprintHelperActionContextFieldSnapshot& Field)
{
	FBlueprintHelperCallFunctionPinType PinType;
	PinType.Category = Field.PinCategory;
	PinType.SubCategory = Field.PinSubCategory;
	PinType.ObjectPath = Field.PinSubCategoryObjectPath;
	PinType.ContainerType = Field.PinContainerType;
	return PinType;
}

bool UGraphWriteActionContextUtils::FindFirstLinkedPinType(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const TArray<FString>& SymbolIds,
	FBlueprintHelperCallFunctionPinType& OutPinType)
{
	for (const FString& SymbolId : SymbolIds)
	{
		if (const FBlueprintHelperCallFunctionPinType* PinType = Snapshot.SymbolPinTypes.Find(SymbolId))
		{
			if (PinType->IsValid())
			{
				OutPinType = *PinType;
				return true;
			}
		}
	}
	return false;
}

const FBlueprintHelperActionContextFieldSnapshot* UGraphWriteActionContextUtils::FindField(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	const FString RequestedMemberGuid = UGraphWriteActionContextUtils::DemandFactValue(Demand, TEXT("field.member_guid"));
	if (!RequestedMemberGuid.IsEmpty())
	{
		if (const FBlueprintHelperActionContextFieldSnapshot* Match = Snapshot.Fields.FindByPredicate(
			[&RequestedMemberGuid](const FBlueprintHelperActionContextFieldSnapshot& Field)
			{
				return UGraphWriteActionContextUtils::SnapshotFactValue(Field, TEXT("field.member_guid")).Equals(RequestedMemberGuid, ESearchCase::IgnoreCase);
			}))
		{
			return Match;
		}
	}

	const FString RequestedMemberName = !UGraphWriteActionContextUtils::DemandFactValue(Demand, TEXT("field.member_name")).IsEmpty()
		? UGraphWriteActionContextUtils::DemandFactValue(Demand, TEXT("field.member_name"))
		: Demand.Query;
	if (!RequestedMemberName.IsEmpty())
	{
		const FString RequestedScope = UGraphWriteActionContextUtils::DemandFactValue(Demand, TEXT("field.local_scope"));
		const FString RequestedOwnerClass = UGraphWriteActionContextUtils::DemandFactValue(Demand, TEXT("field.owner_class"));
		if (const FBlueprintHelperActionContextFieldSnapshot* Match = Snapshot.Fields.FindByPredicate(
			[&RequestedMemberName, &RequestedScope, &RequestedOwnerClass](const FBlueprintHelperActionContextFieldSnapshot& Field)
			{
				const bool bScopeMatches = RequestedScope.IsEmpty() ||
					UGraphWriteActionContextUtils::SnapshotFactValue(Field, TEXT("field.local_scope")).Equals(RequestedScope, ESearchCase::IgnoreCase) ||
					UGraphWriteActionContextUtils::SnapshotFactValue(Field, TEXT("field.function_name")).Equals(RequestedScope, ESearchCase::IgnoreCase);
				const bool bOwnerMatches = RequestedOwnerClass.IsEmpty() ||
					Field.OwnerClassPath.Equals(RequestedOwnerClass, ESearchCase::IgnoreCase);
				return bScopeMatches &&
					bOwnerMatches &&
					(UGraphWriteActionContextUtils::MatchesToken(UGraphWriteActionContextUtils::SnapshotFactValue(Field, TEXT("field.member_name")), RequestedMemberName) ||
						UGraphWriteActionContextUtils::MatchesToken(Field.Name, RequestedMemberName) ||
						UGraphWriteActionContextUtils::MatchesToken(Field.FieldPath, RequestedMemberName));
			}))
		{
			return Match;
		}
	}

	return Snapshot.Fields.FindByPredicate(
		[&Demand](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			return Field.Name == Demand.TargetPath
				|| Field.Name == Demand.PropertyPath
				|| (!Demand.TargetPath.IsEmpty() && Demand.TargetPath.EndsWith(FString(TEXT(".")) + Field.Name));
		});
}

const FBlueprintHelperActionContextFieldSnapshot* UGraphWriteActionContextUtils::FindDelegateField(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	return Snapshot.Fields.FindByPredicate(
		[&Demand](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			if (!Field.bMulticastDelegate)
			{
				return false;
			}
			return UGraphWriteActionContextUtils::MatchesToken(Field.Name, Demand.DelegateName)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.Name, Demand.PropertyPath)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.Name, Demand.Query)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.FieldPath, Demand.PropertyPath);
		});
}

const FBlueprintHelperActionContextFieldSnapshot* UGraphWriteActionContextUtils::FindComponentField(
	const FBlueprintHelperActionContextSnapshot& Snapshot,
	const FBlueprintHelperActionContextDemand& Demand)
{
	return Snapshot.Fields.FindByPredicate(
		[&Demand](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			if (!Field.bComponent)
			{
				return false;
			}
			const FString RequestedComponentName = UGraphWriteActionContextUtils::DemandFactValue(Demand, TEXT("field.component_name"));
			return UGraphWriteActionContextUtils::MatchesToken(Field.Name, Demand.ComponentPath)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.FieldPath, Demand.ComponentPath)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.Name, Demand.TargetPath)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.FieldPath, Demand.TargetPath)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.Name, Demand.BindingObjectPath)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.FieldPath, Demand.BindingObjectPath)
				|| UGraphWriteActionContextUtils::MatchesToken(Field.Name, RequestedComponentName)
				|| UGraphWriteActionContextUtils::MatchesToken(UGraphWriteActionContextUtils::SnapshotFactValue(Field, TEXT("field.component_name")), RequestedComponentName);
		});
}

void UGraphWriteActionContextUtils::ProjectFieldSnapshot(
	FBlueprintHelperResolvedActionContext& Context,
	const FBlueprintHelperActionContextDemand& Demand,
	const FBlueprintHelperActionContextFieldSnapshot& Field)
{
	Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.owner_class"), Field.OwnerClassPath);
	Context.Semantic.CapabilityFacts.FindOrAdd(TEXT("field.member_name"), !UGraphWriteActionContextUtils::SnapshotFactValue(Field, TEXT("field.member_name")).IsEmpty()
		? UGraphWriteActionContextUtils::SnapshotFactValue(Field, TEXT("field.member_name"))
		: Field.Name);
	for (const TPair<FString, FString>& FactPair : Field.CapabilityFacts)
	{
		Context.Semantic.CapabilityFacts.FindOrAdd(FactPair.Key, FactPair.Value);
	}

	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.kind"), Field.Kind);
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.member_name"), Context.Semantic.CapabilityFacts.FindRef(TEXT("field.member_name")));
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.owner_class"), Field.OwnerClassPath);
	for (const TPair<FString, FString>& FactPair : Context.Semantic.CapabilityFacts)
	{
		UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, FactPair.Key, FactPair.Value);
	}
	UGraphWriteActionContextUtils::AddEvidenceIfPresent(Context, TEXT("field.capability_id"), Demand.CapabilityId);
}

// ============================================================================
// From BlueprintHelperActionContextSnapshotBuilder (anonymous namespace)
// ============================================================================

bool UGraphWriteActionContextUtils::SameFieldSnapshotIdentity(
	const FBlueprintHelperActionContextFieldSnapshot& Field,
	const FString& Name,
	const FString& OwnerClassPath,
	const FString& FieldPath)
{
	return Field.Name == Name
		&& Field.OwnerClassPath == OwnerClassPath
		&& (Field.FieldPath == FieldPath || Field.FieldPath.IsEmpty() || FieldPath.IsEmpty());
}

FBlueprintHelperActionContextFieldSnapshot& UGraphWriteActionContextUtils::FindOrAddFieldSnapshot(
	FBlueprintHelperActionContextSnapshot& Snapshot,
	const FString& Name,
	const FString& OwnerClassPath,
	const FString& FieldPath)
{
	if (FBlueprintHelperActionContextFieldSnapshot* Existing = Snapshot.Fields.FindByPredicate(
		[&Name, &OwnerClassPath, &FieldPath](const FBlueprintHelperActionContextFieldSnapshot& Field)
		{
			return UGraphWriteActionContextUtils::SameFieldSnapshotIdentity(Field, Name, OwnerClassPath, FieldPath);
		}))
	{
		return *Existing;
	}

	FBlueprintHelperActionContextFieldSnapshot& Added = Snapshot.Fields.AddDefaulted_GetRef();
	Added.Name = Name;
	Added.OwnerClassPath = OwnerClassPath;
	Added.FieldPath = FieldPath;
	Added.CapabilityFacts.Add(TEXT("field.member_name"), Name);
	return Added;
}

void UGraphWriteActionContextUtils::AddCapabilityFact(
	FBlueprintHelperActionContextFieldSnapshot& Field,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		Field.CapabilityFacts.FindOrAdd(Key, CleanValue);
	}
}

void UGraphWriteActionContextUtils::AddCapabilityGuidFact(
	FBlueprintHelperActionContextFieldSnapshot& Field,
	const FString& Key,
	const FGuid& Value)
{
	if (Value.IsValid())
	{
		UGraphWriteActionContextUtils::AddCapabilityFact(Field, Key, Value.ToString(EGuidFormats::Digits));
	}
}

FString UGraphWriteActionContextUtils::ContainerTypeToString(const EPinContainerType ContainerType)
{
	switch (ContainerType)
	{
	case EPinContainerType::Array:
		return TEXT("array");
	case EPinContainerType::Set:
		return TEXT("set");
	case EPinContainerType::Map:
		return TEXT("map");
	default:
		break;
	}
	return FString();
}

void UGraphWriteActionContextUtils::CaptureDelegateFields(const UClass* Class, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Class)
	{
		return;
	}

	for (TFieldIterator<FMulticastDelegateProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		const FMulticastDelegateProperty* DelegateProperty = *PropertyIt;
		if (!DelegateProperty)
		{
			continue;
		}

		const FString OwnerClassPath = DelegateProperty->GetOwnerClass()
			? DelegateProperty->GetOwnerClass()->GetPathName()
			: Class->GetPathName();
		FBlueprintHelperActionContextFieldSnapshot& Field = UGraphWriteActionContextUtils::FindOrAddFieldSnapshot(
			Snapshot,
			DelegateProperty->GetName(),
			OwnerClassPath,
			DelegateProperty->GetPathName());

		Field.PinCategory = TEXT("delegate");
		Field.Kind = TEXT("delegate");
		UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.member_name"), DelegateProperty->GetName());
		Field.bReadable = false;
		Field.bWritable = true;
		Field.bMulticastDelegate = true;
		Field.bBlueprintAssignable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable);
		Field.bBlueprintCallable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintCallable);
		Field.DelegateSignatureFunctionPath = DelegateProperty->SignatureFunction
			? DelegateProperty->SignatureFunction->GetPathName()
			: FString();
	}
}

void UGraphWriteActionContextUtils::CaptureClassFields(const UClass* Class, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Class)
	{
		return;
	}

	for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		if (!Property)
		{
			continue;
		}

		const FString OwnerClassPath = Property->GetOwnerClass()
			? Property->GetOwnerClass()->GetPathName()
			: Class->GetPathName();
		FBlueprintHelperActionContextFieldSnapshot& Field = UGraphWriteActionContextUtils::FindOrAddFieldSnapshot(
			Snapshot,
			Property->GetName(),
			OwnerClassPath,
			Property->GetPathName());

		Field.bReadable = true;
		Field.bWritable = true;
		Field.Kind = Property->GetOwnerClass() == Class ? TEXT("member") : TEXT("inherited_or_native");
		UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.member_name"), Property->GetName());
		UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.is_inherited_or_native"), Property->GetOwnerClass() != Class ? TEXT("true") : TEXT("false"));

		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			Field.PinCategory = TEXT("object");
			Field.PinSubCategoryObjectPath = ObjectProperty->PropertyClass
				? ObjectProperty->PropertyClass->GetPathName()
				: FString();
			Field.bComponent = ObjectProperty->PropertyClass
				&& ObjectProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass());
			if (Field.bComponent)
			{
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.component_name"), Field.Name);
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.component_owner_class"), OwnerClassPath);
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.component_kind"), TEXT("native_or_inherited_property"));
			}
			if (Field.bComponent)
			{
				UGraphWriteActionContextUtils::CaptureDelegateFields(ObjectProperty->PropertyClass, Snapshot);
			}
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.struct_type"), StructProperty->Struct ? StructProperty->Struct->GetPathName() : FString());
		}

		if (const FMulticastDelegateProperty* DelegateProperty = CastField<FMulticastDelegateProperty>(Property))
		{
			Field.PinCategory = TEXT("delegate");
			Field.Kind = TEXT("delegate");
			Field.bReadable = false;
			Field.bWritable = true;
			Field.bMulticastDelegate = true;
			Field.bBlueprintAssignable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable);
			Field.bBlueprintCallable = DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintCallable);
			Field.DelegateSignatureFunctionPath = DelegateProperty->SignatureFunction
				? DelegateProperty->SignatureFunction->GetPathName()
				: FString();
		}
	}
}

void UGraphWriteActionContextUtils::CaptureFunctionLocalVariables(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint)
	{
		return;
	}

	TArray<UEdGraph*> FunctionGraphs;
	Blueprint->GetAllGraphs(FunctionGraphs);
	for (UEdGraph* Graph : FunctionGraphs)
	{
		if (!Graph || !FBlueprintEditorUtils::DoesSupportLocalVariables(Graph))
		{
			continue;
		}

		TArray<UK2Node_FunctionEntry*> EntryNodes;
		Graph->GetNodesOfClass<UK2Node_FunctionEntry>(EntryNodes);
		for (UK2Node_FunctionEntry* EntryNode : EntryNodes)
		{
			if (!EntryNode)
			{
				continue;
			}

			for (const FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
			{
				FBlueprintHelperActionContextFieldSnapshot Field;
				Field.Name = LocalVar.VarName.ToString();
				Field.Kind = TEXT("local");
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.member_name"), Field.Name);
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.local_scope"), Graph->GetName());
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.function_name"), Graph->GetName());
				UGraphWriteActionContextUtils::AddCapabilityGuidFact(Field, TEXT("field.member_guid"), LocalVar.VarGuid);
				UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.is_local_variable"), TEXT("true"));
				Field.PinCategory = LocalVar.VarType.PinCategory.ToString();
				Field.PinSubCategory = LocalVar.VarType.PinSubCategory.ToString();
				Field.PinSubCategoryObjectPath = LocalVar.VarType.PinSubCategoryObject.Get()
					? LocalVar.VarType.PinSubCategoryObject->GetPathName()
					: FString();
				Field.PinContainerType = UGraphWriteActionContextUtils::ContainerTypeToString(LocalVar.VarType.ContainerType);
				Snapshot.Fields.Add(MoveTemp(Field));
			}
		}
	}
}

void UGraphWriteActionContextUtils::CaptureFunctionInputParameters(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot)
{
	if (!Blueprint || !Blueprint->SkeletonGeneratedClass)
	{
		return;
	}

	for (TFieldIterator<UFunction> FunctionIt(Blueprint->SkeletonGeneratedClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if (!Function)
		{
			continue;
		}

		for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property ||
				!Property->HasAnyPropertyFlags(CPF_Parm) ||
				Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				continue;
			}

			FBlueprintHelperActionContextFieldSnapshot Field;
			Field.Name = Property->GetName();
			Field.Kind = TEXT("function_param");
			UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.member_name"), Field.Name);
			UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.function_name"), Function->GetName());
			UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.local_scope"), Function->GetName());
			UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.param_flags"), TEXT("FUNC_Parm"));
			Field.OwnerClassPath = Blueprint->SkeletonGeneratedClass->GetPathName();
			Field.FieldPath = Function->GetPathName() + TEXT(".") + Field.Name;
			UGraphWriteActionContextUtils::AddCapabilityFact(Field, TEXT("field.is_function_parameter"), TEXT("true"));
			Snapshot.Fields.Add(MoveTemp(Field));
		}
	}
}

// ============================================================================
// From BlueprintHelperGraphWriteProjectedEvidenceQueryService (anonymous namespace)
// ============================================================================

static constexpr const TCHAR* GProjectEvidenceOperationName = TEXT("project_graphwrite_spawner_evidence");

FBlueprintHelperToolResultBase UGraphWriteActionContextUtils::MakeFailure(
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field)
{
	FBlueprintHelperToolError Error;
	Error.Code = Code;
	Error.Stage = Stage;
	Error.Message = Message;
	Error.Field = Field;
	return FBlueprintHelperToolResultBuilder::Failure(
		GProjectEvidenceOperationName,
		FBlueprintHelperToolResultBuilder::GenerateTraceId(),
		Error);
}

FString UGraphWriteActionContextUtils::ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(FieldName, Value);
	}
	return Value.TrimStartAndEnd();
}

TArray<FString> UGraphWriteActionContextUtils::ReadQueries(const TSharedPtr<FJsonObject>& Object)
{
	TArray<FString> Result;
	const TArray<TSharedPtr<FJsonValue>>* QueryValues = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(TEXT("queries"), QueryValues) && QueryValues)
	{
		for (const TSharedPtr<FJsonValue>& QueryValue : *QueryValues)
		{
			FString Query;
			if (QueryValue.IsValid() && QueryValue->TryGetString(Query))
			{
				Query = Query.TrimStartAndEnd();
				if (!Query.IsEmpty())
				{
					Result.Add(Query);
				}
			}
		}
	}

	const FString SingleQuery = UGraphWriteActionContextUtils::ReadStringField(Object, TEXT("query"));
	if (!SingleQuery.IsEmpty())
	{
		Result.Insert(SingleQuery, 0);
	}
	return Result;
}

TSharedRef<FJsonObject> UGraphWriteActionContextUtils::MakeEvidenceJson(const FBlueprintHelperProjectedAssetActionEvidence& Evidence)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("asset_action_stable_id"), Evidence.StableId);
	Json->SetStringField(TEXT("asset_action_node_class"), Evidence.NodeClassPath);
	Json->SetStringField(TEXT("asset_action_spawner_signature"), Evidence.SpawnerSignature);
	Json->SetStringField(TEXT("asset_action_owner_path"), Evidence.OwnerPath);
	if (!Evidence.Query.IsEmpty()) { Json->SetStringField(TEXT("asset_action_query"), Evidence.Query); }
	if (!Evidence.MenuName.IsEmpty()) { Json->SetStringField(TEXT("asset_action_menu_name"), Evidence.MenuName); }
	if (!Evidence.Category.IsEmpty()) { Json->SetStringField(TEXT("asset_action_category"), Evidence.Category); }
	return Json;
}

TSharedRef<FJsonObject> UGraphWriteActionContextUtils::MakeEvidenceJson(const FBlueprintHelperProjectedScheduleActionEvidence& Evidence)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schedule_action_stable_id"), Evidence.StableId);
	Json->SetStringField(TEXT("schedule_node_class"), Evidence.NodeClassPath);
	Json->SetStringField(TEXT("schedule_spawner_signature"), Evidence.SpawnerSignature);
	Json->SetStringField(TEXT("schedule_owner_path"), Evidence.OwnerPath);
	if (!Evidence.Query.IsEmpty()) { Json->SetStringField(TEXT("schedule_query"), Evidence.Query); }
	if (!Evidence.MenuName.IsEmpty()) { Json->SetStringField(TEXT("schedule_menu_name"), Evidence.MenuName); }
	if (!Evidence.Category.IsEmpty()) { Json->SetStringField(TEXT("schedule_category"), Evidence.Category); }
	if (!Evidence.DelegatePinName.IsEmpty()) { Json->SetStringField(TEXT("schedule_delegate_pin_name"), Evidence.DelegatePinName); }
	if (!Evidence.GraphLatentAllowed.IsEmpty()) { Json->SetStringField(TEXT("graph_latent_allowed"), Evidence.GraphLatentAllowed); }
	return Json;
}

TSharedRef<FJsonObject> UGraphWriteActionContextUtils::MakeItemBase(
	const TSharedPtr<FJsonObject>& Request,
	const FString& Kind)
{
	TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("request_id"), UGraphWriteActionContextUtils::ReadStringField(Request, TEXT("request_id")));
	Item->SetStringField(TEXT("operation_id"), UGraphWriteActionContextUtils::ReadStringField(Request, TEXT("operation_id")));
	Item->SetStringField(TEXT("projection_kind"), Kind);
	return Item;
}

TSharedRef<FJsonObject> UGraphWriteActionContextUtils::MakeItemFailure(
	const TSharedPtr<FJsonObject>& Request,
	const FString& Kind,
	const FString& Message)
{
	TSharedRef<FJsonObject> Item = UGraphWriteActionContextUtils::MakeItemBase(Request, Kind);
	Item->SetStringField(TEXT("status"), TEXT("failed"));
	Item->SetStringField(TEXT("message"), Message);
	return Item;
}

TSharedRef<FJsonObject> UGraphWriteActionContextUtils::MakeItemSuccess(
	const TSharedPtr<FJsonObject>& Request,
	const FString& Kind,
	const TSharedRef<FJsonObject>& Evidence,
	const FString& Message)
{
	TSharedRef<FJsonObject> Item = UGraphWriteActionContextUtils::MakeItemBase(Request, Kind);
	Item->SetStringField(TEXT("status"), TEXT("resolved"));
	Item->SetStringField(TEXT("message"), Message);
	Item->SetObjectField(TEXT("evidence"), Evidence);
	return Item;
}

bool UGraphWriteActionContextUtils::TryProjectExactAssetAction(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperAssetActionProjectedCandidate& Candidate,
	FBlueprintHelperProjectedAssetActionEvidence& OutEvidence)
{
	FBlueprintHelperAssetActionProjectionRequest ExactRequest;
	ExactRequest.Blueprint = Blueprint;
	ExactRequest.TargetGraph = Graph;
	ExactRequest.RequiredEvidence.StableId = Candidate.StableId;
	ExactRequest.RequiredEvidence.NodeClassPath = Candidate.NodeClassPath;
	ExactRequest.RequiredEvidence.SpawnerSignature = Candidate.SpawnerSignature;
	ExactRequest.RequiredEvidence.OwnerPath = Candidate.OwnerPath;
	ExactRequest.RequiredEvidence.Query = Candidate.Query;
	ExactRequest.RequiredEvidence.MenuName = Candidate.MenuName;
	ExactRequest.RequiredEvidence.Category = Candidate.Category;

	const FBlueprintHelperAssetActionProjectionResult ExactProjection =
		FBlueprintHelperAssetActionProjectionService::Project(ExactRequest);
	if (ExactProjection.Status != EBlueprintHelperActionResolutionStatus::Resolved ||
		ExactProjection.Candidates.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperAssetActionProjectedCandidate& ExactCandidate = ExactProjection.Candidates[0];
	OutEvidence.StableId = ExactCandidate.StableId;
	OutEvidence.NodeClassPath = ExactCandidate.NodeClassPath;
	OutEvidence.SpawnerSignature = ExactCandidate.SpawnerSignature;
	OutEvidence.OwnerPath = ExactCandidate.OwnerPath;
	OutEvidence.Query = ExactCandidate.Query;
	OutEvidence.MenuName = ExactCandidate.MenuName;
	OutEvidence.Category = ExactCandidate.Category;
	return OutEvidence.HasProjectedIdentity();
}

bool UGraphWriteActionContextUtils::TryProjectAssetActionEvidence(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FString>& Queries,
	FBlueprintHelperProjectedAssetActionEvidence& OutEvidence,
	FString& OutMessage)
{
	for (const FString& Query : Queries)
	{
		FBlueprintHelperAssetActionProjectionRequest Request;
		Request.Blueprint = Blueprint;
		Request.TargetGraph = Graph;
		Request.RequiredEvidence.Query = Query;
		Request.Query = Query;

		const FBlueprintHelperAssetActionProjectionResult Projection =
			FBlueprintHelperAssetActionProjectionService::Project(Request);
		if ((Projection.Status == EBlueprintHelperActionResolutionStatus::Resolved ||
			Projection.Status == EBlueprintHelperActionResolutionStatus::Ambiguous) &&
			Projection.Candidates.Num() > 0 &&
			UGraphWriteActionContextUtils::TryProjectExactAssetAction(Blueprint, Graph, Projection.Candidates[0], OutEvidence))
		{
			OutMessage = FString::Printf(TEXT("asset_action projected from query '%s'."), *Query);
			return true;
		}
		OutMessage = Projection.Message;
	}
	return false;
}

bool UGraphWriteActionContextUtils::TryProjectExactSchedule(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate,
	FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence)
{
	FBlueprintHelperActionDatabaseProjectionRequest ExactRequest;
	ExactRequest.Blueprint = Blueprint;
	ExactRequest.TargetGraph = Graph;
	ExactRequest.RequiredEvidence.StableId = Candidate.StableId;
	ExactRequest.RequiredEvidence.NodeClassPath = Candidate.NodeClassPath;
	ExactRequest.RequiredEvidence.SpawnerSignature = Candidate.SpawnerSignature;
	ExactRequest.RequiredEvidence.OwnerPath = Candidate.OwnerPath;
	ExactRequest.RequiredEvidence.Query = Candidate.Query;
	ExactRequest.RequiredEvidence.MenuName = Candidate.MenuName;
	ExactRequest.RequiredEvidence.Category = Candidate.Category;
	ExactRequest.ErrorPrefix = TEXT("schedule");

	const FBlueprintHelperActionDatabaseProjectionResult ExactProjection =
		FBlueprintHelperActionDatabaseProjectionService::Project(ExactRequest);
	if (ExactProjection.Status != EBlueprintHelperActionResolutionStatus::Resolved ||
		ExactProjection.Candidates.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperActionDatabaseProjectedCandidate& ExactCandidate = ExactProjection.Candidates[0];
	OutEvidence.StableId = ExactCandidate.StableId;
	OutEvidence.NodeClassPath = ExactCandidate.NodeClassPath;
	OutEvidence.SpawnerSignature = ExactCandidate.SpawnerSignature;
	OutEvidence.OwnerPath = ExactCandidate.OwnerPath;
	OutEvidence.Query = ExactCandidate.Query;
	OutEvidence.MenuName = ExactCandidate.MenuName;
	OutEvidence.Category = ExactCandidate.Category;
	return OutEvidence.HasProjectedIdentity();
}

bool UGraphWriteActionContextUtils::TryProjectScheduleEvidence(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FString>& Queries,
	FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence,
	FString& OutMessage)
{
	for (const FString& Query : Queries)
	{
		FBlueprintHelperActionDatabaseProjectionRequest Request;
		Request.Blueprint = Blueprint;
		Request.TargetGraph = Graph;
		Request.RequiredEvidence.Query = Query;
		Request.Query = Query;
		Request.ErrorPrefix = TEXT("schedule");

		const FBlueprintHelperActionDatabaseProjectionResult Projection =
			FBlueprintHelperActionDatabaseProjectionService::Project(Request);
		if ((Projection.Status == EBlueprintHelperActionResolutionStatus::Resolved ||
			Projection.Status == EBlueprintHelperActionResolutionStatus::Ambiguous) &&
			Projection.Candidates.Num() > 0 &&
			UGraphWriteActionContextUtils::TryProjectExactSchedule(Blueprint, Graph, Projection.Candidates[0], OutEvidence))
		{
			OutMessage = FString::Printf(TEXT("schedule projected from query '%s'."), *Query);
			return true;
		}
		OutMessage = Projection.Message;
	}
	return false;
}

TSharedRef<FJsonObject> UGraphWriteActionContextUtils::ProjectRequestItem(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TSharedPtr<FJsonObject>& Request,
	bool& bAllResolved)
{
	const FString Kind = UGraphWriteActionContextUtils::ReadStringField(Request, TEXT("projection_kind")).IsEmpty()
		? UGraphWriteActionContextUtils::ReadStringField(Request, TEXT("kind"))
		: UGraphWriteActionContextUtils::ReadStringField(Request, TEXT("projection_kind"));
	const TArray<FString> Queries = UGraphWriteActionContextUtils::ReadQueries(Request);
	if (Kind.IsEmpty())
	{
		bAllResolved = false;
		return UGraphWriteActionContextUtils::MakeItemFailure(Request, TEXT("unknown"), TEXT("projection request requires kind or projection_kind."));
	}
	if (Queries.Num() == 0)
	{
		bAllResolved = false;
		return UGraphWriteActionContextUtils::MakeItemFailure(Request, Kind, TEXT("projection request requires query or queries."));
	}

	if (Kind.Equals(TEXT("asset_action"), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperProjectedAssetActionEvidence Evidence;
		FString Message;
		if (UGraphWriteActionContextUtils::TryProjectAssetActionEvidence(Blueprint, Graph, Queries, Evidence, Message))
		{
			return UGraphWriteActionContextUtils::MakeItemSuccess(Request, TEXT("asset_action"), UGraphWriteActionContextUtils::MakeEvidenceJson(Evidence), Message);
		}
		bAllResolved = false;
		return UGraphWriteActionContextUtils::MakeItemFailure(Request, TEXT("asset_action"), Message.IsEmpty() ? TEXT("asset_action projection failed.") : Message);
	}

	if (Kind.Equals(TEXT("schedule"), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperProjectedScheduleActionEvidence Evidence;
		FString Message;
		if (UGraphWriteActionContextUtils::TryProjectScheduleEvidence(Blueprint, Graph, Queries, Evidence, Message))
		{
			const FString GraphLatentAllowed = UGraphWriteActionContextUtils::ReadStringField(Request, TEXT("graph_latent_allowed"));
			if (!GraphLatentAllowed.IsEmpty())
			{
				Evidence.GraphLatentAllowed = GraphLatentAllowed;
			}
			return UGraphWriteActionContextUtils::MakeItemSuccess(Request, TEXT("schedule"), UGraphWriteActionContextUtils::MakeEvidenceJson(Evidence), Message);
		}
		bAllResolved = false;
		return UGraphWriteActionContextUtils::MakeItemFailure(Request, TEXT("schedule"), Message.IsEmpty() ? TEXT("schedule projection failed.") : Message);
	}

	bAllResolved = false;
	return UGraphWriteActionContextUtils::MakeItemFailure(Request, Kind, FString::Printf(TEXT("unsupported projection kind '%s'."), *Kind));
}
