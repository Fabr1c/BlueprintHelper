import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import * as fs from 'node:fs';
import * as path from 'node:path';
import { describe, it } from 'node:test';
import { fileURLToPath } from 'node:url';
import { TaskSpecSchema } from '../../task/schema/task-schemas.js';
import {
  PythonTaskOrchestratorError,
  compileTaskSpecWithPython,
  compileGraphWriteAppendWithPython,
} from '../../task/compiler/task-python-orchestrator.js';

const pythonExe = process.env['BPH_TASK_PYTHON'] ?? process.env['PYTHON'] ?? 'python';
const pythonChildProcessAvailable = (() => {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const pythonPath = [
    path.resolve(here, '..', '..', '..', 'python'),
    path.resolve(here, '..', 'python'),
    path.resolve(process.cwd(), 'python'),
  ].find((candidate) => fs.existsSync(candidate)) ?? path.resolve(here, '..', '..', '..', 'python');
  const env = {
    ...process.env,
    PYTHONPATH: process.env['PYTHONPATH']
      ? `${pythonPath}${path.delimiter}${process.env['PYTHONPATH']}`
      : pythonPath,
  };
  const result = spawnSync(pythonExe, ['-m', 'blueprinthelper_task', 'compile-task-spec'], {
    cwd: path.resolve(pythonPath, '..'),
    env,
    input: JSON.stringify({ task_spec: {}, dry_run: true }),
    encoding: 'utf8',
    timeout: 5_000,
  });
  return !result.error;
})();
const describePythonOrchestrator = pythonChildProcessAvailable ? describe : describe.skip;

function makeTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_test',
    task_type: 'edit_blueprint_graph',
    feature_name: 'DoorFeature',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_DoorFeature',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [
        {
          entry_type: 'custom_event',
          name: 'ToggleDoor',
          body: {
            schema: 'BlueprintLogicSpec.v1',
            statements: [
              {
                kind: 'call_function',
                name: 'PrintString',
                args: {
                  InString: {
                    kind: 'literal',
                    value_type: 'string',
                    value: 'hello',
                  },
                },
              },
            ],
          },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    ...overrides,
  };
}

function makeVariableTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_variables',
    task_type: 'edit_blueprint_variables',
    feature_name: 'DoorVariables',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    behavior: {
      variable_strategy: 'member_variables',
      variables: [
        {
          op: 'ensure_member_variable',
          name: 'bDoorOpen',
          pin_type: { category: 'bool' },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: true,
      should_save: false,
    },
    ...overrides,
  };
}

function makeAssetFactoryTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_asset_factory',
    task_type: 'create_asset',
    feature_name: 'InteractInput',
    target: {
      asset_path: '/Game/Input/IA_Interact',
      target_type: 'asset',
    },
    behavior: {
      asset_strategy: 'ensure_asset',
      asset: {
        asset_type: 'input_action',
        value_type: 'bool',
        collision_policy: 'reuse_if_exists',
      },
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: true,
    },
    ...overrides,
  };
}

function makeBlueprintComponentTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_blueprint_component',
    task_type: 'edit_blueprint_components',
    feature_name: 'DoorComponents',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    behavior: {
      component_strategy: 'component_tree',
      changes: [
        {
          kind: 'ensure_component_present',
          name: 'DoorMesh',
          class: 'StaticMeshComponent',
          attach: {
            parent: 'DefaultSceneRoot',
            rule: 'keep_relative',
          },
          on_name_conflict: 'fail_if_exists',
        },
        {
          kind: 'configure_component',
          name: 'DoorMesh',
          properties: [
            {
              property_path: 'Mobility',
              value: 'Movable',
            },
          ],
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    ...overrides,
  };
}

function makeBlueprintClassSettingsTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_blueprint_class_settings',
    task_type: 'edit_blueprint_class_settings',
    feature_name: 'DoorClassSettings',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    behavior: {
      class_settings_strategy: 'class_settings',
      interfaces: {
        ensure_present: ['/Game/Interfaces/BPI_Interact'],
      },
      class_defaults: [
        {
          property_path: 'OpenKickImpulse',
          value: 1200,
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    ...overrides,
  };
}

function makeUMGWidgetTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_umg_widget',
    task_type: 'edit_umg_widget',
    feature_name: 'MainMenuWidget',
    target: {
      asset_path: '/Game/UI/WBP_MainMenu',
      target_type: 'widget_blueprint',
    },
    behavior: {
      widget_strategy: 'widget_blueprint_edit',
      changes: [
        {
          kind: 'create_widget',
          parent_widget_name: 'CanvasRoot',
          widget_class: 'TextBlock',
          widget_name: 'TitleText',
        },
        {
          kind: 'update_widget_property',
          widget_name: 'TitleText',
          property_path: 'Text',
          value: {
            kind: 'literal',
            value_type: 'text',
            value: 'Start Game',
          },
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    ...overrides,
  };
}

function makeDataTableTaskSpec(overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_data_table',
    task_type: 'edit_data_table',
    feature_name: 'WeaponTable',
    target: {
      asset_path: '/Game/Data/DT_Weapons',
      target_type: 'data_table',
    },
    behavior: {
      row_strategy: 'row_edit',
      rows: [
        {
          action: 'add',
          row_name: 'Pistol',
          fields: {
            Damage: '12',
          },
        },
        {
          action: 'delete',
          row_name: 'OldPistol',
        },
      ],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
    ...overrides,
  };
}

describePythonOrchestrator('Python task orchestrator adapter', () => {
  it('compiles GraphWrite Append TaskSpec through Python into a TaskPlan and Bridge payload', async () => {
    const result = await compileGraphWriteAppendWithPython(TaskSpecSchema.parse(makeTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    assert.deepEqual(result.task_plan.execution_policy, {
      dry_run_mode: 'full',
      should_compile: false,
      should_save: false,
      review_baseline_dirty_asset_policy: 'block',
    });
    assert.deepEqual(result.bridge_payload, {
      target: {
        asset_path: '/Game/BP/BP_Door',
        graph: 'EG_DoorFeature',
      },
      feature_name: 'DoorFeature',
      logic_spec: {
        schema: 'BlueprintLogicSpec.v2',
        entry: { kind: 'custom_event', name: 'ToggleDoor', id: 'ToggleDoor_entry' },
        statements: [
          {
            id: 'ToggleDoor_stmt_1',
            kind: 'call',
            target: 'PrintString',
            args: {
              InString: {
                id: 'ToggleDoor_stmt_1_arg_InString',
                kind: 'literal',
                value_type: 'string',
                value: 'hello',
              },
            },
          },
        ],
      },
      dry_run: true,
    });
  });

  it('maps Python semantic errors into TaskSpec compile errors', async () => {
    const spec = TaskSpecSchema.parse(makeTaskSpec({
      behavior: {
        graph_strategy: 'replace_graph',
        entries: [
          {
            entry_type: 'custom_event',
            name: 'ToggleDoor',
            body: { schema: 'BlueprintLogicSpec.v1', statements: [] },
          },
        ],
      },
    }));

    await assert.rejects(
      () => compileGraphWriteAppendWithPython(spec, true),
      (err: unknown) => {
        if (!(err instanceof PythonTaskOrchestratorError)) return false;
        const compileError = err as PythonTaskOrchestratorError;
        return compileError.code === 'unsupported_graph_strategy' &&
          compileError.issues[0]?.path === 'behavior.graph_strategy';
      },
    );
  });

  it('compiles Blueprint Variables TaskSpec through Python into structured IR', async () => {
    const result = await compileTaskSpecWithPython(TaskSpecSchema.parse(makeVariableTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    const step = result.task_plan.steps[0];
    assert.ok(step && 'capability' in step);
    assert.equal(step.capability, 'blueprint_variable');
    assert.deepEqual(result.bridge_payload, {
      asset_path: '/Game/BP/BP_Door',
      variables: [
        {
          name: 'bDoorOpen',
          pin_type: { category: 'bool' },
        },
      ],
      dry_run: true,
    });
  });

  it('compiles AssetFactory TaskSpec through Python into structured IR', async () => {
    const result = await compileTaskSpecWithPython(TaskSpecSchema.parse(makeAssetFactoryTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    assert.deepEqual(result.task_plan.execution_policy, {
      dry_run_mode: 'full',
      should_compile: false,
      should_save: true,
      review_baseline_dirty_asset_policy: 'block',
    });

    const step = result.task_plan.steps[0];
    assert.ok(step && 'capability' in step);
    assert.equal(step.capability, 'asset_factory');
    assert.equal(Object.hasOwn(step as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(step.write, {
      strategy: 'asset_create',
      ops: [
        {
          op: 'create_asset',
          asset_type: 'input_action',
          value_type: 'bool',
          collision: 'reuse_if_exists',
        },
      ],
    });
    assert.deepEqual(result.bridge_payload, {
      task_plan: result.task_plan,
    });
  });

  it('compiles Blueprint Components TaskSpec through Python into structured IR', async () => {
    const result = await compileTaskSpecWithPython(TaskSpecSchema.parse(makeBlueprintComponentTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    const steps = result.task_plan.steps;
    assert.equal(steps.length, 2);
    const componentStepOne = steps[0];
    assert.ok(componentStepOne && 'capability' in componentStepOne);
    assert.deepEqual(componentStepOne.capability, 'blueprint_component');
    assert.deepEqual(componentStepOne.write, {
      strategy: 'component_tree',
      ops: [
        {
          op: 'add_component',
          component_name: 'DoorMesh',
          component_class: 'StaticMeshComponent',
          parent_component: 'DefaultSceneRoot',
          attach_rule: 'keep_relative',
          name_collision_policy: 'fail_if_exists',
        },
      ],
    });
    assert.equal(Object.hasOwn(componentStepOne as Record<string, unknown>, 'operation'), false);
    const componentStepTwo = steps[1];
    assert.ok(componentStepTwo && 'capability' in componentStepTwo);
    assert.deepEqual(componentStepTwo.capability, 'blueprint_component');
    assert.deepEqual(componentStepTwo.write, {
      strategy: 'component_tree',
      ops: [
        {
          op: 'set_component_properties',
          component_name: 'DoorMesh',
          settings: [
            {
              property_path: 'Mobility',
              value: 'Movable',
            },
          ],
        },
      ],
    });
    assert.equal(Object.hasOwn(componentStepTwo as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(result.bridge_payload, {
      task_plan: result.task_plan,
    });
  });

  it('compiles Blueprint Class Settings TaskSpec through Python into structured IR', async () => {
    const result = await compileTaskSpecWithPython(TaskSpecSchema.parse(makeBlueprintClassSettingsTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    const steps = result.task_plan.steps;
    assert.equal(steps.length, 2);
    const classSettingsStepOne = steps[0];
    assert.ok(classSettingsStepOne && 'capability' in classSettingsStepOne);
    assert.deepEqual(classSettingsStepOne.capability, 'blueprint_class_settings');
    assert.deepEqual(classSettingsStepOne.write, {
      strategy: 'class_settings',
      ops: [
        {
          op: 'add_implemented_interfaces',
          interface_paths: ['/Game/Interfaces/BPI_Interact'],
        },
      ],
    });
    assert.equal(Object.hasOwn(classSettingsStepOne as Record<string, unknown>, 'operation'), false);
    const classSettingsStepTwo = steps[1];
    assert.ok(classSettingsStepTwo && 'capability' in classSettingsStepTwo);
    assert.deepEqual(classSettingsStepTwo.capability, 'blueprint_class_settings');
    assert.deepEqual(classSettingsStepTwo.write, {
      strategy: 'class_settings',
      ops: [
        {
          op: 'set_class_default_properties',
          settings: [
            {
              property_path: 'OpenKickImpulse',
              value: 1200,
            },
          ],
        },
      ],
    });
    assert.equal(Object.hasOwn(classSettingsStepTwo as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(result.bridge_payload, {
      task_plan: result.task_plan,
    });
  });

  it('compiles UMG Widget TaskSpec through Python into structured IR', async () => {
    const result = await compileTaskSpecWithPython(TaskSpecSchema.parse(makeUMGWidgetTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    const steps = result.task_plan.steps;
    assert.equal(steps.length, 2);
    const umgStepOne = steps[0];
    assert.ok(umgStepOne && 'capability' in umgStepOne);
    assert.deepEqual(umgStepOne.capability, 'umg_widget');
    assert.deepEqual(umgStepOne.write, {
      strategy: 'widget_tree_edit',
      ops: [
        {
          op: 'add_widget',
          widget_class: 'TextBlock',
          widget_name: 'TitleText',
          parent_widget_name: 'CanvasRoot',
        },
      ],
    });
    assert.equal(Object.hasOwn(umgStepOne as Record<string, unknown>, 'operation'), false);
    const umgStepTwo = steps[1];
    assert.ok(umgStepTwo && 'capability' in umgStepTwo);
    assert.deepEqual(umgStepTwo.capability, 'umg_widget');
    assert.deepEqual(umgStepTwo.write, {
      strategy: 'widget_property_edit',
      ops: [
        {
          op: 'set_widget_property',
          widget_name: 'TitleText',
          property_path: 'Text',
          value: 'Start Game',
        },
      ],
    });
    assert.equal(Object.hasOwn(umgStepTwo as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(result.bridge_payload, {
      task_plan: result.task_plan,
    });
  });

  it('compiles DataTable TaskSpec through Python into structured IR', async () => {
    const result = await compileTaskSpecWithPython(TaskSpecSchema.parse(makeDataTableTaskSpec()), true);

    assert.equal(result.schema, 'BlueprintHelper.TaskCompilerResult.v1');
    assert.equal(result.task_plan.schema, 'BlueprintHelper.TaskPlan.v1');
    const steps = result.task_plan.steps;
    assert.equal(steps.length, 2);
    const dataTableStepOne = steps[0];
    assert.ok(dataTableStepOne && 'capability' in dataTableStepOne);
    assert.deepEqual(dataTableStepOne.capability, 'data_table');
    assert.deepEqual(dataTableStepOne.write, {
      strategy: 'row_edit',
      ops: [
        {
          op: 'add_row',
          row_name: 'Pistol',
          fields: {
            Damage: '12',
          },
        },
      ],
    });
    assert.equal(Object.hasOwn(dataTableStepOne as Record<string, unknown>, 'operation'), false);
    const dataTableStepTwo = steps[1];
    assert.ok(dataTableStepTwo && 'capability' in dataTableStepTwo);
    assert.deepEqual(dataTableStepTwo.capability, 'data_table');
    assert.deepEqual(dataTableStepTwo.write, {
      strategy: 'row_edit',
      ops: [
        {
          op: 'delete_row',
          row_name: 'OldPistol',
        },
      ],
    });
    assert.equal(Object.hasOwn(dataTableStepTwo as Record<string, unknown>, 'operation'), false);
    assert.deepEqual(result.bridge_payload, {
      task_plan: result.task_plan,
    });
  });
});
