import type { GraphWriteSlotDescriptor } from '../../task/compiler/graphwrite/graphwrite-slot-descriptor.js';

export function resolveSlotTemplateAlias(
  templateId: string,
  descriptors: readonly GraphWriteSlotDescriptor[],
): string {
  for (const descriptor of descriptors) {
    if (descriptor.quick_access.template_id === templateId) {
      return templateId;
    }
    if ((descriptor.aliases ?? []).includes(templateId)) {
      return descriptor.quick_access.template_id;
    }
  }
  return templateId;
}
