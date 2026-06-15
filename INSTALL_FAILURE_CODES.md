# BlueprintHelper Install Failure Codes / 安装失败码

This document explains the stable failure codes printed by `install.cmd`, `update.cmd`, and `uninstall.cmd`.

本文说明 `install.cmd`、`update.cmd`、`uninstall.cmd` 输出的稳定失败码。

## How To Read The Output / 如何阅读输出

Failure output has three important fields:

- `Failure code`: stable code used for searching this document.
- `Failure stage`: the install/update/uninstall step that was running.
- `Failure log`: optional log file for nested commands, currently used by update post-install refresh.

失败输出包含三个关键字段：

- `Failure code`：稳定失败码，可用于搜索本文档。
- `Failure stage`：失败时正在运行的安装、更新或卸载阶段。
- `Failure log`：嵌套命令日志路径，目前用于更新后的安装刷新阶段。

## Codes / 失败码

| Code | Meaning | Typical action |
| --- | --- | --- |
| `BH-INSTALL-UNHANDLED` | `install.ps1` failed outside a known command wrapper. | Read the message under the code, then rerun `install.cmd` from a terminal if the window closed too quickly. |
| `BH-INSTALL-EXTERNAL-COMMAND-FAILED` | `install.ps1` started an external command such as `npm`, `node`, `robocopy`, or UBT and that command failed. | Check `Failure stage`; it names the exact command group. Fix the dependency or path reported by the command. |
| `BH-UNINSTALL-UNHANDLED` | `uninstall.ps1` failed outside a known command wrapper. | Read the message under the code and check whether a target path is locked or missing. |
| `BH-UNINSTALL-EXTERNAL-COMMAND-FAILED` | `uninstall.ps1` started an external command and that command failed. | Check `Failure stage`; some uninstall external failures are warnings, but hard failures still print this code. |
| `BH-UPD-UNHANDLED` | `update.ps1` failed outside a more specific update stage. | Read the message under the code. If the failure happened before replacement, no rollback is usually needed. |
| `BH-UPD-BOOTSTRAP-FAILED` | `update.ps1` downloaded and validated a release package, but could not start the updater copied from that package. | Check whether PowerShell can start child processes and whether the downloaded package contains `InstallScripts/update.ps1`. |
| `BH-UPD-RUNNER-FAILED` | The updater copied from the downloaded package started but returned a failure. | Read the runner output first. It usually contains a more specific update failure code and may include a `Failure log`. |
| `BH-UPD-POSTINSTALL-FAILED` | The release zip was downloaded, validated, backed up, and mirrored into place, but the post-update `install.ps1` refresh returned non-zero. | Open the `Post-update install refresh log` / `Failure log`. The updater attempts rollback when this happens before version verification. |

| 失败码 | 含义 | 常见处理 |
| --- | --- | --- |
| `BH-INSTALL-UNHANDLED` | `install.ps1` 在已知命令包装之外失败。 | 阅读失败码下方的错误消息；如果窗口太快关闭，从终端重新运行 `install.cmd`。 |
| `BH-INSTALL-EXTERNAL-COMMAND-FAILED` | `install.ps1` 启动的外部命令失败，例如 `npm`、`node`、`robocopy` 或 UBT。 | 查看 `Failure stage`，它会指出具体命令组；根据该命令输出修复依赖或路径。 |
| `BH-UNINSTALL-UNHANDLED` | `uninstall.ps1` 在已知命令包装之外失败。 | 阅读失败码下方的错误消息，检查目标路径是否被占用或不存在。 |
| `BH-UNINSTALL-EXTERNAL-COMMAND-FAILED` | `uninstall.ps1` 启动的外部命令失败。 | 查看 `Failure stage`；部分卸载外部命令失败只会警告，硬失败仍会输出该失败码。 |
| `BH-UPD-UNHANDLED` | `update.ps1` 在更具体的更新阶段之外失败。 | 阅读失败码下方的错误消息；如果失败发生在替换前，通常不需要回滚。 |
| `BH-UPD-BOOTSTRAP-FAILED` | `update.ps1` 已下载并校验 release 包，但无法启动从新包复制出的 updater。 | 检查 PowerShell 是否能启动子进程，以及下载包是否包含 `InstallScripts/update.ps1`。 |
| `BH-UPD-RUNNER-FAILED` | 从下载包复制出的 updater 已启动，但返回失败。 | 优先阅读 runner 输出；它通常会包含更具体的更新失败码，也可能包含 `Failure log`。 |
| `BH-UPD-POSTINSTALL-FAILED` | Release zip 已下载、校验、备份并镜像到当前目录，但更新后的 `install.ps1` 刷新返回非零。 | 打开 `Post-update install refresh log` / `Failure log`。该阶段在版本验证前失败时，更新器会尝试回滚。 |

## Notes / 说明

Numeric exit codes remain compatible with existing scripts:

- `0`: success.
- `1`: failure.
- `2`: `update.cmd -CheckOnly` found an available update.

数字退出码保持兼容：

- `0`：成功。
- `1`：失败。
- `2`：`update.cmd -CheckOnly` 发现可用更新。
