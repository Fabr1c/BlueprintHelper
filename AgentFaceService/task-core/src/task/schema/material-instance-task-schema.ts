import { z } from 'zod';

export const MaterialInstanceOperationSchema = z.discriminatedUnion('op', [
  z.object({
    op: z.literal('create_material_instance'),
    parent_material: z.string().min(1).optional(),
  }).strict(),
  z.object({
    op: z.literal('set_parent'),
    parent_material: z.string().min(1),
  }).strict(),
  z.object({
    op: z.literal('set_scalar_override'),
    parameter_name: z.string().min(1),
    value: z.number(),
  }).strict(),
  z.object({
    op: z.literal('set_vector_override'),
    parameter_name: z.string().min(1),
    value: z.object({
      r: z.number(),
      g: z.number(),
      b: z.number(),
      a: z.number().optional(),
    }).strict(),
  }).strict(),
  z.object({
    op: z.literal('set_texture_override'),
    parameter_name: z.string().min(1),
    texture_asset: z.string().min(1),
  }).strict(),
  z.object({
    op: z.literal('set_static_switch_override'),
    parameter_name: z.string().min(1),
    value: z.boolean(),
  }).strict(),
  z.object({
    op: z.literal('clear_override'),
    parameter_name: z.string().min(1),
    parameter_type: z.enum(['scalar', 'vector', 'texture', 'static_switch']).optional(),
  }).strict(),
  z.object({
    op: z.literal('read_parameter_schema'),
    parameter_name: z.string().min(1).optional(),
    parameter_type: z.enum(['scalar', 'vector', 'texture', 'static_switch']).optional(),
  }).strict(),
  z.object({
    op: z.literal('read_effective_value'),
    parameter_name: z.string().min(1),
    parameter_type: z.enum(['scalar', 'vector', 'texture', 'static_switch']).optional(),
  }).strict(),
]);

export const MaterialInstanceBehaviorSchema = z.object({
  material_instance_strategy: z.literal('material_instance_edit'),
  operations: z.array(MaterialInstanceOperationSchema).min(1),
}).strict();

export type MaterialInstanceOperation = z.infer<typeof MaterialInstanceOperationSchema>;
