#include "Shared/BlueprintHelperGraphEditorVersionCompat.h"

FBlueprintHelperGraphEditorViewLocation FBlueprintHelperGraphEditorVersionCompat::ZeroViewLocation()
{
	return FBlueprintHelperGraphEditorViewLocation::ZeroVector;
}

void FBlueprintHelperGraphEditorVersionCompat::GetViewLocation(
	const TSharedPtr<SGraphEditor>& GraphEditor,
	FBlueprintHelperGraphEditorViewLocation& OutLocation,
	float& OutZoomAmount)
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->GetViewLocation(OutLocation, OutZoomAmount);
	}
}

void FBlueprintHelperGraphEditorVersionCompat::SetViewLocation(
	const TSharedPtr<SGraphEditor>& GraphEditor,
	const FBlueprintHelperGraphEditorViewLocation& Location,
	const float ZoomAmount,
	const FGuid& BookmarkId)
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->SetViewLocation(Location, ZoomAmount, BookmarkId);
	}
}
