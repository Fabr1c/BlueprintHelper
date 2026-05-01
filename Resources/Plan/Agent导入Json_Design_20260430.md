# BlueprintHelper Agent 导入 JSON 优化设计文档（2026-04-30）

## 一、文档目的

本文档用于定义 BlueprintHelper 面向 IDE / CLI Agent 的蓝图导入 JSON 优化方案。

该方案的核心目标不是替换现有 `BlueprintHelper.JsonToBlueprint` raw JSON 协议，而是在其旁边新增一个更适合 Agent 生成的导入意图协议：

```text
BlueprintHelper.AgentImportGraph
```

现有 raw JSON 仍继续承担完整回放、导出、兼容测试和底层调试职责；新增 Agent 导入 JSON 只表达蓝图构图意图，由插件负责解析、补全、校验、创建节点和自动布局。

---

## 二、背景与问题

### 2.1 现有 raw JSON 不适合 Agent 直接生成

当前 `BlueprintHelper.JsonToBlueprint` 更接近“蓝图节点快照”。它通常包含：

- 节点类型，例如 `K2Node_CallFunction`、`K2Node_VariableSet`。
- 节点坐标，例如 `Pos`、`PosX`、`PosY`。
- 节点 GUID、Pin GUID、Graph GUID。
- 完整 Pin 列表。
- Pin 默认值、Pin 类型、Pin 方向。
- Link 的底层 from / to pin 信息。
- 编辑器显示状态和布局相关信息。

这些字段对完整导出和回放有价值，但对 Agent 编写蓝图逻辑存在明显问题：

1. Agent 很容易生成非法 GUID、非法 Pin 或过期节点结构。
2. Agent 会把注意力浪费在坐标、Pin 细节、编辑器显示状态上。
3. 坐标布局与蓝图逻辑无关，却显著增加 token。
4. raw JSON 对节点底层类型要求高，不利于跨 UE 小版本稳定运行。
5. Agent 的真正意图通常只是“创建事件、调用函数、读写变量、连接执行流和数据流”。

### 2.2 导入协议与导出协议职责不同

现有规划已经将读取方向拆分为：

| 格式 | 用途 |
|------|------|
| `raw_json` | 完整可回放导出协议 |
| `logic_json` | Agent 结构化理解协议 |
| `logic_md` | 人类与 Agent 可读摘要 |

导入方向也应进行同样分层：

| 格式 | 用途 | 是否建议 Agent 生成 |
|------|------|----------------------|
| `BlueprintHelper.JsonToBlueprint` | 精确回放、fixture、调试 | 否 |
| `BlueprintHelper.AgentImportGraph` | Agent 表达蓝图修改意图 | 是 |

### 2.3 `Pos` 不应由 Agent 提供

蓝图节点坐标属于编辑器布局，不属于业务逻辑。Agent 导入 JSON 不应包含以下字段：

```text
Pos
PosX
PosY
NodePosX
NodePosY
NodeWidth
NodeHeight
```

节点摆放应由插件根据图结构统一处理，例如：

- 入口节点从上到下排列。
- 执行流从左到右展开。
- Branch、Switch、Loop 分支自动分层。
- 数据依赖节点靠近消费节点。
- Reroute 节点按需要自动插入。
- 注释框根据包含节点自动计算范围。

---

## 三、设计目标

### 3.1 主要目标

1. 新增面向 Agent 的蓝图导入 JSON schema。
2. 删除 Agent 生成 JSON 中的坐标、GUID、完整 Pin 列表等低层字段。
3. 允许 Agent 使用语义节点描述蓝图逻辑。
4. 插件负责节点类型解析、Pin 补全、默认值补全、连接校验和自动布局。
5. 保持现有 raw JSON 导入链路不破坏。
6. 将导入错误设计为可诊断、可回传给 Agent 的结构化错误。

### 3.2 非目标

第一阶段不处理以下内容：

- 不废弃 `BlueprintHelper.JsonToBlueprint`。
- 不要求所有旧 fixture 迁移。
- 不实现自然语言到蓝图的自由推断。
- 不让 Agent 依赖当前编辑器焦点进行写入。
- 不在第一阶段实现复杂 patch diff 合并。
- 不把 `logic_json` 直接设计成可导入协议。

---

## 四、核心设计原则

### 4.1 Agent 写意图，插件写细节

Agent 负责输出：

```text
要在哪个蓝图、哪个图表中创建哪些逻辑节点，以及它们如何连接。
```

插件负责处理：

```text
节点实例化、Pin 解析、默认值补全、类型检查、坐标布局、编译和保存。
```

### 4.2 显式目标，不依赖焦点

所有写操作必须明确指定：

```json
{
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph"
}
```

除非用户明确要求操作当前激活上下文，否则 Agent 不应依赖当前打开的蓝图或当前选中的图表。

### 4.3 导入 JSON 不保存编辑器状态

Agent 导入 JSON 不包含：

- 节点坐标。
- 节点尺寸。
- Graph 视图缩放。
- Comment bubble 展开状态。
- 编译错误缓存。
- 编辑器 UI 展开状态。

### 4.4 保留确定性字段

虽然需要压缩，但不能压缩到不可诊断。以下字段仍应保留：

- 本地节点 `id`。
- 节点语义 `kind`。
- 函数、变量、事件、类、结构体引用。
- 输入默认值。
- 执行连线。
- 数据连线。
- 导入模式。
- 目标资产路径。
- 目标图表名称。

---

## 五、新 schema 概览

推荐 schema 名称：

```text
BlueprintHelper.AgentImportGraph
```

最小结构：

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "mode": "append",
  "layout": "auto",
  "nodes": [],
  "links": [],
  "options": {
    "compile": true,
    "save": false
  }
}
```

---

## 六、导入模式设计

| mode | 含义 | 风险 | 第一阶段建议 |
|------|------|------|--------------|
| `append` | 在目标图中追加节点 | 低 | 必做 |
| `replace_graph` | 清空目标图后重建 | 中 | 可做 |
| `patch` | 增量修改已有节点 | 高 | 后续阶段 |

第一阶段优先实现 `append`，再实现 `replace_graph`。

`patch` 需要稳定节点匹配、冲突检测和改动审阅机制，不应与最小导入协议同时强行完成。

---

## 七、节点语义设计

Agent 不直接写 `K2Node_*` 细节，而是写语义节点：

```json
{
  "id": "print_hello",
  "kind": "call",
  "function": "/Script/Engine.KismetSystemLibrary:PrintString",
  "inputs": {
    "InString": "Hello from Agent"
  }
}
```

推荐 `kind` 值：

```text
event
custom_event
call
get
set
branch
sequence
switch
loop
cast
construct
make_struct
break_struct
spawn_actor
bind_delegate
unbind_delegate
broadcast
timeline
comment
reroute
```

插件内部负责将这些语义节点映射为 UE 节点类型。

---

## 八、连线设计

连线应区分执行线和数据线：

```json
{
  "kind": "exec",
  "from": "begin_play.then",
  "to": "print_hello.execute"
}
```

```json
{
  "kind": "data",
  "from": "get_health.value",
  "to": "set_health.value"
}
```

推荐保留结构化兼容形式：

```json
{
  "kind": "exec",
  "from_node": "begin_play",
  "from_pin": "then",
  "to_node": "print_hello",
  "to_pin": "execute"
}
```

导入器可以同时支持字符串简写与结构化格式。MCP 文档中优先展示字符串简写，内部校验时统一转换为结构化格式。

---

## 九、布局设计

`Pos` 被移除后，需要新增高层布局策略：

```json
{
  "layout": "auto"
}
```

后续可扩展：

| layout | 行为 |
|--------|------|
| `auto` | 默认执行流布局 |
| `append_right` | 从现有图右侧追加 |
| `append_below` | 从现有图下方追加 |
| `compact` | 压缩布局，适合小函数图 |
| `debug_spread` | 拉开距离，便于调试 |
| `preserve_existing` | patch 模式下保留已有节点位置 |

第一阶段只需要实现 `auto` 和 `append_right`。

---

## 十、示例

### 10.1 BeginPlay 打印字符串

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "mode": "append",
  "layout": "auto",
  "nodes": [
    {
      "id": "begin_play",
      "kind": "event",
      "event": "ReceiveBeginPlay"
    },
    {
      "id": "print_hello",
      "kind": "call",
      "function": "/Script/Engine.KismetSystemLibrary:PrintString",
      "inputs": {
        "InString": "Hello from Agent"
      }
    }
  ],
  "links": [
    {
      "kind": "exec",
      "from": "begin_play.then",
      "to": "print_hello.execute"
    }
  ],
  "options": {
    "compile": true,
    "save": false
  }
}
```

### 10.2 变量声明、读取和写入

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "mode": "append",
  "declarations": {
    "variables": [
      {
        "name": "Health",
        "type": "float",
        "default": 100.0,
        "editable": true
      }
    ]
  },
  "nodes": [
    {
      "id": "begin_play",
      "kind": "event",
      "event": "ReceiveBeginPlay"
    },
    {
      "id": "set_health",
      "kind": "set",
      "var": "Health",
      "value": 100.0
    }
  ],
  "links": [
    {
      "kind": "exec",
      "from": "begin_play.then",
      "to": "set_health.execute"
    }
  ]
}
```

---

## 十一、收益

| 优化项 | 收益 |
|--------|------|
| 移除 Pos | 减少 token，避免 Agent 负责布局 |
| 移除 GUID 必填 | 降低非法导入概率 |
| 语义节点 | 降低 Agent 生成复杂度 |
| 自动 Pin 补全 | 减少错误 Pin 名和类型 |
| 默认值只写非默认项 | 减少冗余 |
| 统一 layout 策略 | 蓝图可读性更稳定 |
| 结构化错误 | Agent 可自动修复 JSON |

---

## 十二、最终结论

应新增 `BlueprintHelper.AgentImportGraph` 作为 Agent 专用导入协议。

该协议的核心定位是：

```text
面向 Agent 的蓝图构图意图格式，而不是 UE 节点快照格式。
```

它应删除 `Pos`、GUID、完整 Pin 列表和编辑器 UI 状态，将这些细节下放给插件处理。这样可以显著降低 token、降低导入失败率，并让 Agent 的输出更稳定、更可诊断。
