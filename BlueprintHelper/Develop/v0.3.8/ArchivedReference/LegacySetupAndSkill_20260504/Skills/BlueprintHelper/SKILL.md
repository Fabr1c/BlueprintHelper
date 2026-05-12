# BlueprintHelper Skill — TaskSpec-first

BlueprintHelper 是 UE5.3+ 的 Agent 编辑辅助系统。

默认流程：

```text
get_runtime_profile → read_task_context → build TaskSpec → preview_task → execute_task → report summary
```

不要把复杂 UE 资产任务拆成大量底层 MCP 调用。底层工具簇是 TaskPlan capability、debug / expert 工具和测试入口。

规则：

- runtime_profile.active_profile 是 safety_profile 唯一来源。
- diagnostics 只定位问题，不替代 runtime_profile。
- LogicMD 用于理解，LogicJson 用于结构化分析，RawJson/resource_ref 用于保真、导入或 Pin/GUID 级调试。
- Asset Factory 只创建资产。
- add_component 只创建组件和 attachment。
- Class Settings 不写图表逻辑，不支持第一版 reparent。
- Enhanced Input 默认不编辑 IA / IMC。
- Append/Replace/Patch/Merge 是 Graph Write capability，不是普通 Agent 默认直调入口。
- preview_blocked、missing capability、rollback blocked/failed 时 stop_and_report。
