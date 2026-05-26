#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "K2Node_PromotableOperator.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"

namespace
{
static FString MakePromotableOperatorStableId(FName OpName)
{
	return FString::Printf(TEXT("promotable_operator:%s"), *OpName.ToString());
}

static FString GetOperatorTokenFromContext(const FBlueprintHelperActionClusterContextView& Context)
{
	const FString SemanticQuery = Context.GetSemantic().Query.TrimStartAndEnd();
	if (!SemanticQuery.IsEmpty())
	{
		return SemanticQuery;
	}

	static const TCHAR* EvidenceKeys[] =
	{
		TEXT("operator_token"),
		TEXT("operator"),
		TEXT("op"),
		TEXT("op_name")
	};

	for (const TCHAR* EvidenceKey : EvidenceKeys)
	{
		if (const FString* EvidenceValue = Context.GetRequest().ContextEvidence.Find(EvidenceKey))
		{
			const FString Trimmed = EvidenceValue->TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				return Trimmed;
			}
		}
	}

	return FString();
}

static FString GetRequestedOpOperationId(const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString FunctionOperation = Request.Semantic.FunctionOperation.TrimStartAndEnd();
	if (FunctionOperation.StartsWith(TEXT("op."), ESearchCase::IgnoreCase))
	{
		return FBlueprintHelperOpCallableCatalog::NormalizeOperationId(FunctionOperation);
	}

	if (const FString* OperationId = Request.ContextEvidence.Find(TEXT("op.operation_id")))
	{
		return FBlueprintHelperOpCallableCatalog::NormalizeOperationId(*OperationId);
	}

	return FString();
}

static EBlueprintHelperActionResolutionStatus MapFunctionResolveStatus(EBlueprintHelperCallFunctionResolveStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperCallFunctionResolveStatus::Resolved:
		return EBlueprintHelperActionResolutionStatus::Resolved;
	case EBlueprintHelperCallFunctionResolveStatus::Ambiguous:
		return EBlueprintHelperActionResolutionStatus::Ambiguous;
	case EBlueprintHelperCallFunctionResolveStatus::Blocked:
		return EBlueprintHelperActionResolutionStatus::Blocked;
	case EBlueprintHelperCallFunctionResolveStatus::NotFound:
	default:
		return EBlueprintHelperActionResolutionStatus::NotFound;
	}
}

static void ApplyArrayIdenticalEvidence(
	const FBlueprintHelperOpCallableEvidence& Evidence,
	FBlueprintHelperCallFunctionResolveRequest& CallRequest)
{
	if (!Evidence.OperationId.Equals(TEXT("array_identical"), ESearchCase::IgnoreCase))
	{
		return;
	}

	FBlueprintHelperCallFunctionPinType LhsPinType =
		FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Evidence.Facts.FindRef(TEXT("op.array_lhs_pin_type")));
	FBlueprintHelperCallFunctionPinType RhsPinType =
		FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(Evidence.Facts.FindRef(TEXT("op.array_rhs_pin_type")));
	if (LhsPinType.IsValid() && LhsPinType.ContainerType.IsEmpty())
	{
		LhsPinType.ContainerType = TEXT("array");
	}
	if (RhsPinType.IsValid() && RhsPinType.ContainerType.IsEmpty())
	{
		RhsPinType.ContainerType = TEXT("array");
	}

	CallRequest.ArgumentNames = { TEXT("ArrayA"), TEXT("ArrayB") };
	CallRequest.ArgumentPinTypes.Add(TEXT("ArrayA"), LhsPinType);
	CallRequest.ArgumentPinTypes.Add(TEXT("ArrayB"), RhsPinType);
	CallRequest.Context.ArgumentNames = CallRequest.ArgumentNames;
	CallRequest.Context.ArgumentPinTypes = CallRequest.ArgumentPinTypes;
}

static FBlueprintHelperActionResolutionResult MakeCallableOpResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperOpCallableEvidence& Evidence,
	const FBlueprintHelperCallFunctionResolveResult& CallResult)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = MapFunctionResolveStatus(CallResult.Status);
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = CallResult.ErrorCode;
	Result.Message = CallResult.Message;
	Result.SelectedStableId = CallResult.Selected.StableId;
	Result.SelectedSpawner = CallResult.Selected.NodeSpawner;
	Result.SelectedFunction = CallResult.Selected.Function;
	Result.CandidateActions = CallResult.CandidateFunctions;
	Result.FunctionCandidate = CallResult.Selected;
	for (FBlueprintHelperCallFunctionCandidateInfo& CandidateInfo : Result.CandidateActions)
	{
		CandidateInfo.CapabilityId = TEXT("op_coverage");
		CandidateInfo.ExpectedNodeFamily = Evidence.Spec.SpawnFamily;
		CandidateInfo.ExpectedNodeClassPath = Evidence.Spec.RequiredNodeClassPath;
		for (const TPair<FString, FString>& FactPair : Evidence.Facts)
		{
			CandidateInfo.CapabilityFacts.FindOrAdd(FactPair.Key, FactPair.Value);
			CandidateInfo.ReadbackFacts.FindOrAdd(FactPair.Key, FactPair.Value);
		}
		CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.operation_id"), Evidence.OperationId);
		CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.source_function_path"), Evidence.Spec.StableCallableId);
		CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.node_class_path"), CandidateInfo.NodeClassPath);
		CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.wildcard_residual"), TEXT("false"));
		if (Result.SelectedSpawner.IsValid())
		{
			CandidateInfo.ReadbackFacts.FindOrAdd(TEXT("op.spawner_class"), Result.SelectedSpawner->GetClass()->GetPathName());
		}
	}
	if (Result.IsResolved())
	{
		Result.Message = FString::Printf(
			TEXT("Resolved op.%s to callable %s through FunctionActionCluster policy."),
			*Evidence.OperationId,
			*Evidence.Spec.StableCallableId);
	}
	return Result;
}

static FBlueprintHelperCallFunctionCandidateInfo MakePromotableOperatorCandidateInfo(
	FName OpName,
	UBlueprintFunctionNodeSpawner* Spawner)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = MakePromotableOperatorStableId(OpName);
	Candidate.DisplayName = OpName.ToString();
	Candidate.Category = TEXT("Utilities|Operators");
	Candidate.NodeClassPath = UK2Node_PromotableOperator::StaticClass()->GetPathName();
	Candidate.MatchReason = TEXT("ue_promotable_operator_spawner");
	Candidate.CapabilityId = TEXT("op_coverage");
	Candidate.ExpectedNodeFamily = TEXT("type_promotion");
	Candidate.ExpectedNodeClassPath = UK2Node_PromotableOperator::StaticClass()->GetPathName();
	Candidate.ReadbackFacts.Add(TEXT("op.type_promotion_operator"), OpName.ToString());
	Candidate.ReadbackFacts.Add(TEXT("op.node_class_path"), Candidate.NodeClassPath);
	Candidate.ReadbackFacts.Add(TEXT("op.wildcard_residual"), TEXT("false"));
	if (Spawner)
	{
		Candidate.ReadbackFacts.Add(TEXT("op.spawner_class"), Spawner->GetClass()->GetPathName());
	}
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Spawner != nullptr;
	Candidate.bFromActionDatabase = true;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = true;
	return Candidate;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (Context.GetSemantic().Kind != EBlueprintHelperActionSemanticKind::Op)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Result.ErrorCode = TEXT("operator_resolver_requires_op_semantic");
		Result.Message = TEXT("Operator resolver only accepts Semantic.Kind=Op.");
		return Result;
	}

	const FString RequestedOperationId = GetRequestedOpOperationId(Request);
	if (FBlueprintHelperOpCallableCatalog::IsTypePromotionOperation(RequestedOperationId))
	{
		FName OpName = NAME_None;
		if (!TryMapOperatorTokenToPromotionName(RequestedOperationId, OpName))
		{
			return MakeInvalidRequestResult(TEXT("unsupported_op_operation"), FString::Printf(TEXT("Unsupported type-promotion op operation '%s'."), *RequestedOperationId));
		}

		UBlueprintFunctionNodeSpawner* Spawner = FindPromotableOperatorSpawner(OpName);
		if (!Spawner)
		{
			return MakeNotFoundResult(Request, FString::Printf(
				TEXT("UE promotable operator spawner was not available for '%s'."),
				*OpName.ToString()));
		}

		return MakePromotableOperatorResult(Request, RequestedOperationId, OpName, Spawner);
	}

	if (RequestedOperationId.Equals(TEXT("array_identical"), ESearchCase::IgnoreCase))
	{
		const FBlueprintHelperArrayTypedPinEvidenceGuardResult GuardResult =
			FBlueprintHelperArrayTypedPinEvidenceGuard::ValidateArrayIdenticalEvidence(Request.ContextEvidence);
		if (!GuardResult.bPassed)
		{
			return MakeInvalidRequestResult(GuardResult.ErrorCode, GuardResult.Message);
		}
	}

	FBlueprintHelperOpCallableEvidence Evidence;
	FString EvidenceErrorCode;
	FString EvidenceMessage;
	if (FBlueprintHelperOpCallableEvidenceReader::Read(Request, Evidence, EvidenceErrorCode, EvidenceMessage))
	{
		FBlueprintHelperCallFunctionResolveRequest CallRequest;
		CallRequest.Blueprint = Request.Blueprint;
		CallRequest.Graph = Request.TargetGraph;
		CallRequest.Query = Evidence.Spec.StableCallableId;
		CallRequest.SearchMode = TEXT("exact");
		CallRequest.AmbiguityPolicy = TEXT("pick_best");
		CallRequest.MaxCandidates = Request.MaxCandidates > 0 ? Request.MaxCandidates : 8;
		CallRequest.Context.Blueprint = Request.Blueprint;
		CallRequest.Context.Graph = Request.TargetGraph;
		CallRequest.CandidatePolicy.RequiredStableCallableIds.Add(Evidence.Spec.StableCallableId);
		if (!Evidence.Spec.RequiredNodeClassPath.IsEmpty())
		{
			CallRequest.CandidatePolicy.PermittedNodeClassPaths.Add(Evidence.Spec.RequiredNodeClassPath);
		}
		ApplyArrayIdenticalEvidence(Evidence, CallRequest);

		const FBlueprintHelperCallFunctionResolveResult CallResult =
			FBlueprintHelperCallFunctionResolver::Resolve(CallRequest);
		return MakeCallableOpResult(Request, Evidence, CallResult);
	}

	if (EvidenceErrorCode != TEXT("missing_op_operation"))
	{
		return MakeInvalidRequestResult(EvidenceErrorCode, EvidenceMessage);
	}

	FName OpName = NAME_None;
	const FString OperatorToken = GetOperatorTokenFromContext(Context);
	if (OperatorToken.IsEmpty())
	{
		return MakeInvalidRequestResult(TEXT("invalid_operator_action_request"), TEXT("operator_context_missing: Semantic.Query or ContextEvidence.operator_token is required."));
	}

	if (!TryMapOperatorTokenToPromotionName(OperatorToken, OpName))
	{
		return MakeInvalidRequestResult(TEXT("unsupported_op_operation"), FString::Printf(
			TEXT("Unsupported operator token '%s'. Supported tokens map to UE type promotion operator names."),
			*OperatorToken));
	}

	UBlueprintFunctionNodeSpawner* Spawner = FindPromotableOperatorSpawner(OpName);
	if (!Spawner)
	{
		return MakeNotFoundResult(Request, FString::Printf(
			TEXT("UE promotable operator spawner was not available for '%s'."),
			*OpName.ToString()));
	}

	return MakePromotableOperatorResult(Request, OperatorToken, OpName, Spawner);
}

bool FBlueprintHelperOperatorActionResolver::TryMapOperatorTokenToPromotionName(
	const FString& Token,
	FName& OutOpName)
{
	const FString Normalized = Token.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("+") || Normalized == TEXT("add"))
	{
		OutOpName = TEXT("Add");
		return true;
	}
	if (Normalized == TEXT("-") || Normalized == TEXT("subtract"))
	{
		OutOpName = TEXT("Subtract");
		return true;
	}
	if (Normalized == TEXT("*") || Normalized == TEXT("multiply"))
	{
		OutOpName = TEXT("Multiply");
		return true;
	}
	if (Normalized == TEXT("/") || Normalized == TEXT("divide"))
	{
		OutOpName = TEXT("Divide");
		return true;
	}
	if (Normalized == TEXT(">") || Normalized == TEXT("greater"))
	{
		OutOpName = TEXT("Greater");
		return true;
	}
	if (Normalized == TEXT(">=") || Normalized == TEXT("greater_equal") || Normalized == TEXT("greater_or_equal"))
	{
		OutOpName = TEXT("GreaterEqual");
		return true;
	}
	if (Normalized == TEXT("<") || Normalized == TEXT("less"))
	{
		OutOpName = TEXT("Less");
		return true;
	}
	if (Normalized == TEXT("<=") || Normalized == TEXT("less_equal") || Normalized == TEXT("less_or_equal"))
	{
		OutOpName = TEXT("LessEqual");
		return true;
	}
	if (Normalized == TEXT("==") || Normalized == TEXT("equal") || Normalized == TEXT("equals"))
	{
		OutOpName = TEXT("EqualEqual");
		return true;
	}
	if (Normalized == TEXT("!=") || Normalized == TEXT("not_equal") || Normalized == TEXT("not_equals"))
	{
		OutOpName = TEXT("NotEqual");
		return true;
	}
	return false;
}

UBlueprintFunctionNodeSpawner* FBlueprintHelperOperatorActionResolver::FindPromotableOperatorSpawner(FName OpName)
{
	if (OpName.IsNone())
	{
		return nullptr;
	}

	FTypePromotion::Get();
	if (UBlueprintFunctionNodeSpawner* Spawner = FTypePromotion::GetOperatorSpawner(OpName))
	{
		return Spawner;
	}

	FBlueprintActionDatabase::Get().RefreshAll();
	return FTypePromotion::GetOperatorSpawner(OpName);
}

FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::MakePromotableOperatorResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& OperatorToken,
	FName OpName,
	UBlueprintFunctionNodeSpawner* Spawner)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.Message = FString::Printf(
		TEXT("Resolved UE promotable operator '%s' for token '%s' using FunctionActionCluster type-promotion evidence."),
		*OpName.ToString(),
		*OperatorToken.TrimStartAndEnd());
	Result.SelectedStableId = MakePromotableOperatorStableId(OpName);
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(MakePromotableOperatorCandidateInfo(OpName, Spawner));
	Result.CandidateActions[0].ReadbackFacts.FindOrAdd(TEXT("op.operation_id")) =
		FBlueprintHelperOpCallableCatalog::NormalizeOperationId(OperatorToken);
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::MakeInvalidRequestResult(
	const FString& Message)
{
	return MakeInvalidRequestResult(TEXT("invalid_operator_action_request"), Message);
}

FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::MakeInvalidRequestResult(
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = ErrorCode.IsEmpty() ? FString(TEXT("invalid_operator_action_request")) : ErrorCode;
	Result.Message = Message;
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::MakeNotFoundResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = TEXT("operator_action_not_found");
	Result.Message = Message.IsEmpty()
		? FString::Printf(
			TEXT("No promotable operator action found for semantic '%s' and query '%s'."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
			*Request.Semantic.Query)
		: Message;
	return Result;
}
