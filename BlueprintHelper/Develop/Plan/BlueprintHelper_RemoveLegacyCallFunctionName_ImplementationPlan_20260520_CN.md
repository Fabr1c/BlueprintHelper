# BlueprintHelper Remove Legacy call_function/name Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or inline execution with explicit checkpoints to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 从 AgentFace schema、compiler、fixtures、docs 中移除旧 `call_function/name` 入口，使函数调用只使用 GraphStatement `kind="call"` + `target`。

**Architecture:** 本计划只清理 Agent-facing legacy 入口，不改变 UE 侧 CallFunctionResolver 主路径。`call` 继续通过 SemanticIR -> FragmentDAG -> CallFunctionResolver -> ActionDatabase/BlueprintActionFilter -> UBlueprintNodeSpawner 写图；旧 `call_function/name` 不再被 schema、compiler、docs 接受或推荐。

**Tech Stack:** TypeScript task-core schema/compiler/docs, CodexPlugin Markdown references, BlueprintHelper Develop/Plan docs.

---

## 背景

当前 `call` 主路径已经符合精简 AgentFace 语义：

```json
{
  "kind": "call",
  "target": "Print String",
  "args": {
    "In String": {
      "kind": "literal",
      "value_type": "string",
      "value": "Hello"
    }
  }
}
```

但项目中仍保留旧兼容入口：

```json
{
  "kind": "call_function",
  "name": "PrintString"
}
```

这与当前硬性规则冲突：架构变更不继续保留旧 Agent / 旧字段 / 旧工具兼容。后续设计新语义族前，需要先移除该兼容入口，避免新旧协议混用。

## 范围

### 必须移除

- `call_function` 作为 AgentFace statement kind。
- `name` 作为函数调用目标字段。
- compiler 中 `call_function -> call` 的自动转换。
- docs 中 “legacy-compatible `kind="call_function"` + `name`” 描述。
- fixtures 中使用 `call_function/name` 的样例。

### 不在本计划内

- 不改 UE 侧 `FBlueprintHelperCallFunctionResolver` 的解析能力。
- 不改 `CallFunctionResolver` 的 ActionDatabase / BlueprintActionFilter / NodeSpawner 主路径。
- 不移除 C++ 内部类名中的 `CallFunction`，因为这是 UE/K2 节点语义，不是 AgentFace legacy 字段。
- 不处理 `set_member_variable/name`，除非执行时发现它和 `call_function/name` 共享同一段必须拆开的旧兼容逻辑；如需处理，应单独立项。

## 文件清单

### TypeScript schema / contract

- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Generated or build output after build: `AgentFaceService/task-core/build/task/schema/task-contract.d.ts`

### TypeScript compiler

- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify if affected: `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`

### Docs

- Modify: `AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`
- Modify: `AgentFaceService/docs/TaskSpec_CLI_QuickStart.md`
- Modify: `AgentFaceService/docs/CLI_Tools_API_Reference.md`
- Modify: `CodexPlugin/skills/blueprint-helper/references/05_Edit_Blueprint_Workflow.md`
- Modify: `CodexPlugin/skills/blueprint-helper/references/04_Tool_Surface_Field_Templates_20260512.md`

### Existing stable note

- Keep or update if needed: `BlueprintHelper/Develop/Plan/BlueprintHelper_CLI_Tips_20260514_CN.md`

This Tips document already says graph statement function calls should use `kind=call`; only update it if implementation details change.

---

## Task 1: Tighten task protocol contract

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`

- [ ] **Step 1: Remove legacy kind from contract**

Change `supported_first_slice` from:

```ts
legacy_statement_kinds: ['call_function', 'set_member_variable'],
```

to either no field, or a field that no longer includes `call_function`:

```ts
legacy_statement_kinds: ['set_member_variable'],
```

Preferred if no remaining consumer depends on the field:

```ts
// Remove legacy_statement_kinds entirely.
```

- [ ] **Step 2: Keep canonical statement kinds unchanged**

Confirm `statement_kinds` remains:

```ts
statement_kinds: ['call', 'set', 'branch', 'let', 'return'],
```

- [ ] **Step 3: Run task-core build when implementation reaches validation**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
```

Expected:

```text
0 TypeScript errors
```

If build emits generated declaration files, review `AgentFaceService/task-core/build/task/schema/task-contract.d.ts` and confirm it no longer exposes `call_function`.

---

## Task 2: Reject `call_function/name` in compiler

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`

- [ ] **Step 1: Remove `call_function` from supported statements**

Change validation from accepting:

```ts
['call', 'set', 'branch', 'let', 'return', 'call_function', 'set_member_variable']
```

to:

```ts
['call', 'set', 'branch', 'let', 'return', 'set_member_variable']
```

If `set_member_variable` is also being removed in a separate task, do not mix it into this plan unless explicitly approved.

- [ ] **Step 2: Remove `call_function -> call` rewrite**

Delete the branch:

```ts
if (statement.kind === 'call_function') {
  out.kind = 'call';
  out.target = (statement as Record<string, unknown>).name;
  delete out.name;
}
```

The compiler must not silently accept old input.

- [ ] **Step 3: Remove `call_function` branches in statement compilation**

Replace checks like:

```ts
if (statement.kind === 'call' || statement.kind === 'call_function') {
```

with:

```ts
if (statement.kind === 'call') {
```

Replace function target extraction with:

```ts
const functionName = getRequiredString(statement, 'target', `${path}.target`);
```

- [ ] **Step 4: Replace internal synthetic interface body output**

In `compositeInterfaceImplementationBody`, replace generated old shape:

```ts
{
  kind: 'call_function',
  name: implementation['call'],
  args: implementation['args'],
}
```

with canonical shape:

```ts
{
  kind: 'call',
  target: implementation['call'],
  args: implementation['args'],
}
```

- [ ] **Step 5: Keep error message explicit**

When a user provides `call_function`, expected compiler behavior should be a clear unsupported statement error:

```text
Use call, set, branch, let, or return.
```

Do not mention `call_function` as a repair path.

---

## Task 3: Update fixtures and tests

**Files:**
- Modify: `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`
- Modify tests only if they explicitly assert old compatibility.

- [ ] **Step 1: Replace fixture statements**

Replace:

```ts
{
  kind: 'call_function',
  name: 'PrintString',
}
```

with:

```ts
{
  kind: 'call',
  target: 'PrintString',
}
```

- [ ] **Step 2: Replace nested fixture statements**

Apply the same replacement inside nested `branch.then`, `branch.else`, interface implementation, and merge/replace graph fixture bodies.

- [ ] **Step 3: Add one negative compiler assertion if there is an existing compiler test file**

Expected behavior:

```ts
expect(() => compileTaskSpec(specWithCallFunction)).toThrow(/unsupported_statement_kind/);
```

Use the project’s existing test helper names; do not introduce a new test harness just for this assertion.

- [ ] **Step 4: Run targeted TypeScript validation**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
```

Expected:

```text
0 TypeScript errors
```

If a test script exists for task-core, run the narrow task compiler tests only. Do not run broad unrelated suites unless needed.

---

## Task 4: Update AgentFace docs

**Files:**
- Modify: `AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`
- Modify: `AgentFaceService/docs/TaskSpec_CLI_QuickStart.md`
- Modify: `AgentFaceService/docs/CLI_Tools_API_Reference.md`

- [ ] **Step 1: Remove old examples**

Delete examples that show:

```json
{
  "kind": "call_function",
  "name": "PrintString"
}
```

Replace with:

```json
{
  "kind": "call",
  "target": "PrintString"
}
```

- [ ] **Step 2: Rewrite CallFunction notes**

The docs should say:

```text
TaskSpec body.statements[] uses GraphStatement short form kind="call" with target. The target may be a native function name, Blueprint display name, owner-qualified native name, or supported explicit target_object member call. The legacy kind="call_function" + name shape is not supported.
```

- [ ] **Step 3: Keep candidate guidance**

Retain `candidate_functions`, `search_mode`, `category_priority`, and `ambiguity` guidance. These are current architecture fields and should not be removed.

---

## Task 5: Update CodexPlugin references

**Files:**
- Modify: `CodexPlugin/skills/blueprint-helper/references/05_Edit_Blueprint_Workflow.md`
- Modify: `CodexPlugin/skills/blueprint-helper/references/04_Tool_Surface_Field_Templates_20260512.md`

- [ ] **Step 1: Remove plugin guidance that recommends legacy shape**

Delete text that says:

```text
legacy-compatible kind="call_function" + name remains accepted
```

- [ ] **Step 2: Replace plugin examples**

Use:

```json
{
  "kind": "call",
  "target": "Print String",
  "args": {
    "In String": {
      "kind": "literal",
      "value_type": "string",
      "value": "Hello"
    }
  }
}
```

- [ ] **Step 3: Keep explicit object call guidance canonical**

Use:

```json
{
  "kind": "call",
  "target_object": "SmokeMesh",
  "target": "SetVisibility",
  "args": {
    "NewVisibility": {
      "kind": "literal",
      "value_type": "bool",
      "value": false
    }
  }
}
```

---

## Task 6: Validate behavior boundary

**Files:**
- No source modification expected unless validation reveals stale references.

- [ ] **Step 1: Search for stale Agent-facing legacy references**

Run:

```powershell
rg -n "kind.*call_function|call_function.*name|legacy-compatible.*call_function|legacy_statement_kinds.*call_function" AgentFaceService CodexPlugin BlueprintHelper/Develop/Plan
```

Expected:

```text
No live AgentFace schema/compiler/docs references that present call_function/name as supported.
```

Archived historical docs under `BlueprintHelper/Develop/v0.*` may remain unchanged if they are clearly archived records.

- [ ] **Step 2: Verify canonical `call` still works**

After implementation, run the narrow preview/execute smoke that writes a fresh graph with:

```json
{
  "kind": "call",
  "target": "Print String",
  "args": {
    "In String": {
      "kind": "literal",
      "value_type": "string",
      "value": "canonical call"
    }
  }
}
```

Expected:

```text
preview passes
execute writes graph
no call_function/name compatibility path is used
```

- [ ] **Step 3: Verify old `call_function` fails clearly**

Run preview with:

```json
{
  "kind": "call_function",
  "name": "PrintString"
}
```

Expected:

```text
preview blocked
error_code or issue code: unsupported_statement_kind
message tells user to use kind="call" + target
```

---

## Completion criteria

- [ ] `call_function/name` is not accepted by AgentFace compiler.
- [ ] `call_function/name` is not advertised by current docs or plugin references.
- [ ] Canonical `kind="call"` + `target` still compiles through SemanticIR and UE CallFunctionResolver.
- [ ] `candidate_functions`, `search_mode`, `category_priority`, `ambiguity`, and `target_object` remain supported.
- [ ] Build validation has run for `AgentFaceService/task-core`.
- [ ] If editor lifecycle is available, one canonical call preview/execute smoke has passed.

## 距离期望差距

当前只是计划文档，尚未执行代码清理。实际完成前，项目仍然保留 `call_function/name` legacy 兼容入口。

## 阻塞内容

1. 尚未执行实现。
2. 尚未运行 TypeScript build 或编辑器端 smoke。

