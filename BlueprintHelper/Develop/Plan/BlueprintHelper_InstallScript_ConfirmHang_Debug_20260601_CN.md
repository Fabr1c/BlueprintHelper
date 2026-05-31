# BlueprintHelper Install 脚本确认后卡住 Debug 记录

日期：2026-06-01

## 问题

用户反馈：当前 `install.cmd` 在安装确认阶段可以进入确认页，但确认后脚本卡住，不继续执行后续安装步骤。

## 根因定位

确认阶段由 `InstallScripts/install-prompts.mjs` 的 raw terminal UI 负责。raw UI 在启动时调用：

- `enableRawMode()`
- `process.stdin.setRawMode(true)`
- `process.stdin.resume()`

确认成功后脚本会写出 selection JSON，并调用 `restoreTerminal()`。原实现只关闭 raw mode：

- `process.stdin.setRawMode(false)`

但没有调用 `process.stdin.pause()`。在 Node.js 中，已经 `resume()` 的 stdin 会继续作为活动 handle 持有事件循环，因此 Node 进程不会自然退出，外层 `install.cmd` 会一直等待 `node InstallScripts/install-prompts.mjs ...` 返回，看起来就是“确认后卡住”。

## 修复

在 `restoreTerminal()` 中关闭 raw mode 后显式调用：

```js
process.stdin.pause();
```

该修复只影响最终退出清理路径。`restoreTerminalForPrompt()` 仍只负责在文本输入前临时关闭 raw mode，不会暂停 stdin，因此不会破坏 `.uproject` / UE 路径等文本输入流程。

## 验证结果

1. 已通过 `node --check InstallScripts/install-prompts.mjs`。
2. 已通过参数模式烟测：`cmd /c install.cmd -SkipBuild -SkipCliLink -SkipCodexMarketplace -SkipCodexAgents -SkipLifecycleMcp -SkipProjectProfile -SkipDefaultPreferences`，脚本能正常进入并完成 PowerShell 安装层。
3. 已通过 selection 生成烟测：先用 `InstallScripts/install.ps1 -WriteNodeDefaults` 写出默认配置，再用 `node InstallScripts/install-prompts.mjs --accept-defaults` 写出 selection JSON。
4. 当前自动化 shell 不是双击启动的真实 raw TTY，因此最终确认阶段的“确认后不挂起”仍需要在用户本机双击 `install.cmd` 再验证。
