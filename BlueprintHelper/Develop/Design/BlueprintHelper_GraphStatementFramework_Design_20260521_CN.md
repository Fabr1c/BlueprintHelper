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
-> AgentFace task-core TypeScript compiler
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

凡是候选空间较宽、正确率依赖上下文的 node/action 选择，GraphWrite 必须优先经过 `BlueprintActionResolutionCore`，并通过 projected ActionContext、ActionDatabase / ActionFilter / NodeSpawner evidence 完成解析。

不允许新增能力绕开 `SpawnerClusterKind -> cluster -> semantic constraint` 链路，直接使用全局函数名查找、硬编码函数名、在 builder / pipeline / coordinator 中硬编码 `UK2Node_*`，或恢复旧 `NodeHandler` / parsed-node fallback。

允许不经过 ActionDatabase 搜索链路的情况只有三类，但仍必须处在已选 cluster 的二级语义映射边界内：

1. canonical singleton semantic，例如 `branch`、`sequence`、`return` 这类没有宽候选空间的唯一控制流节点。
2. UE ActionDatabase / NodeSpawner 无法表达该 semantic operation。
3. 该 semantic operation 本质是多个 UE node 的组合，需要一个语义级 builder 编排多个 NodeFragment。

出现上述例外时，必须在 resolver / builder / singleton evidence 边界写清楚原因，且仍要复用统一 typed target、typed pin、candidate reporting、Review/Debug evidence 模型。

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
| `UBlueprintDelegateNodeSpawner` | delegate 节点 spawner | multicast delegate bind/assign/unbind/call/clear |
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
delegate
delegate_operation=bind|assign|unbind|call|clear
```

职责：

- 事件入口。
- 组件绑定事件。
- 动画通知事件。
- delegate bind/assign/unbind/call/clear。
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
convert + transform_operation
schedule + schedule_operation
asset_action
container_action
```

职责：

- Branch / Sequence / Switch / Return / Loop 等控制流。
- Select。
- Make/Break Struct。
- Make Array / Map / Set 等容器构造。
- SpawnActor / CreateWidget / ConstructObject 等 create 类 action。
- DynamicCast / ClassCast 等显式 Generic transform action；type-promotion 只有在投影出稳定 spawner evidence 后才能成功。
- Timer/delegate/latent node 等显式 Generic schedule action；普通 Kismet timer/latent function call 仍归 FunctionAction。
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
| `delegate` | EventDelegateActionCluster | delegate use-site family; `bind/assign/unbind/call/clear` must be represented by second-stage `delegate_operation` |
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
manual kind -> NewObject<UK2Node_X> shortcut outside the selected cluster, unless it is a documented canonical singleton evidence boundary or ActionDatabase-unrepresentable boundary
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
-> ActionDatabase candidates (wide-surface semantic only; canonical singleton semantic uses cluster-internal singleton evidence)
-> ActionFilter / BlueprintHelper semantic ranking
-> NodeSpawner evidence
-> shared Invoke adapter
-> FragmentDAG / Composer lifecycle
```

`ActionMenuItem` 不作为 Agent 可操作对象引入，因为它绑定 Slate/UI 菜单选择流程；但它承载的候选 evidence、binding、UI spec/search text、spawner payload 价值需要由 BlueprintHelper 的 `ResolvedSpawnerEvidence` 等价承接。

以下硬性规则中的 ActionDatabase / ActionFilter 优先策略适用于 wide-surface semantic。canonical singleton semantic 仍必须经过 `SpawnerClusterKind -> cluster -> semantic constraint` 路径，但可以在已选 cluster 内通过 direct spawn 产生统一 spawner evidence。

硬性规则：

1. 凡是 wide-surface semantic 的节点创建，BlueprintHelper 不允许直接决定 `UK2Node_*` 类型并创建节点；canonical singleton semantic 只能在已选 cluster 的 singleton evidence boundary 内 direct spawn。
2. BlueprintHelper 只负责构造上下文、语义约束、候选排序、preview diagnostics 和可重建的 spawner evidence。
3. 只有 canonical singleton semantic、UE NodeSpawner 无法表达，或 semantic operation 本质是多节点 DAG 编排时，才允许离开 ActionDatabase 搜索链路；其中 singleton direct spawn 仍必须留在已选 cluster 的 evidence boundary 内。
4. ActionResolution layer 只负责解析并返回 spawner evidence，不负责创建节点、连线、应用默认值或触发 post-link lifecycle。
5. Fragment / Composer layer 只消费 spawner evidence，并通过 shared adapter 调用 `UBlueprintNodeSpawner::Invoke`。
6. Preview 是该架构的基线流程：低 token TaskSpec 先获得错误诊断、歧义候选或最小 success，再由 Agent 决定补充语义或执行。
7. `call` 不是所有 graph action 的总入口；`call/get/set/op/construct/control/event/delegate/create` 等都是 compact semantic intent，必须映射到对应 NodeSpawner-family cluster 后在簇内解析；`bind/assign/unbind/call/clear` 这类 delegate 动作属于 `delegate_operation` 二级语义。
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
-> event / component_bound_event / anim_notify_event / delegate + delegate_operation=bind|assign|unbind|call|clear

GenericAssetStructControlActionCluster
-> UBlueprintNodeSpawner / UBlueprintFieldNodeSpawner / UBlueprintAssetNodeSpawner / struct-enum-generic registrar delegates
-> construct / deconstruct / select / single-node control / create / convert+transform_operation / schedule+schedule_operation / asset_action / container_action
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
- Query UE ActionDatabase / BlueprintActionFilter / NodeSpawner-family APIs using the projected graph and semantic context for wide-surface semantics; for canonical singleton semantics, resolve direct singleton spawner evidence inside the selected cluster.
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

## 2026-05-22 Context Consumption Clarification

- `get_property` / `set_property` belong to `FieldVariableActionCluster`; they should use the projected target/property evidence to shrink the field-variable search scope, not create a separate graph-local node path.
- `set_property` FragmentDAG emission must invoke the selected UE `NodeSpawner` through the shared ActionResolution adapter. It must not fall back to parsed-node local spawning.
- `op` belongs to `FunctionActionCluster`, but its semantic is operator constraints. It resolves to UE type-promotion operator spawners from `FTypePromotion::GetOperatorSpawner`, not to pseudo call-function lookup.
- Generic construct/deconstruct may use a dedicated `UBlueprintFieldNodeSpawner` MakeStruct/BreakStruct boundary only when UE FunctionAction/native make-break lookup cannot express the struct operation. This boundary must be explicit in candidate evidence and must remain generic, not Vector-only or type-specific.
- `EventDelegateActionCluster` follows the same projected-context rule. Custom events can resolve through `UBlueprintEventNodeSpawner` only as graph-node/body placement after Signature ownership is respected; component-bound events and delegate bind/unbind/call/clear nodes require projected component/delegate/handler/signature evidence before they can invoke their UE spawner families. `delegate.assign` also requires the same projected evidence, but uses a manual assign factory because the UE AssignDelegate spawner can create Signature-owned custom-event declarations as a side effect.

## 2026-05-23 EventDelegate / Signature Ownership Boundary

Signature owns declaration and signature mutation:

- `ensure_function`
- `ensure_custom_event`
- `ensure_event_dispatcher`
- `ensure_override_event`
- signature pins, mismatch policy, migration, and removal

GraphWrite/EventDelegate owns existing-declaration use-site graph writing:

- `component_bound_event`
- `delegate` use-site nodes with `delegate_operation=bind|assign|unbind|call|clear`
- delegate reference nodes such as `Create Event`
- graph links and body content around those use sites

GraphWrite/EventDelegate must consume projected `ActionContext` / `ActionDataBase` evidence. It must not scan assets inside the resolver to repair missing context, and it must not create or modify handler/function/custom-event/dispatcher signatures.

Handler boundary:

- If the handler implementation already exists, GraphWrite may reference it through projected evidence.
- If the handler implementation does not exist, a prior Signature dependency step must create it before GraphWrite runs.
- The `Bind Event to ...` / `Assign ...` node and the `Create Event` delegate-reference node are GraphWrite use-site nodes.
- The function or event selected by `Create Event` remains Signature-owned declaration/signature state.

AgentFace / lowering boundary:

- `delegate.unbind` and `delegate.unbind_all` must stay explicit.
- Missing callback evidence for `delegate.unbind` must not silently downgrade to `delegate.unbind_all`.
- Positive EventDelegate support requires complete projected evidence, stable candidate evidence, correct node family, execution/asset validation, and missing-evidence diagnostics that still fail deterministically when evidence is absent. Component-bound, bind, unbind, call, and clear operations require `SelectedSpawner != null`; `delegate.assign` is the intentional exception and must expose `ue_delegate_manual_assign_factory` evidence while constructing `UK2Node_AssignDelegate` without auto-creating `UK2Node_CustomEvent`.

Closure note 2026-05-23:

- Gap5 first-stage EventDelegate use-site support is closed for `component_bound_event` and `delegate_operation=bind|assign|unbind|clear|call`.
- The closed scope does not transfer declaration/signature ownership to GraphWrite/EventDelegate.
- The closed scope does not mark broader `event_operation=custom_event/override/native` migration or Generic `create` / `convert` / `schedule` work complete.

## 2026-05-22 Canonical Singleton Direct Spawn Rule

Direct spawn is allowed only as a secondary semantic mapping inside the selected spawner-oriented cluster. It must not introduce a new first-level dispatch path.

The first-level rule remains unchanged:

```text
SemanticIR
-> ActionContextDemandCollector
-> ActionContextScope / BundleProjector
-> FBlueprintHelperActionResolutionRequest
   - ClusterKind = GenericAssetStructControlAction
   - Semantic.Kind = Control / Select / Construct / ...
   - Semantic.Query or constraints = branch / sequence / return / ...
-> SpawnerClusterResolver dispatches only by ClusterKind
-> GenericAssetStructControlActionCluster
-> secondary semantic mapping
-> resolved spawner evidence
-> shared adapter invoke
```

Wide-surface semantics must use the full search/resolution chain because they have large candidate spaces and require context for correctness and generality. Examples include `call`, `op`, field/property access, delegate actions, and broad create/construct operations.

Canonical singleton semantics may use direct `UBlueprintNodeSpawner::Create(UK2Node_*)` after first-level cluster dispatch because their semantic space is unique and not a candidate search problem. Current singleton candidates include:

- `control=branch`
- `control=sequence`
- `control=return`
- `select`, if the selected UE node remains a single canonical Select node for the requested semantic

Direct spawn requirements:

- It must live behind the owning cluster or a shared singleton evidence provider used by that cluster.
- It must return the same `ActionResolutionResult` / spawner evidence shape as search-based resolution.
- Evidence must include semantic kind, singleton kind, node class path, stable id, and reason.
- It must not be used as a fallback when a wide-surface search fails.
- Builders, pipelines, and mutation coordinators must not directly create singleton `UK2Node_*` nodes; they must consume resolved evidence or call the shared singleton boundary.

Therefore, direct spawn is a valid implementation strategy for canonical singleton nodes, but only inside the existing `SpawnerClusterKind -> cluster -> semantic constraint` architecture. It does not weaken the rule that wide-surface GraphWrite actions require projected ActionContext and ActionDatabase/ActionFilter-based candidate resolution.

## 2026-05-23 Semantic Taxonomy Rule: First-Stage Semantic vs Second-Stage Operation

GraphWrite must not use `EBlueprintHelperActionSemanticKind` as an ever-growing list of every AgentFace verb. A value should become first-stage semantic only when it changes at least one of these boundaries:

- projected evidence shape
- owning resolver / spawner family
- candidate search strategy or singleton-evidence strategy
- fragment composition paradigm
- correctness-critical diagnostics and ambiguity policy

If several operations share the same evidence family and resolver strategy, represent them as one first-stage semantic plus a second-stage operation field in SemanticIR / ActionContext evidence.

Current approved example:

```text
EventDelegateActionCluster
  ComponentBoundEvent
  Delegate + delegate_operation=bind|assign|unbind|call|clear
```

This means GraphWrite/EventDelegate must not add top-level `Assign`, `Unbind`, `DelegateCall`, or `DelegateClear` action semantics. `delegate.unbind` and `delegate.unbind_all` remain explicit, but their distinction is held by `delegate_operation` and `unbind_mode`, not by separate first-stage Action semantic values.

Recommended convergence order after Gap5:

| Current kinds | Target first-stage semantic | Second-stage fields | Notes |
|---|---|---|---|
| `get`, `set`, `get_property`, `set_property` | `Field` | `field_operation=get/set`, `field_scope=variable/property_path` | Good first migration after Gap5 because they share FieldVariable evidence. |
| `construct`, `deconstruct` | `Struct` or `TypeStructure` | `type_operation=construct/deconstruct` | Keep separate from broad `create` until struct evidence is clear. |
| `convert` | `TypeTransform` or `Callable` | `transform_operation=cast/convert/promote` | Decide by whether evidence resolves through cast/type promotion or normal function search. |
| `call`, `op` | possibly `Callable` | `callable_operation=function/operator` | Higher risk because function call is the wide-surface search core. |
| `select` | `GenericExpression` or singleton evidence under Generic cluster | `expression_operation=select` | Only direct spawn if Select remains canonical singleton. |
| `event` | `Event` | `event_operation=custom_event/override/native` | Must preserve Signature ownership. |
| `create` | split after evidence audit | `create_operation=spawn_actor/create_widget/construct_object/asset_action` | Current name is too broad to normalize safely without an evidence audit. |
| `schedule` | `AsyncFlow` or `Callable` | `schedule_operation=timer/latent/async` | Lifecycle/latent behavior may justify its own first-stage semantic. |

This taxonomy does not change the top-level `SpawnerClusterKind` dispatch rule. Clusters remain the first dispatch boundary; semantic families and second-stage operations are constraints consumed inside the selected cluster.

## 2026-05-24 Struct / TypeStructure 收敛规则同步

- `construct` / `deconstruct` 仍从 AgentFace compact semantic 进入，但在 ActionResolution 中不再作为一级请求类型。
- ActionResolution 一级只按 `SpawnerClusterKind=GenericAssetStructControlAction` 分发；`construct/deconstruct` 必须成为 `FBlueprintHelperActionSemanticConstraints` 内的 `SemanticFamily=Struct|TypeStructure` 与 `TypeOperation=Construct|Deconstruct`。
- Generic cluster 只负责分发；`Struct/TypeStructure` 具体解析由 `FBlueprintHelperStructTypeStructureActionResolver` 承担。
- 成功 evidence 必须暴露 `struct_path`、`type_structure_id`、`type_operation`、`spawner_class`、`node_class`、`stable_id`、`match_reason`。
- broad `create`、`create_operation`、旧 `make_struct/break_struct` AgentFace token 不属于本路径，不能作为成功兜底。
- UE `UBlueprintFieldNodeSpawner` MakeStruct/BreakStruct boundary 允许作为显式、通用的 Struct/TypeStructure NodeSpawner evidence；它不是旧 `make_struct/break_struct` AgentFace alias。

## 2026-05-24 Function / Field 收敛与 Event Smoke 边界

- `FunctionActionCluster` 当前覆盖 `Call`、`Op`、`Convert -> convert_function`、`Schedule -> schedule_function`，以及 evidence 允许时的 `Schedule -> latent_or_async_function`。`Convert` / `Schedule` 通过 `FBlueprintHelperFunctionSemanticActionResolver` 做二级语义守卫，再复用 call resolver 路径；缺少转换、调度或 latent graph evidence 时返回 `needs_more_semantic_context`，不再落入 `unsupported_function_cluster_semantic`。
- `FieldVariableActionCluster` 当前覆盖 `field_scope=variable|property_path|component_ref|field_access`。复杂 `property_path` 保留 full/root/leaf/role metadata，并通过 dedicated fragment builder 标记交给 GraphWrite fragment 组合；`component_ref` 与 `field_access` 作为独立二级语义，不再被内部改写成普通 property path。
- ActionContext Field 推断会从 linked source / consumer symbol pin 投影 `TargetObjectPinType` 与 `ExpectedReturnPinType`，并记录 `linked_source_pin_type` / `linked_consumer_pin_type` evidence，供 resolver 缩小候选空间。
- TaskSpec TS/Python 编译器已接受 `component_ref` 与 `field_access`，并在 compiled body 中保留复杂 `property_path` 与 nested `field_access`。
- Event lifecycle taxonomy 仍由 BlueprintSignature owning path 管理：`custom_event` / `override_event` / `native_event` 不作为 GraphWrite/EventDelegate public declaration taxonomy。GraphWrite 只消费 Signature 依赖后的 body 写入和 delegate use-site；统一 smoke 已验证 `blueprint_signature.ensure_custom_event -> graph_write` 的依赖边界。

## 2026-05-24 Generic Broad Create Closure

- `Create` 当前作为 `GenericAssetStructControlActionCluster` 的二级语义接入，必须携带显式 `create_operation`，缺少该 evidence 时返回 `needs_more_semantic_context`。
- 已接入的 first slice 为 `spawn_actor`、`create_widget`、`construct_object`、`make_array`、`make_map`、`make_set`，均通过 selected NodeSpawner evidence 后由 `FBlueprintHelperActionNodeSpawnerAdapter` 执行。
- `asset_action` 暂不伪造成功；没有投影出的 ActionDatabase / selected spawner evidence 时保持可行动失败。
- Struct `construct/deconstruct` 仍归 Struct / TypeStructure 语义，不计入 broad create。
- 2026-05-24 verification: AgentFace TS compiler tests、UE 5.6 compile、`BlueprintHelper.GraphWrite.ActionResolution.Contract`、`BlueprintHelper.GraphWrite.ActionResolution.Generic.Create`、`BlueprintHelper.GraphWrite.ActionResolution.Generic`、`BlueprintHelper.GraphWrite.LegacyMainline`、full `BlueprintHelper.GraphWrite` automation 均已通过。

## 2026-05-24 Generic Convert/Schedule Ownership Closure

- Function-owned `convert_function`, `schedule_function`, and `latent_or_async_function` remain in `FunctionActionCluster`.
- Generic-owned `dynamic_cast`, `class_cast`, `type_promotion`, `timer_delegate_node`, and `latent_or_async_node` require explicit second-stage evidence and do not fallback through FunctionAction.
- `dynamic_cast` and `class_cast` can select cast-node spawner evidence when `target_class_path` is projected; missing target class evidence returns `needs_more_semantic_context`.
- `type_promotion`, `timer_delegate_node`, and `latent_or_async_node` are only considered success-capable after projected spawner evidence exists; until then they return deterministic missing-context diagnostics rather than fake success.
- Generic schedule support is only considered complete for operations that expose selected spawner evidence or deterministic missing-context diagnostics; normal Kismet timer calls remain FunctionAction.
- 2026-05-24 verification: AgentFace TS compiler tests、UE 5.6 compile、`BlueprintHelper.GraphWrite.ActionResolution`、`BlueprintHelper.GraphWrite.ActionContext`、full `BlueprintHelper.GraphWrite` automation 均已通过；最新 full report 为 `Saved/Automation/GraphWrite_GenericConvertSchedule_Final_20260524_001/index.json`，155 succeeded + 11 succeeded with warnings，0 failed，0 not run。
