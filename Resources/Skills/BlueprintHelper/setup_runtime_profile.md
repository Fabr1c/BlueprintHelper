# Setup / runtime_profile Policy

SetupProfile 由用户通过 Setup Wizard / 插件 UI / Settings 生成。Agent 只能消费，不能临时覆盖。

禁止在工具参数中传入：

```text
safety_profile
temporary_profile
per_call_profile
one-shot Expert
force_write
no_review
no_journal
permission_override
```

runtime_profile.tool_capabilities 使用 unavailable_only。未出现在 unavailable 中不等于 schema 已确认。
