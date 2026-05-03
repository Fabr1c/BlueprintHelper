# 05 - 修改蓝图工作流

## 1. 目标

以最小、可验证、可回退的方式修改蓝图资产。

## 2. 写入前检查

必须确认：

- `asset_path`
- `graph_name`
- 操作对象：变量 / 函数 / 宏 / 节点 / 连线 / 事件分发器
- 当前结构：至少读取图表列表、变量列表或目标图表逻辑摘要
- 验证方式：编译、读取回查、保存

## 3. 添加变量

流程：

```text
read variables
 -> check name collision
 -> add variable with explicit type/default/category/tooltip if available
 -> compile blueprint
 -> save asset
```

注意：

- 不要猜复杂类型。软引用、类引用、结构体、枚举应先确认路径或类型名。
- 已存在同名变量时，不要重复添加；应询问是否复用或重命名，若任务不能中断则使用最小安全方案并报告。

## 4. 添加函数图 / 宏图

流程：

```text
list graphs
 -> check graph name collision
 -> create function/macro graph
 -> add required inputs/outputs if tool支持
 -> compile
 -> save
```

注意：函数图和宏图不是普通 EventGraph 节点，创建前要确认用户意图。

## 5. 添加节点与连线

流程：

```text
read target graph logic_json/raw_json
 -> identify stable anchor node
 -> create node(s)
 -> connect exec pins
 -> connect data pins
 -> compile
 -> read back target graph
 -> save
```

规则：

- 位置坐标不是 Agent 必填重点；若插件支持自动布局，应交给插件按规则布局。
- 节点插入应基于明确锚点，而不是“看起来在图上某位置”。
- 执行线和数据线要分开处理。
- 如果 links 中缺少 Pin 类型，优先读取 raw_json schema 增强字段；没有时再用 Pin 名启发式，且报告不确定性。

## 6. 删除节点 / 变量 / 图表

删除前必须：

1. 读取引用关系或目标图表。
2. 列出将删除的对象。
3. 尽量确认是否存在依赖。
4. 删除后编译。
5. 若编译失败，优先 Undo 或报告恢复步骤。

## 7. JSON 导入

只有使用兼容 JsonToBlueprint 的 raw JSON 时才导入。

`blueprint_import_json_to_graph` 现在同时接受结构化 RawJson **对象**和**字符串**形式。建议直接传入对象以简化调用。

流程：

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
已修改：<asset_path>
图表：<graph_name>
变更：
- ...
验证：
- 编译：通过/失败
- 保存：已保存/未保存
风险或未完成：...
```
