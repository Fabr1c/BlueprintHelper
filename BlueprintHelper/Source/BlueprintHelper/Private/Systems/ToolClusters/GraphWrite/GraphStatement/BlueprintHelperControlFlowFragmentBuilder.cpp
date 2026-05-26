#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFlowFragmentBuilder.h"

namespace
{
static FString NormalizeOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}
}

bool FBlueprintHelperControlFlowFragmentBuilder::SupportsOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperation(Operation);
	return Normalized == TEXT("switch_int")
		|| Normalized == TEXT("switch_string")
		|| Normalized == TEXT("switch_name")
		|| Normalized == TEXT("switch_enum")
		|| Normalized == TEXT("multi_gate");
}

TArray<FString> FBlueprintHelperControlFlowFragmentBuilder::RequiredEvidenceKeys(const FString& Operation)
{
	const FString Normalized = NormalizeOperation(Operation);
	if (Normalized == TEXT("multi_gate"))
	{
		return { TEXT("generic.control.operation"), TEXT("generic.control.dynamic_output_count") };
	}
	if (Normalized.StartsWith(TEXT("switch_")))
	{
		return { TEXT("generic.control.operation"), TEXT("generic.control.case_values") };
	}
	return { TEXT("generic.control.operation") };
}
