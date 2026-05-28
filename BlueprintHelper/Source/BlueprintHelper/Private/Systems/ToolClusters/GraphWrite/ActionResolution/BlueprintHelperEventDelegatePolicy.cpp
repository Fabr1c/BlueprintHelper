#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"

FBlueprintHelperEventDelegatePolicyDecision FBlueprintHelperEventDelegatePolicy::Evaluate(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
	if (!Request.TargetGraph || !Request.TargetGraph->GetSchema()
		|| !Request.TargetGraph->GetSchema()->IsA<UEdGraphSchema_K2>())
	{
		return UGraphWriteActionEvidenceUtils::BlockEventDelegate(
			TEXT("incompatible_graph_type"),
			TEXT("EventDelegate use-site operations require a K2 graph schema."),
			EBlueprintHelperActionResolutionStatus::InvalidRequest);
	}

	const FString DuplicatePolicy = Evidence.DuplicatePolicy.IsEmpty()
		? FString(TEXT("fail"))
		: Evidence.DuplicatePolicy.TrimStartAndEnd().ToLower();
	if (DuplicatePolicy == TEXT("replace") || DuplicatePolicy == TEXT("merge"))
	{
		return UGraphWriteActionEvidenceUtils::BlockEventDelegate(
			TEXT("duplicate_mutation_policy_blocked"),
			FString::Printf(TEXT("EventDelegate duplicate policy '%s' belongs to a future mutation-owner plan."), *DuplicatePolicy));
	}
	if (DuplicatePolicy == TEXT("fail") && UGraphWriteActionEvidenceUtils::HasExistingBindingEvidence(Request))
	{
		return UGraphWriteActionEvidenceUtils::BlockEventDelegate(
			TEXT("delegate_duplicate_binding"),
			TEXT("EventDelegate duplicate policy fail rejected an existing projected binding."));
	}

	if (Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
	{
		const FString AssignFactory = Evidence.AssignFactory.TrimStartAndEnd();
		if (!AssignFactory.IsEmpty()
			&& !AssignFactory.Equals(TEXT("ue_delegate_manual_assign_factory"), ESearchCase::IgnoreCase))
		{
			return UGraphWriteActionEvidenceUtils::BlockEventDelegate(
				TEXT("assign_side_effect_blocked"),
				TEXT("delegate.assign may only use ue_delegate_manual_assign_factory; UE Assign spawner side effects are blocked."));
		}
	}

	const FString BlueprintAssignableEvidence = UGraphWriteActionEvidenceUtils::GetDirectEvidenceValue(Request, TEXT("event_delegate.delegate_blueprint_assignable"));
	const FString BlueprintCallableEvidence = UGraphWriteActionEvidenceUtils::GetDirectEvidenceValue(Request, TEXT("event_delegate.delegate_blueprint_callable"));
	if ((Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
			|| Evidence.DelegateOperation.Equals(TEXT("bind"), ESearchCase::IgnoreCase)
			|| Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
		&& UGraphWriteActionEvidenceUtils::IsFalseEvidenceValue(BlueprintAssignableEvidence))
	{
		return UGraphWriteActionEvidenceUtils::BlockEventDelegate(
			TEXT("delegate_not_blueprint_assignable"),
			TEXT("EventDelegate bind/assign/component-bound operations require BlueprintAssignable delegate evidence."));
	}
	if (Evidence.DelegateOperation.Equals(TEXT("call"), ESearchCase::IgnoreCase)
		&& UGraphWriteActionEvidenceUtils::IsFalseEvidenceValue(BlueprintCallableEvidence)
		&& !UGraphWriteActionEvidenceUtils::IsTrueEvidenceValue(BlueprintAssignableEvidence))
	{
		return UGraphWriteActionEvidenceUtils::BlockEventDelegate(
			TEXT("delegate_not_blueprint_callable"),
			TEXT("EventDelegate call requires BlueprintCallable delegate evidence."));
	}

	return UGraphWriteActionEvidenceUtils::AllowEventDelegate();
}
