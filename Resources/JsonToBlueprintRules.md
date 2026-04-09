# BlueprintHelper Json -> 蓝图规则

## 目标
该规则用于约束外部大模型输出 **BlueprintHelper 可直接解析的 JSON**，再由插件在当前蓝图图表中生成节点。

当前版本支持以下闭环：
- Blueprint T3D -> JSON
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

> 当前仍不保证完整支持 `Timeline`、`Select`、`Switch`、自定义宏库等更复杂节点。

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

### Schema 2.0（蓝图级操作 + 节点操作）
```json
{
  "version": "2.0",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "blueprint_operations": [],
  "declarations": {
    "local_variables": []
  },
  "nodes": [],
  "links": []
}
```

### 字段说明
- `version`: 当前版本 `"2.0"`，1.x 版的 JSON 也兼容
- `schema`: 固定填 `"BlueprintHelper.JsonToBlueprint"`
- `blueprint_operations`: **v2.0 新增**，蓝图级操作数组（可选），在节点生成之前执行
- `declarations`: 声明区，可选，目前用于本地变量声明
- `nodes`: 节点数组
- `links`: 节点连线数组

### 执行顺序
1. `blueprint_operations`（创建变量、函数、事件分发器等蓝图资产级操作）
2. `declarations.local_variables`（确保本地变量存在）
3. `nodes`（生成节点）
4. `links`（连线）

此顺序保证引用完整性：节点可以引用刚由 blueprint_operations 创建的变量或函数。

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
3. schema 固定为 BlueprintHelper.JsonToBlueprint，version 固定为 "2.0"。
4. `blueprint_operations` 在节点之前执行，可创建成员变量（add_member_variable）、函数图（add_function_graph）、事件分发器（add_event_dispatcher）。
5. `type` 可使用 `K2Node_CallFunction`、`K2Node_VariableGet`、`K2Node_VariableSet`、`K2Node_MacroInstance`、`K2Node_IfThenElse`、`K2Node_ExecutionSequence`、`K2Node_CustomEvent`、`K2Node_Event`、`K2Node_CallDelegate`、`K2Node_AddDelegate`、`K2Node_RemoveDelegate`、`K2Node_ClearDelegate`、`K2Node_AssignDelegate`、`K2Node_CreateDelegate`、`K2Node_MakeArray`、`K2Node_MakeSet`、`K2Node_MakeMap`、`K2Node_MakeStruct`、`K2Node_BreakStruct`。
6. `function_name` 优先使用 Unreal 可蓝图调用函数的原生函数名。
7. 函数图中的本地变量可写入 `declarations.local_variables`，或在变量节点上写 `ensure_exists=true`。EventGraph 中只能使用成员变量（`scope: "member"`），不得使用 `local_variables`。
8. 若需要在 EventGraph 中使用成员变量或事件分发器，先在 `blueprint_operations` 中创建。
9. `inputs` 仅填写未连线的默认值；变量写入值可用 `value`；ForLoop 可用 `first_index/last_index`。
10. `links` 使用 from_id / from_pin / to_id / to_pin。
11. 若无法稳定确认函数、变量或引脚名，宁可省略，不要猜测。
```

---

## 当前插件限制
- 当前生成端稳定支持 `CallFunction`、成员/本地变量 `Get/Set`、标准宏（ForLoop 全系、FlipFlop、DoOnce、DoN、Gate、WhileLoop、IsValid）、`Branch`、`Sequence`、`CustomEvent`、引擎事件（`Event`）、委托操作（`CallDelegate`、`AddDelegate`、`RemoveDelegate`、`ClearDelegate`、`AssignDelegate`、`CreateDelegate`）、容器构造（`MakeArray`、`MakeSet`、`MakeMap`）、结构体操作（`MakeStruct`、`BreakStruct`）。
- **v2.0 蓝图级操作**：支持 `add_member_variable`、`add_function_graph`、`add_event_dispatcher` 三种 blueprint_operations。
- **图表作用域约束**：
  - `declarations.local_variables` 和 `scope: "local"` 仅在**函数图**中有效，EventGraph 中使用会直接失败。
  - 成员变量（`scope: "member"`）可通过 `add_member_variable` 操作自动创建，也可手动在蓝图中创建。
  - 事件节点（`K2Node_Event`、`K2Node_CustomEvent`）只能在 EventGraph 中使用。
- 事件分发器可通过 `add_event_dispatcher` 操作自动创建，创建后委托节点可立即引用。
- 引擎事件节点仅支持蓝图父类中已有的可重写事件。
- 未匹配的函数会进入插件的待映射列表；变量或宏错误会进入未解析列表并附带原因。
- 复杂类型默认值（对象、结构体、数组等）仍不保证完整恢复。
- 多蓝图窗口同时打开时，请确保目标蓝图图表处于激活状态。

