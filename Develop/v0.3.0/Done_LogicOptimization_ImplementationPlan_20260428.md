# BlueprintHelper 逻辑视图优化实施计划（2026-04-28）

## 一、修改原因

当前 BlueprintHelper 已具备 Service 层、Bridge 层和多类蓝图资产操作能力，但 Agent 读取蓝图逻辑时仍主要依赖 raw JSON。raw JSON 适合回放，不适合快速理解。

因此需要在不破坏已有导入/导出能力的前提下，新增逻辑摘要能力，让 MCP / Agent 可以获取更短、更稳定、更语义化的蓝图逻辑视图。

---

## 二、实施原则

### 2.1 不破坏现有功能

- 不删除 `export_to_json`。
- 不改变 `import_json` 必需字段。
- 不把 `logic_json` 当作可导入格式。
- 不要求所有 MCP 工具一次性迁移。

### 2.2 优先独立实现

LogicProcessor 第一阶段只依赖 raw JSON 文本，不依赖 UE 对象模型。这样可以：

- 降低编译耦合。
- 方便对 fixture 做离线测试。
- 避免影响现有导入生成器。

### 2.3 先有可用摘要，再追求完全准确

第一阶段允许通过 Pin 名启发式判断执行线，后续通过 raw JSON schema 增强补足准确性。

---

## 三、任务总表

| 阶段 | 任务 | 类型 | 说明 |
|------|------|------|------|
| T1 | LogicProcessor 类型与接口 | 新增 | Public / Private Services |
| T2 | raw JSON 解析 | 新增 | 支持单图和 full blueprint |
| T3 | 节点语义分类 | 新增 | event/call/set/get/branch/loop 等 |
| T4 | link 分类 | 新增 | exec/data，支持 kind 和启发式 |
| T5 | logic_json writer | 新增 | 结构化摘要 |
| T6 | logic_md writer | 新增 | Markdown 摘要 |
| T7 | Bridge `export_logic` | 新增 | Router 集成 |
| T8 | raw JSON link 字段增强 | 修改 | 可选字段，保持兼容 |
| T9 | MCP Server 工具映射 | 修改 | 增加逻辑读取工具 |
| T10 | fixture 和回归测试 | 新增 | 验证输出稳定性 |

---

## 四、T1：LogicProcessor 类型与接口

新增文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicProcessor.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicProcessor.cpp
```

头文件包含：

```cpp
enum class EBlueprintHelperLogicOutputFormat : uint8;
enum class EBlueprintHelperLogicDetailLevel : uint8;
struct FBlueprintHelperLogicOptions;
struct FBlueprintHelperLogicResult;
class FBlueprintHelperLogicProcessor;
```

通过标准：

- 插件可编译。
- 空 JSON 返回明确错误。
- 非法 JSON 返回 `bSuccess=false` 和错误文本。

---

## 五、T2：raw JSON 解析

支持结构：

### 单图结构

```json
{
  "version": "2.x",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "nodes": [],
  "links": []
}
```

### 完整蓝图结构

```json
{
  "version": "2.x",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "graphs": [
    {
      "name": "EventGraph",
      "nodes": [],
      "links": []
    }
  ]
}
```

兼容字段：

- graph name 可读 `name`、`graph` 或 `graph_name`。
- link 可读 `from_id/from_pin/to_id/to_pin`。
- link 可选读 `source.node/source.pin` 和 `target.node/target.pin`。

通过标准：

- 能解析现有 fixture。
- 能从 full blueprint 中列出每个 graph。

---

## 六、T3：节点语义分类

实现函数：

```cpp
static FString ClassifyNodeKind(const TSharedPtr<FJsonObject>& NodeObject);
static FString ResolveNodeLabel(const TSharedPtr<FJsonObject>& NodeObject);
```

分类优先级：

1. 先按 `type` 精确匹配。
2. 再按 macro/function 字段补充判断。
3. 未识别则输出 `unknown`，保留 raw type。

通过标准：

- Branch 输出 `branch`。
- VariableGet 输出 `get`。
- VariableSet 输出 `set`。
- CallFunction 输出 `call`。
- EnhancedInputAction 输出 `event`。

---

## 七、T4：link 分类

实现策略：

1. link 有 `kind` 时直接使用。
2. 无 `kind` 时根据 `from_pin_type` / `to_pin_type` 判断。
3. 再无类型字段时使用 Pin 名启发式。
4. 仍无法判断则标为 `unknown`。

通过标准：

- `then -> execute` 识别为 exec。
- `Health -> NewHealth` 识别为 data。
- 不确定的 link 不应被静默丢弃。

---

## 八、T5：logic_json writer

输出结构：

```json
{
  "version": "1.0",
  "schema": "BlueprintHelper.LogicGraph",
  "source_schema": "BlueprintHelper.JsonToBlueprint",
  "stats": {},
  "graphs": []
}
```

最小字段：

- `graphs[].name`
- `graphs[].entry_points[]`
- `graphs[].data_dependencies[]`
- `graphs[].orphans[]`
- `stats`

通过标准：

- 输出是合法 JSON。
- 所有节点数量统计正确。
- 不因孤立节点失败。

---

## 九、T6：logic_md writer

输出结构：

```md
# <GraphName>

## Entry: <EventName>

1. Call `<FunctionName>`
2. Set `<Variable>` = <Value>

## Data Dependencies

- `<Source>` -> `<Target>`

## Orphans

- `<NodeLabel>`
```

细节级别：

| detail | 行为 |
|--------|------|
| `brief` | 只输出入口流和关键调用 |
| `normal` | 输出变量读写、数据依赖、孤立节点 |
| `debug` | 输出 node id、raw type、link 推断来源 |

通过标准：

- Markdown 不包含大段 raw JSON。
- 入口流可读。
- debug 模式能追踪节点 ID。

---

## 十、T7：Bridge `export_logic`

修改文件：

```text
Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
```

新增方法：

```cpp
FBlueprintHelperBridgeResponse HandleExportLogic(const FBlueprintHelperBridgeRequest& Req) const;
```

处理流程：

```text
解析 payload
    ↓
构造 FBlueprintHelperExportRequest
    ↓
ExportService.Export
    ↓
FBlueprintHelperLogicProcessor::ProcessRawJson
    ↓
构造 Bridge response
```

通过标准：

- `format=logic_json` 返回结构对象。
- `format=logic_md` 返回 Markdown 字符串。
- 目标蓝图或图表缺失时返回明确错误。

---

## 十一、T8：raw JSON link 字段增强

修改位置：

```text
Source/BlueprintHelper/Private/BlueprintTextConverter.cpp
```

新增 link 字段：

```text
kind
from_pin_type
to_pin_type
from_direction
to_direction
```

通过标准：

- 新字段存在。
- 删除新字段后旧导入仍可工作。
- LogicProcessor 优先使用新字段。

---

## 十二、T9：MCP Server 工具映射

MCP Server 层建议新增：

```text
blueprint_get_logic
blueprint_get_logic_json
blueprint_export_raw_json
```

工具描述必须明确：

- 读逻辑用 logic。
- 写回蓝图用 raw JSON。
- 写操作必须显式指定目标资产和图表。

---

## 十三、T10：测试与回归

新增 fixture 目录：

```text
Resources/TestFixtures/LogicProcessor/
```

建议 fixture：

```text
simple_beginplay_call.raw.json
simple_beginplay_call.logic.json
branch_flow.raw.json
branch_flow.logic.json
forloop_flow.raw.json
forloop_flow.logic.json
delegate_flow.raw.json
delegate_flow.logic.json
multi_graph.raw.json
multi_graph.logic.json
```

通过标准：

- raw -> logic 输出稳定。
- 修改坐标不影响 logic 输出。
- 删除 `kind` 后仍能通过启发式生成基本逻辑。

---

## 十四、推荐实施顺序

1. T1 + T2：先实现可解析 raw JSON。
2. T3 + T4：实现节点与 link 分类。
3. T5：实现 logic JSON。
4. T6：实现 Markdown。
5. T7：Bridge 暴露 `export_logic`。
6. T10：补 fixture。
7. T8：增强 raw JSON link 字段。
8. T9：最后接 MCP Server 工具映射。

---

## 十五、总体验收

最终完成时应满足：

1. 原 `export_to_json` 和 `import_json` 不回归。
2. `export_logic` 能对当前图表输出短摘要。
3. `logic_json` 可被 Agent 用于结构化推理。
4. `logic_md` 可直接放入聊天上下文。
5. 大型蓝图读取时，默认不再返回完整 raw JSON。

