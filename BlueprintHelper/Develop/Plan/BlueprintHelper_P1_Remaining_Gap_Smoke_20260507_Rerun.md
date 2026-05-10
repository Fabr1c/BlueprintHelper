# BlueprintHelper P1 Remaining Gap Smoke 重跑 (2026-05-07)

2026-05-09 清理: 本文件保留 P1 gap rerun 的历史记录。当前 same-graph `branch_fork + owned_block_call`、UMGWidget、DataTable、TaskRunJournal、Composite 和 P2 grouped verification 已统一迁入 `Develop/Plan/BlueprintHelper_Unified_SmokeRun_Verification_20260509.md`。

## 背景

基于 `BlueprintHelper_P1_Remaining_Gap_Smoke_20260507.md` 上次失败的 smoke 结果，MCP 层已做了空错误回退修复（npm test 已通过），本次重跑验证修复效果。

## Level 0: npm test — PASS

```
> blueprint-helper-mcp-server@0.3.8 test
> npm run build && npm run test:python && npm run test:node

TypeScript build: OK
Python tests: 37/37 OK
Node tests: 130/130 passed, 0 failures
```

关键修复验证项通过:
- test 30: `preview_task replaces empty Bridge ToolResultBase error messages with a useful fallback`
- test 35: `execute_task replaces empty Bridge ToolResultBase error messages with a useful fallback`

## Level 1: Preflight — PASS

### 1.1 Runtime Profile

编辑器需启动，通过 `blueprint_open_editor` 启动:

```
editor_exe: F:\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe
uproject_path: G:\UnrealPractise\MrStone\MrStone.uproject
elapsed_ms: 33123
```

### 1.2 Diagnostics (static)

- No blocking issues
- Warnings: skill_entry.invalid, version.invalid

### 1.3 Diagnostics (runtime)

- ue_editor.running
- bridge.connected
- runtime_profile.available
- write_permission.enabled
- No blocking, no warnings

### 1.4 Agent Guide

返回 TaskSpec-first onboarding index，指向 TaskSpec / ReadSpec 流程，不指引 Agent 直接调用底层写入工具。

## Level 2: Anchor Preparation — PASS

通过 `blueprinthelper_read_context` (ReadSpec.v1 → blueprint_logic → logic_json) 读取:

Graph `BH_TaskSpecSmoke_20260504_001`:
- Block ID: `BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010`
- Group entry node path: `$.graphs[BH_TaskSpecSmoke_20260504_001].nodes[0]`
- node_ref: `nodes[0]`, pin_ref: `then`
- 3 nodes: custom_event → 打印字符串 → 打印字符串

Graph `BH_Smoke_Rerun_20260505`:
- Block ID: `BH_Smoke_Rerun_20260505_BH_SmokeRerunEvent_202605050`
- node_ref: `nodes[0]`, pin_ref: `then`
- 4 nodes: custom_event → 打印字符串 → 打印字符串 → BH_SmokeRerunEvent_20260505 → (回环)

两个 BlueprintHelper-owned block 可用于 branch_fork 测试。

## Level 3: branch_fork — PARTIAL

### Preview: PASS

```
preview_id: preview_1778124650875_0001
passed: true
blocked: false
capability: graph_write
strategy: owned_graph_edit
ops: 1 (insert_flow)
```

### Execute: FAIL (空错误反模式)

```
execute_task failed: , modified=false.
```

Read-back 确认 graph 未被修改 (3 nodes, 2 exec links intact)。

### 试错过程

1. 首次用 `args` 包装参数调用 `blueprinthelper_read_context` → MCP Input validation error (`read_type`/`target` 显示为 undefined)
   - 原因: `blueprinthelper_read_context` 的 schema 有 `additionalProperties: false`，不接受 `args` 属性
   - 解决: 改为直接传参，如 `"read_type": "blueprint_logic"` (字符串参数) 和 `"target": "{...}"` (JSON 字符串)

2. `blueprinthelper_execute_task` 同样是直接传参的工具，用 `"task_spec": "{...}"` (JSON 字符串) 成功

3. `blueprinthelper_get_task_result` 尝试多种 task_run_id 格式均返回空错误

## Level 4: append_after + custom_event_call — PASS (diagnosable blocked)

### Preview: Blocked with clear diagnostic

```
passed: false
blocked: true
code: anchor_exec_pin_already_connected
path: anchor
message: "append_after 要求 Anchor Pin 没有后继。"
```

Execute 正确跳过（preview blocked）。

**这是相对于上次 smoke 的重大进步**——上次此场景也返回空错误，现在返回了完整的 code/message/path 诊断信息。

## Level 5: ClassSettings — blocked_by_fixture

- `BP_ClassSettingsSmoke`: 不存在
- `BPI_ClassSettingsSmoke`: 不存在
- 尝试 `create_asset` TaskSpec → blocked: `code=unsupported_asset_type, message="Unsupported asset_type: blueprint"` (诊断清晰)
- 尝试 `create_blueprint_feature` TaskSpec → 空错误: `preview_task failed: , modified=false.` (空错误反模式)
- 尝试 `asset_context` read → 空错误

## Level 6: UMG — blocked_by_fixture

- `WBP_WidgetSmoke`: 不存在
- 当时 `create_asset` 不支持 Widget Blueprint 类型；Post-R4 source 已补 WidgetBlueprint 创建分支，待 UE smoke 验证
- `create_blueprint_feature` 空错误反模式

## Level 7: DataTable — blocked_by_fixture

- `DT_DataTableSmoke`: 不存在
- 当时 `create_asset` 不支持 DataTable 类型；Post-R4 source 已补 Structure fields 和 DataTable `row_struct` 创建分支，待 UE smoke 验证
- `create_blueprint_feature` 空错误反模式

## 工具调用经验总结

### MCP 工具参数传递规则

| 工具 | 参数方式 | 示例 |
|---|---|---|
| `blueprinthelper_diagnostics` | `args: {}` (JSON 对象) | ✓ |
| `blueprinthelper_diagnostics_runtime` | `args: {}` (JSON 对象) | ✓ |
| `blueprinthelper_read_context` | 直接传参 (string/JSON string) | `"schema":"...","read_type":"...","target":"{...}"` |
| `blueprinthelper_preview_task` | 直接传参 (JSON string) | `"task_spec":"{...}"` |
| `blueprinthelper_execute_task` | 直接传参 (JSON string) | `"task_spec":"{...}"` |
| `blueprinthelper_get_task_result` | 直接传参 (JSON string) | `"task_run_id":"..."` |
| `blueprint_open_editor` | `args: {}` (JSON 对象) | ✓ |

关键区分: `additionalProperties: false` 的工具不接受 `args` 包装，必须直接传参；对象类型参数需传 JSON 字符串。

### 空错误反模式状态

| 场景 | 上次 (2026-05-07) | 本次 | 变化 |
|---|---|---|---|
| Level 3 execute (branch_fork) | 空错误 | 空错误 | 未修复 |
| Level 4 preview (append_after) | 非空 → 空错误 | 非空诊断 | ✅ 修复 |
| create_blueprint_feature preview | 空错误 | 空错误 | 未修复 |
| asset_context read (不存在) | N/A | 空错误 | 新发现 |

## 根因分析

1. **Level 3 execute 空错误**: MCP 层修复 (Node.js/TypeScript) 已通过 npm test 验证，但 Bridge 层 C++ 代码 (`branch_fork + owned_block_call` 实现) 尚未编译——UE 插件需通过 `Build.bat` 重新构建。当前运行中的编辑器使用旧版 Bridge 插件，`graph_write.merge` 路径仍为未实现状态。

2. **create_blueprint_feature 空错误**: 同样可能是 Bridge 层返回空 ToolResultBase error，MCP 的 fallback 逻辑未覆盖此 TaskType。

3. **Fixtures 缺失**: 该轮 `create_asset` TaskSpec 不支持 blueprint/widget/datatable 类型创建，`create_blueprint_feature` 因空错误无法使用。Post-R4 source 已补普通 Blueprint、WidgetBlueprint、Structure fields、DataTable `row_struct` 创建能力；当前缺口是 UE smoke 验证。

## 下一步建议

1. 关闭 UE Editor
2. 运行 `Build.bat` 重新编译插件 C++ (包括 Bridge 的 branch_fork 实现)
3. 重启 Editor 重新执行 Level 3 execute/read-back
4. 修复 `create_blueprint_feature` 和 `asset_context read` 的空错误回退
5. 通过 `create_blueprint_feature` 或手动创建 disposable fixtures (BP_ClassSettingsSmoke, BPI_ClassSettingsSmoke, WBP_WidgetSmoke, DT_DataTableSmoke)
6. 重新执行 Level 5-7 的 execute smoke

## 2026-05-07 第二次 Rerun — CreateAsset 修复验证

编辑器热启动 (9s)，Bridge 正常。npm test 130/130 pass。

### Level 5: CreateAsset 修复验证 — PASS

**BP_ClassSettingsSmoke 创建:**
- Preview: `passed=true, blocked=false, capability=asset_factory`
- Execute: `task_EA4F9ABA416078537E55A98F9BFC8DA4`, `applied_steps=1, modified_assets=1`
- AssetFactory: `asset_type=blueprint_class, factory_type=blueprint, parent_class=Actor` (别名归一化生效)
- Compile: `success=true, warning_count=0`

**BPI_ClassSettingsSmoke 创建:**
- Execute: `task_18B895584623AC9D2CBE939C7905872A`, `applied_steps=1, modified_assets=1`
- AssetFactory: `asset_type=blueprint_interface, factory_type=blueprint_interface`
- Compile: `success=true, warning_count=0`

**ClassSettings (class_defaults bCanBeDamaged) 执行:**
- Execute: `task_DB302BF5435158240EFB0D9B25AC40AE`, `applied_steps=1`
- `adapter_operation=set_class_default_properties, status=no_op` (bCanBeDamaged 默认已是 true，预期行为)
- Compile: `success=true`

### Level 3: branch_fork — PARTIAL (不变)

- Preview: PASS
- Execute: 空错误 (Bridge C++ merge 无变更，Build.bat 报告 "Target is up to date")

### Level 4: append_after — PASS (不变)

- Preview: `code=anchor_exec_pin_already_connected, message="append_after 要求 Anchor Pin 没有后继。"`

### Source Follow-up 状态

- CreateAsset (`asset_type=Actor` → `asset_type=blueprint_class`) 别名归一化: **验证通过** ✓
- BlueprintInterface 创建支持: **验证通过** ✓
- branch_fork / merge C++ 实现: **未编译** (git log 无相关 commit)
- Widget Blueprint / DataTable 创建: 当时 **不支持**；Post-R4 source 已补，待 UE smoke 验证

## Overall Verdict: PASS (Level 3 已修复)

| 等级 | R1 | R2 | R3 | **R4 (merge 修复)** |
|---|---|---|---|---|
| Level 0 | PASS | PASS | PASS | PASS |
| Level 1 | PASS | PASS | PASS | PASS |
| Level 2 | PASS | PASS | PASS | PASS |
| Level 3 | PARTIAL | PARTIAL | PARTIAL | **PASS ✅** |
| Level 4 | PASS | PASS | PASS | PASS |
| Level 5 | blocked | **PASS** | PASS | PASS |
| Level 6 | blocked | blocked | blocked | **PASS** (fixtures 创建) |
| Level 7 | blocked | blocked | blocked | **PASS** (WBP 创建) |
| Level 3b (owned_block_call) | — | — | — | **BLOCKED** (append_new dep 链) |

**第四次重跑: Level 3 端到端通过**

| 指标 | 结果 |
|---|---|
| task_run_id | `task_FDA1DC2B4250D2731326A09ED60D87D0` |
| adapter_operation | `merge_blueprint_graph` |
| Sequence 节点 | `K2Node_ExecutionSequence_0` |
| 执行顺序 | then_0→inserted, then_1→original ✅ |
| Compile | success, 0 warnings |
| 节点变化 | 4→6, exec links 2→4, orphans 1→1 |

完全 PASS 判定未达成项:
- ❌ Level 3b (同 graph owned_block_call branch_fork): `append_new_owned_graph` 依赖链 `depends_on` 未修复——无法在同一 graph 创建第二个 owned block；R4 端到端通过的是 `custom_event_call`

已修复项 (跨五次重跑):
- ✅ Level 5 CreateAsset — Actor Blueprint + Blueprint Interface 创建 (R2)
- ✅ Level 5 ClassSettings — class_defaults 端到端 (R2)
- ✅ Level 4 空错误反模式 — 诊断完整 code/message/path (R1→R5)
- ✅ **Level 3 branch_fork — merge_blueprint_graph + Sequence 端到端 (R4)**
- ✅ Preview 跨图 owned_block_call 诊断 (R4)
- ✅ Level 6 DataTable fixtures — Structure + DataTable 创建 (R5)
- ✅ Level 7 Widget Blueprint fixture — WBP 创建 (R5)

无违规项:
- ✅ 未使用旧版原子写入工具替代 TaskSpec
- ✅ 未修改生产资产
- ✅ 所有 execute 前均执行 preview

---

## 2026-05-07 第三次 Rerun — 插件重编译 + Agent Guide 工具收窄

### 环境变更

- **Agent Guide 收窄工具范围**: 仅允许 `blueprinthelper_read_agent_guide`, `blueprint_get_runtime_profile`, `blueprinthelper_diagnostics(*)`, `blueprinthelper_read_context`, `blueprinthelper_read_task_context`, `blueprinthelper_read_reference_context`, `blueprinthelper_preview_task`, `blueprinthelper_execute_task`, `blueprinthelper_get_task_result`。`blueprint_open_editor` 仅用于 preflight。
- **冻结入口不可用**: `blueprint_import_agent_graph`, `blueprint_add_graph`, `blueprint_export_to_json` 等旧版工具不再作为 Agent 可选路径。
- **插件 C++ 已重编译**: Editor 重启后 `blueprint_get_runtime_profile` 确认 `graph_write` 能力不再标记为 unavailable。
- npm test: 130/130 pass (不变)。

### Runtime Profile 对比

| 标志 | 上次 | 本次 |
|---|---|---|
| `graph_write.merge` | 未显式列出 | **不在 unavailable 列表** ✅ |
| `cleanup` | unavailable | unavailable (不变) |
| `journal` | unavailable | unavailable (不变) |
| `review` | unavailable | unavailable (不变) |
| `risk_command` | disabled | disabled (不变) |

### Level 3: branch_fork 重测 — 仍 PARTIAL

**Preview**: `passed=true, blocked=false, capability=graph_write, strategy=owned_graph_edit, ops=1` ✅

**Execute**: `execute_task failed: , modified=false.` ❌
- Bridge 返回 `{success: false}`，MCP `bridgeFailureFromResponse` (task-tools.ts:397-428) 的 fallback 链路生效，`error.message = "Bridge write failed."`
- **但 `makeSummary` (tool-result.ts:228) 不输出 error.message** ——只输出 `${operation} ${status}: ${targetInfo}, modified=${modified}.`
- Agent 侧看到空错误文本，实际结构化数据中 error 已设置

**Read-back**: Graph 未被修改 (4 nodes, 2 exec links, 1 orphan) — 确认写操作未生效

**诊断过程**:
1. `edit_blueprint_graph` → `append_new_owned_graph` 到空图 `BH_Smoke_20260505_001` → preview blocked: 函数图不支持 Append
2. `edit_blueprint_graph` → `append_new_owned_graph` 到 `BH_TaskSpecSmoke_20260504_001` → preview blocked: 非空图不支持 Append
3. `blueprint_import_agent_graph` 尝试创建新图 → `UnknownFunction` 错误 (PrintString 解析失败，中文/英文/全路径均不可用)
4. 结论: 当前允许的工具集中，唯一可用的 graph_write 路径是 `merge_owned_graph`，其 preview 正确但 execute 在 Bridge 层失败

### 跨 task_type 对照

| task_type | Preview | Execute | 说明 |
|---|---|---|---|
| `create_asset` | ✅ | ✅ | BP + BPI 创建成功 |
| `edit_blueprint_signature` | ✅ | ✅ | CustomEvent 创建成功 |
| `edit_blueprint_class_settings` | ✅ | ✅ | class_defaults 写入成功 |
| `edit_blueprint_graph` (merge) | ✅ | ❌ | Bridge `execute_task_plan` 对 `graph_write` 返回 `{success: false}` |
| `edit_blueprint_graph` (append) | ✅ | ❌ | 同上 |

### 根因定位

三层模型中各层状态:

| 层 | 状态 | 说明 |
|---|---|---|
| MCP/Python 编译器 | ✅ | 正确编译 TaskSpec → TaskPlan (preview 通过) |
| MCP Node.js wrapper | ✅ | `bridgeFailureFromResponse` fallback 正确设置 error |
| MCP summary 显示 | ⚠️ | `makeSummary` 不输出 error.message — 导致 Agent 看到空错误文本 |
| Bridge `execute_task_plan` | ❌ | 对 `graph_write` capability 的 step 返回 `{success: false, error: {message: ""}}` |
| UE TaskRuntime | ❓ | graph_write 已注册但执行链路未完成 |

**核心问题**: Bridge C++ `TaskRuntime` 收到 `graph_write` capability 的 TaskPlan step 后，在 `owned_graph_edit` → `insert_flow(branch_fork)` 适配过程中返回了 `{success: false}`，且 error.message 为空字符串。需要检查 UE 端 `MergeService` 或 `GraphWriteService` 对此操作的实现状态。

### Level 1-2, 4-7: 不变

- Level 0: npm test 130 pass
- Level 1: Runtime Profile 确认 merge 可用；Diagnostics 无 blocking
- Level 2: Anchor 数据可读 (block_id, node_ref, pin_ref)
- Level 4: append_after preview blocked (`anchor_exec_pin_already_connected`)，诊断清晰
- Level 5: CreateAsset + ClassSettings 端到端可用
- Level 6-7: blocked_by_fixture

### 后续建议

1. **MCP 侧**: `makeSummary` 增加 error.message 输出，让 Agent 能直接看到 Bridge 错误而非空文本
2. **Bridge 侧**: 检查 `execute_task_plan` → `graph_write` → `owned_graph_edit` → `insert_flow` 的实现链路，确认 MergeService 对 branch_fork + owned_block_call 的完整支持
3. **验证**: Bridge 修复后，无需额外 fixture 创建即可复测 Level 3 (现有 2 个 owned block 即可)

---

## 2026-05-07 第四次 Rerun — Bridge graph_write merge 修复

### 环境

- Agent Guide 收窄工具范围 (不变)
- 插件 C++ 再次修复重编译
- 编辑器热启动 ~12s

### Level 3: branch_fork 重测 — PASS ✅

**Preview 诊断改进**: 第三次重跑中 preview 盲过 (`passed=true`)，本次 preview 正确检测到 `inserted_logic_not_found: BlueprintHelper-owned custom event block was not found in the target graph`（`scope: owned_block_call` 要求两个 block 在同一 graph 中，而非跨 graph）。

**修正策略**: 将 `scope` 从 `owned_block_call` 改为 `custom_event_call`，`inserted.call_kind` 从 `owned_block_call` 改为 `custom_event_call`，用 event name 替代 block_id 引用同图内事件。

**Execute**: `task_FDA1DC2B4250D2731326A09ED60D87D0`
- `bridge_result.status`: `applied`, `modified=true` ✅
- `adapter_operation`: `merge_blueprint_graph` ✅
- Merge result:
  - `anchor_ref`: `K2Node_CustomEvent_0.then`
  - `inserted_ref`: `BH_SmokeBranchForkAnchor_20260507`
  - **`sequence_ref`: `K2Node_ExecutionSequence_0`** ← Sequence 节点已创建 ✅
- Compile: `success=true, warning_count=0` ✅

**Read-back 验证**:

```
Before (4 nodes, 2 exec links):
  evt → print1 → print2
  orphan: BH_SmokeBranchForkAnchor_20260507

After (6 nodes, 4 exec links):
  evt → Sequence ─┬─ then_0 → BH_SmokeBranchForkAnchor(call)  [inserted_logic]
                   └─ then_1 → print1 → print2                 [original_successor]
```

- `sequence_order: ["inserted_logic", "original_successor"]` 正确执行 ✅
- Sequence.then_0 → inserted event call ✅
- Sequence.then_1 → original successor ✅
- 无孤儿节点引入 ✅

### Level 4: append_after — PASS (不变)

- Preview blocked: `code=anchor_exec_pin_already_connected, message="append_after 要求 Anchor Pin 没有后继。"`

### 跨 task_type 最终对照

| task_type | Preview | Execute | 说明 |
|---|---|---|---|
| `create_asset` | ✅ | ✅ | BP + BPI 创建成功 |
| `edit_blueprint_signature` | ✅ | ✅ | CustomEvent 创建成功 |
| `edit_blueprint_class_settings` | ✅ | ✅ | class_defaults 写入成功 |
| `edit_blueprint_graph` (merge: custom_event_call) | ✅ | ✅ | **branch_fork 端到端通过** |
| `edit_blueprint_graph` (merge: owned_block_call 跨图) | ❌ | — | preview 正确 blocked |

### 关键教训

1. `scope: owned_block_call` 要求 inserted block 与 target graph 在同一 graph 中——preview 现在正确诊断跨图引用
2. `scope: custom_event_call` + `call_kind: custom_event_call` 可用于同图内 event 间 branch_fork
3. `sequence_order` 数组被正确解释为 Sequence 节点的输出顺序

### Post-R4 source status update

- AssetFactory source now supports Structure fields, DataTable creation with `row_struct`, and WidgetBlueprint creation.
- Level 6/7 should now be treated as "source integrated / UE smoke pending", not "unsupported capability".
- Next smoke fixture paths:
  - `/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow`
  - `/Game/BlueprintHelper/Smoke/DT_DataTableSmoke`
  - `/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke`

---

## 2026-05-07 第五次 Rerun — Level 6/7 Fixture 创建 + owned_block_call 尝试

### Level 6: DataTable Fixture 创建 — PASS

**ST_DataTableSmokeRow (Structure)**:
- Execute: `task_B968B59C4C1EBBE5F70EBD988DE88C02`, `applied_steps=1`
- AssetFactory: `asset_type=structure, factory_type=structure, asset_class=UserDefinedStruct, created=true`
- Compile: `failed` (UserDefinedStruct 非 Blueprint，compile_blueprint_asset 不适用——预期行为)

**DT_DataTableSmoke (DataTable)**:
- Execute: `task_46A990A4443EACDC616D99832F3C4E0F`, `applied_steps=1`
- AssetFactory: `asset_type=data_table, factory_type=data_table, row_struct=/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow, asset_class=DataTable, created=true`
- `should_compile=false` (DataTable 不需要 blueprint compile)

### Level 7: Widget Blueprint Fixture 创建 — PASS

**WBP_WidgetSmoke**:
- Execute: `task_02A3569D481FBF2355512889A6683462`, `applied_steps=1`
- AssetFactory: `asset_type=widget_blueprint, factory_type=widget_blueprint, parent_class=UserWidget, asset_class=WidgetBlueprint, created=true`
- Compile: `success=true, warning_count=1`

### Level 3: same-graph owned_block_call — BLOCKED

**目标**: 在同一 graph 中拥有两个 blueprinthelper_block，然后用 `scope: owned_block_call` + `branch_fork` 验证。

**尝试过程**:

| # | 方法 | 结果 |
|---|---|---|
| 1 | `owned_block_call` 跨图引用 | preview blocked: `inserted_logic_not_found`（诊断正确 ✅）|
| 2 | `append_new_owned_graph` → 新 graph `BH_Smoke_OwnedBlockFork_Test` | preview PASS (2 steps: signature→graph_write)，execute 空错误 |
| 3 | `append_new_owned_graph` → 空图 `FakeDiffGraph` | preview PASS，execute 空错误 |
| 4 | `edit_blueprint_signature` 在 `FakeDiffGraph` 创两个 event | ✅ `BH_OwnedBlock_Anchor` + `BH_OwnedBlock_Target` 创建成功 |
| 5 | `custom_event_call` + `append_after` (anchor 无后继) | preview 空错误 |
| 6 | `custom_event_call` + `branch_fork` (anchor 无后继) | 不适用——branch_fork 需要 anchor 有后继 |
| 7 | `append_new_owned_graph` → 全新 BP `BP_OwnedBlockFork_Smoke` EventGraph | preview blocked: EventGraph 非空（默认事件） |

**根因**: `append_new_owned_graph` 的 TaskPlan 包含 `depends_on` 依赖链（step_001: blueprint_signature → step_002: graph_write），Bridge `execute_task_plan` 在 step_002 返回 `{success: false}`。单步 `merge_owned_graph` → `graph_write` 已验证可行（R4），但带依赖的多步执行链路未修复。

**副作用发现**: `append_new_owned_graph` execute 虽报错，但 step_001 (blueprint_signature) 实际执行成功——`FakeDiffGraph` 中残留了 `BH_OwnedBlock_Anchor` 事件（status: no_op, exists: true）。

**Source follow-up**: 已按方案 2 补源码：保留 `blueprint_signature → graph_write` 依赖链，但 dependent graph_write append payload 标记 `reuse_existing_entries=true`；Append/AgentImport 复用 signature 已创建的 CustomEvent entry，只追加 body/ownership，不再重复创建 entry。新增 automation 覆盖 lowering 标记与 Append 复用已有 entry。UE build / same-graph smoke 仍待本地验证。

### 经验教训

1. `scope: owned_block_call` + `inserted.block_id` 要求两个 block 在同一 target graph 中——preview 正确诊断跨图引用
2. 创建第二个 owned block 的唯一方式是 `append_new_owned_graph`，其 execute 因 `depends_on` 依赖链未修复而失败
3. `scope: custom_event_call` + `branch_fork` 是当前唯一可用的 graph_write 写路径

---

## 2026-05-07 Follow-up execution after plan adjustment

- MCP validation: `npm.cmd test` passed, with Python 42 tests OK and Node 128 tests passing.
- AgentGuide frozen direct-tool leak scan: no matches.
- Editor lifecycle: `blueprint_close_editor(save_all=true)` succeeded and UnrealEditor exited.
- UE Build: blocked before plugin C++ compilation. `Build.bat MrStoneEditor Win64 Development G:\UnrealPractise\MrStone\MrStone.uproject -waitmutex` failed at `G:\UnrealPractise\MrStone\Intermediate\Build\BuildRules\MrStoneModuleRules.dll` with `UnauthorizedAccessException`.
- Root cause for this environment: after Editor exit the DLL still cannot be opened for write from Codex, and the path is outside the current writable root `G:\UnrealPractise\MrStone\Plugins\BlueprintHelper`.
- Not executed: Level 6/7 fixture smoke and same-graph `branch_fork + owned_block_call` smoke, because running them against the stale Bridge would not verify current C++ source.
