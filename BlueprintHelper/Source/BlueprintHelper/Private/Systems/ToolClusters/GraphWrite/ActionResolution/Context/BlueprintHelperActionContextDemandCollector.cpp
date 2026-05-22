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

static FString BuildStatementQuery(const FBlueprintHelperGraphStatementIR& Statement, const EBlueprintHelperActionSemanticKind SemanticKind)
{
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Call)
	{
		return FirstNonEmpty(
			Statement.ResolvedCallFunctionStableId,
			Statement.Target,
			Statement.Name,
			Statement.PatternName);
	}

	return FirstNonEmpty(
		Statement.ResolvedCallFunctionStableId,
		Statement.Name,
		Statement.PatternName);
}

static FString BuildExpressionQuery(const FBlueprintHelperGraphExpressionIR& Expression, const EBlueprintHelperActionSemanticKind SemanticKind)
{
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
	return SemanticKind == EBlueprintHelperActionSemanticKind::Event
		|| SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
		|| SemanticKind == EBlueprintHelperActionSemanticKind::Bind;
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
		InOutDemand.ComponentPath = ResolveComponentPathFromTarget(Statement.ResolvedTarget);
	}
	if (InOutDemand.DelegateName.IsEmpty())
	{
		InOutDemand.DelegateName = FirstNonEmpty(Statement.Property, Statement.Name, Statement.ResolvedTarget.Member);
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
		BlueprintHelperActionContextDemandCollector::SortedArgumentNames(Statement.Args));
	BlueprintHelperActionContextDemandCollector::CopyExpressionMapContext(Statement.Args, Demand);
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
		BlueprintHelperActionContextDemandCollector::SortedArgumentNames(Expression.Args));
	BlueprintHelperActionContextDemandCollector::CopyExpressionMapContext(Expression.Args, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("left"), Expression.Left, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("right"), Expression.Right, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("value"), Expression.Value, Demand);
	BlueprintHelperActionContextDemandCollector::CopyNamedExpressionContext(TEXT("condition"), Expression.Condition, Demand);
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
	const TArray<FString>& ArgumentNames)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = StableId;
	Demand.SourcePath = SourcePath;
	Demand.SemanticKind = SemanticKind;
	Demand.Query = Query;
	Demand.TargetPath = TargetPath;
	Demand.PropertyPath = PropertyPath;
	Demand.TypeName = TypeName;
	Demand.ExpectedReturnType = TypeName;
	Demand.SearchMode = SearchMode;
	Demand.AmbiguityPolicy = AmbiguityPolicy;
	Demand.CategoryPriority = CategoryPriority;
	Demand.ArgumentNames = ArgumentNames;
	ApplyDemandKinds(Demand);
	if (Demand.ClusterKind == EBlueprintHelperSpawnerClusterKind::FieldVariableAction)
	{
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

	case EBlueprintHelperActionSemanticKind::Get:
	case EBlueprintHelperActionSemanticKind::Set:
	case EBlueprintHelperActionSemanticKind::GetProperty:
	case EBlueprintHelperActionSemanticKind::SetProperty:
		Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
		Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
		break;

	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Bind:
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
	case EBlueprintHelperGraphStatementKind::Set:
		return EBlueprintHelperActionSemanticKind::Set;
	case EBlueprintHelperGraphStatementKind::SetProperty:
		return EBlueprintHelperActionSemanticKind::SetProperty;
	case EBlueprintHelperGraphStatementKind::Branch:
	case EBlueprintHelperGraphStatementKind::Sequence:
	case EBlueprintHelperGraphStatementKind::Return:
		return EBlueprintHelperActionSemanticKind::Control;
	case EBlueprintHelperGraphStatementKind::Let:
		return EBlueprintHelperActionSemanticKind::Set;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}

EBlueprintHelperActionSemanticKind FBlueprintHelperActionContextDemandCollector::ToActionSemanticKind(
	EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Get:
		return EBlueprintHelperActionSemanticKind::Get;
	case EBlueprintHelperGraphExpressionKind::GetProperty:
		return EBlueprintHelperActionSemanticKind::GetProperty;
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
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}
