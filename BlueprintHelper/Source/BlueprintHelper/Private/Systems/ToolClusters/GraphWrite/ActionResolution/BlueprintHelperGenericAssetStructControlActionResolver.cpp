#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"

#include "BlueprintFieldNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_StructOperation.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace
{
static bool IsNodeSpawnerCandidateSemantic(const EBlueprintHelperActionSemanticKind Kind)
{
	return Kind == EBlueprintHelperActionSemanticKind::Construct
		|| Kind == EBlueprintHelperActionSemanticKind::Deconstruct;
}

static bool IsConstructSemantic(const FBlueprintHelperActionResolutionRequest& Request)
{
	return Request.Semantic.Kind == EBlueprintHelperActionSemanticKind::Construct;
}

static FString NormalizeNativeFunctionPath(const FString& RawPath)
{
	FString Path = RawPath.TrimStartAndEnd();
	if (Path.IsEmpty() || Path.Contains(TEXT(":")))
	{
		return Path;
	}

	int32 LastDotIndex = INDEX_NONE;
	if (Path.FindLastChar(TEXT('.'), LastDotIndex) && LastDotIndex > 0 && LastDotIndex < Path.Len() - 1)
	{
		Path = Path.Left(LastDotIndex) + TEXT(":") + Path.Mid(LastDotIndex + 1);
	}
	return Path;
}

static FString NormalizeStructLookupText(const FString& TypeName)
{
	FString Normalized = TypeName.TrimStartAndEnd();
	Normalized.ToLowerInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	if (Normalized.StartsWith(TEXT("struct")))
	{
		Normalized.RightChopInline(6);
	}
	if (Normalized.StartsWith(TEXT("f")))
	{
		Normalized.RightChopInline(1);
	}
	return Normalized;
}

static UScriptStruct* ResolveStructType(const FString& TypeName)
{
	const FString Query = TypeName.TrimStartAndEnd();
	if (Query.IsEmpty())
	{
		return nullptr;
	}

	if (UScriptStruct* DirectStruct = FindObject<UScriptStruct>(nullptr, *Query))
	{
		return DirectStruct;
	}
	if (UScriptStruct* LoadedStruct = LoadObject<UScriptStruct>(nullptr, *Query))
	{
		return LoadedStruct;
	}

	const FString NormalizedQuery = NormalizeStructLookupText(Query);
	if (NormalizedQuery.IsEmpty())
	{
		return nullptr;
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Candidate = *It;
		if (!Candidate)
		{
			continue;
		}

		FString CandidateName = Candidate->GetName();
		CandidateName.ToLowerInline();

		FString CandidateCppName = Candidate->GetPrefixCPP() + Candidate->GetName();
		CandidateCppName.ToLowerInline();

		FString CandidatePath = Candidate->GetPathName();
		CandidatePath.ToLowerInline();

		if (CandidateName == NormalizedQuery
			|| CandidateCppName == NormalizedQuery
			|| CandidatePath == NormalizedQuery
			|| CandidatePath.EndsWith(TEXT(".") + NormalizedQuery))
		{
			return Candidate;
		}
	}

	return nullptr;
}

static FString SemanticTypeFromProperty(const FProperty* Property)
{
	if (!Property)
	{
		return FString();
	}
	if (Property->IsA<FBoolProperty>())
	{
		return TEXT("bool");
	}
	if (Property->IsA<FIntProperty>() || Property->IsA<FUInt32Property>() || Property->IsA<FByteProperty>())
	{
		return TEXT("int");
	}
	if (Property->IsA<FInt64Property>())
	{
		return TEXT("int64");
	}
	if (Property->IsA<FFloatProperty>())
	{
		return TEXT("float");
	}
	if (Property->IsA<FDoubleProperty>())
	{
		return TEXT("double");
	}
	if (Property->IsA<FStrProperty>())
	{
		return TEXT("string");
	}
	if (Property->IsA<FNameProperty>())
	{
		return TEXT("name");
	}
	if (Property->IsA<FTextProperty>())
	{
		return TEXT("text");
	}
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return StructProperty->Struct ? StructProperty->Struct->GetPathName() : FString();
	}
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		return ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetPathName() : FString();
	}
	if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
	{
		return ClassProperty->MetaClass ? ClassProperty->MetaClass->GetPathName() : FString(TEXT("class"));
	}
	return FString();
}

static void AddUniqueQuery(TArray<FString>& Queries, const FString& Query)
{
	const FString Trimmed = Query.TrimStartAndEnd();
	if (!Trimmed.IsEmpty() && !Queries.Contains(Trimmed))
	{
		Queries.Add(Trimmed);
	}
}

static void AddUniqueCandidateInfo(
	TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
	const FBlueprintHelperCallFunctionCandidateInfo& Candidate)
{
	if (Candidate.StableId.IsEmpty())
	{
		CandidateActions.Add(Candidate);
		return;
	}

	for (const FBlueprintHelperCallFunctionCandidateInfo& Existing : CandidateActions)
	{
		if (Existing.StableId == Candidate.StableId)
		{
			return;
		}
	}
	CandidateActions.Add(Candidate);
}

static void AppendCandidateDiagnostics(
	TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
	const FBlueprintHelperActionResolutionResult& FunctionResult,
	const int32 MaxCandidates)
{
	for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : FunctionResult.CandidateActions)
	{
		if (MaxCandidates > 0 && CandidateActions.Num() >= MaxCandidates)
		{
			return;
		}
		AddUniqueCandidateInfo(CandidateActions, Candidate);
	}
}

static FProperty* FindStructPropertyBySemanticName(const UScriptStruct* TargetStruct, const FString& Name)
{
	if (!TargetStruct)
	{
		return nullptr;
	}

	if (FProperty* DirectProperty = FindFProperty<FProperty>(TargetStruct, *Name))
	{
		return DirectProperty;
	}

	for (TFieldIterator<FProperty> It(TargetStruct); It; ++It)
	{
		FProperty* Property = *It;
		if (Property
			&& (Property->GetName().Equals(Name, ESearchCase::IgnoreCase)
				|| Property->GetDisplayNameText().ToString().Equals(Name, ESearchCase::IgnoreCase)))
		{
			return Property;
		}
	}
	return nullptr;
}

static void PopulateFunctionArgumentConstraintsFromStruct(
	const UScriptStruct* TargetStruct,
	const FBlueprintHelperActionSemanticConstraints& SourceSemantic,
	FBlueprintHelperActionSemanticConstraints& FunctionSemantic)
{
	FunctionSemantic.ArgumentNames = SourceSemantic.ArgumentNames;
	FunctionSemantic.ArgumentTypes = SourceSemantic.ArgumentTypes;
	FunctionSemantic.ArgumentPinTypes = SourceSemantic.ArgumentPinTypes;

	TArray<FString> ArgumentTypeNames;
	SourceSemantic.ArgumentTypes.GetKeys(ArgumentTypeNames);
	for (const FString& ArgumentName : ArgumentTypeNames)
	{
		FunctionSemantic.ArgumentNames.AddUnique(ArgumentName);
	}

	TArray<FString> ArgumentPinTypeNames;
	SourceSemantic.ArgumentPinTypes.GetKeys(ArgumentPinTypeNames);
	for (const FString& ArgumentName : ArgumentPinTypeNames)
	{
		FunctionSemantic.ArgumentNames.AddUnique(ArgumentName);
	}

	for (const TPair<FString, FString>& DefaultPair : SourceSemantic.DefaultValues)
	{
		FunctionSemantic.ArgumentNames.AddUnique(DefaultPair.Key);
		if (!FunctionSemantic.ArgumentTypes.Contains(DefaultPair.Key))
		{
			const FString SemanticType = SemanticTypeFromProperty(FindStructPropertyBySemanticName(TargetStruct, DefaultPair.Key));
			if (!SemanticType.IsEmpty())
			{
				FunctionSemantic.ArgumentTypes.Add(DefaultPair.Key, SemanticType);
			}
		}
	}
}

static FBlueprintHelperActionResolutionRequest MakeFunctionActionRequest(
	const FBlueprintHelperActionResolutionRequest& Request,
	UScriptStruct* TargetStruct,
	const FString& Query,
	const FString& SearchMode,
	const bool bConstruct)
{
	FBlueprintHelperActionResolutionRequest FunctionRequest;
	FunctionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	FunctionRequest.Blueprint = Request.Blueprint ? Request.Blueprint : FBlueprintEditorUtils::FindBlueprintForGraph(Request.TargetGraph);
	FunctionRequest.TargetGraph = Request.TargetGraph;
	FunctionRequest.bAllowFuzzyUnique = Request.bAllowFuzzyUnique;
	FunctionRequest.MaxCandidates = Request.MaxCandidates;
	FunctionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Call;
	FunctionRequest.Semantic.Query = Query;
	FunctionRequest.Semantic.SearchMode = SearchMode;
	FunctionRequest.Semantic.AmbiguityPolicy = TEXT("pick_best");
	FunctionRequest.Semantic.CategoryPriority = Request.Semantic.CategoryPriority;
	FunctionRequest.Semantic.TargetObjectType = Request.Semantic.TargetObjectType;
	FunctionRequest.Semantic.TargetObjectPinType = Request.Semantic.TargetObjectPinType;

	PopulateFunctionArgumentConstraintsFromStruct(TargetStruct, Request.Semantic, FunctionRequest.Semantic);

	if (bConstruct)
	{
		FunctionRequest.Semantic.ExpectedReturnType = TargetStruct ? TargetStruct->GetPathName() : FString();
		FunctionRequest.Semantic.ExpectedReturnPinType = Request.Semantic.ExpectedReturnPinType;
	}

	return FunctionRequest;
}

static bool TryResolveFunctionActionSpawner(
	const FBlueprintHelperActionResolutionRequest& Request,
	UScriptStruct* TargetStruct,
	const FString& Query,
	const FString& SearchMode,
	const bool bConstruct,
	const FString& AttemptLabel,
	TArray<FString>& AttemptMessages,
	TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
	FBlueprintHelperActionResolutionResult& OutResult)
{
	const FString TrimmedQuery = Query.TrimStartAndEnd();
	if (TrimmedQuery.IsEmpty())
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest FunctionRequest =
		MakeFunctionActionRequest(Request, TargetStruct, TrimmedQuery, SearchMode, bConstruct);
	FBlueprintHelperActionResolutionResult FunctionResult = FBlueprintHelperActionResolutionCore::Resolve(FunctionRequest);
	AppendCandidateDiagnostics(CandidateActions, FunctionResult, Request.MaxCandidates);

	if (FunctionResult.IsResolved() && FunctionResult.SelectedSpawner.IsValid())
	{
		OutResult = FunctionResult;
		OutResult.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		OutResult.Message = FString::Printf(
			TEXT("Resolved %s for struct '%s' via %s FunctionAction query '%s': %s"),
			bConstruct ? TEXT("construct") : TEXT("deconstruct"),
			TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"),
			*AttemptLabel,
			*TrimmedQuery,
			*FunctionResult.Message);
		return true;
	}

	AttemptMessages.Add(FString::Printf(
		TEXT("%s query '%s' status=%d error=%s message=%s"),
		*AttemptLabel,
		*TrimmedQuery,
		static_cast<int32>(FunctionResult.Status),
		FunctionResult.ErrorCode.IsEmpty() ? TEXT("<none>") : *FunctionResult.ErrorCode,
		FunctionResult.Message.IsEmpty() ? TEXT("<none>") : *FunctionResult.Message));
	return false;
}

static FString GetNativeFunctionLookupName(const FString& NativePath)
{
	FString Trimmed = NativePath.TrimStartAndEnd();
	int32 SeparatorIndex = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT(':'), SeparatorIndex)
		|| Trimmed.FindLastChar(TEXT('.'), SeparatorIndex))
	{
		if (SeparatorIndex >= 0 && SeparatorIndex < Trimmed.Len() - 1)
		{
			return Trimmed.Mid(SeparatorIndex + 1);
		}
	}
	return Trimmed;
}

static FString GetNativeFunctionOwnerLookupName(const FString& NativePath)
{
	FString Trimmed = NativePath.TrimStartAndEnd();
	int32 SeparatorIndex = INDEX_NONE;
	if (Trimmed.FindLastChar(TEXT(':'), SeparatorIndex)
		|| Trimmed.FindLastChar(TEXT('.'), SeparatorIndex))
	{
		if (SeparatorIndex > 0)
		{
			return Trimmed.Left(SeparatorIndex);
		}
	}
	return FString();
}

static bool FunctionOwnerMatches(const UFunction* Function, const FString& OwnerLookup)
{
	if (!Function || OwnerLookup.IsEmpty())
	{
		return true;
	}

	const UClass* OwnerClass = Function->GetOwnerClass();
	if (!OwnerClass)
	{
		return false;
	}

	const FString OwnerName = OwnerClass->GetName();
	const FString OwnerPath = OwnerClass->GetPathName();
	return OwnerLookup.EndsWith(OwnerName)
		|| OwnerPath.EndsWith(OwnerLookup)
		|| OwnerPath.Contains(OwnerLookup);
}

static UFunction* FindNativeStructFunction(const FString& NativePath)
{
	const FString FunctionName = GetNativeFunctionLookupName(NativePath);
	if (FunctionName.IsEmpty())
	{
		return nullptr;
	}

	const FString OwnerLookup = GetNativeFunctionOwnerLookupName(NativePath);
	for (TObjectIterator<UFunction> It; It; ++It)
	{
		UFunction* Function = *It;
		if (Function
			&& Function->GetName().Equals(FunctionName, ESearchCase::IgnoreCase)
			&& FunctionOwnerMatches(Function, OwnerLookup))
		{
			return Function;
		}
	}
	return nullptr;
}

static FBlueprintHelperCallFunctionCandidateInfo MakeNativeStructFunctionCandidateInfo(
	const UFunction* Function,
	const bool bConstruct,
	const UScriptStruct* TargetStruct)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = FString::Printf(
		TEXT("%s:%s"),
		Function && Function->GetOwnerClass() ? *Function->GetOwnerClass()->GetPathName() : TEXT("<owner>"),
		Function ? *Function->GetName() : TEXT("<function>"));
	Candidate.DisplayName = Function ? Function->GetName() : FString();
	Candidate.OwnerClassPath = Function && Function->GetOwnerClass() ? Function->GetOwnerClass()->GetPathName() : FString();
	Candidate.NativeFunctionName = Function ? Function->GetName() : FString();
	Candidate.Category = TEXT("Struct");
	Candidate.NodeClassPath = UBlueprintFunctionNodeSpawner::StaticClass()->GetPathName();
	Candidate.MatchReason = bConstruct ? TEXT("native_make_function_spawner") : TEXT("native_break_function_spawner");
	Candidate.ReturnType = bConstruct && TargetStruct ? TargetStruct->GetPathName() : FString();
	Candidate.Score = 100;
	Candidate.bGraphCompatible = true;
	Candidate.bFromActionDatabase = false;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = Function ? Function->HasAnyFunctionFlags(FUNC_BlueprintPure) : true;
	return Candidate;
}

static bool TryResolveNativeStructFunctionSpawner(
	const FString& NativeFunctionPath,
	const FBlueprintHelperActionResolutionRequest& Request,
	UScriptStruct* TargetStruct,
	const bool bConstruct,
	TArray<FBlueprintHelperCallFunctionCandidateInfo>& CandidateActions,
	FBlueprintHelperActionResolutionResult& OutResult)
{
	UFunction* NativeFunction = FindNativeStructFunction(NativeFunctionPath);
	if (!NativeFunction)
	{
		return false;
	}

	UBlueprintFunctionNodeSpawner* NativeSpawner = UBlueprintFunctionNodeSpawner::Create(NativeFunction);
	if (!NativeSpawner)
	{
		return false;
	}

	FBlueprintHelperCallFunctionCandidateInfo Candidate =
		MakeNativeStructFunctionCandidateInfo(NativeFunction, bConstruct, TargetStruct);
	AddUniqueCandidateInfo(CandidateActions, Candidate);

	OutResult = FBlueprintHelperActionResolutionResult();
	OutResult.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	OutResult.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	OutResult.SelectedStableId = Candidate.StableId;
	OutResult.SelectedSpawner = NativeSpawner;
	OutResult.SelectedFunction = NativeFunction;
	OutResult.CandidateActions = CandidateActions;
	OutResult.Message = FString::Printf(
		TEXT("Resolved %s for struct '%s' via native function spawner '%s'."),
		bConstruct ? TEXT("construct") : TEXT("deconstruct"),
		TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"),
		*Candidate.StableId);
	return true;
}

static void SetStructTypeOnNode(UEdGraphNode* NewNode, FFieldVariant /*StructField*/, TWeakObjectPtr<UScriptStruct> StructPtr)
{
	if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(NewNode))
	{
		StructNode->StructType = StructPtr.Get();
	}
}

static UBlueprintFieldNodeSpawner* CreateDirectStructSpawner(UClass* NodeClass, UScriptStruct* TargetStruct)
{
	if (!NodeClass || !TargetStruct)
	{
		return nullptr;
	}

	UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(NodeClass, TargetStruct);
	if (!NodeSpawner)
	{
		return nullptr;
	}

	NodeSpawner->SetNodeFieldDelegate = UBlueprintFieldNodeSpawner::FSetNodeFieldDelegate::CreateStatic(
		&SetStructTypeOnNode,
		MakeWeakObjectPtr(TargetStruct));
	return NodeSpawner;
}

static FString MakeDirectStructStableId(const bool bConstruct, const UScriptStruct* TargetStruct)
{
	return FString::Printf(
		TEXT("direct_%s_struct:%s"),
		bConstruct ? TEXT("make") : TEXT("break"),
		TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"));
}

static FBlueprintHelperCallFunctionCandidateInfo MakeDirectStructCandidateInfo(
	const bool bConstruct,
	const UScriptStruct* TargetStruct,
	const UClass* NodeClass)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = MakeDirectStructStableId(bConstruct, TargetStruct);
	Candidate.DisplayName = FString::Printf(
		TEXT("%s %s"),
		bConstruct ? TEXT("Make") : TEXT("Break"),
		TargetStruct ? *TargetStruct->GetDisplayNameText().ToString() : TEXT("<null>"));
	Candidate.Category = TEXT("Struct");
	Candidate.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
	Candidate.MatchReason = bConstruct ? TEXT("direct_make_struct_field_spawner") : TEXT("direct_break_struct_field_spawner");
	Candidate.ReturnType = bConstruct && TargetStruct ? TargetStruct->GetPathName() : FString();
	Candidate.Score = 100;
	Candidate.bGraphCompatible = true;
	Candidate.bFromActionDatabase = false;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = true;
	if (!bConstruct && TargetStruct)
	{
		Candidate.InputPins.Add(TargetStruct->GetName());
		Candidate.InputPinTypes.Add(TargetStruct->GetName(), TargetStruct->GetPathName());
	}
	return Candidate;
}

static FBlueprintHelperActionResolutionResult MakeDirectStructSpawnerResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	UScriptStruct* TargetStruct,
	const bool bConstruct,
	const TArray<FString>& AttemptMessages,
	const TArray<FBlueprintHelperCallFunctionCandidateInfo>& FunctionCandidateActions)
{
	UClass* NodeClass = bConstruct ? UK2Node_MakeStruct::StaticClass() : UK2Node_BreakStruct::StaticClass();
	UBlueprintFieldNodeSpawner* DirectSpawner = CreateDirectStructSpawner(NodeClass, TargetStruct);

	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.CandidateActions = FunctionCandidateActions;
	AddUniqueCandidateInfo(Result.CandidateActions, MakeDirectStructCandidateInfo(bConstruct, TargetStruct, NodeClass));

	if (!DirectSpawner)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
		Result.ErrorCode = bConstruct ? TEXT("direct_make_struct_spawner_unavailable") : TEXT("direct_break_struct_spawner_unavailable");
		Result.Message = FString::Printf(
			TEXT("Could not create direct %s struct UBlueprintFieldNodeSpawner for '%s'."),
			bConstruct ? TEXT("make") : TEXT("break"),
			TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"));
		return Result;
	}

	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.Message = FString::Printf(
		TEXT("Resolved %s for struct '%s' to direct %s UBlueprintFieldNodeSpawner after FunctionAction attempts were not applicable%s%s"),
		bConstruct ? TEXT("construct") : TEXT("deconstruct"),
		TargetStruct ? *TargetStruct->GetPathName() : TEXT("<null>"),
		bConstruct ? TEXT("MakeStruct") : TEXT("BreakStruct"),
		AttemptMessages.Num() > 0 ? TEXT(": ") : TEXT("."),
		AttemptMessages.Num() > 0 ? *FString::Join(AttemptMessages, TEXT(" | ")) : TEXT(""));
	Result.SelectedStableId = MakeDirectStructStableId(bConstruct, TargetStruct);
	Result.SelectedSpawner = DirectSpawner;
	return Result;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	if (!IsNodeSpawnerCandidateSemantic(Request.Semantic.Kind))
	{
		return MakeUnsupportedIntentResult(
			Request,
			TEXT("Generic NodeSpawner candidate resolver only accepts construct/deconstruct semantics."));
	}

	const FString TypeName = Request.Semantic.TypeName.TrimStartAndEnd();
	if (TypeName.IsEmpty())
	{
		return MakeNeedsContextResult(
			Request,
			TEXT("Semantic.TypeName is required to resolve construct/deconstruct NodeSpawner candidates."));
	}

	UScriptStruct* TargetStruct = ResolveStructType(TypeName);
	if (!TargetStruct)
	{
		return MakeStructTypeNotFoundResult(
			Request,
			FString::Printf(TEXT("Could not resolve Semantic.TypeName '%s' to a UScriptStruct."), *TypeName));
	}

	const bool bConstruct = IsConstructSemantic(Request);
	TArray<FString> AttemptMessages;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateActions;
	FBlueprintHelperActionResolutionResult FunctionResolvedResult;

	const FString NativeFunctionPath = TargetStruct->GetMetaData(bConstruct ? TEXT("HasNativeMake") : TEXT("HasNativeBreak"));
	const FString NormalizedNativeFunctionPath = NormalizeNativeFunctionPath(NativeFunctionPath);
	if (TryResolveFunctionActionSpawner(
		Request,
		TargetStruct,
		NormalizedNativeFunctionPath,
		TEXT("exact"),
		bConstruct,
		bConstruct ? TEXT("native_make") : TEXT("native_break"),
		AttemptMessages,
		CandidateActions,
		FunctionResolvedResult))
	{
		FunctionResolvedResult.CandidateActions = CandidateActions;
		return FunctionResolvedResult;
	}
	if (TryResolveNativeStructFunctionSpawner(
		NormalizedNativeFunctionPath,
		Request,
		TargetStruct,
		bConstruct,
		CandidateActions,
		FunctionResolvedResult))
	{
		return FunctionResolvedResult;
	}

	TArray<FString> SearchQueries;
	const FString DisplayName = TargetStruct->GetDisplayNameText().ToString();
	const FString StructName = TargetStruct->GetName();
	AddUniqueQuery(SearchQueries, FString::Printf(TEXT("%s %s"), bConstruct ? TEXT("Make") : TEXT("Break"), *DisplayName));
	AddUniqueQuery(SearchQueries, FString::Printf(TEXT("%s %s"), bConstruct ? TEXT("Make") : TEXT("Break"), *StructName));

	for (const FString& SearchQuery : SearchQueries)
	{
		if (TryResolveFunctionActionSpawner(
			Request,
			TargetStruct,
			SearchQuery,
			Request.Semantic.SearchMode.IsEmpty() ? FString(TEXT("ue_search")) : Request.Semantic.SearchMode,
			bConstruct,
			bConstruct ? TEXT("search_make") : TEXT("search_break"),
			AttemptMessages,
			CandidateActions,
			FunctionResolvedResult))
		{
			FunctionResolvedResult.CandidateActions = CandidateActions;
			return FunctionResolvedResult;
		}
	}

	return MakeDirectStructSpawnerResult(Request, TargetStruct, bConstruct, AttemptMessages, CandidateActions);
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::MakeNeedsContextResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = Message;
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::MakeStructTypeNotFoundResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("generic_action_struct_type_not_found");
	Result.Message = Message;
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::MakeUnsupportedIntentResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_generic_node_spawner_candidate_semantic");
	Result.Message = Message;
	return Result;
}
