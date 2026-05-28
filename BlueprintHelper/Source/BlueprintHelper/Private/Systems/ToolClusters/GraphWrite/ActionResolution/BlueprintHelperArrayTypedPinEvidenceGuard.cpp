#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"

FBlueprintHelperArrayTypedPinEvidenceGuardResult FBlueprintHelperArrayTypedPinEvidenceGuard::ValidateArrayIdenticalEvidence(
	const TMap<FString, FString>& Evidence)
{
	const FString LhsEvidence = UGraphWriteActionEvidenceUtils::GetMapEvidenceValue(Evidence, TEXT("op.array_lhs_pin_type"));
	const FString RhsEvidence = UGraphWriteActionEvidenceUtils::GetMapEvidenceValue(Evidence, TEXT("op.array_rhs_pin_type"));
	if (LhsEvidence.IsEmpty() || RhsEvidence.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailArrayPinEvidence(TEXT("array_typed_pin_missing"), TEXT("array_identical requires op.array_lhs_pin_type and op.array_rhs_pin_type."));
	}

	FBlueprintHelperArrayTypedPinEvidenceGuardResult Result;
	Result.LhsPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(LhsEvidence);
	Result.RhsPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(RhsEvidence);

	const FString LhsElement = UGraphWriteActionEvidenceUtils::BuildArrayElementIdentity(Result.LhsPinType);
	const FString RhsElement = UGraphWriteActionEvidenceUtils::BuildArrayElementIdentity(Result.RhsPinType);
	if (LhsElement.IsEmpty() || RhsElement.IsEmpty())
	{
		return UGraphWriteActionEvidenceUtils::FailArrayPinEvidence(TEXT("array_typed_pin_missing"), TEXT("array_identical requires non-wildcard array element pin evidence on both sides."));
	}
	if (!LhsElement.Equals(RhsElement, ESearchCase::IgnoreCase))
	{
		return UGraphWriteActionEvidenceUtils::FailArrayPinEvidence(TEXT("array_typed_pin_mismatch"), FString::Printf(
			TEXT("array_identical requires matching array element evidence, got lhs=%s rhs=%s."),
			*LhsElement,
			*RhsElement));
	}

	Result.bPassed = true;
	return Result;
}
