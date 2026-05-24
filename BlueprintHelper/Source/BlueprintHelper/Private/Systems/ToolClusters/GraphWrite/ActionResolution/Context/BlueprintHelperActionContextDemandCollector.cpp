#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"

namespace BlueprintHelperActionContextDemandCollector
{
static FString FirstNonEmpty(const FString& First, const FString& Second)
{
	return First.IsEmpty() ? Second : First;
}

static FString FirstNonEmpty(const FString& First, const FString& Second, const FString& Third)
{
	return FirstNonEmpty(FirstNonEmpty(First, Second), Third);
}

static FString FirstNonEmpty(const FString& First, const FString& Second, const FString& Third, const FString& Fourth)
{
	return FirstNonEmpty(FirstNonEmpty(First, Second, Third), Fourth);
}

static FString FirstNonEmpty(const FString& First, const FString& Second, const FString& Third, const FString& Fourth, const FString& Fifth)
{
	return FirstNonEmpty(FirstNonEmpty(First, Second, Third, Fourth), Fifth);
}

static FString FirstNonEmpty(
	const FString& First,
	const FString& Second,
	const FString& Third,
	const FString& Fourth,
	const FString& Fifth,
	const FString& Sixth)
{
	return FirstNonEmpty(FirstNonEmpty(First, Second, Third, Fourth, Fifth), Sixth);
}

static bool IsEventDelegateSemantic(const EBlueprintHelperActionSemanticKind SemanticKind);

static FString BuildStatementQuery(const FBlueprintHelperGraphStatementIR& Statement, const EBlueprintHelperActionSemanticKind SemanticKind)
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

	if (IsEventDelegateSemantic(SemanticKind))
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

static FString BuildExpressionQuery(const FBlueprintHelperGraphExpressionIR& Expression, const EBlueprintHelperActionSemanticKind SemanticKind)
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

	return FirstNonEmpty(
		Expression.ResolvedCallFunctionStableId,
		Expression.Name,
		Expression.PatternName);
}

static FString DemandIdFromPath(const FString& Prefix, const FString& Path)
{
	return FString::Printf(TEXT("%s:%s"), *Prefix, *Path);
}

static TArray<FString> SortedArgumentNames(const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args)
{
	TArray<FString> Names;
	Args.GetKeys(Names);
	Names.Sort();
	return Names;
}

static void CopyExpressionMapContext(
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

static void CopyNamedExpressionContext(
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

static FString ResolveComponentPathFromTarget(const FBlueprintHelperGraphResolvedTarget& Target)
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

static bool IsEventDelegateSemantic(const EBlueprintHelperActionSemanticKind SemanticKind)
{
	return SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		|| SemanticKind == EBlueprintHelperActionSemanticKind::Delegate;
}

static EBlueprintHelperActionSemanticFamily ResolveSemanticFamily(
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& FieldScope)
{
	switch (SemanticKind)
	{
	case EBlueprintHelperActionSemanticKind::Call:
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

static EBlueprintHelperTypeOperation ResolveTypeOperation(const EBlueprintHelperActionSemanticKind SemanticKind)
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

static FString GetDefaultValue(
	const FBlueprintHelperActionContextDemand& Demand,
	const TCHAR* Key)
{
	return Demand.DefaultValues.FindRef(Key).TrimStartAndEnd();
}

static FString NormalizeSemanticOperationToken(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}

static bool ShouldRouteConvertToGeneric(const FString& ExplicitFunctionOperation, const FString& ExplicitTransformOperation)
{
	const FString FunctionOperation = NormalizeSemanticOperationToken(ExplicitFunctionOperation);
	const FString TransformOperation = NormalizeSemanticOperationToken(ExplicitTransformOperation);
	return FunctionOperation.IsEmpty()
		&& !TransformOperation.IsEmpty()
		&& TransformOperation != TEXT("convert");
}

static bool ShouldRouteScheduleToGeneric(const FString& ExplicitFunctionOperation, const FString& ExplicitScheduleOperation)
{
	const FString FunctionOperation = NormalizeSemanticOperationToken(ExplicitFunctionOperation);
	const FString ScheduleOperation = NormalizeSemanticOperationToken(ExplicitScheduleOperation);
	return FunctionOperation.IsEmpty()
		&& !ScheduleOperation.IsEmpty()
		&& ScheduleOperation != TEXT("latent_or_async");
}

static FBlueprintHelperCallFunctionPinType MakePinTypeFromToken(const FString& Token)
{
	FBlueprintHelperCallFunctionPinType PinType;
	TArray<FString> Parts;
	Token.TrimStartAndEnd().ParseIntoArray(Parts, TEXT("|"), true);
	if (Parts.Num() > 0)
	{
		PinType.Category = Parts[0];
	}
	if (Parts.Num() > 1)
	{
		PinType.ObjectPath = Parts[1];
	}
	return PinType;
}

static void ApplyFunctionSemanticOperations(FBlueprintHelperActionContextDemand& InOutDemand)
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
			InOutDemand.FunctionOperation = GetDefaultValue(InOutDemand, TEXT("function_operation"));
		}
		if (InOutDemand.FunctionOperation.IsEmpty())
		{
			InOutDemand.FunctionOperation = TEXT("schedule_function");
		}
		if (InOutDemand.ScheduleOperation.IsEmpty())
		{
			InOutDemand.ScheduleOperation = GetDefaultValue(InOutDemand, TEXT("schedule_operation"));
		}
		if (InOutDemand.ScheduleOperation.IsEmpty())
		{
			InOutDemand.ScheduleOperation = TEXT("latent_or_async");
		}
		break;
	default:
		break;
	}
}

static void RouteConvertScheduleDemandToGeneric(FBlueprintHelperActionContextDemand& InOutDemand)
{
	InOutDemand.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	InOutDemand.FunctionOperation.Reset();
	InOutDemand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
	InOutDemand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
}

static void ApplyConvertScheduleEvidence(
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

	const FString FunctionOperation = NormalizeSemanticOperationToken(ExplicitFunctionOperation);
	const FString TransformOperation = NormalizeSemanticOperationToken(ExplicitTransformOperation);
	const FString ScheduleOperation = NormalizeSemanticOperationToken(ExplicitScheduleOperation);
	if (!FunctionOperation.IsEmpty())
	{
		InOutDemand.FunctionOperation = FunctionOperation;
		InOutDemand.DefaultValues.Add(TEXT("function_operation"), FunctionOperation);
	}
	if (!TransformOperation.IsEmpty())
	{
		InOutDemand.TransformOperation = TransformOperation;
		InOutDemand.DefaultValues.Add(TEXT("transform_operation"), TransformOperation);
	}
	if (!ScheduleOperation.IsEmpty())
	{
		InOutDemand.ScheduleOperation = ScheduleOperation;
		InOutDemand.DefaultValues.Add(TEXT("schedule_operation"), ScheduleOperation);
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
	}

	const FString GraphLatentAllowed = ExplicitGraphLatentAllowed.TrimStartAndEnd().ToLower();
	if (!GraphLatentAllowed.IsEmpty())
	{
		InOutDemand.GraphLatentAllowed = GraphLatentAllowed;
		InOutDemand.DefaultValues.Add(TEXT("graph_latent_allowed"), GraphLatentAllowed);
	}

	if (InOutDemand.SemanticKind == EBlueprintHelperActionSemanticKind::Convert
		&& ShouldRouteConvertToGeneric(FunctionOperation, TransformOperation))
	{
		RouteConvertScheduleDemandToGeneric(InOutDemand);
	}
	else if (InOutDemand.SemanticKind == EBlueprintHelperActionSemanticKind::Schedule
		&& ShouldRouteScheduleToGeneric(FunctionOperation, ScheduleOperation))
	{
		RouteConvertScheduleDemandToGeneric(InOutDemand);
	}
}

static void ApplyCreateStatementEvidence(
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
	InOutDemand.Query = InOutDemand.CreateOperation;
	if (!InOutDemand.ClassPath.IsEmpty())
	{
		InOutDemand.TargetPath = InOutDemand.ClassPath;
	}
	if (!Statement.PinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("element"), Statement.PinType.TrimStartAndEnd());
		InOutDemand.ContainerElementPinType = MakePinTypeFromToken(Statement.PinType);
	}
	if (!Statement.KeyPinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("key"), Statement.KeyPinType.TrimStartAndEnd());
		InOutDemand.ContainerKeyPinType = MakePinTypeFromToken(Statement.KeyPinType);
	}
	if (!Statement.ValuePinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("value"), Statement.ValuePinType.TrimStartAndEnd());
		InOutDemand.ContainerValuePinType = MakePinTypeFromToken(Statement.ValuePinType);
	}
}

static void ApplyCreateExpressionEvidence(
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
	InOutDemand.Query = InOutDemand.CreateOperation;
	if (!InOutDemand.ClassPath.IsEmpty())
	{
		InOutDemand.TargetPath = InOutDemand.ClassPath;
	}
	if (!Expression.PinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("element"), Expression.PinType.TrimStartAndEnd());
		InOutDemand.ContainerElementPinType = MakePinTypeFromToken(Expression.PinType);
	}
	if (!Expression.KeyPinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("key"), Expression.KeyPinType.TrimStartAndEnd());
		InOutDemand.ContainerKeyPinType = MakePinTypeFromToken(Expression.KeyPinType);
	}
	if (!Expression.ValuePinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("value"), Expression.ValuePinType.TrimStartAndEnd());
		InOutDemand.ContainerValuePinType = MakePinTypeFromToken(Expression.ValuePinType);
	}
}

static void ApplyEventDelegateStatementEvidence(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (!IsEventDelegateSemantic(InOutDemand.SemanticKind))
	{
		return;
	}

	if (InOutDemand.ComponentPath.IsEmpty())
	{
		InOutDemand.ComponentPath = FirstNonEmpty(
			Statement.ComponentName,
			ResolveComponentPathFromTarget(Statement.ResolvedTarget));
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
	if (InOutDemand.UnbindMode.IsEmpty())
	{
		InOutDemand.UnbindMode = Statement.UnbindMode.TrimStartAndEnd();
	}
	if (!InOutDemand.HandlerName.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("handler_name"), InOutDemand.HandlerName);
	}
	if (!InOutDemand.DelegateOperation.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("delegate_operation"), InOutDemand.DelegateOperation);
	}
	if (!InOutDemand.UnbindMode.IsEmpty())
	{
		InOutDemand.DefaultValues.Add(TEXT("unbind_mode"), InOutDemand.UnbindMode);
	}
}

static void ApplyEventDelegateExpressionEvidence(
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (!IsEventDelegateSemantic(InOutDemand.SemanticKind))
	{
		return;
	}

	if (InOutDemand.ComponentPath.IsEmpty())
	{
		InOutDemand.ComponentPath = ResolveComponentPathFromTarget(Expression.ResolvedTarget);
	}
	if (InOutDemand.DelegateName.IsEmpty())
	{
		InOutDemand.DelegateName = FirstNonEmpty(Expression.Property, Expression.Name, Expression.ResolvedTarget.Member);
	}
	if (InOutDemand.DelegateSignature.IsEmpty() && !Expression.Type.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.DelegateSignature = Expression.Type.TrimStartAndEnd();
	}
}
}

TArray<FBlueprintHelperActionContextDemand> FBlueprintHelperActionContextDemandCollector::CollectFromSemanticIR(
	const FBlueprintHelperGraphSemanticIR& SemanticIR)
{
	TArray<FBlueprintHelperActionContextDemand> Demands = CollectFromStatements(SemanticIR.Statements);

	for (const TPair<FString, FBlueprintHelperGraphSymbol>& SymbolPair : SemanticIR.Symbols)
	{
		for (FBlueprintHelperActionContextDemand& Demand : Demands)
		{
			if (Demand.StatementId == SymbolPair.Value.SourceStatementId
				|| Demand.StatementId == SymbolPair.Value.SourceExpressionId)
			{
				Demand.ConsumerSymbolIds.AddUnique(SymbolPair.Value.SymbolId);
			}
		}
	}

	return Demands;
}

TArray<FBlueprintHelperActionContextDemand> FBlueprintHelperActionContextDemandCollector::CollectFromStatements(
	const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements)
{
	TArray<FBlueprintHelperActionContextDemand> Demands;
	CollectFromStatementArray(Statements, Demands);
	return Demands;
}

FBlueprintHelperActionContextDemand FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
	const FString& StableId,
	const FString& SourcePath,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	const TArray<FString>& ArgumentNames,
	const FString& FieldOperation,
	const FString& FieldScope)
{
	return BuildDemand(
		StableId,
		SourcePath,
		SemanticKind,
		Query,
		TargetPath,
		PropertyPath,
		TypeName,
		SearchMode,
		AmbiguityPolicy,
		CategoryPriority,
		ArgumentNames,
		FieldOperation,
		FieldScope);
}

void FBlueprintHelperActionContextDemandCollector::CollectFromStatementArray(
	const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
	TArray<FBlueprintHelperActionContextDemand>& OutDemands)
{
	for (const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement : Statements)
	{
		if (!Statement.IsValid())
		{
			continue;
		}

		AppendDemandForStatement(*Statement, OutDemands);
		CollectFromStatementArray(Statement->ThenStatements, OutDemands);
		CollectFromStatementArray(Statement->ElseStatements, OutDemands);
	}
}

void FBlueprintHelperActionContextDemandCollector::AppendDemandForStatement(
	const FBlueprintHelperGraphStatementIR& Statement,
	TArray<FBlueprintHelperActionContextDemand>& OutDemands)
{
	const EBlueprintHelperActionSemanticKind SemanticKind = ToActionSemanticKind(Statement.Kind);
	const FString FieldOperation = Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		? Statement.FieldOperation
		: (Statement.Kind == EBlueprintHelperGraphStatementKind::Let ? TEXT("set") : FString());
	const FString FieldScope = Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		? Statement.FieldScope
		: (Statement.Kind == EBlueprintHelperGraphStatementKind::Let ? TEXT("variable") : FString());
	const FString StableId = Statement.StatementId.IsEmpty()
		? BlueprintHelperActionContextDemandCollector::DemandIdFromPath(TEXT("statement"), Statement.Path)
		: Statement.StatementId;

	FBlueprintHelperActionContextDemand Demand = BuildDemand(
		StableId,
		Statement.Path,
		SemanticKind,
		BlueprintHelperActionContextDemandCollector::BuildStatementQuery(Statement, SemanticKind),
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(
			Statement.ResolvedTarget.Raw,
			Statement.Target,
			Statement.Name),
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(
			Statement.Property,
			Statement.ResolvedTarget.PropertyPath),
		Statement.ResolvedTarget.Type,
		Statement.SearchMode,
		Statement.AmbiguityPolicy,
		Statement.CategoryPriority,
		BlueprintHelperActionContextDemandCollector::SortedArgumentNames(Statement.Args),
		FieldOperation,
		FieldScope);
	BlueprintHelperActionContextDemandCollector::CopyExpressionMapContext(Statement.Args, Demand);
	BlueprintHelperActionContextDemandCollector::ApplyCreateStatementEvidence(Statement, Demand);
	BlueprintHelperActionContextDemandCollector::ApplyConvertScheduleEvidence(
		Statement.FunctionOperation,
		Statement.TransformOperation,
		Statement.ScheduleOperation,
		Statement.ClassPath,
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(Statement.Target, Statement.Name),
		Statement.GraphLatentAllowed,
		Demand);
	BlueprintHelperActionContextDemandCollector::ApplyFunctionSemanticOperations(Demand);
	if (Statement.Value.IsValid() && Demand.ExpectedReturnType.IsEmpty())
	{
		Demand.ExpectedReturnType = Statement.Value->Type;
	}
	if (Statement.TargetObject.IsValid())
	{
		Demand.TargetObjectType = Statement.TargetObject->Type;
		Demand.BindingObjectPath = !Statement.TargetObject->ResolvedTarget.Raw.IsEmpty()
			? Statement.TargetObject->ResolvedTarget.Raw
			: Statement.TargetObject->Target;
	}
	BlueprintHelperActionContextDemandCollector::ApplyEventDelegateStatementEvidence(Statement, Demand);

	if (Demand.SemanticKind != EBlueprintHelperActionSemanticKind::Unknown)
	{
		OutDemands.Add(MoveTemp(Demand));
	}

	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Statement.Args)
	{
		if (ArgPair.Value.IsValid())
		{
			AppendDemandForExpression(*ArgPair.Value, StableId, OutDemands);
		}
	}

	if (Statement.Value.IsValid())
	{
		AppendDemandForExpression(*Statement.Value, StableId, OutDemands);
	}

	if (Statement.Condition.IsValid())
	{
		AppendDemandForExpression(*Statement.Condition, StableId, OutDemands);
	}

	if (Statement.TargetObject.IsValid())
	{
		AppendDemandForExpression(*Statement.TargetObject, StableId, OutDemands);
	}
}

void FBlueprintHelperActionContextDemandCollector::AppendDemandForExpression(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& OwnerStatementId,
	TArray<FBlueprintHelperActionContextDemand>& OutDemands)
{
	const EBlueprintHelperActionSemanticKind SemanticKind = ToActionSemanticKind(Expression.Kind);
	const FString FieldOperation = Expression.Kind == EBlueprintHelperGraphExpressionKind::Field
		? Expression.FieldOperation
		: FString();
	const FString FieldScope = Expression.Kind == EBlueprintHelperGraphExpressionKind::Field
		? Expression.FieldScope
		: FString();
	const FString StableId = Expression.ExpressionId.IsEmpty()
		? BlueprintHelperActionContextDemandCollector::DemandIdFromPath(TEXT("expression"), Expression.Path)
		: Expression.ExpressionId;

	FBlueprintHelperActionContextDemand Demand = BuildDemand(
		StableId,
		Expression.Path,
		SemanticKind,
		BlueprintHelperActionContextDemandCollector::BuildExpressionQuery(Expression, SemanticKind),
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(
			Expression.ResolvedTarget.Raw,
			Expression.Target,
			Expression.Name),
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(
			Expression.Property,
			Expression.ResolvedTarget.PropertyPath),
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(
			Expression.Type,
			Expression.ResolvedTarget.Type),
		Expression.SearchMode,
		Expression.AmbiguityPolicy,
		Expression.CategoryPriority,
		BlueprintHelperActionContextDemandCollector::SortedArgumentNames(Expression.Args),
		FieldOperation,
		FieldScope);
	BlueprintHelperActionContextDemandCollector::CopyExpressionMapContext(Expression.Args, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("left"), Expression.Left, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("right"), Expression.Right, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("value"), Expression.Value, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("condition"), Expression.Condition, Demand);
	BlueprintHelperActionContextDemandCollector::ApplyCreateExpressionEvidence(Expression, Demand);
	BlueprintHelperActionContextDemandCollector::ApplyConvertScheduleEvidence(
		Expression.FunctionOperation,
		Expression.TransformOperation,
		Expression.ScheduleOperation,
		Expression.ClassPath,
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(Expression.Target, Expression.Name),
		Expression.GraphLatentAllowed,
		Demand);
	BlueprintHelperActionContextDemandCollector::ApplyFunctionSemanticOperations(Demand);
	Demand.ExpectedReturnType = Expression.Type;
	if (Expression.TargetObject.IsValid())
	{
		Demand.TargetObjectType = Expression.TargetObject->Type;
		Demand.BindingObjectPath = !Expression.TargetObject->ResolvedTarget.Raw.IsEmpty()
			? Expression.TargetObject->ResolvedTarget.Raw
			: Expression.TargetObject->Target;
	}
	BlueprintHelperActionContextDemandCollector::ApplyEventDelegateExpressionEvidence(Expression, Demand);

	if (Demand.SemanticKind != EBlueprintHelperActionSemanticKind::Unknown)
	{
		if (!OwnerStatementId.IsEmpty())
		{
			Demand.SourceSymbolIds.AddUnique(OwnerStatementId);
		}
		OutDemands.Add(MoveTemp(Demand));
	}

	if (Expression.TargetObject.IsValid())
	{
		AppendDemandForExpression(*Expression.TargetObject, OwnerStatementId, OutDemands);
	}

	if (Expression.Value.IsValid())
	{
		AppendDemandForExpression(*Expression.Value, OwnerStatementId, OutDemands);
	}

	if (Expression.Condition.IsValid())
	{
		AppendDemandForExpression(*Expression.Condition, OwnerStatementId, OutDemands);
	}

	if (Expression.ThenValue.IsValid())
	{
		AppendDemandForExpression(*Expression.ThenValue, OwnerStatementId, OutDemands);
	}

	if (Expression.ElseValue.IsValid())
	{
		AppendDemandForExpression(*Expression.ElseValue, OwnerStatementId, OutDemands);
	}

	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
	{
		if (ArgPair.Value.IsValid())
		{
			AppendDemandForExpression(*ArgPair.Value, OwnerStatementId, OutDemands);
		}
	}

	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& FieldPair : Expression.Fields)
	{
		if (FieldPair.Value.IsValid())
		{
			AppendDemandForExpression(*FieldPair.Value, OwnerStatementId, OutDemands);
		}
	}

	for (const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option : Expression.Options)
	{
		if (Option.IsValid())
		{
			AppendDemandForExpression(*Option, OwnerStatementId, OutDemands);
		}
	}

	if (Expression.Left.IsValid())
	{
		AppendDemandForExpression(*Expression.Left, OwnerStatementId, OutDemands);
	}

	if (Expression.Right.IsValid())
	{
		AppendDemandForExpression(*Expression.Right, OwnerStatementId, OutDemands);
	}
}

FBlueprintHelperActionContextDemand FBlueprintHelperActionContextDemandCollector::BuildDemand(
	const FString& StableId,
	const FString& SourcePath,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& PropertyPath,
	const FString& TypeName,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority,
	const TArray<FString>& ArgumentNames,
	const FString& FieldOperation,
	const FString& FieldScope)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = StableId;
	Demand.SourcePath = SourcePath;
	Demand.SemanticKind = SemanticKind;
	Demand.SemanticFamily = BlueprintHelperActionContextDemandCollector::ResolveSemanticFamily(SemanticKind, FieldScope);
	Demand.TypeOperation = BlueprintHelperActionContextDemandCollector::ResolveTypeOperation(SemanticKind);
	Demand.Query = Query;
	Demand.TargetPath = TargetPath;
	Demand.PropertyPath = PropertyPath;
	Demand.TypeName = TypeName;
	if (Demand.SemanticFamily == EBlueprintHelperActionSemanticFamily::Struct)
	{
		Demand.StructPath = TypeName;
	}
	else if (Demand.SemanticFamily == EBlueprintHelperActionSemanticFamily::TypeStructure)
	{
		Demand.TypeStructureId = TypeName;
	}
	Demand.FieldOperation = FieldOperation.TrimStartAndEnd().ToLower();
	Demand.FieldScope = FieldScope.TrimStartAndEnd().ToLower();
	Demand.ExpectedReturnType = TypeName;
	Demand.SearchMode = SearchMode;
	Demand.AmbiguityPolicy = AmbiguityPolicy;
	Demand.CategoryPriority = CategoryPriority;
	Demand.ArgumentNames = ArgumentNames;
	ApplyDemandKinds(Demand);
	BlueprintHelperActionContextDemandCollector::ApplyFunctionSemanticOperations(Demand);
	if (Demand.ClusterKind == EBlueprintHelperSpawnerClusterKind::FieldVariableAction)
	{
		if (!Demand.FieldOperation.IsEmpty())
		{
			Demand.DefaultValues.Add(TEXT("field_operation"), Demand.FieldOperation);
		}
		if (!Demand.FieldScope.IsEmpty())
		{
			Demand.DefaultValues.Add(TEXT("field_scope"), Demand.FieldScope);
		}
		const FString FieldQuery = !Demand.PropertyPath.IsEmpty()
			? Demand.PropertyPath
			: Demand.TargetPath;
		if (!FieldQuery.IsEmpty())
		{
			Demand.Query = FieldQuery;
			if (Demand.TargetPath.IsEmpty())
			{
				Demand.TargetPath = FieldQuery;
			}
		}
	}
	else if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Construct
		|| Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Deconstruct)
	{
		if (!Demand.TypeName.IsEmpty())
		{
			Demand.Query = Demand.TypeName;
			if (Demand.TargetPath.IsEmpty())
			{
				Demand.TargetPath = Demand.TypeName;
			}
			if (Demand.StructPath.IsEmpty() && Demand.SemanticFamily == EBlueprintHelperActionSemanticFamily::Struct)
			{
				Demand.StructPath = Demand.TypeName;
			}
			if (Demand.TypeStructureId.IsEmpty() && Demand.SemanticFamily == EBlueprintHelperActionSemanticFamily::TypeStructure)
			{
				Demand.TypeStructureId = Demand.TypeName;
			}
		}
	}
	return Demand;
}

void FBlueprintHelperActionContextDemandCollector::ApplyDemandKinds(FBlueprintHelperActionContextDemand& Demand)
{
	if (Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Unknown)
	{
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		return;
	}

	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Graph);
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::SearchPolicy);

	switch (Demand.SemanticKind)
	{
	case EBlueprintHelperActionSemanticKind::Call:
	case EBlueprintHelperActionSemanticKind::Op:
	case EBlueprintHelperActionSemanticKind::Convert:
	case EBlueprintHelperActionSemanticKind::Schedule:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	case EBlueprintHelperActionSemanticKind::Field:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Delegate:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Binding);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
	case EBlueprintHelperActionSemanticKind::Select:
	case EBlueprintHelperActionSemanticKind::Control:
	case EBlueprintHelperActionSemanticKind::Create:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	default:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
		break;
	}
}

EBlueprintHelperActionSemanticKind FBlueprintHelperActionContextDemandCollector::ToActionSemanticKind(
	EBlueprintHelperGraphStatementKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		return EBlueprintHelperActionSemanticKind::Call;
	case EBlueprintHelperGraphStatementKind::Field:
		return EBlueprintHelperActionSemanticKind::Field;
	case EBlueprintHelperGraphStatementKind::Branch:
	case EBlueprintHelperGraphStatementKind::Sequence:
	case EBlueprintHelperGraphStatementKind::Return:
		return EBlueprintHelperActionSemanticKind::Control;
	case EBlueprintHelperGraphStatementKind::Create:
		return EBlueprintHelperActionSemanticKind::Create;
	case EBlueprintHelperGraphStatementKind::Convert:
		return EBlueprintHelperActionSemanticKind::Convert;
	case EBlueprintHelperGraphStatementKind::Schedule:
		return EBlueprintHelperActionSemanticKind::Schedule;
	case EBlueprintHelperGraphStatementKind::Let:
		return EBlueprintHelperActionSemanticKind::Field;
	case EBlueprintHelperGraphStatementKind::ComponentBoundEvent:
		return EBlueprintHelperActionSemanticKind::ComponentBoundEvent;
	case EBlueprintHelperGraphStatementKind::Delegate:
		return EBlueprintHelperActionSemanticKind::Delegate;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}

EBlueprintHelperActionSemanticKind FBlueprintHelperActionContextDemandCollector::ToActionSemanticKind(
	EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Field:
		return EBlueprintHelperActionSemanticKind::Field;
	case EBlueprintHelperGraphExpressionKind::Call:
		return EBlueprintHelperActionSemanticKind::Call;
	case EBlueprintHelperGraphExpressionKind::Op:
		return EBlueprintHelperActionSemanticKind::Op;
	case EBlueprintHelperGraphExpressionKind::Construct:
		return EBlueprintHelperActionSemanticKind::Construct;
	case EBlueprintHelperGraphExpressionKind::Deconstruct:
		return EBlueprintHelperActionSemanticKind::Deconstruct;
	case EBlueprintHelperGraphExpressionKind::Select:
		return EBlueprintHelperActionSemanticKind::Select;
	case EBlueprintHelperGraphExpressionKind::Create:
		return EBlueprintHelperActionSemanticKind::Create;
	case EBlueprintHelperGraphExpressionKind::Convert:
		return EBlueprintHelperActionSemanticKind::Convert;
	case EBlueprintHelperGraphExpressionKind::Schedule:
		return EBlueprintHelperActionSemanticKind::Schedule;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}
