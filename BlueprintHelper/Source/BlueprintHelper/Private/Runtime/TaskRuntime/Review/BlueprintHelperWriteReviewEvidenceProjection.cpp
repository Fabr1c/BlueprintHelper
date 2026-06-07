#include "Runtime/TaskRuntime/Review/BlueprintHelperWriteReviewEvidenceProjection.h"

FBlueprintHelperReviewAtomicTarget FBlueprintHelperWriteReviewEvidenceProjection::BuildAtomicTarget(
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	const FBlueprintHelperReviewBoundaryModel& Boundary,
	const FBlueprintHelperDiagnosticProjection& DiagnosticProjection)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Boundary.AssetKey.IsEmpty() ? AcceptedPayload.TargetAssetPath : Boundary.AssetKey;
	Target.GraphName = Boundary.LocationKey.IsEmpty() ? AcceptedPayload.GraphName : Boundary.LocationKey;
	Target.TargetKey = Boundary.TargetKey;
	Target.TargetKind = Boundary.TargetKind;
	Target.TargetSubKind = Boundary.TargetSubKind;
	Target.ScopeIdentity = Boundary.ScopeIdentity.IsEmpty()
		? FBlueprintHelperAcceptedPayloadModelUtils::MakeReviewScopeIdentity(AcceptedPayload)
		: Boundary.ScopeIdentity;
	Target.LifecycleObjectKey = Boundary.LifecycleObjectKey;
	Target.LifecycleParentKey = Boundary.LifecycleParentKey;
	Target.VisualGroupKey = Boundary.VisualGroupKey;
	Target.Ownership = AcceptedPayload.WriteFamily;
	if (!DiagnosticProjection.Code.IsEmpty())
	{
		Target.Diagnostics.Add(DiagnosticItemFromProjection(DiagnosticProjection));
	}
	return Target;
}

void FBlueprintHelperWriteReviewEvidenceProjection::ApplyBoundaryToAtomicTarget(
	const FBlueprintHelperReviewBoundaryModel& Boundary,
	FBlueprintHelperReviewAtomicTarget& InOutTarget)
{
	InOutTarget.AssetPath = Boundary.AssetKey.IsEmpty() ? InOutTarget.AssetPath : Boundary.AssetKey;
	InOutTarget.GraphName = Boundary.LocationKey.IsEmpty() ? InOutTarget.GraphName : Boundary.LocationKey;
	InOutTarget.TargetKey = Boundary.TargetKey.IsEmpty() ? InOutTarget.TargetKey : Boundary.TargetKey;
	InOutTarget.TargetKind = Boundary.TargetKind.IsEmpty() ? InOutTarget.TargetKind : Boundary.TargetKind;
	InOutTarget.TargetSubKind = Boundary.TargetSubKind.IsEmpty() ? InOutTarget.TargetSubKind : Boundary.TargetSubKind;
	InOutTarget.ScopeIdentity = Boundary.ScopeIdentity.IsEmpty() ? InOutTarget.ScopeIdentity : Boundary.ScopeIdentity;
	InOutTarget.LifecycleObjectKey = Boundary.LifecycleObjectKey.IsEmpty() ? InOutTarget.LifecycleObjectKey : Boundary.LifecycleObjectKey;
	InOutTarget.LifecycleParentKey = Boundary.LifecycleParentKey.IsEmpty() ? InOutTarget.LifecycleParentKey : Boundary.LifecycleParentKey;
	InOutTarget.VisualGroupKey = Boundary.VisualGroupKey.IsEmpty() ? InOutTarget.VisualGroupKey : Boundary.VisualGroupKey;
}

void FBlueprintHelperWriteReviewEvidenceProjection::AttachDiagnostics(
	FBlueprintHelperWriteReviewEvidence& Evidence,
	const TArray<FBlueprintHelperDiagnosticProjection>& Diagnostics)
{
	for (const FBlueprintHelperDiagnosticProjection& Projection : Diagnostics)
	{
		FBlueprintHelperDiagnosticItem Item = DiagnosticItemFromProjection(Projection);
		Evidence.Diagnostics.Add(Item);

		for (FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			const bool bMatchesTarget =
				Projection.TargetKey.IsEmpty() ||
				Projection.TargetKey == Target.TargetKey ||
				(!Projection.ScopeIdentity.IsEmpty() && Projection.ScopeIdentity == Target.ScopeIdentity);
			if (bMatchesTarget)
			{
				if (Item.TargetKey.IsEmpty())
				{
					Item.TargetKey = Target.TargetKey;
				}
				Target.Diagnostics.Add(Item);
			}
		}
	}
}

FBlueprintHelperDiagnosticItem FBlueprintHelperWriteReviewEvidenceProjection::DiagnosticItemFromProjection(
	const FBlueprintHelperDiagnosticProjection& Projection)
{
	FBlueprintHelperDiagnosticItem Item;
	if (Projection.Details.IsValid())
	{
		BlueprintHelperDiagnosticItemFromJson(Projection.Details, Item);
	}
	Item.Severity = BlueprintHelperDiagnosticSeverityFromString(Projection.Severity);
	Item.Code = Projection.Code.IsEmpty() ? Item.Code : Projection.Code;
	Item.Message = Projection.Message.IsEmpty() ? Item.Message : Projection.Message;
	Item.GraphName = Projection.GraphName.IsEmpty() ? Item.GraphName : Projection.GraphName;
	Item.TargetKey = Projection.TargetKey.IsEmpty() ? Item.TargetKey : Projection.TargetKey;
	return Item;
}
