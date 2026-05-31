# GraphLayout Occupancy And Data Input Placement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 BlueprintHelper UE 侧 GraphLayout 在 TaskRun 完成后，根据用户可视化规则把 generated nodes 排到稳定、可读、避开已有节点的位置，修复 PureFunction / OperatorOrCompare / VariableInput 与 Exec 流同线或重叠的问题。

**Architecture:** 保持既有 `GameThread snapshot -> worker solve -> GameThread async apply` 架构。新增布局策略边界只放在 `Systems/GraphLayout`：角色锚点解析、数据输入放置、占用/碰撞避让；UI 只编辑 RuleSet JSON 与画布状态，不承担 runtime 布局判断。TaskPlan / GraphWrite 继续只负责生成节点和连线，Review 不记录 layout diff。

**Tech Stack:** Unreal Engine 5.6 C++、Slate、UE Automation Tests、BlueprintHelper GraphLayout subsystem、JSON RuleSet v1。

---

## Non-Negotiable Boundaries

- 不修改 TaskSpec / TaskPlan schema 来表达 layout。
- 不让 GraphWrite / GraphStatement / TaskRuntime 的生成逻辑计算节点坐标。
- 不把碰撞避让、角色偏移推导、异步 apply 生命周期放进 Slate widget。
- 不恢复 Reroute / Knot 角色；Reroute 继续作为 ignored/Unknown。
- 不记录 layout diff 到 Review。
- 不移动 unrelated dirty files；提交时只包含本计划列出的文件。
- 本仓库 AGENTS 规则禁止 Codex 自动 `git add`、`git commit`、`git push`。计划中的 checkpoint 只要求输出手动命令，不由 agent 执行。

## Current Source Facts

- Runtime solver 当前没有消费 `FRuleSet::EditorCanvasRoleCenters`，数据输入只用 `PureInputOffsetX` / `VariableInputOffsetX` 与 `InputPinRowSpacing`。
- `AlignInputsToConsumerPinOrder()` 当前目标为：

```cpp
const FVector2D Target(
	ConsumerNode->Target.X - GetDataInputOffsetX(SourceNode->Role, RuleSet),
	ConsumerNode->Target.Y + InputOrder * RuleSet.InputPinRowSpacing);
```

- 这会让第一个数据输入的 Y 与 consumer/exec 同线。
- `ReserveRow()` 只在单条 `LayoutExecChain()` 的局部 `UsedRows` 中避让，不能全局避开其他 root、existing nodes、data nodes。
- Snapshot 已经有 `Position`、`Size`、`bExisting`，足够在 worker solve 中做矩形占用避让。
- Coordinator 已经按帧 apply：`MaxNodesPerFrame`、`MaxMillisecondsPerFrame` 已存在。本轮不重写 apply queue，只在计划中补充后续风险验证。

## File Structure

### Create

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h`
  - 将 `editor_canvas.role_centers` 解析为 runtime 可用的每角色相对 offset。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.cpp`
  - 实现角色锚点 fallback、clamp、`ExecNode` 基准解析。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h`
  - 定义矩形占用模型、候选位置避让 API。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.cpp`
  - 实现 existing/generated node rect 阻塞、downward search、reserve。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h`
  - 定义“所有非 exec 输入都按 ExecNode 输入规则放置”的策略 API。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.cpp`
  - 实现 consumer input pin order、Pure/Operator/Variable 角色偏移、输入节点避让。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
  - 新增纯 solver automation tests，不依赖实际 Blueprint asset。

### Modify

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
  - 在 `FRuleSet` 增加 collision / occupancy 参数。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp`
  - 默认值、ToJson、role string 保持一致。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp`
  - import/export/validate 新增字段。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
  - 保留 solver 编排，替换局部 data input 同线算法，引入全局 occupancy。
- `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
  - 新增右侧设置面板状态字段。
- `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`
  - 设置面板显示新增非画布参数；修复 Layout 面板 tooltip 为中文；JSON 与设置同步。

### Do Not Modify For This Plan

- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/*`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/*`
- `BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/*`
- Review v2 files
- AgentFaceService / ClaudePlugin / CodexPlugin files

---

## Task 1: Add RED Solver Tests

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Read: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
- Read: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h`

- [x] **Step 1: Create the GraphLayout test file with helpers**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp` with this base content:

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

#include <initializer_list>

namespace BlueprintHelperGraphLayoutSolverTests
{
using namespace BlueprintHelper::GraphLayout;

static FPinSnapshot MakePin(
	const FString& Name,
	EPinDirection Direction,
	bool bExec,
	std::initializer_list<const TCHAR*> LinkedNodeIds = {})
{
	FPinSnapshot Pin;
	Pin.PinId = Name;
	Pin.Name = Name;
	Pin.Direction = Direction;
	Pin.bExec = bExec;
	Pin.Category = bExec ? TEXT("exec") : TEXT("object");
	for (const TCHAR* LinkedNodeId : LinkedNodeIds)
	{
		Pin.LinkedNodeIds.Add(LinkedNodeId);
	}
	return Pin;
}

static FNodeSnapshot MakeNode(
	const FString& NodeId,
	const FString& ClassPath,
	const FString& Title,
	const FVector2D& Position,
	const FVector2D& Size,
	bool bExisting,
	std::initializer_list<FPinSnapshot> Pins)
{
	FNodeSnapshot Node;
	Node.NodeId = NodeId;
	Node.StableName = NodeId;
	Node.ClassPath = ClassPath;
	Node.Title = Title;
	Node.Position = Position;
	Node.Size = Size;
	Node.bExisting = bExisting;
	for (const FPinSnapshot& Pin : Pins)
	{
		Node.Pins.Add(Pin);
	}
	return Node;
}

static const FNodePlacement* FindPlacement(const FLayoutPlan& Plan, const FString& NodeId)
{
	for (const FNodePlacement& Placement : Plan.Placements)
	{
		if (Placement.NodeId == NodeId)
		{
			return &Placement;
		}
	}
	return nullptr;
}

static bool RectsOverlap(const FNodePlacement& A, const FVector2D& ASize, const FNodePlacement& B, const FVector2D& BSize)
{
	const FVector2D AMin = A.TargetPosition;
	const FVector2D AMax = A.TargetPosition + ASize;
	const FVector2D BMin = B.TargetPosition;
	const FVector2D BMax = B.TargetPosition + BSize;
	return AMin.X < BMax.X && AMax.X > BMin.X && AMin.Y < BMax.Y && AMax.Y > BMin.Y;
}

static FRuleSet MakeRuleSetWithCanvasOffsets()
{
	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 420.0f;
	RuleSet.ExecRowSpacing = 220.0f;
	RuleSet.BranchRowSpacing = 160.0f;
	RuleSet.PureInputOffsetX = 260.0f;
	RuleSet.VariableInputOffsetX = 240.0f;
	RuleSet.InputPinRowSpacing = 48.0f;
	RuleSet.bMoveGeneratedNodes = true;
	RuleSet.bMoveExistingNodes = false;
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::ExecNode, FVector2D(300.0f, 100.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::PureFunction, FVector2D(80.0f, 230.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::OperatorOrCompare, FVector2D(90.0f, 310.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::VariableInput, FVector2D(70.0f, 190.0f));
	return RuleSet;
}
}
```

- [x] **Step 2: Add failing test for editor_canvas vertical offsets**

Append this test to the same file:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DataInputsUseEditorCanvasVerticalOffsets,
	"BlueprintHelper.GraphLayout.Solver.DataInputsUseEditorCanvasVerticalOffsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataInputsUseEditorCanvasVerticalOffsets::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print String"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("Pure")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Pure"),
		TEXT("K2Node_CallFunction"),
		TEXT("Get Display Name"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Exec")})}));

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, MakeRuleSetWithCanvasOffsets());
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* PurePlacement = FindPlacement(Plan, TEXT("Pure"));

	TestNotNull(TEXT("Exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("Pure placement exists"), PurePlacement);
	if (!ExecPlacement || !PurePlacement)
	{
		return false;
	}

	TestTrue(TEXT("Pure input is below exec by editor canvas role offset"), PurePlacement->TargetPosition.Y > ExecPlacement->TargetPosition.Y + 80.0f);
	TestTrue(TEXT("Pure input remains left of consumer"), PurePlacement->TargetPosition.X < ExecPlacement->TargetPosition.X);
	return true;
}
```

Expected before implementation: FAIL because current solver places first data input at the same Y as `Exec`.

- [x] **Step 2A: Add failing test for unsafe editor_canvas normalization**

Append this test before the existing blocker test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DataInputCanvasOffsetsAreNormalizedLeftAndBelow,
	"BlueprintHelper.GraphLayout.Solver.DataInputCanvasOffsetsAreNormalizedLeftAndBelow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DataInputCanvasOffsetsAreNormalizedLeftAndBelow::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Set Value"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("Value"), EPinDirection::Input, false, {TEXT("Pure")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Pure"),
		TEXT("K2Node_CallFunction"),
		TEXT("Unsafe Canvas Pure"),
		FVector2D::ZeroVector,
		FVector2D(200.0f, 80.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Exec")})}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::ExecNode, FVector2D(300.0f, 100.0f));
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::PureFunction, FVector2D(560.0f, 40.0f));

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ExecPlacement = FindPlacement(Plan, TEXT("Exec"));
	const FNodePlacement* PurePlacement = FindPlacement(Plan, TEXT("Pure"));

	TestNotNull(TEXT("Exec placement exists"), ExecPlacement);
	TestNotNull(TEXT("Pure placement exists"), PurePlacement);
	if (!ExecPlacement || !PurePlacement)
	{
		return false;
	}

	TestTrue(TEXT("Unsafe right-side canvas input falls back to left side"), PurePlacement->TargetPosition.X < ExecPlacement->TargetPosition.X);
	TestTrue(TEXT("Unsafe upper canvas input falls back below consumer"), PurePlacement->TargetPosition.Y > ExecPlacement->TargetPosition.Y);
	return true;
}
```

Expected before safe anchor normalization: FAIL if runtime uses raw right/up canvas deltas.

- [x] **Step 3: Add failing test for avoiding existing nodes**

Append this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_GeneratedDataInputsAvoidExistingNodes,
	"BlueprintHelper.GraphLayout.Solver.GeneratedDataInputsAvoidExistingNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_GeneratedDataInputsAvoidExistingNodes::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Event"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Event"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_CallFunction"),
		TEXT("Set Actor Location"),
		FVector2D::ZeroVector,
		FVector2D(240.0f, 100.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("Event")}),
			MakePin(TEXT("NewLocation"), EPinDirection::Input, false, {TEXT("MakeVector")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeVector"),
		TEXT("K2Node_CallFunction"),
		TEXT("Make Vector"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ReturnValue"), EPinDirection::Output, false, {TEXT("Exec")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExistingBlocker"),
		TEXT("K2Node_CallFunction"),
		TEXT("User Existing Node"),
		FVector2D(140.0f, 120.0f),
		FVector2D(260.0f, 120.0f),
		true,
		{}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	RuleSet.EditorCanvasRoleCenters.Add(ENodeRole::PureFunction, FVector2D(120.0f, 220.0f));
	RuleSet.CollisionPaddingX = 40.0f;
	RuleSet.CollisionPaddingY = 30.0f;
	RuleSet.CollisionStepY = 60.0f;
	RuleSet.MaxCollisionAttempts = 16;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* DataPlacement = FindPlacement(Plan, TEXT("MakeVector"));
	const FNodePlacement* BlockerPlacement = FindPlacement(Plan, TEXT("ExistingBlocker"));

	TestNotNull(TEXT("Data placement exists"), DataPlacement);
	TestNotNull(TEXT("Existing blocker placement exists"), BlockerPlacement);
	if (!DataPlacement || !BlockerPlacement)
	{
		return false;
	}

	TestFalse(
		TEXT("generated data input does not overlap existing blocker"),
		RectsOverlap(*DataPlacement, FVector2D(220.0f, 90.0f), *BlockerPlacement, FVector2D(260.0f, 120.0f)));
	TestFalse(TEXT("existing blocker is not moved"), BlockerPlacement->bMoveExisting);
	return true;
}
```

Expected before implementation: FAIL because current solver has no occupancy resolver.

- [x] **Step 4: Add failing test for multiple roots/custom events**

Append this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_MultipleExecRootsUseDistinctRows,
	"BlueprintHelper.GraphLayout.Solver.MultipleExecRootsUseDistinctRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_MultipleExecRootsUseDistinctRows::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.GraphName = TEXT("EventGraph");
	Snapshot.Nodes.Add(MakeNode(
		TEXT("CustomA"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Custom A"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ExecA")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExecA"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print A"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("CustomA")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("CustomB"),
		TEXT("K2Node_CustomEvent"),
		TEXT("Custom B"),
		FVector2D::ZeroVector,
		FVector2D(180.0f, 80.0f),
		false,
		{MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ExecB")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ExecB"),
		TEXT("K2Node_CallFunction"),
		TEXT("Print B"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{MakePin(TEXT("ExecIn"), EPinDirection::Input, true, {TEXT("CustomB")})}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 420.0f;
	RuleSet.ExecRowSpacing = 180.0f;
	RuleSet.CollisionPaddingY = 40.0f;
	RuleSet.CollisionStepY = 80.0f;
	RuleSet.MaxCollisionAttempts = 16;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* CustomA = FindPlacement(Plan, TEXT("CustomA"));
	const FNodePlacement* CustomB = FindPlacement(Plan, TEXT("CustomB"));
	const FNodePlacement* ExecA = FindPlacement(Plan, TEXT("ExecA"));
	const FNodePlacement* ExecB = FindPlacement(Plan, TEXT("ExecB"));

	TestNotNull(TEXT("CustomA placement exists"), CustomA);
	TestNotNull(TEXT("CustomB placement exists"), CustomB);
	TestNotNull(TEXT("ExecA placement exists"), ExecA);
	TestNotNull(TEXT("ExecB placement exists"), ExecB);
	if (!CustomA || !CustomB || !ExecA || !ExecB)
	{
		return false;
	}

	TestNotEqual(TEXT("custom event roots do not share Y"), CustomA->TargetPosition.Y, CustomB->TargetPosition.Y);
	TestNotEqual(TEXT("exec successors do not share Y"), ExecA->TargetPosition.Y, ExecB->TargetPosition.Y);
	return true;
}

#endif
```

Expected before implementation: PASS or FAIL depending on current root ordering, but it guards the multi-root regression after occupancy changes.

- [x] **Step 5: Run RED tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Build may fail at this step because `FRuleSet::CollisionPaddingX`, `CollisionPaddingY`, `CollisionStepY`, and `MaxCollisionAttempts` are not defined yet.
- If the fields are temporarily removed to run current solver behavior, the first two tests fail for same-line data input and overlap.

- [x] **Step 6: Checkpoint**

Do not run git add or commit. Record that the RED test file exists and list it in the implementation worker's status.

---

## Task 2: Extend RuleSet JSON With Occupancy Parameters

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Add RuleSet fields**

In `FRuleSet`, add these fields after `InputPinRowSpacing`:

```cpp
float CollisionPaddingX = 60.0f;
float CollisionPaddingY = 40.0f;
float CollisionStepY = 64.0f;
int32 MaxCollisionAttempts = 64;
```

Rationale:

- `CollisionPaddingX/Y` expands node rects so visual wires and node shadows have breathing room.
- `CollisionStepY` controls downward search increments.
- `MaxCollisionAttempts` bounds worker solve time.

- [x] **Step 2: Export the fields to JSON**

In `ToJson(const FRuleSet& RuleSet)`, after `input_pin_row_spacing`, add:

```cpp
Json->SetNumberField(TEXT("collision_padding_x"), RuleSet.CollisionPaddingX);
Json->SetNumberField(TEXT("collision_padding_y"), RuleSet.CollisionPaddingY);
Json->SetNumberField(TEXT("collision_step_y"), RuleSet.CollisionStepY);
Json->SetNumberField(TEXT("max_collision_attempts"), RuleSet.MaxCollisionAttempts);
```

- [x] **Step 3: Validate the JSON fields**

In `FRuleSetJson::Validate`, after `input_pin_row_spacing`, add:

```cpp
TryReadPositiveNumber(Json, TEXT("collision_padding_x"), Defaults.CollisionPaddingX, Validation);
TryReadPositiveNumber(Json, TEXT("collision_padding_y"), Defaults.CollisionPaddingY, Validation);
TryReadPositiveNumber(Json, TEXT("collision_step_y"), Defaults.CollisionStepY, Validation);
TryReadPositiveInt(Json, TEXT("max_collision_attempts"), Defaults.MaxCollisionAttempts, Validation);
```

Inside the existing `solver` object validation block, add aliases:

```cpp
TryReadPositiveNumber(*SolverObject, TEXT("collision_padding_x"), Defaults.CollisionPaddingX, Validation);
TryReadPositiveNumber(*SolverObject, TEXT("collision_padding_y"), Defaults.CollisionPaddingY, Validation);
TryReadPositiveNumber(*SolverObject, TEXT("collision_step_y"), Defaults.CollisionStepY, Validation);
TryReadPositiveInt(*SolverObject, TEXT("max_collision_attempts"), Defaults.MaxCollisionAttempts, Validation);
```

- [x] **Step 4: Import the JSON fields**

In `FRuleSetJson::Import`, after `input_pin_row_spacing`, add:

```cpp
TryReadPositiveNumber(Json, TEXT("collision_padding_x"), OutRuleSet.CollisionPaddingX, OutValidation);
TryReadPositiveNumber(Json, TEXT("collision_padding_y"), OutRuleSet.CollisionPaddingY, OutValidation);
TryReadPositiveNumber(Json, TEXT("collision_step_y"), OutRuleSet.CollisionStepY, OutValidation);
TryReadPositiveInt(Json, TEXT("max_collision_attempts"), OutRuleSet.MaxCollisionAttempts, OutValidation);
```

Inside the existing `solver` object import block, add the same aliases:

```cpp
TryReadPositiveNumber(*SolverObject, TEXT("collision_padding_x"), OutRuleSet.CollisionPaddingX, OutValidation);
TryReadPositiveNumber(*SolverObject, TEXT("collision_padding_y"), OutRuleSet.CollisionPaddingY, OutValidation);
TryReadPositiveNumber(*SolverObject, TEXT("collision_step_y"), OutRuleSet.CollisionStepY, OutValidation);
TryReadPositiveInt(*SolverObject, TEXT("max_collision_attempts"), OutRuleSet.MaxCollisionAttempts, OutValidation);
```

- [x] **Step 5: Add JSON round-trip test**

Append this test before `#endif` in `BlueprintHelperGraphLayoutSolverTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsCollisionSettings,
	"BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsCollisionSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsCollisionSettings::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.CollisionPaddingX = 73.0f;
	RuleSet.CollisionPaddingY = 41.0f;
	RuleSet.CollisionStepY = 67.0f;
	RuleSet.MaxCollisionAttempts = 19;

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("json imports"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	TestEqual(TEXT("collision padding x"), Parsed.CollisionPaddingX, 73.0f);
	TestEqual(TEXT("collision padding y"), Parsed.CollisionPaddingY, 41.0f);
	TestEqual(TEXT("collision step y"), Parsed.CollisionStepY, 67.0f);
	TestEqual(TEXT("max collision attempts"), Parsed.MaxCollisionAttempts, 19);
	return true;
}
```

The required `BlueprintHelperGraphLayoutRuleSetJson.h` include is already listed in the Task 1 file header.

- [x] **Step 6: Run focused build**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Build succeeds if only type/JSON fields are missing blockers.
- Solver behavior tests still fail until later tasks implement role anchors and occupancy.

- [x] **Step 7: Checkpoint**

Do not commit. Record modified files:

```text
BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h
BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp
```

---

## Task 3: Add Role Anchor Resolver

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Create public header**

Create `BlueprintHelperGraphLayoutRoleAnchorResolver.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FRoleAnchor
{
	FVector2D OffsetFromConsumer = FVector2D::ZeroVector;
	bool bFromEditorCanvas = false;
};

class BLUEPRINTHELPER_API FRoleAnchorResolver
{
public:
	static FRoleAnchor ResolveDataInputAnchor(const FRuleSet& RuleSet, ENodeRole Role);
};
}
```

- [x] **Step 2: Implement editor_canvas based offsets**

Create `BlueprintHelperGraphLayoutRoleAnchorResolver.cpp`:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h"

namespace BlueprintHelper::GraphLayout
{
static FVector2D GetFallbackOffset(const FRuleSet& RuleSet, ENodeRole Role)
{
	switch (Role)
	{
	case ENodeRole::VariableInput:
		return FVector2D(-RuleSet.VariableInputOffsetX, RuleSet.InputPinRowSpacing);
	case ENodeRole::OperatorOrCompare:
		return FVector2D(-RuleSet.PureInputOffsetX, RuleSet.InputPinRowSpacing * 2.0f);
	case ENodeRole::PureFunction:
		return FVector2D(-RuleSet.PureInputOffsetX, RuleSet.InputPinRowSpacing);
	default:
		return FVector2D(-RuleSet.PureInputOffsetX, RuleSet.InputPinRowSpacing);
	}
}

FRoleAnchor FRoleAnchorResolver::ResolveDataInputAnchor(const FRuleSet& RuleSet, ENodeRole Role)
{
	FRoleAnchor Anchor;
	Anchor.OffsetFromConsumer = GetFallbackOffset(RuleSet, Role);

	const FVector2D* ExecCenter = RuleSet.EditorCanvasRoleCenters.Find(ENodeRole::ExecNode);
	const FVector2D* RoleCenter = RuleSet.EditorCanvasRoleCenters.Find(Role);
	if (!ExecCenter || !RoleCenter)
	{
		return Anchor;
	}

	const FVector2D RawOffset = *RoleCenter - *ExecCenter;
	if (!FMath::IsFinite(RawOffset.X) || !FMath::IsFinite(RawOffset.Y))
	{
		return Anchor;
	}

	const bool bCanvasOffsetIsLeftAndBelow = RawOffset.X < -1.0f && RawOffset.Y > 1.0f;
	if (!bCanvasOffsetIsLeftAndBelow)
	{
		return Anchor;
	}

	Anchor.OffsetFromConsumer.X = FMath::Clamp(RawOffset.X, -1200.0f, -1.0f);
	Anchor.OffsetFromConsumer.Y = FMath::Clamp(RawOffset.Y, 1.0f, 1200.0f);
	Anchor.bFromEditorCanvas = true;
	return Anchor;
}
}
```

Runtime must treat data input anchors as left-and-below anchors. If a user drags a data role to the right of or above `ExecNode`, the visual editor can still save the canvas state, but solver must fall back to the safe default offset instead of placing input producers on the wrong side.

- [x] **Step 3: Replace data input offset helper usage**

In `BlueprintHelperGraphLayoutSolver.cpp`, include:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h"
```

Then replace `GetDataInputOffsetX()` usage in the data input alignment path with `FRoleAnchorResolver::ResolveDataInputAnchor()`.

The target calculation should become:

```cpp
const FRoleAnchor Anchor = FRoleAnchorResolver::ResolveDataInputAnchor(RuleSet, SourceNode->Role);
const FVector2D Target(
	ConsumerNode->Target.X + Anchor.OffsetFromConsumer.X,
	ConsumerNode->Target.Y + Anchor.OffsetFromConsumer.Y + InputOrder * RuleSet.InputPinRowSpacing);
```

- [x] **Step 4: Run focused tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Build succeeds.
- `DataInputsUseEditorCanvasVerticalOffsets` passes.
- `GeneratedDataInputsAvoidExistingNodes` still fails until occupancy is added.

- [x] **Step 5: Checkpoint**

Do not commit. Note that `editor_canvas.role_centers` is now a runtime input through `FRoleAnchorResolver`, not UI-only metadata.

---

## Task 4: Add Occupancy Resolver

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Create public header**

Create `BlueprintHelperGraphLayoutOccupancyResolver.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FLayoutRect
{
	FString NodeId;
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	bool bMovable = false;
};

class BLUEPRINTHELPER_API FOccupancyResolver
{
public:
	explicit FOccupancyResolver(const FRuleSet& InRuleSet);

	void ReserveExistingNode(const FNodeSnapshot& Node);
	void ReserveTarget(const FString& NodeId, const FVector2D& TargetPosition, const FVector2D& Size, bool bMovable);
	FVector2D ResolveNearestFreeTarget(const FString& NodeId, const FVector2D& DesiredPosition, const FVector2D& Size) const;
	bool WouldOverlap(const FString& NodeId, const FVector2D& TargetPosition, const FVector2D& Size) const;

private:
	FLayoutRect MakeRect(const FString& NodeId, const FVector2D& Position, const FVector2D& Size, bool bMovable) const;
	bool OverlapsAny(const FLayoutRect& Candidate) const;

	const FRuleSet& RuleSet;
	TArray<FLayoutRect> ReservedRects;
};
}
```

- [x] **Step 2: Implement downward candidate search**

Create `BlueprintHelperGraphLayoutOccupancyResolver.cpp`:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"

namespace BlueprintHelper::GraphLayout
{
FOccupancyResolver::FOccupancyResolver(const FRuleSet& InRuleSet)
	: RuleSet(InRuleSet)
{
}

FLayoutRect FOccupancyResolver::MakeRect(
	const FString& NodeId,
	const FVector2D& Position,
	const FVector2D& Size,
	bool bMovable) const
{
	const FVector2D Padding(
		FMath::Max(0.0f, RuleSet.CollisionPaddingX),
		FMath::Max(0.0f, RuleSet.CollisionPaddingY));

	FLayoutRect Rect;
	Rect.NodeId = NodeId;
	Rect.Min = Position - Padding;
	Rect.Max = Position + Size + Padding;
	Rect.bMovable = bMovable;
	return Rect;
}

static bool RectsOverlap(const FLayoutRect& A, const FLayoutRect& B)
{
	return A.Min.X < B.Max.X && A.Max.X > B.Min.X && A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y;
}

bool FOccupancyResolver::OverlapsAny(const FLayoutRect& Candidate) const
{
	for (const FLayoutRect& Reserved : ReservedRects)
	{
		if (Reserved.NodeId == Candidate.NodeId)
		{
			continue;
		}
		if (RectsOverlap(Candidate, Reserved))
		{
			return true;
		}
	}
	return false;
}

void FOccupancyResolver::ReserveExistingNode(const FNodeSnapshot& Node)
{
	ReserveTarget(Node.NodeId, Node.Position, Node.Size, false);
}

void FOccupancyResolver::ReserveTarget(
	const FString& NodeId,
	const FVector2D& TargetPosition,
	const FVector2D& Size,
	bool bMovable)
{
	ReservedRects.Add(MakeRect(NodeId, TargetPosition, Size, bMovable));
}

bool FOccupancyResolver::WouldOverlap(
	const FString& NodeId,
	const FVector2D& TargetPosition,
	const FVector2D& Size) const
{
	return OverlapsAny(MakeRect(NodeId, TargetPosition, Size, true));
}

FVector2D FOccupancyResolver::ResolveNearestFreeTarget(
	const FString& NodeId,
	const FVector2D& DesiredPosition,
	const FVector2D& Size) const
{
	const int32 MaxAttempts = FMath::Max(1, RuleSet.MaxCollisionAttempts);
	const float StepY = FMath::Max(1.0f, RuleSet.CollisionStepY);

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const FVector2D Candidate = DesiredPosition + FVector2D(0.0f, Attempt * StepY);
		if (!WouldOverlap(NodeId, Candidate, Size))
		{
			return Candidate;
		}
	}

	return DesiredPosition + FVector2D(0.0f, MaxAttempts * StepY);
}
}
```

- [x] **Step 3: Seed occupancy from existing nodes**

In `FSolver::Solve`, after creating the working node map, create an occupancy resolver and seed existing blockers:

```cpp
FOccupancyResolver Occupancy(RuleSet);
for (const FNodeSnapshot& Node : Snapshot.Nodes)
{
	if (Node.bExisting && !RuleSet.bMoveExistingNodes)
	{
		Occupancy.ReserveExistingNode(Node);
	}
}
```

Include:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"
```

- [x] **Step 4: Reserve every target as it is assigned**

Change the target-setting path so solver reserves occupied rects after a target is finalized. The simplest implementation is to stop using `SetTarget()` for final target writes and replace calls with:

```cpp
static void SetTarget(
	TMap<FString, FWorkingNode>& Nodes,
	FOccupancyResolver& Occupancy,
	const FString& NodeId,
	const FVector2D& DesiredTarget,
	const FString& Reason)
{
	if (FWorkingNode* Node = Nodes.Find(NodeId))
	{
		const FVector2D Size = Node->Snapshot ? Node->Snapshot->Size : FVector2D(180.0f, 80.0f);
		const FVector2D Target = Occupancy.ResolveNearestFreeTarget(NodeId, DesiredTarget, Size);
		Node->Target = Target;
		Node->bHasTarget = true;
		Node->Reason = Target.Equals(DesiredTarget)
			? Reason
			: FString::Printf(TEXT("%s_avoided_overlap"), *Reason);
		Occupancy.ReserveTarget(NodeId, Target, Size, true);
	}
}
```

Update `LayoutExecChain()` signature to accept `FOccupancyResolver& Occupancy`.

- [x] **Step 5: Run tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Build succeeds.
- Existing blocker overlap test passes or exposes ordering issues to fix in Task 5.
- Multiple roots remain non-overlapping.

- [x] **Step 6: Checkpoint**

Do not commit. Record new files and any solver signature changes.

---

## Task 5: Replace Data Input Alignment With Dedicated Placement Service

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Create data input placement header**

Create `BlueprintHelperGraphLayoutDataInputPlacement.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FDataInputPlacementRequest
{
	FString ConsumerNodeId;
	FString SourceNodeId;
	ENodeRole SourceRole = ENodeRole::Unknown;
	int32 InputOrder = 0;
	FVector2D ConsumerTarget = FVector2D::ZeroVector;
	FVector2D SourceSize = FVector2D(180.0f, 80.0f);
};

class BLUEPRINTHELPER_API FDataInputPlacement
{
public:
	static bool IsDataInputRole(ENodeRole Role);
	static FVector2D BuildDesiredTarget(const FRuleSet& RuleSet, const FDataInputPlacementRequest& Request);
	static const TCHAR* GetReason(ENodeRole Role);
};
}
```

- [x] **Step 2: Implement shared placement math**

Create `BlueprintHelperGraphLayoutDataInputPlacement.cpp`:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h"

namespace BlueprintHelper::GraphLayout
{
bool FDataInputPlacement::IsDataInputRole(ENodeRole Role)
{
	return Role == ENodeRole::VariableInput ||
		Role == ENodeRole::PureFunction ||
		Role == ENodeRole::OperatorOrCompare;
}

FVector2D FDataInputPlacement::BuildDesiredTarget(
	const FRuleSet& RuleSet,
	const FDataInputPlacementRequest& Request)
{
	const FRoleAnchor Anchor = FRoleAnchorResolver::ResolveDataInputAnchor(RuleSet, Request.SourceRole);
	return FVector2D(
		Request.ConsumerTarget.X + Anchor.OffsetFromConsumer.X,
		Request.ConsumerTarget.Y + Anchor.OffsetFromConsumer.Y + Request.InputOrder * RuleSet.InputPinRowSpacing);
}

const TCHAR* FDataInputPlacement::GetReason(ENodeRole Role)
{
	switch (Role)
	{
	case ENodeRole::VariableInput:
		return TEXT("node_input_variable_alignment");
	case ENodeRole::OperatorOrCompare:
		return TEXT("node_input_operator_or_compare_alignment");
	case ENodeRole::PureFunction:
		return TEXT("node_input_pure_alignment");
	default:
		return TEXT("node_input_alignment");
	}
}
}
```

- [x] **Step 3: Update solver to use the service**

In `BlueprintHelperGraphLayoutSolver.cpp`:

- Remove local `IsDataInputRole()`, `GetDataInputOffsetX()`, and `GetDataInputAlignmentReason()` or delegate them to `FDataInputPlacement`.
- Include:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h"
```

Replace the body of `AlignInputsToConsumerPinOrder()` with logic equivalent to:

```cpp
static bool AlignInputsToConsumerPinOrder(
	const FGraphSnapshot& Snapshot,
	TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet,
	FOccupancyResolver& Occupancy)
{
	bool bChanged = false;
	for (const FNodeSnapshot& ConsumerSnapshot : Snapshot.Nodes)
	{
		FWorkingNode* ConsumerNode = Nodes.Find(ConsumerSnapshot.NodeId);
		if (!ConsumerNode || !ConsumerNode->Snapshot || !ConsumerNode->bHasTarget)
		{
			continue;
		}

		int32 InputOrder = 0;
		for (const FPinSnapshot& Pin : ConsumerNode->Snapshot->Pins)
		{
			if (Pin.Direction != EPinDirection::Input || Pin.bExec)
			{
				continue;
			}

			for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
			{
				FWorkingNode* SourceNode = Nodes.Find(LinkedNodeId);
				if (!SourceNode || !SourceNode->Snapshot)
				{
					continue;
				}

				if (FDataInputPlacement::IsDataInputRole(SourceNode->Role) && !SourceNode->bHasTarget)
				{
					FDataInputPlacementRequest Request;
					Request.ConsumerNodeId = ConsumerSnapshot.NodeId;
					Request.SourceNodeId = LinkedNodeId;
					Request.SourceRole = SourceNode->Role;
					Request.InputOrder = InputOrder;
					Request.ConsumerTarget = ConsumerNode->Target;
					Request.SourceSize = SourceNode->Snapshot->Size;

					const FVector2D DesiredTarget = FDataInputPlacement::BuildDesiredTarget(RuleSet, Request);
					SetTarget(
						Nodes,
						Occupancy,
						LinkedNodeId,
						DesiredTarget,
						FDataInputPlacement::GetReason(SourceNode->Role));
					bChanged = true;
				}
			}
			++InputOrder;
		}
	}
	return bChanged;
}
```

This deliberately applies to every consumer node with a target, not only exec nodes. That implements the hard rule: every node input is laid out like an ExecNode input.

- [x] **Step 3A: Define target_pin_order_variable_input_alignment semantics**

Do not remove `bUseTargetPinOrderForVariableInputs` in this round. Its runtime meaning becomes:

```cpp
// true: variable inputs use the consumer pin order as their primary vertical order.
// false: variable inputs still use the same left-and-below anchor, then rely on occupancy search for final vertical separation.
```

Implement this inside `AlignInputsToConsumerPinOrder()` by computing the order before building the request:

```cpp
const int32 PlacementOrder =
	SourceNode->Role == ENodeRole::VariableInput && !RuleSet.bUseTargetPinOrderForVariableInputs
		? 0
		: InputOrder;
Request.InputOrder = PlacementOrder;
```

PureFunction and OperatorOrCompare always use consumer input pin order.

- [x] **Step 4: Preserve recursive data placement**

Keep the existing pass loop:

```cpp
for (int32 PassIndex = 0; PassIndex < Snapshot.Nodes.Num(); ++PassIndex)
{
	if (!AlignInputsToConsumerPinOrder(Snapshot, Nodes, RuleSet, Occupancy))
	{
		break;
	}
}
```

This allows pure node inputs to be placed after the pure node itself receives a target.

- [x] **Step 5: Run tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- All GraphLayout solver tests compile.
- Data input vertical offset test passes.
- Existing node avoidance test passes.
- Multi-root test passes.

- [x] **Step 6: Checkpoint**

Do not commit. Record that runtime now consumes `editor_canvas.role_centers` and applies data input placement recursively.

---

## Task 6: Make Exec Backbone Use Global Occupancy And Row Reservation

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Move UsedRows out of each chain**

Change `LayoutExecChain()` signature from local row reservation to caller-owned rows:

```cpp
static void LayoutExecChain(
	const FString& RootNodeId,
	TMap<FString, FWorkingNode>& Nodes,
	const FRuleSet& RuleSet,
	FOccupancyResolver& Occupancy,
	TMap<int32, TSet<int32>>& UsedRows,
	int32 RootRow,
	TSet<FString>& Visited)
```

Remove the local declaration:

```cpp
TMap<int32, TSet<int32>> UsedRows;
```

- [x] **Step 2: Use shared rows for all roots**

In `FSolver::Solve`, before root loop:

```cpp
TMap<int32, TSet<int32>> UsedRows;
```

Call:

```cpp
LayoutExecChain(Roots[RootIndex], Nodes, RuleSet, Occupancy, UsedRows, RootIndex * 3, VisitedExecNodes);
```

For unvisited exec nodes:

```cpp
int32 DetachedRootRow = FMath::Max(1, Roots.Num()) * 3;
for (const FNodeSnapshot& Node : Snapshot.Nodes)
{
	const ENodeRole Role = RolesById.FindRef(Node.NodeId);
	if (IsExecRole(Role) && !VisitedExecNodes.Contains(Node.NodeId))
	{
		while (UsedRows.FindOrAdd(0).Contains(DetachedRootRow))
		{
			++DetachedRootRow;
		}
		LayoutExecChain(Node.NodeId, Nodes, RuleSet, Occupancy, UsedRows, DetachedRootRow, VisitedExecNodes);
		DetachedRootRow += 3;
	}
}
```

- [x] **Step 3: Route exec target through occupancy**

Inside `LayoutExecChain()`, replace direct target write:

```cpp
const FVector2D Target(Item.Column * RuleSet.ExecColumnSpacing, Row * RuleSet.ExecRowSpacing);
SetTarget(Nodes, Item.NodeId, Target, TEXT("exec_flow"));
```

with the new `SetTarget()` overload that accepts `Occupancy`.

- [x] **Step 4: Run tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Multi-root test passes.
- Existing blockers do not overlap with exec chain placements when `move_existing_nodes=false`.

- [x] **Step 5: Checkpoint**

Do not commit. Record solver now has global row reservation and global occupancy.

---

## Task 7: Update Layout Rule Editor Settings Panel

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`
- Test: compile

- [x] **Step 1: Add setting enum values**

In `SBlueprintHelperLayoutRuleEditor.cpp`, extend `EFloatSetting`:

```cpp
enum EFloatSetting : int32
{
	ExecColumnSpacing = 0,
	ExecRowSpacing,
	BranchRowSpacing,
	PureInputOffsetX,
	VariableInputOffsetX,
	InputPinRowSpacing,
	CollisionPaddingX,
	CollisionPaddingY,
	CollisionStepY,
	MaxMillisecondsPerFrame
};
```

Extend `EIntSetting`:

```cpp
enum EIntSetting : int32
{
	MaxNodesPerFrame = 0,
	MaxCollisionAttempts
};
```

- [x] **Step 2: Add widget state fields**

In `SBlueprintHelperLayoutRuleEditor.h`, after `SettingsInputPinRowSpacing`:

```cpp
float SettingsCollisionPaddingX = 60.0f;
float SettingsCollisionPaddingY = 40.0f;
float SettingsCollisionStepY = 64.0f;
int32 SettingsMaxCollisionAttempts = 64;
```

- [x] **Step 3: Load fields from JSON into settings**

In `RefreshSettingsFromJson()`, after `SettingsInputPinRowSpacing`:

```cpp
SettingsCollisionPaddingX = ParsedRuleSet.CollisionPaddingX;
SettingsCollisionPaddingY = ParsedRuleSet.CollisionPaddingY;
SettingsCollisionStepY = ParsedRuleSet.CollisionStepY;
SettingsMaxCollisionAttempts = ParsedRuleSet.MaxCollisionAttempts;
```

- [x] **Step 4: Write settings back to JSON**

In `HandleFloatSettingChanged()`, add cases:

```cpp
case CollisionPaddingX:
	ParsedRuleSet.CollisionPaddingX = NewValue;
	SettingsCollisionPaddingX = NewValue;
	break;
case CollisionPaddingY:
	ParsedRuleSet.CollisionPaddingY = NewValue;
	SettingsCollisionPaddingY = NewValue;
	break;
case CollisionStepY:
	ParsedRuleSet.CollisionStepY = NewValue;
	SettingsCollisionStepY = NewValue;
	break;
```

In `HandleIntSettingChanged()`, add:

```cpp
case MaxCollisionAttempts:
	ParsedRuleSet.MaxCollisionAttempts = NewValue;
	SettingsMaxCollisionAttempts = NewValue;
	break;
```

- [x] **Step 5: Add Chinese setting rows**

In `BuildSettingsPanel()`, under spacing or a new "避让" section, add:

```cpp
+ SScrollBox::Slot()
.Padding(0.0f, 0.0f, 0.0f, 6.0f)
[
	BuildSettingsSectionHeader(LOCTEXT("SettingsCollisionHeader", "避让"))
]
+ SScrollBox::Slot()
.Padding(0.0f, 0.0f, 0.0f, 5.0f)
[
	BuildFloatSettingRow(
		LOCTEXT("CollisionPaddingXLabel", "水平留白"),
		LOCTEXT("CollisionPaddingXTooltip", "节点避让时在左右方向额外保留的距离，用于减少节点和连线挤在一起。"),
		[this]() { return SettingsCollisionPaddingX; },
		[this](float NewValue) { HandleFloatSettingChanged(CollisionPaddingX, NewValue); },
		0.0f,
		400.0f,
		5.0f)
]
+ SScrollBox::Slot()
.Padding(0.0f, 0.0f, 0.0f, 5.0f)
[
	BuildFloatSettingRow(
		LOCTEXT("CollisionPaddingYLabel", "垂直留白"),
		LOCTEXT("CollisionPaddingYTooltip", "节点避让时在上下方向额外保留的距离，用于避免节点互相压住。"),
		[this]() { return SettingsCollisionPaddingY; },
		[this](float NewValue) { HandleFloatSettingChanged(CollisionPaddingY, NewValue); },
		0.0f,
		400.0f,
		5.0f)
]
+ SScrollBox::Slot()
.Padding(0.0f, 0.0f, 0.0f, 5.0f)
[
	BuildFloatSettingRow(
		LOCTEXT("CollisionStepYLabel", "下移步长"),
		LOCTEXT("CollisionStepYTooltip", "候选位置被占用时，每次向下寻找空位的距离。"),
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
		LOCTEXT("MaxCollisionAttemptsLabel", "最大尝试"),
		LOCTEXT("MaxCollisionAttemptsTooltip", "寻找空位时最多尝试的次数，数值越大越能避让复杂图，但求解耗时也会增加。"),
		[this]() { return SettingsMaxCollisionAttempts; },
		[this](int32 NewValue) { HandleIntSettingChanged(MaxCollisionAttempts, NewValue); },
		1,
		256)
]
```

- [x] **Step 6: Repair existing Layout panel tooltips to Chinese**

Replace garbled tooltip strings in `SBlueprintHelperLayoutRuleEditor.cpp` with readable Chinese. Use these exact meanings:

```text
Import JSON: 通过已绑定的配置入口导入 RuleSet JSON；未绑定时从默认配置文件读取。
Export JSON: 通过已绑定的配置入口导出当前 RuleSet JSON；未绑定时写入默认配置文件。
Copy JSON: 将当前 RuleSet JSON 文本复制到剪贴板。
Paste JSON: 用剪贴板内容替换当前 RuleSet JSON 文本。
Validate: 校验当前 RuleSet JSON；已绑定的 GraphLayout 校验器会提供 schema 级检查。
Reset to Default: 用已配置的默认 RuleSet JSON 替换当前文本。
Rule ID: 布局规则集的稳定标识。
Display name: 布局规则集在界面中显示的名称。
Exec column: 执行节点之间的水平间距。
Exec row: 执行链路之间的垂直间距。
Branch row: 分支链路使用的垂直间距。
Pure offset: 纯函数和运算/比较输入节点的左侧偏移。
Variable offset: 变量输入节点的左侧偏移。
Input pin row: 输入引脚行之间的垂直间距。
Nodes / frame: 每个编辑器帧最多应用的布局节点移动数量。
MS / frame: 每个编辑器帧最多用于应用布局的时间。
Move generated nodes: 允许布局移动本次 Task 生成的节点。
Move existing nodes: 允许布局移动图中已有的用户节点。
Mark dirty after apply: 布局位置变更后将图所在包标记为 dirty。
Save after apply: 布局位置变更应用后保存图所在包。
```

- [x] **Step 7: Compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Build succeeds.
- The Layout widget still compiles with Slate.

- [x] **Step 8: Checkpoint**

Do not commit. Record UI files changed and note that UI only edits JSON; runtime logic remains in GraphLayout services.

---

## Task 8: Verify Config Path And Runtime Boundaries

**Files:**
- Read: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSourceResolver.cpp`
- Read: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Config/BlueprintHelperProjectConfigPaths.h`
- Read: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- Read: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.cpp`
- Read-only confirmation only. If current source contradicts this plan, record a follow-up plan instead of modifying Runtime, GraphWrite, Signature, or Review files in this implementation.

- [x] **Step 1: Confirm GraphLayoutRules path**

Run:

```powershell
rg -n "GetGraphLayoutRulesPath|ResolveRuleSourcePath|GraphLayoutRules.json" BlueprintHelper/Source
```

Expected:

- `GraphLayoutRules.json` resolves under project `.blueprinthelper`, same level as agent profile.
- If the path has drifted, stop this implementation task and record a separate config-path follow-up. Do not fix path resolution inside this occupancy/data-input implementation.

- [x] **Step 2: Confirm TaskRun boundary**

Run:

```powershell
rg -n "FlushGraphLayout|FScopedBlueprintHelperGraphLayoutTask|RecordGeneratedNodes|DiscardPendingTaskLayouts" BlueprintHelper/Source/BlueprintHelper/Private/Runtime BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters
```

Expected:

- `RecordGeneratedNodes()` happens inside GraphWrite/signature services after nodes are generated.
- `FlushGraphLayout()` happens after the TaskPlan has produced all content.
- Apply remains async and frame-limited in coordinator.

- [x] **Step 3: Keep coordinator unchanged unless tests expose apply ordering bugs**

No code change is planned in coordinator for this round. If a test or runtime smoke proves stale apply plans can override newer positions, record it as a follow-up plan for per-graph epoch/coalesce.

- [x] **Step 4: Checkpoint**

Do not commit. Record whether coordinator stayed unchanged.

---

## Task 9: Run Automation And Manual Smoke

**Files:**
- No planned source edits.

- [x] **Step 1: Build the editor target**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- `BUILD SUCCESSFUL` or equivalent UBT success output.

- [x] **Step 2: Run focused UE automation if editor commandlet is available**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout; Quit" -TestExit="Automation Test Queue Empty"
```

Expected:

- GraphLayout tests pass.
- If the commandlet cannot run in the local environment, record the exact error and keep the UBT build as compile verification.

- [x] **Step 3: Manual smoke with a generated graph**

Use the existing BlueprintHelper TaskSpec execution path to generate a graph with:

- two custom events in the same target graph,
- one exec call per event,
- at least one pure function input,
- one operator/compare input,
- two variable get inputs connected to different data pins.

Expected in Blueprint editor:

- custom event roots are on separate rows,
- exec flow reads left to right,
- pure/operator/variable input nodes are below and left of their consumer according to the visual rule canvas,
- generated nodes do not overlap existing user nodes when `move_existing_nodes=false`,
- existing user nodes are not moved,
- async apply does not visibly freeze the editor.

- [x] **Step 4: Verify save_after_apply**

Set `save_after_apply` to `true` in `.blueprinthelper/GraphLayoutRules.json`, run a small generation task, then verify:

- TaskRun result completes before layout apply finishes.
- Layout apply marks/saves only after queued placements complete.
- No Review diff is created for node position changes.

- [x] **Step 5: Reset local test config if it was changed**

If `.blueprinthelper/GraphLayoutRules.json` was modified only for local smoke, restore the intended user configuration by importing/exporting through the Layout widget or by replacing only the test-specific changed values. Do not reset unrelated project config.

---

## Task 10: Final Read-Only Review

**Files:**
- Read all files changed by Tasks 1-9.

- [x] **Step 1: Dispatch one large read-only review worker**

Use a `gpt-5.4 high` read-only worker as required by AGENTS. Ask it to verify:

- new solver helper boundaries are cohesive and not UI-local,
- runtime consumes `editor_canvas.role_centers`,
- collision avoidance uses snapshot rects and respects `move_existing_nodes=false`,
- every non-exec input is placed through the shared data input placement rule,
- JSON import/export/settings panel are synchronized,
- no TaskPlan/GraphWrite/Review boundaries were expanded,
- tests cover same-line data input, existing blocker, multi-root, and JSON round-trip.

- [x] **Step 2: Fix review findings inside the same boundary**

If the reviewer finds issues, fix only files listed in this plan unless the finding proves the plan boundary wrong. Do not introduce legacy layout fallback.

- [x] **Step 3: Run final verification**

Run:

```powershell
git diff --check -- BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout
```

Expected:

- No whitespace errors.

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected:

- Build succeeds.

- [x] **Step 4: Final changed-file list**

Final output must list only files actually changed by this work. Do not include unrelated dirty files already present in the workspace.

---

## Manual Commit Commands For User

After implementation and verification, the user can stage only this task's files with:

```powershell
git add -- `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphLayout_Occupancy_DataInput_ImplementationPlan_20260531_CN.md `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRoleAnchorResolver.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp
```

Suggested commit message after implementation:

```text
新增内容：
1. 新增 GraphLayout 角色锚点、数据输入放置与占用避让服务。
2. 新增 GraphLayout solver 自动化测试覆盖数据输入下方排布、已有节点避让、多 root 排布和 RuleSet JSON 往返。

修复内容：
1. 修复 PureFunction、OperatorOrCompare、VariableInput 未消费规则画布 Y 偏移导致与 Exec 流同线的问题。
2. 修复 generated layout 未避开 existing node rect 导致节点重叠的问题。

变更需求：
1. 扩展 Layout RuleSet JSON 与右侧设置面板，支持节点避让留白、下移步长和最大避让尝试次数。
```

Commit command:

```powershell
git commit -m "新增内容：
1. 新增 GraphLayout 角色锚点、数据输入放置与占用避让服务。
2. 新增 GraphLayout solver 自动化测试覆盖数据输入下方排布、已有节点避让、多 root 排布和 RuleSet JSON 往返。

修复内容：
1. 修复 PureFunction、OperatorOrCompare、VariableInput 未消费规则画布 Y 偏移导致与 Exec 流同线的问题。
2. 修复 generated layout 未避开 existing node rect 导致节点重叠的问题。

变更需求：
1. 扩展 Layout RuleSet JSON 与右侧设置面板，支持节点避让留白、下移步长和最大避让尝试次数。"
```

---

## 2026-05-31 执行记录

- 已完成 Task 1-7：新增 RuleSet collision 参数、角色锚点解析、占用避让、数据输入放置、全局 Exec row 排布、Layout 设置面板与中文 Tips。
- 已完成 Task 8：确认 `GraphLayoutRules.json` 默认路径为项目 `.blueprinthelper/GraphLayoutRules.json`；GraphWrite/Signature 仅在生成后登记节点；TaskRuntime 在所有 step 成功后 flush layout；未新增 Review/layout diff 记录。
- 已完成 Task 9：`git diff --check` 通过，仅有 CRLF 提示；UE 5.6 `TemplateEditor` UBT 编译通过；`BlueprintHelper.GraphLayout` 自动化全组通过，报告 `D:\UEProjects\Template\Saved\Automation\GraphLayout_Full_20260531_001\index.json` 显示 `succeeded=8, failed=0`。
- 已完成 runtime smoke：通过 MCP 启动 Editor，CLI Bridge 可用；创建 smoke Blueprint；在 `save_after_apply=true` 下执行 graph append，`task_995FA1C74E10846E924103BFC9740A0F` 成功，UE log 出现 graph execute 后的 `OBJ SAVEPACKAGE`；ReadContext 验证生成图包含 `BH_LayoutSmokeEvent_20260531_2134 -> PrintString` exec link。
- smoke 后已恢复 `D:\UEProjects\Template\.blueprinthelper\GraphLayoutRules.json` 的 `save_after_apply=false`，关闭 Editor，并删除本轮临时 TaskSpec 与 smoke 资产目录。
- 最终审计发现 `ResolveNearestFreeTarget()` 的 lateral fallback 会破坏语义列；已修复为固定 `DesiredPosition.X` 的纵向 fallback，并用 `OccupancyResolverReturnsNonOverlappingEmergencyTarget` 覆盖 `MaxCollisionAttempts` exhaustion 路径。
