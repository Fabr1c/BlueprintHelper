#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h"

namespace
{
static FBlueprintHelperEventDelegatePolicyDecision Allow()
{
	FBlueprintHelperEventDelegatePolicyDecision Decision;
	Decision.bAllowed = true;
	Decision.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	return Decision;
}

static FBlueprintHelperEventDelegatePolicyDecision Block(
	const FString& ErrorCode,
	const FString& Message,
	const EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::Blocked)
{
	FBlueprintHelperEventDelegatePolicyDecision Decision;
	Decision.bAllowed = false;
	Decision.Status = Status;
	Decision.ErrorCode = ErrorCode;
	Decision.Message = Message;
	return Decision;
}

static FString EvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static bool IsTrueEvidence(const FString& Value)
{
	return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
}

static bool IsFalseEvidence(const FString& Value)
{
	return Value.Equals(TEXT("false"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("0"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("no"), ESearchCase::IgnoreCase);
}

static bool HasExistingBindingEvidence(const FBlueprintHelperActionResolutionRequest& Request)
{
	return !EvidenceValue(Request, TEXT("event_delegate.existing_binding_evidence_id")).IsEmpty()
		|| IsTrueEvidence(EvidenceValue(Request, TEXT("event_delegate.existing_binding")));
}
}

FBlueprintHelperEventDelegatePolicyDecision FBlueprintHelperEventDelegatePolicy::Evaluate(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
	if (!Request.TargetGraph || !Request.TargetGraph->GetSchema()
		|| !Request.TargetGraph->GetSchema()->IsA<UEdGraphSchema_K2>())
	{
		return Block(
			TEXT("incompatible_graph_type"),
			TEXT("EventDelegate use-site operations require a K2 graph schema."),
			EBlueprintHelperActionResolutionStatus::InvalidRequest);
	}

	const FString DuplicatePolicy = Evidence.DuplicatePolicy.IsEmpty()
		? FString(TEXT("fail"))
		: Evidence.DuplicatePolicy.TrimStartAndEnd().ToLower();
	if (DuplicatePolicy == TEXT("replace") || DuplicatePolicy == TEXT("merge"))
	{
		return Block(
			TEXT("duplicate_mutation_policy_blocked"),
			FString::Printf(TEXT("EventDelegate duplicate policy '%s' belongs to a future mutation-owner plan."), *DuplicatePolicy));
	}
	if (DuplicatePolicy == TEXT("fail") && HasExistingBindingEvidence(Request))
	{
		return Block(
			TEXT("delegate_duplicate_binding"),
			TEXT("EventDelegate duplicate policy fail rejected an existing projected binding."));
	}

	if (Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
	{
		const FString AssignFactory = Evidence.AssignFactory.TrimStartAndEnd();
		if (!AssignFactory.IsEmpty()
			&& !AssignFactory.Equals(TEXT("ue_delegate_manual_assign_factory"), ESearchCase::IgnoreCase))
		{
			return Block(
				TEXT("assign_side_effect_blocked"),
				TEXT("delegate.assign may only use ue_delegate_manual_assign_factory; UE Assign spawner side effects are blocked."));
		}
	}

	const FString BlueprintAssignableEvidence = EvidenceValue(Request, TEXT("event_delegate.delegate_blueprint_assignable"));
	const FString BlueprintCallableEvidence = EvidenceValue(Request, TEXT("event_delegate.delegate_blueprint_callable"));
	if ((Evidence.SemanticKind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent
			|| Evidence.DelegateOperation.Equals(TEXT("bind"), ESearchCase::IgnoreCase)
			|| Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
		&& IsFalseEvidence(BlueprintAssignableEvidence))
	{
		return Block(
			TEXT("delegate_not_blueprint_assignable"),
			TEXT("EventDelegate bind/assign/component-bound operations require BlueprintAssignable delegate evidence."));
	}
	if (Evidence.DelegateOperation.Equals(TEXT("call"), ESearchCase::IgnoreCase)
		&& IsFalseEvidence(BlueprintCallableEvidence)
		&& !IsTrueEvidence(BlueprintAssignableEvidence))
	{
		return Block(
			TEXT("delegate_not_blueprint_callable"),
			TEXT("EventDelegate call requires BlueprintCallable delegate evidence."));
	}

	return Allow();
}
