import { z } from 'zod';

export const READ_CONTEXT_LOGIC_FORMATS = ['logic_flow', 'logic_md', 'logic_json'] as const;
export type ReadContextLogicFormat = (typeof READ_CONTEXT_LOGIC_FORMATS)[number];
export const READ_CONTEXT_VIEW_FORMATS = ['logic_flow', 'logic_md', 'logic_json', 'tree_json'] as const;
export type ReadContextViewFormat = (typeof READ_CONTEXT_VIEW_FORMATS)[number];

export const LOGIC_PROJECTION_CALLBACK_CAPABILITIES = [
  'ue.raw_snapshot.logic_json',
  'ue.raw_snapshot.logic_md',
  'ue.raw_snapshot.logic_flow',
] as const;
export type LogicProjectionCallbackCapability =
  (typeof LOGIC_PROJECTION_CALLBACK_CAPABILITIES)[number];

export const LOGIC_PROJECTION_OWNER = 'task-core' as const;

export const ReadContextInputSchema = z.object({
  schema: z.literal('BlueprintHelper.ReadSpec.v1'),
  read_type: z.enum([
    'asset_context',
    'blueprint_logic',
    'component_context',
    'variable_context',
    'graph_context',
    'widget_context',
    'data_table_context',
    'data_asset_context',
    'object_property_context',
  ]),
  target: z.object({
    asset_path: z.string(),
    asset_type: z.string().optional(),
    target_type: z.enum([
      'asset',
      'blueprint',
      'graph',
      'function',
      'event',
      'custom_event',
      'component',
      'member_variable',
      'event_dispatcher',
      'widget',
      'data_table',
      'data_table_row',
      'data_asset',
      'object_property',
      'property',
      'block',
    ]).optional().default('blueprint'),
    target_name: z.string().optional(),
    block_id: z.string().optional(),
  }),
  view: z.object({
    format: z.enum(READ_CONTEXT_VIEW_FORMATS).optional(),
    max_items: z.number().int().positive().optional(),
    detail: z.enum(['brief', 'normal', 'full', 'debug']).optional(),
  }).optional().default({}),
  context: z.object({
    context_id: z.string().optional(),
    task_run_id: z.string().optional(),
  }).optional(),
}).superRefine((input, ctx) => {
  const format = input.view?.format;
  const isLogicRead = new Set(['blueprint_logic', 'graph_context']).has(input.read_type);
  const isWidgetTreeRead = new Set(['widget_context']).has(input.read_type) && !input.target.target_name;
  if (!isLogicRead && !isWidgetTreeRead && format) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['view', 'format'],
      message: 'view.format is only supported for blueprint_logic, graph_context, or widget_context tree reads; omit it for this read_type.',
    });
  }
  if (new Set(['graph_context']).has(input.read_type) && format && format !== 'logic_json') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['view', 'format'],
      message: 'graph_context only supports logic_json format.',
    });
  }
  if (isWidgetTreeRead && format && !new Set(['tree_json', 'logic_flow']).has(format)) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['view', 'format'],
      message: 'widget_context tree reads only support tree_json and logic_flow formats.',
    });
  }
});

export type ReadContextInput = z.infer<typeof ReadContextInputSchema>;
