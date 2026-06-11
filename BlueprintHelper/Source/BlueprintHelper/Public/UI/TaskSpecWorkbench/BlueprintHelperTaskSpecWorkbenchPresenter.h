// BlueprintHelper TaskSpec / ReadContext workbench presenter.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h"

class UEdGraph;

enum class EBlueprintHelperTaskSpecWorkbenchVisualEventType : uint8
{
	InputTextChanged,
	ExportReadContext,
	CandidateSelected
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskSpecWorkbenchVisualEvent
{
	EBlueprintHelperTaskSpecWorkbenchVisualEventType Type =
		EBlueprintHelperTaskSpecWorkbenchVisualEventType::InputTextChanged;
	FString Text;
	FString CardId;
	FString CandidateId;
	EBlueprintHelperReadContextExportFormat ExportFormat =
		EBlueprintHelperReadContextExportFormat::LogicFlow;

	static FBlueprintHelperTaskSpecWorkbenchVisualEvent InputTextChanged(const FString& InText);
	static FBlueprintHelperTaskSpecWorkbenchVisualEvent ExportReadContext(
		EBlueprintHelperReadContextExportFormat InFormat);
	static FBlueprintHelperTaskSpecWorkbenchVisualEvent CandidateSelected(
		const FString& InCardId,
		const FString& InCandidateId);
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskSpecWorkbenchPresenterEvent
{
	FBlueprintHelperTaskSpecWorkbenchSnapshot Snapshot;
	FString StatusText;
	bool bRefreshView = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskSpecWorkbenchPresenter
	: public TSharedFromThis<FBlueprintHelperTaskSpecWorkbenchPresenter>
{
public:
	using FPresenterEventSink = TFunction<void(const FBlueprintHelperTaskSpecWorkbenchPresenterEvent&)>;
	using FContextGraphProvider = TFunction<UEdGraph*()>;

	explicit FBlueprintHelperTaskSpecWorkbenchPresenter(FContextGraphProvider InGraphProvider);

	void SetEventSink(FPresenterEventSink InEventSink);
	FReply HandleVisualEvent(const FBlueprintHelperTaskSpecWorkbenchVisualEvent& Event);
	const FBlueprintHelperTaskSpecWorkbenchSnapshot& GetSnapshot() const;

private:
	FReply HandleInputTextChanged(const FString& InText);
	FReply HandleExportReadContext(EBlueprintHelperReadContextExportFormat Format);
	FReply HandleCandidateSelected(const FString& CardId, const FString& CandidateId);
	void EmitSnapshotEvent(const FString& StatusText);

	FBlueprintHelperTaskSpecWorkbenchStore Store;
	FPresenterEventSink EventSink;
	FContextGraphProvider GraphProvider;
};
