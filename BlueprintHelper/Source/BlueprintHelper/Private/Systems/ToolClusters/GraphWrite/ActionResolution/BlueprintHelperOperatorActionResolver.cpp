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
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

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

	const FString RequestedOperationId = UGraphWriteActionClusterUtils::GetRequestedOpOperationId(Request);
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
		UGraphWriteActionClusterUtils::ApplyArrayIdenticalEvidence(Evidence, CallRequest);

		const FBlueprintHelperCallFunctionResolveResult CallResult =
			FBlueprintHelperCallFunctionResolver::Resolve(CallRequest);
		return UGraphWriteActionClusterUtils::MakeCallableOpResult(Request, Evidence, CallResult);
	}

	if (EvidenceErrorCode != TEXT("missing_op_operation"))
	{
		return MakeInvalidRequestResult(EvidenceErrorCode, EvidenceMessage);
	}

	FName OpName = NAME_None;
	const FString OperatorToken = UGraphWriteActionClusterUtils::GetOperatorTokenFromContext(Context);
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
	Result.SelectedStableId = UGraphWriteActionClusterUtils::MakePromotableOperatorStableId(OpName);
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(UGraphWriteActionClusterUtils::MakePromotableOperatorCandidateInfo(OpName, Spawner));
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
