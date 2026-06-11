import { z } from 'zod';

const REFERENCE_CONTEXT_REQUEST_SCHEMA = 'BlueprintHelper.ReferenceContextRequest.v1';

const ReadReferenceContextRequestSchema = z.object({
  schema: z.literal(REFERENCE_CONTEXT_REQUEST_SCHEMA).optional(),
  asset_path: z.string(),
  target_type: z.enum([
    'asset',
    'blueprint',
    'graph',
    'function',
    'event',
    'custom_event',
    'member_variable',
    'local_variable',
    'event_dispatcher',
    'block',
    'widget',
    'data_table_row',
    'interface',
  ]).optional().default('asset'),
  target_name: z.string().optional(),
  graph_name: z.string().optional(),
  declaring_class_path: z.string().optional(),
  block_id: z.string().optional(),
  widget_name: z.string().optional(),
  row_name: z.string().optional(),
  interface_path: z.string().optional(),
  search_scope: z.enum(['asset', 'project']).optional().default('project'),
  resolution_policy: z.enum(['ue_then_name', 'ue_only', 'name_only']).optional().default('ue_then_name'),
  detail: z.enum(['summary', 'samples', 'full']).optional().default('samples'),
  max_results: z.number().int().positive().max(500).optional().default(50),
}).strict().superRefine((input, ctx) => {
  const memberTargetTypes = new Set([
    'function',
    'event',
    'custom_event',
    'member_variable',
    'local_variable',
    'event_dispatcher',
  ]);
  if (memberTargetTypes.has(input.target_type) && !input.target_name) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['target_name'],
      message: 'target_name is required for member-level reference context.',
    });
  }
  if (input.target_type === 'local_variable' && !input.graph_name) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['graph_name'],
      message: 'graph_name is required for local_variable reference context.',
    });
  }
});

export const ReadReferenceContextInputSchema = ReadReferenceContextRequestSchema.transform((input) => {
  const { schema: _schema, ...bridgeInput } = input;
  return bridgeInput;
});

export type ReadReferenceContextInput = z.infer<typeof ReadReferenceContextInputSchema>;
