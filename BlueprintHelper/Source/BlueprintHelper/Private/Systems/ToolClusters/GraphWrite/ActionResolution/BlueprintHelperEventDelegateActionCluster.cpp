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

namespace
{
static FBlueprintHelperActionResolutionResult MakeMissingEvidenceResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& MissingDetail,
	const FString& Message)
{
	const FString MissingRequiredEvidenceBoundary = TEXT("missing_required_evidence");
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = MissingDetail;
	Result.Message = FString::Printf(TEXT("%s: %s: %s"), *MissingRequiredEvidenceBoundary, *MissingDetail, *Message);
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeEventDelegateBlockedResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static FBlueprintHelperActionResolutionResult MakePolicyResult(
	const FBlueprintHelperEventDelegatePolicyDecision& Decision)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = Decision.Status;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = Decision.ErrorCode;
	Result.Message = Decision.Message;
	return Result;
}

static bool DelegateOperationRequiresHandler(const FString& Operation)
{
	return Operation.Equals(TEXT("bind"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("assign"), ESearchCase::IgnoreCase)
		|| Operation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase);
}

static TSubclassOf<UK2Node_BaseMCDelegate> DelegateNodeClassForOperation(const FString& Operation)
{
	if (Operation.Equals(TEXT("bind"), ESearchCase::IgnoreCase))
	{
		return UK2Node_AddDelegate::StaticClass();
	}
	if (Operation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
	{
		return UK2Node_AssignDelegate::StaticClass();
	}
	if (Operation.Equals(TEXT("unbind"), ESearchCase::IgnoreCase))
	{
		return UK2Node_RemoveDelegate::StaticClass();
	}
	if (Operation.Equals(TEXT("call"), ESearchCase::IgnoreCase))
	{
		return UK2Node_CallDelegate::StaticClass();
	}
	if (Operation.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		return UK2Node_ClearDelegate::StaticClass();
	}
	return nullptr;
}

static FString MakeComponentBoundEventStableId(const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
	return FString::Printf(
		TEXT("component_bound_event:%s:%s"),
		*Evidence.DelegatePropertyPath,
		*Evidence.ComponentBindingFieldPath);
}

static FString MakeDelegateStableId(const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence)
{
	FString StableId = FString::Printf(
		TEXT("delegate:%s:%s"),
		*Evidence.DelegateOperation,
		*Evidence.DelegatePropertyPath);
	if (DelegateOperationRequiresHandler(Evidence.DelegateOperation))
	{
		StableId += FString::Printf(TEXT(":%s"), *Evidence.HandlerName);
	}
	return StableId;
}

static FBlueprintHelperCallFunctionCandidateInfo MakeEventDelegateCandidateInfo(
	const FString& StableId,
	const FString& DisplayName,
	UClass* NodeClass,
	const FString& MatchReason)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = StableId;
	Candidate.DisplayName = DisplayName;
	Candidate.Category = TEXT("EventDelegate");
	Candidate.NodeClassPath = NodeClass ? NodeClass->GetPathName() : FString();
	Candidate.MatchReason = MatchReason;
	Candidate.Score = 100;
	Candidate.bGraphCompatible = NodeClass != nullptr;
	Candidate.bFromActionDatabase = true;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = false;
	return Candidate;
}

static FBlueprintHelperActionResolutionResult MakeResolvedEventDelegateResult(
	const FString& StableId,
	UBlueprintNodeSpawner* Spawner,
	UClass* NodeClass,
	const FString& DisplayName,
	const FString& MatchReason,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.Message = Message;
	Result.SelectedStableId = StableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(MakeEventDelegateCandidateInfo(StableId, DisplayName, NodeClass, MatchReason));
	return Result;
}

static FString EventDelegateEvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static bool ShouldReturnExistingBinding(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
	FString& OutExistingBindingId)
{
	if (!Evidence.DuplicatePolicy.Equals(TEXT("return_existing"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	OutExistingBindingId = EventDelegateEvidenceValue(Request, TEXT("event_delegate.existing_binding_evidence_id"));
	return !OutExistingBindingId.IsEmpty();
}
}

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
			return MakeMissingEvidenceResult(Request, MissingDetail, MissingMessage);
		}
		const FBlueprintHelperEventDelegatePolicyDecision PolicyDecision =
			FBlueprintHelperEventDelegatePolicy::Evaluate(Request, Evidence);
		if (!PolicyDecision.bAllowed)
		{
			return MakePolicyResult(PolicyDecision);
		}
		FString ExistingBindingId;
		if (ShouldReturnExistingBinding(Request, Evidence, ExistingBindingId))
		{
			return MakeResolvedEventDelegateResult(
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
			return MakeEventDelegateBlockedResult(
				Request,
				TEXT("component_bound_event_spawner_unavailable"),
				FString::Printf(TEXT("Could not create component-bound event node spawner for delegate '%s'."), *Evidence.DelegateName));
		}

		const FString StableId = MakeComponentBoundEventStableId(Evidence);
		return MakeResolvedEventDelegateResult(
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
			return MakeMissingEvidenceResult(Request, MissingDetail, MissingMessage);
		}
		const FBlueprintHelperEventDelegatePolicyDecision PolicyDecision =
			FBlueprintHelperEventDelegatePolicy::Evaluate(Request, Evidence);
		if (!PolicyDecision.bAllowed)
		{
			return MakePolicyResult(PolicyDecision);
		}

		TSubclassOf<UK2Node_BaseMCDelegate> NodeClass = DelegateNodeClassForOperation(Evidence.DelegateOperation);
		if (!NodeClass)
		{
			return MakeEventDelegateBlockedResult(
				Request,
				TEXT("delegate_operation_spawner_unavailable"),
				FString::Printf(TEXT("No delegate spawner node class is available for operation '%s'."), *Evidence.DelegateOperation));
		}

		const FString StableId = MakeDelegateStableId(Evidence);
		FString ExistingBindingId;
		if (ShouldReturnExistingBinding(Request, Evidence, ExistingBindingId))
		{
			return MakeResolvedEventDelegateResult(
				ExistingBindingId,
				nullptr,
				NodeClass.Get(),
				Evidence.DelegateName,
				TEXT("existing_delegate_binding"),
				FString::Printf(TEXT("Returned existing delegate binding '%s' for operation '%s'."), *ExistingBindingId, *Evidence.DelegateOperation));
		}
		if (Evidence.DelegateOperation.Equals(TEXT("assign"), ESearchCase::IgnoreCase))
		{
			return MakeResolvedEventDelegateResult(
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
			return MakeEventDelegateBlockedResult(
				Request,
				TEXT("delegate_node_spawner_unavailable"),
				FString::Printf(TEXT("Could not create delegate node spawner for operation '%s' and delegate '%s'."), *Evidence.DelegateOperation, *Evidence.DelegateName));
		}

		return MakeResolvedEventDelegateResult(
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
