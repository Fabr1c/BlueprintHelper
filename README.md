# BlueprintHelper

BlueprintHelper 是一个面向 Unreal Engine 编辑器资产的辅助插件，核心目标是让 AI Agent 能够通过本地 CLI 和 Unreal Editor Bridge 安全地读取、预览和修改蓝图相关资产。

当前版本：`v0.5.4`

## 插件用途

BlueprintHelper 用于把 Agent 的高层编辑意图转换成 Unreal Editor 内可验证、可预览、可执行的资产操作。它不是通用源码编辑器，而是专注于编辑器资产工作流：

- 读取 Blueprint 图表、变量、函数、宏、组件、接口、节点和连线信息。
- 通过 `BlueprintHelper.TaskSpec.v1` 描述写入意图，并在执行前生成预览。
- 修改 Blueprint 逻辑、结构、组件、类设置、事件分发器等编辑器资产内容。
- 读取和修改 UMG Widget 树、Widget 属性、DataAsset 属性和 DataTable 行数据。
- 执行编译、保存、PIE、诊断等 Unreal Editor 相关操作；Agent 控制 Editor 启动/关闭时使用全局 MCP 生命周期工具。
- MCP 只保留 `blueprint_open_editor`、`blueprint_close_editor` lifecycle 入口；废弃 MCP 普通工具不作为 fallback。

## 使用范围

适合使用 BlueprintHelper 的场景：

- AI Agent 需要理解现有 Blueprint 资产结构。
- 需要批量或半自动生成 Blueprint 节点、函数、变量、组件或 UMG 控件。
- 需要在执行写入前检查变更范围、预览结果并降低误改风险。
- 需要把编辑器资产操作纳入可追踪的 CLI 工作流。
- 需要在 Codex、Claude Code 等 Agent 环境中访问 Unreal Editor 资产。

不适合使用 BlueprintHelper 的场景：

- C++、TypeScript、Python、JSON、配置文件、文档等普通仓库文件编辑。
- 通用文件搜索、代码搜索、构建脚本维护。
- 绕过预览和授权流程的低层级 Bridge 写入。
- 依赖当前编辑器焦点标签页进行破坏性操作，除非用户明确要求。

## 核心特色

- CLI-first 普通资产入口：默认通过 `bh <tool_name>` 访问 TaskSpec、ReadSpec、诊断和结果查询能力，适合普通 shell-capable Agent 集成。
- TaskSpec-first 写入架构：Agent 只提交语义化 `BlueprintHelper.TaskSpec.v1`，底层 TaskPlan 由 task-core 和编译器生成。
- 写入前预览：普通编辑流程先读取上下文，再 preview，最后 execute，减少盲写和误操作。
- Template-first JSON 编写：AgentGuide 提供可复制模板，减少 CLI 字段形态和 shell 转义错误。
- Unreal Editor Bridge：通过本地 Bridge 与正在运行的 Unreal Editor 通信，读写真实编辑器资产。
- 多资产面支持：覆盖 Blueprint、UMG、DataAsset、DataTable、编译保存、打开资产和运行时诊断。
- 安全边界清晰：源码文件仍由常规仓库工具处理，编辑器资产由 BlueprintHelper 处理。
- Agent 插件适配：仓库包含 CodexPlugin、ClaudePlugin 和共享的 AgentFaceService 运行时。

## 架构概览

```text
Agent
  -> BlueprintHelper CLI for ordinary reads/writes
  -> AgentFaceService task-core
  -> TaskSpec compiler / read router
  -> Unreal Editor Bridge
  -> UE Task Runtime
  -> BlueprintHelper capability clusters
```

Editor lifecycle boundary:

```text
Agent -> global BlueprintHelper MCP allowlist -> open/close target Unreal Editor
```

Agents must not use CLI lifecycle aliases (`bh open_editor`, `bh close_editor`, `blueprint_open_editor`, or `blueprint_close_editor`) to start or close Unreal Editor. If lifecycle MCP is unavailable, report `lifecycle_mcp_unavailable`.

普通写入流程：

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_task_context 或 blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> blueprinthelper_preview_task
-> blueprinthelper_request_write_session when needed
-> blueprinthelper_execute_task
-> blueprinthelper_get_task_result when needed
```

## 仓库结构

- `BlueprintHelper/`：Unreal Engine Editor 插件主体。
- `AgentFaceService/task-core/`：Bridge client、TaskSpec schema、任务编排和工具注册。
- `AgentFaceService/cli/`：Agent 使用的 CLI 入口。
- `AgentFaceService/mcp/`：全局 MCP allowlist 入口，只保留 editor open、editor close lifecycle。
- `CodexPlugin/`：Codex 插件封装和 skill。
- `ClaudePlugin/`：Claude Code 插件封装和文档。

## 快速开始

1. 将仓库放入 Unreal 项目的插件目录，例如 `YourProject/Plugins/BlueprintHelper`。
2. 在 Unreal Editor 中启用 BlueprintHelper，并按需重新编译项目。
3. 在仓库根目录运行一键安装入口：

```cmd
cd <PLUGIN_ROOT>
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

`install.cmd` 会调用底层 PowerShell 安装脚本并规避普通用户直接运行 `.ps1` 时常见的 ExecutionPolicy 问题。安装器会构建 AgentFaceService、链接 `bh` CLI、通过 Codex 官方插件入口注册本地 marketplace 并安装 `blueprint-helper@blueprint-helper-local`、安装 Codex subagents 和全局 MCP allowlist 入口，并在能确认项目和 UE 根目录时写入 `<ProjectDir>/.blueprinthelper/agent-profile.json`。需要 Claude 插件支持时追加 `-InstallClaudePlugin`；只需要 Claude sideAgents 时追加 `-InstallClaudeAgents`；需要引擎级安装 UE 插件时追加 `-InstallUePluginToEngine`。

交互式安装在安装 Codex subagents 或 Claude sideAgents 时会显示模型/思考等级表单。Codex 只显示推荐组合 `gpt-5.4-mini / high`、`gpt-5.3-codex-spark / xhigh` 与 `gpt-5.4 / high`；Claude 只显示推荐组合 `haiku / high` 与 `sonnet / high`。非交互安装会自动使用推荐默认值，其中两个 explorer 默认轻量模型，`task-worker` 默认更强模型。

安装脚本会在 `npm link` 后移除 npm 生成的 `bh.ps1` / `blueprinthelper-cli.ps1` shim，让 PowerShell 解析到 `.cmd` 启动器，避免 ExecutionPolicy 拦截 `bh`。

4. 启动目标 Unreal Editor 项目；Agent 工作流需要代管启动/关闭时使用全局 MCP allowlist 中的生命周期工具。
5. 使用 CLI 检查运行状态：

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
```

如果 `bh` 不在 PATH 中，可以直接使用：

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js blueprint_get_runtime_profile --json "{}" --select status,summary
```

PowerShell 中复杂 JSON 不要用 inline `--json $json`，优先使用 `--file` 或管道到 `--stdin`：

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```

## 更新 / Update

双击 `upgrade.cmd` 可以检查 GitHub 最新 Release，并在发现远端版本更新时提示确认后更新。更新器以 GitHub Release tag 为准，例如 `v0.5.3`、`v0.5.4`。`update.cmd` 仍保留为旧入口兼容，普通用户文档统一推荐 `upgrade.cmd`。

更新流程会先把当前插件目录备份到同级目录，例如 `BlueprintHelper.backup-v0.5.3-20260520-153000`，再下载 Release zip 并完整替换当前目录。更新或后续安装刷新失败时，会尝试从备份目录回滚当前插件目录。

更新成功后会重新构建 AgentFaceService、重新链接 `bh` CLI，并通过官方入口刷新 Codex marketplace、Codex 插件安装、Codex subagents 和全局 MCP allowlist。如果检测到已安装的 Claude sideAgents，或显式传入 `-InstallClaudePlugin`，更新器也会通过 Claude 官方入口刷新 Claude 插件并同步 Claude sideAgents。用户偏好文件和项目 `.blueprinthelper/agent-profile.json` 不会被更新器覆盖。Engine 级 UE 插件副本只会在显式传入 `-InstallUePluginToEngine` 时更新。

```powershell
.\upgrade.cmd
.\upgrade.cmd -CheckOnly
.\upgrade.cmd -Force
.\upgrade.cmd -Force -InstallClaudePlugin
.\upgrade.cmd -Force -InstallUePluginToEngine -EngineRoot E:\UE_5.6
```

Double-click `upgrade.cmd` to check the latest GitHub Release and update only after confirmation. The updater compares versions by GitHub Release tags such as `v0.5.2` and `v0.5.4`. `update.cmd` remains available as a compatibility entry, but user-facing docs should prefer `upgrade.cmd`.

Before replacing files, the updater backs up the current plugin directory next to it, for example `BlueprintHelper.backup-v0.5.3-20260520-153000`. It then downloads the Release zip and mirrors it into the current plugin directory. If the replacement or post-update refresh fails, it attempts to roll the plugin directory back from that backup.

After a successful update, the updater rebuilds AgentFaceService, relinks the `bh` CLI, and refreshes the Codex marketplace, Codex plugin install, Codex subagents, and global MCP allowlist through the official entries. If existing Claude sideAgents are detected, or `-InstallClaudePlugin` is passed explicitly, the updater also refreshes the Claude plugin through the official Claude entry and syncs Claude sideAgents. User preference files and project `.blueprinthelper/agent-profile.json` are not overwritten. Engine-level UE plugin copies are updated only when `-InstallUePluginToEngine` is explicitly passed.

Interactive install shows model/reasoning forms when installing Codex subagents or Claude sideAgents. Codex only shows the recommended `gpt-5.4-mini / high`, `gpt-5.3-codex-spark / xhigh`, and `gpt-5.4 / high` profiles; Claude only shows the recommended `haiku / high` and `sonnet / high` profiles. Non-interactive install uses the recommended defaults automatically, with lighter explorer models and a stronger `task-worker` model.

## 参与贡献

BlueprintHelper 当前优先接收来自真实 Unreal Editor 项目使用过程中的问题反馈和 bug 修复。提交前请尽量提供可复现步骤、目标资产范围、preview / execute 结果和验证命令。

详细说明见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 版本同步

`v0.5.4` 同步到以下层级：

- Unreal 插件：`BlueprintHelper/BlueprintHelper.uplugin`
- CLI：`AgentFaceService/cli/package.json`
- 共享 task-core：`AgentFaceService/task-core/package.json`
- MCP allowlist 入口：`AgentFaceService/mcp/package.json`
- Codex 插件 manifest：`CodexPlugin/.codex-plugin/plugin.json`
- Claude 插件 manifest：`ClaudePlugin/.claude-plugin/plugin.json`

## 许可证

MIT License. See `LICENSE`.
