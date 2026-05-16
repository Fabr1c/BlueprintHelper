#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace BlueprintHelperCallFunctionResolver
{
static FString NormalizeForCompare(const FString& Value)
{
	FString Result = Value.TrimStartAndEnd();
	return Result.ToLower();
}

static FString NormalizeCompactForCompare(const FString& Value)
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

static bool CompactEquals(const FString& Left, const FString& Right)
{
	const FString NormalizedLeft = NormalizeCompactForCompare(Left);
	return !NormalizedLeft.IsEmpty() && NormalizedLeft == NormalizeCompactForCompare(Right);
}

static FString GetOwnerClassPath(const UFunction* Function)
{
	const UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
	return OwnerClass ? OwnerClass->GetPathName() : FString();
}

static FString GetOwnerClassName(const UFunction* Function)
{
	const UClass* OwnerClass = Function ? Function->GetOwnerClass() : nullptr;
	return OwnerClass ? OwnerClass->GetName() : FString();
}

static FString GetFunctionDisplayName(const UFunction* Function)
{
	if (!Function)
	{
		return FString();
	}

	const FString DisplayName = Function->GetDisplayNameText().ToString();
	return DisplayName.IsEmpty() ? Function->GetName() : DisplayName;
}

static FString GetFunctionCategory(const UFunction* Function)
{
	return Function && Function->HasMetaData(TEXT("Category"))
		? Function->GetMetaData(TEXT("Category"))
		: FString();
}

static FString NormalizeTypeToken(const FString& Type)
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

static UClass* ResolveClassByTypeName(const FString& TypeName)
{
	const FString Query = NormalizeTypeToken(TypeName);
	if (Query.IsEmpty())
	{
		return nullptr;
	}

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class)
		{
			continue;
		}

		const FString ClassName = NormalizeTypeToken(Class->GetName());
		const FString ClassCppName = NormalizeTypeToken(Class->GetPrefixCPP() + Class->GetName());
		const FString ClassPath = NormalizeTypeToken(Class->GetPathName());
		if (ClassName == Query || ClassCppName == Query || ClassPath == Query || ClassPath.EndsWith(TEXT(".") + Query))
		{
			return Class;
		}
	}
	return nullptr;
}

static UScriptStruct* ResolveStructByTypeName(const FString& TypeName)
{
	const FString Query = NormalizeTypeToken(TypeName);
	if (Query.IsEmpty())
	{
		return nullptr;
	}

	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		UScriptStruct* Struct = *StructIt;
		if (!Struct)
		{
			continue;
		}

		const FString StructName = NormalizeTypeToken(Struct->GetName());
		const FString StructCppName = NormalizeTypeToken(Struct->GetPrefixCPP() + Struct->GetName());
		const FString StructPath = NormalizeTypeToken(Struct->GetPathName());
		if (StructName == Query || StructCppName == Query || StructPath == Query || StructPath.EndsWith(TEXT(".") + Query))
		{
			return Struct;
		}
	}
	return nullptr;
}

static bool IsBlueprintCallableFunction(const UFunction* Function)
{
	return Function &&
		Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure) &&
		!Function->HasAnyFunctionFlags(FUNC_Delegate);
}

static bool IsUsableOwnerClass(const UClass* Class)
{
	return Class && !Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);
}

static bool IsGraphCompatible(const UFunction* Function, UBlueprint* Blueprint, UEdGraph* Graph)
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

	UBlueprint* TargetBlueprint = Blueprint ? Blueprint : FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	const UClass* BlueprintClass = nullptr;
	if (TargetBlueprint)
	{
		BlueprintClass = TargetBlueprint->SkeletonGeneratedClass
			? TargetBlueprint->SkeletonGeneratedClass
			: (TargetBlueprint->GeneratedClass ? TargetBlueprint->GeneratedClass : TargetBlueprint->ParentClass);
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (!K2Schema)
	{
		return false;
	}

	uint32 AllowedFunctionTypes =
		UEdGraphSchema_K2::EFunctionType::FT_Pure |
		UEdGraphSchema_K2::EFunctionType::FT_Const |
		UEdGraphSchema_K2::EFunctionType::FT_Protected;
	if (K2Schema->DoesGraphSupportImpureFunctions(Graph))
	{
		AllowedFunctionTypes |= UEdGraphSchema_K2::EFunctionType::FT_Imperative;
	}

	return K2Schema->CanFunctionBeUsedInGraph(BlueprintClass, Function, Graph, AllowedFunctionTypes, false);
}

static bool IsFunctionInputProperty(const FProperty* Property)
{
	if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm))
	{
		return false;
	}

	const bool bOutOnly = Property->HasAnyPropertyFlags(CPF_OutParm)
		&& !Property->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReferenceParm);
	return !bOutOnly;
}

static bool IsFunctionOutputProperty(const FProperty* Property)
{
	return Property && Property->HasAnyPropertyFlags(CPF_Parm)
		&& (Property->HasAnyPropertyFlags(CPF_ReturnParm)
			|| (Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ConstParm)));
}

static FString GetPropertySemanticType(const FProperty* Property)
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

static FProperty* FindInputPropertyByName(const UFunction* Function, const FString& PinName)
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
		if (NormalizeForCompare(PropertyName) == Wanted || NormalizeCompactForCompare(PropertyName) == WantedCompact)
		{
			return Property;
		}
	}
	return nullptr;
}

static bool IsNumericSemanticType(const FString& Type)
{
	const FString Token = NormalizeTypeToken(Type);
	return Token == TEXT("int") || Token == TEXT("integer") || Token == TEXT("int32") || Token == TEXT("int64")
		|| Token == TEXT("byte") || Token == TEXT("float") || Token == TEXT("double") || Token == TEXT("real")
		|| Token == TEXT("number");
}

static bool IsSemanticTypeCompatibleWithProperty(const FString& Type, const FProperty* Property)
{
	const FString Token = NormalizeTypeToken(Type);
	if (Token.IsEmpty() || !Property)
	{
		return true;
	}

	if (Token == TEXT("bool") || Token == TEXT("boolean"))
	{
		return CastField<FBoolProperty>(Property) != nullptr;
	}
	if (Token == TEXT("string"))
	{
		return CastField<FStrProperty>(Property) != nullptr;
	}
	if (Token == TEXT("name"))
	{
		return CastField<FNameProperty>(Property) != nullptr;
	}
	if (Token == TEXT("text"))
	{
		return CastField<FTextProperty>(Property) != nullptr;
	}
	if (IsNumericSemanticType(Token))
	{
		const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
		return Numeric != nullptr && (Numeric->IsInteger() || Numeric->IsFloatingPoint());
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
		return !RequestedClass || !ObjectProperty->PropertyClass || RequestedClass->IsChildOf(ObjectProperty->PropertyClass);
	}

	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
	{
		UClass* RequestedClass = ResolveClassByTypeName(Type);
		return !RequestedClass || !ClassProperty->MetaClass || RequestedClass->IsChildOf(ClassProperty->MetaClass);
	}

	return true;
}

static bool IsPinTypeCompatibleWithProperty(const FBlueprintHelperCallFunctionPinType& PinType, const FProperty* Property)
{
	if (!PinType.IsValid())
	{
		return true;
	}
	if (!PinType.ObjectPath.IsEmpty() && !IsSemanticTypeCompatibleWithProperty(PinType.ObjectPath, Property))
	{
		return false;
	}
	return PinType.Category.IsEmpty() || IsSemanticTypeCompatibleWithProperty(PinType.Category, Property);
}

static bool DoesGraphSupportImpureFunctions(UEdGraph* Graph)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return !Graph || !K2Schema || K2Schema->DoesGraphSupportImpureFunctions(Graph);
}

static bool IsTargetObjectTypeCompatible(const FBlueprintHelperCallFunctionCandidate& Candidate, const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	if (Request.TargetObjectType.TrimStartAndEnd().IsEmpty() && !Request.TargetObjectPinType.IsValid())
	{
		return true;
	}

	const UFunction* Function = Candidate.Function.Get();
	if (!Function)
	{
		return false;
	}

	UClass* RequestedClass = ResolveClassByTypeName(Request.TargetObjectType);
	if (RequestedClass && Function->GetOwnerClass() && !Function->HasAnyFunctionFlags(FUNC_Static))
	{
		return RequestedClass->IsChildOf(Function->GetOwnerClass());
	}

	const FString TargetPinName = !Candidate.TargetObjectPin.IsEmpty() ? Candidate.TargetObjectPin : FString(TEXT("self"));
	if (FProperty* TargetProperty = FindInputPropertyByName(Function, TargetPinName))
	{
		return IsSemanticTypeCompatibleWithProperty(Request.TargetObjectType, TargetProperty)
			&& IsPinTypeCompatibleWithProperty(Request.TargetObjectPinType, TargetProperty);
	}

	return !RequestedClass || !Function->GetOwnerClass() || RequestedClass->IsChildOf(Function->GetOwnerClass());
}

static bool AreRequestedArgumentsCompatible(const FBlueprintHelperCallFunctionCandidate& Candidate, const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	const UFunction* Function = Candidate.Function.Get();
	if (!Function)
	{
		return false;
	}

	for (const FString& ArgumentName : Request.ArgumentNames)
	{
		if (!FindInputPropertyByName(Function, ArgumentName))
		{
			return false;
		}
	}

	for (const TPair<FString, FString>& Pair : Request.ArgumentTypes)
	{
		FProperty* Property = FindInputPropertyByName(Function, Pair.Key);
		if (!Property || !IsSemanticTypeCompatibleWithProperty(Pair.Value, Property))
		{
			return false;
		}
	}

	for (const TPair<FString, FBlueprintHelperCallFunctionPinType>& Pair : Request.ArgumentPinTypes)
	{
		FProperty* Property = FindInputPropertyByName(Function, Pair.Key);
		if (!Property || !IsPinTypeCompatibleWithProperty(Pair.Value, Property))
		{
			return false;
		}
	}

	return true;
}

static bool PassesMetadataFilters(const FBlueprintHelperCallFunctionCandidate& Candidate, const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	if (!Candidate.bBlueprintCallable && !Candidate.bBlueprintPure)
	{
		return false;
	}

	if ((Candidate.bLatent || (!Candidate.bBlueprintPure && Candidate.bBlueprintCallable)) && !DoesGraphSupportImpureFunctions(Request.Graph))
	{
		return false;
	}

	if (Candidate.bRequiresWorldContext && !Request.Graph && !Request.Blueprint)
	{
		return false;
	}

	return IsTargetObjectTypeCompatible(Candidate, Request) && AreRequestedArgumentsCompatible(Candidate, Request);
}

static int32 ComputeTypedConstraintBonus(const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	int32 Bonus = 0;
	Bonus += FMath::Min(160, Request.ArgumentTypes.Num() * 40);
	Bonus += FMath::Min(160, Request.ArgumentPinTypes.Num() * 40);
	if (!Request.TargetObjectType.IsEmpty() || Request.TargetObjectPinType.IsValid())
	{
		Bonus += 120;
	}
	return Bonus;
}

static bool OwnerMatches(const FBlueprintHelperCallFunctionCandidate& Candidate, const FString& OwnerQuery)
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

static void TokenizeSearchText(const FString& Text, TArray<FString>& OutTokens)
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

static FString BuildSearchText(const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	return FString::Printf(
		TEXT("%s %s %s %s"),
		*Candidate.OwnerClassPath,
		*Candidate.NativeFunctionName,
		*Candidate.DisplayName,
		*Candidate.Category);
}

static bool AllQueryTokensContained(const FString& Query, const FString& SearchText)
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

static bool HasExactToken(const FString& Query, const FString& SearchText)
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

static void AddCandidateForFunction(
	UFunction* Function,
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates,
	UBlueprintNodeSpawner* NodeSpawner = nullptr)
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

	FBlueprintHelperCallFunctionCandidate Candidate;
	Candidate.StableId = StableId;
	Candidate.OwnerClassPath = GetOwnerClassPath(Function);
	Candidate.NativeFunctionName = Function->GetName();
	Candidate.DisplayName = GetFunctionDisplayName(Function);
	Candidate.Category = GetFunctionCategory(Function);
	Candidate.NodeClass = UK2Node_CallFunction::StaticClass();
	Candidate.NodeClassPath = UK2Node_CallFunction::StaticClass()->GetPathName();
	Candidate.bFromActionDatabase = NodeSpawner != nullptr;
	Candidate.bGraphCompatible = Candidate.bFromActionDatabase || IsGraphCompatible(Function, Request.Blueprint, Request.Graph);
	Candidate.bBlueprintCallable = Function->HasAnyFunctionFlags(FUNC_BlueprintCallable);
	Candidate.bBlueprintPure = Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	Candidate.bLatent = Function->HasMetaData(TEXT("Latent")) || Function->HasMetaData(TEXT("LatentInfo"));
	Candidate.WorldContextPin = Function->HasMetaData(TEXT("WorldContext")) ? Function->GetMetaData(TEXT("WorldContext")) : FString();
	Candidate.bRequiresWorldContext = !Candidate.WorldContextPin.IsEmpty();
	Candidate.TargetObjectPin = Function->HasAnyFunctionFlags(FUNC_Static) ? Function->GetMetaData(TEXT("DefaultToSelf")) : FString(TEXT("self"));
	for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
	{
		const FProperty* Property = *PropIt;
		if (IsFunctionInputProperty(Property))
		{
			Candidate.InputPins.Add(Property->GetName());
		}
		else if (IsFunctionOutputProperty(Property) && Candidate.ReturnType.IsEmpty())
		{
			Candidate.ReturnType = GetPropertySemanticType(Property);
		}
	}
	Candidate.Function = Function;
	Candidate.NodeSpawner = NodeSpawner;
	InOutCandidates.Add(StableId, Candidate);
}

static void AddActionDatabaseCandidates(
	const FBlueprintHelperCallFunctionResolveRequest& Request,
	TMap<FString, FBlueprintHelperCallFunctionCandidate>& InOutCandidates)
{
	FBlueprintActionContext Context;
	if (Request.Blueprint)
	{
		Context.Blueprints.Add(Request.Blueprint);
	}
	if (Request.Graph)
	{
		Context.Graphs.Add(Request.Graph);
	}

	FBlueprintActionFilter Filter(FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety);
	Filter.Context = Context;
	Filter.PermittedNodeTypes.Add(UK2Node_CallFunction::StaticClass());

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

			AddCandidateForFunction(const_cast<UFunction*>(AssociatedFunction), Request, InOutCandidates, Spawner);
		}
	}
}

static TArray<FBlueprintHelperCallFunctionCandidate> BuildCandidateUniverse(
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	TMap<FString, FBlueprintHelperCallFunctionCandidate> CandidateMap;
	UClass* RequestedTargetClass = ResolveClassByTypeName(Request.TargetObjectType);
	AddActionDatabaseCandidates(Request, CandidateMap);

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
	else if (Request.Blueprint)
	{
		AddClassFunctions(Request.Blueprint->SkeletonGeneratedClass);
		AddClassFunctions(Request.Blueprint->GeneratedClass);
		AddClassFunctions(Request.Blueprint->ParentClass);
	}

	TArray<FBlueprintHelperCallFunctionCandidate> Candidates;
	CandidateMap.GenerateValueArray(Candidates);
	return Candidates;
}

static int32 ApplyCategoryPriorityBonus(
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

static bool IsExactSearchMode(const FString& SearchMode)
{
	return SearchMode.Equals(TEXT("exact"), ESearchCase::IgnoreCase)
		|| SearchMode.Equals(TEXT("precise"), ESearchCase::IgnoreCase);
}

static int32 ScoreCandidate(
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

static void SortCandidates(TArray<FBlueprintHelperCallFunctionCandidate>& Candidates)
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

static FString BuildCandidateSummary(const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	return FString::Printf(
		TEXT("%s display=\"%s\" owner=\"%s\""),
		*Candidate.StableId,
		*Candidate.DisplayName,
		*Candidate.OwnerClassPath);
}

static FString JsonQuote(const FString& Value);

static FString BuildCandidateFunctionJsonString(const FBlueprintHelperCallFunctionCandidate& Candidate)
{
	TArray<FString> InputPins;
	for (const FString& InputPin : Candidate.InputPins)
	{
		InputPins.Add(JsonQuote(InputPin));
	}

	return FString::Printf(
		TEXT("{\"stable_id\":%s,\"display_name\":%s,\"owner\":%s,\"native_name\":%s,\"category\":%s,\"node_class\":%s,\"match_reason\":%s,\"return_type\":%s,\"score\":%d,\"graph_compatible\":%s,\"from_action_database\":%s,\"blueprint_callable\":%s,\"blueprint_pure\":%s,\"latent\":%s,\"requires_world_context\":%s,\"world_context_pin\":%s,\"target_object_pin\":%s,\"input_pins\":[%s]}"),
		*JsonQuote(Candidate.StableId),
		*JsonQuote(Candidate.DisplayName),
		*JsonQuote(Candidate.OwnerClassPath),
		*JsonQuote(Candidate.NativeFunctionName),
		*JsonQuote(Candidate.Category),
		*JsonQuote(Candidate.NodeClassPath),
		*JsonQuote(Candidate.MatchReason),
		*JsonQuote(Candidate.ReturnType),
		Candidate.Score,
		Candidate.bGraphCompatible ? TEXT("true") : TEXT("false"),
		Candidate.bFromActionDatabase ? TEXT("true") : TEXT("false"),
		Candidate.bBlueprintCallable ? TEXT("true") : TEXT("false"),
		Candidate.bBlueprintPure ? TEXT("true") : TEXT("false"),
		Candidate.bLatent ? TEXT("true") : TEXT("false"),
		Candidate.bRequiresWorldContext ? TEXT("true") : TEXT("false"),
		*JsonQuote(Candidate.WorldContextPin),
		*JsonQuote(Candidate.TargetObjectPin),
		*FString::Join(InputPins, TEXT(",")));
}

static FBlueprintHelperCallFunctionCandidateInfo BuildCandidateFunctionInfo(const FBlueprintHelperCallFunctionCandidate& Candidate)
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
	Info.Score = Candidate.Score;
	Info.bGraphCompatible = Candidate.bGraphCompatible;
	Info.bFromActionDatabase = Candidate.bFromActionDatabase;
	Info.bBlueprintCallable = Candidate.bBlueprintCallable;
	Info.bBlueprintPure = Candidate.bBlueprintPure;
	Info.bLatent = Candidate.bLatent;
	Info.bRequiresWorldContext = Candidate.bRequiresWorldContext;
	return Info;
}

static FString JsonQuote(const FString& Value)
{
	FString Escaped = Value;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
	Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
	Escaped.ReplaceInline(TEXT("\b"), TEXT("\\b"));
	Escaped.ReplaceInline(TEXT("\f"), TEXT("\\f"));
	return FString::Printf(TEXT("\"%s\""), *Escaped);
}

static FString BuildCandidateListMessage(
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
		TEXT("{\"target\":%s,\"candidates\":[%s]}"),
		*JsonQuote(TargetQuery),
		*FString::Join(CandidateFunctions, TEXT(",")));
	return FString::Printf(
		TEXT("%s candidate_functions=[%s] Candidates: %s"),
		*Prefix,
		*CandidateFunctionGroup,
		*FString::Join(Summaries, TEXT("; ")));
}

static void SetTopCandidates(
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

static bool HasOwnerCandidate(const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates, const FString& OwnerQuery)
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

static bool HasNativeDisplayConflict(const TArray<FBlueprintHelperCallFunctionCandidate>& Candidates)
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

static void PreferGeneratedClassOverSkeletonDuplicates(
	TArray<FBlueprintHelperCallFunctionCandidate>& Candidates,
	const UBlueprint* Blueprint)
{
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

static void PreferTargetBlueprintCandidates(
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
}

FBlueprintHelperCallFunctionResolveResult FBlueprintHelperCallFunctionResolver::Resolve(
	const FBlueprintHelperCallFunctionResolveRequest& Request)
{
	using namespace BlueprintHelperCallFunctionResolver;

	FBlueprintHelperCallFunctionResolveResult Result;
	const FString Query = Request.Query.TrimStartAndEnd();
	if (Query.IsEmpty())
	{
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Blocked;
		Result.ErrorCode = TEXT("function_call_not_found");
		Result.Message = TEXT("call_function.name is empty.");
		return Result;
	}

	FString QualifiedOwner;
	FString QualifiedFunction;
	const bool bQualifiedQuery = TryParseQualifiedQuery(Query, QualifiedOwner, QualifiedFunction);
	const bool bPickBest = Request.AmbiguityPolicy.Equals(TEXT("pick_best"), ESearchCase::IgnoreCase)
		|| Request.AmbiguityPolicy.Equals(TEXT("best"), ESearchCase::IgnoreCase);

	TArray<FBlueprintHelperCallFunctionCandidate> Candidates = BuildCandidateUniverse(Request);
	if (bQualifiedQuery && !HasOwnerCandidate(Candidates, QualifiedOwner) && !QualifiedOwner.StartsWith(TEXT("/Script/")))
	{
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Blocked;
		Result.ErrorCode = TEXT("explicit_member_call_not_supported");
		Result.Message = TEXT("call_function.name uses an explicit member prefix; first slice supports graph/self/library calls only.");
		return Result;
	}

	TArray<FBlueprintHelperCallFunctionCandidate> ScoredCandidates;
	for (FBlueprintHelperCallFunctionCandidate& Candidate : Candidates)
	{
		if (!PassesMetadataFilters(Candidate, Request))
		{
			continue;
		}

		FString MatchReason;
		const int32 Score = ScoreCandidate(Candidate, Query, QualifiedOwner, QualifiedFunction, Request, MatchReason);
		if (Score <= 0)
		{
			continue;
		}

		Candidate.Score = Score;
		Candidate.MatchReason = MatchReason;
		if (Candidate.bGraphCompatible)
		{
			ScoredCandidates.Add(Candidate);
		}
	}

	if (!bQualifiedQuery)
	{
		PreferTargetBlueprintCandidates(ScoredCandidates, Request.Blueprint);
		PreferGeneratedClassOverSkeletonDuplicates(ScoredCandidates, Request.Blueprint);
	}
	SortCandidates(ScoredCandidates);
	SetTopCandidates(Result, ScoredCandidates, Request.MaxCandidates);

	if (ScoredCandidates.Num() == 0)
	{
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::NotFound;
		Result.ErrorCode = TEXT("function_call_not_found");
		Result.Message = FString::Printf(TEXT("call_function resolve failed: no graph-compatible function matched '%s'."), *Query);
		return Result;
	}

	if (!bQualifiedQuery && HasNativeDisplayConflict(ScoredCandidates))
	{
		if (bPickBest)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = ScoredCandidates[0];
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *ScoredCandidates[0].StableId);
			return Result;
		}

		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Ambiguous;
		Result.ErrorCode = TEXT("ambiguous_function_call");
		Result.Message = BuildCandidateListMessage(
			FString::Printf(TEXT("call_function resolve failed: native and display-name matches conflict for '%s'."), *Query),
			Query,
			Result.Candidates);
		return Result;
	}

	const FBlueprintHelperCallFunctionCandidate& Top = ScoredCandidates[0];
	if (Top.Score >= 850)
	{
		if (bPickBest)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = Top;
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *Top.StableId);
			return Result;
		}

		for (int32 Index = 1; Index < ScoredCandidates.Num(); ++Index)
		{
			const FBlueprintHelperCallFunctionCandidate& Candidate = ScoredCandidates[Index];
			if (Candidate.Score == Top.Score && Candidate.StableId != Top.StableId)
			{
				Result.Status = EBlueprintHelperCallFunctionResolveStatus::Ambiguous;
				Result.ErrorCode = TEXT("ambiguous_function_call");
				Result.Message = BuildCandidateListMessage(
					FString::Printf(TEXT("call_function resolve failed: '%s' is ambiguous."), *Query),
					Query,
					Result.Candidates);
				return Result;
			}
		}

		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
		Result.Selected = Top;
		Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s."), *Query, *Top.StableId);
		return Result;
	}

	if (Top.Score >= 700)
	{
		if (bPickBest)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = Top;
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *Top.StableId);
			return Result;
		}

		int32 CompatibleAboveZero = 0;
		for (const FBlueprintHelperCallFunctionCandidate& Candidate : ScoredCandidates)
		{
			if (Candidate.Score > 0)
			{
				++CompatibleAboveZero;
			}
		}

		if (CompatibleAboveZero == 1)
		{
			Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
			Result.Selected = Top;
			Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s."), *Query, *Top.StableId);
			return Result;
		}
	}

	if (bPickBest)
	{
		Result.Status = EBlueprintHelperCallFunctionResolveStatus::Resolved;
		Result.Selected = Top;
		Result.Message = FString::Printf(TEXT("call_function resolved '%s' to %s by pick_best."), *Query, *Top.StableId);
		return Result;
	}

	Result.Status = EBlueprintHelperCallFunctionResolveStatus::Ambiguous;
	Result.ErrorCode = TEXT("ambiguous_function_call");
	Result.Message = BuildCandidateListMessage(
		FString::Printf(TEXT("call_function resolve failed: '%s' did not identify a unique function."), *Query),
		Query,
		Result.Candidates);
	return Result;
}

FString FBlueprintHelperCallFunctionResolver::MakeStableId(const UFunction* Function)
{
	const FString OwnerClassPath = BlueprintHelperCallFunctionResolver::GetOwnerClassPath(Function);
	return Function && !OwnerClassPath.IsEmpty()
		? FString::Printf(TEXT("%s:%s"), *OwnerClassPath, *Function->GetName())
		: FString();
}

bool FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(
	const FString& Query,
	FString& OutOwner,
	FString& OutFunction)
{
	OutOwner.Reset();
	OutFunction.Reset();

	const FString Trimmed = Query.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	int32 ColonIndex = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT(':'), ColonIndex) && ColonIndex > 0 && ColonIndex < Trimmed.Len() - 1)
	{
		OutOwner = Trimmed.Left(ColonIndex).TrimStartAndEnd();
		OutFunction = Trimmed.Mid(ColonIndex + 1).TrimStartAndEnd();
		return !OutOwner.IsEmpty() && !OutFunction.IsEmpty();
	}

	int32 DotIndex = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT('.'), DotIndex) && DotIndex > 0 && DotIndex < Trimmed.Len() - 1)
	{
		OutOwner = Trimmed.Left(DotIndex).TrimStartAndEnd();
		OutFunction = Trimmed.Mid(DotIndex + 1).TrimStartAndEnd();
		return !OutOwner.IsEmpty() && !OutFunction.IsEmpty();
	}

	return false;
}

UK2Node* FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
	UEdGraph* Graph,
	const FBlueprintHelperCallFunctionCandidate& Candidate,
	const FVector2D& Location,
	FString& OutError)
{
	OutError.Reset();
	if (!Graph)
	{
		OutError = TEXT("call_function spawn failed: target graph is invalid.");
		return nullptr;
	}

	UFunction* Function = Candidate.Function.Get();
	if (!Function)
	{
		OutError = TEXT("call_function spawn failed: resolved function is no longer valid.");
		return nullptr;
	}

	if (UBlueprintNodeSpawner* NodeSpawner = Candidate.NodeSpawner.Get())
	{
		IBlueprintNodeBinder::FBindingSet Bindings;
		UEdGraphNode* SpawnedNode = NodeSpawner->Invoke(Graph, Bindings, Location);
		UK2Node* K2Node = Cast<UK2Node>(SpawnedNode);
		if (!K2Node)
		{
			OutError = FString::Printf(TEXT("call_function spawn failed: action database spawner did not create a K2 node for %s."), *Candidate.StableId);
			return nullptr;
		}
		K2Node->NodePosX = static_cast<int32>(Location.X);
		K2Node->NodePosY = static_cast<int32>(Location.Y);
		if (Graph->GetSchema())
		{
			Graph->GetSchema()->ReconstructNode(*K2Node);
		}
		return K2Node;
	}

	OutError = FString::Printf(
		TEXT("call_function spawn failed: '%s' has no ActionDatabase NodeSpawner; legacy SetFromFunction fallback is disabled."),
		*Candidate.StableId);
	return nullptr;
}
