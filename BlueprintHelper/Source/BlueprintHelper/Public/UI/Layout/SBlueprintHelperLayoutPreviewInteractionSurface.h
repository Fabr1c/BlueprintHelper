#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IInputProcessor;
class FBlueprintHelperLayoutPreviewMouseUpFinalizer;

DECLARE_DELEGATE(FBlueprintHelperLayoutPreviewInteractionEvent);

class BLUEPRINTHELPER_API SBlueprintHelperLayoutPreviewInteractionSurface : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperLayoutPreviewInteractionSurface)
		{
		}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FBlueprintHelperLayoutPreviewInteractionEvent, OnInteractionBegin)
		SLATE_EVENT(FBlueprintHelperLayoutPreviewInteractionEvent, OnInteractionEnd)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SBlueprintHelperLayoutPreviewInteractionSurface() override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	friend class FBlueprintHelperLayoutPreviewMouseUpFinalizer;

	void RegisterMouseUpFinalizer();
	void UnregisterMouseUpFinalizer();
	void FinishInteraction();

	FBlueprintHelperLayoutPreviewInteractionEvent InteractionBeginDelegate;
	FBlueprintHelperLayoutPreviewInteractionEvent InteractionEndDelegate;
	TSharedPtr<IInputProcessor> MouseUpFinalizer;
	bool bInteractionOpen = false;
};
