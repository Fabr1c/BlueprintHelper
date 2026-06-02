#pragma once

#include "CoreMinimal.h"
#include "GraphEditor.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
using FBlueprintHelperGraphEditorViewLocation = FVector2f;
#else
using FBlueprintHelperGraphEditorViewLocation = FVector2D;
#endif

class FBlueprintHelperGraphEditorVersionCompat
{
public:
	static FBlueprintHelperGraphEditorViewLocation ZeroViewLocation();

	static void GetViewLocation(
		const TSharedPtr<SGraphEditor>& GraphEditor,
		FBlueprintHelperGraphEditorViewLocation& OutLocation,
		float& OutZoomAmount);

	static void SetViewLocation(
		const TSharedPtr<SGraphEditor>& GraphEditor,
		const FBlueprintHelperGraphEditorViewLocation& Location,
		const float ZoomAmount,
		const FGuid& BookmarkId);
};
