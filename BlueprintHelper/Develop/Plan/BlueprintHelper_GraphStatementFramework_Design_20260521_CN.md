# BlueprintHelper GraphStatement Framework 总设计文档

## ActionResolution 一级分发规则（2026-05-21 更新）

- `ActionResolutionCore` 的一级请求类型必须是 `EBlueprintHelperSpawnerClusterKind`。
- AgentFace 的 `call/get/set/get_property/set_property/op/construct/deconstruct/select/control` 等语义不再作为 ActionResolution 一级请求类型存在。
- 这些语义只能进入 `FBlueprintHelperActionSemanticConstraints`，作为所选 UE NodeSpawner family cluster 内部的解析约束。
- `SpawnerClusterResolver` 只按 `Request.ClusterKind` 分发，不允许根据 semantic kind 再做一级簇选择。
- GraphStatement / Semantic Resolver 负责把 AgentFace semantic statement 映射为：`SpawnerClusterKind + SemanticConstraints`。
- 新增能力时必须先判断 UE NodeSpawner family 边界，再扩展对应 cluster；不得重新引入 旧语义到簇的一级分发模型。

日期：2026-05-21
适用范围：AgentFace Graph body / BlueprintLogicSpec / SemanticIR / FragmentDAG / UE GraphWrite

## 1. 总目标

GraphStatement Framework 的目标不是让 AgentFace 直接描述 UE 节点，而是在尽可能压缩输入字段的前提下，利用 TaskSpec 提供的 Blueprint、Graph、Scope、typed pin、target object、metadata、search mode、category priority、ambiguity policy 等上下文，复用 UE 编辑器右键菜单背后的 ActionDatabase / BlueprintActionFilter / NodeSpawner 体系，达到 UE 右键菜单大部分节点选择能力，同时保持 AgentFace 顶层语义稳定、低 token、低错误率。

目标链路：

```text
AgentFace TaskSpec
-> BlueprintLogicSpec / Graph body canonical semantic statement
-> TS / Python compiler
-> UE SemanticIR parser
-> Semantic Resolver / typed resolver
-> Semantic Resolver
-> FBlueprintHelperActionResolutionRequest { ClusterKind, SemanticConstraints, GraphContext, TypedPins }
-> BlueprintActionResolutionCore
-> SpawnerClusterResolver.SelectCluster(ClusterKind)
-> selected UBlueprintNodeSpawner or derived spawner
-> NodeFragment adapter
-> FragmentDAG
-> Graph Composer / Linker
-> UE Mutator
-> Review / DebugBundle / ReadContext / LogicFlow
```

## 2. 硬性规则：Spawner-Oriented Clusters

后续 Graph body 能力按 UE NodeSpawner 家族组织簇，而不是按自然语言语义组织底层职责。

AgentFace `kind` 只作为语义输入字段，用于压缩 AgentFace；Semantic Resolver 负责结合 target、type、graph context、pin context、metadata constraints 生成 `SpawnerClusterKind + SemanticConstraints`，`SpawnerClusterResolver` 只按 `SpawnerClusterKind` 分发。

```text
AgentFace semantic statement
-> Semantic Resolver
-> FBlueprintHelperActionResolutionRequest { ClusterKind, SemanticConstraints, GraphContext, TypedPins }
-> BlueprintActionResolutionCore
-> SpawnerClusterResolver.SelectCluster(ClusterKind)
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

覆盖 SemanticConstraints：

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

覆盖 SemanticConstraints：

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

覆盖 SemanticConstraints：

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

覆盖 SemanticConstraints：

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

## 5. AgentFace SemanticConstraints 与簇的关系

AgentFace `kind` 不再定义底层簇边界，只作为 intent 输入。

| SemanticConstraints.Kind | 默认簇 | 说明 |
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
4. 不同 AgentFace 语义可以共享同一个 SpawnerCluster，但不会强迫 AgentFace 暴露 spawner 细节。
5. 新增能力不引入旧 fallback，不恢复旧 handler，不增加局部硬编码路径。
## 9.5 方案 C：UE Action 路径优先的硬性架构口径（2026-05-22）

当前 GraphStatement Framework 选择方案 C 作为后续四大工具簇的统一收敛方向：BlueprintHelper 不完整复刻 UE 右键菜单 UI，也不继续以自研解析 + 临时 NodeSpawner 混合路径作为主线；主路径应尽可能复用 UE 右键菜单背后的 ActionDatabase / ActionFilter / NodeSpawner 体系。

目标链路：

```text
TaskSpec compact semantic
-> SemanticResolver 构造 UE-equivalent context
-> ActionDatabase candidates
-> ActionFilter / BlueprintHelper semantic ranking
-> NodeSpawner evidence
-> shared Invoke adapter
-> FragmentDAG / Composer lifecycle
```

`ActionMenuItem` 不作为 Agent 可操作对象引入，因为它绑定 Slate/UI 菜单选择流程；但它承载的候选 evidence、binding、UI spec/search text、spawner payload 价值需要由 BlueprintHelper 的 `ResolvedSpawnerEvidence` 等价承接。

硬性规则：

1. 凡是 UE ActionDatabase / NodeSpawner 能表达的节点创建，BlueprintHelper 不允许直接决定 `UK2Node_*` 类型并创建节点。
2. BlueprintHelper 只负责构造上下文、语义约束、候选排序、preview diagnostics 和可重建的 spawner evidence。
3. 只有 UE NodeSpawner 无法表达，或 semantic operation 本质是多节点 DAG 编排时，才允许进入专用 FragmentBuilder。
4. ActionResolution layer 只负责解析并返回 spawner evidence，不负责创建节点、连线、应用默认值或触发 post-link lifecycle。
5. Fragment / Composer layer 只消费 spawner evidence，并通过 shared adapter 调用 `UBlueprintNodeSpawner::Invoke`。
6. Preview 是该架构的基线流程：低 token TaskSpec 先获得错误诊断、歧义候选或最小 success，再由 Agent 决定补充语义或执行。
7. `call` 不是所有 graph action 的总入口；`call/get/set/op/construct/control/event/bind/create` 等都是 compact semantic intent，必须映射到对应 NodeSpawner-family cluster 后在簇内解析。
8. `FindFunctionByName`、手扫 `UFunction` / `FProperty`、直接 `NewObject<UK2Node_*>()` 不得作为主路径；若作为 evidence 重建或 ActionDatabase 不可表达的窄例外，必须在对应 resolver/builder 中写明原因。

四大簇在该口径下的主路径：

```text
FunctionActionCluster
-> UBlueprintFunctionNodeSpawner / type-promotion operator spawner
-> call / op / convert_function / schedule_function / latent_or_async_function

FieldVariableActionCluster
-> UBlueprintFieldNodeSpawner / UBlueprintVariableNodeSpawner / UBlueprintComponentNodeSpawner
-> get / set / get_property / set_property / component_ref / field_access

EventDelegateActionCluster
-> UBlueprintEventNodeSpawner / UBlueprintBoundEventNodeSpawner / UAnimNotifyEventNodeSpawner / UBlueprintDelegateNodeSpawner / UBlueprintBoundNodeSpawner
-> event / component_bound_event / anim_notify_event / bind / unbind / assign / delegate_call / delegate_clear

GenericAssetStructControlActionCluster
-> UBlueprintNodeSpawner / UBlueprintFieldNodeSpawner / UBlueprintAssetNodeSpawner / struct-enum-generic registrar delegates
-> construct / deconstruct / select / single-node control / create / asset_action / container_action
-> multi-node control DAG and UE-unrepresentable semantic operations use dedicated FragmentBuilder
```

该方案的预期取舍：

- 正确率优先复用 UE 行为，理论上比自研路径更接近编辑器右键菜单。
- AgentFace 字段保持精简，复杂上下文由 BlueprintHelper 从 TaskSpec、Blueprint、Graph、Schema、typed pins、target/binding object 中推断。
- Preview 必须返回精简但可行动的候选 evidence，避免为了精确选择而膨胀 TaskSpec 字段。
- 迁移过程中应优先清理仍可达的自研 fallback 主路径，再逐簇补齐 UE action context。

## 10. ActionContext Pipeline（2026-05-22 补充）

`ActionResolutionRequest` 不再作为上下文收集入口，而是作为 `ResolvedActionContextBundle` 投影出的执行请求。GraphStatement / SemanticIR 负责表达语义需求，ActionContext Pipeline 负责统一收集、快照、推断、去重、版本校验与投影，ActionResolutionCore 只消费已投影的 request。

主链路：

```text
TaskSpec / SemanticIR
-> ContextDemandCollector
-> GameThread SnapshotBuilder
-> Worker-safe InferenceService
-> ResolvedActionContextBundle
-> BundleProjector
-> ActionResolutionCore
```

硬性规则：

1. Worker 线程不得访问 `UObject*`、`UBlueprint*`、`UEdGraph*`、`UEdGraphPin*`、`FindObject`、`LoadObject` 或 `Graph->GetSchema()` 等 UE 对象/API。
2. UE 对象读取集中在 `SnapshotBuilder`，并输出纯 DTO；worker 只消费不可变 DTO、statement tree、data edge 和 symbol table。
3. `InferenceService` 负责 statement/dataflow/symbol 推断、上下文合并与去重，不得重新读取 UE runtime。
4. `BundleProjector` 是 `ResolvedActionContextBundle -> FBlueprintHelperActionResolutionRequest` 的唯一投影边界；GraphStatementBuilder 不应再直接拼 `ActionRequest.ClusterKind` 或 `ActionRequest.Semantic`。
5. cluster resolver 只能消费 `ActionResolutionRequest`，不允许反向扫描 TaskSpec、重建 ContextDemand、重跑 SnapshotBuilder 或私自调用 BundleProjector。
6. preview 和 execute 必须消费同一套 pipeline；DebugBundle / Review evidence / UI overlay / AcceptReject 状态也应引用同一套 Review/Action 数据模型解释。

### 10.1 Setting 与硬编码约束

ActionContext Pipeline 发现的策略、阈值、候选数量、搜索模式、歧义策略、fallback 开关、padding 或调优默认值，都必须拆分到统一 settings runtime consumption 边界。除 UE API 常量、schema 固定规则、枚举语义强绑定值外，不允许继续散落在 resolver、builder、cluster、UI 或测试辅助逻辑中。

禁止示例：

```text
Context.Semantic.SearchMode = TEXT("settings_default")
Context.Semantic.AmbiguityPolicy = TEXT("settings_default")
MaxCandidates = 20
```

期望形式：

```text
ActionContextDemand / Snapshot DTO
-> settings service resolves runtime defaults
-> InferenceService consumes resolved policy values
-> BundleProjector projects already resolved SemanticConstraints
```

测试层只负责约束边界：当检测到 ActionContext 源码内出现硬编码策略默认值时，应失败并提示该值必须迁移到统一 settings service / runtime consumption 边界。

## 10.2 Four Spawner Cluster Context Consumption Rule (2026-05-22)

Four Spawner-Oriented clusters must consume ActionContext only through the projected `FBlueprintHelperActionResolutionRequest` and `FBlueprintHelperActionClusterContextView` emitted by ActionContextPipeline.

Allowed in clusters:
- Read `Semantic`, `StatementId`, `ProjectedContextHash`, `SemanticConstraintsHash`, and `ContextEvidence`.
- Query UE ActionDatabase / BlueprintActionFilter / NodeSpawner-family APIs using the projected graph and semantic context.
- Return resolved spawner evidence, ambiguity diagnostics, or explicit missing-context diagnostics.

Forbidden in clusters:
- Build `ActionContextDemand`, `Snapshot`, `Inference`, `ResolvedActionContextBundle`, or `ActionContextScope`.
- Re-read TaskSpec or rebuild target/type inference locally.
- Resolve a successful action without a valid UE `UBlueprintNodeSpawner` or documented dedicated FragmentBuilder boundary.
- Silently rerun preview during execute.

Execute path rule:
- Execute does not re-preview. If preview-time context or future evidence is stale, missing, or unresolvable, execute must return explicit diagnostics and stop.
- Missing context must be reported as `InvalidRequest` / needs-more-semantic-context style diagnostics, never repaired inside a cluster.

Implementation note 2026-05-22:
- `call` stable-id fast path using direct `FindFunctionByName` was removed because it could return a `UFunction` without valid `NodeSpawner` evidence.
- `FieldVariableActionResolver` now consumes projected `field_name` evidence for candidate selection and only uses UE property lookup to rebuild the `UBlueprintVariableNodeSpawner`.
- Branch/return control boundary checks now consume the existing `ActionContextScope` projection instead of constructing identity-free local requests.
