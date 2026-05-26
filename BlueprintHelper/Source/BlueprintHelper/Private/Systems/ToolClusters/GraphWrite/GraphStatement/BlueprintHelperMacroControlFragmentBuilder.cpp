#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperMacroControlFragmentBuilder.h"

namespace
{
static FString NormalizeOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}
}

bool FBlueprintHelperMacroControlFragmentBuilder::SupportsOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperation(Operation);
	return Normalized == TEXT("do_once")
		|| Normalized == TEXT("do_n")
		|| Normalized == TEXT("gate")
		|| Normalized == TEXT("flip_flop")
		|| Normalized == TEXT("for_loop")
		|| Normalized == TEXT("for_loop_with_break")
		|| Normalized == TEXT("foreach_loop")
		|| Normalized == TEXT("foreach_loop_with_break")
		|| Normalized == TEXT("while_loop");
}

TArray<FString> FBlueprintHelperMacroControlFragmentBuilder::RequiredEvidenceKeys()
{
	return {
		TEXT("generic.control.operation"),
		TEXT("generic.macro.graph_path"),
		TEXT("generic.macro.pin_shape_snapshot")
	};
}
