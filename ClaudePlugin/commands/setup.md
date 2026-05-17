---
description: Deprecated compatibility entry; use the repository-root install script for BlueprintHelper setup
allowed-tools: Read
---

# BlueprintHelper Setup / 安装入口

## 中文

`/blueprint-helper:setup` 已废弃。首次安装现在由仓库根目录安装脚本负责：

```powershell
.\install.ps1 -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

交互式安装：

```powershell
.\install.ps1 -Interactive
```

或无参数运行：

```cmd
install.cmd
```

需要 Claude Code 插件支持时：

```powershell
.\install.ps1 -InstallClaudePlugin
```

然后从仓库根目录启动 Claude Code，并运行：

```text
/plugin marketplace add ./ClaudePlugin
/plugin install blueprint-helper@blueprint-helper-dev
```

安装后只有在需要修改 safety profile、save policy、missing-capability policy 等偏好时，才使用 `/blueprint-helper:configure`。

不要在此兼容入口中重建旧 setup 流程，不要请求 MCP tool 权限，也不要从这里写 project profile 或 preference 文件。

## English

`/blueprint-helper:setup` is retired. First-run setup is now handled by the repository-root installer:

```powershell
.\install.ps1 -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

Interactive install:

```powershell
.\install.ps1 -Interactive
```

Or run with no arguments:

```cmd
install.cmd
```

For Claude Code plugin support:

```powershell
.\install.ps1 -InstallClaudePlugin
```

Then start Claude Code from the repository root and run:

```text
/plugin marketplace add ./ClaudePlugin
/plugin install blueprint-helper@blueprint-helper-dev
```

After installation, use `/blueprint-helper:configure` only when you need to change safety profile, save policy, missing-capability policy, or similar preferences.

Do not recreate the old setup flow in this compatibility entry, do not request MCP tool permissions here, and do not write project profile or preference files from this entry.
