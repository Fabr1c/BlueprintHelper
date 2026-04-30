# BlueprintHelper Json -> 蓝图规则

## 目标
该规则用于约束外部大模型输出 **BlueprintHelper 可直接解析的 JSON**，再由插件在当前蓝图图表中生成节点。

当前版本支持以下闭环：
- Blueprint -> raw JSON（完整蓝图导出，含多图表、变量、函数签名，可用于导入回放）
- Blueprint -> logic Markdown / logic JSON（只读逻辑理解输出，不用于导入）
- JSON -> Blueprint（多图表导入，含 blueprint_operations + graphs 数组）
- JSON -> `UK2Node_CallFunction`
- JSON -> `K2Node_VariableGet / K2Node_VariableSet`
- JSON -> 标准宏 `K2Node_MacroInstance`（ForLoop / ForLoopWithBreak / ForEachLoop / ForEachLoopWithBreak / WhileLoop / FlipFlop / DoOnce / DoN / Gate / IsValid）
- JSON -> `K2Node_IfThenElse`（Branch）
- JSON -> `K2Node_ExecutionSequence`（Sequence）
- JSON -> `K2Node_CustomEvent`（自定义事件）
- JSON -> `K2Node_Event`（引擎事件：BeginPlay / Tick 等）
- JSON -> `K2Node_CallDelegate`（调用事件分发器）
- JSON -> `K2Node_AddDelegate`（绑定事件分发器）
- JSON -> `K2Node_RemoveDelegate`（解绑事件分发器）
- JSON -> `K2Node_ClearDelegate`（解绑所有）
- JSON -> `K2Node_AssignDelegate`（快捷绑定）
- JSON -> `K2Node_CreateDelegate`（创建委托对象）
- JSON -> `K2Node_MakeArray / K2Node_MakeSet / K2Node_MakeMap`（容器构造）
- JSON -> `K2Node_MakeStruct / K2Node_BreakStruct`（结构体操作）
- JSON -> `declarations.local_variables` 本地变量声明/确保存在
- **v2.0 新增** `blueprint_operations` 蓝图级操作：创建成员变量 / 函数图 / 事件分发器
- **v2.1 新增** `blueprint_operations` 扩展：创建宏图 / 删除图表 / 删除成员变量
- **v2.1 新增** `graphs` 多图表支持：单个 JSON 可向多个图表生成节点
- **v2.1 新增** `ExportBlueprintToJson` 完整蓝图导出（变量、函数签名、宏、所有图表节点和连线）
- **v2.2 新增** 高级节点：`K2Node_Self`、`K2Node_DynamicCast`、`K2Node_SpawnActorFromClass`、`K2Node_FormatText`、`K2Node_GetArrayItem`、`K2Node_Timeline`

> 当前仍不保证完整支持 `Select`、`Switch`、`MathExpression`、自定义宏库等更复杂节点。

---

## 顶层结构
必须输出一个 JSON 对象，禁止输出解释文字，禁止输出 Markdown 代码块包裹以外的说明。

### Schema 1.x（节点操作）
```json
{
  "version": "1.5",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "declarations": {
    "local_variables": []
  },
  "nodes": [],
  "links": []
}
```

### Schema 2.0（蓝图级操作 + 单图表节点操作）
```json
{
  "version": "2.0",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "blueprint_operations": [],
  "existing_node_refs": [],
  "declarations": {
    "local_variables": []
  },
  "nodes": [],
  "links": []
}
```

### Schema 2.1（多图表操作）
```json
{
  "version": "2.1",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "blueprint_operations": [],
  "graphs": [
    {
      "graph": "EventGraph",
      "existing_node_refs": [],
      "declarations": { "local_variables": [] },
      "nodes": [],
      "links": []
    },
    {
      "graph": "MyFunction",
      "existing_node_refs": [],
      "declarations": { "local_variables": [] },
      "nodes": [],
      "links": []
    }
  ]
}
```

> **向后兼容**：如果 JSON 中没有 `graphs` 数组而有顶层 `nodes`/`links`，插件自动按单图表模式处理（默认写入 EventGraph），完全兼容 1.x / 2.0 格式。

### 字段说明
- `version`: 当前版本 `"2.9"`，1.x / 2.0 / 2.1 版的 JSON 也兼容
- `schema`: 固定填 `"BlueprintHelper.JsonToBlueprint"`
- `blueprint_operations`: **v2.0 新增**，蓝图级操作数组（可选），在节点生成之前执行
- `declarations`: 声明区，可选，目前用于本地变量声明
- `nodes`: 节点数组（单图表模式）
- `links`: 节点连线数组（单图表模式）
- `graphs`: **v2.1 新增**，多图表数组，每项包含 `graph`（图表名）、`existing_node_refs`、`declarations`、`nodes`、`links`
- `existing_node_refs`: **v2.9 新增**，引用图中已有节点的数组（可选），用于增量导入时连线到已有节点

### 执行顺序
1. `blueprint_operations`（创建/删除变量、函数、宏图、事件分发器等蓝图资产级操作）
2. 对每个图表（单图表模式或 `graphs` 数组中的每个图表）：
   - `declarations.local_variables`（确保本地变量存在）
   - `nodes`（生成节点）
   - `links`（连线）

此顺序保证引用完整性：节点可以引用刚由 blueprint_operations 创建的变量、函数或宏图。

### graphs 数组字段说明
| 字段 | 必填 | 说明 |
|------|------|------|
| `graph` | 是 | 目标图表名称（如 `EventGraph`、函数名、宏名） |
| `declarations` | 否 | 该图表的声明区（local_variables 等） |
| `nodes` | 是 | 该图表的节点数组 |
| `links` | 是 | 该图表的连线数组 |

> **图表查找顺序**：UbergraphPages → FunctionGraphs → MacroGraphs → DelegateSignatureGraphs

---

## declarations 规则

### local_variables
用于声明当前**函数图**中需要存在的本地变量。

> **⚠ 图表作用域约束**：`local_variables` 仅在函数图（Function Graph）中有效。EventGraph 不支持本地变量。如果目标是 EventGraph，请使用**成员变量**（`scope: "member"`），不要使用 `declarations.local_variables` 和 `scope: "local"`。

```json
{
  "name": "LoopCounter",
  "ensure_exists": true,
  "pin_type": {
    "category": "int"
  },
  "default_value": "0"
}
```

### 字段说明
- `name`: 本地变量名称
- `ensure_exists`: 若变量不存在，生成前自动创建
- `pin_type`: 变量类型描述
- `default_value`: 默认值字符串

### pin_type 最小格式
```json
{
  "category": "bool|int|int64|float|double|string|name|text|object|class|struct|enum",
  "sub_category": "",
  "object_path": "",
  "container": "none|array|set|map",
  "is_reference": false,
  "is_const": false
}
```

---

## blueprint_operations 规则（v2.0）

蓝图级操作在节点生成之前执行，用于在蓝图资产中创建成员变量、函数图、事件分发器等。`blueprint_operations` 是一个数组，每个元素包含 `op` 字段标识操作类型。

### add_member_variable — 创建成员变量

```json
{
  "op": "add_member_variable",
  "name": "Health",
  "pin_type": { "category": "float" },
  "default_value": "100.0",
  "category": "Stats",
  "flags": {
    "blueprint_read_only": false,
    "expose_on_spawn": false
  }
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `op` | 是 | 固定 `"add_member_variable"` |
| `name` | 是 | 变量名称 |
| `pin_type` | 否 | 变量类型，默认 `bool` |
| `default_value` | 否 | 默认值字符串 |
| `category` | 否 | 蓝图编辑器中的分类 |
| `flags.blueprint_read_only` | 否 | 是否蓝图只读 |
| `flags.expose_on_spawn` | 否 | 是否在 SpawnActor 时暴露 |

**幂等**：若同名变量已存在则跳过，不会重复创建。

### add_function_graph — 创建自定义函数

```json
{
  "op": "add_function_graph",
  "name": "CalculateDamage",
  "inputs": [
    { "name": "BaseDamage", "pin_type": { "category": "float" } }
  ],
  "outputs": [
    { "name": "FinalDamage", "pin_type": { "category": "float" } }
  ],
  "is_pure": false
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `op` | 是 | 固定 `"add_function_graph"` |
| `name` | 是 | 函数名称 |
| `inputs` | 否 | 输入参数数组，每项含 `name` + `pin_type` |
| `outputs` | 否 | 输出参数数组，每项含 `name` + `pin_type` |
| `is_pure` | 否 | 是否为纯函数（无执行引脚） |

**幂等**：若同名函数图已存在则跳过。

### add_event_dispatcher — 创建事件分发器

```json
{
  "op": "add_event_dispatcher",
  "name": "OnHealthChanged",
  "params": [
    { "name": "NewHealth", "pin_type": { "category": "float" } }
  ]
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `op` | 是 | 固定 `"add_event_dispatcher"` |
| `name` | 是 | 事件分发器名称 |
| `params` | 否 | 签名参数数组，每项含 `name` + `pin_type` |

**幂等**：若同名事件分发器已存在则跳过。创建后可立即在 `nodes` 中使用 `K2Node_CallDelegate` / `K2Node_AddDelegate` 等委托节点引用该分发器。

### add_macro_graph — 创建宏图（v2.1）

```json
{
  "op": "add_macro_graph",
  "name": "MyMacro",
  "inputs": [
    { "name": "InExec", "pin_type": { "category": "exec" } },
    { "name": "InValue", "pin_type": { "category": "float" } }
  ],
  "outputs": [
    { "name": "OutExec", "pin_type": { "category": "exec" } },
    { "name": "OutResult", "pin_type": { "category": "float" } }
  ]
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `op` | 是 | 固定 `"add_macro_graph"` |
| `name` | 是 | 宏图名称 |
| `inputs` | 否 | Tunnel 输入引脚数组，每项含 `name` + `pin_type`。默认类型 `exec` |
| `outputs` | 否 | Tunnel 输出引脚数组，每项含 `name` + `pin_type`。默认类型 `exec` |

**幂等**：若同名宏图已存在则跳过。

### remove_graph — 删除图表（v2.1）

```json
{
  "op": "remove_graph",
  "name": "OldFunction"
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `op` | 是 | 固定 `"remove_graph"` |
| `name` | 是 | 要删除的函数图或宏图名称 |

**禁止删除 EventGraph。** 若目标图表不存在，操作视为成功（幂等）。

### remove_member_variable — 删除成员变量（v2.1）

```json
{
  "op": "remove_member_variable",
  "name": "OldHealth"
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `op` | 是 | 固定 `"remove_member_variable"` |
| `name` | 是 | 要删除的成员变量名（包括事件分发器属性） |

**幂等**：若变量不存在，操作视为成功。

---

## nodes 规则
每个节点对象格式如下：

```json
{
  "id": "Node_0",
  "type": "K2Node_CallFunction",
  "function_name": "PrintString",
  "name": "PrintString",
  "x": 0,
  "y": 0,
  "inputs": {
    "InString": "Hello"
  }
}
```

### 字段说明
- `id`: 节点唯一 ID，字符串，必须唯一
- `type`: 当前建议固定为 `K2Node_CallFunction`
- `function_name`: Unreal 函数名，优先填写 **NativeName**
- `name`: 显示名称，可与 `function_name` 相同
- `x`: 节点 X 坐标，数字
- `y`: 节点 Y 坐标，数字
- `inputs`: 输入引脚默认值对象，Key 为引脚名，Value 为字符串/数字/布尔

### `K2Node_CallFunction` 约束
1. `function_name` 必须是可用于蓝图调用的函数。
2. 优先使用 Unreal 原生函数名，不要依赖本地化显示名。
3. `inputs` 只填写**未通过连线提供值**的输入引脚默认值。
4. 如果没有默认值，可输出空对象：`{}`。

---

## 变量节点规则

### `K2Node_VariableGet`
```json
{
  "id": "Node_GetLoopCounter",
  "type": "K2Node_VariableGet",
  "name": "LoopCounter",
  "x": 0,
  "y": 0,
  "variable": {
    "scope": "local",
    "name": "LoopCounter",
    "ensure_exists": true,
    "pin_type": {
      "category": "int"
    },
    "default_value": "0"
  }
}
```

### `K2Node_VariableSet`
```json
{
  "id": "Node_SetLoopCounter",
  "type": "K2Node_VariableSet",
  "name": "LoopCounter",
  "x": 300,
  "y": 0,
  "variable": {
    "scope": "local",
    "name": "LoopCounter",
    "ensure_exists": true,
    "pin_type": {
      "category": "int"
    }
  },
  "inputs": {
    "value": "1"
  }
}
```

### variable 字段说明
- `scope`: `member` / `local`
- `name`: 变量名称
- `self_context`: 成员变量是否为 `self` 上下文，默认可省略
- `owner_class_path`: 外部成员变量所属类路径，可选
- `scope_graph_name`: 本地变量所属函数图名，可选
- `ensure_exists`: 本地变量缺失时自动创建（仅 `scope: "local"` 有效）
- `pin_type`: 本地变量自动创建时使用的类型
- `default_value`: 本地变量自动创建时使用的默认值

### 图表作用域与变量选择规则

| 图表类型 | 可用变量 scope | 说明 |
|---------|--------------|------|
| **EventGraph** | `member` | 事件图只能使用蓝图成员变量。成员变量必须已存在于蓝图中。 |
| **Function Graph** | `member` 或 `local` | 函数图可使用成员变量和本地变量。本地变量可通过 `ensure_exists` 自动创建。 |

**判断依据**：
- 如果 JSON 中包含 `K2Node_Event` 或 `K2Node_CustomEvent`，则**一定是 EventGraph**，变量必须用 `scope: "member"`，不得声明 `local_variables`。
- 如果 JSON 是纯函数逻辑（无事件节点），则通常在函数图中使用，可以自由使用 `scope: "local"` 和 `declarations.local_variables`。
- 委托操作节点（CallDelegate / AddDelegate 等）在两种图表中均可使用，但引用的 Event Dispatcher 必须是蓝图**成员变量**。

### 变量节点 Pin Alias
- `value`: 自动映射到变量写入节点的值引脚

---

## 宏节点规则

### `K2Node_MacroInstance`
当前首版只保证标准宏 `ForLoop`。

```json
{
  "id": "Node_ForLoop_0",
  "type": "K2Node_MacroInstance",
  "name": "ForLoop",
  "x": 0,
  "y": 220,
  "macro": {
    "library": "standard",
    "name": "ForLoop"
  },
  "inputs": {
    "first_index": "0",
    "last_index": "9"
  }
}
```

### macro 字段说明
- `library`: `standard` / `asset_path`
- `name`: 宏图名称
- `asset_path`: 自定义宏库蓝图路径，可选

### ForLoop Pin Alias
- `execute`
- `first_index`
- `last_index`
- `loop_body`
- `index`
- `completed`

---

## Branch 节点规则

### `K2Node_IfThenElse`

```json
{
  "id": "Node_Branch_0",
  "type": "K2Node_IfThenElse",
  "name": "Branch",
  "x": 300,
  "y": 0,
  "inputs": {}
}
```

### Branch Pin Alias
- `execute`: 输入执行引脚
- `condition`: 条件输入引脚（bool）
- `true` / `then`: 条件为真时的输出执行引脚
- `false` / `else`: 条件为假时的输出执行引脚

---

## Sequence 节点规则

### `K2Node_ExecutionSequence`

```json
{
  "id": "Node_Sequence_0",
  "type": "K2Node_ExecutionSequence",
  "name": "Sequence",
  "x": 0,
  "y": 0,
  "inputs": {
    "num_outputs": "3"
  }
}
```

### 字段说明
- `num_outputs`（可选）：输出引脚数量，默认为 2。用于指定 Sequence 输出分支数。

### Sequence Pin Alias
- `execute`: 输入执行引脚
- `then_0` / `Then 0`: 第 1 个输出执行引脚
- `then_1` / `Then 1`: 第 2 个输出执行引脚
- 以此类推

---

## 标准宏节点扩展

除 `ForLoop` 外，以下标准宏均可使用 `K2Node_MacroInstance` 生成，只需在 `macro.name` 中填写正确宏名：

### ForLoopWithBreak
```json
{
  "id": "Node_ForLoopBreak_0",
  "type": "K2Node_MacroInstance",
  "name": "ForLoopWithBreak",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "ForLoopWithBreak" },
  "inputs": { "first_index": "0", "last_index": "9" }
}
```
Pin: `execute`, `first_index`, `last_index`, `loop_body`, `index`, `completed`, `break`

### ForEachLoop
```json
{
  "id": "Node_ForEach_0",
  "type": "K2Node_MacroInstance",
  "name": "ForEachLoop",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "ForEachLoop" },
  "inputs": {}
}
```
Pin: `execute`, `array`, `loop_body`, `array_element`, `array_index`, `completed`

### WhileLoop
```json
{
  "id": "Node_While_0",
  "type": "K2Node_MacroInstance",
  "name": "WhileLoop",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "WhileLoop" },
  "inputs": {}
}
```
Pin: `execute`, `condition`, `loop_body`, `completed`

### FlipFlop
```json
{
  "id": "Node_FlipFlop_0",
  "type": "K2Node_MacroInstance",
  "name": "FlipFlop",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "FlipFlop" },
  "inputs": {}
}
```
Pin: `execute`, `a`, `b`, `is_a`

### DoOnce
```json
{
  "id": "Node_DoOnce_0",
  "type": "K2Node_MacroInstance",
  "name": "DoOnce",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "DoOnce" },
  "inputs": {}
}
```
Pin: `execute`, `completed`, `reset`, `start_closed`

### DoN
```json
{
  "id": "Node_DoN_0",
  "type": "K2Node_MacroInstance",
  "name": "DoN",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "DoN" },
  "inputs": { "n": "5" }
}
```
Pin: `execute`, `n`, `reset`, `counter`, `completed`

### Gate
```json
{
  "id": "Node_Gate_0",
  "type": "K2Node_MacroInstance",
  "name": "Gate",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "Gate" },
  "inputs": {}
}
```
Pin: `enter`, `open`, `close`, `toggle`, `start_closed`, `exit`

### IsValid（宏版）
```json
{
  "id": "Node_IsValid_0",
  "type": "K2Node_MacroInstance",
  "name": "IsValid",
  "x": 0, "y": 0,
  "macro": { "library": "standard", "name": "IsValid" },
  "inputs": {}
}
```
Pin: `execute`, `input_object`, `is_valid`, `is_not_valid`

---

## 事件节点规则

### `K2Node_CustomEvent`（自定义事件）

创建一个自定义事件节点，可带自定义输出参数引脚。

```json
{
  "id": "Node_CustomEvent_0",
  "type": "K2Node_CustomEvent",
  "name": "OnDamageReceived",
  "x": 0,
  "y": 0,
  "event": {
    "event_name": "OnDamageReceived",
    "params": [
      { "name": "Damage", "pin_type": { "category": "float" } },
      { "name": "Instigator", "pin_type": { "category": "object", "object_path": "/Script/Engine.Actor" } }
    ]
  }
}
```

### event 字段说明
- `event_name`: 自定义事件名称（必填）
- `params`: 事件输出参数数组，每项包含 `name` 和 `pin_type`

### CustomEvent Pin Alias
- `then`: 输出执行引脚
- `delegate`: 输出委托引脚

---

### `K2Node_Event`（引擎事件）

生成引擎预定义事件节点（如 BeginPlay、Tick）。事件名支持友好名称自动映射。

```json
{
  "id": "Node_BeginPlay",
  "type": "K2Node_Event",
  "name": "BeginPlay",
  "x": 0,
  "y": 0,
  "event": {
    "event_name": "BeginPlay"
  }
}
```

### 常见引擎事件名称
| 友好名称 | 实际函数名 |
|---------|----------|
| BeginPlay | ReceiveBeginPlay |
| Tick | ReceiveTick |
| EndPlay | ReceiveEndPlay |
| AnyDamage | ReceiveAnyDamage |
| ActorBeginOverlap | ReceiveActorBeginOverlap |
| ActorEndOverlap | ReceiveActorEndOverlap |
| Destroyed | ReceiveDestroyed |
| Hit | ReceiveHit |

> 注意：引擎事件节点只能在 EventGraph 中使用。事件必须在蓝图父类中存在。

---

## 委托节点规则

所有委托类节点需要蓝图中已存在对应的事件分发器（Event Dispatcher）属性。

### `K2Node_CallDelegate`（调用事件分发器）

```json
{
  "id": "Node_CallDelegate_0",
  "type": "K2Node_CallDelegate",
  "name": "OnHealthChanged",
  "x": 400,
  "y": 0,
  "delegate": {
    "property_name": "OnHealthChanged"
  }
}
```

### `K2Node_AddDelegate`（绑定事件分发器）

```json
{
  "id": "Node_Bind_0",
  "type": "K2Node_AddDelegate",
  "name": "OnHealthChanged",
  "x": 400,
  "y": 0,
  "delegate": {
    "property_name": "OnHealthChanged"
  }
}
```

### `K2Node_RemoveDelegate`（解绑事件分发器）

```json
{
  "id": "Node_Unbind_0",
  "type": "K2Node_RemoveDelegate",
  "name": "OnHealthChanged",
  "x": 400,
  "y": 0,
  "delegate": {
    "property_name": "OnHealthChanged"
  }
}
```

### `K2Node_ClearDelegate`（解绑所有）

```json
{
  "id": "Node_ClearAll_0",
  "type": "K2Node_ClearDelegate",
  "name": "OnHealthChanged",
  "x": 400,
  "y": 0,
  "delegate": {
    "property_name": "OnHealthChanged"
  }
}
```

### `K2Node_AssignDelegate`（快捷绑定）

同时创建绑定和对应的自定义事件节点。

```json
{
  "id": "Node_Assign_0",
  "type": "K2Node_AssignDelegate",
  "name": "OnHealthChanged",
  "x": 400,
  "y": 0,
  "delegate": {
    "property_name": "OnHealthChanged"
  }
}
```

### `K2Node_CreateDelegate`（创建委托对象）

创建单播委托引用，可指定绑定函数名。

```json
{
  "id": "Node_CreateDelegate_0",
  "type": "K2Node_CreateDelegate",
  "name": "CreateDelegate",
  "x": 400,
  "y": 0,
  "delegate": {
    "function_name": "MyCustomFunction"
  }
}
```

### delegate 字段说明
- `property_name`: 事件分发器属性名（CallDelegate / AddDelegate / RemoveDelegate / ClearDelegate / AssignDelegate 必填）
- `function_name`: 绑定的函数名（CreateDelegate 时使用）

### 委托 Pin Alias
- `execute`: 输入执行引脚
- `then`: 输出执行引脚
- `delegate` / `event`: 委托引脚
- `output_delegate`: CreateDelegate 的输出委托引脚

> 前提条件：蓝图中必须已有对应名称的 Event Dispatcher 成员变量，否则生成会报错。

---

## 容器构造节点规则

### MakeArray — 构造数组字面量

```json
{
  "id": "Node_MakeArray_0",
  "type": "K2Node_MakeArray",
  "name": "MakeArray",
  "x": 0, "y": 0,
  "container": {
    "num_inputs": 3
  },
  "inputs": {
    "[0]": "10",
    "[1]": "20",
    "[2]": "30"
  }
}
```

### MakeSet — 构造集合字面量

```json
{
  "id": "Node_MakeSet_0",
  "type": "K2Node_MakeSet",
  "name": "MakeSet",
  "x": 0, "y": 0,
  "container": {
    "num_inputs": 2
  },
  "inputs": {
    "[0]": "Apple",
    "[1]": "Banana"
  }
}
```

### MakeMap — 构造映射字面量

```json
{
  "id": "Node_MakeMap_0",
  "type": "K2Node_MakeMap",
  "name": "MakeMap",
  "x": 0, "y": 0,
  "container": {
    "num_pairs": 2
  },
  "inputs": {
    "Key 0": "Health",
    "Value 0": "100",
    "Key 1": "Mana",
    "Value 1": "50"
  }
}
```

### container 字段说明
- `num_inputs`: MakeArray / MakeSet 的元素数量（最小 1）
- `num_pairs`: MakeMap 的键值对数量（最小 1）
- 元素类型由连线上下文自动推断，无需在 JSON 中指定

### 容器 Pin Alias
- MakeArray / MakeSet: `[0]`, `[1]`, `[2]`... 为输入引脚，`output` 为输出引脚
- MakeMap: `Key 0`, `Value 0`, `Key 1`, `Value 1`... 为输入引脚，`output` 为输出引脚

---

## 结构体操作节点规则

### MakeStruct — 构造结构体

```json
{
  "id": "Node_MakeVector_0",
  "type": "K2Node_MakeStruct",
  "name": "MakeVector",
  "x": 0, "y": 0,
  "struct": {
    "struct_path": "/Script/CoreUObject.Vector"
  },
  "inputs": {
    "X": "1.0",
    "Y": "2.0",
    "Z": "3.0"
  }
}
```

### BreakStruct — 拆解结构体

```json
{
  "id": "Node_BreakVector_0",
  "type": "K2Node_BreakStruct",
  "name": "BreakVector",
  "x": 400, "y": 0,
  "struct": {
    "struct_path": "/Script/CoreUObject.Vector"
  }
}
```

### struct 字段说明
- `struct_path`: 结构体完整路径（必填），常见值：
  - `/Script/CoreUObject.Vector` — FVector
  - `/Script/CoreUObject.Rotator` — FRotator
  - `/Script/CoreUObject.Transform` — FTransform
  - `/Script/CoreUObject.Vector2D` — FVector2D
  - `/Script/CoreUObject.LinearColor` — FLinearColor
  - `/Script/SlateCore.SlateBrush` — FSlateBrush

### 结构体 Pin Alias
- MakeStruct / BreakStruct 的引脚名称与结构体成员名一致（例如 Vector 的 `X`、`Y`、`Z`），无需额外别名

---

## 高级节点（v2.2）

### K2Node_Self（自身引用）
最简单的纯节点，输出当前蓝图实例的 self 引用。

```json
{
  "id": "self_1",
  "type": "K2Node_Self",
  "x": 0, "y": 0
}
```

无需任何额外字段。

### K2Node_DynamicCast（类型转换）

```json
{
  "id": "cast_1",
  "type": "K2Node_DynamicCast",
  "x": 200, "y": 0,
  "cast": {
    "target_class_path": "/Script/Engine.Character"
  }
}
```

| 字段 | 说明 |
|------|------|
| `cast.target_class_path` | 目标类路径，例如 `/Script/Engine.Character`、`/Script/MyProject.MyActor`。也可以使用顶层 `target_class_path` |

**Pin Alias**：`object`（输入对象）、`cast_result`（转换结果）、`success`（布尔输出）、`valid`（转换成功执行）、`invalid`（转换失败执行）

### K2Node_SpawnActorFromClass（生成 Actor）

```json
{
  "id": "spawn_1",
  "type": "K2Node_SpawnActorFromClass",
  "x": 400, "y": 0,
  "spawn": {
    "class_path": "/Script/Engine.StaticMeshActor"
  },
  "default_values": {
    "SpawnTransform": "()"
  }
}
```

| 字段 | 说明 |
|------|------|
| `spawn.class_path` | 要生成的 Actor 类路径。也可以使用顶层 `class_path` |

**Pin Alias**：`class`、`spawn_transform`、`return_value`、`collision_handling_override`、`owner`、`instigator`

### K2Node_FormatText（格式化文本）
纯节点，根据格式字符串中的 `{ArgName}` 占位符动态创建参数引脚。

```json
{
  "id": "fmt_1",
  "type": "K2Node_FormatText",
  "x": 600, "y": 0,
  "format_text": {
    "format_string": "Hello {Name}, you have {Count} items!"
  },
  "default_values": {
    "Name": "World",
    "Count": "5"
  }
}
```

| 字段 | 说明 |
|------|------|
| `format_text.format_string` | 格式字符串，使用 `{ArgName}` 标记占位符。占位符名称会自动创建对应的输入引脚 |

**Pin Alias**：`format`（格式字符串输入）、`result`（格式化后的文本输出）

### K2Node_GetArrayItem（通过索引获取数组元素）
纯节点，通配符引脚。

```json
{
  "id": "arr_get_1",
  "type": "K2Node_GetArrayItem",
  "x": 800, "y": 0,
  "default_values": {
    "Dimension 1": "0"
  }
}
```

**Pin Alias**：`array`（输入数组）、`index`（索引）、`element`（输出元素）

### K2Node_Timeline（时间轴）
创建时间轴节点及其关联的 UTimelineTemplate。支持基本配置（名称、自动播放、循环）和轨道定义。

```json
{
  "id": "tl_1",
  "type": "K2Node_Timeline",
  "x": 0, "y": 200,
  "timeline": {
    "name": "MyTimeline",
    "auto_play": false,
    "loop": true,
    "float_tracks": ["Alpha", "Speed"],
    "vector_tracks": ["Location"],
    "event_tracks": ["OnHalfway"]
  }
}
```

| 字段 | 说明 |
|------|------|
| `timeline.name` | 时间轴名称（可选，默认自动分配唯一名称） |
| `timeline.auto_play` | 是否自动播放（默认 false） |
| `timeline.loop` | 是否循环（默认 false） |
| `timeline.float_tracks` | Float 轨道名称数组，每个名称对应一个 float 输出引脚 |
| `timeline.vector_tracks` | Vector 轨道名称数组 |
| `timeline.event_tracks` | Event 轨道名称数组，每个名称对应一个执行输出引脚 |

**Pin Alias**：`play`、`play_from_start`、`stop`、`reverse`、`reverse_from_end`、`update`、`finished`、`direction`、`set_new_time`、`new_time`

> 注意：当前不支持在 JSON 中定义轨道曲线关键帧数据，轨道创建后需要在编辑器中手动配置曲线。

---

## v2.3 — 全覆盖收尾节点

### K2Node_Knot（重新路由 / 布线转接）
纯视觉布线节点，无特殊字段。

```json
{
  "id": "Knot_0",
  "type": "K2Node_Knot",
  "x": 200, "y": 100
}
```
别名：`Knot`、`Reroute`。

**Pin Alias**：`input`→InputPin、`output`→OutputPin

---

### EdGraphNode_Comment（注释框）
非 K2Node，不参与连线。

```json
{
  "id": "Comment_0",
  "type": "Comment",
  "x": 0, "y": -100,
  "comment": {
    "text": "这里是主要逻辑",
    "width": 600,
    "height": 200,
    "font_size": 18,
    "color": "(R=1.0,G=1.0,B=1.0,A=1.0)"
  }
}
```
别名：`Comment`、`EdGraphNode_Comment`。

| 字段 | 说明 |
|------|------|
| `comment.text` | 注释文本（必填） |
| `comment.width` | 注释框宽度（默认 400） |
| `comment.height` | 注释框高度（默认 100） |
| `comment.font_size` | 字体大小（默认 18） |
| `comment.color` | 注释框颜色，FLinearColor 格式 `(R=,G=,B=,A=)` |

也可使用扁平格式：`"comment_text": "..."` 省略嵌套对象。

---

### K2Node_Literal（对象引用常量）
用于在图中引用固定的 UObject 对象（Actor、DataAsset 等）。

```json
{
  "id": "Literal_0",
  "type": "K2Node_Literal",
  "x": 0, "y": 0,
  "literal": {
    "object_path": "/Game/Blueprints/BP_MyActor.BP_MyActor_C"
  }
}
```
别名：`Literal`。

| 字段 | 说明 |
|------|------|
| `literal.object_path` | 对象资产路径（必填，格式同 UE 资产路径） |

也可使用扁平格式：`"object_path": "..."` 省略嵌套对象。

---

### K2Node_GetEnumeratorName（枚举名获取器 → FName）
纯节点，接受枚举字节输入，输出 FName。

```json
{
  "id": "EnumName_0",
  "type": "K2Node_GetEnumeratorName",
  "x": 300, "y": 0
}
```
别名：`GetEnumeratorName`。

**Pin Alias**：`input`→EnumeratorValue、`output`→ReturnValue

---

### K2Node_GetEnumeratorNameAsString（枚举名获取器 → FString）
纯节点，接受枚举字节输入，输出 FString。

```json
{
  "id": "EnumString_0",
  "type": "K2Node_GetEnumeratorNameAsString",
  "x": 300, "y": 200
}
```
别名：`GetEnumeratorNameAsString`。

**Pin Alias**：`input`→EnumeratorValue、`output`→ReturnValue

---

### K2Node_ComponentBoundEvent（组件绑定事件）
将组件上的多播委托绑定为事件图中的事件节点。

```json
{
  "id": "CompEvent_0",
  "type": "K2Node_ComponentBoundEvent",
  "x": 0, "y": 0,
  "component_event": {
    "delegate_property": "OnComponentBeginOverlap",
    "delegate_owner_class": "/Script/Engine.PrimitiveComponent",
    "component_property": "CollisionComponent"
  }
}
```
别名：`ComponentBoundEvent`。

| 字段 | 说明 |
|------|------|
| `component_event.delegate_property` | 委托属性名称（必填，如 `OnComponentBeginOverlap`） |
| `component_event.delegate_owner_class` | 委托所属类路径（可选，不填时从组件类推断） |
| `component_event.component_property` | 蓝图上的组件属性名称（必填） |

也可使用扁平格式：`"delegate_property": "..."`, `"component_property": "..."`, `"delegate_owner_class": "..."` 省略嵌套对象。

---

### K2Node_EnhancedInputAction（增强输入动作，v2.9）
生成 Enhanced Input 系统的输入动作事件节点。

```json
{
  "id": "InputAction_0",
  "type": "K2Node_EnhancedInputAction",
  "x": 0, "y": 0,
  "input_action_path": "/Game/Input/IA_Jump"
}
```

别名：`EnhancedInputAction`、`InputAction`。

| 字段 | 说明 |
|------|------|
| `input_action_path` | InputAction 资产路径（必填） |

也可使用嵌套格式：`"input_action": { "path": "/Game/Input/IA_Jump" }`。

---

### K2Node_PromotableOperator（可提升运算符，v2.9）
UE5 的数学运算符节点（加减乘除、比较等），通过 `function_name` 指定运算函数。

```json
{
  "id": "Add_0",
  "type": "K2Node_PromotableOperator",
  "function_name": "Add_IntInt",
  "x": 0, "y": 0
}
```

别名：`PromotableOperator`。

常用运算函数名：

| 运算 | function_name（整数）| function_name（浮点）| function_name（向量）|
|------|---------------------|---------------------|---------------------|
| 加法 | `Add_IntInt` | `Add_FloatFloat` | `Add_VectorVector` |
| 减法 | `Subtract_IntInt` | `Subtract_FloatFloat` | `Subtract_VectorVector` |
| 乘法 | `Multiply_IntInt` | `Multiply_FloatFloat` | `Multiply_VectorFloat` |
| 除法 | `Divide_IntInt` | `Divide_FloatFloat` | `Divide_VectorFloat` |
| 等于 | `EqualEqual_IntInt` | `EqualEqual_FloatFloat` | `EqualEqual_VectorVector` |
| 不等 | `NotEqual_IntInt` | `NotEqual_FloatFloat` | `NotEqual_VectorVector` |
| 大于 | `Greater_IntInt` | `Greater_FloatFloat` | — |
| 小于 | `Less_IntInt` | `Less_FloatFloat` | — |
| 大于等于 | `GreaterEqual_IntInt` | `GreaterEqual_FloatFloat` | — |
| 小于等于 | `LessEqual_IntInt` | `LessEqual_FloatFloat` | — |

> **提示**：对于简单的数学运算，也可以使用 `K2Node_CallFunction` + `function_name` 直接调用 `UKismetMathLibrary` 中的函数，效果等价。

---

### K2Node_CommutativeAssociativeBinaryOperator（交换结合律二元运算符，v2.9）
支持可动态添加输入引脚的运算节点（如多参数加法、乘法）。

```json
{
  "id": "MultiAdd_0",
  "type": "K2Node_CommutativeAssociativeBinaryOperator",
  "function_name": "Add_IntInt",
  "x": 0, "y": 0
}
```

别名：`CommutativeAssociativeBinaryOperator`。

---

### K2Node_SwitchInteger（整数Switch，v2.9）

```json
{
  "id": "SwitchInt_0",
  "type": "K2Node_SwitchInteger",
  "x": 0, "y": 0,
  "switch": {
    "case_values": ["0", "1", "2"],
    "has_default": true,
    "start_index": 0
  }
}
```

别名：`SwitchInteger`、`SwitchOnInt`。

### K2Node_SwitchString（字符串Switch，v2.9）

```json
{
  "id": "SwitchStr_0",
  "type": "K2Node_SwitchString",
  "x": 0, "y": 0,
  "switch": {
    "case_values": ["Hello", "World", "Foo"],
    "has_default": true
  }
}
```

别名：`SwitchString`、`SwitchOnString`。

### K2Node_SwitchName（名称Switch，v2.9）

```json
{
  "id": "SwitchName_0",
  "type": "K2Node_SwitchName",
  "x": 0, "y": 0,
  "switch": {
    "case_values": ["Name1", "Name2"],
    "has_default": true
  }
}
```

别名：`SwitchName`、`SwitchOnName`。

### K2Node_SwitchEnum（枚举Switch，v2.9）

```json
{
  "id": "SwitchEnum_0",
  "type": "K2Node_SwitchEnum",
  "x": 0, "y": 0,
  "switch": {
    "enum_path": "/Script/Engine.ECollisionChannel",
    "has_default": true
  }
}
```

别名：`SwitchEnum`、`SwitchOnEnum`。

---

### K2Node_Select（条件选择，v2.9）

```json
{
  "id": "Select_0",
  "type": "K2Node_Select",
  "x": 0, "y": 0,
  "select": {
    "num_options": 3,
    "enum_path": ""
  }
}
```

别名：`Select`。

| 字段 | 说明 |
|------|------|
| `select.num_options` | 选项数量（默认 2） |
| `select.enum_path` | 绑定的枚举路径（可选，基于枚举选择时填写） |

---

## 数学运算常用函数快查表（v2.9）

以下数学运算可使用 `K2Node_CallFunction` + `function_name` 直接调用，无需特殊节点类型。

### 基础数学（UKismetMathLibrary）

| 操作 | function_name | 备注 |
|------|--------------|------|
| 绝对值 | `Abs` / `Abs_Int` | |
| 取整 | `FFloor` / `FCeil` / `Round` | |
| 最大/最小 | `Max` / `Min` / `FMax` / `FMin` | |
| Clamp | `Clamp` / `FClamp` | |
| 幂运算 | `FPower` | |
| 平方根 | `Sqrt` | |
| 线性插值 | `Lerp` | |
| 映射范围 | `MapRangeClamped` / `MapRangeUnclamped` | |
| 取模 | `Percent_IntInt` / `Percent_FloatFloat` | |
| 随机数 | `RandomInteger` / `RandomFloat` / `RandomIntegerInRange` / `RandomFloatInRange` | |
| 三角函数 | `Sin` / `Cos` / `Tan` / `Asin` / `Acos` / `Atan` / `Atan2` | 弧度 |
| 角度转弧度 | `DegreesToRadians` | |
| 弧度转角度 | `RadiansToDegrees` | |

### 向量运算（UKismetMathLibrary）

| 操作 | function_name | 备注 |
|------|--------------|------|
| 向量加法 | `Add_VectorVector` | |
| 向量减法 | `Subtract_VectorVector` | |
| 向量缩放 | `Multiply_VectorFloat` / `Multiply_VectorVector` | |
| 点积 | `Dot_VectorVector` | |
| 叉积 | `Cross_VectorVector` | |
| 长度 | `VSize` | |
| 长度平方 | `VSizeSquared` | |
| 2D 长度 | `VSize2D` | |
| 归一化 | `Normal` | |
| 距离 | `Vector_Distance` | |
| 距离 2D | `Vector_Distance2D` | |
| 向量插值 | `VLerp` | |
| 随机向量 | `RandomUnitVector` | |
| 向量旋转 | `RotateAngleAxis` | |
| 向量投影 | `ProjectVectorOnToVector` | |

### 旋转器运算（UKismetMathLibrary）

| 操作 | function_name | 备注 |
|------|--------------|------|
| 旋转器加法 | `ComposeRotators` | |
| 旋转器插值 | `RLerp` | |
| 从 X/Y/Z 获取旋转 | `MakeRotFromX` / `MakeRotFromY` / `MakeRotFromZ` | |
| 查找朝向旋转 | `FindLookAtRotation` | |
| 旋转器转向量 | `GetForwardVector` / `GetRightVector` / `GetUpVector` | |
| 分解旋转器 | `BreakRotator` / `BreakRotIntoComponents` | |
| 构造旋转器 | `MakeRotator` | |

### Transform 运算（UKismetMathLibrary）

| 操作 | function_name | 备注 |
|------|--------------|------|
| 构造 Transform | `MakeTransform` | |
| 分解 Transform | `BreakTransform` | |
| Transform 合成 | `ComposeTransforms` | |
| 反转 Transform | `InvertTransform` | |
| 变换位置 | `TransformLocation` | |
| 变换方向 | `TransformDirection` | |
| Transform 插值 | `TLerp` | |

---

## 容器操作函数快查表

以下操作函数为 `K2Node_CallFunction`，使用标准函数调用节点即可。

### Array 操作（UKismetArrayLibrary）
| 操作 | function_name | 备注 |
|------|--------------|------|
| 添加元素 | `Array_Add` | |
| 移除指定索引 | `Array_Remove` | |
| 查找元素 | `Array_Find` | 返回索引 |
| 数组长度 | `Array_Length` | |
| 获取元素 | `Array_Get` | |
| 包含判断 | `Array_Contains` | |
| 清空 | `Array_Clear` | |
| 追加数组 | `Array_Append` | |
| 随机元素 | `Array_Random` | |

### Map 操作（UBlueprintMapLibrary）
| 操作 | function_name | 备注 |
|------|--------------|------|
| 添加键值对 | `Map_Add` | |
| 移除键 | `Map_Remove` | |
| 查找值 | `Map_Find` | |
| 包含判断 | `Map_Contains` | |
| 获取所有键 | `Map_Keys` | |
| 获取所有值 | `Map_Values` | |
| Map 长度 | `Map_Length` | |

### Set 操作（UBlueprintSetLibrary）
| 操作 | function_name | 备注 |
|------|--------------|------|
| 添加元素 | `Set_Add` | |
| 移除元素 | `Set_Remove` | |
| 包含判断 | `Set_Contains` | |
| Set 长度 | `Set_Length` | |
| 清空 | `Set_Clear` | |

---

## links 规则
每条连线对象格式如下：

```json
{
  "from_id": "Node_0",
  "from_pin": "then",
  "to_id": "Node_1",
  "to_pin": "execute"
}
```

### 字段说明
- `from_id`: 起始节点 ID
- `from_pin`: 起始引脚名
- `to_id`: 目标节点 ID
- `to_pin`: 目标引脚名

### 约束
1. 引脚名称必须使用蓝图节点实际 Pin 名称。
2. `exec` 流程连接与普通数据连接都通过 `links` 表达。
3. 不要连接不存在的引脚。
4. 如果不确定引脚名，可以省略该连线，不要猜测。

---

## 推荐输出策略
1. 优先生成 `K2Node_CallFunction`、`K2Node_VariableGet`、`K2Node_VariableSet`。
2. 控制流使用 `K2Node_IfThenElse`（Branch）或 `K2Node_ExecutionSequence`（Sequence）。
3. 循环使用标准宏：`ForLoop`、`ForEachLoop`、`WhileLoop` 等（`K2Node_MacroInstance`）。
4. 流程控制宏：`FlipFlop`、`DoOnce`、`DoN`、`Gate`、`IsValid`（`K2Node_MacroInstance`）。
5. 自定义事件使用 `K2Node_CustomEvent`，引擎事件（BeginPlay / Tick）使用 `K2Node_Event`。
6. 事件分发器操作使用 `K2Node_CallDelegate` / `K2Node_AddDelegate` / `K2Node_RemoveDelegate` / `K2Node_ClearDelegate`。
7. **变量选择须看目标图表**：EventGraph 中只能用 `scope: "member"`（成员变量），不能声明或使用 `local_variables`；函数图中可使用 `scope: "local"` 并配合 `ensure_exists`。
8. 如果 JSON 包含 `K2Node_Event` 或 `K2Node_CustomEvent`，则不得出现 `declarations.local_variables` 和 `scope: "local"` 变量。
9. 若某节点无法稳定确认，宁可省略，也不要猜测错误函数名或错误 pin 名。
9. 节点坐标按从左到右递增，例如每个节点 X 增加 280，Y 保持同一行或少量偏移。

---

## 最小可用示例
```json
{
  "version": "2.0",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "declarations": {
    "local_variables": [
      {
        "name": "LoopCounter",
        "ensure_exists": true,
        "pin_type": {
          "category": "int"
        },
        "default_value": "0"
      }
    ]
  },
  "nodes": [
    {
      "id": "Node_Print_0",
      "type": "K2Node_CallFunction",
      "function_name": "PrintString",
      "name": "PrintString",
      "x": 0,
      "y": 0,
      "inputs": {
        "InString": "Hello MrStone"
      }
    },
    {
      "id": "Node_ForLoop_0",
      "type": "K2Node_MacroInstance",
      "name": "ForLoop",
      "x": 320,
      "y": 180,
      "macro": {
        "library": "standard",
        "name": "ForLoop"
      },
      "inputs": {
        "first_index": "0",
        "last_index": "9"
      }
    },
    {
      "id": "Node_SetLoopCounter",
      "type": "K2Node_VariableSet",
      "name": "LoopCounter",
      "x": 640,
      "y": 180,
      "variable": {
        "scope": "local",
        "name": "LoopCounter",
        "ensure_exists": true,
        "pin_type": {
          "category": "int"
        }
      },
      "inputs": {
        "value": "1"
      }
    }
  ],
  "links": [
    {
      "from_id": "Node_Print_0",
      "from_pin": "then",
      "to_id": "Node_ForLoop_0",
      "to_pin": "execute"
    },
    {
      "from_id": "Node_ForLoop_0",
      "from_pin": "loop_body",
      "to_id": "Node_SetLoopCounter",
      "to_pin": "execute"
    }
  ]
}
```

---

## 给外部大模型的提示模板
你可以将下面这段直接发给外部大模型：

```text
请根据我的蓝图需求，输出 BlueprintHelper 可解析的 JSON。
要求：
1. 只输出 JSON，不要输出解释文字。
2. 顶层必须包含 version、schema、nodes、links。可选包含 blueprint_operations 和 declarations。
3. schema 固定为 BlueprintHelper.JsonToBlueprint，version 固定为 "2.1"。
4. `blueprint_operations` 在节点之前执行，可创建成员变量（add_member_variable）、函数图（add_function_graph）、事件分发器（add_event_dispatcher）、宏图（add_macro_graph），也可删除图表（remove_graph）、删除成员变量（remove_member_variable）。
4b. 多图表模式可使用 `graphs` 数组替代顶层 `nodes`/`links`，每项指定 `graph` 名+对应节点和连线。
5. `type` 可使用 `K2Node_CallFunction`、`K2Node_VariableGet`、`K2Node_VariableSet`、`K2Node_MacroInstance`、`K2Node_IfThenElse`、`K2Node_ExecutionSequence`、`K2Node_CustomEvent`、`K2Node_Event`、`K2Node_CallDelegate`、`K2Node_AddDelegate`、`K2Node_RemoveDelegate`、`K2Node_ClearDelegate`、`K2Node_AssignDelegate`、`K2Node_CreateDelegate`、`K2Node_MakeArray`、`K2Node_MakeSet`、`K2Node_MakeMap`、`K2Node_MakeStruct`、`K2Node_BreakStruct`、`K2Node_EnhancedInputAction`、`K2Node_PromotableOperator`、`K2Node_CommutativeAssociativeBinaryOperator`、`K2Node_SwitchInteger`、`K2Node_SwitchString`、`K2Node_SwitchName`、`K2Node_SwitchEnum`、`K2Node_Select`。
6. `function_name` 优先使用 Unreal 可蓝图调用函数的原生函数名。
7. 函数图中的本地变量可写入 `declarations.local_variables`，或在变量节点上写 `ensure_exists=true`。EventGraph 中只能使用成员变量（`scope: "member"`），不得使用 `local_variables`。
8. 若需要在 EventGraph 中使用成员变量或事件分发器，先在 `blueprint_operations` 中创建。
9. `inputs` 仅填写未连线的默认值；变量写入值可用 `value`；ForLoop 可用 `first_index/last_index`。
10. `links` 使用 from_id / from_pin / to_id / to_pin。
11. 若无法稳定确认函数、变量或引脚名，宁可省略，不要猜测。
```

---

## 蓝图完整导出（v2.1）

插件提供 `ExportBlueprintToJson` 和 `ConvertGraphToJson` 两个导出功能：

- **ExportBlueprintToJson**：导出整个蓝图，输出包含 `blueprint_operations`（成员变量、函数图签名、宏图、事件分发器）和 `graphs` 数组（EventGraph + 函数图 + 宏图的节点和连线）。
- **ConvertGraphToJson**：导出单个图表的节点和连线。

导出的 JSON 格式完全兼容导入规则，可直接用于重建蓝图。

> 注意：导出跳过 FunctionEntry / FunctionResult 节点（它们在 `add_function_graph` 签名中表达），也跳过 UserConstructionScript。

> **v2.9 新增**：导出的每个节点现在包含 `node_guid` 字段（32 位十六进制 GUID），可在增量导入的 `existing_node_refs` 中精确引用。

### MCP 导出工具边界

- `blueprint_export_to_json` 调用 raw JSON 导出，返回符合本规则的 BlueprintHelper JSON，可保存、修改后通过 `blueprint_import_json_to_graph` 写回或回放。
- `blueprint_get_logic` 调用 `export_logic` 且使用 `format=logic_md`，返回面向 Agent 阅读的 Markdown 逻辑摘要，只用于理解、审查和规划。
- `blueprint_get_logic_json` 调用 `export_logic` 且使用 `format=logic_json`，返回结构化逻辑信息，只用于分析；它不是本规则定义的导入 JSON。
- 不得将 logic Markdown 或 logic JSON 直接传给 `blueprint_import_json_to_graph`。如需写回蓝图，必须生成或使用符合本规则的 raw JSON。

---

## 增量导入 — 引用已有节点（v2.9）

在增量导入场景中，新生成的节点可能需要连接到图表中已有的节点。使用 `existing_node_refs` 数组声明对已有节点的引用，之后即可在 `links` 中使用这些引用 ID。

### 字段说明

```json
{
  "existing_node_refs": [
    {
      "id": "ref_begin_play",
      "node_title": "Event BeginPlay"
    },
    {
      "id": "ref_existing_node",
      "node_guid": "A1B2C3D4E5F6789012345678ABCDEF01"
    }
  ],
  "nodes": [...],
  "links": [
    { "from_id": "ref_begin_play", "from_pin": "then", "to_id": "new_node_1", "to_pin": "execute" }
  ]
}
```

| 字段 | 说明 |
|------|------|
| `id` | 引用 ID，在 `links` 中使用 |
| `node_guid` | 精确匹配 UE 节点的 `NodeGuid`（优先级最高）。可从导出 JSON 的 `node_guid` 字段获取 |
| `node_title` | 子串匹配节点标题（`GetNodeTitle(ListView)` 的结果）。匹配第一个命中的节点 |

> **注意**：`__function_entry__` 和 `__function_result__` 已自动注入，无需在 `existing_node_refs` 中声明。

> **注意**：如 JSON `nodes` 数组中包含 `id` 为 `__function_entry__` / `__function_result__` 或 `type` 为 `K2Node_FunctionEntry` / `K2Node_FunctionResult` 的节点，导入时会静默跳过（不报错、不生成），连线通过图表中已有节点自动恢复。

---

## 当前插件限制

> **完整已知 Bug 与限制清单**请参见 [KnownBugs.md](KnownBugs.md)。
- 当前生成端稳定支持 `CallFunction`、成员/本地变量 `Get/Set`、标准宏（ForLoop 全系、FlipFlop、DoOnce、DoN、Gate、WhileLoop、IsValid）、`Branch`、`Sequence`、`CustomEvent`、引擎事件（`Event`）、委托操作（`CallDelegate`、`AddDelegate`、`RemoveDelegate`、`ClearDelegate`、`AssignDelegate`、`CreateDelegate`）、容器构造（`MakeArray`、`MakeSet`、`MakeMap`）、结构体操作（`MakeStruct`、`BreakStruct`）。
- **v2.2 高级节点**：新增 `K2Node_Self`、`K2Node_DynamicCast`、`K2Node_SpawnActorFromClass`、`K2Node_FormatText`、`K2Node_GetArrayItem`、`K2Node_Timeline`。
- **v2.3 全覆盖收尾**：新增 `K2Node_Knot`（布线转接）、`UEdGraphNode_Comment`（注释框）、`K2Node_Literal`（对象引用常量）、`K2Node_GetEnumeratorName`、`K2Node_GetEnumeratorNameAsString`、`K2Node_ComponentBoundEvent`（组件绑定事件）。
- **v2.9 Enhanced Input / 数学运算 / 流程控制**：新增 `K2Node_EnhancedInputAction`、`K2Node_PromotableOperator`、`K2Node_CommutativeAssociativeBinaryOperator`、`K2Node_SwitchInteger` / `SwitchString` / `SwitchName` / `SwitchEnum`、`K2Node_Select`。增量导入支持（`existing_node_refs`、`node_guid`）、虚拟节点容错、ReconstructNode 时序修正。
- **v2.10 DynamicCast 引脚别名修复**：`valid`、`invalid`、`cast_result`、`success` 等别名现在正确映射到 DynamicCast 的实际引脚。
- **v2.10 编辑器生命周期工具**：新增 `blueprint_close_editor`（关闭编辑器）、`blueprint_build_project`（编译项目）、`blueprint_open_editor`（启动编辑器并等待 Bridge 可用）。用于替代 LiveCoding，避免热编译导致的重复类问题。
- **AgentImportGraph 语义导入**：`blueprint_import_agent_graph` 接收 `BlueprintHelper.AgentImportGraph` 对象，用于 Agent 以 `event`、`call`、`get`、`set`、`branch` 等高层节点表达新增蓝图逻辑。该协议不是 raw JSON 回放格式，不得传给 `blueprint_import_json_to_graph`。
- **v2.0 蓝图级操作**：支持 `add_member_variable`、`add_function_graph`、`add_event_dispatcher` 三种 blueprint_operations。
- **v2.1 蓝图级操作扩展**：新增 `add_macro_graph`、`remove_graph`、`remove_member_variable`。
- **v2.1 多图表导入**：`graphs` 数组支持向不同图表（EventGraph / 函数 / 宏）分别生成节点和连线。
- **v2.1 蓝图完整导出**：`ExportBlueprintToJson` 输出可直接重建的完整 JSON。
- **v2.9 增量引用**：`existing_node_refs` 支持引用图中已有节点（按 `node_guid` 或 `node_title` 匹配）。
- **v2.9 虚拟节点容错**：导入时静默跳过 `__function_entry__` / `__function_result__` 节点声明。
- **v2.9 导出 GUID**：导出 JSON 的每个节点包含 `node_guid` 字段，便于增量导入精确引用。
- **图表作用域约束**：
  - `declarations.local_variables` 和 `scope: "local"` 仅在**函数图/宏图**中有效，EventGraph 中使用会直接失败。
  - 成员变量（`scope: "member"`）可通过 `add_member_variable` 操作自动创建，也可手动在蓝图中创建。
  - 事件节点（`K2Node_Event`、`K2Node_CustomEvent`）只能在 EventGraph 中使用。
- 事件分发器可通过 `add_event_dispatcher` 操作自动创建，创建后委托节点可立即引用。
- 引擎事件节点仅支持蓝图父类中已有的可重写事件。
- 未匹配的函数会进入插件的待映射列表；变量或宏错误会进入未解析列表并附带原因。
- 复杂类型默认值（对象、结构体、数组等）仍不保证完整恢复。
- 多蓝图窗口同时打开时，请确保目标蓝图图表处于激活状态。

