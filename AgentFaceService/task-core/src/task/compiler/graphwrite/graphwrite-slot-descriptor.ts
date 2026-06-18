import type { GraphWriteTemplateWriteMode } from './graphwrite-route-descriptor.js';

export type GraphWriteSlotType = 'statement' | 'expression';
export type GraphWriteSlotStatus = 'active' | 'planned' | 'hidden';
export type GraphWriteSlotInputAccepts = 'expression' | 'statement[]';

export interface GraphWriteQuickAccessDescriptor {
  template_id: string;
  family: 'graph_write';
  cluster_id: string;
  operation_id: string;
  quick_access_id: string;
  unsupported_write_modes?: GraphWriteTemplateWriteMode[];
}

export interface GraphWriteSlotInputDescriptor {
  index: number;
  name: string;
  path: string;
  accepts: GraphWriteSlotInputAccepts[];
  type_hint?: string;
}

export interface GraphWriteSlotDescriptor {
  slot_id: string;
  slot_type: GraphWriteSlotType;
  compiler_id: string;
  kind: string;
  template_path: string;
  insert_paths: string[];
  input_slots: GraphWriteSlotInputDescriptor[];
  supported_routes: string[];
  validation_hints: string[];
  keywords: string[];
  tags?: string[];
  quick_access: GraphWriteQuickAccessDescriptor;
  aliases?: string[];
  when_to_use: string;
  when_not_to_use?: string;
  status: GraphWriteSlotStatus;
}

export function isAgentVisibleGraphWriteSlot(slot: GraphWriteSlotDescriptor): boolean {
  return slot.status === 'active' && slot.template_path.length > 0 && slot.supported_routes.length > 0;
}
