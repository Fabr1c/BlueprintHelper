#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperK2GraphEntryEvidence.h"

static FString BlueprintHelperK2GraphEntryEvidenceKindToString(
	const EBlueprintHelperK2GraphEntryKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperK2GraphEntryKind::Event:
		return TEXT("event");
	case EBlueprintHelperK2GraphEntryKind::CustomEvent:
		return TEXT("custom_event");
	case EBlueprintHelperK2GraphEntryKind::FunctionEntry:
		return TEXT("function_entry");
	case EBlueprintHelperK2GraphEntryKind::FunctionResult:
		return TEXT("function_result");
	case EBlueprintHelperK2GraphEntryKind::MacroEntry:
		return TEXT("macro_entry");
	case EBlueprintHelperK2GraphEntryKind::MacroExit:
		return TEXT("macro_exit");
	default:
		return TEXT("unknown");
	}
}

bool FBlueprintHelperK2GraphEntryEvidenceProjector::ProjectToAtomicTarget(
	const FBlueprintHelperK2GraphEntryEvidence& Evidence,
	const int32 StepIndex,
	const int32 AtomicIndex,
	FBlueprintHelperReviewAtomicTarget& OutTarget,
	FString& OutError)
{
	OutTarget = FBlueprintHelperReviewAtomicTarget();
	OutError.Reset();

	if (Evidence.AssetPath.IsEmpty())
	{
		OutError = TEXT("k2_graph_entry_evidence_missing_asset_path");
		return false;
	}
	if (Evidence.GraphName.IsEmpty())
	{
		OutError = TEXT("k2_graph_entry_evidence_missing_graph_name");
		return false;
	}
	if (!Evidence.EntryIdentity.bValid ||
		Evidence.EntryIdentity.Kind == EBlueprintHelperK2GraphEntryKind::Unknown ||
		Evidence.EntryIdentity.StableName.IsEmpty())
	{
		OutError = TEXT("k2_graph_entry_evidence_invalid_entry_identity");
		return false;
	}
	if (Evidence.BodyEntryAnchorJson.IsEmpty())
	{
		OutError = TEXT("k2_graph_entry_evidence_missing_body_entry_anchor");
		return false;
	}
	if (Evidence.AfterBodyFingerprint.IsEmpty())
	{
		OutError = TEXT("k2_graph_entry_evidence_missing_after_body_fingerprint");
		return false;
	}

	const FString EntryKind = BlueprintHelperK2GraphEntryEvidenceKindToString(Evidence.EntryIdentity.Kind);
	OutTarget.AssetPath = Evidence.AssetPath;
	OutTarget.Surface = EBlueprintHelperReviewSurface::Graph;
	OutTarget.GraphName = Evidence.GraphName;
	OutTarget.TargetKind = TEXT("k2_graph_entry");
	OutTarget.TargetSubKind = EntryKind;
	OutTarget.TargetKey = FString::Printf(
		TEXT("k2_graph_entry:%s:%s:%s"),
		*Evidence.GraphName,
		*EntryKind,
		*Evidence.EntryIdentity.StableName);
	OutTarget.ScopeIdentity = FString::Printf(
		TEXT("%s|%s|%s"),
		*Evidence.AssetPath,
		*Evidence.GraphName,
		*OutTarget.TargetKey);
	OutTarget.LifecycleObjectKey = OutTarget.TargetKey;
	OutTarget.VisualGroupKey = FString::Printf(
		TEXT("k2_graph_entry|%s|%s|%s"),
		*Evidence.GraphName,
		*EntryKind,
		*Evidence.EntryIdentity.StableName);
	OutTarget.DisplayLabel = FString::Printf(
		TEXT("K2 %s %s.%s"),
		*EntryKind,
		*Evidence.GraphName,
		*Evidence.EntryIdentity.StableName);
	OutTarget.Ownership = Evidence.TargetOwnership.IsEmpty()
		? TEXT("agent_authored")
		: Evidence.TargetOwnership;
	OutTarget.NodeGuid = Evidence.EntryIdentity.NodeGuid;
	OutTarget.PropertyPath = Evidence.OperationKind;
	OutTarget.AnchorJson = Evidence.BodyEntryAnchorJson;
	OutTarget.GraphBodyBoundaryJson = Evidence.GraphBodyBoundaryJson;
	OutTarget.BeforeSnapshotJson = Evidence.BeforeBodySnapshotJson;
	OutTarget.AfterSnapshotJson = Evidence.AfterBodySnapshotJson;
	OutTarget.ReadbackFingerprintAfter = Evidence.AfterBodyFingerprint;
	OutTarget.ExecutionOrder = StepIndex;
	OutTarget.TaskStepIndex = StepIndex;
	OutTarget.AtomicIndex = AtomicIndex;
	return true;
}
