#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
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

static FString SingletonKindToQuery(const EBlueprintHelperSingletonControlFlowKind Kind)
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
		return FString();
	default:
		return FString();
	}
}

static EBlueprintHelperActionSemanticKind SingletonKindToSemanticKind(const EBlueprintHelperSingletonControlFlowKind Kind)
{
	return Kind == EBlueprintHelperSingletonControlFlowKind::Select
		? EBlueprintHelperActionSemanticKind::Select
		: EBlueprintHelperActionSemanticKind::Control;
}

static FString StableHashString(const FString& Stable)
{
	return LexToString(GetTypeHash(Stable));
}

static void AppendObjectIdentity(FString& Stable, const TCHAR* Label, const UObject* Object)
{
	Stable += Label;
	Stable += TEXT("_path=");
	Stable += Object ? Object->GetPathName() : FString();
	Stable += TEXT("|");
	Stable += Label;
	Stable += TEXT("_name=");
	Stable += Object ? Object->GetName() : FString();
}

static FString BuildSingletonCanonicalStableFields(
	const EBlueprintHelperSingletonControlFlowKind Kind,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& StatementId,
	const FString& Query)
{
	FString Stable;
	Stable += TEXT("singleton_control_flow|kind=");
	Stable += SingletonKindToStableName(Kind);
	Stable += TEXT("|semantic=");
	Stable += FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind);
	Stable += TEXT("|query=");
	Stable += Query;
	Stable += TEXT("|");
	AppendObjectIdentity(Stable, TEXT("blueprint"), Blueprint);
	Stable += TEXT("|");
	AppendObjectIdentity(Stable, TEXT("target_graph"), TargetGraph);
	Stable += TEXT("|statement=");
	Stable += StatementId;
	return Stable;
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

bool FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
	const EBlueprintHelperSingletonControlFlowKind Kind,
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& StatementId,
	const FString& Reason,
	FBlueprintHelperActionResolutionRequest& OutRequest)
{
	OutRequest = FBlueprintHelperActionResolutionRequest();
	if (!Blueprint || !TargetGraph || Kind == EBlueprintHelperSingletonControlFlowKind::Unknown)
	{
		return false;
	}

	const FString Query = SingletonKindToQuery(Kind);
	const EBlueprintHelperActionSemanticKind SemanticKind = SingletonKindToSemanticKind(Kind);
	OutRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	OutRequest.Blueprint = Blueprint;
	OutRequest.TargetGraph = TargetGraph;
	OutRequest.StatementId = StatementId.IsEmpty()
		? FString::Printf(TEXT("singleton_%s"), SingletonKindToStableName(Kind))
		: StatementId;
	const FString StableFields = BuildSingletonCanonicalStableFields(
		Kind,
		SemanticKind,
		Blueprint,
		TargetGraph,
		OutRequest.StatementId,
		Query);
	OutRequest.ProjectedContextHash = StableHashString(FString(TEXT("projected|")) + StableFields);
	OutRequest.SemanticConstraintsHash = StableHashString(FString(TEXT("constraints|")) + StableFields);
	OutRequest.Semantic.Kind = SemanticKind;
	OutRequest.Semantic.Query = Query;
	OutRequest.Semantic.TargetPath = OutRequest.StatementId;
	OutRequest.MaxCandidates = 1;
	return true;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
	const EBlueprintHelperSingletonControlFlowKind Kind,
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& StatementId,
	const FString& Reason)
{
	FBlueprintHelperActionResolutionRequest Request;
	if (!TryBuildCanonicalRequest(Kind, Blueprint, TargetGraph, StatementId, Reason, Request))
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("missing_required_evidence");
		Result.Message = TEXT("Singleton control-flow canonical request requires Blueprint, target graph, and known singleton kind.");
		return Result;
	}

	FBlueprintHelperSingletonControlFlowEvidence Evidence;
	if (!TryResolve(Request, Evidence))
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("unsupported_singleton_control_flow_semantic");
		Result.Message = FString::Printf(
			TEXT("Singleton control-flow provider could not resolve canonical kind '%s'."),
			SingletonKindToStableName(Kind));
		return Result;
	}

	return MakeResolvedResult(Request, Evidence);
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
	const FString KindName = SingletonKindToStableName(Evidence.SingletonKind);
	Candidate.ReadbackFacts.Add(
		TEXT("generic.family"),
		Evidence.SingletonKind == EBlueprintHelperSingletonControlFlowKind::Select ? TEXT("struct_select") : TEXT("control"));
	Candidate.ReadbackFacts.Add(
		TEXT("generic.operation_id"),
		Evidence.SingletonKind == EBlueprintHelperSingletonControlFlowKind::Select
			? TEXT("generic_ops.struct_select.select")
			: FString::Printf(TEXT("generic_ops.control.%s"), *KindName));
	Candidate.ReadbackFacts.Add(TEXT("generic.operation"), KindName);
	Candidate.ReadbackFacts.Add(TEXT("generic.wildcard_residual"), TEXT("false"));
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
