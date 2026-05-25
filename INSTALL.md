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
- 通过 Codex 官方插件入口注册仓库本地 marketplace，并安装 `blueprint-helper@blueprint-helper-local`。
- 安装 Codex subagents 和 lifecycle-only MCP 配置。
- 在能确认唯一 `.uproject` 和 UE 根目录时写入 `<ProjectDir>/.blueprinthelper/agent-profile.json`。
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

`-EngineRoot` 接受 `E:\UE_5.6` 或 `E:\UE_5.6\Engine`。项目 agent profile 会保存 BlueprintHelper lifecycle 工具期望的 UE root 形式。

### Codex Desktop

交互式安装在启用 Codex subagents 时会显示 subagent 模型表单。表单只列出推荐组合：`gpt-5.4-mini / high`、`gpt-5.3-codex-spark / xhigh` 和 `gpt-5.4 / high`，并允许分别为 `blueprint-explorer`、`sourcecode-explorer`、`task-worker` 选择。非交互安装会自动使用推荐默认值：两个 explorer 使用轻量模型，`task-worker` 使用 `gpt-5.4 / high`。

当前安装脚本会通过 Codex 官方插件入口注册仓库本地 marketplace，并安装 `blueprint-helper@blueprint-helper-local`。如果当前机器没有可调用的 Codex 官方插件 CLI，脚本会打印可在 Codex Desktop 或官方 CLI 中继续执行的 marketplace/install 命令。

仓库包含本地 marketplace：

```text
.agents/plugins/marketplace.json
```

安装脚本会通过官方入口安装该 marketplace 中的插件：

```text
blueprint-helper@blueprint-helper-local
```

官方安装入口会读取仓库根目录下的 `.agents/plugins/marketplace.json`，并安装其中的 `blueprint-helper@blueprint-helper-local`。

### Claude Code

Claude Code 插件支持是可选项：

```powershell
.\install.cmd -InstallClaudePlugin
```

该选项会验证本地 `ClaudePlugin` 包，并通过 Claude 官方插件入口安装 `blueprint-helper@blueprint-helper-dev`，同时安装 Claude sideAgent 定义。如果当前机器没有可调用的 Claude 官方插件 CLI，脚本会打印可在 Claude Code 中执行的官方命令：

```text
/plugin marketplace add <BlueprintHelper repository root>\ClaudePlugin
/plugin install blueprint-helper@blueprint-helper-dev
```

如果只想复制 sideAgent 定义，不想验证 Claude 插件源，使用：

```powershell
.\install.cmd -InstallClaudeAgents
```

交互式安装在安装 Claude sideAgents 时会显示 sideAgent 模型表单。当前只显示推荐组合 `haiku / high` 与 `sonnet / high`，并分别确认三个 sideAgent。非交互安装会自动使用推荐默认值：两个 explorer 使用 `haiku / high`，`task-worker` 使用 `sonnet / high`。

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
- Registers the repository local marketplace through the official Codex plugin install entry, then installs `blueprint-helper@blueprint-helper-local`.
- Installs Codex subagents and the lifecycle-only MCP config.
- Writes `<ProjectDir>/.blueprinthelper/agent-profile.json` when a unique `.uproject` and UE root are available.
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

`-EngineRoot` accepts either `E:\UE_5.6` or `E:\UE_5.6\Engine`. The project agent profile stores the UE root form expected by BlueprintHelper lifecycle tools.

### Codex Desktop

When Codex subagents are selected in interactive install, the installer shows a subagent model form. It only lists the recommended profiles: `gpt-5.4-mini / high`, `gpt-5.3-codex-spark / xhigh`, and `gpt-5.4 / high`, then lets you choose a profile for `blueprint-explorer`, `sourcecode-explorer`, and `task-worker`. Non-interactive install uses the recommended defaults automatically: lighter models for the two explorers, and `gpt-5.4 / high` for `task-worker`.

The repository includes a local marketplace:

```text
.agents/plugins/marketplace.json
```

The installer registers that marketplace through the official Codex plugin install entry, then installs:

```text
blueprint-helper@blueprint-helper-local
```

When a callable official Codex plugin CLI is unavailable, the installer prints the marketplace and install commands so the same official path can be completed from Codex Desktop or an available Codex plugin CLI:

```text
plugin marketplace add <BlueprintHelper repository root>
plugin install blueprint-helper@blueprint-helper-local
```

### Claude Code

Claude Code plugin support is optional:

```powershell
.\install.cmd -InstallClaudePlugin
```

This validates the local `ClaudePlugin` package, installs `blueprint-helper@blueprint-helper-dev` through the official Claude plugin entry when a callable Claude plugin CLI is available, and installs the Claude sideAgent definitions. If no callable Claude plugin CLI is available, the installer prints the official commands to run in Claude Code:

```text
/plugin marketplace add <BlueprintHelper repository root>\ClaudePlugin
/plugin install blueprint-helper@blueprint-helper-dev
```

If you only want to copy the sideAgent definitions without validating the Claude plugin source, use:

```powershell
.\install.cmd -InstallClaudeAgents
```

When Claude sideAgents are selected in interactive install, the installer shows a sideAgent model form. The displayed recommendations are `haiku / high` and `sonnet / high`, confirmed separately for the three sideAgents. Non-interactive install uses the recommended defaults automatically: `haiku / high` for the two explorers, and `sonnet / high` for `task-worker`.

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
