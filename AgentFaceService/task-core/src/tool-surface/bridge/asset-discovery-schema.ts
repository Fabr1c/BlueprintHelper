import { z } from 'zod';

export const FindAssetsSemanticTypeSchema = z.enum([
  'blueprint',
  'widget_blueprint',
  'data_table',
  'data_asset',
  'user_defined_struct',
]);

export const FindAssetsClassPathSchema = z.string()
  .regex(/^\/Script\/[^./]+\.[^./]+$/);

export const FindAssetsInputSchema = z.object({
  schema: z.literal('BlueprintHelper.FindAssetsRequest.v1'),
  query: z.string().optional(),
  path_prefixes: z.array(z.string().startsWith('/')).optional(),
  asset_types: z.array(FindAssetsSemanticTypeSchema).optional(),
  asset_classes: z.array(FindAssetsClassPathSchema).optional(),
  recursive: z.boolean().optional(),
  limit: z.number().int().min(1).max(100).optional(),
  include_plugin_content: z.boolean().optional(),
  include_engine_content: z.boolean().optional(),
  include_redirectors: z.boolean().optional(),
}).strict();

export type FindAssetsInput = z.infer<typeof FindAssetsInputSchema>;
