// BlueprintHelper GraphStatement BlueprintHelperGraphFragmentDagBuilderUtils implementation.

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"

FString FBlueprintHelperGraphFragmentDagBuilderUtils::BoolText(const bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}
FString FBlueprintHelperGraphFragmentDagBuilderUtils::NormalizeSymbolKey(const FString& Name)
{
	return Name.TrimStartAndEnd().ToLower();
}
FString FBlueprintHelperGraphFragmentDagBuilderUtils::SanitizeIdPart(const FString& Value)
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
FString FBlueprintHelperGraphFragmentDagBuilderUtils::StatementKindName(const EBlueprintHelperGraphStatementKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		return TEXT("call");
	case EBlueprintHelperGraphStatementKind::Field:
		return TEXT("field");
	case EBlueprintHelperGraphStatementKind::Branch:
		return TEXT("branch");
	case EBlueprintHelperGraphStatementKind::Sequence:
		return TEXT("sequence");
	case EBlueprintHelperGraphStatementKind::Let:
		return TEXT("let");
	case EBlueprintHelperGraphStatementKind::Return:
		return TEXT("return");
	case EBlueprintHelperGraphStatementKind::Create:
		return TEXT("create");
	case EBlueprintHelperGraphStatementKind::Convert:
		return TEXT("convert");
	case EBlueprintHelperGraphStatementKind::Schedule:
		return TEXT("schedule");
	case EBlueprintHelperGraphStatementKind::ContainerAction:
		return TEXT("container_action");
	case EBlueprintHelperGraphStatementKind::ComponentBoundEvent:
		return TEXT("component_bound_event");
	case EBlueprintHelperGraphStatementKind::Delegate:
		return TEXT("delegate");
	default:
		return TEXT("unknown");
	}
}
FString FBlueprintHelperGraphFragmentDagBuilderUtils::ExpressionKindName(const EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Literal:
		return TEXT("literal");
	case EBlueprintHelperGraphExpressionKind::Field:
		return TEXT("field");
	case EBlueprintHelperGraphExpressionKind::Call:
		return TEXT("call");
	case EBlueprintHelperGraphExpressionKind::Op:
		return TEXT("op");
	case EBlueprintHelperGraphExpressionKind::Construct:
		return TEXT("construct");
	case EBlueprintHelperGraphExpressionKind::Deconstruct:
		return TEXT("deconstruct");
	case EBlueprintHelperGraphExpressionKind::Select:
		return TEXT("select");
	case EBlueprintHelperGraphExpressionKind::Create:
		return TEXT("create");
	case EBlueprintHelperGraphExpressionKind::Convert:
		return TEXT("convert");
	case EBlueprintHelperGraphExpressionKind::Schedule:
		return TEXT("schedule");
	case EBlueprintHelperGraphExpressionKind::ContainerAction:
		return TEXT("container_action");
	default:
		return TEXT("unknown");
	}
}
FString FBlueprintHelperGraphFragmentDagBuilderUtils::TargetKindName(const EBlueprintHelperGraphTargetKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphTargetKind::Function:
		return TEXT("function");
	case EBlueprintHelperGraphTargetKind::Component:
		return TEXT("component");
	case EBlueprintHelperGraphTargetKind::ComponentMemberFunction:
		return TEXT("component_member_function");
	case EBlueprintHelperGraphTargetKind::Variable:
		return TEXT("variable");
	case EBlueprintHelperGraphTargetKind::PropertyPath:
		return TEXT("property_path");
	case EBlueprintHelperGraphTargetKind::Temporary:
		return TEXT("temporary");
	default:
		return TEXT("unknown");
	}
}
EBlueprintHelperGraphFragmentDiagnosticSeverity FBlueprintHelperGraphFragmentDagBuilderUtils::ConvertSeverity(const FString& Severity)
{
	if (Severity.Equals(TEXT("info"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperGraphFragmentDiagnosticSeverity::Info;
	}
	if (Severity.Equals(TEXT("warning"), ESearchCase::IgnoreCase)
		|| Severity.Equals(TEXT("warn"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning;
	}
	return EBlueprintHelperGraphFragmentDiagnosticSeverity::Error;
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::AddMetadata(FBlueprintHelperGraphFragmentRef& Fragment, const FString& Key, const FString& Value)
{
	if (!Key.IsEmpty() && !Value.IsEmpty())
	{
		Fragment.Metadata.Add(Key, Value);
	}
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::AddResolvedTargetMetadata(
	FBlueprintHelperGraphFragmentRef& Fragment,
	const FBlueprintHelperGraphResolvedTarget& Target)
{
	AddMetadata(Fragment, TEXT("target.raw"), Target.Raw);
	AddMetadata(Fragment, TEXT("target.kind"), TargetKindName(Target.Kind));
	AddMetadata(Fragment, TEXT("target.owner"), Target.Owner);
	AddMetadata(Fragment, TEXT("target.member"), Target.Member);
	AddMetadata(Fragment, TEXT("target.property_path"), Target.PropertyPath);
	AddMetadata(Fragment, TEXT("target.type"), Target.Type);
	AddMetadata(Fragment, TEXT("target.verified"), BoolText(Target.bVerifiedByContext));
}
static FString FieldPathFull(const FString& Target, const FString& Property, const FString& FieldScope)
{
	const FString CleanTarget = Target.TrimStartAndEnd();
	const FString CleanProperty = Property.TrimStartAndEnd();
	if (FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase)
		&& !CleanTarget.IsEmpty()
		&& !CleanProperty.IsEmpty()
		&& !CleanTarget.Contains(TEXT(".")))
	{
		return CleanTarget + TEXT(".") + CleanProperty;
	}
	if (!CleanProperty.IsEmpty() && CleanTarget.IsEmpty())
	{
		return CleanProperty;
	}
	return !CleanTarget.IsEmpty() ? CleanTarget : CleanProperty;
}
static FString FieldPathRoot(const FString& FullPath)
{
	TArray<FString> Segments;
	FullPath.ParseIntoArray(Segments, TEXT("."), true);
	return Segments.Num() > 0 ? Segments[0].TrimStartAndEnd() : FullPath.TrimStartAndEnd();
}
static FString FieldPathLeaf(const FString& FullPath)
{
	TArray<FString> Segments;
	FullPath.ParseIntoArray(Segments, TEXT("."), true);
	return Segments.Num() > 0 ? Segments.Last().TrimStartAndEnd() : FullPath.TrimStartAndEnd();
}
static void AddFieldPathMetadata(
	FBlueprintHelperGraphFragmentRef& Fragment,
	const FString& Target,
	const FString& Property,
	const FString& FieldScope)
{
	const FString Role = FieldScope.TrimStartAndEnd();
	const FString FullPath = FieldPathFull(Target, Property, Role);
	FBlueprintHelperGraphFragmentDagBuilderUtils::AddMetadata(Fragment, TEXT("field.path.full"), FullPath);
	FBlueprintHelperGraphFragmentDagBuilderUtils::AddMetadata(Fragment, TEXT("field.path.root"), FieldPathRoot(FullPath));
	FBlueprintHelperGraphFragmentDagBuilderUtils::AddMetadata(Fragment, TEXT("field.path.leaf"), FieldPathLeaf(FullPath));
	FBlueprintHelperGraphFragmentDagBuilderUtils::AddMetadata(Fragment, TEXT("field.path.role"), Role);
}
FString FBlueprintHelperGraphFragmentDagBuilderUtils::MakeUniqueFragmentId(FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State, const FString& PreferredId)
{
	FString Base = SanitizeIdPart(PreferredId);
	if (Base.Equals(TEXT("unnamed"), ESearchCase::CaseSensitive))
	{
		Base = FString::Printf(TEXT("fragment_%04d"), ++State.FragmentSerial);
	}

	FString Candidate = Base;
	int32 Suffix = 2;
	while (State.FragmentIds.Contains(Candidate))
	{
		Candidate = FString::Printf(TEXT("%s_%d"), *Base, Suffix++);
	}
	State.FragmentIds.Add(Candidate);
	return Candidate;
}
FBlueprintHelperGraphFragmentRef& FBlueprintHelperGraphFragmentDagBuilderUtils::AddFragment(
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	const FString& PreferredId,
	const FString& SourceStatementId,
	const FString& Path,
	const FString& Kind)
{
	FBlueprintHelperGraphFragmentRef Fragment;
	Fragment.FragmentId = MakeUniqueFragmentId(State, PreferredId);
	Fragment.SourceStatementId = SourceStatementId;
	Fragment.Path = Path;
	Fragment.Kind = Kind;

	const int32 FragmentIndex = State.Dag->Fragments.Add(MoveTemp(Fragment));
	return State.Dag->Fragments[FragmentIndex];
}
FBlueprintHelperGraphFragmentEndpointRef FBlueprintHelperGraphFragmentDagBuilderUtils::MakeEndpoint(
	const FString& FragmentId,
	const FString& PortId,
	const FString& PinName,
	const FString& Type,
	const EBlueprintHelperGraphFragmentPortDirection Direction)
{
	FBlueprintHelperGraphFragmentEndpointRef Endpoint;
	Endpoint.FragmentId = FragmentId;
	Endpoint.PortId = PortId;
	Endpoint.PinName = PinName;
	Endpoint.Type = Type;
	Endpoint.Direction = Direction;
	Endpoint.PinType.Category = Type;
	return Endpoint;
}
FBlueprintHelperGraphFragmentEndpointRef FBlueprintHelperGraphFragmentDagBuilderUtils::MakeExecEndpoint(
	const FString& FragmentId,
	const FString& PortId,
	const FString& PinName,
	const EBlueprintHelperGraphFragmentPortDirection Direction)
{
	return MakeEndpoint(FragmentId, PortId, PinName, TEXT("exec"), Direction);
}
FBlueprintHelperGraphFragmentEndpointRef FBlueprintHelperGraphFragmentDagBuilderUtils::MakeExecEntry(const FString& FragmentId)
{
	return MakeExecEndpoint(FragmentId, TEXT("exec.in"), TEXT("execute"), EBlueprintHelperGraphFragmentPortDirection::ExecInput);
}
FBlueprintHelperGraphFragmentEndpointRef FBlueprintHelperGraphFragmentDagBuilderUtils::MakeExecExit(const FString& FragmentId)
{
	return MakeExecEndpoint(FragmentId, TEXT("exec.out"), TEXT("then"), EBlueprintHelperGraphFragmentPortDirection::ExecOutput);
}
FBlueprintHelperGraphFragmentEndpointRef FBlueprintHelperGraphFragmentDagBuilderUtils::MakeDataInput(
	const FString& FragmentId,
	const FString& Name,
	const FString& Type)
{
	return MakeEndpoint(
		FragmentId,
		TEXT("data.in.") + SanitizeIdPart(Name),
		Name,
		Type,
		EBlueprintHelperGraphFragmentPortDirection::DataInput);
}
FBlueprintHelperGraphFragmentEndpointRef FBlueprintHelperGraphFragmentDagBuilderUtils::MakeDataOutput(
	const FString& FragmentId,
	const FString& Name,
	const FString& Type)
{
	return MakeEndpoint(
		FragmentId,
		TEXT("data.out.") + SanitizeIdPart(Name),
		Name,
		Type,
		EBlueprintHelperGraphFragmentPortDirection::DataOutput);
}

static FString ResolveContainerActionResultContainerType(
	const FString& ContainerKind,
	const FString& ContainerOperation)
{
	const FString Kind = ContainerKind.TrimStartAndEnd().ToLower();
	const FString Operation = ContainerOperation.TrimStartAndEnd().ToLower();
	if ((Kind == TEXT("map") && (Operation == TEXT("keys") || Operation == TEXT("values")))
		|| (Kind == TEXT("set") && Operation == TEXT("to_array")))
	{
		return TEXT("array");
	}
	return FString();
}

static void ApplyContainerActionResultEndpointType(
	const FString& ContainerKind,
	const FString& ContainerOperation,
	const FString& ElementType,
	const FString& KeyType,
	const FString& ValueType,
	const FString& PinType,
	const FString& KeyPinType,
	const FString& ValuePinType,
	FBlueprintHelperGraphFragmentEndpointRef& InOutEndpoint)
{
	const FString ResultType = FBlueprintHelperGraphStatementTypeUtils::ResolveContainerActionResultTypeToken(
		ContainerKind,
		ContainerOperation,
		ElementType,
		KeyType,
		ValueType,
		PinType,
		KeyPinType,
		ValuePinType);
	if (!ResultType.IsEmpty())
	{
		InOutEndpoint.Type = ResultType;
		InOutEndpoint.PinType.Category = ResultType;
	}

	const FString ContainerType = ResolveContainerActionResultContainerType(ContainerKind, ContainerOperation);
	if (!ContainerType.IsEmpty())
	{
		InOutEndpoint.PinType.ContainerType = ContainerType;
	}
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::AddBuilderDiagnostic(
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	const FString& Code,
	const FString& Path,
	const FString& Message,
	const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity)
{
	State.Dag->AddDiagnostic(Code, Path, Message, Severity);
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::AddExecEdge(
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	const FBlueprintHelperGraphFragmentEndpointRef& From,
	const FBlueprintHelperGraphFragmentEndpointRef& To,
	const FString& Path)
{
	if (!From.IsValid() || !To.IsValid())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("exec_edge_invalid"),
			Path,
			TEXT("Skipped exec edge because one endpoint is invalid."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
		return;
	}

	FBlueprintHelperGraphFragmentExecEdge Edge;
	Edge.EdgeId = FString::Printf(TEXT("exec_%04d"), ++State.ExecEdgeSerial);
	Edge.From = From;
	Edge.To = To;
	Edge.Path = Path;
	State.Dag->ExecEdges.Add(MoveTemp(Edge));
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::AddDataEdge(
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer& Producer,
	const FBlueprintHelperGraphFragmentEndpointRef& To,
	const FString& Path)
{
	if (!Producer.IsValid() || !To.IsValid())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("data_edge_invalid"),
			Path,
			TEXT("Skipped data edge because one endpoint is invalid."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
		return;
	}

	FBlueprintHelperGraphFragmentDataEdge Edge;
	Edge.EdgeId = FString::Printf(TEXT("data_%04d"), ++State.DataEdgeSerial);
	Edge.From = Producer.Endpoint;
	Edge.To = To;
	Edge.SymbolId = Producer.SymbolId;
	Edge.Path = Path;
	State.Dag->DataEdges.Add(MoveTemp(Edge));
}
FString FBlueprintHelperGraphFragmentDagBuilderUtils::MakeStatementFragmentId(
	const FBlueprintHelperGraphStatementIR& Statement,
	const FString& Suffix)
{
	const FString SourceId = !Statement.StatementId.IsEmpty() ? Statement.StatementId : Statement.Path;
	if (!SourceId.Contains(TEXT("$")) && !SourceId.Contains(TEXT(".")) && !SourceId.Contains(TEXT("[")) && !SourceId.Contains(TEXT("]")))
	{
		return SourceId;
	}
	return TEXT("stmt_") + StatementKindName(Statement.Kind) + TEXT("_") + SourceId + TEXT("_") + Suffix;
}
FString FBlueprintHelperGraphFragmentDagBuilderUtils::MakeExpressionFragmentId(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& /*Suffix*/)
{
	return FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
}
bool FBlueprintHelperGraphFragmentDagBuilderUtils::FindSymbolProducer(
	const FString& Name,
	const TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer& OutProducer)
{
	const FString Key = NormalizeSymbolKey(Name);
	for (int32 ScopeIndex = SymbolScopes.Num() - 1; ScopeIndex >= 0; --ScopeIndex)
	{
		if (const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer* Producer = SymbolScopes[ScopeIndex].Find(Key))
		{
			OutProducer = *Producer;
			return true;
		}
	}
	return false;
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::RegisterSymbolProducer(
	const FString& Name,
	const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer& Producer,
	const FString& Path,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	const FString Key = NormalizeSymbolKey(Name);
	if (Key.IsEmpty())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("symbol_name_missing"),
			Path,
			TEXT("Skipped symbol registration because the symbol name is empty."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		return;
	}

	if (SymbolScopes.Num() == 0)
	{
		SymbolScopes.AddDefaulted();
	}

	TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>& CurrentScope = SymbolScopes.Last();
	if (CurrentScope.Contains(Key))
	{
		AddBuilderDiagnostic(
			State,
			TEXT("symbol_duplicate_in_scope"),
			Path,
			FString::Printf(TEXT("Replacing duplicate symbol in current DAG scope: %s."), *Name),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
	}

	CurrentScope.Add(Key, Producer);
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::ConnectExpressionToInput(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& FallbackPath,
	const FString& InputName,
	const FString& InputType,
	const FString& ConsumerFragmentId,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer Producer = BuildExpression(Expression, FallbackPath, State, SymbolScopes);
	if (!Producer.IsValid())
	{
		return;
	}

	const FBlueprintHelperGraphFragmentEndpointRef InputEndpoint = MakeDataInput(
		ConsumerFragmentId,
		InputName,
		!InputType.IsEmpty() ? InputType : Producer.Type);
	AddDataEdge(State, Producer, InputEndpoint, FallbackPath);
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::ConnectExpressionMapToInputs(
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
	const FString& FallbackPath,
	const FString& ConsumerFragmentId,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	TArray<FString> Keys;
	Args.GetKeys(Keys);
	Keys.Sort();

	for (const FString& Key : Keys)
	{
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>* Expression = Args.Find(Key);
		if (!Expression)
		{
			continue;
		}

		const FString ArgPath = (*Expression).IsValid() ? (*Expression)->Path : FallbackPath + TEXT(".") + Key;
		ConnectExpressionToInput(*Expression, ArgPath, Key, FString(), ConsumerFragmentId, State, SymbolScopes);
	}
}
FBlueprintHelperGraphFragmentRef& FBlueprintHelperGraphFragmentDagBuilderUtils::AddExpressionFragment(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& Kind,
	const FString& Suffix,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State)
{
	FBlueprintHelperGraphFragmentRef& Fragment = AddFragment(
		State,
		MakeExpressionFragmentId(Expression, Suffix),
		Expression.ExpressionId,
		Expression.Path,
		Kind);

	AddMetadata(Fragment, TEXT("expression_id"), Expression.ExpressionId);
	AddMetadata(Fragment, TEXT("expression_kind"), ExpressionKindName(Expression.Kind));
	AddMetadata(Fragment, TEXT("pattern"), Expression.PatternName);
	AddMetadata(Fragment, TEXT("target"), Expression.Target);
	AddMetadata(Fragment, TEXT("name"), Expression.Name);
	AddMetadata(Fragment, TEXT("property"), Expression.Property);
	AddMetadata(Fragment, TEXT("field_operation"), Expression.FieldOperation);
	AddMetadata(Fragment, TEXT("field_scope"), Expression.FieldScope);
	AddFieldPathMetadata(Fragment, Expression.Target, Expression.Property, Expression.FieldScope);
	AddMetadata(Fragment, TEXT("container_kind"), Expression.ContainerKind);
	AddMetadata(Fragment, TEXT("container_operation"), Expression.ContainerOperation);
	AddMetadata(Fragment, TEXT("element_type"), Expression.ElementType);
	AddMetadata(Fragment, TEXT("key_type"), Expression.KeyType);
	AddMetadata(Fragment, TEXT("value_type"), Expression.ValueType);
	AddMetadata(Fragment, TEXT("type"), Expression.Type);
	AddMetadata(Fragment, TEXT("operator"), Expression.Operator);
	AddMetadata(Fragment, TEXT("literal"), Expression.LiteralValue);
	AddMetadata(Fragment, TEXT("search_mode"), Expression.SearchMode);
	AddMetadata(Fragment, TEXT("ambiguity"), Expression.AmbiguityPolicy);
	if (Expression.CategoryPriority.Num() > 0)
	{
		AddMetadata(Fragment, TEXT("category_priority"), FString::Join(Expression.CategoryPriority, TEXT("|")));
	}
	AddResolvedTargetMetadata(Fragment, Expression.ResolvedTarget);
	Fragment.Layout.Kind = EBlueprintHelperGraphFragmentLayoutKind::Expression;
	Fragment.Layout.Hints.Add(TEXT("source_path"), Expression.Path);
	return Fragment;
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer FBlueprintHelperGraphFragmentDagBuilderUtils::MakeExpressionProducer(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FBlueprintHelperGraphFragmentRef& Fragment,
	const FString& OutputName,
	const FString& FallbackType)
{
	return MakeExpressionProducerFromId(Expression, Fragment.FragmentId, OutputName, FallbackType);
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer FBlueprintHelperGraphFragmentDagBuilderUtils::MakeExpressionProducerFromId(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& FragmentId,
	const FString& OutputName,
	const FString& FallbackType)
{
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer Producer;
	Producer.Endpoint = MakeDataOutput(
		FragmentId,
		OutputName,
		!Expression.Type.IsEmpty() ? Expression.Type : FallbackType);
	Producer.SymbolId = TEXT("expr:") + FragmentId + TEXT(":") + SanitizeIdPart(OutputName);
	Producer.Type = Producer.Endpoint.Type;
	Producer.Path = Expression.Path;
	return Producer;
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer FBlueprintHelperGraphFragmentDagBuilderUtils::BuildPlaceholderExpression(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& Kind,
	const FString& Suffix,
	const FString& OutputName,
	const FString& OutputType,
	const FString& DiagnosticCode,
	const FString& DiagnosticMessage,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes,
	const EBlueprintHelperGraphFragmentDiagnosticSeverity DiagnosticSeverity)
{
	FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, Kind, Suffix, State);
	const FString FragmentId = Fragment.FragmentId;
	if (!DiagnosticCode.IsEmpty())
	{
		AddMetadata(Fragment, TEXT("placeholder"), TEXT("true"));
	}

	if (!DiagnosticCode.IsEmpty())
	{
		AddBuilderDiagnostic(
			State,
			DiagnosticCode,
			Expression->Path,
			DiagnosticMessage,
			DiagnosticSeverity);
	}

	if (Expression->Left.IsValid())
	{
		ConnectExpressionToInput(
			Expression->Left,
			Expression->Left->Path,
			TEXT("left"),
			Expression->Left->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Expression->Right.IsValid())
	{
		ConnectExpressionToInput(
			Expression->Right,
			Expression->Right->Path,
			TEXT("right"),
			Expression->Right->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Expression->Value.IsValid())
	{
		ConnectExpressionToInput(
			Expression->Value,
			Expression->Value->Path,
			TEXT("value"),
			Expression->Value->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Expression->TargetObject.IsValid())
	{
		ConnectExpressionToInput(
			Expression->TargetObject,
			Expression->TargetObject->Path,
			TEXT("target"),
			Expression->TargetObject->Type,
			FragmentId,
			State,
			SymbolScopes);
	}

	ConnectExpressionMapToInputs(Expression->Args, Expression->Path + TEXT(".args"), FragmentId, State, SymbolScopes);
	ConnectExpressionMapToInputs(Expression->Fields, Expression->Path + TEXT(".fields"), FragmentId, State, SymbolScopes);

	for (int32 OptionIndex = 0; OptionIndex < Expression->Options.Num(); ++OptionIndex)
	{
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression->Options[OptionIndex];
		ConnectExpressionToInput(
			Option,
			Option.IsValid() ? Option->Path : FString::Printf(TEXT("%s.options[%d]"), *Expression->Path, OptionIndex),
			FString::Printf(TEXT("option_%d"), OptionIndex),
			Option.IsValid() ? Option->Type : FString(),
			FragmentId,
			State,
			SymbolScopes);
	}

	return MakeExpressionProducerFromId(*Expression, FragmentId, OutputName, OutputType);
}

FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer FBlueprintHelperGraphFragmentDagBuilderUtils::BuildResolvableExpressionFragment(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& Kind,
	const FString& Suffix,
	const FString& OutputName,
	const FString& OutputType,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, Kind, Suffix, State);
	const FString FragmentId = Fragment.FragmentId;

	if (Expression->Left.IsValid())
	{
		ConnectExpressionToInput(
			Expression->Left,
			Expression->Left->Path,
			TEXT("left"),
			Expression->Left->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Expression->Right.IsValid())
	{
		ConnectExpressionToInput(
			Expression->Right,
			Expression->Right->Path,
			TEXT("right"),
			Expression->Right->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Expression->Value.IsValid())
	{
		ConnectExpressionToInput(
			Expression->Value,
			Expression->Value->Path,
			TEXT("value"),
			Expression->Value->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Expression->TargetObject.IsValid())
	{
		ConnectExpressionToInput(
			Expression->TargetObject,
			Expression->TargetObject->Path,
			TEXT("target"),
			Expression->TargetObject->Type,
			FragmentId,
			State,
			SymbolScopes);
	}

	ConnectExpressionMapToInputs(Expression->Args, Expression->Path + TEXT(".args"), FragmentId, State, SymbolScopes);
	ConnectExpressionMapToInputs(Expression->Fields, Expression->Path + TEXT(".fields"), FragmentId, State, SymbolScopes);

	for (int32 OptionIndex = 0; OptionIndex < Expression->Options.Num(); ++OptionIndex)
	{
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression->Options[OptionIndex];
		ConnectExpressionToInput(
			Option,
			Option.IsValid() ? Option->Path : FString::Printf(TEXT("%s.options[%d]"), *Expression->Path, OptionIndex),
			FString::Printf(TEXT("option_%d"), OptionIndex),
			Option.IsValid() ? Option->Type : FString(),
			FragmentId,
			State,
			SymbolScopes);
	}

	return MakeExpressionProducerFromId(*Expression, FragmentId, OutputName, OutputType);
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer FBlueprintHelperGraphFragmentDagBuilderUtils::BuildExpression(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& FallbackPath,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	if (!Expression.IsValid())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("expression_missing"),
			FallbackPath,
			TEXT("Skipped missing expression while building fragment DAG."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
		return FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer();
	}

	switch (Expression->Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Literal:
	{
		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_literal"), TEXT("literal"), State);
		return MakeExpressionProducer(*Expression, Fragment, TEXT("value"), Expression->Type);
	}

	case EBlueprintHelperGraphExpressionKind::Field:
	{
		const bool bPropertyPathField = Expression->FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase);
		const bool bComponentRefField = Expression->FieldScope.Equals(TEXT("component_ref"), ESearchCase::IgnoreCase);
		const bool bFieldAccessField = Expression->FieldScope.Equals(TEXT("field_access"), ESearchCase::IgnoreCase);
		if (!bPropertyPathField && !bComponentRefField && !bFieldAccessField)
		{
			FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer Producer;
			const FString SymbolName = !Expression->Target.IsEmpty() ? Expression->Target : Expression->Name;
			if (FindSymbolProducer(SymbolName, SymbolScopes, Producer))
			{
				return Producer;
			}
		}
		if (Expression->ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::Temporary)
		{
			FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer Producer;
			const FString SymbolName = !Expression->Target.IsEmpty() ? Expression->Target : Expression->Name;
			if (FindSymbolProducer(SymbolName, SymbolScopes, Producer))
			{
				return Producer;
			}

			FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_get_unresolved"), TEXT("unresolved"), State);
			const FString FragmentId = Fragment.FragmentId;
			AddMetadata(Fragment, TEXT("placeholder"), TEXT("true"));
			AddBuilderDiagnostic(
				State,
				TEXT("get_symbol_unresolved"),
				Expression->Path,
				FString::Printf(TEXT("No symbol producer found for field get expression: %s."), *SymbolName),
				EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
			return MakeExpressionProducerFromId(*Expression, FragmentId, TEXT("value"), Expression->Type);
		}

		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(
			*Expression,
			bComponentRefField ? TEXT("expr_get_component_ref") : (bFieldAccessField ? TEXT("expr_get_field_access") : (bPropertyPathField ? TEXT("expr_get_property") : TEXT("expr_get"))),
			bComponentRefField ? TEXT("get_component_ref") : (bFieldAccessField ? TEXT("get_field_access") : (bPropertyPathField ? TEXT("get_property") : TEXT("get"))),
			State);
		if (Expression->TargetObject.IsValid())
		{
			ConnectExpressionToInput(
				Expression->TargetObject,
				Expression->TargetObject->Path,
				TEXT("target"),
				Expression->TargetObject->Type,
				Fragment.FragmentId,
				State,
				SymbolScopes);
		}
		return MakeExpressionProducer(*Expression, Fragment, TEXT("value"), Expression->Type);
	}

	case EBlueprintHelperGraphExpressionKind::Call:
		return BuildResolvableExpressionFragment(
			Expression,
			TEXT("function_action"),
			TEXT("call"),
			TEXT("return"),
			Expression->Type,
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Op:
		return BuildResolvableExpressionFragment(
			Expression,
			TEXT("function_action"),
			TEXT("op"),
			TEXT("result"),
			!Expression->Type.IsEmpty() ? Expression->Type : TEXT("bool"),
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Select:
	{
		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_select"), TEXT("select"), State);
		const FString FragmentId = Fragment.FragmentId;
		if (Expression->Condition.IsValid())
		{
			ConnectExpressionToInput(
				Expression->Condition,
				Expression->Condition->Path,
				TEXT("condition"),
				Expression->Condition->Type,
				FragmentId,
				State,
				SymbolScopes);
		}
		else if (const TSharedPtr<FBlueprintHelperGraphExpressionIR>* ConditionExpression = Expression->Args.Find(TEXT("condition")))
		{
			ConnectExpressionToInput(
				*ConditionExpression,
				(*ConditionExpression).IsValid() ? (*ConditionExpression)->Path : Expression->Path + TEXT(".condition"),
				TEXT("condition"),
				(*ConditionExpression).IsValid() ? (*ConditionExpression)->Type : FString(),
				FragmentId,
				State,
				SymbolScopes);
		}

		if (Expression->ThenValue.IsValid() || Expression->ElseValue.IsValid())
		{
			if (Expression->ElseValue.IsValid())
			{
				ConnectExpressionToInput(
					Expression->ElseValue,
					Expression->ElseValue->Path,
					TEXT("else"),
					Expression->ElseValue->Type,
					FragmentId,
					State,
					SymbolScopes);
			}
			if (Expression->ThenValue.IsValid())
			{
				ConnectExpressionToInput(
					Expression->ThenValue,
					Expression->ThenValue->Path,
					TEXT("then"),
					Expression->ThenValue->Type,
					FragmentId,
					State,
					SymbolScopes);
			}
		}
		else
		{
			for (int32 OptionIndex = 0; OptionIndex < Expression->Options.Num(); ++OptionIndex)
			{
				const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression->Options[OptionIndex];
				ConnectExpressionToInput(
					Option,
					Option.IsValid() ? Option->Path : FString::Printf(TEXT("%s.options[%d]"), *Expression->Path, OptionIndex),
					FString::Printf(TEXT("option_%d"), OptionIndex),
					Option.IsValid() ? Option->Type : FString(),
					FragmentId,
					State,
					SymbolScopes);
			}
		}

		return MakeExpressionProducerFromId(*Expression, FragmentId, TEXT("result"), Expression->Type);
	}

	case EBlueprintHelperGraphExpressionKind::Construct:
		return BuildResolvableExpressionFragment(
			Expression,
			TEXT("generic_action"),
			TEXT("construct"),
			TEXT("value"),
			Expression->Type,
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Deconstruct:
		return BuildResolvableExpressionFragment(
			Expression,
			TEXT("generic_action"),
			TEXT("deconstruct"),
			Expression->FieldNames.Num() > 0 ? Expression->FieldNames[0] : TEXT("value"),
			Expression->Type,
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Create:
		return BuildResolvableExpressionFragment(
			Expression,
			TEXT("generic_action"),
			TEXT("create"),
			TEXT("value"),
			Expression->Type,
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Convert:
		return BuildResolvableExpressionFragment(
			Expression,
			TEXT("generic_action"),
			TEXT("convert"),
			TEXT("value"),
			Expression->Type,
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Schedule:
		return BuildResolvableExpressionFragment(
			Expression,
			TEXT("generic_action"),
			TEXT("schedule"),
			TEXT("then"),
			Expression->Type,
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::ContainerAction:
	{
		FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer Producer = BuildResolvableExpressionFragment(
			Expression,
			TEXT("function_action"),
			TEXT("container_action"),
			TEXT("result"),
			Expression->Type,
			State,
			SymbolScopes);
		ApplyContainerActionResultEndpointType(
			Expression->ContainerKind,
			Expression->ContainerOperation,
			Expression->ElementType,
			Expression->KeyType,
			Expression->ValueType,
			Expression->PinType,
			Expression->KeyPinType,
			Expression->ValuePinType,
			Producer.Endpoint);
		Producer.Type = Producer.Endpoint.Type;
		return Producer;
	}

	case EBlueprintHelperGraphExpressionKind::Unknown:
	default:
	{
		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_unknown"), TEXT("unknown"), State);
		const FString FragmentId = Fragment.FragmentId;
		AddMetadata(Fragment, TEXT("placeholder"), TEXT("true"));
		AddBuilderDiagnostic(
			State,
			TEXT("expression_kind_unknown"),
			Expression->Path,
			TEXT("Unknown expression kind was represented as a placeholder fragment."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		return MakeExpressionProducerFromId(*Expression, FragmentId, TEXT("value"), Expression->Type);
	}
	}
}
FBlueprintHelperGraphFragmentRef& FBlueprintHelperGraphFragmentDagBuilderUtils::AddStatementFragment(
	const FBlueprintHelperGraphStatementIR& Statement,
	const FString& Kind,
	const FString& Suffix,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State)
{
	FBlueprintHelperGraphFragmentRef& Fragment = AddFragment(
		State,
		MakeStatementFragmentId(Statement, Suffix),
		Statement.StatementId,
		Statement.Path,
		Kind);

	AddMetadata(Fragment, TEXT("statement_id"), Statement.StatementId);
	AddMetadata(Fragment, TEXT("statement_kind"), StatementKindName(Statement.Kind));
	AddMetadata(Fragment, TEXT("pattern"), Statement.PatternName);
	AddMetadata(Fragment, TEXT("target"), Statement.Target);
	AddMetadata(Fragment, TEXT("name"), Statement.Name);
	AddMetadata(Fragment, TEXT("property"), Statement.Property);
	AddMetadata(Fragment, TEXT("field_operation"), Statement.FieldOperation);
	AddMetadata(Fragment, TEXT("field_scope"), Statement.FieldScope);
	AddFieldPathMetadata(Fragment, Statement.Target, Statement.Property, Statement.FieldScope);
	AddMetadata(Fragment, TEXT("container_kind"), Statement.ContainerKind);
	AddMetadata(Fragment, TEXT("container_operation"), Statement.ContainerOperation);
	AddMetadata(Fragment, TEXT("element_type"), Statement.ElementType);
	AddMetadata(Fragment, TEXT("key_type"), Statement.KeyType);
	AddMetadata(Fragment, TEXT("value_type"), Statement.ValueType);
	AddMetadata(Fragment, TEXT("result_symbol"), Statement.ResultSymbolName);
	AddMetadata(Fragment, TEXT("search_mode"), Statement.SearchMode);
	AddMetadata(Fragment, TEXT("ambiguity"), Statement.AmbiguityPolicy);
	if (Statement.CategoryPriority.Num() > 0)
	{
		AddMetadata(Fragment, TEXT("category_priority"), FString::Join(Statement.CategoryPriority, TEXT("|")));
	}
	AddResolvedTargetMetadata(Fragment, Statement.ResolvedTarget);
	Fragment.Layout.Kind = EBlueprintHelperGraphFragmentLayoutKind::Statement;
	Fragment.Layout.Hints.Add(TEXT("source_path"), Statement.Path);
	return Fragment;
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow FBlueprintHelperGraphFragmentDagBuilderUtils::BuildSimpleStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	const FString& Kind,
	const FString& Suffix,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow Flow;
	FBlueprintHelperGraphFragmentRef& Fragment = AddStatementFragment(*Statement, Kind, Suffix, State);
	const FString FragmentId = Fragment.FragmentId;
	Flow.Entries.Add(MakeExecEntry(FragmentId));
	Flow.Exits.Add(MakeExecExit(FragmentId));

	ConnectExpressionMapToInputs(Statement->Args, Statement->Path + TEXT(".args"), FragmentId, State, SymbolScopes);
	if (Statement->TargetObject.IsValid())
	{
		ConnectExpressionToInput(
			Statement->TargetObject,
			Statement->TargetObject->Path,
			TEXT("target"),
			Statement->TargetObject->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Statement->Value.IsValid())
	{
		ConnectExpressionToInput(
			Statement->Value,
			Statement->Value->Path,
			TEXT("value"),
			Statement->Value->Type,
			FragmentId,
			State,
			SymbolScopes);
	}
	if (Statement->Condition.IsValid())
	{
		ConnectExpressionToInput(
			Statement->Condition,
			Statement->Condition->Path,
			TEXT("condition"),
			Statement->Condition->Type,
			FragmentId,
			State,
			SymbolScopes);
	}

	if (!Statement->ResultSymbolName.TrimStartAndEnd().IsEmpty())
	{
		const FString ResultType = FBlueprintHelperGraphStatementTypeUtils::ResolveContainerActionResultTypeToken(
			Statement->ContainerKind,
			Statement->ContainerOperation,
			Statement->ElementType,
			Statement->KeyType,
			Statement->ValueType,
			Statement->PinType,
			Statement->KeyPinType,
			Statement->ValuePinType);
		FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer ResultProducer;
		ResultProducer.Endpoint = MakeDataOutput(
			FragmentId,
			TEXT("result"),
			!ResultType.IsEmpty()
				? ResultType
				: (!Statement->ValueType.IsEmpty()
				? Statement->ValueType
				: (!Statement->ElementType.IsEmpty() ? Statement->ElementType : Statement->PinType)));
		ApplyContainerActionResultEndpointType(
			Statement->ContainerKind,
			Statement->ContainerOperation,
			Statement->ElementType,
			Statement->KeyType,
			Statement->ValueType,
			Statement->PinType,
			Statement->KeyPinType,
			Statement->ValuePinType,
			ResultProducer.Endpoint);
		ResultProducer.SymbolId = NormalizeSymbolKey(Statement->ResultSymbolName);
		ResultProducer.Type = ResultProducer.Endpoint.Type;
		ResultProducer.Path = Statement->Path;
		RegisterSymbolProducer(Statement->ResultSymbolName, ResultProducer, Statement->Path, State, SymbolScopes);
	}

	return Flow;
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow FBlueprintHelperGraphFragmentDagBuilderUtils::BuildLetStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow Flow;
	FBlueprintHelperGraphFragmentRef& Fragment = AddStatementFragment(*Statement, TEXT("statement_let"), TEXT("let"), State);
	const FString FragmentId = Fragment.FragmentId;
	AddMetadata(Fragment, TEXT("defines_symbol"), Statement->Name);
	AddMetadata(Fragment, TEXT("symbol_id"), NormalizeSymbolKey(Statement->Name));

	Flow.Entries.Add(MakeExecEntry(FragmentId));
	Flow.Exits.Add(MakeExecExit(FragmentId));

	FString SymbolType;
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer ValueProducer;
	bool bHasValueProducer = false;
	if (Statement->Value.IsValid())
	{
		SymbolType = Statement->Value->Type;
		ValueProducer = BuildExpression(Statement->Value, Statement->Value->Path, State, SymbolScopes);
		bHasValueProducer = ValueProducer.Endpoint.IsValid();
		if (bHasValueProducer)
		{
			AddDataEdge(State, ValueProducer, MakeDataInput(FragmentId, TEXT("value"), Statement->Value->Type), Statement->Value->Path);
		}
	}
	else
	{
		AddBuilderDiagnostic(
			State,
			TEXT("let_value_missing"),
			Statement->Path,
			TEXT("Let statement has no value expression; symbol producer will still be registered structurally."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
	}

	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer SymbolProducer;
	SymbolProducer = bHasValueProducer ? ValueProducer : FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer();
	if (!bHasValueProducer)
	{
		SymbolProducer.Endpoint = MakeDataOutput(FragmentId, TEXT("value"), SymbolType);
	}
	SymbolProducer.SymbolId = NormalizeSymbolKey(Statement->Name);
	SymbolProducer.Type = SymbolType;
	SymbolProducer.Path = Statement->Path;
	RegisterSymbolProducer(Statement->Name, SymbolProducer, Statement->Path, State, SymbolScopes);

	return Flow;
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow FBlueprintHelperGraphFragmentDagBuilderUtils::BuildBranchStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow Flow;
	FBlueprintHelperGraphFragmentRef& BranchFragment = AddStatementFragment(*Statement, TEXT("statement_branch"), TEXT("branch"), State);
	const FString BranchFragmentId = BranchFragment.FragmentId;
	Flow.Entries.Add(MakeExecEntry(BranchFragmentId));

	if (Statement->Condition.IsValid())
	{
		ConnectExpressionToInput(
			Statement->Condition,
			Statement->Condition->Path,
			TEXT("condition"),
			TEXT("bool"),
			BranchFragmentId,
			State,
			SymbolScopes);
	}
	else
	{
		AddBuilderDiagnostic(
			State,
			TEXT("branch_condition_missing"),
			Statement->Path,
			TEXT("Branch statement has no condition expression."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
	}

	FBlueprintHelperGraphFragmentRef& JoinFragment = AddFragment(
		State,
		MakeStatementFragmentId(*Statement, TEXT("join")),
		Statement->StatementId,
		Statement->Path + TEXT(".join"),
		TEXT("join"));
	const FString JoinFragmentId = JoinFragment.FragmentId;
	AddMetadata(JoinFragment, TEXT("source_branch_id"), Statement->StatementId);
	AddMetadata(JoinFragment, TEXT("auto_join"), TEXT("true"));
	JoinFragment.Layout.Kind = EBlueprintHelperGraphFragmentLayoutKind::Join;
	JoinFragment.Layout.Hints.Add(TEXT("source_path"), Statement->Path + TEXT(".join"));

	const FBlueprintHelperGraphFragmentEndpointRef BranchThen = MakeExecEndpoint(BranchFragmentId, TEXT("exec.then"), TEXT("then"), EBlueprintHelperGraphFragmentPortDirection::ExecOutput);
	const FBlueprintHelperGraphFragmentEndpointRef BranchElse = MakeExecEndpoint(BranchFragmentId, TEXT("exec.else"), TEXT("else"), EBlueprintHelperGraphFragmentPortDirection::ExecOutput);
	const FBlueprintHelperGraphFragmentEndpointRef JoinThen = MakeExecEndpoint(JoinFragmentId, TEXT("exec.in.then"), TEXT("then"), EBlueprintHelperGraphFragmentPortDirection::ExecInput);
	const FBlueprintHelperGraphFragmentEndpointRef JoinElse = MakeExecEndpoint(JoinFragmentId, TEXT("exec.in.else"), TEXT("else"), EBlueprintHelperGraphFragmentPortDirection::ExecInput);
	const FBlueprintHelperGraphFragmentEndpointRef JoinExit = MakeExecExit(JoinFragmentId);

	SymbolScopes.AddDefaulted();
	const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow ThenFlow = BuildStatementArray(
		Statement->ThenStatements,
		Statement->Path + TEXT(".then"),
		State,
		SymbolScopes);
	SymbolScopes.Pop();

	SymbolScopes.AddDefaulted();
	const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow ElseFlow = BuildStatementArray(
		Statement->ElseStatements,
		Statement->Path + TEXT(".else"),
		State,
		SymbolScopes);
	SymbolScopes.Pop();

	if (ThenFlow.Entries.Num() == 0)
	{
		AddExecEdge(State, BranchThen, JoinThen, Statement->Path + TEXT(".then"));
	}
	else
	{
		for (const FBlueprintHelperGraphFragmentEndpointRef& Entry : ThenFlow.Entries)
		{
			AddExecEdge(State, BranchThen, Entry, Statement->Path + TEXT(".then"));
		}
		for (const FBlueprintHelperGraphFragmentEndpointRef& Exit : ThenFlow.Exits)
		{
			AddExecEdge(State, Exit, JoinThen, Statement->Path + TEXT(".then.join"));
		}
	}

	if (ElseFlow.Entries.Num() == 0)
	{
		AddExecEdge(State, BranchElse, JoinElse, Statement->Path + TEXT(".else"));
	}
	else
	{
		for (const FBlueprintHelperGraphFragmentEndpointRef& Entry : ElseFlow.Entries)
		{
			AddExecEdge(State, BranchElse, Entry, Statement->Path + TEXT(".else"));
		}
		for (const FBlueprintHelperGraphFragmentEndpointRef& Exit : ElseFlow.Exits)
		{
			AddExecEdge(State, Exit, JoinElse, Statement->Path + TEXT(".else.join"));
		}
	}

	Flow.Exits.Add(JoinExit);
	return Flow;
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow FBlueprintHelperGraphFragmentDagBuilderUtils::BuildStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	const FString& FallbackPath,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	if (!Statement.IsValid())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("statement_missing"),
			FallbackPath,
			TEXT("Skipped missing statement while building fragment DAG."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
		return FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow();
	}

	switch (Statement->Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		return BuildSimpleStatement(Statement, TEXT("statement_call"), TEXT("call"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Field:
	{
		const bool bPropertyPath = Statement->FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase);
		const bool bComponentRef = Statement->FieldScope.Equals(TEXT("component_ref"), ESearchCase::IgnoreCase);
		const bool bFieldAccess = Statement->FieldScope.Equals(TEXT("field_access"), ESearchCase::IgnoreCase);
		return BuildSimpleStatement(
			Statement,
			bComponentRef ? TEXT("statement_set_component_ref") : (bFieldAccess ? TEXT("statement_set_field_access") : (bPropertyPath ? TEXT("statement_set_property") : TEXT("statement_set"))),
			bComponentRef ? TEXT("set_component_ref") : (bFieldAccess ? TEXT("set_field_access") : (bPropertyPath ? TEXT("set_property") : TEXT("set"))),
			State,
			SymbolScopes);
	}

	case EBlueprintHelperGraphStatementKind::Return:
		return BuildSimpleStatement(Statement, TEXT("statement_return"), TEXT("return"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Let:
		return BuildLetStatement(Statement, State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Branch:
		return BuildBranchStatement(Statement, State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Sequence:
		return BuildSimpleStatement(Statement, TEXT("statement_sequence"), TEXT("sequence"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Create:
		return BuildSimpleStatement(Statement, TEXT("statement_create"), TEXT("create"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Convert:
		return BuildSimpleStatement(Statement, TEXT("statement_convert"), TEXT("convert"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Schedule:
		return BuildSimpleStatement(Statement, TEXT("statement_schedule"), TEXT("schedule"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::ContainerAction:
		return BuildSimpleStatement(Statement, TEXT("statement_container_action"), TEXT("container_action"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::ComponentBoundEvent:
		return BuildSimpleStatement(Statement, TEXT("statement_component_bound_event"), TEXT("component_bound_event"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Delegate:
		return BuildSimpleStatement(Statement, TEXT("statement_delegate"), TEXT("delegate"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Unknown:
	default:
	{
		FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow Flow;
		FBlueprintHelperGraphFragmentRef& Fragment = AddStatementFragment(*Statement, TEXT("statement_unknown"), TEXT("unknown"), State);
		AddMetadata(Fragment, TEXT("placeholder"), TEXT("true"));
		Flow.Entries.Add(MakeExecEntry(Fragment.FragmentId));
		Flow.Exits.Add(MakeExecExit(Fragment.FragmentId));
		AddBuilderDiagnostic(
			State,
			TEXT("statement_kind_unknown"),
			Statement->Path,
			TEXT("Unknown statement kind was represented as a placeholder fragment."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		return Flow;
	}
	}
}
FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow FBlueprintHelperGraphFragmentDagBuilderUtils::BuildStatementArray(
	const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
	const FString& Path,
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow SequenceFlow;
	TArray<FBlueprintHelperGraphFragmentEndpointRef> PendingExits;

	for (int32 StatementIndex = 0; StatementIndex < Statements.Num(); ++StatementIndex)
	{
		const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement = Statements[StatementIndex];
		const FString StatementPath = Statement.IsValid()
			? Statement->Path
			: FString::Printf(TEXT("%s[%d]"), *Path, StatementIndex);

		const FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagExecFlow CurrentFlow = BuildStatement(Statement, StatementPath, State, SymbolScopes);
		if (CurrentFlow.IsEmpty())
		{
			continue;
		}

		if (SequenceFlow.Entries.Num() == 0)
		{
			SequenceFlow.Entries = CurrentFlow.Entries;
		}

		if (PendingExits.Num() > 0)
		{
			for (const FBlueprintHelperGraphFragmentEndpointRef& PreviousExit : PendingExits)
			{
				for (const FBlueprintHelperGraphFragmentEndpointRef& CurrentEntry : CurrentFlow.Entries)
				{
					AddExecEdge(State, PreviousExit, CurrentEntry, StatementPath);
				}
			}
		}

		PendingExits = CurrentFlow.Exits;
	}

	SequenceFlow.Exits = PendingExits;
	return SequenceFlow;
}
void FBlueprintHelperGraphFragmentDagBuilderUtils::CopySemanticDiagnostics(
	const FBlueprintHelperGraphSemanticIR& SemanticIR,
	FBlueprintHelperGraphFragmentDag& OutDag)
{
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : SemanticIR.Diagnostics)
	{
		OutDag.AddDiagnostic(
			TEXT("semantic.") + Diagnostic.Code,
			Diagnostic.Path,
			Diagnostic.Message,
			ConvertSeverity(Diagnostic.Severity));
	}
}
