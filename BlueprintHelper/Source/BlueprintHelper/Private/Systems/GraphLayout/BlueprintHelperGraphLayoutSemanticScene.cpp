#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"

#define LOCTEXT_NAMESPACE "BlueprintHelperGraphLayoutSemanticScene"

namespace BlueprintHelper::GraphLayout
{
static FSemanticSceneNodeDefinition MakeSceneNode(
	const ENodeRole Role,
	const FText& Label,
	const FText& TooltipZh,
	const bool bDraggable = true)
{
	FSemanticSceneNodeDefinition Node;
	Node.Role = Role;
	Node.Label = Label;
	Node.TooltipZh = TooltipZh;
	Node.bDraggable = bDraggable;
	return Node;
}

static FSemanticSceneEdgeDefinition MakeSceneEdge(
	const ENodeRole FromRole,
	const ENodeRole ToRole,
	const ESemanticSceneEdgeKind Kind,
	const FLinearColor& Color)
{
	FSemanticSceneEdgeDefinition Edge;
	Edge.FromRole = FromRole;
	Edge.ToRole = ToRole;
	Edge.Kind = Kind;
	Edge.Color = Color;
	return Edge;
}

static FSemanticSceneDefinition MakeLinearExecScene()
{
	FSemanticSceneDefinition Scene;
	Scene.Scene = ESemanticScene::LinearExecChain;
	Scene.DisplayName = LOCTEXT("LinearExecSceneName", "Linear Exec");
	Scene.Nodes = {
		MakeSceneNode(
			ENodeRole::EventEntry,
			LOCTEXT("LinearExecEventLabel", "ExecEntry"),
			LOCTEXT("LinearExecEventTipZh", "执行链入口节点，例如 Event 或 Custom Event。拖动会影响语义预览中的入口基线。")),
		MakeSceneNode(
			ENodeRole::ExecNode,
			LOCTEXT("LinearExecNodeLabel", "Next Exec"),
			LOCTEXT("LinearExecNodeTipZh", "线性执行链中的下一个执行节点。拖动会影响执行列间距和是否保持同一水平线。"))
	};
	Scene.Edges = {
		MakeSceneEdge(ENodeRole::EventEntry, ENodeRole::ExecNode, ESemanticSceneEdgeKind::Exec, FLinearColor(0.85f, 0.18f, 0.14f, 1.0f))
	};
	Scene.DefaultRoleCenters.Add(ENodeRole::EventEntry, FVector2D(92.0f, 126.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::ExecNode, FVector2D(452.0f, 126.0f));
	return Scene;
}

static FSemanticSceneDefinition MakePureDataScene()
{
	FSemanticSceneDefinition Scene;
	Scene.Scene = ESemanticScene::PureDataSubgraph;
	Scene.DisplayName = LOCTEXT("PureDataSceneName", "Pure Data");
	Scene.Nodes = {
		MakeSceneNode(
			ENodeRole::VariableInput,
			LOCTEXT("PureDataVariableLabel", "Data Leaf"),
			LOCTEXT("PureDataVariableTipZh", "数据叶子节点，例如变量 Get、Self 或 Literal。拖动会影响纯数据输入链最左侧节点的位置。")),
		MakeSceneNode(
			ENodeRole::OperatorOrCompare,
			LOCTEXT("PureDataOperatorLabel", "Data Transform"),
			LOCTEXT("PureDataOperatorTipZh", "纯数据变换节点，例如算子、比较或轻量转换。拖动会影响中间变换枢纽的位置。")),
		MakeSceneNode(
			ENodeRole::PureFunction,
			LOCTEXT("PureDataPureLabel", "Data Aggregate"),
			LOCTEXT("PureDataPureTipZh", "纯函数或聚合节点。拖动会影响纯数据子图输出侧节点的位置。"))
	};
	Scene.Edges = {
		MakeSceneEdge(ENodeRole::VariableInput, ENodeRole::OperatorOrCompare, ESemanticSceneEdgeKind::Data, FLinearColor(0.0f, 0.62f, 0.9f, 1.0f)),
		MakeSceneEdge(ENodeRole::OperatorOrCompare, ENodeRole::PureFunction, ESemanticSceneEdgeKind::Data, FLinearColor(0.42f, 0.84f, 0.12f, 1.0f))
	};
	Scene.DefaultRoleCenters.Add(ENodeRole::VariableInput, FVector2D(96.0f, 106.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(356.0f, 150.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::PureFunction, FVector2D(656.0f, 194.0f));
	return Scene;
}

static FSemanticSceneDefinition MakeNodeInputClusterScene()
{
	FSemanticSceneDefinition Scene;
	Scene.Scene = ESemanticScene::NodeInputCluster;
	Scene.DisplayName = LOCTEXT("NodeInputClusterSceneName", "Input Cluster");
	Scene.Nodes = {
		MakeSceneNode(
			ENodeRole::VariableInput,
			LOCTEXT("NodeInputVariableLabel", "Data Leaf"),
			LOCTEXT("NodeInputVariableTipZh", "输入簇最外层的数据叶子节点。拖动会影响消费节点左下侧数据入口的位置。")),
		MakeSceneNode(
			ENodeRole::OperatorOrCompare,
			LOCTEXT("NodeInputOperatorLabel", "Data Transform"),
			LOCTEXT("NodeInputOperatorTipZh", "输入簇中的中间变换节点。拖动会影响数据簇横向展开和包络宽度。")),
		MakeSceneNode(
			ENodeRole::PureFunction,
			LOCTEXT("NodeInputPureLabel", "Input Cluster"),
			LOCTEXT("NodeInputPureTipZh", "靠近消费节点的纯函数或聚合节点。拖动会影响输入簇与消费节点之间的主偏移。")),
		MakeSceneNode(
			ENodeRole::ExecNode,
			LOCTEXT("NodeInputExecLabel", "Consumer"),
			LOCTEXT("NodeInputExecTipZh", "消费输入簇的执行节点。拖动会影响输入簇相对消费节点的距离和行高。"))
	};
	Scene.Edges = {
		MakeSceneEdge(ENodeRole::VariableInput, ENodeRole::OperatorOrCompare, ESemanticSceneEdgeKind::Data, FLinearColor(0.0f, 0.62f, 0.9f, 1.0f)),
		MakeSceneEdge(ENodeRole::OperatorOrCompare, ENodeRole::PureFunction, ESemanticSceneEdgeKind::Data, FLinearColor(0.42f, 0.84f, 0.12f, 1.0f)),
		MakeSceneEdge(ENodeRole::PureFunction, ENodeRole::ExecNode, ESemanticSceneEdgeKind::Data, FLinearColor(0.1f, 0.75f, 0.32f, 1.0f))
	};
	Scene.DefaultRoleCenters.Add(ENodeRole::VariableInput, FVector2D(288.0f, 202.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(248.0f, 158.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::PureFunction, FVector2D(248.0f, 114.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::ExecNode, FVector2D(548.0f, 158.0f));
	return Scene;
}

static FSemanticSceneDefinition MakeMultiExecScene()
{
	FSemanticSceneDefinition Scene;
	Scene.Scene = ESemanticScene::MultiExecOutput;
	Scene.DisplayName = LOCTEXT("MultiExecSceneName", "Multi Exec");
	Scene.Nodes = {
		MakeSceneNode(
			ENodeRole::EventEntry,
			LOCTEXT("MultiExecEventLabel", "Multi Exec"),
			LOCTEXT("MultiExecEventTipZh", "具有多个执行输出的源节点，例如 Sequence 或自定义事件。拖动会影响主执行行和分支行的起点。")),
		MakeSceneNode(
			ENodeRole::ExecNode,
			LOCTEXT("MultiExecPrimaryLabel", "Primary Row"),
			LOCTEXT("MultiExecPrimaryTipZh", "多执行输出场景中的主执行行节点。拖动会影响执行列间距和主行对齐状态。")),
		MakeSceneNode(
			ENodeRole::BranchControl,
			LOCTEXT("MultiExecBranchLabel", "Branch Row"),
			LOCTEXT("MultiExecBranchTipZh", "多执行输出场景中的分支或次执行行节点。拖动会影响分支行间距和额外纵向 padding。"))
	};
	Scene.Edges = {
		MakeSceneEdge(ENodeRole::EventEntry, ENodeRole::ExecNode, ESemanticSceneEdgeKind::Exec, FLinearColor(0.85f, 0.18f, 0.14f, 1.0f)),
		MakeSceneEdge(ENodeRole::EventEntry, ENodeRole::BranchControl, ESemanticSceneEdgeKind::Exec, FLinearColor(0.95f, 0.55f, 0.12f, 1.0f))
	};
	Scene.DefaultRoleCenters.Add(ENodeRole::EventEntry, FVector2D(92.0f, 126.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::ExecNode, FVector2D(452.0f, 126.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::BranchControl, FVector2D(452.0f, 386.0f));
	return Scene;
}

static FSemanticSceneDefinition MakeOccupancyScene()
{
	FSemanticSceneDefinition Scene;
	Scene.Scene = ESemanticScene::Occupancy;
	Scene.DisplayName = LOCTEXT("OccupancySceneName", "Occupancy");
	Scene.Nodes = {
		MakeSceneNode(
			ENodeRole::ExecNode,
			LOCTEXT("OccupancyCandidateLabel", "Candidate"),
			LOCTEXT("OccupancyCandidateTipZh", "待布局的候选节点。拖动会影响与阻挡物的相对基准位置。")),
		MakeSceneNode(
			ENodeRole::Comment,
			LOCTEXT("OccupancyBlockerLabel", "Existing Blocker"),
			LOCTEXT("OccupancyBlockerTipZh", "现有阻挡节点或注释块。拖动会影响碰撞避让的水平和垂直 padding。")),
		MakeSceneNode(
			ENodeRole::AsyncNode,
			LOCTEXT("OccupancyFallbackLabel", "Fallback Row"),
			LOCTEXT("OccupancyFallbackTipZh", "候选节点发生碰撞时使用的后备行。拖动会影响垂直退避步长。"))
	};
	Scene.Edges = {
		MakeSceneEdge(ENodeRole::ExecNode, ENodeRole::Comment, ESemanticSceneEdgeKind::Collision, FLinearColor(0.65f, 0.65f, 0.65f, 1.0f)),
		MakeSceneEdge(ENodeRole::ExecNode, ENodeRole::AsyncNode, ESemanticSceneEdgeKind::Collision, FLinearColor(0.0f, 0.72f, 0.85f, 1.0f))
	};
	Scene.DefaultRoleCenters.Add(ENodeRole::ExecNode, FVector2D(220.0f, 116.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::Comment, FVector2D(280.0f, 156.0f));
	Scene.DefaultRoleCenters.Add(ENodeRole::AsyncNode, FVector2D(280.0f, 220.0f));
	return Scene;
}

static TArray<FSemanticSceneDefinition> BuildSceneDefinitions()
{
	TArray<FSemanticSceneDefinition> Scenes;
	Scenes.Reserve(5);
	Scenes.Add(MakeLinearExecScene());
	Scenes.Add(MakePureDataScene());
	Scenes.Add(MakeNodeInputClusterScene());
	Scenes.Add(MakeMultiExecScene());
	Scenes.Add(MakeOccupancyScene());
	return Scenes;
}

static const TArray<FSemanticSceneDefinition>& GetSceneDefinitions()
{
	static const TArray<FSemanticSceneDefinition> Scenes = BuildSceneDefinitions();
	return Scenes;
}

static FEditorCanvasSceneState MergeStateWithDefaults(const ESemanticScene Scene, const FEditorCanvasSceneState& State)
{
	FEditorCanvasSceneState Result = FSemanticSceneCatalog::BuildDefaultState(Scene);
	for (const TPair<ENodeRole, FVector2D>& Pair : State.RoleCenters)
	{
		if (Pair.Key != ENodeRole::Unknown)
		{
			Result.RoleCenters.Add(Pair.Key, Pair.Value);
		}
	}
	return Result;
}

static float SanitizeScale(const float Scale)
{
	return FMath::Max(Scale, KINDA_SMALL_NUMBER);
}

TArray<FSemanticSceneDefinition> FSemanticSceneCatalog::GetAllScenes()
{
	return GetSceneDefinitions();
}

const FSemanticSceneDefinition* FSemanticSceneCatalog::FindScene(const ESemanticScene Scene)
{
	const TArray<FSemanticSceneDefinition>& Scenes = GetSceneDefinitions();
	for (const FSemanticSceneDefinition& Definition : Scenes)
	{
		if (Definition.Scene == Scene)
		{
			return &Definition;
		}
	}
	return nullptr;
}

FEditorCanvasSceneState FSemanticSceneCatalog::BuildDefaultState(const ESemanticScene Scene)
{
	FEditorCanvasSceneState State;
	const FSemanticSceneDefinition* Definition = FindScene(Scene);
	if (!Definition)
	{
		return State;
	}

	for (const TPair<ENodeRole, FVector2D>& Pair : Definition->DefaultRoleCenters)
	{
		State.RoleCenters.Add(Pair.Key, Pair.Value);
	}
	return State;
}

FEditorCanvasSceneState FSemanticSceneAdapter::ResolveSceneState(const FRuleSet& RuleSet, const ESemanticScene Scene)
{
	const FEditorCanvasSceneState* SavedState = RuleSet.EditorCanvasScenes.Find(Scene);
	if (!SavedState)
	{
		return FSemanticSceneCatalog::BuildDefaultState(Scene);
	}

	return MergeStateWithDefaults(Scene, *SavedState);
}

void FSemanticSceneAdapter::ApplySceneStateToRuleSet(
	const ESemanticScene Scene,
	const FEditorCanvasSceneState& State,
	FRuleSet& RuleSet,
	const float Scale)
{
	const float EffectiveScale = SanitizeScale(Scale);
	const FEditorCanvasSceneState ResolvedState = MergeStateWithDefaults(Scene, State);
	RuleSet.EditorCanvasScenes.Add(Scene, ResolvedState);

	const FVector2D EventCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::EventEntry);
	const FVector2D ExecCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::ExecNode);
	const FVector2D BranchCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::BranchControl);
	const FVector2D PureCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::PureFunction);
	const FVector2D OperatorCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::OperatorOrCompare);
	const FVector2D VariableCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::VariableInput);
	const FVector2D CommentCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::Comment);
	const FVector2D AsyncCenter = ResolvedState.RoleCenters.FindRef(ENodeRole::AsyncNode);

	switch (Scene)
	{
	case ESemanticScene::LinearExecChain:
		if (ExecCenter.X > EventCenter.X)
		{
			RuleSet.ExecColumnSpacing = FMath::Clamp((ExecCenter.X - EventCenter.X) / EffectiveScale, 120.0f, 900.0f);
		}
		RuleSet.bAlignExecNodesHorizontally = FMath::Abs(ExecCenter.Y - EventCenter.Y) <= 8.0f;
		break;

	case ESemanticScene::PureDataSubgraph:
		if (OperatorCenter.X > VariableCenter.X)
		{
			RuleSet.VariableInputOffsetX = FMath::Clamp((OperatorCenter.X - VariableCenter.X) / EffectiveScale, 80.0f, 720.0f);
		}
		if (PureCenter.X > OperatorCenter.X)
		{
			RuleSet.PureInputOffsetX = FMath::Clamp((PureCenter.X - OperatorCenter.X) / EffectiveScale, 80.0f, 720.0f);
		}
		RuleSet.InputPinRowSpacing = FMath::Clamp(
			FMath::Max(FMath::Abs(OperatorCenter.Y - VariableCenter.Y), FMath::Abs(PureCenter.Y - OperatorCenter.Y)) / EffectiveScale,
			24.0f,
			180.0f);
		break;

	case ESemanticScene::NodeInputCluster:
		if (ExecCenter.X > PureCenter.X)
		{
			RuleSet.PureInputOffsetX = FMath::Clamp((ExecCenter.X - PureCenter.X) / EffectiveScale, 80.0f, 720.0f);
		}
		if (ExecCenter.X > VariableCenter.X)
		{
			RuleSet.VariableInputOffsetX = FMath::Clamp((ExecCenter.X - VariableCenter.X) / EffectiveScale, 80.0f, 720.0f);
		}
		RuleSet.InputPinRowSpacing = FMath::Clamp(
			FMath::Max(FMath::Abs(VariableCenter.Y - ExecCenter.Y), FMath::Abs(PureCenter.Y - ExecCenter.Y)) / EffectiveScale,
			24.0f,
			180.0f);
		RuleSet.DataClusterPaddingX = FMath::Clamp(FMath::Abs(OperatorCenter.X - PureCenter.X) / EffectiveScale, 8.0f, 240.0f);
		RuleSet.DataClusterPaddingY = FMath::Clamp(FMath::Abs(VariableCenter.Y - PureCenter.Y) / EffectiveScale, 8.0f, 240.0f);
		break;

	case ESemanticScene::MultiExecOutput:
		if (ExecCenter.X > EventCenter.X)
		{
			RuleSet.ExecColumnSpacing = FMath::Clamp((ExecCenter.X - EventCenter.X) / EffectiveScale, 120.0f, 900.0f);
		}
		RuleSet.bAlignExecNodesHorizontally = FMath::Abs(ExecCenter.Y - EventCenter.Y) <= 8.0f;
		RuleSet.BranchRowSpacing = FMath::Clamp(FMath::Abs(BranchCenter.Y - EventCenter.Y) / EffectiveScale, 80.0f, 640.0f);
		RuleSet.BranchRowPaddingY = FMath::Clamp(FMath::Abs(BranchCenter.Y - ExecCenter.Y) / EffectiveScale, 16.0f, 320.0f);
		break;

	case ESemanticScene::Occupancy:
		RuleSet.CollisionPaddingX = FMath::Clamp(FMath::Abs(CommentCenter.X - ExecCenter.X) / EffectiveScale, 8.0f, 240.0f);
		RuleSet.CollisionPaddingY = FMath::Clamp(FMath::Abs(CommentCenter.Y - ExecCenter.Y) / EffectiveScale, 8.0f, 240.0f);
		RuleSet.CollisionStepY = FMath::Clamp(FMath::Abs(AsyncCenter.Y - CommentCenter.Y) / EffectiveScale, 16.0f, 240.0f);
		break;

	default:
		break;
	}
}
}

#undef LOCTEXT_NAMESPACE
