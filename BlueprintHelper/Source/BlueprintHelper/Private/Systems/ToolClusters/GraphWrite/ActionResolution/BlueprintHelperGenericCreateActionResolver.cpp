#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_GenericCreateObject.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_SpawnActorFromClass.h"
#include "Modules/ModuleManager.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/BlueprintHelperGraphActionUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenWrappers.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

#define MakeInvalidResult FBlueprintHelperGraphActionUtils::MakeInvalidResult
#define MakeUnsupportedResult FBlueprintHelperGraphActionUtils::MakeUnsupportedResult
#define HasFunctionBackedOperationEvidence FBlueprintHelperGraphActionUtils::HasFunctionBackedOperationEvidence
#define ResolveClassEvidence FBlueprintHelperGraphActionUtils::ResolveClassEvidence

bool FBlueprintHelperGenericCreateActionResolver::IsSupportedCreateOperation(const FString& CreateOperation)
{
	const FString Normalized = UGraphWriteActionResolverUtils::NormalizeCreateOperation(CreateOperation);
	return Normalized == TEXT("spawn_actor")
		|| Normalized == TEXT("create_widget")
		|| Normalized == TEXT("construct_object")
		|| Normalized == TEXT("make_array")
		|| Normalized == TEXT("make_map")
		|| Normalized == TEXT("make_set")
		|| Normalized == TEXT("asset_action");
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericCreateActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	const FString Operation = UGraphWriteActionResolverUtils::NormalizeCreateOperation(Context.GetSemantic().CreateOperation);
	if (Operation.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("Create semantic requires create_operation."));
	}

	if (HasFunctionBackedOperationEvidence(Context.GetSemantic()) || UGraphWriteActionResolverUtils::IsFunctionBackedCreateOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("function_backed_operation_wrong_owner"),
			TEXT("Function-backed create operations must route through FunctionActionCluster."));
	}

	if (!IsSupportedCreateOperation(Operation))
	{
		return MakeUnsupportedResult(
			TEXT("unsupported_create_operation"),
			FString::Printf(TEXT("Unsupported create_operation '%s'."), *Operation));
	}

	if (Operation == TEXT("asset_action"))
	{
		return FBlueprintHelperGenericAssetActionResolver::Resolve(Request, Context);
	}

	if (Operation == TEXT("make_array"))
	{
		const FString ElementEvidence = UGraphWriteActionResolverUtils::ResolveContainerElementEvidence(Context.GetSemantic());
		if (ElementEvidence.IsEmpty())
		{
			return MakeInvalidResult(TEXT("needs_more_semantic_context"), TEXT("make_array create requires element pin type evidence."));
		}
		return UGraphWriteActionResolverUtils::MakeResolvedCreateResult(Request, Operation, UK2Node_MakeArray::StaticClass(), ElementEvidence, ElementEvidence);
	}

	if (Operation == TEXT("make_map"))
	{
		const FString KeyEvidence = UGraphWriteActionResolverUtils::ResolveContainerKeyEvidence(Context.GetSemantic());
		const FString ValueEvidence = UGraphWriteActionResolverUtils::ResolveContainerValueEvidence(Context.GetSemantic());
		if (KeyEvidence.IsEmpty() || ValueEvidence.IsEmpty())
		{
			return MakeInvalidResult(TEXT("needs_more_semantic_context"), TEXT("make_map create requires key and value pin type evidence."));
		}
		return UGraphWriteActionResolverUtils::MakeResolvedCreateResult(
			Request,
			Operation,
			UK2Node_MakeMap::StaticClass(),
			KeyEvidence + TEXT(":") + ValueEvidence,
			TEXT("map"));
	}

	if (Operation == TEXT("make_set"))
	{
		const FString ElementEvidence = UGraphWriteActionResolverUtils::ResolveContainerElementEvidence(Context.GetSemantic());
		if (ElementEvidence.IsEmpty())
		{
			return MakeInvalidResult(TEXT("needs_more_semantic_context"), TEXT("make_set create requires element pin type evidence."));
		}
		return UGraphWriteActionResolverUtils::MakeResolvedCreateResult(Request, Operation, UK2Node_MakeSet::StaticClass(), ElementEvidence, TEXT("set"));
	}

	const FString ClassEvidence = ResolveClassEvidence(Context.GetSemantic());
	if (ClassEvidence.IsEmpty())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			FString::Printf(TEXT("%s create requires class evidence."), *Operation));
	}

	if (Operation == TEXT("spawn_actor"))
	{
		return UGraphWriteActionResolverUtils::MakeResolvedCreateResult(Request, Operation, UK2Node_SpawnActorFromClass::StaticClass(), ClassEvidence, ClassEvidence);
	}

	if (Operation == TEXT("create_widget"))
	{
		return UGraphWriteActionResolverUtils::MakeResolvedCreateResult(Request, Operation, UGraphWriteActionResolverUtils::ResolveCreateWidgetNodeClass(), ClassEvidence, ClassEvidence);
	}

	if (Operation == TEXT("construct_object"))
	{
		return UGraphWriteActionResolverUtils::MakeResolvedCreateResult(Request, Operation, UK2Node_GenericCreateObject::StaticClass(), ClassEvidence, ClassEvidence);
	}

	return MakeUnsupportedResult(
		TEXT("unsupported_create_operation"),
		FString::Printf(TEXT("Unsupported create_operation '%s'."), *Operation));
}
