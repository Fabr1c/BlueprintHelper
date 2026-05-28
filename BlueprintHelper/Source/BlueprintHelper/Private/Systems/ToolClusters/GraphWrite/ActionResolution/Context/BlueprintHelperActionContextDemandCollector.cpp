#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionContextUtils.h"

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
		? UGraphWriteActionContextUtils::DemandIdFromPath(TEXT("statement"), Statement.Path)
		: Statement.StatementId;

	FBlueprintHelperActionContextDemand Demand = BuildDemand(
		StableId,
		Statement.Path,
		SemanticKind,
		UGraphWriteActionContextUtils::BuildStatementQuery(Statement, SemanticKind),
		UGraphWriteActionContextUtils::BuildStatementTargetPath(Statement, SemanticKind),
		FirstNonEmpty(
			Statement.Property,
			Statement.ResolvedTarget.PropertyPath),
		Statement.ResolvedTarget.Type,
		Statement.SearchMode,
		Statement.AmbiguityPolicy,
		Statement.CategoryPriority,
		UGraphWriteActionContextUtils::SortedArgumentNames(Statement.Args),
		FieldOperation,
		FieldScope);
	UGraphWriteActionContextUtils::CopyExpressionMapContext(Statement.Args, Demand);
	UGraphWriteActionContextUtils::CopyFocusedContextEvidence(Statement.ContextEvidence, Demand);
	UGraphWriteActionContextUtils::ApplyStatementCapabilityFacts(Statement, Demand);
	UGraphWriteActionContextUtils::ApplyContainerActionStatementEvidence(Statement, Demand);
	UGraphWriteActionContextUtils::ApplyCreateStatementEvidence(Statement, Demand);
	UGraphWriteActionContextUtils::ApplyConvertScheduleEvidence(
		Statement.FunctionOperation,
		Statement.TransformOperation,
		Statement.ScheduleOperation,
		Statement.ClassPath,
		FirstNonEmpty(Statement.Target, Statement.Name),
		Statement.GraphLatentAllowed,
		Demand);
	UGraphWriteActionContextUtils::ApplyFunctionSemanticOperations(Demand);
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
	UGraphWriteActionContextUtils::ApplyEventDelegateStatementEvidence(Statement, Demand);

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
	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Field
		&& Expression.ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::Temporary)
	{
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
		return;
	}

	const EBlueprintHelperActionSemanticKind SemanticKind = ToActionSemanticKind(Expression.Kind);
	const FString FieldOperation = Expression.Kind == EBlueprintHelperGraphExpressionKind::Field
		? Expression.FieldOperation
		: FString();
	const FString FieldScope = Expression.Kind == EBlueprintHelperGraphExpressionKind::Field
		? Expression.FieldScope
		: FString();
	const FString StableId = Expression.ExpressionId.IsEmpty()
		? UGraphWriteActionContextUtils::DemandIdFromPath(TEXT("expression"), Expression.Path)
		: Expression.ExpressionId;

	FBlueprintHelperActionContextDemand Demand = BuildDemand(
		StableId,
		Expression.Path,
		SemanticKind,
		UGraphWriteActionContextUtils::BuildExpressionQuery(Expression, SemanticKind),
		FirstNonEmpty(
			Expression.ResolvedTarget.Raw,
			Expression.Target,
			Expression.Name),
		FirstNonEmpty(
			Expression.Property,
			Expression.ResolvedTarget.PropertyPath),
		FirstNonEmpty(
			Expression.Type,
			Expression.ResolvedTarget.Type),
		Expression.SearchMode,
		Expression.AmbiguityPolicy,
		Expression.CategoryPriority,
		UGraphWriteActionContextUtils::SortedArgumentNames(Expression.Args),
		FieldOperation,
		FieldScope);
	UGraphWriteActionContextUtils::CopyExpressionMapContext(Expression.Args, Demand);
	UGraphWriteActionContextUtils::CopyNamedExpressionContext(TEXT("left"), Expression.Left, Demand);
	UGraphWriteActionContextUtils::CopyNamedExpressionContext(TEXT("right"), Expression.Right, Demand);
	UGraphWriteActionContextUtils::CopyNamedExpressionContext(TEXT("value"), Expression.Value, Demand);
	UGraphWriteActionContextUtils::CopyNamedExpressionContext(TEXT("condition"), Expression.Condition, Demand);
	UGraphWriteActionContextUtils::CopyFocusedContextEvidence(Expression.ContextEvidence, Demand);
	UGraphWriteActionContextUtils::ApplyExpressionCapabilityFacts(Expression, Demand);
	UGraphWriteActionContextUtils::ApplyContainerActionExpressionEvidence(Expression, Demand);
	UGraphWriteActionContextUtils::ApplyCreateExpressionEvidence(Expression, Demand);
	UGraphWriteActionContextUtils::ApplyConvertScheduleEvidence(
		Expression.FunctionOperation,
		Expression.TransformOperation,
		Expression.ScheduleOperation,
		Expression.ClassPath,
		FirstNonEmpty(Expression.Target, Expression.Name),
		Expression.GraphLatentAllowed,
		Demand);
	Demand.ExpectedReturnType = Expression.Type;
	UGraphWriteActionContextUtils::ApplyOpEvidence(
		Expression.FunctionOperation,
		Expression.Operator,
		Expression.ContextEvidence,
		Expression.Left,
		Expression.Right,
		Demand);
	UGraphWriteActionContextUtils::ApplyFunctionSemanticOperations(Demand);
	if (Expression.TargetObject.IsValid())
	{
		Demand.TargetObjectType = Expression.TargetObject->Type;
		Demand.BindingObjectPath = !Expression.TargetObject->ResolvedTarget.Raw.IsEmpty()
			? Expression.TargetObject->ResolvedTarget.Raw
			: Expression.TargetObject->Target;
	}
	UGraphWriteActionContextUtils::ApplyEventDelegateExpressionEvidence(Expression, Demand);

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
	Demand.SemanticFamily = UGraphWriteActionContextUtils::ResolveSemanticFamily(SemanticKind, FieldScope);
	Demand.TypeOperation = UGraphWriteActionContextUtils::ResolveTypeOperation(SemanticKind);
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
	UGraphWriteActionContextUtils::ApplyFunctionSemanticOperations(Demand);
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
		UGraphWriteActionContextUtils::AddFieldDemandFacts(Demand);
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
	case EBlueprintHelperActionSemanticKind::ContainerAction:
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
	case EBlueprintHelperGraphStatementKind::Control:
		return EBlueprintHelperActionSemanticKind::Control;
	case EBlueprintHelperGraphStatementKind::Create:
		return EBlueprintHelperActionSemanticKind::Create;
	case EBlueprintHelperGraphStatementKind::Convert:
		return EBlueprintHelperActionSemanticKind::Convert;
	case EBlueprintHelperGraphStatementKind::Schedule:
		return EBlueprintHelperActionSemanticKind::Schedule;
	case EBlueprintHelperGraphStatementKind::ContainerAction:
		return EBlueprintHelperActionSemanticKind::ContainerAction;
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
	case EBlueprintHelperGraphExpressionKind::ContainerAction:
		return EBlueprintHelperActionSemanticKind::ContainerAction;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}
