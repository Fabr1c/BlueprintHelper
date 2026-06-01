# GraphLayout Pattern-Based Exec Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 GraphLayout 从单一 exec-chain solver 扩展为 pattern-based 布局系统，支持默认开启的 Exec 水平对齐、多 Exec output 分支 row、Pure Data Subgraph / Data Input Cluster 空间预算，并保持 RuleSet 驱动与真实 Blueprint E2E 验证。

**Architecture:** 保持现有 `snapshot -> classify -> solve -> occupancy -> async apply` 架构，新增逻辑只放在 `Systems/GraphLayout` 的 topology / policy / row allocation 边界。RuleSet JSON 是 runtime source of truth；UI 只编辑 RuleSet，不拥有 runtime 布局判断。TaskSpec / GraphWrite / Review 不表达节点坐标，也不新增 layout diff。

**Tech Stack:** Unreal Engine 5.6 C++、BlueprintHelper GraphLayout subsystem、UE Automation Tests、RuleSet JSON v1、BlueprintHelper CLI E2E smoke。

---

## Non-Negotiable Boundaries

- 不模拟编辑器选中节点后按 `Q`；水平对齐由 solver 确定。
- 不修改 TaskSpec / TaskPlan schema 来表达 layout。
- 不让 GraphWrite / GraphStatement / TaskRuntime 计算节点坐标。
- 不把业务布局逻辑放进 Slate UI、Settings Presenter 或单个工具函数。
- 不新增 Review v1 / legacy Transaction / layout Review diff。
- 不扩展旧 Agent / 旧字段兼容。
- 不引入 per-node special policy；第一阶段只允许五类 policy：`LinearExecChainPolicy`、`PureDataSubgraphPolicy`、`NodeInputClusterPolicy`、`MultiExecOutputPolicy`、`OccupancyPolicy`。
- 本仓库规则禁止 Codex 自动 `git add`、`git commit`、`git push`。计划中的 checkpoint 只记录建议命令，由用户手动执行。

## Current Source Facts

- `FRuleSet` 位于 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`，当前已有 spacing、collision、move/apply、role rules 和 `EditorCanvasRoleCenters`。
- RuleSet JSON import/export 位于 `BlueprintHelperGraphLayoutRuleSetJson.cpp` 与 `ToJson(const FRuleSet&)`。
- `FSolver::Solve()` 当前在 `BlueprintHelperGraphLayoutSolver.cpp` 中直接完成 root 查找、exec BFS、data input placement 和 placement 输出。
- `GetExecSuccessors()` 当前只返回 `TArray<FString>`，会丢失 Exec output pin identity。
- `AlignInputsToConsumerPinOrder()` 当前只处理 direct input source，不能把 `VariableGet -> Make Array -> ForEach.Array` 这类 pure data subgraph 当作整体 envelope。
- 现有 GraphLayout tests 集中在 `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`。

## File Structure

### Create

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h`
  - 定义 exec/data topology edge、root group、pin-level successor API。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.cpp`
  - 实现 pin-order-stable topology extraction，保留 Exec output pin identity。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h`
  - 定义 `DataLeaf` / `DataTransform` / `DataAggregate` / `DataSink` 与 pure data subgraph envelope。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.cpp`
  - 从 sink 反向构建 pure data DAG，计算 relative placement 与 envelope。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h`
  - 将 consumer input pins 与 pure data envelopes 组合成 row height budget。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.cpp`
  - 实现 data cluster measurement，复用 `FDataInputPlacement` 生成最终 target。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h`
  - 定义 exec row / branch row allocation 输入输出结构。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.cpp`
  - 根据 cluster height budget 和 branch topology 分配 row baseline。
- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
  - 记录实现期间 RED/GREEN、自动化、E2E smoke 与 blockers。

### Modify

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
  - 增加 pattern layout RuleSet 开关与最小 padding。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp`
  - 导出新 RuleSet 字段到 JSON。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp`
  - 导入、校验新 RuleSet 字段。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h`
  - 扩展 request，支持 sink pin anchor 与 pure data relative target。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.cpp`
  - 保持 leaf placement 逻辑，供 NodeInputClusterPolicy 复用。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
  - 改为编排 topology、pure data、row allocation、placement、occupancy。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
  - 新增 TDD tests，覆盖 RuleSet JSON、pure data subgraph、多 Exec output、row budget、disabled setting。
- `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`
  - 将可视化 setting 按 Pattern 分组展示：Linear Exec、Pure Data、Branch、Occupancy。
- `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
  - 增加 UI state 字段，仅用于编辑 RuleSet JSON。

### Do Not Modify

- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/*`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/*`
- `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review/*`
- `AgentFaceService/*`
- `ClaudePlugin/*`
- `CodexPlugin/*`

---

## Task 1: Add RuleSet Switches And JSON Roundtrip RED/GREEN

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Add failing RuleSet default and JSON test**

Append this test before `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsPatternLayoutSettings,
	"BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsPatternLayoutSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RuleSetJsonRoundTripsPatternLayoutSettings::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet Defaults;
	TestTrue(TEXT("exec horizontal alignment defaults on"), Defaults.bAlignExecNodesHorizontally);
	TestTrue(TEXT("pure data subgraph layout defaults on"), Defaults.bUsePureDataSubgraphLayout);
	TestTrue(TEXT("pattern row budget defaults on"), Defaults.bUsePatternRowHeightBudget);

	FRuleSet RuleSet;
	RuleSet.bAlignExecNodesHorizontally = false;
	RuleSet.bUsePureDataSubgraphLayout = false;
	RuleSet.bUsePatternRowHeightBudget = false;
	RuleSet.DataClusterPaddingX = 37.0f;
	RuleSet.DataClusterPaddingY = 43.0f;
	RuleSet.BranchRowPaddingY = 71.0f;

	const FString Json = FRuleSetJson::ExportString(RuleSet);
	FRuleSet Parsed;
	FValidationResult Validation;
	TestTrue(TEXT("json imports"), FRuleSetJson::ImportString(Json, Parsed, Validation));
	TestFalse(TEXT("exec horizontal alignment roundtrips"), Parsed.bAlignExecNodesHorizontally);
	TestFalse(TEXT("pure data subgraph roundtrips"), Parsed.bUsePureDataSubgraphLayout);
	TestFalse(TEXT("pattern row budget roundtrips"), Parsed.bUsePatternRowHeightBudget);
	TestEqual(TEXT("data cluster padding x"), Parsed.DataClusterPaddingX, 37.0f);
	TestEqual(TEXT("data cluster padding y"), Parsed.DataClusterPaddingY, 43.0f);
	TestEqual(TEXT("branch row padding y"), Parsed.BranchRowPaddingY, 71.0f);
	return true;
}
```

- [x] **Step 2: Run RED test compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: compile fails because `FRuleSet` does not yet define the new fields.

- [x] **Step 3: Add RuleSet fields**

In `FRuleSet` after `InputPinRowSpacing`:

```cpp
bool bAlignExecNodesHorizontally = true;
bool bUsePureDataSubgraphLayout = true;
bool bUsePatternRowHeightBudget = true;
float DataClusterPaddingX = 40.0f;
float DataClusterPaddingY = 40.0f;
float BranchRowPaddingY = 80.0f;
```

- [x] **Step 4: Export fields**

In `ToJson(const FRuleSet& RuleSet)`, after `input_pin_row_spacing`:

```cpp
Json->SetBoolField(TEXT("exec_node_horizontal_alignment_enabled"), RuleSet.bAlignExecNodesHorizontally);
Json->SetBoolField(TEXT("pure_data_subgraph_layout_enabled"), RuleSet.bUsePureDataSubgraphLayout);
Json->SetBoolField(TEXT("pattern_row_height_budget_enabled"), RuleSet.bUsePatternRowHeightBudget);
Json->SetNumberField(TEXT("data_cluster_padding_x"), RuleSet.DataClusterPaddingX);
Json->SetNumberField(TEXT("data_cluster_padding_y"), RuleSet.DataClusterPaddingY);
Json->SetNumberField(TEXT("branch_row_padding_y"), RuleSet.BranchRowPaddingY);
```

- [x] **Step 5: Validate and import fields**

In `FRuleSetJson::Validate`, after `input_pin_row_spacing` validation:

```cpp
TryReadPositiveNumber(Json, TEXT("data_cluster_padding_x"), Defaults.DataClusterPaddingX, Validation);
TryReadPositiveNumber(Json, TEXT("data_cluster_padding_y"), Defaults.DataClusterPaddingY, Validation);
TryReadPositiveNumber(Json, TEXT("branch_row_padding_y"), Defaults.BranchRowPaddingY, Validation);
```

In `FRuleSetJson::Import`, after `input_pin_row_spacing` import:

```cpp
Json->TryGetBoolField(TEXT("exec_node_horizontal_alignment_enabled"), OutRuleSet.bAlignExecNodesHorizontally);
Json->TryGetBoolField(TEXT("pure_data_subgraph_layout_enabled"), OutRuleSet.bUsePureDataSubgraphLayout);
Json->TryGetBoolField(TEXT("pattern_row_height_budget_enabled"), OutRuleSet.bUsePatternRowHeightBudget);
TryReadPositiveNumber(Json, TEXT("data_cluster_padding_x"), OutRuleSet.DataClusterPaddingX, OutValidation);
TryReadPositiveNumber(Json, TEXT("data_cluster_padding_y"), OutRuleSet.DataClusterPaddingY, OutValidation);
TryReadPositiveNumber(Json, TEXT("branch_row_padding_y"), OutRuleSet.BranchRowPaddingY, OutValidation);
```

Also support grouped solver fields inside the existing `solver` object:

```cpp
(*SolverObject)->TryGetBoolField(TEXT("exec_node_horizontal_alignment_enabled"), OutRuleSet.bAlignExecNodesHorizontally);
(*SolverObject)->TryGetBoolField(TEXT("pure_data_subgraph_layout_enabled"), OutRuleSet.bUsePureDataSubgraphLayout);
(*SolverObject)->TryGetBoolField(TEXT("pattern_row_height_budget_enabled"), OutRuleSet.bUsePatternRowHeightBudget);
TryReadPositiveNumber(*SolverObject, TEXT("data_cluster_padding_x"), OutRuleSet.DataClusterPaddingX, OutValidation);
TryReadPositiveNumber(*SolverObject, TEXT("data_cluster_padding_y"), OutRuleSet.DataClusterPaddingY, OutValidation);
TryReadPositiveNumber(*SolverObject, TEXT("branch_row_padding_y"), OutRuleSet.BranchRowPaddingY, OutValidation);
```

- [x] **Step 6: Run GREEN**

Run build and focused automation:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsPatternLayoutSettings;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PatternRuleSet_GREEN_20260601_001"
```

Expected: build succeeds and the focused RuleSet test passes.

- [x] **Step 7: Checkpoint**

Do not commit. Append RED/GREEN result to `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`.

---

## Task 2: Add GraphLayout Topology Helper

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Write failing topology test for multi Exec output identity**

Append this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_TopologyPreservesExecOutputPins,
	"BlueprintHelper.GraphLayout.Topology.PreservesExecOutputPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_TopologyPreservesExecOutputPins::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Split"),
		TEXT("K2Node_CallFunction"),
		TEXT("Generic Multi Exec"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 90.0f),
		false,
		{
			MakePin(TEXT("In"), EPinDirection::Input, true),
			MakePin(TEXT("ThenA"), EPinDirection::Output, true, {TEXT("A")}),
			MakePin(TEXT("ThenB"), EPinDirection::Output, true, {TEXT("B")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("A"), TEXT("K2Node_CallFunction"), TEXT("A"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("B"), TEXT("K2Node_CallFunction"), TEXT("B"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const TArray<FExecEdge> Edges = Topology.GetExecOutputEdges(TEXT("Split"));

	TestEqual(TEXT("two exec output edges"), Edges.Num(), 2);
	TestEqual(TEXT("first edge pin"), Edges[0].SourceOutputPinName, FString(TEXT("ThenA")));
	TestEqual(TEXT("first target"), Edges[0].TargetNodeId, FString(TEXT("A")));
	TestEqual(TEXT("second edge pin"), Edges[1].SourceOutputPinName, FString(TEXT("ThenB")));
	TestEqual(TEXT("second target"), Edges[1].TargetNodeId, FString(TEXT("B")));
	TestTrue(TEXT("split is branch by output count"), Topology.IsMultiExecOutputNode(TEXT("Split")));
	return true;
}
```

Add include:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
```

Expected before implementation: compile fails because the topology types do not exist.

- [x] **Step 2: Create topology header**

Create `BlueprintHelperGraphLayoutTopology.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FExecEdge
{
	FString SourceNodeId;
	FString SourceOutputPinId;
	FString SourceOutputPinName;
	int32 SourceOutputOrdinal = 0;
	FString TargetNodeId;
	int32 TargetOrdinalWithinOutput = 0;
};

struct FDataEdge
{
	FString SourceNodeId;
	FString TargetNodeId;
	FString TargetInputPinId;
	FString TargetInputPinName;
	int32 TargetInputOrdinal = 0;
};

class BLUEPRINTHELPER_API FGraphTopology
{
public:
	void AddNode(const FNodeSnapshot& Node);
	void AddExecEdge(const FExecEdge& Edge);
	void AddDataEdge(const FDataEdge& Edge);

	const FNodeSnapshot* FindNode(const FString& NodeId) const;
	TArray<FExecEdge> GetExecOutputEdges(const FString& NodeId) const;
	TArray<FDataEdge> GetDataInputs(const FString& NodeId) const;
	bool IsMultiExecOutputNode(const FString& NodeId) const;
	int32 CountExecInputs(const FString& NodeId) const;

private:
	TMap<FString, const FNodeSnapshot*> NodesById;
	TMultiMap<FString, FExecEdge> ExecEdgesBySource;
	TMultiMap<FString, FDataEdge> DataEdgesByTarget;
	TMap<FString, int32> ExecInputCounts;
	TMap<FString, int32> ExecOutputPinCounts;
};

class BLUEPRINTHELPER_API FGraphLayoutTopology
{
public:
	static FGraphTopology Build(const FGraphSnapshot& Snapshot);
};
}
```

- [x] **Step 3: Create topology implementation**

Create `BlueprintHelperGraphLayoutTopology.cpp` with stable pin-order traversal:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"

namespace BlueprintHelper::GraphLayout
{
void FGraphTopology::AddNode(const FNodeSnapshot& Node)
{
	NodesById.Add(Node.NodeId, &Node);
	ExecInputCounts.FindOrAdd(Node.NodeId) = 0;
	ExecOutputPinCounts.FindOrAdd(Node.NodeId) = 0;
}

void FGraphTopology::AddExecEdge(const FExecEdge& Edge)
{
	ExecEdgesBySource.Add(Edge.SourceNodeId, Edge);
	++ExecInputCounts.FindOrAdd(Edge.TargetNodeId);
}

void FGraphTopology::AddDataEdge(const FDataEdge& Edge)
{
	DataEdgesByTarget.Add(Edge.TargetNodeId, Edge);
}

const FNodeSnapshot* FGraphTopology::FindNode(const FString& NodeId) const
{
	return NodesById.FindRef(NodeId);
}

TArray<FExecEdge> FGraphTopology::GetExecOutputEdges(const FString& NodeId) const
{
	TArray<FExecEdge> Edges;
	ExecEdgesBySource.MultiFind(NodeId, Edges);
	Algo::Sort(Edges, [](const FExecEdge& Left, const FExecEdge& Right)
	{
		if (Left.SourceOutputOrdinal != Right.SourceOutputOrdinal)
		{
			return Left.SourceOutputOrdinal < Right.SourceOutputOrdinal;
		}
		return Left.TargetOrdinalWithinOutput < Right.TargetOrdinalWithinOutput;
	});
	return Edges;
}

TArray<FDataEdge> FGraphTopology::GetDataInputs(const FString& NodeId) const
{
	TArray<FDataEdge> Edges;
	DataEdgesByTarget.MultiFind(NodeId, Edges);
	Algo::Sort(Edges, [](const FDataEdge& Left, const FDataEdge& Right)
	{
		return Left.TargetInputOrdinal < Right.TargetInputOrdinal;
	});
	return Edges;
}

bool FGraphTopology::IsMultiExecOutputNode(const FString& NodeId) const
{
	return ExecOutputPinCounts.FindRef(NodeId) > 1;
}

int32 FGraphTopology::CountExecInputs(const FString& NodeId) const
{
	return ExecInputCounts.FindRef(NodeId);
}

FGraphTopology FGraphLayoutTopology::Build(const FGraphSnapshot& Snapshot)
{
	FGraphTopology Topology;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		Topology.AddNode(Node);
	}

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		int32 ExecOutputOrdinal = 0;
		for (int32 PinIndex = 0; PinIndex < Node.Pins.Num(); ++PinIndex)
		{
			const FPinSnapshot& Pin = Node.Pins[PinIndex];
			if (Pin.Direction == EPinDirection::Output && Pin.bExec)
			{
				if (Pin.LinkedNodeIds.Num() > 0)
				{
					++Topology.ExecOutputPinCounts.FindOrAdd(Node.NodeId);
				}
				for (int32 TargetIndex = 0; TargetIndex < Pin.LinkedNodeIds.Num(); ++TargetIndex)
				{
					FExecEdge Edge;
					Edge.SourceNodeId = Node.NodeId;
					Edge.SourceOutputPinId = Pin.PinId;
					Edge.SourceOutputPinName = Pin.Name;
					Edge.SourceOutputOrdinal = ExecOutputOrdinal;
					Edge.TargetNodeId = Pin.LinkedNodeIds[TargetIndex];
					Edge.TargetOrdinalWithinOutput = TargetIndex;
					Topology.AddExecEdge(Edge);
				}
				++ExecOutputOrdinal;
			}
			else if (Pin.Direction == EPinDirection::Input && !Pin.bExec)
			{
				for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
				{
					FDataEdge Edge;
					Edge.SourceNodeId = LinkedNodeId;
					Edge.TargetNodeId = Node.NodeId;
					Edge.TargetInputPinId = Pin.PinId;
					Edge.TargetInputPinName = Pin.Name;
					Edge.TargetInputOrdinal = PinIndex;
					Topology.AddDataEdge(Edge);
				}
			}
		}
	}
	return Topology;
}
}
```

Add `#include "Algo/Sort.h"` if the compiler reports `Algo::Sort` unresolved.

- [x] **Step 4: Run GREEN**

Run build and focused automation:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Topology.PreservesExecOutputPins;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_Topology_GREEN_20260601_001"
```

Expected: topology test passes.

- [x] **Step 5: Checkpoint**

Record that exec output pin identity is preserved and no solver behavior has changed yet.

---

## Task 3: Add PureDataSubgraphPolicy

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Write failing Make Array envelope test**

Append this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_PureDataSubgraphMeasuresMakeArrayEnvelope,
	"BlueprintHelper.GraphLayout.PureDataSubgraph.MeasuresMakeArrayEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_PureDataSubgraphMeasuresMakeArrayEnvelope::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("ForEach"),
		TEXT("K2Node_MacroInstance"),
		TEXT("For Each Loop"),
		FVector2D::ZeroVector,
		FVector2D(260.0f, 150.0f),
		false,
		{MakePin(TEXT("Array"), EPinDirection::Input, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("[1]"), EPinDirection::Input, false, {TEXT("Proxy1")}),
			MakePin(TEXT("[2]"), EPinDirection::Input, false, {TEXT("Proxy2")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("ForEach")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy1"), TEXT("K2Node_VariableGet"), TEXT("Proxy 1"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy2"), TEXT("K2Node_VariableGet"), TEXT("Proxy 2"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FPureDataSubgraphEnvelope Envelope =
		FPureDataSubgraphPolicy::MeasureForSink(Snapshot, Topology, TEXT("ForEach"), TEXT("Array"), FRuleSet());

	TestEqual(TEXT("root transform"), Envelope.RootNodeId, FString(TEXT("MakeArray")));
	TestTrue(TEXT("contains make array"), Envelope.NodeIds.Contains(TEXT("MakeArray")));
	TestTrue(TEXT("contains first leaf"), Envelope.NodeIds.Contains(TEXT("Proxy0")));
	TestTrue(TEXT("contains second leaf"), Envelope.NodeIds.Contains(TEXT("Proxy1")));
	TestTrue(TEXT("contains third leaf"), Envelope.NodeIds.Contains(TEXT("Proxy2")));
	TestTrue(TEXT("envelope is taller than transform node"), Envelope.Size.Y > 190.0f);
	TestTrue(TEXT("leaf target is left of transform"), Envelope.RelativeTargets.FindRef(TEXT("Proxy0")).X < Envelope.RelativeTargets.FindRef(TEXT("MakeArray")).X);
	TestTrue(TEXT("leaf order follows input pins"), Envelope.RelativeTargets.FindRef(TEXT("Proxy0")).Y < Envelope.RelativeTargets.FindRef(TEXT("Proxy1")).Y);
	TestTrue(TEXT("leaf order follows input pins 2"), Envelope.RelativeTargets.FindRef(TEXT("Proxy1")).Y < Envelope.RelativeTargets.FindRef(TEXT("Proxy2")).Y);
	return true;
}
```

Add includes:

```cpp
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h"
```

Expected before implementation: compile fails because the policy does not exist.

- [x] **Step 2: Create policy header**

Create:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class EPureDataNodeKind : uint8
{
	None,
	DataLeaf,
	DataTransform,
	DataSink
};

struct FPureDataSubgraphEnvelope
{
	FString SinkNodeId;
	FString SinkPinId;
	FString RootNodeId;
	TArray<FString> NodeIds;
	TMap<FString, FVector2D> RelativeTargets;
	FVector2D Size = FVector2D::ZeroVector;
};

class BLUEPRINTHELPER_API FPureDataSubgraphPolicy
{
public:
	static EPureDataNodeKind ClassifyNode(const FNodeSnapshot& Node);
	static FPureDataSubgraphEnvelope MeasureForSink(
		const FGraphSnapshot& Snapshot,
		const FGraphTopology& Topology,
		const FString& SinkNodeId,
		const FString& SinkPinId,
		const FRuleSet& RuleSet);
};
}
```

- [x] **Step 3: Implement pure data measurement**

Create implementation with these rules:

- A node with any exec pin is `None`.
- A node with at least one non-exec input and at least one non-exec output is `DataTransform`.
- A node with no non-exec input and at least one non-exec output is `DataLeaf`.
- The sink pin's first source becomes `RootNodeId`.
- Transform root sits at relative `(0, 0)`.
- Leaves sit to the left of their transform by `RuleSet.VariableInputOffsetX`.
- Leaves use input pin order and `RuleSet.InputPinRowSpacing`.
- `Envelope.Size` covers every relative target plus node size and `DataClusterPaddingX/Y`.

Core implementation:

```cpp
static bool HasPin(const FNodeSnapshot& Node, EPinDirection Direction, bool bExec)
{
	for (const FPinSnapshot& Pin : Node.Pins)
	{
		if (Pin.Direction == Direction && Pin.bExec == bExec)
		{
			return true;
		}
	}
	return false;
}
```

```cpp
EPureDataNodeKind FPureDataSubgraphPolicy::ClassifyNode(const FNodeSnapshot& Node)
{
	if (HasPin(Node, EPinDirection::Input, true) || HasPin(Node, EPinDirection::Output, true))
	{
		return EPureDataNodeKind::None;
	}
	const bool bHasDataInput = HasPin(Node, EPinDirection::Input, false);
	const bool bHasDataOutput = HasPin(Node, EPinDirection::Output, false);
	if (bHasDataInput && bHasDataOutput)
	{
		return EPureDataNodeKind::DataTransform;
	}
	if (!bHasDataInput && bHasDataOutput)
	{
		return EPureDataNodeKind::DataLeaf;
	}
	return EPureDataNodeKind::None;
}
```

The recursive walk should use `TSet<FString> ClaimedNodes`; when a node is encountered twice, keep the first owner and do not add a second relative target.

- [x] **Step 4: Run GREEN**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.PureDataSubgraph.MeasuresMakeArrayEnvelope;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_PureData_GREEN_20260601_001"
```

Expected: Make Array envelope test passes.

- [x] **Step 5: Checkpoint**

Record that pure data subgraph measurement works independently and solver is not integrated yet.

---

## Task 4: Add NodeInputClusterPolicy And Row Budget

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Write failing cluster budget test**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_NodeInputClusterBudgetIncludesPureDataEnvelope,
	"BlueprintHelper.GraphLayout.NodeInputCluster.BudgetIncludesPureDataEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_NodeInputClusterBudgetIncludesPureDataEnvelope::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(
		TEXT("Exec"),
		TEXT("K2Node_MacroInstance"),
		TEXT("For Each Loop"),
		FVector2D::ZeroVector,
		FVector2D(260.0f, 150.0f),
		false,
		{
			MakePin(TEXT("ExecIn"), EPinDirection::Input, true),
			MakePin(TEXT("LoopBody"), EPinDirection::Output, true),
			MakePin(TEXT("Completed"), EPinDirection::Output, true),
			MakePin(TEXT("Array"), EPinDirection::Input, false, {TEXT("MakeArray")})
		}));
	Snapshot.Nodes.Add(MakeNode(
		TEXT("MakeArray"),
		TEXT("K2Node_MakeArray"),
		TEXT("Make Array"),
		FVector2D::ZeroVector,
		FVector2D(220.0f, 190.0f),
		false,
		{
			MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
			MakePin(TEXT("[1]"), EPinDirection::Input, false, {TEXT("Proxy1")}),
			MakePin(TEXT("[2]"), EPinDirection::Input, false, {TEXT("Proxy2")}),
			MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("Exec")})
		}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy1"), TEXT("K2Node_VariableGet"), TEXT("Proxy 1"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy2"), TEXT("K2Node_VariableGet"), TEXT("Proxy 2"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	FRuleSet RuleSet;
	RuleSet.InputPinRowSpacing = 44.0f;
	RuleSet.DataClusterPaddingY = 40.0f;

	const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
	const FNodeInputClusterBudget Budget =
		FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, TEXT("Exec"), RuleSet);

	TestEqual(TEXT("consumer id"), Budget.ConsumerNodeId, FString(TEXT("Exec")));
	TestTrue(TEXT("budget includes make array"), Budget.NodeIds.Contains(TEXT("MakeArray")));
	TestTrue(TEXT("budget includes proxy"), Budget.NodeIds.Contains(TEXT("Proxy2")));
	TestTrue(TEXT("budget height includes data envelope"), Budget.Height > 230.0f);
	return true;
}
```

- [x] **Step 2: Create policy header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"

namespace BlueprintHelper::GraphLayout
{
struct FNodeInputClusterBudget
{
	FString ConsumerNodeId;
	TArray<FString> NodeIds;
	TMap<FString, FVector2D> RelativeTargets;
	float Width = 0.0f;
	float Height = 0.0f;
};

class BLUEPRINTHELPER_API FNodeInputClusterPolicy
{
public:
	static FNodeInputClusterBudget MeasureForConsumer(
		const FGraphSnapshot& Snapshot,
		const FGraphTopology& Topology,
		const FString& ConsumerNodeId,
		const FRuleSet& RuleSet);
};
}
```

- [x] **Step 3: Implement measurement**

Implementation should:

- Iterate non-exec input pins in pin order.
- For each source:
  - if `FPureDataSubgraphPolicy::ClassifyNode(Source) == DataTransform`, use `MeasureForSink`.
  - if source is `VariableInput`, `PureFunction`, or `OperatorOrCompare`, place it as a single-node leaf using `FDataInputPlacement`.
- Merge all relative targets into one budget.
- Compute `Height = MaxY - MinY + DataClusterPaddingY`.

Use first-owner behavior for duplicate node ids:

```cpp
if (!Budget.RelativeTargets.Contains(NodeId))
{
	Budget.NodeIds.Add(NodeId);
	Budget.RelativeTargets.Add(NodeId, RelativeTarget);
}
```

- [x] **Step 4: Keep FDataInputPlacement narrow**

Do not move DAG traversal into `FDataInputPlacement`. It remains a small target-builder for leaf input roles:

```cpp
static bool IsDataInputRole(ENodeRole Role);
static FVector2D BuildDesiredTarget(const FRuleSet& RuleSet, const FDataInputPlacementRequest& Request);
static const TCHAR* GetReason(ENodeRole Role);
```

- [x] **Step 5: Run GREEN**

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.NodeInputCluster.BudgetIncludesPureDataEnvelope;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_NodeInputCluster_GREEN_20260601_001"
```

Expected: cluster budget test passes.

---

## Task 5: Add RowAllocationPolicy

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Write failing row budget test**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_RowAllocationUsesDataClusterHeight,
	"BlueprintHelper.GraphLayout.RowAllocation.UsesDataClusterHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_RowAllocationUsesDataClusterHeight::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelper::GraphLayout;

	FRuleSet RuleSet;
	RuleSet.ExecRowSpacing = 100.0f;
	RuleSet.BranchRowSpacing = 120.0f;
	RuleSet.BranchRowPaddingY = 30.0f;

	FExecRowBudget Parent;
	Parent.RowId = 0;
	Parent.MinHeight = 80.0f;

	FExecRowBudget ChildA;
	ChildA.RowId = 1;
	ChildA.MinHeight = 260.0f;

	FExecRowBudget ChildB;
	ChildB.RowId = 2;
	ChildB.MinHeight = 80.0f;

	const TArray<FExecRowAllocation> Allocations =
		FGraphLayoutRowAllocationPolicy::Allocate({Parent, ChildA, ChildB}, RuleSet);

	TestEqual(TEXT("three rows"), Allocations.Num(), 3);
	TestEqual(TEXT("parent y"), Allocations[0].BaselineY, 0.0f);
	TestTrue(TEXT("child a respects parent height"), Allocations[1].BaselineY >= 110.0f);
	TestTrue(TEXT("child b respects child a envelope"), Allocations[2].BaselineY >= Allocations[1].BaselineY + 290.0f);
	return true;
}
```

- [x] **Step 2: Create row allocation API**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FExecRowBudget
{
	int32 RowId = 0;
	float MinHeight = 0.0f;
};

struct FExecRowAllocation
{
	int32 RowId = 0;
	float BaselineY = 0.0f;
	float Height = 0.0f;
};

class BLUEPRINTHELPER_API FGraphLayoutRowAllocationPolicy
{
public:
	static TArray<FExecRowAllocation> Allocate(const TArray<FExecRowBudget>& Budgets, const FRuleSet& RuleSet);
};
}
```

- [x] **Step 3: Implement row allocation**

```cpp
TArray<FExecRowAllocation> FGraphLayoutRowAllocationPolicy::Allocate(
	const TArray<FExecRowBudget>& Budgets,
	const FRuleSet& RuleSet)
{
	TArray<FExecRowAllocation> Result;
	float NextY = 0.0f;
	for (const FExecRowBudget& Budget : Budgets)
	{
		FExecRowAllocation Allocation;
		Allocation.RowId = Budget.RowId;
		Allocation.BaselineY = NextY;
		Allocation.Height = FMath::Max(RuleSet.ExecRowSpacing, Budget.MinHeight);
		Result.Add(Allocation);
		NextY += Allocation.Height + RuleSet.BranchRowPaddingY;
	}
	return Result;
}
```

This simple allocator is sufficient for the first integration. Solver integration can map semantic branch rows into ordered `FExecRowBudget` entries before calling it.

- [x] **Step 4: Run GREEN**

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.RowAllocation.UsesDataClusterHeight;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_RowAllocation_GREEN_20260601_001"
```

Expected: row allocation test passes.

---

## Task 6: Integrate Pattern Policies Into Solver

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

- [x] **Step 1: Add failing solver test for generic multi-exec output rows**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_MultiExecOutputNodeUsesBranchRows,
	"BlueprintHelper.GraphLayout.Solver.MultiExecOutputNodeUsesBranchRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_MultiExecOutputNodeUsesBranchRows::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("Event"), TEXT("K2Node_CustomEvent"), TEXT("Event"), FVector2D::ZeroVector, FVector2D(180.0f, 80.0f), false, {MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Split"), TEXT("K2Node_CallFunction"), TEXT("Generic Multi Exec"), FVector2D::ZeroVector, FVector2D(220.0f, 90.0f), false, {
		MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Event")}),
		MakePin(TEXT("ThenA"), EPinDirection::Output, true, {TEXT("A")}),
		MakePin(TEXT("ThenB"), EPinDirection::Output, true, {TEXT("B")})
	}));
	Snapshot.Nodes.Add(MakeNode(TEXT("A"), TEXT("K2Node_CallFunction"), TEXT("A"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("B"), TEXT("K2Node_CallFunction"), TEXT("B"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));

	FRuleSet RuleSet;
	RuleSet.ExecColumnSpacing = 300.0f;
	RuleSet.ExecRowSpacing = 80.0f;
	RuleSet.BranchRowSpacing = 320.0f;
	RuleSet.CollisionPaddingX = 0.0f;
	RuleSet.CollisionPaddingY = 0.0f;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* A = FindPlacement(Plan, TEXT("A"));
	const FNodePlacement* B = FindPlacement(Plan, TEXT("B"));
	TestNotNull(TEXT("A placement exists"), A);
	TestNotNull(TEXT("B placement exists"), B);
	if (!A || !B)
	{
		return false;
	}
	TestTrue(TEXT("multi exec output creates branch row gap"), FMath::Abs(B->TargetPosition.Y - A->TargetPosition.Y) >= 300.0f);
	return true;
}
```

Expected before solver integration: fails because current branch row logic only uses `Role == BranchControl`.

- [x] **Step 2: Add failing solver test for Make Array placement**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_SolverPlacesMakeArrayBetweenLeavesAndForEach,
	"BlueprintHelper.GraphLayout.Solver.PlacesMakeArrayBetweenLeavesAndForEach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_SolverPlacesMakeArrayBetweenLeavesAndForEach::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("Event"), TEXT("K2Node_CustomEvent"), TEXT("Event"), FVector2D::ZeroVector, FVector2D(180.0f, 80.0f), false, {MakePin(TEXT("Then"), EPinDirection::Output, true, {TEXT("ForEach")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("ForEach"), TEXT("K2Node_MacroInstance"), TEXT("For Each Loop"), FVector2D::ZeroVector, FVector2D(260.0f, 150.0f), false, {
		MakePin(TEXT("Exec"), EPinDirection::Input, true, {TEXT("Event")}),
		MakePin(TEXT("LoopBody"), EPinDirection::Output, true),
		MakePin(TEXT("Completed"), EPinDirection::Output, true),
		MakePin(TEXT("Array"), EPinDirection::Input, false, {TEXT("MakeArray")})
	}));
	Snapshot.Nodes.Add(MakeNode(TEXT("MakeArray"), TEXT("K2Node_MakeArray"), TEXT("Make Array"), FVector2D::ZeroVector, FVector2D(220.0f, 190.0f), false, {
		MakePin(TEXT("[0]"), EPinDirection::Input, false, {TEXT("Proxy0")}),
		MakePin(TEXT("[1]"), EPinDirection::Input, false, {TEXT("Proxy1")}),
		MakePin(TEXT("[2]"), EPinDirection::Input, false, {TEXT("Proxy2")}),
		MakePin(TEXT("Array"), EPinDirection::Output, false, {TEXT("ForEach")})
	}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy0"), TEXT("K2Node_VariableGet"), TEXT("Proxy 0"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy1"), TEXT("K2Node_VariableGet"), TEXT("Proxy 1"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("Proxy2"), TEXT("K2Node_VariableGet"), TEXT("Proxy 2"), FVector2D::ZeroVector, FVector2D(160.0f, 40.0f), false, {MakePin(TEXT("Value"), EPinDirection::Output, false, {TEXT("MakeArray")})}));

	FRuleSet RuleSet = MakeRuleSetWithCanvasOffsets();
	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* ForEach = FindPlacement(Plan, TEXT("ForEach"));
	const FNodePlacement* MakeArray = FindPlacement(Plan, TEXT("MakeArray"));
	const FNodePlacement* Proxy0 = FindPlacement(Plan, TEXT("Proxy0"));
	const FNodePlacement* Proxy2 = FindPlacement(Plan, TEXT("Proxy2"));
	TestNotNull(TEXT("ForEach placement exists"), ForEach);
	TestNotNull(TEXT("MakeArray placement exists"), MakeArray);
	TestNotNull(TEXT("Proxy0 placement exists"), Proxy0);
	TestNotNull(TEXT("Proxy2 placement exists"), Proxy2);
	if (!ForEach || !MakeArray || !Proxy0 || !Proxy2)
	{
		return false;
	}
	TestTrue(TEXT("MakeArray is left of ForEach"), MakeArray->TargetPosition.X < ForEach->TargetPosition.X);
	TestTrue(TEXT("Proxy leaf is left of MakeArray"), Proxy0->TargetPosition.X < MakeArray->TargetPosition.X);
	TestTrue(TEXT("Proxy leaves stack by pin order"), Proxy0->TargetPosition.Y < Proxy2->TargetPosition.Y);
	return true;
}
```

- [x] **Step 3: Refactor solver into orchestration**

Inside `FSolver::Solve`:

1. Build topology:

```cpp
const FGraphTopology Topology = FGraphLayoutTopology::Build(Snapshot);
```

2. Build root list using `Topology.CountExecInputs(Node.NodeId)` instead of `CountExecInputs(Snapshot)`.

3. Replace `GetExecSuccessors(*Node->Snapshot)` with `Topology.GetExecOutputEdges(Node->Snapshot->NodeId)`.

4. Treat a node as branch split when:

```cpp
const bool bBranchLike = RuleSet.bAlignExecNodesHorizontally &&
	Topology.IsMultiExecOutputNode(Node->Snapshot->NodeId);
```

5. For each successor edge, preserve `SourceOutputOrdinal` when computing branch row.

6. Before final data placement, measure `FNodeInputClusterBudget` for every exec consumer with a target. Use the maximum cluster height per semantic row to determine row baseline.

- [x] **Step 4: Preserve disabled behavior**

When `RuleSet.bAlignExecNodesHorizontally == false`, keep the old `Role == BranchControl` behavior:

```cpp
const bool bBranchLike = RuleSet.bAlignExecNodesHorizontally
	? Topology.IsMultiExecOutputNode(Node->Snapshot->NodeId)
	: Node->Role == ENodeRole::BranchControl;
```

This is a RuleSet-controlled mode, not a legacy fallback path.

- [x] **Step 5: Place pure data cluster after consumer final target**

After exec nodes have final target positions, for each targeted consumer:

```cpp
const FNodeInputClusterBudget Budget =
	FNodeInputClusterPolicy::MeasureForConsumer(Snapshot, Topology, ConsumerSnapshot.NodeId, RuleSet);
for (const TPair<FString, FVector2D>& Pair : Budget.RelativeTargets)
{
	const FVector2D DesiredTarget = ConsumerNode->Target + Pair.Value;
	SetTarget(Nodes, Occupancy, Pair.Key, DesiredTarget, TEXT("pure_data_subgraph_alignment"));
}
```

Do not place a node twice if `Nodes[Pair.Key].bHasTarget` is already true.

- [x] **Step 6: Run focused GREEN tests**

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Solver.MultiExecOutputNodeUsesBranchRows;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_MultiExec_GREEN_20260601_001"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout.Solver.PlacesMakeArrayBetweenLeavesAndForEach;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_MakeArray_GREEN_20260601_001"
```

Expected: both focused solver tests pass.

- [x] **Step 7: Checkpoint**

Record that solver consumes the new policies and that GraphWrite/TaskRuntime files remain untouched.

---

## Task 7: Add Regression Tests For Existing Nodes And Disabled Setting

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`
- Modify: solver/policies only if the RED tests expose gaps.

- [x] **Step 1: Add disabled setting test**

Append:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLayout_DisabledExecAlignmentUsesRoleBranchOnly,
	"BlueprintHelper.GraphLayout.Solver.DisabledExecAlignmentUsesRoleBranchOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLayout_DisabledExecAlignmentUsesRoleBranchOnly::RunTest(const FString& Parameters)
{
	using namespace BlueprintHelperGraphLayoutSolverTests;
	using namespace BlueprintHelper::GraphLayout;

	FGraphSnapshot Snapshot;
	Snapshot.Nodes.Add(MakeNode(TEXT("Split"), TEXT("K2Node_CallFunction"), TEXT("Generic Multi Exec"), FVector2D::ZeroVector, FVector2D(220.0f, 90.0f), false, {
		MakePin(TEXT("In"), EPinDirection::Input, true),
		MakePin(TEXT("ThenA"), EPinDirection::Output, true, {TEXT("A")}),
		MakePin(TEXT("ThenB"), EPinDirection::Output, true, {TEXT("B")})
	}));
	Snapshot.Nodes.Add(MakeNode(TEXT("A"), TEXT("K2Node_CallFunction"), TEXT("A"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));
	Snapshot.Nodes.Add(MakeNode(TEXT("B"), TEXT("K2Node_CallFunction"), TEXT("B"), FVector2D::ZeroVector, FVector2D(200.0f, 80.0f), false, {MakePin(TEXT("In"), EPinDirection::Input, true, {TEXT("Split")})}));

	FRuleSet RuleSet;
	RuleSet.bAlignExecNodesHorizontally = false;
	RuleSet.ExecRowSpacing = 80.0f;
	RuleSet.BranchRowSpacing = 320.0f;

	const FLayoutPlan Plan = FSolver::Solve(Snapshot, RuleSet);
	const FNodePlacement* A = FindPlacement(Plan, TEXT("A"));
	const FNodePlacement* B = FindPlacement(Plan, TEXT("B"));
	TestNotNull(TEXT("A placement exists"), A);
	TestNotNull(TEXT("B placement exists"), B);
	if (!A || !B)
	{
		return false;
	}
	TestTrue(TEXT("disabled mode does not apply generic multi-output branch spacing"), FMath::Abs(B->TargetPosition.Y - A->TargetPosition.Y) < 200.0f);
	return true;
}
```

- [x] **Step 2: Add existing consumer / generated pure data test**

Extend the current `NonMovableExistingConsumersStayAtCurrentPosition` test or add a new one where:

- `ExistingForEach` is `bExisting=true`, position `(1000, 800)`.
- `MakeArray` and leaves are generated.
- `RuleSet.bMoveExistingNodes=false`.

Expected assertions:

```cpp
TestEqual(TEXT("existing consumer target x remains current"), ExistingPlacement->TargetPosition.X, 1000.0);
TestEqual(TEXT("existing consumer target y remains current"), ExistingPlacement->TargetPosition.Y, 800.0);
TestTrue(TEXT("make array anchors to existing consumer"), MakeArrayPlacement->TargetPosition.X < ExistingPlacement->TargetPosition.X);
TestTrue(TEXT("proxy leaf anchors to make array"), ProxyPlacement->TargetPosition.X < MakeArrayPlacement->TargetPosition.X);
```

- [x] **Step 3: Run full GraphLayout automation**

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_Full_GREEN_20260601_001"
```

Expected: all GraphLayout tests pass.

- [x] **Step 4: Checkpoint**

Record full GraphLayout automation result and report path.

---

## Task 8: Group Pattern Settings In Layout Rule Editor

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`

- [x] **Step 1: Add UI state fields**

In `SBlueprintHelperLayoutRuleEditor.h`, near existing layout setting fields:

```cpp
bool bSettingsAlignExecNodesHorizontally = true;
bool bSettingsUsePureDataSubgraphLayout = true;
bool bSettingsUsePatternRowHeightBudget = true;
float SettingsDataClusterPaddingX = 40.0f;
float SettingsDataClusterPaddingY = 40.0f;
float SettingsBranchRowPaddingY = 80.0f;
```

- [x] **Step 2: Load state from parsed RuleSet**

In `RefreshSettingsFromJson()` after existing spacing assignments:

```cpp
bSettingsAlignExecNodesHorizontally = ParsedRuleSet.bAlignExecNodesHorizontally;
bSettingsUsePureDataSubgraphLayout = ParsedRuleSet.bUsePureDataSubgraphLayout;
bSettingsUsePatternRowHeightBudget = ParsedRuleSet.bUsePatternRowHeightBudget;
SettingsDataClusterPaddingX = ParsedRuleSet.DataClusterPaddingX;
SettingsDataClusterPaddingY = ParsedRuleSet.DataClusterPaddingY;
SettingsBranchRowPaddingY = ParsedRuleSet.BranchRowPaddingY;
```

- [x] **Step 3: Add grouped settings labels**

The UI should show these sections in order:

```text
Linear Exec
- Exec horizontal alignment
- Exec column spacing

Pure Data
- Pure data subgraph layout
- Data cluster padding X
- Data cluster padding Y
- Input pin row spacing

Branch
- Branch row spacing
- Branch row padding Y

Occupancy
- Collision padding X
- Collision padding Y
- Collision step Y
- Max collision attempts
```

Keep the UI write path as RuleSet JSON update. Do not read `graph_layout.*` runtime settings inside the solver.

- [x] **Step 4: Compile**

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: Slate compiles. No runtime behavior changes are introduced from UI-only grouping.

---

## Task 9: Real E2E Smoke With BlueprintHelper CLI

**Files:**
- Create: `Debug/TaskSpecs/GraphLayoutPattern_20260601/00_create_actor.json`
- Create: `Debug/TaskSpecs/GraphLayoutPattern_20260601/01_pattern_graph.json`
- Read generated CLI result artifacts.

- [x] **Step 1: Create smoke asset TaskSpec**

Create `00_create_actor.json`:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_graphlayout_pattern_create_actor_20260601",
  "task_type": "create_asset",
  "feature_name": "GraphLayoutPatternSmokeActor",
  "target": {
    "asset_path": "/Game/BlueprintHelperTemp/GraphLayoutPattern/BP_GraphLayoutPattern_20260601",
    "target_type": "blueprint_class"
  },
  "behavior": {
    "asset_strategy": "ensure_asset",
    "asset": {
      "asset_type": "blueprint_class",
      "parent_class": "Actor",
      "collision_policy": "reuse_if_exists"
    }
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [x] **Step 2: Create pattern graph TaskSpec**

Create `01_pattern_graph.json`:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_graphlayout_pattern_graph_20260601",
  "task_type": "edit_blueprint_graph",
  "feature_name": "GraphLayoutPatternGraph",
  "target": {
    "asset_path": "/Game/BlueprintHelperTemp/GraphLayoutPattern/BP_GraphLayoutPattern_20260601",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "prefer_new_graph": true,
    "graph_name": "EG_GraphLayoutPattern_20260601",
    "allow_modify_user_nodes": false,
    "allow_create_assets": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "CE_GraphLayoutPattern_20260601",
        "body": {
          "schema": "BlueprintLogicSpec.v2",
          "statements": [
            {
              "kind": "control",
              "control": "branch",
              "condition": {
                "kind": "literal",
                "value_type": "bool",
                "value": true
              },
              "then": [
                {
                  "kind": "create",
                  "create_operation": "make_array",
                  "pin_type": {
                    "category": "string"
                  },
                  "args": {
                    "item": {
                      "kind": "literal",
                      "value_type": "string",
                      "value": "GraphLayout pattern item"
                    }
                  },
                  "result_symbol": "LayoutStringArray"
                },
                {
                  "kind": "call",
                  "target": "PrintString",
                  "args": {
                    "InString": {
                      "kind": "literal",
                      "value_type": "string",
                      "value": "Then branch"
                    }
                  }
                }
              ],
              "else": [
                {
                  "kind": "call",
                  "target": "PrintString",
                  "args": {
                    "InString": {
                      "kind": "literal",
                      "value_type": "string",
                      "value": "Else branch"
                    }
                  }
                }
              ]
            },
            {
              "kind": "call",
              "target": "PrintString",
              "args": {
                "InString": {
                  "kind": "call",
                  "target": "GetDisplayName",
                  "args": {
                    "Object": {
                      "kind": "literal",
                      "value_type": "object",
                      "value": "Self"
                    }
                  }
                }
              }
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [x] **Step 3: Execute E2E through CLI**

Ensure the editor/Bridge is running, then run:

```powershell
bh.cmd task execute --file "Debug\TaskSpecs\GraphLayoutPattern_20260601\00_create_actor.json" --format json
bh.cmd task execute --file "Debug\TaskSpecs\GraphLayoutPattern_20260601\01_pattern_graph.json" --format json
```

Expected:

- both commands return `status=executed` or `status=completed`;
- the second result has `graph_layout_flush` timing;
- graph write result contains generated graph `EG_GraphLayoutPattern_20260601`;
- generated Blueprint compiles.

Evidence:

- Exact 00/01 smoke:
  - create/no-op asset task: `task_5E844A8E4BA92D0B4DDDF5B5C514C5DD`;
  - pattern graph task: `task_A0C4C5BB4B50C04FAD2F1187BB0657A3`;
  - graph write stats: `requested_node_count=12`, `spawned_node_count=7`, `created_link_count=5`, `layout_record_node_count=6`;
  - compile/save succeeded.
- Long/fresh 5-semantics nested smoke:
  - final graph task: `task_F100F9A44009D37061B243AC5421CB6F`;
  - final result artifact: `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\task_F100F9A44009D37061B243AC5421CB6F\result.json`;
  - graph write stats: `requested_node_count=34`, `spawned_node_count=20`, `created_link_count=21`, `layout_record_node_count=19`;
  - compile/save succeeded.

- [x] **Step 4: Inspect actual graph positions**

Use existing read context or editor visual inspection. The evidence must record:

- custom event and exec nodes read left-to-right;
- branch outputs are on separate rows;
- pure data / create node is located between source leaves and the downstream sink;
- data cluster does not overlap the branch row or next exec row;
- existing node pinning remains respected.

If CLI readback cannot expose coordinates, attach a screenshot path and list manual visual observations in `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`.

Evidence:

- Current-position export summary: `Debug/TaskSpecs/GraphLayoutPattern_20260601/04_current_graph_positions_summary.json`, with `node_count=19`, `link_count=13`.
- Fresh macro fixed export summary: `Debug/TaskSpecs/GraphLayoutPattern_20260601/35_macro_exec_fixed_fresh_positions_summary.json`, with `node_count=28`, `link_count=27`.
- Fresh `logic_json` readback artifact: `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_1780318717273\result.json`, with `nodes=28`, `exec_links=20`, `data_links=7`.
- Fresh `logic_flow` readback artifact: `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_1780318716951\result.json`; it reaches `For Each Loop`. Remaining `macro_boundary_ambiguous` is presentation-only because raw export and `logic_json` both confirm the terminal `then -> Exec` link.

- [x] **Step 5: Clean temporary project config only if changed**

If `.blueprinthelper/GraphLayoutRules.json` was modified to test settings, restore only the changed fields. Do not reset unrelated user config.

Evidence:

- No `.blueprinthelper/GraphLayoutRules.json` project config mutation was required for this run.

---

## Task 10: Final Verification And Review

**Files:**
- Read all changed files from Tasks 1-9.
- Modify only files already listed in this plan if review finds issues.

- [x] **Step 1: Run whitespace check**

```powershell
git diff --check -- BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout Debug/GraphLayout_PatternBasedExecLayout_20260601.md
```

Expected: no whitespace errors.

Evidence:

- `git diff --check` returned exit code `0` for GraphLayout, TaskRuntime, BlueprintSignature, GraphWrite test, Debug, and Report paths. Git only reported LF/CRLF working-copy warnings, not whitespace errors.

- [x] **Step 2: Run full build**

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: build succeeds.

Evidence:

- `Build.bat TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload` returned exit code `0`.
- UBT output included `Result: Succeeded`.

- [x] **Step 3: Run full GraphLayout automation**

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphLayout;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphLayout_Final_20260601_001"
```

Expected: all GraphLayout tests pass.

Evidence:

- `D:\UEProjects\Template\Saved\Automation\GraphLayout_Final_20260601_006\index.json`
- Result: `succeeded=23`, `succeededWithWarnings=0`, `failed=0`.
- `Template.log` recorded `**** TEST COMPLETE. EXIT CODE: 0 ****`.

- [x] **Step 4: Run broad regression if time allows**

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\TaskRuntime_GraphLayoutRegression_20260601_001"
```

Expected: TaskRuntime tests pass. If they are too slow or environment-blocked, record exact blocker in Debug doc and keep GraphLayout automation + real E2E as required gates.

Evidence:

- RED: `D:\UEProjects\Template\Saved\Automation\TaskRuntime_CompositeCreateFeature_Red_20260601_001\index.json` reproduced `owned_replace_target_not_blueprinthelper_owned`.
- GREEN focused: `D:\UEProjects\Template\Saved\Automation\TaskRuntime_CompositeCreateFeature_Green_20260601_001\index.json`, `failed=0`.
- Signature missing-graph ownership RED: `D:\UEProjects\Template\Saved\Automation\Signature_MissingGraphOwnership_RED_20260601_001\index.json`, `failed=1`.
- Signature missing-graph ownership GREEN: `D:\UEProjects\Template\Saved\Automation\Signature_MissingGraphOwnership_GREEN_20260601_001\index.json`, `succeeded=1`, `failed=0`.
- Signature service suite: `D:\UEProjects\Template\Saved\Automation\Signature_Service_GREEN_20260601_001\index.json`, `succeeded=19`, `succeededWithWarnings=1`, `failed=0`.
- Broad TaskRuntime: `D:\UEProjects\Template\Saved\Automation\TaskRuntime_GraphLayoutRegression_20260601_007\index.json`, `succeeded=37`, `succeededWithWarnings=4`, `failed=0`.
- Fresh CLI E2E ownership handoff:
  - create task: `task_944717714D0683EF5E357EBECEB9FFC1`;
  - replace task: `task_42B80E48495B3380660C50919D8C49AD`;
  - readback `logic_json`: `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_1780319834382\result.json`;
  - readback `logic_flow`: `D:\UEProjects\Template\Saved\BlueprintHelper\Cli\cli_1780319838723\result.json`;
  - flow confirms `CE_SignatureOwnedHandoff_20260601_Fresh01 -> 打印字符串` with `warnings=[]`.

- [x] **Step 5: Final source review checklist**

Confirm:

- `FSolver::Solve` orchestrates policies; it does not contain pure data DAG recursion inline.
- `PureDataSubgraphPolicy` has no UI dependency.
- `NodeInputClusterPolicy` measures envelope before row allocation.
- `MultiExecOutput` behavior is topology-based, not hardcoded by class title.
- RuleSet JSON roundtrips all new fields.
- UI grouping edits only RuleSet JSON.
- TaskSpec compiler and Review production files are unchanged.
- GraphWrite production ownership checks remain unchanged; only GraphWrite regression tests were extended.
- TaskRuntime was changed only for the GraphLayout flush failure gate.
- BlueprintSignature was changed only to write ownership metadata for newly created custom event entries; reused user-authored events are not adopted.

- [x] **Step 6: Final changed-file list**

Final response must list only files actually changed by this task. Ignore unrelated dirty files already present in the workspace.

---

## Manual Commit Commands For User

After implementation and verification, stage only this task's files. `AGENT.md` is intentionally excluded because it is unrelated to this implementation batch.

```powershell
git add -- `
  BlueprintHelper/Develop/Design/BlueprintHelper_GraphLayout_ExecNodeHorizontalAlignment_Design_20260601_CN.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_GraphLayout_PatternBasedExecLayout_ImplementationPlan_20260601_CN.md `
  BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_LongE2E_LogicFlow_Readback_Report_20260601_CN.md `
  BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md `
  BlueprintHelper/Develop/Report/BlueprintHelper_GraphWrite_MacroExecLink_And_MakeArrayType_Fix_Report_20260601_CN.md `
  BlueprintHelper/Develop/Report/BlueprintHelper_TaskRuntime_SignatureOwnedHandoff_Fix_Report_20260601_CN.md `
  BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/BlueprintSignature/BlueprintHelperSignatureServiceTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp
```

Suggested commit message after implementation:

```text
新增内容：
1. 新增 GraphLayout pattern-based topology、pure data subgraph、input cluster 与 row allocation 策略。
2. 新增 RuleSet 配置与自动化测试覆盖 Exec 水平对齐、多 Exec output 分支和 pure data transform 布局。
3. 新增 GraphLayout pattern E2E、long E2E、logic_json/logic_flow 回读和 signature-owned handoff 验证报告。

修复内容：
1. 修复 GraphLayout TaskRuntime graph layout flush 未阻断后续 compile/save 的失败传播。
2. 修复 signature-created custom event entry 未写 ownership 导致 owned replace body 被拒绝的问题，覆盖现有 graph 和 missing graph 两条创建路径。
3. 修复 ForEach macro exec link 与 MakeArray JSON pin_type 在长 E2E 中的回读断链问题。

变更需求：
1. 将 Layout Rule Editor 设置按 Linear Exec、Pure Data、Branch、Occupancy 分组。
2. 将 row height budget 优先级调整为 Pure Data / Data Input Cluster -> Branch Row -> Linear Exec。
```

Commit command:

```powershell
git commit -m "新增内容：
1. 新增 GraphLayout pattern-based topology、pure data subgraph、input cluster 与 row allocation 策略。
2. 新增 RuleSet 配置与自动化测试覆盖 Exec 水平对齐、多 Exec output 分支和 pure data transform 布局。
3. 新增 GraphLayout pattern E2E、long E2E、logic_json/logic_flow 回读和 signature-owned handoff 验证报告。

修复内容：
1. 修复 GraphLayout TaskRuntime graph layout flush 未阻断后续 compile/save 的失败传播。
2. 修复 signature-created custom event entry 未写 ownership 导致 owned replace body 被拒绝的问题，覆盖现有 graph 和 missing graph 两条创建路径。
3. 修复 ForEach macro exec link 与 MakeArray JSON pin_type 在长 E2E 中的回读断链问题。

变更需求：
1. 将 Layout Rule Editor 设置按 Linear Exec、Pure Data、Branch、Occupancy 分组。
2. 将 row height budget 优先级调整为 Pure Data / Data Input Cluster -> Branch Row -> Linear Exec。"
```
