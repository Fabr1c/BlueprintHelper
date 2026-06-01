// This Project Is Made By Fabric

#pragma once

#include "CoreMinimal.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "Widgets/SCompoundWidget.h"

class SMultiLineEditableTextBox;
class SBlueprintHelperLayoutRuleCanvas;
class STextBlock;
class SWidget;

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
	FReply OnAlignExecRowClicked();

	void HandleRuleSetTextChanged(const FText& InText);
	void HandleCanvasRuleSetChanged(const FString& InRuleSetJson);
	TSharedRef<SWidget> BuildSettingsPanel();
	void RefreshSettingsFromJson();
	void HandleTextSettingCommitted(int32 SettingId, const FText& NewValue);
	void HandleFloatSettingChanged(int32 SettingId, float NewValue);
	void HandleIntSettingChanged(int32 SettingId, int32 NewValue);
	void HandleBoolSettingChanged(int32 SettingId, bool bNewValue);
	void CommitSettingsRuleSetJson(const FString& InUpdatedRuleSetJson);
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
	bool bUpdatingSettingsFromJson = false;

	FString SettingsRuleId = TEXT("default_readable_exec_with_left_data");
	FString SettingsDisplayName = TEXT("Default Readable Exec With Left Data");
	float SettingsExecColumnSpacing = 360.0f;
	float SettingsExecRowSpacing = 220.0f;
	float SettingsBranchRowSpacing = 260.0f;
	float SettingsPureInputOffsetX = 300.0f;
	float SettingsVariableInputOffsetX = 260.0f;
	float SettingsInputPinRowSpacing = 44.0f;
	bool bSettingsAlignExecNodesHorizontally = true;
	bool bSettingsUsePureDataSubgraphLayout = true;
	bool bSettingsUsePatternRowHeightBudget = true;
	float SettingsDataClusterPaddingX = 40.0f;
	float SettingsDataClusterPaddingY = 40.0f;
	float SettingsBranchRowPaddingY = 80.0f;
	float SettingsCollisionPaddingX = 60.0f;
	float SettingsCollisionPaddingY = 40.0f;
	float SettingsCollisionStepY = 64.0f;
	float SettingsMaxMillisecondsPerFrame = 2.0f;
	int32 SettingsMaxNodesPerFrame = 24;
	int32 SettingsMaxCollisionAttempts = 64;
	bool bSettingsMoveGeneratedNodes = true;
	bool bSettingsMoveExistingNodes = false;
	bool bSettingsMarkDirtyAfterApply = true;
	bool bSettingsSaveAfterApply = false;

	FBlueprintHelperLayoutRuleEditorSettings LayoutRuleEditorSettings;
	FBlueprintHelperLayoutRuleEditorImportJson ImportJsonDelegate;
	FBlueprintHelperLayoutRuleEditorExportJson ExportJsonDelegate;
	FBlueprintHelperLayoutRuleEditorValidateJson ValidateJsonDelegate;
	FBlueprintHelperLayoutRuleEditorJsonChanged RuleSetJsonChangedDelegate;

	TSharedPtr<SBlueprintHelperLayoutRuleCanvas> RuleCanvas;
	TSharedPtr<SMultiLineEditableTextBox> RuleSetTextBox;
	TSharedPtr<STextBlock> ValidationStatusTextBlock;
};
