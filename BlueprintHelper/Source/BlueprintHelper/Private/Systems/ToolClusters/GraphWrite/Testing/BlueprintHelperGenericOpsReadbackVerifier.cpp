#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Systems/ToolClusters/GraphWrite/Testing/Utils/GraphWriteTestingUtils.h"

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
		return UGraphWriteTestingUtils::Fail(
			TEXT("wrong_runtime_owner"),
			FString::Printf(TEXT("GenericOps readback failed: result is not resolved, code=%s."), *Result.ErrorCode),
			OutFailureCode,
			OutFailure);
	}
	if (!Result.ErrorCode.IsEmpty())
	{
		return UGraphWriteTestingUtils::Fail(
			*Result.ErrorCode,
			FString::Printf(TEXT("GenericOps readback failed: resolved result carries error code %s."), *Result.ErrorCode),
			OutFailureCode,
			OutFailure);
	}
	if (!Expectation.StableId.IsEmpty()
		&& !Result.SelectedStableId.Equals(Expectation.StableId, ESearchCase::IgnoreCase))
	{
		return UGraphWriteTestingUtils::Fail(
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
		return UGraphWriteTestingUtils::Fail(
			TEXT("missing_evidence"),
			TEXT("GenericOps readback failed: selected spawner evidence missing."),
			OutFailureCode,
			OutFailure);
	}
	if (Expectation.bRequireSelectedFunction && !Result.SelectedFunction.IsValid())
	{
		return UGraphWriteTestingUtils::Fail(
			TEXT("missing_evidence"),
			TEXT("GenericOps readback failed: selected function evidence missing."),
			OutFailureCode,
			OutFailure);
	}

	const FBlueprintHelperCallFunctionCandidateInfo* Candidate = UGraphWriteTestingUtils::FindSelectedCandidate(Result);
	if (!Candidate)
	{
		return UGraphWriteTestingUtils::Fail(
			TEXT("missing_evidence"),
			TEXT("GenericOps readback failed: selected candidate evidence missing."),
			OutFailureCode,
			OutFailure);
	}
	if (!Expectation.NodeClassPath.IsEmpty()
		&& !Candidate->NodeClassPath.Equals(Expectation.NodeClassPath, ESearchCase::IgnoreCase))
	{
		return UGraphWriteTestingUtils::Fail(
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
		const FString Family = UGraphWriteTestingUtils::FactValue(*Candidate, TEXT("generic.family"));
		if (!Family.Equals(Expectation.Family, ESearchCase::IgnoreCase))
		{
			return UGraphWriteTestingUtils::Fail(
				TEXT("missing_evidence"),
				FString::Printf(TEXT("GenericOps readback failed: family fact missing or mismatched for %s."), *Expectation.Family),
				OutFailureCode,
				OutFailure);
		}
	}
	if (!Expectation.OperationId.IsEmpty())
	{
		const FString Operation = UGraphWriteTestingUtils::FactValue(*Candidate, TEXT("generic.operation_id"));
		if (!Operation.Equals(Expectation.OperationId, ESearchCase::IgnoreCase))
		{
			return UGraphWriteTestingUtils::Fail(
				TEXT("missing_evidence"),
				FString::Printf(TEXT("GenericOps readback failed: operation id missing or mismatched for %s."), *Expectation.OperationId),
				OutFailureCode,
				OutFailure);
		}
	}
	if (Expectation.bRequireNoWildcardResidual)
	{
		const FString WildcardResidual = UGraphWriteTestingUtils::FactValue(*Candidate, TEXT("generic.wildcard_residual"));
		if (!WildcardResidual.IsEmpty() && !WildcardResidual.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			return UGraphWriteTestingUtils::Fail(
				TEXT("wildcard_residual"),
				TEXT("GenericOps readback failed: wildcard residual is still present."),
				OutFailureCode,
				OutFailure);
		}
	}

	for (const TPair<FString, FString>& RequiredFact : Expectation.RequiredFacts)
	{
		const FString ActualValue = UGraphWriteTestingUtils::FactValue(*Candidate, RequiredFact.Key);
		if (ActualValue.IsEmpty())
		{
			return UGraphWriteTestingUtils::Fail(
				TEXT("missing_evidence"),
				FString::Printf(TEXT("GenericOps readback failed: required fact %s is missing."), *RequiredFact.Key),
				OutFailureCode,
				OutFailure);
		}
		if (!RequiredFact.Value.IsEmpty()
			&& !ActualValue.Equals(RequiredFact.Value, ESearchCase::IgnoreCase))
		{
			return UGraphWriteTestingUtils::Fail(
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
		return UGraphWriteTestingUtils::Fail(
			TEXT("actual_node_missing"),
			TEXT("GenericOps readback failed: spawned UE node is missing."),
			OutFailureCode,
			OutFailure);
	}
	if (!Expectation.NodeClassPath.IsEmpty()
		&& !SpawnedNode->GetClass()->GetPathName().Equals(Expectation.NodeClassPath, ESearchCase::IgnoreCase))
	{
		return UGraphWriteTestingUtils::Fail(
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
		if (!UGraphWriteTestingUtils::HasPinNamed(SpawnedNode, PinName, TOptional<EEdGraphPinDirection>()))
		{
			return UGraphWriteTestingUtils::Fail(
				TEXT("actual_pin_missing"),
				FString::Printf(TEXT("GenericOps readback failed: required pin %s is missing."), *PinName),
				OutFailureCode,
				OutFailure);
		}
	}
	for (const FString& PinName : Expectation.RequiredInputPins)
	{
		if (!UGraphWriteTestingUtils::HasPinNamed(SpawnedNode, PinName, TOptional<EEdGraphPinDirection>(EGPD_Input)))
		{
			return UGraphWriteTestingUtils::Fail(
				TEXT("actual_pin_missing"),
				FString::Printf(TEXT("GenericOps readback failed: required input pin %s is missing."), *PinName),
				OutFailureCode,
				OutFailure);
		}
	}
	for (const FString& PinName : Expectation.RequiredOutputPins)
	{
		if (!UGraphWriteTestingUtils::HasPinNamed(SpawnedNode, PinName, TOptional<EEdGraphPinDirection>(EGPD_Output)))
		{
			return UGraphWriteTestingUtils::Fail(
				TEXT("actual_pin_missing"),
				FString::Printf(TEXT("GenericOps readback failed: required output pin %s is missing."), *PinName),
				OutFailureCode,
				OutFailure);
		}
	}
	return true;
}
