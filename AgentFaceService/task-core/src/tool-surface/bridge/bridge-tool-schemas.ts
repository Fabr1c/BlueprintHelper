import type { z } from 'zod';
import { BRIDGE_TOOL_DESCRIPTORS } from './bridge-tool-descriptor.js';

export const bridgeToolSchemas: Record<string, z.ZodTypeAny> = Object.fromEntries(
  BRIDGE_TOOL_DESCRIPTORS
    .filter((descriptor) => descriptor.schema !== undefined)
    .map((descriptor) => [descriptor.tool_name, descriptor.schema as z.ZodTypeAny]),
);
