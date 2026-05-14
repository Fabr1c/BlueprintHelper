# BlueprintHelper CLI / MCP 边界清点（2026-05-14 修订）

## 最终结论

MCP 重新废弃冻结。BlueprintHelper Agent-facing 工具入口仍然是 CLI，后续普通读写、测试、DebugBundle、TaskSpec 执行都只按 CLI 主线推进。

本轮曾短暂评估将 MCP 恢复为 `open_editor`/长驻生命周期通道，但实测结论修正为：编辑器进程无法在 Codex shell 工具调用后长期存活，根因是当前沙盒/宿主进程生命周期清理，不是 CLI 协议或 CLI 长连接模型天然不可行。因此不应为了这个问题恢复 MCP 主线。

## CLI 职责

CLI 是唯一正常 Agent-facing 工具入口，覆盖：

- `blueprint_open_editor`
- `blueprint_close_editor`
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

MCP 只保留为冻结的遗留兼容/排查路径：

- 不作为普通 Agent 工作流入口。
- 不新增 Agent-facing 能力。
- 不作为 `open_editor`/`close_editor` 的推荐通道。
- 仅在历史兼容、回归定位或用户明确要求检查 MCP 时被动使用。

## 沙盒限制记录

`bh open_editor` 在普通 PowerShell 中可以启动并保持编辑器长期可用；在 Codex shell 工具内，启动后的 UnrealEditor 子进程可能随工具调用结束被宿主清理。这是执行环境限制。解决方向应是沙盒/宿主启动策略或让用户在外部终端启动，而不是恢复 MCP 主线。

## Agent 规则

1. 默认只使用 CLI。
2. 遇到编辑器未启动时，优先尝试 CLI `bh open_editor`；如果当前沙盒无法保持编辑器存活，明确报告沙盒限制，并让用户在普通 PowerShell 中运行同一 CLI 命令。
3. `bh close_editor` 是短命令，继续由 CLI 承担。
4. MCP 不再进入正常任务规划。