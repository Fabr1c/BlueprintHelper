# BlueprintHelper UE Action Context Input Matrix

日期：2026-05-22

目的：整理方案 C 架构下四大 Spawner-Oriented Cluster 需要提供给 UE ActionDatabase / ActionFilter / NodeSpawner 链路的上下文。本文档预留“如何获取”列，用于补充每个上下文字段在当前 TaskSpec、SemanticIR、UE Editor runtime 或 preview 反馈中的来源。

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

## 填写规则

- “如何获取”列由后续补充，建议写清楚来源路径，例如 TaskSpec 字段、SemanticIR 推断、UE runtime 查询、Blueprint/Graph 反查、preview 二次确认等。
- 如果某字段不能稳定获取，应写明“不可稳定获取”以及 fallback 策略。
- 如果某字段不应暴露给 AgentFace，应写明“UE 侧推断，不进入 AgentFace”。
- 如果某字段需要新增 TaskSpec 字段，应写明字段名、是否可选、preview 缺失时的错误或候选返回策略。

---

## 1. 共同最小上下文

### 1.1 GraphContext

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `UBlueprint* Blueprint` | 构造 `FBlueprintActionContext`，限定 action 候选和 self class | 是 | target.asset_path搜索 |  |
| `UEdGraph* Graph` | 构造 `FBlueprintActionContext`，判断 graph 类型和 schema 兼容性 | 是 | scope_policy.graph_name搜索 |  |
| `UEdGraphSchema_K2* Schema` | ActionFilter、pin 兼容、连线和 reconstruct 生命周期 | 是 | unknown |  |
| `GraphType` | 判断 event graph / function graph / macro / construction script 等限制 | 是 | behavior.entries.entry_type |  |
| `CurrentFunctionFlags` | 判断 pure / const / static / impure / latent 是否允许 | 条件必须 | behavior.entries.entry_type搜索然后获取具体内容 | function graph 内更重要 |
| `bImpureAllowed` | 过滤 impure callable、latent callable、exec 节点 | 是 | behavior.entries.entry_type |  |
| `bLatentAllowed` | 过滤 latent / async action | 条件必须 | behavior.entries.entry_type | latent_or_async / schedule 必须 |

### 1.2 TypedPinContext

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `DraggedFromPins` 等价信息 | 模拟 UE 右键菜单从 pin 拖出时的上下文 | 条件必须 |  | 可以是真实 pin 或 typed pin evidence |
| `SourcePinType` | 过滤函数参数、变量、operator、construct/deconstruct 候选 | 条件必须 |  |  |
| `TargetPinType` | 过滤 set/property write/return/select 输出 | 条件必须 |  |  |
| `PinDirection` | 区分输入、输出、exec pin 场景 | 条件必须 |  |  |
| `ExpectedReturnType` | 函数、convert、construct、select 候选排序 | 条件必须 |  |  |
| `ArgumentPinTypes` | 函数调用、delegate、operator 候选过滤 | 条件必须 |  |  |
| `WildcardPromotionTarget` | select、operator、container、make/break 的 wildcard 类型收敛 | 条件必须 |  |  |

### 1.3 TargetContext

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `SelfClass` | self 调用、成员函数、成员变量、event 可见性 | 是 |  |  |
| `TargetObjectType` | 函数调用、field access、component ref、delegate bind | 条件必须 |  |  |
| `TargetObjectPinType` | 从 target pin 推断候选范围 | 条件必须 |  |  |
| `SelectedObjects` 等价信息 | 模拟 MyBlueprint / SCS / Content Browser 选中对象 | 条件必须 |  | bound event / component / asset action 关键 |
| `BindingObject` | bound event、delegate、component action 的绑定对象 | 条件必须 |  |  |
| `WorldContextAvailability` | SpawnActor、CreateWidget、Delay、Timer、Latent/Async 过滤 | 条件必须 |  |  |

### 1.4 SearchPolicy

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `Query` | 候选搜索文本和 exact/fuzzy 匹配 | 条件必须 |  |  |
| `NameHint` | 语义名、友好名、字段名等候选提示 | 条件必须 |  |  |
| `SearchMode` | exact / fuzzy / candidates | 是 |  |  |
| `CategoryPriority` | 优先 Timer、Math、FlowControl、Utilities 等分类 | 可选 |  |  |
| `AmbiguityPolicy` | pick_best / require_user_resolution / candidates_only | 是 |  |  |
| `MaxCandidates` | preview 返回候选数量控制 | 是 |  |  |

### 1.5 ResolvedSpawnerEvidence

| evidence 字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `CandidateId` | preview/execute 间重建候选 | 是 |  | 不持久化裸 UObject 指针 |
| `SpawnerClass` | 说明使用哪个 NodeSpawner family | 是 |  |  |
| `NodeClass` | DebugBundle / Review evidence / preview 诊断 | 是 |  |  |
| `AssociatedFunction` | function action 重建和诊断 | 条件必须 |  |  |
| `AssociatedProperty` | field/variable/delegate action 重建和诊断 | 条件必须 |  |  |
| `AssociatedStruct` | construct/deconstruct 重建和诊断 | 条件必须 |  |  |
| `AssociatedAsset` | asset action 重建和诊断 | 条件必须 |  |  |
| `MenuName` | preview 友好候选展示 | 是 |  |  |
| `Category` | 候选排序和诊断 | 可选 |  |  |
| `Keywords` | fuzzy/candidate preview 辅助 | 可选 |  |  |
| `SemanticScore` | BlueprintHelper 语义排序 | 是 |  |  |
| `GraphCompatibilityResult` | preview 诊断 | 是 |  |  |
| `PinCompatibilityResult` | preview 诊断 | 条件必须 |  |  |
| `BindingEvidence` | bound event / delegate / asset binding 重建 | 条件必须 |  |  |

---

## 2. FunctionActionCluster 上下文

覆盖语义：

```text
call
op
convert_function
schedule_function
latent_or_async_function
```

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `FunctionQuery` | 函数名、友好名、搜索文本 | 条件必须 |  |  |
| `OwnerClassHint` | 限定函数 owner class 或 library | 可选 |  | 不应成为必填 AgentFace 字段 |
| `TargetObjectType` | 成员函数、call-on-member、component function | 条件必须 |  |  |
| `TargetObjectPinType` | 从 pin 推断 target object | 条件必须 |  |  |
| `ArgumentNames` | 参数名匹配和默认值应用 | 可选 |  |  |
| `ArgumentTypes` | 函数重载和候选过滤 | 条件必须 |  |  |
| `ArgumentPinTypes` | 模拟 dragged pin / typed argument context | 条件必须 |  |  |
| `ExpectedReturnType` | convert/call/select-like function 筛选 | 条件必须 |  |  |
| `ExpectedReturnPinType` | 精细类型过滤 | 条件必须 |  |  |
| `WorldContextAvailability` | world context function / latent / timer | 条件必须 |  |  |
| `AsyncPolicy` | allow / require_latent_or_async / forbid_latent | 条件必须 |  |  |
| `SchedulePolicy` | timer / latent / async / any | 条件必须 |  |  |
| `PureImpurePolicy` | pure only / impure allowed / latent allowed | 是 |  |  |
| `OperatorName` | op 语义映射到 UE type promotion operator | 条件必须 |  |  |
| `OperandPinTypes` | op promotion 类型推断 | 条件必须 |  |  |

---

## 3. FieldVariableActionCluster 上下文

覆盖语义：

```text
get
set
get_property
set_property
component_ref
field_access
```

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `FieldName` | 变量、字段、组件、属性名 | 条件必须 |  |  |
| `PropertyPath` | get_property / set_property 的路径 | 条件必须 |  |  |
| `AccessIntent` | get / set / read path / write path | 是 |  |  |
| `Scope` | member / inherited / local / param / component | 条件必须 |  |  |
| `OwnerClass` | ActionDatabase field visibility / owner filtering | 条件必须 |  |  |
| `FunctionGraph` | local variable / param action | 条件必须 |  |  |
| `ExpectedValuePinType` | get 输出或 set 输入类型 | 条件必须 |  |  |
| `TargetObjectType` | 非 self 对象字段访问 | 条件必须 |  |  |
| `ReadWritePermission` | BlueprintRead / BlueprintWrite / private/protected | 是 |  |  |
| `ComponentName` | component ref / component variable | 条件必须 |  |  |
| `ComponentClass` | component action / component ref 过滤 | 条件必须 |  |  |

---

## 4. EventDelegateActionCluster 上下文

覆盖语义：

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

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `EventName` | custom event / implementable event | 条件必须 |  |  |
| `EventFunction` | engine event / override event / interface event | 条件必须 |  |  |
| `EventUniquenessPolicy` | reuse / focus_existing / create_new | 是 |  |  |
| `DelegateProperty` | delegate node spawner / bound event spawner | 条件必须 |  |  |
| `DelegateOwnerClass` | delegate visibility and binding compatibility | 条件必须 |  |  |
| `BindingObject` | component / actor / selected object binding | 条件必须 |  |  |
| `ComponentProperty` | component bound event | 条件必须 |  |  |
| `ComponentName` | 从 TaskSpec 名称定位组件 | 条件必须 |  |  |
| `DelegateSignaturePinTypes` | bind/assign/call pin 兼容 | 条件必须 |  |  |
| `AnimSkeletonPath` | anim notify event | 条件必须 |  |  |
| `AnimNotifyName` | anim notify event 名称 | 条件必须 |  |  |

---

## 5. GenericAssetStructControlActionCluster 上下文

覆盖语义：

```text
construct
deconstruct
select
control
create
asset_action
container_action
```

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `StructType` | make/break struct | 条件必须 |  |  |
| `EnumType` | switch/select enum | 条件必须 |  |  |
| `ContainerKind` | array / map / set | 条件必须 |  |  |
| `ElementType` | array/set/container wildcard | 条件必须 |  |  |
| `KeyType` | map key type | 条件必须 |  |  |
| `ValueType` | map value type / select result | 条件必须 |  |  |
| `ControlIntent` | branch / sequence / switch / return / loop / gate | 条件必须 |  |  |
| `ExecSourceContext` | exec source pin / previous statement continuation | 条件必须 |  |  |
| `ContinuationPolicy` | branch then/else 是否自动接回后续语句 | 条件必须 |  |  |
| `ReturnGraphContext` | function return node / output pins | 条件必须 |  |  |
| `DesiredResultType` | select / construct 输出类型 | 条件必须 |  |  |
| `CreateTargetClass` | SpawnActor / CreateWidget / ConstructObject | 条件必须 |  |  |
| `OuterOrOwnerContext` | ConstructObject / Widget owner / component owner | 条件必须 |  |  |
| `WorldContextAvailability` | SpawnActor / Widget / async create | 条件必须 |  |  |
| `FactoryFunction` | factory-backed create / async action | 条件必须 |  |  |
| `AssetData` | asset action / asset-backed node | 条件必须 |  |  |

---

## 6. 当前待填写问题

| 问题 | 结论 / 如何获取 | 备注 |
|---|---|---|
| 哪些上下文可以从 TaskSpec 直接读取？ |  |  |
| 哪些上下文可以从 Blueprint / Graph / Schema 反查？ |  |  |
| 哪些上下文可以从 typed pin / data edge 推断？ |  |  |
| 哪些上下文必须通过 preview 候选返回让 Agent 二次选择？ |  |  |
| 哪些上下文不应进入 AgentFace，只能 UE 侧内部推断？ |  |  |
| 哪些上下文缺失时应返回 `NeedsMoreSemanticContext`？ |  |  |
| 哪些语义必须允许专用 FragmentBuilder？ |  |  |
| 哪些旧路径必须删除或隔离为不可调用？ |  |  |

## 7. 初步例外清单

以下场景可以不完全走 UE ActionDatabase / NodeSpawner 单节点路径，但必须在实现中写明原因：

| 场景 | 原因 | 如何获取 / 后续补充 |
|---|---|---|
| 多节点 control DAG | 一个 semantic operation 本质上需要多个 UE 节点编排 |  |
| branch then/else 自动接回后续语句 | 需要 Graph Composer 控制 exec DAG |  |
| 复杂 property path composition | 可能需要 get/set + break/make struct 多节点组合 |  |
| multi-return orchestration | 需要函数图返回节点和 exec continuation 管理 |  |
| UE ActionDatabase 不暴露的内部节点生命周期 | 必须专用 builder 或 adapter hook |  |
