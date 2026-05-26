#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.h"

namespace
{
static FString GetEvidenceValue(const FBlueprintHelperActionResolutionRequest& Request, const FString& Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static FString ReadRequestedOperationId(const FBlueprintHelperActionResolutionRequest& Request)
{
	const FString FunctionOperation = Request.Semantic.FunctionOperation.TrimStartAndEnd();
	if (FunctionOperation.StartsWith(TEXT("op."), ESearchCase::IgnoreCase))
	{
		return FunctionOperation;
	}

	const FString EvidenceOperation = GetEvidenceValue(Request, TEXT("op.operation_id"));
	if (!EvidenceOperation.IsEmpty())
	{
		return EvidenceOperation;
	}

	return FString();
}
}

bool FBlueprintHelperOpCallableEvidenceReader::Read(
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperOpCallableEvidence& OutEvidence,
	FString& OutErrorCode,
	FString& OutMessage)
{
	OutEvidence = FBlueprintHelperOpCallableEvidence();
	OutErrorCode.Reset();
	OutMessage.Reset();

	const FString OperationId = FBlueprintHelperOpCallableCatalog::NormalizeOperationId(ReadRequestedOperationId(Request));
	if (OperationId.IsEmpty())
	{
		OutErrorCode = TEXT("missing_op_operation");
		OutMessage = TEXT("Op callable evidence requires op.operation_id or Semantic.FunctionOperation=op.<id>.");
		return false;
	}

	if (const FBlueprintHelperOpCallableSpec* Excluded = FBlueprintHelperOpCallableCatalog::FindExcludedSpec(OperationId))
	{
		OutErrorCode = Excluded->RejectionCode.IsEmpty()
			? FString(TEXT("excluded_op_operation"))
			: Excluded->RejectionCode;
		OutMessage = FString::Printf(TEXT("Op operation is excluded: %s."), *OperationId);
		return false;
	}

	const FBlueprintHelperOpCallableSpec* Spec = FBlueprintHelperOpCallableCatalog::FindSupportedSpec(OperationId);
	if (!Spec)
	{
		OutErrorCode = TEXT("unsupported_op_operation");
		OutMessage = FString::Printf(TEXT("Unsupported op operation: %s."), *OperationId);
		return false;
	}

	for (const FString& RequiredKey : Spec->RequiredEvidenceKeys)
	{
		if (GetEvidenceValue(Request, RequiredKey).IsEmpty())
		{
			OutErrorCode = FString::Printf(TEXT("missing_op_evidence.%s"), *RequiredKey);
			OutMessage = FString::Printf(TEXT("Op operation %s requires evidence key %s."), *OperationId, *RequiredKey);
			return false;
		}
	}

	OutEvidence.OperationId = OperationId;
	OutEvidence.Spec = *Spec;
	OutEvidence.Facts.Add(TEXT("op.operation_id"), OperationId);
	OutEvidence.Facts.Add(TEXT("op.spawn_family"), Spec->SpawnFamily);
	OutEvidence.Facts.Add(TEXT("op.stable_callable_id"), Spec->StableCallableId);
	OutEvidence.Facts.Add(TEXT("op.required_node_class_path"), Spec->RequiredNodeClassPath);
	for (const TPair<FString, FString>& EvidencePair : Request.ContextEvidence)
	{
		if (EvidencePair.Key.StartsWith(TEXT("op.")) && !EvidencePair.Value.TrimStartAndEnd().IsEmpty())
		{
			OutEvidence.Facts.FindOrAdd(EvidencePair.Key, EvidencePair.Value.TrimStartAndEnd());
		}
	}
	return true;
}
