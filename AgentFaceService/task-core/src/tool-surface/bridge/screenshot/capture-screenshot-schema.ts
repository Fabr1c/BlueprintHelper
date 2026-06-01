import { z } from 'zod';

export const CaptureScreenshotTargetSchema = z.enum(['auto', 'active_window', 'active_viewport']);

export const CaptureScreenshotInputSchema = z.object({
  schema: z.literal('BlueprintHelper.CaptureScreenshotRequest.v1'),
  asset_path: z.string().min(1),
  graph_name: z.string().min(1).optional(),
  block_ref: z.string().min(1).optional(),
  node_ref: z.string().min(1).optional(),
  label: z.string().min(1).max(80).regex(/^[A-Za-z0-9_.-]+$/).optional(),
  capture_target: CaptureScreenshotTargetSchema.default('auto'),
  settle_delay_ms: z.number().int().min(0).max(5000).default(250),
}).strict().superRefine((value, ctx) => {
  if ((value.block_ref || value.node_ref) && !value.graph_name) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: value.block_ref ? ['block_ref'] : ['node_ref'],
      message: 'graph_name is required when block_ref or node_ref is provided.',
    });
  }
  if ((value.graph_name || value.block_ref || value.node_ref) && value.capture_target !== 'auto') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['capture_target'],
      message: 'capture_target must be auto for graph_name, block_ref, or node_ref screenshots; graph targets capture the Graph area only.',
    });
  }
});

export type CaptureScreenshotInput = z.infer<typeof CaptureScreenshotInputSchema>;
