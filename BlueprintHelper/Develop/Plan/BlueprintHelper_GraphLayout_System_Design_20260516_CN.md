# BlueprintHelper GraphLayout System Design 2026-05-16

## 1. 设计结论

GraphLayout 是 UE 侧的后置视觉排版系统，不属于 TaskPlan / GraphWrite 的功能性写入职责。

TaskPlan / GraphWrite 只负责：

- 创建节点。
- 创建 Pin 和默认值。
- 创建节点连线。
- 写入非 layout 的 ownership / review / debug / transaction 事实。

GraphLayout 只负责：

- 在一个 Task 的所有功能性写入完成后读取完整图结构。
- 根据用户配置的 Layout RuleSet 计算视觉位置。
- 异步、分帧写入 `NodePosX/Y`，避免 Editor 卡顿。

Layout 不改变 Blueprint 逻辑，不进入 Review 系统，不记录 layout diff，不影响 TaskRun 的 `success/failed/modified` 判定。

## 2. 非目标

第一版不做这些事：

- 不让 TaskSpec / TaskPlan / GraphWrite payload 表达 layout 规则。
- 不把 `set_node_position` 作为普通 GraphWrite 能力继续扩展。
- 不让 Review 系统展示或记录 layout diff。
- 不让 Layout Apply 阻塞 TaskRun 完成。
- 不在 worker 线程访问 `UEdGraph`、`UEdGraphNode`、`UEdGraphPin`、`UK2Node`、Slate Widget 或 Editor subsystem。
- 不在第一版自动新增 Reroute 节点。第一版只移动已有 Reroute；自动插入 Reroute 作为后续能力。

## 3. 已废弃旧内容

当前代码中已存在一些旧 layout 入口。本轮只添加 `DEPRECATED_LAYOUT` 标注，不改变行为。

| 旧入口 | 状态 | 后续方向 |
|---|---|---|
| `FBlueprintHelperNodeFragment::LayoutHints` | 废弃 legacy hint bag | 移除，或仅保留 debug-only metadata |
| `PopulateCommonFragmentMetadata` 写入 `x/y` hint | 废弃 spawn metadata | 不作为 RuleSet / Solver 输入 |
| `FBlueprintHelperGraphFragmentLayoutRef` | 废弃 fragment debug layout model | 不作为真实排版模型 |
| TaskPlan / GraphWrite `set_node_position` | 废弃兼容 op | 从普通 GraphWrite 流程移除，必要时仅保留内部 repair/debug |
| `EBlueprintHelperPatchScope::NodePosition` | 废弃兼容 scope | 被 GraphLayout Apply path 替代 |
| `EBlueprintHelperPatchType::SetNodePosition` | 废弃兼容 patch type | 被 GraphLayout Apply path 替代 |
| `replace_blueprint_graph.options.preserve_layout` / `bPreserveLayout` | 废弃兼容选项 | 被 GraphLayout policy / RuleSet 替代 |
| AgentImport payload `layout` | 废弃 import hint | 被 GraphLayout RuleSet 替代 |
| payload-level `layout:auto` | 废弃旧默认 | GraphWrite payload 不再表达 layout |
| NodeHandler 写 `NodeData.X/Y` 到 `NodePosX/Y` | 临时生成坐标 | 允许作为 spawn 避让；最终位置由 GraphLayout 覆盖 |

## 4. 生命周期

最新生命周期如下：

```text
TaskPlan execute
-> all functional clusters finish
-> compile/save/review pipeline finishes if requested by TaskRun
-> TaskRun returns completed/failed
-> enqueue GraphLayoutJob for completed graph writes
-> GameThread snapshot complete graph
-> worker solve
-> enqueue GameThread apply batches
-> per-frame async apply NodePosX/Y
-> optional visual dirty/save policy
```

关键边界：

- LayoutJob 必须在 Task 所有内容生成完成后排队，否则它无法看到完整节点和连线。
- TaskRun 在 Layout Apply 前完成。
- LayoutJob 的失败、取消、过期不反向改变 TaskRun 结果。
- LayoutJob 可以有自己的 lightweight status，但不写 ReviewRecord，也不写 TaskRun 逻辑 diff。

## 5. 系统模块

| 模块 | 职责 | 线程 |
|---|---|---|
| `FBlueprintHelperGraphLayoutRuleSet` | 用户可交换的 layout 规则数据 | Plain data |
| `FBlueprintHelperGraphLayoutRuleSetJson` | RuleSet JSON 导入、导出、校验、版本迁移 | Game Thread 或 worker；不访问 UObject |
| `FBlueprintHelperGraphLayoutSnapshotBuilder` | 从 `UEdGraph` 捕获不可变快照 | Game Thread only |
| `FBlueprintHelperGraphLayoutClassifier` | 根据 RuleSet 将节点分类为角色 | Worker-safe |
| `FBlueprintHelperGraphLayoutSolver` | 计算节点、已有 Reroute 的目标位置 | Worker-safe |
| `FBlueprintHelperGraphLayoutCoordinator` | 管理 LayoutJob 生命周期、取消、过期校验 | Game Thread orchestration |
| `FBlueprintHelperGraphLayoutApplyQueue` | Game Thread 分帧应用 `NodePosX/Y` | Game Thread tick |
| `SBlueprintHelperLayoutRuleEditor` | 可视化编辑、预览、导入导出 RuleSet | Slate / Game Thread |

## 6. 线程模型

### 6.1 GameThread Snapshot

SnapshotBuilder 在 Game Thread 捕获纯数据：

```text
graph_id
graph_revision
ruleset_version
task_run_id
layout_job_id
node_guid
node_path
node_class
node_title
node_comment
current NodePosX/Y
estimated node size
pin id/name/direction/category/subcategory
links
ownership tags
created_or_reused_by_task flag
is_user_node flag
is_reroute flag
```

Snapshot 之后 worker 不得持有或访问 UObject 指针。

### 6.2 Worker Solve

Worker 输入：

```text
FBlueprintHelperGraphLayoutSnapshot
FBlueprintHelperGraphLayoutRuleSet
FBlueprintHelperGraphLayoutSolveOptions
```

Worker 输出：

```text
FBlueprintHelperGraphLayoutPlan:
  layout_job_id
  graph_revision
  ruleset_version
  node_targets:
    node_guid -> target_x / target_y
  reroute_targets:
    node_guid -> target_x / target_y
  warnings
  stale_guards
```

Worker 只做纯数据计算：

- 节点角色分类。
- Exec 主干排序。
- Branch / Switch / Sequence 分支 lane。
- PureFunction / VariableInput 数据依赖树。
- 多个 VariableInput 的对齐。
- 已有 Reroute 拉直。
- 碰撞避让。

### 6.3 GameThread Async Apply

Apply 不一次性写完整个 plan。Solver 完成后只把 plan 入队给 `GraphLayoutApplyQueue`。

ApplyQueue 每帧处理一个或多个 batch：

```text
max_nodes_per_frame = 24
max_ms_per_frame = 2.0
```

每帧达到任一限制就停止，下一帧继续。

每个 batch 只允许做轻量操作：

- 校验 Graph 仍存在。
- 校验 Node 仍存在。
- 校验 RuleSet / graph_revision 未过期，或按 policy 接受局部过期。
- 写入 `NodePosX/Y`。
- 记录内部 job status 计数。

Apply 阶段禁止做：

- 节点分类。
- 关系求解。
- 尺寸估算。
- RuleSet 解析。
- Review / DebugBundle 写入。
- 编译。

## 7. TaskRun / Review / Save 边界

### 7.1 TaskRun

TaskRun 只描述功能性写入结果。LayoutJob 在 TaskRun 完成后排队。

TaskRun 可以返回非逻辑提示：

```json
{
  "layout_job": {
    "status": "queued",
    "job_id": "layout_..."
  }
}
```

该提示不影响：

- `status`
- `modified`
- `modified_assets`
- compile result
- save result
- review pending changes

### 7.2 Review

Review 系统不接收 layout diff。

原因：

- `NodePosX/Y` 是用户侧视觉布局，不改变 Blueprint 逻辑。
- Review 应聚焦功能性变更。
- Layout diff 会制造噪声，降低 Review 可读性。

### 7.3 Save / Dirty

Layout Apply 会改变编辑器内节点位置。第一版建议独立配置 persistence policy：

```text
persistence:
  mark_dirty_after_apply: true
  save_after_apply: false
```

默认解释：

- `mark_dirty_after_apply=true`：用户可以看到资产有视觉布局变更。
- `save_after_apply=false`：不在 TaskRun 完成后隐式保存，避免用户误以为 TaskRun 仍在执行。

如果未来需要 CLI 自动保存视觉布局，应新增显式配置，例如：

```text
persistence.save_after_apply = true
```

即便保存失败，也只影响 LayoutJob status，不反向改变 TaskRun。

## 8. RuleSet 工作方式

RuleSet 描述角色、相对关系、对齐、Reroute、Solver 和 Apply 策略。它不描述某次 TaskRun 的具体节点坐标。

### 8.1 节点角色

第一版内置建议角色：

| Role | 颜色 | 用途 |
|---|---|---|
| `EventEntry` | purple | Custom Event、BeginPlay、InputAction、Interface Event、Override Event |
| `ExecNode` | red | 普通有 Exec pin 的执行节点 |
| `BranchControl` | orange | Branch、Switch、Sequence、Gate、DoOnce、Loop |
| `PureFunction` | green | 无 Exec pin、有数据输出的纯函数或表达式节点 |
| `VariableInput` | blue | Variable Get、Self、Literal、Component Get、对象引用 getter |
| `AsyncNode` | cyan | Delay、Timeline、Async Action、latent node |
| `DelegateNode` | yellow | Bind、Assign、Unbind、Call Dispatcher |
| `Reroute` | gray | Knot / Reroute |
| `Comment` | gray | Comment node，不参与主求解 |

分类规则必须有 priority。更具体的角色优先，例如 Branch 同时有 Exec pin，但应优先归为 `BranchControl`。

### 8.2 相对布局规则

用户编辑的是角色关系，不是绝对坐标。

示例：

```text
EventEntry -> ExecNode: right
ExecNode -> ExecNode: right
BranchControl.then -> child exec chain: right_above
BranchControl.else -> child exec chain: right_below
PureFunction -> consuming node: left
VariableInput -> consuming node: left
AsyncNode.completed -> ExecNode: right
```

### 8.3 VariableInput 对齐规则

多个 VariableInput 输入同一个消费节点时，应可配置左右对齐。

第一版建议规则：

```text
alignment_groups:
  - id: variable_inputs_to_same_consumer
    match:
      source_role: VariableInput
      same_target_node: true
      link_kind: data
    align:
      axis: x
      anchor: left_of_target
      order: target_pin_order
      spacing_y: 72
      align_node: direct_source
```

`align_node` 可选：

- `direct_source`：只对直接输入节点对齐。第一版默认。
- `chain_root`：对表达式链最左源头对齐。后续增强。

### 8.4 Reroute 拉直规则

Reroute 作为一等规则进入 RuleSet。

第一版支持：

```text
reroute_policy:
  mode: preserve | straighten_existing
  apply_to: data_links | exec_links | both
  max_added_reroutes_per_link: 0
  straighten_threshold_px: 24
```

说明：

- `preserve`：不移动 Reroute。
- `straighten_existing`：只移动本次可移动范围内已有 Reroute。
- `max_added_reroutes_per_link=0`：第一版不自动新增 Reroute。

后续可扩展：

```text
mode: insert_and_straighten
```

但它会新增节点，虽然不改变逻辑，仍需要单独的生命周期和回滚策略。

## 9. RuleSet JSON 导入导出

RuleSet 必须支持 JSON 导入导出，方便 Agent 测试和用户交换。

Schema：

```text
BlueprintHelper.GraphLayoutRuleSet.v1
```

示例：

```json
{
  "schema": "BlueprintHelper.GraphLayoutRuleSet.v1",
  "id": "readable_exec_with_left_data",
  "display_name": "Readable Exec With Left Data",
  "version": 1,
  "node_roles": [
    {
      "id": "EventEntry",
      "color": "purple",
      "priority": 100,
      "match": {
        "node_classes": ["K2Node_CustomEvent", "K2Node_Event", "K2Node_InputAction"]
      }
    },
    {
      "id": "BranchControl",
      "color": "orange",
      "priority": 90,
      "match": {
        "node_classes": ["K2Node_IfThenElse", "K2Node_Switch", "K2Node_ExecutionSequence"]
      }
    },
    {
      "id": "ExecNode",
      "color": "red",
      "priority": 50,
      "match": {
        "has_exec_pin": true
      }
    },
    {
      "id": "PureFunction",
      "color": "green",
      "priority": 40,
      "match": {
        "has_exec_pin": false,
        "has_data_output": true
      }
    },
    {
      "id": "VariableInput",
      "color": "blue",
      "priority": 30,
      "match": {
        "node_classes": ["K2Node_VariableGet", "K2Node_Self", "K2Node_Literal"]
      }
    }
  ],
  "relationship_rules": [
    {
      "id": "exec_left_to_right",
      "from_role": "ExecNode",
      "to_role": "ExecNode",
      "edge": "exec",
      "place": "right",
      "spacing_x": 420,
      "align_y": "center"
    },
    {
      "id": "data_left_of_consumer",
      "from_role": "VariableInput",
      "to_role": "*",
      "edge": "data",
      "place": "left",
      "spacing_x": 260
    }
  ],
  "alignment_groups": [
    {
      "id": "variable_inputs_to_same_consumer",
      "match": {
        "source_role": "VariableInput",
        "same_target_node": true,
        "link_kind": "data"
      },
      "align": {
        "axis": "x",
        "anchor": "left_of_target",
        "order": "target_pin_order",
        "spacing_y": 72,
        "align_node": "direct_source"
      }
    }
  ],
  "reroute_policy": {
    "mode": "straighten_existing",
    "apply_to": "data_links",
    "max_added_reroutes_per_link": 0,
    "straighten_threshold_px": 24
  },
  "solver": {
    "exec_horizontal_spacing": 420,
    "data_horizontal_spacing": 260,
    "lane_vertical_spacing": 180,
    "branch_vertical_spacing": 260,
    "avoid_overlap": true,
    "move_user_nodes": false
  },
  "apply": {
    "move_generated_nodes": true,
    "include_reused_entry_nodes": true,
    "max_nodes_per_frame": 24,
    "max_ms_per_frame": 2.0
  },
  "persistence": {
    "mark_dirty_after_apply": true,
    "save_after_apply": false
  },
  "metadata": {
    "created_by": "BlueprintHelper",
    "description": "Exec flow left-to-right, data inputs aligned on the left."
  }
}
```

### 9.1 导入规则

Import 必须：

- 校验 `schema`。
- 校验 `id`、`node_roles[].id` 唯一。
- 校验 relationship / alignment 引用的 role 存在。
- 校验 spacing、budget、priority 等数值范围。
- 对未知字段给 warning，不直接失败。
- 导入失败不覆盖当前规则。
- 导入成功生成新的 `ruleset_version`。
- 支持 dry-run validate，方便 Agent 和 CLI 测试。

### 9.2 导出规则

Export 必须：

- 输出完整 RuleSet JSON。
- 支持 pretty / minified。
- 不输出本机绝对路径。
- 不输出当前 TaskRun 的节点列表、layout plan、layout job 状态。
- 不输出 solver 缓存或分类缓存。

### 9.3 Widget 功能

Layout Rule Editor 第一版需要：

- Import JSON
- Export JSON
- Copy JSON
- Paste JSON
- Validate
- Reset to Default

导入前应展示校验结果：

```text
Valid with warnings
- Unknown color token "rose"; fallback to default.
- apply.max_nodes_per_frame clamped from 500 to 64.
```

### 9.4 Agent / CLI 测试入口

后续建议提供配置/调试向入口：

```text
blueprinthelper_validate_layout_ruleset
blueprinthelper_import_layout_ruleset
blueprinthelper_export_layout_ruleset
```

这些入口不属于普通写图 workflow，不替代 TaskSpec / TaskPlan。

## 10. 可视化编辑模型

Widget 不直接实现 runtime layout 逻辑。Widget 只编辑 RuleSet。

建议 UI：

- 左侧：角色列表和颜色。
- 中间：规则画布，用彩色框表达角色关系。
- 右侧：选中规则的参数面板。
- 底部：JSON validate / import / export 状态。

用户拖动红色、绿色、蓝色等框时，本质上是在编辑：

```text
role relationship
alignment group
spacing
priority
reroute policy
```

不是编辑绝对坐标。

## 11. LayoutJob 状态

LayoutJob 有独立状态，不进入 Review。

```text
queued
snapshotting
solving
apply_queued
applying
applied
aborted_stale_graph
aborted_ruleset_changed
aborted_editor_shutdown
failed
```

状态可用于 UI 显示和调试，但不改变 TaskRun 结果。

## 12. 第一版验收

第一版完成标准：

1. 旧 layout 字段已明确废弃，新增代码不依赖旧字段作为规则来源。
2. RuleSet v1 可导入、导出、校验。
3. TaskRun 完成后才 enqueue LayoutJob。
4. Snapshot 在 Game Thread 完成，并且 worker 不访问 UObject。
5. Solver 能处理 EventEntry、ExecNode、BranchControl、PureFunction、VariableInput、Reroute 的基础规则。
6. 多个 VariableInput 到同一消费节点时可按 `target_pin_order` 左侧对齐。
7. 已有 Reroute 可按 `straighten_existing` 拉直。
8. ApplyQueue 分帧写入，每帧受 `max_nodes_per_frame` 和 `max_ms_per_frame` 双限制。
9. Layout 不生成 ReviewRecord，不记录 layout diff，不影响 TaskRun success/failed。
10. Widget 可以导入/导出 JSON，并能展示校验 warning/error。

## 13. 风险

| 风险 | 控制 |
|---|---|
| Worker 访问 UObject | Snapshot 复制纯数据；worker contract 禁止 UObject 指针 |
| Apply 期间用户编辑图 | Apply 前校验 graph_revision / node existence；过期则 abort |
| 分帧 Apply 留下半排版状态 | Job status 显示 applying；下一帧继续；过期时 abort |
| Layout Apply 后资产变 dirty | persistence policy 显式配置，默认 mark dirty but not autosave |
| 自动新增 Reroute 改变图结构 | 第一版禁用新增，只移动已有 Reroute |
| RuleSet JSON 被 Agent 写坏 | dry-run validate；导入失败不覆盖当前配置 |
| Review 噪声 | Layout 不进入 Review/TaskRun diff |

## 14. 当前状态

已完成：

- 旧 layout 入口已添加 `DEPRECATED_LAYOUT` 标注。
- 主合同文档已声明 layout 不再属于 TaskPlan / GraphWrite。
- 本文已写入第一版 GraphLayout 系统设计。

未完成：

- 下面 2026-05-16 Implementation Update 记录当前实现状态；本节原未完成项已不再作为最新状态。

## 15. 2026-05-16 Implementation Update

已落地第一版 UE 侧 GraphLayout 系统：

- 新增 `Systems/GraphLayout` 纯数据层：RuleSet、JSON import/export/validate、SnapshotBuilder、Classifier、Solver。
- 新增 `FBlueprintHelperGraphLayoutCoordinator`：Task 后置触发、GameThread snapshot、worker solve、GameThread 分帧 apply。
- GraphWrite 成功路径只记录本次生成节点候选，不承担 layout 语义；真正 enqueue 发生在 `execute_task_plan` 返回后。
- ApplyQueue 使用 `max_nodes_per_frame` / `max_ms_per_frame` 双限制，默认 `24` 个节点、`2.0ms` 每帧。
- Solver 已覆盖 `EventEntry`、`ExecNode`、`BranchControl`、`PureFunction`、`VariableInput`、`AsyncNode`、`DelegateNode`、`Reroute`、`Comment` 的基础分类。
- 多个 `VariableInput` 默认按消费节点 input pin 顺序左侧对齐。
- 已有 `Reroute` 默认按邻近已布局节点做拉直；第一版不自动新增 Reroute。
- 新增 `SBlueprintHelperLayoutRuleEditor` 并接入 BlueprintHelper 主窗口 `Layout` 页，支持 Import / Export / Copy / Paste / Validate / Reset。
- RuleSet JSON 配置默认落在 `Saved/BlueprintHelper/GraphLayoutRules.json`，未配置时使用内置默认规则。
- Layout 不写 ReviewRecord，不写 TaskRun diff，不改变 TaskRun success/failed。

当前限制：

- 拖拽式规则画布已实现第一版；拖拽仍主要编辑 RuleSet 的参数化关系，同时会把 editor-only 画布角色位置保存到 `editor_canvas.role_centers`，保证关闭并重新打开 Widget 后视觉位置稳定。
- `save_after_apply` 已接入实际保存；保存失败仍只记录 GraphLayout job warning，不反向改变 TaskRun / Review。
- 自动插入 Reroute 仍然禁用，后续需要单独生命周期与回滚策略。

## 16. 2026-05-16 Drag Canvas And Save Apply Update

本轮目标：

- 在 Layout Rule Editor 内加入拖拽式规则画布。画布中的角色节点代表 RuleSet 参数化关系，不保存绝对坐标。
- 拖动 `ExecNode` / `BranchControl` / `PureFunction` / `VariableInput` / `AsyncNode` 等角色后，回写 RuleSet JSON 中的 spacing / offset 字段，并同步 JSON 编辑框。
- `save_after_apply=true` 时，在分帧 layout apply 完成且确实改变图位置后，保存对应 graph 的 outer package。
- 保存失败只记录 GraphLayout job log，不反向改变 TaskRun / Review 状态。

已完成：

- `SBlueprintHelperLayoutRuleEditor` 内新增拖拽式规则画布，展示 EventEntry、ExecNode、BranchControl、PureFunction、VariableInput、AsyncNode、DelegateNode、Reroute、Comment 等角色关系。
- 拖动核心角色会同步 RuleSet JSON，并自动触发现有配置保存入口；Reroute 当前由 VariableInput 到 ExecNode 的关系自动派生位置，不作为独立可拖拽源。
- 画布角色位置作为 editor-only JSON 写入 `editor_canvas.role_centers`，不参与运行时 solver 逻辑，只用于规则编辑器恢复可视化状态。
- `save_after_apply=true` 时，ApplyQueue 分帧应用完实际位置变化后，通过 editor save API 保存 graph outer package；无位置变化时不触发额外保存。
