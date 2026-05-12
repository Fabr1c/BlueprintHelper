# BlueprintHelper 混合 TaskSpec / TaskPlan 架构源文档同步索引

日期：2026-05-04  
状态：同步补丁包  
目标：把新确认的混合架构同步回现有设计文档，而不是推翻原有工具簇文档。

---

## 1. 本次同步总原则

本次同步不推翻已有 11 类工具簇，也不推翻原有四层架构。

新的统一口径是：

```text
Agent
→ MCP Agent-facing Task Tools
→ Python / MCP Task Compiler
→ UE Plugin Task Runtime
→ Existing UE Capability Clusters
```

已有工具簇改为：

```text
内部能力簇 / TaskPlan step / Debug-Expert 工具 / 测试入口
```

普通 Agent-facing 入口收敛为：

```text
blueprinthelper_read_task_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprint_get_runtime_profile
blueprinthelper_diagnostics
```

---

## 2. 必须同步的源文档

| 优先级 | 文档 | 同步内容 |
|---|---|---|
| P0 | BlueprintHelper 插件架构 | 增加 Task Compiler / Task Runtime 的混合架构口径 |
| P0 | 写工具设计.同步稿 | 增加 TaskSpec / TaskPlan / task_run_id 与 Graph Write 关系 |
| P0 | Transaction / Journal / Review | 增加 task_run_id / TaskRunJournal / Review 按 task 分组 |
| P0 | Safety Profile / dry_run | 增加 TaskSpec / TaskPlan 安全策略、context_stale、preview/execute 规则 |
| P0 | Validation / Diagnostics | 增加 read_task_context / preview_task / execute_task 边界 |
| P1 | Asset Factory / Component / Class Settings / Enhanced Input | 标注为内部 capability / TaskPlan step，不再默认 Agent 直接调用 |
| P1 | Version Roadmap | 增加 v0.4/v0.5 混合架构版本归属 |
| P1 | 核心三端能力缺口 | 增加 Task Orchestration Gap |

---

## 3. 本补丁包文件

```text
BlueprintHelper_Architecture_Synced_20260504.md
GraphWrite_Setup_Cleanup_Synced_20260504.md
06_Transaction_Journal_Review_Design_Synced_20260504.md
07_Safety_Profile_DryRun_Design_Synced_20260504.md
05_Validation_Diagnostics_Tools_Design_Synced_20260504.md
01_Asset_Factory_Tools_Design_Synced_20260504.md
02_Blueprint_Component_Tools_Design_Synced_20260504.md
03_Blueprint_Class_Settings_Tools_Design_Synced_20260504.md
04_Enhanced_Input_Boundary_Design_Synced_20260504.md
Version_Roadmap_Synced_20260504.md
Core_Three_End_Gap_Synced_20260504.md
```

---

## 4. 不需要改动的核心口径

以下口径保持不变：

```text
1. Asset Factory 只创建资产。
2. add_component 只创建组件和 attachment。
3. Class Settings 只修改类设置，不写图表逻辑。
4. Enhanced Input 当前不默认编辑 IA / IMC。
5. Append / Replace / Patch / Merge 的 Graph Write 边界不变。
6. transaction_id 仍是一写操作一次。
7. 所有 UE 写操作内部仍进入 Journal / Review。
8. 普通工具成功结果不默认返回 transaction / review / safety。
9. safety_profile 只来自 runtime_profile.active_profile。
10. dry_run 是写前预检，Review 是写后审计。
```

---

## 5. 新增核心口径

```text
1. TaskSpec 是 Agent-facing 语义规格。
2. TaskPlan 是 Task Compiler 输出给 UE Task Runtime 的执行计划。
3. TaskContextPack 用于 Agent 生成 TaskSpec 前获取足够上下文。
4. preview_task 负责 TaskSpec 校验、policy 检查、dry_run/preflight，不写资产。
5. execute_task 负责执行通过 preview 的 TaskPlan。
6. task_run_id 是一次 TaskSpec / TaskPlan 执行的总 ID。
7. TaskRunJournal 负责关联 child transaction_ids。
8. Review UI 默认应按 task_run_id 分组，再展开 transaction。
9. Bridge 层错误由 Python / MCP 归一化为 Agent-facing Task Error。
10. UE 插件侧适合做 Task Runtime，不适合做 Agent TaskSpec suggested_patch 编译器。
```

---

## 6. 建议合入路径

本同步包已按新架构检查，以下两处已修正：

```text
1. Task Compiler 统一写作 Python / MCP Task Compiler。
2. UE 执行层统一写作 UE Task Runtime → Existing UE Capability Clusters。
```

如果保持现有插件目录结构，建议放入：

```text
BlueprintHelper/Develop/Plan/HybridArchitecture/
```

或直接覆盖现有设计文档：

```text
BlueprintHelper/Develop/Plan/BlueprintHelper_Architecture.md
BlueprintHelper/Develop/Plan/GraphWrite_Setup_Cleanup.md
BlueprintHelper/Develop/Plan/Transaction_Journal_Review.md
BlueprintHelper/Develop/Plan/Safety_Profile_DryRun.md
BlueprintHelper/Develop/Plan/Validation_Diagnostics.md
```

---

## 7. 下一步建议

下一步应优先输出三份 schema 文档：

```text
1. BlueprintHelper.TaskContextPack.v1
2. BlueprintHelper.TaskSpec.v1
3. BlueprintHelper.TaskPlan.v1
```

然后再同步 Agent Skill：

```text
Agent 默认流程：read_task_context → build TaskSpec → preview_task → repair → execute_task → report summary
```
