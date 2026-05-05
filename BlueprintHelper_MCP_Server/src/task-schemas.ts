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
  schema: z.literal('BlueprintLogicSpec.v1'),
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

export const GraphWriteTaskSpecSchema = TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_graph'),
  scope_policy: z.object({
    graph_name: z.string().min(1),
    allow_modify_user_nodes: z.boolean().optional().default(false),
  }).passthrough(),
  behavior: z.object({
    graph_strategy: z.string(),
    entries: z.array(z.object({
      entry_type: z.string(),
      name: z.string().min(1),
      body: BlueprintLogicSpecSchema,
    }).passthrough()).min(1),
  }).passthrough(),
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
      collision: z.string().min(1).optional(),
      collision_policy: z.string().min(1).optional(),
    }).passthrough(),
  }).passthrough(),
}).passthrough();

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
      parent_widget_name: z.string().min(1).optional(),
      parent_name: z.string().min(1).optional(),
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

export const TaskSpecSchema = z.union([
  GraphWriteTaskSpecSchema,
  BlueprintVariableTaskSpecSchema,
  AssetFactoryTaskSpecSchema,
  BlueprintComponentTaskSpecSchema,
  BlueprintClassSettingsTaskSpecSchema,
  UMGWidgetTaskSpecSchema,
  DataTableTaskSpecSchema,
]);

export const ReadTaskContextInputSchema = z.object({
  target: z.object({
    asset_path: z.string().min(1),
  }).passthrough(),
  intent: z.string().optional(),
  feature_name: z.string().optional(),
});

export const PreviewTaskInputSchema = z.object({
  task_spec: TaskSpecSchema,
});

export const ExecuteTaskInputSchema = z.object({
  task_spec: TaskSpecSchema,
});

export const GetTaskResultInputSchema = z.object({
  task_run_id: z.string().min(1),
});

export const TaskIssueSchema = z.object({
  code: z.string().min(1),
  path: z.string(),
  message: z.string().min(1),
  suggested_patch: z.record(z.unknown()).optional(),
});

export const TaskContextPackSchema = z.object({
  schema: z.literal(TASK_CONTEXT_PACK_SCHEMA),
  context_id: z.string().min(1),
  intent: z.string().optional(),
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
    recommended_graph_name: z.string().optional(),
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

export const UMGWidgetTaskPlanStepSchema = structuredCapabilityStepSchema(
  'umg_widget',
  ['widget_tree_edit', 'widget_property_edit'],
);

export const DataTableTaskPlanStepSchema = structuredCapabilityStepSchema(
  'data_table',
  ['row_edit'],
);

export const TaskPlanStepSchema = z.union([
  GraphWriteTaskPlanStepSchema,
  BlueprintVariableTaskPlanStepSchema,
  AssetFactoryTaskPlanStepSchema,
  BlueprintComponentTaskPlanStepSchema,
  BlueprintClassSettingsTaskPlanStepSchema,
  UMGWidgetTaskPlanStepSchema,
  DataTableTaskPlanStepSchema,
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
  preview_id: z.string().min(1),
  task_type: z.string().min(1),
  feature_name: z.string().optional(),
  status: z.enum(['completed', 'failed']),
  target_assets: z.array(z.string().min(1)),
  steps: z.array(z.object({
    step_id: z.string().min(1),
    operation: z.string().min(1),
    status: z.string().min(1),
    transaction_id: z.string().optional(),
  }).passthrough()),
  bridge_result: z.record(z.unknown()).optional(),
}).passthrough();

export type TaskSpec = z.infer<typeof TaskSpecSchema>;
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
export type UMGWidgetTaskPlanStep = z.infer<typeof UMGWidgetTaskPlanStepSchema>;
export type DataTableTaskPlanStep = z.infer<typeof DataTableTaskPlanStepSchema>;
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
  nodes: AgentImportNode[];
  links: AgentImportLink[];
  dry_run: boolean;
}
