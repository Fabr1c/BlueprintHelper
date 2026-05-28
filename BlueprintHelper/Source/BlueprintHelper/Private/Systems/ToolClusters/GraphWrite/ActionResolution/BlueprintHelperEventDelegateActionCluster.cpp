#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"

#include "BlueprintBoundEventNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_RemoveDelegate.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!OwnsSemanticKind(Context.GetSemantic().Kind))
	{
		return MakeUnsupportedIntentResult(Request);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		FBlueprintHelperEventDelegateUseSiteEvidence Evidence;
		FString MissingDetail;
		FString MissingMessage;
		if (!FBlueprintHelperEventDelegateUseSiteEvidenceReader::TryRead(
			Request,
			Context.GetSemantic().Kind,
			Evidence,
			MissingDetail,
			MissingMessage))
		{
			return UGraphWriteActionClusterUtils::MakeMissingEvidenceResult(Request, MissingDetail, MissingMessage);
		}
		const FBlueprintHelperEventDelegatePolicyDecision PolicyDecision =
			FBlueprintHelperEventDelegatePolicy::Evaluate(Request, Evidence);
		if (!PolicyDecision.bAllowed)
		{
			return UGraphWriteActionClusterUtils::MakePolicyResult(PolicyDecision);
		}
		FString ExistingBindingId;
		if (UGraphWriteActionClusterUtils::ShouldReturnExistingBinding(Request, Evidence, ExistingBindingId))
		{
			return UGraphWriteActionClusterUtils::MakeResolvedEventDelegateResult(
				ExistingBindingId,
				nullptr,
				UK2Node_ComponentBoundEvent::StaticClass(),
				Evidence.DelegateName,
				TEXT("existing_component_bound_event_binding"),
				FString::Printf(TEXT("Returned existing component-bound event binding '%s'."), *ExistingBindingId));
		}

		UBlueprintBoundEventNodeSpawner* Spawner = UBlueprintBoundEventNodeSpawner::Create(
			UK2Node_ComponentBoundEvent::StaticClass(),
			Evidence.DelegateProperty);
		if (!Spawner)
		{
			return UGraphWriteActionClusterUtils::MakeEventDelegateBlockedResult(
				Request,
				TEXT("component_bound_event_spawner_unavailable"),
				FString::Printf(TEXT("Could not create component-bound event node spawner for delegate '%s'."), *Evidence.DelegateName));
		}

		const FString StableId = UGraphWriteActionClusterUtils::MakeComponentBoundEventStableId(Evidence);
		return UGraphWriteActionClusterUtils::MakeResolvedEventDelegateResult(
			StableId,
			Spawner,
			UK2Node_ComponentBoundEvent::StaticClass(),
			Evidence.DelegateName,
			TEXT("ue_bound_event_node_spawner"),
			FString::Printf(TEXT("Resolved component-bound event '%s' through UBlueprintBoundEventNodeSpawner evidence."), *Evidence.DelegateName));
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Delegate)
	{
		FBlueprintHelperEventDelegateUseSiteEvidence Evidence;
		FString MissingDetail;
		FString MissingMessage;
		if (!FBlueprintHelperEventDelegateUseSiteEvidenceReader::TryRead(
			Request,
			Context.GetSemantic().Kind,
			Evidence,
			MissingDetail,
			MissingMessage))
		{
			return UGraphWriteActionClusterUtils::MakeMissingEvidenceResult(Request, MissingDetail, MissingMessage);
		}
		const FBlueprintHelperEventDelegatePolicyDecision PolicyDecision =
			FBlueprintHelperEventDelegatePolicy::Evaluate(Request, Evidence);
		if (!PolicyDecision.bAllowed)
		{
			return UGraphWriteActionClusterUtils::MakePolicyResult(PolicyDecision);
		}

		TSubclassOf<UK2Node_BaseMCDelegate> NodeClass = UGraphWriteActionClusterUtils::DelegateNodeClassForOperation(Evidence.DelegateOperation);
		if (!NodeClass)
		{
			return UGraphWriteActionClusterUtils::MakeEventDelegateBlockedResult(
				Request,
				TEXT("delegate_operation_spawner_unavailable"),
				FString::Printf(TEXT("No delegate spawner node class is available for operation '%s'."), *Evidence.DelegateOperation));
		}

		const FString StableId = UGraphWriteActionClusterUtils::MakeDelegateStableId(Evidence);
		FString ExistingBindingId;
		if (UGraphWriteActionClusterUtils::ShouldReturnExistingBinding(Request, Evidence, ExistingBindingId))
		{
			return UGraphWriteActionClusterUtils::MakeResolvedEventDelegateResult(
				ExistingBindingId,
				nullptr,
				NodeClass.Get(),
				Evidence.DelegateName,
				TEXT("existing_delegate_binding"),
				FString::Printf(TEXT("Returned existing delegate binding '%s' for operation '%s'."), *ExistingBindingId, *Evidence.DelegateOperation));
		}
		if (Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
		{
			return UGraphWriteActionClusterUtils::MakeResolvedEventDelegateResult(
				StableId,
				nullptr,
				NodeClass.Get(),
				Evidence.DelegateName,
				TEXT("ue_delegate_manual_assign_factory"),
				FString::Printf(TEXT("Resolved delegate '%s' operation 'assign' through manual assign factory evidence."), *Evidence.DelegateName));
		}

		UBlueprintDelegateNodeSpawner* Spawner = UBlueprintDelegateNodeSpawner::Create(
			NodeClass,
			Evidence.DelegateProperty);
		if (!Spawner)
		{
			return UGraphWriteActionClusterUtils::MakeEventDelegateBlockedResult(
				Request,
				TEXT("delegate_node_spawner_unavailable"),
				FString::Printf(TEXT("Could not create delegate node spawner for operation '%s' and delegate '%s'."), *Evidence.DelegateOperation, *Evidence.DelegateName));
		}

		return UGraphWriteActionClusterUtils::MakeResolvedEventDelegateResult(
			StableId,
			Spawner,
			NodeClass.Get(),
			Evidence.DelegateName,
			TEXT("ue_delegate_node_spawner"),
			FString::Printf(TEXT("Resolved delegate '%s' operation '%s' through UBlueprintDelegateNodeSpawner evidence."), *Evidence.DelegateName, *Evidence.DelegateOperation));
	}

	return MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperEventDelegateActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Delegate:
		return true;
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("unsupported_event_delegate_cluster_semantic");
	Result.Message = FString::Printf(
		TEXT("EventDelegateActionCluster does not own semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::MakeNeedsMoreSemanticContextResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = FString::Printf(
		TEXT("EventDelegateActionCluster needs projected component/delegate binding context for semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}
