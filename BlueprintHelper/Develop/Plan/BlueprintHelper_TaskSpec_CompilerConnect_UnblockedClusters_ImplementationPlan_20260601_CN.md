# BlueprintHelper TaskSpec Compiler Connect Unblocked Clusters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 接通 canonical TypeScript compiler 顶层 `edit_blueprint_components`、`edit_umg_widget`、`edit_data_table`，让已有 schema / TaskPlan / C++ runtime 簇从 preview/execute 入口可达。

**Architecture:** 只在 TaskSpec compiler 层补齐语义到 TaskPlan IR 的 lowering，不新增 runtime 簇、不绕过 TaskRuntime、不恢复 direct Bridge 写入口。三个能力分别降低为现有 `blueprint_component`、`umg_widget`、`data_table` capability step；每个 step 继续保持 C++ adapter 要求的单 op 形状。

**Tech Stack:** TypeScript `node:test` + `assert`、Zod TaskSpec/TaskPlan schema、BlueprintHelper C++ TaskRuntime adapters、BlueprintHelper CLI `bh task preview/execute`。

---

## Source Inputs

- `BlueprintHelper/Develop/Gap/BlueprintHelper_TaskSpec_UnconnectedToolClusters_SourceAudit_20260601_CN.md`
- `Debug/BlueprintHelper_TaskSpec_UnconnectedToolClusters_20260601.md`
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
- `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/TaskPlanAdapters/DataTable/BlueprintHelperDataTableTaskPlanAdapter.cpp`

## File Structure

- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Add top-level compiler branches.
  - Add lowering functions for component, UMG widget, and DataTable.
  - Add shared helpers for settings arrays, per-step capability creation, and explicit semantic errors.
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
  - Add the three task types to `CANONICAL_TS_TASK_TYPES`.
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`
  - Positive compiler tests for all three existing fixtures.
  - Registry exposure tests.
  - Targeted negative tests for missing required semantic fields.
- Modify: `AgentFaceService/agent-guide/Templates/write/taskspec_edit_umg_widget_template.json`
  - Change `target.target_type` from `blueprint` to `widget_blueprint`.
- Modify: `AgentFaceService/agent-guide/Templates/write/taskspec_edit_data_table_rows_template.json`
  - Change `target.target_type` from `asset` to `data_table`.
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_TaskSpec_UnconnectedToolClusters_SourceAudit_20260601_CN.md`
  - After implementation, move the three rows from “已接线但未放行” to “compiler 已放行” and record verification commands.
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_Report_20260601_CN.md`
  - Required because this plan will modify source code when executed.
- Update: `Debug/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_20260601.md`
  - Append implementation and verification evidence.

## Task 1: Failing Compiler Tests

**Files:**
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`
- Read: `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`
- Read: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Read: `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`

- [ ] **Step 1: Add tests that define the expected connected behavior**

Create `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';
import { isCanonicalTsTaskType } from './task-compiler-registry.js';
import {
  componentTaskPlanFixture,
  componentTaskSpecFixture,
  dataTableTaskPlanFixture,
  dataTableTaskSpecFixture,
  widgetTaskPlanFixture,
  widgetTaskSpecFixture,
} from '../fixtures/task-protocol.fixtures.js';

function clone<T>(value: T): T {
  return structuredClone(value);
}

test('canonical registry advertises component, UMG widget, and DataTable task types', () => {
  assert.equal(isCanonicalTsTaskType('edit_blueprint_components'), true);
  assert.equal(isCanonicalTsTaskType('edit_umg_widget'), true);
  assert.equal(isCanonicalTsTaskType('edit_data_table'), true);
});

test('edit_blueprint_components lowers to existing blueprint_component TaskPlan shape', () => {
  assert.deepEqual(
    compileTaskSpecToTaskPlan(componentTaskSpecFixture as never),
    componentTaskPlanFixture,
  );
});

test('edit_umg_widget lowers create/update/delete changes to existing umg_widget TaskPlan shape', () => {
  assert.deepEqual(
    compileTaskSpecToTaskPlan(widgetTaskSpecFixture as never),
    widgetTaskPlanFixture,
  );
});

test('edit_data_table lowers add/update/delete rows to existing data_table TaskPlan shape', () => {
  assert.deepEqual(
    compileTaskSpecToTaskPlan(dataTableTaskSpecFixture as never),
    dataTableTaskPlanFixture,
  );
});

test('component configure change requires a non-empty properties array or object', () => {
  const taskSpec = clone(componentTaskSpecFixture) as Record<string, unknown>;
  const behavior = taskSpec.behavior as Record<string, unknown>;
  behavior.changes = [{
    kind: 'configure_component',
    name: 'DoorRoot',
  }];

  assert.throws(
    () => compileTaskSpecToTaskPlan(taskSpec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'missing_component_properties');
      return true;
    },
  );
});

test('UMG update_widget_property requires value before runtime lowering', () => {
  const taskSpec = clone(widgetTaskSpecFixture) as Record<string, unknown>;
  const behavior = taskSpec.behavior as Record<string, unknown>;
  behavior.changes = [{
    kind: 'update_widget_property',
    widget_name: 'TitleText',
    property_path: 'Text',
  }];

  assert.throws(
    () => compileTaskSpecToTaskPlan(taskSpec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'missing_umg_widget_property_value');
      return true;
    },
  );
});

test('DataTable update rows require fields before runtime lowering', () => {
  const taskSpec = clone(dataTableTaskSpecFixture) as Record<string, unknown>;
  const behavior = taskSpec.behavior as Record<string, unknown>;
  behavior.rows = [{
    action: 'update',
    row_name: 'Shotgun',
  }];

  assert.throws(
    () => compileTaskSpecToTaskPlan(taskSpec as never),
    (error) => {
      assert.ok(error instanceof TaskSpecCompileError);
      assert.equal(error.code, 'taskspec_semantic_invalid');
      assert.equal(error.issues[0]?.code, 'missing_data_table_row_fields');
      return true;
    },
  );
});
```

- [ ] **Step 2: Run tests and verify they fail for the current reason**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node -- task-compiler.unblocked-clusters
```

Expected now:

- Registry test fails because `isCanonicalTsTaskType(...)` returns `false`.
- Positive compiler tests fail with `unsupported_task_type`.
- Negative tests may also fail with `unsupported_task_type`; that is acceptable before implementation.

## Task 2: Compiler Registry Gate

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
- Test: `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`

- [ ] **Step 1: Add the three task types to the canonical allow set**

Update `CANONICAL_TS_TASK_TYPES`:

```ts
const CANONICAL_TS_TASK_TYPES = new Set([
  'create_asset',
  'create_blueprint_feature',
  'edit_blueprint_graph',
  'edit_blueprint_variables',
  'edit_object_properties',
  'edit_blueprint_signature',
  'edit_blueprint_class_settings',
  'edit_blueprint_components',
  'edit_umg_widget',
  'edit_data_table',
]);
```

- [ ] **Step 2: Run registry-focused test**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node -- task-compiler.unblocked-clusters
```

Expected after this task:

- Registry test passes.
- Positive compiler tests still fail until Task 3 lowers the three task types.

## Task 3: Component TaskSpec Lowering

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Test: `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`

- [ ] **Step 1: Add the top-level component branch**

In `compileTaskSpecToTaskPlan`, add this before the `edit_blueprint_graph` guard:

```ts
  if (taskSpec.task_type === 'edit_blueprint_components') {
    return compileBlueprintComponentsTaskSpecToTaskPlan(taskSpec);
  }
```

- [ ] **Step 2: Add component lowering functions**

Add these functions near `compileBlueprintClassSettingsTaskSpecToTaskPlan`, keeping the same local style as existing compiler helpers:

```ts
function compileBlueprintComponentsTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_components' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'component_strategy',
    'component_tree',
    'behavior.component_strategy',
    'Use component_strategy="component_tree".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const steps: TaskPlanStep[] = [];

  changes.forEach((rawChange, changeIndex) => {
    const path = `behavior.changes[${changeIndex}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_component_change', path, message: 'Provide a component change object.' },
      ]);
    }

    const change = rawChange as Record<string, unknown>;
    const kind = getRequiredString(change, 'kind', `${path}.kind`);
    if (kind === 'ensure_component_present') {
      const addOp = omitUndefined({
        op: 'add_component',
        component_name: getRequiredString(change, 'name', `${path}.name`),
        component_class: getRequiredString(change, 'class', `${path}.class`),
        parent_component: componentParent(change),
        socket_name: componentSocket(change),
        attach_rule: componentAttachRule(change),
        name_collision_policy: normalizeComponentCollisionPolicy(change['on_name_conflict']),
      });
      const addStep = makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [addOp],
      );
      steps.push(addStep);

      const settings = propertySettingsArray(change['properties'], `${path}.properties`, false);
      if (settings.length > 0) {
        steps.push({
          ...makeCompositeCapabilityStep(
            steps.length + 1,
            'blueprint_component',
            taskSpec.target.asset_path,
            'component_tree',
            [{
              op: 'set_component_properties',
              component_name: addOp.component_name,
              settings,
            }],
          ),
          depends_on: [addStep.step_id],
        } as TaskPlanStep);
      }
      return;
    }

    if (kind === 'configure_component') {
      const settings = propertySettingsArray(change['properties'], `${path}.properties`, true);
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [{
          op: 'set_component_properties',
          component_name: getRequiredString(change, 'name', `${path}.name`),
          settings,
        }],
      ));
      return;
    }

    if (kind === 'remove_component') {
      steps.push(makeCompositeCapabilityStep(
        steps.length + 1,
        'blueprint_component',
        taskSpec.target.asset_path,
        'component_tree',
        [{
          op: 'remove_component',
          component_name: getRequiredString(change, 'name', `${path}.name`),
        }],
      ));
      return;
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported component change kind: ${kind}`, [
      {
        code: 'unsupported_component_change_kind',
        path: `${path}.kind`,
        message: 'Use ensure_component_present, configure_component, or remove_component.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function componentParent(component: Record<string, unknown>): unknown {
  return compositeComponentParent(component);
}

function componentSocket(component: Record<string, unknown>): unknown {
  return compositeComponentSocket(component);
}

function componentAttachRule(component: Record<string, unknown>): unknown {
  return compositeComponentAttachRule(component);
}
```

- [ ] **Step 3: Add generic property settings helper**

Add this helper near `compositeSettingsArray`:

```ts
function propertySettingsArray(rawSettings: unknown, path: string, requireNonEmpty: boolean): Record<string, unknown>[] {
  if (rawSettings === undefined || rawSettings === null) {
    if (requireNonEmpty) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
        {
          code: 'missing_component_properties',
          path,
          message: 'Provide at least one property setting.',
        },
      ]);
    }
    return [];
  }

  const settings = Array.isArray(rawSettings)
    ? rawSettings.map((rawSetting, index) => {
        if (!isRecord(rawSetting)) {
          throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting must be an object.', [
            {
              code: 'invalid_property_setting',
              path: `${path}[${index}]`,
              message: 'Use { "property_path": "...", "value": ... }.',
            },
          ]);
        }
        const setting = rawSetting as Record<string, unknown>;
        if (!Object.hasOwn(setting, 'value')) {
          throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Property setting requires value.', [
            {
              code: 'missing_property_value',
              path: `${path}[${index}].value`,
              message: 'Provide value.',
            },
          ]);
        }
        return {
          property_path: getRequiredString(setting, 'property_path', `${path}[${index}].property_path`),
          value: literalValue(setting['value']),
        };
      })
    : isRecord(rawSettings)
      ? Object.entries(rawSettings).map(([propertyPath, value]) => ({
          property_path: propertyPath,
          value: literalValue(value),
        }))
      : undefined;

  if (!settings) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object or array.`, [
      {
        code: 'invalid_property_settings',
        path,
        message: 'Use an object map or an array of { property_path, value } settings.',
      },
    ]);
  }

  if (requireNonEmpty && settings.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must not be empty.`, [
      {
        code: 'missing_component_properties',
        path,
        message: 'Provide at least one property setting.',
      },
    ]);
  }

  return settings;
}
```

- [ ] **Step 4: Run component tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node -- task-compiler.unblocked-clusters
```

Expected after this task:

- Component positive and negative tests pass.
- UMG/DataTable positive tests still fail until their lowering functions are added.

## Task 4: UMG Widget TaskSpec Lowering

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Test: `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`

- [ ] **Step 1: Add the top-level UMG branch**

In `compileTaskSpecToTaskPlan`, add this before the `edit_blueprint_graph` guard:

```ts
  if (taskSpec.task_type === 'edit_umg_widget') {
    return compileUMGWidgetTaskSpecToTaskPlan(taskSpec);
  }
```

- [ ] **Step 2: Add UMG lowering functions**

Add:

```ts
function compileUMGWidgetTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_umg_widget' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'widget_strategy',
    'widget_blueprint_edit',
    'behavior.widget_strategy',
    'Use widget_strategy="widget_blueprint_edit".',
  );

  const changes = requiredArray(behavior, 'changes', 'behavior.changes');
  const steps = changes.map((rawChange, index) => {
    const path = `behavior.changes[${index}]`;
    if (!isRecord(rawChange)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_umg_widget_change', path, message: 'Provide a UMG widget change object.' },
      ]);
    }

    const change = rawChange as Record<string, unknown>;
    const kind = getRequiredString(change, 'kind', `${path}.kind`);
    if (kind === 'create_widget') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [omitUndefined({
          op: 'add_widget',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          widget_class: getRequiredString(change, 'widget_class', `${path}.widget_class`),
          parent_widget_name: optionalString(change, 'parent_widget_name'),
          parent_name: optionalString(change, 'parent_name'),
        })],
      );
    }

    if (kind === 'update_widget_property') {
      if (!Object.hasOwn(change, 'value')) {
        throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.value is required.`, [
          {
            code: 'missing_umg_widget_property_value',
            path: `${path}.value`,
            message: 'Provide value for update_widget_property.',
          },
        ]);
      }
      const propertyPath = optionalString(change, 'property_path');
      const propertyName = optionalString(change, 'property_name');
      if (!propertyPath && !propertyName) {
        throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.property_path is required.`, [
          {
            code: 'missing_umg_widget_property_path',
            path: `${path}.property_path`,
            message: 'Provide property_path or property_name.',
          },
        ]);
      }
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_property_edit',
        [omitUndefined({
          op: 'set_widget_property',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
          property_path: propertyPath,
          property_name: propertyName,
          value: literalValue(change['value']),
        })],
      );
    }

    if (kind === 'delete_widget') {
      return makeCompositeCapabilityStep(
        index + 1,
        'umg_widget',
        taskSpec.target.asset_path,
        'widget_tree_edit',
        [{
          op: 'remove_widget',
          widget_name: getRequiredString(change, 'widget_name', `${path}.widget_name`),
        }],
      );
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported UMG widget change kind: ${kind}`, [
      {
        code: 'unsupported_umg_widget_change_kind',
        path: `${path}.kind`,
        message: 'Use create_widget, update_widget_property, or delete_widget.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}
```

- [ ] **Step 3: Run UMG tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node -- task-compiler.unblocked-clusters
```

Expected after this task:

- Component and UMG tests pass.
- DataTable positive test still fails until Task 5.

## Task 5: DataTable TaskSpec Lowering

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Test: `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`

- [ ] **Step 1: Add the top-level DataTable branch**

In `compileTaskSpecToTaskPlan`, add this before the `edit_blueprint_graph` guard:

```ts
  if (taskSpec.task_type === 'edit_data_table') {
    return compileDataTableTaskSpecToTaskPlan(taskSpec);
  }
```

- [ ] **Step 2: Add DataTable lowering functions**

Add:

```ts
function compileDataTableTaskSpecToTaskPlan(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_data_table' }>,
): TaskPlan {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  assertExactString(
    behavior,
    'row_strategy',
    'row_edit',
    'behavior.row_strategy',
    'Use row_strategy="row_edit".',
  );

  const rows = requiredArray(behavior, 'rows', 'behavior.rows');
  const steps = rows.map((rawRow, index) => {
    const path = `behavior.rows[${index}]`;
    if (!isRecord(rawRow)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
        { code: 'invalid_data_table_row', path, message: 'Provide a DataTable row object.' },
      ]);
    }

    const row = rawRow as Record<string, unknown>;
    const action = getRequiredString(row, 'action', `${path}.action`);
    const rowName = getRequiredString(row, 'row_name', `${path}.row_name`);

    if (action === 'add') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [omitUndefined({
          op: 'add_row',
          row_name: rowName,
          fields: optionalFieldsObject(row['fields'], `${path}.fields`, false),
        })],
      );
    }

    if (action === 'update') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [{
          op: 'update_row',
          row_name: rowName,
          fields: optionalFieldsObject(row['fields'], `${path}.fields`, true),
        }],
      );
    }

    if (action === 'delete') {
      return makeCompositeCapabilityStep(
        index + 1,
        'data_table',
        taskSpec.target.asset_path,
        'row_edit',
        [{
          op: 'delete_row',
          row_name: rowName,
        }],
      );
    }

    throw new TaskSpecCompileError('taskspec_semantic_invalid', `Unsupported DataTable row action: ${action}`, [
      {
        code: 'unsupported_data_table_row_action',
        path: `${path}.action`,
        message: 'Use add, update, or delete.',
      },
    ]);
  });

  return makeTaskPlanWithSteps(taskSpec, steps);
}

function optionalFieldsObject(value: unknown, path: string, requireNonEmpty: boolean): Record<string, unknown> | undefined {
  if (value === undefined || value === null) {
    if (requireNonEmpty) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is required.`, [
        {
          code: 'missing_data_table_row_fields',
          path,
          message: 'DataTable update rows require a non-empty fields object.',
        },
      ]);
    }
    return undefined;
  }

  if (!isRecord(value) || Object.keys(value).length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty object.`, [
      {
        code: requireNonEmpty ? 'missing_data_table_row_fields' : 'invalid_data_table_row_fields',
        path,
        message: 'Use a non-empty object keyed by row field name.',
      },
    ]);
  }

  return value;
}
```

- [ ] **Step 3: Run DataTable tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node -- task-compiler.unblocked-clusters
```

Expected after this task:

- All tests in `task-compiler.unblocked-clusters.test.ts` pass.

## Task 6: Template And Agent-Facing Contract Sync

**Files:**
- Modify: `AgentFaceService/agent-guide/Templates/write/taskspec_edit_umg_widget_template.json`
- Modify: `AgentFaceService/agent-guide/Templates/write/taskspec_edit_data_table_rows_template.json`
- Review: `AgentFaceService/agent-guide/Templates/write/SEMANTIC_INDEX.md`
- Review: `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md`
- Review: `ClaudePlugin/skills/blueprint-helper/references/06_UMG_Data_Workflows.md`
- Review: `CodexPlugin/skills/blueprint-helper/references/06_UMG_Data_Workflows.md`

- [ ] **Step 1: Align UMG template target type**

Change:

```json
"target_type": "blueprint"
```

to:

```json
"target_type": "widget_blueprint"
```

- [ ] **Step 2: Align DataTable template target type**

Change:

```json
"target_type": "asset"
```

to:

```json
"target_type": "data_table"
```

- [ ] **Step 3: Confirm the semantic index does not describe these templates as blocked**

Run:

```powershell
rg -n "edit_blueprint_components|edit_umg_widget|edit_data_table|unsupported_task_type|未放行|not supported" AgentFaceService\agent-guide\Templates\write AgentFaceService\agent-guide\Workflows ClaudePlugin\skills\blueprint-helper\references CodexPlugin\skills\blueprint-helper\references
```

Expected after implementation:

- Templates may still contain the three `task_type` values.
- No agent-facing workflow/reference should say these three are compiler-unsupported.
- If a stale warning is present, remove it in the same task and note the exact file in the report.

## Task 7: Full TypeScript Verification

**Files:**
- No source edits unless tests reveal a real compile issue.

- [ ] **Step 1: Run task-core build and tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
```

Expected:

- TypeScript build succeeds.
- Node test runner exits with code 0.

- [ ] **Step 2: Run CLI build**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli
npm.cmd run build
```

Expected:

- CLI build succeeds because it consumes the updated task-core compiler exports.

## Task 8: Preview Smoke Tests Without Writing Assets

**Files:**
- Create temporary files under `.tmp/taskspec-compiler-connect/`.
- Do not commit `.tmp` files.

- [ ] **Step 1: Create concrete preview specs from templates**

Create three temporary JSON files:

`.tmp/taskspec-compiler-connect/component.json`

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "edit_blueprint_components",
  "feature_name": "CompilerConnect_ComponentSmoke",
  "target": {
    "asset_path": "/Game/BlueprintHelperTemp/BP_TaskSpecCompilerConnect",
    "target_type": "blueprint"
  },
  "behavior": {
    "component_strategy": "component_tree",
    "changes": [
      {
        "kind": "ensure_component_present",
        "name": "CompilerConnectRoot",
        "class": "SceneComponent",
        "on_name_conflict": "reuse_if_exists"
      },
      {
        "kind": "configure_component",
        "name": "CompilerConnectRoot",
        "properties": [
          {
            "property_path": "Mobility",
            "value": "Movable"
          }
        ]
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "block"
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

`.tmp/taskspec-compiler-connect/umg.json`

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "edit_umg_widget",
  "feature_name": "CompilerConnect_UMGSmoke",
  "target": {
    "asset_path": "/Game/BlueprintHelperTemp/WBP_TaskSpecCompilerConnect",
    "target_type": "widget_blueprint"
  },
  "behavior": {
    "widget_strategy": "widget_blueprint_edit",
    "changes": [
      {
        "kind": "create_widget",
        "widget_name": "CompilerConnectRoot",
        "widget_class": "CanvasPanel"
      },
      {
        "kind": "create_widget",
        "widget_name": "CompilerConnectText",
        "widget_class": "TextBlock",
        "parent_widget_name": "CompilerConnectRoot"
      },
      {
        "kind": "update_widget_property",
        "widget_name": "CompilerConnectText",
        "property_path": "Text",
        "value": "TaskSpec compiler connected"
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "block"
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

`.tmp/taskspec-compiler-connect/datatable.json`

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "edit_data_table",
  "feature_name": "CompilerConnect_DataTableSmoke",
  "target": {
    "asset_path": "/Game/BlueprintHelperTemp/DT_TaskSpecCompilerConnect",
    "target_type": "data_table"
  },
  "behavior": {
    "row_strategy": "row_edit",
    "rows": [
      {
        "action": "add",
        "row_name": "CompilerConnectRow",
        "fields": {
          "Label": "Connected",
          "Value": "1"
        }
      },
      {
        "action": "update",
        "row_name": "CompilerConnectRow",
        "fields": {
          "Value": "2"
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "block"
  },
  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

- [ ] **Step 2: Run grouped CLI preview for compile-path validation**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\.tmp\taskspec-compiler-connect\component.json --format summary
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\.tmp\taskspec-compiler-connect\umg.json --format summary
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\.tmp\taskspec-compiler-connect\datatable.json --format summary
```

Expected:

- None of the three commands returns `unsupported_task_type`.
- If the Editor/Bridge is unavailable, the error must be a runtime/bridge availability error after TaskSpec compile, not a compiler rejection.

## Task 9: C++ Runtime Verification

**Files:**
- No source edits unless C++ tests reveal a mismatch between compiler output and existing adapters.

- [ ] **Step 1: Run targeted Unreal automation tests for adapter layers**

Use the project’s established UBT/automation command for BlueprintHelper tests. The targeted test names to run are:

```text
BlueprintHelper.TaskRuntime.ClusterHub
BlueprintHelper.BlueprintComponent.TaskPlanAdapter
BlueprintHelper.UMGWidget.TaskPlanWidgetAdapter
BlueprintHelper.DataTable.TaskPlanAdapter
```

Expected:

- Existing adapter tests pass.
- If a failure appears in adapter tests after TS compiler changes, treat it as a TaskPlan shape mismatch and fix TS lowering, not C++ runtime behavior.

- [ ] **Step 2: Run a real E2E preview/execute only after editor write session is available**

With the editor running and write permission granted through the normal BlueprintHelper write-session flow:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\.tmp\taskspec-compiler-connect\component.json --format summary
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\.tmp\taskspec-compiler-connect\umg.json --format summary
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\.tmp\taskspec-compiler-connect\datatable.json --format summary
```

Expected:

- Preview reaches TaskRuntime planning for all three task types.
- Any failure is asset existence, row struct, widget class, property import, or write permission related; it is not `unsupported_task_type`.

## Task 10: Documentation And Report

**Files:**
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_TaskSpec_UnconnectedToolClusters_SourceAudit_20260601_CN.md`
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_Report_20260601_CN.md`
- Modify: `Debug/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_20260601.md`

- [ ] **Step 1: Update Gap document classification**

In `BlueprintHelper_TaskSpec_UnconnectedToolClusters_SourceAudit_20260601_CN.md`, change the final classification:

```markdown
### 已由 canonical TS compiler 顶层放行

- `create_asset` / `AssetFactory`
- `create_blueprint_feature` / composite
- `edit_blueprint_graph` / `GraphWrite`
- `edit_blueprint_variables` / `BlueprintVariables`
- `edit_object_properties` / `ObjectProperty`
- `edit_blueprint_signature` / `Signature`
- `edit_blueprint_class_settings` / `ClassSettings`
- `edit_blueprint_components` / `Component`
- `edit_umg_widget` / `UMGWidget`
- `edit_data_table` / `DataTable`
```

Add a verification subsection with actual command output summaries from Tasks 7-9.

- [ ] **Step 2: Create implementation report**

Create `BlueprintHelper/Develop/Report/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_Report_20260601_CN.md`:

```markdown
# TaskSpec Compiler 接通未放行工具簇报告

日期：2026-06-01

## 改动原因

`edit_blueprint_components`、`edit_umg_widget`、`edit_data_table` 已有 schema、TaskPlan step、C++ TaskRuntime 簇，但 canonical TS compiler 顶层未放行，导致 preview/execute 返回 `unsupported_task_type`。

## 改动范围

- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler-registry.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler.unblocked-clusters.test.ts`
- `AgentFaceService/agent-guide/Templates/write/taskspec_edit_umg_widget_template.json`
- `AgentFaceService/agent-guide/Templates/write/taskspec_edit_data_table_rows_template.json`
- `BlueprintHelper/Develop/Gap/BlueprintHelper_TaskSpec_UnconnectedToolClusters_SourceAudit_20260601_CN.md`
- `Debug/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_20260601.md`

## 改动结果

1. canonical TS compiler 顶层放行 10 个 TaskSpec 类型。
2. Component 独立 TaskSpec 降低到 `blueprint_component/component_tree`。
3. UMG Widget 独立 TaskSpec 降低到 `umg_widget/widget_tree_edit` 与 `umg_widget/widget_property_edit`。
4. DataTable 独立 TaskSpec 降低到 `data_table/row_edit`。

## 验证结果

| 验证项 | 命令 | 结果 | 证据摘要 |
| --- | --- | --- | --- |
| task-core build | `cd AgentFaceService/task-core; npm.cmd run build` | `PASS` / `FAIL` / `BLOCKED` | 记录最后 5 行输出或首个错误。 |
| task-core node tests | `cd AgentFaceService/task-core; npm.cmd run test:node` | `PASS` / `FAIL` / `BLOCKED` | 记录失败测试名或通过数量。 |
| cli build | `cd AgentFaceService/cli; npm.cmd run build` | `PASS` / `FAIL` / `BLOCKED` | 记录最后 5 行输出或首个错误。 |
| CLI preview smoke | `node .\AgentFaceService\cli\build\cli\index.js task preview --file <spec> --format summary` | `PASS` / `FAIL` / `BLOCKED` | 逐个记录 component/umg/datatable 是否仍有 `unsupported_task_type`。 |
| UE automation/E2E | 项目既有 UE automation / write-session preview 命令 | `PASS` / `FAIL` / `BLOCKED` | 记录测试名、资产路径、失败阶段；如 Editor/Bridge 不可用，标记 `BLOCKED: editor_or_bridge_unavailable`。 |
```

- [ ] **Step 3: Update Debug evidence**

Append to `Debug/BlueprintHelper_TaskSpec_CompilerConnect_UnblockedClusters_20260601.md`:

```markdown
## Implementation Evidence

- Added compiler branches for `edit_blueprint_components`, `edit_umg_widget`, `edit_data_table`.
- Added registry allow-list entries.
- Added `task-compiler.unblocked-clusters.test.ts`.
- Updated UMG/DataTable templates to match contract target types.

## Verification Evidence

- task-core build:
- task-core node tests:
- cli build:
- CLI preview smoke:
- UE automation/E2E:
```

Fill every bullet with the real command result before closure.

## Self-Review Checklist

- [ ] The three task types are in `CANONICAL_TS_TASK_TYPES`.
- [ ] `compileTaskSpecToTaskPlan` has top-level branches for all three.
- [ ] Each generated TaskPlan step uses `capability/write`, not `operation`.
- [ ] Each generated TaskPlan step contains exactly one `write.ops[]` entry for Component, UMG, and DataTable adapter compatibility.
- [ ] UMG `create_widget` and `delete_widget` use `widget_tree_edit`; `update_widget_property` uses `widget_property_edit`.
- [ ] DataTable `update` requires a non-empty `fields` object before runtime.
- [ ] Component `configure_component` requires non-empty properties before runtime.
- [ ] Templates use contract-aligned target types.
- [ ] Gap, Debug, and Report docs reflect actual verification status.
- [ ] No direct Bridge write entry is introduced.

## Execution Status 2026-06-01

- [x] The three task types are in `CANONICAL_TS_TASK_TYPES`.
- [x] `compileTaskSpecToTaskPlan` has top-level branches for all three.
- [x] Each generated TaskPlan step uses `capability/write`, not `operation`.
- [x] Each generated TaskPlan step contains exactly one `write.ops[]` entry for Component, UMG, and DataTable adapter compatibility.
- [x] UMG `create_widget` and `delete_widget` use `widget_tree_edit`; `update_widget_property` uses `widget_property_edit`.
- [x] DataTable `update` requires a non-empty `fields` object before runtime.
- [x] Component `configure_component` requires non-empty properties before runtime.
- [x] Templates use contract-aligned target types.
- [x] Gap, Debug, and Report docs reflect actual verification status.
- [x] No direct Bridge write entry is introduced.

Verification summary:

- `AgentFaceService/task-core`: `npm.cmd run build` PASS.
- `AgentFaceService/task-core`: `npm.cmd run test:node` PASS, `tests 316`, `pass 316`, `fail 0`.
- `AgentFaceService/cli`: `npm.cmd run build` PASS.
- CLI preview smoke: Component and UMGWidget real-asset dry-runs PASS; DataTable compiler path PASS and runtime blocks only because the current project has no DataTable asset.
- UE automation: `BlueprintHelper.TaskRuntime.Cluster` PASS, 6 succeeded / 0 failed.
- UE automation: `BlueprintHelper.TaskPlan.ComponentAdapter` PASS, 4 succeeded / 0 failed.
- UE automation: `BlueprintHelper.TaskPlan.WidgetAdapter` PASS, 4 succeeded / 0 failed.
- UE automation: `BlueprintHelper.TaskPlan.DataTableAdapter` PASS, 9 succeeded / 0 failed.
