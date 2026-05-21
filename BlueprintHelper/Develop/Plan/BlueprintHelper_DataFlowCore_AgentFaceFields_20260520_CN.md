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
AgentFace schema/docs
-> TS/Python compiler
-> SemanticIR parser
-> Resolver
-> Pattern Registry
-> NodeFragment Builder
-> FragmentDAG
-> Composer/Linker
-> UE Mutator
-> Review/Debug
-> ReadContext/LogicFlow
```

不要在 AgentFace schema 或 compiler 中引入 UE 节点名，也不要为了单个 UE 节点直接扩展 AgentFace 字段。

## 新增 Graph body 能力标准接入流程

新增 Graph body 能力时必须按以下流程接入：

1. AgentFace schema / docs 定义 canonical semantic shape。
2. TS / Python compiler 只保留 canonical shape，不做旧字段 normalization。
3. SemanticIR parser 解析 `kind` 和字段。
4. Semantic Resolver 解析 scope、symbol、target、type、candidate。
5. Pattern Registry 根据 semantic kind 和 typed context 选择 builder。
6. NodeFragment Builder 生成 fragment。
7. FragmentDAG Builder 建立 data / exec edge。
8. Graph Composer / Linker 消费 edge 并连接 pin。
9. UE Graph Mutator 创建或修改 `UK2Node`。
10. Review evidence / DebugBundle 消费同一份 semantic + fragment evidence。
11. ReadContext / LogicFlow 输出同一套 canonical semantic 信息。

不允许：

1. 新增 AgentFace 字段后直接在 compiler 中生成 UE 节点名。
2. 新增能力时包装旧 `NodeHandler` 作为 Pattern。
3. 使用 `if kind == X then NewObject<UK2Node_X>` 绕过 Pattern Registry / NodeFragment。
4. 在解析层接受旧字段作为 alias。
5. 为通过测试保留 hidden fallback。
## Canonical Graph body statement/expression path

```text
AgentFace schema/docs
-> TS/Python compiler
-> SemanticIR parser
-> Resolver
-> Pattern Registry
-> NodeFragment Builder
-> FragmentDAG
-> Composer/Linker
-> UE Mutator
-> Review/Debug
-> ReadContext/LogicFlow
```

旧 NodeHandler / parsed-node fallback 不允许保留，也不是 deprecated compatibility。旧 Graph body shapes `call_function` / `set_member_variable` / `ref` / `compare` / `make_struct` 必须报 unsupported kind，不允许 compiler normalization、alias、deprecated mapping 或 hidden fallback。

## 实现状态（Slice C 文档同步，2026-05-21）

本节只同步文档，不重新读取或验证代码实现；因此除文档同步本身外，不把代码切片 A/B 标记为完成。

[x] 已完成：文档明确旧 NodeHandler / parsed-node fallback 不允许保留，也不是 deprecated compatibility。
[x] 已完成：文档明确 Graph body statement/expression canonical 路径为 AgentFace schema/docs -> TS/Python compiler -> SemanticIR parser -> Resolver -> Pattern Registry -> NodeFragment Builder -> FragmentDAG -> Composer/Linker -> UE Mutator -> Review/Debug -> ReadContext/LogicFlow。
[x] 已完成：文档明确旧 Graph body shapes `call_function` / `set_member_variable` / `ref` / `compare` / `make_struct` 必须作为 unsupported kind 报错，不允许 compiler normalization、alias、deprecated mapping 或 hidden fallback。
[ ] 未完成/待验证：TS compiler canonical allowlist、old-shape rejection tests、fixture cleanup 的当前代码状态未在本次文档同步中验证。
[ ] 未完成/待验证：Python compiler parity 与 interface integration 是否只生成 `call` / `target` 未在本次文档同步中验证。
[ ] 未完成/待验证：UE SemanticIR parser、Resolver、Pattern Registry、NodeFragment Builder、FragmentDAG、Composer/Linker、Mutator 是否已完整接入 first-batch kinds 未在本次文档同步中验证。
[ ] 未完成/待验证：construct/deconstruct 两阶段 preview 是否返回 candidate field lists 且不写资产未在本次文档同步中验证。
[ ] 未完成/待验证：Review/Debug evidence 与 ReadContext/LogicFlow 是否消费同一 canonical semantic/fragment evidence 未在本次文档同步中验证。
[ ] 未完成/待验证：UE compile、TS tests、Python tests、editor preview/execute smoke 和 readback 未在本次文档同步中运行。

## Spawner-Oriented AgentFace Intent / call 分层范式

`call` 是 AgentFace 的 compact intent 之一，不是所有 UE action 的底层簇。底层簇按 UE `NodeSpawner` 家族划分，而不是按自然语言语义划分。

统一链路：

```text
AgentFace compact intent
-> Semantic Resolver / typed resolver
-> SpawnerClusterResolver
-> BlueprintActionResolutionCore
-> selected UBlueprintNodeSpawner or derived spawner
-> cluster-specific NodeFragment adapter
```

底层四簇：

```text
FunctionActionCluster
FieldVariableActionCluster
EventDelegateActionCluster
GenericAssetStructControlActionCluster
```

AgentFace intent 到底层簇的路由：

| AgentFace intent | 底层簇 | 说明 |
|---|---|---|
| `call` | FunctionActionCluster | Agent 明确请求 callable/action |
| `op` | FunctionActionCluster | operator intent 转 typed function/action query |
| `get` | FieldVariableActionCluster | 读变量、局部变量、参数、组件引用、临时值 |
| `set` | FieldVariableActionCluster | 写变量、局部变量、输出变量 |
| `get_property` | FieldVariableActionCluster；必要时转 GenericAssetStructControlActionCluster | 简单字段访问在 Field 簇；struct path 组合用 Generic/Struct 能力 |
| `set_property` | FieldVariableActionCluster；必要时转 GenericAssetStructControlActionCluster | 同上 |
| `construct` | GenericAssetStructControlActionCluster | value/struct/container 构造；不覆盖 object lifecycle create |
| `deconstruct` | GenericAssetStructControlActionCluster | struct/value 拆解和字段发现 |
| `select` | GenericAssetStructControlActionCluster | 数据流值选择 |
| `control` | GenericAssetStructControlActionCluster | exec flow control |
| `event` | EventDelegateActionCluster | event entry / custom event |
| `component_bound_event` | EventDelegateActionCluster | component-bound event |
| `bind` | EventDelegateActionCluster | delegate binding |
| `create` | GenericAssetStructControlActionCluster | Actor/Object/Widget 等实例创建 |
| `convert` | FunctionActionCluster 或 GenericAssetStructControlActionCluster | function/type-promotion cast 优先 Function；generic cast action 走 Generic |
| `schedule` | FunctionActionCluster 或 GenericAssetStructControlActionCluster | function/timer/latent 优先 Function；generic async action 走 Generic |

禁止把以下 UE 细节推回 AgentFace：

```text
Greater_IntInt
EqualEqual_ObjectObject
MakeVector
BreakVector
KismetMathLibrary.*
UK2Node_*
UBlueprintNodeSpawner class names
```

判定标准：

- AgentFace 顶层字段保持 compact semantic intent。
- 底层职责按 NodeSpawner 家族归属，不按自然语义归属。
- 如果 UE 右键菜单能表达该 node/action，必须优先走 `BlueprintActionResolutionCore + UBlueprintNodeSpawner`。
- 如果 UE ActionDatabase 无法表达，或该 intent 是多节点语义组合，才允许专用 FragmentBuilder，并必须记录原因。

## Spawner-Oriented Cluster 口径

本文件中的 DataFlowCore 字段只定义 AgentFace 语义 intent，不定义 UE 底层工具簇边界。

当前统一口径：

```text
AgentFace kind
-> SpawnerClusterResolver
-> BlueprintActionResolutionCore
-> selected UBlueprintNodeSpawner or derived spawner
-> NodeFragment adapter
-> FragmentDAG
-> Composer / Linker / Mutator
```

因此，`call` 不再被视为 UE action resolution 的总入口；`call` 只是 `FunctionActionCluster` 的一个 intent。`get` / `set` / `op` / `construct` / `deconstruct` / `select` 等 intent 必须复用同一套 action resolution 基础设施，而不是复用 `call` 的局部 handler。

首批字段到 Spawner-Oriented Cluster 的映射：

| AgentFace kind | 默认 Spawner-Oriented Cluster | 说明 |
|---|---|---|
| `call` | `FunctionActionCluster` | 普通 callable/action |
| `op` | `FunctionActionCluster` | operator 通过 typed operands 约束 function/type-promotion spawner |
| `get` | `FieldVariableActionCluster` | symbol / variable / field / component ref 读取 |
| `set` | `FieldVariableActionCluster` | symbol / variable / field 写入 |
| `get_property` | `FieldVariableActionCluster` | property path 读取；复杂 path 可组合 struct/generic fragment |
| `set_property` | `FieldVariableActionCluster` | property path 写入；复杂 path 可组合 struct/generic fragment |
| `construct` | `GenericAssetStructControlActionCluster` | Make Struct / container / generic construct action |
| `deconstruct` | `GenericAssetStructControlActionCluster` | Break Struct / generic deconstruct action |
| `select` | `GenericAssetStructControlActionCluster` | Select 类 generic data-flow action |

后续/相邻 intent 的口径：

| AgentFace kind | 默认 Spawner-Oriented Cluster |
|---|---|
| `event` | `EventDelegateActionCluster` |
| `component_bound_event` | `EventDelegateActionCluster` |
| `bind` / `unbind` / `assign` | `EventDelegateActionCluster` |
| `control` | `GenericAssetStructControlActionCluster` |
| `create` | `GenericAssetStructControlActionCluster` |
| `convert` | `FunctionActionCluster` 或 `GenericAssetStructControlActionCluster`，由 resolver 根据 type/action 判断 |
| `schedule` | `FunctionActionCluster` 或 `GenericAssetStructControlActionCluster`，由 resolver 根据 timer/latent/async action 判断 |

已废弃表述：

1. `optional UE action or function resolver reuse`。
2. “数据流 / 执行流 / 普通调用 / 实例类型绑定调度”作为底层四簇。
3. `call_function` 作为 action resolution 的特殊长期边界。
4. `FindFunctionByName()` 或旧 node fallback 作为解析兜底。

正确表述：凡是 UE 右键菜单可以表达的 node/action 选择，必须通过 `BlueprintActionResolutionCore + UBlueprintNodeSpawner` 或派生 spawner 解析；AgentFace 字段只提供 semantic intent 和最小必要约束。