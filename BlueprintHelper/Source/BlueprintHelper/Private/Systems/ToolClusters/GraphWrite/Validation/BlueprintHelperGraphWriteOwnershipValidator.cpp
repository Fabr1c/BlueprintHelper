#include "Systems/ToolClusters/GraphWrite/Validation/BlueprintHelperGraphWriteOwnershipValidator.h"

namespace BlueprintHelperGraphWriteOwnershipValidation
{
	static FString NormalizeBlockRef(const FString& BlockRef)
	{
		FString Result = BlockRef;
		Result.TrimStartAndEndInline();
		return Result;
	}

	static bool TargetMatchesBlockRef(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FString& BlockRef)
	{
		if (BlockRef.IsEmpty() || !Target.TargetKind.Equals(TEXT("graph_block"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		const FString TargetKey = Target.TargetKey.TrimStartAndEnd();
		return TargetKey.Equals(BlockRef, ESearchCase::IgnoreCase) ||
			TargetKey.EndsWith(FString::Printf(TEXT(":block:%s"), *BlockRef), ESearchCase::IgnoreCase) ||
			TargetKey.EndsWith(FString::Printf(TEXT("_%s"), *BlockRef), ESearchCase::IgnoreCase) ||
			TargetKey.EndsWith(BlockRef, ESearchCase::IgnoreCase);
	}

	static bool HasAtomicTargetForBlockRef(
		const TArray<FBlueprintHelperReviewAtomicTarget>& AtomicTargets,
		const FString& BlockRef)
	{
		for (const FBlueprintHelperReviewAtomicTarget& Target : AtomicTargets)
		{
			if (TargetMatchesBlockRef(Target, BlockRef))
			{
				return true;
			}
		}
		return false;
	}
}

FBlueprintHelperGraphWriteOwnershipValidationResult FBlueprintHelperGraphWriteOwnershipValidator::Validate(
	const FBlueprintHelperGraphWriteOwnershipValidationInput& Input)
{
	using namespace BlueprintHelperGraphWriteOwnershipValidation;

	FBlueprintHelperGraphWriteOwnershipValidationResult Result;
	for (const FString& RawBlockRef : Input.GeneratedBlockRefs)
	{
		const FString BlockRef = NormalizeBlockRef(RawBlockRef);
		if (BlockRef.IsEmpty())
		{
			continue;
		}

		if (HasAtomicTargetForBlockRef(Input.AtomicTargets, BlockRef))
		{
			continue;
		}

		FBlueprintGeneratorDiagnostic Diagnostic;
		Diagnostic.Code = TEXT("unregistered_generated_node");
		Diagnostic.NodeId = BlockRef;
		Diagnostic.Message = FString::Printf(
			TEXT("Generated GraphWrite block '%s' has no matching Review atomic target."),
			*BlockRef);
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
	}

	Result.bPassed = Result.Diagnostics.Num() == 0;
	return Result;
}
