---
description: Deprecated compatibility entry; installation is handled only by the repository-root installer
allowed-tools: Read
---

# BlueprintHelper Setup / 安装入口

## 中文

`/blueprint-helper:setup` 已废弃，只保留为兼容提示入口。

首次安装、Claude Code 插件安装、Claude sideAgent 安装、Codex 插件安装、CLI 构建、项目 profile、用户偏好文件和生命周期 MCP 配置，全部由仓库根目录安装脚本负责。

请从 BlueprintHelper 仓库根目录运行安装脚本，并按安装器提示选择需要的组件：

```cmd
install.cmd
```

或阅读仓库根目录的 `INSTALL.md` 后使用 `install.cmd`。

此命令不得重新实现安装流程，不得打印独立的 marketplace/install 命令，不得请求 MCP tool 权限，也不得写入 project profile 或 preference 文件。

安装完成后，只有在需要修改 safety profile、save policy、missing-capability policy 等偏好时，才使用 `/blueprint-helper:configure`。

## English

`/blueprint-helper:setup` is retired and remains only as a compatibility pointer.

First-run setup, Claude Code plugin installation, Claude sideAgent installation, Codex plugin installation, CLI build, project profile creation, user preference files, and lifecycle MCP configuration are all owned by the repository-root installer.

Run the installer from the BlueprintHelper repository root and choose the needed components there:

```cmd
install.cmd
```

Or read the repository-root `INSTALL.md` and use `install.cmd`.

This command must not recreate install logic, print standalone marketplace/install commands, request MCP tool permissions, or write project profile or preference files.

After installation, use `/blueprint-helper:configure` only when changing preferences such as safety profile, save policy, or missing-capability policy.
