import { z } from 'zod';

export const CapabilityDescriptorSchema = z.object({
  schema: z.literal('BlueprintHelper.CapabilityDescriptor.v1'),
  id: z.string().min(1),
  family: z.string().min(1),
  operation: z.string().min(1),
  asset_kinds: z.array(z.string().min(1)).min(1),
  routing: z.object({
    cli_command: z.string().min(1),
    bridge_command: z.string().min(1).optional(),
    handler_id: z.string().min(1),
  }),
  preview: z.object({
    supported: z.boolean(),
    required_for_execute: z.boolean(),
  }),
  runtime: z.object({
    adapter_id: z.string().min(1),
    status: z.enum(['active', 'planned', 'blocked_until_p4']),
  }),
  review: z.object({
    evidence_adapter: z.string().min(1).optional(),
    restore_adapter: z.string().min(1).optional(),
    surface_adapter: z.string().min(1).optional(),
  }),
  debug: z.object({
    projection_adapter: z.string().min(1).optional(),
    export_projection: z.string().min(1).optional(),
  }),
  read_context: z.object({
    projection_adapter: z.string().min(1).optional(),
    route_refs: z.array(z.string().min(1)).default([]),
  }),
  ui: z.object({
    presenter_adapter: z.string().min(1).optional(),
    surface: z.string().min(1).optional(),
  }),
  safety: z.object({
    risk: z.enum(['none', 'low', 'medium', 'high']),
    reserved_only: z.boolean(),
    write_approval_required: z.boolean(),
  }),
});

export const RuntimeCapabilityStateSchema = z.object({
  registered_runtime_adapter_ids: z.array(z.string().min(1)).default([]),
  allow_write_capabilities: z.boolean().default(true),
  allow_high_risk_capabilities: z.boolean().default(true),
});

export type CapabilityDescriptor = z.infer<typeof CapabilityDescriptorSchema>;
export type RuntimeCapabilityState = z.infer<typeof RuntimeCapabilityStateSchema>;

