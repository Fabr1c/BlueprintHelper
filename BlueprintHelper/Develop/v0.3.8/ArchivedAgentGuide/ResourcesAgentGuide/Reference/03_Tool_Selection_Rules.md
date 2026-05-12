# 03 - Tool Selection Rules

## 1. Do Not Start From Tool Names

先判断用户意图能否表达为 TaskSpec。普通资产写入默认使用任务级工具，不拆成底层 MCP 调用。

默认主线:

```text
profile
-> context
-> TaskSpec
-> preview
-> request_write_session if write_permission is disabled
-> execute
-> result
```

If preview passes but write permission is disabled, call `blueprinthelper_request_write_session`. The user only sees a simple accept/reject Editor prompt. Rejection means stop and report. The running Editor/Bridge owns the approved scope and lifetime, so delegated SideAgents do not need raw session data. Do not use env-token fallback.

## 2. Intent Mapping

| User intent | Use |
|---|---|
| 查看 Blueprint 做什么 | `blueprinthelper_read_context` |
| 给 Blueprint 加变量 | TaskSpec `edit_blueprint_variables` |
| 给图表增加逻辑 | TaskSpec `edit_blueprint_graph` |
| 改 Widget 文案或属性 | TaskSpec `edit_umg_widget` |
| 改 DataTable 行 | TaskSpec `edit_data_table` |
| 改 DataAsset 或 UObject 属性 | TaskSpec `edit_object_properties` |
| 创建测试 fixture 资产 | TaskSpec `create_asset` |
| 组合组件、变量、接口和逻辑 | TaskSpec `create_blueprint_feature` |
| 写 C++ 或配置文件 | 普通仓库工具 |

## 3. Read Format Choice

```text
Need human summary -> read_context view.format=logic_md
Need structured anchors -> read_context view.format=logic_json
Need schema only -> read_context view.format=schema
Need reference impact -> read_reference_context
```

## 4. Write Rules

- Agent 提交 TaskSpec，不提交 TaskPlan。
- TaskSpec 描述目标资产、范围、资源引用、行为和验证策略。
- Preview 是写入门禁。blocked 时不 execute。
- 底层 capability 由 Task Runtime 调用，不由普通 Agent 直接选择。

## 5. Current Focus Context

只读问题可以利用当前上下文。写入必须显式给出目标资产、目标图表或等价 TaskSpec 目标。不要依赖当前打开的标签页执行写入。

## 6. Error Handling

- Bridge 不可用: 报告 Editor/Bridge 状态问题。
- TaskSpec schema 或 semantic 错误: 按 issues path 和 suggested patch 修正。
- preview blocked: 停止报告 blocker 或调整 TaskSpec。
- context_required: 重新读取上下文后再生成 TaskSpec。
- capability missing: 报告能力缺口，不改用冻结入口。
