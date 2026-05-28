#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_DynamicCast.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformSpawnerFactory.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/BlueprintHelperGraphActionUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

#define MakeInvalidResult FBlueprintHelperGraphActionUtils::MakeInvalidResult
#define MakeUnsupportedResult FBlueprintHelperGraphActionUtils::MakeUnsupportedResult
#define HasFunctionBackedOperationEvidence FBlueprintHelperGraphActionUtils::HasFunctionBackedOperationEvidence
#define ResolveClassEvidence FBlueprintHelperGraphActionUtils::ResolveClassEvidence

bool FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedTransformOperation(const FString& TransformOperation)
{
	const FString Normalized = NormalizeOperation(TransformOperation);
	return Normalized == TEXT("dynamic_cast")
		|| Normalized == TEXT("class_cast")
		|| Normalized == TEXT("type_promotion");
}

bool FBlueprintHelperGenericTransformScheduleActionResolver::IsSupportedScheduleOperation(const FString& ScheduleOperation)
{
	const FString Normalized = NormalizeOperation(ScheduleOperation);
	return Normalized == TEXT("timer_delegate_node")
		|| Normalized == TEXT("latent_or_async_node");
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericTransformScheduleActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Convert)
	{
		return UGraphWriteActionResolverUtils::ResolveConvert(Request, Context);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Schedule)
	{
		return UGraphWriteActionResolverUtils::ResolveSchedule(Request, Context);
	}

	return MakeUnsupportedResult(
		TEXT("unsupported_generic_transform_schedule_semantic"),
		FString::Printf(
			TEXT("GenericTransformScheduleActionResolver does not own semantic kind '%s'."),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(Context.GetSemantic().Kind)));
}
