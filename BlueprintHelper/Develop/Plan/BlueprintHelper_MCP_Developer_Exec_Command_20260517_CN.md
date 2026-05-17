# BlueprintHelper MCP Developer Exec Command

日期：2026-05-17

## 决策

本轮只给 MCP 增加开发者用途的 Unreal Editor console command 入口：

- MCP tool 名称：`blueprint_developer_exec_console_command`
- Bridge command：`exec_console_command`
- audience 元数据：`blueprinthelper/audience = developer`
- ordinary agent 标记：`blueprinthelper/ordinaryAgentCallable = false`

旧的 `blueprint_exec_console_command` 仍保持 frozen / unregistered，不恢复为普通 MCP 工具。

## 边界

普通 Agent-facing 工作流仍然不通过 MCP 执行资产读写或 console command：

- TaskSpec / ReadSpec / diagnostics / DebugCase 主线继续走 CLI / task-core。
- MCP 默认职责仍是 editor lifecycle：`blueprint_open_editor`、`blueprint_close_editor`。
- 新增 exec 工具只用于 BlueprintHelper 本地开发、测试编排和人工确认过的调试场景。

## 安全约束

MCP 注册不绕过 UE Bridge 的高风险命令防线。实际执行仍由 Bridge 端校验：

- `exec_console_command` 仍属于 high-risk command。
- Editor 进程环境需要显式允许 high-risk command。
- Bridge 写会话 / 授权仍按现有 validator 规则执行。

## 验证

- 新增 MCP regression 覆盖：developer exec tool 注册、旧 frozen 名称未注册、metadata 标记为 developer、普通 Agent-facing 列表不包含该工具、Bridge payload 只转发 `{ command }`。
- Codex global MCP 安装输出改为 `surface = lifecycle_plus_developer`，并分开列出 `agent_facing_tools` 与 `developer_tools`。
- Codex MCP hook 提示同步为：global MCP = editor lifecycle + developer-only exec；普通读写仍走 CLI。
- 已跑：`npm.cmd --prefix AgentFaceService/mcp run build:mcp` 通过。
- 已跑：内联 focused node 校验通过，确认 developer exec tool 注册、metadata 与 Bridge payload。
- 说明：直接跑整个 `tools.regression.test` 文件时，新增测试通过，但该文件仍有既存旧断言按“非 lifecycle-only MCP”预期失败；本轮未重写整套旧回归。

## 2026-05-17 UE Automation

- MCP 启动编辑器通过：`EDITOR_BRIDGE_AVAILABLE`，项目 `D:\UEProjects\Template\Template.uproject`。
- `Automation RunTests BlueprintHelper.Safety.RequestValidator.DisablesHighRiskByDefault`：PASS，1 succeeded / 0 failed，报告 `D:\UEProjects\Template\Saved\Automation\MCPDeveloperExecSafety_20260517_002\index.json`。
- `Automation RunTests BlueprintHelper.Router.Cluster`：PASS，7 succeeded / 0 failed，报告 `D:\UEProjects\Template\Saved\Automation\MCPDeveloperExecRouterCluster_20260517_001\index.json`。
- `Automation RunTests BlueprintHelper.Safety.RequestValidator`：3 succeeded / 1 failed，报告 `D:\UEProjects\Template\Saved\Automation\MCPDeveloperExecRequestValidator_20260517_001\index.json`。失败项为 `RequiresWriteSession`，当前项目 `.blueprinthelper/agent-profile.json` 开启 `AutoRepair`，普通写命令不会被 write session 拦截；该失败是测试预期与当前安全 profile 不匹配，不作为本轮 developer exec MCP 注册 bug。
- 结论：developer exec MCP 注册没有绕过 high-risk 防线；`exec_console_command` 默认禁用的 UE 自动化仍通过。普通写命令授权行为按当前 `AutoRepair` profile 放行。
