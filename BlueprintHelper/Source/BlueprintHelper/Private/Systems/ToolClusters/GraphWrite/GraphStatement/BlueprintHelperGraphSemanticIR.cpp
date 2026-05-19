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
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.h"
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
		Names.Add(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(CleanName));
		if (!Type.IsEmpty())
		{
			Context.TargetTypes.Add(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(CleanName), Type);
		}
	};

	auto AddTargetStruct = [&Context](const FString& Name, const UStruct* Struct)
	{
		const FString CleanName = Name.TrimStartAndEnd();
		if (!CleanName.IsEmpty() && Struct)
		{
			Context.TargetStructs.Add(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(CleanName), Struct);
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
	return VariableNames.Contains(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(Name));
}

bool FBlueprintHelperGraphSemanticContext::IsComponent(const FString& Name) const
{
	return ComponentNames.Contains(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(Name));
}

bool FBlueprintHelperGraphSemanticContext::IsFunction(const FString& Name) const
{
	return FunctionNames.Contains(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(Name));
}

FString FBlueprintHelperGraphSemanticContext::FindTargetType(const FString& Name) const
{
	if (const FString* Type = TargetTypes.Find(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(Name)))
	{
		return *Type;
	}
	return FString();
}

bool FBlueprintHelperGraphSemanticContext::TryFindTargetStruct(const FString& Name, const UStruct*& OutStruct) const
{
	OutStruct = nullptr;
	if (const UStruct* const* StructPtr = TargetStructs.Find(FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(Name)))
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
		CurrentStruct = FBlueprintHelperGraphSemanticIRUtils::TryResolveStructByTypeName(FindTargetType(OwnerName));
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
	const FString Key = FBlueprintHelperGraphSemanticIRUtils::NormalizeSymbolKey(Name);
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
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("logic_spec_invalid"), TEXT("$"), TEXT("BlueprintLogicSpec object is invalid."));
		return false;
	}

	LogicSpecObject->TryGetStringField(TEXT("schema"), OutIR.Schema);
	if (!OutIR.Schema.Equals(TEXT("BlueprintLogicSpec.v2"), ESearchCase::IgnoreCase))
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
			OutIR,
			TEXT("logic_spec_schema_unsupported"),
			TEXT("$.schema"),
			FString::Printf(TEXT("Unsupported BlueprintLogicSpec schema: %s."), *OutIR.Schema),
			TEXT("warning"));
	}

	const TArray<TSharedPtr<FJsonValue>>* StatementValues = nullptr;
	if (!LogicSpecObject->TryGetArrayField(TEXT("statements"), StatementValues) || !StatementValues)
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("logic_spec_statements_missing"), TEXT("$.statements"), TEXT("BlueprintLogicSpec.statements must be an array."));
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
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("statement_invalid"), Path, TEXT("Statement must be an object."));
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
	Statement->Kind = FBlueprintHelperGraphSemanticIRUtils::ParseStatementKind(KindString);
	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Unknown)
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("statement_kind_unsupported"), Path + TEXT(".kind"), FString::Printf(TEXT("Unsupported statement kind: %s."), *KindString));
		return Statement;
	}

	StatementObject->TryGetStringField(TEXT("target"), Statement->Target);
	StatementObject->TryGetStringField(TEXT("name"), Statement->Name);
	StatementObject->TryGetStringField(TEXT("resolved_stable_id"), Statement->ResolvedCallFunctionStableId);
	StatementObject->TryGetStringField(TEXT("search_mode"), Statement->SearchMode);
	StatementObject->TryGetStringField(TEXT("ambiguity"), Statement->AmbiguityPolicy);
	StatementObject->TryGetStringField(TEXT("ambiguity_policy"), Statement->AmbiguityPolicy);
	FBlueprintHelperGraphSemanticIRUtils::ReadOptionalStringArrayField(StatementObject, TEXT("category_priority"), Statement->CategoryPriority);
	ParseExpressionMap(StatementObject, TEXT("args"), Path + TEXT(".args"), Statement->Args, OutIR);
	if (const TSharedPtr<FJsonValue>* TargetObject = StatementObject->Values.Find(TEXT("target_object")))
	{
		Statement->TargetObject = ParseExpression(*TargetObject, Path + TEXT(".target_object"), OutIR);
	}

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
		Expression->LiteralValue = FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(ExpressionValue);
		Expression->Type = FBlueprintHelperGraphSemanticIRUtils::JsonValueToSemanticType(ExpressionValue);
		return Expression;
	}

	const TSharedPtr<FJsonObject> ExpressionObject = ExpressionValue->AsObject();
	if (!ExpressionObject.IsValid())
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("expression_invalid"), Path, TEXT("Expression object is invalid."));
		return Expression;
	}

	ExpressionObject->TryGetStringField(TEXT("id"), Expression->ExpressionId);
	FString KindString;
	ExpressionObject->TryGetStringField(TEXT("kind"), KindString);
	Expression->Kind = FBlueprintHelperGraphSemanticIRUtils::ParseExpressionKind(KindString);
	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Unknown)
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("expression_kind_unsupported"), Path + TEXT(".kind"), FString::Printf(TEXT("Unsupported expression kind: %s."), *KindString));
	}

	ExpressionObject->TryGetStringField(TEXT("target"), Expression->Target);
	ExpressionObject->TryGetStringField(TEXT("name"), Expression->Name);
	ExpressionObject->TryGetStringField(TEXT("resolved_stable_id"), Expression->ResolvedCallFunctionStableId);
	ExpressionObject->TryGetStringField(TEXT("search_mode"), Expression->SearchMode);
	ExpressionObject->TryGetStringField(TEXT("ambiguity"), Expression->AmbiguityPolicy);
	ExpressionObject->TryGetStringField(TEXT("ambiguity_policy"), Expression->AmbiguityPolicy);
	FBlueprintHelperGraphSemanticIRUtils::ReadOptionalStringArrayField(ExpressionObject, TEXT("category_priority"), Expression->CategoryPriority);
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
		Expression->LiteralValue = FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(*LiteralValue);
		if (Expression->Type.IsEmpty())
		{
			Expression->Type = FBlueprintHelperGraphSemanticIRUtils::JsonValueToSemanticType(*LiteralValue);
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
	if (const TSharedPtr<FJsonValue>* TargetObject = ExpressionObject->Values.Find(TEXT("target_object")))
	{
		Expression->TargetObject = ParseExpression(*TargetObject, Path + TEXT(".target_object"), OutIR);
	}
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

	Statement->PatternName = FBlueprintHelperGraphSemanticIRUtils::StatementPatternName(Statement->Kind);

	switch (Statement->Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		if (Statement->Target.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("statement_target_missing"), Statement->Path + TEXT(".target"), TEXT("call statement requires target."));
		}
		Statement->ResolvedTarget = FBlueprintHelperGraphSemanticIRUtils::ResolveTargetString(Statement->Target, Statement->Kind, EBlueprintHelperGraphExpressionKind::Unknown, Context);
		FBlueprintHelperGraphSemanticIRUtils::AddUnverifiedTargetDiagnostic(OutIR, Context, Statement->ResolvedTarget, Statement->Path + TEXT(".target"));
		break;

	case EBlueprintHelperGraphStatementKind::Set:
		if (Statement->Target.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("statement_target_missing"), Statement->Path + TEXT(".target"), TEXT("set statement requires target."));
		}
		Statement->ResolvedTarget = FBlueprintHelperGraphSemanticIRUtils::ResolveTargetString(Statement->Target, Statement->Kind, EBlueprintHelperGraphExpressionKind::Unknown, Context);
		FBlueprintHelperGraphSemanticIRUtils::AddUnverifiedTargetDiagnostic(OutIR, Context, Statement->ResolvedTarget, Statement->Path + TEXT(".target"));
		break;

	case EBlueprintHelperGraphStatementKind::Branch:
		if (!Statement->Condition.IsValid())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("branch_condition_missing"), Statement->Path + TEXT(".condition"), TEXT("branch statement requires condition."));
		}
		break;

	case EBlueprintHelperGraphStatementKind::Let:
		Statement->ResultSymbolName = Statement->Name;
		if (Statement->Name.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("let_name_missing"), Statement->Path + TEXT(".name"), TEXT("let statement requires name."));
		}
		if (!Statement->Value.IsValid())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("let_value_missing"), Statement->Path + TEXT(".value"), TEXT("let statement requires value."));
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
	ResolveExpression(Statement->TargetObject, OutIR, Context, ScopeStack);
	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Branch &&
		Statement->Condition.IsValid() &&
		!Statement->Condition->Type.IsEmpty() &&
		!FBlueprintHelperGraphSemanticIRUtils::IsSemanticBoolType(Statement->Condition->Type))
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
			OutIR,
			TEXT("branch_condition_type_mismatch"),
			Statement->Path + TEXT(".condition"),
			FString::Printf(TEXT("branch condition must be bool, got %s."), *Statement->Condition->Type));
	}

	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Let)
	{
		FBlueprintHelperGraphSemanticIRUtils::RegisterSymbol(OutIR, Statement->Name, Statement->StatementId, Statement->Value, Statement->Path + TEXT(".name"), ScopeStack);
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

	Expression->PatternName = FBlueprintHelperGraphSemanticIRUtils::ExpressionPatternName(Expression->Kind);

	switch (Expression->Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Get:
	case EBlueprintHelperGraphExpressionKind::GetProperty:
	case EBlueprintHelperGraphExpressionKind::Call:
		if (Expression->Target.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("expression_target_missing"), Expression->Path + TEXT(".target"), FString::Printf(TEXT("%s expression requires target."), *Expression->PatternName));
		}
		Expression->ResolvedTarget = FBlueprintHelperGraphSemanticIRUtils::ResolveTargetString(Expression->Target, EBlueprintHelperGraphStatementKind::Unknown, Expression->Kind, Context);
		FBlueprintHelperGraphSemanticIRUtils::AddUnverifiedTargetDiagnostic(OutIR, Context, Expression->ResolvedTarget, Expression->Path + TEXT(".target"));
		if (Expression->Kind != EBlueprintHelperGraphExpressionKind::Call && Expression->Type.IsEmpty())
		{
			Expression->Type = Expression->ResolvedTarget.Type;
		}
		break;

	case EBlueprintHelperGraphExpressionKind::Ref:
		if (Expression->Name.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("ref_name_missing"), Expression->Path + TEXT(".name"), TEXT("ref expression requires name."));
		}
		else
		{
			FBlueprintHelperGraphSymbol Symbol;
			if (FBlueprintHelperGraphSemanticIRUtils::FindSymbolInScopes(Expression->Name, ScopeStack, Symbol))
			{
				Expression->ResolvedTarget = FBlueprintHelperGraphSemanticIRUtils::ResolveTargetString(Expression->Name, EBlueprintHelperGraphStatementKind::Unknown, Expression->Kind, Context);
				Expression->ResolvedTarget.Type = Symbol.Type;
				if (Expression->Type.IsEmpty())
				{
					Expression->Type = Symbol.Type;
				}
			}
			else
			{
				FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("ref_symbol_not_found"), Expression->Path + TEXT(".name"), FString::Printf(TEXT("Temporary symbol not found: %s."), *Expression->Name));
			}
		}
		break;

	case EBlueprintHelperGraphExpressionKind::Compare:
		if (Expression->Operator.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("compare_operator_missing"), Expression->Path + TEXT(".op"), TEXT("compare expression requires op."));
		}
		if (!Expression->Left.IsValid())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("compare_left_missing"), Expression->Path + TEXT(".left"), TEXT("compare expression requires left."));
		}
		if (!Expression->Right.IsValid())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("compare_right_missing"), Expression->Path + TEXT(".right"), TEXT("compare expression requires right."));
		}
		Expression->Type = TEXT("bool");
		break;

	case EBlueprintHelperGraphExpressionKind::Select:
		if (Expression->Options.Num() == 0)
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("select_options_missing"), Expression->Path + TEXT(".options"), TEXT("select expression requires options."));
		}
		break;

	case EBlueprintHelperGraphExpressionKind::MakeStruct:
		if (Expression->Type.TrimStartAndEnd().IsEmpty())
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(OutIR, TEXT("make_struct_type_missing"), Expression->Path + TEXT(".type"), TEXT("make_struct expression requires type."));
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
	ResolveExpression(Expression->TargetObject, OutIR, Context, ScopeStack);

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
		!FBlueprintHelperGraphSemanticIRUtils::AreSemanticTypesCompatible(Expression->Left->Type, Expression->Right->Type))
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
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
			!FBlueprintHelperGraphSemanticIRUtils::IsSemanticBoolType(ConditionExpression->Type) &&
			!FBlueprintHelperGraphSemanticIRUtils::IsSemanticIntegerType(ConditionExpression->Type))
		{
			FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
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
				!FBlueprintHelperGraphSemanticIRUtils::AreSemanticTypesCompatible(Expression->Type, Option->Type))
			{
				FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
					OutIR,
					TEXT("select_option_type_mismatch"),
					FString::Printf(TEXT("%s.options[%d]"), *Expression->Path, OptionIndex),
					FString::Printf(TEXT("select option type %s does not match inferred select type %s."), *Option->Type, *Expression->Type));
			}
		}
	}
}
