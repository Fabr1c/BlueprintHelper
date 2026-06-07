import {
  composeReadContextTemplate,
  composeTaskSpecTemplate,
  isToolCapabilityDomain,
  isToolCapabilityKind,
  listReadContextTemplateClusters,
  listReadContextTemplateDomains,
  listReadContextTemplateQuickAccess,
  listReadContextTemplateTargets,
  listReadContextTemplateViews,
  listTaskSpecTemplateClusters,
  listTaskSpecTemplateFamilies,
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
  listTaskSpecTemplateWriteModes,
  listToolCapabilities,
  listToolDomains,
  type ToolAudience,
  type ToolRisk,
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

  if (command.kind === 'tools.templates.families') {
    return listTaskSpecTemplateFamilies({
      workflow: command.workflow ?? 'preview_execute',
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.templates.write_modes') {
    return listTaskSpecTemplateWriteModes({
      family: required(command.family, 'Missing template family.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.templates.clusters') {
    return listTaskSpecTemplateClusters({
      family: required(command.family, 'Missing template family.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.templates.operations') {
    return listTaskSpecTemplateOperations({
      family: required(command.family, 'Missing template family.'),
      cluster: required(command.cluster, 'Missing template cluster.'),
      writeMode: required(command.writeMode, 'Missing template write mode.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.templates.quick_access') {
    return listTaskSpecTemplateQuickAccess({
      family: required(command.family, 'Missing template family.'),
      cluster: required(command.cluster, 'Missing template cluster.'),
      operation: required(command.operation, 'Missing template operation.'),
      writeMode: required(command.writeMode, 'Missing template write mode.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.templates.compose') {
    return composeTaskSpecTemplate({
      family: required(command.family, 'Missing template family.'),
      writeMode: required(command.writeMode, 'Missing template write mode.'),
      templateIds: command.templateIds ?? [],
      outputPath: required(command.outputPath, 'Missing template output path.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.read_templates.domains') {
    return listReadContextTemplateDomains() as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.read_templates.clusters') {
    return listReadContextTemplateClusters({
      domain: required(command.domain, 'Missing read template domain.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.read_templates.targets') {
    return listReadContextTemplateTargets({
      domain: required(command.domain, 'Missing read template domain.'),
      readCluster: required(command.readCluster, 'Missing read template cluster.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.read_templates.views') {
    return listReadContextTemplateViews({
      domain: required(command.domain, 'Missing read template domain.'),
      readCluster: required(command.readCluster, 'Missing read template cluster.'),
      targetKind: required(command.targetKind, 'Missing read template target kind.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.read_templates.quick_access') {
    return listReadContextTemplateQuickAccess({
      domain: required(command.domain, 'Missing read template domain.'),
      readCluster: required(command.readCluster, 'Missing read template cluster.'),
      targetKind: required(command.targetKind, 'Missing read template target kind.'),
      viewTemplate: required(command.viewTemplate, 'Missing read template view.'),
    }) as unknown as Record<string, unknown>;
  }

  if (command.kind === 'tools.read_templates.compose') {
    return composeReadContextTemplate({
      domain: required(command.domain, 'Missing read template domain.'),
      readCluster: required(command.readCluster, 'Missing read template cluster.'),
      targetKind: required(command.targetKind, 'Missing read template target kind.'),
      viewTemplate: required(command.viewTemplate, 'Missing read template view.'),
      templateIds: command.templateIds ?? [],
      outputPath: required(command.outputPath, 'Missing read template output path.'),
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

function required(value: string | undefined, message: string): string {
  if (!value) {
    throw new Error(message);
  }
  return value;
}
