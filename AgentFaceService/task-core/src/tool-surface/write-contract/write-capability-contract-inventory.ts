import { getGraphWriteRoutesForTemplateDiscovery } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';
import type { GraphWriteRouteDescriptor } from '../../task/compiler/graphwrite/graphwrite-route-descriptor.js';
import {
  getToolCapabilityDescriptor,
  listToolCapabilities,
  listToolDomains,
} from '../catalog/tool-capability-catalog.js';
import type { ToolCapabilityDomain, ToolCapabilityListItem } from '../catalog/tool-capability-types.js';
import { getBridgeToolDescriptor } from '../bridge/bridge-tool-descriptor.js';
import { getActiveReadContextRouteDescriptors } from '../templates/read-context-template-registry.js';
import {
  getSupportedNonGraphWriteTemplateFamilies,
  listNonGraphWriteValidationClassificationDescriptors,
} from '../templates/non-graphwrite-template-metadata.js';
import type { NonGraphWriteOperationDescriptor } from '../templates/non-graphwrite-operation-metadata.js';
import type {
  PreviewExecuteClassification,
  WriteCapabilityContract,
  WriteCapabilitySourceRef,
  WriteCapabilityVisibility,
} from './write-capability-contract-types.js';

const CONTRACT_SCHEMA = 'BlueprintHelper.WriteCapabilityContractAudit.v1' as const;

export function buildWriteCapabilityContractInventory(): readonly WriteCapabilityContract[] {
  const contracts = new Map<string, WriteCapabilityContract>();
  for (const contract of [
    ...buildCatalogWriteContracts(),
    ...buildGraphWriteRouteContracts(),
    ...buildNonGraphWriteOperationContracts(),
  ]) {
    contracts.set(contract.capability_id, contract);
  }
  return [...contracts.values()].sort((left, right) => left.capability_id.localeCompare(right.capability_id));
}

function buildCatalogWriteContracts(): WriteCapabilityContract[] {
  const domains = listToolDomains({ includeReserved: true }).items
    .map((domain) => domain.id)
    .filter((domain): domain is ToolCapabilityDomain => domain !== 'animation');
  const contracts: WriteCapabilityContract[] = [];
  for (const domain of domains) {
    const defaultItems = listToolCapabilities({ domain, kind: 'write' }).items;
    const expertItems = domain === 'review'
      ? listToolCapabilities({ domain, kind: 'write', audience: 'expert', expert: true }).items
      : [];
    for (const item of uniqueItemsById([...defaultItems, ...expertItems])) {
      contracts.push(catalogItemToContract(item));
    }
  }
  return contracts;
}

function catalogItemToContract(item: ToolCapabilityListItem): WriteCapabilityContract {
  const descriptor = getToolCapabilityDescriptor(item.id);
  const commandToolName = item.cli_command.replace(/^bh\s+/, '').split(/\s+/)[0];
  const toolName = descriptor?.tool_name ?? (commandToolName && commandToolName.length > 0 ? commandToolName : item.id);
  const bridgeDescriptor = getBridgeToolDescriptor(toolName);
  const stopConditions = descriptor?.stop_conditions ?? [];
  const routeRefs = descriptor?.route_refs ?? [];
  const sourceRefs: WriteCapabilitySourceRef[] = [
    {
      kind: item.id === 'review.write.apply_action' ? 'review_tool' : 'catalog',
      id: item.id,
      file: 'AgentFaceService/task-core/src/tool-surface/catalog/tool-capability-catalog.ts',
    },
  ];
  if (bridgeDescriptor) {
    sourceRefs.push({
      kind: 'bridge_tool',
      id: bridgeDescriptor.tool_name,
      file: 'AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-descriptor.ts',
    });
  }

  return makeContract({
    capabilityId: item.id,
    toolName,
    writeFamily: item.domain,
    visibility: item.audience === 'expert' ? 'developer_only' : 'active',
    sourceRefs,
    inputEvidence: {
      schema_refs: bridgeDescriptor?.schema ? [`bridge:${toolName}:zod_schema`] : taskSpecSchemaRefsForTool(toolName),
      template_refs: [...item.cli_template_ids],
      help_refs: [...(descriptor?.help_usage ?? [])],
      validator_refs: [],
    },
    previewExecute: {
      classification: routeRefs.length > 0 ? 'unknown' : 'preview_decidable',
      evidence: routeRefs.map((route) => `route_ref:${route}`),
      runtime_only_notes: [],
    },
    gates: {
      write_session_evidence: item.requires_write_session || stopConditions.includes('write_session_required')
        ? [`capability:${item.id}:requires_write_session`]
        : [],
      source_control_evidence: stopConditions
        .filter((condition) => isSourceControlStopCondition(condition))
        .map((condition) => `stop_condition:${condition}`),
      close_save_evidence: item.id === 'editor.write.asset.save' ? ['capability:editor.write.asset.save'] : [],
    },
    readback: {
      read_context_refs: readbackRefsForDomain(item.domain),
      verification_fields: verificationFieldsForDomain(item.domain),
      recipe_refs: [],
    },
    reviewDebug: {
      review_v2_evidence: item.id === 'review.write.apply_action' ? ['capability:review.write.apply_action'] : [],
      debug_bundle_evidence: [],
    },
    tests: {
      ts_tests: ['AgentFaceService/task-core/src/tool-surface/catalog/tool-capability-catalog.test.ts'],
      cli_tests: ['AgentFaceService/cli/src/cli/tools-command.test.ts'],
      ue_tests: [],
      e2e_refs: [],
    },
  });
}

function buildGraphWriteRouteContracts(): WriteCapabilityContract[] {
  return getGraphWriteRoutesForTemplateDiscovery().map((route) => graphWriteRouteToContract(route));
}

function graphWriteRouteToContract(route: GraphWriteRouteDescriptor): WriteCapabilityContract {
  return makeContract({
    capabilityId: `graphwrite.route.${route.route_id}`,
    toolName: 'blueprinthelper_execute_task',
    writeFamily: 'graph_write',
    visibility: 'active',
    sourceRefs: [{
      kind: 'graphwrite_route',
      id: route.route_id,
      file: 'AgentFaceService/task-core/src/task/compiler/graphwrite/graphwrite-route-registry.ts',
    }],
    taskType: route.task_type,
    routeId: route.route_id,
    runtimeAdapterId: route.runtime_adapter_id,
    inputEvidence: {
      schema_refs: ['BlueprintHelper.TaskSpec.v1'],
      template_refs: route.template_path ? [route.template_path] : [],
      help_refs: route.quick_access ? [`quick_access:${route.quick_access.template_id}`] : [],
      validator_refs: [],
    },
    previewExecute: {
      classification: toPreviewExecuteClassification(route.validation_classification),
      evidence: [
        `compiler:${route.compiler_id}`,
        `adapter_sync:${route.adapter_sync}`,
      ],
      runtime_only_notes: [...(route.runtime_only_validation_notes ?? [])],
    },
    gates: {
      write_session_evidence: ['capability:blueprint.write.taskspec.execute:requires_write_session'],
      source_control_evidence: [],
      close_save_evidence: [],
    },
    readback: {
      read_context_refs: readbackRefsForDomain('blueprint'),
      verification_fields: ['asset_path', 'target_name', 'graph_name'],
      recipe_refs: [],
    },
    reviewDebug: {
      review_v2_evidence: [],
      debug_bundle_evidence: [],
    },
    tests: {
      ts_tests: ['AgentFaceService/task-core/src/task/compiler/graphwrite/graphwrite-route-registry.test.ts'],
      cli_tests: [],
      ue_tests: [],
      e2e_refs: [],
    },
  });
}

function buildNonGraphWriteOperationContracts(): WriteCapabilityContract[] {
  const familyById = new Map(getSupportedNonGraphWriteTemplateFamilies().map((family) => [family.family, family]));
  return listNonGraphWriteValidationClassificationDescriptors().map((descriptor) => {
    const family = familyById.get(descriptor.family);
    return nonGraphWriteOperationToContract(descriptor, family?.task_type);
  });
}

function nonGraphWriteOperationToContract(
  descriptor: NonGraphWriteOperationDescriptor,
  taskType: string | undefined,
): WriteCapabilityContract {
  return makeContract({
    capabilityId: `non_graphwrite.operation.${descriptor.family}.${descriptor.cluster_id}.${descriptor.operation_id}`,
    toolName: 'blueprinthelper_execute_task',
    writeFamily: descriptor.family,
    visibility: 'active',
    sourceRefs: [{
      kind: 'non_graphwrite_operation',
      id: descriptor.template_id,
      file: 'AgentFaceService/task-core/src/tool-surface/templates/non-graphwrite-operation-metadata.ts',
    }],
    taskType,
    inputEvidence: {
      schema_refs: ['BlueprintHelper.TaskSpec.v1'],
      template_refs: [descriptor.template_path],
      help_refs: [`quick_access:${descriptor.template_id}`],
      validator_refs: [],
    },
    previewExecute: {
      classification: descriptor.validation_classification,
      evidence: [`operation:${descriptor.operation_id}`],
      runtime_only_notes: [...(descriptor.runtime_only_validation_notes ?? [])],
    },
    gates: {
      write_session_evidence: ['capability:blueprint.write.taskspec.execute:requires_write_session'],
      source_control_evidence: [],
      close_save_evidence: [],
    },
    readback: {
      read_context_refs: readbackRefsForFamily(descriptor.family),
      verification_fields: verificationFieldsForFamily(descriptor.family),
      recipe_refs: [],
    },
    reviewDebug: {
      review_v2_evidence: [],
      debug_bundle_evidence: [],
    },
    tests: {
      ts_tests: ['AgentFaceService/task-core/src/tool-surface/templates/non-graphwrite-template-metadata.test.ts'],
      cli_tests: [],
      ue_tests: [],
      e2e_refs: [],
    },
  });
}

function makeContract(input: {
  readonly capabilityId: string;
  readonly toolName: string;
  readonly writeFamily: string;
  readonly visibility: WriteCapabilityVisibility;
  readonly sourceRefs: readonly WriteCapabilitySourceRef[];
  readonly taskType?: string;
  readonly routeId?: string;
  readonly runtimeAdapterId?: string;
  readonly inputEvidence: WriteCapabilityContract['input_evidence'];
  readonly previewExecute: WriteCapabilityContract['preview_execute'];
  readonly gates: WriteCapabilityContract['gates'];
  readonly readback: WriteCapabilityContract['readback'];
  readonly reviewDebug: WriteCapabilityContract['review_debug'];
  readonly tests: WriteCapabilityContract['tests'];
}): WriteCapabilityContract {
  return {
    schema: CONTRACT_SCHEMA,
    capability_id: input.capabilityId,
    tool_name: input.toolName,
    write_family: input.writeFamily,
    visibility: input.visibility,
    source_refs: [...input.sourceRefs],
    task_type: input.taskType,
    route_id: input.routeId,
    runtime_adapter_id: input.runtimeAdapterId,
    input_evidence: cloneInputEvidence(input.inputEvidence),
    preview_execute: {
      classification: input.previewExecute.classification,
      evidence: [...input.previewExecute.evidence],
      runtime_only_notes: [...input.previewExecute.runtime_only_notes],
    },
    gates: {
      write_session_evidence: [...input.gates.write_session_evidence],
      source_control_evidence: [...input.gates.source_control_evidence],
      close_save_evidence: [...input.gates.close_save_evidence],
    },
    readback: {
      read_context_refs: [...input.readback.read_context_refs],
      verification_fields: [...input.readback.verification_fields],
      recipe_refs: [...input.readback.recipe_refs],
    },
    review_debug: {
      review_v2_evidence: [...input.reviewDebug.review_v2_evidence],
      debug_bundle_evidence: [...input.reviewDebug.debug_bundle_evidence],
    },
    tests: {
      ts_tests: [...input.tests.ts_tests],
      cli_tests: [...input.tests.cli_tests],
      ue_tests: [...input.tests.ue_tests],
      e2e_refs: [...input.tests.e2e_refs],
    },
  };
}

function cloneInputEvidence(
  evidence: WriteCapabilityContract['input_evidence'],
): WriteCapabilityContract['input_evidence'] {
  return {
    schema_refs: [...evidence.schema_refs],
    template_refs: [...evidence.template_refs],
    help_refs: [...evidence.help_refs],
    validator_refs: [...evidence.validator_refs],
  };
}

function uniqueItemsById(items: readonly ToolCapabilityListItem[]): ToolCapabilityListItem[] {
  return [...new Map(items.map((item) => [item.id, item])).values()];
}

function taskSpecSchemaRefsForTool(toolName: string): string[] {
  return toolName === 'blueprinthelper_execute_task' ? ['BlueprintHelper.TaskSpec.v1'] : [];
}

function isSourceControlStopCondition(condition: string): boolean {
  return condition.includes('source_control')
    || condition.includes('checkout')
    || condition === 'checked_out_by_other'
    || condition === 'not_editable';
}

function toPreviewExecuteClassification(
  classification: GraphWriteRouteDescriptor['validation_classification'],
): PreviewExecuteClassification {
  return classification ?? 'unknown';
}

function readbackRefsForFamily(family: string): string[] {
  if (family === 'umg_widget') return activeReadRoutes((route) => route.family === 'widget_blueprint');
  if (family === 'data_table') return activeReadRoutes((route) => route.family === 'data_table');
  if (family === 'object_properties' || family === 'asset_factory') {
    return activeReadRoutes((route) => route.family === 'data_asset' || route.family === 'blueprint');
  }
  if (family === 'material_graph') return activeReadRoutes((route) => route.family === 'material');
  if (family.startsWith('blueprint_')) return activeReadRoutes((route) => route.family === 'blueprint');
  return [];
}

function verificationFieldsForFamily(family: string): string[] {
  if (family === 'umg_widget') return ['asset_path', 'target_name'];
  if (family === 'data_table') return ['asset_path', 'row_name'];
  if (family === 'object_properties' || family === 'asset_factory') return ['asset_path', 'property_path'];
  if (family === 'material_graph') return ['asset_path', 'graph_name'];
  if (family.startsWith('blueprint_')) return ['asset_path', 'target_name'];
  return [];
}

function readbackRefsForDomain(domain: string): string[] {
  if (domain === 'umg') return activeReadRoutes((route) => route.family === 'widget_blueprint');
  if (domain === 'data') return activeReadRoutes((route) => route.family === 'data_asset' || route.family === 'data_table');
  if (domain === 'material') return activeReadRoutes((route) => route.family === 'material');
  if (domain === 'blueprint') return activeReadRoutes((route) => route.family === 'blueprint');
  return [];
}

function verificationFieldsForDomain(domain: string): string[] {
  if (domain === 'editor' || domain === 'project' || domain === 'review') return [];
  return ['asset_path'];
}

function activeReadRoutes(predicate: (route: ReturnType<typeof getActiveReadContextRouteDescriptors>[number]) => boolean): string[] {
  return getActiveReadContextRouteDescriptors()
    .filter(predicate)
    .map((route) => route.template_id);
}
