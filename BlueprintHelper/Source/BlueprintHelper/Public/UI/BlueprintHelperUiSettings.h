// BlueprintHelper UI runtime settings value objects.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

struct BLUEPRINTHELPER_API FBlueprintHelperMainWindowSettings
{
	FString DefaultTab = TEXT("tools");
	float TabBarPadding = 6.0f;
	FMargin TabButtonSpacing = FMargin(0.0f, 0.0f, 6.0f, 0.0f);
	FLinearColor ActiveTabColor = FLinearColor(0.18f, 0.34f, 0.62f, 1.0f);
	FLinearColor InactiveTabColor = FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
	FText CleanupButtonLabel = FText::FromString(TEXT("Clean Review Data"));
	float CleanupButtonMarginLeft = 10.0f;
};

struct BLUEPRINTHELPER_API FBlueprintHelperNotificationSettings
{
	bool bCleanupUseThrobber = true;
	bool bCleanupUseSuccessFailIcons = false;
	bool bCleanupFireAndForget = false;
	float CleanupFadeOutSeconds = 0.5f;
	float CleanupExpireSeconds = 4.0f;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskSpecWorkbenchSettings
{
	float TopPadding = 8.0f;
	FMargin ButtonSpacing = FMargin(0.0f, 0.0f, 8.0f, 0.0f);
	FVector2D MainSplitRatio = FVector2D(0.30f, 0.70f);
	FVector2D LeftSplitRatio = FVector2D(0.58f, 0.42f);
	float PreviewWidth = 220.0f;
	float PreviewMinHeight = 72.0f;
	float PreviewContainerPadding = 8.0f;
	FLinearColor DefaultBlockColor = FLinearColor(0.10f, 0.22f, 0.48f, 1.0f);
	FLinearColor GraphLogicBlockColor = FLinearColor(0.06f, 0.34f, 0.14f, 1.0f);
	FLinearColor DiagnosticBlockColor = FLinearColor(0.28f, 0.28f, 0.28f, 1.0f);
	FLinearColor SelectedBlockColor = FLinearColor(0.58f, 0.46f, 0.08f, 1.0f);
};

struct BLUEPRINTHELPER_API FBlueprintHelperLayoutRuleEditorSettings
{
	FVector2D CanvasDesiredSize = FVector2D(760.0f, 320.0f);
	FVector2D NodeSize = FVector2D(128.0f, 44.0f);
	float CanvasRuleScale = 0.45f;
	FString DefaultRuleId = TEXT("default_readable_exec_with_left_data");
	FString DefaultRuleDisplayName = TEXT("Default Readable Exec With Left Data");
	float ExecColumnSpacing = 360.0f;
	float ExecRowSpacing = 220.0f;
	float BranchRowSpacing = 260.0f;
	float PureInputOffsetX = 300.0f;
	float VariableInputOffsetX = 260.0f;
	float InputPinRowSpacing = 44.0f;
	float MaxMillisecondsPerFrame = 2.0f;
	int32 MaxNodesPerFrame = 24;
	bool bMoveGeneratedNodes = true;
	bool bMoveExistingNodes = false;
	bool bMarkDirtyAfterApply = true;
	bool bSaveAfterApply = false;
	FVector2D SideSplitterRatio = FVector2D(0.30f, 0.70f);
};
