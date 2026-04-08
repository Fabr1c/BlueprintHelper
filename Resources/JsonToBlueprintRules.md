# BlueprintHelper Json -> 蓝图规则

## 目标
该规则用于约束外部大模型输出 **BlueprintHelper 可直接解析的 JSON**，再由插件在当前蓝图图表中生成节点。

当前版本支持以下闭环：
- Blueprint T3D -> JSON
- JSON -> `UK2Node_CallFunction`
- JSON -> `K2Node_VariableGet / K2Node_VariableSet`
- JSON -> 标准宏 `K2Node_MacroInstance(ForLoop)`
- JSON -> `declarations.local_variables` 本地变量声明/确保存在

> 当前仍不保证完整支持 `Branch`、`Sequence`、`Timeline`、`Custom Event`、自定义宏库等更复杂节点。

---

## 顶层结构
必须输出一个 JSON 对象，禁止输出解释文字，禁止输出 Markdown 代码块包裹以外的说明。

```json
{
  "version": "1.1",
  "schema": "BlueprintHelper.JsonToBlueprint",
  "declarations": {
    "local_variables": []
  },
  "nodes": [],
  "links": []
}
```

### 字段说明
- `version`: 固定填 `"1.0"`
- `schema`: 固定填 `"BlueprintHelper.JsonToBlueprint"`
- `declarations`: 声明区，可选，目前用于本地变量声明
- `nodes`: 节点数组
- `links`: 节点连线数组

---

## declarations 规则

### local_variables
用于声明当前函数图中需要存在的本地变量。

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
- `ensure_exists`: 本地变量缺失时自动创建
- `pin_type`: 本地变量自动创建时使用的类型
- `default_value`: 本地变量自动创建时使用的默认值

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
2. 若需要 `ForLoop`，只使用标准宏 `ForLoop`。
3. 本地变量建议同时写入 `declarations.local_variables` 或在变量节点上写 `ensure_exists=true`。
4. 若某节点无法稳定确认，宁可省略，也不要猜测错误函数名或错误 pin 名。
5. 节点坐标按从左到右递增，例如每个节点 X 增加 280，Y 保持同一行或少量偏移。

---

## 最小可用示例
```json
{
  "version": "1.1",
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
2. 顶层必须包含 version、schema、nodes、links。
3. schema 固定为 BlueprintHelper.JsonToBlueprint。
4. `type` 可使用 `K2Node_CallFunction`、`K2Node_VariableGet`、`K2Node_VariableSet`、`K2Node_MacroInstance`。
5. `function_name` 优先使用 Unreal 可蓝图调用函数的原生函数名。
6. 本地变量可写入 `declarations.local_variables`，或在变量节点上写 `ensure_exists=true`。
7. `inputs` 仅填写未连线的默认值；变量写入值可用 `value`；ForLoop 可用 `first_index/last_index`。
8. `links` 使用 from_id / from_pin / to_id / to_pin。
9. 若无法稳定确认函数、变量或引脚名，宁可省略，不要猜测。
```

---

## 当前插件限制
- 当前生成端稳定支持 `CallFunction`、成员/本地变量 `Get/Set`、标准宏 `ForLoop`。
- 本地变量仅保证在当前函数图中自动创建与使用。
- 未匹配的函数会进入插件的待映射列表；变量或宏错误会进入未解析列表并附带原因。
- 复杂类型默认值（对象、结构体、数组等）仍不保证完整恢复。
- 多蓝图窗口同时打开时，请确保目标蓝图图表处于激活状态。

