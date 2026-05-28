#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MultiGate.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchString.h"
#include "UObject/WeakObjectPtr.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!UGraphWriteActionResolverUtils::IsGenericNodeSpawnerSemantic(Context.GetSemantic().Kind))
	{
		return UGraphWriteActionResolverUtils::MakeUnsupportedGenericSemanticResult(
			Request,
			TEXT("Generic NodeSpawner candidate resolver only accepts select/control semantics; Struct/TypeStructure construct/deconstruct must use FBlueprintHelperStructTypeStructureActionResolver."));
	}

	return UGraphWriteActionResolverUtils::ResolveSingletonControlFlowNodeSpawner(Request);
}
