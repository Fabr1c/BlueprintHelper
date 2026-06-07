import {
  GRAPHWRITE_SLOT_MANIFEST,
} from './generated/graphwrite-slot-manifest.generated.js';
import {
  isAgentVisibleGraphWriteSlot,
  type GraphWriteSlotDescriptor,
  type GraphWriteSlotType,
} from './graphwrite-slot-descriptor.js';

const ALL_GRAPHWRITE_SLOTS: readonly GraphWriteSlotDescriptor[] = GRAPHWRITE_SLOT_MANIFEST;
const SLOTS_BY_ID = new Map(ALL_GRAPHWRITE_SLOTS.map((slot) => [slot.slot_id, slot]));

export function getAllGraphWriteSlotDescriptors(): readonly GraphWriteSlotDescriptor[] {
  return ALL_GRAPHWRITE_SLOTS;
}

export function getGraphWriteSlotsForRoute(
  routeId: string,
  slotType?: GraphWriteSlotType,
): readonly GraphWriteSlotDescriptor[] {
  return ALL_GRAPHWRITE_SLOTS.filter((slot) => {
    if (!isAgentVisibleGraphWriteSlot(slot)) {
      return false;
    }
    if (slotType && slot.slot_type !== slotType) {
      return false;
    }
    return slot.supported_routes.includes(routeId);
  });
}

export function requireGraphWriteSlotById(slotId: string): GraphWriteSlotDescriptor {
  const slot = SLOTS_BY_ID.get(slotId);
  if (!slot) {
    throw new Error(`Unknown GraphWrite slot descriptor id: ${slotId}`);
  }
  return slot;
}

export function getGraphWriteSlotsForTemplateDiscovery(
  routeId: string,
  slotType?: GraphWriteSlotType,
): readonly GraphWriteSlotDescriptor[] {
  return getGraphWriteSlotsForRoute(routeId, slotType);
}
