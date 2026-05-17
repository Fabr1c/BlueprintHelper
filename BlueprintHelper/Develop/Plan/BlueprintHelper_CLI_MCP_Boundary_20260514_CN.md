# BlueprintHelper CLI / MCP 边界清点（2026-05-14 修订）

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

## 最终结论

当前边界修正为：BlueprintHelper 普通 Agent-facing 资产读写入口仍然是 CLI；MCP 只保留明确 allowlist：`blueprint_open_editor`、`blueprint_close_editor` 和开发者专用 `blueprint_developer_exec_console_command`。

原因：在 Codex 沙盒内，用普通 shell/CLI 启动的 UnrealEditor 子进程可能随工具调用结束被宿主清理；全局 MCP 生命周期工具能作为长驻 companion 管住编辑器启动/关闭。开发者 exec command 只服务本地测试编排。这个边界不改变普通资产读写主线，TaskSpec、DebugBundle、读上下文、写任务执行仍全部走 CLI。

## CLI 职责

CLI 是普通 Agent-facing 工具入口，覆盖：

- `blueprint_get_runtime_profile`
- `blueprinthelper_diagnostics`
- `blueprinthelper_diagnostics_runtime`
- `blueprinthelper_read_agent_guide`
- `blueprinthelper_read_context`
- `blueprinthelper_read_task_context`
- `blueprinthelper_read_reference_context`
- `blueprinthelper_preview_task`
- `blueprinthelper_request_write_session`
- `blueprinthelper_execute_task`
- `blueprinthelper_get_task_result`
- `blueprinthelper_get_debug_case`
- `blueprinthelper_list_debug_cases`
- `blueprinthelper_export_debug_bundle`

## MCP 职责

MCP 只保留 allowlist 职责：

- `blueprint_open_editor`
- `blueprint_close_editor`
- `blueprint_developer_exec_console_command`，仅限 BlueprintHelper 本地开发/测试编排

约束：

- 不作为普通资产读写入口。
- 不新增 Agent-facing 能力。
- 不承载 TaskSpec preview/execute/read/debug bundle。
- 不恢复旧 MCP 普通工具面。
- 不新增、不运行废弃 MCP 普通工具测试；已删除的 legacy tool regression/import/task/debug 测试不得恢复。

## 沙盒限制记录

`bh open_editor` 在普通 PowerShell 中可以启动并保持编辑器长期可用；在 Codex shell 工具内，启动后的 UnrealEditor 子进程可能随工具调用结束被宿主清理。这是执行环境限制。

当前稳定策略是：Codex 工作流使用全局 MCP 启动/关闭编辑器；编辑器就绪后，普通 BlueprintHelper 操作仍使用 CLI。需要自动化控制 Unreal console command 时，只允许使用开发者 exec command，且不能把它扩展成普通资产读写工具。

## Agent 规则

1. 普通读写、测试、DebugBundle、TaskSpec 执行默认只使用 CLI。
2. 编辑器未启动时，使用全局 MCP `blueprint_open_editor`。
3. 任务闭环结束或需要编译前，使用全局 MCP `blueprint_close_editor`。
4. 自动化测试需要 UE console command 时，可由开发者工作流使用全局 MCP `blueprint_developer_exec_console_command`。
5. 不向 Agent 暴露或恢复旧 MCP 普通工具面；不补测、不运行废弃 MCP 普通工具测试。
6. 如果 MCP allowlist 工具不可用，再明确报告环境限制，并让用户在外部 PowerShell 启动/关闭编辑器或手动执行对应开发命令。
