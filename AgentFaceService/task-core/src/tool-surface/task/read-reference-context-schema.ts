import { z } from 'zod';

export const ReadReferenceContextInputSchema = z.object({
  asset_path: z.string(),
  target_type: z.enum([
    'asset',
    'blueprint',
    'graph',
    'function',
    'event',
    'custom_event',
    'member_variable',
    'block',
    'widget',
    'data_table_row',
    'interface',
  ]).optional().default('asset'),
  target_name: z.string().optional(),
  graph_name: z.string().optional(),
  block_id: z.string().optional(),
  widget_name: z.string().optional(),
  row_name: z.string().optional(),
  interface_path: z.string().optional(),
  scope: z.enum([
    'safety_context',
    'dependencies',
    'referencers',
    'external_dependents',
    'all',
  ]).optional().default('safety_context'),
  max_results: z.number().int().positive().max(500).optional().default(50),
  include_samples: z.boolean().optional().default(true),
});

export type ReadReferenceContextInput = z.infer<typeof ReadReferenceContextInputSchema>;
