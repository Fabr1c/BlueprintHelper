# BlueprintHelper Install / 安装

## 中文

从仓库根目录运行安装脚本：

```powershell
.\install.ps1
```

交互式安装：

```powershell
.\install.ps1 -Interactive
```

Windows 用户也可以运行：

```cmd
install.cmd
```

`install.cmd` 无参数时进入交互式安装；有参数时会透传给 `install.ps1`。

### 默认安装内容

- 构建 `AgentFaceService/task-core`、`AgentFaceService/cli`、`AgentFaceService/mcp`。
- 全局链接 CLI，使 `bh` 和 `blueprinthelper-cli` 可用。
- 注册 Codex Desktop 本地 marketplace 条目 `blueprint-helper`。
- 安装 Codex subagents 和 lifecycle-only MCP 配置。
- 在能确认唯一 `.uproject` 和 UE 根目录时写入 `<ProjectDir>/.blueprinthelper/agent-profile.json`。
- 仅在缺失时创建 Claude/Codex 用户偏好文件。

### 常用选项

```powershell
.\install.ps1 -SkipCliLink
.\install.ps1 -SkipBuild
.\install.ps1 -Interactive
.\install.ps1 -InstallClaudePlugin
.\install.ps1 -InstallClaudeAgents
.\install.ps1 -ProjectFile D:\UEProjects\Template\Template.uproject -EngineRoot E:\UE_5.6
.\install.ps1 -RunDiagnostics
.\install.ps1 -InstallUePluginToEngine -EngineRoot E:\UE_5.6
```

`-EngineRoot` 接受 `E:\UE_5.6` 或 `E:\UE_5.6\Engine`。项目 agent profile 会保存 BlueprintHelper lifecycle 工具期望的 UE root 形式。

### Codex Desktop

仓库包含本地 marketplace：

```text
.agents/plugins/marketplace.json
```

安装脚本也会创建或更新用户目录下的 Codex marketplace：

```text
%USERPROFILE%\.agents\plugins\marketplace.json
```

它会让 `./plugins/blueprint-helper` 指向当前 checkout 的 `CodexPlugin`。如果该路径已存在且指向别处，只有在你确认要替换时才使用 `-Force`。

### Claude Code

Claude Code 插件支持是可选项：

```powershell
.\install.ps1 -InstallClaudePlugin
```

该选项会验证本地 `ClaudePlugin` 包，并安装 Claude sideAgent 定义。然后从仓库根目录启动 Claude Code，并运行安装脚本打印的命令：

```text
/plugin marketplace add ./ClaudePlugin
/plugin install blueprint-helper@blueprint-helper-dev
```

如果只想复制 sideAgent 定义，不想验证 Claude 插件源，使用：

```powershell
.\install.ps1 -InstallClaudeAgents
```

### Unreal Engine 插件

UE 侧插件是包含 `BlueprintHelper.uplugin` 的 `BlueprintHelper/` 文件夹。

推荐项目级安装：

```text
<YourProject>\Plugins\BlueprintHelper\BlueprintHelper.uplugin
```

也支持引擎级安装：

```powershell
.\install.ps1 -InstallUePluginToEngine -EngineRoot E:\UE_5.6\Engine
```

它会复制到：

```text
<Engine>\Plugins\Marketplace\BlueprintHelper
```

Agent 工作流不依赖 UE 插件是项目级还是引擎级安装。Codex 和 CLI 使用已加载 UE 插件暴露的 Unreal Editor Bridge。请保留此源码 checkout 用于 `AgentFaceService`、`CodexPlugin` 和全局 lifecycle MCP 脚本，或在移动运行时时设置 `BLUEPRINTHELPER_ROOT`。

### 已废弃的 Claude setup 命令

旧的 Claude `/blueprint-helper:setup` 已合并进根安装脚本：

```powershell
.\install.ps1 -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

安装后如果需要修改安全 profile、保存策略或缺失能力策略，再使用 `/blueprint-helper:configure` 或 Codex 的 `blueprint-helper-configure` skill。

## English

Run the installer from the repository root:

```powershell
.\install.ps1
```

Interactive install:

```powershell
.\install.ps1 -Interactive
```

Windows users can also run:

```cmd
install.cmd
```

`install.cmd` opens the interactive installer when launched without arguments. When arguments are supplied, it passes them through to `install.ps1`.

### Default Install

- Builds `AgentFaceService/task-core`, `AgentFaceService/cli`, and `AgentFaceService/mcp`.
- Links the CLI globally so `bh` and `blueprinthelper-cli` are available, then removes npm-generated `.ps1` shims when `.cmd` launchers exist so PowerShell ExecutionPolicy does not block `bh`.
- Registers the Codex Desktop local marketplace entry for `blueprint-helper`.
- Installs Codex subagents and the lifecycle-only MCP config.
- Writes `<ProjectDir>/.blueprinthelper/agent-profile.json` when a unique `.uproject` and UE root are available.
- Creates Claude/Codex user preference files only when they are missing.

### Useful Options

```powershell
.\install.ps1 -SkipCliLink
.\install.ps1 -SkipBuild
.\install.ps1 -Interactive
.\install.ps1 -InstallClaudePlugin
.\install.ps1 -InstallClaudeAgents
.\install.ps1 -ProjectFile D:\UEProjects\Template\Template.uproject -EngineRoot E:\UE_5.6
.\install.ps1 -RunDiagnostics
.\install.ps1 -InstallUePluginToEngine -EngineRoot E:\UE_5.6
```

`-EngineRoot` accepts either `E:\UE_5.6` or `E:\UE_5.6\Engine`. The project agent profile stores the UE root form expected by BlueprintHelper lifecycle tools.

### Codex Desktop

The repository includes a local marketplace:

```text
.agents/plugins/marketplace.json
```

The installer also creates or updates the user-level Codex marketplace:

```text
%USERPROFILE%\.agents\plugins\marketplace.json
```

It points `./plugins/blueprint-helper` to this checkout's `CodexPlugin`. If that path already exists and points somewhere else, use `-Force` only when you intentionally want to replace it.

### Claude Code

Claude Code plugin support is optional:

```powershell
.\install.ps1 -InstallClaudePlugin
```

This validates the local `ClaudePlugin` package and installs the Claude sideAgent definitions. Then start Claude Code from the repository root and run the commands printed by the installer:

```text
/plugin marketplace add ./ClaudePlugin
/plugin install blueprint-helper@blueprint-helper-dev
```

If you only want to copy the sideAgent definitions without validating the Claude plugin source, use:

```powershell
.\install.ps1 -InstallClaudeAgents
```

### Unreal Engine Plugin

The UE-side plugin is the `BlueprintHelper/` folder containing `BlueprintHelper.uplugin`.

Recommended project install:

```text
<YourProject>\Plugins\BlueprintHelper\BlueprintHelper.uplugin
```

Engine install is also supported:

```powershell
.\install.ps1 -InstallUePluginToEngine -EngineRoot E:\UE_5.6\Engine
```

That copies the plugin to:

```text
<Engine>\Plugins\Marketplace\BlueprintHelper
```

The Agent workflow is not tied to whether the UE plugin is project-installed or engine-installed. Codex and the CLI use the Unreal Editor Bridge exposed by the loaded UE plugin. Keep this source checkout available for `AgentFaceService`, `CodexPlugin`, and the global lifecycle MCP script, or set `BLUEPRINTHELPER_ROOT` if you move the runtime.

### Retired Claude Setup Command

The old Claude `/blueprint-helper:setup` flow has been folded into the root installer:

```powershell
.\install.ps1 -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

After installation, use `/blueprint-helper:configure` or the Codex `blueprint-helper-configure` skill only when you want to change safety profile, save policy, or missing-capability policy.

### PowerShell CLI Notes

If an older install still resolves `bh` to `bh.ps1`, rerun `install.ps1` or call `bh.cmd`. For JSON payloads, prefer `--file` or pipe generated JSON to `--stdin`; inline `--json $json` can lose quotes in PowerShell before Node receives it.

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```
