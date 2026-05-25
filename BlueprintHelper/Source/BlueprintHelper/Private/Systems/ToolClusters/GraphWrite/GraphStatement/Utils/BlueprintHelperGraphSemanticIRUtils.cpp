// BlueprintHelper GraphStatement BlueprintHelperGraphSemanticIRUtils implementation.

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

FString FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		return LexToString(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	case EJson::Null:
		return FString();
	case EJson::Object:
	{
		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer);
		return Serialized;
	}
	case EJson::Array:
	{
		FString Serialized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Value->AsArray(), Writer);
		return Serialized;
	}
	default:
		return FString();
	}
}
FString FBlueprintHelperGraphSemanticIRUtils::JsonValueToSemanticType(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	switch (Value->Type)
	{
	case EJson::Boolean:
		return TEXT("bool");
	case EJson::Number:
	{
		const double Number = Value->AsNumber();
		return FMath::IsNearlyEqual(Number, FMath::TruncToDouble(Number))
			? FString(TEXT("int"))
			: FString(TEXT("double"));
	}
	case EJson::String:
		return TEXT("string");
	default:
		return FString();
	}
}
EBlueprintHelperGraphStatementKind FBlueprintHelperGraphSemanticIRUtils::ParseStatementKind(const FString& Kind)
{
	if (Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Call;
	if (Kind.Equals(TEXT("field"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Field;
	if (Kind.Equals(TEXT("branch"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Branch;
	if (Kind.Equals(TEXT("sequence"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Sequence;
	if (Kind.Equals(TEXT("let"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Let;
	if (Kind.Equals(TEXT("return"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Return;
	if (Kind.Equals(TEXT("create"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Create;
	if (Kind.Equals(TEXT("convert"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Convert;
	if (Kind.Equals(TEXT("schedule"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Schedule;
	if (Kind.Equals(TEXT("container_action"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::ContainerAction;
	if (Kind.Equals(TEXT("component_bound_event"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::ComponentBoundEvent;
	if (Kind.Equals(TEXT("delegate"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Delegate;
	return EBlueprintHelperGraphStatementKind::Unknown;
}
EBlueprintHelperGraphExpressionKind FBlueprintHelperGraphSemanticIRUtils::ParseExpressionKind(const FString& Kind)
{
	if (Kind.Equals(TEXT("literal"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Literal;
	if (Kind.Equals(TEXT("field"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Field;
	if (Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Call;
	if (Kind.Equals(TEXT("op"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Op;
	if (Kind.Equals(TEXT("construct"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Construct;
	if (Kind.Equals(TEXT("deconstruct"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Deconstruct;
	if (Kind.Equals(TEXT("select"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Select;
	if (Kind.Equals(TEXT("create"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Create;
	if (Kind.Equals(TEXT("convert"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Convert;
	if (Kind.Equals(TEXT("schedule"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Schedule;
	if (Kind.Equals(TEXT("container_action"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::ContainerAction;
	if (Kind.Equals(TEXT("get"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Field;
	if (Kind.Equals(TEXT("get_property"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Field;
	return EBlueprintHelperGraphExpressionKind::Unknown;
}
void FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FString& Code,
	const FString& Path,
	const FString& Message,
	const FString& Severity)
{
	FBlueprintHelperGraphSemanticDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Path = Path;
	Diagnostic.Message = Message;
	Diagnostic.Severity = Severity;
	OutIR.Diagnostics.Add(MoveTemp(Diagnostic));
}
void FBlueprintHelperGraphSemanticIRUtils::ReadOptionalStringArrayField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	TArray<FString>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString Text;
		if (Value.IsValid() && Value->TryGetString(Text) && !Text.TrimStartAndEnd().IsEmpty())
		{
			OutValues.Add(Text.TrimStartAndEnd());
		}
	}
}
FString FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(const FString& Name)
{
	return Name.TrimStartAndEnd().ToLower();
}
FString FBlueprintHelperGraphSemanticIRUtils::NormalizeSemanticTypeToken(const FString& Type)
{
	FString Token = Type;
	Token.TrimStartAndEndInline();
	Token.ToLowerInline();
	Token.ReplaceInline(TEXT(" "), TEXT(""));
	Token.ReplaceInline(TEXT("-"), TEXT(""));
	Token.ReplaceInline(TEXT("_"), TEXT(""));
	if (Token.StartsWith(TEXT("f")) || Token.StartsWith(TEXT("u")) || Token.StartsWith(TEXT("a")))
	{
		Token.RightChopInline(1);
	}
	return Token;
}
bool FBlueprintHelperGraphSemanticIRUtils::IsSemanticBoolType(const FString& Type)
{
	const FString Token = NormalizeSemanticTypeToken(Type);
	return Token == TEXT("bool") || Token == TEXT("boolean");
}
bool FBlueprintHelperGraphSemanticIRUtils::IsSemanticIntegerType(const FString& Type)
{
	const FString Token = NormalizeSemanticTypeToken(Type);
	return Token == TEXT("int") || Token == TEXT("integer") || Token == TEXT("int32") || Token == TEXT("int64") || Token == TEXT("byte");
}
bool FBlueprintHelperGraphSemanticIRUtils::IsSemanticNumericType(const FString& Type)
{
	const FString Token = NormalizeSemanticTypeToken(Type);
	return IsSemanticIntegerType(Type) || Token == TEXT("float") || Token == TEXT("double") || Token == TEXT("real") || Token == TEXT("number");
}
bool FBlueprintHelperGraphSemanticIRUtils::AreSemanticTypesCompatible(const FString& Left, const FString& Right)
{
	const FString LeftToken = NormalizeSemanticTypeToken(Left);
	const FString RightToken = NormalizeSemanticTypeToken(Right);
	if (LeftToken.IsEmpty() || RightToken.IsEmpty())
	{
		return true;
	}
	if (LeftToken == RightToken)
	{
		return true;
	}
	if (IsSemanticNumericType(LeftToken) && IsSemanticNumericType(RightToken))
	{
		return true;
	}
	return false;
}
FString FBlueprintHelperGraphSemanticIRUtils::NormalizeTypeLookupKey(const FString& Name)
{
	FString Result = NormalizeSymbolKey(Name);
	if (Result.StartsWith(TEXT("class ")))
	{
		Result.RightChopInline(6);
	}
	if (Result.StartsWith(TEXT("struct ")))
	{
		Result.RightChopInline(7);
	}
	if (Result.EndsWith(TEXT("_c")))
	{
		Result.LeftChopInline(2);
	}
	if (Result.StartsWith(TEXT("f")) || Result.StartsWith(TEXT("u")) || Result.StartsWith(TEXT("a")))
	{
		Result.RightChopInline(1);
	}
	return Result;
}
const UStruct* FBlueprintHelperGraphSemanticIRUtils::TryResolveStructByTypeName(const FString& TypeName)
{
	const FString Wanted = NormalizeTypeLookupKey(TypeName);
	if (Wanted.IsEmpty())
	{
		return nullptr;
	}

	for (TObjectIterator<UStruct> It; It; ++It)
	{
		const UStruct* Candidate = *It;
		if (!Candidate)
		{
			continue;
		}

		const FString CandidateName = NormalizeTypeLookupKey(Candidate->GetName());
		const FString CandidateCppName = NormalizeTypeLookupKey(Candidate->GetPrefixCPP() + Candidate->GetName());
		const FString CandidatePath = NormalizeTypeLookupKey(Candidate->GetPathName());
		if (CandidateName == Wanted || CandidateCppName == Wanted || CandidatePath == Wanted || CandidatePath.EndsWith(TEXT(".") + Wanted))
		{
			return Candidate;
		}
	}

	return nullptr;
}
FString FBlueprintHelperGraphSemanticIRUtils::StatementPatternName(EBlueprintHelperGraphStatementKind Kind)
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
FString FBlueprintHelperGraphSemanticIRUtils::ExpressionPatternName(EBlueprintHelperGraphExpressionKind Kind)
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
FBlueprintHelperGraphResolvedTarget FBlueprintHelperGraphSemanticIRUtils::ResolveTargetString(
	const FString& RawTarget,
	EBlueprintHelperGraphStatementKind StatementKind,
	EBlueprintHelperGraphExpressionKind ExpressionKind,
	const FBlueprintHelperGraphSemanticContext& Context)
{
	FBlueprintHelperGraphResolvedTarget Target;
	Target.Raw = RawTarget.TrimStartAndEnd();
	if (Target.Raw.IsEmpty())
	{
		return Target;
	}

	FString Owner;
	FString Remainder;
	const bool bHasOwner = Target.Raw.Split(TEXT("."), &Owner, &Remainder, ESearchCase::CaseSensitive, ESearchDir::FromStart);

	if (StatementKind == EBlueprintHelperGraphStatementKind::Call
		|| ExpressionKind == EBlueprintHelperGraphExpressionKind::Call)
	{
		FString NativeOwner;
		FString NativeMember;
		if (Target.Raw.Split(TEXT(":"), &NativeOwner, &NativeMember, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
			&& !NativeMember.IsEmpty())
		{
			Target.Kind = EBlueprintHelperGraphTargetKind::Function;
			Target.Owner = NativeOwner;
			Target.Member = NativeMember;
			Target.Type = Context.FindTargetType(NativeMember);
			Target.bVerifiedByContext = Context.IsFunction(NativeMember) || Context.IsFunction(Target.Raw);
			return Target;
		}

		if (bHasOwner)
		{
			Target.Kind = EBlueprintHelperGraphTargetKind::ComponentMemberFunction;
			Target.Owner = Owner;
			Target.Member = Remainder;
			Target.Type = Context.FindTargetType(Owner);
			Target.bVerifiedByContext = Context.HasMemberFunction(Owner, Remainder) || Context.IsFunction(Remainder) || Context.IsFunction(Target.Raw);
			return Target;
		}

		Target.Kind = EBlueprintHelperGraphTargetKind::Function;
		Target.Member = Target.Raw;
		Target.Type = Context.FindTargetType(Target.Raw);
		Target.bVerifiedByContext = Context.IsFunction(Target.Raw);
		return Target;
	}

	if (bHasOwner
		&& (ExpressionKind == EBlueprintHelperGraphExpressionKind::Field
			|| StatementKind == EBlueprintHelperGraphStatementKind::Field
			|| Context.IsComponent(Owner)
			|| Context.IsVariable(Owner)))
	{
		Target.Kind = EBlueprintHelperGraphTargetKind::PropertyPath;
		Target.Owner = Owner;
		Target.PropertyPath = Remainder;
		FString ResolvedPropertyType;
		Target.bVerifiedByContext = Context.HasPropertyPath(Owner, Remainder, ResolvedPropertyType);
		Target.Type = ResolvedPropertyType.IsEmpty() ? Context.FindTargetType(Owner) : ResolvedPropertyType;
		return Target;
	}

	if (Context.IsComponent(Target.Raw))
	{
		Target.Kind = EBlueprintHelperGraphTargetKind::Component;
		Target.Member = Target.Raw;
		Target.Type = Context.FindTargetType(Target.Raw);
		Target.bVerifiedByContext = true;
		return Target;
	}

	Target.Kind = EBlueprintHelperGraphTargetKind::Variable;
	Target.Member = Target.Raw;
	Target.Type = Context.FindTargetType(Target.Raw);
	Target.bVerifiedByContext = Context.IsVariable(Target.Raw);
	return Target;
}
void FBlueprintHelperGraphSemanticIRUtils::RegisterSymbol(
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FString& Name,
	const FString& StatementId,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& SourceExpression,
	const FString& Path,
	TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack)
{
	const FString Key = NormalizeSymbolKey(Name);
	if (Key.IsEmpty())
	{
		AddDiagnostic(OutIR, TEXT("symbol_name_missing"), Path, TEXT("let statement requires a non-empty name."));
		return;
	}

	if (ScopeStack.Num() == 0)
	{
		ScopeStack.AddDefaulted();
	}

	TMap<FString, FBlueprintHelperGraphSymbol>& CurrentScope = ScopeStack.Last();
	if (CurrentScope.Contains(Key))
	{
		AddDiagnostic(OutIR, TEXT("symbol_duplicate"), Path, FString::Printf(TEXT("Duplicate temporary symbol: %s."), *Name));
		return;
	}

	FBlueprintHelperGraphSymbol Symbol;
	Symbol.Name = Name;
	Symbol.SymbolId = Key;
	Symbol.ScopeId = FString::Printf(TEXT("scope_%d"), ScopeStack.Num() - 1);
	Symbol.SourceStatementId = StatementId;
	Symbol.SourceExpressionId = SourceExpression.IsValid() ? SourceExpression->ExpressionId : FString();
	Symbol.Type = SourceExpression.IsValid() ? SourceExpression->Type : FString();
	Symbol.Path = Path;
	CurrentScope.Add(Key, Symbol);
	OutIR.Symbols.Add(Symbol.ScopeId + TEXT(":") + Key + TEXT(":") + Path, MoveTemp(Symbol));
}
void FBlueprintHelperGraphSemanticIRUtils::AddUnverifiedTargetDiagnostic(
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FBlueprintHelperGraphSemanticContext& Context,
	const FBlueprintHelperGraphResolvedTarget& Target,
	const FString& Path)
{
	if (!Context.HasVariables() || Target.Raw.IsEmpty() || Target.bVerifiedByContext)
	{
		return;
	}

	AddDiagnostic(
		OutIR,
		TEXT("target_unverified"),
		Path,
		FString::Printf(TEXT("Target was not found in Blueprint context: %s."), *Target.Raw),
		TEXT("warning"));
}
bool FBlueprintHelperGraphSemanticIRUtils::FindSymbolInScopes(
	const FString& Name,
	const TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack,
	FBlueprintHelperGraphSymbol& OutSymbol)
{
	const FString Key = NormalizeSymbolKey(Name);
	for (int32 ScopeIndex = ScopeStack.Num() - 1; ScopeIndex >= 0; --ScopeIndex)
	{
		if (const FBlueprintHelperGraphSymbol* Symbol = ScopeStack[ScopeIndex].Find(Key))
		{
			OutSymbol = *Symbol;
			return true;
		}
	}
	return false;
}
