// BlueprintHelper TaskSpec / ReadContext workbench presenter.

#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchPresenter.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Systems/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchServices.h"

FBlueprintHelperTaskSpecWorkbenchVisualEvent
FBlueprintHelperTaskSpecWorkbenchVisualEvent::InputTextChanged(const FString& InText)
{
	FBlueprintHelperTaskSpecWorkbenchVisualEvent Event;
	Event.Type = EBlueprintHelperTaskSpecWorkbenchVisualEventType::InputTextChanged;
	Event.Text = InText;
	return Event;
}

FBlueprintHelperTaskSpecWorkbenchVisualEvent
FBlueprintHelperTaskSpecWorkbenchVisualEvent::ExportReadContext(
	EBlueprintHelperReadContextExportFormat InFormat)
{
	FBlueprintHelperTaskSpecWorkbenchVisualEvent Event;
	Event.Type = EBlueprintHelperTaskSpecWorkbenchVisualEventType::ExportReadContext;
	Event.ExportFormat = InFormat;
	return Event;
}

FBlueprintHelperTaskSpecWorkbenchVisualEvent
FBlueprintHelperTaskSpecWorkbenchVisualEvent::CandidateSelected(
	const FString& InCardId,
	const FString& InCandidateId)
{
	FBlueprintHelperTaskSpecWorkbenchVisualEvent Event;
	Event.Type = EBlueprintHelperTaskSpecWorkbenchVisualEventType::CandidateSelected;
	Event.CardId = InCardId;
	Event.CandidateId = InCandidateId;
	return Event;
}

FBlueprintHelperTaskSpecWorkbenchPresenter::FBlueprintHelperTaskSpecWorkbenchPresenter(
	FContextGraphProvider InGraphProvider)
	: GraphProvider(MoveTemp(InGraphProvider))
{
	HandleInputTextChanged(FString());
}

void FBlueprintHelperTaskSpecWorkbenchPresenter::SetEventSink(FPresenterEventSink InEventSink)
{
	EventSink = MoveTemp(InEventSink);
	EmitSnapshotEvent(Store.GetSnapshot().StatusText);
}

FReply FBlueprintHelperTaskSpecWorkbenchPresenter::HandleVisualEvent(
	const FBlueprintHelperTaskSpecWorkbenchVisualEvent& Event)
{
	switch (Event.Type)
	{
	case EBlueprintHelperTaskSpecWorkbenchVisualEventType::InputTextChanged:
		return HandleInputTextChanged(Event.Text);
	case EBlueprintHelperTaskSpecWorkbenchVisualEventType::ExportReadContext:
		return HandleExportReadContext(Event.ExportFormat);
	case EBlueprintHelperTaskSpecWorkbenchVisualEventType::CandidateSelected:
		return HandleCandidateSelected(Event.CardId, Event.CandidateId);
	default:
		return FReply::Handled();
	}
}

const FBlueprintHelperTaskSpecWorkbenchSnapshot&
FBlueprintHelperTaskSpecWorkbenchPresenter::GetSnapshot() const
{
	return Store.GetSnapshot();
}

FReply FBlueprintHelperTaskSpecWorkbenchPresenter::HandleInputTextChanged(const FString& InText)
{
	FBlueprintHelperTaskSpecWorkbenchSnapshot Snapshot;
	Snapshot.Input = FBlueprintHelperWorkbenchInputClassifier::Classify(InText);
	Snapshot.Preview = FBlueprintHelperTaskSpecPreviewModelBuilder::BuildPreviewModel(Snapshot.Input);
	Snapshot.StatusText = Snapshot.Input.StatusText;

	if (Snapshot.Input.InputType == EBlueprintHelperWorkbenchInputType::TaskSpec)
	{
		FString CandidateStatus;
		UEdGraph* ContextGraph = GraphProvider ? GraphProvider() : nullptr;
		Snapshot.CandidateCards =
			FBlueprintHelperTaskSpecCallFunctionCandidateCoordinator::BuildCandidateCards(
				InText,
				ContextGraph,
				CandidateStatus);
		if (!CandidateStatus.IsEmpty())
		{
			Snapshot.StatusText += TEXT(" ");
			Snapshot.StatusText += CandidateStatus;
		}
	}

	Store.ReplaceSnapshot(Snapshot);
	EmitSnapshotEvent(Snapshot.StatusText);
	return FReply::Handled();
}

FReply FBlueprintHelperTaskSpecWorkbenchPresenter::HandleExportReadContext(
	EBlueprintHelperReadContextExportFormat Format)
{
	const FBlueprintHelperTaskSpecWorkbenchSnapshot& CurrentSnapshot = Store.GetSnapshot();
	FBlueprintHelperReadContextExportRequest Request;
	Request.SourceText = CurrentSnapshot.Input.RawText;
	Request.Format = Format;

	const FBlueprintHelperReadContextExportResult Result =
		FBlueprintHelperReadContextExportService::Export(Request);
	if (Result.bSucceeded)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Result.ExportText);
	}

	FBlueprintHelperTaskSpecWorkbenchSnapshot NextSnapshot = CurrentSnapshot;
	NextSnapshot.StatusText = Result.Message;
	Store.ReplaceSnapshot(NextSnapshot);
	EmitSnapshotEvent(NextSnapshot.StatusText);
	return FReply::Handled();
}

FReply FBlueprintHelperTaskSpecWorkbenchPresenter::HandleCandidateSelected(
	const FString& CardId,
	const FString& CandidateId)
{
	Store.SelectCandidate(CardId, CandidateId);
	EmitSnapshotEvent(Store.GetSnapshot().StatusText);
	return FReply::Handled();
}

void FBlueprintHelperTaskSpecWorkbenchPresenter::EmitSnapshotEvent(const FString& StatusText)
{
	if (!EventSink)
	{
		return;
	}

	FBlueprintHelperTaskSpecWorkbenchPresenterEvent Event;
	Event.Snapshot = Store.GetSnapshot();
	Event.StatusText = StatusText;
	Event.bRefreshView = true;
	EventSink(Event);
}
