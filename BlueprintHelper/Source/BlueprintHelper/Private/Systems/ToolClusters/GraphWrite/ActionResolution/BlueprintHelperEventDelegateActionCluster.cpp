#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.h"

#include "BlueprintEventNodeSpawner.h"
#include "K2Node_CustomEvent.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"

namespace
{
static FString GetFirstNonEmptyText(const TArray<FString>& Values)
{
	for (const FString& Value : Values)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			return Trimmed;
		}
	}
	return FString();
}

static FString GetEvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static FString GetCustomEventName(const FBlueprintHelperActionClusterContextView& Context)
{
	const FBlueprintHelperActionResolutionRequest& Request = Context.GetRequest();
	return GetFirstNonEmptyText({
		Context.GetSemantic().Query,
		Context.GetSemantic().TargetPath,
		Context.GetSemantic().StableId,
		GetEvidenceValue(Request, TEXT("event_name")),
		GetEvidenceValue(Request, TEXT("custom_event_name")),
		GetEvidenceValue(Request, TEXT("signature_name"))
	});
}

static FString GetDelegateName(const FBlueprintHelperActionClusterContextView& Context)
{
	const FBlueprintHelperActionResolutionRequest& Request = Context.GetRequest();
	return GetFirstNonEmptyText({
		GetEvidenceValue(Request, TEXT("delegate_name")),
		Context.GetSemantic().Query,
		Context.GetSemantic().TargetPath,
		Context.GetSemantic().PropertyPath,
		Context.GetSemantic().StableId
	});
}

static FString GetComponentPath(const FBlueprintHelperActionResolutionRequest& Request)
{
	return GetFirstNonEmptyText({
		GetEvidenceValue(Request, TEXT("component_path")),
		GetEvidenceValue(Request, TEXT("component_object_path"))
	});
}

static FString GetBindingObjectPath(const FBlueprintHelperActionResolutionRequest& Request)
{
	return GetFirstNonEmptyText({
		GetEvidenceValue(Request, TEXT("binding_object_path")),
		GetEvidenceValue(Request, TEXT("binding_object")),
		GetEvidenceValue(Request, TEXT("target_object_path"))
	});
}

static FString GetDelegateSignature(const FBlueprintHelperActionResolutionRequest& Request)
{
	return GetFirstNonEmptyText({
		GetEvidenceValue(Request, TEXT("delegate_signature")),
		GetEvidenceValue(Request, TEXT("signature_name")),
		GetEvidenceValue(Request, TEXT("delegate_type"))
	});
}

static FString MakeCustomEventStableId(const FString& EventName)
{
	return FString::Printf(TEXT("custom_event:%s"), *EventName);
}

static FBlueprintHelperCallFunctionCandidateInfo MakeCustomEventCandidateInfo(
	const FString& EventName,
	UBlueprintEventNodeSpawner* Spawner)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = MakeCustomEventStableId(EventName);
	Candidate.DisplayName = EventName;
	Candidate.Category = TEXT("Event");
	Candidate.NodeClassPath = UK2Node_CustomEvent::StaticClass()->GetPathName();
	Candidate.MatchReason = TEXT("ue_custom_event_node_spawner");
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Spawner != nullptr;
	Candidate.bFromActionDatabase = true;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = false;
	return Candidate;
}

static FBlueprintHelperActionResolutionResult MakeResolvedCustomEventResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& EventName,
	UBlueprintEventNodeSpawner* Spawner)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.Message = FString::Printf(
		TEXT("Resolved custom event '%s' through EventDelegateActionCluster using UBlueprintEventNodeSpawner evidence."),
		*EventName);
	Result.SelectedStableId = MakeCustomEventStableId(EventName);
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(MakeCustomEventCandidateInfo(EventName, Spawner));
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeMissingEvidenceResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& MissingDetail,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("missing_required_evidence");
	Result.Message = FString::Printf(TEXT("%s: %s"), *MissingDetail, *Message);
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeUnsupportedCompleteDelegateSpawnerResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FString& SemanticName,
	const FString& DelegateName)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Result.ErrorCode = TEXT("unsupported_intent");
	Result.Message = FString::Printf(
		TEXT("%s delegate spawner boundary is not complete for delegate '%s': missing safe UE spawner-family routing for projected component/binding object plus delegate signature evidence."),
		*SemanticName,
		*DelegateName);
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
}

FBlueprintHelperActionResolutionResult FBlueprintHelperEventDelegateActionCluster::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!OwnsSemanticKind(Context.GetSemantic().Kind))
	{
		return MakeUnsupportedIntentResult(Request);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Event)
	{
		const FString EventName = GetCustomEventName(Context);
		if (EventName.IsEmpty())
		{
			return MakeMissingEvidenceResult(
				Request,
				TEXT("event_name_missing"),
				TEXT("EventDelegateActionCluster requires Semantic.Query, Semantic.TargetPath, or ContextEvidence.event_name for event semantics."));
		}

		UBlueprintEventNodeSpawner* Spawner = UBlueprintEventNodeSpawner::Create(
			UK2Node_CustomEvent::StaticClass(),
			FName(*EventName));
		if (!Spawner)
		{
			return MakeEventDelegateBlockedResult(
				Request,
				TEXT("event_node_spawner_unavailable"),
				FString::Printf(TEXT("Could not create UBlueprintEventNodeSpawner for custom event '%s'."), *EventName));
		}

		return MakeResolvedCustomEventResult(Request, EventName, Spawner);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::ComponentBoundEvent)
	{
		const FString ComponentPath = GetComponentPath(Request);
		const FString DelegateName = GetDelegateName(Context);
		const FString DelegateSignature = GetDelegateSignature(Request);
		if (ComponentPath.IsEmpty())
		{
			return MakeMissingEvidenceResult(
				Request,
				TEXT("component_missing"),
				TEXT("Component-bound event resolution requires ContextEvidence.component_path."));
		}
		if (DelegateName.IsEmpty())
		{
			return MakeMissingEvidenceResult(
				Request,
				TEXT("delegate_name_missing"),
				TEXT("Component-bound event resolution requires ContextEvidence.delegate_name or semantic delegate query."));
		}
		if (DelegateSignature.IsEmpty())
		{
			return MakeMissingEvidenceResult(
				Request,
				TEXT("delegate_signature_missing"),
				TEXT("Component-bound event resolution requires ContextEvidence.delegate_signature."));
		}

		return MakeUnsupportedCompleteDelegateSpawnerResult(
			Request,
			TEXT("component_bound_event"),
			DelegateName);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Bind)
	{
		const FString BindingObjectPath = GetBindingObjectPath(Request);
		const FString DelegateName = GetDelegateName(Context);
		const FString DelegateSignature = GetDelegateSignature(Request);
		if (BindingObjectPath.IsEmpty())
		{
			return MakeMissingEvidenceResult(
				Request,
				TEXT("binding_object_missing"),
				TEXT("Delegate bind resolution requires ContextEvidence.binding_object_path."));
		}
		if (DelegateName.IsEmpty())
		{
			return MakeMissingEvidenceResult(
				Request,
				TEXT("delegate_name_missing"),
				TEXT("Delegate bind resolution requires ContextEvidence.delegate_name or semantic delegate query."));
		}
		if (DelegateSignature.IsEmpty())
		{
			return MakeMissingEvidenceResult(
				Request,
				TEXT("delegate_signature_missing"),
				TEXT("Delegate bind resolution requires ContextEvidence.delegate_signature."));
		}

		return MakeUnsupportedCompleteDelegateSpawnerResult(
			Request,
			TEXT("bind"),
			DelegateName);
	}

	return MakeUnsupportedIntentResult(Request);
}

bool FBlueprintHelperEventDelegateActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Bind:
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
		TEXT("EventDelegateActionCluster needs event/delegate binding context for semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}
