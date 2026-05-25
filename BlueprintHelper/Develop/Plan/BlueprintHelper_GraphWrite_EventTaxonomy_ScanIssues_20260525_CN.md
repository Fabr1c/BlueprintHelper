# BlueprintHelper GraphWrite Event Taxonomy Scan Issues 2026-05-25

## Summary

本文记录 2026-05-25 对 GraphWrite event taxonomy 的只读 scan 结果，目标是分清三类入口：

- `custom_event`：BlueprintSignature 拥有声明/签名生命周期；GraphWrite 只在已有声明或显式依赖证据后写入 body、调用或 use-site。
- `override/native event`：BlueprintSignature 拥有 UE class/native function 事件声明；GraphWrite 只引用 Signature 结果并保留 taxonomy evidence。
- `delegate/component bound event`：委托绑定、组件绑定事件，应走 EventDelegate action cluster。

本次仅导出问题与证据，不改代码，不定义最终修复方案。

## Current Classification

| 分类 | 当前主要入口 | 当前归属判断 | 状态 |
| --- | --- | --- | --- |
| `custom_event` | `BlueprintSignature.ensure_custom_event`、`logic_spec.entry.kind=custom_event`、GraphWrite append/merge/replace body/use-site | 声明归 BlueprintSignature；GraphWrite 只消费声明后的 body/use-site evidence | 边界需硬化 |
| `override/native event` | `BlueprintSignature.ensure_override_event`、`native_event/override_event` signature policy | 声明归 BlueprintSignature；GraphWrite 只消费 event reference/evidence | reference/evidence 需保留 taxonomy |
| `delegate/component bound event` | `component_bound_event` / `delegate` statement、ActionContext demand/evidence、EventDelegateActionCluster | 应归入 EventDelegate action cluster | 边界基本清晰 |

## Detailed Issues

### EVTAX-001: custom_event declaration ownership 需要保持 Signature-owned

严重程度：Medium-Low

现象：
- `EBlueprintHelperGraphStatementKind` 没有 `CustomEvent` 或 `Event` declaration kind。
- 这不是 GraphWrite 必须补齐的声明能力缺口；现有设计已经规定 `ensure_custom_event` 由 BlueprintSignature owning path 管理。
- 当前需要硬化的是边界表达：GraphWrite 可以写 custom event body/use-site，但不能创建、修改或伪造 custom event declaration。

证据：
- `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - `EventDelegate / Signature Ownership Boundary` 规定 `ensure_custom_event`、`ensure_override_event` 归 Signature owning path。
  - 2026-05-24 同步记录进一步确认：`custom_event` / `override_event` / `native_event` 不作为 GraphWrite/EventDelegate public declaration taxonomy。
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - `EBlueprintHelperGraphStatementKind` 定义包含 `ComponentBoundEvent`、`Delegate`，不包含 `CustomEvent`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
  - `ParseStatementKind()` 只解析 `component_bound_event` / `delegate`，不解析 `custom_event`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
  - semantic entry 仍用 `EParsedBlueprintNodeType::CustomEvent` 和 `K2Node_CustomEvent` 建模。

影响：
- 如果继续把它描述成 GraphWrite 缺失能力，后续实现容易把 Signature declaration ownership 错误迁入 GraphWrite/EventDelegate。
- 当前 GraphWrite custom event body/use-site 写入需要更强 dependency/evidence gate，避免在缺少 Signature 声明时伪造成功。

已同意方向：
- 不新增 GraphWrite public declaration taxonomy：不新增 `custom_event_declaration`，不把 `ensure_custom_event` 放进 GraphWrite/EventDelegate。
- GraphWrite 只消费 `BlueprintSignature.ensure_custom_event` 后的 declaration/signature evidence。
- 后续实现应补强 dependency/evidence gate：缺少 custom event declaration evidence 时，GraphWrite body/use-site deterministic fail。

仍需讨论：
- 是否需要新增 use-site-only 语义，例如 `custom_event_body` / `custom_event_call`，用于表达 body 写入和调用。
- 如果保留 `logic_spec.entry.kind=custom_event`，它应如何显式引用 Signature step 产出的 declaration evidence。

### EVTAX-002: override/native event declaration ownership 需要保持 Signature-owned

严重程度：Medium

现象：
- `ensure_override_event` 已经能处理 `native_event` / `override_event`，并创建 `UK2Node_Event`。
- 这不是 GraphWrite 必须接管的 declaration 能力；现有设计要求 event lifecycle taxonomy 仍由 BlueprintSignature owning path 管理。
- 当前需要补强的是 GraphWrite reference/evidence/readback：GraphWrite parser/classification 只区分 `EParsedBlueprintNodeType::Event` 和 `CustomEvent`，不携带 `native_event` / `override_event` 分级。
- GraphWrite ActionResolution 中 `EBlueprintHelperActionSemanticKind::Event` 存在字符串映射，但没有实际 declaration cluster ownership。

证据：
- `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - 2026-05-24 同步记录确认：`custom_event` / `override_event` / `native_event` 不作为 GraphWrite/EventDelegate public declaration taxonomy。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.cpp`
  - `EnsureOverrideEvent()` 校验 `native_event` / `override_event`。
  - `CreateOverrideEventNode()` 使用 `UK2Node_Event`、`SetExternalMember()`、`bOverrideFunction=true`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintSignature/Utils/BlueprintHelperSignatureMutationUtils.cpp`
  - `ResolveNativeOrOverrideEventName()` 映射 `ReceiveBeginPlay`、`ReceiveTick`、`ReceiveActorBeginOverlap`。
  - `FindOverrideEventInBlueprint()` 使用 `FindOverrideForFunction()` 和 `bOverrideFunction`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h`
  - `FParsedEventReference` 只有 `EventName` 与 params，没有 event taxonomy 字段。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - `EBlueprintHelperActionSemanticKind::Event` 只映射到 semantic family，没有 cluster demand 入口。

影响：
- 如果把该问题描述为 GraphWrite 缺少 declaration 能力，后续实现容易错误迁移 BlueprintSignature 职责。
- GraphWrite payload 中若表达 `K2Node_Event`，当前只能得到 generic `Event`，无法在 body/readback/review evidence 层可靠保留 native/override taxonomy。
- Replace、readback、review evidence 层可能无法区分 UE override event 与普通事件。

已同意方向：
- 继续由 BlueprintSignature 独占 declaration ownership：GraphWrite 不新增 `override_event_declaration` / `native_event_declaration`，不接管 `ensure_override_event`。
- GraphWrite 只消费 `BlueprintSignature.ensure_override_event` 后的 event function / graph / taxonomy evidence。
- 建立统一 event reference model，至少保留 `{ event_name, event_taxonomy, source_cluster }` 或等价 metadata，避免 GraphWrite 将 `UK2Node_Event` 降级成不可区分的 generic `Event`。

仍需讨论：
- 是否需要新增 use-site-only 语义，例如 `native_event_body` / `override_event_body`，用于明确 body 写入但不创建 declaration。
- Replace/readback/review evidence 是否共用同一 event reference model，还是分别做最小字段保留。

### EVTAX-003: Replace 路径同时匹配 UK2Node_Event 和 UK2Node_CustomEvent，语义边界偏弱

严重程度：High

现象：
- Replace service 的 entry 匹配同时接受 `UK2Node_CustomEvent`、`UK2Node_Event`、`UK2Node_FunctionEntry`。
- `EventBody` / `CustomEventBody` / `Graph` 路径存在并列处理，当前边界更像按节点名匹配，而不是按 event taxonomy 匹配。

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
  - `NodeMatchesEntryName()` 对 `UK2Node_CustomEvent` 使用 `CustomFunctionName` 匹配。
  - 同一函数对 `UK2Node_Event` 使用 `GetFunctionName()` / `EventReference.GetMemberName()` 匹配。
  - replace scope 包含 `event_body` / `custom_event_body` / `graph` 相关分支。

影响：
- custom event 与 override/native event 在 replace 路径上可能被同一套 entry-name 规则耦合。
- 如果后续新增 GraphWrite event taxonomy，Replace 是必须先重新划分 ownership 的高风险入口。

已同意方向：
- Replace 按 scope 拆 entry resolver，不再让 `custom_event_body`、`event_body`、`graph` 共用模糊 entry-name matcher。
- `custom_event_body` resolver 只能匹配 `UK2Node_CustomEvent`，并要求 Signature-owned custom event declaration evidence 或等价 existing declaration evidence。
- `event_body` resolver 只能匹配 `UK2Node_Event`，并要求 EVTAX-002 的 event reference/taxonomy evidence，至少能区分 `native_event` / `override_event`。

仍需讨论：
- `graph` scope 是否仍允许 broad graph-level replace，还是必须禁止匹配 entry body。
- Replace 的 result/review evidence 是否也要带 `event_taxonomy`，与 EVTAX-004 合并处理。

### EVTAX-004: review/evidence 层把 custom_event 折叠进 generic event

严重程度：Medium

现象：
- GraphFragment evidence 将 `custom_event` 与 `event` 一起归到 event scope。
- Evidence utils 用 `event_name` / `event` / `custom_event_name` 混合读取事件名。

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.cpp`
  - `custom_event`、`event`、`eventgraph` 归到同一个 event review scope。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentEvidenceUtils.cpp`
  - 多处用 `{ event_name, event, custom_event_name }` 作为同一组 metadata fallback。

影响：
- Review target、DebugBundle、readback summary 中可能看不出 custom event 与 override/native event 的差异。
- 如果后续需要精确 Reject/Accept 或 scope ownership，该折叠会增加歧义。

已同意方向：
- Review/UI 层可以继续使用 generic `Event` scope，避免扩大 UI grouping、Reject/Accept target、snapshot restore 的变更面。
- GraphWrite evidence metadata 必须保留 `event_taxonomy`，至少区分 `custom_event`、`native_event`、`override_event`。
- EVTAX-002 的 event reference model、EVTAX-003 的 Replace result/review evidence 应共用该 taxonomy 字段，避免各自维护解释。

仍需讨论：
- `delegate/component_bound_event` 是否需要在同一字段中表达为 `delegate_event`，还是保持 EventDelegate 独立 evidence family。
- 旧 metadata key `{ event_name, event, custom_event_name }` 是否只作为兼容读取，不再作为新写入主字段。

### EVTAX-005: EventDelegate cluster ownership 清晰，但 handler fallback 引入 custom event 类型耦合

严重程度：Medium-Low

现象：
- EventDelegateActionCluster 只 owns `ComponentBoundEvent` / `Delegate`，这是正确边界。
- `EventDelegateUseSiteEvidence` 在解析 handler 时，会扫描 `UK2Node_CustomEvent` 作为 handler function 的 fallback。

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
  - `OwnsSemanticKind()` 只返回 `ComponentBoundEvent` / `Delegate`。
  - `ComponentBoundEvent` 通过 `UBlueprintBoundEventNodeSpawner`。
  - `Delegate` 的 `bind/assign/unbind/call/clear` 通过 delegate spawner 或 manual assign factory。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
  - `ResolveHandlerFunction()` 在 `FindFunctionByName()` 失败后扫描 `UK2Node_CustomEvent`。

影响：
- 这不是 ownership 混用，但新增入口时容易把“delegate handler 是 custom event”误解为“EventDelegate owns custom_event creation”。
- 当前实现依赖已有 handler，不应伪造或自动创建 custom event handler。

已同意方向：
- EventDelegate resolver 移除/禁止 `UK2Node_CustomEvent` handler fallback scan；handler evidence 缺失时 deterministic fail，不在 EventDelegate 内修复上下文。
- 采用拆分方案：BlueprintSignature 负责 handler declaration/signature lifecycle，产出权威 handler evidence；ActionContext projection 只负责把 Signature evidence 转成 GraphWrite/EventDelegate 所需 use-site evidence；EventDelegate 只消费投影后的 handler reference evidence。
- 推荐 evidence 边界至少包含 `handler_name`、`handler_scope_class_path`、`handler_function_path`、`handler_source_cluster`。如果 handler 缺失，必须通过显式 BlueprintSignature dependency step 创建/确认，不允许 EventDelegate 内部创建或扫描修复。

仍需讨论：
- 是否需要为 handler dependency 增加统一 schema 字段，例如 `handler_dependency_id` / `signature_evidence_id`，让 GraphWrite 能稳定引用前置 Signature step。

### EVTAX-006: legacy/private parsed node pipeline 仍能解析 Event/CustomEvent/Delegate aliases，易与 GraphStatement 主线混淆

严重程度：Medium

现象：
- Private parsed DTO 已不再公开，但 private pipeline 仍保留 `EParsedBlueprintNodeType::{CustomEvent, Event, AddDelegate, RemoveDelegate, ClearDelegate, AssignDelegate, CreateDelegate, ComponentBoundEvent}`。
- `BlueprintGraphJsonParser` 接受 `BindEvent`、`CreateEvent` 等 aliases，并映射到 delegate parsed node types。
- 这些 parser aliases 与 GraphStatement `delegate` / `component_bound_event` 主线是相邻语义，但不是同一入口。

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h`
  - parsed enum 同时包含 event、custom event、delegate node variants。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.cpp`
  - `K2Node_CustomEvent` / `CustomEvent` / `Custom_Event` -> `CustomEvent`。
  - `K2Node_Event` / `Event` -> `Event`。
  - `BindEvent` -> `AddDelegate`。
  - `CreateEvent` -> `CreateDelegate`。
  - `K2Node_ComponentBoundEvent` / `ComponentBoundEvent` -> `ComponentBoundEvent`。

影响：
- 如果后续从 parser aliases 直接补行为，可能绕过 GraphStatement / ActionContext / ActionResolution。
- 需要明确 private parsed pipeline 是 compatibility/readback 边界，还是仍参与新 GraphWrite creation path。

需要讨论：
- 新增 event taxonomy 是否必须禁止走 parsed-node creation fast path。
- parser aliases 是否应只用于 read/import/legacy conversion，而不能表达新的 write semantics。

## Non-Issues Confirmed By Scan

### NI-001: EventDelegateActionCluster 没有声明 ensure_custom_event / ensure_override_event

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteFunctionFieldUnifiedSmokeTests.cpp`
  - 已检查 EventDelegate source 不包含 `ensure_custom_event`、`ensure_override_event`、`native_event`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - 边界测试禁止 `ensure_custom_event`、`ensure_override_event`、`fake_delegate_success`。

判断：
- 当前 EventDelegate cluster 没有伪造 custom/override success。

### NI-002: delegate/component-bound event 已有明确 ActionContext -> EventDelegateActionCluster 路径

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - `ComponentBoundEvent` / `Delegate` demand 的 `ClusterKind=EventDelegateAction`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
  - `ComponentBoundEvent` / `Delegate` 分发到 `FBlueprintHelperEventDelegateFragmentBuilder`。

判断：
- delegate/component-bound 的 cluster ownership 当前基本正确。

## Discussion Questions

1. GraphWrite 是否需要新增第一等 `custom_event` statement，还是继续把 custom event 限定为 `logic_spec.entry`？
2. `override/native event` 是否继续属于 BlueprintSignature cluster，GraphWrite 只通过 dependency fact 引用？
3. 如果 GraphWrite 纳入 `override/native event`，需要新增 `EventActionCluster`，还是扩展现有 EventDelegate cluster 以外的新 resolver？
4. Replace 是否应先拆 entry resolver，再讨论具体修复？
5. Evidence 是否需要新增 `event_taxonomy` 字段，避免 Review 层继续折叠 custom/native/override？
6. EventDelegate handler 缺失时，是否必须返回 missing evidence，而不是在 EventDelegate 内创建 handler？

## Suggested Discussion Order

1. 先决定 `override/native event` 是否属于 GraphWrite write taxonomy。
2. 再决定 `custom_event` 是独立 statement 还是 entry-only dependency。
3. 然后处理 Replace 的 entry resolver 拆分。
4. 最后处理 evidence metadata 的 taxonomy 保留策略。
