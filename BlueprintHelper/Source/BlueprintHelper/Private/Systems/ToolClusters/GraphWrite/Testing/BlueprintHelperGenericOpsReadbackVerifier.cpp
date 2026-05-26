#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

namespace
{
static bool Fail(
	const TCHAR* Code,
	const FString& Message,
	FString& OutFailureCode,
	FString& OutFailure)
{
	OutFailureCode = Code;
	OutFailure = Message;
	return false;
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

	return Result.CandidateActions.Num() == 1 ? &Result.CandidateActions[0] : nullptr;
}

static FString FactValue(
	const FBlueprintHelperCallFunctionCandidateInfo& Candidate,
	const FString& Key)
{
	if (const FString* Value = Candidate.ReadbackFacts.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	if (const FString* Value = Candidate.CapabilityFacts.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static bool HasPinNamed(
	const UEdGraphNode* Node,
	const FString& PinName,
	const TOptional<EEdGraphPinDirection> Direction)
{
	if (!Node)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || !Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Direction.IsSet() || Pin->Direction == Direction.GetValue())
		{
			return true;
		}
	}
	return false;
}
}

bool FBlueprintHelperGenericOpsReadbackVerifier::Verify(
	const FBlueprintHelperActionResolutionResult& Result,
	const FBlueprintHelperGenericOpsReadbackExpectation& Expectation,
	FString& OutFailureCode,
	FString& OutFailure)
{
	OutFailureCode.Reset();
	OutFailure.Reset();

	if (Result.Status != EBlueprintHelperActionResolutionStatus::Resolved)
	{
		return Fail(
			TEXT("wrong_runtime_owner"),
			FString::Printf(TEXT("GenericOps readback failed: result is not resolved, code=%s."), *Result.ErrorCode),
			OutFailureCode,
			OutFailure);
	}
	if (!Result.ErrorCode.IsEmpty())
	{
		return Fail(
			*Result.ErrorCode,
			FString::Printf(TEXT("GenericOps readback failed: resolved result carries error code %s."), *Result.ErrorCode),
			OutFailureCode,
			OutFailure);
	}
	if (!Expectation.StableId.IsEmpty()
		&& !Result.SelectedStableId.Equals(Expectation.StableId, ESearchCase::IgnoreCase))
	{
		return Fail(
			TEXT("missing_evidence"),
			FString::Printf(
				TEXT("GenericOps readback failed: stable id mismatch, expected %s got %s."),
				*Expectation.StableId,
				*Result.SelectedStableId),
			OutFailureCode,
			OutFailure);
	}
	if (Expectation.bRequireSelectedSpawner && !Result.SelectedSpawner.IsValid())
	{
		return Fail(
			TEXT("missing_evidence"),
			TEXT("GenericOps readback failed: selected spawner evidence missing."),
			OutFailureCode,
			OutFailure);
	}
	if (Expectation.bRequireSelectedFunction && !Result.SelectedFunction.IsValid())
	{
		return Fail(
			TEXT("missing_evidence"),
			TEXT("GenericOps readback failed: selected function evidence missing."),
			OutFailureCode,
			OutFailure);
	}

	const FBlueprintHelperCallFunctionCandidateInfo* Candidate = FindSelectedCandidate(Result);
	if (!Candidate)
	{
		return Fail(
			TEXT("missing_evidence"),
			TEXT("GenericOps readback failed: selected candidate evidence missing."),
			OutFailureCode,
			OutFailure);
	}
	if (!Expectation.NodeClassPath.IsEmpty()
		&& !Candidate->NodeClassPath.Equals(Expectation.NodeClassPath, ESearchCase::IgnoreCase))
	{
		return Fail(
			TEXT("missing_evidence"),
			FString::Printf(
				TEXT("GenericOps readback failed: node class mismatch, expected %s got %s."),
				*Expectation.NodeClassPath,
				*Candidate->NodeClassPath),
			OutFailureCode,
			OutFailure);
	}
	if (!Expectation.Family.IsEmpty())
	{
		const FString Family = FactValue(*Candidate, TEXT("generic.family"));
		if (!Family.Equals(Expectation.Family, ESearchCase::IgnoreCase))
		{
			return Fail(
				TEXT("missing_evidence"),
				FString::Printf(TEXT("GenericOps readback failed: family fact missing or mismatched for %s."), *Expectation.Family),
				OutFailureCode,
				OutFailure);
		}
	}
	if (!Expectation.OperationId.IsEmpty())
	{
		const FString Operation = FactValue(*Candidate, TEXT("generic.operation_id"));
		if (!Operation.Equals(Expectation.OperationId, ESearchCase::IgnoreCase))
		{
			return Fail(
				TEXT("missing_evidence"),
				FString::Printf(TEXT("GenericOps readback failed: operation id missing or mismatched for %s."), *Expectation.OperationId),
				OutFailureCode,
				OutFailure);
		}
	}
	if (Expectation.bRequireNoWildcardResidual)
	{
		const FString WildcardResidual = FactValue(*Candidate, TEXT("generic.wildcard_residual"));
		if (!WildcardResidual.IsEmpty() && !WildcardResidual.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			return Fail(
				TEXT("wildcard_residual"),
				TEXT("GenericOps readback failed: wildcard residual is still present."),
				OutFailureCode,
				OutFailure);
		}
	}

	for (const TPair<FString, FString>& RequiredFact : Expectation.RequiredFacts)
	{
		const FString ActualValue = FactValue(*Candidate, RequiredFact.Key);
		if (ActualValue.IsEmpty())
		{
			return Fail(
				TEXT("missing_evidence"),
				FString::Printf(TEXT("GenericOps readback failed: required fact %s is missing."), *RequiredFact.Key),
				OutFailureCode,
				OutFailure);
		}
		if (!RequiredFact.Value.IsEmpty()
			&& !ActualValue.Equals(RequiredFact.Value, ESearchCase::IgnoreCase))
		{
			return Fail(
				TEXT("missing_evidence"),
				FString::Printf(
					TEXT("GenericOps readback failed: required fact %s expected %s got %s."),
					*RequiredFact.Key,
					*RequiredFact.Value,
					*ActualValue),
				OutFailureCode,
				OutFailure);
		}
	}

	return true;
}

bool FBlueprintHelperGenericOpsReadbackVerifier::Verify(
	const FBlueprintHelperActionResolutionResult& Result,
	const UEdGraphNode* SpawnedNode,
	const FBlueprintHelperGenericOpsReadbackExpectation& Expectation,
	FString& OutFailureCode,
	FString& OutFailure)
{
	if (!Verify(Result, Expectation, OutFailureCode, OutFailure))
	{
		return false;
	}
	if (!SpawnedNode)
	{
		return Fail(
			TEXT("actual_node_missing"),
			TEXT("GenericOps readback failed: spawned UE node is missing."),
			OutFailureCode,
			OutFailure);
	}
	if (!Expectation.NodeClassPath.IsEmpty()
		&& !SpawnedNode->GetClass()->GetPathName().Equals(Expectation.NodeClassPath, ESearchCase::IgnoreCase))
	{
		return Fail(
			TEXT("actual_node_class_mismatch"),
			FString::Printf(
				TEXT("GenericOps readback failed: spawned node class mismatch, expected %s got %s."),
				*Expectation.NodeClassPath,
				*SpawnedNode->GetClass()->GetPathName()),
			OutFailureCode,
			OutFailure);
	}
	for (const FString& PinName : Expectation.RequiredPins)
	{
		if (!HasPinNamed(SpawnedNode, PinName, TOptional<EEdGraphPinDirection>()))
		{
			return Fail(
				TEXT("actual_pin_missing"),
				FString::Printf(TEXT("GenericOps readback failed: required pin %s is missing."), *PinName),
				OutFailureCode,
				OutFailure);
		}
	}
	for (const FString& PinName : Expectation.RequiredInputPins)
	{
		if (!HasPinNamed(SpawnedNode, PinName, TOptional<EEdGraphPinDirection>(EGPD_Input)))
		{
			return Fail(
				TEXT("actual_pin_missing"),
				FString::Printf(TEXT("GenericOps readback failed: required input pin %s is missing."), *PinName),
				OutFailureCode,
				OutFailure);
		}
	}
	for (const FString& PinName : Expectation.RequiredOutputPins)
	{
		if (!HasPinNamed(SpawnedNode, PinName, TOptional<EEdGraphPinDirection>(EGPD_Output)))
		{
			return Fail(
				TEXT("actual_pin_missing"),
				FString::Printf(TEXT("GenericOps readback failed: required output pin %s is missing."), *PinName),
				OutFailureCode,
				OutFailure);
		}
	}
	return true;
}
