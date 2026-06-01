# GraphLayout Pattern-Based Exec Layout Plan Report

Date: 2026-06-01

Status: planning report plus Task 1 through Task 10 final implementation evidence, including full GraphLayout automation, long E2E logic_json/logic_flow readback, signature-owned handoff closure, and broad TaskRuntime regression evidence.

## Final Closure Update

### 改动原因

最终审计发现本报告顶部状态仍停留在 Task 1-8，且 Task 6 历史段落仍写着不要标记完成；同时 Debug 文档内旧的 TaskRuntime ownership failure 和 terminal macro link failure 没有明确标记为已被后续 GREEN 证据 supersede。

### 改动过程

1. 保留 Task 1-8 的历史分阶段证据，不重写当时的 RED/GREEN 过程。
2. 将本报告状态更新为 Task 1-10 final implementation evidence。
3. 明确 Task 6 历史段落中的 “不要标记完成” 已被后续 quality review、full GraphLayout automation 和最终 implementation plan 勾选状态 supersede。
4. 将最终验证证据同步到 implementation plan 和 Debug 文档。

### 改动结果

- Full GraphLayout：`D:\UEProjects\Template\Saved\Automation\GraphLayout_Final_20260601_006\index.json`，`succeeded=23`，`failed=0`。
- Long E2E readback：raw export、`logic_json`、`logic_flow` 已确认 terminal `then -> Exec` link；`macro_boundary_ambiguous` 只保留为展示层 warning。
- Signature ownership handoff：
  - RED：`D:\UEProjects\Template\Saved\Automation\Signature_MissingGraphOwnership_RED_20260601_001\index.json`，`failed=1`。
  - GREEN：`D:\UEProjects\Template\Saved\Automation\Signature_MissingGraphOwnership_GREEN_20260601_001\index.json`，`succeeded=1`，`failed=0`。
  - Signature suite：`D:\UEProjects\Template\Saved\Automation\Signature_Service_GREEN_20260601_001\index.json`，`succeeded=19`，`succeededWithWarnings=1`，`failed=0`。
  - Broad TaskRuntime：`D:\UEProjects\Template\Saved\Automation\TaskRuntime_GraphLayoutRegression_20260601_007\index.json`，`succeeded=37`，`succeededWithWarnings=4`，`failed=0`。

### 审计结论

- Task 6 已由后续 review 和 final verification 收束；本报告中早期 “不要标记完成” 是历史 checkpoint，不再代表最终状态。
- Task 9/10 已在 implementation plan 中标记完成，并附有真实 E2E/readback/automation 证据。
- `AGENT.md` 不属于本任务改动范围，也不在建议 stage 命令内。

## Task 7: Disabled Setting And Existing ForEach Regression

### 改动原因

Task 7 要求在 solver 层补齐两个回归保护：一是 `bAlignExecNodesHorizontally=false` 时 generic multi-exec 节点不能再按新规则强制走 branch row spacing；二是 existing consumer 不可移动时，generated pure data cluster 仍应锚定到 existing consumer 的真实当前位置。

### 改动过程

1. 在 `BlueprintHelperGraphLayoutSolverTests.cpp` 新增 `BlueprintHelper.GraphLayout.Solver.DisabledExecAlignmentUsesRoleBranchOnly`。
2. 该测试构造没有 `BranchControl` 分类的 generic `K2Node_CallFunction` 多 exec output 节点，关闭 `bAlignExecNodesHorizontally` 后断言两个 output successor 的 Y 差仍小于 branch row spacing。
3. 新增 `BlueprintHelper.GraphLayout.Solver.NonMovableExistingForEachAnchorsGeneratedPureCluster`。
4. 该测试构造 `ExistingForEach` 位于 `(1000, 800)` 且 `bExisting=true`、`RuleSet.bMoveExistingNodes=false`，再生成 `MakeArray` 和 `Proxy0/Proxy1` pure leaves。
5. 断言 existing consumer 不可移动且 target 仍为 `(1000, 800)`，`MakeArray` 位于 existing consumer 左侧，proxy leaf 位于 `MakeArray` 左侧，并且 `MakeArray` 的 reason 为 `pure_data_subgraph_alignment`。
6. 本轮未修改 solver 或 policy 生产代码。

### 改动结果

- Build：`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 返回 exit code 0，UBT 输出 `Result: Succeeded`。
- Focused disabled setting：`D:\UEProjects\Template\Saved\Automation\GraphLayout_Task7_Focused_GREEN_20260601_001\index.json` 记录 `succeeded: 1`、`failed: 0`、`notRun: 0`。
- Focused existing ForEach：`D:\UEProjects\Template\Saved\Automation\GraphLayout_Task7_ExistingForEach_GREEN_20260601_001\index.json` 记录 `succeeded: 1`、`failed: 0`、`notRun: 0`。
- Full GraphLayout：`D:\UEProjects\Template\Saved\Automation\GraphLayout_Full_GREEN_20260601_001\index.json` 记录 `succeeded: 21`、`failed: 0`、`notRun: 0`。
- `Template.log` 记录 `**** TEST COMPLETE. EXIT CODE: 0 ****`。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- Task 7 只扩展测试，不修改 solver/policy 生产代码。
- Full GraphLayout automation 已覆盖 Task 1-7 当前全部 GraphLayout 测试，结果通过。
- 没有执行 `git add`、`git commit`、`git push`，也没有修改 `.git` 内容。

## Task 8: Group Pattern Settings In Layout Rule Editor

### 改动原因

Task 8 要求只在 `SBlueprintHelperLayoutRuleEditor` UI 编辑器表面补齐 pattern-based layout 的 RuleSet 编辑能力，不允许把这些开关/数值改成读取 `graph_layout.*` runtime setting，也不允许触碰 solver、tests 或其他非 UI 边界。

### 改动过程

1. 在 `SBlueprintHelperLayoutRuleEditor.h` 新增 UI 本地 state：
   - `bSettingsAlignExecNodesHorizontally`
   - `bSettingsUsePureDataSubgraphLayout`
   - `bSettingsUsePatternRowHeightBudget`
   - `SettingsDataClusterPaddingX`
   - `SettingsDataClusterPaddingY`
   - `SettingsBranchRowPaddingY`
2. 在 `Construct()` 中用默认 `FRuleSet` 初始化上述 pattern setting 的默认值，避免 settings panel 首帧出现未初始化显示。
3. 在 `RefreshSettingsFromJson()` 中从 parsed `FRuleSet` 读取上述值，保证 UI state 的 source of truth 仍然是 RuleSet JSON。
4. 扩展 float / bool setting enum 和 handler 分支，让 UI 修改这些字段时继续走：
   - `ImportString(RuleSetJson, ParsedRuleSet, Validation)`
   - mutate `ParsedRuleSet`
   - `FRuleSetJson::ExportString(ParsedRuleSet)`
5. 重组 `BuildSettingsPanel()` 的 GraphLayout pattern section 顺序为：
   - `Linear Exec`
   - `Pure Data`
   - `Branch`
   - `Occupancy`
6. 保留 `Rule` / `Apply` / `Persistence` section，以及已有的 `Exec row spacing`、`Pure input offset X`、`Variable input offset X` 这些非本任务新增但仍然必要的编辑能力；只是移动到更合适的分组，没有删除。

### 改动结果

- UI 现在可以直接编辑并写回以下 RuleSet 字段：
  - `bAlignExecNodesHorizontally`
  - `bUsePureDataSubgraphLayout`
  - `bUsePatternRowHeightBudget`
  - `DataClusterPaddingX`
  - `DataClusterPaddingY`
  - `BranchRowPaddingY`
- Settings panel 已按计划把 pattern settings 分组显示，并保持现有 Rule / Apply / Persistence 能力不丢失。
- UI 写回路径仍然完全依赖 `FRuleSetJson::ExportString(ParsedRuleSet)`，没有引入任何 solver/runtime setting 读取分支。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 验证

- 只读检查：
  - `rg` 确认新增字段、section label、refresh path、handler 分支都已存在。
  - `git diff --check -- BlueprintHelper/Source/BlueprintHelper/Public/UI/Layout/SBlueprintHelperLayoutRuleEditor.h BlueprintHelper/Source/BlueprintHelper/Private/UI/Layout/SBlueprintHelperLayoutRuleEditor.cpp` 返回 exit code `0`。
- 本轮未运行 build：
  - 原因：避免和 Task 6 worker 的 build / automation 争抢。
- 本轮未运行真实 E2E：
  - 原因：用户本轮允许至少做快速只读检查；同时 Task 8 scope 限定为 UI worker，且为避免与正在运行的自动化争用资源。

### 审计结论

- Task 8 按用户澄清后的边界执行：GraphLayout RuleSet 控件只添加/重组在 `SBlueprintHelperLayoutRuleEditor` 这个 LayoutPanel / RuleEditor UI 中，没有新增或修改 global SettingsPanel、`BlueprintHelperSettingsPresenter`、`BlueprintHelperUiSettings` 的项目设置项。
- 没有修改 solver、tests、TaskRuntime、GraphWrite、Review、AgentFaceService、ClaudePlugin、CodexPlugin、AGENTS.md。
- 没有执行 `git add`、`git commit`、`git push`，也没有修改 `.git` 内容。

## Task 6: Integrate Pattern Policies Into Solver RED/GREEN

### 改动原因

Task 6 要求把前序已经独立完成的 topology、pure data subgraph、node input cluster 和 row allocation policy 接入 `FSolver`，让 solver 只负责 GraphLayout policy 编排，不把布局语义扩散到 UI、TaskRuntime、GraphWrite、Review 或 Agent 插件边界。

### 改动过程

1. 先新增 `BlueprintHelper.GraphLayout.Solver.MultiExecOutputNodeUsesBranchRows`，构造 `Event -> Split -> A/B`，其中 `Split` 是普通 `K2Node_CallFunction` 多 exec output 节点，不依赖 `BranchControl` 分类。
2. 先新增 `BlueprintHelper.GraphLayout.Solver.PlacesMakeArrayBetweenLeavesAndForEach`，构造 `Event -> ForEach` 且 `ForEach.Array <- MakeArray <- Proxy0/Proxy1/Proxy2`。
3. RED build 成功，说明新增测试可编译；随后两个 focused automation 均按预期失败。
4. MakeArray 场景的旧 solver 已经能靠多 pass direct-input placement 放出位置，因此补充 `MakeArray` reason 等于 `pure_data_subgraph_alignment` 的断言，用来证明 solver 是否真正消费 `FNodeInputClusterPolicy`。
5. `FSolver::Solve` 新增 `FGraphLayoutTopology::Build(Snapshot)`，root detection 改用 `Topology.CountExecInputs(NodeId)`。
6. `LayoutExecChain` 改为消费 `Topology.GetExecOutputEdges(NodeId)`，保留 `SourceOutputOrdinal`，并按 `bAlignExecNodesHorizontally ? Topology.IsMultiExecOutputNode(NodeId) : Role == BranchControl` 判定 branch-like。
7. 对 branch-like successor 使用 `SourceOutputOrdinal` 和 `BranchRowSpacing` 计算 row offset，使 generic multi-exec 节点默认获得 branch row spacing。
8. 在 exec target 完成后接入 `FNodeInputClusterPolicy`，按 `ConsumerTarget + RelativeTarget` 放置 cluster 内节点，reason 为 `pure_data_subgraph_alignment`，且跳过已经有 target 的节点。
9. 在 `bUsePatternRowHeightBudget` 开启时，按 semantic exec row 统计 `FNodeInputClusterPolicy` 的最大数据 cluster 高度，再通过 `FGraphLayoutRowAllocationPolicy` reflow row baseline。
10. 为避免大改 solver 行为，row budget 集成保留现有 exec traversal 产生 semantic row id，再做 row baseline reflow，并在 reflow 后重建 occupancy；这是本轮最小且架构一致的集成方式。

### 改动结果

- RED build：
  - 命令：`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload`
  - 结果：exit code 0，新增测试编译通过。
- RED automation：
  - `GraphLayout_MultiExec_RED_20260601_001\index.json`：`succeeded: 0`，`failed: 1`。
  - `GraphLayout_MakeArray_RED_20260601_001\index.json`：`succeeded: 0`，`failed: 1`。
- GREEN build：
  - 同一 build 命令返回 exit code 0，UBT 输出 `Result: Succeeded`。
- GREEN focused automation：
  - `GraphLayout_MultiExec_GREEN_20260601_001\index.json`：`succeeded: 1`，`failed: 0`，`notRun: 0`。
  - `GraphLayout_MakeArray_GREEN_20260601_001\index.json`：`succeeded: 1`，`failed: 0`，`notRun: 0`。
- GREEN solver regression：
  - `GraphLayout_Solver_Task6_GREEN_20260601_001\index.json`：`succeeded: 9`，`failed: 0`，`notRun: 0`。
  - `Template.log` 记录 `**** TEST COMPLETE. EXIT CODE: 0 ****`。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- `FSolver` 只编排 GraphLayout policy；没有改动 UI、TaskRuntime、GraphWrite、Review、AgentFaceService、ClaudePlugin 或 CodexPlugin。
- `FDataInputPlacement` 没有新增 DAG traversal。
- 没有执行 `git add`、`git commit`、`git push`，也没有删除或修改 `.git` 文件。
- Historical checkpoint: 当时不在 implementation plan 中标记 Task 6 完成；该状态留给 controller/review。Final Closure Update 已 supersede 该 checkpoint。

## Task 6 Quality Review Fix: Row Reflow Baseline Bump Propagation

### 改动原因

Task 6 质量复审指出 `ReflowExecTargetsToAllocatedRows` 仍按 exec 节点独立解析 fresh PatternOccupancy。若 pinned existing reservation 阻挡了父 exec 的 allocated baseline，父节点会被 occupancy 向下推，但同一 semantic row 中未被阻挡的子节点可能保留旧 baseline，破坏单输出 exec chain 的水平行关系。

### 改动过程

1. 先新增 RED 回归 `BlueprintHelper.GraphLayout.Solver.RowReflowPropagatesPinnedBaselineBlocker`。
2. 测试构造两个 root，让 `Event -> Exec` 进入 semantic row 3；row allocation 后该行 baseline 为 `450`，同时放置 non-movable existing `PinnedBlocker` 在 `(0, 450)`，只阻挡 parent column。
3. RED build 通过后运行 focused automation，确认旧 solver 下 `Event` 被推到 `550`，但 `Exec` 仍停在 `450`，触发 “child successor is not above the bumped parent row” 和 row-aligned 断言失败。
4. 最小修改 `ReflowExecTargetsToAllocatedRows`：按原 semantic row 分组 movable exec nodes，按 row id 排序处理；每一行先用 occupancy 测量最大 resolved baseline，不写入中间 per-node target，并在 bumped baseline 上重新检查直到该 row 稳定；随后把该行所有 movable exec node 放到同一 resolved baseline。
5. 将当前 row 的 baseline bump 累积到后续 semantic rows。Pinned existing node 只作为 occupancy reservation 存在，并继续跳过 movable row placement。

### 改动结果

- RED build：`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 返回 exit code 0，UBT 输出 `Result: Succeeded`。
- RED focused automation：`GraphLayout_RowReflow_RED_20260601_001\index.json` 记录 `succeeded: 0`、`failed: 1`、`notRun: 0`，失败消息证明 child 留在 `450` 而 parent 已到 `550`。
- GREEN build：同一 build 命令返回 exit code 0，UBT 输出 `Result: Succeeded`。
- GREEN focused automation：`GraphLayout_RowReflow_GREEN_20260601_002\index.json` 记录 `succeeded: 1`、`failed: 0`、`notRun: 0`。
- GREEN solver suite：`GraphLayout_Solver_Task6_Reflow_GREEN_20260601_002\index.json` 记录 `succeeded: 10`、`failed: 0`、`notRun: 0`；`Template.log` 记录 `**** TEST COMPLETE. EXIT CODE: 0 ****`。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- 本次只修复 Task 6 solver reflow 行传播问题，没有重写 solver 架构。
- 没有改动 UI、TaskRuntime、GraphWrite、Review、AgentFaceService、ClaudePlugin、CodexPlugin 或 AGENT.md。
- 没有执行 `git add`、`git commit`、`git push`，也没有删除或修改 `.git` 文件。
- Historical checkpoint: 当时不在 implementation plan 中标记 Task 6 完成；该状态继续留给 controller/review。Final Closure Update 已 supersede 该 checkpoint。

## Task 5 Review Approval

### 规格复审结果

- 规格复审结论：APPROVED。
- 复审确认 `BlueprintHelperGraphLayoutRowAllocationPolicy.h` 定义了 Task 5 要求的 row budget/allocation structs 与 `Allocate(...)` API。
- 复审确认 `BlueprintHelperGraphLayoutRowAllocationPolicy.cpp` 保持输入顺序，首行 `BaselineY = 0`，使用 `Max(ExecRowSpacing, MinHeight)`，并按 `Height + BranchRowPaddingY` 推进下一行。
- 复审确认 `BlueprintHelper.GraphLayout.RowAllocation.UsesDataClusterHeight` 覆盖了指定三行 budget 场景。

### 质量复审结果

- 质量复审结论：APPROVED。
- 复审确认 standalone policy API 符合现有 GraphLayout policy 边界。
- 复审确认 exported class 和 data-only structs 形态合适。
- 复审确认空输入会自然返回空 allocation list。
- 复审确认 focused test 对 Task 5 这个小型独立 policy 足够。

### 状态更新

- 根据规格复审与质量复审结果，implementation plan 中 Task 5 的步骤已标记完成。

## Task 5: Add RowAllocationPolicy RED/GREEN

### 改动原因

Task 5 要求把 exec row baseline/height 分配逻辑先独立沉到 `Systems/GraphLayout` policy 边界中，用 TDD 锁定 “高数据 cluster 会抬高后续 row baseline” 的语义，同时明确本轮不接入 `FSolver`。

### 改动过程

1. 先在 `BlueprintHelperGraphLayoutSolverTests.cpp` 增加 `BlueprintHelperGraphLayoutRowAllocationPolicy.h` include。
2. 追加 RED 用例 `BlueprintHelper.GraphLayout.RowAllocation.UsesDataClusterHeight`。
3. 只保留测试改动先跑 build，确认 RED 失败点是缺少 `RowAllocationPolicy` surface，而不是断言或测试拼写问题。
4. 新增 `BlueprintHelperGraphLayoutRowAllocationPolicy.h`，定义 `FExecRowBudget`、`FExecRowAllocation` 和 `FGraphLayoutRowAllocationPolicy`。
5. 新增 `BlueprintHelperGraphLayoutRowAllocationPolicy.cpp`，最小实现 `Allocate(...)`：
   - 保持输入 budget 顺序；
   - 第一行 `BaselineY = 0`；
   - `Height = Max(RuleSet.ExecRowSpacing, Budget.MinHeight)`；
   - 下一行 baseline 递增 `Allocation.Height + RuleSet.BranchRowPaddingY`。
6. 重新运行 build 验证 GREEN。
7. 运行 `UnrealEditor-Cmd.exe` focused automation，确认真实 `Template.uproject` runtime 下该测试为 GREEN。

### 改动结果

- RED build：
  - 命令：`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload`
  - 结果：exit code 1。
  - 关键证据：`fatal error C1083`，缺少 `Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h`。
- GREEN build：
  - 同一 build 命令返回 exit code 0。
  - UBT 输出 `Result: Succeeded`。
- GREEN automation：
  - 用例：`BlueprintHelper.GraphLayout.RowAllocation.UsesDataClusterHeight`
  - 报告：`D:\UEProjects\Template\Saved\Automation\GraphLayout_RowAllocation_GREEN_20260601_001\index.json`
  - 结果：`succeeded: 1`、`failed: 0`、`notRun: 0`
  - `Template.log` 记录 `Test Completed. Result={成功}` 与 `**** TEST COMPLETE. EXIT CODE: 0 ****`
- 本轮实现没有接入 `FSolver`，只新增了可复用 row allocation policy 边界，符合 Task 5 的边界要求。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRowAllocationPolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- 本轮没有改动 `FSolver`、UI、TaskRuntime、GraphWrite、Review、AgentFaceService、ClaudePlugin 或 CodexPlugin。
- 没有执行 `git add`、`git commit`、`git push`，也没有改动 `.git`。
- 不要在 implementation plan 中把 Task 5 标记完成；该状态继续留给 controller/review 流程处理。

## Task 4: Add NodeInputClusterPolicy And Row Budget RED/GREEN

### 改动原因

Task 4 要求在不触碰 UI、TaskRuntime、GraphWrite、Review 边界的前提下，把 exec consumer 周围的数据输入 cluster 测量独立到 `Systems/GraphLayout` 新 policy 中，并且先用 TDD 锁定 `For Each Loop.Array <- MakeArray <- Proxy0/Proxy1/Proxy2` 这类 pure-data envelope 会抬高 row budget 的语义。

### 改动过程

1. 先在 `BlueprintHelperGraphLayoutSolverTests.cpp` 添加 `BlueprintHelperGraphLayoutNodeInputClusterPolicy.h` include。
2. 追加 RED 用例 `BlueprintHelper.GraphLayout.NodeInputCluster.BudgetIncludesPureDataEnvelope`，构造 `Exec(Array <- MakeArray <- Proxy0/Proxy1/Proxy2)` 数据图。
3. 运行指定 build 验证 RED，失败原因是 `BlueprintHelperGraphLayoutNodeInputClusterPolicy.h` 尚不存在。
4. 新增 `BlueprintHelperGraphLayoutNodeInputClusterPolicy.h`，定义 `FNodeInputClusterBudget` 与 `FNodeInputClusterPolicy`。
5. 新增 `BlueprintHelperGraphLayoutNodeInputClusterPolicy.cpp`，让 `MeasureForConsumer` 按 consumer 非 exec input pin 顺序遍历数据源。
6. 对 pure data transform source，复用 `FPureDataSubgraphPolicy::MeasureForSink`；对 `VariableInput` / `PureFunction` / `OperatorOrCompare` leaf source，继续复用窄边界 `FDataInputPlacement`。
7. 首次 GREEN build 成功后，focused automation 暴露真实缺口：budget 只按 raw member node bounds 回算，高度没有把 pure-data envelope 已经保留的 inner padding 算进去。
8. 接受最小修复：在 `FNodeInputClusterPolicy` 内补充 transform envelope bounds，合并 raw relative targets 与 envelope extent，再统一应用 cluster 外层 padding。
9. 重新运行 build 与 focused UE automation，最终 GREEN。

### 改动结果

- RED：`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 返回 exit code 1，关键错误为缺少 `Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h`。
- 首次 runtime GREEN 尝试失败：`D:\UEProjects\Template\Saved\Automation\GraphLayout_NodeInputCluster_GREEN_20260601_001\index.json` 曾记录 `failed: 1`，失败点是 `budget height includes data envelope`。
- 最终 GREEN build：同一 build 命令返回 exit code 0，输出 `Result: Succeeded`。
- 最终 GREEN automation：`BlueprintHelper.GraphLayout.NodeInputCluster.BudgetIncludesPureDataEnvelope` 返回 exit code 0。
- Automation report：`D:\UEProjects\Template\Saved\Automation\GraphLayout_NodeInputCluster_GREEN_20260601_001\index.json` 最终记录 `succeeded: 1`、`failed: 0`、`notRun: 0`。
- Runtime log：`D:\UEProjects\Template\Saved\Logs\Template.log` 记录 `Test Completed. Result={成功}` 与 `**** TEST COMPLETE. EXIT CODE: 0 ****`。
- `FDataInputPlacement` 保持窄职责，没有被扩展成 DAG traversal 或 cluster orchestrator。
- Task 4 仍未接入 `FSolver`，因此本轮真实 runtime/E2E 入口就是 `UnrealEditor-Cmd.exe` 加载真实 `Template.uproject` 执行 focused automation；没有额外 solver/CLI 布局入口可验证。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- 本轮没有改动 `FSolver`、UI、TaskRuntime、GraphWrite、Review、AgentFaceService、ClaudePlugin 或 CodexPlugin。
- 没有执行 `git add`、`git commit`、`git push`，也没有删除或修改 `.git` 文件。
- 不要在 implementation plan 中把 Task 4 标记为完成；该状态留给 controller 在 review 之后更新。

## Task 4 Quality Follow-up: Source-Specific Transform Measurement And Duplicate Bounds

### 改动原因

Task 4 质量 review 指出了两个真实缺口：

1. `NodeInputClusterPolicy` 之前按 sink pin 调用 `MeasureForSink`，当同一个 input pin 链接多个 pure-data transform 时，会重复拿到第一个 pure source，后续 linked transform 既不会按自身测量，也无法保持 linked-node order。
2. duplicate transform 通过多个 input pin 或重复 link 进入 cluster 时，first-owner target 虽然只保留第一次，但 supplemental envelope bounds 仍会按后续偏移继续累加，导致 `Width/Height` 被虚增。

这两个问题都属于 GraphLayout policy 边界本身，不能通过扩展 `FDataInputPlacement` 或伪造 topology 来修。

### 改动过程

1. 先补充两个 RED 用例：
   - `BlueprintHelper.GraphLayout.NodeInputCluster.MeasuresEachTransformLinkOnSamePin`
   - `BlueprintHelper.GraphLayout.NodeInputCluster.DuplicateTransformDoesNotInflateBounds`
2. 只加测试后先 build，确认这是行为级 RED，不是编译面缺口。
3. 运行 focused automation `BlueprintHelper.GraphLayout.NodeInputCluster`，观察到：
   - 第二个 transform / leaf 缺失；
   - same-pin linked order 断言失败；
   - duplicate transform 的 budget height 被虚增。
4. 在 `BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h/.cpp` 新增 `MeasureForRoot(...)`，让 GraphLayout policy 可以按已知 transform root 做 source-specific pure-data measurement。
5. 保持 `MeasureForSink(...)` 外部语义不变，只是内部委托到 `MeasureForRoot(...)`。
6. `NodeInputClusterPolicy` 改为对每个 transform `LinkedNodeId` 调用 `MeasureForRoot(...)`，不再用 sink-pin 级别的 first-pure-source 结果去复用所有 transform。
7. cluster placement order 改为跟随实际 linked-source 遍历顺序，而不是同一 pin 下所有 link 共用一个 pin ordinal。
8. supplemental envelope bounds 改为只针对当前 envelope pass 中真正抢到 first-owner target 的 node ids 计算；duplicate node group 不再追加 shifted bounds。
9. 重新运行 build、NodeInputCluster focused automation 和 PureDataSubgraph focused regression。

### 改动结果

- RED build：`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 返回 exit code 0；说明 review follow-up 的 RED 是行为失败而不是 surface 缺失。
- RED automation：`D:\UEProjects\Template\Saved\Automation\GraphLayout_NodeInputCluster_REVIEW_RED_20260601_001\index.json` 记录 `succeeded: 1`、`failed: 2`、`notRun: 0`。
- RED 失败点：
  - `MeasuresEachTransformLinkOnSamePin` 缺少第二个 transform / leaf，且 linked order 断言失败。
  - `DuplicateTransformDoesNotInflateBounds` 的 `budget height` 断言失败。
- GREEN build：同一 build 命令返回 exit code 0，输出 `Result: Succeeded`。
- NodeInputCluster GREEN：`D:\UEProjects\Template\Saved\Automation\GraphLayout_NodeInputCluster_REVIEW_GREEN_20260601_001\index.json` 记录 `succeeded: 3`、`failed: 0`、`notRun: 0`。
- NodeInputCluster GREEN 覆盖：
  - `BlueprintHelper.GraphLayout.NodeInputCluster.BudgetIncludesPureDataEnvelope`
  - `BlueprintHelper.GraphLayout.NodeInputCluster.DuplicateTransformDoesNotInflateBounds`
  - `BlueprintHelper.GraphLayout.NodeInputCluster.MeasuresEachTransformLinkOnSamePin`
- PureDataSubgraph regression：`D:\UEProjects\Template\Saved\Automation\GraphLayout_PureData_REVIEW_GREEN_20260601_001\index.json` 记录 `succeeded: 2`、`failed: 0`、`notRun: 0`。
- `Template.log` 记录上述 focused automation 最终均为 `Test Completed. Result={成功}`，并有 `**** TEST COMPLETE. EXIT CODE: 0 ****`。
- `FDataInputPlacement` 没有被扩展成 DAG traversal；GraphLayout policy 逻辑仍留在 `PureDataSubgraphPolicy` / `NodeInputClusterPolicy`。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- 这次 follow-up 扩展了 Task 4 write scope，因为 reviewer 指定的 clean fix 需要新增 source-specific pure-data measurement API。
- 没有改动 `FSolver`、UI、TaskRuntime、GraphWrite、Review、AgentFaceService、ClaudePlugin 或 CodexPlugin。
- 没有执行 `git add`、`git commit`、`git push`，也没有删除或修改 `.git` 文件。
- Historical checkpoint: 当时 implementation plan 中的 Task 4 仍然不要标记完成。Final Closure Update 已 supersede 该 checkpoint。

### 质量复审结果

- 质量复审结论：APPROVED。
- 复审确认 transform envelope 已按实际 `LinkedNodeId` 测量，不再复用同一个 sink pin 的第一个 pure source。
- 复审确认 supplemental bounds 已受 first-owner target claim 约束，duplicate transform 不再虚增 `Width/Height`。
- 复审确认 follow-up tests 覆盖 same-pin transform links、duplicate transform bounds 和原 pure-data envelope 场景。
- 根据该复审结果，implementation plan 中 Task 4 的步骤已标记完成。

## Task 3 Quality Follow-up: Pure Data Sink Source Selection

### 改动原因

Task 3 质量审计发现 `PureDataSubgraphPolicy` 初版存在一个真实缺口：当 sink 的同一个 data input pin 先连接到非 pure 节点，再连接到有效 pure data transform 时，`MeasureForSink` 会误选第一个 source 并提前返回，导致后续有效 pure data subgraph 没有被测量。

### 改动过程

1. 先补充 RED 用例 `BlueprintHelper.GraphLayout.PureDataSubgraph.SkipsNonPureFirstSinkSource`。
2. 该用例构造 `Consumer.Value <- AExecSource / MakeArray <- Proxy0`，其中 `AExecSource` 带 Exec pin，应被 `ClassifyNode` 判定为 `None`。
3. RED 验证报告为 `D:\UEProjects\Template\Saved\Automation\GraphLayout_PureData_RED_Review_20260601_001\index.json`，失败点是 root 错误为 `AExecSource`，且 `MakeArray` / `Proxy0` 没有被测量。
4. 在 `FDataEdge` 中新增 `TargetLinkedNodeOrdinal`，让 topology 保留 target pin 的实际 linked-node 顺序。
5. `FGraphTopology::GetDataInputs` 改为按 target input ordinal、target input pin id、target linked-node ordinal、source node id 排序。
6. `FPureDataSubgraphPolicy::MeasureForSink` 改为跳过缺失节点和非 pure source，只选择第一个符合 pure-data 分类的 source 作为 root。

### 改动结果

- Build：`Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload` 返回 exit code 0。
- Review GREEN：`D:\UEProjects\Template\Saved\Automation\GraphLayout_PureData_GREEN_Review_20260601_001\index.json` 记录 `succeeded: 1`、`failed: 0`。
- 原 Task 3 GREEN 回归：`D:\UEProjects\Template\Saved\Automation\GraphLayout_PureData_GREEN_20260601_002\index.json` 记录 `succeeded: 1`、`failed: 0`。
- Topology 回归：`D:\UEProjects\Template\Saved\Automation\GraphLayout_Topology_GREEN_20260601_005\index.json` 记录 `succeeded: 1`、`failed: 0`。
- `git diff --check` 对本轮相关 GraphLayout/Test 文件返回 exit code 0，仅有已修改文件的 LF/CRLF working-copy warning。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 审计结论

- 本 follow-up 没有改动 `FSolver`、UI、TaskRuntime、GraphWrite、Review、AgentFaceService、ClaudePlugin 或 CodexPlugin。
- 没有执行 `git add`、`git commit`、`git push`，也没有删除或修改 `.git` 文件。
- Task 3 仍需等待质量复审通过后，才能在 implementation plan 中标记完成。

### 质量复审结果

- 质量复审结论：APPROVED。
- 复审确认 `TargetLinkedNodeOrdinal` 已进入 topology edge contract，并且 build/sort 路径稳定。
- 复审确认 `MeasureForSink` 已跳过缺失或非 pure source，再选择第一个有效 pure data source。
- 复审确认新增/更新的 focused tests 覆盖了非 pure 首个 source 和 same-pin linked-node order。
- 根据该复审结果，implementation plan 中 Task 3 的步骤已标记完成。

## Task 3: Add PureDataSubgraphPolicy RED/GREEN

### 改动原因

Task 3 要求在不改动 `FSolver` 行为的前提下，先用 TDD 锁定 `VariableGet -> Make Array -> ForEach.Array` 这类 pure data subgraph 的测量语义，再新增独立的 `PureDataSubgraphPolicy` 边界，为后续 NodeInputClusterPolicy、row budget 和 solver orchestration 提供可复用测量结果。

### 改动过程

1. 先在 `BlueprintHelperGraphLayoutSolverTests.cpp` 添加 `BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h` include。
2. 追加 `BlueprintHelper.GraphLayout.PureDataSubgraph.MeasuresMakeArrayEnvelope` 自动化测试，构造 `ForEach.Array <- MakeArray <- Proxy0/Proxy1/Proxy2` 数据图。
3. 运行指定 build 验证 RED，失败原因是 `BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h` 尚不存在。
4. 新增 `BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h`，定义 `EPureDataNodeKind`、`FPureDataSubgraphEnvelope` 和 `FPureDataSubgraphPolicy`，命名空间为 `BlueprintHelper::GraphLayout`。
5. 新增 `BlueprintHelperGraphLayoutPureDataSubgraphPolicy.cpp`，实现 `ClassifyNode` 和 `MeasureForSink`。
6. `MeasureForSink` 通过 `FGraphTopology::GetDataInputs` 从指定 sink input pin 反向找到第一 source 作为 root，并通过 topology-owned `FindNode` 读取节点快照。
7. root transform 的 relative target 固定为 `(0, 0)`；输入 source 按 data input pin order 排列到 transform 左侧。
8. envelope size 由所有 relative target、对应 node size 和 `DataClusterPaddingX/Y` 计算。
9. 重新运行 build 和 focused UE automation 验证 GREEN。

### 改动结果

- RED：`Build.bat TemplateEditor Win64 Development` 返回 exit code 1，关键错误为缺少 `Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h`。
- GREEN build：同一 build 命令返回 exit code 0，输出 `Result: Succeeded`。
- GREEN automation：`BlueprintHelper.GraphLayout.PureDataSubgraph.MeasuresMakeArrayEnvelope` 返回 exit code 0。
- Automation report：`D:\UEProjects\Template\Saved\Automation\GraphLayout_PureData_GREEN_20260601_001\index.json` 记录 `succeeded: 1`、`failed: 0`、`notRun: 0`。
- Runtime log：`D:\UEProjects\Template\Saved\Logs\Template.log` 记录 `Test Completed. Result={成功}` 和 `**** TEST COMPLETE. EXIT CODE: 0 ****`。
- `ClassifyNode` 对任意 exec pin 返回 `None`；data-output-only 返回 `DataLeaf`；同时有 data input 和 data output 返回 `DataTransform`。
- `MeasureForSink` 不保存外部 `FGraphSnapshot` 指针，节点读取走 topology-owned `FindNode`。
- `FSolver` 未在 Task 3 中改动。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- Task 3 没有执行 `git add`、`git commit`、`git push`，也没有删除或修改 `.git` 文件。
- 当前工具面没有暴露可调用的 Task/subagent 派发工具，因此本轮按 subagent-driven-development 的检查点手动完成了实现、自检、规格审计、质量审计和验证记录。
- 真实 runtime 验证通过 `UnrealEditor-Cmd.exe` 加载真实 `Template.uproject` 执行；因为 Task 3 明确不接入 `FSolver`，本轮没有额外 CLI/solver 布局 E2E 入口可验证。

## Task 2: Add GraphLayout Topology Helper RED/GREEN

### 改动原因

Task 2 要求在不改变 `FSolver` 行为的前提下，先补充 topology 自动化测试，再新增可复用的 GraphLayout topology helper，用于保留 exec output pin identity，并为后续 pattern-based branch row / pure data policy 提供统一拓扑读取边界。

### 改动过程

1. 先在 `BlueprintHelperGraphLayoutSolverTests.cpp` 添加 `BlueprintHelperGraphLayoutTopology.h` include。
2. 追加 `BlueprintHelper.GraphLayout.Topology.PreservesExecOutputPins` 自动化测试，测试 `Split` 节点的 `ThenA` / `ThenB` exec output pin 顺序和目标节点顺序。
3. 运行指定 build 验证 RED：编译失败于缺失的 topology include/type surface。
4. 新增 `BlueprintHelperGraphLayoutTopology.h`，定义 `FExecEdge`、`FDataEdge`、`FGraphTopology`、`FGraphLayoutTopology`，命名空间为 `BlueprintHelper::GraphLayout`。
5. 新增 `BlueprintHelperGraphLayoutTopology.cpp`，从 `FGraphSnapshot` 提取 exec/data topology，并用 `Algo::Sort` 稳定排序 exec output edges。
6. 重新运行 build 与 focused UE automation 验证 GREEN。
7. 根据本地 review 修正多 Exec output 语义：未连接的 Exec output pin 也计入 `IsMultiExecOutputNode`，并补充 `SparseSplit` 测试覆盖。
8. 根据 code quality review 修复 topology ownership：`FGraphTopology` 内部复制节点快照，避免返回对象持有外部 snapshot 裸指针。
9. 补充 data edge 稳定排序和无效引用过滤，确保后续 pure-data/input-cluster policy 消费 topology 时不会受脏引用影响。
10. 根据 quality re-review 移除 `TMap` value 指针缓存，`FindNode` 直接查询 topology-owned map，避免 `TMap` 变更导致缓存指针失效。

### 改动结果

- RED：`Build.bat TemplateEditor Win64 Development` 返回 exit code 1，关键错误为 `fatal error C1083`，缺失 `Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h`。
- GREEN build：同一 build 命令返回 exit code 0，输出 `Result: Succeeded`。
- GREEN automation：`BlueprintHelper.GraphLayout.Topology.PreservesExecOutputPins` 返回 exit code 0。
- Automation report：`D:\UEProjects\Template\Saved\Automation\GraphLayout_Topology_GREEN_20260601_004\index.json` 记录 `succeeded: 1`、`failed: 0`、`notRun: 0`。
- Runtime log：`D:\UEProjects\Template\Saved\Logs\Template.log` 记录 `Test Completed. Result={成功}` 和 `**** TEST COMPLETE. EXIT CODE: 0 ****`。
- `GetExecOutputEdges(TEXT("Split"))` 按 `SourceOutputOrdinal` 再按 `TargetOrdinalWithinOutput` 排序。
- `IsMultiExecOutputNode(TEXT("Split"))` 对两个已连接 exec output pin 返回 true。
- `IsMultiExecOutputNode(TEXT("SparseSplit"))` 对两个 Exec output pin 但只有一个已连接的节点也返回 true。
- `FGraphTopology` 持有节点快照副本，`FindNode` 不依赖外部 `FGraphSnapshot` 生命周期，也不缓存 `TMap` value 指针。
- `GetDataInputs(TEXT("DataConsumer"))` 覆盖无效 source 过滤和同 pin ordinal 的稳定 source id 排序。
- `FSolver` 未在 Task 2 中改动。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

### 审计结论

- Task 2 没有执行 `git add`、`git commit`、`git push`，也没有删除或修改 `.git` 文件。
- 当前工具面没有暴露可调用的 subagent/Task 派发工具，因此本轮按 subagent-driven-development 的检查点做了手动规格审计、质量审计和验证记录。
- 真实 runtime 验证通过 `UnrealEditor-Cmd.exe` 加载真实 `Template.uproject` 执行；因为 Task 2 明确不接入 `FSolver`，本轮没有额外 CLI 布局 E2E 可验证入口。

## Task 1: Add RuleSet Switches And JSON Roundtrip RED/GREEN

### 改动原因

Task 1 要求先用 TDD 增加 RuleSet JSON roundtrip 覆盖，再把 pattern-based layout 的 RuleSet 开关和 padding 字段作为 runtime source of truth 接入 JSON import/export。

### 改动过程

1. 先在 `BlueprintHelperGraphLayoutSolverTests.cpp` 添加 `BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsPatternLayoutSettings` 自动化测试。
2. 运行指定 build 命令验证 RED，编译失败原因为 `FRuleSet` 尚未定义新增字段。
3. 在 `FRuleSet` 中新增 Task 1 要求的 3 个开关和 3 个 padding 字段。
4. 在 `ToJson(const FRuleSet&)` 中导出新增字段。
5. 在 `FRuleSetJson::Validate` / `FRuleSetJson::Import` 中支持顶层字段，并在既有 `solver` 对象导入路径中支持同名字段。
6. 根据 code quality review 补充 `solver` 嵌套对象导入测试，锁定新增 pattern 字段的兼容导入路径。
6. 重新运行 build 和 focused UE automation 验证 GREEN。

### 改动结果

- RED：build 在新增测试后失败，错误集中在 `FRuleSet` 缺少 `bAlignExecNodesHorizontally`、`bUsePureDataSubgraphLayout`、`bUsePatternRowHeightBudget`、`DataClusterPaddingX`、`DataClusterPaddingY`、`BranchRowPaddingY`。
- GREEN build：`Build.bat TemplateEditor Win64 Development` 返回 exit code 0，输出 `Result: Succeeded`。
- GREEN automation：`BlueprintHelper.GraphLayout.RuleSetJson.RoundTripsPatternLayoutSettings` 返回 exit code 0，automation report 为 `succeeded: 1`、`failed: 0`。
- Review follow-up：补充 `solver` 嵌套对象导入覆盖后再次运行 build 和 focused automation，`D:\UEProjects\Template\Saved\Automation\GraphLayout_PatternRuleSet_GREEN_20260601_002\index.json` 记录 `succeeded: 1`、`failed: 0`。
- Runtime smoke：focused test 通过 `UnrealEditor-Cmd.exe` 加载真实 `Template.uproject` 执行，`Template.log` 记录 `**** TEST COMPLETE. EXIT CODE: 0 ****`。

### 代码改动范围

- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphLayout/BlueprintHelperGraphLayoutSolverTests.cpp`

### 文档改动范围

- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

## 改动原因

`BlueprintHelper_GraphLayout_ExecNodeHorizontalAlignment_Design_20260601_CN.md` 已经确定 GraphLayout 需要从单一 exec-chain 布局扩展为 pattern-based 布局：

- Exec 节点水平对齐默认开启。
- 任意多 Exec output 节点都按分支类拓扑处理。
- Pure data input/output transform 需要独立处理，例如 `VariableGet -> Make Array -> ForEach.Array`。
- 布局优先级调整为 `Data Input Cluster -> Branch Row -> Exec Mainline`。

因此本轮产出一份可执行实现文档，用于后续 subagent-driven-development 或 executing-plans 分步实现。

## 改动过程

1. 读取设计文档和现有 GraphLayout 源码边界。
2. 核对 `FRuleSet`、RuleSet JSON、`FSolver::Solve()`、`GetExecSuccessors()`、`AlignInputsToConsumerPinOrder()`、现有 GraphLayout automation tests。
3. 编写执行计划，拆分为 RuleSet JSON、pin-level topology、pure data subgraph、node input cluster、row allocation、solver orchestration、UI grouping、CLI E2E smoke 和最终验收。
4. 新增 Debug 证据文档，记录规划阶段的源码事实和自检结果。
5. 派发只读 sourcecode-explorer subagent 审计计划与 AGENTS.md 边界。

## 改动结果

新增或确认以下文档产物：

- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphLayout_PatternBasedExecLayout_ImplementationPlan_20260601_CN.md`
- `Debug/GraphLayout_PatternBasedExecLayout_20260601.md`
- `BlueprintHelper/Develop/Report/BlueprintHelper_GraphLayout_PatternBasedExecLayout_Plan_Report_20260601_CN.md`

执行计划覆盖以下实现入口：

- `Systems/GraphLayout` 内新增 topology / pure data / input cluster / row allocation policy 边界。
- `FRuleSet` 和 RuleSet JSON 承载 runtime setting。
- `FSolver` 只负责策略编排，不在 UI 或 TaskSpec / GraphWrite / Review 边界中实现布局语义。
- 真实 E2E smoke 必须通过 BlueprintHelper CLI 创建 Blueprint graph 后检查证据。

## 代码改动范围

规划阶段没有改动 C++、TypeScript、Python 或 BlueprintHelper runtime 代码；从 Task 1 开始，本报告顶部记录了实际 C++ 改动范围与验证结果。

后续执行计划预计会改动的代码范围已在 implementation plan 的 `File Structure` 和每个 Task 的 `Files` 小节中列明。

## 审计结论

只读 subagent 审计指出：

1. 需要可检索的 Report 文档来避免会话级证据缺口。本文件即为补齐。
2. 当前状态仍是 planning evidence only，不能声明实现已经完成；自动化测试和真实 E2E 必须在后续执行实现后再作为闭环证据。

## 验证状态

本轮只做文档规划与静态自检，没有运行 UE 自动化或真实 E2E。

已执行的文档自检：

- 执行文档存在 implementation-plan header 和 agentic-worker notice。
- 执行文档任务使用 checkbox syntax。
- 执行文档未发现常见草稿标记或延后实现描述。
