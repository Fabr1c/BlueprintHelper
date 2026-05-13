# 03 - Runtime Profile And Diagnostics

`blueprint_get_runtime_profile` 是任务前事实来源。重点读取:

```text
bridge_status
config_status
write_permission
risk_command
active_profile.safety_profile
active_profile.missing_capability_policy
tool_capabilities.mode
```

If `write_permission.disabled` is true and the reason is `write_session_missing`, finish the TaskSpec preview first, then call `blueprinthelper_request_write_session` before execute. The approval UI is a minimal Editor accept/reject dialog. A rejected or failed request is a stop-and-report condition, not a reason to fall back to token setup. Once approved, the running Editor/Bridge holds the scoped permission for Main Agent and SideAgent tool execution.

`blueprinthelper_diagnostics` 用于静态安装和配置检查。`blueprinthelper_diagnostics_runtime` 用于 Editor/Bridge 可达时的运行时链路检查。

诊断结果中的 blocking 项表示环境需要处理，不等于诊断工具自身失败。只有工具返回 failed 才表示诊断调用失败。

普通 Agent 不展开底层工具清单。需要写入时继续走 TaskSpec preview 和 execute。
