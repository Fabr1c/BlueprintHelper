#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Select.h"

namespace
{
static FString NormalizeSingletonControlQuery(const FString& Query)
{
	FString Normalized = Query.TrimStartAndEnd();
	Normalized.ToLowerInline();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("_"), TEXT(""));
	Normalized.ReplaceInline(TEXT("-"), TEXT(""));
	return Normalized;
}

static const TCHAR* SingletonKindToStableName(const EBlueprintHelperSingletonControlFlowKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperSingletonControlFlowKind::Branch:
		return TEXT("branch");
	case EBlueprintHelperSingletonControlFlowKind::Sequence:
		return TEXT("sequence");
	case EBlueprintHelperSingletonControlFlowKind::Return:
		return TEXT("return");
	case EBlueprintHelperSingletonControlFlowKind::Select:
		return TEXT("select");
	default:
		return TEXT("unknown");
	}
}

static const TCHAR* SingletonKindToDisplayName(const EBlueprintHelperSingletonControlFlowKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperSingletonControlFlowKind::Branch:
		return TEXT("Branch");
	case EBlueprintHelperSingletonControlFlowKind::Sequence:
		return TEXT("Sequence");
	case EBlueprintHelperSingletonControlFlowKind::Return:
		return TEXT("Return");
	case EBlueprintHelperSingletonControlFlowKind::Select:
		return TEXT("Select");
	default:
		return TEXT("Unknown");
	}
}

static bool MakeEvidence(
	const EBlueprintHelperSingletonControlFlowKind Kind,
	TSubclassOf<UEdGraphNode> NodeClass,
	FBlueprintHelperSingletonControlFlowEvidence& OutEvidence)
{
	OutEvidence = FBlueprintHelperSingletonControlFlowEvidence();
	if (!NodeClass)
	{
		return false;
	}

	const FString KindName = SingletonKindToStableName(Kind);
	const FString NodeClassPath = NodeClass->GetPathName();
	OutEvidence.SingletonKind = Kind;
	OutEvidence.NodeClass = NodeClass;
	OutEvidence.StableId = FString::Printf(
		TEXT("singleton_control_flow:%s:%s"),
		*KindName,
		*NodeClassPath);
	OutEvidence.Reason = FString::Printf(
		TEXT("GenericAssetStructControl singleton control-flow provider resolved canonical %s node class %s."),
		*KindName,
		*NodeClassPath);
	return true;
}
}

bool FBlueprintHelperSingletonControlFlowEvidenceProvider::TryResolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperSingletonControlFlowEvidence& OutEvidence)
{
	OutEvidence = FBlueprintHelperSingletonControlFlowEvidence();
	if (Request.ClusterKind != EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction)
	{
		return false;
	}

	if (Request.Semantic.Kind == EBlueprintHelperActionSemanticKind::Select)
	{
		return MakeEvidence(
			EBlueprintHelperSingletonControlFlowKind::Select,
			UK2Node_Select::StaticClass(),
			OutEvidence);
	}

	if (Request.Semantic.Kind != EBlueprintHelperActionSemanticKind::Control)
	{
		return false;
	}

	const FString Query = NormalizeSingletonControlQuery(Request.Semantic.Query);
	if (Query == TEXT("branch"))
	{
		return MakeEvidence(
			EBlueprintHelperSingletonControlFlowKind::Branch,
			UK2Node_IfThenElse::StaticClass(),
			OutEvidence);
	}
	if (Query == TEXT("sequence"))
	{
		return MakeEvidence(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			UK2Node_ExecutionSequence::StaticClass(),
			OutEvidence);
	}
	if (Query == TEXT("return"))
	{
		return MakeEvidence(
			EBlueprintHelperSingletonControlFlowKind::Return,
			UK2Node_FunctionResult::StaticClass(),
			OutEvidence);
	}

	return false;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperSingletonControlFlowEvidenceProvider::MakeResolvedResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperSingletonControlFlowEvidence& Evidence)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = Evidence.StableId;

	UClass* NodeClass = Evidence.NodeClass.Get();
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = Evidence.StableId;
	Candidate.DisplayName = SingletonKindToDisplayName(Evidence.SingletonKind);
	Candidate.Category = TEXT("Generic.SingletonControlFlow");
	Candidate.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
	Candidate.MatchReason = Evidence.Reason;
	Candidate.Score = 100;
	Candidate.bGraphCompatible = true;
	Candidate.bFromActionDatabase = false;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = Evidence.SingletonKind == EBlueprintHelperSingletonControlFlowKind::Select;
	Result.CandidateActions.Add(Candidate);

	if (!NodeClass || Evidence.SingletonKind == EBlueprintHelperSingletonControlFlowKind::Unknown || Evidence.StableId.IsEmpty())
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
		Result.ErrorCode = TEXT("singleton_control_flow_evidence_invalid");
		Result.Message = TEXT("Singleton control-flow evidence is incomplete.");
		return Result;
	}

	Result.SelectedSpawner = UBlueprintNodeSpawner::Create(NodeClass);
	if (!Result.SelectedSpawner.IsValid())
	{
		Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
		Result.ErrorCode = TEXT("singleton_control_flow_spawner_unavailable");
		Result.Message = FString::Printf(
			TEXT("Singleton control-flow node spawner unavailable for '%s'."),
			*Evidence.StableId);
		return Result;
	}

	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.Message = FString::Printf(
		TEXT("Resolved singleton control-flow node spawner '%s' for semantic '%s'."),
		*Evidence.StableId,
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}
