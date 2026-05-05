# 03 - runtime_profile 与 diagnostics

`runtime_profile` 是任务前事实来源。Agent 从中读取：

```text
bridge_status
config_status
write_permission
risk_command
active_profile.safety_profile
active_profile.missing_capability_policy
tool_capabilities.mode=unavailable_only
```

`diagnostics` 只用于定位问题，返回报告在 `data.markdown`。

规则：

```text
Markdown 中的 Blocking 不等于 diagnostics 工具失败。
ok=true/status=completed 表示诊断命令执行成功。
ok=false/status=failed 才表示诊断工具自身失败。
```

普通工具成功结果不默认返回 safety / transaction / review。
