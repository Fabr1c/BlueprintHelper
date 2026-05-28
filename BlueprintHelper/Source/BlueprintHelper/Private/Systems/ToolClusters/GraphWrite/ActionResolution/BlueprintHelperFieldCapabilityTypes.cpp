#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

const FBlueprintHelperFieldCapabilitySpec* FBlueprintHelperFieldCapabilityRegistry::FindById(const FString& CapabilityId)
{
	return UGraphWriteActionEvidenceUtils::FindKnownFieldCapabilitySpec(CapabilityId, false);
}

TArray<FBlueprintHelperFieldCapabilitySpec> FBlueprintHelperFieldCapabilityRegistry::GetFirstClassSpecs()
{
	TArray<FBlueprintHelperFieldCapabilitySpec> Result;
	Result.Reserve(17);

	for (const FBlueprintHelperFieldCapabilitySpec& Spec : UGraphWriteActionEvidenceUtils::GetAllFieldCapabilitySpecs())
	{
		if (Spec.bFirstClassStatement)
		{
			Result.Add(Spec);
		}
	}

	return Result;
}

bool FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(
	const FString& CapabilityId,
	FString& OutRejectReason)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FindById(CapabilityId);
	if (!Spec)
	{
		OutRejectReason = TEXT("unknown_field_capability");
		return false;
	}

	if (!Spec->bFirstClassStatement)
	{
		OutRejectReason = Spec->RejectReason.IsEmpty()
			? FString(TEXT("unsupported_field_capability"))
			: Spec->RejectReason;
		return false;
	}

	OutRejectReason.Reset();
	return true;
}

TArray<FBlueprintHelperFieldCapabilitySpec> FBlueprintHelperFieldCapabilityRegistry::GetSpecsByOperationAndScope(
	const FString& FieldOperation,
	const FString& FieldScope)
{
	const FString NormalizedOperation = UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(FieldOperation);
	const FString NormalizedScope = UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(FieldScope);
	TArray<FBlueprintHelperFieldCapabilitySpec> Result;

	if (NormalizedOperation.IsEmpty() || NormalizedScope.IsEmpty())
	{
		return Result;
	}

	for (const FBlueprintHelperFieldCapabilitySpec& Spec : UGraphWriteActionEvidenceUtils::GetAllFieldCapabilitySpecs())
	{
		if (!Spec.bFirstClassStatement)
		{
			continue;
		}

		if (UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(Spec.FieldOperation) == NormalizedOperation &&
			UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(Spec.FieldScope) == NormalizedScope)
		{
			Result.Add(Spec);
		}
	}

	return Result;
}

const FBlueprintHelperFieldCapabilitySpec* FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(
	const FString& FieldOperation,
	const FString& FieldScope)
{
	const TArray<FBlueprintHelperFieldCapabilitySpec> Candidates =
		GetSpecsByOperationAndScope(FieldOperation, FieldScope);
	if (Candidates.Num() == 1)
	{
		return UGraphWriteActionEvidenceUtils::FindKnownFieldCapabilitySpec(Candidates[0].Id, true);
	}

	return nullptr;
}

FString FBlueprintHelperFieldCapabilityRegistry::MakeStableCapabilityKey(
	const FBlueprintHelperFieldCapabilitySpec& Spec)
{
	return FString::Printf(
		TEXT("field-capability:%s:%s:%s"),
		*UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(Spec.Id),
		*UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(Spec.FieldOperation),
		*UGraphWriteActionEvidenceUtils::NormalizeFieldCapabilityToken(Spec.FieldScope));
}
