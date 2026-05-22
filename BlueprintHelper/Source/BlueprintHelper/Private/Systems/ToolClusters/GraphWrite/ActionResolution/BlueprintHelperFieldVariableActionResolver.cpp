#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.h"

#include "BlueprintVariableNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "UObject/UnrealType.h"

namespace
{
struct FBlueprintHelperVariableActionCandidate
{
	FBlueprintHelperCallFunctionCandidateInfo Info;
	TWeakObjectPtr<UBlueprintNodeSpawner> Spawner;
};

static FString NormalizeFieldVariableToken(const FString& Value)
{
	FString Normalized = Value.TrimStartAndEnd().ToLower();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	return Normalized;
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
	const FBPVariableDescription& Variable,
	EBlueprintHelperActionSemanticKind SemanticKind)
{
	return FString::Printf(
		TEXT("field_variable:%s:%s:%s"),
		Blueprint ? *Blueprint->GetPathName() : TEXT("unknown_blueprint"),
		*Variable.VarName.ToString(),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind));
}

static FBlueprintHelperCallFunctionCandidateInfo BuildCandidateInfo(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBPVariableDescription& Variable,
	const UClass* OwnerClass,
	const UClass* NodeClass,
	const int32 Score)
{
	FBlueprintHelperCallFunctionCandidateInfo Info;
	Info.StableId = MakeVariableStableId(Request.Blueprint, Variable, Request.Semantic.Kind);
	Info.DisplayName = Variable.VarName.ToString();
	Info.OwnerClassPath = OwnerClass ? OwnerClass->GetPathName() : FString();
	Info.NativeFunctionName = Variable.VarName.ToString();
	Info.Category = TEXT("field_variable");
	Info.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
	Info.MatchReason = FString::Printf(
		TEXT("semantic=%s,score=%d,type=%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
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
}

bool FBlueprintHelperFieldVariableActionResolver::IsSupportedSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	return Kind == EBlueprintHelperActionSemanticKind::Get
		|| Kind == EBlueprintHelperActionSemanticKind::Set;
}

bool FBlueprintHelperFieldVariableActionResolver::IsWritableSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	return Kind == EBlueprintHelperActionSemanticKind::Set;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context) const
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;

	if (!IsSupportedSemanticKind(Context.GetSemantic().Kind))
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("needs_more_semantic_context");
		Result.Message = FString::Printf(
			TEXT("FieldVariableActionCluster needs typed target/property-path context for semantic kind '%s'."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
		return Result;
	}

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

	UClass* OwnerClass = ResolveOwnerClass(Request.Blueprint);
	TSubclassOf<UK2Node_Variable> NodeClass = IsWritableSemanticKind(Request.Semantic.Kind)
		? UK2Node_VariableSet::StaticClass()
		: UK2Node_VariableGet::StaticClass();

	const TMap<FString, FString>& Evidence = Context.GetEvidence();
	FString FieldName = Evidence.FindRef(TEXT("field_name"));
	if (FieldName.IsEmpty())
	{
		FieldName = !Request.Semantic.TargetPath.IsEmpty()
			? Request.Semantic.TargetPath
			: Request.Semantic.Query;
	}

	if (FieldName.TrimStartAndEnd().IsEmpty())
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ErrorCode = TEXT("field_target_missing");
		Result.Message = TEXT("Field variable action requires projected field_name evidence or semantic target.");
		return Result;
	}

	const FProperty* Property = FindVariableProperty(Request.Blueprint, FName(*FieldName));
	if (!Property)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ErrorCode = TEXT("field_action_unresolvable");
		Result.Message = FString::Printf(
			TEXT("Field variable action not found from projected context: semantic=%s field=%s query=%s target=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*FieldName,
			*Request.Semantic.Query,
			*Request.Semantic.TargetPath);
		return Result;
	}

	UBlueprintVariableNodeSpawner* Spawner = UBlueprintVariableNodeSpawner::CreateFromMemberOrParam(
		NodeClass,
		Property);
	if (!Spawner)
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
		Result.ErrorCode = TEXT("field_action_unresolvable");
		Result.Message = FString::Printf(
			TEXT("Field variable node spawner unavailable: semantic=%s field=%s."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*FieldName);
		return Result;
	}

	FBlueprintHelperCallFunctionCandidateInfo CandidateInfo;
	CandidateInfo.StableId = FString::Printf(
		TEXT("field_variable:%s:%s:%s"),
		Request.Blueprint ? *Request.Blueprint->GetPathName() : TEXT("unknown_blueprint"),
		*FieldName,
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	CandidateInfo.DisplayName = FieldName;
	CandidateInfo.OwnerClassPath = !Evidence.FindRef(TEXT("field_owner_class")).IsEmpty()
		? Evidence.FindRef(TEXT("field_owner_class"))
		: (OwnerClass ? OwnerClass->GetPathName() : FString());
	CandidateInfo.NativeFunctionName = FieldName;
	CandidateInfo.Category = TEXT("field_variable");
	CandidateInfo.NodeClassPath = NodeClass.Get() ? NodeClass->GetPathName() : FString();
	CandidateInfo.MatchReason = FString::Printf(
		TEXT("projected_context_evidence semantic=%s field=%s"),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*FieldName);
	CandidateInfo.ReturnType = !Evidence.FindRef(TEXT("field_pin_category")).IsEmpty()
		? Evidence.FindRef(TEXT("field_pin_category"))
		: Property->GetCPPType();
	CandidateInfo.TargetObjectPin = TEXT("self");
	CandidateInfo.Score = 100;
	CandidateInfo.bGraphCompatible = true;

	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.SelectedStableId = CandidateInfo.StableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Reset();
	Result.CandidateActions.Add(CandidateInfo);
	Result.Message = FString::Printf(
		TEXT("Field variable action resolved: semantic=%s variable=%s."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*FieldName);
	return Result;
}
