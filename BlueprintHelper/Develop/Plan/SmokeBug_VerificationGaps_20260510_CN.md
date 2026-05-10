# Smoke Bug - Verification Gaps 2026-05-10

来源：`BlueprintHelper_NewProject_Full_SmokeRun_20260510.md`

本文记录 smoke 已暴露但尚未完成验证的缺口。这里不把未执行项伪装成失败，只记录需要补跑或重写用例的事项。

## SMOKE-VER-20260510-01: UE Automation 未执行

**优先级**：P1

**现象**

- Smoke final report 中 `UE Automation: NOT RUN`。
- Review / Debug 相关自动化组也没有产出报告。

**影响**

当前 smoke 不能证明以下自动化仍通过：

- `BlueprintHelper.Review.UI`
- `BlueprintHelper.Review.Action`
- `BlueprintHelper.Review.Integration`
- `BlueprintHelper.RuntimeDiagnostics.Debug`

**建议补充**

- 按 smoke 文档第 3 节分组运行 Editor-Cmd automation。
- 每个 group 独立进程，避免报告只捕获第一组 queue。

## SMOKE-VER-20260510-02: ReviewPanel 手动环未执行

**优先级**：P1

**现象**

- Smoke final matrix 中 `ReviewPanel: NOT RUN`。
- 本仓库已有独立 `ReviewPanelBug_20260510_CN.md` 记录后续手动 ReviewPanel smoke 暴露的问题。

**影响**

该 smoke 不能证明 ReviewPanel 的 pending load、Accept、Reject、RejectAll、Graph diff block 绘制已满足合同。

**建议补充**

- 修复 ReviewPanel bug 文档中的 P0/P1 项后，重新执行 smoke 第 13 节。
- 将 ReviewPanel 手动结果回填到 smoke 报告和本索引。

## SMOKE-VER-20260510-03: Debug / DebugBundle 手动环未执行

**优先级**：P1

**现象**

- Smoke final matrix 中 `Debug: NOT RUN`。
- 未记录 `debug_case_ids`、DebugBundle manifest ids、Review summary artifact 验证结果。

**影响**

当前只证明 MCP 响应未明显泄漏敏感信息，不能证明 reject needs_action / reject_failed 会正确生成 DebugCase 和 bundle summary artifact。

**建议补充**

- 补跑 `BlueprintHelper.Review.Integration.*DebugCase*` 和 `BlueprintHelper.RuntimeDiagnostics.Debug.*`。
- 手动触发一次 reject needs_action，验证 `blueprinthelper_get_debug_case` 仍只返回 summary-only。

## SMOKE-VER-20260510-04: Object property 正向写入未验证

**优先级**：P2

**现象**

- Smoke final matrix 中 `Object property: PARTIAL`。
- invalid case blocked，但 valid write 因 class settings 被 BPI parent bug 阻断而未运行。

**建议补充**

- 使用不依赖 Interface 的 fixture 验证 valid object-property write。
- 保留 invalid-value negative case，要求返回结构化 issue 且资产不 dirty。

## SMOKE-VER-20260510-05: Negative case 设计混入了正常 create_asset 行为

**优先级**：P2

**现象**

- Smoke 中 `create_asset (neg) BP_NonExistent` 实际创建了新资产。
- 对 create_asset 来说，不存在目标资产是正常成功路径，不是 negative case。

**建议补充**

- 删除该 negative case，或改成对不存在资产执行 `edit_blueprint_graph` / `edit_object_properties`。
- 期望错误应为 `target_asset_not_found` 或等价 blocked issue。
