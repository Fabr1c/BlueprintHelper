// This Project Is Made By Fabric

#include "UI/Layout/SBlueprintHelperLayoutRuleEditor.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "UI/BlueprintHelperUiSettingsResolver.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SLeafWidget.h"
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
	const FVector2D CanvasDesiredSize(760.0f, 320.0f);
	const FVector2D CanvasNodeSize(128.0f, 44.0f);
	const float CanvasRuleScale = 0.45f;

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
		MaxMillisecondsPerFrame
	};

	enum EIntSetting : int32
	{
		MaxNodesPerFrame = 0
	};

	enum EBoolSetting : int32
	{
		MoveGeneratedNodes = 0,
		MoveExistingNodes,
		MarkDirtyAfterApply,
		SaveAfterApply
	};

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

	TSharedRef<SWidget> BuildRoleChip(const FText& Label, const FLinearColor& Color)
	{
		return SNew(SBorder)
			.BorderBackgroundColor(Color)
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(STextBlock)
				.Text(Label)
				.ColorAndOpacity(FLinearColor::White)
			];
	}

	FLinearColor GetRoleColor(BlueprintHelper::GraphLayout::ENodeRole Role)
	{
		switch (Role)
		{
		case BlueprintHelper::GraphLayout::ENodeRole::EventEntry: return FLinearColor(0.42f, 0.2f, 0.78f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::ExecNode: return FLinearColor(0.85f, 0.1f, 0.08f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::BranchControl: return FLinearColor(0.88f, 0.45f, 0.08f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::PureFunction: return FLinearColor(0.1f, 0.65f, 0.25f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::OperatorOrCompare: return FLinearColor(0.42f, 0.78f, 0.1f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::VariableInput: return FLinearColor(0.0f, 0.5f, 0.85f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::AsyncNode: return FLinearColor(0.0f, 0.68f, 0.78f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::DelegateNode: return FLinearColor(0.78f, 0.66f, 0.08f, 1.0f);
		case BlueprintHelper::GraphLayout::ENodeRole::Comment: return FLinearColor(0.22f, 0.22f, 0.22f, 1.0f);
		default: return FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);
		}
	}

	struct FCanvasRoleNode
	{
		BlueprintHelper::GraphLayout::ENodeRole Role = BlueprintHelper::GraphLayout::ENodeRole::Unknown;
		FText Label;
		FVector2D Center = FVector2D::ZeroVector;
		bool bDraggable = true;
	};
}

DECLARE_DELEGATE_OneParam(FBlueprintHelperLayoutRuleCanvasChanged, const FString&);

class SBlueprintHelperLayoutRuleCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperLayoutRuleCanvas)
	{
	}
				SLATE_ARGUMENT(FBlueprintHelperLayoutRuleEditorSettings, LayoutRuleEditorSettings)
SLATE_EVENT(FBlueprintHelperLayoutRuleCanvasChanged, OnRuleSetChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
				LayoutRuleEditorSettings = InArgs._LayoutRuleEditorSettings;
RuleSetChangedDelegate = InArgs._OnRuleSetChanged;
		SetRuleSetJson(BlueprintHelperLayoutRuleEditorLocal::GetFallbackDefaultJson());
	}

	void SetRuleSetJson(const FString& InRuleSetJson)
	{
		BlueprintHelper::GraphLayout::FRuleSet ImportedRuleSet;
		BlueprintHelper::GraphLayout::FValidationResult Validation;
		if (BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(InRuleSetJson, ImportedRuleSet, Validation))
		{
			RuleSet = ImportedRuleSet;
		}
		else
		{
			RuleSet = BlueprintHelper::GraphLayout::FRuleSet();
		}
		BuildCanvasFromRuleSet();
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return LayoutRuleEditorSettings.CanvasDesiredSize;
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FVector2D CanvasSize = AllottedGeometry.GetLocalSize();
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(CanvasSize, FSlateLayoutTransform(FVector2D::ZeroVector)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.035f, 0.035f, 0.04f, 1.0f));

		DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1,
			BlueprintHelper::GraphLayout::ENodeRole::EventEntry,
			BlueprintHelper::GraphLayout::ENodeRole::ExecNode,
			FLinearColor(0.85f, 0.18f, 0.14f, 1.0f));
		DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1,
			BlueprintHelper::GraphLayout::ENodeRole::ExecNode,
			BlueprintHelper::GraphLayout::ENodeRole::BranchControl,
			FLinearColor(0.85f, 0.18f, 0.14f, 1.0f));
		DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1,
			BlueprintHelper::GraphLayout::ENodeRole::PureFunction,
			BlueprintHelper::GraphLayout::ENodeRole::ExecNode,
			FLinearColor(0.1f, 0.75f, 0.32f, 1.0f));
		DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1,
			BlueprintHelper::GraphLayout::ENodeRole::OperatorOrCompare,
			BlueprintHelper::GraphLayout::ENodeRole::ExecNode,
			FLinearColor(0.42f, 0.84f, 0.12f, 1.0f));
		DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1,
			BlueprintHelper::GraphLayout::ENodeRole::VariableInput,
			BlueprintHelper::GraphLayout::ENodeRole::ExecNode,
			FLinearColor(0.0f, 0.62f, 0.9f, 1.0f));
		DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1,
			BlueprintHelper::GraphLayout::ENodeRole::ExecNode,
			BlueprintHelper::GraphLayout::ENodeRole::AsyncNode,
			FLinearColor(0.0f, 0.72f, 0.85f, 1.0f));
		DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1,
			BlueprintHelper::GraphLayout::ENodeRole::AsyncNode,
			BlueprintHelper::GraphLayout::ENodeRole::DelegateNode,
			FLinearColor(0.78f, 0.66f, 0.08f, 1.0f));

		const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
		for (const BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode& Node : BuildRoleNodes())
		{
			const FVector2D TopLeft = Node.Center - LayoutRuleEditorSettings.NodeSize * 0.5f;
			const bool bDragged = DraggedRole.IsSet() && DraggedRole.GetValue() == Node.Role;
			const FLinearColor RoleColor = BlueprintHelperLayoutRuleEditorLocal::GetRoleColor(Node.Role);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				AllottedGeometry.ToPaintGeometry(LayoutRuleEditorSettings.NodeSize, FSlateLayoutTransform(TopLeft)),
				WhiteBrush,
				ESlateDrawEffect::None,
				bDragged ? RoleColor * 1.25f : RoleColor);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 3,
				AllottedGeometry.ToPaintGeometry(LayoutRuleEditorSettings.NodeSize - FVector2D(12.0f, 0.0f), FSlateLayoutTransform(TopLeft + FVector2D(8.0f, 13.0f))),
				Node.Label,
				LabelFont,
				ESlateDrawEffect::None,
				FLinearColor::White);
		}

		const FText FooterText = FText::Format(
			LOCTEXT("CanvasFooter", "exec {0}px | data {1}px | pin row {2}px | apply {3}/frame {4}ms | save_after_apply {5}"),
			FText::AsNumber(FMath::RoundToInt(RuleSet.ExecColumnSpacing)),
			FText::AsNumber(FMath::RoundToInt(RuleSet.VariableInputOffsetX)),
			FText::AsNumber(FMath::RoundToInt(RuleSet.InputPinRowSpacing)),
			FText::AsNumber(RuleSet.MaxNodesPerFrame),
			FText::AsNumber(RuleSet.MaxMillisecondsPerFrame),
			RuleSet.bSaveAfterApply ? LOCTEXT("SaveAfterApplyOn", "on") : LOCTEXT("SaveAfterApplyOff", "off"));
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry.ToPaintGeometry(FVector2D(CanvasSize.X - 24.0f, 20.0f), FSlateLayoutTransform(FVector2D(12.0f, CanvasSize.Y - 28.0f))),
			FooterText,
			FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9),
			ESlateDrawEffect::None,
			FLinearColor(0.72f, 0.72f, 0.72f, 1.0f));

		return LayerId + 4;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const TOptional<BlueprintHelper::GraphLayout::ENodeRole> HitRole = HitTestRole(LocalPos);
		if (!HitRole.IsSet())
		{
			return FReply::Unhandled();
		}

		DraggedRole = HitRole.GetValue();
		DragOffset = LocalPos - RoleCenters.FindRef(DraggedRole.GetValue());
		return FReply::Handled().CaptureMouse(AsShared());
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!DraggedRole.IsSet() || !HasMouseCapture())
		{
			return FReply::Unhandled();
		}

		const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		FVector2D NewCenter = LocalPos - DragOffset;
		const FVector2D CanvasSize = MyGeometry.GetLocalSize();
		NewCenter.X = FMath::Clamp(NewCenter.X, 40.0f, FMath::Max(40.0f, CanvasSize.X - 40.0f));
		NewCenter.Y = FMath::Clamp(NewCenter.Y, 40.0f, FMath::Max(40.0f, CanvasSize.Y - 40.0f));
		RoleCenters.Add(DraggedRole.GetValue(), NewCenter);
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !DraggedRole.IsSet())
		{
			return FReply::Unhandled();
		}

		DraggedRole.Reset();
		ExportCanvasToRuleSet();
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
		return HitTestRole(LocalPos).IsSet()
			? FCursorReply::Cursor(EMouseCursor::GrabHand)
			: FCursorReply::Unhandled();
	}

private:
	FBlueprintHelperLayoutRuleEditorSettings LayoutRuleEditorSettings;

	TArray<BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode> BuildRoleNodes() const
	{
		using namespace BlueprintHelper::GraphLayout;
		return {
			{ENodeRole::Comment, LOCTEXT("CanvasComment", "Comment"), RoleCenters.FindRef(ENodeRole::Comment), false},
			{ENodeRole::EventEntry, LOCTEXT("CanvasEventEntry", "EventEntry"), RoleCenters.FindRef(ENodeRole::EventEntry), true},
			{ENodeRole::ExecNode, LOCTEXT("CanvasExecNode", "ExecNode"), RoleCenters.FindRef(ENodeRole::ExecNode), true},
			{ENodeRole::BranchControl, LOCTEXT("CanvasBranchControl", "BranchControl"), RoleCenters.FindRef(ENodeRole::BranchControl), true},
			{ENodeRole::PureFunction, LOCTEXT("CanvasPureFunction", "PureFunction"), RoleCenters.FindRef(ENodeRole::PureFunction), true},
			{ENodeRole::OperatorOrCompare, LOCTEXT("CanvasOperatorOrCompare", "OperatorOrCompare"), RoleCenters.FindRef(ENodeRole::OperatorOrCompare), true},
			{ENodeRole::VariableInput, LOCTEXT("CanvasVariableInput", "VariableInput"), RoleCenters.FindRef(ENodeRole::VariableInput), true},
			{ENodeRole::AsyncNode, LOCTEXT("CanvasAsyncNode", "AsyncNode"), RoleCenters.FindRef(ENodeRole::AsyncNode), true},
			{ENodeRole::DelegateNode, LOCTEXT("CanvasDelegateNode", "DelegateNode"), RoleCenters.FindRef(ENodeRole::DelegateNode), true}
		};
	}

	void BuildCanvasFromRuleSet()
	{
		using namespace BlueprintHelper::GraphLayout;
		const float Scale = LayoutRuleEditorSettings.CanvasRuleScale;
		const FVector2D EventCenter(92.0f, 126.0f);
		const FVector2D ExecCenter = EventCenter + FVector2D(RuleSet.ExecColumnSpacing * Scale, 0.0f);
		const FVector2D BranchCenter = ExecCenter + FVector2D(RuleSet.ExecColumnSpacing * Scale, RuleSet.BranchRowSpacing * Scale);
		const FVector2D PureCenter = ExecCenter - FVector2D(RuleSet.PureInputOffsetX * Scale, RuleSet.InputPinRowSpacing * Scale);
		const FVector2D OperatorCenter = ExecCenter - FVector2D(RuleSet.PureInputOffsetX * Scale, 0.0f);
		const FVector2D VariableCenter = ExecCenter + FVector2D(-RuleSet.VariableInputOffsetX * Scale, RuleSet.InputPinRowSpacing * Scale);
		const FVector2D AsyncCenter = ExecCenter + FVector2D(RuleSet.ExecColumnSpacing * Scale, RuleSet.ExecRowSpacing * Scale);
		RoleCenters.Reset();
		RoleCenters.Add(ENodeRole::Comment, FVector2D(92.0f, 36.0f));
		RoleCenters.Add(ENodeRole::EventEntry, EventCenter);
		RoleCenters.Add(ENodeRole::ExecNode, ExecCenter);
		RoleCenters.Add(ENodeRole::BranchControl, BranchCenter);
		RoleCenters.Add(ENodeRole::PureFunction, PureCenter);
		RoleCenters.Add(ENodeRole::OperatorOrCompare, OperatorCenter);
		RoleCenters.Add(ENodeRole::VariableInput, VariableCenter);
		RoleCenters.Add(ENodeRole::AsyncNode, AsyncCenter);
		RoleCenters.Add(ENodeRole::DelegateNode, AsyncCenter + FVector2D(0.0f, 58.0f));

		for (const TPair<ENodeRole, FVector2D>& SavedCenter : RuleSet.EditorCanvasRoleCenters)
		{
			if (RoleCenters.Contains(SavedCenter.Key))
			{
				RoleCenters.Add(SavedCenter.Key, SavedCenter.Value);
			}
		}
	}

	TOptional<BlueprintHelper::GraphLayout::ENodeRole> HitTestRole(const FVector2D& LocalPos) const
	{
		for (const BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode& Node : BuildRoleNodes())
		{
			if (!Node.bDraggable)
			{
				continue;
			}
			const FVector2D TopLeft = Node.Center - LayoutRuleEditorSettings.NodeSize * 0.5f;
			const FVector2D BottomRight = TopLeft + LayoutRuleEditorSettings.NodeSize;
			if (LocalPos.X >= TopLeft.X && LocalPos.X <= BottomRight.X &&
				LocalPos.Y >= TopLeft.Y && LocalPos.Y <= BottomRight.Y)
			{
				return Node.Role;
			}
		}
		return {};
	}

	void DrawRelationship(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& AllottedGeometry,
		int32 LayerId,
		BlueprintHelper::GraphLayout::ENodeRole FromRole,
		BlueprintHelper::GraphLayout::ENodeRole ToRole,
		const FLinearColor& Color) const
	{
		const FVector2D From = RoleCenters.FindRef(FromRole);
		const FVector2D To = RoleCenters.FindRef(ToRole);
		if (From.IsZero() || To.IsZero())
		{
			return;
		}

		TArray<FVector2D> Points;
		Points.Add(From);
		Points.Add(To);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Color,
			true,
			2.0f);
	}

	void ExportCanvasToRuleSet()
	{
		using namespace BlueprintHelper::GraphLayout;
		const float Scale = LayoutRuleEditorSettings.CanvasRuleScale;
		const FVector2D EventCenter = RoleCenters.FindRef(ENodeRole::EventEntry);
		const FVector2D ExecCenter = RoleCenters.FindRef(ENodeRole::ExecNode);
		const FVector2D BranchCenter = RoleCenters.FindRef(ENodeRole::BranchControl);
		const FVector2D PureCenter = RoleCenters.FindRef(ENodeRole::PureFunction);
		const FVector2D VariableCenter = RoleCenters.FindRef(ENodeRole::VariableInput);
		const FVector2D AsyncCenter = RoleCenters.FindRef(ENodeRole::AsyncNode);

		TArray<float> HorizontalSamples;
		if (ExecCenter.X > EventCenter.X)
		{
			HorizontalSamples.Add((ExecCenter.X - EventCenter.X) / Scale);
		}
		if (BranchCenter.X > ExecCenter.X)
		{
			HorizontalSamples.Add((BranchCenter.X - ExecCenter.X) / Scale);
		}
		if (AsyncCenter.X > ExecCenter.X)
		{
			HorizontalSamples.Add((AsyncCenter.X - ExecCenter.X) / Scale);
		}
		if (HorizontalSamples.Num() > 0)
		{
			float Sum = 0.0f;
			for (float Sample : HorizontalSamples)
			{
				Sum += Sample;
			}
			RuleSet.ExecColumnSpacing = FMath::Clamp(Sum / HorizontalSamples.Num(), 120.0f, 900.0f);
		}

		RuleSet.BranchRowSpacing = FMath::Clamp(FMath::Abs(BranchCenter.Y - ExecCenter.Y) / Scale, 80.0f, 640.0f);
		RuleSet.ExecRowSpacing = FMath::Clamp(FMath::Abs(AsyncCenter.Y - ExecCenter.Y) / Scale, 80.0f, 640.0f);
		if (ExecCenter.X > PureCenter.X)
		{
			RuleSet.PureInputOffsetX = FMath::Clamp((ExecCenter.X - PureCenter.X) / Scale, 80.0f, 720.0f);
		}
		if (ExecCenter.X > VariableCenter.X)
		{
			RuleSet.VariableInputOffsetX = FMath::Clamp((ExecCenter.X - VariableCenter.X) / Scale, 80.0f, 720.0f);
		}
		RuleSet.InputPinRowSpacing = FMath::Clamp(FMath::Abs(VariableCenter.Y - PureCenter.Y) / (2.0f * Scale), 24.0f, 180.0f);

		RuleSet.EditorCanvasRoleCenters.Reset();
		for (const TPair<ENodeRole, FVector2D>& Pair : RoleCenters)
		{
			if (Pair.Key != ENodeRole::Unknown)
			{
				RuleSet.EditorCanvasRoleCenters.Add(Pair.Key, Pair.Value);
			}
		}

		const FString UpdatedJson = FRuleSetJson::ExportString(RuleSet);
		if (RuleSetChangedDelegate.IsBound())
		{
			RuleSetChangedDelegate.Execute(UpdatedJson);
		}
	}

	BlueprintHelper::GraphLayout::FRuleSet RuleSet;
	TMap<BlueprintHelper::GraphLayout::ENodeRole, FVector2D> RoleCenters;
	TOptional<BlueprintHelper::GraphLayout::ENodeRole> DraggedRole;
	FVector2D DragOffset = FVector2D::ZeroVector;
	FBlueprintHelperLayoutRuleCanvasChanged RuleSetChangedDelegate;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SBlueprintHelperLayoutRuleEditor::Construct(const FArguments& InArgs)
{
		LayoutRuleEditorSettings = FBlueprintHelperUiSettingsResolver::LoadLayoutRuleEditorSettings();
	SettingsRuleId = LayoutRuleEditorSettings.DefaultRuleId;
	SettingsDisplayName = LayoutRuleEditorSettings.DefaultRuleDisplayName;
	SettingsExecColumnSpacing = LayoutRuleEditorSettings.ExecColumnSpacing;
	SettingsExecRowSpacing = LayoutRuleEditorSettings.ExecRowSpacing;
	SettingsBranchRowSpacing = LayoutRuleEditorSettings.BranchRowSpacing;
	SettingsPureInputOffsetX = LayoutRuleEditorSettings.PureInputOffsetX;
	SettingsVariableInputOffsetX = LayoutRuleEditorSettings.VariableInputOffsetX;
	SettingsInputPinRowSpacing = LayoutRuleEditorSettings.InputPinRowSpacing;
	SettingsMaxMillisecondsPerFrame = LayoutRuleEditorSettings.MaxMillisecondsPerFrame;
	SettingsMaxNodesPerFrame = LayoutRuleEditorSettings.MaxNodesPerFrame;
	bSettingsMoveGeneratedNodes = LayoutRuleEditorSettings.bMoveGeneratedNodes;
	bSettingsMoveExistingNodes = LayoutRuleEditorSettings.bMoveExistingNodes;
	bSettingsMarkDirtyAfterApply = LayoutRuleEditorSettings.bMarkDirtyAfterApply;
	bSettingsSaveAfterApply = LayoutRuleEditorSettings.bSaveAfterApply;RuleSetJson = InArgs._InitialRuleSetJson;
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

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.58f)
		[
			SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 6.0f)
		[
			SNew(SBorder)
			.Padding(1.0f)
			[
				SAssignNew(RuleCanvas, SBlueprintHelperLayoutRuleCanvas)
				.LayoutRuleEditorSettings(LayoutRuleEditorSettings)
				.OnRuleSetChanged(FBlueprintHelperLayoutRuleCanvasChanged::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::HandleCanvasRuleSetChanged))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SWrapBox)
			.UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleEventEntry", "EventEntry"), FLinearColor(0.42f, 0.2f, 0.78f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleExecNode", "ExecNode"), FLinearColor(0.85f, 0.1f, 0.08f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleBranchControl", "BranchControl"), FLinearColor(0.88f, 0.45f, 0.08f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RolePureFunction", "PureFunction"), FLinearColor(0.1f, 0.65f, 0.25f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleOperatorOrCompare", "OperatorOrCompare"), FLinearColor(0.42f, 0.78f, 0.1f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleVariableInput", "VariableInput"), FLinearColor(0.0f, 0.5f, 0.85f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleAsyncNode", "AsyncNode"), FLinearColor(0.0f, 0.68f, 0.78f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleDelegateNode", "DelegateNode"), FLinearColor(0.78f, 0.66f, 0.08f, 1.0f))
			]
			+ SWrapBox::Slot().Padding(0.0f, 0.0f, 6.0f, 6.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildRoleChip(LOCTEXT("RoleComment", "Comment"), FLinearColor(0.22f, 0.22f, 0.22f, 1.0f))
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
					LOCTEXT("ImportJson", "Import JSON"),
					LOCTEXT("ImportJsonTooltip", "通过已绑定的配置入口导入 RuleSet JSON；未绑定时从默认 Saved 文件读取。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnImportJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("ExportJson", "Export JSON"),
					LOCTEXT("ExportJsonTooltip", "通过已绑定的配置入口导出当前 RuleSet JSON；未绑定时写入默认 Saved 文件。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnExportJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("CopyJson", "Copy JSON"),
					LOCTEXT("CopyJsonTooltip", "将当前 RuleSet JSON 文本复制到剪贴板。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnCopyJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("PasteJson", "Paste JSON"),
					LOCTEXT("PasteJsonTooltip", "用剪贴板内容替换当前 RuleSet JSON 文本。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnPasteJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("Validate", "Validate"),
					LOCTEXT("ValidateTooltip", "校验当前 RuleSet JSON；已绑定的 GraphLayout 校验器会提供 schema 级检查。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnValidateClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("ResetDefault", "Reset to Default"),
					LOCTEXT("ResetDefaultTooltip", "用已配置的默认 RuleSet JSON 替换当前文本。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnResetToDefaultClicked))
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
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

	RefreshCanvasFromJson();
	RefreshSettingsFromJson();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildSettingsPanel()
{
	using namespace BlueprintHelperLayoutRuleEditorLocal;

	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsPanelTitle", "Rule Settings"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsRuleHeader", "Rule"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildTextSettingRow(
				LOCTEXT("RuleIdLabel", "ID"),
				LOCTEXT("RuleIdTooltip", "布局规则集的稳定标识。"),
				[this]() { return SettingsRuleId; },
				[this](const FText& NewValue) { HandleTextSettingCommitted(RuleId, NewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildTextSettingRow(
				LOCTEXT("DisplayNameLabel", "Display name"),
				LOCTEXT("DisplayNameTooltip", "布局规则集在界面中显示的名称。"),
				[this]() { return SettingsDisplayName; },
				[this](const FText& NewValue) { HandleTextSettingCommitted(DisplayName, NewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsSpacingHeader", "Spacing"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("ExecColumnSpacingLabel", "Exec column"),
				LOCTEXT("ExecColumnSpacingTooltip", "执行节点之间的水平间距。"),
				[this]() { return SettingsExecColumnSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(ExecColumnSpacing, NewValue); },
				120.0f,
				1200.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("ExecRowSpacingLabel", "Exec row"),
				LOCTEXT("ExecRowSpacingTooltip", "执行链路之间的垂直间距。"),
				[this]() { return SettingsExecRowSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(ExecRowSpacing, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("BranchRowSpacingLabel", "Branch row"),
				LOCTEXT("BranchRowSpacingTooltip", "分支链路使用的垂直间距。"),
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
				LOCTEXT("PureInputOffsetLabel", "Pure offset"),
				LOCTEXT("PureInputOffsetTooltip", "纯函数和运算/比较输入节点的左侧偏移。"),
				[this]() { return SettingsPureInputOffsetX; },
				[this](float NewValue) { HandleFloatSettingChanged(PureInputOffsetX, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("VariableInputOffsetLabel", "Variable offset"),
				LOCTEXT("VariableInputOffsetTooltip", "变量输入节点的左侧偏移。"),
				[this]() { return SettingsVariableInputOffsetX; },
				[this](float NewValue) { HandleFloatSettingChanged(VariableInputOffsetX, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("InputPinRowSpacingLabel", "Input pin row"),
				LOCTEXT("InputPinRowSpacingTooltip", "输入引脚行之间的垂直间距。"),
				[this]() { return SettingsInputPinRowSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(InputPinRowSpacing, NewValue); },
				24.0f,
				220.0f,
				2.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsApplyHeader", "Apply"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildIntSettingRow(
				LOCTEXT("MaxNodesPerFrameLabel", "Nodes / frame"),
				LOCTEXT("MaxNodesPerFrameTooltip", "每个编辑器帧最多应用的布局节点移动数量。"),
				[this]() { return SettingsMaxNodesPerFrame; },
				[this](int32 NewValue) { HandleIntSettingChanged(MaxNodesPerFrame, NewValue); },
				1,
				256)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("MaxMillisecondsPerFrameLabel", "MS / frame"),
				LOCTEXT("MaxMillisecondsPerFrameTooltip", "每个编辑器帧最多用于应用布局的时间。"),
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
				LOCTEXT("MoveGeneratedNodesLabel", "Move generated nodes"),
				LOCTEXT("MoveGeneratedNodesTooltip", "允许布局移动本次 Task 生成的节点。"),
				[this]() { return bSettingsMoveGeneratedNodes; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MoveGeneratedNodes, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("MoveExistingNodesLabel", "Move existing nodes"),
				LOCTEXT("MoveExistingNodesTooltip", "允许布局移动图中已有的用户节点。"),
				[this]() { return bSettingsMoveExistingNodes; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MoveExistingNodes, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsPersistenceHeader", "Persistence"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("MarkDirtyAfterApplyLabel", "Mark dirty after apply"),
				LOCTEXT("MarkDirtyAfterApplyTooltip", "布局位置变更后将图所在包标记为 dirty。"),
				[this]() { return bSettingsMarkDirtyAfterApply; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MarkDirtyAfterApply, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("SaveAfterApplyLabel", "Save after apply"),
				LOCTEXT("SaveAfterApplyTooltip", "布局位置变更应用后保存图所在包。"),
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
	RuleSetJson = InRuleSetJson;

	if (RuleSetTextBox.IsValid())
	{
		TGuardValue<bool> UpdatingTextGuard(bUpdatingTextFromCode, true);
		RuleSetTextBox->SetText(FText::FromString(RuleSetJson));
	}

	FString Message;
	bLastValidationPassed = ValidateRuleSetJson(Message);
	SetStatusMessage(Message, bLastValidationPassed);
	RefreshCanvasFromJson();
	RefreshSettingsFromJson();

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
	SetStatusMessage(TEXT("Imported RuleSet JSON."), true);
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
		ExportMessage = bExported ? TEXT("Exported RuleSet JSON.") : TEXT("Export RuleSet JSON failed.");
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
	SetStatusMessage(TEXT("Copied RuleSet JSON to clipboard."), true);
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnPasteJsonClicked()
{
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	if (ClipboardText.TrimStartAndEnd().IsEmpty())
	{
		SetStatusMessage(TEXT("Clipboard does not contain RuleSet JSON."), false);
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
	SetStatusMessage(TEXT("Reset RuleSet JSON to default."), true);
	return FReply::Handled();
}

void SBlueprintHelperLayoutRuleEditor::HandleRuleSetTextChanged(const FText& InText)
{
	if (bUpdatingTextFromCode)
	{
		return;
	}

	RuleSetJson = InText.ToString();
	SetStatusMessage(TEXT("RuleSet JSON edited. Validate before export."), false);
	RefreshCanvasFromJson();
	RefreshSettingsFromJson();

	if (RuleSetJsonChangedDelegate.IsBound())
	{
		RuleSetJsonChangedDelegate.Execute(RuleSetJson);
	}
}

void SBlueprintHelperLayoutRuleEditor::HandleCanvasRuleSetChanged(const FString& InRuleSetJson)
{
	RuleSetJson = InRuleSetJson;

	if (RuleSetTextBox.IsValid())
	{
		TGuardValue<bool> UpdatingTextGuard(bUpdatingTextFromCode, true);
		RuleSetTextBox->SetText(FText::FromString(RuleSetJson));
	}

	FString Message;
	bLastValidationPassed = ValidateRuleSetJson(Message);
	RefreshSettingsFromJson();
	if (bLastValidationPassed)
	{
		bool bSaved = false;
		FString ExportMessage;
		if (ExportJsonDelegate.IsBound())
		{
			bSaved = ExportJsonDelegate.Execute(RuleSetJson);
			ExportMessage = bSaved
				? TEXT("RuleSet updated and saved from canvas.")
				: TEXT("RuleSet updated from canvas, but save failed.");
		}
		else
		{
			bSaved = SaveJsonToDefaultFile(RuleSetJson, ExportMessage);
		}
		SetStatusMessage(ExportMessage, bSaved);
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
	SettingsExecColumnSpacing = ParsedRuleSet.ExecColumnSpacing;
	SettingsExecRowSpacing = ParsedRuleSet.ExecRowSpacing;
	SettingsBranchRowSpacing = ParsedRuleSet.BranchRowSpacing;
	SettingsPureInputOffsetX = ParsedRuleSet.PureInputOffsetX;
	SettingsVariableInputOffsetX = ParsedRuleSet.VariableInputOffsetX;
	SettingsInputPinRowSpacing = ParsedRuleSet.InputPinRowSpacing;
	SettingsMaxNodesPerFrame = ParsedRuleSet.MaxNodesPerFrame;
	SettingsMaxMillisecondsPerFrame = ParsedRuleSet.MaxMillisecondsPerFrame;
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
		SetStatusMessage(TEXT("RuleSet JSON is invalid; settings were not applied."), false);
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
		SetStatusMessage(TEXT("RuleSet JSON is invalid; settings were not applied."), false);
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
	case MaxMillisecondsPerFrame:
		ParsedRuleSet.MaxMillisecondsPerFrame = FMath::Clamp(NewValue, 0.25f, 20.0f);
		break;
	default:
		return;
	}

	CommitSettingsRuleSetJson(BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(ParsedRuleSet));
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
		SetStatusMessage(TEXT("RuleSet JSON is invalid; settings were not applied."), false);
		return;
	}

	using namespace BlueprintHelperLayoutRuleEditorLocal;
	switch (SettingId)
	{
	case MaxNodesPerFrame:
		ParsedRuleSet.MaxNodesPerFrame = FMath::Clamp(NewValue, 1, 256);
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
		SetStatusMessage(TEXT("RuleSet JSON is invalid; settings were not applied."), false);
		return;
	}

	using namespace BlueprintHelperLayoutRuleEditorLocal;
	switch (SettingId)
	{
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
	RuleSetJson = InUpdatedRuleSetJson;

	if (RuleSetTextBox.IsValid())
	{
		TGuardValue<bool> UpdatingTextGuard(bUpdatingTextFromCode, true);
		RuleSetTextBox->SetText(FText::FromString(RuleSetJson));
	}

	RefreshCanvasFromJson();
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
			bSaved ? TEXT("RuleSet updated and saved from settings.") : TEXT("RuleSet updated from settings, but save failed."),
			bSaved);
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
		OutMessage = TEXT("RuleSet JSON is empty.");
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutMessage = FString::Printf(TEXT("RuleSet JSON parse failed: %s"), *Reader->GetErrorMessage());
		return false;
	}

#if BLUEPRINTHELPER_LAYOUT_RULE_EDITOR_HAS_GRAPH_LAYOUT_RULESET_JSON
	const BlueprintHelper::GraphLayout::FValidationResult Validation = BlueprintHelper::GraphLayout::FRuleSetJson::Validate(RootObject);
	if (!Validation.bValid)
	{
		OutMessage = Validation.Errors.Num() > 0
			? FString::Join(Validation.Errors, TEXT(" "))
			: TEXT("RuleSet JSON failed GraphLayout validation.");
		return false;
	}

	OutMessage = Validation.Warnings.Num() > 0
		? FString::Printf(TEXT("RuleSet JSON is valid. Warnings: %s"), *FString::Join(Validation.Warnings, TEXT(" ")))
		: TEXT("RuleSet JSON is valid.");
	return true;
#else
	OutMessage = TEXT("RuleSet JSON is valid JSON. Schema-level GraphLayout validation is not bound.");
	return true;
#endif
}

void SBlueprintHelperLayoutRuleEditor::RefreshCanvasFromJson()
{
	if (RuleCanvas.IsValid())
	{
#if BLUEPRINTHELPER_LAYOUT_RULE_EDITOR_HAS_GRAPH_LAYOUT_RULESET_JSON
		BlueprintHelper::GraphLayout::FRuleSet ParsedRuleSet;
		BlueprintHelper::GraphLayout::FValidationResult Validation;
		if (!BlueprintHelper::GraphLayout::FRuleSetJson::ImportString(RuleSetJson, ParsedRuleSet, Validation))
		{
			return;
		}
#endif
		RuleCanvas->SetRuleSetJson(RuleSetJson);
	}
}

FString SBlueprintHelperLayoutRuleEditor::LoadJsonFromDefaultFile(FString& OutMessage) const
{
	const FString DefaultJsonFilePath = GetDefaultJsonFilePath();
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *DefaultJsonFilePath))
	{
		OutMessage = FString::Printf(TEXT("Import failed. No config hook is bound and default file was not readable: %s"), *DefaultJsonFilePath);
		return TEXT("");
	}

	OutMessage = FString::Printf(TEXT("Imported RuleSet JSON from %s"), *DefaultJsonFilePath);
	return JsonText;
}

bool SBlueprintHelperLayoutRuleEditor::SaveJsonToDefaultFile(const FString& JsonText, FString& OutMessage) const
{
	const FString DefaultJsonFilePath = GetDefaultJsonFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(DefaultJsonFilePath), true);

	if (!FFileHelper::SaveStringToFile(JsonText, *DefaultJsonFilePath))
	{
		OutMessage = FString::Printf(TEXT("Export failed. No config hook is bound and default file was not writable: %s"), *DefaultJsonFilePath);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Exported RuleSet JSON to %s"), *DefaultJsonFilePath);
	return true;
}

FString SBlueprintHelperLayoutRuleEditor::GetDefaultJsonFilePath() const
{
	return FBlueprintHelperProjectConfigPaths::GetGraphLayoutRulesPath();
}

#undef LOCTEXT_NAMESPACE
