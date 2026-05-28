#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperContainerActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	return UGraphWriteActionClusterUtils::ResolveInternal(Request);
}

FBlueprintHelperActionResolutionResult FBlueprintHelperContainerActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	return UGraphWriteActionClusterUtils::ResolveInternal(Context.GetRequest().StatementId == Request.StatementId ? Request : Context.GetRequest());
}
