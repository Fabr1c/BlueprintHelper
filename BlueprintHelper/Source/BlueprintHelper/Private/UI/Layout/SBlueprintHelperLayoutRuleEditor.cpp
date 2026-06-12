// This Project Is Made By Fabric

#include "UI/Layout/SBlueprintHelperLayoutRuleEditor.h"

#include "EdGraph/EdGraph.h"
#include "GraphEditor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"
#include "UI/BlueprintHelperUiSettingsResolver.h"
#include "UI/Layout/SBlueprintHelperLayoutPreviewInteractionSurface.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#if __has_include("Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h")
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#define BLUEPRINTHELPER_LAYOUT_RULE_EDITOR_HAS_GRAPH_LAYOUT_RULESET_JSON 1
#else
#define BLUEPRINTHELPER_LAYOUT_RULE_EDITOR_HAS_GRAPH_LAYOUT_RULESET_JSON 0
#endif

#define LOCTEXT_NAMESPACE "SBlueprintHelperLayoutRuleEditor"

namespace BlueprintHelperLayoutRuleEditorLocal
{
	const FLinearColor ValidStatusColor(0.1f, 0.65f, 0.25f, 1.0f);
	const FLinearColor InvalidStatusColor(0.9f, 0.2f, 0.12f, 1.0f);

	enum ETextSetting : int32
	{
		RuleId = 0,
		DisplayName
	};

	enum EFloatSetting : int32
	{
		ExecColumnSpacing = 0,
		ExecRowSpacing,
		BranchRowSpacing,
		PureInputOffsetX,
		VariableInputOffsetX,
		InputPinRowSpacing,
		DataClusterPaddingX,
		DataClusterPaddingY,
		BranchRowPaddingY,
		CollisionPaddingX,
		CollisionPaddingY,
		CollisionStepY,
		MaxMillisecondsPerFrame
	};

	enum EUiFloatSetting : int32
	{
		CanvasWidth = 0,
		CanvasHeight
	};

	enum EIntSetting : int32
	{
		MaxNodesPerFrame = 0,
		MaxCollisionAttempts
	};

	enum EBoolSetting : int32
	{
		AlignExecNodesHorizontally = 0,
		UsePureDataSubgraphLayout,
		UsePatternRowHeightBudget,
		MoveGeneratedNodes,
		MoveExistingNodes,
		MarkDirtyAfterApply,
		SaveAfterApply
	};

	constexpr float MinMillisecondsPerFrame = 0.25f;
	constexpr float MaxMillisecondsPerFrameSetting = 20.0f;
	constexpr float MinCanvasWidth = 760.0f;
	constexpr float MaxCanvasWidth = 2000.0f;
	constexpr float MinCanvasHeight = 440.0f;
	constexpr float MaxCanvasHeight = 1600.0f;

	float ClampMillisecondsPerFrame(float Value)
	{
		return FMath::Clamp(Value, MinMillisecondsPerFrame, MaxMillisecondsPerFrameSetting);
	}

	float ClampCanvasWidth(float Value)
	{
		return FMath::Clamp(Value, MinCanvasWidth, MaxCanvasWidth);
	}

	float ClampCanvasHeight(float Value)
	{
		return FMath::Clamp(Value, MinCanvasHeight, MaxCanvasHeight);
	}

	FString GetFallbackDefaultJson()
	{
#if BLUEPRINTHELPER_LAYOUT_RULE_EDITOR_HAS_GRAPH_LAYOUT_RULESET_JSON
		return BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(BlueprintHelper::GraphLayout::FRuleSet());
#else
		return TEXT("{\n")
			TEXT("  \"schema\": \"BlueprintHelper.GraphLayoutRuleSet.v1\",\n")
			TEXT("  \"role_rules\": []\n")
			TEXT("}");
#endif
	}

	TSharedRef<SWidget> BuildToolbarButton(const FText& Label, const FText& ToolTip, const FOnClicked& OnClicked)
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(ToolTip)
			.OnClicked(OnClicked);
	}

	TSharedRef<SWidget> BuildSettingsSectionHeader(const FText& Label)
	{
		return SNew(STextBlock)
			.Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))
			.ColorAndOpacity(FLinearColor(0.86f, 0.86f, 0.86f, 1.0f));
	}

	TSharedRef<SWidget> BuildTextSettingRow(
		const FText& Label,
		const FText& ToolTip,
		TFunction<FString()> GetValue,
		TFunction<void(const FText&)> SetValue)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.42f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(ToolTip)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.58f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([GetValue]()
				{
					return FText::FromString(GetValue());
				})
				.ToolTipText(ToolTip)
				.OnTextCommitted_Lambda([SetValue](const FText& NewText, ETextCommit::Type)
				{
					SetValue(NewText);
				})
			];
	}

	TSharedRef<SWidget> BuildFloatSettingRow(
		const FText& Label,
		const FText& ToolTip,
		TFunction<float()> GetValue,
		TFunction<void(float)> SetValue,
		float MinValue,
		float MaxValue,
		float Delta)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(ToolTip)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpinBox<float>)
				.ToolTipText(ToolTip)
				.MinValue(MinValue)
				.MaxValue(MaxValue)
				.Delta(Delta)
				.Value_Lambda([GetValue]()
				{
					return GetValue();
				})
				.OnValueChanged_Lambda([SetValue](float NewValue)
				{
					SetValue(NewValue);
				})
			];
	}

	TSharedRef<SWidget> BuildIntSettingRow(
		const FText& Label,
		const FText& ToolTip,
		TFunction<int32()> GetValue,
		TFunction<void(int32)> SetValue,
		int32 MinValue,
		int32 MaxValue)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(ToolTip)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpinBox<int32>)
				.ToolTipText(ToolTip)
				.MinValue(MinValue)
				.MaxValue(MaxValue)
				.Delta(1)
				.Value_Lambda([GetValue]()
				{
					return GetValue();
				})
				.OnValueChanged_Lambda([SetValue](int32 NewValue)
				{
					SetValue(NewValue);
				})
			];
	}

	TSharedRef<SWidget> BuildBoolSettingRow(
		const FText& Label,
		const FText& ToolTip,
		TFunction<bool()> GetValue,
		TFunction<void(bool)> SetValue)
	{
		return SNew(SCheckBox)
			.ToolTipText(ToolTip)
			.IsChecked_Lambda([GetValue]()
			{
				return GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([SetValue](ECheckBoxState NewState)
			{
				SetValue(NewState == ECheckBoxState::Checked);
			})
			[
				SNew(STextBlock)
				.Text(Label)
			];
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SBlueprintHelperLayoutRuleEditor::Construct(const FArguments& InArgs)
{
	LayoutRuleEditorSettings = FBlueprintHelperUiSettingsResolver::LoadLayoutRuleEditorSettings();
	LayoutRuleEditorSettings.MaxMillisecondsPerFrame =
		BlueprintHelperLayoutRuleEditorLocal::ClampMillisecondsPerFrame(LayoutRuleEditorSettings.MaxMillisecondsPerFrame);
	LayoutRuleEditorSettings.CanvasDesiredSize.X =
		BlueprintHelperLayoutRuleEditorLocal::ClampCanvasWidth(LayoutRuleEditorSettings.CanvasDesiredSize.X);
	LayoutRuleEditorSettings.CanvasDesiredSize.Y =
		BlueprintHelperLayoutRuleEditorLocal::ClampCanvasHeight(LayoutRuleEditorSettings.CanvasDesiredSize.Y);
	const BlueprintHelper::GraphLayout::FRuleSet DefaultRuleSet;

	SettingsRuleId = LayoutRuleEditorSettings.DefaultRuleId;
	SettingsDisplayName = LayoutRuleEditorSettings.DefaultRuleDisplayName;
	SettingsCanvasWidth = LayoutRuleEditorSettings.CanvasDesiredSize.X;
	SettingsCanvasHeight = LayoutRuleEditorSettings.CanvasDesiredSize.Y;
	SettingsExecColumnSpacing = LayoutRuleEditorSettings.ExecColumnSpacing;
	SettingsExecRowSpacing = LayoutRuleEditorSettings.ExecRowSpacing;
	SettingsBranchRowSpacing = LayoutRuleEditorSettings.BranchRowSpacing;
	SettingsPureInputOffsetX = LayoutRuleEditorSettings.PureInputOffsetX;
	SettingsVariableInputOffsetX = LayoutRuleEditorSettings.VariableInputOffsetX;
	SettingsInputPinRowSpacing = LayoutRuleEditorSettings.InputPinRowSpacing;
	bSettingsAlignExecNodesHorizontally = DefaultRuleSet.bAlignExecNodesHorizontally;
	bSettingsUsePureDataSubgraphLayout = DefaultRuleSet.bUsePureDataSubgraphLayout;
	bSettingsUsePatternRowHeightBudget = DefaultRuleSet.bUsePatternRowHeightBudget;
	SettingsDataClusterPaddingX = DefaultRuleSet.DataClusterPaddingX;
	SettingsDataClusterPaddingY = DefaultRuleSet.DataClusterPaddingY;
	SettingsBranchRowPaddingY = DefaultRuleSet.BranchRowPaddingY;
	SettingsMaxMillisecondsPerFrame = LayoutRuleEditorSettings.MaxMillisecondsPerFrame;
	SettingsMaxNodesPerFrame = LayoutRuleEditorSettings.MaxNodesPerFrame;
	bSettingsMoveGeneratedNodes = LayoutRuleEditorSettings.bMoveGeneratedNodes;
	bSettingsMoveExistingNodes = LayoutRuleEditorSettings.bMoveExistingNodes;
	bSettingsMarkDirtyAfterApply = LayoutRuleEditorSettings.bMarkDirtyAfterApply;
	bSettingsSaveAfterApply = LayoutRuleEditorSettings.bSaveAfterApply;
	RuleSetJson = InArgs._InitialRuleSetJson;
	DefaultRuleSetJson = InArgs._DefaultRuleSetJson;
	ImportJsonDelegate = InArgs._OnImportJson;
	ExportJsonDelegate = InArgs._OnExportJson;
	ValidateJsonDelegate = InArgs._OnValidateJson;
	RuleSetJsonChangedDelegate = InArgs._OnRuleSetJsonChanged;

	if (RuleSetJson.IsEmpty())
	{
		RuleSetJson = DefaultRuleSetJson.IsEmpty()
			? BlueprintHelperLayoutRuleEditorLocal::GetFallbackDefaultJson()
			: DefaultRuleSetJson;
	}

	if (DefaultRuleSetJson.IsEmpty())
	{
		DefaultRuleSetJson = BlueprintHelperLayoutRuleEditorLocal::GetFallbackDefaultJson();
	}

	FString InitialStatus;
	bLastValidationPassed = ValidateRuleSetJson(InitialStatus);
	PreviewService = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewService>();
	PreviewMaterializer = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewMaterializer>();
	PreviewState = EPreviewState::PreviewLoading;
	PreviewStatusMessage = TEXT("正在构建预览数据...");

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.58f)
		[
			SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(0.64f)
		.Padding(0.0f)
		[
			SAssignNew(WorkspaceBox, SBox)
			[
				BuildPreviewWorkspace()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("ImportJson", "导入 JSON"),
					LOCTEXT("ImportJsonTooltip", "通过已绑定的配置入口导入 RuleSet JSON；未绑定时从默认配置文件读取。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnImportJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("ExportJson", "导出 JSON"),
					LOCTEXT("ExportJsonTooltip", "通过已绑定的配置入口导出当前 RuleSet JSON；未绑定时写入默认配置文件。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnExportJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("CopyJson", "复制 JSON"),
					LOCTEXT("CopyJsonTooltip", "将当前 RuleSet JSON 文本复制到剪贴板。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnCopyJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("PasteJson", "粘贴 JSON"),
					LOCTEXT("PasteJsonTooltip", "用剪贴板内容替换当前 RuleSet JSON 文本。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnPasteJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("Validate", "校验"),
					LOCTEXT("ValidateTooltip", "校验当前 RuleSet JSON；已绑定的 GraphLayout 校验器会提供 schema 级检查。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnValidateClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("ResetDefault", "恢复默认"),
					LOCTEXT("ResetDefaultTooltip", "用已配置的默认 RuleSet JSON 替换当前文本。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnResetToDefaultClicked))
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.36f)
		.Padding(8.0f, 0.0f)
		[
			SNew(SBorder)
			.Padding(4.0f)
			[
				SAssignNew(RuleSetTextBox, SMultiLineEditableTextBox)
				.Text(FText::FromString(RuleSetJson))
				.HintText(LOCTEXT("RuleSetJsonHint", "在此编辑 GraphLayout RuleSet JSON。"))
				.OnTextChanged(this, &SBlueprintHelperLayoutRuleEditor::HandleRuleSetTextChanged)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SAssignNew(ValidationStatusTextBlock, STextBlock)
			.Text(FText::FromString(InitialStatus))
			.ColorAndOpacity(bLastValidationPassed
				? BlueprintHelperLayoutRuleEditorLocal::ValidStatusColor
				: BlueprintHelperLayoutRuleEditorLocal::InvalidStatusColor)
		]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.42f)
		.Padding(0.0f, 8.0f, 8.0f, 8.0f)
		[
			SNew(SBorder)
			.Padding(10.0f)
			[
				BuildSettingsPanel()
			]
		]
	];

	RefreshSettingsFromJson();
	StartPreviewBuild();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SBlueprintHelperLayoutRuleEditor::~SBlueprintHelperLayoutRuleEditor()
{
	CancelActivePreview();
	if (PreviewService.IsValid())
	{
		PreviewService->CancelAll();
	}
}

void SBlueprintHelperLayoutRuleEditor::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	PumpPreviewMaterializer();
}

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildSceneToolbar()
{
	TSharedRef<SWrapBox> SceneToolbar = SNew(SWrapBox)
		.UseAllottedSize(true);
	for (const BlueprintHelper::GraphLayout::FSemanticSceneDefinition& SceneDefinition :
		BlueprintHelper::GraphLayout::FSemanticSceneCatalog::GetAllScenes())
	{
		SceneToolbar->AddSlot()
			.Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					SceneDefinition.DisplayName,
					FText::Format(
						LOCTEXT("CanvasSceneTooltip", "切换到 {0} 语义场景并刷新原生蓝图预览。"),
						SceneDefinition.DisplayName),
					FOnClicked::CreateLambda([this, Scene = SceneDefinition.Scene]()
					{
						CurrentScene = Scene;
						SetPreviewState(EPreviewState::PreviewLoading, TEXT("正在刷新预览数据..."));
						StartPreviewBuild();
						return FReply::Handled();
					}))
			];
	}
	SceneToolbar->AddSlot()
		.Padding(0.0f, 0.0f, 6.0f, 6.0f)
		[
			BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
				LOCTEXT("AlignExecRow", "对齐执行行"),
				LOCTEXT("AlignExecRowTooltip", "将线性执行节点对齐到执行入口基线。"),
				FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnAlignExecRowClicked))
		];
	SceneToolbar->AddSlot()
		.Padding(0.0f, 0.0f, 6.0f, 6.0f)
		[
			BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
				LOCTEXT("PreviewLayoutRule", "刷新预览"),
				LOCTEXT("PreviewLayoutRuleTooltip", "为当前语义场景重新构建可拖拽的原生蓝图图表预览。"),
				FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnPreviewClicked))
		];
	SceneToolbar->AddSlot()
		.Padding(0.0f, 0.0f, 6.0f, 6.0f)
		[
			BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
				LOCTEXT("ApplyPreviewChanges", "应用预览修改"),
				LOCTEXT("ApplyPreviewChangesTooltip", "将当前预览中已拖动的节点和已调整的避让范围写入 RuleSet JSON、同步右侧设置并保存，然后刷新预览。"),
				FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnApplyPreviewChangesClicked))
		];
	SceneToolbar->AddSlot()
		.Padding(0.0f, 0.0f, 6.0f, 6.0f)
		[
			BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
				LOCTEXT("DiscardPreviewChanges", "放弃预览修改"),
				LOCTEXT("DiscardPreviewChangesTooltip", "丢弃当前未应用的预览拖拽和避让范围大小修改，并按已保存的 RuleSet 重新构建预览。"),
				FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnDiscardPreviewChangesClicked))
		];
	return SceneToolbar;
}

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildPreviewWorkspace()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 4.0f)
		[
			BuildSceneToolbar()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(PreviewStatusMessage))
			.AutoWrapText(true)
			.ColorAndOpacity(PreviewState == EPreviewState::PreviewError
				? BlueprintHelperLayoutRuleEditorLocal::InvalidStatusColor
				: FLinearColor(0.78f, 0.78f, 0.78f, 1.0f))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			BuildPreviewContent()
		];
}

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildPreviewContent()
{
	if (PreviewState == EPreviewState::PreviewReady && PreviewInteractionSurface.IsValid())
	{
		return SNew(SBorder)
			.Padding(1.0f)
			[
				PreviewInteractionSurface.ToSharedRef()
			];
	}

	return BuildPreviewStatusWidget();
}

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildPreviewStatusWidget() const
{
	return SNew(SBorder)
		.Padding(14.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(PreviewStatusMessage))
			.AutoWrapText(true)
			.ColorAndOpacity(PreviewState == EPreviewState::PreviewError
				? BlueprintHelperLayoutRuleEditorLocal::InvalidStatusColor
				: FLinearColor(0.78f, 0.78f, 0.78f, 1.0f))
		];
}

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildSettingsPanel()
{
	using namespace BlueprintHelperLayoutRuleEditorLocal;
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsPanelTitle", "布局规则设置"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsUiHeader", "界面设置"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("CanvasWidthLabel", "画布宽度"),
				LOCTEXT("CanvasWidthTooltip", "可拖拽语义画布的显示宽度，仅影响 LayoutPanel 编辑器界面。"),
				[this]() { return SettingsCanvasWidth; },
				[this](float NewValue) { HandleUiFloatSettingChanged(CanvasWidth, NewValue); },
				760.0f,
				2000.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("CanvasHeightLabel", "画布高度"),
				LOCTEXT("CanvasHeightTooltip", "可拖拽语义画布的显示高度；过低会让多执行出口等场景绘制出界。"),
				[this]() { return SettingsCanvasHeight; },
				[this](float NewValue) { HandleUiFloatSettingChanged(CanvasHeight, NewValue); },
				440.0f,
				1600.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsRuleHeader", "规则基础"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildTextSettingRow(
				LOCTEXT("RuleIdLabel", "ID"),
				LOCTEXT("RuleIdTooltip", "当前 GraphLayout RuleSet 的稳定标识。"),
				[this]() { return SettingsRuleId; },
				[this](const FText& NewValue) { HandleTextSettingCommitted(RuleId, NewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildTextSettingRow(
				LOCTEXT("DisplayNameLabel", "显示名称"),
				LOCTEXT("DisplayNameTooltip", "当前 GraphLayout RuleSet 展示给用户的名称。"),
				[this]() { return SettingsDisplayName; },
				[this](const FText& NewValue) { HandleTextSettingCommitted(DisplayName, NewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsLinearExecHeader", "线性执行"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("AlignExecNodesHorizontallyLabel", "执行节点水平对齐"),
				LOCTEXT("AlignExecNodesHorizontallyTooltip", "为带分支能力的布局模式启用 RuleSet 驱动的执行节点水平对齐。"),
				[this]() { return bSettingsAlignExecNodesHorizontally; },
				[this](bool bNewValue) { HandleBoolSettingChanged(AlignExecNodesHorizontally, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("ExecColumnSpacingLabel", "执行列距"),
				LOCTEXT("ExecColumnSpacingTooltip", "相邻执行列之间的水平距离。"),
				[this]() { return SettingsExecColumnSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(ExecColumnSpacing, NewValue); },
				120.0f,
				1200.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("ExecRowSpacingLabel", "执行行距"),
				LOCTEXT("ExecRowSpacingTooltip", "线性执行行之间的垂直距离。"),
				[this]() { return SettingsExecRowSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(ExecRowSpacing, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsPureDataHeader", "纯数据"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("UsePureDataSubgraphLayoutLabel", "纯数据子图布局"),
				LOCTEXT("UsePureDataSubgraphLayoutTooltip", "启用 RuleSet 驱动的纯数据子图测量与摆放。"),
				[this]() { return bSettingsUsePureDataSubgraphLayout; },
				[this](bool bNewValue) { HandleBoolSettingChanged(UsePureDataSubgraphLayout, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("DataClusterPaddingXLabel", "数据簇水平边距"),
				LOCTEXT("DataClusterPaddingXTooltip", "围绕纯数据包围盒预留的额外水平边距。"),
				[this]() { return SettingsDataClusterPaddingX; },
				[this](float NewValue) { HandleFloatSettingChanged(DataClusterPaddingX, NewValue); },
				1.0f,
				400.0f,
				5.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("DataClusterPaddingYLabel", "数据簇垂直边距"),
				LOCTEXT("DataClusterPaddingYTooltip", "围绕纯数据包围盒预留的额外垂直边距。"),
				[this]() { return SettingsDataClusterPaddingY; },
				[this](float NewValue) { HandleFloatSettingChanged(DataClusterPaddingY, NewValue); },
				1.0f,
				400.0f,
				5.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("InputPinRowSpacingLabel", "输入 Pin 行距"),
				LOCTEXT("InputPinRowSpacingTooltip", "模式数据摆放中消费者输入 Pin 行之间的垂直距离。"),
				[this]() { return SettingsInputPinRowSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(InputPinRowSpacing, NewValue); },
				24.0f,
				220.0f,
				2.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("PureInputOffsetLabel", "纯输入水平偏移"),
				LOCTEXT("PureInputOffsetTooltip", "纯函数和运算节点输入使用的水平偏移。"),
				[this]() { return SettingsPureInputOffsetX; },
				[this](float NewValue) { HandleFloatSettingChanged(PureInputOffsetX, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("VariableInputOffsetLabel", "变量输入水平偏移"),
				LOCTEXT("VariableInputOffsetTooltip", "变量输入节点使用的水平偏移。"),
				[this]() { return SettingsVariableInputOffsetX; },
				[this](float NewValue) { HandleFloatSettingChanged(VariableInputOffsetX, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsBranchHeader", "分支"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("BranchRowSpacingLabel", "分支行距"),
				LOCTEXT("BranchRowSpacingTooltip", "分支输出行之间使用的垂直距离。"),
				[this]() { return SettingsBranchRowSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(BranchRowSpacing, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("BranchRowPaddingYLabel", "分支行垂直边距"),
				LOCTEXT("BranchRowPaddingYTooltip", "行高预算后，在已分配分支行之间插入的额外垂直边距。"),
				[this]() { return SettingsBranchRowPaddingY; },
				[this](float NewValue) { HandleFloatSettingChanged(BranchRowPaddingY, NewValue); },
				1.0f,
				400.0f,
				5.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("UsePatternRowHeightBudgetLabel", "模式行高预算"),
				LOCTEXT("UsePatternRowHeightBudgetTooltip", "允许测量出的数据簇高度预算扩展执行行基线。"),
				[this]() { return bSettingsUsePatternRowHeightBudget; },
				[this](bool bNewValue) { HandleBoolSettingChanged(UsePatternRowHeightBudget, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsOccupancyHeader", "占位避让"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("CollisionPaddingXLabel", "碰撞水平边距"),
				LOCTEXT("CollisionPaddingXTooltip", "处理重叠时预留的额外水平边距。"),
				[this]() { return SettingsCollisionPaddingX; },
				[this](float NewValue) { HandleFloatSettingChanged(CollisionPaddingX, NewValue); },
				1.0f,
				400.0f,
				5.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("CollisionPaddingYLabel", "碰撞垂直边距"),
				LOCTEXT("CollisionPaddingYTooltip", "处理重叠时预留的额外垂直边距。"),
				[this]() { return SettingsCollisionPaddingY; },
				[this](float NewValue) { HandleFloatSettingChanged(CollisionPaddingY, NewValue); },
				1.0f,
				400.0f,
				5.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("CollisionStepYLabel", "碰撞搜索步长"),
				LOCTEXT("CollisionStepYTooltip", "处理重叠时使用的垂直搜索增量。"),
				[this]() { return SettingsCollisionStepY; },
				[this](float NewValue) { HandleFloatSettingChanged(CollisionStepY, NewValue); },
				8.0f,
				400.0f,
				4.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildIntSettingRow(
				LOCTEXT("MaxCollisionAttemptsLabel", "最大碰撞尝试"),
				LOCTEXT("MaxCollisionAttemptsTooltip", "求解器放弃候选位置前允许的最大重叠处理次数。"),
				[this]() { return SettingsMaxCollisionAttempts; },
				[this](int32 NewValue) { HandleIntSettingChanged(MaxCollisionAttempts, NewValue); },
				1,
				256)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsApplyHeader", "应用"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildIntSettingRow(
				LOCTEXT("MaxNodesPerFrameLabel", "每帧节点数"),
				LOCTEXT("MaxNodesPerFrameTooltip", "一个编辑器帧内最多应用的节点移动数量。"),
				[this]() { return SettingsMaxNodesPerFrame; },
				[this](int32 NewValue) { HandleIntSettingChanged(MaxNodesPerFrame, NewValue); },
				1,
				256)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("MaxMillisecondsPerFrameLabel", "每帧毫秒数"),
				LOCTEXT("MaxMillisecondsPerFrameTooltip", "应用布局移动时每帧允许的最大时间预算。"),
				[this]() { return SettingsMaxMillisecondsPerFrame; },
				[this](float NewValue) { HandleFloatSettingChanged(MaxMillisecondsPerFrame, NewValue); },
				0.25f,
				20.0f,
				0.25f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("MoveGeneratedNodesLabel", "移动生成节点"),
				LOCTEXT("MoveGeneratedNodesTooltip", "允许布局应用移动当前任务生成的节点。"),
				[this]() { return bSettingsMoveGeneratedNodes; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MoveGeneratedNodes, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("MoveExistingNodesLabel", "移动已有节点"),
				LOCTEXT("MoveExistingNodesTooltip", "允许布局应用移动用户已有节点。"),
				[this]() { return bSettingsMoveExistingNodes; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MoveExistingNodes, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsPersistenceHeader", "持久化"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("MarkDirtyAfterApplyLabel", "应用后标脏"),
				LOCTEXT("MarkDirtyAfterApplyTooltip", "布局移动应用后将所属 package 标记为 dirty。"),
				[this]() { return bSettingsMarkDirtyAfterApply; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MarkDirtyAfterApply, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("SaveAfterApplyLabel", "应用后保存"),
				LOCTEXT("SaveAfterApplyTooltip", "布局移动应用后保存所属 package。"),
				[this]() { return bSettingsSaveAfterApply; },
				[this](bool bNewValue) { HandleBoolSettingChanged(SaveAfterApply, bNewValue); })
		];
}

FString SBlueprintHelperLayoutRuleEditor::GetRuleSetJson() const
{
	return RuleSetJson;
}

void SBlueprintHelperLayoutRuleEditor::SetRuleSetJson(const FString& InRuleSetJson)
{
	ClearPendingPreviewInteractionCommit();
	RuleSetJson = InRuleSetJson;

	if (RuleSetTextBox.IsValid())
	{
		TGuardValue<bool> UpdatingTextGuard(bUpdatingTextFromCode, true);
		RuleSetTextBox->SetText(FText::FromString(RuleSetJson));
	}

	FString Message;
	bLastValidationPassed = ValidateRuleSetJson(Message);
	SetStatusMessage(Message, bLastValidationPassed);
	RefreshSettingsFromJson();
	if (bLastValidationPassed)
	{
		SetPreviewState(EPreviewState::PreviewLoading, TEXT("正在刷新预览数据..."));
		StartPreviewBuild();
	}

	if (RuleSetJsonChangedDelegate.IsBound())
	{
		RuleSetJsonChangedDelegate.Execute(RuleSetJson);
	}
}

FReply SBlueprintHelperLayoutRuleEditor::OnImportJsonClicked()
{
	FString ImportedJson;
	if (ImportJsonDelegate.IsBound())
	{
		ImportedJson = ImportJsonDelegate.Execute();
	}
	else
	{
		FString Message;
		ImportedJson = LoadJsonFromDefaultFile(Message);
		if (ImportedJson.IsEmpty())
		{
			SetStatusMessage(Message, false);
			return FReply::Handled();
		}
	}

	SetRuleSetJson(ImportedJson);
	SetStatusMessage(TEXT("已导入 RuleSet JSON。"), true);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnExportJsonClicked()
{
	FString ValidationMessage;
	if (!ValidateRuleSetJson(ValidationMessage))
	{
		SetStatusMessage(ValidationMessage, false);
		return FReply::Handled();
	}

	bool bExported = false;
	FString ExportMessage;
	if (ExportJsonDelegate.IsBound())
	{
		bExported = ExportJsonDelegate.Execute(RuleSetJson);
		ExportMessage = bExported ? TEXT("已导出 RuleSet JSON。") : TEXT("导出 RuleSet JSON 失败。");
	}
	else
	{
		bExported = SaveJsonToDefaultFile(RuleSetJson, ExportMessage);
	}

	SetStatusMessage(ExportMessage, bExported);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnCopyJsonClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*RuleSetJson);
	SetStatusMessage(TEXT("已复制 RuleSet JSON 到剪贴板。"), true);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnPasteJsonClicked()
{
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	if (ClipboardText.TrimStartAndEnd().IsEmpty())
	{
		SetStatusMessage(TEXT("剪贴板中没有 RuleSet JSON。"), false);
		return FReply::Handled();
	}

	SetRuleSetJson(ClipboardText);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnValidateClicked()
{
	FString Message;
	bLastValidationPassed = ValidateRuleSetJson(Message);
	SetStatusMessage(Message, bLastValidationPassed);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnResetToDefaultClicked()
{
	SetRuleSetJson(DefaultRuleSetJson);
	SetStatusMessage(TEXT("已将 RuleSet JSON 恢复为默认值。"), true);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnAlignExecRowClicked()
{
	BlueprintHelper::GraphLayout::FRuleSet ParsedRuleSet;
	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(RuleSetJson, ParsedRuleSet, Validation))
	{
		SetStatusMessage(TEXT("RuleSet JSON 无效，无法对齐执行行。"), false);
		return FReply::Handled();
	}

	CurrentScene = BlueprintHelper::GraphLayout::ESemanticScene::LinearExecChain;
	BlueprintHelper::GraphLayout::FEditorCanvasSceneState SceneState =
		BlueprintHelper::GraphLayout::FSemanticSceneAdapter::ResolveSceneState(ParsedRuleSet, CurrentScene);
	if (!SceneState.RoleCenters.Contains(BlueprintHelper::GraphLayout::ENodeRole::EventEntry) ||
		!SceneState.RoleCenters.Contains(BlueprintHelper::GraphLayout::ENodeRole::ExecNode))
	{
		SetStatusMessage(TEXT("当前规则缺少执行入口或执行节点场景锚点。"), false);
		return FReply::Handled();
	}

	FVector2D ExecCenter = SceneState.RoleCenters.FindRef(BlueprintHelper::GraphLayout::ENodeRole::ExecNode);
	ExecCenter.Y = SceneState.RoleCenters.FindRef(BlueprintHelper::GraphLayout::ENodeRole::EventEntry).Y;
	SceneState.RoleCenters.Add(BlueprintHelper::GraphLayout::ENodeRole::ExecNode, ExecCenter);
	BlueprintHelper::GraphLayout::FSemanticSceneAdapter::ApplySceneStateToRuleSet(
		CurrentScene,
		SceneState,
		ParsedRuleSet,
		1.0f);
	CommitSettingsRuleSetJson(BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(ParsedRuleSet));
	SetStatusMessage(TEXT("已对齐执行行并刷新预览。"), true);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnPreviewClicked()
{
	SetPreviewState(EPreviewState::PreviewLoading, TEXT("正在刷新预览数据..."));
	StartPreviewBuild();
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnApplyPreviewChangesClicked()
{
	ApplyPendingPreviewInteractionCommit();
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnDiscardPreviewChangesClicked()
{
	if (!PreviewInteractionCommitCoordinator.HasPendingChanges())
	{
		SetStatusMessage(TEXT("没有待放弃的预览修改。"), true);
		return FReply::Handled();
	}

	ClearPendingPreviewInteractionCommit();
	SetStatusMessage(TEXT("已放弃未应用的预览修改。"), true);
	SetPreviewState(EPreviewState::PreviewLoading, TEXT("正在恢复预览..."));
	StartPreviewBuild();
	return FReply::Handled();
}

void SBlueprintHelperLayoutRuleEditor::HandleRuleSetTextChanged(const FText& InText)
{
	if (bUpdatingTextFromCode)
	{
		return;
	}

	ClearPendingPreviewInteractionCommit();
	RuleSetJson = InText.ToString();
	SetStatusMessage(TEXT("RuleSet JSON 已编辑，请先校验再导出。"), false);
	RefreshSettingsFromJson();

	if (RuleSetJsonChangedDelegate.IsBound())
	{
		RuleSetJsonChangedDelegate.Execute(RuleSetJson);
	}
}

void SBlueprintHelperLayoutRuleEditor::RefreshSettingsFromJson()
{
	BlueprintHelper::GraphLayout::FRuleSet ParsedRuleSet;
	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(RuleSetJson, ParsedRuleSet, Validation))
	{
		return;
	}

	TGuardValue<bool> UpdatingSettingsGuard(bUpdatingSettingsFromJson, true);
	SettingsRuleId = ParsedRuleSet.Id;
	SettingsDisplayName = ParsedRuleSet.DisplayName;
	SettingsCanvasWidth = LayoutRuleEditorSettings.CanvasDesiredSize.X;
	SettingsCanvasHeight = LayoutRuleEditorSettings.CanvasDesiredSize.Y;
	SettingsExecColumnSpacing = ParsedRuleSet.ExecColumnSpacing;
	SettingsExecRowSpacing = ParsedRuleSet.ExecRowSpacing;
	SettingsBranchRowSpacing = ParsedRuleSet.BranchRowSpacing;
	SettingsPureInputOffsetX = ParsedRuleSet.PureInputOffsetX;
	SettingsVariableInputOffsetX = ParsedRuleSet.VariableInputOffsetX;
	SettingsInputPinRowSpacing = ParsedRuleSet.InputPinRowSpacing;
	bSettingsAlignExecNodesHorizontally = ParsedRuleSet.bAlignExecNodesHorizontally;
	bSettingsUsePureDataSubgraphLayout = ParsedRuleSet.bUsePureDataSubgraphLayout;
	bSettingsUsePatternRowHeightBudget = ParsedRuleSet.bUsePatternRowHeightBudget;
	SettingsDataClusterPaddingX = ParsedRuleSet.DataClusterPaddingX;
	SettingsDataClusterPaddingY = ParsedRuleSet.DataClusterPaddingY;
	SettingsBranchRowPaddingY = ParsedRuleSet.BranchRowPaddingY;
	SettingsCollisionPaddingX = ParsedRuleSet.CollisionPaddingX;
	SettingsCollisionPaddingY = ParsedRuleSet.CollisionPaddingY;
	SettingsCollisionStepY = ParsedRuleSet.CollisionStepY;
	SettingsMaxCollisionAttempts = ParsedRuleSet.MaxCollisionAttempts;
	SettingsMaxNodesPerFrame = ParsedRuleSet.MaxNodesPerFrame;
	SettingsMaxMillisecondsPerFrame =
		BlueprintHelperLayoutRuleEditorLocal::ClampMillisecondsPerFrame(ParsedRuleSet.MaxMillisecondsPerFrame);
	LayoutRuleEditorSettings.MaxMillisecondsPerFrame = SettingsMaxMillisecondsPerFrame;
	bSettingsMoveGeneratedNodes = ParsedRuleSet.bMoveGeneratedNodes;
	bSettingsMoveExistingNodes = ParsedRuleSet.bMoveExistingNodes;
	bSettingsMarkDirtyAfterApply = ParsedRuleSet.bMarkDirtyAfterApply;
	bSettingsSaveAfterApply = ParsedRuleSet.bSaveAfterApply;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlueprintHelperLayoutRuleEditor::HandleTextSettingCommitted(int32 SettingId, const FText& NewValue)
{
	if (bUpdatingSettingsFromJson)
	{
		return;
	}

	BlueprintHelper::GraphLayout::FRuleSet ParsedRuleSet;
	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(RuleSetJson, ParsedRuleSet, Validation))
	{
		SetStatusMessage(TEXT("RuleSet JSON 无效，设置未应用。"), false);
		return;
	}

	using namespace BlueprintHelperLayoutRuleEditorLocal;
	const FString NewString = NewValue.ToString().TrimStartAndEnd();
	switch (SettingId)
	{
	case RuleId:
		ParsedRuleSet.Id = NewString;
		break;
	case DisplayName:
		ParsedRuleSet.DisplayName = NewString;
		break;
	default:
		return;
	}

	CommitSettingsRuleSetJson(BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(ParsedRuleSet));
}

void SBlueprintHelperLayoutRuleEditor::HandleFloatSettingChanged(int32 SettingId, float NewValue)
{
	if (bUpdatingSettingsFromJson)
	{
		return;
	}

	BlueprintHelper::GraphLayout::FRuleSet ParsedRuleSet;
	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(RuleSetJson, ParsedRuleSet, Validation))
	{
		SetStatusMessage(TEXT("RuleSet JSON 无效，设置未应用。"), false);
		return;
	}

	using namespace BlueprintHelperLayoutRuleEditorLocal;
	switch (SettingId)
	{
	case ExecColumnSpacing:
		ParsedRuleSet.ExecColumnSpacing = FMath::Clamp(NewValue, 120.0f, 1200.0f);
		break;
	case ExecRowSpacing:
		ParsedRuleSet.ExecRowSpacing = FMath::Clamp(NewValue, 80.0f, 900.0f);
		break;
	case BranchRowSpacing:
		ParsedRuleSet.BranchRowSpacing = FMath::Clamp(NewValue, 80.0f, 900.0f);
		break;
	case PureInputOffsetX:
		ParsedRuleSet.PureInputOffsetX = FMath::Clamp(NewValue, 80.0f, 900.0f);
		break;
	case VariableInputOffsetX:
		ParsedRuleSet.VariableInputOffsetX = FMath::Clamp(NewValue, 80.0f, 900.0f);
		break;
	case InputPinRowSpacing:
		ParsedRuleSet.InputPinRowSpacing = FMath::Clamp(NewValue, 24.0f, 220.0f);
		break;
	case DataClusterPaddingX:
		ParsedRuleSet.DataClusterPaddingX = FMath::Clamp(NewValue, 1.0f, 400.0f);
		break;
	case DataClusterPaddingY:
		ParsedRuleSet.DataClusterPaddingY = FMath::Clamp(NewValue, 1.0f, 400.0f);
		break;
	case BranchRowPaddingY:
		ParsedRuleSet.BranchRowPaddingY = FMath::Clamp(NewValue, 1.0f, 400.0f);
		break;
	case CollisionPaddingX:
		ParsedRuleSet.CollisionPaddingX = FMath::Clamp(NewValue, 1.0f, 400.0f);
		break;
	case CollisionPaddingY:
		ParsedRuleSet.CollisionPaddingY = FMath::Clamp(NewValue, 1.0f, 400.0f);
		break;
	case CollisionStepY:
		ParsedRuleSet.CollisionStepY = FMath::Clamp(NewValue, 8.0f, 400.0f);
		break;
	case MaxMillisecondsPerFrame:
		ParsedRuleSet.MaxMillisecondsPerFrame = BlueprintHelperLayoutRuleEditorLocal::ClampMillisecondsPerFrame(NewValue);
		break;
	default:
		return;
	}

	CommitSettingsRuleSetJson(BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(ParsedRuleSet));
}

void SBlueprintHelperLayoutRuleEditor::HandleUiFloatSettingChanged(int32 SettingId, float NewValue)
{
	using namespace BlueprintHelperLayoutRuleEditorLocal;
	switch (SettingId)
	{
	case CanvasWidth:
		LayoutRuleEditorSettings.CanvasDesiredSize.X = ClampCanvasWidth(NewValue);
		break;
	case CanvasHeight:
		LayoutRuleEditorSettings.CanvasDesiredSize.Y = ClampCanvasHeight(NewValue);
		break;
	default:
		return;
	}

	SettingsCanvasWidth = LayoutRuleEditorSettings.CanvasDesiredSize.X;
	SettingsCanvasHeight = LayoutRuleEditorSettings.CanvasDesiredSize.Y;
	if (WorkspaceBox.IsValid())
	{
		WorkspaceBox->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);

	const FString CanvasSizeValue = FString::Printf(
		TEXT("[%s, %s]"),
		*FString::SanitizeFloat(LayoutRuleEditorSettings.CanvasDesiredSize.X),
		*FString::SanitizeFloat(LayoutRuleEditorSettings.CanvasDesiredSize.Y));
	FString SettingError;
	const bool bSaved = FBlueprintHelperSettingStore::UpdateProjectSettingValue(
		TEXT("ui.layout_rule_editor.canvas_desired_size"),
		CanvasSizeValue,
		SettingError);
	SetStatusMessage(
		bSaved
			? TEXT("已保存界面画布尺寸设置。")
			: FString::Printf(TEXT("界面画布尺寸已更新，但保存失败：%s"), *SettingError),
		bSaved);
}

void SBlueprintHelperLayoutRuleEditor::HandleIntSettingChanged(int32 SettingId, int32 NewValue)
{
	if (bUpdatingSettingsFromJson)
	{
		return;
	}

	BlueprintHelper::GraphLayout::FRuleSet ParsedRuleSet;
	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(RuleSetJson, ParsedRuleSet, Validation))
	{
		SetStatusMessage(TEXT("RuleSet JSON 无效，设置未应用。"), false);
		return;
	}

	using namespace BlueprintHelperLayoutRuleEditorLocal;
	switch (SettingId)
	{
	case MaxNodesPerFrame:
		ParsedRuleSet.MaxNodesPerFrame = FMath::Clamp(NewValue, 1, 256);
		break;
	case MaxCollisionAttempts:
		ParsedRuleSet.MaxCollisionAttempts = FMath::Clamp(NewValue, 1, 256);
		break;
	default:
		return;
	}

	CommitSettingsRuleSetJson(BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(ParsedRuleSet));
}

void SBlueprintHelperLayoutRuleEditor::HandleBoolSettingChanged(int32 SettingId, bool bNewValue)
{
	if (bUpdatingSettingsFromJson)
	{
		return;
	}

	BlueprintHelper::GraphLayout::FRuleSet ParsedRuleSet;
	BlueprintHelper::GraphLayout::FValidationResult Validation;
	if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(RuleSetJson, ParsedRuleSet, Validation))
	{
		SetStatusMessage(TEXT("RuleSet JSON 无效，设置未应用。"), false);
		return;
	}

	using namespace BlueprintHelperLayoutRuleEditorLocal;
	switch (SettingId)
	{
	case AlignExecNodesHorizontally:
		ParsedRuleSet.bAlignExecNodesHorizontally = bNewValue;
		break;
	case UsePureDataSubgraphLayout:
		ParsedRuleSet.bUsePureDataSubgraphLayout = bNewValue;
		break;
	case UsePatternRowHeightBudget:
		ParsedRuleSet.bUsePatternRowHeightBudget = bNewValue;
		break;
	case MoveGeneratedNodes:
		ParsedRuleSet.bMoveGeneratedNodes = bNewValue;
		break;
	case MoveExistingNodes:
		ParsedRuleSet.bMoveExistingNodes = bNewValue;
		break;
	case MarkDirtyAfterApply:
		ParsedRuleSet.bMarkDirtyAfterApply = bNewValue;
		break;
	case SaveAfterApply:
		ParsedRuleSet.bSaveAfterApply = bNewValue;
		break;
	default:
		return;
	}

	CommitSettingsRuleSetJson(BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(ParsedRuleSet));
}

void SBlueprintHelperLayoutRuleEditor::CommitSettingsRuleSetJson(const FString& InUpdatedRuleSetJson)
{
	ClearPendingPreviewInteractionCommit();
	RuleSetJson = InUpdatedRuleSetJson;

	if (RuleSetTextBox.IsValid())
	{
		TGuardValue<bool> UpdatingTextGuard(bUpdatingTextFromCode, true);
		RuleSetTextBox->SetText(FText::FromString(RuleSetJson));
	}

	RefreshSettingsFromJson();

	FString Message;
	bLastValidationPassed = ValidateRuleSetJson(Message);
	if (bLastValidationPassed)
	{
		bool bSaved = false;
		if (ExportJsonDelegate.IsBound())
		{
			bSaved = ExportJsonDelegate.Execute(RuleSetJson);
		}
		else
		{
			FString ExportMessage;
			bSaved = SaveJsonToDefaultFile(RuleSetJson, ExportMessage);
		}
		SetStatusMessage(
			bSaved ? TEXT("已从设置更新并保存 RuleSet。") : TEXT("已从设置更新 RuleSet，但保存失败。"),
			bSaved);
		SetPreviewState(EPreviewState::PreviewLoading, TEXT("正在刷新预览数据..."));
		StartPreviewBuild();
	}
	else
	{
		SetStatusMessage(Message, false);
	}

	if (RuleSetJsonChangedDelegate.IsBound())
	{
		RuleSetJsonChangedDelegate.Execute(RuleSetJson);
	}
}

void SBlueprintHelperLayoutRuleEditor::SetStatusMessage(const FString& InMessage, bool bInValid)
{
	if (!ValidationStatusTextBlock.IsValid())
	{
		return;
	}

	ValidationStatusTextBlock->SetText(FText::FromString(InMessage));
	ValidationStatusTextBlock->SetColorAndOpacity(bInValid
		? BlueprintHelperLayoutRuleEditorLocal::ValidStatusColor
		: BlueprintHelperLayoutRuleEditorLocal::InvalidStatusColor);
}

bool SBlueprintHelperLayoutRuleEditor::ValidateRuleSetJson(FString& OutMessage) const
{
	if (ValidateJsonDelegate.IsBound())
	{
		return ValidateJsonDelegate.Execute(RuleSetJson, OutMessage);
	}

	const FString TrimmedJson = RuleSetJson.TrimStartAndEnd();
	if (TrimmedJson.IsEmpty())
	{
		OutMessage = TEXT("RuleSet JSON 为空。");
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutMessage = FString::Printf(TEXT("RuleSet JSON 解析失败：%s"), *Reader->GetErrorMessage());
		return false;
	}

#if BLUEPRINTHELPER_LAYOUT_RULE_EDITOR_HAS_GRAPH_LAYOUT_RULESET_JSON
	const BlueprintHelper::GraphLayout::FValidationResult Validation = BlueprintHelper::GraphLayout::FRuleSetJson::Validate(RootObject);
	if (!Validation.bValid)
	{
		OutMessage = Validation.Errors.Num() > 0
			? FString::Join(Validation.Errors, TEXT(" "))
			: TEXT("RuleSet JSON 未通过 GraphLayout 校验。");
		return false;
	}

	OutMessage = Validation.Warnings.Num() > 0
		? FString::Printf(TEXT("RuleSet JSON 有效。警告：%s"), *FString::Join(Validation.Warnings, TEXT(" ")))
		: TEXT("RuleSet JSON 有效。");
	return true;
#else
	OutMessage = TEXT("RuleSet JSON 是有效 JSON；当前未绑定 GraphLayout schema 级校验。");
	return true;
#endif
}

void SBlueprintHelperLayoutRuleEditor::SetPreviewState(
	const EPreviewState InPreviewState,
	const FString& InStatusMessage)
{
	PreviewState = InPreviewState;
	PreviewStatusMessage = InStatusMessage;
	RefreshWorkspace();
}

void SBlueprintHelperLayoutRuleEditor::RefreshWorkspace()
{
	if (!WorkspaceBox.IsValid())
	{
		return;
	}

	WorkspaceBox->SetContent(BuildPreviewWorkspace());
}

void SBlueprintHelperLayoutRuleEditor::StartPreviewBuild()
{
	ClearPendingPreviewInteractionCommit();
	if (!PreviewService.IsValid())
	{
		PreviewService = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewService>();
	}

	if (ActivePreviewJobId != 0)
	{
		PreviewService->Cancel(ActivePreviewJobId);
		ActivePreviewJobId = 0;
	}

	PendingPreviewBuildResult.Reset();
	PreviewGraphEditor.Reset();
	PreviewInteractionSurface.Reset();
	PreviewInteractionModel.Reset();
	ActivePreviewGraph.Reset();
	PreviewMaterializer = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewMaterializer>();

	BlueprintHelper::GraphLayout::FGraphLayoutPreviewRequest Request;
	Request.Scene = CurrentScene;
	Request.RuleSetJson = RuleSetJson;
	const uint64 PreviewGeneration = ++ActivePreviewGeneration;
	ActivePreviewJobId = PreviewService->StartPreviewBuild(
		Request,
		BlueprintHelper::GraphLayout::FGraphLayoutPreviewBuildCompleted::CreateSP(
			this,
			&SBlueprintHelperLayoutRuleEditor::HandlePreviewBuildCompleted,
			PreviewGeneration));
}

void SBlueprintHelperLayoutRuleEditor::HandlePreviewBuildCompleted(
	const BlueprintHelper::GraphLayout::FGraphLayoutPreviewBuildResult& Result,
	const uint64 ExpectedPreviewGeneration)
{
	if (ExpectedPreviewGeneration != ActivePreviewGeneration ||
		(Result.JobId != 0 && Result.JobId != ActivePreviewJobId) ||
		PreviewState != EPreviewState::PreviewLoading)
	{
		return;
	}

	ActivePreviewJobId = 0;
	if (!Result.bSuccess)
	{
		SetPreviewState(
			EPreviewState::PreviewError,
			Result.Error.IsEmpty() ? TEXT("预览数据构建失败。") : Result.Error);
		return;
	}

	PendingPreviewBuildResult = Result;
	SetPreviewState(EPreviewState::PreviewMaterializing, TEXT("正在实例化预览图表..."));
}

void SBlueprintHelperLayoutRuleEditor::PumpPreviewMaterializer()
{
	if (PreviewState != EPreviewState::PreviewMaterializing)
	{
		return;
	}

	if (!PreviewMaterializer.IsValid())
	{
		PreviewMaterializer = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewMaterializer>();
	}

	if (PendingPreviewBuildResult.IsSet())
	{
		const BlueprintHelper::GraphLayout::FGraphLayoutPreviewBuildResult& BuildResult =
			PendingPreviewBuildResult.GetValue();
		PreviewMaterializer->Begin(
			BuildResult.Sample,
			BuildResult.LayoutPlan);
		PendingPreviewBuildResult.Reset();
	}

	if (!PreviewMaterializer->IsComplete())
	{
		PreviewMaterializer->Tick(LayoutRuleEditorSettings.MaxMillisecondsPerFrame);
	}

	if (!PreviewMaterializer->IsComplete())
	{
		return;
	}

	const BlueprintHelper::GraphLayout::FGraphLayoutPreviewMaterializerResult& MaterializerResult =
		PreviewMaterializer->GetResult();
	if (!MaterializerResult.Error.IsEmpty() || !MaterializerResult.PreviewGraph.IsValid())
	{
		SetPreviewState(
			EPreviewState::PreviewError,
			MaterializerResult.Error.IsEmpty()
				? TEXT("预览图表实例化失败。")
				: MaterializerResult.Error);
		return;
	}

	ActivePreviewGraph = MaterializerResult.PreviewGraph.Get();
	PreviewInteractionModel.Initialize(MaterializerResult, ActivePreviewGraph.Get());
	BuildPreviewGraphEditor(MaterializerResult.PreviewGraph.Get());
	SetPreviewState(EPreviewState::PreviewReady, TEXT("预览已就绪。"));
}

void SBlueprintHelperLayoutRuleEditor::BuildPreviewGraphEditor(UEdGraph* PreviewGraph)
{
	if (!PreviewGraph)
	{
		PreviewGraphEditor.Reset();
		PreviewInteractionSurface.Reset();
		return;
	}

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("LayoutRulePreviewCornerText", "预览");
	Appearance.InstructionText = LOCTEXT("LayoutRulePreviewInstruction", "拖拽节点校准 GraphLayout 规则");
	Appearance.ReadOnlyText = LOCTEXT("LayoutRulePreviewReadOnly", "仅预览");

	TSharedRef<SGraphEditor> Editor = SAssignNew(PreviewGraphEditor, SGraphEditor)
		.IsEditable(true)
		.DisplayAsReadOnly(false)
		.GraphToEdit(PreviewGraph)
		.Appearance(Appearance)
		.ShowGraphStateOverlay(false);
	PreviewInteractionSurface = SNew(SBlueprintHelperLayoutPreviewInteractionSurface)
		.OnInteractionBegin(FBlueprintHelperLayoutPreviewInteractionEvent::CreateSP(
			this,
			&SBlueprintHelperLayoutRuleEditor::HandlePreviewInteractionBegin))
		.OnInteractionEnd(FBlueprintHelperLayoutPreviewInteractionEvent::CreateSP(
			this,
			&SBlueprintHelperLayoutRuleEditor::HandlePreviewInteractionEnd))
		[
			Editor
		];
	Editor->NotifyGraphChanged();
}

void SBlueprintHelperLayoutRuleEditor::HandlePreviewInteractionBegin()
{
	if (ActivePreviewGraph.IsValid())
	{
		PreviewInteractionModel.BeginInteraction(ActivePreviewGraph.Get());
	}
}

void SBlueprintHelperLayoutRuleEditor::HandlePreviewInteractionEnd()
{
	if (!ActivePreviewGraph.IsValid())
	{
		return;
	}

	BlueprintHelper::GraphLayout::FGraphLayoutPreviewInteractionCommit Commit;
	if (!PreviewInteractionModel.EndInteraction(ActivePreviewGraph.Get(), Commit))
	{
		if (!Commit.RejectionReason.IsEmpty())
		{
			SetStatusMessage(
				FString::Printf(TEXT("预览仅允许移动节点或调整避让范围大小，已拒绝本次修改：%s"), *Commit.RejectionReason),
				false);
			SetPreviewState(EPreviewState::PreviewLoading, TEXT("正在恢复预览..."));
			StartPreviewBuild();
		}
		return;
	}

	PreviewInteractionCommitCoordinator.Append(Commit);
	SetStatusMessage(TEXT("预览修改待确认。点击“应用预览修改”写入设置，或点击“放弃预览修改”还原。"), true);
	SetPreviewState(EPreviewState::PreviewReady, TEXT("预览修改待确认。"));
}

bool SBlueprintHelperLayoutRuleEditor::ApplyPendingPreviewInteractionCommit()
{
	const BlueprintHelper::GraphLayout::FGraphLayoutPreviewInteractionApplyResult ApplyResult =
		PreviewInteractionCommitCoordinator.ConsumePendingRuleSetJson(RuleSetJson, CurrentScene);
	if (ApplyResult.Status == BlueprintHelper::GraphLayout::EGraphLayoutPreviewInteractionApplyStatus::NoPendingChanges)
	{
		SetStatusMessage(ApplyResult.Message, true);
		return false;
	}

	if (ApplyResult.Status == BlueprintHelper::GraphLayout::EGraphLayoutPreviewInteractionApplyStatus::Failed)
	{
		SetStatusMessage(ApplyResult.Message, false);
		return false;
	}

	CommitSettingsRuleSetJson(ApplyResult.UpdatedRuleSetJson);
	SetStatusMessage(ApplyResult.Message, true);
	return true;
}

void SBlueprintHelperLayoutRuleEditor::CancelActivePreview()
{
	ClearPendingPreviewInteractionCommit();
	if (PreviewService.IsValid() && ActivePreviewJobId != 0)
	{
		PreviewService->Cancel(ActivePreviewJobId);
	}

	ActivePreviewJobId = 0;
	++ActivePreviewGeneration;
	PendingPreviewBuildResult.Reset();
	PreviewGraphEditor.Reset();
	PreviewInteractionSurface.Reset();
	PreviewInteractionModel.Reset();
	ActivePreviewGraph.Reset();
	if (PreviewMaterializer.IsValid())
	{
		PreviewMaterializer->Cancel();
	}
}

void SBlueprintHelperLayoutRuleEditor::ClearPendingPreviewInteractionCommit()
{
	PreviewInteractionCommitCoordinator.Reset();
}

FString SBlueprintHelperLayoutRuleEditor::LoadJsonFromDefaultFile(FString& OutMessage) const
{
	const FString DefaultJsonFilePath = GetDefaultJsonFilePath();
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *DefaultJsonFilePath))
	{
		OutMessage = FString::Printf(TEXT("导入失败：未绑定配置入口，且默认文件不可读：%s"), *DefaultJsonFilePath);
		return TEXT("");
	}

	OutMessage = FString::Printf(TEXT("已从 %s 导入 RuleSet JSON"), *DefaultJsonFilePath);
	return JsonText;
}

bool SBlueprintHelperLayoutRuleEditor::SaveJsonToDefaultFile(const FString& JsonText, FString& OutMessage) const
{
	const FString DefaultJsonFilePath = GetDefaultJsonFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(DefaultJsonFilePath), true);

	if (!FFileHelper::SaveStringToFile(JsonText, *DefaultJsonFilePath))
	{
		OutMessage = FString::Printf(TEXT("导出失败：未绑定配置入口，且默认文件不可写：%s"), *DefaultJsonFilePath);
		return false;
	}

	OutMessage = FString::Printf(TEXT("已将 RuleSet JSON 导出到 %s"), *DefaultJsonFilePath);
	return true;
}

FString SBlueprintHelperLayoutRuleEditor::GetDefaultJsonFilePath() const
{
	return FBlueprintHelperProjectConfigPaths::GetGraphLayoutRulesPath();
}

#undef LOCTEXT_NAMESPACE
