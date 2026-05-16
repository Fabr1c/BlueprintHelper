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

namespace
{
static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
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

static FString JsonValueToSemanticType(const TSharedPtr<FJsonValue>& Value)
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

static EBlueprintHelperGraphStatementKind ParseStatementKind(const FString& Kind)
{
	if (Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Call;
	if (Kind.Equals(TEXT("set"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Set;
	if (Kind.Equals(TEXT("branch"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Branch;
	if (Kind.Equals(TEXT("let"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Let;
	if (Kind.Equals(TEXT("return"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Return;
	return EBlueprintHelperGraphStatementKind::Unknown;
}

static EBlueprintHelperGraphExpressionKind ParseExpressionKind(const FString& Kind)
{
	if (Kind.Equals(TEXT("literal"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Literal;
	if (Kind.Equals(TEXT("get"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Get;
	if (Kind.Equals(TEXT("get_property"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::GetProperty;
	if (Kind.Equals(TEXT("ref"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Ref;
	if (Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Call;
	if (Kind.Equals(TEXT("compare"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Compare;
	if (Kind.Equals(TEXT("select"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Select;
	if (Kind.Equals(TEXT("make_struct"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::MakeStruct;
	return EBlueprintHelperGraphExpressionKind::Unknown;
}

static void AddDiagnostic(
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FString& Code,
	const FString& Path,
	const FString& Message,
	const FString& Severity = TEXT("error"))
{
	FBlueprintHelperGraphSemanticDiagnostic Diagnostic;
	Diagnostic.Code = Code;
	Diagnostic.Path = Path;
	Diagnostic.Message = Message;
	Diagnostic.Severity = Severity;
	OutIR.Diagnostics.Add(MoveTemp(Diagnostic));
}

static void ReadOptionalStringArrayField(
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

static FString NormalizeSymbolKey(const FString& Name)
{
	return Name.TrimStartAndEnd().ToLower();
}

static FString NormalizeSemanticTypeToken(const FString& Type)
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

static bool IsSemanticBoolType(const FString& Type)
{
	const FString Token = NormalizeSemanticTypeToken(Type);
	return Token == TEXT("bool") || Token == TEXT("boolean");
}

static bool IsSemanticIntegerType(const FString& Type)
{
	const FString Token = NormalizeSemanticTypeToken(Type);
	return Token == TEXT("int") || Token == TEXT("integer") || Token == TEXT("int32") || Token == TEXT("int64") || Token == TEXT("byte");
}

static bool IsSemanticNumericType(const FString& Type)
{
	const FString Token = NormalizeSemanticTypeToken(Type);
	return IsSemanticIntegerType(Type) || Token == TEXT("float") || Token == TEXT("double") || Token == TEXT("real") || Token == TEXT("number");
}

static bool AreSemanticTypesCompatible(const FString& Left, const FString& Right)
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

static FString NormalizeTypeLookupKey(const FString& Name)
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

static const UStruct* TryResolveStructByTypeName(const FString& TypeName)
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

static FString StatementPatternName(EBlueprintHelperGraphStatementKind Kind)
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

static FString ExpressionPatternName(EBlueprintHelperGraphExpressionKind Kind)
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

static FBlueprintHelperGraphResolvedTarget ResolveTargetString(
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

	if (ExpressionKind == EBlueprintHelperGraphExpressionKind::Ref)
	{
		Target.Kind = EBlueprintHelperGraphTargetKind::Temporary;
		Target.Member = Target.Raw;
		Target.bVerifiedByContext = true;
		return Target;
	}

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

	if (ExpressionKind == EBlueprintHelperGraphExpressionKind::GetProperty
		|| (bHasOwner && (Context.IsComponent(Owner) || Context.IsVariable(Owner)))
		|| (StatementKind == EBlueprintHelperGraphStatementKind::Set && bHasOwner)
		|| (ExpressionKind == EBlueprintHelperGraphExpressionKind::Get && bHasOwner))
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

static void RegisterSymbol(
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

static void AddUnverifiedTargetDiagnostic(
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

static bool FindSymbolInScopes(
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
}

bool FBlueprintHelperGraphResolvedTarget::IsResolved() const
{
	return Kind != EBlueprintHelperGraphTargetKind::Unknown && !Raw.IsEmpty();
}

FBlueprintHelperGraphSemanticContext FBlueprintHelperGraphSemanticContext::FromBlueprint(const UBlueprint* Blueprint)
{
	FBlueprintHelperGraphSemanticContext Context;
	if (!Blueprint)
	{
		return Context;
	}

	auto AddTypedName = [&Context](TSet<FString>& Names, const FString& Name, const FString& Type)
	{
		const FString CleanName = Name.TrimStartAndEnd();
		if (CleanName.IsEmpty())
		{
			return;
		}
		Names.Add(NormalizeSymbolKey(CleanName));
		if (!Type.IsEmpty())
		{
			Context.TargetTypes.Add(NormalizeSymbolKey(CleanName), Type);
		}
	};

	auto AddTargetStruct = [&Context](const FString& Name, const UStruct* Struct)
	{
		const FString CleanName = Name.TrimStartAndEnd();
		if (!CleanName.IsEmpty() && Struct)
		{
			Context.TargetStructs.Add(NormalizeSymbolKey(CleanName), Struct);
		}
	};

	auto ResolvePropertyStruct = [](const FProperty* Property) -> const UStruct*
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			return StructProperty->Struct;
		}
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			return ObjectProperty->PropertyClass;
		}
		if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
		{
			return ClassProperty->MetaClass;
		}
		return nullptr;
	};

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		AddTypedName(Context.VariableNames, Variable.VarName.ToString(), Variable.VarType.PinCategory.ToString());
		if (UObject* TypeObject = Variable.VarType.PinSubCategoryObject.Get())
		{
			AddTargetStruct(Variable.VarName.ToString(), Cast<UStruct>(TypeObject));
		}
	}

	if (Blueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!Node)
			{
				continue;
			}
			const FString ComponentName = Node->GetVariableName().ToString();
			const FString ComponentType = Node->ComponentClass ? Node->ComponentClass->GetName() : FString();
			AddTypedName(Context.ComponentNames, ComponentName, ComponentType);
			AddTypedName(Context.VariableNames, ComponentName, ComponentType);
			AddTargetStruct(ComponentName, Node->ComponentClass);
		}
	}

	auto AddClassFunctions = [&Context, &AddTypedName](const UClass* Class)
	{
		if (!Class)
		{
			return;
		}
		for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			const UFunction* Function = *FunctionIt;
			if (!Function)
			{
				continue;
			}

			AddTypedName(Context.FunctionNames, Function->GetName(), TEXT("function"));
			const FString DisplayName = Function->GetDisplayNameText().ToString();
			if (!DisplayName.IsEmpty())
			{
				AddTypedName(Context.FunctionNames, DisplayName, TEXT("function"));
			}
		}
	};

	auto AddClassMembers = [&Context, &AddTypedName, &AddTargetStruct, &ResolvePropertyStruct, &AddClassFunctions](const UClass* Class)
	{
		if (!Class)
		{
			return;
		}
		AddClassFunctions(Class);
		for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			AddTypedName(Context.VariableNames, PropertyIt->GetName(), PropertyIt->GetCPPType());
			AddTargetStruct(PropertyIt->GetName(), ResolvePropertyStruct(*PropertyIt));
		}
	};

	AddClassMembers(Blueprint->SkeletonGeneratedClass);
	AddClassMembers(Blueprint->GeneratedClass);
	AddClassMembers(Blueprint->ParentClass);
	AddClassFunctions(UKismetSystemLibrary::StaticClass());
	AddClassFunctions(UKismetMathLibrary::StaticClass());
	return Context;
}

bool FBlueprintHelperGraphSemanticContext::HasVariables() const
{
	return VariableNames.Num() > 0 || ComponentNames.Num() > 0 || FunctionNames.Num() > 0;
}

bool FBlueprintHelperGraphSemanticContext::IsVariable(const FString& Name) const
{
	return VariableNames.Contains(NormalizeSymbolKey(Name));
}

bool FBlueprintHelperGraphSemanticContext::IsComponent(const FString& Name) const
{
	return ComponentNames.Contains(NormalizeSymbolKey(Name));
}

bool FBlueprintHelperGraphSemanticContext::IsFunction(const FString& Name) const
{
	return FunctionNames.Contains(NormalizeSymbolKey(Name));
}

FString FBlueprintHelperGraphSemanticContext::FindTargetType(const FString& Name) const
{
	if (const FString* Type = TargetTypes.Find(NormalizeSymbolKey(Name)))
	{
		return *Type;
	}
	return FString();
}

bool FBlueprintHelperGraphSemanticContext::TryFindTargetStruct(const FString& Name, const UStruct*& OutStruct) const
{
	OutStruct = nullptr;
	if (const UStruct* const* StructPtr = TargetStructs.Find(NormalizeSymbolKey(Name)))
	{
		OutStruct = *StructPtr;
		return OutStruct != nullptr;
	}
	return false;
}

bool FBlueprintHelperGraphSemanticContext::HasMemberFunction(const FString& OwnerName, const FString& FunctionName) const
{
	const UStruct* Struct = nullptr;
	if (!TryFindTargetStruct(OwnerName, Struct) || !Struct)
	{
		return false;
	}

	const UClass* Class = Cast<UClass>(Struct);
	return Class && Class->FindFunctionByName(FName(*FunctionName)) != nullptr;
}

bool FBlueprintHelperGraphSemanticContext::HasPropertyPath(const FString& OwnerName, const FString& PropertyPath, FString& OutType) const
{
	OutType.Reset();
	const UStruct* CurrentStruct = nullptr;
	if (!TryFindTargetStruct(OwnerName, CurrentStruct) || !CurrentStruct)
	{
		CurrentStruct = TryResolveStructByTypeName(FindTargetType(OwnerName));
		if (!CurrentStruct)
		{
			return false;
		}
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.Num() == 0)
	{
		return false;
	}

	const FProperty* LastProperty = nullptr;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		if (!CurrentStruct)
		{
			return false;
		}

		LastProperty = FindFProperty<FProperty>(CurrentStruct, FName(*Segments[Index]));
		if (!LastProperty)
		{
			return false;
		}

		if (Index < Segments.Num() - 1)
		{
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(LastProperty))
			{
				CurrentStruct = StructProperty->Struct;
			}
			else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(LastProperty))
			{
				CurrentStruct = ObjectProperty->PropertyClass;
			}
			else
			{
				return false;
			}
		}
	}

	OutType = LastProperty ? LastProperty->GetCPPType() : FString();
	return LastProperty != nullptr;
}

bool FBlueprintHelperGraphSemanticIR::HasErrors() const
{
	return Diagnostics.ContainsByPredicate([](const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase);
	});
}

bool FBlueprintHelperGraphSemanticIR::TryFindSymbol(const FString& Name, FBlueprintHelperGraphSymbol& OutSymbol) const
{
	const FString Key = NormalizeSymbolKey(Name);
	for (const TPair<FString, FBlueprintHelperGraphSymbol>& Pair : Symbols)
	{
		if (Pair.Value.SymbolId == Key)
		{
			OutSymbol = Pair.Value;
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
	const TSharedPtr<FJsonObject>& LogicSpecObject,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	return BuildFromLogicSpec(LogicSpecObject, FBlueprintHelperGraphSemanticContext(), OutIR);
}

bool FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
	const TSharedPtr<FJsonObject>& LogicSpecObject,
	const UBlueprint* Blueprint,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	return BuildFromLogicSpec(LogicSpecObject, FBlueprintHelperGraphSemanticContext::FromBlueprint(Blueprint), OutIR);
}

bool FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
	const TSharedPtr<FJsonObject>& LogicSpecObject,
	const FBlueprintHelperGraphSemanticContext& Context,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	OutIR = FBlueprintHelperGraphSemanticIR();
	if (!LogicSpecObject.IsValid())
	{
		AddDiagnostic(OutIR, TEXT("logic_spec_invalid"), TEXT("$"), TEXT("BlueprintLogicSpec object is invalid."));
		return false;
	}

	LogicSpecObject->TryGetStringField(TEXT("schema"), OutIR.Schema);
	if (!OutIR.Schema.Equals(TEXT("BlueprintLogicSpec.v2"), ESearchCase::IgnoreCase))
	{
		AddDiagnostic(
			OutIR,
			TEXT("logic_spec_schema_unsupported"),
			TEXT("$.schema"),
			FString::Printf(TEXT("Unsupported BlueprintLogicSpec schema: %s."), *OutIR.Schema),
			TEXT("warning"));
	}

	const TArray<TSharedPtr<FJsonValue>>* StatementValues = nullptr;
	if (!LogicSpecObject->TryGetArrayField(TEXT("statements"), StatementValues) || !StatementValues)
	{
		AddDiagnostic(OutIR, TEXT("logic_spec_statements_missing"), TEXT("$.statements"), TEXT("BlueprintLogicSpec.statements must be an array."));
		return false;
	}

	ParseStatementArray(*StatementValues, TEXT("$.statements"), OutIR.Statements, OutIR);
	ResolveSemanticIR(OutIR, Context);
	return !OutIR.HasErrors();
}

void FBlueprintHelperGraphSemanticIRBuilder::ParseStatementArray(
	const TArray<TSharedPtr<FJsonValue>>& StatementValues,
	const FString& Path,
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& OutStatements,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	for (int32 Index = 0; Index < StatementValues.Num(); ++Index)
	{
		const FString StatementPath = FString::Printf(TEXT("%s[%d]"), *Path, Index);
		const TSharedPtr<FJsonObject> StatementObject = StatementValues[Index].IsValid()
			? StatementValues[Index]->AsObject()
			: nullptr;
		TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = ParseStatement(StatementObject, StatementPath, OutIR);
		if (Statement.IsValid())
		{
			OutStatements.Add(Statement);
		}
	}
}

TSharedPtr<FBlueprintHelperGraphStatementIR> FBlueprintHelperGraphSemanticIRBuilder::ParseStatement(
	const TSharedPtr<FJsonObject>& StatementObject,
	const FString& Path,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	if (!StatementObject.IsValid())
	{
		AddDiagnostic(OutIR, TEXT("statement_invalid"), Path, TEXT("Statement must be an object."));
		return nullptr;
	}

	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->Path = Path;
	StatementObject->TryGetStringField(TEXT("id"), Statement->StatementId);
	if (Statement->StatementId.IsEmpty())
	{
		Statement->StatementId = Path;
	}

	FString KindString;
	StatementObject->TryGetStringField(TEXT("kind"), KindString);
	Statement->Kind = ParseStatementKind(KindString);
	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Unknown)
	{
		AddDiagnostic(OutIR, TEXT("statement_kind_unsupported"), Path + TEXT(".kind"), FString::Printf(TEXT("Unsupported statement kind: %s."), *KindString));
		return Statement;
	}

	StatementObject->TryGetStringField(TEXT("target"), Statement->Target);
	StatementObject->TryGetStringField(TEXT("name"), Statement->Name);
	StatementObject->TryGetStringField(TEXT("search_mode"), Statement->SearchMode);
	StatementObject->TryGetStringField(TEXT("ambiguity"), Statement->AmbiguityPolicy);
	StatementObject->TryGetStringField(TEXT("ambiguity_policy"), Statement->AmbiguityPolicy);
	ReadOptionalStringArrayField(StatementObject, TEXT("category_priority"), Statement->CategoryPriority);
	ParseExpressionMap(StatementObject, TEXT("args"), Path + TEXT(".args"), Statement->Args, OutIR);

	if (const TSharedPtr<FJsonValue>* Value = StatementObject->Values.Find(TEXT("value")))
	{
		Statement->Value = ParseExpression(*Value, Path + TEXT(".value"), OutIR);
	}
	if (const TSharedPtr<FJsonValue>* Condition = StatementObject->Values.Find(TEXT("condition")))
	{
		Statement->Condition = ParseExpression(*Condition, Path + TEXT(".condition"), OutIR);
	}

	const TArray<TSharedPtr<FJsonValue>>* ThenValues = nullptr;
	if (StatementObject->TryGetArrayField(TEXT("then"), ThenValues) && ThenValues)
	{
		ParseStatementArray(*ThenValues, Path + TEXT(".then"), Statement->ThenStatements, OutIR);
	}

	const TArray<TSharedPtr<FJsonValue>>* ElseValues = nullptr;
	if (StatementObject->TryGetArrayField(TEXT("else"), ElseValues) && ElseValues)
	{
		ParseStatementArray(*ElseValues, Path + TEXT(".else"), Statement->ElseStatements, OutIR);
	}

	return Statement;
}

TSharedPtr<FBlueprintHelperGraphExpressionIR> FBlueprintHelperGraphSemanticIRBuilder::ParseExpression(
	const TSharedPtr<FJsonValue>& ExpressionValue,
	const FString& Path,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Expression = MakeShared<FBlueprintHelperGraphExpressionIR>();
	Expression->Path = Path;
	Expression->ExpressionId = Path;

	if (!ExpressionValue.IsValid())
	{
		Expression->Kind = EBlueprintHelperGraphExpressionKind::Literal;
		return Expression;
	}

	if (ExpressionValue->Type != EJson::Object)
	{
		Expression->Kind = EBlueprintHelperGraphExpressionKind::Literal;
		Expression->LiteralValue = JsonValueToString(ExpressionValue);
		Expression->Type = JsonValueToSemanticType(ExpressionValue);
		return Expression;
	}

	const TSharedPtr<FJsonObject> ExpressionObject = ExpressionValue->AsObject();
	if (!ExpressionObject.IsValid())
	{
		AddDiagnostic(OutIR, TEXT("expression_invalid"), Path, TEXT("Expression object is invalid."));
		return Expression;
	}

	ExpressionObject->TryGetStringField(TEXT("id"), Expression->ExpressionId);
	FString KindString;
	ExpressionObject->TryGetStringField(TEXT("kind"), KindString);
	Expression->Kind = ParseExpressionKind(KindString);
	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Unknown)
	{
		AddDiagnostic(OutIR, TEXT("expression_kind_unsupported"), Path + TEXT(".kind"), FString::Printf(TEXT("Unsupported expression kind: %s."), *KindString));
	}

	ExpressionObject->TryGetStringField(TEXT("target"), Expression->Target);
	ExpressionObject->TryGetStringField(TEXT("name"), Expression->Name);
	ExpressionObject->TryGetStringField(TEXT("search_mode"), Expression->SearchMode);
	ExpressionObject->TryGetStringField(TEXT("ambiguity"), Expression->AmbiguityPolicy);
	ExpressionObject->TryGetStringField(TEXT("ambiguity_policy"), Expression->AmbiguityPolicy);
	ReadOptionalStringArrayField(ExpressionObject, TEXT("category_priority"), Expression->CategoryPriority);
	ExpressionObject->TryGetStringField(TEXT("type"), Expression->Type);
	if (Expression->Type.IsEmpty())
	{
		ExpressionObject->TryGetStringField(TEXT("value_type"), Expression->Type);
	}
	ExpressionObject->TryGetStringField(TEXT("op"), Expression->Operator);
	if (Expression->Operator.IsEmpty())
	{
		ExpressionObject->TryGetStringField(TEXT("operator"), Expression->Operator);
	}

	if (const TSharedPtr<FJsonValue>* LiteralValue = ExpressionObject->Values.Find(TEXT("value")))
	{
		Expression->LiteralValue = JsonValueToString(*LiteralValue);
		if (Expression->Type.IsEmpty())
		{
			Expression->Type = JsonValueToSemanticType(*LiteralValue);
		}
	}
	if (const TSharedPtr<FJsonValue>* Left = ExpressionObject->Values.Find(TEXT("left")))
	{
		Expression->Left = ParseExpression(*Left, Path + TEXT(".left"), OutIR);
	}
	if (const TSharedPtr<FJsonValue>* Right = ExpressionObject->Values.Find(TEXT("right")))
	{
		Expression->Right = ParseExpression(*Right, Path + TEXT(".right"), OutIR);
	}

	ParseExpressionMap(ExpressionObject, TEXT("args"), Path + TEXT(".args"), Expression->Args, OutIR);
	if (const TSharedPtr<FJsonValue>* Condition = ExpressionObject->Values.Find(TEXT("condition")))
	{
		Expression->Args.Add(TEXT("condition"), ParseExpression(*Condition, Path + TEXT(".condition"), OutIR));
	}
	else if (const TSharedPtr<FJsonValue>* Index = ExpressionObject->Values.Find(TEXT("index")))
	{
		Expression->Args.Add(TEXT("condition"), ParseExpression(*Index, Path + TEXT(".index"), OutIR));
	}

	const TArray<TSharedPtr<FJsonValue>>* OptionValues = nullptr;
	if (ExpressionObject->TryGetArrayField(TEXT("options"), OptionValues) && OptionValues)
	{
		for (int32 Index = 0; Index < OptionValues->Num(); ++Index)
		{
			Expression->Options.Add(ParseExpression((*OptionValues)[Index], FString::Printf(TEXT("%s.options[%d]"), *Path, Index), OutIR));
		}
	}

	return Expression;
}

void FBlueprintHelperGraphSemanticIRBuilder::ParseExpressionMap(
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	const FString& Path,
	TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& OutExpressions,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	const TSharedPtr<FJsonObject>* ArgsObject = nullptr;
	if (!Object.IsValid() || !Object->TryGetObjectField(FieldName, ArgsObject) || !ArgsObject || !ArgsObject->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ArgsObject)->Values)
	{
		OutExpressions.Add(Pair.Key, ParseExpression(Pair.Value, Path + TEXT(".") + Pair.Key, OutIR));
	}
}

void FBlueprintHelperGraphSemanticIRBuilder::ResolveSemanticIR(
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FBlueprintHelperGraphSemanticContext& Context)
{
	TArray<TMap<FString, FBlueprintHelperGraphSymbol>> ScopeStack;
	ScopeStack.AddDefaulted();
	ResolveStatementArray(OutIR.Statements, OutIR, Context, ScopeStack);
}

void FBlueprintHelperGraphSemanticIRBuilder::ResolveStatementArray(
	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FBlueprintHelperGraphSemanticContext& Context,
	TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack)
{
	for (const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement : Statements)
	{
		ResolveStatement(Statement, OutIR, Context, ScopeStack);
	}
}

void FBlueprintHelperGraphSemanticIRBuilder::ResolveStatement(
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FBlueprintHelperGraphSemanticContext& Context,
	TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack)
{
	if (!Statement.IsValid())
	{
		return;
	}

	Statement->PatternName = StatementPatternName(Statement->Kind);

	switch (Statement->Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		if (Statement->Target.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(OutIR, TEXT("statement_target_missing"), Statement->Path + TEXT(".target"), TEXT("call statement requires target."));
		}
		Statement->ResolvedTarget = ResolveTargetString(Statement->Target, Statement->Kind, EBlueprintHelperGraphExpressionKind::Unknown, Context);
		AddUnverifiedTargetDiagnostic(OutIR, Context, Statement->ResolvedTarget, Statement->Path + TEXT(".target"));
		break;

	case EBlueprintHelperGraphStatementKind::Set:
		if (Statement->Target.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(OutIR, TEXT("statement_target_missing"), Statement->Path + TEXT(".target"), TEXT("set statement requires target."));
		}
		Statement->ResolvedTarget = ResolveTargetString(Statement->Target, Statement->Kind, EBlueprintHelperGraphExpressionKind::Unknown, Context);
		AddUnverifiedTargetDiagnostic(OutIR, Context, Statement->ResolvedTarget, Statement->Path + TEXT(".target"));
		break;

	case EBlueprintHelperGraphStatementKind::Branch:
		if (!Statement->Condition.IsValid())
		{
			AddDiagnostic(OutIR, TEXT("branch_condition_missing"), Statement->Path + TEXT(".condition"), TEXT("branch statement requires condition."));
		}
		break;

	case EBlueprintHelperGraphStatementKind::Let:
		Statement->ResultSymbolName = Statement->Name;
		if (Statement->Name.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(OutIR, TEXT("let_name_missing"), Statement->Path + TEXT(".name"), TEXT("let statement requires name."));
		}
		if (!Statement->Value.IsValid())
		{
			AddDiagnostic(OutIR, TEXT("let_value_missing"), Statement->Path + TEXT(".value"), TEXT("let statement requires value."));
		}
		break;

	case EBlueprintHelperGraphStatementKind::Return:
	case EBlueprintHelperGraphStatementKind::Unknown:
	default:
		break;
	}

	for (TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Statement->Args)
	{
		ResolveExpression(ArgPair.Value, OutIR, Context, ScopeStack);
	}
	ResolveExpression(Statement->Value, OutIR, Context, ScopeStack);
	ResolveExpression(Statement->Condition, OutIR, Context, ScopeStack);
	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Branch &&
		Statement->Condition.IsValid() &&
		!Statement->Condition->Type.IsEmpty() &&
		!IsSemanticBoolType(Statement->Condition->Type))
	{
		AddDiagnostic(
			OutIR,
			TEXT("branch_condition_type_mismatch"),
			Statement->Path + TEXT(".condition"),
			FString::Printf(TEXT("branch condition must be bool, got %s."), *Statement->Condition->Type));
	}

	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Let)
	{
		RegisterSymbol(OutIR, Statement->Name, Statement->StatementId, Statement->Value, Statement->Path + TEXT(".name"), ScopeStack);
	}

	if (Statement->ThenStatements.Num() > 0)
	{
		ScopeStack.AddDefaulted();
		ResolveStatementArray(Statement->ThenStatements, OutIR, Context, ScopeStack);
		ScopeStack.Pop();
	}
	if (Statement->ElseStatements.Num() > 0)
	{
		ScopeStack.AddDefaulted();
		ResolveStatementArray(Statement->ElseStatements, OutIR, Context, ScopeStack);
		ScopeStack.Pop();
	}
}

void FBlueprintHelperGraphSemanticIRBuilder::ResolveExpression(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	FBlueprintHelperGraphSemanticIR& OutIR,
	const FBlueprintHelperGraphSemanticContext& Context,
	TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& ScopeStack)
{
	if (!Expression.IsValid())
	{
		return;
	}

	Expression->PatternName = ExpressionPatternName(Expression->Kind);

	switch (Expression->Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Get:
	case EBlueprintHelperGraphExpressionKind::GetProperty:
	case EBlueprintHelperGraphExpressionKind::Call:
		if (Expression->Target.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(OutIR, TEXT("expression_target_missing"), Expression->Path + TEXT(".target"), FString::Printf(TEXT("%s expression requires target."), *Expression->PatternName));
		}
		Expression->ResolvedTarget = ResolveTargetString(Expression->Target, EBlueprintHelperGraphStatementKind::Unknown, Expression->Kind, Context);
		AddUnverifiedTargetDiagnostic(OutIR, Context, Expression->ResolvedTarget, Expression->Path + TEXT(".target"));
		if (Expression->Kind != EBlueprintHelperGraphExpressionKind::Call && Expression->Type.IsEmpty())
		{
			Expression->Type = Expression->ResolvedTarget.Type;
		}
		break;

	case EBlueprintHelperGraphExpressionKind::Ref:
		if (Expression->Name.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(OutIR, TEXT("ref_name_missing"), Expression->Path + TEXT(".name"), TEXT("ref expression requires name."));
		}
		else
		{
			FBlueprintHelperGraphSymbol Symbol;
			if (FindSymbolInScopes(Expression->Name, ScopeStack, Symbol))
			{
				Expression->ResolvedTarget = ResolveTargetString(Expression->Name, EBlueprintHelperGraphStatementKind::Unknown, Expression->Kind, Context);
				Expression->ResolvedTarget.Type = Symbol.Type;
				if (Expression->Type.IsEmpty())
				{
					Expression->Type = Symbol.Type;
				}
			}
			else
			{
				AddDiagnostic(OutIR, TEXT("ref_symbol_not_found"), Expression->Path + TEXT(".name"), FString::Printf(TEXT("Temporary symbol not found: %s."), *Expression->Name));
			}
		}
		break;

	case EBlueprintHelperGraphExpressionKind::Compare:
		if (Expression->Operator.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(OutIR, TEXT("compare_operator_missing"), Expression->Path + TEXT(".op"), TEXT("compare expression requires op."));
		}
		if (!Expression->Left.IsValid())
		{
			AddDiagnostic(OutIR, TEXT("compare_left_missing"), Expression->Path + TEXT(".left"), TEXT("compare expression requires left."));
		}
		if (!Expression->Right.IsValid())
		{
			AddDiagnostic(OutIR, TEXT("compare_right_missing"), Expression->Path + TEXT(".right"), TEXT("compare expression requires right."));
		}
		Expression->Type = TEXT("bool");
		break;

	case EBlueprintHelperGraphExpressionKind::Select:
		if (Expression->Options.Num() == 0)
		{
			AddDiagnostic(OutIR, TEXT("select_options_missing"), Expression->Path + TEXT(".options"), TEXT("select expression requires options."));
		}
		break;

	case EBlueprintHelperGraphExpressionKind::MakeStruct:
		if (Expression->Type.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(OutIR, TEXT("make_struct_type_missing"), Expression->Path + TEXT(".type"), TEXT("make_struct expression requires type."));
		}
		break;

	case EBlueprintHelperGraphExpressionKind::Literal:
	case EBlueprintHelperGraphExpressionKind::Unknown:
	default:
		break;
	}

	for (TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression->Args)
	{
		ResolveExpression(ArgPair.Value, OutIR, Context, ScopeStack);
	}
	for (TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option : Expression->Options)
	{
		ResolveExpression(Option, OutIR, Context, ScopeStack);
	}
	ResolveExpression(Expression->Left, OutIR, Context, ScopeStack);
	ResolveExpression(Expression->Right, OutIR, Context, ScopeStack);

	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Select && Expression->Type.IsEmpty())
	{
		for (const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option : Expression->Options)
		{
			if (Option.IsValid() && !Option->Type.IsEmpty())
			{
				Expression->Type = Option->Type;
				break;
			}
		}
	}
	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Compare &&
		Expression->Left.IsValid() &&
		Expression->Right.IsValid() &&
		!AreSemanticTypesCompatible(Expression->Left->Type, Expression->Right->Type))
	{
		AddDiagnostic(
			OutIR,
			TEXT("compare_operand_type_mismatch"),
			Expression->Path,
			FString::Printf(TEXT("compare operands have incompatible types: %s vs %s."), *Expression->Left->Type, *Expression->Right->Type));
	}
	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Select)
	{
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>* ConditionExpressionPtr = Expression->Args.Find(TEXT("condition"));
		const TSharedPtr<FBlueprintHelperGraphExpressionIR> ConditionExpression = ConditionExpressionPtr ? *ConditionExpressionPtr : nullptr;
		if (ConditionExpression.IsValid() &&
			!ConditionExpression->Type.IsEmpty() &&
			!IsSemanticBoolType(ConditionExpression->Type) &&
			!IsSemanticIntegerType(ConditionExpression->Type))
		{
			AddDiagnostic(
				OutIR,
				TEXT("select_condition_type_mismatch"),
				Expression->Path + TEXT(".condition"),
				FString::Printf(TEXT("select condition/index must be bool or integer-compatible, got %s."), *ConditionExpression->Type));
		}

		for (int32 OptionIndex = 0; OptionIndex < Expression->Options.Num(); ++OptionIndex)
		{
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression->Options[OptionIndex];
			if (Option.IsValid() &&
				!Expression->Type.IsEmpty() &&
				!Option->Type.IsEmpty() &&
				!AreSemanticTypesCompatible(Expression->Type, Option->Type))
			{
				AddDiagnostic(
					OutIR,
					TEXT("select_option_type_mismatch"),
					FString::Printf(TEXT("%s.options[%d]"), *Expression->Path, OptionIndex),
					FString::Printf(TEXT("select option type %s does not match inferred select type %s."), *Option->Type, *Expression->Type));
			}
		}
	}
}
