#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "K2Node_PromotableOperator.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"

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

	FName OpName = NAME_None;
	const FString OperatorToken = GetOperatorTokenFromContext(Context);
	if (OperatorToken.IsEmpty())
	{
		return MakeInvalidRequestResult(TEXT("operator_context_missing: Semantic.Query or ContextEvidence.operator_token is required."));
	}

	if (!TryMapOperatorTokenToPromotionName(OperatorToken, OpName))
	{
		return MakeInvalidRequestResult(FString::Printf(
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
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::MakeInvalidRequestResult(
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = TEXT("invalid_operator_action_request");
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
