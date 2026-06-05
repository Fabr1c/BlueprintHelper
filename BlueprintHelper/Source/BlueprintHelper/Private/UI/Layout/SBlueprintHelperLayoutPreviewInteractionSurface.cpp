#include "UI/Layout/SBlueprintHelperLayoutPreviewInteractionSurface.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

class FBlueprintHelperLayoutPreviewMouseUpFinalizer : public IInputProcessor
{
public:
	explicit FBlueprintHelperLayoutPreviewMouseUpFinalizer(
		const TWeakPtr<SBlueprintHelperLayoutPreviewInteractionSurface>& InSurface)
		: Surface(InSurface)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (TSharedPtr<SBlueprintHelperLayoutPreviewInteractionSurface> PinnedSurface = Surface.Pin())
			{
				PinnedSurface->FinishInteraction();
			}
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("BlueprintHelperLayoutPreviewMouseUpFinalizer");
	}

private:
	TWeakPtr<SBlueprintHelperLayoutPreviewInteractionSurface> Surface;
};

void SBlueprintHelperLayoutPreviewInteractionSurface::Construct(const FArguments& InArgs)
{
	InteractionBeginDelegate = InArgs._OnInteractionBegin;
	InteractionEndDelegate = InArgs._OnInteractionEnd;
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

SBlueprintHelperLayoutPreviewInteractionSurface::~SBlueprintHelperLayoutPreviewInteractionSurface()
{
	UnregisterMouseUpFinalizer();
}

FReply SBlueprintHelperLayoutPreviewInteractionSurface::OnPreviewMouseButtonDown(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (!bInteractionOpen)
		{
			bInteractionOpen = true;
			RegisterMouseUpFinalizer();
			InteractionBeginDelegate.ExecuteIfBound();
		}
	}
	return FReply::Unhandled();
}

FReply SBlueprintHelperLayoutPreviewInteractionSurface::OnMouseButtonUp(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FinishInteraction();
	}
	return FReply::Unhandled();
}

void SBlueprintHelperLayoutPreviewInteractionSurface::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	FinishInteraction();
	SCompoundWidget::OnFocusLost(InFocusEvent);
}

void SBlueprintHelperLayoutPreviewInteractionSurface::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	FinishInteraction();
	SCompoundWidget::OnMouseCaptureLost(CaptureLostEvent);
}

void SBlueprintHelperLayoutPreviewInteractionSurface::RegisterMouseUpFinalizer()
{
	if (MouseUpFinalizer.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	MouseUpFinalizer = MakeShared<FBlueprintHelperLayoutPreviewMouseUpFinalizer>(
		StaticCastWeakPtr<SBlueprintHelperLayoutPreviewInteractionSurface>(AsWeak()));
	FSlateApplication::Get().RegisterInputPreProcessor(MouseUpFinalizer);
}

void SBlueprintHelperLayoutPreviewInteractionSurface::UnregisterMouseUpFinalizer()
{
	if (!MouseUpFinalizer.IsValid() || !FSlateApplication::IsInitialized())
	{
		MouseUpFinalizer.Reset();
		return;
	}

	FSlateApplication::Get().UnregisterInputPreProcessor(MouseUpFinalizer);
	MouseUpFinalizer.Reset();
}

void SBlueprintHelperLayoutPreviewInteractionSurface::FinishInteraction()
{
	if (!bInteractionOpen)
	{
		UnregisterMouseUpFinalizer();
		return;
	}

	bInteractionOpen = false;
	UnregisterMouseUpFinalizer();
	InteractionEndDelegate.ExecuteIfBound();
}
