# BlueprintHelper GraphWrite OpCoverage Capability Extension Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在未实施原计划的前提下，替代原 `2026-05-26-graphwrite-op-coverage-extension.md`，按当前 GraphWrite 架构补齐 `kind=op` 中已纳入且未排除的全部 P0/P1/P2 operation，同时保留现有 TypePromotion operator 行为。

**Architecture:** `op` 不新增 `graphwrite_op` runtime cluster。所有 op 作为 `FunctionActionCluster` 内的 `Semantic.Kind=Op`、`SemanticFamily=Operator`、`second_stage_operation=op.<id>` 处理；operation-specific 数据只进入 `ContextEvidence`，再由 `FBlueprintHelperOpCallableEvidenceReader` 读取成局部 DTO。Resolver 只选择或拒绝 UE callable/spawner evidence，Builder 只消费 resolution result 并走 shared spawn/compose/readback coordinator。

**Tech Stack:** Unreal Engine 5.6 production baseline / BlueprintGraph / K2 / ActionDatabase / NodeSpawner / BlueprintHelper GraphWrite C++、AgentFaceService task-core TypeScript/Zod、UE Automation Tests、Node test runner。

---

## 0. 本计划状态

本文件是原 OpCoverage 计划的**实施前替代版**，不是实施后的修复计划。执行时不要先实现原计划再打补丁；应直接按本文替代原计划执行。

**2026-05-26 execution status:** implemented. Git commit steps in this plan were not executed because the repository AGENTS rule forbids `git add` / `git commit` / `git push` for completed tasks; final handoff must provide manual commands instead.

| Task | Status | Evidence |
|---|---|---|
| Task 1 Contract logical OpCoverage | Done | `graphwrite-capability-contract.ts` exposes `op_coverage` as a logical operation group; `GRAPHWRITE_CAPABILITY_CONTRACT.clusters` still has no `graphwrite_op`. |
| Task 2 Catalog / evidence reader | Done | `FBlueprintHelperOpCallableCatalog` and `FBlueprintHelperOpCallableEvidenceReader` own supported/excluded op metadata and deterministic evidence rejection. |
| Task 3 ActionContext evidence map | Done | `kind=op` evidence is projected through `ContextEvidence`; evidence key/value pairs participate in `SemanticConstraintsHash`; no op-only core DTO fields were added. |
| Task 4 FunctionAction resolver path | Done | TypePromotion ops resolve first; function-backed ops resolve through request-scoped `FBlueprintHelperCallFunctionCandidatePolicy`. |
| Task 5 `array_identical` guard | Done | Array typed pin evidence is required and `UK2Node_CallArrayFunction` is only permitted in the guarded request scope. |
| Task 6 Readback / E2E | Done | OpCoverage readback verifier checks operation id, node/spawner/function facts, pins, wildcard residual, and deterministic negative diagnostics. |
| Task 7 Docs / final gate | Done | Active plan, capability matrix, and design document are synchronized; final TS/UE build and focused automation gates passed. |

Post-review fixes:

- All P0 `commutative_function` specs now carry request-scoped `UK2Node_CommutativeAssociativeBinaryOperator` policy, covering `bitwise_and`, `bitwise_or`, `boolean_or`, `boolean_nand`, `max`, `min`, and `string_append` in addition to `boolean_and`.
- `array_identical` guard now compares full array element identity, including object path for struct/object/class-like element categories.
- `array_identical` tests cover scalar arrays, struct arrays, object arrays, and named-part pin-type tokens.
- OpCoverage readback verifier selects candidate evidence by selected stable id instead of blindly trusting the first candidate.

Final verification evidence:

```text
npm.cmd --prefix AgentFaceService/task-core run build -> exit 0
npm.cmd --prefix AgentFaceService/task-core run test:node -> 179 pass, 0 fail
E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -NoHotReloadFromIDE -WaitMutex -> exit 0
Automation RunTests BlueprintHelper.GraphWrite.ActionContext -> exit 0
Automation RunTests BlueprintHelper.GraphWrite.GraphSemanticIR.RuntimeFact.OpContextEvidenceSurvivesBuildRequest -> exit 0
Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.Operator -> exit 0
Automation RunTests BlueprintHelper.GraphWrite.CallFunctionResolver.DefaultRequestExcludesCallArrayFunction -> exit 0
Automation RunTests BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical -> exit 0
Automation RunTests BlueprintHelper.GraphWrite.OpCoverage.Readback.RejectsMismatchedFirstCandidate -> exit 0
Automation RunTests BlueprintHelper.GraphWrite.OpCoverage -> exit 0
```

**替代原计划中的错误方向：**

- 不新增 `graphwrite_op` runtime cluster。
- 不把 `OpSpawnPath`、`FunctionStableId`、`OwnerClass`、`FunctionName` 等 op 专属字段塞进 `FBlueprintHelperActionSemanticConstraints` / `FBlueprintHelperActionContextDemand`。
- 不全局放开 `UK2Node_CallArrayFunction` 到普通 call resolver。
- 不新增独立完整 `array_identical` builder，除非 evidence guard 证明 shared coordinator 无法覆盖。
- 不通过 display name / menu string / Slate action item / UI selected pin 判断执行成功。

---

## 1. Scope and Capability Matrix

### 1.1 保留的既有 TypePromotion ops

以下 operation 继续走现有 `FTypePromotion::GetOperatorSpawner()` / type promotion path，优先级高于 callable-op path：

```text
add, subtract, multiply, divide, greater, greater_equal, less, less_equal, equal, not_equal
```

### 1.2 新增支持的 P0 commutative function ops

```text
bitwise_and, bitwise_or, boolean_and, boolean_or, boolean_nand, max, min, string_append
```

### 1.3 新增支持的 P1 compact call function ops

```text
boolean_not, boolean_xor, boolean_nor, bitwise_not, bitwise_xor,
abs, modulo, negate, dot, dot3, cross, cross3, near_equal, intpoint_equal, transform_compose,
equal_exact, not_equal_exact, equal_ignore_case, not_equal_ignore_case,
datetime_add_datetime, datetime_add_timespan, datetime_subtract_datetime, datetime_subtract_timespan,
datetime_equal, datetime_not_equal, datetime_greater, datetime_greater_equal, datetime_less, datetime_less_equal
```

### 1.4 新增支持的 P2 special node op

```text
array_identical
```

`array_identical` 只能在 statement-local array typed pin evidence 完整时通过；不得静默降级成 `equal`。

### 1.5 仍排除的 operations

```text
enum_equal, enum_not_equal, SlateBrush equality, convert_numeric, convert_string_text_name,
array_map_set_mutation, validity_predicate
```

排除项必须在 TypeScript contract、C++ catalog、resolver diagnostics 和 E2E negative fixtures 中同时可见。

---

## 2. Architecture Gates

| Gate | Rule | Enforcement |
|---|---|---|
| Cluster ownership | `op.*` runtime owner 固定为 `FunctionActionCluster` | contract test + C++ guard test |
| Evidence locality | op-specific evidence 只存在于 `ContextEvidence` / focused reader DTO | header grep guard + unit test |
| Resolver responsibility | resolver 只返回 selected spawner/function/candidate 或 deterministic rejection | resolver tests |
| Builder responsibility | builder 不查找函数、不选择 node class、不修复 typed evidence | fragment tests |
| Callable candidate scope | `UK2Node_CallArrayFunction`、`UK2Node_CommutativeAssociativeBinaryOperator` 只能 request-scoped 放行 | call resolver tests |
| No fake success | 缺 stable evidence、缺 typed pins、residual wildcard 均失败 | DebugBundle/readback tests |

---

## 3. File Structure

### AgentFaceService contract / schema

- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - 将 OpCoverage 发布为 logical capability group，不发布 runtime `graphwrite_op` cluster。
  - 每个 supported operation 记录 `runtimeCluster: 'FunctionAction'`、`semanticKind: 'op'`、`semanticFamily: 'operator'`、`secondStageOperation: 'op.<id>'`。
  - 记录 excluded operations 与 deterministic rejection reason。
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
  - 验证 P0/P1/P2 全覆盖、排除项可见、无 `graphwrite_op` cluster。
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - 仅导出 operation allowlist / rejected list / evidence key names；不把 UE node class 作为 Agent-facing 字段。
- Create: `AgentFaceService/task-core/src/task/schema/task-schemas.op-coverage-extension.test.ts`
  - 验证 statement-local evidence shape 与 missing evidence rejection。

### UE ActionResolution / evidence

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.cpp`
  - 单一 op allowlist / exclusion source of truth。
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.cpp`
  - `FBlueprintHelperOpCallableEvidenceReader` 从 `Request.ContextEvidence` 读取局部 evidence DTO。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp`
  - 保留 TypePromotion path；新增 callable-op path；`array_identical` 先走 typed array guard。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h`
  - 增加 request-scoped `PermittedNodeClassPaths` / `RequiredNodeClassPath` / `CandidateSourcePolicy`，默认不扩大普通 call 候选。

### GraphStatement / ActionContext projection

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - 将 `kind=op` 的 operation id 和 typed evidence 放入 context evidence map，不增加 op 专属 IR 字段组。
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.cpp`
  - 解析 scalar/container pin type evidence，供 op、container、select 复用。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - 收集 `op.operation_id`、`op.evidence.*` 到 `FBlueprintHelperResolvedActionContext::Evidence`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - 将 evidence map 纳入 semantic hash；不新增 op-specific core fields。

### Fragment / readback / tests

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.cpp`
  - 校验 operation id、node class、spawner class、source function、pin type/direction、wildcard residual。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteOpCoverageContractTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOpCallableEvidenceTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOperatorActionResolverTests.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOpCoverageEndToEndTests.cpp`

### Active docs

- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_OpCoverage_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - 仅补充 OpCoverage ownership/readback matrix，不修改四大 cluster 架构。
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
  - 更新 op supported/excluded 状态。

不要修改 `BlueprintHelper/Develop/v*` 下归档文档。

---

## 4. Tasks

### Task 1: Contract 先行，发布 logical OpCoverage 而非 runtime cluster

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
- Create: `AgentFaceService/task-core/src/task/schema/task-schemas.op-coverage-extension.test.ts`

- [ ] **Step 1: 写失败测试：禁止 `graphwrite_op` cluster**

Add to `graphwrite-capability-contract.test.ts`:

```ts
it('does not publish op coverage as a runtime graphwrite_op cluster', () => {
  const runtimeClusterIds = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.map((cluster) => cluster.id);
  assert.ok(!runtimeClusterIds.includes('graphwrite_op'));

  const opGroup = GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups.find((group) => group.id === 'op_coverage');
  assert.ok(opGroup);
  for (const operation of opGroup.operations.filter((item) => item.supportStatus === 'supported')) {
    assert.equal(operation.runtimeCluster, 'FunctionAction');
    assert.equal(operation.semanticKind, 'op');
    assert.equal(operation.semanticFamily, 'operator');
    assert.ok(operation.secondStageOperation.startsWith('op.'));
  }
});
```

- [ ] **Step 2: 写失败测试：P0/P1/P2 supported list 完整**

Use the exact operation arrays from §1.2-§1.4. Expected failure before implementation: `op_coverage group missing` or missing operation ids.

- [ ] **Step 3: 实现 contract group**

Add `OP_COVERAGE_OPERATIONS` and `OP_COVERAGE_EXCLUDED_OPERATIONS` to `graphwrite-capability-contract.ts`. Keep the four runtime clusters unchanged.

- [ ] **Step 4: 运行 TypeScript contract tests**

```bash
cd AgentFaceService/task-core
npm test -- graphwrite-capability-contract
```

Expected after implementation:

```text
All graphwrite capability contract tests passed.
```

- [ ] **Step 5: Commit**

```bash
git add AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts \
        AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts \
        AgentFaceService/task-core/src/task/schema/task-schemas.op-coverage-extension.test.ts
git commit -m "feat(graphwrite): publish function-owned op coverage contract"
```

### Task 2: 建立 op catalog 与 focused evidence reader

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOpCallableEvidenceTests.cpp`

- [ ] **Step 1: 写 catalog 测试**

Test expectations:

```text
- Supported op count = 38 newly added ops + existing TypePromotion ops tracked separately.
- Excluded op count includes enum_equal and enum_not_equal.
- array_identical has RequiredEvidence = array_lhs_pin_type,array_rhs_pin_type.
- boolean_and has SpawnFamily = commutative_function.
- abs has SpawnFamily = call_function_compact.
```

- [ ] **Step 2: 实现 `FBlueprintHelperOpCallableSpec`**

Required fields:

```cpp
FString OperationId;
FString SpawnFamily;          // type_promotion | commutative_function | call_function_compact | special_node
FString StableCallableId;     // empty for type promotion; required for function-backed specs
FString RequiredNodeClassPath; // optional, request-scoped only
TArray<FString> RequiredEvidenceKeys;
FString RejectionCode;
```

- [ ] **Step 3: 实现 `FBlueprintHelperOpCallableEvidenceReader`**

Reader rules:

```text
- Read operation id from Request.Semantic.FunctionOperation or ContextEvidence["op.operation_id"].
- Reject unknown operation with unsupported_op_operation.
- Reject excluded operation with excluded_op_operation.
- Reject missing required evidence with missing_op_evidence.<key>.
- Never read display name, menu text, UI selection, or global function search as proof.
```

- [ ] **Step 4: 运行 C++ unit tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.OpCoverage.Evidence; Quit"
```

Expected:

```text
BlueprintHelper.GraphWrite.OpCoverage.Evidence completed with 0 failed
```

- [ ] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.cpp \
        BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableEvidence.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOpCallableEvidenceTests.cpp
git commit -m "feat(graphwrite): add op callable evidence reader"
```

### Task 3: 通过 ActionContext evidence map 投影 op evidence

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`

- [ ] **Step 1: 写 guard 测试：禁止新增 op core fields**

The test must fail if these field names appear in `FBlueprintHelperActionSemanticConstraints` or `FBlueprintHelperActionContextDemand`:

```text
OpSpawnPath, FunctionStableId, OwnerClass, FunctionName, bRequiresArrayTypedPins
```

- [ ] **Step 2: 将 statement-local evidence 投影为 map keys**

Canonical keys:

```text
op.operation_id
op.spawn_family
op.stable_callable_id
op.required_node_class_path
op.argument_pin_type.0
op.argument_pin_type.1
op.expected_return_pin_type
op.array_lhs_pin_type
op.array_rhs_pin_type
```

- [ ] **Step 3: Hash evidence map**

`BlueprintHelperActionContextBundleProjector.cpp` must include sorted `Evidence` key/value pairs in `SemanticConstraintsHash` so preview/execute cannot reuse stale op evidence.

- [ ] **Step 4: Run tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.OpCoverage.ActionContext; Quit"
```

- [ ] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp \
        BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp
git commit -m "feat(graphwrite): project op evidence through context map"
```

### Task 4: 在 FunctionAction / Operator resolver 中解析 callable ops

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOperatorActionResolverTests.cpp`

- [ ] **Step 1: 写 resolver tests**

Required cases:

```text
- add still resolves through TypePromotion before callable catalog.
- boolean_and resolves with FunctionAction cluster and commutative function node class policy.
- abs resolves with FunctionAction cluster and compact function stable id.
- enum_equal returns excluded_op_operation.
- unknown op returns unsupported_op_operation.
- call resolver default request does not include UK2Node_CallArrayFunction.
```

- [ ] **Step 2: Add request-scoped candidate policy to call resolver**

Default behavior:

```text
PermittedNodeClassPaths empty => existing call-function behavior only.
RequiredNodeClassPath set => reject any selected candidate with different actual node class.
CandidateSourcePolicy=OperatorOnly => only consume projected callable/operator evidence.
```

- [ ] **Step 3: Implement `FBlueprintHelperOperatorActionResolver` callable branch**

Resolver order:

```text
1. Read operation id.
2. If operation is existing TypePromotion op, use existing TypePromotion path.
3. Else read `FBlueprintHelperOpCallableEvidence`.
4. Build `FBlueprintHelperCallFunctionResolveRequest` with request-scoped node-class policy.
5. Return selected spawner/function evidence, or deterministic diagnostics.
```

- [ ] **Step 4: Run resolver tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.OperatorActionResolver; Quit"
```

- [ ] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOperatorActionResolverTests.cpp
git commit -m "feat(graphwrite): resolve op callable functions through FunctionAction"
```

### Task 5: `array_identical` typed evidence guard

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperArrayIdenticalOpTests.cpp`

- [ ] **Step 1: Write guard tests**

Expected behavior:

```text
array_identical without lhs/rhs array pin evidence => array_typed_pin_missing
array_identical with mismatched array element types => array_typed_pin_mismatch
array_identical with valid typed array pins => resolver permits UK2Node_CallArrayFunction only for this request
array_identical never falls back to op.equal
```

- [ ] **Step 2: Implement guard**

The guard must parse the two pin type tokens using `BlueprintHelperGraphStatementPinTypeParser` and require:

```text
PinContainerType == Array
Element pin type is non-wildcard
LHS/RHS element type compatible by UE schema rules or exact evidence match
```

- [ ] **Step 3: Route through shared coordinator**

`array_identical` uses selected callable/spawner evidence and the existing fragment spawn path. Only add a dedicated fragment helper if a failing test proves a required UE post-spawn pin normalization cannot be expressed by shared coordinator.

- [ ] **Step 4: Run tests**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.OpCoverage.ArrayIdentical; Quit"
```

- [ ] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperArrayIdenticalOpTests.cpp
git commit -m "feat(graphwrite): gate array identical by typed array evidence"
```

### Task 6: Shared fragment consumption and readback facts

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOpCoverageEndToEndTests.cpp`

- [ ] **Step 1: Write readback tests**

Each positive op test must assert:

```text
node_class
spawner_class
operation_id
source_function_path or type_promotion_operator
pin name/type/direction
actual return pin type
wildcard_residual=false
compile diagnostics absent
```

- [ ] **Step 2: Ensure builder only consumes `ActionResolutionResult`**

Builder may read:

```text
SelectedSpawner, SelectedFunction, SelectedStableId, FunctionCandidate, ContextEvidence facts already projected
```

Builder must not read:

```text
Function name from TaskSpec, owner class, display name, menu string, global FindFunctionByName result
```

- [ ] **Step 3: Add DebugBundle facts**

Required error codes:

```text
unsupported_op_operation
excluded_op_operation
missing_op_evidence
array_typed_pin_missing
array_typed_pin_mismatch
op_callable_not_found
op_callable_ambiguous
op_node_class_mismatch
wildcard_residual
```

- [ ] **Step 4: Run E2E smoke**

```powershell
& "$env:UE_EDITOR_CMD" "$env:UE_PROJECT_FILE" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.OpCoverage; Quit"
```

- [ ] **Step 5: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.h \
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.cpp \
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperOpCoverageEndToEndTests.cpp
git commit -m "feat(graphwrite): verify op coverage readback facts"
```

### Task 7: Documentation and final gate

**Files:**
- Create: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_OpCoverage_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md`
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

- [ ] **Step 1: Update capability matrix**

Mark supported/excluded operations exactly as §1. Do not mark `enum_equal` / `enum_not_equal` as partial; they are rejected until stable non-UI evidence exists.

- [ ] **Step 2: Run final gate**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& $UE_BUILD_BAT TemplateEditor Win64 Development -Project=$UPROJECT -WaitMutex -NoHotReloadFromIDE
& $UE_EDITOR_CMD $UPROJECT -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.OpCoverage;Quit" -TestExit="Automation Test Queue Empty"
```

Expected:

```text
No graphwrite_op runtime cluster
No global CallArrayFunction permission
No supported op without FunctionAction evidence
No positive test with residual wildcard or fake stable id
```

- [ ] **Step 3: Commit**

```bash
git add BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_OpCoverage_CapabilityExtension_ArchitectureAlignedPlan_20260526_CN.md \
        AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md \
        BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md
git commit -m "docs(graphwrite): document function-owned op coverage extension"
```

---

## 5. Final Acceptance Checklist

- [x] `GRAPHWRITE_CAPABILITY_CONTRACT.clusters` 中没有 `graphwrite_op`。
- [x] 所有 supported `op.*` 都声明 `runtimeCluster=FunctionAction`。
- [x] `FBlueprintHelperActionSemanticConstraints` 和 `FBlueprintHelperActionContextDemand` 未新增 op 专属字段组。
- [x] `FBlueprintHelperOperatorActionResolver` 保留 TypePromotion first。
- [x] `UK2Node_CallArrayFunction` 仅在 `array_identical` request scope 中放行。
- [x] `array_identical` 缺 array typed pin evidence 必须失败。
- [x] Builder 不执行 function lookup，不依赖 display/menu text。
- [x] DebugBundle 覆盖 unsupported/excluded/missing evidence/ambiguous/wildcard residual。
- [x] Preview 与 execute 使用同一 evidence path。

---

## 6. 具体改造理由

1. **符合 GraphWrite runtime cluster 约束。** `op` 属于 FunctionAction 的 operator 语义，不应被提升成 `graphwrite_op` 一级簇。这样保持 `ActionResolutionCore -> SpawnerClusterResolver -> FunctionActionCluster` 主链路稳定。
2. **降低 core DTO 耦合。** P0/P1/P2 op 的函数路径、node class、array typed pin 等信息只对 op 有意义，放入 `ContextEvidence` + focused reader 可以避免核心结构随 operation 数量膨胀。
3. **提升通用性。** 大部分新增 op 本质是 callable/function-backed operator，复用 `FBlueprintHelperCallFunctionResolver` 比新增独立 resolver/builder 更通用，也能共享 ActionDatabase、ActionFilter、candidate diagnostics 和 readback。
4. **避免候选空间污染。** `UK2Node_CallArrayFunction` 只 request-scoped 放行，避免普通 call 因候选扩大而引入错误匹配。
5. **保持 Builder 单一职责。** Builder 不再选择函数或 node class，只消费 selected evidence 并组合 fragment，降低与 resolver、schema、UE action lookup 的耦合。
6. **强化 no-fake-success。** `array_identical`、wildcard、excluded op、missing evidence 都有稳定 rejection 和 DebugBundle facts，避免通过 display name 或不完整 pin evidence 假成功。
