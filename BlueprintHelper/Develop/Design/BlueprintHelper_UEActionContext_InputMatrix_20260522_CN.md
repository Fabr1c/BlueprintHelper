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
| `DraggedFromPins` 等价信息 | 模拟 UE 右键菜单从 pin 拖出时的上下文 | 条件必须 | 一般条件下无，但在behavior.entries.body.statements.args有输入的情况下可以根据args内的输入变量结合statements前后文推断出类型 | 可以是真实 pin 或 typed pin evidence |
| `SourcePinType` | 过滤函数参数、变量、operator、construct/deconstruct 候选 | 条件必须 | 同上 |  |
| `TargetPinType` | 过滤 set/property write/return/select 输出 | 条件必须 | 同上，结合后文可以推断 |  |
| `PinDirection` | 区分输入、输出、exec pin 场景 | 条件必须 | 完全可以根据json推断 |  |
| `ExpectedReturnType` | 函数、convert、construct、select 候选排序 | 条件必须 | 同上可以推断 |  |
| `ArgumentPinTypes` | 函数调用、delegate、operator 候选过滤 | 条件必须 | statements[x].kind |  |
| `WildcardPromotionTarget` | select、operator、container、make/break 的 wildcard 类型收敛 | 条件必须 | 可以根据前后文推断，或直接连接后由编辑器自己收敛转换类型 |  |

### 1.3 TargetContext

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `SelfClass` | self 调用、成员函数、成员变量、event 可见性 | 是 | 前面能直接拿到UBlueprint*这里就能获取 |  |
| `TargetObjectType` | 函数调用、field access、component ref、delegate bind | 条件必须 | 同上，如果要访问某个ref的同样获取蓝图指针访问 |  |
| `TargetObjectPinType` | 从 target pin 推断候选范围 | 条件必须 |  |  |
| `SelectedObjects` 等价信息 | 模拟 MyBlueprint / SCS / Content Browser 选中对象 | 条件必须 | unknown | bound event / component / asset action 关键 |
| `BindingObject` | bound event、delegate、component action 的绑定对象 | 条件必须 | unknown |  |
| `WorldContextAvailability` | SpawnActor、CreateWidget、Delay、Timer、Latent/Async 过滤 | 条件必须 | 前文能判断是否为eventgraph，同样 |  |

### 1.4 SearchPolicy

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `Query` | 候选搜索文本和 exact/fuzzy 匹配 | 条件必须 | 参考CallFunction流程 |  |
| `NameHint` | 语义名、友好名、字段名等候选提示 | 条件必须 | statements[x].target |  |
| `SearchMode` | exact / fuzzy / candidates | 是 | 后续给behavior添加一个Query字段启动精确搜索和模糊匹配，但都要从preview流程返回候选 |  |
| `CategoryPriority` | 优先 Timer、Math、FlowControl、Utilities 等分类 | 可选 | statements[x]可选字段 |  |
| `AmbiguityPolicy` | pick_best / require_user_resolution / candidates_only | 是 | 应该已经存在了的策略字段，如果没有就新增behavior.AmbiguityPolicy |  |
| `MaxCandidates` | preview 返回候选数量控制 | 是 | Setting配置项 |  |

### 1.5 ResolvedSpawnerEvidence

| evidence 字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `CandidateId` | preview/execute 间重建候选 | 是 | 具体不清楚了，参考CallFunction | 不持久化裸 UObject 指针 |
| `SpawnerClass` | 说明使用哪个 NodeSpawner family | 是 | 具体不清楚了，参考CallFunction |  |
| `NodeClass` | DebugBundle / Review evidence / preview 诊断 | 是 | 具体不清楚了，参考CallFunction |  |
| `AssociatedFunction` | function action 重建和诊断 | 条件必须 | 具体不清楚了，参考CallFunction |  |
| `AssociatedProperty` | field/variable/delegate action 重建和诊断 | 条件必须 | 具体不清楚了，参考CallFunction |  |
| `AssociatedStruct` | construct/deconstruct 重建和诊断 | 条件必须 | 具体不清楚了，参考CallFunction |  |
| `AssociatedAsset` | asset action 重建和诊断 | 条件必须 | 具体不清楚了，参考CallFunction |  |
| `MenuName` | preview 友好候选展示 | 是 | 具体不清楚了，参考CallFunction |  |
| `Category` | 候选排序和诊断 | 可选 | 无需 | 具体不清楚了，参考CallFunction |
| `Keywords` | fuzzy/candidate preview 辅助 | 可选 | 无需 |  |
| `SemanticScore` | BlueprintHelper 语义排序 | 是 | 具体不清楚了，参考CallFunction |  |
| `GraphCompatibilityResult` | preview 诊断 | 是 | 具体不清楚了，参考CallFunction |  |
| `PinCompatibilityResult` | preview 诊断 | 条件必须 | 具体不清楚了，参考CallFunction |  |
| `BindingEvidence` | bound event / delegate / asset binding 重建 | 条件必须 | 具体不清楚了，参考CallFunction |  |

---

### 1.6 已确认共享上下文获取途径（2026-05-22 回填）

本节用于回填已经确认的共享上下文获取途径。若本节与前表“如何获取”列存在冲突，以本节为当前架构口径；后续实现应把这些获取途径收敛到统一的 `ActionResolutionCore` / context builder，而不是在各个 cluster 内重复拼接。

| 字段 | 已确认获取途径 | 当前判断 |
|---|---|---|
| `UBlueprint* Blueprint` | 由 TaskSpec 的 `target.asset_path` / graph scope 解析到资产路径，经资产解析器加载或查找对象，并校验为 `UBlueprint`。 | 可稳定获取 |
| `UEdGraph* Graph` | 由 `scope_policy.graph_name`、replace/merge/patch 目标选择器、function/event/custom event scope 在目标 Blueprint 内解析；新增 owned graph 场景先由签名/图创建链路生成或确保图存在，再进入 GraphWrite body。 | 可稳定获取，但必须以 UE 实际图对象为准 |
| `UEdGraphSchema_K2* Schema` | 从已解析 `Graph->GetSchema()` 获取并校验 K2 schema。 | 可稳定获取 |
| `GraphType` | 从实际 `UEdGraph` 类型、outer、schema、function graph/custom event/ubergraph 分类推导；不能只依赖 `behavior.entries.entry_type`。 | 可稳定获取，需要集中实现 |
| `CurrentFunctionFlags` | 对 function graph 从 `UFunction`、函数签名或函数图元数据获取；custom event 不等价于普通 `UFunction` flags。 | 可获取，需区分 function 与 event |
| `bImpureAllowed` | 由 `GraphType`、函数 pure/static/const flags、schema 规则联合推导；event graph/custom event 通常允许 impure，pure function graph 不允许 impure。 | 可获取，需集中规则 |
| `bLatentAllowed` | 由 `GraphType`、函数 flags、world context/latent 支持规则推导；event graph 通常允许，pure/static function 路径需要阻断。 | 可获取，需集中规则 |
| `DraggedFromPins` 等价信息 | CLI 没有真实拖拽 pin；用 data edge、expression consumer、symbol table、literal `value_type`、已解析 UE pin 形成“合成拖拽上下文”。 | 可合成，不能伪装成 UI selected pin |
| `SourcePinType` | 来自 producer expression 的输出 pin、literal `value_type`、变量/属性元数据、函数返回 pin、construct/select 输出 pin，或 post-spawn/post-link 后的 UE pin。 | 可获取，覆盖率取决于 typed data edge |
| `TargetPinType` | 来自 consumer pin，例如 call arg、set value、set_property value、return value、branch condition，或目标节点解析后的 UE pin。 | 可获取，需统一消费 data edge |
| `PinDirection` | 由 statement/expression 所在位置和真实 `UEdGraphPin::Direction` 确认。 | 可稳定获取 |
| `ExpectedReturnType` | 优先从 consumer context 推断；construct 可由显式 type 得到；无 consumer 的 call/op/select 需要 preview 候选或保持为 rank hint。 | 部分可获取 |
| `ArgumentPinTypes` | 来自参数 expression、delegate/function signature、literal `value_type`、变量/属性元数据、上游 data edge；不能从 `statements[x].kind` 直接推断。 | 可获取，需 resolver 补齐 |
| `WildcardPromotionTarget` | 从 typed source/consumer edge 推断；若仍为 wildcard，交由 UE schema / node spawner / pin connection 后续 promotion，并写入 evidence/debug。 | 部分可获取 |
| `SelfClass` | 从 `Blueprint->GeneratedClass` 或 `Blueprint->SkeletonGeneratedClass` 获取。 | 可稳定获取 |
| `TargetObjectType` | 从显式 semantic target、component/variable/property metadata、data edge source pin type 获取；缺省为 self class。 | 可获取，需 typed target resolver |
| `TargetObjectPinType` | 从 target expression 输出 pin或目标对象解析后的真实 UE pin 获取。 | 可获取，依赖 target resolver |
| `SelectedObjects` 等价信息 | CLI 无真实 UI selection；仅当 TaskSpec semantic target 给出 component、asset、binding identity 时合成。 | 共享层缺口 |
| `BindingObject` | 从 component bound event、delegate bind、asset action 的 semantic target 合成。 | cluster 专属缺口 |
| `WorldContextAvailability` | 由 Blueprint/Graph 上下文、函数 metadata、self class/world owning context 共同推导。 | 可获取，需集中规则 |
| `Query` | 来自 `target`、`op`、`property_path`、`field_name`、semantic query 等短语义字段。 | 可稳定获取 |
| `NameHint` | 来自 `target`、`name`、`function_name`、`event_name`、`property_path` 等用户语义字段，以及候选 display/menu name。 | 可稳定获取 |
| `SearchMode` | 默认来自 settings；允许 statement/expression 级覆盖。不能只依赖全局 behavior query。 | 需补 per-node 覆盖 |
| `CategoryPriority` | 来自语义字段；也可从 schedule/op/function role 推导。 | 可获取，建议作为 rank hint |
| `AmbiguityPolicy` | 默认来自 settings/request；允许 semantic 局部覆盖；不唯一时返回精简候选。 | 可获取 |
| `MaxCandidates` | 默认来自 settings；建议保留 request/statement 级覆盖。 | 可获取 |
| `CandidateId` | 由 resolved candidate / spawner evidence 生成稳定 id；不能持久化原始 UObject 指针。 | 可生成 |
| `SpawnerClass` | 由 `ResolvedSpawner->GetClass()->GetPathName()` 在 preview/execute 阶段记录。 | 可稳定获取 |
| `NodeClass` | 由 spawner 的 node class 或实际 spawned node class 获取。 | 可稳定获取 |
| `AssociatedFunction` | 由 `FBlueprintActionInfo::GetAssociatedFunction()` 或 function candidate 解析。 | FunctionActionCluster 可获取 |
| `AssociatedProperty` | 由 field/delegate/variable spawner payload 或 ActionInfo associated field/property 解析。 | FieldVariable/EventDelegate 需补齐 |
| `AssociatedStruct` | 由 struct make/break/select 相关 spawner payload 或 semantic type 解析。 | GenericAssetStructControl 需补齐 |
| `AssociatedAsset` | 由 asset node spawner payload 或 `FAssetData` 解析。 | AssetAction 需补齐 |
| `MenuName` | 由 spawner `GetUiSpec` / candidate display name 获取。 | 可获取 |
| `Category` | 由 UI spec category 或 semantic category priority 获取。 | 可获取 |
| `Keywords` | 由 UI spec keywords 获取；没有时为空。 | 可获取但非必需 |
| `SemanticScore` | 由 BlueprintHelper semantic ranker / candidate score 生成。 | 可生成 |
| `GraphCompatibilityResult` | 由 ActionFilter、graph compatibility check 或候选 mismatch reason 记录。 | 可生成，需统一 evidence |
| `PinCompatibilityResult` | 由 ActionFilter、schema connection check 或 pin mismatch reason 记录。 | 可生成，需统一 evidence |
| `BindingEvidence` | 由合成 binding object、delegate signature、bound event spawner binding 记录。 | EventDelegate/ComponentBoundEvent 缺口 |

### 1.7 当前真实获取率判断

| 范围 | 获取率判断 | 主要差距 |
|---|---:|---|
| 共享上下文稳定实现 | 约 55%-60% | 已有 Blueprint/Graph/Schema/基础 query，但 typed pin、binding object、per-node search policy、evidence 标准化仍不完整。 |
| 源码层可实现上限 | 约 80%-85% | TaskSpec + UE runtime + typed data edge + preview 可以补齐大部分上下文；真实 UI selected object / pin drag 无法完整等价，只能合成。 |
| FunctionActionCluster | 约 70%-75% | call/action resolver 已具备基础链路，仍需把 op/schedule/latent/convert 的语义约束并入同一 evidence 模型。 |
| FieldVariableActionCluster | 约 45%-55% | 变量/属性上下文可从 Blueprint/metadata 获取，但 selected/binding 等价上下文和 field spawner evidence 仍需补齐。 |
| GenericAssetStructControlActionCluster | 约 45%-55% | construct/deconstruct/select/control 可获得部分类型上下文，但 struct/action payload、wildcard promotion、post-link evidence 仍需统一。 |
| EventDelegateActionCluster | 历史口径：约 20%-30%；当前 use-site scoped gap 已由 Gap5 后续闭环 | 本行原本混合了 event declaration、handler/signature、component-bound/delegate use-site。当前职责边界已拆分：`BlueprintSignature` owns declaration/signature；GraphWrite/EventDelegate 只消费 projected evidence 写 component-bound/delegate use-site；更宽真实资产覆盖仍是后续测试矩阵问题。 |

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
component_bound_event
bind
unbind
assign
delegate_call
delegate_clear
```

| 上下文字段 | 用途 | 是否必须 | 如何获取 | 备注 |
|---|---|---:|---|---|
| `DelegateProperty` | delegate node spawner / bound event spawner | 条件必须 |  |  |
| `DelegateOwnerClass` | delegate visibility and binding compatibility | 条件必须 |  |  |
| `BindingObject` | component / actor / selected object binding | 条件必须 |  |  |
| `ComponentProperty` | component bound event | 条件必须 |  |  |
| `ComponentName` | 从 TaskSpec 名称定位组件 | 条件必须 |  |  |
| `DelegateSignaturePinTypes` | bind/assign/call pin 兼容 | 条件必须 |  |  |

排除项：`event`、`EventName`、`EventFunction`、`EventUniquenessPolicy` 属于 Signature-owned event entry / GraphWrite body target resolution，不进入 `EventDelegateActionCluster`。GraphWrite 只能消费上游 `BlueprintSignature` 投影出的 entry/signature evidence 后写 body/use-site。

排除项：`anim_notify_event`、`AnimSkeletonPath`、`AnimNotifyName` 属于 Animation Blueprint / Animation tooling，不进入当前 GraphWrite/EventDelegateActionCluster 的 ActionContext 输入矩阵。

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

## 8. ActionContext Pipeline 获取路径收敛（2026-05-22 补充）

已确认共享上下文获取路径应由 ActionContext Pipeline 消费。本文前面的字段矩阵描述“需要哪些上下文”，本节描述“这些上下文由哪个边界负责进入统一模型”。

```text
TaskSpec / SemanticIR
-> ContextDemandCollector
-> GameThread SnapshotBuilder
-> Worker-safe InferenceService
-> ResolvedActionContextBundle
-> BundleProjector
-> ActionResolutionCore
```

边界分工：

1. `ContextDemandCollector` 从 statement tree / SemanticIR 收集需求，只输出 `StatementId`、semantic kind、cluster kind、query、target、property、type、symbol id 和 required kinds 等纯数据。
2. `SnapshotBuilder` 负责 UE runtime 可读字段，包括 Blueprint、Graph、Schema、GraphType、函数/事件图限制、成员变量、component/field 元数据和可稳定读取的 pin 类型快照。
3. `InferenceService` 负责 statement/dataflow/symbol 推断字段，包括 typed source/target edge、synthetic dragged pin context、target object type、argument pin types、return type、binding hints 和去重后的 evidence。
4. `BundleProjector` 负责把共享上下文投影为 `ActionResolutionRequest`，并在缺少 `StatementId`、Blueprint 或 Graph 时返回明确错误。
5. `ActionResolutionCore` 不再直接负责上下文收集；cluster resolver 不得重建 demand、snapshot、bundle 或 projection。

Setting / hardcoded 约束：

1. `SearchMode`、`AmbiguityPolicy`、`MaxCandidates`、candidate limit、ranking threshold、fallback 开关、category priority 默认值等策略字段必须来自统一 settings runtime boundary。
2. statement 或 request 允许覆盖设置值，但覆盖也必须通过统一 settings/service 解析后进入 DTO，不允许 resolver、builder、cluster、UI 或测试辅助逻辑复制默认值。
3. UE API 常量、schema 固定规则和枚举语义强绑定值可以保留为代码常量；其他调优值、数量、阈值和策略默认值不应硬编码。
4. 如果实现过程中发现新的硬编码策略值，应先补 settings/service 消费边界，再让 ActionContext Pipeline 消费已解析后的 policy DTO。
