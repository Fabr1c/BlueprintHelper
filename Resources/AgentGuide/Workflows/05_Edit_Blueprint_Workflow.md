# 05 - 修改蓝图工作流

## 1. 目标

以 TaskSpec-first 的方式修改蓝图资产：Agent -> MCP Task Tools -> Python/MCP Task Compiler -> UE Task Runtime -> Existing Capability Clusters。Agent 只提交结构化 TaskSpec，不提交 TaskPlan；Python / MCP 编译 TaskPlan，UE Task Runtime 调用现有能力簇并负责事务、Review、rollback、compile / diagnostics / save。

## 2. 写入前检查

必须确认：

- `asset_path`
- 图表写入的 `graph_name` 或 TaskSpec 中等价目标
- 修改范围：是否允许创建资产、修改用户节点、接入已有执行流、编辑输入映射
- 当前结构：通过 `blueprinthelper_read_context` 获取 ReadContextPack，必要时补充 logic_md / logic_json；高风险修改前使用 `blueprinthelper_read_reference_context`
- 验证方式：TaskSpec `validation.should_compile`、`validation.should_save`

标准流程：

```text
get_runtime_profile
 -> read_context / read_reference_context as needed
 -> Agent 生成 TaskSpec
 -> preview_task
 -> 修正 TaskSpec 或 stop_and_report
 -> execute_task
 -> get_task_result if needed
 -> report task summary
```

## 3. 添加变量

流程：

```text
read_context
 -> TaskSpec.variables[] 描述 name/type/default/category/tooltip
 -> preview_task 检查 name collision / 类型 / scope_policy
 -> execute_task
 -> Task Runtime compile / diagnostics / save according to `validation.should_compile` / `validation.should_save`
```

注意：

- 不要猜复杂类型。软引用、类引用、结构体、枚举应先确认路径或类型名。
- 已存在同名变量时，不要重复添加；通过 TaskSpec 的 policy 表达 reuse / fail / ask_user，不要在 execute 后再猜。

## 4. 添加函数图 / 宏图

流程：

```text
read_context
 -> TaskSpec 声明函数 / Custom Event / graph 目标和签名
 -> preview_task 检查已有图表、签名能力和外部依赖
 -> execute_task
```

注意：Function / Event Signature Management 是未来 UE Capability Cluster。它只处理声明和签名层，由 TaskPlan step 调用；函数体、事件体和执行流仍由 Graph Write / Replace / Patch / Merge 能力簇处理。

## 5. 添加节点与连线

流程：

```text
read_context
 -> 必要时读取 target graph logic_json/raw_json
 -> TaskSpec.behavior / integration 描述入口、逻辑、资源引用和接入策略
 -> preview_task 生成 TaskPlan 并 dry_run
 -> execute_task 由 UE Task Runtime 调 Graph Write capability
 -> compile / diagnostics / save according to `validation.should_compile` / `validation.should_save`
```

规则：

- 位置坐标不是 Agent 必填重点；若插件支持自动布局，应交给插件按规则布局。
- 节点插入或执行流接入应在 TaskSpec 中基于明确锚点或明确策略，而不是“看起来在图上某位置”。
- 执行线和数据线要分开处理。
- 如果 links 中缺少 Pin 类型，优先读取 raw_json schema 增强字段；没有时再用 Pin 名启发式，且报告不确定性。

## 6. 删除节点 / 变量 / 图表

删除前必须：

1. 读取引用关系或目标图表。
2. 在 TaskSpec 中列出将删除的对象和允许修改范围。
3. 通过 preview_task / dry_run 确认依赖、external_dependents、rollback 能力。
4. preview blocked 时停止，不得继续 execute。
5. execute 后由 Task Runtime 执行编译、诊断、rollback 或报告恢复步骤。

## 7. JSON 导入

只有使用兼容 JsonToBlueprint 的 raw JSON 时才导入。

`blueprint_import_json_to_graph` 现在同时接受结构化 RawJson **对象**和**字符串**形式。建议直接传入对象以简化调用。

普通主线应优先通过 TaskSpec 表达目标。直接 JSON 导入属于 debug / expert 路径，流程：

```text
validate json
 -> dry-run/diagnose if available
 -> import into explicit asset/graph
 -> compile
 -> export logic view
 -> compare expected logic
 -> save
```

不要导入：

- `logic_md`
- `logic_json`
- 由 LLM 随意生成且未校验 schema 的 JSON
- `importable=false` 的 JSON（MCP 层和 C++ 层双重守卫拒绝）
- `schema` 以 `BlueprintHelper.Logic` 开头的 JSON

## 8. 写后报告模板

```text
任务：<feature_name or task_type>
task_run_id：<if returned and user needs it>
目标资产：<asset_path>
状态：completed / preview_blocked / failed
变更摘要：
- ...
验证：
- 编译：通过/失败
- 保存：已保存/未保存
风险或未完成：...
```

普通用户报告默认不展开 TaskPlan steps、child transaction、ToolResultBase 原始 JSON 或底层 Bridge Error。调试、失败定位、rollback、审计场景除外。
