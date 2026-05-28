#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Select.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEvidenceWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

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

	const FString Query = UGraphWriteActionResolverUtils::SingletonKindToQuery(Kind);
	const EBlueprintHelperActionSemanticKind SemanticKind = UGraphWriteActionResolverUtils::SingletonKindToSemanticKind(Kind);
	OutRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	OutRequest.Blueprint = Blueprint;
	OutRequest.TargetGraph = TargetGraph;
	OutRequest.StatementId = StatementId.IsEmpty()
		? FString::Printf(TEXT("singleton_%s"), UGraphWriteActionResolverUtils::SingletonKindToStableName(Kind))
		: StatementId;
	const FString StableFields = UGraphWriteActionResolverUtils::BuildSingletonCanonicalStableFields(
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
			UGraphWriteActionResolverUtils::SingletonKindToStableName(Kind));
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
		return UGraphWriteActionResolverUtils::MakeEvidence(
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
		return UGraphWriteActionResolverUtils::MakeEvidence(
			EBlueprintHelperSingletonControlFlowKind::Branch,
			UK2Node_IfThenElse::StaticClass(),
			OutEvidence);
	}
	if (Query == TEXT("sequence"))
	{
		return UGraphWriteActionResolverUtils::MakeEvidence(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			UK2Node_ExecutionSequence::StaticClass(),
			OutEvidence);
	}
	if (Query == TEXT("return"))
	{
		return UGraphWriteActionResolverUtils::MakeEvidence(
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
	Candidate.DisplayName = UGraphWriteActionResolverUtils::SingletonKindToDisplayName(Evidence.SingletonKind);
	Candidate.Category = TEXT("Generic.SingletonControlFlow");
	Candidate.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
	Candidate.MatchReason = Evidence.Reason;
	Candidate.Score = 100;
	Candidate.bGraphCompatible = true;
	Candidate.bFromActionDatabase = false;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = Evidence.SingletonKind == EBlueprintHelperSingletonControlFlowKind::Select;
	const FString KindName = UGraphWriteActionResolverUtils::SingletonKindToStableName(Evidence.SingletonKind);
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
