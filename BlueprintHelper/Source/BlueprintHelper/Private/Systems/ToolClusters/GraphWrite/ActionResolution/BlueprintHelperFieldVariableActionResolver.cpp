#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.h"

#include "BlueprintVariableNodeSpawner.h"
#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/MemberReference.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldPathResolution.h"
#include "UObject/UnrealType.h"

namespace
{
struct FBlueprintHelperVariableActionCandidate
{
	FBlueprintHelperCallFunctionCandidateInfo Info;
	TWeakObjectPtr<UBlueprintNodeSpawner> Spawner;
};

struct FBlueprintHelperResolvedFieldIdentity
{
	FString CapabilityId;
	FString FieldKind;
	FString OwnerClassPath;
	FString MemberName;
	FGuid MemberGuid;
	FString LocalScopeName;
	FString FunctionName;
	FString TargetPinRef;
	FString TargetPinCategory;
	FString TargetPinObjectPath;
	FString ExpectedNodeFamily;
	FString ExpectedNodeClassPath;
	FString DiagnosticReason;
};

static FString NormalizeFieldVariableToken(const FString& Value)
{
	FString Normalized = Value.TrimStartAndEnd().ToLower();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	return Normalized;
}

static FString NormalizeFieldBoundaryToken(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static FString FieldCapabilityRootKindToString(const EBlueprintHelperFieldCapabilityRootKind RootKind)
{
	switch (RootKind)
	{
	case EBlueprintHelperFieldCapabilityRootKind::Member:
		return TEXT("member");
	case EBlueprintHelperFieldCapabilityRootKind::InheritedMember:
		return TEXT("inherited_member");
	case EBlueprintHelperFieldCapabilityRootKind::SparseData:
		return TEXT("sparse_data");
	case EBlueprintHelperFieldCapabilityRootKind::FunctionParam:
		return TEXT("function_param");
	case EBlueprintHelperFieldCapabilityRootKind::Local:
		return TEXT("local");
	case EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember:
		return TEXT("object_pin_member");
	case EBlueprintHelperFieldCapabilityRootKind::ComponentRef:
		return TEXT("component_ref");
	case EBlueprintHelperFieldCapabilityRootKind::ComponentProperty:
		return TEXT("component_property");
	case EBlueprintHelperFieldCapabilityRootKind::StructMember:
		return TEXT("struct_member");
	case EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath:
		return TEXT("nested_property_path");
	default:
		return TEXT("unsupported");
	}
}

static bool FieldCapabilityWrites(const FBlueprintHelperFieldCapabilitySpec& Spec)
{
	return Spec.AccessMode == EBlueprintHelperFieldCapabilityAccessMode::Set;
}

static const FBlueprintHelperFieldCapabilitySpec* ResolveFieldCapabilitySpecForRequest(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	if (!Request.Semantic.CapabilityId.TrimStartAndEnd().IsEmpty())
	{
		return FBlueprintHelperFieldCapabilityRegistry::FindById(Request.Semantic.CapabilityId);
	}

	if (const FBlueprintHelperFieldCapabilitySpec* InferredSpec =
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(
			Request.Semantic.FieldOperation,
			Request.Semantic.FieldScope))
	{
		if (!InferredSpec->bRequiresTargetPin
			|| !Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref")).TrimStartAndEnd().IsEmpty())
		{
			return InferredSpec;
		}
	}

	const FString Operation = NormalizeFieldBoundaryToken(Request.Semantic.FieldOperation);
	const FString Scope = NormalizeFieldBoundaryToken(Request.Semantic.FieldScope);
	if (Operation == TEXT("get") && (Scope == TEXT("variable") || Scope == TEXT("property_path")))
	{
		return FBlueprintHelperFieldCapabilityRegistry::FindById(TEXT("field.member_get"));
	}
	if (Operation == TEXT("set") && (Scope == TEXT("variable") || Scope == TEXT("property_path")))
	{
		return FBlueprintHelperFieldCapabilityRegistry::FindById(TEXT("field.member_set"));
	}
	return nullptr;
}

static bool IsFunctionScopedCapability(const FBlueprintHelperFieldCapabilitySpec& Spec)
{
	return Spec.RootKind == EBlueprintHelperFieldCapabilityRootKind::Local
		|| Spec.RootKind == EBlueprintHelperFieldCapabilityRootKind::FunctionParam
		|| Spec.bRequiresFunctionScope;
}

static FString DescribePinType(const FEdGraphPinType& PinType)
{
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		return TEXT("bool");
	}
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		return TEXT("int");
	}
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		return TEXT("string");
	}
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		return TEXT("name");
	}
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		return TEXT("text");
	}
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real && PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
	{
		return TEXT("float");
	}
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real && PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
	{
		return TEXT("double");
	}

	FString Result = PinType.PinCategory.ToString();
	if (!PinType.PinSubCategory.IsNone())
	{
		Result += TEXT(".");
		Result += PinType.PinSubCategory.ToString();
	}
	if (PinType.PinSubCategoryObject.IsValid())
	{
		Result += TEXT(":");
		Result += PinType.PinSubCategoryObject->GetPathName();
	}
	return Result;
}

static bool IsComponentRefFieldScope(const FString& FieldScope)
{
	return NormalizeFieldBoundaryToken(FieldScope) == TEXT("component_ref");
}

static bool IsFieldAccessFieldScope(const FString& FieldScope)
{
	return NormalizeFieldBoundaryToken(FieldScope) == TEXT("field_access");
}

static FString GetEvidenceValue(const TMap<FString, FString>& Evidence, const TCHAR* Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static void AddCapabilityFactIfPresent(
	FBlueprintHelperActionResolutionRequest& Request,
	const FString& Key,
	const FString& Value)
{
	const FString CleanValue = Value.TrimStartAndEnd();
	if (!Key.IsEmpty() && !CleanValue.IsEmpty())
	{
		Request.Semantic.CapabilityFacts.FindOrAdd(Key, CleanValue);
	}
}

static FString CapabilityFactOrEvidence(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TMap<FString, FString>& Evidence,
	const FString& FactKey,
	const TCHAR* EvidenceKey)
{
	const FString FactValue = Request.Semantic.CapabilityFacts.FindRef(FactKey).TrimStartAndEnd();
	return !FactValue.IsEmpty() ? FactValue : GetEvidenceValue(Evidence, EvidenceKey);
}

static FString CapabilityFactOrEvidence(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TMap<FString, FString>& Evidence,
	const FString& FactKey,
	const TCHAR* FirstEvidenceKey,
	const TCHAR* SecondEvidenceKey)
{
	const FString FactValue = Request.Semantic.CapabilityFacts.FindRef(FactKey).TrimStartAndEnd();
	if (!FactValue.IsEmpty())
	{
		return FactValue;
	}
	const FString FirstEvidenceValue = GetEvidenceValue(Evidence, FirstEvidenceKey);
	return !FirstEvidenceValue.IsEmpty() ? FirstEvidenceValue : GetEvidenceValue(Evidence, SecondEvidenceKey);
}

static void BackfillCapabilityFactsFromEvidence(
	FBlueprintHelperActionResolutionRequest& Request,
	const TMap<FString, FString>& Evidence)
{
	AddCapabilityFactIfPresent(Request, TEXT("field.member_name"), GetEvidenceValue(Evidence, TEXT("field_name")));
	AddCapabilityFactIfPresent(Request, TEXT("field.owner_class"), GetEvidenceValue(Evidence, TEXT("field_owner_class")));
	AddCapabilityFactIfPresent(Request, TEXT("field.property_path"), GetEvidenceValue(Evidence, TEXT("property_path")));
	AddCapabilityFactIfPresent(Request, TEXT("field.component_name"), GetEvidenceValue(Evidence, TEXT("component_name")));
	AddCapabilityFactIfPresent(Request, TEXT("field.component_name"), GetEvidenceValue(Evidence, TEXT("component_property_name")));
	AddCapabilityFactIfPresent(Request, TEXT("field.component_owner_class"), GetEvidenceValue(Evidence, TEXT("component_owner_class")));
	AddCapabilityFactIfPresent(Request, TEXT("field.component_owner_class"), GetEvidenceValue(Evidence, TEXT("component_binding_owner_class_path")));
	AddCapabilityFactIfPresent(Request, TEXT("field.component_kind"), GetEvidenceValue(Evidence, TEXT("component_kind")));
	AddCapabilityFactIfPresent(Request, TEXT("field.target_pin_ref"), GetEvidenceValue(Evidence, TEXT("target_pin_ref")));
	AddCapabilityFactIfPresent(Request, TEXT("field.target_pin_type"), GetEvidenceValue(Evidence, TEXT("linked_pin_type_category")));
	AddCapabilityFactIfPresent(Request, TEXT("field.target_pin_object_path"), GetEvidenceValue(Evidence, TEXT("linked_pin_type_object_path")));

	const FString TargetPinCategory = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type")).TrimStartAndEnd();
	const FString TargetPinObjectPath = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")).TrimStartAndEnd();
	if (!Request.Semantic.TargetObjectPinType.IsValid() && (!TargetPinCategory.IsEmpty() || !TargetPinObjectPath.IsEmpty()))
	{
		Request.Semantic.TargetObjectPinType.Category = TargetPinCategory;
		Request.Semantic.TargetObjectPinType.ObjectPath = TargetPinObjectPath;
	}
	if (Request.Semantic.TargetObjectType.TrimStartAndEnd().IsEmpty() && !TargetPinObjectPath.IsEmpty())
	{
		Request.Semantic.TargetObjectType = TargetPinObjectPath;
	}
}

static FBlueprintHelperActionResolutionResult MakeFieldMissingEvidenceResult(const FString& Message, const FString& ErrorCode = TEXT("missing_required_evidence"))
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static FString FirstNonEmptyFieldValue(const FString& First, const FString& Second)
{
	return !First.TrimStartAndEnd().IsEmpty() ? First.TrimStartAndEnd() : Second.TrimStartAndEnd();
}

static FString FirstNonEmptyFieldValue(const FString& First, const FString& Second, const FString& Third)
{
	return FirstNonEmptyFieldValue(FirstNonEmptyFieldValue(First, Second), Third);
}

static bool TypeMatches(const FString& ExpectedType, const FBlueprintHelperCallFunctionPinType& ExpectedPinType, const FEdGraphPinType& CandidateType)
{
	if (!ExpectedType.TrimStartAndEnd().IsEmpty())
	{
		const FString CandidateTypeText = DescribePinType(CandidateType);
		const FString Expected = NormalizeFieldVariableToken(ExpectedType);
		const FString Candidate = NormalizeFieldVariableToken(CandidateTypeText);
		return Candidate.Contains(Expected)
			|| Expected.Contains(Candidate)
			|| NormalizeFieldVariableToken(CandidateType.PinCategory.ToString()) == Expected;
	}

	if (ExpectedPinType.IsValid())
	{
		if (!ExpectedPinType.Category.IsEmpty()
			&& !CandidateType.PinCategory.ToString().Equals(ExpectedPinType.Category, ESearchCase::IgnoreCase))
		{
			return false;
		}
		if (!ExpectedPinType.SubCategory.IsEmpty()
			&& !CandidateType.PinSubCategory.ToString().Equals(ExpectedPinType.SubCategory, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	return true;
}

static UClass* ResolveOwnerClass(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return nullptr;
	}
	if (Blueprint->SkeletonGeneratedClass)
	{
		return Blueprint->SkeletonGeneratedClass;
	}
	if (Blueprint->GeneratedClass)
	{
		return Blueprint->GeneratedClass;
	}
	return Blueprint->ParentClass;
}

static FString ResolveOwnerClassPath(const UBlueprint* Blueprint, const UClass* OwnerClass)
{
	if (OwnerClass)
	{
		return OwnerClass->GetPathName();
	}
	if (Blueprint && Blueprint->GeneratedClass)
	{
		return Blueprint->GeneratedClass->GetPathName();
	}
	if (Blueprint && Blueprint->SkeletonGeneratedClass)
	{
		return Blueprint->SkeletonGeneratedClass->GetPathName();
	}
	if (Blueprint && Blueprint->ParentClass)
	{
		return Blueprint->ParentClass->GetPathName();
	}
	return FString();
}

static const FProperty* FindVariableProperty(UBlueprint* Blueprint, const FName VariableName)
{
	if (!Blueprint || VariableName.IsNone())
	{
		return nullptr;
	}

	if (Blueprint->SkeletonGeneratedClass)
	{
		if (const FProperty* Property = FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, VariableName))
		{
			return Property;
		}
	}
	if (Blueprint->GeneratedClass)
	{
		if (const FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VariableName))
		{
			return Property;
		}
	}
	return Blueprint->ParentClass ? FindFProperty<FProperty>(Blueprint->ParentClass, VariableName) : nullptr;
}

static UClass* FindClassByPath(const FString& ClassPath)
{
	const FString CleanPath = ClassPath.TrimStartAndEnd();
	return CleanPath.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *CleanPath);
}

static const FProperty* FindPropertyOnClass(UClass* OwnerClass, const FName PropertyName)
{
	if (!OwnerClass || PropertyName.IsNone())
	{
		return nullptr;
	}

	for (UClass* Class = OwnerClass; Class; Class = Class->GetSuperClass())
	{
		if (const FProperty* Property = FindFProperty<FProperty>(Class, PropertyName))
		{
			return Property;
		}
	}
	return nullptr;
}

static const FProperty* ResolveFieldProperty(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperFieldCapabilitySpec* CapabilitySpec,
	const FString& FieldName)
{
	const FName FieldFName(*FieldName.TrimStartAndEnd());
	if (FieldFName.IsNone())
	{
		return nullptr;
	}

	if (CapabilitySpec && CapabilitySpec->bRequiresTargetPin)
	{
		const FString OwnerClassPath = FirstNonEmptyFieldValue(
			Request.Semantic.CapabilityFacts.FindRef(TEXT("field.owner_class")),
			Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path")));
		if (const FProperty* TargetProperty = FindPropertyOnClass(FindClassByPath(OwnerClassPath), FieldFName))
		{
			return TargetProperty;
		}
	}

	return FindVariableProperty(Request.Blueprint, FieldFName);
}

static bool ResolveFunctionScope(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TMap<FString, FString>& Evidence,
	const FBlueprintHelperFieldCapabilitySpec& Spec,
	FString& OutScopeName,
	FBlueprintHelperActionResolutionResult& OutResult)
{
	if (!IsFunctionScopedCapability(Spec))
	{
		return true;
	}

	OutScopeName = FirstNonEmptyFieldValue(
		Request.Semantic.CapabilityFacts.FindRef(TEXT("field.function_name")),
		Request.Semantic.CapabilityFacts.FindRef(TEXT("field.local_scope")),
		GetEvidenceValue(Evidence, TEXT("local_scope")));
	if (OutScopeName.IsEmpty() && Request.TargetGraph)
	{
		OutScopeName = Request.TargetGraph->GetName();
	}

	if (!Request.TargetGraph || !FBlueprintEditorUtils::DoesSupportLocalVariables(Request.TargetGraph))
	{
		OutResult.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		OutResult.ErrorCode = TEXT("missing_or_mismatched_function_scope");
		OutResult.Message = TEXT("Field capability requires a function graph scope.");
		return false;
	}

	if (!OutScopeName.IsEmpty() && !Request.TargetGraph->GetName().Equals(OutScopeName, ESearchCase::IgnoreCase))
	{
		OutResult.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		OutResult.ErrorCode = TEXT("missing_or_mismatched_function_scope");
		OutResult.Message = FString::Printf(
			TEXT("Field capability scope mismatch: expected=%s target_graph=%s."),
			*OutScopeName,
			*Request.TargetGraph->GetName());
		return false;
	}
	return true;
}

static FBPVariableDescription* FindLocalVariableDescription(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& FieldName,
	UK2Node_FunctionEntry** OutEntryNode = nullptr)
{
	if (!Request.Blueprint || !Request.TargetGraph || FieldName.TrimStartAndEnd().IsEmpty())
	{
		return nullptr;
	}
	return FBlueprintEditorUtils::FindLocalVariable(
		Request.Blueprint,
		Request.TargetGraph,
		FName(*FieldName.TrimStartAndEnd()),
		OutEntryNode);
}

static FProperty* ResolveLocalVariableProperty(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBPVariableDescription& LocalVariable)
{
	if (!Request.Blueprint || !Request.Blueprint->SkeletonGeneratedClass || !Request.TargetGraph)
	{
		return nullptr;
	}

	FMemberReference Reference;
	Reference.SetLocalMember(LocalVariable.VarName, Request.TargetGraph->GetName(), LocalVariable.VarGuid);
	return Reference.ResolveMember<FProperty>(Request.Blueprint->SkeletonGeneratedClass);
}

static UFunction* FindFunctionForGraph(UBlueprint* Blueprint, UEdGraph* FunctionGraph)
{
	if (!Blueprint || !FunctionGraph)
	{
		return nullptr;
	}

	if (Blueprint->SkeletonGeneratedClass)
	{
		if (UFunction* Function = FindUField<UFunction>(Blueprint->SkeletonGeneratedClass, FunctionGraph->GetFName()))
		{
			return Function;
		}
	}
	if (Blueprint->GeneratedClass)
	{
		if (UFunction* Function = FindUField<UFunction>(Blueprint->GeneratedClass, FunctionGraph->GetFName()))
		{
			return Function;
		}
	}
	return Blueprint->ParentClass ? FindUField<UFunction>(Blueprint->ParentClass, FunctionGraph->GetFName()) : nullptr;
}

static bool IsDisallowedFunctionParam(const FProperty* Param, const FString& ParamFlags)
{
	return !Param
		|| Param->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
		|| ParamFlags.Contains(TEXT("ReturnParm"))
		|| ParamFlags.Contains(TEXT("OutParm"))
		|| ParamFlags.Contains(TEXT("ReferenceParm"));
}

static FProperty* FindFunctionInputParameter(
	UFunction* Function,
	const FString& FieldName,
	const FString& ParamFlags,
	FString& OutErrorCode)
{
	if (!Function || FieldName.TrimStartAndEnd().IsEmpty())
	{
		OutErrorCode = TEXT("function_param_not_found");
		return nullptr;
	}

	const FName ParamName(*FieldName.TrimStartAndEnd());
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt && ParamIt->HasAnyPropertyFlags(CPF_Parm); ++ParamIt)
	{
		FProperty* Param = *ParamIt;
		if (!Param || Param->GetFName() != ParamName)
		{
			continue;
		}

		if (IsDisallowedFunctionParam(Param, ParamFlags))
		{
			OutErrorCode = TEXT("function_output_param_belongs_to_control_return");
			return nullptr;
		}
		return Param;
	}

	OutErrorCode = TEXT("function_param_not_found");
	return nullptr;
}

static bool ConvertPropertyToPinType(const FProperty* Property, FEdGraphPinType& OutPinType)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	return Schema && Schema->ConvertPropertyToPinType(Property, OutPinType);
}

static int32 ScoreVariableCandidate(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBPVariableDescription& Variable)
{
	const FString VariableName = Variable.VarName.ToString();
	const FString Query = Request.Semantic.Query.TrimStartAndEnd();
	const FString TargetPath = Request.Semantic.TargetPath.TrimStartAndEnd();
	const FString Wanted = !Query.IsEmpty() ? Query : TargetPath;
	const FString NormalizedVariable = NormalizeFieldVariableToken(VariableName);
	const FString NormalizedWanted = NormalizeFieldVariableToken(Wanted);

	int32 Score = 0;
	if (NormalizedWanted.IsEmpty())
	{
		Score += 10;
	}
	else if (NormalizedVariable == NormalizedWanted)
	{
		Score += 100;
	}
	else if (Request.bAllowFuzzyUnique && NormalizedVariable.Contains(NormalizedWanted))
	{
		Score += 55;
	}
	else
	{
		return INDEX_NONE;
	}

	if (!TargetPath.IsEmpty() && NormalizeFieldVariableToken(TargetPath) == NormalizedVariable)
	{
		Score += 20;
	}

	if (!TypeMatches(Request.Semantic.ExpectedReturnType, Request.Semantic.ExpectedReturnPinType, Variable.VarType))
	{
		return INDEX_NONE;
	}

	Score += 15;
	return Score;
}

static FString MakeVariableStableId(
	const UBlueprint* Blueprint,
	const FString& FieldName,
	const FString& FieldOperation,
	const FString& FieldScope,
	const FString& Prefix = TEXT("field_variable"))
{
	return FString::Printf(
		TEXT("%s:%s:%s:field:%s:%s"),
		*Prefix,
		Blueprint ? *Blueprint->GetPathName() : TEXT("unknown_blueprint"),
		*FieldName,
		*NormalizeFieldBoundaryToken(FieldOperation),
		*NormalizeFieldBoundaryToken(FieldScope));
}

static FBlueprintHelperCallFunctionCandidateInfo BuildCandidateInfo(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBPVariableDescription& Variable,
	const UClass* OwnerClass,
	const UClass* NodeClass,
	const int32 Score)
{
	FBlueprintHelperCallFunctionCandidateInfo Info;
	Info.StableId = MakeVariableStableId(
		Request.Blueprint,
		Variable.VarName.ToString(),
		Request.Semantic.FieldOperation,
		Request.Semantic.FieldScope);
	Info.DisplayName = Variable.VarName.ToString();
	Info.OwnerClassPath = OwnerClass ? OwnerClass->GetPathName() : FString();
	Info.NativeFunctionName = Variable.VarName.ToString();
	Info.Category = TEXT("field_variable");
	Info.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
	Info.MatchReason = FString::Printf(
		TEXT("semantic=%s,field_operation=%s,field_scope=%s,score=%d,type=%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.FieldOperation,
		*Request.Semantic.FieldScope,
		Score,
		*DescribePinType(Variable.VarType));
	Info.ReturnType = DescribePinType(Variable.VarType);
	Info.TargetObjectPin = TEXT("self");
	Info.Score = Score;
	Info.bGraphCompatible = Request.TargetGraph != nullptr;
	return Info;
}

static void SortFieldVariableCandidates(TArray<FBlueprintHelperVariableActionCandidate>& Candidates)
{
	Candidates.Sort([](
		const FBlueprintHelperVariableActionCandidate& Left,
		const FBlueprintHelperVariableActionCandidate& Right)
	{
		if (Left.Info.Score != Right.Info.Score)
		{
			return Left.Info.Score > Right.Info.Score;
		}
		return Left.Info.DisplayName < Right.Info.DisplayName;
	});
}

static TArray<FBlueprintHelperVariableActionCandidate> BuildProjectedFieldCandidates(
	const FBlueprintHelperActionResolutionRequest& Request,
	const UClass* OwnerClass,
	const UClass* NodeClass)
{
	TArray<FBlueprintHelperVariableActionCandidate> Candidates;
	if (!Request.Blueprint)
	{
		return Candidates;
	}

	for (const FBPVariableDescription& Variable : Request.Blueprint->NewVariables)
	{
		const int32 Score = ScoreVariableCandidate(Request, Variable);
		if (Score == INDEX_NONE)
		{
			continue;
		}

		FBlueprintHelperVariableActionCandidate Candidate;
		Candidate.Info = BuildCandidateInfo(Request, Variable, OwnerClass, NodeClass, Score);
		Candidates.Add(MoveTemp(Candidate));
	}

	SortFieldVariableCandidates(Candidates);
	return Candidates;
}

static void SetFieldCandidateDiagnostics(
	FBlueprintHelperActionResolutionResult& Result,
	const TArray<FBlueprintHelperVariableActionCandidate>& Candidates,
	const int32 MaxCandidates)
{
	const int32 Limit = FMath::Max(1, MaxCandidates);
	for (const FBlueprintHelperVariableActionCandidate& Candidate : Candidates)
	{
		if (Result.CandidateActions.Num() >= Limit)
		{
			break;
		}
		Result.CandidateActions.Add(Candidate.Info);
	}
}
}

bool FBlueprintHelperFieldVariableActionResolver::IsSupportedSemanticKind(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Field)
	{
		if (const FBlueprintHelperFieldCapabilitySpec* Spec =
			FBlueprintHelperFieldCapabilityRegistry::FindById(Semantic.CapabilityId))
		{
			return Spec->bFirstClassStatement;
		}
	}

	const FString FieldOperation = NormalizeFieldBoundaryToken(Semantic.FieldOperation);
	const FString FieldScope = NormalizeFieldBoundaryToken(Semantic.FieldScope);
	return Semantic.Kind == EBlueprintHelperActionSemanticKind::Field
		&& (FieldOperation == TEXT("get")
			|| FieldOperation == TEXT("set")
			|| FieldOperation == TEXT("get_property")
			|| FieldOperation == TEXT("set_property"))
		&& (FieldScope == TEXT("variable")
			|| FieldScope == TEXT("property_path")
			|| FieldScope == TEXT("component_ref")
			|| FieldScope == TEXT("field_access"));
}

bool FBlueprintHelperFieldVariableActionResolver::IsWritableFieldOperation(const FString& FieldOperation)
{
	return NormalizeFieldBoundaryToken(FieldOperation) == TEXT("set");
}

static bool ResolveFieldIdentity(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperFieldCapabilitySpec& Spec,
	const FString& FieldName,
	const FProperty* ResolvedProperty,
	const UClass* ResolvedOwnerClass,
	FBlueprintHelperResolvedFieldIdentity& OutIdentity)
{
	const FString ResolvedFieldName = FirstNonEmptyFieldValue(
		FieldName,
		Request.Semantic.CapabilityFacts.FindRef(TEXT("field.member_name")),
		Request.Semantic.Query);
	if (ResolvedFieldName.IsEmpty())
	{
		OutIdentity.DiagnosticReason = TEXT("missing_resolved_field_name");
		return false;
	}

	OutIdentity.CapabilityId = Spec.Id;
	OutIdentity.FieldKind = FieldCapabilityRootKindToString(Spec.RootKind);
	const FString RequestedOwnerClass = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.owner_class"));
	const FString TargetPinObjectPath = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_object_path"));
	if (Spec.bRequiresTargetPin)
	{
		OutIdentity.OwnerClassPath = FirstNonEmptyFieldValue(RequestedOwnerClass, TargetPinObjectPath);
	}
	if (OutIdentity.OwnerClassPath.IsEmpty())
	{
		OutIdentity.OwnerClassPath = ResolveOwnerClassPath(Request.Blueprint, ResolvedOwnerClass);
	}
	if (OutIdentity.OwnerClassPath.IsEmpty())
	{
		OutIdentity.OwnerClassPath = RequestedOwnerClass;
	}
	if (OutIdentity.OwnerClassPath.IsEmpty() && ResolvedProperty)
	{
		if (const UStruct* OwnerStruct = ResolvedProperty->GetOwnerStruct())
		{
			OutIdentity.OwnerClassPath = OwnerStruct->GetPathName();
		}
	}
	OutIdentity.MemberName = ResolvedProperty ? ResolvedProperty->GetName() : ResolvedFieldName;
	FGuid::Parse(Request.Semantic.CapabilityFacts.FindRef(TEXT("field.member_guid")), OutIdentity.MemberGuid);
	OutIdentity.LocalScopeName = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.local_scope"));
	OutIdentity.FunctionName = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.function_name"));
	if (OutIdentity.LocalScopeName.IsEmpty() && Request.TargetGraph && IsFunctionScopedCapability(Spec))
	{
		OutIdentity.LocalScopeName = Request.TargetGraph->GetName();
	}
	if (OutIdentity.FunctionName.IsEmpty() && Request.TargetGraph && IsFunctionScopedCapability(Spec))
	{
		OutIdentity.FunctionName = Request.TargetGraph->GetName();
	}
	OutIdentity.TargetPinRef = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_ref"));
	OutIdentity.TargetPinCategory = Request.Semantic.CapabilityFacts.FindRef(TEXT("field.target_pin_type"));
	OutIdentity.TargetPinObjectPath = TargetPinObjectPath;
	OutIdentity.ExpectedNodeFamily = Spec.ExpectedNodeFamily;
	OutIdentity.ExpectedNodeClassPath = Spec.ExpectedNodeClass;
	return true;
}

static void ApplyResolvedFieldIdentityToCandidate(
	const FBlueprintHelperResolvedFieldIdentity& Identity,
	FBlueprintHelperActionCandidate& Candidate)
{
	Candidate.CapabilityId = Identity.CapabilityId;
	Candidate.ExpectedNodeFamily = Identity.ExpectedNodeFamily;
	Candidate.ExpectedNodeClassPath = Identity.ExpectedNodeClassPath;

	if (!Identity.CapabilityId.IsEmpty())
	{
		Candidate.ReadbackFacts.Add(TEXT("capability_id"), Identity.CapabilityId);
	}
	if (!Identity.FieldKind.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.kind"), Identity.FieldKind);
		Candidate.ReadbackFacts.Add(TEXT("field_kind"), Identity.FieldKind);
	}
	if (!Identity.OwnerClassPath.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.owner_class"), Identity.OwnerClassPath);
		Candidate.ReadbackFacts.Add(TEXT("owner_class"), Identity.OwnerClassPath);
	}
	if (!Identity.MemberName.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.member_name"), Identity.MemberName);
		Candidate.ReadbackFacts.Add(TEXT("member_name"), Identity.MemberName);
	}
	if (!Identity.LocalScopeName.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.local_scope"), Identity.LocalScopeName);
		Candidate.ReadbackFacts.Add(TEXT("local_scope"), Identity.LocalScopeName);
	}
	if (!Identity.FunctionName.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.function_name"), Identity.FunctionName);
		Candidate.ReadbackFacts.Add(TEXT("function_name"), Identity.FunctionName);
	}
	if (!Identity.TargetPinRef.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.target_pin_ref"), Identity.TargetPinRef);
		Candidate.ReadbackFacts.Add(TEXT("target_pin_ref"), Identity.TargetPinRef);
	}
	if (!Identity.TargetPinCategory.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.target_pin_type"), Identity.TargetPinCategory);
		Candidate.ReadbackFacts.Add(TEXT("target_pin_category"), Identity.TargetPinCategory);
	}
	if (!Identity.TargetPinObjectPath.IsEmpty())
	{
		Candidate.CapabilityFacts.Add(TEXT("field.target_pin_object_path"), Identity.TargetPinObjectPath);
		Candidate.ReadbackFacts.Add(TEXT("target_pin_object_path"), Identity.TargetPinObjectPath);
		if (!Identity.OwnerClassPath.Equals(Identity.TargetPinObjectPath, ESearchCase::IgnoreCase))
		{
			Candidate.ReadbackFacts.Add(TEXT("owner_projected_from_target_pin"), Identity.TargetPinObjectPath);
		}
	}

	Candidate.StableId = FString::Printf(
		TEXT("%s:%s:%s:%s:%s"),
		*Candidate.StableId,
		*Identity.CapabilityId,
		*Identity.OwnerClassPath,
		*Identity.MemberName,
		*Identity.TargetPinObjectPath);
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context) const
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;

	const FBlueprintHelperFieldCapabilitySpec* CapabilitySpec = ResolveFieldCapabilitySpecForRequest(Request);
	if (!Request.Semantic.CapabilityId.TrimStartAndEnd().IsEmpty() && !CapabilitySpec)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ErrorCode = TEXT("unknown_field_capability");
		Result.Message = FString::Printf(
			TEXT("Unknown Field capability id: %s."),
			*Request.Semantic.CapabilityId);
		return Result;
	}

	if (CapabilitySpec && !CapabilitySpec->bFirstClassStatement)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ErrorCode = CapabilitySpec->RejectReason.IsEmpty()
			? FString(TEXT("unsupported_field_capability"))
			: CapabilitySpec->RejectReason;
		Result.Message = FString::Printf(
			TEXT("Field capability is not a first-class user statement: capability=%s reason=%s."),
			*CapabilitySpec->Id,
			*Result.ErrorCode);
		return Result;
	}

	if (!IsSupportedSemanticKind(Context.GetSemantic()) && !CapabilitySpec)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("needs_more_semantic_context");
		Result.Message = FString::Printf(
			TEXT("FieldVariableActionCluster needs field semantic context: semantic=%s field_operation=%s field_scope=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope);
		return Result;
	}

	FBlueprintHelperActionResolutionRequest EffectiveRequest = Request;
	if (CapabilitySpec)
	{
		if (EffectiveRequest.Semantic.CapabilityId.TrimStartAndEnd().IsEmpty())
		{
			EffectiveRequest.Semantic.CapabilityId = CapabilitySpec->Id;
		}
		if (EffectiveRequest.Semantic.FieldOperation.TrimStartAndEnd().IsEmpty())
		{
			EffectiveRequest.Semantic.FieldOperation = CapabilitySpec->FieldOperation;
		}
		if (EffectiveRequest.Semantic.FieldScope.TrimStartAndEnd().IsEmpty())
		{
			EffectiveRequest.Semantic.FieldScope = CapabilitySpec->FieldScope;
		}
	}
	const TMap<FString, FString>& Evidence = Context.GetEvidence();
	BackfillCapabilityFactsFromEvidence(EffectiveRequest, Evidence);

	if (!Request.Blueprint)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("field_variable_missing_blueprint");
		Result.Message = TEXT("field_variable_missing_blueprint");
		return Result;
	}

	if (!Request.TargetGraph)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("field_variable_missing_target_graph");
		Result.Message = TEXT("field_variable_missing_target_graph");
		return Result;
	}

	UClass* OwnerClass = ResolveOwnerClass(EffectiveRequest.Blueprint);
	TSubclassOf<UK2Node_Variable> NodeClass = (CapabilitySpec
		? FieldCapabilityWrites(*CapabilitySpec)
		: IsWritableFieldOperation(EffectiveRequest.Semantic.FieldOperation))
		? UK2Node_VariableSet::StaticClass()
		: UK2Node_VariableGet::StaticClass();

	const bool bComponentRefSemantic = IsComponentRefFieldScope(EffectiveRequest.Semantic.FieldScope)
		|| (CapabilitySpec && CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ComponentRef);
	const bool bFieldAccessSemantic = IsFieldAccessFieldScope(EffectiveRequest.Semantic.FieldScope)
		|| (CapabilitySpec
			&& (CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember
				|| CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ComponentProperty
				|| CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::StructMember
				|| CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath));
	const bool bFunctionScopedSemantic = CapabilitySpec && IsFunctionScopedCapability(*CapabilitySpec);
	if (CapabilitySpec && CapabilitySpec->bRequiresTargetPin)
	{
		const FString TargetPinRef = CapabilityFactOrEvidence(EffectiveRequest, Evidence, TEXT("field.target_pin_ref"), TEXT("target_pin_ref"));
		const FString TargetPinCategory = CapabilityFactOrEvidence(EffectiveRequest, Evidence, TEXT("field.target_pin_type"), TEXT("linked_pin_type_category"));
		const FString TargetPinObjectPath = CapabilityFactOrEvidence(EffectiveRequest, Evidence, TEXT("field.target_pin_object_path"), TEXT("linked_pin_type_object_path"));
		if (TargetPinRef.IsEmpty() || TargetPinCategory.IsEmpty() || TargetPinObjectPath.IsEmpty())
		{
			return MakeFieldMissingEvidenceResult(
				TEXT("Field capability requires explicit target pin projection."),
				TEXT("missing_target_pin_projection"));
		}

		AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.target_pin_ref"), TargetPinRef);
		AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.target_pin_type"), TargetPinCategory);
		AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.target_pin_object_path"), TargetPinObjectPath);
		if (!EffectiveRequest.Semantic.TargetObjectPinType.IsValid())
		{
			EffectiveRequest.Semantic.TargetObjectPinType.Category = TargetPinCategory;
			EffectiveRequest.Semantic.TargetObjectPinType.ObjectPath = TargetPinObjectPath;
		}
	}
	const FBlueprintHelperResolvedFieldPath ResolvedPath =
		FBlueprintHelperFieldPathResolution::Resolve(EffectiveRequest, Evidence);

	if (!ResolvedPath.bIsValid)
	{
		return MakeFieldMissingEvidenceResult(FString::Printf(
			TEXT("%s semantic=%s field_operation=%s field_scope=%s query=%s target=%s property_path=%s."),
			*ResolvedPath.Message,
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath,
			*Request.Semantic.PropertyPath),
			ResolvedPath.ErrorCode.IsEmpty() ? TEXT("needs_more_semantic_context") : ResolvedPath.ErrorCode);
	}

	if (!bComponentRefSemantic && !bFieldAccessSemantic && !bFunctionScopedSemantic && ResolvedPath.OwnerClassPath.IsEmpty())
	{
		return MakeFieldMissingEvidenceResult(FString::Printf(
			TEXT("Field variable action requires explicit owner evidence: semantic=%s field_operation=%s field_scope=%s query=%s target=%s property_path=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath,
			*Request.Semantic.PropertyPath));
	}

	FString FieldName = FirstNonEmptyFieldValue(
		EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.member_name")),
		EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.component_name")),
		GetEvidenceValue(Evidence, TEXT("field_name")));
	if (FieldName.IsEmpty() && bFunctionScopedSemantic)
	{
		FieldName = FirstNonEmptyFieldValue(
			EffectiveRequest.Semantic.Query,
			EffectiveRequest.Semantic.TargetPath);
	}
	if (FieldName.IsEmpty() && ResolvedPath.Role != EBlueprintHelperFieldPathRole::Variable)
	{
		FieldName = ResolvedPath.LeafName;
	}

	if (FieldName.IsEmpty() && !EffectiveRequest.Semantic.TargetPath.TrimStartAndEnd().IsEmpty())
	{
		FBlueprintHelperActionResolutionRequest CandidateRequest = EffectiveRequest;
		CandidateRequest.Semantic.Query = EffectiveRequest.Semantic.TargetPath.TrimStartAndEnd();
		const TArray<FBlueprintHelperVariableActionCandidate> Candidates =
			BuildProjectedFieldCandidates(CandidateRequest, OwnerClass, NodeClass.Get());
		if (Candidates.Num() == 1)
		{
			FieldName = Candidates[0].Info.DisplayName;
		}
		else if (Candidates.Num() > 1)
		{
			Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
			Result.ErrorCode = TEXT("ambiguous_candidates");
			Result.Message = FString::Printf(
				TEXT("Field variable action is ambiguous without projected field_name evidence: semantic=%s field_operation=%s field_scope=%s owner=%s query=%s."),
				*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
				*Request.Semantic.FieldOperation,
				*Request.Semantic.FieldScope,
				*ResolvedPath.OwnerClassPath,
				*Request.Semantic.Query);
			SetFieldCandidateDiagnostics(Result, Candidates, Request.MaxCandidates);
			return Result;
		}
	}

	if (FieldName.TrimStartAndEnd().IsEmpty())
	{
		return MakeFieldMissingEvidenceResult(FString::Printf(
			TEXT("Field variable action requires projected field_name evidence: semantic=%s field_operation=%s field_scope=%s owner=%s query=%s target=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*ResolvedPath.OwnerClassPath,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath));
	}

	const FProperty* Property = nullptr;
	FProperty* MutableProperty = nullptr;
	FBPVariableDescription* LocalVariable = nullptr;
	bool bResolvedLocalVariable = false;
	bool bResolvedFunctionParam = false;

	if (bFunctionScopedSemantic)
	{
		FString FunctionScopeName;
		if (!ResolveFunctionScope(EffectiveRequest, Evidence, *CapabilitySpec, FunctionScopeName, Result))
		{
			return Result;
		}
		AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.local_scope"), FunctionScopeName);
		AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.function_name"), FunctionScopeName);

		if (CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::Local)
		{
			UK2Node_FunctionEntry* EntryNode = nullptr;
			LocalVariable = FindLocalVariableDescription(EffectiveRequest, FieldName, &EntryNode);
			if (!LocalVariable || !EntryNode)
			{
				Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
				Result.ErrorCode = TEXT("local_variable_not_found");
				Result.Message = FString::Printf(
					TEXT("Local Field capability not found in function scope: field=%s function=%s."),
					*FieldName,
					*FunctionScopeName);
				return Result;
			}

			MutableProperty = ResolveLocalVariableProperty(EffectiveRequest, *LocalVariable);
			Property = MutableProperty;
			bResolvedLocalVariable = true;
		}
		else if (CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::FunctionParam)
		{
			const FString ParamFlags = EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.param_flags"));
			FString ParamErrorCode;
			UFunction* Function = FindFunctionForGraph(EffectiveRequest.Blueprint, EffectiveRequest.TargetGraph);
			MutableProperty = FindFunctionInputParameter(Function, FieldName, ParamFlags, ParamErrorCode);
			Property = MutableProperty;
			bResolvedFunctionParam = true;
			if (Function && EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.function_name")).IsEmpty())
			{
				AddCapabilityFactIfPresent(EffectiveRequest, TEXT("field.function_name"), Function->GetName());
			}
		}
	}
	else
	{
		Property = ResolveFieldProperty(EffectiveRequest, CapabilitySpec, FieldName);
		MutableProperty = const_cast<FProperty*>(Property);
	}

	if (!Property)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ErrorCode = bFunctionScopedSemantic
			? FString(TEXT("function_scope_field_unresolvable"))
			: FString(TEXT("field_action_unresolvable"));
		Result.Message = FString::Printf(
			TEXT("Field variable action not found from projected context: semantic=%s field_operation=%s field_scope=%s field=%s query=%s target=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*FieldName,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath);
		return Result;
	}

	const FObjectPropertyBase* ComponentObjectProperty = nullptr;
	if (CapabilitySpec && CapabilitySpec->RootKind == EBlueprintHelperFieldCapabilityRootKind::ComponentRef)
	{
		ComponentObjectProperty = CastField<FObjectPropertyBase>(Property);
		const UClass* ComponentPropertyClass = ComponentObjectProperty
			? ComponentObjectProperty->PropertyClass
			: nullptr;
		if (!ComponentObjectProperty || !ComponentPropertyClass || !ComponentPropertyClass->IsChildOf(UActorComponent::StaticClass()))
		{
			Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
			Result.ErrorCode = TEXT("not_class_component_property");
			Result.Message = FString::Printf(
				TEXT("Component ref field action requires an object component property: field=%s."),
				*FieldName);
			return Result;
		}
	}

	UBlueprintVariableNodeSpawner* Spawner = bResolvedLocalVariable && LocalVariable
		? UBlueprintVariableNodeSpawner::CreateFromLocal(NodeClass, EffectiveRequest.TargetGraph, *LocalVariable, MutableProperty)
		: UBlueprintVariableNodeSpawner::CreateFromMemberOrParam(
			NodeClass,
			Property,
			bResolvedFunctionParam ? EffectiveRequest.TargetGraph : nullptr);
	if (!Spawner)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ErrorCode = TEXT("field_action_unresolvable");
		Result.Message = FString::Printf(
			TEXT("Field variable node spawner unavailable: semantic=%s field_operation=%s field_scope=%s field=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.FieldOperation,
			*Request.Semantic.FieldScope,
			*FieldName);
		return Result;
	}

	FBlueprintHelperCallFunctionCandidateInfo CandidateInfo;
	const FString StableIdPrefix = bComponentRefSemantic
		? TEXT("field_component_ref")
		: (bFieldAccessSemantic ? TEXT("field_access") : TEXT("field_variable"));
	CandidateInfo.StableId = MakeVariableStableId(
		EffectiveRequest.Blueprint,
		FieldName,
		EffectiveRequest.Semantic.FieldOperation,
		EffectiveRequest.Semantic.FieldScope,
		StableIdPrefix);
	CandidateInfo.DisplayName = FieldName;
	CandidateInfo.OwnerClassPath = ResolvedPath.OwnerClassPath;
	CandidateInfo.NativeFunctionName = FieldName;
	CandidateInfo.Category = bComponentRefSemantic
		? TEXT("field_component_ref")
		: (bFieldAccessSemantic ? TEXT("field_access") : TEXT("field_variable"));
	CandidateInfo.NodeClassPath = NodeClass.Get() ? NodeClass->GetPathName() : FString();
	CandidateInfo.MatchReason = FString::Printf(
		TEXT("projected_context_evidence semantic=%s field_operation=%s field_scope=%s field=%s path=%s root=%s leaf=%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.FieldOperation,
		*Request.Semantic.FieldScope,
		*FieldName,
		*ResolvedPath.FullPath,
		*ResolvedPath.RootName,
		*ResolvedPath.LeafName);
	CandidateInfo.ReturnType = !Evidence.FindRef(TEXT("field_pin_category")).IsEmpty()
		? Evidence.FindRef(TEXT("field_pin_category"))
		: Property->GetCPPType();
	CandidateInfo.TargetObjectPin = TEXT("self");
	CandidateInfo.Score = 100;
	CandidateInfo.bGraphCompatible = true;
	if (CapabilitySpec)
	{
		FBlueprintHelperResolvedFieldIdentity Identity;
		if (ResolveFieldIdentity(EffectiveRequest, *CapabilitySpec, FieldName, Property, OwnerClass, Identity))
		{
			ApplyResolvedFieldIdentityToCandidate(Identity, CandidateInfo);
		}
	}
	if (ComponentObjectProperty)
	{
		CandidateInfo.CapabilityFacts.FindOrAdd(TEXT("field.component_name"), FieldName);
		CandidateInfo.CapabilityFacts.FindOrAdd(TEXT("field.component_owner_class"), EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.component_owner_class")));
		CandidateInfo.CapabilityFacts.FindOrAdd(TEXT("field.component_kind"), EffectiveRequest.Semantic.CapabilityFacts.FindRef(TEXT("field.component_kind")));
		CandidateInfo.ReadbackFacts.Add(TEXT("component_name"), FieldName);
		CandidateInfo.ReadbackFacts.Add(TEXT("component_ref_spawner"), TEXT("UBlueprintVariableNodeSpawner"));
		CandidateInfo.ReadbackFacts.Add(
			TEXT("component_property_class"),
			ComponentObjectProperty->PropertyClass ? ComponentObjectProperty->PropertyClass->GetPathName() : FString());
	}

	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.SelectedStableId = CandidateInfo.StableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Reset();
	Result.CandidateActions.Add(CandidateInfo);
	Result.bRequiresDedicatedFragmentBuilder = ResolvedPath.bRequiresFragmentDecomposition;
	Result.MatchReason = ResolvedPath.bRequiresFragmentDecomposition
		? TEXT("complex_property_path_requires_field_path_fragment_builder")
		: CandidateInfo.MatchReason;
	Result.Message = FString::Printf(
		TEXT("Field variable action resolved: semantic=%s field_operation=%s field_scope=%s variable=%s."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.FieldOperation,
		*Request.Semantic.FieldScope,
		*FieldName);
	return Result;
}
