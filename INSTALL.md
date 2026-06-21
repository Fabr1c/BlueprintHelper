# BlueprintHelper Install / 安装

## 中文

从仓库根目录运行安装脚本：

```powershell
.\install.cmd
```

交互式安装：

```powershell
.\install.cmd -Interactive
```

Windows 用户也可以运行：

```cmd
install.cmd
```

`install.cmd` 无参数时进入交互式安装；有参数时会透传给底层 PowerShell 安装器。

### 默认安装内容

- 构建 `AgentFaceService/task-core`、`AgentFaceService/cli`、`AgentFaceService/mcp`。
- 全局链接 CLI，使 `bh` 和 `blueprinthelper-cli` 可用。
- 直接写入 Codex `config.toml`，注册仓库本地 marketplace 并启用 `blueprint-helper@blueprint-helper-local`。
- 安装 Codex subagents 和 lifecycle-only MCP 配置。
- 在能确认唯一 `.uproject` 和 UE 根目录时写入 `<ProjectDir>/.blueprinthelper/project-profile.json`，生成 `<ProjectDir>/.blueprinthelper/AgentWorkFlow.md`，并刷新项目根 `AGENTS.md` / `CLAUDE.md` 的 BlueprintHelper marker。
- 仅在缺失时创建 Claude/Codex 用户偏好文件。

### 常用选项

```powershell
.\install.cmd -SkipCliLink
.\install.cmd -SkipBuild
.\install.cmd -Interactive
.\install.cmd -InstallClaudePlugin
.\install.cmd -InstallClaudeAgents
.\install.cmd -ProjectFile D:\UEProjects\Template\Template.uproject -EngineRoot E:\UE_5.6
.\install.cmd -RunDiagnostics
.\install.cmd -InstallUePluginToEngine -EngineRoot E:\UE_5.6
```

`-EngineRoot` 接受 `E:\UE_5.6` 或 `E:\UE_5.6\Engine`。项目 project-profile 会保存 BlueprintHelper lifecycle 工具期望的 UE root 形式；Agent 工作流规则写入 `.blueprinthelper/AgentWorkFlow.md`，根提示词文件只保留指向该文件的短入口。

### Codex Desktop

交互式安装优先使用 Node.js 内置终端交互。启用 Codex subagents 时，`blueprint-explorer`、`sourcecode-explorer`、`sourcecode-worker`、`task-worker` 会以表格显示，模型和思考等级是独立字段。模型选项为 `gpt-5.4-mini`、`gpt-5.3-codex-spark`、`gpt-5.5`、`gpt-5.4`；思考等级选项为 `high`、`xhigh`。非交互安装会自动使用推荐默认值：两个 explorer 使用轻量模型，`sourcecode-worker` 使用 `gpt-5.5 / xhigh`，`task-worker` 使用 `gpt-5.4 / high`。

仓库包含本地 marketplace：

```text
.agents/plugins/marketplace.json
```

安装脚本会直接写入 `%USERPROFILE%\.codex\config.toml`，把该 marketplace 注册为 `blueprint-helper-local`，并启用：

```text
blueprint-helper@blueprint-helper-local
```

同一个 `config.toml` 也会写入 lifecycle MCP server 和三个需要确认的工具 approval section：`blueprint_open_editor`、`blueprint_close_editor`、`blueprint_developer_exec_console_command`。

### Claude Code

Claude Code 插件支持是可选项：

```powershell
.\install.cmd -InstallClaudePlugin
```

该选项会验证本地 `ClaudePlugin` 包，直接写入 `C:\Users\<username>\.claude\settings.json`，在 `extraKnownMarketplaces` 中注册 `blueprint-helper-dev`，在 `enabledPlugins` 中启用 `blueprint-helper@blueprint-helper-dev`，同时安装 Claude sideAgent 定义，并通过 `ClaudePlugin/hooks/hooks.json` 暴露 Claude Code workflow hooks。

如果只想复制 sideAgent 定义，不想验证 Claude 插件源，使用：

```powershell
.\install.cmd -InstallClaudeAgents
```

交互式安装启用 Claude sideAgents 时，也会使用同一个 Node.js 内置终端交互。四个 sideAgent 的模型和思考等级分开选择：模型选项为 `haiku`、`sonnet`、`opus`，思考等级选项为 `high`、`xhigh`。非交互安装会自动使用推荐默认值：两个 explorer 使用 `haiku / high`，`sourcecode-worker` 使用 `opus / high`，`task-worker` 使用 `sonnet / high`。如果需要选择模型和思考等级，请使用无参数 `install.cmd` 或 `.\install.cmd -Interactive`；带参数的非交互安装不会弹出选择表单。

### Unreal Engine 插件

UE 侧插件是包含 `BlueprintHelper.uplugin` 的 `BlueprintHelper/` 文件夹。

推荐项目级安装：

```text
<YourProject>\Plugins\BlueprintHelper\BlueprintHelper.uplugin
```

也支持引擎级安装：

```powershell
.\install.cmd -InstallUePluginToEngine -EngineRoot E:\UE_5.6\Engine
```

它会复制到：

```text
<Engine>\Plugins\Marketplace\BlueprintHelper
```

Agent 工作流不依赖 UE 插件是项目级还是引擎级安装。Codex 和 CLI 使用已加载 UE 插件暴露的 Unreal Editor Bridge。请保留此源码 checkout 用于 `AgentFaceService`、`CodexPlugin` 和全局 lifecycle MCP 脚本，或在移动运行时时设置 `BLUEPRINTHELPER_ROOT`。

### 卸载

仓库根目录只保留 `.cmd` 用户入口；底层 PowerShell 和 Node 实现脚本位于 `InstallScripts/`。双击或运行：

```powershell
.\uninstall.cmd
```

交互式卸载默认移除全局 `bh` CLI 链接、Codex/Claude 插件入口、Codex/Claude subagents 和 Codex lifecycle MCP 配置。项目 `.blueprinthelper/project-profile.json`、`.blueprinthelper/AgentWorkFlow.md`、根提示词 marker 与 Engine 级 UE 插件副本默认保留，只有明确选择或传参时才删除：

```powershell
.\uninstall.cmd -RemoveProjectProfile -ProjectFile <Project.uproject>
.\uninstall.cmd -RemoveUePluginFromEngine -EngineRoot E:\UE_5.6
```

### 已废弃的 Claude setup 命令

旧的 Claude `/blueprint-helper:setup` 已合并进根安装脚本：

```powershell
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

安装后如果需要修改安全 profile、保存策略或缺失能力策略，再使用 `/blueprint-helper:configure` 或 Codex 的 `blueprint-helper-configure` skill。

## English

Run the installer from the repository root:

```powershell
.\install.cmd
```

Interactive install:

```powershell
.\install.cmd -Interactive
```

Windows users can also run:

```cmd
install.cmd
```

`install.cmd` opens the interactive installer when launched without arguments. When arguments are supplied, it passes them through to the underlying PowerShell installer.

### Default Install

- Builds `AgentFaceService/task-core`, `AgentFaceService/cli`, and `AgentFaceService/mcp`.
- Links the CLI globally so `bh` and `blueprinthelper-cli` are available, then removes npm-generated `.ps1` shims when `.cmd` launchers exist so PowerShell ExecutionPolicy does not block `bh`.
- Writes the Codex `config.toml` directly to register the repository local marketplace and enable `blueprint-helper@blueprint-helper-local`.
- Installs Codex subagents and the lifecycle-only MCP config.
- Writes `<ProjectDir>/.blueprinthelper/project-profile.json`, creates `<ProjectDir>/.blueprinthelper/AgentWorkFlow.md`, and refreshes the project-root `AGENTS.md` / `CLAUDE.md` BlueprintHelper markers when a unique `.uproject` and UE root are available.
- Creates Claude/Codex user preference files only when they are missing.

### Useful Options

```powershell
.\install.cmd -SkipCliLink
.\install.cmd -SkipBuild
.\install.cmd -Interactive
.\install.cmd -InstallClaudePlugin
.\install.cmd -InstallClaudeAgents
.\install.cmd -ProjectFile D:\UEProjects\Template\Template.uproject -EngineRoot E:\UE_5.6
.\install.cmd -RunDiagnostics
.\install.cmd -InstallUePluginToEngine -EngineRoot E:\UE_5.6
```

`-EngineRoot` accepts either `E:\UE_5.6` or `E:\UE_5.6\Engine`. The project profile stores the UE root form expected by BlueprintHelper lifecycle tools; workflow guidance lives in `.blueprinthelper/AgentWorkFlow.md`, while root prompt files keep only the managed entry marker.

### Codex Desktop

Interactive install prefers Node.js built-in terminal prompts. When Codex subagents are selected, `blueprint-explorer`, `sourcecode-explorer`, `sourcecode-worker`, and `task-worker` are shown in a table with separate model and reasoning fields. Model options are `gpt-5.4-mini`, `gpt-5.3-codex-spark`, `gpt-5.5`, and `gpt-5.4`; reasoning options are `high` and `xhigh`. Non-interactive install uses the recommended defaults automatically: lighter models for the two explorers, `gpt-5.5 / xhigh` for `sourcecode-worker`, and `gpt-5.4 / high` for `task-worker`.

The repository includes a local marketplace:

```text
.agents/plugins/marketplace.json
```

The installer writes `%USERPROFILE%\.codex\config.toml` directly, registers that marketplace as `blueprint-helper-local`, and enables:

```text
blueprint-helper@blueprint-helper-local
```

The same `config.toml` also receives the lifecycle MCP server and explicit approval sections for `blueprint_open_editor`, `blueprint_close_editor`, and `blueprint_developer_exec_console_command`.

Codex plugin packaging also includes BlueprintHelper workflow command hooks. These hooks observe shell commands, write task ledgers under `<Project>/Saved/BlueprintHelper/HookLedger`, block invalid `bh task execute` calls, and remind agents to run readback after successful execute.

### Claude Code

Claude Code plugin support is optional:

```powershell
.\install.cmd -InstallClaudePlugin
```

This validates the local `ClaudePlugin` package, writes `C:\Users\<username>\.claude\settings.json` directly, registers `blueprint-helper-dev` in `extraKnownMarketplaces`, enables `blueprint-helper@blueprint-helper-dev` in `enabledPlugins`, installs the Claude sideAgent definitions, and exposes Claude Code workflow hooks through `ClaudePlugin/hooks/hooks.json`.

If you only want to copy the sideAgent definitions without validating the Claude plugin source, use:

```powershell
.\install.cmd -InstallClaudeAgents
```

When Claude sideAgents are selected in interactive install, the same Node.js built-in terminal prompt flow is used. The four sideAgents expose separate model and reasoning fields: model options are `haiku`, `sonnet`, and `opus`, and reasoning options are `high` and `xhigh`. Non-interactive install uses the recommended defaults automatically: `haiku / high` for the two explorers, `opus / high` for `sourcecode-worker`, and `sonnet / high` for `task-worker`. Use no-argument `install.cmd` or `.\install.cmd -Interactive` when you need to choose model and reasoning; parameterized non-interactive installs do not show the selection form.

Claude workflow parity includes hook-enforced preview/execute/readback guards on Claude Code versions that support plugin hooks. The hook manifest uses `hooks/hooks.json`, `${CLAUDE_PLUGIN_ROOT}`, and the shared BlueprintHelper hook core.

### Unreal Engine Plugin

The UE-side plugin is the `BlueprintHelper/` folder containing `BlueprintHelper.uplugin`.

Recommended project install:

```text
<YourProject>\Plugins\BlueprintHelper\BlueprintHelper.uplugin
```

Engine install is also supported:

```powershell
.\install.cmd -InstallUePluginToEngine -EngineRoot E:\UE_5.6\Engine
```

That copies the plugin to:

```text
<Engine>\Plugins\Marketplace\BlueprintHelper
```

The Agent workflow is not tied to whether the UE plugin is project-installed or engine-installed. Codex and the CLI use the Unreal Editor Bridge exposed by the loaded UE plugin. Keep this source checkout available for `AgentFaceService`, `CodexPlugin`, and the global lifecycle MCP script, or set `BLUEPRINTHELPER_ROOT` if you move the runtime.

### Uninstall

The repository root keeps only `.cmd` user entry points; the underlying PowerShell and Node implementation scripts live under `InstallScripts/`. Double-click or run:

```powershell
.\uninstall.cmd
```

Interactive uninstall removes the global `bh` CLI link, Codex/Claude plugin entries, Codex/Claude subagents, and the Codex lifecycle MCP config by default. Project `.blueprinthelper/project-profile.json`, `.blueprinthelper/AgentWorkFlow.md`, project-root prompt markers, and Engine-level UE plugin copies are kept unless explicitly selected or passed as command-line options:

```powershell
.\uninstall.cmd -RemoveProjectProfile -ProjectFile <Project.uproject>
.\uninstall.cmd -RemoveUePluginFromEngine -EngineRoot E:\UE_5.6
```

### Retired Claude Setup Command

The old Claude `/blueprint-helper:setup` flow has been folded into the root installer:

```powershell
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

After installation, use `/blueprint-helper:configure` or the Codex `blueprint-helper-configure` skill only when you want to change safety profile, save policy, or missing-capability policy.

### PowerShell CLI Notes

If an older install still resolves `bh` to `bh.ps1`, rerun `install.cmd` or call `bh.cmd`. For JSON payloads, prefer `--file` or pipe generated JSON to `--stdin`; inline `--json $json` can lose quotes in PowerShell before Node receives it.

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```
