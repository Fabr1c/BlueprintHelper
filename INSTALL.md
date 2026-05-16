# BlueprintHelper Install

Run the root installer from this repository root:

```powershell
.\install.ps1
```

Windows users can also run:

```cmd
install.cmd
```

The default install does four things:

- builds `AgentFaceService/task-core`, `AgentFaceService/cli`, and `AgentFaceService/mcp`;
- links the CLI globally, so `bh` and `blueprinthelper-cli` are available;
- registers the Codex desktop local marketplace entry for `blueprint-helper`;
- installs Codex subagents and the lifecycle-only MCP config.
- writes `<ProjectDir>/.blueprinthelper/agent-profile.json` when a unique `.uproject` and UE root are available;
- creates default Claude/Codex user preference files only when they are missing.

Useful options:

```powershell
.\install.ps1 -SkipCliLink
.\install.ps1 -SkipBuild
.\install.ps1 -InstallClaudeAgents
.\install.ps1 -ProjectFile D:\UEProjects\Template\Template.uproject -EngineRoot E:\UE_5.6
.\install.ps1 -RunDiagnostics
.\install.ps1 -InstallUePluginToEngine -EngineRoot E:\UE_5.6
```

`-EngineRoot` accepts either `E:\UE_5.6` or `E:\UE_5.6\Engine`. The project agent profile stores the UE root form expected by BlueprintHelper lifecycle tools.

## Codex Desktop

The repository includes a repo-local marketplace at:

```text
.agents/plugins/marketplace.json
```

The installer also creates or updates the home-local Codex marketplace at:

```text
%USERPROFILE%\.agents\plugins\marketplace.json
```

It points `./plugins/blueprint-helper` to this checkout's `CodexPlugin` folder. If that path already exists and points somewhere else, rerun with `-Force` only when you intentionally want to replace it.

## Unreal Engine Plugin

The UE-side plugin is the `BlueprintHelper/` folder containing:

```text
BlueprintHelper/BlueprintHelper.uplugin
```

Recommended project install:

```text
<YourProject>\Plugins\BlueprintHelper\BlueprintHelper.uplugin
```

Engine install is also supported:

```powershell
.\install.ps1 -InstallUePluginToEngine -EngineRoot E:\UE_5.6\Engine
```

That copies the UE plugin to:

```text
<Engine>\Plugins\Marketplace\BlueprintHelper
```

The Agent workflow is not tied to whether the UE plugin is project-installed or engine-installed. Codex and the CLI use the Unreal Editor Bridge exposed by the loaded UE plugin. Keep this source checkout available for `AgentFaceService`, `CodexPlugin`, and the global lifecycle MCP script, or set `BLUEPRINTHELPER_ROOT` to this repository root if you move the runtime.

## Retired Claude Setup Command

The old Claude `/blueprint-helper:setup` flow has been folded into this installer. Use:

```powershell
.\install.ps1 -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

Then use `/blueprint-helper:configure` or the Codex `blueprint-helper-configure` skill only when you want to change safety or workflow preferences after installation.
