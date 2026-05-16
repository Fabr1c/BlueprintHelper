// This Project Is Made By Fabric

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SMultiLineEditableTextBox;
class SBlueprintHelperLayoutRuleCanvas;
class STextBlock;

DECLARE_DELEGATE_RetVal(FString, FBlueprintHelperLayoutRuleEditorImportJson);
DECLARE_DELEGATE_RetVal_OneParam(bool, FBlueprintHelperLayoutRuleEditorExportJson, const FString&);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FBlueprintHelperLayoutRuleEditorValidateJson, const FString&, FString&);
DECLARE_DELEGATE_OneParam(FBlueprintHelperLayoutRuleEditorJsonChanged, const FString&);

/**
 * Slate-only editor for the GraphLayout RuleSet JSON text.
 *
 * The widget intentionally owns only UI/config-facing text operations. Runtime
 * graph layout parsing, normalization, and placement must stay in GraphLayout
 * services and can be connected through the delegates below.
 */
class BLUEPRINTHELPER_API SBlueprintHelperLayoutRuleEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperLayoutRuleEditor)
		: _InitialRuleSetJson(TEXT(""))
		, _DefaultRuleSetJson(TEXT(""))
		{
		}

		SLATE_ARGUMENT(FString, InitialRuleSetJson)
		SLATE_ARGUMENT(FString, DefaultRuleSetJson)
		SLATE_EVENT(FBlueprintHelperLayoutRuleEditorImportJson, OnImportJson)
		SLATE_EVENT(FBlueprintHelperLayoutRuleEditorExportJson, OnExportJson)
		SLATE_EVENT(FBlueprintHelperLayoutRuleEditorValidateJson, OnValidateJson)
		SLATE_EVENT(FBlueprintHelperLayoutRuleEditorJsonChanged, OnRuleSetJsonChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FString GetRuleSetJson() const;
	void SetRuleSetJson(const FString& InRuleSetJson);

private:
	FReply OnImportJsonClicked();
	FReply OnExportJsonClicked();
	FReply OnCopyJsonClicked();
	FReply OnPasteJsonClicked();
	FReply OnValidateClicked();
	FReply OnResetToDefaultClicked();

	void HandleRuleSetTextChanged(const FText& InText);
	void HandleCanvasRuleSetChanged(const FString& InRuleSetJson);
	void SetStatusMessage(const FString& InMessage, bool bInValid);
	bool ValidateRuleSetJson(FString& OutMessage) const;
	void RefreshCanvasFromJson();

	FString LoadJsonFromDefaultFile(FString& OutMessage) const;
	bool SaveJsonToDefaultFile(const FString& JsonText, FString& OutMessage) const;
	FString GetDefaultJsonFilePath() const;

	FString RuleSetJson;
	FString DefaultRuleSetJson;
	bool bLastValidationPassed = false;
	bool bUpdatingTextFromCode = false;

	FBlueprintHelperLayoutRuleEditorImportJson ImportJsonDelegate;
	FBlueprintHelperLayoutRuleEditorExportJson ExportJsonDelegate;
	FBlueprintHelperLayoutRuleEditorValidateJson ValidateJsonDelegate;
	FBlueprintHelperLayoutRuleEditorJsonChanged RuleSetJsonChangedDelegate;

	TSharedPtr<SBlueprintHelperLayoutRuleCanvas> RuleCanvas;
	TSharedPtr<SMultiLineEditableTextBox> RuleSetTextBox;
	TSharedPtr<STextBlock> ValidationStatusTextBlock;
};
