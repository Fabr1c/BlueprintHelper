// BlueprintHelper Review details geometry resolver.

#include "UI/Review/BlueprintHelperReviewDetailsGeometryResolver.h"

#include "IDetailsView.h"
#include "PropertyPath.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "SKismetInspector.h"
#include "UI/Review/BlueprintHelperReviewPanelGeometryUtils.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.h"
#include "UObject/UnrealType.h"

bool FBlueprintHelperReviewDetailsGeometryResolver::ResolveRowGeometry(
	const FBlueprintHelperReviewVisibleChange& Change,
	const TSharedPtr<SWidget>& OverlayWidget,
	const FBlueprintHelperReviewDetailsGeometryResolutionContext& Context,
	FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor) const
{
	if (!Context.KismetInspector.IsValid())
	{
		OutAnchor.Reason = TEXT("details_inspector_unavailable");
		return false;
	}
	if (!OverlayWidget.IsValid())
	{
		OutAnchor.Reason = TEXT("overlay_geometry_unavailable");
		return false;
	}

	TArray<FString> Candidates;
	FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(
		FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(Change, EBlueprintHelperReviewSurface::Details),
		Candidates);
	FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Change.LocationKey, Candidates);
	FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Change.DisplayLabel, Candidates);
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface == EBlueprintHelperReviewSurface::Details
			|| BlueprintHelperReviewTargetKindCanRouteToDetails(Target.TargetKind))
		{
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.TargetKey, Candidates);
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.PropertyPath, Candidates);
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.DisplayLabel, Candidates);
			FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Target.TargetKind, Candidates);
		}
	}

	bool bRequestedPropertyScroll = false;
	UObject* DetailsObject = Context.ResolveDetailsObject ? Context.ResolveDetailsObject() : nullptr;
	if (DetailsObject)
	{
		if (TSharedPtr<IDetailsView> PropertyView = Context.KismetInspector->GetPropertyView())
		{
			const TArray<FString> InitialCandidates = Candidates;
			for (const FString& Candidate : InitialCandidates)
			{
				FString PropertyName = Candidate;
				PropertyName.TrimStartAndEndInline();
				int32 DelimiterIndex = INDEX_NONE;
				if (PropertyName.FindLastChar(TEXT(':'), DelimiterIndex)
					|| PropertyName.FindLastChar(TEXT('/'), DelimiterIndex)
					|| PropertyName.FindLastChar(TEXT('.'), DelimiterIndex))
				{
					PropertyName = PropertyName.Mid(DelimiterIndex + 1);
				}
				PropertyName.TrimStartAndEndInline();
				if (PropertyName.IsEmpty())
				{
					continue;
				}

				if (FProperty* Property = FindFProperty<FProperty>(DetailsObject->GetClass(), FName(*PropertyName)))
				{
					FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(Property->GetName(), Candidates);
					FBlueprintHelperReviewPanelGeometryUtils::AddSearchCandidatesFromText(
						Property->GetDisplayNameText().ToString(),
						Candidates);
					const TSharedRef<FPropertyPath> PropertyPath =
						FPropertyPath::Create(TWeakFieldPtr<FProperty>(Property));
#if BLUEPRINTHELPER_UE_HAS_DETAILS_VIEW_SCROLL_PROPERTY_BOOL
					PropertyView->ScrollPropertyIntoView(*PropertyPath, true);
#endif
					PropertyView->HighlightProperty(*PropertyPath);
					bRequestedPropertyScroll = true;
				}
			}
		}
	}

	if (Candidates.Num() == 0)
	{
		OutAnchor.Reason = TEXT("missing_geometry_target");
		return false;
	}

	if (FBlueprintHelperReviewPanelGeometryUtils::ResolveTextGeometryRecursive(
		Context.KismetInspector.ToSharedRef(),
		OverlayWidget,
		Candidates,
		TEXT("details_text"),
		OutAnchor))
	{
		return true;
	}

	OutAnchor.TargetText = Candidates[0];
	OutAnchor.Reason = bRequestedPropertyScroll
		? TEXT("details_row_geometry_not_ready")
		: TEXT("no_matching_details_text");
	return false;
}
