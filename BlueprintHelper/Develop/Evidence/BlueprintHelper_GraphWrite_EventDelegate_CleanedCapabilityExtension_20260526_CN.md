# BlueprintHelper GraphWrite EventDelegate 清洗与能力拓展文档

日期：2026-05-26  
输入：`BlueprintHelper_GraphWrite_EventDelegate_UEEditorCapability_EngineSourceReadResult_20260525_CN.md`  
适用范围：BlueprintHelper UE 5.3+ 插件 / GraphWrite / TaskSpec / EventDelegateActionCluster

---

## 0. 清洗结论

本次清洗后，EventDelegate 仍保留 **6 个 GraphWrite use-site 能力方向**，但这些能力不再按 UE 编辑器 UI 入口描述，也不把右键菜单、拖拽、拖拽 Pin、Slate 选中态或组件面板命令作为插件可实现能力。

保留的能力是最终节点语义：

| GraphWrite operation | 清洗后结论 | 当前实现状态 |
|---|---|---|
| `event_delegate.component_bound_event` | 保留；通过 TaskSpec/statement-local component/delegate/binding evidence 创建或引用 `UK2Node_ComponentBoundEvent`。 | 有基础 resolver/builder，但缺 duplicate policy、dynamic binding readback、graph/flag gate。 |
| `event_delegate.delegate_bind` | 保留；通过 delegate property + target object + projected handler/signature evidence 创建 `UK2Node_AddDelegate` use-site。 | 有基础节点生成与 `CreateDelegate` 链接，但 handler object scope、signature readback、diagnostic 仍不足。 |
| `event_delegate.delegate_assign` | 保留为能力方向，但 **implementation policy 需讨论**；UE spawner 会自动创建 attached custom event，和“EventDelegate 不创建 handler declaration”边界冲突。 | 当前用 manual assign factory 绕过 UE spawner；这不是 editor-equivalent。 |
| `event_delegate.delegate_unbind` | 保留；必须是 single-handler unbind，不得静默降级为 clear all。 | 有基础节点生成；仍缺 handler/signature readback 与 object scope。 |
| `event_delegate.delegate_call` | 保留；通过 delegate signature 展开参数 pin，statement 内必须提供 args/default/link evidence。 | 有基础节点生成和 literal default 传入；缺 signature pin map/readback。 |
| `event_delegate.delegate_clear` | 保留；等价 UE “Unbind all / Clear Delegate”，无 handler。 | 有基础节点生成；contract/debug/readback 仍需补。 |

结论口径：**保留节点语义，不保留 UI 操作入口**。源文档中“右键空白图”“delegate 变量拖拽菜单”“拖拽 Pin”“组件树/SCS 右键 Add Event 菜单”等只作为 UE 源码发现路径或行为证据，不作为 BlueprintHelper 可执行能力。

---

## 1. 已查看的非归档文档与当前实现基线

按项目说明，`Develop/v*` 下归档文档未作为最新事实源。本次只参考当前 `Develop/Design`、`Develop/Plan`、`Develop/Evidence` 下的非归档文档，以及当前源码实现。

已核对的主要文档：

| 文档 | 与本次清洗相关的结论 |
|---|---|
| `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` | GraphWrite 主路径应消费 ActionContext / ActionDatabase / NodeSpawner evidence；不把 Slate `ActionMenuItem` 当作 Agent 可操作对象；EventDelegate 只 owns delegate/component-bound use-site。 |
| `BlueprintHelper/Develop/Design/BlueprintHelper_UEActionContext_InputMatrix_20260522_CN.md` | CLI 无真实拖拽 pin、selected object、UI selection，只能用 TaskSpec/data edge/symbol/typed pin 合成等价 evidence；EventDelegate 需要 delegate property、owner、binding object、component property、signature pin types。 |
| `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md` | 全局 contract 已存在，但 EventDelegate/component-bound 仍未作为完整 supported use-site 能力展开。 |
| `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_OpCoverage_CleanedCapabilityExtension_20260526_CN.md` | 已有 OpCoverage 清洗口径：UI/菜单/拖拽入口不纳入 GraphWrite；本次 EventDelegate 采用同一口径。 |
| `BlueprintHelper/Develop/Evidence/BlueprintHelper_GraphWrite_EventDelegate_UEEditorCapability_EngineSourceReadResult_20260525_CN.md` | UE 源码证据显示 6 个 use-site operation 有明确节点族与 spawner/validation 规则，但原文混有 UI 入口描述，需要清洗。 |

当前实现基线：

| 区域 | 已有实现 | 仍缺的能力 |
|---|---|---|
| AgentFace TaskSpec / compiler | 已支持 `component_bound_event`、`delegate.bind`、`delegate.assign`、`delegate.unbind`、`delegate.unbind_all`、`delegate.call` public shape，并 lower 到内部 `kind=component_bound_event` 或 `kind=delegate + delegate_operation`。 | public shape 没有写清 statement-local evidence contract；capability contract 仍未把 EventDelegate use-site 拆为 supported operations。 |
| ActionContext projection | 已把 `context_evidence` 透传；能从 snapshot 推导 delegate property path、owner class、delegate signature、component binding field path 等。 | binding object evidence 形态未统一；linked typed pin / function return / field get 等 target source 未收敛为 statement-local target evidence。 |
| EventDelegate resolver | `component_bound_event` 使用 `UBlueprintBoundEventNodeSpawner`；`bind/unbind/call/clear` 使用 `UBlueprintDelegateNodeSpawner`；`assign` 特判返回 manual factory。 | 缺 ActionDatabase/ActionFilter 等价筛选、graph compatibility hard gate、delegate flag gate；`assign` manual factory 与 UE spawner 等价性不一致。 |
| FragmentBuilder | 能 spawn primary node；bind/assign/unbind 会附加 `UK2Node_CreateDelegate`；存在 component binding 时会手动 new `UK2Node_VariableGet`。 | 不应由 EventDelegate builder 创建 component getter；CreateDelegate 只设置 handler name，未充分消费 handler function path/object scope/signature evidence；call args/readback 不完整。 |
| Readback / DebugBundle | 通用 Fragment metadata 有 statement id、semantic kind、delegate operation 等 ownership tags。 | 缺 EventDelegate-specific readback facts：delegate property、target binding object、handler scope、call args、dynamic binding、compile diagnostics correlation。 |

---

## 2. 上下文边界：TaskSpec global + statement[] 独立

### 2.1 允许的全局共享上下文

`TaskSpec` 只能作为全局共享上下文，适合承载：

- 目标资产 / Blueprint 引用。
- 目标图 / graph strategy / entry selection。
- 执行 profile、review policy、preview/execute 开关。
- 全局默认 duplicate policy 或 strictness policy，但具体 operation 的证据仍应可由 statement 本身解释。

### 2.2 statement-local 必填原则

每个 `statement[]` 必须独立表达本 statement 执行所需语义。EventDelegate 不得依赖：

- 上一个 statement 创建了什么临时 symbol。
- 当前编辑器 UI 选中了什么组件、节点或 pin。
- 用户刚刚从哪个 pin 拖拽。
- 右键菜单当前上下文、Action Menu 过滤状态或 Slate widget 状态。
- 图内扫描猜测出来的同名 custom event / handler。

允许 statement 内部使用：

- 当前 statement 的 `context_evidence`。
- 当前 statement 的 nested expression / args。
- 当前 statement 显式引用的稳定节点/pin anchor。
- 当前 statement 显式给出的 BlueprintSignature/Field/FunctionAction 依赖 evidence id。

如果后续需要跨 statement 共享 `binding_object_evidence_id`、临时 pin producer、handler signature registry、operation catalog 或 enum/delegate lookup cache，必须作为新的架构讨论，不在本清洗文档中默认开放。

---

## 3. 适合 BlueprintHelper 且当前实现没有/未闭环的能力拓展

### P0：必须补齐，否则当前节点生成不等价

| capability_id | 能力拓展 | 当前缺口 | statement-local evidence 需求 | owner / 实现位置 | 验收标准 |
|---|---|---|---|---|---|
| `event_delegate.spawner_equivalence.delegate_assign` | `delegate.assign` 通过 policy-safe UE spawner evidence 或明确替代策略完成。 | 当前 resolver 对 `assign` 返回 `ue_delegate_manual_assign_factory`，builder 手动 `NewObject<UK2Node_AssignDelegate>`，不是 UE spawner 等价。 | `delegate_property_path`、`delegate_owner_class_path`、`delegate_signature_function_path`、`target_object_evidence`、`handler_policy`。 | ActionResolution + FragmentBuilder + BlueprintSignature boundary。 | 不再静默使用 manual factory；若允许 UE auto attached custom event，DebugBundle 记录 Signature side effect；若禁止 side effect，则返回 deterministic diagnostic 或采用经讨论批准的 use-site-only 替代路径。 |
| `event_delegate.graph_and_flag_gate` | resolver 阶段强制 graph compatibility 与 delegate property flags。 | 当前 resolver 创建 spawner/节点 class 后未显式阻断 FunctionGraph 中的 `assign/component_bound_event`，也未形成 callable/assignable flag 诊断。 | `graph_type`、`blueprint_type`、`delegate_blueprint_assignable`、`delegate_blueprint_callable`、`delegate_property_path`。 | ActionResolution / ActionContext。 | `component_bound_event` 与 `assign` 在 FunctionGraph deterministic fail；`delegate.call` 只允许 `BlueprintCallable`；`bind/assign` 只允许 `BlueprintAssignable`；错误进入 DebugBundle。 |
| `event_delegate.statement_local_binding_object` | target object 统一投影为 statement-local binding object evidence。 | 当前 builder 对 component target 硬编码创建 `UK2Node_VariableGet`；无法通用支持 self、component_ref、field_get、linked pin、function return。 | `binding_object_kind=self|component_ref|field_get_ref|linked_pin_ref|function_return_ref`；对应 `component_binding_field_path`、`field_get_output_pin_ref`、`linked_pin_ref` 或同 statement expression output。 | ActionContext + Field / FunctionAction + FragmentBuilder。 | EventDelegate builder 不再 new component getter；只消费已投影 output pin 或 self target；target pin readback 能说明来源。 |
| `event_delegate.handler_signature_use_site_contract` | bind/unbind/assign 的 handler/signature use-site evidence 闭环。 | EvidenceReader 要求 handler/signature，但 builder 只把 `handler_name` 写入 `UK2Node_CreateDelegate::SetFunction`；object scope、signature compatibility 与 handler source 未完整落到节点/readback。 | `handler_name`、`handler_function_path`、`handler_scope_class_path`、`handler_source_cluster`、`signature_evidence_id`、`delegate_signature_function_path`、可选 `create_delegate_object_ref`。 | BlueprintSignature + ActionContext + DelegateLinkFragmentUtils。 | 缺 handler/signature 时 deterministic fail；`CreateDelegate` node 的 function name、object pin、delegate output link、signature compatibility 可 readback；不扫描图猜 handler。 |
| `event_delegate.component_bound_duplicate_policy` | component-bound event 重复事件策略。 | 当前实现没有明确 `fail|return_existing|replace|merge` 三态/四态策略，也没有 statement-level result。 | `component_binding_field_path`、`delegate_property_path`、`duplicate_policy=fail|return_existing`；`replace/merge` 需要另行讨论。 | ActionResolution + Readback + Safety/Review。 | 同一 `(ComponentPropertyName, DelegatePropertyName)` 已存在时默认 fail 或 return existing；不会 silent duplicate。 |
| `event_delegate.component_dynamic_binding_readback` | `UK2Node_ComponentBoundEvent` dynamic binding 成功事实读回。 | 当前主要验证节点存在，缺 `FBlueprintComponentDelegateBinding` facts。 | `component_binding_field_path`、`delegate_property_path`、`handler_function_path`。 | Readback / Testing。 | readback 输出 node class、component property、delegate property、custom function name、dynamic binding facts；compile warning/error 可定位 statement。 |
| `event_delegate.delegate_call_arg_readback` | `delegate.call` 参数 pin 与 delegate signature readback。 | 当前能传 literal default，但没有 signature pin map、arg default/link 精确验证。 | `args` map、`delegate_signature_function_path`、每个 arg 的 pin type/default/link evidence。 | FragmentBuilder + Readback。 | 未识别 arg fail；缺 required arg 或 pin 类型不兼容 fail；默认值/links 与 statement 一致。 |

### P1：能力稳定性和用户可诊断性

| capability_id | 能力拓展 | 当前缺口 | statement-local evidence 需求 | owner / 实现位置 | 验收标准 |
|---|---|---|---|---|---|
| `event_delegate.compile_diagnostic_correlation` | UE compile diagnostics 与 statement/node guid 关联。 | DelegateNodeHandlers、CreateDelegate、ComponentBoundEvent 产生的错误当前难以映射回 statement。 | `statement_id -> node_guid[]` map；operation id；handler node guid。 | Readback / DebugBundle / compile post-processing。 | DebugBundle 中每条相关 compile error/warning 有 statement id、node guid、operation id、source node class。 |
| `event_delegate.debugbundle_operation_facts` | DebugBundle 记录 EventDelegate operation facts。 | 只有通用 fragment metadata，不足以排查 delegate target/handler/signature。 | 无额外 TaskSpec；来自 resolver/builder/readback。 | DebugBundle producer。 | 至少记录 selected spawner class、node class、delegate property path、signature function path、binding object projection source、handler evidence id、graph compatibility result、duplicate handling result。 |
| `event_delegate.capability_contract_expansion` | machine-readable contract 拆出或补全 EventDelegate use-site operations。 | 当前 `graphwrite-capability-contract.ts` 中 delegate component-bound 仍在 event cluster 且 `discussion-gated`，没有 6 个 use-site operation 的 requiredEvidenceKeys。 | 不需要新 TaskSpec 字段；使用本文件第 4 节字段。 | AgentFaceService task-core schema/contract。 | Contract 有 `event_delegate.component_bound_event`、`delegate.bind/assign/unbind/call/clear` 状态；supported/gated 与实现状态一致；测试覆盖 required evidence keys。 |
| `event_delegate.statement_schema_validation` | Agent-facing TaskSpec 对 EventDelegate evidence 做结构化校验。 | 当前 public shape 只要求 component/delegate/handler 或 target/delegate/handler，复杂 evidence 都在松散 `context_evidence`。 | `context_evidence` 中稳定 keys；可先用 documented contract + runtime diagnostic，不必一口气 Zod 强类型。 | Task compiler + runtime validator。 | 缺关键 evidence 的 preview/execute 返回稳定错误码；不让 resolver 通过图扫描补齐。 |
| `event_delegate.review_scope_keep_graph_block` | 保持 graph_block 级 Review，不新增 per-delegate review target，同时把细节放 DebugBundle。 | 源码风险中指出 per-delegate review target 会过细；当前仍需固化策略。 | 无。 | Review evidence policy。 | Review target 仍为 graph surface / graph block；delegate details 只进入 DebugBundle/readback facts。 |

### P2：可实现但需要先讨论上下文扩展

| capability_id | 能力拓展 | 为什么需要讨论 | 建议默认处理 |
|---|---|---|---|
| `event_delegate.binding_object_function_return_target` | delegate target 来自 function return object。 | 如果 function call 是同一 statement 内 nested expression，可以作为 statement-local composition；如果依赖上一 statement 的输出，就违反 statement 独立上下文。 | 第一版只消费同 statement expression output 或显式 stable pin anchor；跨 statement symbol 需另开讨论。 |
| `event_delegate.binding_object_linked_pin_target` | delegate target 来自已存在 typed object pin。 | 可行，但 pin anchor 必须稳定且在 statement 内显式给出；不能模拟“从 pin 拖拽”。 | 接受 `linked_pin_ref` / `node_ref + pin_ref`，并在 readback 校验 pin type 兼容。 |
| `event_delegate.duplicate_replace_or_merge` | component-bound duplicate event replace/merge。 | 会修改已有用户节点或 handler body，涉及 Review、rollback、Safety。 | 默认只允许 `fail` 或 `return_existing`。 |
| `event_delegate.assign_auto_attached_custom_event` | 使用 UE `UK2Node_AssignDelegate::PostPlacedNewNode` 自动创建 attached custom event。 | 这是 handler declaration side effect，属于 BlueprintSignature 边界；和“EventDelegate 只写 use-site”冲突。 | 默认 discussion-gated；除非用户批准 side effect 记录方式，否则不作为自动执行能力。 |

---

## 4. 清洗后的 per-operation TaskSpec evidence contract

### 4.1 `component_bound_event`

最低 statement 形态：

```json
{
  "kind": "component_bound_event",
  "component": "ButtonComponent",
  "delegate": "OnClicked",
  "handler": "HandleButtonClicked",
  "context_evidence": {
    "component_binding_owner_class_path": "/Game/BP_Door.BP_Door_C",
    "component_property_name": "ButtonComponent",
    "component_binding_field_path": "/Game/BP_Door.BP_Door_C:ButtonComponent",
    "component_class_path": "/Script/Engine.ButtonComponent",
    "delegate_owner_class_path": "/Script/Engine.ButtonComponent",
    "delegate_property_name": "OnClicked",
    "delegate_property_path": "/Script/Engine.ButtonComponent:OnClicked",
    "delegate_signature_function_path": "/Script/Engine.ActorComponent:ComponentOnClickedSignature__DelegateSignature",
    "delegate_signature": "/Script/Engine.ActorComponent:ComponentOnClickedSignature__DelegateSignature",
    "handler_name": "HandleButtonClicked",
    "handler_scope_class_path": "/Game/BP_Door.BP_Door_C",
    "handler_function_path": "/Game/BP_Door.BP_Door_C:HandleButtonClicked",
    "handler_source_cluster": "BlueprintSignature",
    "signature_evidence_id": "sig:button:on_clicked",
    "duplicate_policy": "fail"
  }
}
```

规则：

- `component` / `delegate` 可作为 human-readable short fields，但不能替代 stable field path。
- 只支持 Actor Blueprint 中可解析为 `FObjectProperty` 的组件属性；instance-only component 只允许 diagnostic-only。
- 默认 duplicate policy：`fail`；`return_existing` 可作为安全选项；`replace/merge` 需要讨论。

### 4.2 `delegate.bind` / `delegate.unbind`

最低 statement 形态：

```json
{
  "kind": "delegate.bind",
  "target": "self",
  "delegate": "OnHealthChanged",
  "handler": "HandleHealthChanged",
  "context_evidence": {
    "binding_object_kind": "self",
    "delegate_owner_class_path": "/Game/BP_Health.BP_Health_C",
    "delegate_property_name": "OnHealthChanged",
    "delegate_property_path": "/Game/BP_Health.BP_Health_C:OnHealthChanged",
    "delegate_signature_function_path": "/Game/BP_Health.BP_Health_C:OnHealthChanged__DelegateSignature",
    "delegate_signature": "/Game/BP_Health.BP_Health_C:OnHealthChanged__DelegateSignature",
    "handler_name": "HandleHealthChanged",
    "handler_scope_class_path": "/Game/BP_Health.BP_Health_C",
    "handler_function_path": "/Game/BP_Health.BP_Health_C:HandleHealthChanged",
    "handler_source_cluster": "BlueprintSignature",
    "signature_evidence_id": "sig:on_health_changed"
  }
}
```

`delegate.unbind` 必须额外 lower 为 `delegate_operation=unbind` + `unbind_mode=single`。缺 handler 时不能降级到 `clear`。

### 4.3 `delegate.assign`

最低 statement 形态同 `delegate.bind`，但必须多一个策略字段：

```json
{
  "kind": "delegate.assign",
  "target": "self",
  "delegate": "OnHealthChanged",
  "handler": "HandleHealthChanged",
  "context_evidence": {
    "handler_policy": "require_projected_handler",
    "assign_auto_attached_event_policy": "forbid"
  }
}
```

规则：

- 如果 `assign_auto_attached_event_policy=allow`，必须把 UE spawner 的 attached custom event side effect 写入 BlueprintSignature/Review/DebugBundle。
- 如果 `assign_auto_attached_event_policy=forbid`，直接调用 UE `UK2Node_AssignDelegate` spawner 可能不符合边界，需要先讨论替代实现策略。
- 不应继续无说明地使用 manual factory，因为它绕过 UE spawner 行为；但也不能无条件启用 UE auto custom event side effect。

### 4.4 `delegate.call`

最低 statement 形态：

```json
{
  "kind": "delegate.call",
  "target": "self",
  "delegate": "OnHealthChanged",
  "args": {
    "NewHealth": { "kind": "literal", "type": "float", "value": "75.0" }
  },
  "context_evidence": {
    "binding_object_kind": "self",
    "delegate_owner_class_path": "/Game/BP_Health.BP_Health_C",
    "delegate_property_name": "OnHealthChanged",
    "delegate_property_path": "/Game/BP_Health.BP_Health_C:OnHealthChanged",
    "delegate_signature_function_path": "/Game/BP_Health.BP_Health_C:OnHealthChanged__DelegateSignature",
    "delegate_blueprint_callable": "true"
  }
}
```

规则：

- `args` 只允许 delegate signature 中存在的 input pins。
- 必须验证 pin type、default value、link target。
- `delegate_blueprint_callable=false` 时 deterministic fail。

### 4.5 `delegate.clear` / public `delegate.unbind_all`

Agent-facing 推荐保留 `delegate.unbind_all`，compiler lowering 到内部：

```json
{
  "kind": "delegate",
  "delegate_operation": "clear",
  "unbind_mode": "all"
}
```

规则：

- 不允许 `handler` / `handler_name`。
- DebugBundle 显示 UE title `Unbind all / Clear Delegate`，避免用户误解为 single-handler unbind。

---

## 5. 不适合纳入 EventDelegate 能力的项目

这些项已在伴随的 marked 源文档中原位标记。

| 原文能力/入口 | 清洗结论 | 原因 |
|---|---|---|
| 右键空白图 Action Menu | UI-only excluded；保留其背后的 NodeSpawner evidence。 | BlueprintHelper 不模拟右键菜单；statement 必须直接给 semantic/evidence。 |
| delegate 变量/组件拖拽菜单 | UI-only excluded；保留最终 `bind/assign/unbind/call/clear` operation。 | 没有真实 drag/drop context；只能用 statement-local target/delegate evidence。 |
| 拖拽 Pin / DraggedFromPins | UI-only excluded；可用 typed data edge、linked pin ref、nested expression output 合成，但不能伪装成 UI 拖拽。 | CLI/MCP 无 Slate pin drag 事件。 |
| SCS 组件树右键 Add Event 菜单 | UI-only excluded；保留 `component_bound_event`。 | 不调用组件面板命令；只使用 stable component property evidence。 |
| selected object / selected component / View existing event | UI-only excluded；已有事件策略用 duplicate readback 表达。 | TaskSpec 没有编辑器选中态。 |
| custom event / override / native event declaration | Not EventDelegate；归 BlueprintSignature。 | EventDelegate 只消费 projected handler/signature evidence。 |
| handler 选择 UI / Create Event 独立入口 | Not first-class EventDelegate operation。 | `UK2Node_CreateDelegate` 只作为 bind/unbind use-site helper。 |
| UMG designer widget event、Details panel delegate binding、Animation Blueprint events | excluded / other owner。 | 非普通 Blueprint graph body use-site。 |
| timer/latent/async delegate helper | GenericSchedule / FunctionAction。 | 非 multicast delegate property node family。 |
| function return target 上游节点自动创建 | discussion-gated。 | EventDelegate 不应创建上游 function/field；只能消费同 statement composition 或显式 stable pin evidence。 |

---

## 6. 建议实现顺序

1. **先收敛 resolver hard gates**：graph type、Blueprint type、delegate flags、duplicate check、clear/unbind mode。  
2. **再处理 binding object projection**：移除 EventDelegate builder 内 component getter 创建；改为消费 `binding_object_kind + evidence`。  
3. **补 handler/signature use-site contract**：`CreateDelegate` object pin、function path、signature compatibility、missing evidence deterministic failure。  
4. **处理 `assign` 策略**：先和用户确认是否允许 UE auto attached custom event side effect；确认后再改 manual factory。  
5. **补 readback/debug**：delegate property、target object、handler、call args、component dynamic binding、compile diagnostic correlation。  
6. **更新 capability contract 和测试矩阵**：将 supported/gated 状态与真实实现同步。

---

## 7. 需要用户先确认的决策

| decision_id | 决策点 | 默认建议 |
|---|---|---|
| ED-A1 | `delegate.assign` 是否允许 UE 自动创建 attached custom event？ | 默认不允许；先讨论 side effect 如何归 BlueprintSignature/Review。 |
| ED-A2 | component-bound duplicate 默认策略是 `fail` 还是 `return_existing`？ | 默认 `fail`；`return_existing` 可作为安全增强。 |
| ED-A3 | binding object evidence 是否用统一 `binding_object_evidence_id`？ | 默认不用跨 statement id；每个 statement 内显式写 evidence。 |
| ED-A4 | handler/signature 最小 contract 是只接受 function path，还是也接受 event node ref / custom event ref / CreateDelegate ref？ | 默认接受 `handler_function_path + handler_scope_class_path + signature_evidence_id`；其他形式后续扩展。 |
| ED-A5 | FunctionGraph 是否允许 bind/unbind/call/clear？ | 按 UE 规则允许；`assign/component_bound_event` 禁止。也可保守第一版只允许 EventGraph。 |
| ED-A6 | linked pin / function return target 是否允许引用其他 statement 的产物？ | 默认不允许；只允许同 statement nested expression 或稳定 existing pin anchor。 |

---

## 8. 输出文件说明

本文件是能力拓展文档。伴随文件：

- `BlueprintHelper_GraphWrite_EventDelegate_UEEditorCapability_EngineSourceReadResult_20260525_CN_MARKED.md`：在原探索结果结构上做原位清洗标记，标出 UI-only/excluded/discussion-gated 项。
