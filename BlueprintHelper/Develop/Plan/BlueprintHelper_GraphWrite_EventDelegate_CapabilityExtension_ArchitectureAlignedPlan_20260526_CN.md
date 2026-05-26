# BlueprintHelper GraphWrite EventDelegate Capability Extension Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 在未实施原计划的前提下，替代原 `BlueprintHelper_EventDelegate_CapabilityExtension_ImplementationPlan_20260526.md`，补齐 GraphWrite / EventDelegate use-site 能力：`component_bound_event`、`delegate.bind`、`delegate.assign`、`delegate.unbind`、`delegate.call`、`delegate.clear/unbind_all`。

**Architecture:** EventDelegate 只负责 use-site，不负责 event declaration、custom event creation、handler creation、handler signature mutation。所有 EventDelegate operation 都映射到既有 `EventDelegateActionCluster`，`bind/assign/unbind/call/clear` 是 `delegate_operation` 二级语义；operation-specific 数据进入 `ContextEvidence` 并由 `FBlueprintHelperEventDelegateUseSiteEvidenceReader` 读取。`delegate.assign` 保留 `ue_delegate_manual_assign_factory`，禁止依赖 UE Assign spawner 的自动 Custom Event 副作用。

**Tech Stack:** Unreal Engine 5.3+ Editor C++ plugin、BlueprintGraph/K2/NodeSpawner、BlueprintHelper GraphWrite、AgentFaceService task-core TypeScript/Zod、UE Automation Tests、Node test runner。

---

## 0. 本计划状态

本文件是原 EventDelegate 计划的**实施前替代版**，不是对已实施代码的修复计划。执行时直接按本文实施，不执行原计划中与本文冲突的任务。

**替代原计划中的错误方向：**

- 不移除 `ue_delegate_manual_assign_factory`。
- 不新增 `assign_auto_attached_event_policy=allow`。
- 不让 GraphWrite/EventDelegate 通过 UE Assign spawner 副作用创建 Custom Event。
- 不把 handler/signature declaration 或 mutation 纳入 GraphWrite/EventDelegate。
- 不让 FragmentBuilder 创建 binding object 的 `UK2Node_VariableGet`；binding object 必须由 projected evidence 或上游 pin 给出。
- 不把 duplicate `replace/merge` 当默认 GraphWrite mutation 能力；默认只支持 `fail` / `return_existing`，`replace/merge` 稳定拒绝。

---

## 1. Scope and Capability Matrix

| capability_id | Status | Runtime owner | Required boundary |
|---|---|---|---|
| `event_delegate.component_bound_event` | supported | `EventDelegateActionCluster` | existing component/delegate property evidence + graph/flag gate |
| `event_delegate.delegate_bind` | supported | `EventDelegateActionCluster` | binding object + delegate property + existing handler/signature evidence |
| `event_delegate.delegate_assign` | supported | `EventDelegateActionCluster` | manual assign factory + existing handler/signature evidence; no Custom Event side effect |
| `event_delegate.delegate_unbind` | supported | `EventDelegateActionCluster` | binding object + delegate property + handler evidence; missing handler fails |
| `event_delegate.delegate_call` | supported | `EventDelegateActionCluster` | delegate signature arg validation + defaults/links readback |
| `event_delegate.delegate_clear` / `delegate.unbind_all` | supported | `EventDelegateActionCluster` | delegate property + binding object; handler evidence not required |
| `event_delegate.component_bound_duplicate_policy.fail` | supported | `EventDelegateActionCluster` | duplicate returns deterministic failure |
| `event_delegate.component_bound_duplicate_policy.return_existing` | supported | `EventDelegateActionCluster` | existing binding must be read back and reported |
| `event_delegate.component_bound_duplicate_policy.replace/merge` | gated rejected | none | future mutation-owner plan only |

### Excluded domains

```text
Action Menu simulation, delegate drag menus, dragged pins, Slate selected object/component state,
SCS right-click execution, UMG designer widget event, Details panel delegate binding,
Animation Blueprint events, timer/latent/async delegate helpers,
custom/native/override event declaration, handler chooser/Create Event as first-class GraphWrite operation,
automatic upstream function/field/handler creation
```

---

## 2. Architecture Gates

| Gate | Rule | Enforcement |
|---|---|---|
| Use-site only | EventDelegate never creates or mutates declarations/signatures | schema tests + C++ tests |
| Runtime ownership | operations map to `EventDelegateActionCluster`, not new first-stage semantic | contract tests |
| Evidence locality | delegate-specific fields stay in `ContextEvidence` / focused reader DTO | header grep guard |
| Assign boundary | `delegate.assign` uses manual factory; UE Assign spawner side-effect path is forbidden | resolver tests |
| Binding object | builder consumes projected target pins/evidence; it does not synthesize variable getter | fragment tests |
| Review scope | Review remains graph/block scoped; delegate facts go to DebugBundle/readback | result payload tests |
| No implicit downgrade | `delegate.unbind` missing handler fails; it never becomes `clear` | operation tests |

---

## 3. File Structure

### AgentFaceService contract / schema

- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - Publish EventDelegate as logical capability group with `runtimeCluster: 'EventDelegateAction'`.
  - Model `delegate_operation` as second-stage operation: `bind | assign | unbind | call | clear`.
  - Remove any `assign_auto_attached_event_policy` contract.
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
  - Assert EventDelegate operations, required evidence keys, and forbidden side-effect policy absence.
- Modify: `AgentFaceService/task-core/src/tests/task/task-contract-graphwrite.test.ts`
  - Add TaskSpec lowering tests for bind/assign/unbind/call/clear evidence.
- Modify: `AgentFaceService/task-core/src/tests/task/task-protocol-graphwrite-contract.test.ts`
  - Add stable Agent-facing diagnostic tests for missing handler/signature/binding evidence.

### ActionContext and EventDelegate evidence

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
  - Keep `FBlueprintHelperEventDelegateUseSiteEvidence` as the focused DTO and read from `Request.ContextEvidence`.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.cpp`
  - Centralize graph compatibility, delegate flags, duplicate policy, unbind/clear mode, assign boundary, P2 gates.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - Project event/delegate facts into evidence map and semantic hash without expanding core DTO field groups.

### Resolver / Fragment / readback

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
  - Apply policy gates before spawner/factory selection; preserve manual assign factory.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.cpp`
  - Resolve only explicit/projection-backed targets: `self`、`component_ref`、`field_get_ref`、`linked_pin_ref`、same-statement `function_return_ref`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp`
  - Consume projected target pins; apply call args/defaults/links; no builder-created component getter.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.cpp`
  - Expand CreateDelegate link request with handler path/scope/signature/object evidence and readback pins.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.cpp`
  - Collect node class, delegate property, target object, handler, call args, component dynamic binding, node guid, compile diagnostics.

### Tests and docs

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateUseSiteEvidenceTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateFragmentBuilderTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateDebugBundleTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_EventDelegate_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md`
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`

不要修改 `BlueprintHelper/Develop/v*` 归档文档。

---

## 4. Tasks

### Task 1: Contract 先行，发布 EventDelegate logical operation group

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
- Modify: `AgentFaceService/task-core/src/tests/task/task-protocol-graphwrite-contract.test.ts`

- [x] **Step 1: 写失败测试：EventDelegate operations runtime owner 固定**

Add assertions:

```ts
const group = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups.find((item) => item.id === 'event_delegate');
assert.ok(group);

for (const op of ['component_bound_event', 'delegate.bind', 'delegate.assign', 'delegate.unbind', 'delegate.call', 'delegate.clear']) {
  const operation = group.operations.find((item) => item.id === `event_delegate.${op}`);
  assert.ok(operation, op);
  assert.equal(operation.runtimeCluster, 'EventDelegateAction');
  assert.ok(operation.semanticKind === 'component_bound_event' || operation.semanticKind === 'delegate');
}
```

- [x] **Step 2: 写失败测试：禁止 assign auto-attached policy**

```ts
const serialized = JSON.stringify(GRAPHWRITE_CAPABILITY_CONTRACT);
assert.equal(serialized.includes('assign_auto_attached_event_policy'), false);
assert.equal(serialized.includes('attached_custom_event'), false);
```

- [x] **Step 3: 实现 contract**

Required evidence keys:

```text
component_bound_event:
  component_binding_owner_class_path, component_property_name, component_binding_field_path,
  component_class_path, delegate_owner_class_path, delegate_property_name, delegate_property_path,
  delegate_signature_function_path, handler_function_path, handler_source_cluster,
  signature_evidence_id, duplicate_policy

delegate.bind / assign / unbind:
  binding_object_kind, binding_object_evidence_id, delegate_owner_class_path,
  delegate_property_name, delegate_property_path, delegate_signature_function_path,
  handler_function_path, handler_source_cluster, signature_evidence_id

delegate.call:
  binding_object_kind, binding_object_evidence_id, delegate_owner_class_path,
  delegate_property_name, delegate_property_path, delegate_signature_function_path,
  call_arg_policy

delegate.clear:
  binding_object_kind, binding_object_evidence_id, delegate_owner_class_path,
  delegate_property_name, delegate_property_path
```

- [x] **Step 4: Run TypeScript tests**

```bash
cd AgentFaceService/task-core
npm test -- graphwrite-capability-contract
```

- [x] **Step 5: Commit**

```bash
git add AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts \
        AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts \
        AgentFaceService/task-core/src/tests/task/task-protocol-graphwrite-contract.test.ts
git commit -m "feat(graphwrite): publish event delegate use-site contract"
```

### Task 2: Focused EventDelegate evidence reader，避免 core DTO 膨胀

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateUseSiteEvidenceTests.cpp`

- [x] **Step 1: 写 missing evidence tests**

Required deterministic errors:

```text
missing_delegate_property_evidence
missing_binding_object_evidence
missing_handler_evidence
missing_signature_evidence
invalid_delegate_operation
handler_not_allowed_for_clear
handler_required_for_unbind
```

- [x] **Step 2: Implement reader rules**

Reader reads only:

```text
Request.Semantic.Kind
Request.Semantic.ScheduleOperation / FunctionOperation only if already projected as second-stage op
Request.ContextEvidence["event_delegate.*"]
Request.Blueprint / Request.TargetGraph only for validation context, not for discovery
```

Reader must not:

```text
scan TaskSpec JSON
create handler
choose handler
mutate signature
read Slate/UI selected object
```

- [x] **Step 3: Add core field guard**

Fail if new fields such as `AssignAutoAttachedEventPolicy` or `AttachedCustomEventName` appear in core action context/request structs.

- [x] **Step 4: Run tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventDelegate.Evidence; Quit"
```

- [x] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateUseSiteEvidence.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateUseSiteEvidenceTests.cpp
git commit -m "feat(graphwrite): harden event delegate evidence reader"
```

### Task 3: ActionContext projection for binding object and handler/signature evidence

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionContextTests.cpp`

- [x] **Step 1: 写 projection tests**

Cases:

```text
self binding object projects event_delegate.binding_object_kind=self
component_ref projects component path and delegate owner class
linked_pin_ref requires stable node/pin anchor
same-statement function_return_ref is accepted only when producer node is in the same fragment
cross-statement temporary symbol returns binding_object_cross_statement_unsupported
```

- [x] **Step 2: Implement evidence map projection**

Canonical keys:

```text
event_delegate.operation
event_delegate.binding_object_kind
event_delegate.binding_object_evidence_id
event_delegate.delegate_owner_class_path
event_delegate.delegate_property_name
event_delegate.delegate_property_path
event_delegate.delegate_signature_function_path
event_delegate.handler_function_path
event_delegate.handler_source_cluster
event_delegate.signature_evidence_id
event_delegate.duplicate_policy
event_delegate.unbind_mode
event_delegate.call_arg.<name>.pin_type
```

- [x] **Step 3: Include evidence in hash**

Sort evidence keys before hash so preview/execute context reuse is deterministic.

- [x] **Step 4: Run tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventDelegate.ActionContext; Quit"
```

- [x] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionContextTests.cpp
git commit -m "feat(graphwrite): project event delegate use-site evidence"
```

### Task 4: Resolver policy gates and manual assign boundary

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp`

- [x] **Step 1: 写 resolver tests**

Required cases:

```text
component_bound_event rejects incompatible graph type
component_bound_event duplicate fail returns delegate_duplicate_binding
component_bound_event duplicate return_existing returns resolved existing binding evidence
delegate.assign uses ue_delegate_manual_assign_factory
delegate.assign never invokes UE Assign spawner when it would create Custom Event
delegate.unbind without handler returns handler_required_for_unbind
delegate.clear with handler evidence returns handler_not_allowed_for_clear
replace/merge duplicate policy returns duplicate_mutation_policy_blocked
```

- [x] **Step 2: Implement `FBlueprintHelperEventDelegatePolicy`**

Policy decisions:

```text
Graph gate: event graph/custom event/function graph allowed only when K2 schema and impure policy permit.
Delegate flags: bind/assign require multicast assignable/callable evidence as applicable.
Duplicate policy: fail, return_existing supported; replace, merge blocked.
Assign policy: manual factory only; UE side-effect assign spawner blocked.
Handler policy: required for bind/assign/unbind; forbidden for clear; call validates args only.
```

- [x] **Step 3: Apply gates before spawner/factory selection**

No node/factory invocation may happen before policy result is `Allowed`.

- [x] **Step 4: Run resolver tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventDelegate.ActionCluster; Quit"
```

- [x] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegatePolicy.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateActionClusterTests.cpp
git commit -m "feat(graphwrite): enforce event delegate use-site policy gates"
```

### Task 5: Fragment builder consumes projected binding object only

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateFragmentBuilderTests.cpp`

- [x] **Step 1: 写 builder boundary tests**

Assertions:

```text
component_ref uses projected component pin/evidence
field_get_ref requires projected field-get result pin
linked_pin_ref requires stable node/pin anchor
same-statement function_return_ref is accepted when producer belongs to same fragment
builder never creates UK2Node_VariableGet for missing binding object
CreateDelegate link uses existing handler_function_path and signature_evidence_id
```

- [x] **Step 2: Implement binding object resolver**

Return a local DTO:

```cpp
struct FBlueprintHelperEventDelegateBindingObjectResolution
{
    bool bResolved;
    FString ErrorCode;
    FString ObjectEvidenceId;
    UEdGraphPin* ObjectPin;
    UObject* StableObject;
};
```

It must not create nodes. It may only bind to existing pins/nodes already created by the current fragment or explicitly projected by ActionContext.

- [x] **Step 3: Apply call/default/link rules**

`delegate.call` must validate each arg against the delegate signature pin, then read back:

```text
arg pin name
arg pin type
literal default or linked source pin
missing required arg diagnostics
```

- [x] **Step 4: Run builder tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventDelegate.FragmentBuilder; Quit"
```

- [x] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateBindingObjectResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateFragmentBuilderTests.cpp
git commit -m "feat(graphwrite): build event delegate use sites from projected targets"
```

### Task 6: Readback, DebugBundle, compile diagnostic correlation

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateDebugBundleTests.cpp`

- [x] **Step 1: 写 readback tests**

Required facts:

```text
node_class
spawner_or_factory_kind
delegate_owner_class_path
delegate_property_path
delegate_signature_function_path
binding_object_kind
handler_function_path where applicable
component dynamic binding target for component_bound_event
call arg pin/default/link facts for delegate.call
statement id -> node guid -> compile diagnostic correlation
```

- [x] **Step 2: Implement readback collector**

Collector must consume actual spawned nodes and UE dynamic binding data, not request intent only.

- [x] **Step 3: Keep Review graph/block scoped**

Do not add delegate operation as a Review atomic target. Delegate details belong in DebugBundle/readback facts.

- [x] **Step 4: Run tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventDelegate.DebugBundle; Quit"
```

- [x] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Readback/BlueprintHelperEventDelegateReadback.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperEventDelegateDebugBundleTests.cpp
git commit -m "feat(graphwrite): add event delegate readback facts"
```

### Task 7: E2E matrix and docs

**Files:**
- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_EventDelegate_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md`
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

- [x] **Step 1: Add E2E cases**

Minimum matrix:

```text
positive: component_bound_event, bind, assign, unbind, call, clear
negative: missing binding object, missing handler, missing signature, incompatible graph, duplicate fail, replace/merge blocked, assign side-effect blocked
```

- [x] **Step 2: Run final gate**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& $UE_BUILD_BAT TemplateEditor Win64 Development -Project=$UPROJECT -WaitMutex -NoHotReloadFromIDE
& $UE_EDITOR_CMD $UPROJECT -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.EventDelegate;Quit" -TestExit="Automation Test Queue Empty"
```

Expected:

```text
No Custom Event declaration created by GraphWrite/EventDelegate
No assign_auto_attached_event_policy in contract or DebugBundle
delegate.assign uses manual factory evidence
Review remains graph/block scoped
```

- [x] **Step 3: Commit**

```bash
git add BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_EventDelegate_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md \
        AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md \
        BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md
git commit -m "docs(graphwrite): document event delegate use-site extension"
```

---

## 5. Final Acceptance Checklist

- [x] `delegate.bind/assign/unbind/call/clear` are second-stage operations, not new first-stage runtime clusters.
- [x] `delegate.assign` does not invoke UE Assign spawner side effect path.
- [x] `ue_delegate_manual_assign_factory` remains the supported assign path.
- [x] No handler/signature/event declaration is created or modified by GraphWrite/EventDelegate.
- [x] Binding object evidence is projected; FragmentBuilder does not synthesize `UK2Node_VariableGet`.
- [x] `delegate.unbind` missing handler fails and never downgrades to `clear`.
- [x] Duplicate `replace/merge` is deterministically blocked.
- [x] Review stays graph/block scoped; delegate details are DebugBundle/readback facts.
- [x] Missing evidence failures are stable and Agent-facing.

---

## 6. 具体改造理由

1. **符合 Signature ownership。** Event declaration、handler creation、signature mutation 归 BlueprintSignature；GraphWrite/EventDelegate 只能消费既有 declaration/signature evidence 写 use-site。
2. **保留 manual assign factory 是架构需要。** UE Assign spawner 可能自动创建 Custom Event，这会绕过 BlueprintSignature ownership；manual factory 是明确例外，用于阻断该副作用。
3. **降低 EventDelegate 与 ActionContext core 耦合。** 将 delegate-specific 字段收敛到 `ContextEvidence` 与 `FBlueprintHelperEventDelegateUseSiteEvidence`，避免 core DTO 被绑定策略、handler 策略、duplicate 策略持续污染。
4. **保持 FragmentBuilder 单一职责。** Builder 只使用 projected binding object/handler/signature，不自行创建 getter 或选择 handler，从而减少 resolver、builder、signature system 之间的横向耦合。
5. **提高失败可解释性。** `handler_required_for_unbind`、`duplicate_mutation_policy_blocked`、`assign_side_effect_blocked` 等错误码使 Agent 能修正 TaskSpec，而不是获得不透明 UE compile failure。
6. **保持 Review 粒度稳定。** delegate operation 是图写入内部事实，不需要把 Review atomic target 从 graph/block 扩张到 delegate 级，避免 Review 模型被能力细节拖散。

## 7. Completion Evidence (2026-05-26)

Status: COMPLETE under the repository `AGENTS.md` rule that forbids `git add`, `git commit`, and `git push` after task completion. The plan's commit steps are treated as handled by leaving the worktree unstaged and reporting manual commit commands in the final task output.

Implemented scope:

- Published EventDelegate as a use-site logical operation group owned by `EventDelegateActionCluster`.
- Preserved `delegate_operation` as second-stage semantics for `bind`, `assign`, `unbind`, `call`, and `clear`.
- Kept `delegate.assign` on `ue_delegate_manual_assign_factory`; UE Assign spawner side-effect creation is policy-blocked.
- Added focused EventDelegate evidence reader diagnostics and core-field guards for forbidden side-effect fields.
- Projected EventDelegate evidence through ActionContext under `event_delegate.*`, including binding object anchors, duplicate policy, call arg pin type, and deterministic cross-statement temporary rejection.
- Added `FBlueprintHelperEventDelegatePolicy` for graph, duplicate, handler, clear/unbind, and assign-side-effect gates.
- Added projected-only binding object resolver for `self`, `component_ref`, `field_get_ref`, `linked_pin_ref`, and same-statement `function_return_ref`; cross-statement temporary function-return binding is rejected.
- Updated FragmentBuilder so it does not synthesize `UK2Node_VariableGet`, records projected binding object pins, and validates/records `delegate.call` arg pins/defaults.
- Added EventDelegate readback facts for actual fragment nodes, node guid, component dynamic binding target, delegate property/signature facts, binding object facts, call arg default/link facts, and statement/node diagnostic correlation.
- Kept Review graph/block scoped; delegate facts are exposed through readback/DebugBundle facts, not delegate-level Review atomic targets.
- Updated TaskSpec capability matrix and GraphStatement framework design docs with EventDelegate use-site boundaries and verification.

Verification:

```text
npm.cmd --prefix AgentFaceService/task-core run build
  Result: passed

npm.cmd --prefix AgentFaceService/task-core run test:node
  Result: 195/195 passed

E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
  Result: Succeeded

Automation RunTests BlueprintHelper.GraphWrite.EventDelegate
  Result: 9/9 passed, including 3 EventDelegate.ActionContext tests

Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.EventDelegate
  Result: 16/16 passed

Automation RunTests BlueprintHelper.GraphWrite.GraphStatement.EventDelegate
  Result: 6/6 passed

Automation RunTests BlueprintHelper.GraphWrite.ActionContext.EventDelegate
  Result: 2/2 passed for the legacy ActionContext focused suite

Automation RunTests BlueprintHelper.GraphWrite.ToolResult.EventDelegate
  Result: 1/1 passed
```

Notes:

- UE startup logs include unrelated EOS/network and JetBrains port-file warnings, but all listed automation runs ended with `TEST COMPLETE. EXIT CODE: 0`.
- No `BlueprintHelper/Develop/v*` archived documentation was modified.
- No git staging, commit, or push was performed.