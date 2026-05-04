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

export const TaskSpecSchema = z.object({
  schema: z.literal(TASK_SPEC_SCHEMA),
  context_id: z.string().optional(),
  task_type: z.string(),
  feature_name: z.string().optional(),
  target: z.object({
    asset_path: z.string().min(1),
    target_type: z.string().optional().default('blueprint'),
  }).passthrough(),
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
  execution_policy: z.object({
    dry_run_mode: z.enum(['none', 'quick', 'full']).optional().default('full'),
    on_missing_capability: z.string().optional(),
  }).passthrough().optional().default({ dry_run_mode: 'full' }),
  validation: z.object({
    should_compile: z.boolean().optional().default(false),
    should_save: z.boolean().optional().default(false),
  }).passthrough().optional().default({ should_compile: false, should_save: false }).superRefine((value, ctx) => {
    if ('compile' in value || 'save' in value) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        message: 'Use validation.should_compile / validation.should_save; validation.compile / validation.save are not TaskSpec fields.',
      });
    }
  }),
}).passthrough();

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

export const AppendTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('append_blueprint_graph'),
  target: z.object({
    asset_path: z.string().min(1),
    graph: z.string().min(1),
  }),
  args: z.object({
    feature_name: z.string().optional(),
    nodes: z.array(AgentImportNodeSchema).min(1),
    links: z.array(AgentImportLinkSchema),
  }),
});

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
  steps: z.array(AppendTaskPlanStepSchema).min(1),
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
