# BlueprintHelper GraphStatement Framework 总设计文档

## ActionResolution 一级分发规则（2026-05-21 更新）

- `ActionResolutionCore` 的一级请求类型必须是 `EBlueprintHelperSpawnerClusterKind`。
- AgentFace 的 `call/get/set/get_property/set_property/op/construct/deconstruct/select/control` 等语义不再作为 ActionResolution 一级请求类型存在。
- 这些语义只能进入 `FBlueprintHelperActionSemanticConstraints`，作为所选 UE NodeSpawner family cluster 内部的解析约束。
- `SpawnerClusterResolver` 只按 `Request.ClusterKind` 分发，不允许根据 semantic kind 再做一级簇选择。
- GraphStatement / Semantic Resolver 负责把 AgentFace semantic intent 映射为：`SpawnerClusterKind + SemanticConstraints`。
- 新增能力时必须先判断 UE NodeSpawner family 边界，再扩展对应 cluster；不得重新引入 `ActionIntent -> cluster` 的一级分发模型。

日期：2026-05-21
适用范围：AgentFace Graph body / BlueprintLogicSpec / SemanticIR / FragmentDAG / UE GraphWrite

## 1. 总目标

GraphStatement Framework 的目标不是让 AgentFace 直接描述 UE 节点，而是在尽可能压缩输入字段的前提下，利用 TaskSpec 提供的 Blueprint、Graph、Scope、typed pin、target object、metadata、search mode、category priority、ambiguity policy 等上下文，复用 UE 编辑器右键菜单背后的 ActionDatabase / BlueprintActionFilter / NodeSpawner 体系，达到 UE 右键菜单大部分节点选择能力，同时保持 AgentFace 顶层语义稳定、低 token、低错误率。

目标链路：

```text
AgentFace TaskSpec
-> BlueprintLogicSpec / Graph body canonical semantic intent
-> TS / Python compiler
-> UE SemanticIR parser
-> Semantic Resolver / typed resolver
-> SpawnerClusterResolver
-> BlueprintActionResolutionCore
-> selected UBlueprintNodeSpawner or derived spawner
-> NodeFragment adapter
-> FragmentDAG
-> Graph Composer / Linker
-> UE Mutator
-> Review / DebugBundle / ReadContext / LogicFlow
```

## 2. 硬性规则：Spawner-Oriented Clusters

后续 Graph body 能力按 UE NodeSpawner 家族组织簇，而不是按自然语言语义组织底层职责。

AgentFace `kind` 仍然是语义 intent，用于压缩字段；底层簇由 `SpawnerClusterResolver` 根据 intent、target、type、graph context、pin context、metadata constraints 选择。

```text
AgentFace semantic intent
-> SpawnerClusterResolver
-> BlueprintActionResolutionCore
-> UBlueprintNodeSpawner candidate
-> semantic-specific NodeFragment adapter
```

凡是 UE 编辑器右键菜单可以表达的 node/action 选择，GraphWrite 必须优先经过 `BlueprintActionResolutionCore`，并最终选择 `UBlueprintNodeSpawner` 或其派生 spawner。

不允许新增能力绕开该链路，直接使用全局函数名查找、硬编码函数名、硬编码 `UK2Node_*`，或恢复旧 `NodeHandler` / parsed-node fallback。

允许直接专用 FragmentBuilder 的情况只有两类：

1. UE ActionDatabase / NodeSpawner 无法表达该 semantic operation。
2. 该 semantic operation 本质是多个 UE node 的组合，需要一个语义级 builder 编排多个 NodeFragment。

出现上述例外时，必须在 resolver / builder 边界写清楚原因，且仍要复用统一 typed target、typed pin、candidate reporting、Review/Debug evidence 模型。

## 3. UE 5.6 NodeSpawner 清单

| Spawner 类 | 角色 | 主要覆盖能力 |
|---|---|---|
| `UBlueprintNodeSpawner` | 基类 / generic node spawner | 通用节点创建；许多 K2Node 通过 `Create(NodeClass)` 注册菜单 action |
| `UBlueprintFieldNodeSpawner` | 字段节点公共基类 | 字段相关节点，关联 `FFieldVariant` |
| `UBlueprintFunctionNodeSpawner` | 函数节点 spawner | callable function / Kismet library / Blueprint function |
| `UBlueprintVariableNodeSpawner` | 变量节点 spawner | 成员变量、本地变量、参数 get/set |
| `UBlueprintEventNodeSpawner` | 事件节点 spawner | 普通事件、自定义事件、函数事件入口 |
| `UBlueprintBoundEventNodeSpawner` | 绑定事件 spawner | delegate/component bound event |
| `UAnimNotifyEventNodeSpawner` | 动画通知事件 spawner | Animation Notify 事件 |
| `UBlueprintDelegateNodeSpawner` | delegate 节点 spawner | multicast delegate bind/assign/call/remove/clear |
| `UBlueprintComponentNodeSpawner` | 组件节点 spawner | component reference / component action |
| `UBlueprintAssetNodeSpawner` | 资产节点 spawner | asset-backed node creation |
| `UBlueprintBoundNodeSpawner` | 绑定对象上下文 spawner | bound generic node action |

补充：UE 右键菜单还会通过 `UBlueprintNodeSpawner::Create(NodeClass)`、`K2Node::GetMenuActions()`、`FBlueprintActionDatabaseRegistrar` 的 struct / enum / function spawner delegate、`UBlueprintTypePromotion` operator spawner map 提供更多 action。

## 4. 四大 Spawner-Oriented Clusters

### 4.1 FunctionActionCluster

底层：

```text
UBlueprintFunctionNodeSpawner
UBlueprintTypePromotion operator spawner
function/action registrar delegates
```

覆盖 intent：

```text
call
op
convert_function
schedule_function
latent_or_async_function
```

职责：

- 普通函数调用。
- Kismet library / Blueprint function 调用。
- operator 通过 typed operands 转成 function/action query。
- 部分 cast/convert、timer/latent/async function action。

### 4.2 FieldVariableActionCluster

底层：

```text
UBlueprintFieldNodeSpawner
UBlueprintVariableNodeSpawner
UBlueprintComponentNodeSpawner
```

覆盖 intent：

```text
get
set
get_property
set_property
component_ref
field_access
```

职责：

- 成员变量、本地变量、参数读写。
- 组件引用。
- 字段访问。
- 简单 property path 读写。
- 复杂 property path 可组合 Struct/Generic 簇的 fragment。

### 4.3 EventDelegateActionCluster

底层：

```text
UBlueprintEventNodeSpawner
UBlueprintBoundEventNodeSpawner
UAnimNotifyEventNodeSpawner
UBlueprintDelegateNodeSpawner
UBlueprintBoundNodeSpawner
```

覆盖 intent：

```text
event
component_bound_event
anim_notify_event
bind
unbind
assign
delegate_call
delegate_clear
```

职责：

- 事件入口。
- 组件绑定事件。
- 动画通知事件。
- delegate bind/assign/call/remove/clear。
- 绑定对象上下文 action。

### 4.4 GenericAssetStructControlActionCluster

底层：

```text
UBlueprintNodeSpawner
UBlueprintAssetNodeSpawner
struct / enum / generic registrar delegates
```

覆盖 intent：

```text
construct
deconstruct
select
control
create
asset_action
container_action
```

职责：

- Branch / Sequence / Switch / Return / Loop 等控制流。
- Select。
- Make/Break Struct。
- Make Array / Map / Set 等容器构造。
- SpawnActor / CreateWidget / ConstructObject 等 create 类 action。
- 资产驱动 node action。
- UE ActionDatabase 无法细分到专用 spawner 家族的 generic K2Node action。

## 5. AgentFace intent 与簇的关系

AgentFace `kind` 不再定义底层簇边界，只作为 intent 输入。

| AgentFace intent | 默认簇 | 说明 |
|---|---|---|
| `call` | FunctionActionCluster | Agent 明确请求 callable/action |
| `op` | FunctionActionCluster | operator 通过 typed constraints 选择 function/type-promotion spawner |
| `get` | FieldVariableActionCluster | 符号读取 |
| `set` | FieldVariableActionCluster | 符号写入 |
| `get_property` | FieldVariableActionCluster | 简单 property path；复杂 path 可组合 Struct/Generic |
| `set_property` | FieldVariableActionCluster | 简单 property write；复杂 path 可组合 Struct/Generic |
| `event` | EventDelegateActionCluster | 事件入口 |
| `component_bound_event` | EventDelegateActionCluster | 组件绑定事件 |
| `bind` | EventDelegateActionCluster | delegate 绑定 |
| `construct` | GenericAssetStructControlActionCluster | value/struct/container 构造 |
| `deconstruct` | GenericAssetStructControlActionCluster | value/struct/container 拆解 |
| `select` | GenericAssetStructControlActionCluster | 数据流选择 |
| `control` | GenericAssetStructControlActionCluster | 执行流节点 |
| `create` | GenericAssetStructControlActionCluster | 对象/Actor/Widget/asset-backed action 创建 |
| `convert` | FunctionActionCluster 或 GenericAssetStructControlActionCluster | 由 resolver 根据 cast/type-promotion/action 类型选择 |
| `schedule` | FunctionActionCluster 或 GenericAssetStructControlActionCluster | 由 resolver 根据 timer/latent/async action 类型选择 |

## 6. Preview-driven baseline

当前架构与 preview 强关联。Preview 是 AgentFace 语义压缩能够成立的关键流程：Agent 先提交低 token semantic TaskSpec，通过 preview 获取足够精简但可行动的反馈，再决定 execute 或补充下一轮 TaskSpec。

```text
Agent writes compact semantic TaskSpec
-> preview
-> SpawnerClusterResolver / BlueprintActionResolutionCore 执行候选解析
-> 返回三类响应之一
   1. actionable error diagnostics
   2. success with ambiguity / candidate_actions / candidate_functions
   3. minimal success
-> Agent 根据返回继续修正或 execute
```

`call_function` 专项历史缓存不应继续作为长期架构边界。缓存应迁移为通用 `action_resolution_cache`，覆盖所有 Spawner-Oriented Cluster 的候选解析。

## 7. 禁止路径

以下路径不允许作为新能力 fallback：

```text
FBlueprintNodeHandlerRegistry
FBlueprintOperationHandlerRegistry
parsed-node mutation fallback
FindFunctionByName global existence check
manual kind -> NewObject<UK2Node_X> shortcut, unless documented as ActionDatabase-unrepresentable
legacy AgentFace aliases: call_function, set_member_variable, ref, compare, make_struct
```

`FindFunctionByName()` 类全局函数查找不属于统一 action resolution。若当前 `op` 或其他 resolver 仍依赖它，应迁移到 `BlueprintActionResolutionCore` 后删除。

## 8. 成功标准

1. AgentFace 不暴露 UE node class、Kismet 函数后缀、owner class、wildcard pin 等低层细节。
2. UE 侧通过 TaskSpec 上下文尽量复用右键菜单级 action/node 选择能力。
3. 底层职责按 NodeSpawner 家族划分，避免按自然语义重复实现解析逻辑。
4. 不同 AgentFace intent 可以共享同一个 SpawnerCluster，但不会强迫 AgentFace 暴露 spawner 细节。
5. 新增能力不引入旧 fallback，不恢复旧 handler，不增加局部硬编码路径。