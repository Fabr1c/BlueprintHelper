# BlueprintHelper 更新入口统一 Debug 记录

日期：2026-06-01

## 问题

根目录同时存在 `update.cmd` 与 `upgrade.cmd`。虽然 `upgrade.cmd` 只是转发到 `update.cmd`，没有第二套更新实现，但对用户和维护者会形成两个更新入口的错觉。

## 决策

统一保留 `update.cmd` 作为唯一根目录更新入口，删除 `upgrade.cmd`。

理由：

1. 用户前置需求均围绕 `update.cmd` 描述，例如双击确认更新、`update.ps1 -Force`、可视化进度条。
2. 实际实现脚本名为 `InstallScripts/update.ps1`，保留 `update.cmd` 与内部实现命名一致。
3. 删除兼容别名可以避免后续文档、安装提示和脚本维护再次分裂。

## 变更范围

1. 删除根目录 `upgrade.cmd`。
2. 根 `README.md` 的更新说明统一到 `update.cmd`。
3. `BlueprintHelper/Develop/v0.5.4/README.md` 的用户入口统一到 `update.cmd`。
4. 当前仍在 `Develop/Plan` 的脚本审计计划移除 `upgrade.cmd` 引用。

## 验证结果

1. 根目录 `.cmd` 文件只剩 `install.cmd`、`update.cmd`、`uninstall.cmd`。
2. 当前非归档用户文档和脚本中不再存在 `upgrade.cmd` 当前入口引用；本 Debug 记录中仅作为已删除对象出现。
3. `cmd /c update.cmd -CheckOnly` 能进入 updater 并显示进度条，随后在访问 GitHub Release 时失败：`基础连接已经关闭: 接收时发生错误。` 该失败属于当前环境网络/TLS 接收问题，不是入口统一造成的脚本路由问题。
4. `git diff --check` 通过。
