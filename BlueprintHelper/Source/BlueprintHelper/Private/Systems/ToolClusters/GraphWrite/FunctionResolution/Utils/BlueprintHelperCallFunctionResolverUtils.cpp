// BlueprintHelper call_function resolver utility helpers implementation.

#include "Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "Kismet/BlueprintMapLibrary.h"
#include "Kismet/BlueprintSetLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetTextLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/GraphWriteFunctionResolutionUtils.h"

FString FBlueprintHelperCallFunctionResolverUtils::NormalizeForCompare(const FString& Value)
{
	FString Result = Value.TrimStartAndEnd();
	return Result.ToLower();
}

FString FBlueprintHelperCallFunctionResolverUtils::NormalizeCompactForCompare(const FString& Value)
{
	const FString Normalized = NormalizeForCompare(Value);
	FString Result;
	Result.Reserve(Normalized.Len());
	for (const TCHAR Character : Normalized)
	{
		if (FChar::IsAlnum(Character))
		{
			Result.AppendChar(Character);
		}
	}
	return Result;
}

bool FBlueprintHelperCallFunctionResolverUtils::CompactEquals(const FString& Left, const FString& Right)
{
	const FString NormalizedLeft = NormalizeCompactForCompare(Left);
	return !NormalizedLeft.IsEmpty() && NormalizedLeft == NormalizeCompactForCompare(Right);
}

FString FBlueprintHelperCallFunctionResolverUtils::GetOwnerClassPath(const UFunction* Function)
{
	const UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
	return OwnerClass ? OwnerClass->GetPathName() : FString();
}

FString FBlueprintHelperCallFunctionResolverUtils::GetOwnerClassName(const UFunction* Function)
{
	const UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
	return OwnerClass ? OwnerClass->GetName() : FString();
}

FString FBlueprintHelperCallFunctionResolverUtils::GetFunctionDisplayName(const UFunction* Function)
{
	if (!Function)
	{
		return FString();
	}

	const FString DisplayName = Function->GetDisplayNameText().ToString();
	return DisplayName.IsEmpty() ? Function->GetName() : DisplayName;
}

FString FBlueprintHelperCallFunctionResolverUtils::GetFunctionCategory(const UFunction* Function)
{
	return Function && Function->HasMetaData(TEXT("Category"))
		? Function->GetMetaData(TEXT("Category"))
		: FString();
}

FString FBlueprintHelperCallFunctionResolverUtils::NormalizeTypeToken(const FString& Type)
{
	FString Token = Type.TrimStartAndEnd().ToLower();
	Token.ReplaceInline(TEXT(" "), TEXT(""));
	Token.ReplaceInline(TEXT("-"), TEXT(""));
	Token.ReplaceInline(TEXT("_"), TEXT(""));
	if ((Token.StartsWith(TEXT("f")) || Token.StartsWith(TEXT("u")) || Token.StartsWith(TEXT("a"))) && Token.Len() > 1)
	{
		Token.RightChopInline(1);
	}
	return Token;
}

UClass* FBlueprintHelperCallFunctionResolverUtils::ResolveClassByTypeName(const FString& TypeName)
{
	const FString Trimmed = TypeName.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return nullptr;
	}
	if (UClass* DirectClass = FindObject<UClass>(nullptr, *Trimmed))
	{
		return DirectClass;
	}
	if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *Trimmed))
	{
		return LoadedClass;
	}
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class)
		{
			continue;
		}

		if (Class->GetPathName().Equals(Trimmed, ESearchCase::IgnoreCase)
			|| Class->GetName().Equals(Trimmed, ESearchCase::IgnoreCase)
			|| Trimmed.EndsWith(TEXT(".") + Class->GetName(), ESearchCase::IgnoreCase))
		{
			return Class;
		}
	}

	const auto MatchesBuiltinClass = [&Trimmed](const UClass* Class)
	{
		return Class
			&& (Class->GetPathName().Equals(Trimmed, ESearchCase::IgnoreCase)
				|| Class->GetName().Equals(Trimmed, ESearchCase::IgnoreCase)
				|| Trimmed.EndsWith(TEXT(".") + Class->GetName(), ESearchCase::IgnoreCase));
	};
	if (MatchesBuiltinClass(UKismetArrayLibrary::StaticClass()))
	{
		return UKismetArrayLibrary::StaticClass();
	}
	if (MatchesBuiltinClass(UBlueprintMapLibrary::StaticClass()))
	{
		return UBlueprintMapLibrary::StaticClass();
	}
	if (MatchesBuiltinClass(UBlueprintSetLibrary::StaticClass()))
	{
		return UBlueprintSetLibrary::StaticClass();
	}
	return nullptr;
}

UScriptStruct* FBlueprintHelperCallFunctionResolverUtils::ResolveStructByTypeName(const FString& TypeName)
{
	const FString Trimmed = TypeName.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return nullptr;
	}
	if (UScriptStruct* DirectStruct = FindObject<UScriptStruct>(nullptr, *Trimmed))
	{
		return DirectStruct;
	}
	if (UScriptStruct* LoadedStruct = LoadObject<UScriptStruct>(nullptr, *Trimmed))
	{
		return LoadedStruct;
	}

	const FString Query = NormalizeTypeToken(Trimmed);
	if (Query == TEXT("vector"))
	{
		return TBaseStructure<FVector>::Get();
	}
	if (Query == TEXT("vector2d"))
	{
		return TBaseStructure<FVector2D>::Get();
	}
	if (Query == TEXT("rotator"))
	{
		return TBaseStructure<FRotator>::Get();
	}
	if (Query == TEXT("transform"))
	{
		return TBaseStructure<FTransform>::Get();
	}
	if (Query == TEXT("linearcolor") || Query == TEXT("color"))
	{
		return TBaseStructure<FLinearColor>::Get();
	}
	return nullptr;
}

bool FBlueprintHelperCallFunctionResolverUtils::IsBlueprintCallableFunction(const UFunction* Function)
{
	return Function &&
		Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure) &&
		!Function->HasAnyFunctionFlags(FUNC_Delegate);
}

bool FBlueprintHelperCallFunctionResolverUtils::IsUsableOwnerClass(const UClass* Class)
{
	return Class && !Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);
}

bool FBlueprintHelperCallFunctionResolverUtils::IsGraphCompatible(const UFunction* Function, UBlueprint* Blueprint, UEdGraph* Graph)
{
	if (!IsBlueprintCallableFunction(Function))
	{
		return false;
	}

	const UClass* OwnerClass = Function->GetOwnerClass();
	if (!IsUsableOwnerClass(OwnerClass))
	{
		return false;
	}

	if (!Graph)
	{
		return true;
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (!K2Schema)
	{
		return false;
	}

	const bool bImpureCall = Function->HasMetaData(TEXT("Latent"))
		|| Function->HasMetaData(TEXT("LatentInfo"))
		|| !Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	if (bImpureCall && !K2Schema->DoesGraphSupportImpureFunctions(Graph))
	{
		return false;
	}

	if (Function->HasMetaData(TEXT("WorldContext")) && !Blueprint && !FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
	{
		return false;
	}

	return true;
}

bool FBlueprintHelperCallFunctionResolverUtils::IsFunctionInputProperty(const FProperty* Property)
{
	if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm))
	{
		return false;
	}

	const bool bOutOnly = Property->HasAnyPropertyFlags(CPF_OutParm)
		&& !Property->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReferenceParm);
	return !bOutOnly;
}

bool FBlueprintHelperCallFunctionResolverUtils::IsFunctionOutputProperty(const FProperty* Property)
{
	return Property && Property->HasAnyPropertyFlags(CPF_Parm)
		&& (Property->HasAnyPropertyFlags(CPF_ReturnParm)
			|| (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ConstParm)));
}

FString FBlueprintHelperCallFunctionResolverUtils::GetPropertySemanticType(const FProperty* Property)
{
	if (!Property)
	{
		return FString();
	}
	if (CastField<FBoolProperty>(Property)) return TEXT("bool");
	if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
	{
		if (Numeric->IsInteger()) return TEXT("int");
		if (Numeric->IsFloatingPoint()) return TEXT("float");
	}
	if (CastField<FStrProperty>(Property)) return TEXT("string");
	if (CastField<FNameProperty>(Property)) return TEXT("name");
	if (CastField<FTextProperty>(Property)) return TEXT("text");
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return StructProperty->Struct ? StructProperty->Struct->GetPathName() : TEXT("struct");
	}
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		return ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetPathName() : TEXT("object");
	}
	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
	{
		return ClassProperty->MetaClass ? ClassProperty->MetaClass->GetPathName() : TEXT("class");
	}
	return Property->GetCPPType();
}

FProperty* FBlueprintHelperCallFunctionResolverUtils::FindInputPropertyByName(const UFunction* Function, const FString& PinName)
{
	if (!Function || PinName.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}

	const FString Wanted = NormalizeForCompare(PinName);
	const FString WantedCompact = NormalizeCompactForCompare(PinName);
	for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!IsFunctionInputProperty(Property))
		{
			continue;
		}

		const FString PropertyName = Property->GetName();
		const FString PropertyDisplayName = Property->GetDisplayNameText().ToString();
		const FString BoolPrefixStrippedName = CastField<FBoolProperty>(Property)
			? UGraphWriteFunctionResolutionUtils::StripLeadingBoolPrefixForCompare(PropertyName)
			: FString();
		if (NormalizeForCompare(PropertyName) == Wanted
			|| NormalizeCompactForCompare(PropertyName) == WantedCompact
			|| (!PropertyDisplayName.IsEmpty()
				&& (NormalizeForCompare(PropertyDisplayName) == Wanted
					|| NormalizeCompactForCompare(PropertyDisplayName) == WantedCompact))
			|| (!BoolPrefixStrippedName.IsEmpty()
				&& (NormalizeForCompare(BoolPrefixStrippedName) == Wanted
					|| NormalizeCompactForCompare(BoolPrefixStrippedName) == WantedCompact)))
		{
			return Property;
		}
	}
	return nullptr;
}

bool FBlueprintHelperCallFunctionResolverUtils::IsNumericSemanticType(const FString& Type)
{
	const FString Token = NormalizeTypeToken(Type);
	return Token == TEXT("int") || Token == TEXT("integer") || Token == TEXT("int32") || Token == TEXT("int64")
		|| Token == TEXT("byte") || Token == TEXT("float") || Token == TEXT("double") || Token == TEXT("real")
		|| Token == TEXT("number");
}

bool FBlueprintHelperCallFunctionResolverUtils::IsSemanticTypeCompatibleWithProperty(const FString& Type, const FProperty* Property)
{
	const FString Token = NormalizeTypeToken(Type);
	if (Token.IsEmpty() || !Property)
	{
		return true;
	}

	if (Token == TEXT("bool") || Token == TEXT("boolean"))
	{
		return CastField<FBoolProperty>(Property) != nullptr
			|| CastField<FStrProperty>(Property) != nullptr
			|| CastField<FTextProperty>(Property) != nullptr;
	}
	if (Token == TEXT("string"))
	{
		return CastField<FStrProperty>(Property) != nullptr
			|| CastField<FTextProperty>(Property) != nullptr
			|| CastField<FNameProperty>(Property) != nullptr;
	}
	if (Token == TEXT("name"))
	{
		return CastField<FNameProperty>(Property) != nullptr
			|| CastField<FStrProperty>(Property) != nullptr
			|| CastField<FTextProperty>(Property) != nullptr;
	}
	if (Token == TEXT("text"))
	{
		return CastField<FTextProperty>(Property) != nullptr
			|| CastField<FStrProperty>(Property) != nullptr;
	}
	if (IsNumericSemanticType(Token))
	{
		const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
		return (Numeric != nullptr && (Numeric->IsInteger() || Numeric->IsFloatingPoint()))
			|| CastField<FStrProperty>(Property) != nullptr
			|| CastField<FTextProperty>(Property) != nullptr;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		UScriptStruct* RequestedStruct = ResolveStructByTypeName(Type);
		return !RequestedStruct || !StructProperty->Struct || StructProperty->Struct == RequestedStruct
			|| NormalizeTypeToken(StructProperty->Struct->GetName()) == Token
			|| NormalizeTypeToken(StructProperty->Struct->GetPathName()) == Token;
	}

	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		UClass* RequestedClass = ResolveClassByTypeName(Type);
		return !RequestedClass || !ObjectProperty->PropertyClass
			|| RequestedClass->IsChildOf(ObjectProperty->PropertyClass)
			|| ObjectProperty->PropertyClass->IsChildOf(RequestedClass);
	}

	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
	{
		UClass* RequestedClass = ResolveClassByTypeName(Type);
		return !RequestedClass || !ClassProperty->MetaClass
			|| RequestedClass->IsChildOf(ClassProperty->MetaClass)
			|| ClassProperty->MetaClass->IsChildOf(RequestedClass);
	}

	return true;
}

bool FBlueprintHelperCallFunctionResolverUtils::IsPinTypeCompatibleWithProperty(const FBlueprintHelperCallFunctionPinType& PinType, const FProperty* Property)
{
	if (!PinType.IsValid())
	{
		return true;
	}
	if (!UGraphWriteFunctionResolutionUtils::IsContainerPinCompatibleWithProperty(PinType, Property))
	{
		return false;
	}
	if (NormalizeTypeToken(PinType.Category) == TEXT("wildcard"))
	{
		return true;
	}
	if (!PinType.ObjectPath.IsEmpty() && !IsSemanticTypeCompatibleWithProperty(PinType.ObjectPath, Property))
	{
		return false;
	}
	return PinType.Category.IsEmpty() || IsSemanticTypeCompatibleWithProperty(PinType.Category, Property);
}

bool FBlueprintHelperCallFunctionResolverUtils::DoesGraphSupportImpureFunctions(UEdGraph* Graph)
{
	if (!Graph)
	{
		return true;
	}

	const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());
	if (!K2Schema)
	{
		K2Schema = GetDefault<UEdGraphSchema_K2>();
	}
	return !K2Schema || K2Schema->DoesGraphSupportImpureFunctions(Graph);
}

FBlueprintHelperK2CallContext FBlueprintHelperCallFunctionResolverUtils::BuildEffectiveContext(
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	FBlueprintHelperK2CallContext Context = Request.Context;
	if (!Context.Blueprint)
	{
		Context.Blueprint = Request.Blueprint;
	}
	if (!Context.Graph)
	{
		Context.Graph = Request.Graph;
	}
	if (!Context.Schema && Context.Graph)
	{
		Context.Schema = Context.Graph->GetSchema();
	}
	if (!Context.Blueprint && Context.Graph)
	{
		Context.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Context.Graph);
	}
	if (!Context.SelfClass && Context.Blueprint)
	{
		Context.SelfClass = Context.Blueprint->GeneratedClass
			? Context.Blueprint->GeneratedClass
			: (Context.Blueprint->SkeletonGeneratedClass
				? Context.Blueprint->SkeletonGeneratedClass
				: Context.Blueprint->ParentClass);
	}
	if (Context.GraphKind.IsEmpty() && Context.Graph)
	{
		Context.GraphKind = Context.Graph->GetClass()->GetName();
	}
	if (Context.ArgumentNames.Num() == 0)
	{
		Context.ArgumentNames = Request.ArgumentNames;
	}
	if (Context.ArgumentTypes.Num() == 0)
	{
		Context.ArgumentTypes = Request.ArgumentTypes;
	}
	if (Context.ArgumentPinTypes.Num() == 0)
	{
		Context.ArgumentPinTypes = Request.ArgumentPinTypes;
	}
	if (Context.TargetObjectType.IsEmpty())
	{
		Context.TargetObjectType = Request.TargetObjectType;
	}
	if (!Context.TargetObjectPinType.IsValid())
	{
		Context.TargetObjectPinType = Request.TargetObjectPinType;
	}
	if (Context.ExpectedReturnType.IsEmpty())
	{
		Context.ExpectedReturnType = Request.ExpectedReturnType;
	}
	if (!Context.ExpectedReturnPinType.IsValid())
	{
		Context.ExpectedReturnPinType = Request.ExpectedReturnPinType;
	}
	UGraphWriteFunctionResolutionUtils::RemoveTargetObjectSemanticPortFromContext(Context);
	return Context;
}

bool FBlueprintHelperCallFunctionResolverUtils::IsTargetObjectTypeCompatible(const FBlueprintHelperCallFunctionCandidate& Candidate, const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	const FBlueprintHelperK2CallContext Context = BuildEffectiveContext(Request);
	if (Context.TargetObjectType.TrimStartAndEnd().IsEmpty() && !Context.TargetObjectPinType.IsValid())
	{
		return true;
	}

	const UFunction* Function = Candidate.Function.Get();
	if (!Function)
	{
		return false;
	}

	UClass* RequestedClass = ResolveClassByTypeName(Context.TargetObjectType);
	if (!RequestedClass && Context.TargetObjectPinType.IsValid())
	{
		RequestedClass = UGraphWriteFunctionResolutionUtils::ResolveClassFromPinType(Context.TargetObjectPinType);
	}
	if (RequestedClass && Function->GetOwnerClass() && !Function->HasAnyFunctionFlags(FUNC_Static))
	{
		return RequestedClass->IsChildOf(Function->GetOwnerClass());
	}

	const FString TargetPinName = !Candidate.TargetObjectPin.IsEmpty() ? Candidate.TargetObjectPin : FString(TEXT("self"));
	if (FProperty* TargetProperty = FindInputPropertyByName(Function, TargetPinName))
	{
		return IsSemanticTypeCompatibleWithProperty(Context.TargetObjectType, TargetProperty)
			&& IsPinTypeCompatibleWithProperty(Context.TargetObjectPinType, TargetProperty);
	}

	return !RequestedClass || !Function->GetOwnerClass() || RequestedClass->IsChildOf(Function->GetOwnerClass());
}

bool FBlueprintHelperCallFunctionResolverUtils::IsExpectedReturnTypeCompatible(const FBlueprintHelperCallFunctionCandidate& Candidate, const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	const FBlueprintHelperK2CallContext Context = BuildEffectiveContext(Request);
	if (Context.ExpectedReturnType.TrimStartAndEnd().IsEmpty() && !Context.ExpectedReturnPinType.IsValid())
	{
		return true;
	}

	const UFunction* Function = Candidate.Function.Get();
	if (!Function)
	{
		return false;
	}

	const FProperty* ReturnProperty = nullptr;
	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		const FProperty* Property = *PropIt;
		if (Property && Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			ReturnProperty = Property;
			break;
		}
	}

	return ReturnProperty
		&& IsSemanticTypeCompatibleWithProperty(Context.ExpectedReturnType, ReturnProperty)
		&& IsPinTypeCompatibleWithProperty(Context.ExpectedReturnPinType, ReturnProperty);
}

bool FBlueprintHelperCallFunctionResolverUtils::AreRequestedArgumentsCompatible(const FBlueprintHelperCallFunctionCandidate& Candidate, const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	const UFunction* Function = Candidate.Function.Get();
	if (!Function)
	{
		return false;
	}

	const FBlueprintHelperK2CallContext Context = BuildEffectiveContext(Request);
	for (const FString& ArgumentName : Context.ArgumentNames)
	{
		if (!FindInputPropertyByName(Function, ArgumentName))
		{
			return false;
		}
	}

	for (const TPair<FString, FString>& Pair : Context.ArgumentTypes)
	{
		FProperty* Property = FindInputPropertyByName(Function, Pair.Key);
		if (!Property || !IsSemanticTypeCompatibleWithProperty(Pair.Value, Property))
		{
			return false;
		}
	}

	for (const TPair<FString, FBlueprintHelperCallFunctionPinType>& Pair : Context.ArgumentPinTypes)
	{
		FProperty* Property = FindInputPropertyByName(Function, Pair.Key);
		if (!Property || !IsPinTypeCompatibleWithProperty(Pair.Value, Property))
		{
			return false;
		}
	}

	return true;
}

FString FBlueprintHelperCallFunctionResolverUtils::DescribeCandidateMismatch(
	const FBlueprintHelperCallFunctionCandidate& Candidate,
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	const UFunction* Function = Candidate.Function.Get();
	if (!Function)
	{
		return TEXT("function_invalid");
	}
	if (!Candidate.bBlueprintCallable && !Candidate.bBlueprintPure)
	{
		return TEXT("not_blueprint_callable");
	}

	const FBlueprintHelperK2CallContext Context = BuildEffectiveContext(Request);
	if ((Candidate.bLatent || (!Candidate.bBlueprintPure && Candidate.bBlueprintCallable))
		&& !DoesGraphSupportImpureFunctions(Context.Graph))
	{
		return TEXT("impure_or_latent_call_not_supported_by_graph");
	}
	if (!Candidate.bGraphCompatible)
	{
		return TEXT("graph_incompatible");
	}
	if (!IsTargetObjectTypeCompatible(Candidate, Request))
	{
		return FString::Printf(
			TEXT("target_object_type_mismatch:target=%s target_pin=%s"),
			*Context.TargetObjectType,
			*UGraphWriteFunctionResolutionUtils::DescribePinTypeForDiagnostics(Context.TargetObjectPinType));
	}
	if (!IsExpectedReturnTypeCompatible(Candidate, Request))
	{
		return FString::Printf(
			TEXT("return_type_mismatch:expected=%s expected_pin=%s actual=%s"),
			*Context.ExpectedReturnType,
			*UGraphWriteFunctionResolutionUtils::DescribePinTypeForDiagnostics(Context.ExpectedReturnPinType),
			*Candidate.ReturnType);
	}

	for (const FString& ArgumentName : Context.ArgumentNames)
	{
		if (!FindInputPropertyByName(Function, ArgumentName))
		{
			return FString::Printf(TEXT("missing_argument_pin:%s"), *ArgumentName);
		}
	}
	for (const TPair<FString, FString>& Pair : Context.ArgumentTypes)
	{
		FProperty* Property = FindInputPropertyByName(Function, Pair.Key);
		if (!Property)
		{
			return FString::Printf(TEXT("missing_argument_pin:%s"), *Pair.Key);
		}
		if (!IsSemanticTypeCompatibleWithProperty(Pair.Value, Property))
		{
			return FString::Printf(
				TEXT("argument_type_mismatch:%s expected=%s actual=%s"),
				*Pair.Key,
				*GetPropertySemanticType(Property),
				*Pair.Value);
		}
	}
	for (const TPair<FString, FBlueprintHelperCallFunctionPinType>& Pair : Context.ArgumentPinTypes)
	{
		FProperty* Property = FindInputPropertyByName(Function, Pair.Key);
		if (!Property)
		{
			return FString::Printf(TEXT("missing_argument_pin:%s"), *Pair.Key);
		}
		if (!IsPinTypeCompatibleWithProperty(Pair.Value, Property))
		{
			return FString::Printf(
				TEXT("argument_pin_type_mismatch:%s expected=%s actual=%s"),
				*Pair.Key,
				*GetPropertySemanticType(Property),
				*UGraphWriteFunctionResolutionUtils::DescribePinTypeForDiagnostics(Pair.Value));
		}
	}
	return FString();
}

bool FBlueprintHelperCallFunctionResolverUtils::PassesMetadataFilters(const FBlueprintHelperCallFunctionCandidate& Candidate, const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	return DescribeCandidateMismatch(Candidate, Request).IsEmpty();
}

int32 FBlueprintHelperCallFunctionResolverUtils::ComputeTypedConstraintBonus(const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	const FBlueprintHelperK2CallContext Context = BuildEffectiveContext(Request);
	int32 Bonus = 0;
	Bonus += FMath::Min(160, Context.ArgumentTypes.Num() * 40);
	Bonus += FMath::Min(160, Context.ArgumentPinTypes.Num() * 40);
	if (!Context.TargetObjectType.IsEmpty() || Context.TargetObjectPinType.IsValid())
	{
		Bonus += 120;
	}
	if (!Context.ExpectedReturnType.IsEmpty() || Context.ExpectedReturnPinType.IsValid())
	{
		Bonus += 80;
	}
	return Bonus;
}

bool FBlueprintHelperCallFunctionResolverUtils::OwnerMatches(const FBlueprintHelperCallFunctionCandidate& Candidate, const FString& OwnerQuery)
{
	const FString Query = NormalizeForCompare(OwnerQuery);
	if (Query.IsEmpty())
	{
		return false;
	}

	const FString OwnerPath = NormalizeForCompare(Candidate.OwnerClassPath);
	if (OwnerPath == Query)
	{
		return true;
	}

	const UFunction* Function = Candidate.Function.Get();
	const FString OwnerName = NormalizeForCompare(GetOwnerClassName(Function));
	if (OwnerName == Query)
	{
		return true;
	}

	if (OwnerName.StartsWith(TEXT("u")) && OwnerName.Mid(1) == Query)
	{
		return true;
	}

	return false;
}

void FBlueprintHelperCallFunctionResolverUtils::TokenizeSearchText(const FString& Text, TArray<FString>& OutTokens)
{
	FString Normalized = NormalizeForCompare(Text);
	Normalized.ReplaceInline(TEXT("/"), TEXT(" "));
	Normalized.ReplaceInline(TEXT("."), TEXT(" "));
	Normalized.ReplaceInline(TEXT(":"), TEXT(" "));
	Normalized.ReplaceInline(TEXT("_"), TEXT(" "));
	Normalized.ReplaceInline(TEXT("-"), TEXT(" "));

	TArray<FString> RawTokens;
	Normalized.ParseIntoArrayWS(RawTokens);
	for (const FString& Token : RawTokens)
	{
		if (!Token.IsEmpty())
		{
			OutTokens.AddUnique(Token);
		}
	}
}

FString FBlueprintHelperCallFunctionResolverUtils::BuildSearchText(const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	return FString::Printf(
		TEXT("%s %s %s %s"),
		*Candidate.OwnerClassPath,
		*Candidate.NativeFunctionName,
		*Candidate.DisplayName,
		*Candidate.Category);
}

bool FBlueprintHelperCallFunctionResolverUtils::AllQueryTokensContained(const FString& Query, const FString& SearchText)
{
	TArray<FString> QueryTokens;
	TokenizeSearchText(Query, QueryTokens);
	if (QueryTokens.Num() == 0)
	{
		return false;
	}

	const FString NormalizedSearch = NormalizeForCompare(SearchText);
	for (const FString& QueryToken : QueryTokens)
	{
		if (!NormalizedSearch.Contains(QueryToken))
		{
			return false;
		}
	}
	return true;
}

bool FBlueprintHelperCallFunctionResolverUtils::HasExactToken(const FString& Query, const FString& SearchText)
{
	const FString NormalizedQuery = NormalizeForCompare(Query);
	if (NormalizedQuery.IsEmpty())
	{
		return false;
	}

	TArray<FString> SearchTokens;
	TokenizeSearchText(SearchText, SearchTokens);
	return SearchTokens.Contains(NormalizedQuery);
}

void FBlueprintHelperCallFunctionResolverUtils::AddCandidateForFunction(
	UFunction* Function,
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates,
	UBlueprintNodeSpawner* NodeSpawner)
{
	if (!IsBlueprintCallableFunction(Function))
	{
		return;
	}

	const FString StableId = FBlueprintHelperCallFunctionResolver::MakeStableId(Function);
	if (StableId.IsEmpty() || InOutCandidates.Contains(StableId))
	{
		return;
	}
	if (!UGraphWriteFunctionResolutionUtils::IsStableCallableIdPermitted(StableId, Request))
	{
		return;
	}

	const FBlueprintHelperK2CallContext Context = BuildEffectiveContext(Request);
	const bool bCompatibleByGraph = IsGraphCompatible(Function, Context.Blueprint, Context.Graph);
	UBlueprintNodeSpawner* EffectiveNodeSpawner = NodeSpawner;
	if (!EffectiveNodeSpawner && bCompatibleByGraph)
	{
		EffectiveNodeSpawner = UBlueprintFunctionNodeSpawner::Create(Function);
	}

	const TSubclassOf<UK2Node_CallFunction> CandidateNodeClass = UGraphWriteFunctionResolutionUtils::ResolveCandidateNodeClass(Function, EffectiveNodeSpawner);
	if (!UGraphWriteFunctionResolutionUtils::IsNodeClassPermitted(*CandidateNodeClass, Request))
	{
		return;
	}

	FBlueprintHelperCallFunctionCandidate Candidate;
	Candidate.StableId = StableId;
	Candidate.OwnerClassPath = GetOwnerClassPath(Function);
	Candidate.NativeFunctionName = Function->GetName();
	Candidate.DisplayName = GetFunctionDisplayName(Function);
	Candidate.Category = GetFunctionCategory(Function);
	Candidate.NodeClass = CandidateNodeClass;
	Candidate.NodeClassPath = CandidateNodeClass ? CandidateNodeClass->GetPathName() : FString();
	Candidate.bFromActionDatabase = NodeSpawner != nullptr;
	Candidate.bGraphCompatible = NodeSpawner != nullptr || bCompatibleByGraph;
	Candidate.bBlueprintCallable = Function->HasAnyFunctionFlags(FUNC_BlueprintCallable);
	Candidate.bBlueprintPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	Candidate.bLatent = Function->HasMetaData(TEXT("Latent")) || Function->HasMetaData(TEXT("LatentInfo"));
	Candidate.WorldContextPin = Function->HasMetaData(TEXT("WorldContext")) ? Function->GetMetaData(TEXT("WorldContext")) : FString();
	Candidate.bRequiresWorldContext = !Candidate.WorldContextPin.IsEmpty();
	Candidate.bCustomThunk = Function->HasMetaData(TEXT("CustomThunk"));
	Candidate.bHasArrayParm = Function->HasMetaData(TEXT("ArrayParm"));
	Candidate.bHasArrayTypeDependentParams = Function->HasMetaData(TEXT("ArrayTypeDependentParams"));
	Candidate.bDeterminesOutputType = Function->HasMetaData(TEXT("DeterminesOutputType"));
	Candidate.TargetObjectPin = Function->HasAnyFunctionFlags(FUNC_Static) ? Function->GetMetaData(TEXT("DefaultToSelf")) : FString(TEXT("self"));
	for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
	{
		const FProperty* Property = *PropIt;
		if (IsFunctionInputProperty(Property))
		{
			Candidate.InputPins.Add(Property->GetName());
			Candidate.InputPinTypes.Add(Property->GetName(), GetPropertySemanticType(Property));
		}
		else if (IsFunctionOutputProperty(Property) && Candidate.ReturnType.IsEmpty())
		{
			Candidate.ReturnType = GetPropertySemanticType(Property);
		}
	}
	Candidate.Function = Function;
	Candidate.NodeSpawner = EffectiveNodeSpawner;
	InOutCandidates.Add(StableId, Candidate);
}

void FBlueprintHelperCallFunctionResolverUtils::AddActionDatabaseCandidates(
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates)
{
	const FBlueprintHelperK2CallContext EffectiveContext = BuildEffectiveContext(Request);
	FBlueprintActionContext Context;
	if (EffectiveContext.Blueprint)
	{
		Context.Blueprints.Add(EffectiveContext.Blueprint);
	}
	if (EffectiveContext.Graph)
	{
		Context.Graphs.Add(EffectiveContext.Graph);
	}

	FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety);
	Filter.Context = Context;
	TArray<UClass*> PermittedNodeClasses;
	UGraphWriteFunctionResolutionUtils::GetPermittedNodeClasses(Request, PermittedNodeClasses);
	for (UClass* PermittedNodeClass : PermittedNodeClasses)
	{
		Filter.PermittedNodeTypes.Add(PermittedNodeClass);
	}

	FBlueprintActionDatabase::Get().RefreshAll();
	const FBlueprintActionDatabase::FActionRegistry& ActionRegistry = FBlueprintActionDatabase::Get().GetAllActions();
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& RegistryPair : ActionRegistry)
	{
		const UObject* ActionOwner = RegistryPair.Key.ResolveObjectPtr();
		for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : RegistryPair.Value)
		{
			UBlueprintNodeSpawner* Spawner = SpawnerPtr.Get();
			if (!Spawner)
			{
				continue;
			}

			FBlueprintActionInfo ActionInfo(ActionOwner, Spawner);
			if (Filter.IsFiltered(ActionInfo))
			{
				continue;
			}

			UFunction const* AssociatedFunction = ActionInfo.GetAssociatedFunction();
			if (!AssociatedFunction)
			{
				continue;
			}

			AddCandidateForFunction(UGraphWriteFunctionResolutionUtils::CanonicalizeActionDatabaseFunction(AssociatedFunction), Request, InOutCandidates, Spawner);
		}
	}
}

TArray<FBlueprintHelperCallFunctionCandidate> FBlueprintHelperCallFunctionResolverUtils::BuildCandidateUniverse(
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	TMap<FString, FBlueprintHelperCallFunctionCandidate> CandidateMap;
	const FBlueprintHelperK2CallContext Context = BuildEffectiveContext(Request);
	UClass* RequestedTargetClass = ResolveClassByTypeName(Context.TargetObjectType);
	if (!RequestedTargetClass && Context.TargetObjectPinType.IsValid())
	{
		RequestedTargetClass = UGraphWriteFunctionResolutionUtils::ResolveClassFromPinType(Context.TargetObjectPinType);
	}
	AddActionDatabaseCandidates(Request, CandidateMap);
	UGraphWriteFunctionResolutionUtils::AddRequiredStableCallableCandidates(Request, CandidateMap);

	auto AddClassFunctions = [&CandidateMap, &Request](UClass* Class)
	{
		if (!IsUsableOwnerClass(Class))
		{
			return;
		}

		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			AddCandidateForFunction(*FuncIt, Request, CandidateMap);
		}
	};

	if (RequestedTargetClass)
	{
		for (UClass* Class = RequestedTargetClass; Class; Class = Class->GetSuperClass())
		{
			AddClassFunctions(Class);
		}
	}
	else if (Context.Blueprint)
	{
		AddClassFunctions(Context.Blueprint->SkeletonGeneratedClass);
		AddClassFunctions(Context.Blueprint->GeneratedClass);
		AddClassFunctions(Context.Blueprint->ParentClass);
	}

	TArray<FBlueprintHelperCallFunctionCandidate> Candidates;
	CandidateMap.GenerateValueArray(Candidates);
	return Candidates;
}

int32 FBlueprintHelperCallFunctionResolverUtils::ApplyCategoryPriorityBonus(
	const FBlueprintHelperCallFunctionCandidate& Candidate,
	const TArray<FString>& CategoryPriority)
{
	for (int32 Index = 0; Index < CategoryPriority.Num(); ++Index)
	{
		const FString Priority = CategoryPriority[Index].TrimStartAndEnd();
		if (Priority.IsEmpty())
		{
			continue;
		}

		if (Candidate.Category.Contains(Priority, ESearchCase::IgnoreCase)
			|| Candidate.OwnerClassPath.Contains(Priority, ESearchCase::IgnoreCase)
			|| Candidate.NativeFunctionName.Contains(Priority, ESearchCase::IgnoreCase)
			|| Candidate.DisplayName.Contains(Priority, ESearchCase::IgnoreCase))
		{
			return FMath::Max(10, 80 - Index * 10);
		}
	}
	return 0;
}

bool FBlueprintHelperCallFunctionResolverUtils::IsExactSearchMode(const FString& SearchMode)
{
	return SearchMode.Equals(TEXT("exact"), ESearchCase::IgnoreCase)
		|| SearchMode.Equals(TEXT("precise"), ESearchCase::IgnoreCase);
}

int32 FBlueprintHelperCallFunctionResolverUtils::ScoreCandidate(
	const FBlueprintHelperCallFunctionCandidate& Candidate,
	const FString& Query,
	const FString& QualifiedOwner,
	const FString& QualifiedFunction,
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	FString& OutMatchReason)
{
	OutMatchReason.Reset();
	const int32 PriorityBonus = ApplyCategoryPriorityBonus(Candidate, Request.CategoryPriority);
	const int32 TypedBonus = ComputeTypedConstraintBonus(Request);
	const bool bExactMode = IsExactSearchMode(Request.SearchMode);

	if (!QualifiedFunction.IsEmpty())
	{
		if (OwnerMatches(Candidate, QualifiedOwner) &&
			Candidate.NativeFunctionName.Equals(QualifiedFunction, ESearchCase::IgnoreCase))
		{
			OutMatchReason = TEXT("owner-qualified exact native");
			return 1000 + PriorityBonus + TypedBonus;
		}
		return 0;
	}

	if (Candidate.StableId.Equals(Query, ESearchCase::IgnoreCase))
	{
		OutMatchReason = TEXT("stable id exact");
		return 1000 + PriorityBonus + TypedBonus;
	}

	if (Candidate.NativeFunctionName.Equals(Query, ESearchCase::IgnoreCase))
	{
		OutMatchReason = TEXT("native exact");
		return 900 + PriorityBonus + TypedBonus;
	}

	if (Candidate.DisplayName.Equals(Query, ESearchCase::IgnoreCase))
	{
		OutMatchReason = TEXT("display exact");
		return 850 + PriorityBonus + TypedBonus;
	}

	if (bExactMode)
	{
		return 0;
	}

	if (CompactEquals(Candidate.NativeFunctionName, Query) || CompactEquals(Candidate.DisplayName, Query))
	{
		OutMatchReason = TEXT("compact exact");
		return 850 + PriorityBonus + TypedBonus;
	}

	const FString SearchText = BuildSearchText(Candidate);
	if (HasExactToken(Query, SearchText))
	{
		OutMatchReason = TEXT("search token exact");
		return 700 + PriorityBonus + TypedBonus;
	}

	if (AllQueryTokensContained(Query, SearchText))
	{
		OutMatchReason = TEXT("search tokens contained");
		return 500 + PriorityBonus + TypedBonus;
	}

	return 0;
}

void FBlueprintHelperCallFunctionResolverUtils::SortCandidates(TArray<FBlueprintHelperCallFunctionCandidate>& Candidates)
{
	Candidates.Sort([](const FBlueprintHelperCallFunctionCandidate& Left, const FBlueprintHelperCallFunctionCandidate& Right)
	{
		if (Left.Score != Right.Score)
		{
			return Left.Score > Right.Score;
		}
		if (Left.OwnerClassPath != Right.OwnerClassPath)
		{
			return Left.OwnerClassPath < Right.OwnerClassPath;
		}
		if (Left.NativeFunctionName != Right.NativeFunctionName)
		{
			return Left.NativeFunctionName < Right.NativeFunctionName;
		}
		return Left.DisplayName < Right.DisplayName;
	});
}

FString FBlueprintHelperCallFunctionResolverUtils::BuildCandidateSummary(const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	return FString::Printf(
		TEXT("%s display=\"%s\" owner=\"%s\""),
		*Candidate.StableId,
		*Candidate.DisplayName,
		*Candidate.OwnerClassPath);
}


FString FBlueprintHelperCallFunctionResolverUtils::BuildCandidateFunctionJsonString(const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	return FString::Printf(
		TEXT("{\"stable_id\":%s,\"display_name\":%s,\"owner_class\":%s,\"native_name\":%s}"),
		*JsonQuote(Candidate.StableId),
		*JsonQuote(Candidate.DisplayName),
		*JsonQuote(Candidate.OwnerClassPath),
		*JsonQuote(Candidate.NativeFunctionName));
}

FBlueprintHelperCallFunctionCandidateInfo FBlueprintHelperCallFunctionResolverUtils::BuildCandidateFunctionInfo(const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	FBlueprintHelperCallFunctionCandidateInfo Info;
	Info.StableId = Candidate.StableId;
	Info.DisplayName = Candidate.DisplayName;
	Info.OwnerClassPath = Candidate.OwnerClassPath;
	Info.NativeFunctionName = Candidate.NativeFunctionName;
	Info.Category = Candidate.Category;
	Info.NodeClassPath = Candidate.NodeClassPath;
	Info.MatchReason = Candidate.MatchReason;
	Info.ReturnType = Candidate.ReturnType;
	Info.WorldContextPin = Candidate.WorldContextPin;
	Info.TargetObjectPin = Candidate.TargetObjectPin;
	Info.InputPins = Candidate.InputPins;
	Info.InputPinTypes = Candidate.InputPinTypes;
	Info.MismatchReason = Candidate.MismatchReason;
	Info.Score = Candidate.Score;
	Info.bGraphCompatible = Candidate.bGraphCompatible;
	Info.bFromActionDatabase = Candidate.bFromActionDatabase;
	Info.bBlueprintCallable = Candidate.bBlueprintCallable;
	Info.bBlueprintPure = Candidate.bBlueprintPure;
	Info.bLatent = Candidate.bLatent;
	Info.bRequiresWorldContext = Candidate.bRequiresWorldContext;
	Info.bCustomThunk = Candidate.bCustomThunk;
	Info.bHasArrayParm = Candidate.bHasArrayParm;
	Info.bHasArrayTypeDependentParams = Candidate.bHasArrayTypeDependentParams;
	Info.bDeterminesOutputType = Candidate.bDeterminesOutputType;
	return Info;
}

FString FBlueprintHelperCallFunctionResolverUtils::JsonQuote(const FString& Value)
{
	FString Escaped;
	Escaped.Reserve(Value.Len());
	for (const TCHAR Character : Value)
	{
		switch (Character)
		{
		case TEXT('\\'):
			Escaped += TEXT("\\\\");
			break;
		case TEXT('"'):
			Escaped += TEXT("\\\"");
			break;
		case TEXT('\r'):
			Escaped += TEXT("\\r");
			break;
		case TEXT('\n'):
			Escaped += TEXT("\\n");
			break;
		case TEXT('\t'):
			Escaped += TEXT("\\t");
			break;
		case TEXT('\b'):
			Escaped += TEXT("\\b");
			break;
		case TEXT('\f'):
			Escaped += TEXT("\\f");
			break;
		default:
			if (Character < 0x20)
			{
				Escaped += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Character));
			}
			else
			{
				Escaped.AppendChar(Character);
			}
			break;
		}
	}
	return FString::Printf(TEXT("\"%s\""), *Escaped);
}

FString FBlueprintHelperCallFunctionResolverUtils::BuildCandidateListMessage(
	const FString& Prefix,
	const FString& TargetQuery,
	const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates)
{
	TArray<FString> Summaries;
	TArray<FString> CandidateFunctions;
	for (const FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		Summaries.Add(BuildCandidateSummary(Candidate));
		CandidateFunctions.Add(BuildCandidateFunctionJsonString(Candidate));
	}
	const FString CandidateFunctionGroup = FString::Printf(
		TEXT("{\"query\":%s,\"candidates\":[%s]}"),
		*JsonQuote(TargetQuery),
		*FString::Join(CandidateFunctions, TEXT(",")));
	return FString::Printf(
		TEXT("%s candidate_functions=[%s] Candidates: %s"),
		*Prefix,
		*CandidateFunctionGroup,
		*FString::Join(Summaries, TEXT("; ")));
}

void FBlueprintHelperCallFunctionResolverUtils::SetTopCandidates(
	FBlueprintHelperCallFunctionResolveResult& Result,
	const TArray<FBlueprintHelperCallFunctionCandidate>& ScoredCandidates,
	int32 MaxCandidates)
{
	const int32 Limit = FMath::Max(1, MaxCandidates);
	for (const FBlueprintHelperCallFunctionCandidate& Candidate : ScoredCandidates)
	{
		if (Result.Candidates.Num() >= Limit)
		{
			break;
		}
		Result.Candidates.Add(Candidate);
		Result.CandidateFunctions.Add(BuildCandidateFunctionInfo(Candidate));
	}
}

bool FBlueprintHelperCallFunctionResolverUtils::HasOwnerCandidate(const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates, const FString& OwnerQuery)
{
	for (const FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		if (OwnerMatches(Candidate, OwnerQuery))
		{
			return true;
		}
	}
	return false;
}

bool FBlueprintHelperCallFunctionResolverUtils::HasNativeDisplayConflict(const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates)
{
	TSet<FString> NativeExactStableIds;
	TSet<FString> DisplayExactStableIds;
	for (const FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		if (Candidate.Score == 900)
		{
			NativeExactStableIds.Add(Candidate.StableId);
		}
		else if (Candidate.Score == 850)
		{
			DisplayExactStableIds.Add(Candidate.StableId);
		}
	}

	if (NativeExactStableIds.Num() == 0 || DisplayExactStableIds.Num() == 0)
	{
		return false;
	}

	for (const FString& StableId : DisplayExactStableIds)
	{
		NativeExactStableIds.Add(StableId);
	}
	return NativeExactStableIds.Num() > 1;
}

void FBlueprintHelperCallFunctionResolverUtils::PreferGeneratedClassOverSkeletonDuplicates(
	TArray<FBlueprintHelperCallFunctionCandidate>& Candidates,
	const UBlueprint* Blueprint)
{
	auto MakeBlueprintFunctionKey = [](const UBlueprint* OwnerBlueprint, const FString& FunctionName)
	{
		return OwnerBlueprint
			? FString::Printf(TEXT("%s:%s"), *OwnerBlueprint->GetPathName(), *NormalizeForCompare(FunctionName))
			: FString();
	};

	TSet<FString> AnyBlueprintGeneratedFunctionKeys;
	for (const FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		const UFunction* Function = Candidate.Function.Get();
		const UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
		const UBlueprint* OwnerBlueprint = OwnerClass ? Cast<UBlueprint>(OwnerClass->ClassGeneratedBy) : nullptr;
		if (OwnerBlueprint && OwnerBlueprint->GeneratedClass.Get() == OwnerClass)
		{
			AnyBlueprintGeneratedFunctionKeys.Add(MakeBlueprintFunctionKey(OwnerBlueprint, Candidate.NativeFunctionName));
		}
	}

	if (AnyBlueprintGeneratedFunctionKeys.Num() > 0)
	{
		Candidates.RemoveAll([&AnyBlueprintGeneratedFunctionKeys, &MakeBlueprintFunctionKey](const FBlueprintHelperCallFunctionCandidate& Candidate)
		{
			const UFunction* Function = Candidate.Function.Get();
			const UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
			const UBlueprint* OwnerBlueprint = OwnerClass ? Cast<UBlueprint>(OwnerClass->ClassGeneratedBy) : nullptr;
			return OwnerBlueprint &&
				OwnerBlueprint->SkeletonGeneratedClass.Get() == OwnerClass &&
				AnyBlueprintGeneratedFunctionKeys.Contains(MakeBlueprintFunctionKey(OwnerBlueprint, Candidate.NativeFunctionName));
		});
	}

	if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->SkeletonGeneratedClass)
	{
		return;
	}

	TSet<FString> GeneratedFunctionKeys;
	for (const FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		const UFunction* Function = Candidate.Function.Get();
		if (Function && Function->GetOwnerClass() == Blueprint->GeneratedClass)
		{
			GeneratedFunctionKeys.Add(NormalizeForCompare(Candidate.NativeFunctionName));
		}
	}

	if (GeneratedFunctionKeys.Num() == 0)
	{
		return;
	}

	Candidates.RemoveAll([Blueprint, &GeneratedFunctionKeys](const FBlueprintHelperCallFunctionCandidate& Candidate)
	{
		const UFunction* Function = Candidate.Function.Get();
		return Function &&
			Function->GetOwnerClass() == Blueprint->SkeletonGeneratedClass &&
			GeneratedFunctionKeys.Contains(NormalizeForCompare(Candidate.NativeFunctionName));
	});
}

void FBlueprintHelperCallFunctionResolverUtils::PreferTargetBlueprintCandidates(
	TArray<FBlueprintHelperCallFunctionCandidate>& Candidates,
	const UBlueprint* Blueprint)
{
	if (!Blueprint || (!Blueprint->GeneratedClass && !Blueprint->SkeletonGeneratedClass))
	{
		return;
	}

	auto IsTargetBlueprintClass = [Blueprint](const UClass* Class)
	{
		return Class &&
			(Class == Blueprint->GeneratedClass || Class == Blueprint->SkeletonGeneratedClass);
	};

	TSet<FString> TargetFunctionKeys;
	for (const FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		const UFunction* Function = Candidate.Function.Get();
		if (Function && IsTargetBlueprintClass(Function->GetOwnerClass()))
		{
			TargetFunctionKeys.Add(NormalizeForCompare(Candidate.NativeFunctionName));
		}
	}

	if (TargetFunctionKeys.Num() == 0)
	{
		return;
	}

	Candidates.RemoveAll([&IsTargetBlueprintClass, &TargetFunctionKeys](const FBlueprintHelperCallFunctionCandidate& Candidate)
	{
		const UFunction* Function = Candidate.Function.Get();
		return Function &&
			!IsTargetBlueprintClass(Function->GetOwnerClass()) &&
			TargetFunctionKeys.Contains(NormalizeForCompare(Candidate.NativeFunctionName));
	});
}
