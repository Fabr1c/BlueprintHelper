# BlueprintHelper GraphWrite TS Connect UE-Done Capabilities Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the GraphWrite capabilities listed in `BlueprintHelper_GraphWrite_UESideDone_TSNotConnected_Audit_20260526_CN.md` so Agent-facing TaskSpec, TS compiler lowering, and UE SemanticIR routing agree on the same non-UI statement shapes.

**Architecture:** Keep TaskSpec as the only shared outer context and keep every new shape inside `statement[]` or expression-local `context_evidence`. Extend existing compiler normalization and UE SemanticIR/action-resolution boundaries; do not add UI/editor-menu behavior, legacy fallback, or ad hoc bridge payload fields.

**Tech Stack:** TypeScript `AgentFaceService/task-core`, JSON agent-guide templates, UE 5.6 C++ GraphWrite SemanticIR/action-context/fragment builders, node tests, focused Unreal automation.

---

## Implementation Result - 2026-05-26

**Status:** Implemented and verified for the requested UE-done / TS-not-connected and half-connected GraphWrite scope.

**Connected routes:**

- TS TaskSpec compiler now accepts and lowers generic `kind="control"` operations for switch, multi-gate, and StandardMacros while preserving singleton `branch/sequence/return` behavior.
- UE SemanticIR now has a first-class `Control` statement kind and routes generic control statements through the existing `GenericAssetStructControlAction` action-provider boundary.
- Generic controls are now contractually terminal unless a future operation-specific body/continuation contract is added; TS rejects implicit linear continuation after a generic control.
- TS and UE both preserve/derive `function_operation="create_function"` for function-backed create operations and route those creates through `FunctionAction` instead of generic create ownership.
- Function-backed create statements and expressions now require a callable target/name/stable-id before they can enter the TS or UE routing path.
- Public TS contract and guide templates now expose `create`, `convert`, `schedule`, generic control, asset-backed create, function-backed create, function-backed schedule, and struct-field capability examples.
- `switch_enum` evidence now includes `generic.control.enum_path` on the GenericOps contract and schema test surface.

**Verification run:**

- `npm.cmd --prefix AgentFaceService/task-core test` -> passed, 214/214 node tests.
- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -NoHotReload -WaitMutex` -> passed.
- `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GraphSemanticIR.RuntimeFact.FunctionBackedCreateRequiresTarget;Quit"` -> passed.
- `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps;Quit"` -> passed, 24/24 GenericOps tests.
- `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite;Quit"` -> failed in existing call-function and replace/block-scoped tests; first logged failure was `PrintString` call resolution returning zero graph-compatible candidates, outside the generic control/create TS-connection path changed here.

## File Structure

- Modify `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Add generic control operation vocabulary and normalize `kind: "control"` statements for switch, multi-gate, and StandardMacros.
  - Normalize function-backed create ownership by preserving or deriving `function_operation: "create_function"`.
  - Preserve create semantic fields in import-flow and semantic logic outputs.
- Create `AgentFaceService/task-core/src/task/compiler/task-compiler.generic-control.test.ts`
  - Red/green tests for switch, multi-gate, StandardMacros control lowering and required evidence validation.
- Modify `AgentFaceService/task-core/src/task/compiler/task-compiler.create.test.ts`
  - Red/green tests for function-backed create, asset_action evidence, and ownership-mix rejection.
- Modify `AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts`
  - Add representative public function-backed transform/schedule vocabulary tests without changing already-working raw compiler routing.
- Modify `AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts`
  - Add a first-class `set_fields_in_struct` TaskSpec alias test if the implementation chooses an alias; otherwise add explicit `field.struct_member_set` preservation coverage.
- Modify `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - Add `generic.control.enum_path` to GenericOps evidence keys and switch-enum required evidence.
- Modify `AgentFaceService/task-core/src/task/schema/task-contract.ts`
  - Publish the first-slice statement/expression lists for `create`, `convert`, and `schedule`, and document generic control as `kind=control` plus `control`.
- Modify `AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_ops_template.json`
  - Add concrete examples for `switch_int`, `asset_action`, function-backed create, function-backed convert, `set_fields_in_struct`, and typed select proof.
- Modify `AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_schedule_template.json`
  - Add concrete function-backed schedule examples while keeping `timer_delegate_node` generic schedule ownership unmixed.
- Modify UE C++ only where TS lowering requires existing UE capabilities to become reachable:
  - `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
- Create/modify focused UE tests:
  - `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsEvidenceTests.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsOwnershipTests.cpp`

## Task 1: Generic Control TaskSpec Compiler Route

**Files:**
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.generic-control.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`

- [ ] **Step 1: Write failing TS tests**

Add tests that compile these statements:

```ts
{
  kind: 'control',
  control: 'switch_int',
  case_values: [0, 1],
  context_evidence: {
    'generic.control.default_policy': 'has_default'
  }
}
```

Expected compiled task-plan and append bridge logic statement:

```ts
{
  kind: 'control',
  control: 'switch_int',
  control_operation: 'switch_int',
  context_evidence: {
    'generic.control.operation': 'switch_int',
    'generic.control.case_values': '0,1',
    'generic.control.default_policy': 'has_default'
  }
}
```

Also cover:

```ts
{ kind: 'control', control: 'multi_gate', dynamic_output_count: 3 }
{ kind: 'control', control: 'for_loop', macro_graph_path: '/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForLoop', macro_pin_shape_snapshot: 'Exec,LoopBody,Completed,Index' }
```

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
node AgentFaceService/task-core/build/task/compiler/task-compiler.generic-control.test.js
```

Expected before implementation: `unsupported_control_kind` or missing compiled test file before build.

- [ ] **Step 2: Implement compiler vocabulary and evidence normalization**

In `task-compiler.ts`, add immutable operation sets:

```ts
const GENERIC_CONTROL_SWITCH_KINDS = new Set(['switch_int', 'switch_string', 'switch_name', 'switch_enum']);
const GENERIC_CONTROL_DYNAMIC_KINDS = new Set(['multi_gate']);
const GENERIC_CONTROL_MACRO_KINDS = new Set(['do_once', 'do_n', 'gate', 'flip_flop', 'for_loop', 'for_loop_with_break', 'foreach_loop', 'foreach_loop_with_break', 'while_loop']);
```

Update `SUPPORTED_GRAPH_BODY_CONTROL_KINDS` to include all generic control kinds. Add a normalizer that derives `context_evidence['generic.control.operation']` from `control`, maps `case_values`, `enum_path`, `dynamic_output_count`, `macro_graph_path`, and `macro_pin_shape_snapshot` into namespaced evidence, and leaves `branch/sequence/return` behavior unchanged.

- [ ] **Step 3: Preserve generic controls as `kind=control`**

Change `cloneLogicStatementWithCompiledIds()` so only singleton controls lower to `kind=branch|sequence|return`. Generic controls must keep:

```ts
out.kind = 'control';
out.control = controlKind;
out.control_operation = controlKind;
```

Change `compileStatementFlow()` so generic controls compile as a simple node with `kind: 'control'`, copied context evidence, `entry: node.execute`, and a conservative `then` exit only for compiler flow continuity.

- [ ] **Step 4: Update capability contract evidence**

Add `generic.control.enum_path` to `GENERIC_OPS_EVIDENCE_KEYS` and to switch-enum required evidence. Keep `switch_int/string/name` on `generic.control.case_values`.

- [ ] **Step 5: Run focused TS tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
node AgentFaceService/task-core/build/task/compiler/task-compiler.generic-control.test.js
node AgentFaceService/task-core/build/task/schema/graphwrite-capability-contract.test.js
```

Expected: all focused tests pass.

## Task 2: UE SemanticIR Route for Generic Control Statements

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsEvidenceTests.cpp`

- [ ] **Step 1: Write failing UE semantic route test**

Add a test that builds an IR statement with `Kind = EBlueprintHelperGraphStatementKind::Control`, `ControlOperation = "switch_int"`, and `ContextEvidence["generic.control.case_values"] = "0,1"`, then asserts `CollectFromStatements()` produces:

```cpp
SemanticKind == EBlueprintHelperActionSemanticKind::Control
Query == TEXT("switch_int")
DefaultValues["generic.control.operation"] == TEXT("switch_int")
DefaultValues["generic.control.case_values"] == TEXT("0,1")
```

Run:

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.Evidence; Quit"
```

Expected before implementation: compile failure because `Control` / `ControlOperation` do not exist.

- [ ] **Step 2: Add a real SemanticIR control statement kind**

Add `Control` to `EBlueprintHelperGraphStatementKind`, parse `kind: "control"`, and parse `control` / `control_operation` into `FBlueprintHelperGraphStatementIR::ControlOperation`.

- [ ] **Step 3: Route generic control to existing GenericAssetStructControlAction**

In `BlueprintHelperGraphFragmentBuilderRegistry.cpp`, add a `Statement.Kind == Control` branch that builds an action-provider fragment with `EBlueprintHelperActionSemanticKind::Control`, `Request.Query = ControlOperation`, and existing `ContextEvidence`.

Do not modify `Branch`, `Sequence`, or `Return` routing.

- [ ] **Step 4: Keep DAG/pipeline names coherent**

Add `Control` to `StatementPatternName()`, `StatementKindName()`, semantic statement id generation, and DAG simple-statement building as `statement_control` / `control`.

- [ ] **Step 5: Run focused UE tests**

Run:

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.Evidence; Quit"
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.Ownership; Quit"
```

Expected: both suites pass.

## Task 3: Function-Backed Create Route

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.create.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsOwnershipTests.cpp`

- [ ] **Step 1: Write failing TS tests**

Add create compiler tests for:

```ts
{
  kind: 'create',
  create_operation: 'function_backed_create',
  target: 'CreateWidget',
  function_operation: 'create_function',
  class_path: '/Script/UMG.UserWidget',
  args: {}
}
```

Expected task-plan and bridge statements preserve:

```ts
function_operation: 'create_function'
create_operation: 'function_backed_create'
target: 'CreateWidget'
class_path: '/Script/UMG.UserWidget'
```

Also add:

```ts
{ kind: 'create', create_operation: 'async_action', target: 'AsyncLoadPrimaryAsset' }
```

Expected compiler output derives `function_operation: "create_function"`.

Add a rejection test:

```ts
{ kind: 'create', create_operation: 'spawn_actor', function_operation: 'create_function' }
```

Expected: `unsupported_create_owner_mix`.

- [ ] **Step 2: Implement TS create ownership normalization**

Add a function-backed create set:

```ts
const FUNCTION_BACKED_CREATE_OPERATIONS = new Set(['async_action', 'function_backed_create', 'function_backed_spawn', 'function_backed_construct']);
```

Normalize function-backed create statements and expressions so `function_operation` is present and equals `create_function`. Reject generic create operations when `function_operation` is present.

- [ ] **Step 3: Write failing UE demand collector test**

Add a UE test that constructs a `Create` statement with `FunctionOperation = "create_function"`, `CreateOperation = "function_backed_create"`, `Target = "CreateWidget"`, and asserts the collected demand uses `ClusterKind == FunctionAction`, `FunctionOperation == "create_function"`, and `Query == "CreateWidget"`.

- [ ] **Step 4: Implement UE create FunctionAction routing**

Update `ApplyCreateStatementEvidence()` and expression equivalent so function-backed create:

```cpp
InOutDemand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
InOutDemand.FunctionOperation = TEXT("create_function");
InOutDemand.Query = FirstNonEmpty(Statement.Target, Statement.Name, Statement.CreateOperation);
InOutDemand.DefaultValues.Add(TEXT("function_operation"), TEXT("create_function"));
```

Update create fragment build request query to use the function target for function-backed create and the create operation for generic create.

- [ ] **Step 5: Run focused tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
node AgentFaceService/task-core/build/task/compiler/task-compiler.create.test.js
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps.Ownership; Quit"
```

Expected: focused TS and UE tests pass.

## Task 4: Public First-Class Template and Partial-Gap Coverage

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_ops_template.json`
- Modify: `AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_schedule_template.json`

- [ ] **Step 1: Add TS tests for public vocabulary that already has raw compiler routes**

Add tests that prove these fields survive task-plan and append-bridge lowering:

```ts
{ kind: 'convert', function_operation: 'convert_function', transform_operation: 'blueprint_autocast' }
{ kind: 'schedule', function_operation: 'schedule_function', schedule_operation: 'delay' }
{ kind: 'create', create_operation: 'asset_action', context_evidence: { asset_action_stable_id: '...', asset_action_spawner_signature: '...' } }
{
  kind: 'field',
  field_operation: 'set',
  field_scope: 'property_path',
  capability_id: 'field.struct_member_set',
  capability_facts: { 'generic.struct.selected_field_paths': 'X,Y' }
}
{ kind: 'select', context_evidence: { 'generic.select.result_type_proof': 'string' } }
```

- [ ] **Step 2: Update public contract**

In `task-contract.ts`, include `create`, `convert`, and `schedule` in `supported_first_slice.statement_kinds` and `expression_kinds`, and note generic controls as `kind="control"` plus `control`.

- [ ] **Step 3: Update templates**

Add representative examples without expanding global context:

```json
{ "kind": "control", "control": "switch_int", "case_values": [0, 1], "context_evidence": { "generic.control.default_policy": "has_default" } }
{ "kind": "create", "create_operation": "asset_action", "context_evidence": { "asset_action_stable_id": "__ASSET_ACTION_STABLE_ID__", "asset_action_spawner_signature": "__ASSET_ACTION_SIGNATURE__" } }
{ "kind": "create", "create_operation": "function_backed_create", "target": "__FACTORY_FUNCTION_NAME__", "function_operation": "create_function" }
```

- [ ] **Step 4: Run focused TS tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
node AgentFaceService/task-core/build/task/compiler/task-compiler.convert-schedule.test.js
node AgentFaceService/task-core/build/task/compiler/task-compiler.field.test.js
```

Expected: focused TS tests pass.

## Task 5: Final Verification

**Files:**
- No production changes unless tests expose a defect.

- [ ] **Step 1: Run task-core full checks**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: TypeScript build succeeds and node tests pass.

- [ ] **Step 2: Run focused UE automation**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericOps; Quit"
```

Expected: GenericOps focused suite passes.

- [ ] **Step 3: Run formatting and dirty-scope checks**

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors; dirty scope is limited to the plan, TS/C++ implementation, tests, and templates for this task plus pre-existing unrelated files.

- [ ] **Step 4: Manual commit handoff only**

Do not run git add, git commit, or git push. Final output must include a scoped manual command such as:

```powershell
git add -- BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_TSConnect_UEDoneCapabilities_ImplementationPlan_20260526_CN.md AgentFaceService/task-core/src/task/compiler/task-compiler.ts AgentFaceService/task-core/src/task/compiler/task-compiler.generic-control.test.ts AgentFaceService/task-core/src/task/compiler/task-compiler.create.test.ts AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts AgentFaceService/task-core/src/task/schema/task-contract.ts AgentFaceService/task-core/src/task/schema/task-schemas.genericops-extension.test.ts AgentFaceService/task-core/src/tests/task/task-contract-graphwrite.test.ts AgentFaceService/task-core/src/tests/task/task-contract.test.ts AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_ops_template.json AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_schedule_template.json BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericOpsEvidenceTests.cpp BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp
git commit -m "新增内容：" -m "1. 接通 GraphWrite generic control、create、convert、schedule 与 struct-field 的 TS/UE 语义路由。" -m "2. 增加 UE 已实现能力的 TaskSpec 公开契约、模板和覆盖测试。" -m "修复内容：" -m "1. 修复 function-backed create 缺少 callable target 仍进入 FunctionAction 路由的问题。" -m "2. 修复 generic control 暴露隐式线性 continuation 但 UE 无统一 then pin 的契约缺口。" -m "3. 修复 switch_enum 缺少 enum_path evidence 的契约同步问题。"
```
