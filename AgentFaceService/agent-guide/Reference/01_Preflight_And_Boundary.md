# 01 - Preflight And Boundary

## 1. Task Type Decision

Agent 先判断任务是否需要 Unreal Editor:

```text
UE asset read or write -> BlueprintHelper TaskSpec-first tool flow
Source/config/docs edit -> normal repository tools
Mixed task -> split first; code edits do not use BlueprintHelper tools
```

Ordinary plugin usage must not inspect BlueprintHelper plugin package or implementation source (`CodexPlugin/`, `ClaudePlugin/`, `AgentFaceService/`, or UE `BlueprintHelper/`) to learn how to use the plugin. Use installed skill instructions, AgentGuide, CLI reference, and templates instead. Read plugin source only for explicit BlueprintHelper plugin development, installation repair, or debugging tasks.

UE asset evidence must come from BlueprintHelper CLI/Bridge, Editor-visible state/screenshots, preview/execute results, or readback results. If those evidence sources disagree, treat it as `evidence_conflict`: stop_and_report. Do not read `.uasset`, `.umap`, or other Unreal binary asset files as fallback evidence.

## 1.1 Practical Development Architecture Gate

Before any write dispatch, the Main Agent decides whether the change is Blueprint-only, source-plus-Blueprint, or source-only support.

- BlueprintHelper is a Blueprint assistance tool; it should not move heavy implementation into Blueprint.
- Complex gameplay logic, heavy computation, or graph work likely to require 30+ nodes belongs in C++.
- Simple Blueprint business logic should remain light control flow with less than 25 nodes per function/event/macro.
- C++ should expose Blueprint extension points through `BlueprintImplementableEvent`, `BlueprintNativeEvent`, or overridable interfaces.
- Data-driven content should use `UDataAsset`, config structs, or explicit Blueprint config variables instead of hardcoded content.
- When source changes are required before asset wiring, delegate a bounded `sourcecode-worker` package first, run source verification, then continue to TaskWorker for Blueprint wiring.

## 2. BlueprintHelper Tool Surface Scope

适用:

- 读取 Blueprint、UMG、DataAsset、DataTable 等 UE 资产上下文。
- 通过 TaskSpec 修改 Blueprint 图表、变量、组件、Class Settings、UMG、DataTable 行和对象属性。
- 通过 Task Runtime 执行预览、写入、验证和结果查询。

不适用:

- 全仓库代码搜索。
- C++、TypeScript、Python、配置、脚本和普通文档编辑。
- 直接修改 `.uproject`、`.uplugin`、`.Build.cs`、`.Target.cs`。
- 生成 AGENTS.md、memory 或普通项目说明。

## 3. Required Preflight

资产写入前必须确认:

1. 目标项目和 Editor/Bridge 状态可用。
2. 目标资产路径明确，例如 `/Game/Blueprints/BP_Player`。
3. 图表、函数、控件、行名或 block 锚点等目标上下文明确。
4. 修改范围明确，尤其是是否允许修改用户节点、接入已有执行流、创建资产。
5. 写入前已执行 preview，且 preview 未 blocked。

## 4. Pre-dispatch Editor/Bridge Gate

主 Agent 完成意图、目标和 scope 判断后，派发任何 SideAgent 前必须插入一个轻量 Editor/Bridge gate：

1. 确认 BlueprintHelper CLI 可用：`bh` 或 built CLI entry。
2. 运行 runtime profile：
   `bh blueprint_get_runtime_profile --json "{}" --select status,summary`
3. 如果 runtime profile 确认目标 Editor/Bridge 可达，继续派发 SideAgent。
4. 如果 runtime profile 或 diagnostics 显示 Editor/Bridge 不可用、陈旧或不是目标项目，主 Agent 可以对目标项目调用一次 `mcp__blueprint_helper__blueprint_open_editor`，然后复查 runtime profile。
5. 如果全局 lifecycle MCP 不可用，停止并报告 `lifecycle_mcp_unavailable`；不要通过 CLI lifecycle alias 或 shell 启动 Editor。
6. 如果 Editor 已启动但 Bridge 仍不可用，停止或只派发 bounded diagnostics 任务，并把 `Bridge unavailable` 放入 stop condition；不要让 SideAgent 修复 lifecycle。
7. 如果 `read_context`、截图/Editor 画面、preview、execute 或 readback 结果互相不一致，停止并报告 `evidence_conflict`；不要读取 `.uasset` / `.umap` 二进制文件作为 fallback。

该 gate 应保持简短；只有 runtime profile 缺失、含糊或报告 Bridge/runtime 问题时才运行完整 diagnostics。

## 5. Default Loop

```text
understand request
-> pre-dispatch editor/Bridge gate
-> read compact context
-> build TaskSpec
-> preview
-> request write session only if write_permission is disabled
-> execute only after preview passes
-> read task result when needed
-> report concise summary
```

Write authorization is running Editor/Bridge based. If preview succeeds but `write_permission` is disabled, call `blueprinthelper_request_write_session`; the Editor displays a simple accept/reject dialog. If the user rejects it, stop and report. Delegated SideAgents may execute within the approved scope and lifetime. Do not use `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or direct `auth_session` handling for ordinary writes.

Source-control checkout is also required before execute when P4/Perforce or another UE source-control provider marks target assets read-only, checked out by another user, conflicted, or not editable. Use `blueprinthelper_source_control_status` / `blueprinthelper_source_control_checkout` and stop on occupied, conflicted, unavailable, failed checkout, or not-editable states.

## 6. Frozen Tool Boundary

已注册但冻结的兼容、测试和专家工具不在 AgentGuide 中列为调用入口。普通 Agent 遇到 TaskSpec 无法表达的需求时，应停止报告缺口，而不是改用冻结入口。

同样，证据冲突不能通过直接读取 UE 二进制资产文件恢复。遇到 `evidence_conflict` 时只允许 stop_and_report，不能把 `.uasset`、`.umap` 或其它二进制资产文件当作事实源 fallback。
