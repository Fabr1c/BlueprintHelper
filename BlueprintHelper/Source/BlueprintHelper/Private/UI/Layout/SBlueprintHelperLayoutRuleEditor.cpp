// This Project Is Made By Fabric

#include "UI/Layout/SBlueprintHelperLayoutRuleEditor.h"

#include "EdGraph/EdGraph.h"
#include "GraphEditor.h"
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
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewMaterializer.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"
#include "UI/BlueprintHelperUiSettingsResolver.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SNullWidget.h"
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

	float ClampMillisecondsPerFrame(float Value)
	{
		return FMath::Clamp(Value, MinMillisecondsPerFrame, MaxMillisecondsPerFrameSetting);
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
		FText Tooltip;
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

	void SetSemanticScene(BlueprintHelper::GraphLayout::ESemanticScene InScene)
	{
		if (CurrentScene == InScene)
		{
			return;
		}

		ExportCanvasToRuleSet();
		CurrentScene = InScene;
		UpdateHoveredRole({});
		BuildCanvasFromRuleSet();
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	BlueprintHelper::GraphLayout::ESemanticScene GetCurrentScene() const
	{
		return CurrentScene;
	}

	void CommitCurrentScene()
	{
		ExportCanvasToRuleSet();
	}

	void AlignExecRowToEntry()
	{
		using namespace BlueprintHelper::GraphLayout;
		if (!RoleCenters.Contains(ENodeRole::EventEntry) || !RoleCenters.Contains(ENodeRole::ExecNode))
		{
			return;
		}

		FVector2D ExecCenter = RoleCenters.FindRef(ENodeRole::ExecNode);
		ExecCenter.Y = RoleCenters.FindRef(ENodeRole::EventEntry).Y;
		RoleCenters.Add(ENodeRole::ExecNode, ExecCenter);
		RuleSet.bAlignExecNodesHorizontally = true;
		ExportCanvasToRuleSet();
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

		if (const BlueprintHelper::GraphLayout::FSemanticSceneDefinition* Definition = GetCurrentSceneDefinition())
		{
			for (const BlueprintHelper::GraphLayout::FSemanticSceneEdgeDefinition& Edge : Definition->Edges)
			{
				DrawRelationship(OutDrawElements, AllottedGeometry, LayerId + 1, Edge);
			}
		}

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

		const FText FooterText = BuildFooterText();
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
		UpdateHoveredRole(HitRole);
		DragOffset = LocalPos - RoleCenters.FindRef(DraggedRole.GetValue());
		return FReply::Handled().CaptureMouse(AsShared());
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		if (!DraggedRole.IsSet() || !HasMouseCapture())
		{
			UpdateHoveredRole(HitTestRole(LocalPos));
			return FReply::Unhandled();
		}

		UpdateHoveredRole({});
		FVector2D NewCenter = LocalPos - DragOffset;
		const FVector2D CanvasSize = MyGeometry.GetLocalSize();
		NewCenter.X = FMath::Clamp(NewCenter.X, 40.0f, FMath::Max(40.0f, CanvasSize.X - 40.0f));
		NewCenter.Y = FMath::Clamp(NewCenter.Y, 40.0f, FMath::Max(40.0f, CanvasSize.Y - 40.0f));
		RoleCenters.Add(DraggedRole.GetValue(), NewCenter);
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !DraggedRole.IsSet())
		{
			return FReply::Unhandled();
		}

		DraggedRole.Reset();
		UpdateHoveredRole(HitTestRole(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition())));
		ExportCanvasToRuleSet();
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		UpdateHoveredRole({});
		SLeafWidget::OnMouseLeave(MouseEvent);
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

	const BlueprintHelper::GraphLayout::FSemanticSceneDefinition* GetCurrentSceneDefinition() const
	{
		return BlueprintHelper::GraphLayout::FSemanticSceneCatalog::FindScene(CurrentScene);
	}

	const BlueprintHelper::GraphLayout::FSemanticSceneNodeDefinition* FindCurrentSceneNodeDefinition(
		const BlueprintHelper::GraphLayout::ENodeRole Role) const
	{
		const BlueprintHelper::GraphLayout::FSemanticSceneDefinition* Definition = GetCurrentSceneDefinition();
		if (!Definition)
		{
			return nullptr;
		}

		for (const BlueprintHelper::GraphLayout::FSemanticSceneNodeDefinition& Node : Definition->Nodes)
		{
			if (Node.Role == Role)
			{
				return &Node;
			}
		}

		return nullptr;
	}

	TArray<BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode> BuildRoleNodes() const
	{
		TArray<BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode> Nodes;
		const BlueprintHelper::GraphLayout::FSemanticSceneDefinition* Definition = GetCurrentSceneDefinition();
		if (!Definition)
		{
			return Nodes;
		}

		Nodes.Reserve(Definition->Nodes.Num());
		for (const BlueprintHelper::GraphLayout::FSemanticSceneNodeDefinition& NodeDefinition : Definition->Nodes)
		{
			BlueprintHelperLayoutRuleEditorLocal::FCanvasRoleNode Node;
			Node.Role = NodeDefinition.Role;
			Node.Label = NodeDefinition.Label;
			Node.Tooltip = NodeDefinition.TooltipZh;
			Node.Center = RoleCenters.FindRef(NodeDefinition.Role);
			Node.bDraggable = NodeDefinition.bDraggable;
			Nodes.Add(Node);
		}

		return Nodes;
	}

	void BuildCanvasFromRuleSet()
	{
		RoleCenters.Reset();
		const BlueprintHelper::GraphLayout::FSemanticSceneDefinition* Definition = GetCurrentSceneDefinition();
		if (!Definition)
		{
			return;
		}

		const BlueprintHelper::GraphLayout::FEditorCanvasSceneState SceneState =
			BlueprintHelper::GraphLayout::FSemanticSceneAdapter::ResolveSceneState(RuleSet, CurrentScene);
		for (const BlueprintHelper::GraphLayout::FSemanticSceneNodeDefinition& NodeDefinition : Definition->Nodes)
		{
			RoleCenters.Add(NodeDefinition.Role, SceneState.RoleCenters.FindRef(NodeDefinition.Role));
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

	void UpdateHoveredRole(const TOptional<BlueprintHelper::GraphLayout::ENodeRole> InHoveredRole)
	{
		if (HoveredRole == InHoveredRole)
		{
			return;
		}

		HoveredRole = InHoveredRole;
		if (!HoveredRole.IsSet())
		{
			SetToolTip(TSharedPtr<IToolTip>());
			return;
		}

		const BlueprintHelper::GraphLayout::FSemanticSceneNodeDefinition* NodeDefinition =
			FindCurrentSceneNodeDefinition(HoveredRole.GetValue());
		if (!NodeDefinition || NodeDefinition->TooltipZh.IsEmpty())
		{
			SetToolTip(TSharedPtr<IToolTip>());
			return;
		}

		SetToolTip(
			SNew(SToolTip)
			[
				SNew(STextBlock)
				.Text(NodeDefinition->TooltipZh)
				.WrapTextAt(280.0f)
			]);
	}

	FVector2D GetNodeBoundaryPoint(const FVector2D& Center, const FVector2D& Direction) const
	{
		const FVector2D HalfSize = LayoutRuleEditorSettings.NodeSize * 0.5f;
		const float DistanceToVerticalEdge = FMath::Abs(Direction.X) > KINDA_SMALL_NUMBER
			? HalfSize.X / FMath::Abs(Direction.X)
			: TNumericLimits<float>::Max();
		const float DistanceToHorizontalEdge = FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER
			? HalfSize.Y / FMath::Abs(Direction.Y)
			: TNumericLimits<float>::Max();
		return Center + Direction * FMath::Min(DistanceToVerticalEdge, DistanceToHorizontalEdge);
	}

	void DrawRelationship(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& AllottedGeometry,
		int32 LayerId,
		const BlueprintHelper::GraphLayout::FSemanticSceneEdgeDefinition& Edge) const
	{
		const FVector2D From = RoleCenters.FindRef(Edge.FromRole);
		const FVector2D To = RoleCenters.FindRef(Edge.ToRole);
		if (From.IsZero() || To.IsZero())
		{
			return;
		}

		const FVector2D Direction = (To - From).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			return;
		}

		const FVector2D Start = GetNodeBoundaryPoint(From, Direction);
		const FVector2D Tip = GetNodeBoundaryPoint(To, -Direction);

		TArray<FVector2D> Points;
		Points.Add(Start);
		Points.Add(Tip);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Edge.Color,
			true,
			2.0f);

		const FVector2D Perpendicular(-Direction.Y, Direction.X);
		const float ArrowLength = 12.0f;
		const float ArrowWidth = 5.0f;
		TArray<FVector2D> ArrowA;
		ArrowA.Add(Tip);
		ArrowA.Add(Tip - Direction * ArrowLength + Perpendicular * ArrowWidth);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			ArrowA,
			ESlateDrawEffect::None,
			Edge.Color,
			true,
			2.0f);

		TArray<FVector2D> ArrowB;
		ArrowB.Add(Tip);
		ArrowB.Add(Tip - Direction * ArrowLength - Perpendicular * ArrowWidth);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			ArrowB,
			ESlateDrawEffect::None,
			Edge.Color,
			true,
			2.0f);
	}

	void ExportCanvasToRuleSet()
	{
		BlueprintHelper::GraphLayout::FEditorCanvasSceneState SceneState;
		const BlueprintHelper::GraphLayout::FSemanticSceneDefinition* Definition = GetCurrentSceneDefinition();
		if (Definition)
		{
			for (const BlueprintHelper::GraphLayout::FSemanticSceneNodeDefinition& NodeDefinition : Definition->Nodes)
			{
				if (NodeDefinition.Role != BlueprintHelper::GraphLayout::ENodeRole::Unknown)
				{
					SceneState.RoleCenters.Add(NodeDefinition.Role, RoleCenters.FindRef(NodeDefinition.Role));
				}
			}
			BlueprintHelper::GraphLayout::FSemanticSceneAdapter::ApplySceneStateToRuleSet(
				CurrentScene,
				SceneState,
				RuleSet,
				LayoutRuleEditorSettings.CanvasRuleScale);
		}

		const FString UpdatedJson = BlueprintHelper::GraphLayout::FRuleSetJson::ExportString(RuleSet);
		if (RuleSetChangedDelegate.IsBound())
		{
			RuleSetChangedDelegate.Execute(UpdatedJson);
		}
	}

	FText BuildFooterText() const
	{
		switch (CurrentScene)
		{
		case BlueprintHelper::GraphLayout::ESemanticScene::LinearExecChain:
			return FText::Format(
				LOCTEXT("LinearCanvasFooter", "Linear Exec | align {0} | column {1}px"),
				RuleSet.bAlignExecNodesHorizontally ? LOCTEXT("LinearAlignOn", "on") : LOCTEXT("LinearAlignOff", "off"),
				FText::AsNumber(FMath::RoundToInt(RuleSet.ExecColumnSpacing)));
		case BlueprintHelper::GraphLayout::ESemanticScene::PureDataSubgraph:
			return FText::Format(
				LOCTEXT("PureDataCanvasFooter", "Pure Data Subgraph | pure offset {0}px | leaf offset {1}px | pin row {2}px"),
				FText::AsNumber(FMath::RoundToInt(RuleSet.PureInputOffsetX)),
				FText::AsNumber(FMath::RoundToInt(RuleSet.VariableInputOffsetX)),
				FText::AsNumber(FMath::RoundToInt(RuleSet.InputPinRowSpacing)));
		case BlueprintHelper::GraphLayout::ESemanticScene::NodeInputCluster:
			return FText::Format(
				LOCTEXT("InputClusterCanvasFooter", "Node Input Cluster | data pad {0}/{1}px | pin row {2}px"),
				FText::AsNumber(FMath::RoundToInt(RuleSet.DataClusterPaddingX)),
				FText::AsNumber(FMath::RoundToInt(RuleSet.DataClusterPaddingY)),
				FText::AsNumber(FMath::RoundToInt(RuleSet.InputPinRowSpacing)));
		case BlueprintHelper::GraphLayout::ESemanticScene::MultiExecOutput:
			return FText::Format(
				LOCTEXT("MultiExecCanvasFooter", "Multi Exec Output | align {0} | branch row {1}px | padding {2}px"),
				RuleSet.bAlignExecNodesHorizontally ? LOCTEXT("MultiExecAlignOn", "on") : LOCTEXT("MultiExecAlignOff", "off"),
				FText::AsNumber(FMath::RoundToInt(RuleSet.BranchRowSpacing)),
				FText::AsNumber(FMath::RoundToInt(RuleSet.BranchRowPaddingY)));
		case BlueprintHelper::GraphLayout::ESemanticScene::Occupancy:
			return FText::Format(
				LOCTEXT("OccupancyCanvasFooter", "Occupancy | padding {0}/{1}px | step {2}px | attempts {3}"),
				FText::AsNumber(FMath::RoundToInt(RuleSet.CollisionPaddingX)),
				FText::AsNumber(FMath::RoundToInt(RuleSet.CollisionPaddingY)),
				FText::AsNumber(FMath::RoundToInt(RuleSet.CollisionStepY)),
				FText::AsNumber(RuleSet.MaxCollisionAttempts));
		default:
			return FText::GetEmpty();
		}
	}

	BlueprintHelper::GraphLayout::FRuleSet RuleSet;
	TMap<BlueprintHelper::GraphLayout::ENodeRole, FVector2D> RoleCenters;
	TOptional<BlueprintHelper::GraphLayout::ENodeRole> DraggedRole;
	TOptional<BlueprintHelper::GraphLayout::ENodeRole> HoveredRole;
	BlueprintHelper::GraphLayout::ESemanticScene CurrentScene =
		BlueprintHelper::GraphLayout::ESemanticScene::LinearExecChain;
	FVector2D DragOffset = FVector2D::ZeroVector;
	FBlueprintHelperLayoutRuleCanvasChanged RuleSetChangedDelegate;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SBlueprintHelperLayoutRuleEditor::Construct(const FArguments& InArgs)
{
	LayoutRuleEditorSettings = FBlueprintHelperUiSettingsResolver::LoadLayoutRuleEditorSettings();
	LayoutRuleEditorSettings.MaxMillisecondsPerFrame =
		BlueprintHelperLayoutRuleEditorLocal::ClampMillisecondsPerFrame(LayoutRuleEditorSettings.MaxMillisecondsPerFrame);
	const BlueprintHelper::GraphLayout::FRuleSet DefaultRuleSet;

	SettingsRuleId = LayoutRuleEditorSettings.DefaultRuleId;
	SettingsDisplayName = LayoutRuleEditorSettings.DefaultRuleDisplayName;
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
	PreviewState = EPreviewState::Edit;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.58f)
		[
			SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f)
		[
			SAssignNew(WorkspaceBox, SBox)
			[
				BuildEditWorkspace()
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
					LOCTEXT("ImportJsonTooltip", "通过已绑定的配置入口导入 RuleSet JSON；未绑定时从默认配置文件读取。"),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnImportJsonClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("ExportJson", "Export JSON"),
					LOCTEXT("ExportJsonTooltip", "通过已绑定的配置入口导出当前 RuleSet JSON；未绑定时写入默认配置文件。"),
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

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildEditWorkspace()
{
	if (!RuleCanvas.IsValid())
	{
		RuleCanvas = SNew(SBlueprintHelperLayoutRuleCanvas)
			.LayoutRuleEditorSettings(LayoutRuleEditorSettings)
			.OnRuleSetChanged(FBlueprintHelperLayoutRuleCanvasChanged::CreateSP(
				this,
				&SBlueprintHelperLayoutRuleEditor::HandleCanvasRuleSetChanged));
		RuleCanvas->SetRuleSetJson(RuleSetJson);
	}

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
			SNew(SBorder)
			.Padding(1.0f)
			[
				RuleCanvas.ToSharedRef()
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
		];
}

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildSceneToolbar()
{
	if (PreviewState != EPreviewState::Edit)
	{
		return SNullWidget::NullWidget;
	}

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
						LOCTEXT("CanvasSceneTooltip", "Configure the draggable {0} semantic scene."),
						SceneDefinition.DisplayName),
					FOnClicked::CreateLambda([this, Scene = SceneDefinition.Scene]()
					{
						if (RuleCanvas.IsValid())
						{
							RuleCanvas->SetSemanticScene(Scene);
						}
						return FReply::Handled();
					}))
			];
	}
	SceneToolbar->AddSlot()
		.Padding(0.0f, 0.0f, 6.0f, 6.0f)
		[
			BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
				LOCTEXT("AlignExecRow", "Align Exec Row"),
				LOCTEXT("AlignExecRowTooltip", "Align the linear exec node to the ExecEntry baseline."),
				FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnAlignExecRowClicked))
		];
	SceneToolbar->AddSlot()
		.Padding(0.0f, 0.0f, 6.0f, 6.0f)
		[
			BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
				LOCTEXT("PreviewLayoutRule", "Preview"),
				LOCTEXT("PreviewLayoutRuleTooltip", "Build a read-only native Blueprint graph preview for the active semantic scene."),
				FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnPreviewClicked))
		];
	return SceneToolbar;
}

TSharedRef<SWidget> SBlueprintHelperLayoutRuleEditor::BuildPreviewWorkspace()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				BlueprintHelperLayoutRuleEditorLocal::BuildToolbarButton(
					LOCTEXT("ReturnToEdit", "Return to Edit"),
					LOCTEXT("ReturnToEditTooltip", "Cancel the active preview and restore the draggable rule canvas."),
					FOnClicked::CreateSP(this, &SBlueprintHelperLayoutRuleEditor::OnReturnToEditClicked))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(PreviewStatusMessage))
				.AutoWrapText(true)
				.ColorAndOpacity(PreviewState == EPreviewState::PreviewError
					? BlueprintHelperLayoutRuleEditorLocal::InvalidStatusColor
					: FLinearColor(0.78f, 0.78f, 0.78f, 1.0f))
			]
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
	if (PreviewState == EPreviewState::PreviewReady && PreviewGraphEditor.IsValid())
	{
		return SNew(SBorder)
			.Padding(1.0f)
			[
				PreviewGraphEditor.ToSharedRef()
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
				LOCTEXT("RuleIdTooltip", "Stable identifier for this GraphLayout RuleSet."),
				[this]() { return SettingsRuleId; },
				[this](const FText& NewValue) { HandleTextSettingCommitted(RuleId, NewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildTextSettingRow(
				LOCTEXT("DisplayNameLabel", "Display name"),
				LOCTEXT("DisplayNameTooltip", "User-facing name shown for this GraphLayout RuleSet."),
				[this]() { return SettingsDisplayName; },
				[this](const FText& NewValue) { HandleTextSettingCommitted(DisplayName, NewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsLinearExecHeader", "Linear Exec"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("AlignExecNodesHorizontallyLabel", "Exec horizontal alignment"),
				LOCTEXT("AlignExecNodesHorizontallyTooltip", "Enable RuleSet-driven horizontal exec alignment for branch-capable patterns."),
				[this]() { return bSettingsAlignExecNodesHorizontally; },
				[this](bool bNewValue) { HandleBoolSettingChanged(AlignExecNodesHorizontally, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("ExecColumnSpacingLabel", "Exec column spacing"),
				LOCTEXT("ExecColumnSpacingTooltip", "Horizontal distance between adjacent exec columns."),
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
				LOCTEXT("ExecRowSpacingLabel", "Exec row spacing"),
				LOCTEXT("ExecRowSpacingTooltip", "Vertical distance between linear exec rows."),
				[this]() { return SettingsExecRowSpacing; },
				[this](float NewValue) { HandleFloatSettingChanged(ExecRowSpacing, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsPureDataHeader", "Pure Data"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("UsePureDataSubgraphLayoutLabel", "Pure data subgraph layout"),
				LOCTEXT("UsePureDataSubgraphLayoutTooltip", "Enable RuleSet-driven pure-data subgraph measurement and placement."),
				[this]() { return bSettingsUsePureDataSubgraphLayout; },
				[this](bool bNewValue) { HandleBoolSettingChanged(UsePureDataSubgraphLayout, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("DataClusterPaddingXLabel", "Data cluster padding X"),
				LOCTEXT("DataClusterPaddingXTooltip", "Extra horizontal padding reserved around measured pure-data envelopes."),
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
				LOCTEXT("DataClusterPaddingYLabel", "Data cluster padding Y"),
				LOCTEXT("DataClusterPaddingYTooltip", "Extra vertical padding reserved around measured pure-data envelopes."),
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
				LOCTEXT("InputPinRowSpacingLabel", "Input pin row spacing"),
				LOCTEXT("InputPinRowSpacingTooltip", "Vertical spacing between consumer input-pin rows inside pattern data placement."),
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
				LOCTEXT("PureInputOffsetLabel", "Pure input offset X"),
				LOCTEXT("PureInputOffsetTooltip", "Horizontal offset used for pure-function and operator inputs."),
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
				LOCTEXT("VariableInputOffsetLabel", "Variable input offset X"),
				LOCTEXT("VariableInputOffsetTooltip", "Horizontal offset used for variable input nodes."),
				[this]() { return SettingsVariableInputOffsetX; },
				[this](float NewValue) { HandleFloatSettingChanged(VariableInputOffsetX, NewValue); },
				80.0f,
				900.0f,
				10.0f)
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsBranchHeader", "Branch"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("BranchRowSpacingLabel", "Branch row spacing"),
				LOCTEXT("BranchRowSpacingTooltip", "Vertical spacing used between branch output rows."),
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
				LOCTEXT("BranchRowPaddingYLabel", "Branch row padding Y"),
				LOCTEXT("BranchRowPaddingYTooltip", "Extra vertical padding inserted between allocated branch rows after row-height budgeting."),
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
				LOCTEXT("UsePatternRowHeightBudgetLabel", "Pattern row height budget"),
				LOCTEXT("UsePatternRowHeightBudgetTooltip", "Allow measured data-cluster height budgets to expand allocated exec row baselines."),
				[this]() { return bSettingsUsePatternRowHeightBudget; },
				[this](bool bNewValue) { HandleBoolSettingChanged(UsePatternRowHeightBudget, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSettingsSectionHeader(LOCTEXT("SettingsOccupancyHeader", "Occupancy"))
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildFloatSettingRow(
				LOCTEXT("CollisionPaddingXLabel", "Collision padding X"),
				LOCTEXT("CollisionPaddingXTooltip", "Extra horizontal padding reserved while resolving overlap."),
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
				LOCTEXT("CollisionPaddingYLabel", "Collision padding Y"),
				LOCTEXT("CollisionPaddingYTooltip", "Extra vertical padding reserved while resolving overlap."),
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
				LOCTEXT("CollisionStepYLabel", "Collision step Y"),
				LOCTEXT("CollisionStepYTooltip", "Vertical search increment used when resolving overlap."),
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
				LOCTEXT("MaxCollisionAttemptsLabel", "Max collision attempts"),
				LOCTEXT("MaxCollisionAttemptsTooltip", "Maximum number of overlap-resolution attempts before the solver gives up on a candidate position."),
				[this]() { return SettingsMaxCollisionAttempts; },
				[this](int32 NewValue) { HandleIntSettingChanged(MaxCollisionAttempts, NewValue); },
				1,
				256)
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
				LOCTEXT("MaxNodesPerFrameTooltip", "Maximum node moves applied in one editor frame."),
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
				LOCTEXT("MaxMillisecondsPerFrameTooltip", "Maximum time budget per frame while applying layout movement."),
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
				LOCTEXT("MoveGeneratedNodesTooltip", "Allow layout application to move nodes generated by the current task."),
				[this]() { return bSettingsMoveGeneratedNodes; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MoveGeneratedNodes, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("MoveExistingNodesLabel", "Move existing nodes"),
				LOCTEXT("MoveExistingNodesTooltip", "Allow layout application to move pre-existing user nodes."),
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
				LOCTEXT("MarkDirtyAfterApplyTooltip", "Mark the owning package dirty after layout movement is applied."),
				[this]() { return bSettingsMarkDirtyAfterApply; },
				[this](bool bNewValue) { HandleBoolSettingChanged(MarkDirtyAfterApply, bNewValue); })
		]
		+ SScrollBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 0.0f)
		[
			BuildBoolSettingRow(
				LOCTEXT("SaveAfterApplyLabel", "Save after apply"),
				LOCTEXT("SaveAfterApplyTooltip", "Save the owning package after layout movement is applied."),
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

FReply SBlueprintHelperLayoutRuleEditor::OnAlignExecRowClicked()
{
	if (RuleCanvas.IsValid())
	{
		RuleCanvas->SetSemanticScene(BlueprintHelper::GraphLayout::ESemanticScene::LinearExecChain);
		RuleCanvas->AlignExecRowToEntry();
	}
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnPreviewClicked()
{
	if (RuleCanvas.IsValid())
	{
		EditSceneBeforePreview = RuleCanvas->GetCurrentScene();
		RuleCanvas->CommitCurrentScene();
	}

	SetPreviewState(EPreviewState::PreviewLoading, TEXT("Building preview data..."));
	StartPreviewBuild();
	return FReply::Handled();
}

FReply SBlueprintHelperLayoutRuleEditor::OnReturnToEditClicked()
{
	CancelActivePreview();
	SetPreviewState(EPreviewState::Edit, TEXT(""));

	if (RuleCanvas.IsValid() && RuleCanvas->GetCurrentScene() != EditSceneBeforePreview)
	{
		RuleCanvas->SetSemanticScene(EditSceneBeforePreview);
	}

	RefreshCanvasFromJson();
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
		SetStatusMessage(TEXT("RuleSet JSON is invalid; settings were not applied."), false);
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

	WorkspaceBox->SetContent(IsPreviewMode()
		? BuildPreviewWorkspace()
		: BuildEditWorkspace());
}

void SBlueprintHelperLayoutRuleEditor::StartPreviewBuild()
{
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
	PreviewMaterializer = MakeUnique<BlueprintHelper::GraphLayout::FGraphLayoutPreviewMaterializer>();

	BlueprintHelper::GraphLayout::FGraphLayoutPreviewRequest Request;
	Request.Scene = RuleCanvas.IsValid() ? RuleCanvas->GetCurrentScene() : EditSceneBeforePreview;
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
			Result.Error.IsEmpty() ? TEXT("Preview data build failed.") : Result.Error);
		return;
	}

	PendingPreviewBuildResult = Result;
	SetPreviewState(EPreviewState::PreviewMaterializing, TEXT("Materializing preview graph..."));
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
				? TEXT("Preview graph materialization failed.")
				: MaterializerResult.Error);
		return;
	}

	BuildPreviewGraphEditor(MaterializerResult.PreviewGraph.Get());
	SetPreviewState(EPreviewState::PreviewReady, TEXT("Preview ready."));
}

void SBlueprintHelperLayoutRuleEditor::BuildPreviewGraphEditor(UEdGraph* PreviewGraph)
{
	if (!PreviewGraph)
	{
		PreviewGraphEditor.Reset();
		return;
	}

	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("LayoutRulePreviewCornerText", "Preview");
	Appearance.InstructionText = LOCTEXT("LayoutRulePreviewInstruction", "Read-only GraphLayout Preview");
	Appearance.ReadOnlyText = LOCTEXT("LayoutRulePreviewReadOnly", "Preview Only");

	TSharedRef<SGraphEditor> Editor = SAssignNew(PreviewGraphEditor, SGraphEditor)
		.IsEditable(false)
		.DisplayAsReadOnly(false)
		.GraphToEdit(PreviewGraph)
		.Appearance(Appearance)
		.ShowGraphStateOverlay(false);
	Editor->NotifyGraphChanged();
}

void SBlueprintHelperLayoutRuleEditor::CancelActivePreview()
{
	if (PreviewService.IsValid() && ActivePreviewJobId != 0)
	{
		PreviewService->Cancel(ActivePreviewJobId);
	}

	ActivePreviewJobId = 0;
	++ActivePreviewGeneration;
	PendingPreviewBuildResult.Reset();
	PreviewGraphEditor.Reset();
	if (PreviewMaterializer.IsValid())
	{
		PreviewMaterializer->Cancel();
	}
}

bool SBlueprintHelperLayoutRuleEditor::IsPreviewMode() const
{
	return PreviewState != EPreviewState::Edit;
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
