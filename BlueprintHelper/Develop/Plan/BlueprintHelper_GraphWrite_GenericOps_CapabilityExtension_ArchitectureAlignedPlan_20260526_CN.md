# BlueprintHelper GraphWrite GenericOps Capability Extension Implementation Plan
> **Execution status (2026-05-26): COMPLETE.** Implementation, focused UE tests, TypeScript tests, UE 5.6 build, docs sync, and final read-only subagent audit are complete. Per `AGENTS.md`, commit steps are a manual commit handoff only; no `git add`, `git commit`, or `git push` was executed.
>
> **Architecture clarification:** GenericOps is an Agent-facing logical capability umbrella, not a runtime cluster and not a blanket `kind` expansion. “First-class Field capability” means stable Field capability IDs plus Field registry/resolver/evidence/readback boundaries; Field-specific facts stay under `field.*` and do not get copied into GenericOps, core `kind`, or broad shared DTOs.


> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在未实施原计划的前提下，替代原 `BlueprintHelper_GenericOps_CapabilityExtension_ImplementationPlan_20260526.md`，覆盖 GenericOps 清洗文档中提及且未排除的全部能力，同时按 GraphWrite 四大 runtime cluster、focused evidence、高内聚低耦合的方式实施。

**Architecture:** GenericOps 是 Agent-facing logical capability umbrella，不是 runtime cluster。每个 operation 必须映射到既有 runtime owner：Function-backed convert/create/schedule/op/container 归 `FunctionActionCluster`；control、StandardMacros、struct/deconstruct/select、`asset_action`、generic schedule node 归 `GenericAssetStructControlActionCluster`；async proxy delegate output connection 只能显式衔接 EventDelegate use-site；不得创建 handler/signature。Operation-specific evidence 进入 `ContextEvidence` 并由 focused readers 读取，Builder 只消费 resolved evidence 与 shared coordinator。

**Tech Stack:** Unreal Engine 5.3+ / BlueprintGraph / K2 / NodeSpawner / StandardMacros / ActionDatabase / BlueprintHelper GraphWrite C++、AgentFaceService task-core TypeScript/Zod、UE Automation Tests、Node test runner。

---

## 0. 本计划状态

本文件是原 GenericOps 计划的**实施前替代版**，不是实施后的修复计划。执行时直接按本文实施；原计划中与本文冲突的 cluster、resolver、builder、DTO 扩张任务不执行。

**替代原计划中的错误方向：**

- 不新增 `control`、`generic_transform`、`generic_create`、`struct_select`、`generic_op` runtime cluster。
- 不把 operation-specific 字段批量写入 `FBlueprintHelperActionSemanticConstraints` / `FBlueprintHelperActionContextDemand`。
- 不在 Builder 中查函数、选 node class、创建 handler、修 signature 或隐式拼 context。
- 不把 function-backed convert/create/schedule 放入 Generic resolver。
- 不把 OpCoverage 复制进 GenericOps；GenericOps 依赖独立 OpCoverage 计划提供 `op.*` FunctionAction 能力。
- 不把 split/recombine pin 作为 GraphWrite statement operation；它只可作为未来 PinOperation 或 readback fact。

---

## 1. Operation Ownership Matrix

### 1.1 Control operations

| Operation | Runtime owner | Implementation boundary |
|---|---|---|
| `branch`, `sequence`, `return` | `GenericAssetStructControlActionCluster` | existing canonical singleton boundary |
| `switch_int`, `switch_string`, `switch_name`, `switch_enum` | `GenericAssetStructControlActionCluster` | control evidence provider + dedicated/singleton fragment boundary |
| `multi_gate` | `GenericAssetStructControlActionCluster` | dedicated fragment boundary with dynamic pin readback |
| `do_once`, `do_n`, `gate`, `flip_flop`, `for_loop`, `for_loop_with_break`, `foreach_loop`, `foreach_loop_with_break`, `while_loop` | `GenericAssetStructControlActionCluster` | StandardMacros evidence: explicit macro graph path + pin shape snapshot |

### 1.2 Container operations

Container operations remain a GenericOps public area, but runtime owner is the current callable/container path under `FunctionActionCluster` / `FBlueprintHelperContainerActionResolver`.

```text
array: get, set, add, add_unique, append, insert, remove_item, remove_index, clear,
       contains, find, length, shuffle, shuffle_from_stream, identical, resize, reverse,
       is_empty, is_not_empty, last_index, swap, filter_array, is_valid_index, random,
       random_from_stream, sort_string, sort_name, sort_byte, sort_int, sort_int64, sort_float
map:   add, remove, find, contains, keys, values, clear, length,
       is_empty, is_not_empty, get_key_value_by_index, get_last_index
set:   add, remove, contains, clear, length, to_array,
       add_items, remove_items, is_empty, is_not_empty, intersection, union, difference,
       get_item_by_index, get_last_index
```

### 1.3 Transform / convert operations

| Operation | Runtime owner | Notes |
|---|---|---|
| `dynamic_cast`, `class_cast` | `GenericAssetStructControlActionCluster` | generic K2 cast node/spawner evidence |
| `type_promotion` | `FunctionActionCluster` for operator path; Generic only if projected generic node evidence exists | no display-name matching |
| `function_conversion`, `blueprint_autocast`, `numeric_conversion`, `string_name_text_conversion`, `enum_conversion` | `FunctionActionCluster` | callable/function-backed conversion |
| `link_time_auto_conversion` | linker/readback layer | no standalone GraphWrite statement success without actual conversion readback |
| `object_to_soft_object`, `class_to_soft_class` | FunctionAction or Generic according to projected spawner/function evidence | resolver must reject ambiguous owner |

### 1.4 Create / asset operations

| Operation | Runtime owner | Notes |
|---|---|---|
| `spawn_actor`, `create_widget`, `construct_object` | Generic if node spawner-backed; FunctionAction if function-backed | owner decided by projected evidence |
| `make_array`, `make_map`, `make_set` | `GenericAssetStructControlActionCluster` | container construction, not container mutation operation |
| `asset_action` | `GenericAssetStructControlActionCluster` | ActionDatabase projected asset evidence required |
| `async_action`, `function_backed_create`, `function_backed_spawn`, `function_backed_construct` | `FunctionActionCluster` | factory UFunction path; output delegate connection is explicit use-site |

### 1.5 Schedule operations

| Operation | Runtime owner | Notes |
|---|---|---|
| `timer_delegate_node`, `latent_or_async_node` | `GenericAssetStructControlActionCluster` | projected generic spawner evidence required |
| `timer_by_function_name`, `timer_by_handle`, `timer_clear_by_handle`, `timer_clear_by_function_name`, `timer_pause_by_handle`, `timer_pause_by_function_name`, `timer_unpause_by_handle`, `timer_unpause_by_function_name`, `delay`, `retriggerable_delay`, `delay_until_next_tick`, `generic_latent_function_call` | `FunctionActionCluster` | schedule_function / latent_or_async_function path |
| `async_proxy_output_delegate_connection` | FunctionAction + EventDelegate explicit use-site | no implicit handler/signature creation |

### 1.6 Construct / Deconstruct / Select operations

| Operation | Runtime owner | Notes |
|---|---|---|
| `make_struct`, `break_struct` | `GenericAssetStructControlActionCluster` | struct/type structure evidence |
| `set_fields_in_struct` | `GenericAssetStructControlActionCluster` | selected field policy + readback proof |
| `select` | `GenericAssetStructControlActionCluster` | enum/object/class/soft/interface proof; residual wildcard fails |

---

## 2. Architecture Gates

| Gate | Rule | Enforcement |
|---|---|---|
| Runtime cluster stability | no new `EBlueprintHelperSpawnerClusterKind` values | TypeScript + C++ guard tests |
| Ownership first | every public operation maps to one existing owner | contract matrix tests |
| Evidence locality | operation details go to `ContextEvidence` + focused readers | header grep guard |
| Function-backed ownership | function-backed convert/create/schedule/container/op route through FunctionAction | resolver tests |
| Generic boundary | Generic resolver only handles generic node/spawner, control, asset, struct/select, generic schedule | ownership tests |
| Builder boundary | builders do not select spawners or synthesize missing evidence | fragment tests |
| No fake success | wildcard residual, missing macro snapshot, missing asset evidence, latent not allowed all fail | readback/DebugBundle tests |

---

## 3. File Structure

### AgentFaceService contract / schema

- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - Publish GenericOps as logical groups with `runtimeOwner` per operation, not new runtime clusters.
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
  - Verify operation ownership matrix and required evidence keys.
- Create: `AgentFaceService/task-core/src/task/schema/task-schemas.genericops-extension.test.ts`
  - Validate TaskSpec shapes, evidence failures, and no UE node class leakage.
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - Export operation constants and evidence key constants only.

### Focused evidence readers

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.cpp`
  - Houses focused readers:
    - `FBlueprintHelperControlOperationEvidenceReader`
    - `FBlueprintHelperMacroInstanceEvidenceReader`
    - `FBlueprintHelperGenericCreateEvidenceReader`
    - `FBlueprintHelperGenericTransformEvidenceReader`
    - `FBlueprintHelperGenericScheduleEvidenceReader`
    - `FBlueprintHelperStructFieldPolicyEvidenceReader`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - Project evidence map keys and hash them.

### UE resolvers/builders/readback

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFlowFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFlowFragmentBuilder.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperMacroControlFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperMacroControlFragmentBuilder.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.cpp`

### Tests and docs

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsContractTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsOwnershipTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperControlFlowExtensionTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStandardMacroControlFlowTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionCoverageExtensionTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperTransformConversionPolicyTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateExtensionTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperScheduleVocabularyExtensionTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStructSelectHardeningTests.cpp`
- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md`
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

不要修改 `BlueprintHelper/Develop/v*` 归档文档。

---

## 4. Tasks

### Task 1: Contract normalize，按 ownership 发布 GenericOps logical groups

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
- Create: `AgentFaceService/task-core/src/task/schema/task-schemas.genericops-extension.test.ts`

- [x] **Step 1: 写失败测试：禁止新增 GenericOps runtime clusters**

```ts
const clusterIds = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => cluster.id);
for (const forbidden of ['control', 'generic_transform', 'generic_create', 'struct_select', 'generic_op']) {
  assert.equal(clusterIds.includes(forbidden), false, forbidden);
}
```

- [x] **Step 2: 写 ownership matrix 测试**

Assert representative mappings:

```ts
assertOperationOwner('generic_ops.control.switch_enum', 'GenericAssetStructControlAction');
assertOperationOwner('generic_ops.control.for_loop', 'GenericAssetStructControlAction');
assertOperationOwner('generic_ops.container.array.add', 'FunctionAction');
assertOperationOwner('generic_ops.transform.function_conversion', 'FunctionAction');
assertOperationOwner('generic_ops.create.asset_action', 'GenericAssetStructControlAction');
assertOperationOwner('generic_ops.create.function_backed_create', 'FunctionAction');
assertOperationOwner('generic_ops.schedule.timer_by_handle', 'FunctionAction');
assertOperationOwner('generic_ops.schedule.timer_delegate_node', 'GenericAssetStructControlAction');
assertOperationOwner('generic_ops.struct.set_fields_in_struct', 'GenericAssetStructControlAction');
```

- [x] **Step 3: Implement logical groups**

Groups:

```text
generic_ops.control
generic_ops.container
generic_ops.transform
generic_ops.create
generic_ops.schedule
generic_ops.struct_select
```

Each operation must publish:

```text
operation id
runtimeOwner
semanticKind
semanticFamily
secondStageOperation
requiredEvidenceKeys
excludedReason when rejected
```

- [x] **Step 4: Run TypeScript tests**

```bash
cd AgentFaceService/task-core
npm test -- graphwrite-capability-contract
```

- [x] **Step 5: Manual commit handoff**

```bash
git add AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts \
        AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts \
        AgentFaceService/task-core/src/task/schema/task-schemas.genericops-extension.test.ts
git commit -m "feat(graphwrite): publish ownership-scoped generic ops contract"
```

### Task 2: Focused evidence readers and ActionContext projection

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsEvidenceTests.cpp`

- [x] **Step 1: 写 core DTO guard tests**

Fail if new operation-specific fields such as these appear in core structs:

```text
SwitchCasePins, MacroGraphPath, MacroPinShapeSnapshot, ExposeOnSpawnProperties,
AsyncProxyDelegateHandlers, SelectOptionProofs, SetFieldsInStructFields
```

- [x] **Step 2: Implement focused readers**

Reader DTOs:

```text
ControlOperationEvidence: operation, case values, default policy, dynamic output count
MacroInstanceEvidence: macro_graph_path, macro_pin_shape_snapshot, world/context policy
GenericCreateEvidence: create_operation, class_path, asset_path, expose_on_spawn evidence keys
GenericTransformEvidence: transform_operation, source_pin_type, target_pin_type, cast/interface policy
GenericScheduleEvidence: schedule_operation, graph_latent_allowed, handler evidence id when required
StructFieldPolicyEvidence: struct_path, selected_field_paths, optional_pin_policy, result type proof
```

- [x] **Step 3: Project canonical evidence map keys**

Use prefixes:

```text
generic.control.*
generic.macro.*
generic.create.*
generic.transform.*
generic.schedule.*
generic.struct.*
generic.select.*
container.*
```

- [x] **Step 4: Hash evidence map**

The BundleProjector must hash sorted evidence keys so preview/execute cannot reuse stale GenericOps context.

- [x] **Step 5: Run tests and manual commit handoff**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.Evidence; Quit"
```

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsEvidenceTests.cpp
git commit -m "feat(graphwrite): add focused generic ops evidence readers"
```

### Task 3: Control flow and StandardMacros under Generic cluster

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFlowFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFlowFragmentBuilder.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperMacroControlFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperMacroControlFragmentBuilder.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperControlFlowExtensionTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStandardMacroControlFlowTests.cpp`

- [x] **Step 1: 写 control boundary tests**

Required:

```text
switch_int/string/name/enum require explicit case/default evidence
multi_gate requires output count/readback expectation
do_once/do_n/gate/flip_flop/loop ops require macro_graph_path and macro_pin_shape_snapshot
macro op without pin snapshot returns macro_pin_shape_snapshot_missing
builder records singleton_or_macro_boundary reason for any direct K2 node path
```

- [x] **Step 2: Implement Generic boundary classification**

`GenericActionProviderBoundary` may classify:

```text
NodeSpawnerCandidate for projected UE spawner evidence
DedicatedFragmentBuilderRequired for switch/multigate/macro only after evidence reader passes
NeedsMoreSemanticContext for missing cases/snapshot
Unsupported for UI/selection/menu-driven control requests
```

- [x] **Step 3: Implement builders with readback hooks**

Builders may create canonical/dedicated control nodes only within the selected Generic cluster boundary. They must emit:

```text
node class
operation id
case/default pin names
dynamic output count
macro graph path
macro pin shape snapshot match
```

- [x] **Step 4: Run tests and manual commit handoff**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.Control; Quit"
```

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFlowFragmentBuilder.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFlowFragmentBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperMacroControlFragmentBuilder.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperMacroControlFragmentBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperControlFlowExtensionTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStandardMacroControlFlowTests.cpp
git commit -m "feat(graphwrite): add generic control and macro operations"
```

### Task 4: ContainerAction vocabulary expansion through FunctionAction-owned callable evidence

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionCoverageExtensionTests.cpp`

- [x] **Step 1: 写 coverage tests**

For every array/map/set operation in §1.2, assert:

```text
operation appears in vocabulary
runtime owner is FunctionAction
required roles are explicit
missing element/key/value evidence fails
selected UFunction path is read back
collection pin type is non-wildcard
```

- [x] **Step 2: Expand vocabulary**

Each spec must declare:

```text
container_kind
container_operation
stable_ufunction_path
required_roles
result_kind
wildcard_policy
readback_pin_roles
```

- [x] **Step 3: Resolver uses shared callable evidence**

`FBlueprintHelperContainerActionResolver` must not duplicate function lookup logic. It builds a callable request with the vocabulary spec and uses existing function resolution/candidate evidence.

- [x] **Step 4: Run tests and manual commit handoff**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ContainerAction; Quit"
```

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionCoverageExtensionTests.cpp
git commit -m "feat(graphwrite): expand container action callable vocabulary"
```

### Task 5: Function-backed convert/create/schedule ownership

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsOwnershipTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperTransformConversionPolicyTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperScheduleVocabularyExtensionTests.cpp`

- [x] **Step 1: 写 ownership tests**

Expected owners:

```text
function_conversion => FunctionAction
blueprint_autocast => FunctionAction
numeric/string/name/text/enum conversion => FunctionAction
timer_by_function_name / timer_by_handle / clear/pause/unpause => FunctionAction
delay/retriggerable_delay/delay_until_next_tick/generic_latent_function_call => FunctionAction
function_backed_create/spawn/construct/async_action factory UFunction => FunctionAction
```

- [x] **Step 2: Implement FunctionAction second-stage operations**

Add handling for:

```text
convert_function
create_function
schedule_function
latent_or_async_function
```

- [x] **Step 3: Add ambiguity guard**

If both Generic node evidence and function-backed evidence are present for the same statement, return:

```text
ambiguous_generic_function_owner
```

unless the operation contract explicitly declares a deterministic owner.

- [x] **Step 4: Keep Generic transform/schedule narrow**

`GenericTransformScheduleActionResolver` should handle only Generic node/spawner evidence such as dynamic/class cast node or generic timer/latent node evidence. It must reject function-backed evidence with `function_backed_operation_wrong_owner`.

- [x] **Step 5: Run tests and manual commit handoff**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.Ownership; Quit"
```

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsOwnershipTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperTransformConversionPolicyTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperScheduleVocabularyExtensionTests.cpp
git commit -m "feat(graphwrite): route function-backed generic ops through FunctionAction"
```

### Task 6: Generic create / asset / schedule node hardening

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateExtensionTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericScheduleNodeTests.cpp`

- [x] **Step 1: 写 asset/create/schedule tests**

Required:

```text
asset_action requires projected ActionDatabase asset evidence
asset_action rejects query-only selector
spawn_actor/create_widget/construct_object read back expose-on-spawn pins after selected spawner evidence
timer_delegate_node requires projected generic schedule spawner evidence and handler/signature evidence when delegate link is requested
latent_or_async_node checks graph_latent_allowed=true
async proxy output delegate connection requires explicit EventDelegate use-site plan; no implicit handler creation
```

- [x] **Step 2: Harden asset evidence**

Do not accept raw asset path as success proof unless it matches projected `AssociatedAsset` / stable spawner evidence.

- [x] **Step 3: Harden expose-on-spawn readback**

Resolver selects create spawner. Builder applies exposed pins. Readback verifies actual pin names/types/defaults/links.

- [x] **Step 4: Harden generic schedule**

Generic schedule success requires:

```text
selected spawner evidence
graph_latent_allowed where latent
handler/signature evidence for delegate connection
actual node class readback
```

- [x] **Step 5: Run tests and manual commit handoff**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.CreateSchedule; Quit"
```

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericCreateExtensionTests.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericScheduleNodeTests.cpp
git commit -m "feat(graphwrite): harden generic create asset and schedule nodes"
```

### Task 7: Struct / Deconstruct / Select hardening

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStructSelectHardeningTests.cpp`

- [x] **Step 1: 写 struct/select tests**

Required:

```text
make_struct reads back actual enabled fields
break_struct reads back output field paths
set_fields_in_struct requires struct path and selected field paths
select enum/object/class/soft/interface requires result type proof
select with unresolved wildcard returns wildcard_residual
split/recombine pin is not accepted as GraphWrite statement operation
```

- [x] **Step 2: Implement SetFieldsInStruct evidence path**

`StructFieldPolicyEvidence` must include:

```text
struct_path
selected_field_paths
enabled_optional_pin_names
input/output pin type proof
```

- [x] **Step 3: Harden Select readback**

Verify actual option pins, index pin, result pin type, enum/object/class/soft/interface target proof.

- [x] **Step 4: Run tests and manual commit handoff**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.StructSelect; Quit"
```

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStructSelectHardeningTests.cpp
git commit -m "feat(graphwrite): harden struct and select generic operations"
```

### Task 8: Shared GenericOps readback / DebugBundle facts

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsDebugBundleTests.cpp`

- [x] **Step 1: 写 DebugBundle/readback tests**

Required family-specific facts:

```text
control: operation id, node class, case/default pins, dynamic output count
macro: macro graph path, pin shape snapshot match
container: UFunction path, collection pin type, return pin type
transform: source/target pin type, conversion/cast identity, residual wildcard
create: node class/spawner/asset/class path, expose-on-spawn pin proof
schedule: latent allowed proof, handler/signature evidence where required
struct/select: field policy, result type proof
```

- [x] **Step 2: Implement verifier**

The verifier must compare actual UE node/pins after spawn/link against projected/resolved evidence. It must not treat request intent alone as readback success.

- [x] **Step 3: Add common error codes**

```text
missing_evidence
schema_rejection
wrong_runtime_owner
macro_pin_shape_snapshot_missing
macro_spawner_unavailable
asset_reference_mismatch
latent_not_allowed
handler_missing
wildcard_residual
expose_on_spawn_pin_missing
select_result_type_unresolved
```

- [x] **Step 4: Run tests and manual commit handoff**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.DebugBundle; Quit"
```

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsDebugBundleTests.cpp
git commit -m "feat(graphwrite): add generic ops readback verifier"
```

### Task 9: E2E capability matrix and docs

**Files:**
- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md`
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: active GenericOps/Container/Control/Schedule plan docs under `BlueprintHelper/Develop/Plan/` as needed.

- [x] **Step 1: Add focused E2E smoke cases**

Minimum positive coverage:

```text
switch_enum, multi_gate, do_once macro, foreach_loop macro,
array.add, array.identical, map.keys, set.union,
dynamic_cast, function_conversion,
spawn_actor expose-on-spawn, asset_action projected evidence,
timer_delegate_node, delay function-backed,
set_fields_in_struct, select enum
```

Minimum negative coverage:

```text
missing macro snapshot, missing container element type, wrong runtime owner,
asset query-only selector, latent not allowed, missing handler for async delegate connection,
wildcard select result, split/recombine statement rejected
```

- [x] **Step 2: Run final gate and manual commit handoff**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& $UE_BUILD_BAT TemplateEditor Win64 Development -Project=$UPROJECT -WaitMutex -NoHotReloadFromIDE
& $UE_EDITOR_CMD $UPROJECT -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps;Quit" -TestExit="Automation Test Queue Empty"
```

Expected:

```text
No new runtime cluster
No function-backed operation owned by Generic resolver
No successful GenericOps action without projected evidence or documented singleton/dedicated boundary
No handler/signature creation by GenericOps
No UI/menu/display-name/selected-state dependency
```

- [x] **Step 3: Manual commit handoff**

```bash
git add BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericOps_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md \
        AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md \
        BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md \
        BlueprintHelper/Develop/Plan
git commit -m "docs(graphwrite): document architecture-aligned generic ops extension"
```

---

## 5. Final Acceptance Checklist

- [x] GenericOps contract has logical groups only; no new runtime cluster ids.
- [x] Every operation declares a single runtime owner.
- [x] Function-backed convert/create/schedule/container paths route through FunctionAction.
- [x] Control/macro/struct/select/asset/generic schedule node paths route through GenericAssetStructControlAction.
- [x] OpCoverage is not duplicated inside GenericOps.
- [x] Operation-specific evidence is read by focused readers from `ContextEvidence`.
- [x] Builders do not choose spawners, functions, node class, handler, or signature.
- [x] StandardMacros require explicit macro graph path and pin shape snapshot.
- [x] AssetAction requires projected ActionDatabase/asset evidence.
- [x] Latent/schedule nodes check `graph_latent_allowed`.
- [x] Select and struct operations fail on unresolved wildcard or missing field policy evidence.
- [x] Split/recombine pin is not accepted as a GraphWrite statement operation.
- [x] DebugBundle includes all required missing evidence / wrong owner / wildcard / latent / asset mismatch codes.

---

## 6. 具体改造理由

1. **符合四大 cluster 架构。** GenericOps 是能力集合，不是 runtime 分发层。把每个 operation 映射到现有 owner，可以避免恢复旧的自然语义一级分发。
2. **高内聚。** Container vocabulary、control/macro evidence、generic create evidence、struct/select policy 分别由 focused reader/resolver 管理；每个模块围绕单一 evidence family 聚合。
3. **低耦合。** Core DTO 不随 operation 增长而新增字段组；Resolver 不读取 TaskSpec 原文；Builder 不选择 action。各层通过 projected evidence 和 action resolution result 交互。
4. **高通用性。** Function-backed convert/create/schedule/container 统一走 FunctionAction，复用 callable resolution、ActionDatabase/ActionFilter、typed pins、candidate diagnostics 和 readback。
5. **避免重复 source of truth。** OpCoverage 由独立计划维护，GenericOps 只声明依赖关系，防止两个计划各自维护 op schema/resolver/catalog。
6. **保留 UE 行为边界。** Wide-surface action 优先用 ActionDatabase / NodeSpawner evidence；只有 canonical singleton、UE ActionDatabase 不可表达或多节点 DAG 才使用 dedicated builder。
7. **更强 no-fake-success。** 每个 GenericOps success 都必须有真实 UE readback：node class、spawner/function/type identity、pin name/type/direction、dynamic pin count、wildcard residual 与 compile diagnostic correlation。


---

## 7. Completion Evidence (2026-05-26)

All task checkboxes are complete under the repository rule that commit steps are a manual handoff, not an executed git operation.

Verification run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node # 188/188
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development 'D:\UEProjects\Template\Template.uproject' -NoHotReloadFromIDE -WaitMutex
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -NoSound -NoSplash -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GenericOps;Quit' # 22/22
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -NoSound -NoSplash -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic;Quit' # 24/24
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -NoSound -NoSplash -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ContainerAction;Quit' # 14/14
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -NoSound -NoSplash -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GenericSchedule;Quit' # 4/4
```

Notes:

- No new runtime clusters were added for GenericOps.
- GenericOps public operations are logical contract groups with explicit `runtimeOwner` mapping.
- Function-backed create/convert/schedule/container operations stay under FunctionAction.
- Field capability remains Field-owned (`field.capability_id` + `field.*` evidence); no Field-tool-cluster-specific payload was added to GenericOps/core DTOs.
- `select` now rejects wildcard/unresolved result proof; `set_fields_in_struct` requires selected field policy evidence; `split_pin` and `recombine_pin` remain rejected as GraphWrite statements.
