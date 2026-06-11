// BlueprintHelper TaskSpec / ReadContext workbench Slate widget.

#pragma once

#include "CoreMinimal.h"
#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperTaskSpecWorkbenchPresenter;
class SBorder;
class SMultiLineEditableTextBox;
class STableViewBase;
class STextBlock;
class UEdGraph;

class BLUEPRINTHELPER_API SBlueprintHelperTaskSpecWorkbench : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperTaskSpecWorkbench)
		: _GraphResolver(nullptr)
	{
	}

	SLATE_ARGUMENT(const FBlueprintHelperGraphResolver*, GraphResolver)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnExportLogicFlowClicked();
	FReply OnExportLogicJsonClicked();
	void OnInputTextChanged(const FText& InText);
	void HandlePresenterEvent(const struct FBlueprintHelperTaskSpecWorkbenchPresenterEvent& Event);
	void RefreshFromSnapshot(const FBlueprintHelperTaskSpecWorkbenchSnapshot& Snapshot);
	TSharedRef<ITableRow> GenerateCandidateCardRow(
		TSharedPtr<FBlueprintHelperCallFunctionCardModel> Item,
		const TSharedRef<STableViewBase>& OwnerTable);
	bool IsCandidateSelected(const FString& CardId, const FString& CandidateId) const;
	void OnCandidateCheckStateChanged(
		ECheckBoxState NewState,
		FString CardId,
		FString CandidateId);
	TSharedRef<SWidget> BuildPreviewContent() const;
	TSharedRef<SWidget> BuildPreviewBlockWidget(
		const FBlueprintHelperTaskSpecPreviewBlock& Block) const;
	UEdGraph* GetCurrentTargetGraph() const;

	const FBlueprintHelperGraphResolver* GraphResolver = nullptr;
	TSharedPtr<FBlueprintHelperTaskSpecWorkbenchPresenter> Presenter;
	FBlueprintHelperTaskSpecWorkbenchSnapshot CurrentSnapshot;
	TArray<TSharedPtr<FBlueprintHelperCallFunctionCardModel>> CandidateCardItems;
	TSharedPtr<SMultiLineEditableTextBox> MainTextBox;
	TSharedPtr<SListView<TSharedPtr<FBlueprintHelperCallFunctionCardModel>>> CandidateListView;
	TSharedPtr<SBorder> PreviewContainer;
	TSharedPtr<STextBlock> StatusTextBlock;
};
