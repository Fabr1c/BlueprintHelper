import { z } from 'zod';
import {
  SignaturePinSpecSchema,
  assertNoDuplicateSignaturePinNames,
} from './blueprint-pin-type-spec.js';
import {
  getGraphWriteRouteByScope,
  getGraphWriteRequiredFieldByStrategy,
} from '../compiler/graphwrite/graphwrite-route-registry.js';
import { UMG_WIDGET_OPERATION_MANIFEST } from '../../tool-surface/templates/generated/umg-widget-operation-manifest.generated.js';
import { MaterialInstanceBehaviorSchema } from './material-instance-task-schema.js';

export { GRAPHWRITE_CAPABILITY_CONTRACT } from './graphwrite-capability-contract.js';
export {
  GENERIC_OPS_EVIDENCE_KEYS,
  GENERIC_OPS_FORBIDDEN_RUNTIME_CLUSTER_IDS,
  GENERIC_OPS_OPERATION_GROUP_IDS,
  GENERIC_OPS_OPERATION_IDS,
  OP_COVERAGE_EVIDENCE_KEYS,
  OP_COVERAGE_EXCLUDED_OPERATION_IDS,
  OP_COVERAGE_SUPPORTED_OPERATION_IDS,
} from './graphwrite-capability-contract.js';
export type {
  GraphWriteCapabilityContract,
  GraphWriteClusterContract,
  GraphWriteEvidenceProjectionSource,
  GraphWriteLogicalRuntimeCluster,
  GraphWriteOperationContract,
  GraphWriteOperationGroupContract,
  GraphWriteOperationGroupOperation,
  GraphWriteReviewEvidencePolicy,
  GraphWriteRuntimeOwner,
  GraphWriteSupportStatus,
} from './graphwrite-capability-contract.js';

export const TASK_SPEC_SCHEMA = 'BlueprintHelper.TaskSpec.v1';
export const TASK_PLAN_SCHEMA = 'BlueprintHelper.TaskPlan.v1';
export const TASK_PREVIEW_SCHEMA = 'BlueprintHelper.TaskPreview.v1';
export const TASK_EXECUTION_SCHEMA = 'BlueprintHelper.TaskExecution.v1';
export const TASK_RUN_JOURNAL_SCHEMA = 'BlueprintHelper.TaskRunJournal.v1';
export const TASK_ERROR_SCHEMA = 'BlueprintHelper.TaskError.v1';
export const TASK_VERIFICATION_SCHEMA = 'BlueprintHelper.TaskVerification.v1';

export const CONTAINER_ACTION_OPERATIONS_BY_KIND = {
  array: [
    'get',
    'set',
    'add',
    'add_unique',
    'append',
    'insert',
    'remove_item',
    'remove_index',
    'clear',
    'contains',
    'find',
    'length',
    'shuffle',
    'shuffle_from_stream',
    'identical',
    'resize',
    'reverse',
    'is_empty',
    'is_not_empty',
    'last_index',
    'swap',
    'filter_array',
    'is_valid_index',
    'random',
    'random_from_stream',
    'sort_string',
    'sort_name',
    'sort_byte',
    'sort_int',
    'sort_int64',
    'sort_float',
  ],
  map: [
    'add',
    'remove',
    'find',
    'contains',
    'keys',
    'values',
    'clear',
    'length',
    'is_empty',
    'is_not_empty',
    'get_key_value_by_index',
    'get_last_index',
  ],
  set: [
    'add',
    'remove',
    'contains',
    'clear',
    'length',
    'to_array',
    'add_items',
    'remove_items',
    'is_empty',
    'is_not_empty',
    'intersection',
    'union',
    'difference',
    'get_item_by_index',
    'get_last_index',
  ],
} as const;

export const CONTAINER_ACTION_EXPRESSION_OPERATIONS_BY_KIND = {
  array: ['get', 'contains', 'find', 'length', 'identical', 'is_empty', 'is_not_empty', 'last_index', 'is_valid_index', 'random', 'random_from_stream'],
  map: ['find', 'contains', 'keys', 'values', 'length', 'is_empty', 'is_not_empty', 'get_key_value_by_index', 'get_last_index'],
  set: ['contains', 'length', 'to_array', 'is_empty', 'is_not_empty', 'get_item_by_index', 'get_last_index'],
} as const;

export const CONTAINER_ACTION_ROLE_FIELDS = [
  'target',
  'item',
  'items',
  'key',
  'value',
  'index',
  'other',
  'result',
  'size',
  'first_index',
  'second_index',
  'random_stream',
  'filter_class',
] as const;
export const CONTAINER_ACTION_TYPE_FIELDS = ['element_type', 'key_type', 'value_type'] as const;

export const CONTAINER_ACTION_REQUIRED_ROLES_BY_KIND_OPERATION = {
  array: {
    get: ['target', 'index'],
    set: ['target', 'index', 'item'],
    add: ['target', 'item'],
    add_unique: ['target', 'item'],
    append: ['target', 'items'],
    insert: ['target', 'index', 'item'],
    remove_item: ['target', 'item'],
    remove_index: ['target', 'index'],
    clear: ['target'],
    contains: ['target', 'item'],
    find: ['target', 'item'],
    length: ['target'],
    shuffle: ['target'],
    shuffle_from_stream: ['target', 'random_stream'],
    identical: ['target', 'items'],
    resize: ['target', 'size'],
    reverse: ['target'],
    is_empty: ['target'],
    is_not_empty: ['target'],
    last_index: ['target'],
    swap: ['target', 'first_index', 'second_index'],
    filter_array: ['target', 'filter_class'],
    is_valid_index: ['target', 'index'],
    random: ['target'],
    random_from_stream: ['target', 'random_stream'],
    sort_string: ['target'],
    sort_name: ['target'],
    sort_byte: ['target'],
    sort_int: ['target'],
    sort_int64: ['target'],
    sort_float: ['target'],
  },
  map: {
    add: ['target', 'key', 'value'],
    remove: ['target', 'key'],
    find: ['target', 'key'],
    contains: ['target', 'key'],
    keys: ['target'],
    values: ['target'],
    clear: ['target'],
    length: ['target'],
    is_empty: ['target'],
    is_not_empty: ['target'],
    get_key_value_by_index: ['target', 'index'],
    get_last_index: ['target'],
  },
  set: {
    add: ['target', 'item'],
    remove: ['target', 'item'],
    contains: ['target', 'item'],
    clear: ['target'],
    length: ['target'],
    to_array: ['target'],
    add_items: ['target', 'items'],
    remove_items: ['target', 'items'],
    is_empty: ['target'],
    is_not_empty: ['target'],
    intersection: ['target', 'other', 'result'],
    union: ['target', 'other', 'result'],
    difference: ['target', 'other', 'result'],
    get_item_by_index: ['target', 'index'],
    get_last_index: ['target'],
  },
} as const;

export const CONTAINER_ACTION_RESULT_OUTPUT_PIN_BY_KIND_OPERATION: {
  readonly [K in keyof typeof CONTAINER_ACTION_OPERATIONS_BY_KIND]: Partial<Record<(typeof CONTAINER_ACTION_OPERATIONS_BY_KIND)[K][number], string>>;
} = {
  array: {
    get: 'Item',
    add: 'ReturnValue',
    add_unique: 'ReturnValue',
    remove_item: 'ReturnValue',
    contains: 'ReturnValue',
    find: 'ReturnValue',
    length: 'ReturnValue',
    identical: 'ReturnValue',
    is_empty: 'ReturnValue',
    is_not_empty: 'ReturnValue',
    last_index: 'ReturnValue',
    is_valid_index: 'ReturnValue',
    random: 'OutItem',
    random_from_stream: 'OutItem',
  },
  map: {
    remove: 'ReturnValue',
    find: 'Value',
    contains: 'ReturnValue',
    keys: 'Keys',
    values: 'Values',
    length: 'ReturnValue',
    is_empty: 'ReturnValue',
    is_not_empty: 'ReturnValue',
    get_last_index: 'ReturnValue',
  },
  set: {
    remove: 'ReturnValue',
    contains: 'ReturnValue',
    length: 'ReturnValue',
    to_array: 'Result',
    is_empty: 'ReturnValue',
    is_not_empty: 'ReturnValue',
    get_item_by_index: 'Item',
    get_last_index: 'ReturnValue',
  },
} as const;

export const CONTAINER_ACTION_OPERATION_IDS = Object.entries(CONTAINER_ACTION_OPERATIONS_BY_KIND)
  .flatMap(([containerKind, operations]) => operations.map((operation) => `container.${containerKind}.${operation}`));

export function isSupportedContainerActionKind(value: string): value is keyof typeof CONTAINER_ACTION_OPERATIONS_BY_KIND {
  return Object.hasOwn(CONTAINER_ACTION_OPERATIONS_BY_KIND, value);
}

export function getSupportedContainerActionOperations(containerKind: string): readonly string[] {
  return isSupportedContainerActionKind(containerKind)
    ? CONTAINER_ACTION_OPERATIONS_BY_KIND[containerKind]
    : [];
}

export function isSupportedContainerActionOperation(containerKind: string, containerOperation: string): boolean {
  return getSupportedContainerActionOperations(containerKind).includes(containerOperation);
}

export function isExpressionContainerActionOperation(containerKind: string, containerOperation: string): boolean {
  return isSupportedContainerActionKind(containerKind)
    ? CONTAINER_ACTION_EXPRESSION_OPERATIONS_BY_KIND[containerKind].includes(containerOperation as never)
    : false;
}

export function getContainerActionResultOutputPin(containerKind: string, containerOperation: string): string | undefined {
  const kind = containerKind.trim().toLowerCase();
  const operation = containerOperation.trim().toLowerCase();
  if (!isSupportedContainerActionKind(kind)) {
    return undefined;
  }
  const outputPins = CONTAINER_ACTION_RESULT_OUTPUT_PIN_BY_KIND_OPERATION[kind] as Record<string, string | undefined>;
  return outputPins[operation];
}

export function isSingleResultContainerActionOperation(containerKind: string, containerOperation: string): boolean {
  return getContainerActionResultOutputPin(containerKind, containerOperation) !== undefined;
}

export function isValueExpressionContainerActionOperation(containerKind: string, containerOperation: string): boolean {
  return isExpressionContainerActionOperation(containerKind, containerOperation)
    && isSingleResultContainerActionOperation(containerKind, containerOperation);
}

export function getRequiredContainerActionRoles(
  containerKind: string,
  containerOperation: string,
): readonly (typeof CONTAINER_ACTION_ROLE_FIELDS)[number][] {
  if (!isSupportedContainerActionKind(containerKind)) {
    return [];
  }
  return CONTAINER_ACTION_REQUIRED_ROLES_BY_KIND_OPERATION[containerKind][containerOperation as keyof typeof CONTAINER_ACTION_REQUIRED_ROLES_BY_KIND_OPERATION[typeof containerKind]] ?? [];
}

export const ContainerActionShapeSchema = z.object({
  kind: z.literal('container_action'),
  container_kind: z.string().min(1),
  container_operation: z.string().min(1),
  target: z.unknown().optional(),
  item: z.unknown().optional(),
  items: z.unknown().optional(),
  key: z.unknown().optional(),
  value: z.unknown().optional(),
  index: z.unknown().optional(),
  other: z.unknown().optional(),
  result: z.unknown().optional(),
  size: z.unknown().optional(),
  first_index: z.unknown().optional(),
  second_index: z.unknown().optional(),
  random_stream: z.unknown().optional(),
  filter_class: z.unknown().optional(),
  element_type: z.string().min(1).optional(),
  key_type: z.string().min(1).optional(),
  value_type: z.string().min(1).optional(),
  result_symbol: z.string().min(1).optional(),
  context_evidence: z.record(z.unknown()).optional(),
}).passthrough().superRefine((value, ctx) => {
  const containerKind = value.container_kind.trim().toLowerCase();
  const containerOperation = value.container_operation.trim().toLowerCase();
  if (!isSupportedContainerActionKind(containerKind)) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['container_kind'],
      message: 'Use array, map, or set.',
    });
    return;
  }
  if (!isSupportedContainerActionOperation(containerKind, containerOperation)) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['container_operation'],
      message: 'Use a first-class V1 container operation.',
    });
    return;
  }
  getRequiredContainerActionRoles(containerKind, containerOperation).forEach((role) => {
    if (value[role] === undefined) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: [role],
        message: `container_action ${containerKind}.${containerOperation} requires ${role}.`,
      });
    }
  });
  if (value.result_symbol !== undefined && !isValueExpressionContainerActionOperation(containerKind, containerOperation)) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['result_symbol'],
      message: 'result_symbol is only supported for query container_action operations with a single result output.',
    });
  }
});

const LiteralValueExprSchema = z.object({
  kind: z.literal('literal'),
  value_type: z.string(),
  value: z.unknown(),
}).passthrough();

export const ValueExprSchema = z.union([
  LiteralValueExprSchema,
  z.object({ kind: z.string() }).passthrough(),
]);

const GraphWriteResolutionPolicySchema = z.enum(['default', 'auto_search']).optional();

const GraphWriteActionSelectionSchema = z.object({
  candidate_id: z.string().regex(/^preview:[A-Za-z0-9_-]+:[A-Za-z0-9_.:-]+:\d+$/),
}).passthrough();

export const BlueprintLogicStatementSchema = z.object({
  kind: z.string(),
  resolution_policy: GraphWriteResolutionPolicySchema,
  action_selection: GraphWriteActionSelectionSchema.optional(),
}).passthrough().superRefine((value, ctx) => {
  if (value.kind === 'action') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['kind'],
      message: 'kind="action" is not an Agent-facing GraphWrite statement kind; use kind="call" with resolution_policy="auto_search" for broad callable search.',
    });
  }
  if (value.action_selection && value.kind !== 'call') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['action_selection'],
      message: 'action_selection is supported only on kind="call" statements.',
    });
  }
  if (value.resolution_policy && value.kind !== 'call') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['resolution_policy'],
      message: 'resolution_policy is supported only on kind="call" statements.',
    });
  }
});

export const BlueprintLogicSpecSchema = z.object({
  schema: z.union([z.literal('BlueprintLogicSpec.v1'), z.literal('BlueprintLogicSpec.v2')]),
  statements: z.array(BlueprintLogicStatementSchema),
}).passthrough();

const TaskVerificationRequirementSchema = z.object({
  id: z.string().min(1),
  fact: z.string().min(1),
  target: z.record(z.unknown()).optional(),
  expected: z.unknown().optional(),
  operator: z.enum(['equals', 'contains', 'exists', 'not_exists', 'matches']).optional(),
  source_evidence: z.object({
    read_context_id: z.string().min(1).optional(),
    fingerprint: z.string().min(1).optional(),
    asset_path: z.string().min(1).optional(),
  }).strict().optional(),
}).strict().superRefine((value, ctx) => {
  if (!Object.hasOwn(value, 'expected') && value.operator === undefined) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['expected'],
      message: 'verification.requirements[] must provide expected or operator.',
    });
  }
});

export const TaskVerificationContractSchema = z.object({
  schema: z.literal(TASK_VERIFICATION_SCHEMA),
  mode: z.enum(['required', 'advisory']).optional().default('required'),
  requirements: z.array(TaskVerificationRequirementSchema).min(1),
}).strict();

const TaskSpecBaseSchema = z.object({
  schema: z.literal(TASK_SPEC_SCHEMA),
  context_id: z.string().optional(),
  task_type: z.string(),
  feature_name: z.string().optional(),
  target: z.object({
    asset_path: z.string().min(1),
    target_type: z.string().optional().default('blueprint'),
  }).passthrough(),
  verification: TaskVerificationContractSchema.optional(),
}).passthrough();

function rejectAgentFacingTaskSpecPolicyFields(value: unknown, ctx: z.RefinementCtx): void {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return;
  }
  for (const field of ['execution_policy', 'validation'] as const) {
    if (!Object.hasOwn(value, field)) continue;
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: [field],
      message: `${field} is an internal runtime policy namespace and is not accepted in Agent-facing TaskSpec.`,
    });
  }
}

function agentFacingTaskSpecSchema(schema: z.ZodTypeAny): z.ZodTypeAny {
  return schema.superRefine((value, ctx) => {
    rejectAgentFacingTaskSpecPolicyFields(value, ctx);
  });
}

const GraphWriteAppendEntrySchema = z.object({
  entry_type: z.string(),
  name: z.string().min(1),
  event_kind: z.enum([
    'custom_event',
    'override_event',
    'component_bound_event',
    'input_action_event',
    'dispatcher_event',
  ]).optional(),
  catalog_evidence: z.object({
    source: z.enum(['signature', 'graph_action_catalog']),
    signature_evidence_id: z.string().min(1).optional(),
    action_stable_id: z.string().min(1).optional(),
    context_fingerprint: z.string().min(1).optional(),
  }).strict().superRefine((value, ctx) => {
    if (value.source === 'signature' && !value.signature_evidence_id) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['signature_evidence_id'],
        message: 'catalog_evidence.source="signature" requires signature_evidence_id.',
      });
    }
    if (value.source === 'graph_action_catalog' && !value.action_stable_id) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['action_stable_id'],
        message: 'catalog_evidence.source="graph_action_catalog" requires action_stable_id.',
      });
    }
    if (value.source === 'graph_action_catalog' && !value.context_fingerprint) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['context_fingerprint'],
        message: 'catalog_evidence.source="graph_action_catalog" requires context_fingerprint.',
      });
    }
  }).optional(),
  body: BlueprintLogicSpecSchema,
}).passthrough().superRefine((value, ctx) => {
  const eventKind = value.event_kind ?? 'custom_event';
  if (eventKind !== 'custom_event' && !value.catalog_evidence) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['catalog_evidence'],
      message: `${eventKind} append entries require catalog_evidence.`,
    });
  }
});

const GraphWriteReplaceSchema = z.object({
  scope: z.string().min(1),
  selector: z.object({
    kind: z.string().min(1),
    name: z.string().min(1).optional(),
    block_id: z.string().min(1).optional(),
    graph_id: z.string().min(1).optional(),
    node_ref: z.string().min(1).optional(),
    node_path: z.string().min(1).optional(),
  }).passthrough(),
  inputs: z.array(z.record(z.unknown())).optional(),
  body: BlueprintLogicSpecSchema,
  options: z.object({
    strict: z.boolean().optional(),
  }).passthrough().optional(),
}).passthrough().superRefine((value, ctx) => {
  const route = getGraphWriteRouteByScope('replace_owned_graph', value.scope);
  if (!route || route.status === 'planned') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope'],
      message: route?.status === 'planned'
        ? `${value.scope} is planned and is not available to TaskSpec authoring yet.`
        : `Unsupported replace scope "${value.scope}".`,
    });
    return;
  }
  const selector = route.selector;
  if (!selector) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope'],
      message: `${value.scope} is missing GraphWrite selector metadata.`,
    });
    return;
  }
  const expectedKind = selector.expected_kind;
  if (value.selector.kind !== expectedKind) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['selector', 'kind'],
      message: `${value.scope} requires selector.kind="${expectedKind}".`,
    });
  }
  for (const field of selector.required_fields) {
    if (field === 'name') {
      if (!value.selector.name) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['selector', 'name'],
          message: `${value.scope} requires selector.name.`,
        });
      }
    } else if (field === 'block_id') {
      if (!value.selector.block_id) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['selector', 'block_id'],
          message: `${value.scope} requires selector.block_id.`,
        });
      }
    } else if (typeof value.selector[field] !== 'string' || value.selector[field].length === 0) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['selector', field],
        message: `${value.scope} requires selector.${field}.`,
      });
    }
  }
});

function requireGraphWritePatchRefString(
  ctx: z.RefinementCtx,
  ref: Record<string, unknown> | undefined,
  path: readonly string[],
  field: string,
): string | undefined {
  const value = ref?.[field];
  if (typeof value === 'string' && value.length > 0) {
    return value;
  }
  ctx.addIssue({
    code: z.ZodIssueCode.custom,
    path: [...path, field],
    message: `${path.join('.')}.${field} is required.`,
  });
  return undefined;
}

function rejectRedundantGraphWriteEndpointBlockId(
  ctx: z.RefinementCtx,
  ref: Record<string, unknown> | undefined,
  path: string,
): void {
  if (ref && Object.hasOwn(ref, 'block_id')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: [path, 'block_id'],
      message: `${path}.block_id is redundant; the compiler derives it from target_ref.block_id.`,
    });
  }
}

const GraphWritePatchSchema = z.object({
  kind: z.enum([
    'set_pin_default',
    'set_node_comment',
    'connect_pins',
    'disconnect_link',
    'replace_link',
    'delete_owned_node',
  ]),
  scope: z.string().min(1).optional(),
  target_ref: z.record(z.unknown()),
  source_ref: z.record(z.unknown()).optional(),
  replacement_ref: z.record(z.unknown()).optional(),
  delete_policy: z.record(z.unknown()).optional(),
  value: z.unknown().optional(),
  expected_old_state: z.record(z.unknown()).optional(),
}).passthrough().superRefine((value, ctx) => {
  const expectedScopeByKind: Record<string, string> = {
    set_pin_default: 'pin_default',
    set_node_comment: 'node_comment',
    connect_pins: 'connect_pins',
    disconnect_link: 'disconnect_link',
    replace_link: 'replace_link',
    delete_owned_node: 'node_delete',
  };
  const expectedScope = expectedScopeByKind[value.kind];
  if (value.scope && value.scope !== expectedScope) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope'],
      message: `${value.kind} requires scope ${expectedScope}; omit scope or set it to ${expectedScope}.`,
    });
  }

  const targetRef = value.target_ref as Record<string, unknown>;
  const sourceRef = value.source_ref as Record<string, unknown> | undefined;
  const replacementRef = value.replacement_ref as Record<string, unknown> | undefined;
  const deletePolicy = value.delete_policy as Record<string, unknown> | undefined;

  requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'block_id');
  requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'node_ref');

  switch (value.kind) {
    case 'set_pin_default':
      requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'pin_ref');
      if (!Object.hasOwn(value, 'value')) {
        ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['value'], message: `${value.kind} requires value.` });
      }
      break;
    case 'set_node_comment':
      if (!Object.hasOwn(value, 'value')) {
        ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['value'], message: `${value.kind} requires value.` });
      }
      break;
    case 'connect_pins':
      requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'pin_ref');
      rejectRedundantGraphWriteEndpointBlockId(ctx, sourceRef, 'source_ref');
      requireGraphWritePatchRefString(ctx, sourceRef, ['source_ref'], 'node_ref');
      requireGraphWritePatchRefString(ctx, sourceRef, ['source_ref'], 'pin_ref');
      if (value.expected_old_state !== undefined) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['expected_old_state'],
          message: 'connect_pins does not support expected_old_state; use read_context refs and rely on preview/runtime link compatibility checks.',
        });
      }
      break;
    case 'disconnect_link':
      requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'pin_ref');
      requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'link_ref');
      if (value.expected_old_state !== undefined) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['expected_old_state'],
          message: 'disconnect_link does not support expected_old_state; use read_context target_ref.link_ref for the current source and target endpoints.',
        });
      }
      break;
    case 'replace_link':
      requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'pin_ref');
      requireGraphWritePatchRefString(ctx, targetRef, ['target_ref'], 'link_ref');
      rejectRedundantGraphWriteEndpointBlockId(ctx, replacementRef, 'replacement_ref');
      requireGraphWritePatchRefString(ctx, replacementRef, ['replacement_ref'], 'node_ref');
      requireGraphWritePatchRefString(ctx, replacementRef, ['replacement_ref'], 'pin_ref');
      if (value.expected_old_state !== undefined) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['expected_old_state'],
          message: 'replace_link does not support expected_old_state; use read_context target_ref.link_ref and replacement_ref only.',
        });
      }
      break;
    case 'delete_owned_node':
      if (value.expected_old_state !== undefined) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['expected_old_state'],
          message: 'delete_owned_node does not support expected_old_state; use target_ref plus delete_policy only.',
        });
      }
      if (deletePolicy?.break_links === false) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['delete_policy', 'break_links'],
          message: 'delete_owned_node requires delete_policy.break_links=true.',
        });
      }
      if (deletePolicy?.allow_entry_node === true) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['delete_policy', 'allow_entry_node'],
          message: 'delete_owned_node does not allow delete_policy.allow_entry_node=true.',
        });
      }
      if (deletePolicy?.allow_lifecycle_root === true) {
        ctx.addIssue({
          code: z.ZodIssueCode.custom,
          path: ['delete_policy', 'allow_lifecycle_root'],
          message: 'delete_owned_node does not allow delete_policy.allow_lifecycle_root=true.',
        });
      }
      break;
  }
});

const GraphWriteMergeSchema = z.object({
  kind: z.literal('insert_flow'),
  scope: z.enum(['owned_block_call', 'custom_event_call', 'function_call']),
  insert_strategy: z.enum(['append_after', 'insert_between', 'branch_fork']),
  anchor: z.record(z.unknown()),
  inserted: z.record(z.unknown()),
  sequence_order: z.array(z.string()).optional(),
}).passthrough().superRefine((value, ctx) => {
  if (typeof value.anchor.node_ref !== 'string' || value.anchor.node_ref.length === 0) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['anchor', 'node_ref'], message: 'anchor.node_ref is required.' });
  }
  if (typeof value.anchor.pin_ref !== 'string' || value.anchor.pin_ref.length === 0) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['anchor', 'pin_ref'], message: 'anchor.pin_ref is required.' });
  }
  if (value.inserted.call_kind !== value.scope) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['inserted', 'call_kind'], message: `inserted.call_kind must be ${value.scope}.` });
  }
  if (value.scope === 'owned_block_call') {
    if (typeof value.inserted.block_id !== 'string' || value.inserted.block_id.length === 0) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['inserted', 'block_id'], message: 'owned_block_call requires inserted.block_id.' });
    }
  } else if (typeof value.inserted.name !== 'string' || value.inserted.name.length === 0) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['inserted', 'name'], message: `${value.scope} requires inserted.name.` });
  }
  if (value.insert_strategy === 'branch_fork') {
    if (!value.sequence_order || value.sequence_order.length === 0) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order'], message: 'branch_fork requires sequence_order.' });
    } else {
      value.sequence_order.forEach((entry, index) => {
        if (entry !== 'inserted_logic' && entry !== 'original_successor') {
          ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order', index], message: 'Use inserted_logic or original_successor.' });
        }
      });
    }
  } else if (value.sequence_order !== undefined) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order'], message: 'sequence_order is only valid for branch_fork.' });
  }
});

const ExternalGraphAnchorSchema = z.object({
  schema: z.literal('BlueprintHelper.ExternalGraphAnchor.v1'),
  asset_path: z.string().min(1),
  graph_name: z.string().min(1),
  node_guid: z.string().min(1),
  node_class: z.string().min(1),
  pin_name: z.string().min(1).optional(),
  pin_direction: z.enum(['input', 'output']).optional(),
  semantic_role: z.enum(['exec_boundary', 'node', 'body_entry']),
  fingerprint: z.string().min(1),
}).strict();

const ExternalCompactAnchorSchema = z.object({
  anchor_type: z.enum(['external_node', 'external_pin', 'external_link', 'external_body']),
  anchor_ref: z.string().min(1),
}).strict().superRefine((value, ctx) => {
  const prefixByType: Record<string, string> = {
    external_node: 'xnode:v1:',
    external_pin: 'xpin:v1:',
    external_link: 'xlink:v1:',
    external_body: 'xbody:v1:',
  };
  const expectedPrefix = prefixByType[value.anchor_type];
  if (!value.anchor_ref.startsWith(expectedPrefix)) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['anchor_ref'],
      message: `${value.anchor_type} requires compact anchor_ref prefix ${expectedPrefix}.`,
    });
  }
  if (/^(nodes|pins|links)\[\d+\]$/u.test(value.anchor_ref.trim())) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['anchor_ref'],
      message: 'Read-view refs such as links[n] are display-only and cannot be used for external graph writes.',
    });
  }
});

const ExternalCompactPinAnchorSchema = ExternalCompactAnchorSchema.superRefine((value, ctx) => {
  if (value.anchor_type !== 'external_pin') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['anchor_type'],
      message: 'external link connect/replace endpoints require anchor_type="external_pin".',
    });
  }
});

const ExternalCompactNodeAnchorSchema = ExternalCompactAnchorSchema.superRefine((value, ctx) => {
  if (value.anchor_type !== 'external_node') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['anchor_type'],
      message: 'external node patches require anchor_type="external_node".',
    });
  }
});

const ExternalCompactLinkAnchorSchema = ExternalCompactAnchorSchema.superRefine((value, ctx) => {
  if (value.anchor_type !== 'external_link') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['anchor_type'],
      message: 'external link disconnect/replace requires anchor_type="external_link".',
    });
  }
});

const ExternalExecBoundaryAnchorSchema = ExternalGraphAnchorSchema.superRefine((value, ctx) => {
  if (value.semantic_role !== 'exec_boundary') {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['semantic_role'], message: 'merge_external_flow requires semantic_role="exec_boundary".' });
  }
  if (!value.pin_name) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['pin_name'], message: 'exec_boundary external anchors require pin_name.' });
  }
  if (value.pin_direction !== 'output') {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['pin_direction'], message: 'exec_boundary external anchors require pin_direction="output".' });
  }
});

const LogicJsonAnchorSelectorSchema = z.object({
  schema: z.literal('BlueprintHelper.LogicJsonAnchorSelector.v1'),
  asset_path: z.string().min(1),
  graph_name: z.string().min(1).optional(),
  graph: z.string().min(1).optional(),
  entry_name: z.string().min(1).optional(),
  node_ref: z.string().min(1).optional(),
  link_ref: z.string().min(1).optional(),
  pin_ref: z.string().min(1).optional(),
}).strict().superRefine((value, ctx) => {
  if (!value.graph_name && !value.graph) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['graph_name'],
      message: 'LogicJson anchor selector requires graph_name or graph.',
    });
  }
  if (value.graph_name && value.graph && value.graph_name !== value.graph) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['graph'],
      message: 'LogicJson anchor selector graph and graph_name must match when both are provided.',
    });
  }
  const hasNodeRef = typeof value.node_ref === 'string' && value.node_ref.length > 0;
  const hasLinkRef = typeof value.link_ref === 'string' && value.link_ref.length > 0;
  if (hasNodeRef === hasLinkRef) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['node_ref'],
      message: 'LogicJson anchor selector requires exactly one of node_ref or link_ref.',
    });
  }
  if (hasNodeRef && !value.pin_ref) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['pin_ref'],
      message: 'LogicJson node_ref selector requires pin_ref for merge_external_flow.',
    });
  }
});

const ExternalNodeAnchorSchema = ExternalGraphAnchorSchema.superRefine((value, ctx) => {
  if (value.semantic_role !== 'node') {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['semantic_role'], message: 'patch_external_graph requires semantic_role="node".' });
  }
});

const ExternalBodyEntryAnchorSchema = ExternalGraphAnchorSchema.superRefine((value, ctx) => {
  if (value.semantic_role !== 'body_entry') {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['semantic_role'], message: 'replace_external_body requires semantic_role="body_entry".' });
  }
});

const ExternalGraphWriteInsertedBodySchema = z.object({
  schema: z.union([z.literal('BlueprintLogicSpec.v1'), z.literal('BlueprintLogicSpec.v2')]),
  statements: z.array(BlueprintLogicStatementSchema),
}).passthrough();

const GraphWriteExternalMergeSchema = z.object({
  kind: z.literal('insert_external_flow'),
  insert_strategy: z.enum(['append_after', 'insert_between', 'branch_fork']),
  anchor: z.union([ExternalExecBoundaryAnchorSchema, LogicJsonAnchorSelectorSchema, ExternalCompactLinkAnchorSchema]),
  inserted: z.object({
    body: ExternalGraphWriteInsertedBodySchema,
  }).passthrough(),
  sequence_order: z.array(z.enum(['inserted_logic', 'original_successor'])).optional(),
}).passthrough().superRefine((value, ctx) => {
  if (
    value.anchor &&
    'anchor_type' in value.anchor &&
    value.anchor.anchor_type === 'external_link' &&
    value.insert_strategy !== 'insert_between'
  ) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['insert_strategy'],
      message: 'external_link anchors are only valid for merge_external_flow insert_strategy="insert_between".',
    });
  }
  if (value.insert_strategy === 'branch_fork') {
    if (!value.sequence_order || value.sequence_order.length === 0) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order'], message: 'branch_fork requires sequence_order.' });
      return;
    }
    const uniqueEntries = new Set(value.sequence_order);
    if (value.sequence_order.length > 2 || uniqueEntries.size !== value.sequence_order.length || !uniqueEntries.has('inserted_logic')) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['sequence_order'],
        message: 'branch_fork sequence_order must contain unique entries and include inserted_logic.',
      });
    }
  } else if (value.sequence_order !== undefined) {
    ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['sequence_order'], message: 'sequence_order is only valid for branch_fork.' });
  }
});

const GraphWriteExternalPatchSchema = z.object({
  kind: z.enum(['set_external_pin_default', 'set_external_node_comment', 'set_external_node_property']),
  anchor: z.union([ExternalNodeAnchorSchema, ExternalCompactNodeAnchorSchema]),
  property_descriptor_id: z.enum(['k2.node.comment', 'k2.call.function_target', 'k2.field.member_reference']).optional(),
  value: z.unknown(),
  expected_old_state: z.record(z.unknown()),
}).strict().superRefine((value, ctx) => {
  const isCompactAnchor = 'anchor_type' in value.anchor;
  const hasPinName = !isCompactAnchor && 'pin_name' in value.anchor && Boolean(value.anchor.pin_name);
  if (value.kind === 'set_external_pin_default' && !hasPinName) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['anchor', 'pin_name'],
      message: 'set_external_pin_default requires anchor.pin_name.',
    });
  }
  if (value.kind === 'set_external_node_property' && !value.property_descriptor_id) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['property_descriptor_id'],
      message: 'set_external_node_property requires property_descriptor_id.',
    });
  }
  if (value.kind !== 'set_external_node_property' && value.property_descriptor_id) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['property_descriptor_id'],
      message: 'property_descriptor_id is only valid for set_external_node_property.',
    });
  }
});

const GraphWriteExternalLinkPatchSchema = z.discriminatedUnion('kind', [
  z.object({
    kind: z.literal('connect_pins'),
    source: ExternalCompactPinAnchorSchema,
    target: ExternalCompactPinAnchorSchema,
  }).strict(),
  z.object({
    kind: z.literal('disconnect_link'),
    anchor: ExternalCompactLinkAnchorSchema,
  }).strict(),
  z.object({
    kind: z.literal('replace_link'),
    anchor: ExternalCompactLinkAnchorSchema,
    replacement: ExternalCompactPinAnchorSchema,
  }).strict(),
]);

const GraphWriteExternalReplaceBodySchema = z.object({
  scope: z.enum(['custom_event_body', 'event_body', 'function_body']),
  anchor: ExternalBodyEntryAnchorSchema,
  body: BlueprintLogicSpecSchema,
  expected_body_fingerprint: z.string().min(1),
  require_full_dry_run: z.literal(true),
}).strict();

const ExternalMutationPolicySchema = z.object({
  strategy: z.enum(['merge_external_flow', 'patch_external_graph', 'patch_external_links', 'replace_external_body']),
  allowed_mutations: z.array(z.string().min(1)).min(1),
}).passthrough();

function validateExactExternalMutationPolicy(input: {
  strategy: string;
  scopePolicy: Record<string, unknown>;
  ctx: z.RefinementCtx;
}): void {
  const { strategy, scopePolicy, ctx } = input;
  const requiredMutationsByStrategy: Record<string, string[]> = {
    merge_external_flow: ['exec_boundary_link'],
    patch_external_graph: ['pin_default', 'node_comment', 'node_property'],
    patch_external_links: ['link_connect', 'link_disconnect', 'link_replace'],
    replace_external_body: ['body_replace'],
  };
  const expectedMutations = requiredMutationsByStrategy[strategy];
  if (!expectedMutations) {
    return;
  }

  if (scopePolicy['allow_modify_user_nodes'] === true) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope_policy', 'allow_modify_user_nodes'],
      message: `${strategy} requires allow_modify_user_nodes=false.`,
    });
  }

  const rawPolicy = scopePolicy['external_mutation_policy'];
  const policy = typeof rawPolicy === 'object' && rawPolicy !== null && !Array.isArray(rawPolicy)
    ? rawPolicy as Record<string, unknown>
    : undefined;
  if (!policy) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope_policy', 'external_mutation_policy'],
      message: `${strategy} requires scope_policy.external_mutation_policy.`,
    });
    return;
  }

  if (policy['strategy'] !== strategy) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope_policy', 'external_mutation_policy', 'strategy'],
      message: `${strategy} requires external_mutation_policy.strategy="${strategy}".`,
    });
  }

  const allowedMutations = Array.isArray(policy['allowed_mutations'])
    ? policy['allowed_mutations'].filter((value): value is string => typeof value === 'string')
    : [];
  const exactMatch = allowedMutations.length === expectedMutations.length
    && expectedMutations.every((mutation) => allowedMutations.includes(mutation))
    && allowedMutations.every((mutation) => expectedMutations.includes(mutation));
  if (!exactMatch) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['scope_policy', 'external_mutation_policy', 'allowed_mutations'],
      message: `${strategy} requires external_mutation_policy.allowed_mutations to be exactly [${expectedMutations.map((mutation) => `"${mutation}"`).join(', ')}].`,
    });
  }
}

const GraphWriteBehaviorSchema = z.object({
  graph_strategy: z.string(),
  entries: z.array(GraphWriteAppendEntrySchema).min(1).optional(),
  replace: GraphWriteReplaceSchema.optional(),
  patches: z.array(GraphWritePatchSchema).min(1).optional(),
  merges: z.array(GraphWriteMergeSchema).min(1).optional(),
  external_merges: z.array(GraphWriteExternalMergeSchema).min(1).optional(),
  external_patches: z.array(GraphWriteExternalPatchSchema).min(1).optional(),
  external_link_patches: z.array(GraphWriteExternalLinkPatchSchema).min(1).optional(),
  external_replace: GraphWriteExternalReplaceBodySchema.optional(),
}).passthrough().superRefine((value, ctx) => {
  const requiredFieldByStrategy = getGraphWriteRequiredFieldByStrategy();
  const requiredField = requiredFieldByStrategy[value.graph_strategy] as keyof typeof value | undefined;
  if (!requiredField) return;
  if (requiredField && value[requiredField] === undefined) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: [requiredField],
      message: `${value.graph_strategy} requires behavior.${requiredField}.`,
    });
  }
  (['entries', 'replace', 'patches', 'merges', 'external_merges', 'external_patches', 'external_link_patches', 'external_replace'] as const)
    .filter((field) => field !== requiredField && value[field] !== undefined)
    .forEach((field) => {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: [field],
        message: `${field} does not belong to graph_strategy ${value.graph_strategy}.`,
    });
  });
});

const MATERIAL_GRAPH_STRATEGIES = [
  'append_new_owned_graph',
  'replace_owned_graph',
  'patch_owned_graph',
  'merge_owned_graph',
] as const;

export const MATERIAL_GRAPH_COMMON_SELECTORS = [
  'constant',
  'scalar_parameter',
  'vector_parameter',
  'texture_object_parameter',
  'texture_sample',
  'add',
  'multiply',
  'static_switch_parameter',
] as const;

const MaterialCommonSelectorSchema = z.enum(MATERIAL_GRAPH_COMMON_SELECTORS);

const MaterialCandidateSelectorSchema = z.object({
  candidate_id: z.string().min(1).optional(),
  query: z.string().min(1).optional(),
}).strict().superRefine((value, ctx) => {
  const hasCandidateId = value.candidate_id !== undefined;
  const hasQuery = value.query !== undefined;
  if (hasCandidateId === hasQuery) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: 'Material candidate selector requires exactly one of candidate_id or query.',
    });
  }
});

const MaterialExpressionSelectorSchema = z.union([
  MaterialCommonSelectorSchema,
  MaterialCandidateSelectorSchema,
]);

const MaterialGraphNodeSpecSchema = z.object({
  node_key: z.string().min(1),
  selector: MaterialExpressionSelectorSchema,
  properties: z.record(z.unknown()).optional(),
}).strict();

const MaterialPinEndpointSchema = z.object({
  node_key: z.string().min(1),
  pin: z.string().min(1),
}).strict();

const MaterialGraphLinkSpecSchema = z.object({
  from: MaterialPinEndpointSchema,
  to: MaterialPinEndpointSchema,
}).strict();

const MaterialGraphDeleteSpecSchema = z.object({
  node_key: z.string().min(1),
}).strict();

const MaterialGraphAppendEntrySchema = z.object({
  block_id: z.string().min(1),
  nodes: z.array(MaterialGraphNodeSpecSchema).min(1),
  links: z.array(MaterialGraphLinkSpecSchema).optional().default([]),
}).strict();

const MaterialGraphReplaceSchema = z.object({
  block_id: z.string().min(1),
  nodes: z.array(MaterialGraphNodeSpecSchema).min(1),
  links: z.array(MaterialGraphLinkSpecSchema).optional().default([]),
}).strict();

const MaterialGraphPatchSchema = z.object({
  block_id: z.string().min(1),
  nodes: z.array(MaterialGraphNodeSpecSchema).optional(),
  links: z.array(MaterialGraphLinkSpecSchema).optional(),
  deletes: z.array(MaterialGraphDeleteSpecSchema).optional(),
}).strict().superRefine((value, ctx) => {
  if (!value.nodes?.length && !value.links?.length && !value.deletes?.length) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: 'Material graph patch requires at least one of nodes, links, or deletes.',
    });
  }
});

const MaterialGraphMergeSchema = z.object({
  block_id: z.string().min(1),
  nodes: z.array(MaterialGraphNodeSpecSchema).optional(),
  links: z.array(MaterialGraphLinkSpecSchema).optional(),
}).strict().superRefine((value, ctx) => {
  if (!value.nodes?.length && !value.links?.length) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: 'Material graph merge requires at least one of nodes or links.',
    });
  }
});

const MaterialGraphBehaviorSchema = z.object({
  graph_strategy: z.enum(MATERIAL_GRAPH_STRATEGIES),
  entries: z.array(MaterialGraphAppendEntrySchema).min(1).optional(),
  replace: MaterialGraphReplaceSchema.optional(),
  patches: z.array(MaterialGraphPatchSchema).min(1).optional(),
  merges: z.array(MaterialGraphMergeSchema).min(1).optional(),
}).strict().superRefine((value, ctx) => {
  const requiredFieldByStrategy = {
    append_new_owned_graph: 'entries',
    replace_owned_graph: 'replace',
    patch_owned_graph: 'patches',
    merge_owned_graph: 'merges',
  } as const;
  const requiredField = requiredFieldByStrategy[value.graph_strategy];
  if (value[requiredField] === undefined) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: [requiredField],
      message: `${value.graph_strategy} requires behavior.${requiredField}.`,
    });
  }
  (['entries', 'replace', 'patches', 'merges'] as const)
    .filter((field) => field !== requiredField && value[field] !== undefined)
    .forEach((field) => {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: [field],
        message: `${field} does not belong to graph_strategy ${value.graph_strategy}.`,
      });
    });
});

export const MaterialGraphTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_material_graph'),
  target: z.object({
    asset_path: z.string().min(1),
    target_type: z.enum(['asset', 'material', 'material_graph']).optional().default('material_graph'),
  }).passthrough(),
  behavior: MaterialGraphBehaviorSchema,
}).passthrough().superRefine((value, ctx) => {
  for (const forbiddenField of ['domain', 'position', 'comment', 'label']) {
    if (Object.hasOwn(value, forbiddenField)) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: [forbiddenField],
        message: `edit_material_graph does not accept top-level ${forbiddenField}.`,
      });
    }
  }
  if (Object.hasOwn(value.target, 'graph_path')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['target', 'graph_path'],
      message: 'edit_material_graph targets the material asset graph implicitly; target.graph_path is not a P0 field.',
    });
  }
}));

export const MaterialInstanceTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_material_instance'),
  target: z.object({
    asset_path: z.string().min(1),
    target_type: z.enum(['asset', 'material_instance']).optional().default('material_instance'),
  }).passthrough(),
  behavior: MaterialInstanceBehaviorSchema,
}).passthrough());

const GraphWriteAutoSearchPolicySchema = z.object({
  mode: z.enum(['off', 'on_preview_resolution_failure']).optional().default('off'),
  max_candidates_per_statement: z.number().int().min(1).max(10).optional().default(3),
  max_auto_search_statements: z.number().int().min(1).max(64).optional().default(16),
  max_total_auto_search_ms: z.number().int().min(1).max(1000).optional().default(120),
  detail_level: z.enum(['short', 'diagnostic']).optional().default('short'),
}).strict();

const GraphWritePolicySchema = z.object({
  auto_search: GraphWriteAutoSearchPolicySchema.optional(),
}).passthrough().optional().default({});

export const GraphWriteTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_graph'),
  scope_policy: z.object({
    graph_name: z.string().min(1).optional().default('EventGraph'),
    allow_modify_user_nodes: z.boolean().optional().default(false),
    external_mutation_policy: ExternalMutationPolicySchema.optional(),
  }).passthrough().optional().default({ graph_name: 'EventGraph', allow_modify_user_nodes: false }),
  graph_write_policy: GraphWritePolicySchema,
  behavior: GraphWriteBehaviorSchema,
}).passthrough().superRefine((value, ctx) => {
  validateExactExternalMutationPolicy({
    strategy: value.behavior.graph_strategy,
    scopePolicy: value.scope_policy,
    ctx,
  });
}));

export const BLUEPRINT_VARIABLE_REPLICATION_MODES = ['none', 'replicated', 'rep_notify'] as const;

export const BLUEPRINT_VARIABLE_REPLICATION_CONDITIONS = [
  'none',
  'initial_only',
  'owner_only',
  'skip_owner',
  'simulated_only',
  'autonomous_only',
  'simulated_or_physics',
  'initial_or_owner',
  'custom',
  'replay_or_owner',
  'replay_only',
  'simulated_only_no_replay',
  'simulated_or_physics_no_replay',
  'skip_replay',
] as const;

const BlueprintVariableReplicationSettingSchema = z.object({
  mode: z.enum(BLUEPRINT_VARIABLE_REPLICATION_MODES),
  condition: z.enum(BLUEPRINT_VARIABLE_REPLICATION_CONDITIONS).optional(),
  notify_function: z.string().min(1).optional(),
  create_notify_function: z.boolean().optional(),
  reuse_existing_notify_function: z.boolean().optional(),
}).strict().superRefine((value, ctx) => {
  if (value.mode === 'none' && value.condition !== undefined && value.condition !== 'none') {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['condition'],
      message: 'Replication condition is accepted only for replicated and rep_notify modes.',
    });
  }
});

const BlueprintVariablePropertySettingSchema = z.object({
  property_path: z.string().min(1),
  value: z.unknown(),
}).passthrough().superRefine((value, ctx) => {
  if (value.property_path !== 'replication') {
    return;
  }
  const result = BlueprintVariableReplicationSettingSchema.safeParse(value.value);
  if (result.success) {
    return;
  }
  for (const issue of result.error.issues) {
    ctx.addIssue({
      ...issue,
      path: ['value', ...issue.path],
    });
  }
});

export const BlueprintVariableTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_variables'),
  behavior: z.object({
    variable_strategy: z.string(),
    function_name: z.string().min(1).optional(),
    variables: z.array(z.object({
      op: z.string().min(1).optional(),
      kind: z.string().min(1).optional(),
      name: z.string().min(1),
      pin_type: z.record(z.unknown()).optional(),
      variable_type: z.record(z.unknown()).optional(),
      properties: z.array(BlueprintVariablePropertySettingSchema).optional(),
      value: z.unknown().optional(),
    }).passthrough()).optional(),
    changes: z.array(z.object({
      kind: z.string().min(1),
      name: z.string().min(1),
      pin_type: z.record(z.unknown()).optional(),
      variable_type: z.record(z.unknown()).optional(),
      properties: z.array(BlueprintVariablePropertySettingSchema).optional(),
      value: z.unknown().optional(),
    }).passthrough()).optional(),
    defaults: z.array(z.object({
      kind: z.string().min(1).optional(),
      name: z.string().min(1),
      value: z.unknown(),
    }).passthrough()).optional(),
  }).passthrough(),
}).passthrough());

export const AssetFactoryTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('create_asset'),
  behavior: z.object({
    asset_strategy: z.literal('ensure_asset'),
    asset: z.object({
      asset_type: z.string().min(1),
      parent_class: z.string().min(1).optional(),
      value_type: z.string().min(1).optional(),
      fields: z.array(z.object({
        name: z.string().min(1),
        type: z.enum(['int', 'float', 'bool', 'string']),
        default_value: z.unknown().optional(),
      }).passthrough()).min(1).optional(),
      row_struct: z.string().min(1).optional(),
      data_asset_class: z.string().min(1).optional(),
      collision: z.string().min(1).optional(),
      collision_policy: z.string().min(1).optional(),
    }).passthrough(),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  const assetType = value.behavior.asset.asset_type.trim().toLowerCase();
  if ((assetType === 'data_table' || assetType === 'datatable') && !value.behavior.asset.row_struct) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['behavior', 'asset', 'row_struct'],
      message: 'asset_type=data_table requires behavior.asset.row_struct.',
    });
  }
  if ((assetType === 'data_asset' || assetType === 'dataasset') && !value.behavior.asset.data_asset_class) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['behavior', 'asset', 'data_asset_class'],
      message: 'asset_type=data_asset requires behavior.asset.data_asset_class. Use a concrete UDataAsset subclass; in a new project, create a PrimaryDataAsset Blueprint class first and pass its asset path or generated class path.',
    });
  }
}));

const COMPONENT_NAME_COLLISION_POLICY_VALUES = ['reuse_if_exists', 'fail_if_exists', 'block_if_class_mismatch'] as const;
const COMPONENT_ON_NAME_CONFLICT_VALUES = [
  'reuse_existing',
  'reuse_if_type_matches',
  'reuse_if_exists',
  'fail_if_exists',
  'block_if_class_mismatch',
] as const;
const COMPONENT_TRANSFORM_POLICY_VALUES = ['preserve_world', 'preserve_relative', 'reset_relative'] as const;
const COMPONENT_OLD_ROOT_POLICY_VALUES = ['keep_as_child', 'remove_default_scene_root_when_empty'] as const;
const COMPONENT_DEFAULT_ROOT_POLICY_VALUES = ['require_scene_component', 'create_default_scene_root_when_needed'] as const;
const COMPONENT_DELETE_POLICY_VALUES = [
  'block_if_children',
  'promote_children',
  'delete_owned_children',
  'reattach_children_to_parent',
] as const;
const FORBIDDEN_AGENT_COMPONENT_FIELDS = [
  'op',
  'attach_to',
  'class_spec',
  'component_name',
  'component_class',
  'parent_component',
  'new_parent_component',
  'socket_name',
  'new_component_name',
  'component_id',
  'component_template_path',
  'component_origin',
  'class_path',
  'is_root',
  'is_default_scene_root',
  'is_owned_scs',
  'is_inherited',
  'is_native',
  'can_delete',
  'can_rename',
  'can_reparent',
  'children',
  'relative_transform',
  'selected_defaults',
  'readback_revision',
  'readback_fingerprint',
  'readback_fingerprint_before',
  'readback_fingerprint_after',
  'before_parent',
  'after_parent',
  'before_root',
  'after_root',
  'changed_properties',
] as const;

const BlueprintComponentChangeSchema = z.object({
  kind: z.enum([
    'ensure_component_present',
    'configure_component',
    'rename_component',
    'reparent_component',
    'attach_component',
    'detach_component',
    'set_root_component',
    'remove_component',
  ]),
  name: z.string().min(1).optional(),
  class: z.string().min(1).optional(),
  parent: z.string().min(1).optional(),
  new_parent: z.string().min(1).optional(),
  socket: z.string().min(1).optional(),
  attach_rule: z.string().min(1).optional(),
  name_collision_policy: z.enum(COMPONENT_NAME_COLLISION_POLICY_VALUES).optional(),
  on_name_conflict: z.enum(COMPONENT_ON_NAME_CONFLICT_VALUES).optional(),
  properties: z.array(z.record(z.unknown())).optional(),
  attach: z.record(z.unknown()).optional(),
  new_name: z.string().min(1).optional(),
  transform_policy: z.enum(COMPONENT_TRANSFORM_POLICY_VALUES).optional(),
  old_root_policy: z.enum(COMPONENT_OLD_ROOT_POLICY_VALUES).optional(),
  default_root_policy: z.enum(COMPONENT_DEFAULT_ROOT_POLICY_VALUES).optional(),
  delete_policy: z.enum(COMPONENT_DELETE_POLICY_VALUES).optional(),
}).passthrough().superRefine((value, ctx) => {
  FORBIDDEN_AGENT_COMPONENT_FIELDS.forEach((field) => {
    if (!Object.hasOwn(value, field)) {
      return;
    }
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: [field],
      message: `${field} is runtime or internal component metadata and is not allowed in agent-authored component changes.`,
    });
  });
});

export const BlueprintComponentTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_components'),
  behavior: z.object({
    component_strategy: z.literal('component_tree'),
    changes: z.array(BlueprintComponentChangeSchema).min(1),
  }).passthrough(),
}).passthrough());

export const BlueprintClassSettingsTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_class_settings'),
  behavior: z.object({
    class_settings_strategy: z.literal('class_settings'),
    interfaces: z.object({
      ensure_present: z.array(z.string().min(1)).optional(),
      ensure_absent: z.array(z.string().min(1)).optional(),
    }).passthrough().optional(),
    class_defaults: z.array(z.record(z.unknown())).optional(),
    reparent: z.object({
      new_parent_class: z.string().min(1),
    }).passthrough().optional(),
  }).passthrough().superRefine((value, ctx) => {
    if (Object.hasOwn(value, 'parent_class')) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['parent_class'],
        message: 'Use behavior.reparent.new_parent_class for Blueprint reparent operations.',
      });
    }
  }),
}).passthrough());

const UMG_WIDGET_CHANGE_KIND_VALUES = UMG_WIDGET_OPERATION_MANIFEST
  .map((descriptor) => descriptor.kind) as [string, ...string[]];

const UMGWidgetChangeSchema = z.object({
  kind: z.enum(UMG_WIDGET_CHANGE_KIND_VALUES),
  widget_name: z.string().min(1).optional(),
  root_widget_name: z.string().min(1).optional(),
  source_widget_name: z.string().min(1).optional(),
  target_parent_name: z.string().min(1).optional(),
  widget_class: z.string().min(1).optional(),
  wrapper_class: z.string().min(1).optional(),
  wrapper_name: z.string().min(1).optional(),
  new_widget_class: z.string().min(1).optional(),
  new_widget_name: z.string().min(1).optional(),
  new_parent_class: z.string().min(1).optional(),
  parent_name: z.string().min(1).optional(),
  new_parent_name: z.string().min(1).optional(),
  slot_name: z.string().min(1).optional(),
  host_widget_name: z.string().min(1).optional(),
  virtual_index: z.number().int().min(0).optional(),
  expected_parent_name: z.string().min(1).optional(),
  expected_virtual_index: z.number().int().min(0).optional(),
  expected_content_widget_name: z.string().min(1).optional(),
  replace_existing: z.boolean().optional(),
  property_name: z.string().min(1).optional(),
  property_path: z.string().min(1).optional(),
  expected_slot_class_path: z.string().min(1).optional(),
  expected_widget_class_path: z.string().min(1).optional(),
  expected_parent_class: z.string().min(1).optional(),
  replacement_policy: z.enum(['promote_single_child', 'replace_with_empty_root', 'remove_empty_root']).optional(),
  replacement_widget_class: z.string().min(1).optional(),
  replacement_widget_name: z.string().min(1).optional(),
  expected_root_class_path: z.string().min(1).optional(),
  name_mapping: z.record(z.string().min(1)).optional(),
  preserve_children: z.boolean().optional(),
  preserve_slot: z.boolean().optional(),
  is_variable: z.boolean().optional(),
  value: z.unknown().optional(),
}).passthrough().superRefine((change, ctx) => {
  if (Object.hasOwn(change, 'parent_widget_name')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['parent_widget_name'],
      message: 'Use parent_name for WidgetTree parent targeting.',
    });
  }
  if (Object.hasOwn(change, 'insert_index') || Object.hasOwn(change, 'child_index')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: Object.hasOwn(change, 'insert_index') ? ['insert_index'] : ['child_index'],
      message: 'Use virtual_index for WidgetTree position targeting.',
    });
  }
});

export const UMGWidgetTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_umg_widget'),
  behavior: z.object({
    widget_strategy: z.literal('widget_blueprint_edit'),
    changes: z.array(UMGWidgetChangeSchema).min(1),
  }).passthrough(),
}).passthrough());

export const DataTableTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_data_table'),
  behavior: z.object({
    row_strategy: z.literal('row_edit'),
    rows: z.array(z.object({
      action: z.enum(['add', 'update', 'delete']),
      row_name: z.string().min(1),
      fields: z.record(z.unknown()).optional(),
    }).passthrough()).min(1),
  }).passthrough(),
}).passthrough());

export const ObjectPropertyTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_object_properties'),
  behavior: z.object({
    property_strategy: z.literal('property_edit'),
    changes: z.array(z.object({
      kind: z.enum(['set_property', 'set_object_property']).optional(),
      property_path: z.string().min(1),
      value: z.unknown(),
    }).passthrough()).min(1),
  }).passthrough(),
}).passthrough());

const BlueprintSignatureChangeSchema = z.object({
  kind: z.enum([
    'ensure_function',
    'ensure_interface_function',
    'ensure_custom_event',
    'ensure_interface_event',
    'ensure_macro',
    'ensure_event_dispatcher',
    'ensure_override_event',
    'remove_signature',
  ]),
  function_name: z.string().min(1).optional(),
  event_name: z.string().min(1).optional(),
  graph_name: z.string().min(1).optional(),
  macro_name: z.string().min(1).optional(),
  dispatcher_name: z.string().min(1).optional(),
  signature_kind: z.enum([
    'function',
    'interface_function',
    'custom_event',
    'interface_event',
    'event_dispatcher',
    'override_event',
    'native_event',
  ]).optional(),
  signature_name: z.string().min(1).optional(),
  interface_path: z.string().min(1).optional(),
  inputs: z.array(SignaturePinSpecSchema).optional(),
  outputs: z.array(SignaturePinSpecSchema).optional(),
  is_pure: z.boolean().optional(),
  name_collision_policy: z.enum(['reuse_if_exists', 'fail_if_exists']).optional(),
  signature_mismatch_policy: z.enum(['block', 'migrate_if_unreferenced']).optional(),
  event_kind: z.enum(['native_event', 'override_event']).optional(),
  execute_policy: z.enum(['blocked_preflight', 'create_if_missing', 'execute_if_unreferenced']).optional(),
  require_reference_context: z.literal(true).optional(),
}).passthrough();

export const BlueprintSignatureTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('edit_blueprint_signature'),
  behavior: z.object({
    signature_strategy: z.literal('signature_edit'),
    changes: z.array(BlueprintSignatureChangeSchema).min(1),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  value.behavior.changes.forEach((change, index) => {
    const path = ['behavior', 'changes', index];
    if (change.kind === 'ensure_function' && !change.function_name) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: [...path, 'function_name'], message: 'ensure_function requires function_name.' });
    }
    if (change.kind === 'ensure_interface_function' && (!change.function_name || !change.interface_path)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'ensure_interface_function requires function_name and interface_path.' });
    }
    if (change.kind === 'ensure_custom_event' && (!change.event_name || !change.graph_name)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'ensure_custom_event requires event_name and graph_name.' });
    }
    if (change.kind === 'ensure_interface_event' && (!change.event_name || !change.graph_name || !change.interface_path)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'ensure_interface_event requires event_name, graph_name, and interface_path.' });
    }
    if (change.kind === 'ensure_macro' && !change.macro_name) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: [...path, 'macro_name'], message: 'ensure_macro requires macro_name.' });
    }
    if (change.kind === 'ensure_event_dispatcher' && !change.dispatcher_name) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: [...path, 'dispatcher_name'], message: 'ensure_event_dispatcher requires dispatcher_name.' });
    }
    if (change.kind === 'ensure_override_event' && !change.event_name) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path: [...path, 'event_name'], message: 'ensure_override_event requires event_name.' });
    }
    if (change.kind === 'remove_signature' && !(change.signature_name || change.function_name || change.event_name || change.dispatcher_name)) {
      ctx.addIssue({ code: z.ZodIssueCode.custom, path, message: 'remove_signature requires signature_name, function_name, event_name, or dispatcher_name.' });
    }
    for (const field of ['inputs', 'outputs'] as const) {
      const pins = Array.isArray(change[field]) ? change[field] : undefined;
      if (pins) {
        const duplicate = assertNoDuplicateSignaturePinNames(pins, [...path, field]);
        if (duplicate) {
          ctx.addIssue({
            code: z.ZodIssueCode.custom,
            path: duplicate.path,
            message: duplicate.message,
          });
        }
      }
    }
  });
}));

export const CompositeBlueprintFeatureTaskSpecSchema: z.ZodTypeAny = agentFacingTaskSpecSchema(TaskSpecBaseSchema.extend({
  task_type: z.literal('create_blueprint_feature'),
  scope_policy: z.object({
    graph_name: z.string().min(1).optional(),
    prefer_new_graph: z.boolean().optional(),
    allow_modify_user_nodes: z.boolean().optional().default(false),
    allow_merge_existing_execution_flow: z.boolean().optional().default(false),
    allow_create_assets: z.boolean().optional().default(false),
    allow_edit_input_mapping: z.boolean().optional().default(false),
  }).passthrough().optional(),
  asset_policy: z.record(z.unknown()).optional(),
  resources: z.record(z.unknown()).optional(),
  components: z.array(z.object({
    name: z.string().min(1),
    class: z.string().min(1),
    attach_to: z.string().min(1).nullable().optional(),
    attach: z.record(z.unknown()).optional(),
    set_as_root: z.boolean().optional(),
    properties: z.union([
      z.record(z.unknown()),
      z.array(z.record(z.unknown())),
    ]).optional(),
  }).passthrough()).optional(),
  variables: z.array(z.object({
    name: z.string().min(1),
    type: z.string().min(1).optional(),
    pin_type: z.record(z.unknown()).optional(),
    variable_type: z.record(z.unknown()).optional(),
    default: z.unknown().optional(),
    category: z.string().optional(),
    tooltip: z.string().optional(),
    flags: z.record(z.unknown()).optional(),
    metadata: z.record(z.unknown()).optional(),
  }).passthrough()).optional(),
  class_settings: z.object({
    implemented_interfaces: z.array(z.string().min(1)).optional(),
    class_defaults: z.union([
      z.record(z.unknown()),
      z.array(z.record(z.unknown())),
    ]).optional(),
    reparent: z.object({
      new_parent_class: z.string().min(1),
    }).optional(),
  }).passthrough().optional(),
  behavior: GraphWriteBehaviorSchema.optional(),
  integration: z.record(z.unknown()).optional(),
}).passthrough().superRefine((value, ctx) => {
  const classSettings = value.class_settings as Record<string, unknown> | undefined;
  if (classSettings && Object.hasOwn(classSettings, 'parent_class')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['class_settings', 'parent_class'],
      message: 'Use class_settings.reparent.new_parent_class for Blueprint reparent operations.',
    });
  }
  const hasComponents = Array.isArray(value.components) && value.components.length > 0;
  const hasVariables = Array.isArray(value.variables) && value.variables.length > 0;
  const hasClassSettings = value.class_settings !== undefined;
  const hasBehavior = value.behavior !== undefined;
  if (!hasComponents && !hasVariables && !hasClassSettings && !hasBehavior) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: 'create_blueprint_feature requires at least one of components, variables, class_settings, or behavior.',
    });
  }
}));

export const TaskSpecSchema: z.ZodTypeAny = z.union([
  CompositeBlueprintFeatureTaskSpecSchema,
  MaterialGraphTaskSpecSchema,
  MaterialInstanceTaskSpecSchema,
  GraphWriteTaskSpecSchema,
  BlueprintVariableTaskSpecSchema,
  AssetFactoryTaskSpecSchema,
  BlueprintComponentTaskSpecSchema,
  BlueprintClassSettingsTaskSpecSchema,
  UMGWidgetTaskSpecSchema,
  DataTableTaskSpecSchema,
  ObjectPropertyTaskSpecSchema,
  BlueprintSignatureTaskSpecSchema,
]).superRefine((value, ctx) => {
  if (value && typeof value === 'object' && !Array.isArray(value) && Object.hasOwn(value, 'intent')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['intent'],
      message: 'TaskSpec.intent is compiler/runtime generated; Agents must express executable intent through task_type and behavior.',
    });
  }
});

export const PreviewTaskInputSchema: z.ZodTypeAny = z.object({
  task_spec: TaskSpecSchema,
  develop: z.boolean().optional().default(false),
});

export const TaskPreviewTokenSchema = z.string().regex(/^[0-9a-fA-F]{32}$/, {
  message: 'preview_token must be a 32-character hex string.',
});

export const ExecuteTaskInputSchema: z.ZodTypeAny = z.object({
  task_spec: TaskSpecSchema,
  develop: z.boolean().optional().default(false),
  preview_token: TaskPreviewTokenSchema.optional(),
});

export const GetTaskResultInputSchema = z.object({
  task_run_id: z.string().min(1),
});

export const TaskIssueSchema = z.object({
  code: z.string().min(1),
  path: z.string(),
  message: z.string().min(1),
  suggested_patch: z.record(z.unknown()).optional(),
}).passthrough();

export const AgentImportNodeSchema = z.object({
  id: z.string().min(1),
  kind: z.string().min(1),
  name: z.string().optional(),
  function: z.string().optional(),
  inputs: z.record(z.unknown()).optional(),
  var: z.string().optional(),
  value: z.string().optional(),
}).passthrough();

export const AgentImportLinkSchema = z.object({
  kind: z.enum(['exec', 'data']),
  from: z.string().min(1),
  to: z.string().min(1),
});

export const TaskPlanTargetSchema = z.object({
  asset_path: z.string().min(1),
  graph: z.string().min(1),
}).passthrough();

export const AssetOnlyTaskPlanTargetSchema = z.object({
  asset_path: z.string().min(1),
}).passthrough();

export const AppendTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('append_blueprint_graph'),
  target: TaskPlanTargetSchema,
  args: z.object({
    feature_name: z.string().optional(),
    nodes: z.array(AgentImportNodeSchema).min(1),
    links: z.array(AgentImportLinkSchema),
  }),
});

export const ReplaceTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('replace_blueprint_graph'),
  target: TaskPlanTargetSchema.extend({
    replace_scope: z.string().min(1).optional(),
  }).passthrough(),
  args: z.object({
    selector: z.record(z.unknown()).optional(),
    replacement: z.object({
      nodes: z.array(AgentImportNodeSchema),
      links: z.array(AgentImportLinkSchema),
    }).passthrough(),
    options: z.record(z.unknown()).optional(),
  }).passthrough(),
});

export const PatchTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('patch_blueprint_graph'),
  target: TaskPlanTargetSchema.extend({
    patch_scope: z.string().min(1).optional(),
  }).passthrough(),
  args: z.object({
    patch_type: z.string().min(1),
    patched_ref: z.record(z.unknown()).optional(),
    patch: z.record(z.unknown()).optional(),
    expected_old_state: z.record(z.unknown()).optional(),
  }).passthrough(),
});

export const MergeTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  operation: z.literal('merge_blueprint_graph'),
  target: TaskPlanTargetSchema.extend({
    merge_scope: z.string().min(1).optional(),
    insert_strategy: z.string().min(1).optional(),
  }).passthrough(),
  args: z.object({
    anchor: z.record(z.unknown()).optional(),
    inserted: z.record(z.unknown()).optional(),
    sequence_order: z.array(z.string()).optional(),
  }).passthrough(),
});

export const GraphWriteStructuredIrTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  capability: z.literal('graph_write'),
  target: TaskPlanTargetSchema,
  write: z.object({
    strategy: z.string().min(1),
    ops: z.array(z.object({
      op: z.string().min(1),
    }).passthrough()).min(1),
  }).passthrough(),
  constraints: z.object({
    allow_modify_user_nodes: z.boolean(),
    ownership_scope: z.string().min(1),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  if (Object.hasOwn(value, 'operation')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['operation'],
      message: 'GraphWrite IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.',
    });
  }
});

export const GraphWriteLoweringAdapterTaskPlanStepSchema = z.discriminatedUnion('operation', [
  AppendTaskPlanStepSchema,
  ReplaceTaskPlanStepSchema,
  PatchTaskPlanStepSchema,
  MergeTaskPlanStepSchema,
]);

export const GraphWriteTaskPlanStepSchema = z.union([
  GraphWriteStructuredIrTaskPlanStepSchema,
  GraphWriteLoweringAdapterTaskPlanStepSchema,
]);

export const BlueprintVariableTaskPlanStepSchema = z.object({
  step_id: z.string().min(1),
  capability: z.literal('blueprint_variable'),
  target: z.object({
    asset_path: z.string().min(1),
  }).passthrough(),
  write: z.object({
    strategy: z.string().min(1),
    ops: z.array(z.object({
      op: z.string().min(1),
    }).passthrough()).min(1),
  }).passthrough(),
  constraints: z.object({
    allow_remove_referenced_variables: z.boolean(),
  }).passthrough(),
}).passthrough().superRefine((value, ctx) => {
  if (Object.hasOwn(value, 'operation')) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ['operation'],
      message: 'Blueprint variable IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.',
    });
  }
});

const StructuredCapabilityWriteSchema = z.object({
  strategy: z.string().min(1),
  ops: z.array(z.object({
    op: z.string().min(1),
  }).passthrough()).min(1),
}).passthrough();

function structuredCapabilityStepSchema<TCapability extends string>(
  capability: TCapability,
  strategies: readonly [string, ...string[]],
) {
  return z.object({
    step_id: z.string().min(1),
    capability: z.literal(capability),
    target: AssetOnlyTaskPlanTargetSchema,
    write: StructuredCapabilityWriteSchema.extend({
      strategy: z.enum(strategies),
    }).passthrough(),
    constraints: z.record(z.unknown()).optional(),
  }).passthrough().superRefine((value, ctx) => {
    if (Object.hasOwn(value, 'operation')) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['operation'],
        message: `${capability} IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.`,
      });
    }
  });
}

export const AssetFactoryTaskPlanStepSchema = structuredCapabilityStepSchema(
  'asset_factory',
  ['asset_create'],
);

export const BlueprintComponentTaskPlanStepSchema = structuredCapabilityStepSchema(
  'blueprint_component',
  ['component_tree'],
);

export const BlueprintClassSettingsTaskPlanStepSchema = structuredCapabilityStepSchema(
  'blueprint_class_settings',
  ['class_settings'],
);

export const BlueprintSignatureTaskPlanStepSchema = structuredCapabilityStepSchema(
  'blueprint_signature',
  ['function_signature', 'custom_event_signature', 'macro_signature', 'event_dispatcher_signature', 'override_event_signature'],
);

export const UMGWidgetTaskPlanStepSchema = structuredCapabilityStepSchema(
  'umg_widget',
  ['widget_tree_edit', 'widget_property_edit'],
);

export const DataTableTaskPlanStepSchema = structuredCapabilityStepSchema(
  'data_table',
  ['row_edit'],
);

export const ObjectPropertyTaskPlanStepSchema = structuredCapabilityStepSchema(
  'object_property',
  ['property_edit'],
);

export const MaterialInstanceTaskPlanStepSchema = structuredCapabilityStepSchema(
  'material_instance',
  ['material_instance_edit'],
);

export const TaskPlanStepSchema = z.union([
  GraphWriteTaskPlanStepSchema,
  BlueprintVariableTaskPlanStepSchema,
  AssetFactoryTaskPlanStepSchema,
  BlueprintComponentTaskPlanStepSchema,
  BlueprintClassSettingsTaskPlanStepSchema,
  BlueprintSignatureTaskPlanStepSchema,
  UMGWidgetTaskPlanStepSchema,
  DataTableTaskPlanStepSchema,
  ObjectPropertyTaskPlanStepSchema,
  MaterialInstanceTaskPlanStepSchema,
]);

export const TaskPlanSchema = z.object({
  schema: z.literal(TASK_PLAN_SCHEMA),
  task_name: z.string().optional(),
  task_type: z.string().min(1),
  context_id: z.string().optional(),
  target_assets: z.array(z.string().min(1)).min(1),
  verification: TaskVerificationContractSchema.optional(),
  execution_policy: z.object({
    dry_run_mode: z.enum(['none', 'quick', 'full']),
    should_compile: z.boolean(),
    should_save: z.boolean(),
    review_baseline_dirty_asset_policy: z.enum(['block', 'save_before_archive', 'allow_stale_disk_snapshot']).optional().default('block'),
  }),
  steps: z.array(TaskPlanStepSchema).min(1),
});

export const TaskErrorSchema = z.object({
  schema: z.literal(TASK_ERROR_SCHEMA).optional(),
  code: z.string().min(1),
  category: z.string().min(1),
  stage: z.string().min(1),
  message: z.string().min(1),
  retryable: z.boolean(),
  rollback_result: z.string().optional(),
  agent_action: z.string().optional(),
  issues: z.array(TaskIssueSchema).optional(),
}).passthrough();

export const TaskRunJournalSchema = z.object({
  schema: z.literal(TASK_RUN_JOURNAL_SCHEMA),
  task_run_id: z.string().min(1),
  preview_id: z.string().min(1).optional(),
  task_type: z.string().min(1),
  feature_name: z.string().optional(),
  generated_intent: z.string().optional(),
  status: z.enum(['completed', 'failed', 'partial_failure']),
  target_assets: z.array(z.string().min(1)),
  steps: z.array(z.object({
    step_id: z.string().min(1),
    operation: z.string().min(1),
    status: z.enum(['completed', 'failed', 'blocked', 'skipped']),
    depends_on: z.array(z.string().min(1)).optional(),
    blocked_by_step_ids: z.array(z.string().min(1)).optional(),
    blocked_reason: z.string().min(1).optional(),
    transaction_id: z.string().optional(),
    result: z.record(z.unknown()).optional(),
    error: z.unknown().nullable().optional(),
  }).passthrough()),
  recovery: z.object({
    recommended_action: z.string().min(1),
    safe_to_retry: z.boolean(),
    rollback_available: z.boolean(),
    notes: z.array(z.string()),
  }).optional(),
  bridge_result: z.record(z.unknown()).optional(),
}).passthrough();

export type TaskSpec = z.infer<typeof TaskSpecSchema>;
export type TaskVerificationContract = z.infer<typeof TaskVerificationContractSchema>;
export type TaskPreviewToken = z.infer<typeof TaskPreviewTokenSchema>;
export type BlueprintLogicStatement = z.infer<typeof BlueprintLogicStatementSchema>;
export type BlueprintLogicSpec = z.infer<typeof BlueprintLogicSpecSchema>;
export type TaskIssue = z.infer<typeof TaskIssueSchema>;
export type AgentImportNode = z.infer<typeof AgentImportNodeSchema>;
export type AgentImportLink = z.infer<typeof AgentImportLinkSchema>;
export type AppendTaskPlanStep = z.infer<typeof AppendTaskPlanStepSchema>;
export type ReplaceTaskPlanStep = z.infer<typeof ReplaceTaskPlanStepSchema>;
export type PatchTaskPlanStep = z.infer<typeof PatchTaskPlanStepSchema>;
export type MergeTaskPlanStep = z.infer<typeof MergeTaskPlanStepSchema>;
export type GraphWriteStructuredIrTaskPlanStep = z.infer<typeof GraphWriteStructuredIrTaskPlanStepSchema>;
export type GraphWriteLoweringAdapterTaskPlanStep = z.infer<typeof GraphWriteLoweringAdapterTaskPlanStepSchema>;
export type GraphWriteTaskPlanStep = z.infer<typeof GraphWriteTaskPlanStepSchema>;
export type MaterialGraphTaskSpec = z.infer<typeof MaterialGraphTaskSpecSchema>;
export type MaterialInstanceTaskSpec = z.infer<typeof MaterialInstanceTaskSpecSchema>;
export type BlueprintVariableTaskPlanStep = z.infer<typeof BlueprintVariableTaskPlanStepSchema>;
export type AssetFactoryTaskPlanStep = z.infer<typeof AssetFactoryTaskPlanStepSchema>;
export type BlueprintComponentTaskPlanStep = z.infer<typeof BlueprintComponentTaskPlanStepSchema>;
export type BlueprintClassSettingsTaskPlanStep = z.infer<typeof BlueprintClassSettingsTaskPlanStepSchema>;
export type BlueprintSignatureTaskPlanStep = z.infer<typeof BlueprintSignatureTaskPlanStepSchema>;
export type UMGWidgetTaskPlanStep = z.infer<typeof UMGWidgetTaskPlanStepSchema>;
export type DataTableTaskPlanStep = z.infer<typeof DataTableTaskPlanStepSchema>;
export type ObjectPropertyTaskPlanStep = z.infer<typeof ObjectPropertyTaskPlanStepSchema>;
export type MaterialInstanceTaskPlanStep = z.infer<typeof MaterialInstanceTaskPlanStepSchema>;
export type BlueprintSignatureTaskSpec = z.infer<typeof BlueprintSignatureTaskSpecSchema>;
export type TaskPlanStep = z.infer<typeof TaskPlanStepSchema>;
export type TaskPlan = z.infer<typeof TaskPlanSchema>;
export type TaskError = z.infer<typeof TaskErrorSchema>;
export type TaskRunJournal = z.infer<typeof TaskRunJournalSchema>;

export interface AppendBridgePayload {
  target: {
    asset_path: string;
    graph: string;
  };
  feature_name?: string;
  logic_spec: {
    schema: 'BlueprintLogicSpec.v2';
    entry?: Record<string, unknown>;
    statements: BlueprintLogicStatement[];
  };
  dry_run: boolean;
}
