#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

namespace
{
struct FBlueprintHelperDagDataProducer
{
	FBlueprintHelperGraphFragmentEndpointRef Endpoint;
	FString SymbolId;
	FString Type;
	FString Path;

	bool IsValid() const
	{
		return Endpoint.IsValid();
	}
};

struct FBlueprintHelperDagExecFlow
{
	TArray<FBlueprintHelperGraphFragmentEndpointRef> Entries;
	TArray<FBlueprintHelperGraphFragmentEndpointRef> Exits;

	bool IsEmpty() const
	{
		return Entries.Num() == 0 && Exits.Num() == 0;
	}
};

struct FBlueprintHelperDagBuildState
{
	FBlueprintHelperGraphFragmentDag* Dag = nullptr;
	TSet<FString> FragmentIds;
	int32 FragmentSerial = 0;
	int32 ExecEdgeSerial = 0;
	int32 DataEdgeSerial = 0;
};

static FString BoolText(const bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

static FString NormalizeSymbolKey(const FString& Name)
{
	return Name.TrimStartAndEnd().ToLower();
}

static FString SanitizeIdPart(const FString& Value)
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

static FString StatementKindName(const EBlueprintHelperGraphStatementKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		return TEXT("call");
	case EBlueprintHelperGraphStatementKind::Set:
		return TEXT("set");
	case EBlueprintHelperGraphStatementKind::Branch:
		return TEXT("branch");
	case EBlueprintHelperGraphStatementKind::Let:
		return TEXT("let");
	case EBlueprintHelperGraphStatementKind::Return:
		return TEXT("return");
	default:
		return TEXT("unknown");
	}
}

static FString ExpressionKindName(const EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Literal:
		return TEXT("literal");
	case EBlueprintHelperGraphExpressionKind::Get:
		return TEXT("get");
	case EBlueprintHelperGraphExpressionKind::GetProperty:
		return TEXT("get_property");
	case EBlueprintHelperGraphExpressionKind::Ref:
		return TEXT("ref");
	case EBlueprintHelperGraphExpressionKind::Call:
		return TEXT("call");
	case EBlueprintHelperGraphExpressionKind::Compare:
		return TEXT("compare");
	case EBlueprintHelperGraphExpressionKind::Select:
		return TEXT("select");
	case EBlueprintHelperGraphExpressionKind::MakeStruct:
		return TEXT("make_struct");
	default:
		return TEXT("unknown");
	}
}

static FString TargetKindName(const EBlueprintHelperGraphTargetKind Kind)
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

static EBlueprintHelperGraphFragmentDiagnosticSeverity ConvertSeverity(const FString& Severity)
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

static void AddMetadata(FBlueprintHelperGraphFragmentRef& Fragment, const FString& Key, const FString& Value)
{
	if (!Key.IsEmpty() && !Value.IsEmpty())
	{
		Fragment.Metadata.Add(Key, Value);
	}
}

static void AddResolvedTargetMetadata(
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

static FString MakeUniqueFragmentId(FBlueprintHelperDagBuildState& State, const FString& PreferredId)
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

static FBlueprintHelperGraphFragmentRef& AddFragment(
	FBlueprintHelperDagBuildState& State,
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

static FBlueprintHelperGraphFragmentEndpointRef MakeEndpoint(
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

static FBlueprintHelperGraphFragmentEndpointRef MakeExecEndpoint(
	const FString& FragmentId,
	const FString& PortId,
	const FString& PinName,
	const EBlueprintHelperGraphFragmentPortDirection Direction = EBlueprintHelperGraphFragmentPortDirection::Unknown)
{
	return MakeEndpoint(FragmentId, PortId, PinName, TEXT("exec"), Direction);
}

static FBlueprintHelperGraphFragmentEndpointRef MakeExecEntry(const FString& FragmentId)
{
	return MakeExecEndpoint(FragmentId, TEXT("exec.in"), TEXT("execute"), EBlueprintHelperGraphFragmentPortDirection::ExecInput);
}

static FBlueprintHelperGraphFragmentEndpointRef MakeExecExit(const FString& FragmentId)
{
	return MakeExecEndpoint(FragmentId, TEXT("exec.out"), TEXT("then"), EBlueprintHelperGraphFragmentPortDirection::ExecOutput);
}

static FBlueprintHelperGraphFragmentEndpointRef MakeDataInput(
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

static FBlueprintHelperGraphFragmentEndpointRef MakeDataOutput(
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

static void AddBuilderDiagnostic(
	FBlueprintHelperDagBuildState& State,
	const FString& Code,
	const FString& Path,
	const FString& Message,
	const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity = EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning)
{
	State.Dag->AddDiagnostic(Code, Path, Message, Severity);
}

static void AddExecEdge(
	FBlueprintHelperDagBuildState& State,
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

static void AddDataEdge(
	FBlueprintHelperDagBuildState& State,
	const FBlueprintHelperDagDataProducer& Producer,
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

static FString MakeStatementFragmentId(
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

static FString MakeExpressionFragmentId(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& Suffix)
{
	const FString SourceId = !Expression.ExpressionId.IsEmpty() ? Expression.ExpressionId : Expression.Path;
	if (!SourceId.Contains(TEXT("$")) && !SourceId.Contains(TEXT(".")) && !SourceId.Contains(TEXT("[")) && !SourceId.Contains(TEXT("]")))
	{
		return SourceId;
	}
	return TEXT("expr_") + ExpressionKindName(Expression.Kind) + TEXT("_") + SourceId + TEXT("_") + Suffix;
}

static FBlueprintHelperDagDataProducer BuildExpression(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& FallbackPath,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);

static bool FindSymbolProducer(
	const FString& Name,
	const TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes,
	FBlueprintHelperDagDataProducer& OutProducer)
{
	const FString Key = NormalizeSymbolKey(Name);
	for (int32 ScopeIndex = SymbolScopes.Num() - 1; ScopeIndex >= 0; --ScopeIndex)
	{
		if (const FBlueprintHelperDagDataProducer* Producer = SymbolScopes[ScopeIndex].Find(Key))
		{
			OutProducer = *Producer;
			return true;
		}
	}
	return false;
}

static void RegisterSymbolProducer(
	const FString& Name,
	const FBlueprintHelperDagDataProducer& Producer,
	const FString& Path,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
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

	TMap<FString, FBlueprintHelperDagDataProducer>& CurrentScope = SymbolScopes.Last();
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

static void ConnectExpressionToInput(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& FallbackPath,
	const FString& InputName,
	const FString& InputType,
	const FString& ConsumerFragmentId,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	const FBlueprintHelperDagDataProducer Producer = BuildExpression(Expression, FallbackPath, State, SymbolScopes);
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

static void ConnectExpressionMapToInputs(
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
	const FString& FallbackPath,
	const FString& ConsumerFragmentId,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
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

static FBlueprintHelperGraphFragmentRef& AddExpressionFragment(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& Kind,
	const FString& Suffix,
	FBlueprintHelperDagBuildState& State)
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

static FBlueprintHelperDagDataProducer MakeExpressionProducerFromId(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& FragmentId,
	const FString& OutputName,
	const FString& FallbackType);

static FBlueprintHelperDagDataProducer MakeExpressionProducer(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FBlueprintHelperGraphFragmentRef& Fragment,
	const FString& OutputName,
	const FString& FallbackType)
{
	return MakeExpressionProducerFromId(Expression, Fragment.FragmentId, OutputName, FallbackType);
}

static FBlueprintHelperDagDataProducer MakeExpressionProducerFromId(
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& FragmentId,
	const FString& OutputName,
	const FString& FallbackType)
{
	FBlueprintHelperDagDataProducer Producer;
	Producer.Endpoint = MakeDataOutput(
		FragmentId,
		OutputName,
		!Expression.Type.IsEmpty() ? Expression.Type : FallbackType);
	Producer.SymbolId = TEXT("expr:") + FragmentId + TEXT(":") + SanitizeIdPart(OutputName);
	Producer.Type = Producer.Endpoint.Type;
	Producer.Path = Expression.Path;
	return Producer;
}

static FBlueprintHelperDagDataProducer BuildPlaceholderExpression(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& Kind,
	const FString& Suffix,
	const FString& OutputName,
	const FString& OutputType,
	const FString& DiagnosticCode,
	const FString& DiagnosticMessage,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes,
	const EBlueprintHelperGraphFragmentDiagnosticSeverity DiagnosticSeverity = EBlueprintHelperGraphFragmentDiagnosticSeverity::Info)
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

	ConnectExpressionMapToInputs(Expression->Args, Expression->Path + TEXT(".args"), FragmentId, State, SymbolScopes);

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

static FBlueprintHelperDagDataProducer BuildExpression(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	const FString& FallbackPath,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	if (!Expression.IsValid())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("expression_missing"),
			FallbackPath,
			TEXT("Skipped missing expression while building fragment DAG."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
		return FBlueprintHelperDagDataProducer();
	}

	switch (Expression->Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Ref:
	{
		FBlueprintHelperDagDataProducer Producer;
		if (FindSymbolProducer(Expression->Name, SymbolScopes, Producer))
		{
			return Producer;
		}

		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_ref_unresolved"), TEXT("unresolved"), State);
		const FString FragmentId = Fragment.FragmentId;
		AddMetadata(Fragment, TEXT("placeholder"), TEXT("true"));
		AddBuilderDiagnostic(
			State,
			TEXT("ref_symbol_unresolved"),
			Expression->Path,
			FString::Printf(TEXT("No symbol producer found for ref expression: %s."), *Expression->Name),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		return MakeExpressionProducerFromId(*Expression, FragmentId, TEXT("value"), Expression->Type);
	}

	case EBlueprintHelperGraphExpressionKind::Literal:
	{
		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_literal"), TEXT("literal"), State);
		return MakeExpressionProducer(*Expression, Fragment, TEXT("value"), Expression->Type);
	}

	case EBlueprintHelperGraphExpressionKind::Get:
	{
		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_get"), TEXT("get"), State);
		return MakeExpressionProducer(*Expression, Fragment, TEXT("value"), Expression->Type);
	}

	case EBlueprintHelperGraphExpressionKind::GetProperty:
	{
		FBlueprintHelperGraphFragmentRef& Fragment = AddExpressionFragment(*Expression, TEXT("expr_get_property"), TEXT("get_property"), State);
		return MakeExpressionProducer(*Expression, Fragment, TEXT("value"), Expression->Type);
	}

	case EBlueprintHelperGraphExpressionKind::Call:
		return BuildPlaceholderExpression(
			Expression,
			TEXT("expr_call"),
			TEXT("call"),
			TEXT("return"),
			Expression->Type,
			FString(),
			FString(),
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Compare:
		return BuildPlaceholderExpression(
			Expression,
			TEXT("expr_compare"),
			TEXT("compare"),
			TEXT("result"),
			TEXT("bool"),
			FString(),
			FString(),
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::Select:
		return BuildPlaceholderExpression(
			Expression,
			TEXT("expr_select"),
			TEXT("select"),
			TEXT("result"),
			Expression->Type,
			FString(),
			FString(),
			State,
			SymbolScopes);

	case EBlueprintHelperGraphExpressionKind::MakeStruct:
		return BuildPlaceholderExpression(
			Expression,
			TEXT("expr_make_struct"),
			TEXT("make_struct"),
			TEXT("value"),
			Expression->Type,
			FString(),
			FString(),
			State,
			SymbolScopes);

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

static FBlueprintHelperGraphFragmentRef& AddStatementFragment(
	const FBlueprintHelperGraphStatementIR& Statement,
	const FString& Kind,
	const FString& Suffix,
	FBlueprintHelperDagBuildState& State)
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

static FBlueprintHelperDagExecFlow BuildStatementArray(
	const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
	const FString& Path,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);

static FBlueprintHelperDagExecFlow BuildSimpleStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	const FString& Kind,
	const FString& Suffix,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperDagExecFlow Flow;
	FBlueprintHelperGraphFragmentRef& Fragment = AddStatementFragment(*Statement, Kind, Suffix, State);
	const FString FragmentId = Fragment.FragmentId;
	Flow.Entries.Add(MakeExecEntry(FragmentId));
	Flow.Exits.Add(MakeExecExit(FragmentId));

	ConnectExpressionMapToInputs(Statement->Args, Statement->Path + TEXT(".args"), FragmentId, State, SymbolScopes);
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

	return Flow;
}

static FBlueprintHelperDagExecFlow BuildLetStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperDagExecFlow Flow;
	FBlueprintHelperGraphFragmentRef& Fragment = AddStatementFragment(*Statement, TEXT("statement_let"), TEXT("let"), State);
	const FString FragmentId = Fragment.FragmentId;
	AddMetadata(Fragment, TEXT("defines_symbol"), Statement->Name);
	AddMetadata(Fragment, TEXT("symbol_id"), NormalizeSymbolKey(Statement->Name));

	Flow.Entries.Add(MakeExecEntry(FragmentId));
	Flow.Exits.Add(MakeExecExit(FragmentId));

	FString SymbolType;
	FBlueprintHelperDagDataProducer ValueProducer;
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

	FBlueprintHelperDagDataProducer SymbolProducer;
	SymbolProducer = bHasValueProducer ? ValueProducer : FBlueprintHelperDagDataProducer();
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

static FBlueprintHelperDagExecFlow BuildBranchStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperDagExecFlow Flow;
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
	const FBlueprintHelperDagExecFlow ThenFlow = BuildStatementArray(
		Statement->ThenStatements,
		Statement->Path + TEXT(".then"),
		State,
		SymbolScopes);
	SymbolScopes.Pop();

	SymbolScopes.AddDefaulted();
	const FBlueprintHelperDagExecFlow ElseFlow = BuildStatementArray(
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

static FBlueprintHelperDagExecFlow BuildStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	const FString& FallbackPath,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	if (!Statement.IsValid())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("statement_missing"),
			FallbackPath,
			TEXT("Skipped missing statement while building fragment DAG."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
		return FBlueprintHelperDagExecFlow();
	}

	switch (Statement->Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		return BuildSimpleStatement(Statement, TEXT("statement_call"), TEXT("call"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Set:
		return BuildSimpleStatement(Statement, TEXT("statement_set"), TEXT("set"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Return:
		return BuildSimpleStatement(Statement, TEXT("statement_return"), TEXT("return"), State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Let:
		return BuildLetStatement(Statement, State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Branch:
		return BuildBranchStatement(Statement, State, SymbolScopes);

	case EBlueprintHelperGraphStatementKind::Unknown:
	default:
	{
		FBlueprintHelperDagExecFlow Flow;
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

static FBlueprintHelperDagExecFlow BuildStatementArray(
	const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
	const FString& Path,
	FBlueprintHelperDagBuildState& State,
	TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes)
{
	FBlueprintHelperDagExecFlow SequenceFlow;
	TArray<FBlueprintHelperGraphFragmentEndpointRef> PendingExits;

	for (int32 StatementIndex = 0; StatementIndex < Statements.Num(); ++StatementIndex)
	{
		const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement = Statements[StatementIndex];
		const FString StatementPath = Statement.IsValid()
			? Statement->Path
			: FString::Printf(TEXT("%s[%d]"), *Path, StatementIndex);

		const FBlueprintHelperDagExecFlow CurrentFlow = BuildStatement(Statement, StatementPath, State, SymbolScopes);
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

static void CopySemanticDiagnostics(
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
}

bool FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(
	const FBlueprintHelperGraphSemanticIR& SemanticIR,
	FBlueprintHelperGraphFragmentDag& OutDag)
{
	OutDag.Reset();
	OutDag.Schema = TEXT("BlueprintHelperGraphFragmentDag.v1");
	OutDag.Metadata.Add(TEXT("builder"), TEXT("statement_tree_to_fragment_dag.phase2.narrow"));
	OutDag.Metadata.Add(TEXT("source_schema"), SemanticIR.Schema);
	OutDag.Metadata.Add(TEXT("statement_count"), LexToString(SemanticIR.Statements.Num()));
	OutDag.Metadata.Add(TEXT("symbol_count"), LexToString(SemanticIR.Symbols.Num()));

	CopySemanticDiagnostics(SemanticIR, OutDag);

	FBlueprintHelperDagBuildState State;
	State.Dag = &OutDag;

	TArray<TMap<FString, FBlueprintHelperDagDataProducer>> SymbolScopes;
	SymbolScopes.AddDefaulted();

	const FBlueprintHelperDagExecFlow TopLevelFlow = BuildStatementArray(
		SemanticIR.Statements,
		TEXT("$.statements"),
		State,
		SymbolScopes);

	OutDag.EntryExitRefs.Entries = TopLevelFlow.Entries;
	OutDag.EntryExitRefs.Exits = TopLevelFlow.Exits;

	if (SemanticIR.Statements.Num() == 0)
	{
		OutDag.AddDiagnostic(
			TEXT("statement_tree_empty"),
			TEXT("$.statements"),
			TEXT("Semantic IR contains no statements to arrange into a fragment DAG."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
	}
	else
	{
		if (!OutDag.EntryExitRefs.HasEntry())
		{
			OutDag.AddDiagnostic(
				TEXT("dag_entry_missing"),
				TEXT("$.statements"),
				TEXT("Fragment DAG has no valid entry endpoint."),
				EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		}
		if (!OutDag.EntryExitRefs.HasExit())
		{
			OutDag.AddDiagnostic(
				TEXT("dag_exit_missing"),
				TEXT("$.statements"),
				TEXT("Fragment DAG has no valid exit endpoint."),
				EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
		}
	}

	return !OutDag.HasErrors();
}
