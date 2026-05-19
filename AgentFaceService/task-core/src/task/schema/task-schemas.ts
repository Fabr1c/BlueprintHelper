import { z } from 'zod';

export const TASK_CONTEXT_PACK_SCHEMA = 'BlueprintHelper.TaskContextPack.v1';
export const TASK_SPEC_SCHEMA = 'BlueprintHelper.TaskSpec.v1';
export const TASK_PLAN_SCHEMA = 'BlueprintHelper.TaskPlan.v1';
export const TASK_PREVIEW_SCHEMA = 'BlueprintHelper.TaskPreview.v1';
export const TASK_EXECUTION_SCHEMA = 'BlueprintHelper.TaskExecution.v1';
export const TASK_RUN_JOURNAL_SCHEMA = 'BlueprintHelper.TaskRunJournal.v1';
export const TASK_ERROR_SCHEMA = 'BlueprintHelper.TaskError.v1';

const LiteralValueExprSchema = z.object({
  kind: z.literal('literal'),
  value_type: z.string(),
  value: z.unknown(),
}).passthrough();

export const ValueExprSchema = z.union([
  LiteralValueExprSchema,
  z.object({ kind: z.string() }).passthrough(),
]);

export const BlueprintLogicStatementSchema = z.object({
  kind: z.string(),
}).passthrough();

export const BlueprintLogicSpecSchema = z.object({
  schema: z.union([z.literal('BlueprintLogicSpec.v1'), z.literal('BlueprintLogicSpec.v2')]),
  statements: z.array(BlueprintLogicStatementSchema),
}).passthrough();

const TaskValidationPolicySchema = z.object({
  should_compile: z.boolean().optional().default(false),
  should_save: z.boolean().optional().default(false),
}).passthrough().optional().default({ should_compile: false, should_save: false }).superRefine((value, ctx) => {
  if ('compile' in value || 'save' in value) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: 'Use validation.should_compile / validation.should_save; validation.compile / validation.save are not TaskSpec fields.',
    });
  }
});

const TaskSpecBaseSchema = z.object({
  schema: z.literal(TASK_SPEC_SCHEMA),
  context_id: z.string().optional(),
  task_type: z.string(),
  feature_name: z.string().optional(),
  target: z.object({
    asset_path: z.string().min(1),
    target_type: z.string().optional().default('blueprint'),
  }).passthrough(),
  execution_policy: z.object({
    dry_run_mode: z.enum(['none', 'quick', 'full']).optional().default('full'),
    on_missing_capability: z.string().optional(),
  }).passthrough().optional().default({ dry_run_mode: 'full' }),
  validation: TaskValidationPolicySchema,
}).passthrough();

const GraphWriteAppendEntrySchema = z.object({
  entry_type: z.string(),
  name: z.string().min(1),
  body: BlueprintLogicSpecSchema,
}).passthrough();

const GraphWriteReplaceSchema = z.object({
  scope: z.enum(['custom_event_definition', 'custom_event_body', 'function_body', 'event_body', 'block_implementation']),
  selector: z.object({
    kind: z.enum(['custom_event', 'function', 'event', 'block']),
    name: z.string().min(1).optional(),
    block_id: z.string().min(1).optional(),
    graph_id: z.string().min(1).optional(),
    node_ref: z.string().min(1).optional(),
    node_path: z.string().min(1).optional(),
  }).passthrough(),
  inputs: z.array(z.record(z.unknown())).optional(),
  body: BlueprintLogicSpecSchema,
  options: z.object({
    strict: z.boolean().optional(),
    preserve_layout: z.boolean().optional(),
  }).passthrough().optional(),
}).passthrough().superRefine((value, ctx) => {
  const expectedKindByScope: Record<string, string> = {
    custom_event_definition: 'custom_event',
    custom_event_body: 'custom_event',
    function_body: 'function',
    event_body: 'event',
    block_implementation: 'block',
  };
  const expectedKind = expectedKindByScope[value.scope];
  if (value.selector.kind !== expectedKind) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['selector', 'kind'],
      message: `${value.scope} requires selector.kind="${expectedKind}".`,
    });
  }
  if (value.selector.kind === 'block') {
    if (!value.selector.block_id) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['selector', 'block_id'],
        message: 'block_implementation requires selector.block_id.',
      });
    }
  } else if (!value.selector.name) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['selector', 'name'],
      message: `${value.scope} requires selector.name.`,
    });
  }
});

const GraphWritePatchSchema = z.object({
  kind: z.enum(['set_pin_default', 'set_node_comment', 'set_node_position']),
  scope: z.string().min(1).optional(),
  target_ref: z.record(z.unknown()),
  value: z.unknown().optional(),
  patch: z.record(z.unknown()).optional(),
  expected_old_state: z.record(z.unknown()).optional(),
}).passthrough().superRefine((value, ctx) => {
  const expectedScopeByKind: Record<string, string> = {
    set_pin_default: 'pin_default',
    set_node_comment: 'node_comment',
    set_node_position: 'node_position',
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
  if ((value.kind === 'set_pin_default' || value.kind === 'set_node_comment') && !Object.hasOwn(value, 'value')) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['value'], message: `${value.kind} requires value.` });
  }
  if (value.kind === 'set_node_position') {
    const patch = value.patch;
    if (!patch || (typeof patch.x !== 'number' && typeof patch.y !== 'number')) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['patch'], message: 'set_node_position requires patch.x and/or patch.y as numbers.' });
    }
  }
});

const GraphWriteMergeSchema = z.object({
  kind: z.literal('insert_flow'),
  scope: z.enum(['owned_block_call', 'custom_event_call', 'function_call']),
  insert_strategy: z.enum(['append_after', 'insert_between', 'branch_fork']),
  anchor: z.record(z.unknown()),
  inserted: z.record(z.unknown()),
  sequence_order: z.array(z.string()).optional(),
}).passthrough().superRefine((value, ctx) => {
  if (typeof value.anchor.node_ref !== 'string' || value.anchor.node_ref.length === 0) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['anchor', 'node_ref'], message: 'anchor.node_ref is required.' });
  }
  if (typeof value.anchor.pin_ref !== 'string' || value.anchor.pin_ref.length === 0) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['anchor', 'pin_ref'], message: 'anchor.pin_ref is required.' });
  }
  if (value.inserted.call_kind !== value.scope) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['inserted', 'call_kind'], message: `inserted.call_kind must be ${value.scope}.` });
  }
  if (value.scope === 'owned_block_call') {
    if (typeof value.inserted.block_id !== 'string' || value.inserted.block_id.length === 0) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['inserted', 'block_id'], message: 'owned_block_call requires inserted.block_id.' });
    }
  } else if (typeof value.inserted.name !== 'string' || value.inserted.name.length === 0) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['inserted', 'name'], message: `${value.scope} requires inserted.name.` });
  }
  if (value.insert_strategy === 'branch_fork') {
    if (!value.sequence_order || value.sequence_order.length === 0) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order'], message: 'branch_fork requires sequence_order.' });
    } else {
      value.sequence_order.forEach((entry, index) => {
        if (entry !== 'inserted_logic' && entry !== 'original_successor') {
          ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order', index], message: 'Use inserted_logic or original_successor.' });
        }
      });
    }
  } else if (value.sequence_order !== undefined) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order'], message: 'sequence_order is only valid for branch_fork.' });
  }
});

const GraphWriteBehaviorSchema = z.object({
  graph_strategy: z.string(),
  entries: z.array(GraphWriteAppendEntrySchema).min(1).optional(),
  replace: GraphWriteReplaceSchema.optional(),
  patches: z.array(GraphWritePatchSchema).min(1).optional(),
  merges: z.array(GraphWriteMergeSchema).min(1).optional(),
}).passthrough().superRefine((value, ctx) => {
  const requiredFieldByStrategy: Record<string, keyof typeof value> = {
    append_new_owned_graph: 'entries',
    replace_owned_graph: 'replace',
    patch_owned_graph: 'patches',
    merge_owned_graph: 'merges',
  };
  const requiredField = requiredFieldByStrategy[value.graph_strategy];
  if (!requiredField) return;
  if (requiredField && value[requiredField] === undefined) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: [requiredField],
      message: `${value.graph_strategy} requires behavior.${requiredField}.`,
    });
  }
  (['entries', 'replace', 'patches', 'merges'] as const)
    .filter((field) => field !== requiredField && value[field] !== undefined)
    .forEach((field) => {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: [field],
        message: `${field} does not belong to graph_strategy ${value.graph_strategy}.`,
      });
    });
});

export const GraphWriteTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_graph'),
  scope_policy: z.object({
    graph_name: z.string().min(1),
    allow_modify_user_nodes: z.boolean().optional().default(false),
  }).passthrough(),
  behavior: GraphWriteBehaviorSchema,
}).passthrough();

export const BlueprintVariableTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_variables'),
  behavior: z.object({
    variable_strategy: z.string(),
    function_name: z.string().min(1).optional(),
    variables: z.array(z.object({
      op: z.string().min(1).optional(),
      kind: z.string().min(1).optional(),
      name: z.string().min(1),
      pin_type: z.record(z.unknown()).optional(),
      variable_type: z.record(z.unknown()).optional(),
      properties: z.array(z.record(z.unknown())).optional(),
      value: z.unknown().optional(),
    }).passthrough()).optional(),
    changes: z.array(z.object({
      kind: z.string().min(1),
      name: z.string().min(1),
      pin_type: z.record(z.unknown()).optional(),
      variable_type: z.record(z.unknown()).optional(),
      properties: z.array(z.record(z.unknown())).optional(),
      value: z.unknown().optional(),
    }).passthrough()).optional(),
    defaults: z.array(z.object({
      kind: z.string().min(1).optional(),
      name: z.string().min(1),
      value: z.unknown(),
    }).passthrough()).optional(),
  }).passthrough(),
}).passthrough();

export const AssetFactoryTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('create_asset'),
  behavior: z.object({
    asset_strategy: z.literal('ensure_asset'),
    asset: z.object({
      asset_type: z.string().min(1),
      parent_class: z.string().min(1).optional(),
      value_type: z.string().min(1).optional(),
      fields: z.array(z.object({
        name: z.string().min(1),
        type: z.enum(['int', 'float', 'bool', 'string']),
        default_value: z.unknown().optional(),
      }).passthrough()).min(1).optional(),
      row_struct: z.string().min(1).optional(),
      data_asset_class: z.string().min(1).optional(),
      collision: z.string().min(1).optional(),
      collision_policy: z.string().min(1).optional(),
    }).passthrough(),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  const assetType = value.behavior.asset.asset_type.trim().toLowerCase();
  if ((assetType === 'data_table' || assetType === 'datatable') && !value.behavior.asset.row_struct) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['behavior', 'asset', 'row_struct'],
      message: 'asset_type=data_table requires behavior.asset.row_struct.',
    });
  }
  if ((assetType === 'data_asset' || assetType === 'dataasset') && !value.behavior.asset.data_asset_class) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['behavior', 'asset', 'data_asset_class'],
      message: 'asset_type=data_asset requires behavior.asset.data_asset_class. Use a concrete UDataAsset subclass; in a new project, create a PrimaryDataAsset Blueprint class first and pass its asset path or generated class path.',
    });
  }
});

export const BlueprintComponentTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_components'),
  behavior: z.object({
    component_strategy: z.literal('component_tree'),
    changes: z.array(z.object({
      kind: z.string().min(1),
      name: z.string().min(1).optional(),
      class: z.string().min(1).optional(),
      attach: z.record(z.unknown()).optional(),
      on_name_conflict: z.string().min(1).optional(),
      properties: z.array(z.record(z.unknown())).optional(),
    }).passthrough()).min(1),
  }).passthrough(),
}).passthrough();

export const BlueprintClassSettingsTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_class_settings'),
  behavior: z.object({
    class_settings_strategy: z.literal('class_settings'),
    interfaces: z.object({
      ensure_present: z.array(z.string().min(1)).optional(),
      ensure_absent: z.array(z.string().min(1)).optional(),
    }).passthrough().optional(),
    class_defaults: z.array(z.record(z.unknown())).optional(),
    parent_class: z.string().min(1).optional(),
  }).passthrough(),
}).passthrough();

export const UMGWidgetTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_umg_widget'),
  behavior: z.object({
    widget_strategy: z.literal('widget_blueprint_edit'),
    changes: z.array(z.object({
      kind: z.string().min(1),
      widget_name: z.string().min(1).optional(),
      widget_class: z.string().min(1).optional(),
      parent_widget_name: z.string().optional(),
      parent_name: z.string().optional(),
      property_name: z.string().min(1).optional(),
      property_path: z.string().min(1).optional(),
      value: z.unknown().optional(),
    }).passthrough()).min(1),
  }).passthrough(),
}).passthrough();

export const DataTableTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_data_table'),
  behavior: z.object({
    row_strategy: z.literal('row_edit'),
    rows: z.array(z.object({
      action: z.enum(['add', 'update', 'delete']),
      row_name: z.string().min(1),
      fields: z.record(z.unknown()).optional(),
    }).passthrough()).min(1),
  }).passthrough(),
}).passthrough();

export const ObjectPropertyTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_object_properties'),
  behavior: z.object({
    property_strategy: z.literal('property_edit'),
    changes: z.array(z.object({
      kind: z.enum(['set_property', 'set_object_property']).optional(),
      property_path: z.string().min(1),
      value: z.unknown(),
    }).passthrough()).min(1),
  }).passthrough(),
}).passthrough();

const BlueprintSignatureChangeSchema = z.object({
  kind: z.enum([
    'ensure_function',
    'ensure_interface_function',
    'ensure_custom_event',
    'ensure_interface_event',
    'ensure_event_dispatcher',
    'ensure_override_event',
    'remove_signature',
  ]),
  function_name: z.string().min(1).optional(),
  event_name: z.string().min(1).optional(),
  graph_name: z.string().min(1).optional(),
  dispatcher_name: z.string().min(1).optional(),
  signature_kind: z.enum([
    'function',
    'interface_function',
    'custom_event',
    'interface_event',
    'event_dispatcher',
    'override_event',
    'native_event',
  ]).optional(),
  signature_name: z.string().min(1).optional(),
  interface_path: z.string().min(1).optional(),
  inputs: z.array(z.record(z.unknown())).optional(),
  outputs: z.array(z.record(z.unknown())).optional(),
  is_pure: z.boolean().optional(),
  name_collision_policy: z.enum(['reuse_if_exists', 'fail_if_exists']).optional(),
  signature_mismatch_policy: z.enum(['block', 'migrate_if_unreferenced']).optional(),
  event_kind: z.enum(['native_event', 'override_event']).optional(),
  execute_policy: z.enum(['blocked_preflight', 'create_if_missing', 'execute_if_unreferenced']).optional(),
  require_reference_context: z.literal(true).optional(),
}).passthrough();

export const BlueprintSignatureTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_signature'),
  behavior: z.object({
    signature_strategy: z.literal('signature_edit'),
    changes: z.array(BlueprintSignatureChangeSchema).min(1),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  value.behavior.changes.forEach((change, index) => {
    const path = ['behavior', 'changes', index];
    if (change.kind === 'ensure_function' && !change.function_name) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: [...path, 'function_name'], message: 'ensure_function requires function_name.' });
    }
    if (change.kind === 'ensure_interface_function' && (!change.function_name || !change.interface_path)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'ensure_interface_function requires function_name and interface_path.' });
    }
    if (change.kind === 'ensure_custom_event' && (!change.event_name || !change.graph_name)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'ensure_custom_event requires event_name and graph_name.' });
    }
    if (change.kind === 'ensure_interface_event' && (!change.event_name || !change.graph_name || !change.interface_path)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'ensure_interface_event requires event_name, graph_name, and interface_path.' });
    }
    if (change.kind === 'ensure_event_dispatcher' && !change.dispatcher_name) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: [...path, 'dispatcher_name'], message: 'ensure_event_dispatcher requires dispatcher_name.' });
    }
    if (change.kind === 'ensure_override_event' && !change.event_name) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: [...path, 'event_name'], message: 'ensure_override_event requires event_name.' });
    }
    if (change.kind === 'remove_signature' && !(change.signature_name || change.function_name || change.event_name || change.dispatcher_name)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'remove_signature requires signature_name, function_name, event_name, or dispatcher_name.' });
    }
  });
});

export const CompositeBlueprintFeatureTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('create_blueprint_feature'),
  scope_policy: z.object({
    graph_name: z.string().min(1).optional(),
    prefer_new_graph: z.boolean().optional(),
    allow_modify_user_nodes: z.boolean().optional().default(false),
    allow_merge_existing_execution_flow: z.boolean().optional().default(false),
    allow_create_assets: z.boolean().optional().default(false),
    allow_edit_input_mapping: z.boolean().optional().default(false),
  }).passthrough().optional(),
  asset_policy: z.record(z.unknown()).optional(),
  resources: z.record(z.unknown()).optional(),
  components: z.array(z.object({
    name: z.string().min(1),
    class: z.string().min(1),
    attach_to: z.string().min(1).nullable().optional(),
    attach: z.record(z.unknown()).optional(),
    set_as_root: z.boolean().optional(),
    properties: z.union([
      z.record(z.unknown()),
      z.array(z.record(z.unknown())),
    ]).optional(),
  }).passthrough()).optional(),
  variables: z.array(z.object({
    name: z.string().min(1),
    type: z.string().min(1).optional(),
    pin_type: z.record(z.unknown()).optional(),
    variable_type: z.record(z.unknown()).optional(),
    default: z.unknown().optional(),
    category: z.string().optional(),
    tooltip: z.string().optional(),
    flags: z.record(z.unknown()).optional(),
    metadata: z.record(z.unknown()).optional(),
  }).passthrough()).optional(),
  class_settings: z.object({
    implemented_interfaces: z.array(z.string().min(1)).optional(),
    class_defaults: z.union([
      z.record(z.unknown()),
      z.array(z.record(z.unknown())),
    ]).optional(),
  }).passthrough().optional(),
  behavior: GraphWriteBehaviorSchema.optional(),
  integration: z.record(z.unknown()).optional(),
}).passthrough().superRefine((value, ctx) => {
  const hasComponents = Array.isArray(value.components) && value.components.length > 0;
  const hasVariables = Array.isArray(value.variables) && value.variables.length > 0;
  const hasClassSettings = value.class_settings !== undefined;
  const hasBehavior = value.behavior !== undefined;
  if (!hasComponents && !hasVariables && !hasClassSettings && !hasBehavior) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: 'create_blueprint_feature requires at least one of components, variables, class_settings, or behavior.',
    });
  }
});

export const TaskSpecSchema: z.ZodTypeAny = z.union([
  CompositeBlueprintFeatureTaskSpecSchema,
  GraphWriteTaskSpecSchema,
  BlueprintVariableTaskSpecSchema,
  AssetFactoryTaskSpecSchema,
  BlueprintComponentTaskSpecSchema,
  BlueprintClassSettingsTaskSpecSchema,
  UMGWidgetTaskSpecSchema,
  DataTableTaskSpecSchema,
  ObjectPropertyTaskSpecSchema,
  BlueprintSignatureTaskSpecSchema,
]).superRefine((value, ctx) => {
  if (value && typeof value === 'object' && !Array.isArray(value) && Object.hasOwn(value, 'intent')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['intent'],
      message: 'TaskSpec.intent is compiler/runtime generated; Agents must express executable intent through task_type and behavior.',
    });
  }
});

export const ReadTaskContextInputSchema = z.object({
  target: z.object({
    asset_path: z.string().min(1),
  }).passthrough(),
  feature_name: z.string().optional(),
}).passthrough().superRefine((value, ctx) => {
  if (Object.hasOwn(value, 'intent')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['intent'],
      message: 'read_task_context no longer accepts Agent-authored intent.',
    });
  }
});

export const PreviewTaskInputSchema: z.ZodTypeAny = z.object({
  task_spec: TaskSpecSchema,
  develop: z.boolean().optional().default(false),
});

export const TaskPreviewTokenSchema = z.object({
  preview_id: z.string().min(1),
  task_plan_hash: z.string().min(1),
  task_spec_hash: z.string().min(1),
  execution_policy_hash: z.string().min(1),
  created_at: z.string().min(1),
}).passthrough();

export const ExecuteTaskInputSchema: z.ZodTypeAny = z.object({
  task_spec: TaskSpecSchema,
  develop: z.boolean().optional().default(false),
  preview_token: TaskPreviewTokenSchema.optional(),
});

export const GetTaskResultInputSchema = z.object({
  task_run_id: z.string().min(1),
});

export const TaskIssueSchema = z.object({
  code: z.string().min(1),
  path: z.string(),
  message: z.string().min(1),
  suggested_patch: z.record(z.unknown()).optional(),
}).passthrough();

export const TaskContextPackSchema = z.object({
  schema: z.literal(TASK_CONTEXT_PACK_SCHEMA),
  context_id: z.string().min(1),
  feature_name: z.string().optional(),
  runtime: z.object({
    bridge_reachable: z.boolean(),
    profile: z.unknown(),
  }).passthrough(),
  target: z.object({
    asset_path: z.string().min(1),
    exists: z.boolean(),
    asset_info: z.unknown().optional(),
  }).passthrough(),
  blueprint_summary: z.object({
    graphs: z.array(z.unknown()),
  }).passthrough(),
  recommended_constraints: z.object({
    prefer_new_graph: z.boolean(),
    allow_modify_user_nodes: z.boolean(),
    graph_strategy: z.literal('append_new_owned_graph'),
  }).passthrough(),
}).passthrough();

export const AgentImportNodeSchema = z.object({
  id: z.string().min(1),
  kind: z.string().min(1),
  name: z.string().optional(),
  function: z.string().optional(),
  inputs: z.record(z.unknown()).optional(),
  var: z.string().optional(),
  value: z.string().optional(),
}).passthrough();

export const AgentImportLinkSchema = z.object({
  kind: z.enum(['exec', 'data']),
  from: z.string().min(1),
  to: z.string().min(1),
});

export const TaskPlanTargetSchema = z.object({
  asset_path: z.string().min(1),
  graph: z.string().min(1),
}).passthrough();

export const AssetOnlyTaskPlanTargetSchema = z.object({
  asset_path: z.string().min(1),
}).passthrough();

export const AppendTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('append_blueprint_graph'),
  target: TaskPlanTargetSchema,
  args: z.object({
    feature_name: z.string().optional(),
    nodes: z.array(AgentImportNodeSchema).min(1),
    links: z.array(AgentImportLinkSchema),
  }),
});

export const ReplaceTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('replace_blueprint_graph'),
  target: TaskPlanTargetSchema.extend({
    replace_scope: z.string().min(1).optional(),
  }).passthrough(),
  args: z.object({
    selector: z.record(z.unknown()).optional(),
    replacement: z.object({
      nodes: z.array(AgentImportNodeSchema),
      links: z.array(AgentImportLinkSchema),
    }).passthrough(),
    options: z.record(z.unknown()).optional(),
  }).passthrough(),
});

export const PatchTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('patch_blueprint_graph'),
  target: TaskPlanTargetSchema.extend({
    patch_scope: z.string().min(1).optional(),
  }).passthrough(),
  args: z.object({
    patch_type: z.string().min(1),
    patched_ref: z.record(z.unknown()).optional(),
    patch: z.record(z.unknown()).optional(),
    expected_old_state: z.record(z.unknown()).optional(),
  }).passthrough(),
});

export const MergeTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('merge_blueprint_graph'),
  target: TaskPlanTargetSchema.extend({
    merge_scope: z.string().min(1).optional(),
    insert_strategy: z.string().min(1).optional(),
  }).passthrough(),
  args: z.object({
    anchor: z.record(z.unknown()).optional(),
    inserted: z.record(z.unknown()).optional(),
    sequence_order: z.array(z.string()).optional(),
  }).passthrough(),
});

export const GraphWriteStructuredIrTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  capability: z.literal('graph_write'),
  target: TaskPlanTargetSchema,
  write: z.object({
    strategy: z.string().min(1),
    ops: z.array(z.object({
      op: z.string().min(1),
    }).passthrough()).min(1),
  }).passthrough(),
  constraints: z.object({
    allow_modify_user_nodes: z.boolean(),
    ownership_scope: z.string().min(1),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  if (Object.hasOwn(value, 'operation')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['operation'],
      message: 'GraphWrite IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.',
    });
  }
});

export const GraphWriteLoweringAdapterTaskPlanStepSchema = z.discriminatedUnion('operation', [
  AppendTaskPlanStepSchema,
  ReplaceTaskPlanStepSchema,
  PatchTaskPlanStepSchema,
  MergeTaskPlanStepSchema,
]);

export const GraphWriteTaskPlanStepSchema = z.union([
  GraphWriteStructuredIrTaskPlanStepSchema,
  GraphWriteLoweringAdapterTaskPlanStepSchema,
]);

export const BlueprintVariableTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  capability: z.literal('blueprint_variable'),
  target: z.object({
    asset_path: z.string().min(1),
  }).passthrough(),
  write: z.object({
    strategy: z.string().min(1),
    ops: z.array(z.object({
      op: z.string().min(1),
    }).passthrough()).min(1),
  }).passthrough(),
  constraints: z.object({
    allow_remove_referenced_variables: z.boolean(),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  if (Object.hasOwn(value, 'operation')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['operation'],
      message: 'Blueprint variable IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.',
    });
  }
});

const StructuredCapabilityWriteSchema = z.object({
  strategy: z.string().min(1),
  ops: z.array(z.object({
    op: z.string().min(1),
  }).passthrough()).min(1),
}).passthrough();

function structuredCapabilityStepSchema<TCapability extends string>(
  capability: TCapability,
  strategies: readonly [string, ...string[]],
) {
  return z.object({
    step_id: z.string().min(1),
    capability: z.literal(capability),
    target: AssetOnlyTaskPlanTargetSchema,
    write: StructuredCapabilityWriteSchema.extend({
      strategy: z.enum(strategies),
    }).passthrough(),
    constraints: z.record(z.unknown()).optional(),
  }).passthrough().superRefine((value, ctx) => {
    if (Object.hasOwn(value, 'operation')) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['operation'],
        message: `${capability} IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.`,
      });
    }
  });
}

export const AssetFactoryTaskPlanStepSchema = structuredCapabilityStepSchema(
  'asset_factory',
  ['asset_create'],
);

export const BlueprintComponentTaskPlanStepSchema = structuredCapabilityStepSchema(
  'blueprint_component',
  ['component_tree'],
);

export const BlueprintClassSettingsTaskPlanStepSchema = structuredCapabilityStepSchema(
  'blueprint_class_settings',
  ['class_settings'],
);

export const BlueprintSignatureTaskPlanStepSchema = structuredCapabilityStepSchema(
  'blueprint_signature',
  ['function_signature', 'custom_event_signature', 'event_dispatcher_signature', 'override_event_signature'],
);

export const UMGWidgetTaskPlanStepSchema = structuredCapabilityStepSchema(
  'umg_widget',
  ['widget_tree_edit', 'widget_property_edit'],
);

export const DataTableTaskPlanStepSchema = structuredCapabilityStepSchema(
  'data_table',
  ['row_edit'],
);

export const ObjectPropertyTaskPlanStepSchema = structuredCapabilityStepSchema(
  'object_property',
  ['property_edit'],
);

export const TaskPlanStepSchema = z.union([
  GraphWriteTaskPlanStepSchema,
  BlueprintVariableTaskPlanStepSchema,
  AssetFactoryTaskPlanStepSchema,
  BlueprintComponentTaskPlanStepSchema,
  BlueprintClassSettingsTaskPlanStepSchema,
  BlueprintSignatureTaskPlanStepSchema,
  UMGWidgetTaskPlanStepSchema,
  DataTableTaskPlanStepSchema,
  ObjectPropertyTaskPlanStepSchema,
]);

export const TaskPlanSchema = z.object({
  schema: z.literal(TASK_PLAN_SCHEMA),
  task_name: z.string().optional(),
  task_type: z.string().min(1),
  context_id: z.string().optional(),
  target_assets: z.array(z.string().min(1)).min(1),
  execution_policy: z.object({
    dry_run_mode: z.enum(['none', 'quick', 'full']),
    should_compile: z.boolean(),
    should_save: z.boolean(),
    review_baseline_dirty_asset_policy: z.enum(['block', 'save_before_archive', 'allow_stale_disk_snapshot']).optional().default('block'),
  }),
  steps: z.array(TaskPlanStepSchema).min(1),
});

export const TaskErrorSchema = z.object({
  schema: z.literal(TASK_ERROR_SCHEMA).optional(),
  code: z.string().min(1),
  category: z.string().min(1),
  stage: z.string().min(1),
  message: z.string().min(1),
  retryable: z.boolean(),
  rollback_result: z.string().optional(),
  agent_action: z.string().optional(),
  issues: z.array(TaskIssueSchema).optional(),
}).passthrough();

export const TaskRunJournalSchema = z.object({
  schema: z.literal(TASK_RUN_JOURNAL_SCHEMA),
  task_run_id: z.string().min(1),
  preview_id: z.string().min(1).optional(),
  task_type: z.string().min(1),
  feature_name: z.string().optional(),
  generated_intent: z.string().optional(),
  status: z.enum(['completed', 'failed', 'partial_failure']),
  target_assets: z.array(z.string().min(1)),
  steps: z.array(z.object({
    step_id: z.string().min(1),
    operation: z.string().min(1),
    status: z.enum(['completed', 'failed', 'blocked', 'skipped']),
    depends_on: z.array(z.string().min(1)).optional(),
    blocked_by_step_ids: z.array(z.string().min(1)).optional(),
    blocked_reason: z.string().min(1).optional(),
    transaction_id: z.string().optional(),
    result: z.record(z.unknown()).optional(),
    error: z.unknown().nullable().optional(),
  }).passthrough()),
  recovery: z.object({
    recommended_action: z.string().min(1),
    safe_to_retry: z.boolean(),
    rollback_available: z.boolean(),
    notes: z.array(z.string()),
  }).optional(),
  bridge_result: z.record(z.unknown()).optional(),
}).passthrough();

export type TaskSpec = z.infer<typeof TaskSpecSchema>;
export type TaskPreviewToken = z.infer<typeof TaskPreviewTokenSchema>;
export type BlueprintLogicStatement = z.infer<typeof BlueprintLogicStatementSchema>;
export type BlueprintLogicSpec = z.infer<typeof BlueprintLogicSpecSchema>;
export type ReadTaskContextInput = z.infer<typeof ReadTaskContextInputSchema>;
export type TaskIssue = z.infer<typeof TaskIssueSchema>;
export type AgentImportNode = z.infer<typeof AgentImportNodeSchema>;
export type AgentImportLink = z.infer<typeof AgentImportLinkSchema>;
export type AppendTaskPlanStep = z.infer<typeof AppendTaskPlanStepSchema>;
export type ReplaceTaskPlanStep = z.infer<typeof ReplaceTaskPlanStepSchema>;
export type PatchTaskPlanStep = z.infer<typeof PatchTaskPlanStepSchema>;
export type MergeTaskPlanStep = z.infer<typeof MergeTaskPlanStepSchema>;
export type GraphWriteStructuredIrTaskPlanStep = z.infer<typeof GraphWriteStructuredIrTaskPlanStepSchema>;
export type GraphWriteLoweringAdapterTaskPlanStep = z.infer<typeof GraphWriteLoweringAdapterTaskPlanStepSchema>;
export type GraphWriteTaskPlanStep = z.infer<typeof GraphWriteTaskPlanStepSchema>;
export type BlueprintVariableTaskPlanStep = z.infer<typeof BlueprintVariableTaskPlanStepSchema>;
export type AssetFactoryTaskPlanStep = z.infer<typeof AssetFactoryTaskPlanStepSchema>;
export type BlueprintComponentTaskPlanStep = z.infer<typeof BlueprintComponentTaskPlanStepSchema>;
export type BlueprintClassSettingsTaskPlanStep = z.infer<typeof BlueprintClassSettingsTaskPlanStepSchema>;
export type BlueprintSignatureTaskPlanStep = z.infer<typeof BlueprintSignatureTaskPlanStepSchema>;
export type UMGWidgetTaskPlanStep = z.infer<typeof UMGWidgetTaskPlanStepSchema>;
export type DataTableTaskPlanStep = z.infer<typeof DataTableTaskPlanStepSchema>;
export type ObjectPropertyTaskPlanStep = z.infer<typeof ObjectPropertyTaskPlanStepSchema>;
export type BlueprintSignatureTaskSpec = z.infer<typeof BlueprintSignatureTaskSpecSchema>;
export type TaskPlanStep = z.infer<typeof TaskPlanStepSchema>;
export type TaskPlan = z.infer<typeof TaskPlanSchema>;
export type TaskError = z.infer<typeof TaskErrorSchema>;
export type TaskRunJournal = z.infer<typeof TaskRunJournalSchema>;

export interface AppendBridgePayload {
  target: {
    asset_path: string;
    graph: string;
  };
  feature_name?: string;
  logic_spec: {
    schema: 'BlueprintLogicSpec.v2';
    entry?: Record<string, unknown>;
    statements: BlueprintLogicStatement[];
  };
  dry_run: boolean;
}
