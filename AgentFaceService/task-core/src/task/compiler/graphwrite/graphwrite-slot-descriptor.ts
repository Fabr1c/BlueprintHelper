export type GraphWriteSlotType = 'statement' | 'expression';
export type GraphWriteSlotStatus = 'active' | 'planned' | 'hidden';

export interface GraphWriteSlotDescriptor {
  slot_id: string;
  slot_type: GraphWriteSlotType;
  compiler_id: string;
  kind: string;
  template_path: string;
  insert_paths: string[];
  supported_routes: string[];
  validation_hints: string[];
  keywords: string[];
  when_to_use: string;
  when_not_to_use?: string;
  status: GraphWriteSlotStatus;
}

export function isAgentVisibleGraphWriteSlot(slot: GraphWriteSlotDescriptor): boolean {
  return slot.status === 'active' && slot.template_path.length > 0 && slot.supported_routes.length > 0;
}
