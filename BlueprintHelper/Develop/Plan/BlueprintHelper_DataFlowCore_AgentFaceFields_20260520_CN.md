# BlueprintHelper 第一批数据流核心 AgentFace 字段记录

日期：2026-05-20

## 目标

记录第一批数据流核心语义操作的 AgentFace 输入字段设计。

本批字段遵循以下原则：

1. AgentFace 只表达语义意图，不暴露 UE 节点名。
2. UE 节点选择、类型推断、函数匹配、pin 连接由 Semantic Resolver、Pattern Registry、Fragment Builder、Composer/Linker 负责。
3. 不引入 `make` / `break` 作为 AgentFace canonical kind，统一使用 `construct` / `deconstruct`。
4. 字段保持简洁，复杂候选发现通过 preview / resolver 返回，再由后续 TaskSpec 继续执行。
5. 本批字段仅属于 `BlueprintLogicSpec` / Graph body 写入语义，不抢占 AssetFactory、Signature、Component、WidgetTree、DataTable、ObjectProperty、ClassSettings 等工具簇职责。

## 第一批 kind

```text
get
set
get_property
set_property
op
construct
deconstruct
select
```

## 通用字段

```text
kind          语义操作
target        被操作对象、符号或表达式
value         写入值或输入值
property      属性名或属性路径
type          期望构造/拆解的类型
fields        字段映射
operator      运算符
args          运算输入
condition     select 条件
then          select 条件为真时的值
else          select 条件为假时的值
target_kind   可选消歧字段
```

`value`、`args`、`fields` 内允许直接写 literal，编译器自动包装成 literal expression。

## get

读取当前作用域里的命名符号。

命名符号可以是变量、局部变量、参数、组件引用、`let` 临时值。

```json
{
  "kind": "get",
  "target": "DoorPanel"
}
```

可选消歧：

```json
{
  "kind": "get",
  "target": "DoorPanel",
  "target_kind": "component"
}
```

`target_kind` 可选值：

```text
auto
variable
local
parameter
component
```

默认值为 `auto`，由 resolver 根据 Blueprint、Graph、Scope 推断。

## set

写入当前作用域里的命名符号。

用于变量、局部变量、输出变量等。

```json
{
  "kind": "set",
  "target": "bDoorOpen",
  "value": true
}
```

表达式值示例：

```json
{
  "kind": "set",
  "target": "OpenAngle",
  "value": {
    "kind": "op",
    "operator": "*",
    "args": [
      { "kind": "get", "target": "MaxOpenAngle" },
      0.5
    ]
  }
}
```

## get_property

读取对象或结构体上的属性。

```json
{
  "kind": "get_property",
  "target": {
    "kind": "get",
    "target": "DoorPanel"
  },
  "property": "RelativeRotation"
}
```

允许属性路径：

```json
{
  "kind": "get_property",
  "target": {
    "kind": "get",
    "target": "DoorPanel"
  },
  "property": "RelativeRotation.Yaw"
}
```

单字段读取优先使用 `get_property`，不使用 `deconstruct`。

## set_property

写入对象或结构体上的属性。

```json
{
  "kind": "set_property",
  "target": {
    "kind": "get",
    "target": "DoorPanel"
  },
  "property": "RelativeRotation",
  "value": {
    "kind": "construct",
    "type": "Rotator",
    "fields": {
      "Pitch": 0,
      "Yaw": 90,
      "Roll": 0
    }
  }
}
```

## op

通用运算表达式。

AgentFace 不暴露 UE 函数名，resolver 负责将 `>`、`==`、`+`、`and`、`not` 等操作映射到合适的 UE 函数或节点。

```json
{
  "kind": "op",
  "operator": ">",
  "args": [
    { "kind": "get", "target": "Health" },
    0
  ]
}
```

布尔组合示例：

```json
{
  "kind": "op",
  "operator": "and",
  "args": [
    { "kind": "get", "target": "bHasKey" },
    { "kind": "get", "target": "bDoorLocked" }
  ]
}
```

## construct

构造复合值。

用于 `Vector`、`Rotator`、`Transform`、自定义 Struct、Array、Map 等。

`construct` 采用与 `deconstruct` 对称的两阶段规则，避免 Agent 在不知道 Struct 或类型内部字段时猜字段名。

### 查询模式

`fields` 为空或缺失时，只做类型解析和字段发现，不写资产。

```json
{
  "kind": "construct",
  "type": "Rotator"
}
```

CLI preview / resolver 返回：

```json
{
  "status": "needs_construct_fields",
  "target_type": "Rotator",
  "available_fields": [
    {
      "name": "Pitch",
      "type": "float",
      "default": 0
    },
    {
      "name": "Yaw",
      "type": "float",
      "default": 0
    },
    {
      "name": "Roll",
      "type": "float",
      "default": 0
    }
  ]
}
```

如果 `type` 省略，但消费者上下文能推断类型，resolver 可以使用消费者 pin / property / assignment target 的类型进行字段发现。

```json
{
  "kind": "set_property",
  "target": {
    "kind": "get",
    "target": "DoorPanel"
  },
  "property": "RelativeRotation",
  "value": {
    "kind": "construct"
  }
}
```

上述场景可以从 `RelativeRotation` 推断 `Rotator`。

如果既没有 `type`，也无法从消费者上下文推断目标类型，preview 阶段阻断：

```json
{
  "status": "needs_construct_type",
  "message": "construct requires type or typed consumer context"
}
```

### 写入模式

`fields` 非空时，才生成真实 construct fragment。

```json
{
  "kind": "construct",
  "type": "Vector",
  "fields": {
    "X": 0,
    "Y": 0,
    "Z": 100
  }
}
```

自定义 Struct 示例：

```json
{
  "kind": "construct",
  "type": "FWeaponStats",
  "fields": {
    "Damage": 25,
    "Range": 1200,
    "bCritical": true
  }
}
```

`construct` 表达构造语义，UE 层可以按上下文选择 `Make Vector`、`Make Struct`、默认 pin、split pin 或其他实现方式。

规则：

1. `fields` 为空或缺失：只发现可构造字段，不写资产。
2. `fields` 非空：生成真实 construct fragment。
3. `type` 可省略，但必须能从消费者上下文推断。
4. 字段名错误：preview 阶段阻断，返回合法字段候选。
5. 缺失字段默认使用 resolver 确认的类型默认值；如果默认值无法安全确认，preview 阶段要求补齐字段。

## deconstruct

拆解复合值。

`deconstruct` 采用两阶段规则，避免 Agent 在不知道 Struct 内部字段时猜字段名。

### 查询模式

`fields` 为空时，只做类型解析和字段发现，不写资产。

```json
{
  "kind": "deconstruct",
  "target": {
    "kind": "get",
    "target": "HitResult"
  }
}
```

CLI preview / resolver 返回：

```json
{
  "status": "needs_deconstruct_fields",
  "target_type": "HitResult",
  "available_fields": [
    {
      "name": "ImpactPoint",
      "type": "Vector"
    },
    {
      "name": "ImpactNormal",
      "type": "Vector"
    },
    {
      "name": "Actor",
      "type": "Actor"
    }
  ]
}
```

### 写入模式

`fields` 非空时，才生成真实 deconstruct fragment。

```json
{
  "kind": "deconstruct",
  "target": {
    "kind": "get",
    "target": "HitResult"
  },
  "fields": {
    "ImpactPoint": "HitPoint",
    "ImpactNormal": "HitNormal"
  }
}
```

含义：

```text
HitPoint = HitResult.ImpactPoint
HitNormal = HitResult.ImpactNormal
```

规则：

1. `fields` 为空：只发现字段，不写资产。
2. `fields` 非空：生成真实 deconstruct fragment。
3. `fields` 写错：preview 阶段阻断，返回合法字段候选。
4. 单字段读取：使用 `get_property`。
5. 多字段拆解并复用：使用两阶段 `deconstruct`。

## select

数据流条件选择，不控制执行流。

对应 UE `Select` 节点或等价数据 fragment。

```json
{
  "kind": "select",
  "condition": {
    "kind": "get",
    "target": "bDoorOpen"
  },
  "then": 90,
  "else": 0
}
```

结合 `set_property` 和 `construct`：

```json
{
  "kind": "set_property",
  "target": {
    "kind": "get",
    "target": "DoorPanel"
  },
  "property": "RelativeRotation",
  "value": {
    "kind": "construct",
    "type": "Rotator",
    "fields": {
      "Pitch": 0,
      "Yaw": {
        "kind": "select",
        "condition": {
          "kind": "get",
          "target": "bDoorOpen"
        },
        "then": 90,
        "else": 0
      },
      "Roll": 0
    }
  }
}
```

## 决策记录

1. `construct/deconstruct` 是 AgentFace / SemanticIR canonical kind。
2. 不提供 `make/break` alias，避免语义层和 UE 节点实现层混淆。
3. `construct/deconstruct` 都不要求 Agent 猜完整字段；字段发现通过 resolver 查询模式返回。
4. 第一批字段只覆盖数据流核心，不覆盖 exec flow、event、delegate、spawn、cast、timeline、timer 等后续类别。
5. `get_property/set_property` 只表示 Graph body 内对象表达式或结构体表达式的属性读写节点；不替代资产级 `edit_object_properties`、组件模板属性、WidgetTree 设计时属性、Class defaults 或 DataAsset 字段编辑。
6. 如果目标变更属于资产、签名、组件树、WidgetTree、DataTable、DataAsset、ClassSettings 生命周期，应继续走对应工具簇，而不是扩展本批数据流字段。

## 后续实现入口

后续实现应按以下链路接入：

```text
AgentFace TaskSpec
-> BlueprintLogicSpec
-> SemanticIR
-> Semantic Resolver
-> Pattern Registry
-> NodeFragment Builder
-> Fragment DAG
-> Graph Composer / Linker
-> UE Graph Mutator
```

不要在 AgentFace schema 或 compiler 中引入 UE 节点名，也不要为了单个 UE 节点直接扩展 AgentFace 字段。
