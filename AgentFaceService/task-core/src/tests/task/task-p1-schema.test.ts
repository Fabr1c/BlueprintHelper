import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import {
  TaskPlanSchema,
  TaskSpecSchema,
} from '../../task/schema/task-schemas.js';

function baseSpec(taskType: string, behavior: Record<string, unknown>, overrides: Record<string, unknown> = {}) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: `ctx_${taskType}`,
    task_type: taskType,
    feature_name: 'P1Feature',
    target: {
      asset_path: '/Game/Blueprints/BP_Door',
      target_type: 'blueprint',
    },
    behavior,
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

describe('P1 TaskSpec schema validation', () => {
  it('accepts TaskSpec shapes for TaskPlan-ready capability clusters', () => {
    const specs = [
      baseSpec('create_asset', {
        asset_strategy: 'ensure_asset',
        asset: {
          asset_type: 'input_action',
          value_type: 'bool',
          collision_policy: 'reuse_if_exists',
        },
      }, {
        target: {
          asset_path: '/Game/Input/IA_Interact',
          target_type: 'asset',
        },
      }),
      baseSpec('create_asset', {
        asset_strategy: 'ensure_asset',
        asset: {
          asset_type: 'structure',
          fields: [
            { name: 'Damage', type: 'float' },
            { name: 'Ammo', type: 'int' },
          ],
          collision_policy: 'reuse_if_exists',
        },
      }, {
        target: {
          asset_path: '/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow',
          target_type: 'asset',
        },
      }),
      baseSpec('create_asset', {
        asset_strategy: 'ensure_asset',
        asset: {
          asset_type: 'data_table',
          row_struct: '/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow',
          collision_policy: 'reuse_if_exists',
        },
      }, {
        target: {
          asset_path: '/Game/BlueprintHelper/Smoke/DT_DataTableSmoke',
          target_type: 'asset',
        },
      }),
      baseSpec('create_asset', {
        asset_strategy: 'ensure_asset',
        asset: {
          asset_type: 'data_asset',
          data_asset_class: '/Game/BlueprintHelper/Smoke/BP_BHSmokeDataAssetClass',
          collision_policy: 'reuse_if_exists',
        },
      }, {
        target: {
          asset_path: '/Game/BlueprintHelper/Smoke/DA_BHSmokeData',
          target_type: 'asset',
        },
        validation: {
          should_compile: false,
          should_save: true,
        },
      }),
      baseSpec('create_asset', {
        asset_strategy: 'ensure_asset',
        asset: {
          asset_type: 'widget_blueprint',
          collision_policy: 'reuse_if_exists',
        },
      }, {
        target: {
          asset_path: '/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke',
          target_type: 'asset',
        },
      }),
      baseSpec('edit_blueprint_components', {
        component_strategy: 'component_tree',
        changes: [
          {
            kind: 'ensure_component_present',
            name: 'DoorMesh',
            class: 'StaticMeshComponent',
          },
        ],
      }),
      baseSpec('edit_blueprint_class_settings', {
        class_settings_strategy: 'class_settings',
        interfaces: {
          ensure_present: ['/Game/Interfaces/BPI_Interact'],
        },
      }),
      baseSpec('edit_umg_widget', {
        widget_strategy: 'widget_blueprint_edit',
        changes: [
          {
            kind: 'create_widget',
            widget_class: 'TextBlock',
            widget_name: 'TitleText',
            parent_widget_name: '',
          },
        ],
      }, {
        target: {
          asset_path: '/Game/UI/WBP_MainMenu',
          target_type: 'widget_blueprint',
        },
      }),
      baseSpec('edit_data_table', {
        row_strategy: 'row_edit',
        rows: [
          {
            action: 'add',
            row_name: 'Pistol',
            fields: {
              Damage: '12',
            },
          },
        ],
      }, {
        target: {
          asset_path: '/Game/Data/DT_Weapons',
          target_type: 'data_table',
        },
      }),
    ];

    for (const spec of specs) {
      assert.doesNotThrow(() => TaskSpecSchema.parse(spec));
    }
  });

  it('rejects DataAsset creation without an explicit DataAsset class', () => {
    const result = TaskSpecSchema.safeParse(baseSpec('create_asset', {
      asset_strategy: 'ensure_asset',
      asset: {
        asset_type: 'data_asset',
        collision_policy: 'reuse_if_exists',
      },
    }, {
      target: {
        asset_path: '/Game/BlueprintHelper/Smoke/DA_BHSmokeData',
        target_type: 'asset',
      },
      validation: {
        should_compile: false,
        should_save: true,
      },
    }));

    assert.equal(result.success, false);
    if (!result.success) {
      assert.equal(result.error.issues[0]?.path.join('.'), 'behavior.asset.data_asset_class');
      assert.match(result.error.issues[0]?.message ?? '', /concrete UDataAsset subclass/);
    }
  });

  it('accepts the composite create_blueprint_feature TaskSpec shape', () => {
    const spec = {
      schema: 'BlueprintHelper.TaskSpec.v1',
      context_id: 'ctx_physics_door',
      task_type: 'create_blueprint_feature',
      feature_name: 'PhysicsDoor',
      target: {
        asset_path: '/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor',
        target_type: 'blueprint',
      },
      scope_policy: {
        prefer_new_graph: true,
        graph_name: 'EG_PhysicsDoor',
        allow_modify_user_nodes: false,
        allow_create_assets: false,
      },
      asset_policy: {
        if_target_asset_missing: 'fail',
        if_referenced_asset_missing: 'fail',
        if_component_exists: 'reuse_if_type_matches',
      },
      resources: {
        static_meshes: {
          door_mesh: '/Game/BlueprintHelperTest/Meshes/SM_Door',
        },
      },
      components: [
        {
          name: 'DoorMesh',
          class: 'StaticMeshComponent',
          attach_to: 'SceneRoot',
          properties: {
            StaticMesh: '$resources.static_meshes.door_mesh',
            Mobility: 'Movable',
          },
        },
      ],
      variables: [
        {
          name: 'bDoorOpen',
          type: 'bool',
          default: false,
          category: 'Door',
        },
      ],
      class_settings: {
        implemented_interfaces: [
          '/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable',
        ],
      },
      behavior: {
        graph_strategy: 'append_new_owned_graph',
        entries: [
          {
            entry_type: 'custom_event',
            name: 'OpenPhysicsDoor',
            body: {
              schema: 'BlueprintLogicSpec.v1',
              statements: [
                {
                  kind: 'set',
                  target: 'bDoorOpen',
                  value: {
                    kind: 'literal',
                    value_type: 'bool',
                    value: true,
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
        should_compile: true,
        should_save: false,
      },
    };

    assert.doesNotThrow(() => TaskSpecSchema.parse(spec));
  });

  it('rejects TaskPlan and adapter language in Agent-facing P1 TaskSpec shapes', () => {
    assert.throws(() => TaskSpecSchema.parse(baseSpec('create_asset', {
      asset_strategy: 'asset_create',
      asset: {
        asset_type: 'input_action',
      },
    }, {
      target: {
        asset_path: '/Game/Input/IA_Interact',
        target_type: 'asset',
      },
    })));

    assert.throws(() => TaskSpecSchema.parse(baseSpec('edit_data_table', {
      row_strategy: 'row_edit',
      rows: [
        {
          op: 'add_row',
          row_name: 'Pistol',
          fields: {
            Damage: '12',
          },
        },
      ],
    }, {
      target: {
        asset_path: '/Game/Data/DT_Weapons',
        target_type: 'data_table',
      },
    })));
  });
});

describe('P1 TaskPlan schema validation', () => {
  it('accepts structured IR steps for P1 capability clusters without adapter operation fields', () => {
    const taskPlan = {
      schema: 'BlueprintHelper.TaskPlan.v1',
      task_name: 'P1Feature',
      task_type: 'p1_multi_capability_probe',
      target_assets: ['/Game/Blueprints/BP_Door'],
      execution_policy: {
        dry_run_mode: 'full',
        should_compile: true,
        should_save: false,
      },
      steps: [
        {
          step_id: 'step_001',
          capability: 'asset_factory',
          target: { asset_path: '/Game/Input/IA_Interact' },
          write: {
            strategy: 'asset_create',
            ops: [{ op: 'create_asset', asset_type: 'input_action' }],
          },
        },
        {
          step_id: 'step_002',
          capability: 'blueprint_component',
          target: { asset_path: '/Game/Blueprints/BP_Door' },
          write: {
            strategy: 'component_tree',
            ops: [{ op: 'remove_component', component_name: 'DoorMesh' }],
          },
        },
        {
          step_id: 'step_003',
          capability: 'blueprint_class_settings',
          target: { asset_path: '/Game/Blueprints/BP_Door' },
          write: {
            strategy: 'class_settings',
            ops: [{ op: 'remove_implemented_interfaces', interface_paths: ['/Game/BPI_Door'] }],
          },
        },
        {
          step_id: 'step_004',
          capability: 'umg_widget',
          target: { asset_path: '/Game/UI/WBP_MainMenu' },
          write: {
            strategy: 'widget_tree_edit',
            ops: [{ op: 'remove_widget', widget_name: 'OldButton' }],
          },
        },
        {
          step_id: 'step_005',
          capability: 'data_table',
          target: { asset_path: '/Game/Data/DT_Weapons' },
          write: {
            strategy: 'row_edit',
            ops: [{ op: 'delete_row', row_name: 'OldPistol' }],
          },
        },
      ],
    };

    assert.doesNotThrow(() => TaskPlanSchema.parse(taskPlan));
  });
});
