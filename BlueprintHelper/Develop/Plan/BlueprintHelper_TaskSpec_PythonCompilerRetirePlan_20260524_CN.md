# TaskSpec Python Compiler Retire Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 TaskSpec compiler 的生产与测试 canonical source 收敛到 TypeScript task-core compiler，移除 Python compiler fallback、parity oracle、Python tests 和相关 contract 依赖。

**Architecture:** TS compiler 从 `ts_fast_path` 提升为唯一 canonical compiler；compiler service 继续保留统一选择边界，但不再注册或 fallback 到 Python。删除 Python compiler 时必须先保证 BUG-001/002 和四簇 smoke 已通过，避免把 smoke blocker 与 retire 改动混在同一风险面。

**Tech Stack:** TypeScript task-core compiler/service/policy, Node tests, UE 5.6 C++ contract automation, PowerShell verification.

---

## Preconditions

执行本计划前必须满足：

```text
BUG-001 fixed and four-cluster Function+Field smoke positive path passes.
BUG-002 fixed and four-cluster EventDelegate smoke positive path passes.
npm --prefix AgentFaceService/task-core run test:node passes.
UE 5.6 TemplateEditor build passes.
```

如果任一条件不满足，先执行 `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Bug001_002_FixPlan_20260524_CN.md`，不要开始 retire。

## File Structure

- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Responsibility: strategy id type 收敛到 canonical TS。
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
  - Responsibility: 删除 Python strategy 注册和 import，仅注册 canonical TS strategy。
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-policy.ts`
  - Responsibility: 删除 parity/fallback policy，默认选择 canonical TS，不再支持 Python strategy env alias。
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-service.ts`
  - Responsibility: 保留 service 边界，去掉 fallback diagnostics。
- Delete: `AgentFaceService/task-core/src/task/compiler/task-python-orchestrator.ts`
  - Responsibility: 删除 Node -> Python 子进程桥。
- Delete: `AgentFaceService/task-core/src/task/compiler/task-plan-parity.ts`
  - Responsibility: 删除 TS/Python parity oracle。
- Modify: `AgentFaceService/task-core/src/index.ts`
  - Responsibility: 停止导出 Python orchestrator 和 parity API。
- Modify: `AgentFaceService/task-core/package.json`
  - Responsibility: 删除 `test:python`，`test` 只跑 build + node tests。
- Modify: `AgentFaceService/mcp/package.json`
  - Responsibility: 删除转发到 task-core `test:python` 的 MCP package script，避免安装/验证入口保留 Python 测试别名。
- Delete: `AgentFaceService/task-core/python/`
  - Responsibility: 删除 Python compiler package、runtime orchestrator、tests、pycache。
- Delete: `AgentFaceService/task-core/src/tests/task/task-python-orchestrator.regression.test.ts`
  - Responsibility: 删除 Python orchestrator coverage。
- Delete: `AgentFaceService/task-core/src/tests/task/task-plan-parity.test.ts`
  - Responsibility: 删除 TS/Python parity coverage。
- Create: `AgentFaceService/task-core/src/tests/task/task-compiler.canonical-ts.test.ts`
  - Responsibility: 添加 canonical TS compiler smoke，替代 parity oracle 的最低覆盖。
- Modify: `AgentFaceService/task-core/src/tests/task/task-compiler-policy.test.ts`
  - Responsibility: 覆盖 canonical TS only policy。
- Modify: `AgentFaceService/task-core/src/tests/task/task-spec-runner.regression.test.ts`
  - Responsibility: 预期 strategy 从 `canonical_python` / `ts_fast_path` 收敛为 `canonical_ts`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - Responsibility: EventDelegate boundary source guard 改读 TS contract/compiler，不再读 `graph_write_append.py`。
- Modify: docs under `BlueprintHelper/Develop/Plan` and `BlueprintHelper/Develop/Gap` only where they describe live compiler ownership.
  - Responsibility: 将 live ownership 从 “TS fast path + Python canonical fallback” 改为 “canonical TS compiler”。
- Modify: live docs under `AgentFaceService/docs/*.md` only where they describe current TaskSpec compiler ownership or current executable path.
  - Responsibility: 将 `TaskSpec -> Python compiler`、`task-core/python`、`TS/Python compiler` 等 live truth 改为 canonical TS compiler；历史段落必须明确标注为 historical，不能作为当前状态。
- Modify: current AgentFace documentation files that still describe Python compiler ownership:
  - `AgentFaceService/docs/Install_CLI_QuickStart.md`
  - `AgentFaceService/docs/TaskSpec_CLI_QuickStart.md`
  - `AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`
  - `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
  - Responsibility: 当前 CLI/TaskSpec guidance 统一改为 canonical TS compiler。
- Modify: current BlueprintHelper design/gap/status docs that still describe TS/Python compiler ownership:
  - `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - `BlueprintHelper/Develop/Design/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md`
  - `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_PublicSchemaDelegateLowering_CodingStyleGaps_20260524_CN.md`
  - Responsibility: 当前设计/Gap/status truth 统一改为 canonical TS compiler；已完成历史 evidence 不得继续作为当前入口说明。
- Modify: `ClaudePlugin/README.md`
  - Responsibility: 将 Agent-facing TaskSpec-first 架构链路中的 `task-core -> Python Task Compiler` 与 “dispatches to the Python compiler” 改为 canonical TS compiler。
- Archive or rewrite: non-archived historical implementation/smoke plans under `BlueprintHelper/Develop/Plan/*.md` that still contain Python compiler strategy/test paths.
  - Responsibility: 若文档仍是当前计划/状态，改写为 canonical TS；若只是旧执行记录，移动到 `BlueprintHelper/Develop/ArchivedReference/TaskSpecPythonCompilerRetire_20260524/` 或用无 Python compiler 词面的历史指针替代 active copy，保证 active Plan 目录不再保留 Python compiler ownership 残留。
- Audit only: root install/update scripts and setup docs (`install.ps1`, `install.cmd`, `update.ps1`, `update.cmd`, `upgrade.cmd`, `INSTALL.md`) plus plugin docs/scripts.
  - Responsibility: 确认安装、更新、bootstrap、global MCP 安装路径没有 Python compiler / `test:python` / `task-core/python` 残留；若扫描发现残留，必须加入本计划修改范围。

## Task 1: Add Canonical TS Policy Tests

**Files:**
- Modify: `AgentFaceService/task-core/src/tests/task/task-compiler-policy.test.ts`

- [ ] **Step 1: Replace fallback-oriented tests**

Replace these tests:

```ts
test('default compiler policy selects canonical_python for TaskSpec types not covered by TS fast path', () => {
  const policy = createTaskCompilerPolicy();
  const registry = createDefaultTaskCompilerRegistry();
  const selection = policy.select(TaskSpecSchema.parse(makeAssetFactoryTaskSpec()), registry);

  assert.equal(selection.strategyId, 'canonical_python');
  assert.equal(selection.fallbackReason, 'ts_fast_path_not_supported');
});

test('default compiler policy selects ts_fast_path for covered TaskSpec types with passed parity', () => {
  const policy = createTaskCompilerPolicy();
  const registry = createDefaultTaskCompilerRegistry();
  const selection = policy.select(TaskSpecSchema.parse(makeGraphWriteTaskSpec()), registry);

  assert.equal(selection.strategyId, 'ts_fast_path');
  assert.equal(selection.parityStatus, 'passed');
});

test('compiler policy disables ts_fast_path when parity gate is failed', () => {
  const policy = createTaskCompilerPolicy({
    parityTable: {
      edit_blueprint_graph: {
        status: 'failed',
        reason: 'intentional_test_failure',
      },
    },
  });
  const registry = createDefaultTaskCompilerRegistry();
  const selection = policy.select(TaskSpecSchema.parse(makeGraphWriteTaskSpec()), registry);

  assert.equal(selection.strategyId, 'canonical_python');
  assert.equal(selection.parityStatus, 'failed');
  assert.equal(selection.fallbackReason, 'ts_fast_path_parity_failed');
});
```

with:

```ts
test('default compiler policy selects canonical_ts for covered TaskSpec types', () => {
  const policy = createTaskCompilerPolicy();
  const registry = createDefaultTaskCompilerRegistry();
  const selection = policy.select(TaskSpecSchema.parse(makeGraphWriteTaskSpec()), registry);

  assert.equal(selection.strategyId, 'canonical_ts');
  assert.equal(Object.hasOwn(selection, 'fallbackReason'), false);
  assert.equal(Object.hasOwn(selection, 'parityStatus'), false);
});

test('default compiler policy rejects TaskSpec types not supported by canonical TS compiler', () => {
  const policy = createTaskCompilerPolicy();
  const registry = createDefaultTaskCompilerRegistry();

  assert.throws(
    () => policy.select(TaskSpecSchema.parse(makeAssetFactoryTaskSpec()), registry),
    (err: unknown) => err instanceof TaskSpecCompileError
      && err.code === 'task_compiler_strategy_unavailable'
      && err.issues.some((issue) => issue.message.includes('canonical_ts')),
  );
});
```

- [ ] **Step 2: Update forced strategy test**

Replace:

```ts
const compiler = createTaskSpecCompiler({ strategyMode: 'ts_fast_path' });
```

with:

```ts
const compiler = createTaskSpecCompiler({ strategyMode: 'canonical_ts' });
```

Replace expected strategy in the service test:

```ts
assert.equal(compiled.strategyId, 'ts_fast_path');
assert.equal(compiled.diagnostics?.parityStatus, 'passed');
```

with:

```ts
assert.equal(compiled.strategyId, 'canonical_ts');
assert.equal(compiled.diagnostics?.parityStatus, undefined);
```

- [ ] **Step 3: Run policy test and verify it fails before implementation**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run build
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\build\tests\task\task-compiler-policy.test.js
```

Expected before implementation:

```text
actual 'ts_fast_path' does not equal expected 'canonical_ts'
```

## Task 2: Collapse Compiler Strategy Model To Canonical TS

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-policy.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-service.ts`

- [ ] **Step 1: Narrow strategy type**

In `task-compiler.ts`, replace:

```ts
export type TaskCompilerStrategyId = 'canonical_python' | 'ts_fast_path' | 'python_worker';
```

with:

```ts
export type TaskCompilerStrategyId = 'canonical_ts';
```

- [ ] **Step 2: Replace registry implementation**

In `task-compiler-registry.ts`, remove:

```ts
import { compileTaskSpecWithPython } from './task-python-orchestrator.js';
```

Replace the strategy set name and factory with:

```ts
const CANONICAL_TS_TASK_TYPES = new Set([
  'create_blueprint_feature',
  'edit_blueprint_graph',
  'edit_blueprint_variables',
  'edit_object_properties',
  'edit_blueprint_signature',
]);
```

Replace `createDefaultTaskCompilerRegistry` with:

```ts
export function createDefaultTaskCompilerRegistry(): TaskCompilerRegistry {
  return new TaskCompilerRegistry()
    .register(createCanonicalTsCompilerStrategy());
}
```

Delete `createCanonicalPythonCompilerStrategy` and `createDisabledPythonWorkerCompilerStrategy`.

Replace `createTsFastPathCompilerStrategy` with:

```ts
export function createCanonicalTsCompilerStrategy(): TaskCompilerStrategy {
  return {
    id: 'canonical_ts',
    canCompile(taskSpec) {
      return CANONICAL_TS_TASK_TYPES.has(taskSpec.task_type);
    },
    async compile(taskSpec, _options: TaskCompileOptions) {
      return createCompiledTaskPlan({
        taskPlan: compileTaskSpecToTaskPlan(taskSpec),
        strategyId: 'canonical_ts',
      });
    },
  };
}

export function isCanonicalTsTaskType(taskType: string): boolean {
  return CANONICAL_TS_TASK_TYPES.has(taskType);
}
```

- [ ] **Step 3: Replace policy implementation**

In `task-compiler-policy.ts`, remove parity table types and fallback fields:

```ts
export type TaskCompilerParityStatus = 'passed' | 'failed' | 'unknown';
export interface TaskCompilerParityEntry { ... }
export type TaskCompilerParityTable = ...
```

Set the strategy mode type to:

```ts
export type TaskCompilerStrategyMode = TaskCompilerStrategyId | 'auto';
```

Set env aliases to:

```ts
const STRATEGY_MODE_ALIASES: Record<string, TaskCompilerStrategyMode> = {
  auto: 'auto',
  canonical: 'canonical_ts',
  canonical_ts: 'canonical_ts',
};
```

Replace `createTaskCompilerPolicy` with:

```ts
export function createTaskCompilerPolicy(options: TaskCompilerPolicyOptions = {}): TaskCompilerPolicy {
  const strategyMode = options.strategyMode ?? strategyModeFromEnv() ?? 'auto';

  return {
    select(taskSpec, registry) {
      const strategyId = strategyMode === 'auto' ? 'canonical_ts' : strategyMode;
      const strategy = registry.require(strategyId);
      if (!strategy.canCompile(taskSpec, { dryRun: true })) {
        throw unavailableStrategy(strategyId, taskSpec, 'strategy_cannot_compile_task_type');
      }
      return { strategyId };
    },
  };
}
```

Keep `unavailableStrategy`, but remove references to parity/fallback fields.

- [ ] **Step 4: Simplify compiler service diagnostics**

In `task-compiler-service.ts`, remove this block from diagnostics merge:

```ts
          ...(selection.parityStatus ? { parityStatus: selection.parityStatus } : {}),
          ...(selection.parityReason ? { parityReason: selection.parityReason } : {}),
          ...(selection.fallbackReason ? { fallbackReason: selection.fallbackReason } : {}),
```

Keep the service boundary and structured error wrapping.

- [ ] **Step 5: Run compiler policy test**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run build
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\build\tests\task\task-compiler-policy.test.js
```

Expected:

```text
all policy tests pass
```

## Task 3: Remove Python Orchestrator And Parity Exports

**Files:**
- Delete: `AgentFaceService/task-core/src/task/compiler/task-python-orchestrator.ts`
- Delete: `AgentFaceService/task-core/src/task/compiler/task-plan-parity.ts`
- Modify: `AgentFaceService/task-core/src/index.ts`
- Delete: `AgentFaceService/task-core/src/tests/task/task-python-orchestrator.regression.test.ts`
- Delete: `AgentFaceService/task-core/src/tests/task/task-plan-parity.test.ts`
- Create: `AgentFaceService/task-core/src/tests/task/task-compiler.canonical-ts.test.ts`

- [ ] **Step 1: Remove exports**

In `index.ts`, delete:

```ts
export * from './task/compiler/task-python-orchestrator.js';
export * from './task/compiler/task-plan-parity.js';
```

- [ ] **Step 2: Delete Python-specific TypeScript files**

Delete:

```text
AgentFaceService/task-core/src/task/compiler/task-python-orchestrator.ts
AgentFaceService/task-core/src/task/compiler/task-plan-parity.ts
AgentFaceService/task-core/src/tests/task/task-python-orchestrator.regression.test.ts
AgentFaceService/task-core/src/tests/task/task-plan-parity.test.ts
```

- [ ] **Step 3: Delete parity test and add a TS-only compiler smoke**

Create `AgentFaceService/task-core/src/tests/task/task-compiler.canonical-ts.test.ts` with:

```ts
import assert from 'node:assert/strict';
import test from 'node:test';
import { compileTaskSpecToTaskPlan } from '../../task/compiler/task-compiler.js';
import { TaskSpecSchema } from '../../task/schema/task-schemas.js';

test('canonical TS compiler emits graph_write TaskPlan for edit_blueprint_graph', () => {
  const taskPlan = compileTaskSpecToTaskPlan(TaskSpecSchema.parse({
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_canonical_ts_graph',
    task_type: 'edit_blueprint_graph',
    feature_name: 'CanonicalTsGraph',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'BH_CanonicalTsGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'RunCanonicalTsGraph',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [],
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  } as never));

  assert.equal(taskPlan.schema, 'BlueprintHelper.TaskPlan.v1');
  assert.equal(taskPlan.steps.some((step) => (step as Record<string, unknown>).capability === 'graph_write'), true);
});
```

- [ ] **Step 4: Run a source search**

Run:

```powershell
rg -n "task-python-orchestrator|compileTaskSpecWithPython|compareTaskPlanParity|task-plan-parity" D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\src
```

Expected:

```text
no matches
```

## Task 4: Remove Python Package And Python Test Scripts

**Files:**
- Modify: `AgentFaceService/task-core/package.json`
- Modify: `AgentFaceService/mcp/package.json`
- Delete: `AgentFaceService/task-core/python/`

- [ ] **Step 1: Remove Python test script from package.json**

Replace:

```json
"scripts": {
  "build": "node ../scripts/clean-build.mjs && node ../scripts/run-tsc.mjs",
  "test": "npm run build && npm run test:python && npm run test:node",
  "test:node": "node scripts/run-node-tests.mjs",
  "test:python": "python -m unittest discover -s python/tests -t python",
  "dev": "tsc --watch"
}
```

with:

```json
"scripts": {
  "build": "node ../scripts/clean-build.mjs && node ../scripts/run-tsc.mjs",
  "test": "npm run build && npm run test:node",
  "test:node": "node scripts/run-node-tests.mjs",
  "dev": "tsc --watch"
}
```

- [ ] **Step 2: Remove MCP package Python test forwarding script**

In `AgentFaceService/mcp/package.json`, replace:

```json
"scripts": {
  "build": "node ../scripts/run-package-npm.mjs ../task-core run build && node ../scripts/clean-build.mjs && node ../scripts/run-tsc.mjs",
  "build:mcp": "node ../scripts/clean-build.mjs && node ../scripts/run-tsc.mjs",
  "test": "node ../scripts/run-package-npm.mjs ../task-core run test && npm run build:mcp && npm run test:node",
  "test:node": "node scripts/run-node-tests.mjs",
  "test:python": "node ../scripts/run-package-npm.mjs ../task-core run test:python",
  "start": "node build/index.js",
  "dev": "tsc --watch"
}
```

with:

```json
"scripts": {
  "build": "node ../scripts/run-package-npm.mjs ../task-core run build && node ../scripts/clean-build.mjs && node ../scripts/run-tsc.mjs",
  "build:mcp": "node ../scripts/clean-build.mjs && node ../scripts/run-tsc.mjs",
  "test": "node ../scripts/run-package-npm.mjs ../task-core run test && npm run build:mcp && npm run test:node",
  "test:node": "node scripts/run-node-tests.mjs",
  "start": "node build/index.js",
  "dev": "tsc --watch"
}
```

- [ ] **Step 3: Delete the Python compiler tree**

Delete the directory:

```text
AgentFaceService/task-core/python/
```

Before deleting, verify the resolved path is exactly:

```powershell
Resolve-Path D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\python
```

Expected:

```text
D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\python
```

Use a controlled file deletion method inside the workspace. Do not run `git add`, `git commit`, or `git push`.

- [ ] **Step 4: Verify package scripts**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test
rg -n '"test:python"|python/tests|task-core run test:python' D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\package.json D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\mcp\package.json D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\package.json
```

Expected:

```text
build passes
node tests pass
no python unittest invocation appears
package script search returns no matches
```

## Task 5: Update C++ Contract Guard Away From Python Source

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Replace Python source path with TS compiler and contract paths**

In `FBlueprintHelperGraphWriteDelegatePublicInternalBoundaryContractTest`, replace:

```cpp
const FString PythonCompilerPath = FPaths::Combine(
	FPaths::ProjectPluginsDir(),
	TEXT("BlueprintHelper"),
	TEXT("AgentFaceService"),
	TEXT("task-core"),
	TEXT("python"),
	TEXT("blueprinthelper_task"),
	TEXT("compiler"),
	TEXT("graph_write_append.py"));
```

with:

```cpp
const FString TsCompilerPath = FPaths::Combine(
	FPaths::ProjectPluginsDir(),
	TEXT("BlueprintHelper"),
	TEXT("AgentFaceService"),
	TEXT("task-core"),
	TEXT("src"),
	TEXT("task"),
	TEXT("compiler"),
	TEXT("task-compiler.ts"));
const FString TsContractPath = FPaths::Combine(
	FPaths::ProjectPluginsDir(),
	TEXT("BlueprintHelper"),
	TEXT("AgentFaceService"),
	TEXT("task-core"),
	TEXT("src"),
	TEXT("task"),
	TEXT("schema"),
	TEXT("task-contract.ts"));
```

Replace `PythonCompilerSource` with:

```cpp
FString TsCompilerSource;
FString TsContractSource;
```

Load both files and report:

```cpp
if (!FFileHelper::LoadFileToString(TsCompilerSource, *TsCompilerPath))
{
	AddError(FString::Printf(TEXT("TS GraphWrite compiler source could not be read: %s"), *TsCompilerPath));
	bClean = false;
}
if (!FFileHelper::LoadFileToString(TsContractSource, *TsContractPath))
{
	AddError(FString::Printf(TEXT("TS TaskSpec contract source could not be read: %s"), *TsContractPath));
	bClean = false;
}
```

- [ ] **Step 2: Replace required Python tokens**

Replace:

```cpp
const TArray<FString> RequiredPythonTokens = {
	TEXT("PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES"),
	TEXT("INTERNAL_DELEGATE_STATEMENT_KIND"),
	TEXT("FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS"),
	TEXT("delegate_operation")
};
for (const FString& Token : RequiredPythonTokens)
{
	bClean &= TestTrue(*FString::Printf(TEXT("Python compiler declares delegate boundary token %s"), *Token), PythonCompilerSource.Contains(Token));
}
```

with:

```cpp
const TArray<FString> RequiredTsCompilerTokens = {
	TEXT("PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES"),
	TEXT("INTERNAL_DELEGATE_STATEMENT_KIND"),
	TEXT("FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS"),
	TEXT("delegate_operation")
};
for (const FString& Token : RequiredTsCompilerTokens)
{
	bClean &= TestTrue(*FString::Printf(TEXT("TS compiler declares delegate boundary token %s"), *Token), TsCompilerSource.Contains(Token));
}

const TArray<FString> RequiredTsContractTokens = {
	TEXT("event_delegate_use_site_boundary"),
	TEXT("delegate.bind"),
	TEXT("delegate.unbind_all"),
	TEXT("public_to_internal_lowering")
};
for (const FString& Token : RequiredTsContractTokens)
{
	bClean &= TestTrue(*FString::Printf(TEXT("TS contract declares delegate boundary token %s"), *Token), TsContractSource.Contains(Token));
}
```

- [ ] **Step 3: Run contract automation**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -run=Automation -Test='BlueprintHelper.GraphWrite.ActionResolution.Contract.DelegatePublicInternalBoundary' -unattended -nop4 -nosplash -NullRHI"
```

Expected:

```text
PASSED
```

## Task 6: Update Strategy Expectations In Remaining Node Tests

**Files:**
- Modify: `AgentFaceService/task-core/src/tests/task/task-spec-runner.regression.test.ts`
- Modify any test found by source search.

- [ ] **Step 1: Replace old strategy names**

Run:

```powershell
rg -n "canonical_python|ts_fast_path|python_worker|parityStatus|fallbackReason" D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\src\tests
```

For every test expectation:

```ts
assert.equal(compiled.strategyId, 'ts_fast_path');
assert.equal(strategyStage?.strategy, 'canonical_python');
assert.equal(result.diagnostics?.fallbackReason, 'ts_fast_path_not_supported');
```

replace with canonical TS expectations:

```ts
assert.equal(compiled.strategyId, 'canonical_ts');
assert.equal(strategyStage?.strategy, 'canonical_ts');
assert.equal(result.diagnostics?.fallbackReason, undefined);
```

Delete tests that exist only to verify Python fallback or parity disable behavior rather than preserving a fake compatibility path.

- [ ] **Step 2: Run node tests**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run build
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test:node
```

Expected:

```text
all node tests pass
```

## Task 7: Source And Documentation Cleanup

**Files:**
- Modify live docs only where current ownership is stated:
  - `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
  - `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
  - any current non-archived `BlueprintHelper/Develop/Plan/*.md` that says Python compiler remains canonical fallback.
  - `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - `BlueprintHelper/Develop/Design/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md`
  - `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_PublicSchemaDelegateLowering_CodingStyleGaps_20260524_CN.md`
  - `AgentFaceService/docs/Install_CLI_QuickStart.md`
  - `AgentFaceService/docs/TaskSpec_CLI_QuickStart.md`
  - `AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`
  - `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
  - `ClaudePlugin/README.md`

- [ ] **Step 1: Search live source and docs**

Run:

```powershell
rg -n "canonical_python|python_worker|compileTaskSpecWithPython|task-python-orchestrator|task-plan-parity|Python compiler|Python Task Compiler|dispatches to the Python compiler|TaskSpec -> Python|task-core -> Python|task-core/python|blueprinthelper_task|compile-task-spec|test:python|ts_fast_path|TS fast path|TS/Python compiler|graph_write_append.py" `
  D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService `
  D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop `
  D:\UEProjects\Template\Plugins\BlueprintHelper\install.ps1 `
  D:\UEProjects\Template\Plugins\BlueprintHelper\install.cmd `
  D:\UEProjects\Template\Plugins\BlueprintHelper\update.ps1 `
  D:\UEProjects\Template\Plugins\BlueprintHelper\update.cmd `
  D:\UEProjects\Template\Plugins\BlueprintHelper\upgrade.cmd `
  D:\UEProjects\Template\Plugins\BlueprintHelper\INSTALL.md `
  D:\UEProjects\Template\Plugins\BlueprintHelper\CodexPlugin `
  D:\UEProjects\Template\Plugins\BlueprintHelper\ClaudePlugin `
  -g "*.ts" -g "*.md" -g "*.cpp" -g "*.h" -g "*.json" -g "*.ps1" -g "*.cmd" -g "*.bat" -g "*.cjs" -g "*.mjs" `
  -g "!**/build/**" -g "!**/node_modules/**" `
  -g "!**/v0.*/**" `
  -g "!**/ArchivedReference/**" `
  -g "!**/PlanArtifacts/**" `
  -g "!**/BlueprintHelper_TaskSpec_PythonCompilerRetirePlan_20260524_CN.md"
```

Expected after cleanup:

```text
No source matches for canonical_python, python_worker, compileTaskSpecWithPython, task-python-orchestrator, task-plan-parity.
No package, install, update, bootstrap, plugin README, or active documentation matches for test:python, task-core/python, blueprinthelper_task, compile-task-spec, TaskSpec -> Python, task-core -> Python, Python Task Compiler, or Python compiler as current ownership.
The retire plan file itself, build output, node_modules, archived v0.* docs, ArchivedReference docs, and PlanArtifacts are excluded from this active-residual gate.
Archived docs may still mention historical Python compiler decisions under BlueprintHelper/Develop/v0.*; do not rewrite archived references unless they are imported into current status docs.
Current live docs state canonical TS compiler ownership.
```

- [ ] **Step 2: Update current docs with concise wording**

Use this wording in live status docs:

```text
TaskSpec compiler ownership: AgentFace task-core TypeScript compiler is the canonical production compiler. The former Python compiler fallback and TS/Python parity gate have been retired; new TaskSpec capabilities must be implemented and tested in TS first.
```

Do not edit archived historical reports unless a live status page links to them as current truth.

- [ ] **Step 3: Explicitly audit install/update scripts and plugin docs**

Run:

```powershell
rg -n "Python compiler|Python Task Compiler|dispatches to the Python compiler|TaskSpec -> Python|task-core -> Python|test:python|task-core/python|task-core\\python|blueprinthelper_task|compile-task-spec|canonical_python|ts_fast_path|task-python-orchestrator|task-plan-parity" D:\UEProjects\Template\Plugins\BlueprintHelper\install.ps1 D:\UEProjects\Template\Plugins\BlueprintHelper\install.cmd D:\UEProjects\Template\Plugins\BlueprintHelper\update.ps1 D:\UEProjects\Template\Plugins\BlueprintHelper\update.cmd D:\UEProjects\Template\Plugins\BlueprintHelper\upgrade.cmd D:\UEProjects\Template\Plugins\BlueprintHelper\INSTALL.md D:\UEProjects\Template\Plugins\BlueprintHelper\CodexPlugin D:\UEProjects\Template\Plugins\BlueprintHelper\ClaudePlugin -g "*.ps1" -g "*.cmd" -g "*.bat" -g "*.md" -g "*.cjs" -g "*.mjs" -g "*.json"
```

Expected:

```text
No Python compiler, Python Task Compiler, Python test, task-core/python, old strategy ownership, or `task-core -> Python` architecture path remains in install/update/bootstrap/plugin docs/scripts.
Generic mentions that normal repository tools may edit Python source files are acceptable only if they do not describe TaskSpec compiler ownership or invoke Python compiler/test:python.
```

- [ ] **Step 4: Clear non-archived BlueprintHelper Develop doc residuals**

Run:

```powershell
rg -l "canonical_python|python_worker|compileTaskSpecWithPython|task-python-orchestrator|task-plan-parity|Python compiler|Python Task Compiler|dispatches to the Python compiler|TaskSpec -> Python|task-core -> Python|task-core/python|blueprinthelper_task|compile-task-spec|test:python|ts_fast_path|TS fast path|TS/Python compiler|TS / Python compiler|graph_write_append.py" `
  D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop `
  -g "*.md" `
  -g "!**/v0.*/**" `
  -g "!**/ArchivedReference/**" `
  -g "!**/PlanArtifacts/**" `
  -g "!**/BlueprintHelper_TaskSpec_PythonCompilerRetirePlan_20260524_CN.md"
```

Current audit baseline before cleanup lists these active docs and plans:

```text
BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md
BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_PublicSchemaDelegateLowering_CodingStyleGaps_20260524_CN.md
BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md
BlueprintHelper/Develop/Design/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_ActionResolution_P2P3_FunctionGeneric_ImplementationPlan_20260521_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_DataFlowCore_FirstBatch_ImplementationPlan_20260521_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FieldKindConvergence_Plan_20260523_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourCluster_EndToEndSmokePlan_20260524_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Bug001_002_FixPlan_20260524_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FunctionFieldConvergence_SmokePlan_20260524_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_Gap5_EventDelegateUseSite_ImplementationPlan_20260523_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericBroadCreate_ImplementationPlan_20260524_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GenericConvertSchedule_ImplementationPlan_20260524_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_LegacyMainlineCleanup_ImplementationPlan_20260522_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_PublicSchemaDelegateLoweringBoundary_Plan_20260523_CN.md
BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_SemanticMainlineCleanup_ImplementationPlan_20260522_CN.md
```

For each baseline match:

```text
If the file is current guidance/status/design: replace Python compiler / TS fast path / parity wording with canonical TS compiler wording.
If the file is a completed historical implementation or smoke plan: move it under BlueprintHelper/Develop/ArchivedReference/TaskSpecPythonCompilerRetire_20260524/ or replace the active copy with a short pointer that contains no retired compiler terms.
Do not leave retired compiler strings in non-archived active docs or plans.
```

Expected after cleanup:

```text
No matches from the command above.
```

## Final Verification Commands

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\mcp run test
rg -n "canonical_python|python_worker|compileTaskSpecWithPython|task-python-orchestrator|task-plan-parity|graph_write_append.py|test:python|task-core/python|task-core\\python|blueprinthelper_task|compile-task-spec|TaskSpec -> Python|task-core -> Python|Python compiler|Python Task Compiler|dispatches to the Python compiler|ts_fast_path|TS fast path|TS/Python compiler" `
  D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService `
  D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source `
  D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop `
  D:\UEProjects\Template\Plugins\BlueprintHelper\install.ps1 `
  D:\UEProjects\Template\Plugins\BlueprintHelper\install.cmd `
  D:\UEProjects\Template\Plugins\BlueprintHelper\update.ps1 `
  D:\UEProjects\Template\Plugins\BlueprintHelper\update.cmd `
  D:\UEProjects\Template\Plugins\BlueprintHelper\upgrade.cmd `
  D:\UEProjects\Template\Plugins\BlueprintHelper\INSTALL.md `
  D:\UEProjects\Template\Plugins\BlueprintHelper\CodexPlugin `
  D:\UEProjects\Template\Plugins\BlueprintHelper\ClaudePlugin `
  -g "*.ts" -g "*.md" -g "*.cpp" -g "*.h" -g "*.json" -g "*.ps1" -g "*.cmd" -g "*.bat" -g "*.cjs" -g "*.mjs" `
  -g "!**/build/**" -g "!**/node_modules/**" `
  -g "!**/v0.*/**" `
  -g "!**/ArchivedReference/**" `
  -g "!**/PlanArtifacts/**" `
  -g "!**/BlueprintHelper_TaskSpec_PythonCompilerRetirePlan_20260524_CN.md"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -run=Automation -Test='BlueprintHelper.GraphWrite.ActionResolution.Contract.DelegatePublicInternalBoundary' -unattended -nop4 -nosplash -NullRHI"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1"
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Expected:

```text
task-core test uses build + node tests only
mcp test does not expose or invoke test:python
source/doc/script search finds no active Python compiler path, Python test script, old strategy id, plugin README stale compiler path, or current-doc Python compiler ownership after excluding build output, node_modules, historical archives, plan artifacts, and this retire plan file
DelegatePublicInternalBoundary automation passes
four-cluster smoke remains passing
UE build Result: Succeeded
```

## Manual Commit Guidance

Workers must not run `git add`, `git commit`, or `git push` in this repository. After verification, report changed files and suggest this manual commit message:

```text
变更需求：
1. 将 TaskSpec compiler canonical ownership 收敛到 TypeScript task-core compiler。
2. 移除 Python compiler fallback、parity oracle、Python tests、package script、安装/更新入口和相关 source/doc contract 依赖。
```

Suggested manual commands for the user only:

```powershell
git add AgentFaceService/task-core/src/task/compiler/task-compiler.ts `
        AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts `
        AgentFaceService/task-core/src/task/compiler/task-compiler-policy.ts `
        AgentFaceService/task-core/src/task/compiler/task-compiler-service.ts `
        AgentFaceService/task-core/src/index.ts `
        AgentFaceService/task-core/package.json `
        AgentFaceService/mcp/package.json `
        AgentFaceService/task-core/src/tests/task/task-compiler-policy.test.ts `
        AgentFaceService/task-core/src/tests/task/task-spec-runner.regression.test.ts `
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp `
        AgentFaceService/docs/Install_CLI_QuickStart.md `
        AgentFaceService/docs/TaskSpec_CLI_QuickStart.md `
        AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md `
        AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md `
        ClaudePlugin/README.md `
        BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md `
        BlueprintHelper/Develop/Design/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md `
        BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md `
        BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_PublicSchemaDelegateLowering_CodingStyleGaps_20260524_CN.md `
        BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md
git add -u AgentFaceService/task-core/src/task/compiler/task-python-orchestrator.ts `
           AgentFaceService/task-core/src/task/compiler/task-plan-parity.ts `
           AgentFaceService/task-core/src/tests/task/task-python-orchestrator.regression.test.ts `
           AgentFaceService/task-core/src/tests/task/task-plan-parity.test.ts `
           AgentFaceService/task-core/python
# If Task 7 Step 4 archives historical plans, also add only those moved files:
git add BlueprintHelper/Develop/ArchivedReference/TaskSpecPythonCompilerRetire_20260524
git add -u BlueprintHelper/Develop/Plan
git commit -m "refactor: retire Python TaskSpec compiler"
```

## Self-Review

- Spec coverage: plan covers strategy model, service/policy, exports, task-core and MCP package scripts, Python tree deletion, tests, C++ contract guard, AgentFaceService/BlueprintHelper/ClaudePlugin live docs, install/update/bootstrap script audit, and smoke regression.
- Placeholder scan: no execution step depends on unspecified behavior; deletion and replacement steps name exact files and expected searches.
- Type consistency: `canonical_ts` is the only new strategy id; `canonical_python`, `python_worker`, `ts_fast_path`, `parityStatus`, and `fallbackReason` are removed from active source expectations.
