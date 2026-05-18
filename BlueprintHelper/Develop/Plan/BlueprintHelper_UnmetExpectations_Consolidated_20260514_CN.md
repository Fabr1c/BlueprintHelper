# BlueprintHelper 当前总账 2026-05-17

本文取代旧版 `BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md` 的过期未完成清单。旧 smoke bug、旧 gap matrix 和早期 implementation plan 仍可作为历史上下文，但当前状态以本文和本轮 PASS 报告为准。

## 0. 状态口径

- `Closed`：已有源码、构建和自动化/CLI 证据，当前不再作为开放 TODO 追踪。
- `Automation Closed / UI Pending`：后端、命令行或 Automation 已闭环，但真实 Editor UI 视觉/交互仍需验收。
- `Open`：仍需要实现、复测或产品决策。
- `Future`：不阻塞当前主链路，但属于后续架构/体验增强。

本轮 2026-05-17 未使用 MCP；验证通过 `UnrealEditor-Cmd.exe` Automation 和 UBT。

## 1. 已关闭项

| ID | 项目 | 当前状态 | 证据 |
|---|---|---|---|
| D1 | UMG / DataTable dry-run 未来状态 | Closed | `../v0.4.3/ArchivedReference/RetiredReviewDebugDocs_20260518/Plan/BlueprintHelper_Smoke_Rerun_UMG_FutureState_Debug_20260517_CN.md` |
| D1-execute | UMG WidgetTree execute smoke | Closed | `Saved/Automation/UMGWidgetTreeExecute_20260517_003/index.json` |
| B1-backend | Debug / DebugBundle 后端自动化环 | Closed | `Saved/Automation/DebugRuntimeDiagnostics_20260517_002/index.json` |
| B1-review | Review reject needs_action / reject_failed -> DebugCase | Closed | `Saved/Automation/ReviewDebugNeedsAction_20260517_001/index.json`, `Saved/Automation/ReviewDebugFailed_20260517_001/index.json` |
| ReviewReject | Review reject action ring | Closed | `Saved/Automation/ReviewRejectAction_20260517_002/index.json` |
| D2 | JSON number / bool 到 UE import text | Closed | 2026-05-15 自动闭环记录；DataTable rows preview/execute 已通过 |
| D3 | `read_context` 统一上下文入口 | Closed | asset/component/variable/graph/DataTable/ObjectProperty/DataAsset/Widget context 已覆盖 |
| D4 | 缺失资产 negative read 诊断 | Closed | `target_asset_not_found` preview blocked 路径已验证 |
| A7-backend | Graph Replace Review Reject 后端链路 | Closed | CLI reject 后 pending=0；UI 视觉仍归 A7-UI |
| ToolSurface | AgentFace tool-surface 拆分 | Closed | task-core/cli build 通过，未发现剩余拆分阻塞 |

2026-05-17 本轮修复摘要：

1. UMG 新增 widget 注册 `WidgetVariableNameToGuidMap`，避免 execute 后编译出现 widget 无 GUID。
2. DataTable 和 TaskRuntime 解析内存资产时先查 loaded object，再 fallback load，消除 transient fixture dry-run warning。
3. TaskRuntime 只在实际存在 `call_function` 语句时解析 Blueprint/Graph，避免非图步骤被误当 Blueprint。
4. Review reject 在 current hash 与 `RecordedAfterHash` 不一致时进入 `needs_action`，并保留 DebugCase 链路。
5. lifecycle root/child 测试改用真实 `ChangeId`，不再把 transaction id 当 Review identity。

## 2. 仍开放的主要 TODO

### A1. ReviewPanel live Editor smoke

状态：Open。

需要在真实 Editor UI 中验证：

1. pending load。
2. Final Changes row 选择和刷新。
3. Components / MyBlueprint / Graph / WidgetTree / Details 的 row highlight 和选中态。
4. Accept / Reject / RejectAll 点击后 UI 是否稳定移除或更新对应行。
5. Debug export / DebugBundle UI 入口是否符合当前 summary/redaction 边界。

### A2. DataTable ReviewPanel BUG-01/02 UI 复核

状态：Automation/implementation evidence exists, UI Pending。

仍需真实 UI 确认：

1. 同一 DataTable row 的 Accept/Reject 操作只在预期操作列出现一次。
2. DataTable 不再分裂成 `/DT` 与 `/DT.DT` 两个 Review scope。
3. 中央 DataTable row diff 命中同一 asset scope。

### A5. Native panel parity / Details / WidgetTree / GraphPanel

状态：Open / UI Pending。

当前实现已经覆盖多个 native-style 切片，但仍不是完整复制 UE 私有面板源码后的最终形态。待确认：

1. DetailsView diff row 是否稳定对齐原生 property row。
2. WidgetTree row highlight 和 selection action 是否满足实际 ReviewPanel 操作。
3. Components / MyBlueprint / GraphPanel 缩放、滚动、选择、跳转和 underlay diff 是否稳定。
4. 新增组件这类无具体 property path 的 Review 是否需要整块 DetailsView diff，仍需产品/交互决策。

### A6. Function scope / Local Variables Review

状态：Open。

仍需补：

1. Function/event/macro scope 下 Local Variables 的 Review 分组。
2. MyBlueprint function scope 展示与 Final Changes identity 对齐。
3. 对应 Accept/Reject 行为和测试。

### A7-UI. Graph 事件/节点 Reject UI 视觉验收

状态：Backend Closed / UI Pending。

后端 CLI 和 Review reject ring 已通过；仍需在 ReviewPanel 中确认点击 Reject 后：

1. Final Changes row 从 pending 列表移除。
2. Components/MyBlueprint/Graph diff row 同步刷新。
3. target already missing、current state changed 等状态在 UI 中可解释。

### A8. Asset lifecycle root reject UI detach / reload / cascade

状态：Open / UI Pending。

后端 lifecycle 自动化已有覆盖；真实 UI 仍需确认：

1. Reject asset lifecycle root 后 same-asset child rows 是否同步移除。
2. root reject failure 时 child rows 是否保留 pending。
3. asset 被删除或 detach 后 ReviewPanel 是否稳定 reload，不跳错资产。

### A9. 2026-05-13 Physics Door ReviewPanel live bug 修复项

状态：源码修复已有记录，UI Pending。

仍需在真实 Physics Door/ReviewPanel 流程中验证旧 bug 是否不再复现。

### B1-UI. DebugBundle ReviewPanel 手动 UI 环

状态：Backend Closed / UI Pending。

后端 DebugCase、DebugBundle summary、redaction、Review summary artifact 已通过 Automation。仍需 UI 验收：

1. DebugPanel `LoadBundle` 能显示当前 bundle。
2. `CaptureFocus` 能稳定等待/遍历各 surface geometry。
3. 真实 Reject 后 DebugBundle 记录的状态与 UI 行为一致。

### B2. Baseline semantic snapshot Stage 2/3

状态：Core path complete on 2026-05-17。

已完成 Stage 1：dirty policy、baseline trust metadata、最小 `baseline.semantic.json`、TaskRunJournal baseline 诊断、DebugBundle artifact 输出。

本轮已完成：

1. ReviewStore / Reject / GraphWrite journal / Debug summary 已切换到 semantic target snapshot hash。
2. graph node / graph block 已纳入统一 target snapshot。
3. 旧 `FBlueprintHelperReviewHashService` 与 `ComputeAtomicTargetHash` 调用点已删除，不保留 legacy graph hash fallback。
4. 缺少可恢复 before snapshot 的 snapshot-restore target 会进入 `needs_action`。
5. 新增 semantic hash / reject guard / DebugBundle 自动化并通过。

2026-05-17 决策：Stage 2/3 采用 semantic target snapshot hash 全量替换旧 graph hash，不保留兼容层；旧 pending Review records 不做自动迁移，遇到旧 hash 应重新生成 Review evidence 或进入 `needs_action`。迁移计划已归档，见 `../v0.4.3/ArchivedReference/RetiredReviewDebugDocs_20260518/Plan/BlueprintHelper_BaselineSemanticHash_Migration_20260517_CN.md`。

剩余边界：

1. Accept 后 snapshot compaction 仍为非目标项，需等 retention 配置阶段单独排期。

### C1/C2/D5. 总体 SmokeRun / Total PASS 报告刷新

状态：Refreshed for automated backend scope on 2026-05-17。

最新报告：

- `BlueprintHelper_Total_PASS_Report_20260517_CN.md`

边界：该 PASS 报告覆盖当前可自动化/命令行后端范围，不声明 live ReviewPanel UI 已完成。

## 3. 文档层处理

已清理：

1. 旧 `SmokeBug_*` 文档保留为历史，不再直接代表当前开放状态。
2. `BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md` 保留为历史 gap matrix。
3. 旧 MCP 相关 smoke 记录不再作为当前执行入口；当前普通读写和测试默认走 CLI / Automation。

仍需后续可选整理：

1. 给 `BlueprintHelper_Unified_SmokeRun_Verification_20260509.md` 顶部追加指向 2026-05-17 Total PASS 报告的迁移说明。
2. 把旧文档中的 mojibake 段落逐步归档或重写为历史摘要。
3. 如果后续完成 live ReviewPanel UI smoke，再把 A1/A2/A5/A7/A8/A9/B1-UI 从本文移到 Closed。

## 4. 当前优先级

1. P0：执行 ReviewPanel live UI smoke，覆盖 A1/A2/A7/A8/A9/B1-UI。
2. P1：补 Function scope / Local Variables Review。
3. P1：推进 Baseline semantic snapshot Stage 2/3 的产品决策和实现。
4. P2：历史文档归档、旧 runbook 顶部状态指针刷新。

## 2026-05-18 ReviewPanel 自动/手动测试分账更新

自动化状态：Closed。

1. 已运行 `BlueprintHelper.Review` 总前缀自动化。
2. 报告：`D:\UEProjects\Template\Saved\Automation\ReviewPanel_All_20260518_001\index.json`。
3. 结果：120 total，110 succeeded，10 succeeded with warnings，0 failed。
4. 10 个 warning 为 ObjectPath deprecation warning，不是 ReviewPanel 断言失败。

用户手动状态：UI Pending。

1. 最新手动验收手册：`ReviewPanel_UI_ManualValidation_Runbook_20260518_CN.md`。
2. A1/A2/A5/A7/A8/A9/B1-UI 仍需真实 Editor UI 验收。
3. 手动验收重点：Final Changes 选择联动、native row highlight、GraphPanel underlay、DataTable row 操作、DataAsset/ST 只读 row action、asset lifecycle root cascade、DebugPanel LoadBundle/CaptureFocus、Clean Review Data。
4. 自动化通过不关闭 live UI smoke；只有用户手动记录通过后，才能把对应 UI 项移到 Closed。
