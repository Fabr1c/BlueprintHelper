#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.h"

#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

namespace
{
static bool Fail(FString& OutFailure, const FString& Message)
{
	OutFailure = Message;
	return false;
}

static const FBlueprintHelperCallFunctionCandidateInfo* FirstCandidate(
	const FBlueprintHelperActionResolutionResult& Result)
{
	return Result.CandidateActions.Num() > 0 ? &Result.CandidateActions[0] : nullptr;
}

static const FBlueprintHelperCallFunctionCandidateInfo* FindSelectedCandidate(
	const FBlueprintHelperActionResolutionResult& Result)
{
	const FString SelectedStableId = !Result.SelectedStableId.TrimStartAndEnd().IsEmpty()
		? Result.SelectedStableId.TrimStartAndEnd()
		: Result.FunctionCandidate.StableId.TrimStartAndEnd();
	if (!SelectedStableId.IsEmpty())
	{
		if (const FBlueprintHelperCallFunctionCandidateInfo* Candidate = Result.CandidateActions.FindByPredicate(
			[&SelectedStableId](const FBlueprintHelperCallFunctionCandidateInfo& CandidateInfo)
			{
				return CandidateInfo.StableId.Equals(SelectedStableId, ESearchCase::IgnoreCase);
			}))
		{
			return Candidate;
		}
	}

	return Result.CandidateActions.Num() == 1 ? FirstCandidate(Result) : nullptr;
}

static bool FunctionHasInputPin(const UFunction* Function, const FString& PinName)
{
	if (!Function || PinName.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		const FProperty* Property = *PropIt;
		if (Property
			&& !Property->HasAnyPropertyFlags(CPF_ReturnParm)
			&& Property->GetName().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

static bool FunctionHasReturnPin(const UFunction* Function)
{
	if (!Function)
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		const FProperty* Property = *PropIt;
		if (Property && Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			return true;
		}
	}
	return false;
}
}

bool FBlueprintHelperOpCoverageReadbackVerifier::Verify(
	const FBlueprintHelperActionResolutionResult& Result,
	const FBlueprintHelperOpCoverageReadbackExpectation& Expectation,
	FString& OutFailure)
{
	OutFailure.Reset();
	if (Result.Status != EBlueprintHelperActionResolutionStatus::Resolved)
	{
		return Fail(OutFailure, FString::Printf(TEXT("op readback failed: result is not resolved, code=%s."), *Result.ErrorCode));
	}
	if (!Result.ErrorCode.IsEmpty())
	{
		return Fail(OutFailure, FString::Printf(TEXT("op readback failed: resolved result carries error code %s."), *Result.ErrorCode));
	}
	if (!Expectation.StableId.IsEmpty() && !Result.SelectedStableId.Equals(Expectation.StableId, ESearchCase::IgnoreCase))
	{
		return Fail(OutFailure, FString::Printf(
			TEXT("op readback failed: stable id mismatch, expected %s got %s."),
			*Expectation.StableId,
			*Result.SelectedStableId));
	}

	const FBlueprintHelperCallFunctionCandidateInfo* Candidate = FindSelectedCandidate(Result);
	if (!Candidate)
	{
		return Fail(OutFailure, TEXT("op readback failed: selected candidate evidence missing."));
	}
	if (!Expectation.OperationId.IsEmpty()
		&& !Candidate->ReadbackFacts.FindRef(TEXT("op.operation_id")).Equals(Expectation.OperationId, ESearchCase::IgnoreCase))
	{
		return Fail(OutFailure, FString::Printf(TEXT("op readback failed: operation id missing or mismatched for %s."), *Expectation.OperationId));
	}
	if (!Expectation.NodeClassPath.IsEmpty() && !Candidate->NodeClassPath.Equals(Expectation.NodeClassPath, ESearchCase::IgnoreCase))
	{
		return Fail(OutFailure, FString::Printf(
			TEXT("op readback failed: node class mismatch, expected %s got %s."),
			*Expectation.NodeClassPath,
			*Candidate->NodeClassPath));
	}
	if (!Candidate->ReadbackFacts.FindRef(TEXT("op.node_class_path")).Equals(Candidate->NodeClassPath, ESearchCase::IgnoreCase))
	{
		return Fail(OutFailure, TEXT("op readback failed: node class readback fact does not match selected candidate."));
	}
	if (!Candidate->ReadbackFacts.FindRef(TEXT("op.wildcard_residual")).Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		return Fail(OutFailure, TEXT("op readback failed: wildcard residual is not false."));
	}
	if (Candidate->ReadbackFacts.FindRef(TEXT("op.spawner_class")).IsEmpty())
	{
		return Fail(OutFailure, TEXT("op readback failed: spawner class fact missing."));
	}

	if (Expectation.bTypePromotion)
	{
		if (Candidate->ReadbackFacts.FindRef(TEXT("op.type_promotion_operator")).IsEmpty())
		{
			return Fail(OutFailure, TEXT("op readback failed: type promotion operator fact missing."));
		}
		return true;
	}

	const UFunction* Function = Result.SelectedFunction.Get();
	if (!Function)
	{
		return Fail(OutFailure, TEXT("op readback failed: selected function missing."));
	}
	if (!Candidate->ReadbackFacts.FindRef(TEXT("op.source_function_path")).Equals(Result.SelectedStableId, ESearchCase::IgnoreCase))
	{
		return Fail(OutFailure, TEXT("op readback failed: source function path fact does not match selected stable id."));
	}
	for (const FString& RequiredInputPin : Expectation.RequiredInputPins)
	{
		if (!FunctionHasInputPin(Function, RequiredInputPin))
		{
			return Fail(OutFailure, FString::Printf(TEXT("op readback failed: required input pin %s missing."), *RequiredInputPin));
		}
	}
	if (!Expectation.ExpectedReturnType.IsEmpty())
	{
		if (!FunctionHasReturnPin(Function))
		{
			return Fail(OutFailure, TEXT("op readback failed: expected return pin missing."));
		}
		if (!Candidate->ReturnType.Equals(Expectation.ExpectedReturnType, ESearchCase::IgnoreCase))
		{
			return Fail(OutFailure, FString::Printf(
				TEXT("op readback failed: return type mismatch, expected %s got %s."),
				*Expectation.ExpectedReturnType,
				*Candidate->ReturnType));
		}
	}
	return true;
}
