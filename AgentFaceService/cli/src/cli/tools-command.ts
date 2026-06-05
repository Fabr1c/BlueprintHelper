import {
  buildReadonlyToolCommandManifestRegistry,
  createToolsTemplateBuilder,
  isToolCapabilityDomain,
  isToolCapabilityKind,
  listToolCapabilities,
  listToolDomains,
  type ToolAudience,
  type ToolRisk,
  type ToolTemplateSlotKind,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';
import type { CliCommand } from './output.js';

export function runToolsCommand(command: CliCommand): Record<string, unknown> {
  if (command.kind === 'tools.domains') {
    return listToolDomains({
      audience: command.audience ?? 'default',
      includeReserved: command.includeReserved === true,
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.list') {
    const domain = required(command.toolDomain, 'Missing tools list domain.');
    const kind = required(command.toolCatalogKind, 'Missing tools list kind.');
    if (!isToolCapabilityDomain(domain)) {
      throw new Error(`Unsupported BlueprintHelper tools domain: ${domain}`);
    }
    if (!isToolCapabilityKind(kind)) {
      throw new Error(`Unsupported BlueprintHelper tools kind: ${kind}`);
    }

    return listToolCapabilities({
      domain,
      kind,
      audience: command.audience ?? 'default',
      expert: command.expert,
      requiresBridge: command.requiresBridge,
      risks: command.risks?.map(parseToolRisk),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.templates') {
    return createToolsTemplateBuilder(buildReadonlyToolCommandManifestRegistry()).getTemplateDispatch(required(command.toolId, 'Missing tools template tool id.'), {
      route: command.routeId,
      slot: command.slot,
      slotKind: command.slotKind ? parseToolTemplateSlotKind(command.slotKind) : undefined,
    }) as unknown as Record<string, unknown>;
  }

  throw new Error(`Unsupported BlueprintHelper tools command: ${command.kind}`);
}

export function parseToolAudience(value: string): ToolAudience {
  if (value === 'default' || value === 'compat' || value === 'expert') {
    return value;
  }
  throw new Error(`Unsupported BlueprintHelper tools audience: ${value}`);
}

export function parseToolRisk(value: string): ToolRisk {
  if (value === 'none' || value === 'low' || value === 'medium' || value === 'high' || value === 'critical') {
    return value;
  }
  throw new Error(`Unsupported BlueprintHelper tools risk: ${value}`);
}

export function parseToolTemplateSlotKind(value: string): ToolTemplateSlotKind {
  if (
    value === 'statement'
    || value === 'expression'
    || value === 'target'
    || value === 'view'
    || value === 'patch'
    || value === 'merge'
  ) {
    return value;
  }
  throw new Error(`Unsupported BlueprintHelper template slot kind: ${value}`);
}

function required(value: string | undefined, message: string): string {
  if (!value) {
    throw new Error(message);
  }
  return value;
}
