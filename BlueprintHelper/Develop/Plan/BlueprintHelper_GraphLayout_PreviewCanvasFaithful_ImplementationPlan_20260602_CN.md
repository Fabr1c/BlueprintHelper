# GraphLayout Preview Canvas Faithful Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 LayoutRuleEditor 的 Preview 显示“当前拖拽语义画布规则的真实效果”，包括 role 重叠在 Preview 中真实重叠，以及 Entry 到后续 Exec 节点按 exec pin 水平拉直。

**Architecture:** 新增 preview-only `FGraphLayoutPreviewSemanticProjector`，专门把 `editor_canvas.scenes.<scene>.role_centers` 投影成 Preview `FLayoutPlan`。Runtime `FSolver` 与 GraphLayout 执行路径不改；Preview 不再依赖 solver occupancy 自动避让来重排 role anchor 节点，因为用户要求 role 重叠必须真实显示。`SBlueprintHelperLayoutRuleEditor` 仍只负责切换 Preview 状态和传递 RuleSet JSON，不承载投影算法。

**Tech Stack:** UE 5.6 C++、Slate、SGraphEditor、BlueprintHelper GraphLayout RuleSet/Preview service/materializer、UE Automation tests。

---

## Execution Status - 2026-06-02

- Status: implemented and verified.
- Implementation report: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PreviewCanvasFaithful_Report_20260602_CN.md`
- Debug evidence: `Debug/GraphLayout_PreviewCanvasMapping_Debug_20260602.md`
- Execution correction from the original task text:
  - The implementation does not add or use `ProjectBySolverFallback`.
  - Non-anchor fixed sample nodes use sample-relative placement only.
  - Role anchor nodes are never passed through `FSolver` or occupancy resolution in Preview.

## Non-Negotiable Requirements

- Preview 的目标是“当前拖拽语义画布规则的真实效果”，不是 runtime solver 经过 collision avoidance 后的最终美化结果。
- role 重叠必须完全真实显示：如果两个 role center 在画布上重叠，Preview 中对应语义 anchor 必须重叠，且 native node bounds 必须发生实际相交。
- Entry 到后一个 Exec node 必须按 exec pin anchor 水平拉直，不能只按 node top-left 或 node center 对齐。
- Preview 的判定基准是当前选中的语义页拖拽画布，而不是当前打开的 UEdGraph，也不是当前蓝图实际图表。
- Preview 可以用固定内置 sample graph，但 sample graph 的语义锚点位置必须由当前拖拽 RuleSet/scene 决定。
- 任何 preview-only 补齐节点只能保持 sample graph 内部相对关系，不能通过 solver/occupancy 改写 role anchor 的相对关系。
- 不把 `canvas_desired_size` 引入 runtime `FSolver`。
- 不新增 legacy `editor_canvas.role_centers` 根字段兼容。
- 不在 UI widget 中堆积 preview projection、graph 构建、solver 编排或生命周期逻辑。
- 按 AGENTS.md，本计划执行时不运行 `git add` / `git commit` / `git push`；任务末尾只输出建议提交命令。

## Current Root Cause

- `SBlueprintHelperLayoutRuleCanvas::ExportCanvasToRuleSet()` 已保存 `EditorCanvasScenes[Scene]`，但 `FGraphLayoutPreviewService::BuildPreviewData(...)` 只导入 RuleSet、构建 fixed sample，然后调用 `FSolver::Solve(...)`。
- `FSemanticSceneAdapter::ApplySceneStateToRuleSet(...)` 当前把 role center 差值投影成标量：
  - `ExecColumnSpacing`
  - `BranchRowSpacing`
  - `BranchRowPaddingY`
  - `PureInputOffsetX`
  - `VariableInputOffsetX`
  - `InputPinRowSpacing`
  - `CollisionPaddingX/Y`
  - `CollisionStepY`
- `FSolver::LayoutExecChain(...)` 用 `Target = (Column * ExecColumnSpacing, Row * ExecRowSpacing)` 对齐 top-left，不知道 native exec pin 在节点内部的视觉 offset。
- `FOccupancyResolver::ResolveNearestFreeTarget(...)` 会把重叠目标向下避让，所以 role overlap 会被 Preview 消掉。

## File Structure

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h`
  - 声明 preview-only semantic projection API、anchor offset helper、scene role projection contract。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp`
  - 实现 role center 到 preview placement 的直接投影，不使用 `FOccupancyResolver`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h`
  - 在 `FGraphLayoutPreviewNodeSpec` 上增加 preview anchor metadata，使 sample 明确哪个 node 代表哪个 draggable role。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.cpp`
  - 给 fixed sample nodes 标注 `PreviewAnchorRole`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.cpp`
  - Preview build 使用 `FGraphLayoutPreviewSemanticProjector::Project(...)`，不再直接调用 `FSolver::Solve(...)` 作为主路径。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
  - 添加 preview semantic projection 自动化测试。
- Modify: `Debug/GraphLayout_PreviewCanvasMapping_Debug_20260602.md`
  - 追加实现阶段证据、测试报告路径、E2E 结果。
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PreviewCanvasFaithful_Report_20260602_CN.md`
  - 会话改动报告。

---

## Task 1: RED Tests For Preview Fidelity Bugs

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_PreviewCanvasMapping_Debug_20260602.md`

- [ ] **Step 1: Add failing tests for pin-straightened Entry -> Exec preview**

In `BlueprintHelperGraphLayoutSolverTests.cpp`, after `FBlueprintHelperGraphLayout_PreviewServiceBuildsPureDataResult`, add this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensEntryToFirstExec,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorStraightensEntryToFirstExec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorStraightensEntryToFirstExec::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState LinearScene;
	LinearScene.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(100.0f, 120.0f));
	LinearScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(500.0f, 120.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);
	RuleSet.ExecColumnSpacing = 360.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.CollisionPaddingX = 240.0f;
	RuleSet.CollisionPaddingY = 240.0f;

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::LinearExecChain;
	Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

	FGraphLayoutPreviewBuildResult Result;
	TestTrue(TEXT("preview builds"), Service.BuildPreviewDataForTest(Request, Result));
	TestTrue(TEXT("result success"), Result.bSuccess);

	const FNodePlacement* EventPlacement = FindPlacement(Result.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* ResetPlacement = FindPlacement(Result.LayoutPlan, TEXT("ResetState"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("reset placement exists"), ResetPlacement);
	if (!EventPlacement || !ResetPlacement)
	{
		return false;
	}

	const float EventExecOutputY = EventPlacement->TargetPosition.Y + 58.0f;
	const float ResetExecInputY = ResetPlacement->TargetPosition.Y + 48.0f;
	TestEqual(TEXT("event output pin and first exec input pin are horizontal"), ResetExecInputY, EventExecOutputY);
	return true;
}
```

- [ ] **Step 2: Add failing test for true role overlap display**

In the same test file, add this test immediately after the straightening test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesRoleOverlap,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorPreservesRoleOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesRoleOverlap::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	FEditorCanvasSceneState LinearScene;
	LinearScene.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(240.0f, 160.0f));
	LinearScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(240.0f, 160.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);
	RuleSet.ExecColumnSpacing = 900.0f;
	RuleSet.CollisionPaddingX = 400.0f;
	RuleSet.CollisionPaddingY = 400.0f;
	RuleSet.CollisionStepY = 400.0f;

	FGraphLayoutPreviewService Service;
	FGraphLayoutPreviewRequest Request;
	Request.Scene = ESemanticScene::LinearExecChain;
	Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

	FGraphLayoutPreviewBuildResult Result;
	TestTrue(TEXT("preview builds"), Service.BuildPreviewDataForTest(Request, Result));
	TestTrue(TEXT("result success"), Result.bSuccess);

	const FNodePlacement* EventPlacement = FindPlacement(Result.LayoutPlan, TEXT("EventStart"));
	const FNodePlacement* ResetPlacement = FindPlacement(Result.LayoutPlan, TEXT("ResetState"));
	TestNotNull(TEXT("event placement exists"), EventPlacement);
	TestNotNull(TEXT("reset placement exists"), ResetPlacement);
	if (!EventPlacement || !ResetPlacement)
	{
		return false;
	}

	TestTrue(
		TEXT("overlapped role anchors produce overlapping native node bounds"),
		RectanglesOverlap(*EventPlacement, FVector2D(220.0f, 88.0f), *ResetPlacement, FVector2D(228.0f, 96.0f)));
	TestTrue(
		TEXT("overlap is not removed by solver occupancy"),
		FMath::Abs(EventPlacement->TargetPosition.X - ResetPlacement->TargetPosition.X) < 260.0f &&
		FMath::Abs(EventPlacement->TargetPosition.Y - ResetPlacement->TargetPosition.Y) < 140.0f);
	return true;
}
```

- [ ] **Step 3: Run RED tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.SemanticProjector;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewSemanticProjector_RED_20260602_001"
```

Expected:

```text
failed >= 1
```

The likely failures before implementation:

```text
event output pin and first exec input pin are horizontal
overlapped role anchors produce overlapping native node bounds
```

- [ ] **Step 4: Append RED evidence to Debug doc**

Append to `Debug/GraphLayout_PreviewCanvasMapping_Debug_20260602.md`:

```markdown
## RED Verification

- Command: `Automation RunTests BlueprintHelper.GraphLayout.Preview.SemanticProjector`
- Report: `D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewSemanticProjector_RED_20260602_001\index.json`
- Expected: current preview path fails pin-straightening and role-overlap fidelity because it still runs fixed sample through `FSolver::Solve(...)`.
```

---

## Task 2: Add Preview Anchor Metadata To Sample Nodes

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [ ] **Step 1: Extend preview node specs**

In `FGraphLayoutPreviewNodeSpec`, add:

```cpp
	ENodeRole PreviewAnchorRole = ENodeRole::Unknown;
	bool bUsePreviewRoleAnchor = false;
```

The resulting struct should read:

```cpp
struct FGraphLayoutPreviewNodeSpec
{
	FString NodeId;
	FString Title;
	EGraphLayoutPreviewNodeFactory Factory = EGraphLayoutPreviewNodeFactory::GenericK2;
	ENodeRole Role = ENodeRole::Unknown;
	ENodeRole PreviewAnchorRole = ENodeRole::Unknown;
	bool bUsePreviewRoleAnchor = false;
	FVector2D Size = FVector2D(220.0f, 96.0f);
};
```

- [ ] **Step 2: Extend the local `AddPreviewNode` helper**

In `BlueprintHelperGraphLayoutPreviewSampleFactory.cpp`, change the helper signature to:

```cpp
static void AddPreviewNode(
	FGraphLayoutPreviewSample& OutSample,
	const FString& NodeId,
	const FString& ClassPath,
	const FString& Title,
	const EGraphLayoutPreviewNodeFactory Factory,
	const ENodeRole Role,
	const FVector2D& Position,
	const FVector2D& Size,
	const bool bExisting,
	std::initializer_list<FPinSnapshot> Pins,
	const ENodeRole PreviewAnchorRole = ENodeRole::Unknown)
```

Inside the helper, after `NodeSpec.Role = Role;`, add:

```cpp
	NodeSpec.PreviewAnchorRole = PreviewAnchorRole;
	NodeSpec.bUsePreviewRoleAnchor = PreviewAnchorRole != ENodeRole::Unknown;
```

- [ ] **Step 3: Mark anchored nodes in linear exec sample**

In `BuildLinearExecSample(...)`, change the first two calls:

```cpp
	AddPreviewNode(
		OutSample,
		TEXT("EventStart"),
		TEXT("K2Node_CustomEvent"),
		TEXT("On Preview Trigger"),
		EGraphLayoutPreviewNodeFactory::CustomEvent,
		ENodeRole::EventEntry,
		FVector2D(0.0f, 0.0f),
		FVector2D(220.0f, 88.0f),
		false,
		{MakePreviewPin(TEXT("then"), EPinDirection::Output, true)},
		ENodeRole::EventEntry);
	AddPreviewNode(
		OutSample,
		TEXT("ResetState"),
		TEXT("K2Node_CallFunction"),
		TEXT("Reset State"),
		EGraphLayoutPreviewNodeFactory::CallFunction,
		ENodeRole::ExecNode,
		FVector2D(80.0f, 24.0f),
		FVector2D(228.0f, 96.0f),
		false,
		{
			MakePreviewPin(TEXT("execute"), EPinDirection::Input, true),
			MakePreviewPin(TEXT("then"), EPinDirection::Output, true)
		},
		ENodeRole::ExecNode);
```

- [ ] **Step 4: Mark anchored nodes in multi exec sample**

In `BuildMultiExecSample(...)`, anchor representative nodes:

```cpp
// EventStart -> EventEntry
// PrimaryPrint -> ExecNode
// Branch -> BranchControl
```

Use these exact final arguments:

```cpp
ENodeRole::EventEntry
ENodeRole::ExecNode
ENodeRole::BranchControl
```

Do not anchor `Sequence`, `BranchCondition`, `BranchPrint`, or `CompletedPrint` in this task.

- [ ] **Step 5: Add metadata assertions**

Extend `FBlueprintHelperGraphLayout_PreviewSampleFactoryBuildsFiveComplexSamples`:

```cpp
if (Scene.Scene == ESemanticScene::LinearExecChain)
{
	const FGraphLayoutPreviewNodeSpec* EventSpec = Sample.Nodes.FindByPredicate(
		[](const FGraphLayoutPreviewNodeSpec& Spec) { return Spec.NodeId == TEXT("EventStart"); });
	const FGraphLayoutPreviewNodeSpec* ResetSpec = Sample.Nodes.FindByPredicate(
		[](const FGraphLayoutPreviewNodeSpec& Spec) { return Spec.NodeId == TEXT("ResetState"); });
	TestTrue(TEXT("linear event uses preview role anchor"), EventSpec && EventSpec->bUsePreviewRoleAnchor && EventSpec->PreviewAnchorRole == ENodeRole::EventEntry);
	TestTrue(TEXT("linear first exec uses preview role anchor"), ResetSpec && ResetSpec->bUsePreviewRoleAnchor && ResetSpec->PreviewAnchorRole == ENodeRole::ExecNode);
}
if (Scene.Scene == ESemanticScene::MultiExecOutput)
{
	const FGraphLayoutPreviewNodeSpec* EventSpec = Sample.Nodes.FindByPredicate(
		[](const FGraphLayoutPreviewNodeSpec& Spec) { return Spec.NodeId == TEXT("EventStart"); });
	const FGraphLayoutPreviewNodeSpec* PrimarySpec = Sample.Nodes.FindByPredicate(
		[](const FGraphLayoutPreviewNodeSpec& Spec) { return Spec.NodeId == TEXT("PrimaryPrint"); });
	const FGraphLayoutPreviewNodeSpec* BranchSpec = Sample.Nodes.FindByPredicate(
		[](const FGraphLayoutPreviewNodeSpec& Spec) { return Spec.NodeId == TEXT("Branch"); });
	TestTrue(TEXT("multi event uses preview role anchor"), EventSpec && EventSpec->PreviewAnchorRole == ENodeRole::EventEntry);
	TestTrue(TEXT("multi primary uses preview role anchor"), PrimarySpec && PrimarySpec->PreviewAnchorRole == ENodeRole::ExecNode);
	TestTrue(TEXT("multi branch uses preview role anchor"), BranchSpec && BranchSpec->PreviewAnchorRole == ENodeRole::BranchControl);
}
```

- [ ] **Step 6: Run metadata tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.SampleFactoryBuildsFiveComplexSamples;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewAnchorMetadata_GREEN_20260602_001"
```

Expected:

```text
succeeded=1
failed=0
```

---

## Task 3: Implement Preview Semantic Projector

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [ ] **Step 1: Create public header**

Create `BlueprintHelperGraphLayoutPreviewSemanticProjector.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
class BLUEPRINTHELPER_API FGraphLayoutPreviewSemanticProjector
{
public:
	static FLayoutPlan Project(const FGraphLayoutPreviewSample& Sample, const FRuleSet& RuleSet);

private:
	static FVector2D GetAnchorOffset(const FGraphLayoutPreviewNodeSpec& NodeSpec, ENodeRole AnchorRole);
	static FVector2D BuildTopLeftFromAnchor(
		const FGraphLayoutPreviewNodeSpec& NodeSpec,
		ENodeRole AnchorRole,
		const FVector2D& AnchorPosition);
	static const FGraphLayoutPreviewNodeSpec* FindNodeSpec(
		const FGraphLayoutPreviewSample& Sample,
		const FString& NodeId);
	static const FNodeSnapshot* FindSnapshotNode(
		const FGraphLayoutPreviewSample& Sample,
		const FString& NodeId);
	static void AddPlacement(
		FLayoutPlan& Plan,
		const FGraphLayoutPreviewSample& Sample,
		const FGraphLayoutPreviewNodeSpec& NodeSpec,
		const FVector2D& TargetPosition,
		const FString& Reason);
	static void ProjectLinearExec(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
	static void ProjectMultiExec(
		const FGraphLayoutPreviewSample& Sample,
		const FRuleSet& RuleSet,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
	static bool HasPlacementForNode(
		const FLayoutPlan& Plan,
		const FString& NodeId);
	static void ProjectAnchoredNodesByRole(
		const FGraphLayoutPreviewSample& Sample,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
	static void ProjectRemainingNodesBySampleOffset(
		const FGraphLayoutPreviewSample& Sample,
		const FEditorCanvasSceneState& SceneState,
		FLayoutPlan& Plan);
};
}
```

- [ ] **Step 2: Implement anchor offset helpers**

Create `BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp` with this opening implementation:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSemanticScene.h"

namespace BlueprintHelper::GraphLayout
{
static FVector2D MakeCenterOffset(const FVector2D& Size)
{
	return Size * 0.5f;
}

FVector2D FGraphLayoutPreviewSemanticProjector::GetAnchorOffset(
	const FGraphLayoutPreviewNodeSpec& NodeSpec,
	const ENodeRole AnchorRole)
{
	const FVector2D Size = NodeSpec.Size;
	switch (AnchorRole)
	{
	case ENodeRole::EventEntry:
		return FVector2D(FMath::Max(0.0f, Size.X - 18.0f), 58.0f);
	case ENodeRole::ExecNode:
	case ENodeRole::BranchControl:
	case ENodeRole::AsyncNode:
	case ENodeRole::DelegateNode:
		return FVector2D(16.0f, 48.0f);
	case ENodeRole::PureFunction:
	case ENodeRole::OperatorOrCompare:
	case ENodeRole::VariableInput:
	case ENodeRole::Comment:
	default:
		return MakeCenterOffset(Size);
	}
}

FVector2D FGraphLayoutPreviewSemanticProjector::BuildTopLeftFromAnchor(
	const FGraphLayoutPreviewNodeSpec& NodeSpec,
	const ENodeRole AnchorRole,
	const FVector2D& AnchorPosition)
{
	return AnchorPosition - GetAnchorOffset(NodeSpec, AnchorRole);
}
```

This intentionally treats exec scene role centers as pin anchors, so same-row roles produce straight exec wires.

- [ ] **Step 3: Implement common lookup and placement helpers**

Append:

```cpp
const FGraphLayoutPreviewNodeSpec* FGraphLayoutPreviewSemanticProjector::FindNodeSpec(
	const FGraphLayoutPreviewSample& Sample,
	const FString& NodeId)
{
	return Sample.Nodes.FindByPredicate([&NodeId](const FGraphLayoutPreviewNodeSpec& Spec)
	{
		return Spec.NodeId == NodeId;
	});
}

const FNodeSnapshot* FGraphLayoutPreviewSemanticProjector::FindSnapshotNode(
	const FGraphLayoutPreviewSample& Sample,
	const FString& NodeId)
{
	return Sample.Snapshot.Nodes.FindByPredicate([&NodeId](const FNodeSnapshot& Node)
	{
		return Node.NodeId == NodeId;
	});
}

void FGraphLayoutPreviewSemanticProjector::AddPlacement(
	FLayoutPlan& Plan,
	const FGraphLayoutPreviewSample& Sample,
	const FGraphLayoutPreviewNodeSpec& NodeSpec,
	const FVector2D& TargetPosition,
	const FString& Reason)
{
	const FNodeSnapshot* SnapshotNode = FindSnapshotNode(Sample, NodeSpec.NodeId);
	FNodePlacement Placement;
	Placement.NodeId = NodeSpec.NodeId;
	Placement.Role = NodeSpec.Role;
	Placement.CurrentPosition = SnapshotNode ? SnapshotNode->Position : FVector2D::ZeroVector;
	Placement.TargetPosition = TargetPosition;
	Placement.bMoveExisting = true;
	Placement.Reason = Reason;
	Plan.Placements.Add(Placement);
}
```

- [ ] **Step 4: Implement linear exec projection**

Append:

```cpp
void FGraphLayoutPreviewSemanticProjector::ProjectLinearExec(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	const FGraphLayoutPreviewNodeSpec* EventSpec = FindNodeSpec(Sample, TEXT("EventStart"));
	const FGraphLayoutPreviewNodeSpec* ResetSpec = FindNodeSpec(Sample, TEXT("ResetState"));
	if (!EventSpec || !ResetSpec)
	{
		Plan.Issues.Add(TEXT("linear preview sample is missing anchored nodes"));
		return;
	}

	const FVector2D EventAnchor = SceneState.RoleCenters.FindRef(ENodeRole::EventEntry);
	const FVector2D ExecAnchor = SceneState.RoleCenters.FindRef(ENodeRole::ExecNode);
	AddPlacement(
		Plan,
		Sample,
		*EventSpec,
		BuildTopLeftFromAnchor(*EventSpec, ENodeRole::EventEntry, EventAnchor),
		TEXT("preview_semantic_role_anchor"));
	AddPlacement(
		Plan,
		Sample,
		*ResetSpec,
		BuildTopLeftFromAnchor(*ResetSpec, ENodeRole::ExecNode, ExecAnchor),
		TEXT("preview_semantic_role_anchor"));

	const TArray<FString> ChainNodeIds = {
		TEXT("SetCounter"),
		TEXT("PrintLabel"),
		TEXT("DelayAsync")
	};
	FVector2D PreviousExecAnchor = ExecAnchor;
	for (const FString& NodeId : ChainNodeIds)
	{
		const FGraphLayoutPreviewNodeSpec* NodeSpec = FindNodeSpec(Sample, NodeId);
		if (!NodeSpec)
		{
			continue;
		}
		PreviousExecAnchor.X += RuleSet.ExecColumnSpacing;
		const FVector2D Target = BuildTopLeftFromAnchor(*NodeSpec, ENodeRole::ExecNode, PreviousExecAnchor);
		AddPlacement(Plan, Sample, *NodeSpec, Target, TEXT("preview_exec_pin_straighten"));
	}
}
```

- [ ] **Step 5: Implement multi exec projection**

Append:

```cpp
void FGraphLayoutPreviewSemanticProjector::ProjectMultiExec(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	const FVector2D EventAnchor = SceneState.RoleCenters.FindRef(ENodeRole::EventEntry);
	const FVector2D PrimaryAnchor = SceneState.RoleCenters.FindRef(ENodeRole::ExecNode);
	const FVector2D BranchAnchor = SceneState.RoleCenters.FindRef(ENodeRole::BranchControl);

	const TMap<FString, TPair<ENodeRole, FVector2D>> AnchoredNodes = {
		{TEXT("EventStart"), TPair<ENodeRole, FVector2D>(ENodeRole::EventEntry, EventAnchor)},
		{TEXT("PrimaryPrint"), TPair<ENodeRole, FVector2D>(ENodeRole::ExecNode, PrimaryAnchor)},
		{TEXT("Branch"), TPair<ENodeRole, FVector2D>(ENodeRole::BranchControl, BranchAnchor)}
	};

	for (const TPair<FString, TPair<ENodeRole, FVector2D>>& Pair : AnchoredNodes)
	{
		if (const FGraphLayoutPreviewNodeSpec* NodeSpec = FindNodeSpec(Sample, Pair.Key))
		{
			AddPlacement(
				Plan,
				Sample,
				*NodeSpec,
				BuildTopLeftFromAnchor(*NodeSpec, Pair.Value.Key, Pair.Value.Value),
				TEXT("preview_semantic_role_anchor"));
		}
	}

	if (const FGraphLayoutPreviewNodeSpec* SequenceSpec = FindNodeSpec(Sample, TEXT("Sequence")))
	{
		const FVector2D SequenceAnchor(
			(EventAnchor.X + PrimaryAnchor.X) * 0.5f,
			EventAnchor.Y);
		AddPlacement(
			Plan,
			Sample,
			*SequenceSpec,
			BuildTopLeftFromAnchor(*SequenceSpec, ENodeRole::BranchControl, SequenceAnchor),
			TEXT("preview_exec_pin_straighten"));
	}

	if (const FGraphLayoutPreviewNodeSpec* BranchConditionSpec = FindNodeSpec(Sample, TEXT("BranchCondition")))
	{
		const FVector2D ConditionCenter = BranchAnchor + FVector2D(-RuleSet.VariableInputOffsetX, 0.0f);
		AddPlacement(
			Plan,
			Sample,
			*BranchConditionSpec,
			BuildTopLeftFromAnchor(*BranchConditionSpec, ENodeRole::VariableInput, ConditionCenter),
			TEXT("preview_semantic_data_anchor"));
	}

	if (const FGraphLayoutPreviewNodeSpec* BranchPrintSpec = FindNodeSpec(Sample, TEXT("BranchPrint")))
	{
		const FVector2D BranchPrintAnchor = BranchAnchor + FVector2D(RuleSet.ExecColumnSpacing, 0.0f);
		AddPlacement(
			Plan,
			Sample,
			*BranchPrintSpec,
			BuildTopLeftFromAnchor(*BranchPrintSpec, ENodeRole::ExecNode, BranchPrintAnchor),
			TEXT("preview_exec_pin_straighten"));
	}

	if (const FGraphLayoutPreviewNodeSpec* CompletedPrintSpec = FindNodeSpec(Sample, TEXT("CompletedPrint")))
	{
		const FVector2D CompletedAnchor = PrimaryAnchor + FVector2D(RuleSet.ExecColumnSpacing, 0.0f);
		AddPlacement(
			Plan,
			Sample,
			*CompletedPrintSpec,
			BuildTopLeftFromAnchor(*CompletedPrintSpec, ENodeRole::ExecNode, CompletedAnchor),
			TEXT("preview_exec_pin_straighten"));
	}
}
```

- [ ] **Step 6: Implement `Project(...)` without solver fallback**

Append:

```cpp
bool FGraphLayoutPreviewSemanticProjector::HasPlacementForNode(
	const FLayoutPlan& Plan,
	const FString& NodeId)
{
	return Plan.Placements.ContainsByPredicate([&NodeId](const FNodePlacement& Placement)
	{
		return Placement.NodeId == NodeId;
	});
}

void FGraphLayoutPreviewSemanticProjector::ProjectAnchoredNodesByRole(
	const FGraphLayoutPreviewSample& Sample,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (!NodeSpec.bUsePreviewRoleAnchor || HasPlacementForNode(Plan, NodeSpec.NodeId))
		{
			continue;
		}
		const FVector2D Anchor = SceneState.RoleCenters.FindRef(NodeSpec.PreviewAnchorRole);
		AddPlacement(
			Plan,
			Sample,
			NodeSpec,
			BuildTopLeftFromAnchor(NodeSpec, NodeSpec.PreviewAnchorRole, Anchor),
			TEXT("preview_semantic_role_anchor"));
	}
}

void FGraphLayoutPreviewSemanticProjector::ProjectRemainingNodesBySampleOffset(
	const FGraphLayoutPreviewSample& Sample,
	const FEditorCanvasSceneState& SceneState,
	FLayoutPlan& Plan)
{
	const FVector2D SceneOrigin = SceneState.RoleCenters.Num() > 0
		? SceneState.RoleCenters.CreateConstIterator()->Value
		: FVector2D::ZeroVector;
	for (const FGraphLayoutPreviewNodeSpec& NodeSpec : Sample.Nodes)
	{
		if (HasPlacementForNode(Plan, NodeSpec.NodeId))
		{
			continue;
		}
		const FNodeSnapshot* SnapshotNode = FindSnapshotNode(Sample, NodeSpec.NodeId);
		const FVector2D SamplePosition = SnapshotNode ? SnapshotNode->Position : FVector2D::ZeroVector;
		AddPlacement(
			Plan,
			Sample,
			NodeSpec,
			SceneOrigin + SamplePosition,
			TEXT("preview_sample_relative"));
	}
}

FLayoutPlan FGraphLayoutPreviewSemanticProjector::Project(
	const FGraphLayoutPreviewSample& Sample,
	const FRuleSet& RuleSet)
{
	FLayoutPlan Plan;
	Plan.Classifications = FClassifier::ClassifyGraph(Sample.Snapshot, RuleSet);
	const FEditorCanvasSceneState SceneState = FSemanticSceneAdapter::ResolveSceneState(RuleSet, Sample.Scene);

	switch (Sample.Scene)
	{
	case ESemanticScene::LinearExecChain:
		ProjectLinearExec(Sample, RuleSet, SceneState, Plan);
		break;
	case ESemanticScene::MultiExecOutput:
		ProjectMultiExec(Sample, RuleSet, SceneState, Plan);
		break;
	default:
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		break;
	}

	ProjectRemainingNodesBySampleOffset(Sample, SceneState, Plan);
	return Plan;
}
}
```

Do not call `FSolver::Solve(...)` anywhere in this projector. `preview_sample_relative` is only for non-anchor filler nodes in the fixed sample; anchored role nodes must keep the raw draggable canvas relationship, including true overlap.

Add this include at the top because `FClassifier` is used:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
```

- [ ] **Step 7: Run RED tests again**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.SemanticProjector;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewSemanticProjector_ProjectorOnly_20260602_001"
```

Expected before Task 4:

```text
failed >= 1
```

Reason: projector exists, but PreviewService still calls `FSolver::Solve(...)`.

---

## Task 4: Wire Preview Service To Semantic Projector

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [ ] **Step 1: Replace solver call in preview build**

In `BlueprintHelperGraphLayoutPreviewService.cpp`, replace:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
```

with:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h"
```

Then replace:

```cpp
	OutResult.LayoutPlan = FSolver::Solve(OutResult.Sample.Snapshot, RuleSet);
```

with:

```cpp
	OutResult.LayoutPlan = FGraphLayoutPreviewSemanticProjector::Project(OutResult.Sample, RuleSet);
```

- [ ] **Step 2: Ensure no silent empty fallback**

Keep the existing empty placement guard:

```cpp
	if (OutResult.LayoutPlan.Placements.Num() == 0)
	{
		OutResult.bSuccess = false;
		OutResult.Error = TEXT("Preview layout produced no placements.");
		return false;
	}
```

Do not catch projection issues and rerun `FSolver` here. If projection is broken, Preview must fail visibly instead of lying about the dragged canvas effect.

- [ ] **Step 3: Run semantic projector tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.SemanticProjector;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewSemanticProjector_GREEN_20260602_001"
```

Expected:

```text
succeeded=2
failed=0
```

- [ ] **Step 4: Run full Preview tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewSemanticProjector_FullPreview_20260602_001"
```

Expected:

```text
failed=0
```

---

## Task 5: Extend Projector Coverage For Data/Input/Occupancy Scenes

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [ ] **Step 1: Anchor representative data sample nodes**

In `BuildPureDataSample(...)`, add anchors:

```cpp
// SelfRef -> VariableInput
// ComposeKey -> OperatorOrCompare
// BuildArray -> PureFunction
```

Set final `PreviewAnchorRole` arguments:

```cpp
ENodeRole::VariableInput
ENodeRole::OperatorOrCompare
ENodeRole::PureFunction
```

In `BuildNodeInputClusterSample(...)`, add anchors:

```cpp
// ContextGet -> VariableInput
// IsValidGate -> OperatorOrCompare
// ComposePayload -> PureFunction
// Consumer -> ExecNode
```

In `BuildOccupancySample(...)`, add anchors:

```cpp
// CandidateExec -> ExecNode
// DelayAsync -> AsyncNode
// CommentBlocker -> Comment
```

- [ ] **Step 2: Use anchor-first projection for every remaining semantic scene**

In `BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp`, keep the existing default branch anchor-first and sample-relative only:

```cpp
	default:
		ProjectAnchoredNodesByRole(Sample, SceneState, Plan);
		break;
```

Keep the shared `ProjectRemainingNodesBySampleOffset(Sample, SceneState, Plan);` call after the switch. This makes pure data, node input cluster, and occupancy preview consume the current draggable role centers directly as anchors. Nodes without a role anchor keep their fixed-sample relative offset from the first scene anchor; they are not routed through solver occupancy.

- [ ] **Step 3: Add a guard against accidentally reintroducing solver fallback**

Search the projector implementation:

```powershell
Select-String -Path "BlueprintHelper\Source\BlueprintHelper\Private\Systems\GraphLayout\BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp" -Pattern "FSolver|Occupancy|ProjectBySolverFallback"
```

Expected:

```text
no output
```

If this command returns any line, remove that dependency before proceeding. Preview must not silently re-layout overlapped role anchors.

- [ ] **Step 4: Add scene-wide role-overlap test**

Add a test that loops through every scene, overlaps all saved role centers to one point, builds Preview, and verifies every anchored node bounds intersects at least one other anchored node:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesOverlapAcrossScenes,
	"BlueprintHelper.GraphLayout.Preview.SemanticProjectorPreservesOverlapAcrossScenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewSemanticProjectorPreservesOverlapAcrossScenes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	for (const FSemanticSceneDefinition& SceneDefinition : FSemanticSceneCatalog::GetAllScenes())
	{
		FRuleSet RuleSet;
		FEditorCanvasSceneState SceneState;
		for (const FSemanticSceneNodeDefinition& NodeDefinition : SceneDefinition.Nodes)
		{
			SceneState.RoleCenters.Add(NodeDefinition.Role, FVector2D(320.0f, 180.0f));
		}
		RuleSet.EditorCanvasScenes.Add(SceneDefinition.Scene, SceneState);

		FGraphLayoutPreviewService Service;
		FGraphLayoutPreviewRequest Request;
		Request.Scene = SceneDefinition.Scene;
		Request.RuleSetJson = FRuleSetJson::ExportString(RuleSet);

		FGraphLayoutPreviewBuildResult Result;
		TestTrue(FString::Printf(TEXT("preview builds for %s"), ToString(SceneDefinition.Scene)), Service.BuildPreviewDataForTest(Request, Result));
		TestTrue(TEXT("result success"), Result.bSuccess);
		TestTrue(TEXT("has placements"), Result.LayoutPlan.Placements.Num() > 0);

		int32 OverlapPairCount = 0;
		for (int32 LeftIndex = 0; LeftIndex < Result.LayoutPlan.Placements.Num(); ++LeftIndex)
		{
			for (int32 RightIndex = LeftIndex + 1; RightIndex < Result.LayoutPlan.Placements.Num(); ++RightIndex)
			{
				const FNodePlacement& Left = Result.LayoutPlan.Placements[LeftIndex];
				const FNodePlacement& Right = Result.LayoutPlan.Placements[RightIndex];
				if (Left.Reason == TEXT("preview_semantic_role_anchor") &&
					Right.Reason == TEXT("preview_semantic_role_anchor") &&
					RectanglesOverlap(Left, FVector2D(240.0f, 120.0f), Right, FVector2D(240.0f, 120.0f)))
				{
					++OverlapPairCount;
				}
			}
		}
		TestTrue(FString::Printf(TEXT("anchored preview nodes overlap for %s"), ToString(SceneDefinition.Scene)), OverlapPairCount > 0);
	}
	return true;
}
```

If node-specific sizes are needed for exact assertions, replace the generic `240 x 120` size with a local helper that reads `Result.Sample.Nodes` by `NodeId`.

- [ ] **Step 5: Run scene-wide tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.SemanticProjectorPreservesOverlapAcrossScenes;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewOverlapScenes_GREEN_20260602_001"
```

Expected:

```text
succeeded=1
failed=0
```

---

## Task 6: Native Preview Materialization Verification

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: `Debug/GraphLayout_PreviewCanvasMapping_Debug_20260602.md`

- [ ] **Step 1: Add materializer position test for overlap**

Add this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PreviewMaterializerPreservesSemanticOverlap,
	"BlueprintHelper.GraphLayout.Preview.MaterializerPreservesSemanticOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PreviewMaterializerPreservesSemanticOverlap::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	TestTrue(TEXT("preview materializer test runs on the game thread"), IsInGameThread());

	FRuleSet RuleSet;
	FEditorCanvasSceneState LinearScene;
	LinearScene.RoleCenters.Add(ENodeRole::EventEntry, FVector2D(260.0f, 180.0f));
	LinearScene.RoleCenters.Add(ENodeRole::ExecNode, FVector2D(260.0f, 180.0f));
	RuleSet.EditorCanvasScenes.Add(ESemanticScene::LinearExecChain, LinearScene);

	FGraphLayoutPreviewSample Sample;
	FString Error;
	TestTrue(TEXT("linear sample builds"), FGraphLayoutPreviewSampleFactory::BuildSample(ESemanticScene::LinearExecChain, Sample, Error));
	const FLayoutPlan Plan = FGraphLayoutPreviewSemanticProjector::Project(Sample, RuleSet);

	FGraphLayoutPreviewMaterializer Materializer;
	FGraphLayoutPreviewMaterializerResult Result;
	TestTrue(TEXT("materializer creates graph"), Materializer.MaterializeForTest(Sample, Plan, Result));
	TestTrue(TEXT("preview graph valid"), Result.PreviewGraph.IsValid());
	if (!Result.PreviewGraph.IsValid())
	{
		return false;
	}

	UEdGraphNode* EventNode = nullptr;
	UEdGraphNode* ResetNode = nullptr;
	for (UEdGraphNode* Node : Result.PreviewGraph->Nodes)
	{
		if (Node && Node->GetName().Contains(TEXT("EventStart")))
		{
			EventNode = Node;
		}
		if (Node && Node->GetName().Contains(TEXT("ResetState")))
		{
			ResetNode = Node;
		}
	}
	TestNotNull(TEXT("event node materialized"), EventNode);
	TestNotNull(TEXT("reset node materialized"), ResetNode);
	if (!EventNode || !ResetNode)
	{
		return false;
	}

	const FNodePlacement* EventPlacement = FindPlacement(Plan, TEXT("EventStart"));
	const FNodePlacement* ResetPlacement = FindPlacement(Plan, TEXT("ResetState"));
	TestEqual(TEXT("event materializer uses semantic plan x"), EventNode->NodePosX, FMath::RoundToInt(EventPlacement->TargetPosition.X));
	TestEqual(TEXT("event materializer uses semantic plan y"), EventNode->NodePosY, FMath::RoundToInt(EventPlacement->TargetPosition.Y));
	TestEqual(TEXT("reset materializer uses semantic plan x"), ResetNode->NodePosX, FMath::RoundToInt(ResetPlacement->TargetPosition.X));
	TestEqual(TEXT("reset materializer uses semantic plan y"), ResetNode->NodePosY, FMath::RoundToInt(ResetPlacement->TargetPosition.Y));
	return true;
}
```

Add include if needed:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h"
```

- [ ] **Step 2: Run materializer test**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview.MaterializerPreservesSemanticOverlap;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewMaterializerOverlap_GREEN_20260602_001"
```

Expected:

```text
succeeded=1
failed=0
```

- [ ] **Step 3: Append materializer evidence to Debug doc**

Append:

```markdown
## Materializer Verification

- `FGraphLayoutPreviewMaterializer` still consumes `FLayoutPlan.TargetPosition` directly.
- Semantic overlap is preserved after transient `UEdGraph` materialization.
- Report: `D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewMaterializerOverlap_GREEN_20260602_001\index.json`
```

---

## Task 7: Build, Focused Automation, And Real E2E

**Files:**
- Modify: `Debug/GraphLayout_PreviewCanvasMapping_Debug_20260602.md`
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PreviewCanvasFaithful_Report_20260602_CN.md`

- [ ] **Step 1: Build**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

```text
Result: Succeeded
```

- [ ] **Step 2: Run focused automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Preview;BlueprintHelper.GraphLayout.SemanticScene;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewCanvasFaithful_Focused_20260602_001"
```

Expected:

```text
failed=0
```

- [ ] **Step 3: Run wider GraphLayout automation if focused tests pass**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PreviewCanvasFaithful_Full_20260602_001"
```

Expected:

```text
failed=0
```

- [ ] **Step 4: Real editor E2E smoke**

Run via BlueprintHelper MCP lifecycle tools:

```text
blueprint_open_editor(project_file="D:\UEProjects\Template\Template.uproject", wait_timeout_ms=120000)
```

Expected:

```text
EDITOR_BRIDGE_AVAILABLE
```

Manual UI check in LayoutPanel:

```text
1. Open LayoutPanel.
2. Select Linear Exec semantic page.
3. Drag EventEntry and ExecNode to the same Y but different X.
4. Click Preview.
5. Verify On Preview Trigger -> Reset State exec wire is horizontal.
6. Return to Edit.
7. Drag EventEntry and ExecNode to overlap.
8. Click Preview.
9. Verify the native preview nodes visibly overlap instead of being separated by occupancy.
```

Then close:

```text
blueprint_close_editor(save_all=false)
```

Verify no editor remains:

```powershell
Get-Process UnrealEditor -ErrorAction SilentlyContinue
```

Expected:

```text
no output
```

- [ ] **Step 5: Run diff hygiene**

Run:

```powershell
git diff --check
```

Expected:

```text
exit code 0
```

Line-ending warnings are acceptable if no whitespace errors are reported.

- [ ] **Step 6: Write report**

Create `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PreviewCanvasFaithful_Report_20260602_CN.md`:

```markdown
# GraphLayout Preview Canvas Faithful Report - 2026-06-02

## 改动原因

- Preview 需要显示当前拖拽语义画布规则的真实效果。
- Entry 到首个 Exec 节点未按 exec pin 水平拉直。
- role 重叠被 Preview solver/occupancy 消掉，和可拖拽画布不一致。

## 改动范围

- 新增 `FGraphLayoutPreviewSemanticProjector`。
- 为 preview sample node 增加 role anchor metadata。
- PreviewService 改为使用 semantic projector 生成 Preview layout plan。
- 保留 runtime `FSolver` 和真实 GraphLayout apply 路径不变。

## 验证结果

- Build: 填写通过/失败与命令。
- Focused Automation: 填写 report path 与 succeeded/failed。
- Full GraphLayout Automation: 填写 report path 与 succeeded/failed。
- Real Editor E2E: 填写打开、Preview 检查、关闭结果。

## 结果

- role 重叠在 Preview 中真实显示。
- Entry -> first exec preview link 按 exec pin 水平拉直。
- Runtime solver 不受 Preview 语义投影影响。
```

Do not run `git add`, `git commit`, or `git push`.

---

## Self-Review Checklist

- [ ] Requirement coverage: Preview uses current draggable semantic canvas scene state, not only scalar RuleSet fields.
- [ ] Requirement coverage: role overlap is preserved and tested.
- [ ] Requirement coverage: Entry -> first exec pin straightening is tested.
- [ ] Architecture: projection logic lives in GraphLayout preview service/projector boundary, not `SBlueprintHelperLayoutRuleEditor`.
- [ ] Architecture: runtime solver and apply path remain unchanged.
- [ ] Test coverage: RED tests fail before service integration and pass after projector wiring.
- [ ] Test coverage: native materializer consumes semantic target positions exactly.
- [ ] Docs: Debug and Report files updated.
- [ ] Git rule: no automated `git add` / `git commit` / `git push`.

## Suggested Manual Commit Scope After Implementation

Do not execute automatically. When implementation and verification are complete, manually stage only these files:

```powershell
git add -- BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSemanticProjector.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp
git add -f -- Debug/GraphLayout_PreviewCanvasMapping_Debug_20260602.md BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PreviewCanvasFaithful_Report_20260602_CN.md
git commit
```

Suggested commit message:

```text
新增内容：
1. 新增 GraphLayout Preview 语义投影器

修复内容：
1. 修复 Preview 中 Entry 到首个 Exec 节点未按 exec pin 拉直
2. 修复可拖拽 role 重叠在 Preview 中被 occupancy 消掉的问题
```
