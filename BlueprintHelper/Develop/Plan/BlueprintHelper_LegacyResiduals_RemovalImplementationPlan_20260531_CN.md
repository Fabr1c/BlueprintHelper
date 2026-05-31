# BlueprintHelper Legacy Residuals Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the two active legacy residuals recorded in `BlueprintHelper/Develop/Gap/BlueprintHelper_LegacyResiduals_SourceAudit_20260531_CN.md`: the deprecated GraphWrite layout mutation path and the reachable Review Reject first-slice placeholder.

**Architecture:** Treat `set_node_position` / `node_position` as retired GraphWrite semantics, not as a compatibility mode. Layout placement must be owned by GraphLayout boundaries, while Review Reject must route through Review v2 data-model semantics or an explicit integrity diagnostic instead of the old archive-baseline placeholder.

**Tech Stack:** TypeScript task-core schema / compiler / Node tests; Unreal Engine 5.6 C++ plugin runtime and automation tests; BlueprintHelper TaskSpec -> TaskPlan -> Bridge -> UE TaskRuntime pipeline.

---

## Execution Rules

- Do not run `git add`, `git commit`, or `git push`; record manual commit scopes only.
- Use UTF-8 without BOM for JSON and Markdown files.
- Keep TaskSpec agent-facing fields semantic and minimal. Do not preserve `set_node_position` as an agent-authored input.
- Do not route layout behavior through GraphWrite as a shortcut. If placement is needed, use or extend GraphLayout service / coordinator boundaries in a separate feature.
- Keep Review Reject semantics aligned with Review v2: rollback target is evidence before snapshot; current/latest drift belongs in diagnostics, not as a blanket Reject blocker.

## File Responsibility Map

### GraphWrite Deprecated Layout Removal

- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - Remove `preserve_layout` from public replace options.
  - Remove `set_node_position` / `node_position` from public patch schema and validation.
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
  - Remove deprecated layout names from public contract and runtime-supported operation lists.
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Remove compiler support for `set_node_position` and `node_position` lowering.
  - Remove `preserve_layout: false` emitted by composite GraphWrite replacement steps.
- Modify: `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`
  - Remove `preserve_layout` fixture fields from public protocol examples.
- Modify: `AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts`
  - Replace the current success assertion for node position with rejection coverage.
- Create: `AgentFaceService/task-core/src/task/schema/task-schemas.deprecated-layout.test.ts`
  - Assert public GraphWrite TaskSpec schema rejects `set_node_position`.
  - Assert public contract no longer advertises `preserve_layout`, `set_node_position`, or `node_position`.
- Modify: `AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts`
  - Add source-boundary guard against deprecated GraphWrite layout mutation support in production runtime files.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h`
  - Remove `NodePosition` and `SetNodePosition` enum values, string emitters, and parsers.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h`
  - Remove `ApplySetNodePosition` declaration.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp`
  - Remove `SetNodePosition` mutation branch and implementation.
  - Remove the review-recording bypass tied to `SetNodePosition`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
  - Remove `set_node_position` handling from GraphWrite IR patch scope and adapter-operation selection.

### Review Reject Placeholder Removal

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
  - Replace the first-slice placeholder fallback with the same explicit unmatched persisted-target diagnostic used by the UI command path.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
  - Update the old placeholder test to require `persisted_review_targets_not_found` and no `archive_baseline` rollback mode.
  - Add a focused guard that the old first-slice message is absent from direct Reject fallback behavior.
- Optional source audit only: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
  - No code change expected if `RejectVisibleChange(*MatchedChange)` returns the corrected result.

### Documentation

- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_LegacyResiduals_SourceAudit_20260531_CN.md`
  - After implementation and verification, update both Open Gaps with exact evidence. Do not mark closed unless all commands in this plan pass.

---

## Task 1: Add Red Tests for Deprecated GraphWrite Layout Public Surface

**Files:**
- Create: `AgentFaceService/task-core/src/task/schema/task-schemas.deprecated-layout.test.ts`
- Modify: `AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts`

- [ ] **Step 1: Add schema and contract rejection tests**

Create `AgentFaceService/task-core/src/task/schema/task-schemas.deprecated-layout.test.ts` with this content:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';
import { GraphWriteTaskSpecSchema } from './task-schemas.js';
import { TASK_PROTOCOL_CONTRACT_V1 } from './task-contract.js';

function makePatchOwnedGraphSpec(patch: Record<string, unknown>): Record<string, unknown> {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      target_type: 'blueprint',
      asset_path: '/Game/BH_Tests/BP_DeprecatedLayout',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'patch_owned_graph',
      patches: [patch],
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

test('GraphWrite TaskSpec rejects deprecated node-position patch authoring', () => {
  const result = GraphWriteTaskSpecSchema.safeParse(makePatchOwnedGraphSpec({
    kind: 'set_node_position',
    target_ref: {
      block_id: 'BH_DeprecatedLayout',
      group_entry_node_path: 'logic.groups[0].entry.node_path',
      node_ref: 'nodes[0]',
    },
    patch: {
      x: 320,
      y: 160,
    },
  }));

  assert.equal(result.success, false);
  if (!result.success) {
    assert.match(JSON.stringify(result.error.issues), /set_node_position/u);
  }
});

test('GraphWrite public contract does not advertise deprecated layout patch semantics', () => {
  const contract = TASK_PROTOCOL_CONTRACT_V1 as Record<string, any>;
  const irContract = contract.graph_write_taskplan_ir_contract;
  const patchContract = irContract.strategies.patch_owned_graph;

  assert.deepEqual(patchContract.kinds, ['set_pin_default', 'set_node_comment']);
  assert.deepEqual(Object.keys(patchContract.scope_derivation).sort(), ['set_node_comment', 'set_pin_default']);
  assert.deepEqual(Object.keys(patchContract.field_shapes).sort(), ['set_node_comment', 'set_pin_default']);
  assert.equal(JSON.stringify(irContract).includes('set_node_position'), false);
  assert.equal(JSON.stringify(irContract).includes('node_position'), false);
  assert.equal(JSON.stringify(irContract).includes('preserve_layout'), false);
});
```

- [ ] **Step 2: Replace the compiler success regression with rejection coverage**

In `AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts`, replace the test named:

```ts
it('compiles node comment and node position patches into runtime-ready structured graph_write IR', () => {
```

with:

```ts
it('rejects deprecated node position patches before GraphWrite lowering', () => {
  const spec = makeTaskSpec({
    behavior: {
      graph_strategy: 'patch_owned_graph',
      patches: [
        {
          kind: 'set_node_position',
          target_ref: {
            block_id: 'BH_DoorFeature_ToggleDoor',
            group_entry_node_path: 'logic.groups[0].entry.node_path',
            node_ref: 'nodes[0]',
          },
          patch: {
            x: 320,
            y: 160,
          },
        },
      ],
    },
  });

  assert.throws(
    () => compileTaskSpecToTaskPlan(TaskSpecSchema.parse(spec)),
    /set_node_position/u,
  );
});
```

- [ ] **Step 3: Run the red Node test pass**

Run:

```powershell
Set-Location AgentFaceService/task-core
npm.cmd run build
npm.cmd run test:node
```

Expected before implementation:

```text
task-schemas.deprecated-layout.test.js fails because set_node_position is still accepted or contract still advertises it.
task-compiler.regression.test.js may still compile the deprecated op until implementation removes it.
```

- [ ] **Step 4: Record manual checkpoint scope**

Do not run git commands. Record these intended files for the eventual manual commit:

```text
AgentFaceService/task-core/src/task/schema/task-schemas.deprecated-layout.test.ts
AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts
```

---

## Task 2: Remove Deprecated Layout Semantics from TypeScript Schema, Contract, Compiler, and Fixtures

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`

- [ ] **Step 1: Remove `preserve_layout` and `set_node_position` from TaskSpec schema**

In `AgentFaceService/task-core/src/task/schema/task-schemas.ts`, change `GraphWriteReplaceSchema.options` to:

```ts
  options: z.object({
    strict: z.boolean().optional(),
  }).passthrough().optional(),
```

Change `GraphWritePatchSchema` to remove `set_node_position`:

```ts
const GraphWritePatchSchema = z.object({
  kind: z.enum(['set_pin_default', 'set_node_comment']),
  scope: z.string().min(1).optional(),
  target_ref: z.record(z.unknown()),
  value: z.unknown().optional(),
  expected_old_state: z.record(z.unknown()).optional(),
}).passthrough().superRefine((value, ctx) => {
  const expectedScopeByKind: Record<string, string> = {
    set_pin_default: 'pin_default',
    set_node_comment: 'node_comment',
  };
  const expectedScope = expectedScopeByKind[value.kind];
  if (value.scope && value.scope !== expectedScope) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope'],
      message: `${value.kind} uses scope ${expectedScope}; omit scope or set it to ${expectedScope}.`,
    });
  }
  if (typeof value.target_ref.node_ref !== 'string' || value.target_ref.node_ref.length === 0) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['target_ref', 'node_ref'], message: 'target_ref.node_ref is required.' });
  }
  if (value.kind === 'set_pin_default' && (typeof value.target_ref.pin_ref !== 'string' || value.target_ref.pin_ref.length === 0)) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['target_ref', 'pin_ref'], message: 'set_pin_default requires target_ref.pin_ref.' });
  }
  if (!Object.hasOwn(value, 'value')) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['value'], message: `${value.kind} requires value.` });
  }
});
```

- [ ] **Step 2: Remove deprecated layout entries from public contract**

In `AgentFaceService/task-core/src/task/schema/task-contract.ts`:

Use this replacement inside `replace_owned_graph`:

```ts
      options: ['strict'],
```

Use this replacement inside `patch_owned_graph`:

```ts
      kinds: ['set_pin_default', 'set_node_comment'],
      scope_derivation: {
        set_pin_default: 'pin_default',
        set_node_comment: 'node_comment',
      },
      field_shapes: {
        set_pin_default: ['target_ref.block_id', 'target_ref.group_entry_node_path', 'target_ref.node_ref', 'target_ref.pin_ref', 'target_ref.link_ref', 'value'],
        set_node_comment: ['target_ref.block_id', 'target_ref.group_entry_node_path', 'target_ref.node_ref', 'value'],
      },
```

Remove `set_node_position` from both `supported_structural_ops` and `runtime_supported_structural_ops`.

Remove `args.options.preserve_layout` from `graph_write_lowering_adapter_contract.operations[].args_optional_paths` for `replace_blueprint_graph`.

- [ ] **Step 3: Remove compiler lowering**

In `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`, update the composite replace step to omit `preserve_layout`:

```ts
          options: {
            strict: true,
          },
```

Update `compilePatchGraphWriteOps` allowlist and error message:

```ts
    if (!['set_pin_default', 'set_node_comment'].includes(kind)) {
      throw new TaskSpecCompileError('unsupported_graph_write_patch', `Unsupported GraphWrite patch kind: ${kind}`, [
        {
          code: 'unsupported_graph_write_patch',
          path: `${path}.kind`,
          message: 'Use set_pin_default or set_node_comment.',
        },
      ]);
    }
```

Update `defaultPatchScope`:

```ts
function defaultPatchScope(kind: string): string {
  if (kind === 'set_node_comment') return 'node_comment';
  return 'pin_default';
}
```

Remove the `set_node_position` branch from `compilePatchPayload`, and update its final unsupported message:

```ts
  throw new TaskSpecCompileError('unsupported_graph_write_patch', `Unsupported GraphWrite patch kind: ${kind}`, [
    {
      code: 'unsupported_graph_write_patch',
      path: `${path}.kind`,
      message: 'Use set_pin_default or set_node_comment.',
    },
  ]);
```

- [ ] **Step 4: Remove fixture examples for `preserve_layout`**

In `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`, remove the `preserve_layout: false` fields at the currently reported locations:

```text
task-protocol.fixtures.ts:225
task-protocol.fixtures.ts:290
task-protocol.fixtures.ts:613
```

Keep surrounding `strict` options intact.

- [ ] **Step 5: Run TypeScript verification**

Run:

```powershell
Set-Location AgentFaceService/task-core
npm.cmd run build
npm.cmd run test:node
```

Expected after implementation:

```text
build succeeds.
test:node succeeds.
The deprecated-layout tests pass.
```

- [ ] **Step 6: Run targeted residual scan**

Run from repo root:

```powershell
Set-Location D:\UEProjects\Template\Plugins\BlueprintHelper
rg -n "set_node_position|node_position|preserve_layout" AgentFaceService/task-core/src -g "*.ts"
```

Expected after Task 2:

```text
No active TS schema/compiler/contract/fixture hits.
Test names or rejection assertions may still mention set_node_position.
```

- [ ] **Step 7: Record manual checkpoint scope**

Do not run git commands. Record these intended files for the eventual manual commit:

```text
AgentFaceService/task-core/src/task/schema/task-schemas.ts
AgentFaceService/task-core/src/task/schema/task-contract.ts
AgentFaceService/task-core/src/task/compiler/task-compiler.ts
AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts
AgentFaceService/task-core/src/task/schema/task-schemas.deprecated-layout.test.ts
AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts
```

---

## Task 3: Add Source Boundary Guard for UE Deprecated Layout Runtime Path

**Files:**
- Modify: `AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts`

- [ ] **Step 1: Add a production-source guard test**

Append this test to `AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts`:

```ts
test('GraphWrite runtime does not retain deprecated layout mutation support', () => {
  const productionFiles = [
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Runtime', 'TaskRuntime', 'BlueprintHelperTaskRuntimeService.cpp'),
    path.resolve(UE_SOURCE_ROOT, 'Public', 'Shared', 'GraphWrite', 'BlueprintHelperPatchGraphTypes.h'),
    path.resolve(UE_SOURCE_ROOT, 'Public', 'Systems', 'ToolClusters', 'GraphWrite', 'BlueprintHelperPatchBlueprintGraphService.h'),
    path.resolve(UE_SOURCE_ROOT, 'Private', 'Systems', 'ToolClusters', 'GraphWrite', 'BlueprintHelperPatchBlueprintGraphService.cpp'),
  ];

  for (const filePath of productionFiles) {
    const source = fs.readFileSync(filePath, 'utf8');
    assert.doesNotMatch(source, /set_node_position/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /\bnode_position\b/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /\bSetNodePosition\b/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /\bNodePosition\b/u, toRepoRelativePath(filePath));
    assert.doesNotMatch(source, /DEPRECATED_LAYOUT/u, toRepoRelativePath(filePath));
  }
});
```

- [ ] **Step 2: Run the red source-boundary test**

Run:

```powershell
Set-Location AgentFaceService/task-core
npm.cmd run build
npm.cmd run test:node
```

Expected before Task 4:

```text
architecture-boundaries.test.js fails on UE production files that still contain set_node_position / NodePosition.
```

- [ ] **Step 3: Record manual checkpoint scope**

Do not run git commands. Record this intended file for the eventual manual commit:

```text
AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts
```

---

## Task 4: Remove Deprecated Layout Runtime Path from UE C++

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`

- [ ] **Step 1: Remove patch enum values and parsers**

In `BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h`:

Remove these enum values:

```cpp
NodePosition,
SetNodePosition,
```

Remove these string conversions and parsers:

```cpp
case EBlueprintHelperPatchScope::NodePosition:      return TEXT("node_position");
if (Str.Equals(TEXT("node_position"), ESearchCase::IgnoreCase))     { Out = EBlueprintHelperPatchScope::NodePosition; return true; }
case EBlueprintHelperPatchType::SetNodePosition:         return TEXT("set_node_position");
if (Str.Equals(TEXT("set_node_position"), ESearchCase::IgnoreCase))          { Out = EBlueprintHelperPatchType::SetNodePosition; return true; }
```

Keep legitimate graph snapshot / UI bounds uses of node position outside GraphWrite patch mutation untouched.

- [ ] **Step 2: Remove patch service mutation function**

In `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h`, remove:

```cpp
bool ApplySetNodePosition(UEdGraphNode* Node, const TSharedPtr<FJsonObject>& Payload, bool& bOutChanged, FString& OutError) const;
```

In `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp`, remove the `SetNodePosition` case:

```cpp
case EBlueprintHelperPatchType::SetNodePosition:
    return ApplySetNodePosition(Target.Node, Request.PatchPayload, bOutChanged, OutError);
```

Remove the full `FBlueprintHelperPatchBlueprintGraphService::ApplySetNodePosition` function.

- [ ] **Step 3: Remove review-recording bypass tied to layout mutation**

In `BlueprintHelperPatchBlueprintGraphService.cpp`, replace:

```cpp
const bool bShouldRecordReview = Request.PatchType != EBlueprintHelperPatchType::SetNodePosition
    && Request.PatchType != EBlueprintHelperPatchType::SetNodeComment;
```

with:

```cpp
const bool bShouldRecordReview = Request.PatchType != EBlueprintHelperPatchType::SetNodeComment;
```

- [ ] **Step 4: Remove TaskRuntime lowering**

In `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`, update `DefaultPatchScopeForGraphWriteOp`:

```cpp
static FString DefaultPatchScopeForGraphWriteOp(const FString& OpName)
{
    if (OpName == TEXT("set_node_comment"))
    {
        return TEXT("node_comment");
    }
    return TEXT("pin_default");
}
```

Update adapter operation selection by removing `OpName == TEXT("set_node_position")` from:

```cpp
if (OpName == TEXT("set_pin_default") ||
    OpName == TEXT("set_node_comment"))
{
    OutAdapterOperation = TEXT("patch_blueprint_graph");
    return TryBuildGraphWriteIrPatchPayload(TargetObject, AssetPath, GraphName, FirstOpObject, OpName, bDryRun, OutPayload, OutError);
}
```

- [ ] **Step 5: Run C++ residual scan**

Run:

```powershell
Set-Location D:\UEProjects\Template\Plugins\BlueprintHelper
rg -n "set_node_position|node_position|SetNodePosition|NodePosition|DEPRECATED_LAYOUT" BlueprintHelper/Source/BlueprintHelper/Private/Runtime BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp -g "*.h" -g "*.cpp"
```

Expected after implementation:

```text
No hits in the checked production mutation path files.
```

- [ ] **Step 6: Run TypeScript source-boundary verification**

Run:

```powershell
Set-Location AgentFaceService/task-core
npm.cmd run build
npm.cmd run test:node
```

Expected:

```text
architecture-boundaries.test.js passes.
```

- [ ] **Step 7: Run UE compile**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
Result: Succeeded
```

- [ ] **Step 8: Record manual checkpoint scope**

Do not run git commands. Record these intended files for the eventual manual commit:

```text
BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h
BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp
AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts
```

---

## Task 5: Add Red Test for Review Reject Placeholder Removal

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Update the existing placeholder test expectation**

Replace the body of `FBlueprintHelperReviewRejectVisibleChangeTest::RunTest` with:

```cpp
bool FBlueprintHelperReviewRejectVisibleChangeTest::RunTest(const FString& Parameters)
{
    FBlueprintHelperReviewVisibleChange Change;
    Change.ChangeId = TEXT("tx_t2");
    Change.AssetPath = TEXT("/Game/BP_Door");
    Change.LocationKey = TEXT("graph:EventGraph/node:PrintString/input:InString");
    Change.LatestEvidenceId = TEXT("tx_t2");
    Change.SourceEvidenceIds.Add(TEXT("tx_t1"));
    Change.SourceEvidenceIds.Add(TEXT("tx_t2"));
    Change.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;
    Change.BeforeSummary = TEXT("Open");
    Change.AfterSummary = TEXT("Door Opened");

    FBlueprintHelperReviewActionService ActionService;
    const FBlueprintHelperReviewActionResult Result = ActionService.RejectVisibleChange(Change);

    TestFalse(TEXT("reject without persisted targets does not fake a completed rollback"), Result.bSucceeded);
    TestEqual(TEXT("reject without persisted targets enters needs action"),
        Result.NewStatus,
        EBlueprintHelperReviewChangeStatus::NeedsAction);
    TestEqual(TEXT("reject without persisted targets reports the data-model integrity gap"),
        Result.Message,
        FString(TEXT("persisted_review_targets_not_found")));
    TestTrue(TEXT("reject without persisted targets does not select archive rollback mode"),
        Result.RollbackMode.IsEmpty());
    TestFalse(TEXT("reject no longer reports first Review UI slice placeholder"),
        Result.Message.Contains(TEXT("first Review UI slice")));
    return true;
}
```

- [ ] **Step 2: Run the red C++ automation test**

Run a targeted automation command if the editor is already open through the approved lifecycle tool; otherwise skip runtime automation until implementation verification and rely on compile after Task 6:

```powershell
bh.cmd bridge ping --select status,summary
```

Expected before implementation:

```text
The updated Review test fails if run, because RejectVisibleChange still returns the first-slice placeholder and archive_baseline rollback mode.
```

- [ ] **Step 3: Record manual checkpoint scope**

Do not run git commands. Record this intended file for the eventual manual commit:

```text
BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp
```

---

## Task 6: Replace Review Reject First-Slice Placeholder with Review v2 Integrity Diagnostic

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Replace no-options Reject fallback**

In `BlueprintHelperReviewActionService.cpp`, replace the fallback at the end of `FBlueprintHelperReviewActionService::RejectVisibleChange(const FBlueprintHelperReviewVisibleChange& Change) const`:

```cpp
FBlueprintHelperReviewActionResult Result;
Result.bSucceeded = false;
Result.TargetEvidenceId = Change.LatestEvidenceId;
Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
Result.RollbackMode = TEXT("archive_baseline");
Result.Message = TEXT("Archive-baseline rollback backend is not wired in the first Review UI slice.");
return Result;
```

with:

```cpp
FBlueprintHelperReviewActionResult Result;
Result.bSucceeded = false;
Result.TargetEvidenceId = Change.LatestEvidenceId;
Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
Result.Message = TEXT("persisted_review_targets_not_found");
return Result;
```

Reasoning:

- This matches `AcceptVisibleChange` unmatched behavior.
- This matches `FBlueprintHelperReviewPanelCommandService::RejectVisibleChange` unmatched behavior.
- It avoids a fake archive-baseline rollback mode when no persisted review target exists.

- [ ] **Step 2: Check lifecycle root fallback behavior**

In the same file, inspect:

```cpp
FBlueprintHelperReviewActionService::RejectLifecycleRootVisibleChange(
    const FBlueprintHelperReviewVisibleChange& Root,
    const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges) const
```

No code change is needed if it calls `RejectVisibleChange(Root)` and therefore inherits the new `persisted_review_targets_not_found` behavior for unresolved roots.

- [ ] **Step 3: Run placeholder residual scan**

Run:

```powershell
Set-Location D:\UEProjects\Template\Plugins\BlueprintHelper
rg -n "first Review UI slice|Archive-baseline rollback backend is not wired" BlueprintHelper/Source -g "*.h" -g "*.cpp"
```

Expected:

```text
No hits.
```

- [ ] **Step 4: Run UE compile**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
Result: Succeeded
```

- [ ] **Step 5: Run targeted Review automation when editor lifecycle is available**

Use MCP editor lifecycle tools for open / close if the executing agent has them available. Then run the targeted automation test through the established BlueprintHelper automation path for:

```text
BlueprintHelper.Review.Action.RejectRequestsArchiveBaselineRollback
BlueprintHelper.Review.Action.RejectSucceedsWithMatchingHashAndSnapshotBaseline
BlueprintHelper.Review.Action.RejectBlocksCurrentStateMismatch
```

Expected:

```text
RejectRequestsArchiveBaselineRollback passes with persisted_review_targets_not_found semantics.
RejectSucceedsWithMatchingHashAndSnapshotBaseline still passes.
RejectBlocksCurrentStateMismatch still passes.
```

- [ ] **Step 6: Record manual checkpoint scope**

Do not run git commands. Record these intended files for the eventual manual commit:

```text
BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp
```

---

## Task 7: Update Gap Document and Run Full Closure Verification

**Files:**
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_LegacyResiduals_SourceAudit_20260531_CN.md`

- [ ] **Step 1: Run final TypeScript verification**

Run:

```powershell
Set-Location D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
```

Expected:

```text
build succeeds.
test:node succeeds.
```

- [ ] **Step 2: Run final C++ compile**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected:

```text
Result: Succeeded
```

- [ ] **Step 3: Run final residual scans**

Run from repo root:

```powershell
Set-Location D:\UEProjects\Template\Plugins\BlueprintHelper
rg -n "set_node_position|node_position|SetNodePosition|NodePosition|DEPRECATED_LAYOUT" AgentFaceService/task-core/src BlueprintHelper/Source/BlueprintHelper/Private/Runtime BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp -g "*.ts" -g "*.h" -g "*.cpp"
rg -n "first Review UI slice|Archive-baseline rollback backend is not wired" BlueprintHelper/Source -g "*.h" -g "*.cpp"
```

Expected:

```text
No active production hits for the removed layout mutation path.
No hits for the first-slice Review Reject placeholder.
```

- [ ] **Step 4: Update the Gap document with evidence**

In `BlueprintHelper/Develop/Gap/BlueprintHelper_LegacyResiduals_SourceAudit_20260531_CN.md`, update:

```text
### Gap 1. Deprecated GraphWrite layout path is still active end-to-end
```

to record:

```text
Status update - 2026-05-31:
Closed after removal of agent-facing TaskSpec schema support, task-core compiler lowering, UE TaskRuntime lowering, and PatchBlueprintGraphService SetNodePosition mutation support.
Evidence:
- npm.cmd run build: PASS
- npm.cmd run test:node: PASS
- UE 5.6 TemplateEditor build: PASS
- residual scan for set_node_position/node_position in active production mutation path: PASS
```

Update:

```text
### Gap 2. Review Reject still has reachable first-slice placeholder fallback
```

to record:

```text
Status update - 2026-05-31:
Closed after no-options RejectVisibleChange fallback was changed to persisted_review_targets_not_found and the first-slice placeholder string was removed from active source.
Evidence:
- UE 5.6 TemplateEditor build: PASS
- targeted Review automation: PASS
- residual scan for first Review UI slice placeholder: PASS
```

Only mark a Gap closed if the corresponding commands actually pass. If automation could not run because the editor lifecycle or Bridge was unavailable, leave the Gap status as OPEN and record the concrete blocker.

- [ ] **Step 5: Run Markdown residual check**

Run:

```powershell
Select-String -LiteralPath 'BlueprintHelper\Develop\Gap\BlueprintHelper_LegacyResiduals_SourceAudit_20260531_CN.md' -Pattern 'OPEN|not_run|BLOCKED|first Review UI slice|set_node_position'
```

Expected:

```text
Any remaining OPEN / not_run / BLOCKED text must match the real verification state. Do not erase blocker text to make the document look closed.
```

- [ ] **Step 6: Record final manual commit scope and message**

Do not run git commands. Suggested manual commit message after all implementation tasks pass:

```text
修复内容：
1. 移除 GraphWrite 旧布局写入通道
2. 移除 Review Reject 旧占位回退分支

变更需求：
1. 更新旧实现残留 Gap 文档为真实收敛状态
```

Manual file list should include only files changed by this plan:

```text
AgentFaceService/task-core/src/task/schema/task-schemas.ts
AgentFaceService/task-core/src/task/schema/task-contract.ts
AgentFaceService/task-core/src/task/compiler/task-compiler.ts
AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts
AgentFaceService/task-core/src/task/schema/task-schemas.deprecated-layout.test.ts
AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts
AgentFaceService/task-core/src/tests/architecture/architecture-boundaries.test.ts
BlueprintHelper/Source/BlueprintHelper/Public/Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h
BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h
BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp
BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp
BlueprintHelper/Develop/Gap/BlueprintHelper_LegacyResiduals_SourceAudit_20260531_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_LegacyResiduals_RemovalImplementationPlan_20260531_CN.md
```

---

## Self-Review

Spec coverage:

- Gap 1 is covered by Tasks 1-4 and final verification in Task 7.
- Gap 2 is covered by Tasks 5-6 and final verification in Task 7.
- Non-blocking audit items are not implemented as code changes because the Gap file classified them as non-blocking residuals or acceptable guards.

Placeholder scan:

- The plan contains no unresolved placeholder markers.
- Every code-changing step names exact files and concrete snippets.
- Every verification step has an expected result.

Type consistency:

- TypeScript plan consistently removes `set_node_position`, `node_position`, and `preserve_layout` from public schema / contract / compiler surfaces.
- C++ plan consistently removes `NodePosition`, `SetNodePosition`, and `ApplySetNodePosition` from the active GraphWrite mutation path.
- Review plan consistently replaces the old placeholder with `persisted_review_targets_not_found`.
