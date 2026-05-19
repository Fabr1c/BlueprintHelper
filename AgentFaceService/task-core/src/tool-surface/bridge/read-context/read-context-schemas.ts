import { z } from 'zod';

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
    format: z.enum(['logic_md', 'logic_json']).optional(),
    max_items: z.number().int().positive().optional(),
    detail: z.enum(['brief', 'normal', 'full', 'debug']).optional(),
  }).optional().default({}),
  context: z.object({
    context_id: z.string().optional(),
    task_run_id: z.string().optional(),
  }).optional(),
}).superRefine((input, ctx) => {
  const format = input.view?.format;
  const isLogicRead = input.read_type === 'blueprint_logic' || input.read_type === 'graph_context';
  if (!isLogicRead && format) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['view', 'format'],
      message: 'view.format is only supported for blueprint_logic or graph_context; omit it for this read_type.',
    });
  }
  if (input.read_type === 'graph_context' && format && format !== 'logic_json') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['view', 'format'],
      message: 'graph_context only supports logic_json format.',
    });
  }
});

export type ReadContextInput = z.infer<typeof ReadContextInputSchema>;
