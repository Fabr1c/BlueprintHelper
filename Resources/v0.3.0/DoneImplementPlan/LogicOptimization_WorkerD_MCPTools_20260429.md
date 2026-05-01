# Worker D MCP Tools Execution Plan

## 目标

在 MCPServer 层新增 Agent 友好的逻辑读取工具，调用 Bridge 的 `export_logic`。本执行线不修改 UE C++。

## 依赖

依赖 Worker B 的 Bridge 命令契约：

```text
export_logic
```

## 文件边界

修改：

```text
MCPServer/src/tools.ts
Resources/AGENT.md
Resources/JsonToBlueprintRules.md
```

新增：

```text
无
```

移除：

```text
无
```

越界规则：

- 不修改 `MCPServer/src/bridge-client.ts`。
- 不修改 `MCPServer/src/index.ts`。
- 不修改 UE C++。
- 如果需要新增 MCP resource，先提交请求变更文档。

## 工具设计

新增两个工具：

```text
blueprint_get_logic
blueprint_get_logic_json
```

保留旧工具：

```text
blueprint_export_to_json
```

不新增 `blueprint_export_raw_json`，避免和现有 `blueprint_export_to_json` 重复。只更新描述，明确它返回可导入 raw JSON。

## 输入参数

`blueprint_get_logic`：

```text
target_blueprint optional string
target_graph optional string
scope optional enum single_graph | full_blueprint
detail optional enum brief | normal | debug
include_data_dependencies optional boolean
include_orphans optional boolean
```

默认：

```text
format=logic_md
scope=single_graph
detail=normal
include_data_dependencies=true
include_orphans=true
```

`blueprint_get_logic_json`：

同上，额外支持：

```text
include_node_ids optional boolean
include_positions optional boolean
include_raw_node_types optional boolean
```

默认：

```text
format=logic_json
include_node_ids=false
include_positions=false
include_raw_node_types=false
```

## 实现步骤

- [ ] 在 `tools.ts` 的 `blueprint_export_to_json` 之后新增 `blueprint_get_logic`。
- [ ] 新增 `blueprint_get_logic_json`。
- [ ] 两个工具都调用 `bridge.sendCommand('export_logic', payload)`。
- [ ] `blueprint_get_logic` 成功且存在 `result.markdown` 时直接返回 Markdown 文本。
- [ ] `blueprint_get_logic_json` 使用 `toToolResult` 返回结构化 JSON 字符串。
- [ ] 更新文件顶部工具数量注释。
- [ ] 更新 `Resources/AGENT.md` 的工具数量和用途边界。
- [ ] 更新 `Resources/JsonToBlueprintRules.md` 的导出说明，明确 raw JSON 和 logic 输出差异。

## 最小修改约束

- 不删除 `blueprint_export_to_json`。
- 不改变 `toToolResult` 的通用行为。
- 不让 logic 输出进入 `blueprint_import_json_to_graph`。
- 不新增生命周期工具。

## 验收

在 `MCPServer` 目录执行：

```powershell
npm run build
```

通过标准：

- TypeScript 编译通过。
- `blueprint_get_logic` 描述中明确用于读逻辑。
- `blueprint_export_to_json` 描述中明确用于 raw JSON 写回或回放。
- `Resources/AGENT.md` 的工具数量与新增工具一致。

