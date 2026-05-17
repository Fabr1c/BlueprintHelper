import { z } from 'zod';

export const ReadFunctionChainContextInputSchema = z.object({
  asset_path: z.string().min(1),
  target_type: z.enum(['function', 'event', 'custom_event']),
  target_name: z.string().min(1),
  graph_name: z.string().min(1).optional(),
  max_depth: z.number().int().min(0).max(12).optional().default(3),
  include_data_dependencies: z.boolean().optional().default(true),
  expand_cross_asset: z.boolean().optional().default(true),
}).strict();

export type ReadFunctionChainContextInput = z.infer<typeof ReadFunctionChainContextInputSchema>;
