# 03 - 工具选择规则

## 1. 不要先猜工具名

Agent 应先按任务类型选择工具类别，再从 MCP `tools/list` 中选择具体工具。具体工具名可能随版本变化，工具描述是最终依据。

## 2. 用户意图映射表

| 用户意图 | 应选能力 | 不应选能力 |
|---|---|---|
| “查看这个蓝图做什么” | 资产搜索 + 逻辑导出 | 直接 raw JSON 全量读取 |
| “给蓝图加一个变量” | 蓝图变量写操作 + 编译保存 | C++ 文件编辑 |
| “给 EventGraph 加节点” | 蓝图图表节点写操作 | 当前焦点图表写入 |
| “把 Widget 里按钮文案改掉” | UMG WidgetTree / 属性写入 | 蓝图节点导入 |
| “改 DataTable 这一行” | DataTable 读写 | 资产源码文件编辑 |
| “写一个 C++ 类” | 普通代码工具 | BlueprintHelper MCP |
| “编译整个项目” | MCP Server build_project 或普通构建命令 | 蓝图编译工具 |
| “打开 Unreal Editor” | open_editor | 资产浏览工具 |
| “运行 PIE” | 编辑器 PIE 命令 | 项目编译工具 |

## 3. 读格式选择

```text
Need understand?        -> logic_md
Need structured summary? -> logic_json
Need exact nodes/pins?   -> raw_json
Need import/replay?      -> raw_json-compatible JsonToBlueprint protocol
```

## 4. 写格式选择

优先使用原子工具：添加变量、添加节点、设置属性、更新 DataTable 行。

只有在以下情况使用 JSON 导入：

- 用户明确要求导入一段蓝图 JSON。
- 需要批量创建结构，原子工具成本过高。
- 已有可兼容的 JsonToBlueprint raw JSON。

不要用 `logic_json` 或 `logic_md` 直接导入。

## 5. 当前焦点上下文规则

允许使用当前激活图表 / 当前打开资产的情况：

- 用户明确说“当前打开的蓝图”或“当前图表”。
- 操作是只读查询。
- 操作失败不会破坏资产。

不允许依赖当前焦点的情况：

- 删除节点 / 变量 / 控件。
- 重命名。
- 批量写入。
- 导入 JSON。
- 保存资产。

## 6. 错误处理规则

- Bridge 不可用：报告编辑器或 Bridge 状态问题，提示需要启动编辑器或检查端口。
- 资产找不到：先搜索相近资产名，不要凭空创建同名资产，除非用户要求创建。
- 图表找不到：列出图表后让计划回退到正确图表；不要自动新建同名图表，除非用户要求。
- 编译失败：停止继续写入，返回错误摘要和可能原因。
- 参数 schema 校验失败：修正参数，不要改变用户目标。
