# BlueprintHelper Hybrid Source Docs Sync Package

日期：2026-05-04  
状态：混合 TaskSpec / TaskPlan 架构确认后的来源文档同步稿  
适用范围：BlueprintHelper v0.4 / v0.5 设计文档、Agent Skill、MCP Server、UE 插件侧 Task Runtime 规划。

---

## 0. 同步结论

本次同步不推翻 2026-05-03 已确认的工具簇字段协议，而是调整它们在新架构中的位置。

```text
旧口径：Agent 直接面对大量 MCP 底层工具。
新口径：Agent 面对少量任务级 MCP 工具；现有工具簇变成 Python / UE Task Runtime 的内部 capability、debug tool、测试入口。
```

新主链路：

```text
Agent
→ MCP Agent-facing Task Tools
→ Python / MCP Task Compiler
→ UE Plugin Task Runtime
→ Existing UE Capability Clusters
→ Unreal Editor
```

新增核心概念：

```text
TaskContextPack / context_id
TaskSpec
TaskPlan
task_run_id
TaskRunJournal
Task Error Layer
Bridge Operation Error Layer
```

保留既有概念：

```text
runtime_profile
Safety Profile
dry_run
validation
transaction_id
Transaction Journal
Review
rollback
block_id
ownership metadata
LogicMD / LogicJson / RawJson / resource_ref
Append / Replace / Patch / Merge
Asset Factory / Component / Class Settings / Enhanced Input Boundary
```

---

## 1. 文档同步优先级

### P0：必须同步

| 文档 | 同步目标 |
|---|---|
| `BlueprintHelper 插件架构.txt` | 将“四层架构”扩展为“Agent-facing Task Tools + Task Compiler + UE Task Runtime + Capability Clusters”的混合架构。 |
| `写工具设计.同步稿.synced_20260503.md` | 明确 Graph Write 不再是普通 Agent 默认直连入口，而是 TaskPlan / UE Task Runtime 内部能力。 |
| `06_Transaction_Journal_Review_Design_SyncedDiff_20260503.md` | 新增 `task_run_id`、`TaskRunJournal`、task-level Review 分组、Task-level RejectAll。 |
| `05_Validation_Diagnostics_Tools_Design_SyncedDiff_20260503.md` | 新增 `read_task_context`、`preview_task`、`context_required`、`preview_blocked` 的 ok/status 语义。 |
| `07_Safety_Profile_DryRun_Design_SyncedDiff_20260503.md` | 新增 TaskSpec / TaskPlan 层的安全策略：runtime_profile 仍是权威来源，禁止 per-call profile override。 |
| `核心三端能力缺口.txt` | 从“三端能力缺口”更新为“四段任务链路缺口”：Task Compiler、Task Runtime、TaskRunJournal、Error Normalizer。 |

### P1：需要同步

| 文档 | 同步目标 |
|---|---|
| `01_Asset_Factory_Tools_Design_SyncedDiff_20260503.md` | Asset Factory 继续保留原边界，但普通 Agent 不默认直接调用；TaskSpec 的 resources / asset_policy 编译到 Asset Factory step。 |
| `02_Blueprint_Component_Tools_Design_SyncedDiff_20260503.md` | Component 工具继续作为 TaskPlan step；add_component 与 set_component_properties 仍分离。 |
| `03_Blueprint_Class_Settings_Tools_Design_SyncedDiff_20260503.md` | Class Settings 继续只负责声明层；TaskSpec 中 interface implementation body 仍转 Graph Write。 |
| `04_Enhanced_Input_Boundary_Design_SyncedDiff_20260503.md` | TaskContextPack 返回 IA 候选；TaskSpec 必须显式区分 IA 引用、IMC 编辑、事件入口接入。 |
| `UE工具设计与字段收敛.txt` | Append / Replace / Patch / Merge 字段仍有效，但默认定位变成内部 capability / debug-facing tool。 |
| `全功能测试用例生成.txt` | 测试路径改为 read_context → preview_task → execute_task；底层工具保留单元测试 / debug 测试。 |
| `版本规划与定义.txt` | v0.4/v0.5 版本线新增 TaskRun / Task Runtime / Task Compiler 主题。 |

---

## 2. 总体口径替换

### 2.1 旧表述

```text
Agent 通过 MCP 直接调用 Asset Factory、Component、Class Settings、Graph Write、Validation 等大量工具完成任务。
```

### 2.2 新表述

```text
Agent 默认通过任务级 MCP 工具提交 TaskSpec。Python / MCP 侧将 TaskSpec 编译为 TaskPlan，UE 插件侧 Task Runtime 执行 TaskPlan。现有工具簇继续作为内部 capability、debug tool 和测试入口。
```

---

## 3. 新增文档建议

建议在 `BlueprintHelper/Resources/Plan/` 新增以下文档：

```text
00_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md
08_Source_Docs_Hybrid_Sync_Map_20260504.md
09_Agent_Facing_Task_Tools_Design_20260504.md
10_TaskSpec_TaskPlan_Error_Layers_20260504.md
11_TaskRun_Journal_ContextPack_Design_20260504.md
12_Source_Doc_Addendum_Snippets_20260504.md
```

---

## 4. 新架构下的默认 Agent 流程

```text
1. Agent 调用 blueprinthelper_read_task_context。
2. Python / MCP 返回 TaskContextPack。
3. Agent 基于 context_id 生成 TaskSpec。
4. Agent 调用 blueprinthelper_preview_task。
5. preview_task 返回 passed / context_required / preview_blocked / structured error。
6. Agent 根据 suggested_patch 修正 TaskSpec，必要时询问用户。
7. Agent 调用 blueprinthelper_execute_task。
8. UE Task Runtime 执行 TaskPlan，生成 task_run_id 和多个 child transaction_id。
9. Review UI 按 task_run_id 分组显示。
10. Agent 最终报告只输出任务摘要、修改资产、编译/保存/未完成项。
```

---

## 5. 不变规则

以下规则不因新增 Task Compiler / Task Runtime 而改变：

```text
1. Asset Factory 只创建资产，不添加接口到蓝图，不写接口函数 body。
2. add_component 只创建组件和 attachment，不设置 mesh / collision / physics / material。
3. add_implemented_interface 只修改 Implemented Interfaces。
4. Append 只追加独立逻辑块。
5. Merge 才负责接入已有执行流。
6. Patch 必须精确定位 node / pin / link / default value。
7. Replace 必须替换明确目标。
8. Enhanced Input 默认不编辑 IA / IMC。
9. runtime_profile.active_profile 是 safety_profile 唯一 Agent 来源。
10. 所有真实 UE 写操作仍进入 Transaction Journal / Review。
```

---

## 6. 新增规则

```text
1. TaskSpec 不是 transaction 的替代品。
2. 一次 TaskSpec 执行生成一个 task_run_id。
3. 一个 task_run_id 可包含多个 transaction_id。
4. TaskRunJournal 负责组织 TaskSpec、TaskPlan、child transactions、validation summary。
5. Agent 默认不接收完整 child transaction_id 列表，除非 debug / rollback / failure 需要。
6. preview_task 永远不修改 UE 资产。
7. preview_task 可以返回 context_required，指导 Agent 读取更多上下文。
8. preview_blocked / dry_run blocked 属于工具成功执行但任务不可执行，通常 ok=true。
9. TaskSpec schema / semantic / suggested_patch 在 Python / MCP 层处理。
10. Bridge / UE operation error 由 Python Error Normalizer 转译为 Agent-facing Task Error。
```

---

## 7. 版本线同步建议

```text
v0.4.0：TaskRun / Transaction Review / grouped Review / rollback workflow
v0.5.0：Agent-facing Task Tools / TaskContextPack / TaskSpec / Task Compiler / TaskPlan
v0.6.0：UE Task Runtime 完整执行 DAG、跨资产任务、上下文索引与批量验证
v1.0.0：稳定 TaskSpec / TaskPlan schema、稳定 Review / rollback、稳定 debug tool 暴露策略
```

---

## 8. 实现同步建议

### Phase 1：文档与协议

```text
1. 固定 TaskContextPack.v1。
2. 固定 TaskSpec.v1。
3. 固定 TaskPlan.v1。
4. 固定 Task Error Layer。
5. 固定 Bridge Operation Error Layer。
```

### Phase 2：Python / MCP Task Compiler

```text
1. blueprinthelper_read_task_context。
2. blueprinthelper_preview_task。
3. TaskSpec schema / semantic validation。
4. suggested_patch。
5. TaskSpec → TaskPlan。
```

### Phase 3：UE Task Runtime

```text
1. ExecuteTaskPlan Bridge command。
2. TaskPlan validator。
3. TaskExecutionContext。
4. task_run_id。
5. TaskRunJournal。
6. child transaction grouping。
```

### Phase 4：Review UI

```text
1. 按 task_run_id 分组。
2. 展开 child transactions。
3. Task-level AcceptAll / RejectAll。
4. rollback blocked / failed 展示。
```
