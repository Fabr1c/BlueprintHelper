import {
  composeReadContextTemplate,
  composeTaskSpecTemplate,
  createRuntimeCapabilityState,
  isToolCapabilityDomain,
  isToolCapabilityKind,
  listReadContextTemplateClusters,
  listReadContextTemplateFamilies,
  listReadContextTemplates,
  listTaskSpecTemplateClusters,
  listTaskSpecTemplateFamilies,
  listTaskSpecTemplateOperations,
  listTaskSpecTemplateQuickAccess,
  listTaskSpecTemplateWriteModes,
  resolveCliCommandExecutorDescriptor,
  listToolCapabilities,
  listToolDomains,
  type CliCommandExecutorDescriptor,
  type RuntimeCapabilityState,
  type ToolAudience,
  type ToolRisk,
} from '@blueprinthelper/task-core/tool-surface/tool-registry';
import type { CliCommand } from './output.js';

interface ToolsCommandExecutor extends CliCommandExecutorDescriptor<CliCommand['kind']> {
  readonly id: string;
  readonly kind: CliCommand['kind'];
  readonly kinds: readonly CliCommand['kind'][];
  readonly run: (command: CliCommand) => Record<string, unknown>;
}

const TOOLS_COMMAND_EXECUTORS: readonly ToolsCommandExecutor[] = [
  {
    id: 'tools.domains',
    kind: 'tools.domains',
    kinds: ['tools.domains'],
    run: (command) => listToolDomains({
      audience: command.audience ?? 'default',
      includeReserved: command.includeReserved === true,
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.list',
    kind: 'tools.list',
    kinds: ['tools.list'],
    run: (command) => {
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
        runtime: runtimeStateFromCommand(command),
      }) as unknown as Record<string, unknown>;
    },
  },
  {
    id: 'tools.templates.families',
    kind: 'tools.templates.families',
    kinds: ['tools.templates.families'],
    run: (command) => listTaskSpecTemplateFamilies({
      workflow: command.workflow ?? 'preview_execute',
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.templates.write_modes',
    kind: 'tools.templates.write_modes',
    kinds: ['tools.templates.write_modes'],
    run: (command) => listTaskSpecTemplateWriteModes({
      family: required(command.family, 'Missing template family.'),
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.templates.clusters',
    kind: 'tools.templates.clusters',
    kinds: ['tools.templates.clusters'],
    run: (command) => listTaskSpecTemplateClusters({
      family: required(command.family, 'Missing template family.'),
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.templates.operations',
    kind: 'tools.templates.operations',
    kinds: ['tools.templates.operations'],
    run: (command) => listTaskSpecTemplateOperations({
      family: required(command.family, 'Missing template family.'),
      cluster: required(command.cluster, 'Missing template cluster.'),
      writeMode: required(command.writeMode, 'Missing template write mode.'),
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.templates.quick_access',
    kind: 'tools.templates.quick_access',
    kinds: ['tools.templates.quick_access'],
    run: (command) => listTaskSpecTemplateQuickAccess({
      family: required(command.family, 'Missing template family.'),
      cluster: required(command.cluster, 'Missing template cluster.'),
      operation: required(command.operation, 'Missing template operation.'),
      writeMode: required(command.writeMode, 'Missing template write mode.'),
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.templates.compose',
    kind: 'tools.templates.compose',
    kinds: ['tools.templates.compose'],
    run: (command) => composeTaskSpecTemplate({
      family: required(command.family, 'Missing template family.'),
      writeMode: required(command.writeMode, 'Missing template write mode.'),
      templateIds: command.templateIds ?? [],
      outputPath: required(command.outputPath, 'Missing template output path.'),
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.read_templates.families',
    kind: 'tools.read_templates.families',
    kinds: ['tools.read_templates.families'],
    run: () => listReadContextTemplateFamilies() as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.read_templates.clusters',
    kind: 'tools.read_templates.clusters',
    kinds: ['tools.read_templates.clusters'],
    run: (command) => listReadContextTemplateClusters({
      family: required(command.family, 'Missing read template family.'),
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.read_templates.list',
    kind: 'tools.read_templates.list',
    kinds: ['tools.read_templates.list'],
    run: (command) => listReadContextTemplates({
      family: required(command.family, 'Missing read template family.'),
      cluster: required(command.cluster, 'Missing read template cluster.'),
    }) as unknown as Record<string, unknown>,
  },
  {
    id: 'tools.read_templates.compose',
    kind: 'tools.read_templates.compose',
    kinds: ['tools.read_templates.compose'],
    run: (command) => composeReadContextTemplate({
      templateId: required(command.templateId, 'Missing read template id.'),
      outputPath: required(command.outputPath, 'Missing read template output path.'),
    }) as unknown as Record<string, unknown>,
  },
];

export function runToolsCommand(command: CliCommand): Record<string, unknown> {
  const executor = resolveCliCommandExecutorDescriptor(TOOLS_COMMAND_EXECUTORS, command.kind);
  if (!executor) {
    throw new Error(`Unsupported BlueprintHelper CLI registry command: ${command.kind}`);
  }
  return executor.run(command);
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

function runtimeStateFromCommand(command: CliCommand): RuntimeCapabilityState | undefined {
  if (command.runtimeCapabilityState) {
    return createRuntimeCapabilityState(command.runtimeCapabilityState);
  }
  if (!command.runtimeAdapterIds) {
    return undefined;
  }
  return createRuntimeCapabilityState({
    registered_runtime_adapter_ids: command.runtimeAdapterIds,
  });
}

function required(value: string | undefined, message: string): string {
  if (!value) {
    throw new Error(message);
  }
  return value;
}
