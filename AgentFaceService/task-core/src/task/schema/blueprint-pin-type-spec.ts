import { z } from 'zod';

const ScalarPinCategorySchema = z.enum([
  'exec',
  'bool',
  'boolean',
  'byte',
  'int',
  'int32',
  'int64',
  'float',
  'double',
  'real',
  'name',
  'string',
  'text',
]);

const ObjectBackedPinCategorySchema = z.enum([
  'object',
  'class',
  'soft_object',
  'soft_class',
  'interface',
  'struct',
  'enum',
]);

export const PinContainerTypeSchema = z.enum(['none', 'array', 'set', 'map']).optional();

export type BlueprintPinTypeSpec = {
  category: z.infer<typeof ScalarPinCategorySchema> | z.infer<typeof ObjectBackedPinCategorySchema>;
  sub_category?: string;
  object_path?: string;
  container_type?: 'none' | 'array' | 'set' | 'map';
  value_type?: BlueprintPinTypeSpec;
  is_reference?: boolean;
  is_const?: boolean;
};

export const BlueprintPinTypeSpecSchema: z.ZodType<BlueprintPinTypeSpec> = z.lazy(() =>
  z.object({
    category: z.union([ScalarPinCategorySchema, ObjectBackedPinCategorySchema]),
    sub_category: z.string().min(1).optional(),
    object_path: z.string().min(1).optional(),
    container_type: PinContainerTypeSchema,
    value_type: BlueprintPinTypeSpecSchema.optional(),
    is_reference: z.boolean().optional(),
    is_const: z.boolean().optional(),
  }).superRefine((value, ctx) => {
    const objectBacked = ObjectBackedPinCategorySchema.safeParse(value.category).success;
    if (objectBacked && !value.object_path) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['object_path'],
        message: `${value.category} pin_type requires object_path.`,
      });
    }

    const container = value.container_type ?? 'none';
    if (container !== 'map' && value.value_type) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['value_type'],
        message: 'pin_type.value_type is only valid when container_type is map.',
      });
    }

    if (container === 'map' && !value.value_type) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['value_type'],
        message: 'map pin_type requires value_type.',
      });
    }

    if (container === 'map' && value.value_type && (value.value_type.container_type ?? 'none') !== 'none') {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['value_type', 'container_type'],
        message: 'map pin_type.value_type must not be a container.',
      });
    }
  }),
);

export const SignatureDefaultValueSpecSchema = z.object({
  mode: z.enum(['literal']),
  value: z.string(),
});

export type SignatureDefaultValueSpec = z.infer<typeof SignatureDefaultValueSpecSchema>;

export const SignaturePinSpecSchema = z.object({
  name: z.string().min(1),
  pin_type: BlueprintPinTypeSpecSchema,
  default_value: SignatureDefaultValueSpecSchema.optional(),
}).passthrough();

export type SignaturePinSpec = z.infer<typeof SignaturePinSpecSchema>;

export function assertNoDuplicateSignaturePinNames(
  pins: Array<{ name: string }>,
  path: readonly (string | number)[],
): { path: Array<string | number>; message: string } | undefined {
  const names = new Set<string>();
  for (let index = 0; index < pins.length; index += 1) {
    const pin = pins[index];
    if (names.has(pin.name)) {
      return {
        path: [...path, index, 'name'],
        message: `Duplicate signature pin name: ${pin.name}.`,
      };
    }
    names.add(pin.name);
  }
  return undefined;
}
