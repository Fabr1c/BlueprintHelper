import type {
  AgentImportLink,
  AgentImportNode,
  AppendBridgePayload,
  AppendTaskPlanStep,
  BlueprintLogicStatement,
  GraphWriteStructuredIrTaskPlanStep,
  TaskIssue,
  TaskPlan,
  TaskSpec,
} from '../../schema/task-schemas.js';
import { BlueprintPinTypeSpecSchema } from '../../schema/blueprint-pin-type-spec.js';
import {
  CONTAINER_ACTION_OPERATIONS_BY_KIND,
  CONTAINER_ACTION_ROLE_FIELDS,
  CONTAINER_ACTION_TYPE_FIELDS,
  getContainerActionResultOutputPin,
  getRequiredContainerActionRoles,
  isExpressionContainerActionOperation,
  isValueExpressionContainerActionOperation,
  isSupportedContainerActionKind,
  isSupportedContainerActionOperation,
} from '../../schema/task-schemas.js';
import { collectGraphWriteConnectivityPreflightIssues } from '../graphwrite-connectivity-preflight.js';
import {
  getGraphWriteRequiredFieldByStrategy,
  getSupportedGraphWriteStrategies,
  normalizeSelectorWithDescriptor,
  requireGraphWriteRouteByScope,
} from './graphwrite-route-registry.js';
import { createDefaultExpressionCompilerRegistry } from './expression-compiler-registry.js';
import { createDefaultStatementCompilerRegistry } from './statement-compiler-registry.js';
import { createGraphWriteExpressionCompilerRegistrations } from './graphwrite-expression-compilers.js';
import { createGraphWriteStatementCompilerRegistrations } from './graphwrite-statement-compilers.js';
import type {
  CompiledConditionFlow,
  CompiledStatementFlow,
  CompileFlowContext,
  GraphWriteExpressionCompileInput,
  GraphWriteStatementCompileInput,
  GraphWriteStatementNodeCompileInput,
} from './graphwrite-compiler-types.js';
import { requiredNonEmptyArray, asRecord, isNonEmptyRecord, isRecord } from '../compiler-helpers.js';
import { TaskSpecCompileError } from '../task-compiler-errors.js';

export function taskPlanToAppendBridgePayload(taskPlan: TaskPlan, dryRun: boolean): AppendBridgePayload {
  const step = taskPlan.steps.find((candidate) => (
    ('capability' in candidate && candidate.capability === 'graph_write') ||
    ('operation' in candidate && candidate.operation === 'append_blueprint_graph')
  ));
  if (!step) {
    throw new TaskSpecCompileError('unsupported_taskplan_operation', 'TaskPlan does not contain an append_blueprint_graph step.', [
      {
        code: 'unsupported_taskplan_operation',
        path: 'steps',
        message: 'TaskPlan requires a GraphWrite append step.',
      },
    ]);
  }

  if ('capability' in step && step.capability === 'graph_write') {
    return graphWriteTaskPlanToAppendBridgePayload(taskPlan, step as GraphWriteStructuredIrTaskPlanStep, dryRun);
  }

  if (!('operation' in step) || step.operation !== 'append_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_taskplan_operation', 'TaskPlan does not contain an append_blueprint_graph step.', [
      {
        code: 'unsupported_taskplan_operation',
        path: 'steps[0].operation',
        message: 'Only append_blueprint_graph lowering adapter TaskPlan steps are supported in the first MCP slice.',
      },
    ]);
  }

  const appendStep = step as AppendTaskPlanStep;
  const appendArgs = appendStep.args as Record<string, unknown>;
  const logicSpec = appendArgs['logic_spec'];
  if (!isRecord(logicSpec)) {
    throw new TaskSpecCompileError('logic_spec_required', 'append_blueprint_graph requires args.logic_spec/SemanticIR.', [
      {
        code: 'logic_spec_required',
        path: 'steps[0].args.logic_spec',
        message: 'Legacy nodes/links append payloads are disabled. Provide BlueprintLogicSpec.v2 logic_spec.',
      },
    ]);
  }

  return {
    target: {
      asset_path: appendStep.target.asset_path,
      graph: appendStep.target.graph,
    },
    ...(appendStep.args.feature_name ? { feature_name: appendStep.args.feature_name } : {}),
    logic_spec: logicSpec as AppendBridgePayload['logic_spec'],
    dry_run: dryRun,
  };
}

function graphWriteTaskPlanToAppendBridgePayload(
  taskPlan: TaskPlan,
  step: GraphWriteStructuredIrTaskPlanStep,
  dryRun: boolean,
): AppendBridgePayload {
  if (step.write.strategy !== 'owned_graph_edit') {
    throw new TaskSpecCompileError('unsupported_graph_write_strategy', `Unsupported GraphWrite strategy: ${step.write.strategy}`, [
      {
        code: 'unsupported_graph_write_strategy',
        path: 'steps[0].write.strategy',
        message: 'Only owned_graph_edit can lower to append_blueprint_graph in the first MCP slice.',
      },
    ]);
  }

  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  const logicStatements: BlueprintLogicStatement[] = [];
  let logicEntry: Record<string, unknown> | undefined;
  const cloneOptions: LogicCloneOptions = {
    defaultFieldOwnerClass: defaultFieldOwnerClassForBlueprintAsset(step.target.asset_path),
  };
  step.write.ops.forEach((rawOp, opIndex) => {
    if (rawOp.op !== 'ensure_entry') {
      throw new TaskSpecCompileError('unsupported_graph_write_op', `Unsupported GraphWrite op for append lowering: ${rawOp.op}`, [
        {
          code: 'unsupported_graph_write_op',
          path: `steps[0].write.ops[${opIndex}].op`,
          message: 'Only ensure_entry lowers to append_blueprint_graph in the first MCP slice.',
        },
      ]);
    }

    logicStatements.push(...compileEnsureEntryOpIntoAppendPayload(nodes, links, rawOp as Record<string, unknown>, `steps[0].write.ops[${opIndex}]`, cloneOptions));
    if (!logicEntry && isRecord(rawOp) && rawOp.entry_type === 'custom_event' && typeof rawOp.name === 'string') {
      const eventKind = graphWriteEnsureEntryEventKind(rawOp);
      const catalogEvidence = graphWriteCatalogEvidence(rawOp['catalog_evidence']);
      logicEntry = {
        kind: eventKind,
        name: rawOp.name,
        id: `${toIdSegment(rawOp.name)}_entry`,
        ...(catalogEvidence ? { catalog_evidence: catalogEvidence } : {}),
        ...(typeof rawOp.signature_evidence_id === 'string' && rawOp.signature_evidence_id.trim().length > 0
          ? {
              source_cluster: 'blueprint_signature',
              signature_evidence_id: rawOp.signature_evidence_id.trim(),
            }
          : {}),
      };
    }
  });

  return {
    target: {
      asset_path: step.target.asset_path,
      graph: step.target.graph,
    },
    ...(taskPlan.task_name ? { feature_name: taskPlan.task_name } : {}),
    logic_spec: {
      schema: 'BlueprintLogicSpec.v2',
      ...(logicEntry ? { entry: logicEntry } : {}),
      statements: logicStatements,
    },
    dry_run: dryRun,
  };
}

export function assertSupportedTaskSpec(taskSpec: TaskSpec) {
  if (taskSpec.task_type !== 'edit_blueprint_graph') {
    throw new TaskSpecCompileError('unsupported_task_type', `Unsupported TaskSpec task_type: ${taskSpec.task_type}`, [
      {
        code: 'unsupported_task_type',
        path: 'task_type',
        message: 'This compiler path only supports edit_blueprint_graph.',
      },
    ]);
  }

  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = getRequiredString(behavior, 'graph_strategy', 'behavior.graph_strategy');
  const supportedStrategies = getSupportedGraphWriteStrategies();
  if (!supportedStrategies.includes(strategy)) {
    throw new TaskSpecCompileError('unsupported_graph_strategy', 'Unsupported GraphWrite graph_strategy.', [
      {
        code: 'unsupported_graph_strategy',
        path: 'behavior.graph_strategy',
        message: `Use ${supportedStrategies.join(', ')}.`,
        suggested_patch: { op: 'replace', path: '/behavior/graph_strategy', value: supportedStrategies[0] ?? 'append_new_owned_graph' },
      },
    ]);
  }
  const requiredFieldByStrategy = getGraphWriteRequiredFieldByStrategy();
  const requiredField = requiredFieldByStrategy[strategy];
  for (const field of ['entries', 'replace', 'patches', 'merges', 'external_merges', 'external_patches', 'external_replace']) {
    if (field !== requiredField && behavior[field] !== undefined) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${field} does not belong to graph_strategy ${strategy}.`, [
        {
          code: 'graph_write_strategy_field_mismatch',
          path: `behavior.${field}`,
          message: `Use behavior.${requiredField} for ${strategy}.`,
        },
      ]);
    }
  }

  if (taskSpec.scope_policy.allow_modify_user_nodes) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'unsupported_scope_policy: Modifying user nodes is not supported for GraphWrite owned strategies.', [
      {
        code: 'unsupported_scope_policy',
        path: 'scope_policy.allow_modify_user_nodes',
        message: 'Set allow_modify_user_nodes=false and target BlueprintHelper-owned graph logic.',
        suggested_patch: { op: 'replace', path: '/scope_policy/allow_modify_user_nodes', value: false },
      },
    ]);
  }
  if (strategy === 'merge_external_flow') {
    validateExternalGraphWriteScopePolicy(taskSpec, strategy, ['exec_boundary_link']);
  }
  if (strategy === 'patch_external_graph') {
    validateExternalGraphWriteScopePolicy(taskSpec, strategy, ['pin_default', 'node_comment']);
  }
  if (strategy === 'replace_external_body') {
    if (taskSpec.execution_policy.dry_run_mode !== 'full') {
      throw new TaskSpecCompileError('unsupported_execution_policy', 'replace_external_body requires execution_policy.dry_run_mode="full".', [
        {
          code: 'replace_external_body_requires_full_dry_run',
          path: 'execution_policy.dry_run_mode',
          message: 'Set dry_run_mode="full" for replace_external_body.',
          suggested_patch: { op: 'replace', path: '/execution_policy/dry_run_mode', value: 'full' },
        },
      ]);
    }
    validateExternalGraphWriteScopePolicy(taskSpec, strategy, ['body_replace']);
  }
}

function validateExternalGraphWriteScopePolicy(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>,
  strategy: string,
  requiredMutations: string[],
): void {
  const policy = asRecord(taskSpec.scope_policy['external_mutation_policy']);
  if (!policy) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'unsupported_scope_policy: External graph writes require scope_policy.external_mutation_policy.', [
      {
        code: 'missing_external_mutation_policy',
        path: 'scope_policy.external_mutation_policy',
        message: `Set external_mutation_policy.strategy="${strategy}" with the required allowed_mutations.`,
      },
    ]);
  }

  const policyStrategy = typeof policy['strategy'] === 'string' ? policy['strategy'] : '';
  if (policyStrategy !== strategy) {
    throw new TaskSpecCompileError('unsupported_scope_policy', 'unsupported_scope_policy: External graph mutation policy strategy does not match graph_strategy.', [
      {
        code: 'external_mutation_policy_strategy_mismatch',
        path: 'scope_policy.external_mutation_policy.strategy',
        message: `Use strategy="${strategy}".`,
        suggested_patch: { op: 'replace', path: '/scope_policy/external_mutation_policy/strategy', value: strategy },
      },
    ]);
  }

  const allowedMutations = Array.isArray(policy['allowed_mutations'])
    ? policy['allowed_mutations'].filter((value): value is string => typeof value === 'string')
    : [];
  const exactMatch = allowedMutations.length === requiredMutations.length
    && requiredMutations.every((mutation) => allowedMutations.includes(mutation))
    && allowedMutations.every((mutation) => requiredMutations.includes(mutation));
  if (!exactMatch) {
    throw new TaskSpecCompileError(
      'unsupported_scope_policy',
      `unsupported_scope_policy: External graph mutation policy allowlist must be exactly: ${requiredMutations.join(', ')}.`,
      [
      {
        code: 'external_mutation_policy_exact_allowlist_required',
        path: 'scope_policy.external_mutation_policy.allowed_mutations',
        message: `Use exactly: ${requiredMutations.join(', ')}.`,
      },
      ],
    );
  }
}

export type GraphWriteCompiledOp = Record<string, unknown> & { op: string };
export type GraphWriteAppendEventKind =
  | 'custom_event'
  | 'override_event'
  | 'component_bound_event'
  | 'input_action_event'
  | 'dispatcher_event';
export type GraphWriteCatalogEvidence = {
  source: 'signature' | 'graph_action_catalog';
  signature_evidence_id?: string;
  action_stable_id?: string;
  context_fingerprint?: string;
};
export type GraphWriteSignatureSplit = {
  op: 'ensure_custom_event';
  event_name: string;
  inputs?: unknown;
  name_collision_policy: string;
};

export interface GraphWriteCompileOptions {
  defaultFieldOwnerClass?: string;
}

interface LogicCloneOptions {
  defaultFieldOwnerClass?: string;
  graphLocalSymbols?: Set<string>;
}

const graphWriteStatementCompilerRegistry = createDefaultStatementCompilerRegistry(
  createGraphWriteStatementCompilerRegistrations({
    compileBranchFlow: compileBranchStatementFlowFromCompilerService,
    compileReturnFlow: compileReturnStatementFlowFromCompilerService,
    compileSequenceFlow: compileSequenceStatementFlowFromCompilerService,
    compileGenericControlFlow: compileGenericControlStatementFlowFromCompilerService,
    compileLetFlow: compileLetStatementFlowFromCompilerService,
    compileContainerActionFlow: compileContainerActionStatementFlowFromCompilerService,
    compileDefaultExecFlow: compileDefaultExecStatementFlowFromCompilerService,
    compileCallNode: compileCallStatementNodeFromCompilerService,
    compileComponentBoundEventNode: compileComponentBoundEventStatementNodeFromCompilerService,
    compileContainerActionNode: compileContainerActionStatementNodeFromCompilerService,
    compileGenericControlNode: compileGenericControlStatementNodeFromCompilerService,
    compileConvertOrScheduleNode: compileConvertOrScheduleStatementNodeFromCompilerService,
    compileCreateNode: compileCreateStatementNodeFromCompilerService,
    compileDelegateNode: compileDelegateStatementNodeFromCompilerService,
    compileFieldNode: compileFieldStatementNodeFromCompilerService,
    compileSetNode: compileSetStatementNodeFromCompilerService,
    compileSetPropertyNode: compileSetPropertyStatementNodeFromCompilerService,
    compileUnsupportedNode: compileUnsupportedStatementNodeFromCompilerService,
  }),
);

const graphWriteExpressionCompilerRegistry = createDefaultExpressionCompilerRegistry(
  createGraphWriteExpressionCompilerRegistrations({
    compileLiteral: compileLiteralExpressionFromCompilerService,
    compileContainerAction: compileContainerActionExpressionFromCompilerService,
    compileFieldGet: compileFieldGetExpressionFromCompilerService,
    compileGeneral: compileGeneralExpressionFromCompilerService,
  }),
);

export function makeCustomEventSignatureEvidenceId(eventName: string): string {
  return `signature:custom_event:${eventName}`;
}

export function assertGraphWriteConnectivityPreflight(
  statements: BlueprintLogicStatement[],
  basePath: string,
): void {
  const issues = collectGraphWriteConnectivityPreflightIssues(
    statements as unknown as Record<string, unknown>[],
    basePath,
  );
  if (issues.length === 0) {
    return;
  }

  const taskIssues: TaskIssue[] = issues.map((issue) => ({
    code: issue.code,
    path: issue.path,
    message: issue.message,
  }));
  throw new TaskSpecCompileError(
    'taskspec_semantic_invalid',
    `GraphWrite connectivity static preflight failed: ${taskIssues[0]?.code ?? 'unknown_issue'}.`,
    taskIssues,
  );
}

const PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES = new Map<string, string>([
  ['component_bound_event', 'component_bound_event'],
  ['delegate.bind', 'bind'],
  ['delegate.assign', 'assign'],
  ['delegate.unbind', 'unbind'],
  ['delegate.unbind_all', 'clear'],
  ['delegate.call', 'call'],
]);
const INTERNAL_DELEGATE_STATEMENT_KIND = 'delegate';
const DELEGATE_STATEMENT_OPERATION_KINDS = new Set(['bind', 'assign', 'unbind', 'clear', 'call']);
const CONTAINER_ACTION_KIND = 'container_action';
const GRAPH_CONTAINER_ACTION_FIELDS = [
  'container_kind',
  'container_operation',
  'element_type',
  'key_type',
  'value_type',
  'target',
  'item',
  'items',
  'key',
  'value',
  'index',
  'result_symbol',
  'context_evidence',
] as const;
const SUPPORTED_CONTAINER_KINDS = new Set(Object.keys(CONTAINER_ACTION_OPERATIONS_BY_KIND));
const SUPPORTED_CONTAINER_OPERATIONS = new Map<string, ReadonlySet<string>>(
  Object.entries(CONTAINER_ACTION_OPERATIONS_BY_KIND).map(([containerKind, operations]) => [
    containerKind,
    new Set(operations),
  ]),
);
const FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS = new Set([
  'delegate',
  'bind',
  'assign',
  'unbind',
  'unbind_all',
  'delegate_call',
  'delegate_clear',
]);
const VALUE_PRODUCING_STATEMENT_KINDS = new Set([
  'call',
  'create',
  'convert',
  'schedule',
  CONTAINER_ACTION_KIND,
]);
const GRAPH_BODY_SINGLETON_CONTROL_KINDS = new Set(['branch', 'sequence', 'return']);
const GRAPH_BODY_SWITCH_CONTROL_KINDS = new Set(['switch_int', 'switch_string', 'switch_name', 'switch_enum']);
const GRAPH_BODY_DYNAMIC_CONTROL_KINDS = new Set(['multi_gate']);
const GRAPH_BODY_MACRO_CONTROL_KINDS = new Set([
  'do_once',
  'do_n',
  'gate',
  'flip_flop',
  'for_loop',
  'for_loop_with_break',
  'foreach_loop',
  'foreach_loop_with_break',
  'while_loop',
]);
const SUPPORTED_GRAPH_BODY_CONTROL_KINDS = new Set([
  ...GRAPH_BODY_SINGLETON_CONTROL_KINDS,
  ...GRAPH_BODY_SWITCH_CONTROL_KINDS,
  ...GRAPH_BODY_DYNAMIC_CONTROL_KINDS,
  ...GRAPH_BODY_MACRO_CONTROL_KINDS,
]);
const GRAPH_CONVERT_SCHEDULE_FIELDS = [
  'function_operation',
  'transform_operation',
  'schedule_operation',
  'target_class_path',
  'graph_latent_allowed',
] as const;
const GENERIC_SCHEDULE_OPERATIONS = new Set(['timer_delegate_node', 'latent_or_async_node']);
const FUNCTION_BACKED_CREATE_OPERATIONS = new Set([
  'async_action',
  'function_backed_create',
  'function_backed_spawn',
  'function_backed_construct',
]);
const GENERIC_CREATE_OPERATIONS = new Set([
  'spawn_actor',
  'create_widget',
  'construct_object',
  'make_array',
  'make_map',
  'make_set',
  'asset_action',
]);
const FIELD_STATEMENT_KIND_MAP = new Map([
  ['set', { operation: 'set', scope: 'variable' }],
  ['set_property', { operation: 'set', scope: 'property_path' }],
]);
const FIELD_EXPRESSION_KIND_MAP = new Map([
  ['get', { operation: 'get', scope: 'variable' }],
  ['get_property', { operation: 'get', scope: 'property_path' }],
]);
const SUPPORTED_FIELD_SCOPES = new Set(['variable', 'property_path', 'component_ref', 'field_access']);
const FIELD_SCOPES_WITH_PROPERTY_PATH = new Set(['property_path', 'field_access']);
const GRAPHWRITE_STRUCTURED_PIN_TYPE_FIELDS = [
  'pin_type',
  'key_pin_type',
  'value_pin_type',
  'result_pin_type',
  'return_pin_type',
] as const;
const GRAPHWRITE_STRUCTURED_PIN_TYPE_EVIDENCE_FIELDS = [
  'pin_type',
  'result_pin_type',
  'return_pin_type',
  'output_pin_type',
  'source_pin_type',
  'target_pin_type',
  'schedule_result_pin_type',
  'schedule_output_pin_type',
  'type_promotion_source_pin_type',
  'type_promotion_target_pin_type',
  'type_promotion_result_pin_type',
  'generic.transform.source_pin_type',
  'generic.transform.target_pin_type',
] as const;
const GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY = 'generic.select.result_type_proof';

function applyFieldTaxonomy(record: Record<string, unknown>, operation: string, scope: string): void {
  record.kind = 'field';
  record.field_operation = operation;
  record.field_scope = scope;
}

function copyConvertScheduleSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>): void {
  GRAPH_CONVERT_SCHEDULE_FIELDS.forEach((field) => {
    if (Object.hasOwn(source, field)) {
      target[field] = source[field];
    }
  });
}

function normalizeSemanticToken(value: unknown): string {
  return typeof value === 'string' ? value.trim().toLowerCase() : '';
}

function normalizeEvidenceValue(value: unknown): string {
  if (Array.isArray(value)) {
    return value.map((entry) => normalizeEvidenceValue(entry)).filter((entry) => entry.length > 0).join(',');
  }
  if (typeof value === 'string') return value.trim();
  if (typeof value === 'number' || typeof value === 'boolean') return String(value);
  return '';
}

function contextEvidenceValue(record: Record<string, unknown>, key: string): string {
  const evidence = record.context_evidence;
  if (!isRecord(evidence) || !Object.hasOwn(evidence, key)) {
    return '';
  }
  return normalizeEvidenceValue(evidence[key]);
}

function firstEvidenceValue(record: Record<string, unknown>, evidenceKey: string, fields: readonly string[]): string {
  const fromEvidence = contextEvidenceValue(record, evidenceKey);
  if (fromEvidence.length > 0) {
    return fromEvidence;
  }
  for (const field of fields) {
    if (Object.hasOwn(record, field)) {
      const value = normalizeEvidenceValue(record[field]);
      if (value.length > 0) {
        return value;
      }
    }
  }
  return '';
}

function isGenericControlKind(controlKind: string): boolean {
  return SUPPORTED_GRAPH_BODY_CONTROL_KINDS.has(controlKind) && !GRAPH_BODY_SINGLETON_CONTROL_KINDS.has(controlKind);
}

function requireGenericControlEvidence(value: string, path: string, evidenceKey: string): string {
  if (value.length > 0) {
    return value;
  }
  throw new TaskSpecCompileError('missing_required_evidence', `Generic control requires ${evidenceKey}.`, [
    {
      code: 'missing_required_evidence',
      path,
      message: `Generic control requires ${evidenceKey}.`,
    },
  ]);
}

function genericControlContextEvidence(
  record: Record<string, unknown>,
  controlKind: string,
  path: string,
): Record<string, unknown> {
  const evidence = isRecord(record.context_evidence) ? { ...record.context_evidence } : {};
  evidence['generic.control.operation'] = controlKind;

  if (GRAPH_BODY_SWITCH_CONTROL_KINDS.has(controlKind)) {
    evidence['generic.control.case_values'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.control.case_values', ['case_values']),
      `${path}.case_values`,
      'generic.control.case_values',
    );
    const defaultPolicy = firstEvidenceValue(record, 'generic.control.default_policy', ['default_policy']);
    if (defaultPolicy.length > 0) {
      evidence['generic.control.default_policy'] = defaultPolicy;
    }
    if (controlKind === 'switch_enum') {
      evidence['generic.control.enum_path'] = requireGenericControlEvidence(
        firstEvidenceValue(record, 'generic.control.enum_path', ['enum_path']),
        `${path}.enum_path`,
        'generic.control.enum_path',
      );
    }
  } else if (GRAPH_BODY_DYNAMIC_CONTROL_KINDS.has(controlKind)) {
    evidence['generic.control.dynamic_output_count'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.control.dynamic_output_count', ['dynamic_output_count']),
      `${path}.dynamic_output_count`,
      'generic.control.dynamic_output_count',
    );
  } else if (GRAPH_BODY_MACRO_CONTROL_KINDS.has(controlKind)) {
    evidence['generic.macro.graph_path'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.macro.graph_path', ['macro_graph_path']),
      `${path}.macro_graph_path`,
      'generic.macro.graph_path',
    );
    evidence['generic.macro.pin_shape_snapshot'] = requireGenericControlEvidence(
      firstEvidenceValue(record, 'generic.macro.pin_shape_snapshot', ['macro_pin_shape_snapshot']),
      `${path}.macro_pin_shape_snapshot`,
      'generic.macro.pin_shape_snapshot',
    );
    const worldContextPolicy = firstEvidenceValue(record, 'generic.macro.world_context_policy', ['macro_world_context_policy']);
    if (worldContextPolicy.length > 0) {
      evidence['generic.macro.world_context_policy'] = worldContextPolicy;
    }
  }

  return evidence;
}

function applyGenericControlSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>, controlKind: string, path: string): void {
  target.kind = 'control';
  target.control = controlKind;
  target.control_operation = controlKind;
  target.context_evidence = genericControlContextEvidence(source, controlKind, path);
  delete target.case_values;
  delete target.enum_path;
  delete target.default_policy;
  delete target.dynamic_output_count;
  delete target.macro_graph_path;
  delete target.macro_pin_shape_snapshot;
  delete target.macro_world_context_policy;
}

function isFunctionBackedCreateOperation(createOperation: string): boolean {
  return FUNCTION_BACKED_CREATE_OPERATIONS.has(createOperation);
}

function validateCreateOwnership(record: Record<string, unknown>, path: string): { createOperation: string; functionOperation: string } {
  const createOperation = getRequiredString(record, 'create_operation', `${path}.create_operation`).trim().toLowerCase();
  const functionOperation = normalizeSemanticToken(record.function_operation);
  if (isFunctionBackedCreateOperation(createOperation)) {
    const callableTarget = optionalString(record, 'target') ?? optionalString(record, 'name');
    if (!callableTarget) {
      throw new TaskSpecCompileError('missing_create_function_target', 'missing_create_function_target: Function-backed create operations require a callable target.', [
        {
          code: 'missing_create_function_target',
          path: `${path}.target`,
          message: 'Provide the callable factory function name in target for function-backed create operations.',
        },
      ]);
    }
    if (functionOperation.length > 0 && functionOperation !== 'create_function') {
      throw new TaskSpecCompileError('unsupported_create_owner_mix', 'unsupported_create_owner_mix: Function-backed create operations require function_operation=create_function.', [
        {
          code: 'unsupported_create_owner_mix',
          path: `${path}.function_operation`,
          message: 'Use function_operation=create_function for function-backed create operations.',
        },
      ]);
    }
    return { createOperation, functionOperation: 'create_function' };
  }
  if (functionOperation.length > 0 || GENERIC_CREATE_OPERATIONS.has(createOperation) && functionOperation.length > 0) {
    throw new TaskSpecCompileError('unsupported_create_owner_mix', 'unsupported_create_owner_mix: Generic create operations must not specify function_operation.', [
      {
        code: 'unsupported_create_owner_mix',
        path: `${path}.function_operation`,
        message: 'Remove function_operation for Generic create operations. Use it only for function-backed create factories.',
      },
    ]);
  }
  return { createOperation, functionOperation: '' };
}

function copyCreateSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>, path: string): void {
  const { createOperation, functionOperation } = validateCreateOwnership(source, path);
  target.create_operation = createOperation;
  if (functionOperation.length > 0) {
    target.function_operation = functionOperation;
  } else {
    delete target.function_operation;
  }
}

function validateConvertScheduleOwnership(record: Record<string, unknown>, path: string): void {
  const kind = typeof record.kind === 'string' ? record.kind : '';
  if (kind !== 'schedule') {
    return;
  }

  const functionOperation = typeof record.function_operation === 'string' ? record.function_operation.trim() : '';
  const scheduleOperation = typeof record.schedule_operation === 'string' ? record.schedule_operation.trim().toLowerCase() : '';
  if (functionOperation.length > 0 && GENERIC_SCHEDULE_OPERATIONS.has(scheduleOperation)) {
    throw new TaskSpecCompileError('unsupported_schedule_owner_mix', 'unsupported_schedule_owner_mix: Generic schedule operations must not specify function_operation.', [
      {
        code: 'unsupported_schedule_owner_mix',
        path: `${path}.function_operation`,
        message: 'Remove the FunctionAction operation field for Generic Schedule timer or latent nodes. Use it only for FunctionAction-owned schedule functions.',
      },
    ]);
  }
}

function invalidPinTypeMessage(path: string): string {
  return `${path} must be a structured BlueprintPinTypeSpec object.`;
}

function invalidPinTypeEvidenceMessage(path: string): string {
  return `${path} must be structured pin-type evidence.`;
}

function throwLegacyPinTypeTokenUnsupported(path: string): never {
  throw new TaskSpecCompileError('legacy_pin_type_token_unsupported', invalidPinTypeMessage(path), [
    {
      code: 'legacy_pin_type_token_unsupported',
      path,
      message: 'Use a structured BlueprintPinTypeSpec object instead of a legacy string token.',
    },
  ]);
}

function throwInvalidStructuredPinType(
  code: 'invalid_pin_type' | 'invalid_pin_type_evidence',
  path: string,
  message: string,
): never {
  throw new TaskSpecCompileError(code, message, [
    {
      code,
      path,
      message,
    },
  ]);
}

function joinIssuePath(basePath: string, issuePath: readonly (string | number)[]): string {
  if (issuePath.length === 0) return basePath;
  return `${basePath}.${issuePath.join('.')}`;
}

function requireStructuredPinType(
  value: unknown,
  path: string,
  code: 'invalid_pin_type' | 'invalid_pin_type_evidence' = 'invalid_pin_type',
): Record<string, unknown> {
  if (typeof value === 'string') {
    throwLegacyPinTypeTokenUnsupported(path);
  }
  if (!isRecord(value)) {
    throwInvalidStructuredPinType(
      code,
      path,
      code === 'invalid_pin_type' ? invalidPinTypeMessage(path) : invalidPinTypeEvidenceMessage(path),
    );
  }

  const parsed = BlueprintPinTypeSpecSchema.safeParse(value);
  if (!parsed.success) {
    const firstIssue = parsed.error.issues[0];
    throw new TaskSpecCompileError(
      code,
      code === 'invalid_pin_type' ? invalidPinTypeMessage(path) : invalidPinTypeEvidenceMessage(path),
      [{
        code,
        path: joinIssuePath(path, firstIssue?.path ?? []),
        message: firstIssue?.message ?? (code === 'invalid_pin_type'
          ? invalidPinTypeMessage(path)
          : invalidPinTypeEvidenceMessage(path)),
      }],
    );
  }
  return parsed.data as Record<string, unknown>;
}

function requireStructuredResultTypeProofEvidence(value: unknown, path: string): Record<string, unknown> {
  if (typeof value === 'string') {
    throwLegacyPinTypeTokenUnsupported(path);
  }
  if (!isRecord(value)) {
    throwInvalidStructuredPinType('invalid_pin_type_evidence', path, `${path} must be an object with pin_type evidence.`);
  }
  if (!Object.hasOwn(value, 'pin_type')) {
    throwInvalidStructuredPinType('invalid_pin_type_evidence', `${path}.pin_type`, `${path}.pin_type is required.`);
  }
  return {
    ...value,
    pin_type: requireStructuredPinType(value.pin_type, `${path}.pin_type`, 'invalid_pin_type_evidence'),
  };
}

function normalizeStructuredPinTypeEvidence(
  evidence: Record<string, unknown>,
  path: string,
): Record<string, unknown> {
  const normalized: Record<string, unknown> = { ...evidence };
  GRAPHWRITE_STRUCTURED_PIN_TYPE_EVIDENCE_FIELDS.forEach((field) => {
    if (Object.hasOwn(normalized, field)) {
      normalized[field] = requireStructuredPinType(normalized[field], `${path}.${field}`, 'invalid_pin_type_evidence');
    }
  });
  if (Object.hasOwn(normalized, GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY)) {
    normalized[GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY] = requireStructuredResultTypeProofEvidence(
      normalized[GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY],
      `${path}.${GRAPHWRITE_RESULT_TYPE_PROOF_EVIDENCE_KEY}`,
    );
  }
  return normalized;
}

function validateStructuredGraphWritePinTypeUsage(record: Record<string, unknown>, path: string): void {
  GRAPHWRITE_STRUCTURED_PIN_TYPE_FIELDS.forEach((field) => {
    if (Object.hasOwn(record, field)) {
      requireStructuredPinType(record[field], `${path}.${field}`);
    }
  });
  const evidence = record.context_evidence;
  if (isRecord(evidence)) {
    normalizeStructuredPinTypeEvidence(evidence, `${path}.context_evidence`);
  }
}

function copyStructuredPinTypeFields(source: Record<string, unknown>, target: Record<string, unknown>, path = ''): void {
  GRAPHWRITE_STRUCTURED_PIN_TYPE_FIELDS.forEach((field) => {
    if (Object.hasOwn(source, field)) {
      target[field] = requireStructuredPinType(source[field], path ? `${path}.${field}` : field);
    }
  });
}

function copyContainerActionSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>): void {
  GRAPH_CONTAINER_ACTION_FIELDS.forEach((field) => {
    if (Object.hasOwn(source, field)) {
      target[field] = source[field];
    }
  });
}

function copyContextEvidence(source: Record<string, unknown>, target: Record<string, unknown>, path = 'context_evidence'): void {
  const evidence = source['context_evidence'];
  if (isRecord(evidence)) {
    target['context_evidence'] = normalizeStructuredPinTypeEvidence(evidence, path);
  }
}

export function defaultFieldOwnerClassForBlueprintAsset(assetPath: string): string | undefined {
  const normalizedAssetPath = assetPath.trim();
  if (normalizedAssetPath.length === 0) return undefined;
  if (/\/[^/]+\.[^/.]+_C$/.test(normalizedAssetPath)) {
    return normalizedAssetPath;
  }

  if (normalizedAssetPath.includes('.')) {
    return `${normalizedAssetPath}_C`;
  }

  const assetName = normalizedAssetPath.split('/').filter((segment) => segment.length > 0).at(-1);
  return assetName ? `${normalizedAssetPath}.${assetName}_C` : undefined;
}

function applyDefaultFieldOwnerEvidence(
  record: Record<string, unknown>,
  operation: string,
  scope: string,
  options: LogicCloneOptions,
): void {
  if (operation !== 'get' || scope !== 'variable' || !options.defaultFieldOwnerClass) {
    return;
  }
  const graphLocalName = fieldGetSymbolName(record);
  if (graphLocalName && options.graphLocalSymbols?.has(graphLocalName.toLowerCase())) {
    return;
  }

  const evidence = isRecord(record.context_evidence) ? { ...record.context_evidence } : {};
  if (!Object.hasOwn(evidence, 'field_owner_class')) {
    evidence.field_owner_class = options.defaultFieldOwnerClass;
  }
  record.context_evidence = evidence;
}

function fieldGetSymbolName(record: Record<string, unknown>): string | undefined {
  return optionalString(record, 'name') ?? optionalString(record, 'target');
}

function registerGraphLocalSymbols(statement: BlueprintLogicStatement, options: LogicCloneOptions): void {
  const statementRecord = statement as Record<string, unknown>;
  if (!options.graphLocalSymbols) {
    options.graphLocalSymbols = new Set<string>();
  }

  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  if (kind === 'let') {
    const name = optionalString(statementRecord, 'name');
    if (name) {
      options.graphLocalSymbols.add(name.toLowerCase());
    }
  }

  const resultSymbol = optionalString(statementRecord, 'result_symbol');
  if (resultSymbol) {
    options.graphLocalSymbols.add(resultSymbol.toLowerCase());
  }
}

function fieldScopeUsesPropertyPath(scope: string): boolean {
  return FIELD_SCOPES_WITH_PROPERTY_PATH.has(scope);
}

function fieldOperationScope(record: Record<string, unknown>, path: string): { operation: string; scope: string } {
  const operation = getRequiredString(record, 'field_operation', `${path}.field_operation`).trim().toLowerCase();
  const scope = getRequiredString(record, 'field_scope', `${path}.field_scope`).trim().toLowerCase();
  if (operation !== 'get' && operation !== 'set') {
    throw new TaskSpecCompileError('unsupported_field_operation', `Unsupported field_operation: ${operation}`, [
      {
        code: 'unsupported_field_operation',
        path: `${path}.field_operation`,
        message: 'Use get or set.',
      },
    ]);
  }
  if (!SUPPORTED_FIELD_SCOPES.has(scope)) {
    throw new TaskSpecCompileError('unsupported_field_scope', `Unsupported field_scope: ${scope}`, [
      {
        code: 'unsupported_field_scope',
        path: `${path}.field_scope`,
        message: 'Use variable, property_path, component_ref, or field_access.',
      },
    ]);
  }
  return { operation, scope };
}

function delegateStatementOperation(statement: Record<string, unknown>): string | undefined {
  const kind = typeof statement.kind === 'string' ? statement.kind : '';
  if (kind === INTERNAL_DELEGATE_STATEMENT_KIND) {
    const operation = typeof statement.delegate_operation === 'string' ? statement.delegate_operation : '';
    return DELEGATE_STATEMENT_OPERATION_KINDS.has(operation) ? operation : undefined;
  }
  const operation = PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.get(kind);
  return operation && operation !== 'component_bound_event' ? operation : undefined;
}

function validateDelegateStatementShape(statement: Record<string, unknown>, path: string): void {
  const kind = typeof statement.kind === 'string' ? statement.kind : '';
  if (kind === 'component_bound_event') {
    getRequiredString(statement, 'component', `${path}.component`);
    getRequiredString(statement, 'delegate', `${path}.delegate`);
    getRequiredString(statement, 'handler', `${path}.handler`);
    return;
  }

  const operation = delegateStatementOperation(statement);
  if (!operation) {
    return;
  }

  getRequiredString(statement, 'target', `${path}.target`);
  getRequiredString(statement, 'delegate', `${path}.delegate`);
  if (operation === 'bind' || operation === 'assign' || operation === 'unbind') {
    getRequiredString(statement, 'handler', `${path}.handler`);
  }
  if (operation === 'call') {
    validateExpressionMap(statement.args, `${path}.args`);
  }
}

function normalizeContainerActionKind(value: unknown, path: string): string {
  const containerKind = getRequiredString({ value }, 'value', path).trim().toLowerCase();
  if (!SUPPORTED_CONTAINER_KINDS.has(containerKind) || !isSupportedContainerActionKind(containerKind)) {
    throw new TaskSpecCompileError('unsupported_container_kind', `Unsupported container_kind: ${containerKind}`, [
      {
        code: 'unsupported_container_kind',
        path,
        message: 'Use array, map, or set.',
      },
    ]);
  }
  return containerKind;
}

function normalizeContainerActionOperation(containerKind: string, value: unknown, path: string): string {
  const containerOperation = getRequiredString({ value }, 'value', path).trim().toLowerCase();
  if (!SUPPORTED_CONTAINER_OPERATIONS.get(containerKind)?.has(containerOperation) || !isSupportedContainerActionOperation(containerKind, containerOperation)) {
    throw new TaskSpecCompileError('unsupported_container_operation', `Unsupported container_operation: ${containerKind}.${containerOperation}`, [
      {
        code: 'unsupported_container_operation',
        path,
        message: 'Unsupported container_operation in first-class V1 container_action.',
      },
    ]);
  }
  return containerOperation;
}

function validateContainerActionShape(
  record: Record<string, unknown>,
  path: string,
  usage: 'statement' | 'expression',
): { containerKind: string; containerOperation: string } {
  const containerKind = normalizeContainerActionKind(record.container_kind, `${path}.container_kind`);
  const containerOperation = normalizeContainerActionOperation(containerKind, record.container_operation, `${path}.container_operation`);
  if (usage === 'expression' && !isValueExpressionContainerActionOperation(containerKind, containerOperation)) {
    throw new TaskSpecCompileError('unsupported_container_operation', `Unsupported container_operation: ${containerKind}.${containerOperation}`, [
      {
        code: 'unsupported_container_operation',
        path: `${path}.container_operation`,
        message: 'Unsupported container_operation for expression container_action with a single result output.',
      },
    ]);
  }
  if (!Object.hasOwn(record, 'target')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'container_action requires target.', [
      {
        code: 'taskspec_semantic_invalid',
        path: `${path}.target`,
        message: 'Provide the target container expression.',
      },
    ]);
  }
  getRequiredContainerActionRoles(containerKind, containerOperation).forEach((role) => {
    if (!Object.hasOwn(record, role)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `container_action ${containerKind}.${containerOperation} requires ${role}.`, [
        {
          code: 'taskspec_semantic_invalid',
          path: `${path}.${role}`,
          message: `container_action ${containerKind}.${containerOperation} requires ${role}.`,
        },
      ]);
    }
  });
  CONTAINER_ACTION_TYPE_FIELDS.forEach((field) => {
    if (!Object.hasOwn(record, field)) return;
    const value = record[field];
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.${field} must be a non-empty string.`, [
        {
          code: 'taskspec_semantic_invalid',
          path: `${path}.${field}`,
          message: `${path}.${field} must be a non-empty string.`,
        },
      ]);
    }
  });
  if (Object.hasOwn(record, 'result_symbol')) {
    getRequiredString(record, 'result_symbol', `${path}.result_symbol`);
    if (!isValueExpressionContainerActionOperation(containerKind, containerOperation)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'result_symbol is only supported for query container_action operations with a single result output.', [
        {
          code: 'taskspec_semantic_invalid',
          path: `${path}.result_symbol`,
          message: 'result_symbol is only supported for query container_action operations with a single result output.',
        },
      ]);
    }
  }
  return { containerKind, containerOperation };
}

function normalizeContainerActionRoleValue(role: (typeof CONTAINER_ACTION_ROLE_FIELDS)[number], value: unknown): unknown {
  if (role === 'target' && typeof value === 'string' && value.trim().length > 0) {
    return { kind: 'get', name: value.trim() };
  }
  return value;
}

function normalizeContainerActionRoleValueForFlow(role: (typeof CONTAINER_ACTION_ROLE_FIELDS)[number], value: unknown): unknown {
  const normalizedValue = normalizeContainerActionRoleValue(role, value);
  if (
    isRecord(normalizedValue)
    && (normalizedValue.kind === 'get' || normalizedValue.kind === 'field')
    && typeof normalizedValue.name === 'string'
    && normalizedValue.name.trim().length > 0
    && !Object.hasOwn(normalizedValue, 'target')
  ) {
    return { ...normalizedValue, target: normalizedValue.name.trim() };
  }
  return normalizedValue;
}

function validateContainerActionRoleExpressions(record: Record<string, unknown>, path: string): void {
  CONTAINER_ACTION_ROLE_FIELDS.forEach((role) => {
    if (!Object.hasOwn(record, role)) return;
    const value = normalizeContainerActionRoleValue(role, record[role]);
    if (role === 'items' && Array.isArray(value)) {
      validateExpressionList(value, `${path}.${role}`);
      return;
    }
    validateSupportedExpression(value, `${path}.${role}`);
  });
}

function statementKindSupportsResultSymbol(kind: string): boolean {
  return VALUE_PRODUCING_STATEMENT_KINDS.has(kind);
}

function statementResultSymbolRequiresOutputEvidence(kind: string): boolean {
  return kind === 'call' || kind === 'schedule';
}

function hasExplicitResultOutputEvidence(record: Record<string, unknown>): boolean {
  const topLevelTypeFields = ['value_type', 'result_type', 'output_type', 'return_type'];
  if (topLevelTypeFields.some((field) => optionalString(record, field))) {
    return true;
  }
  if (Object.hasOwn(record, 'pin_type') || Object.hasOwn(record, 'result_pin_type') || Object.hasOwn(record, 'return_pin_type')) {
    return true;
  }
  const evidence = record.context_evidence;
  if (isRecord(evidence)) {
    const evidenceTypeFields = [
      'value_type',
      'result_type',
      'output_type',
      'return_type',
    ];
    return evidenceTypeFields.some((field) => optionalString(evidence, field))
      || GRAPHWRITE_STRUCTURED_PIN_TYPE_EVIDENCE_FIELDS.some((field) => Object.hasOwn(evidence, field))
      || Object.hasOwn(evidence, 'result_pin')
      || Object.hasOwn(evidence, 'return_pin')
      || Object.hasOwn(evidence, 'output_pin');
  }
  return false;
}

function validateStatementResultSymbol(record: Record<string, unknown>, path: string): void {
  if (!Object.hasOwn(record, 'result_symbol')) return;

  getRequiredString(record, 'result_symbol', `${path}.result_symbol`);
  const kind = typeof record.kind === 'string' ? record.kind : '';
  if (!statementKindSupportsResultSymbol(kind)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'result_symbol requires a value-producing statement.', [
      {
        code: 'taskspec_semantic_invalid',
        path: `${path}.result_symbol`,
        message: 'Use result_symbol only on call, create, convert, schedule, or query container_action statements.',
      },
    ]);
  }
  if (statementResultSymbolRequiresOutputEvidence(kind) && !hasExplicitResultOutputEvidence(record)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'result_symbol requires explicit result output evidence.', [
      {
        code: 'taskspec_semantic_invalid',
        path: `${path}.result_symbol`,
        message: 'call and schedule result_symbol require value_type, pin_type, or context_evidence that identifies a data output.',
      },
    ]);
  }
}

export function validateSupportedStatements(statements: BlueprintLogicStatement[], path: string): void {
  statements.forEach((statement, statementIndex) => {
    const statementRecord = statement as Record<string, unknown>;
    const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
    const statementPath = `${path}[${statementIndex}]`;
    validateStructuredGraphWritePinTypeUsage(statementRecord, statementPath);
    if (FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS.has(kind)) {
      throw new TaskSpecCompileError('unsupported_statement_kind', 'Unsupported GraphWrite statement kind.', [
        {
          code: 'unsupported_statement_kind',
          path: `${statementPath}.kind`,
          message: 'Use component_bound_event or delegate.bind/delegate.assign/delegate.unbind/delegate.unbind_all/delegate.call in Agent-facing TaskSpec. The compiler owns kind=delegate + delegate_operation lowering.',
        },
      ]);
    }
    if (kind === 'branch' || kind === 'return' || kind === 'sequence') {
      throw new TaskSpecCompileError('unsupported_statement_kind', 'Unsupported GraphWrite statement kind.', [
        {
          code: 'unsupported_statement_kind',
          path: `${statementPath}.kind`,
          message: 'Use kind=control with control=branch, control=return, or control=sequence.',
        },
      ]);
    }
    const delegateOperation = delegateStatementOperation(statementRecord);
    const initialControlKind = kind === 'control' ? getControlStatementKind(statementRecord, statementPath) : undefined;
    graphWriteStatementCompilerRegistry.requireForStatement({
      kind,
      path: statementPath,
      controlKind: initialControlKind,
      delegateOperation,
    });
    validateStatementResultSymbol(statementRecord, statementPath);
    if (PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.has(kind)) {
      validateDelegateStatementShape(statementRecord, statementPath);
    } else if (kind === CONTAINER_ACTION_KIND) {
      validateContainerActionShape(statementRecord, statementPath, 'statement');
      validateContainerActionRoleExpressions(statementRecord, statementPath);
    } else if (kind === 'call') {
      validateExpressionMap(statementRecord.args, `${statementPath}.args`);
    } else if (kind === 'create') {
      validateCreateShape(statementRecord, statementPath);
      validateExpressionMap(statementRecord.args, `${statementPath}.args`);
    } else if (kind === 'convert' || kind === 'schedule') {
      validateConvertScheduleOwnership(statementRecord, statementPath);
      validateExpressionMap(statementRecord.args, `${statementPath}.args`);
    } else if (kind === 'field') {
      const { operation } = fieldOperationScope(statementRecord, statementPath);
      if (operation !== 'set') {
        throw new TaskSpecCompileError('unsupported_field_operation', 'Field statements require field_operation=set.', [
          {
            code: 'unsupported_field_operation',
            path: `${statementPath}.field_operation`,
            message: 'Field statements require field_operation=set.',
          },
        ]);
      }
      validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
    } else if (kind === 'let' || kind === 'set' || kind === 'set_property') {
      validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
    } else if (kind === 'control') {
      const controlKind = initialControlKind ?? getControlStatementKind(statementRecord, statementPath);
      if (controlKind === 'branch') {
        validateSupportedExpression(statementRecord.condition, `${statementPath}.condition`);
        validateSupportedStatements(Array.isArray(statementRecord.then) ? statementRecord.then as BlueprintLogicStatement[] : [], `${statementPath}.then`);
        validateSupportedStatements(Array.isArray(statementRecord['else']) ? statementRecord['else'] as BlueprintLogicStatement[] : [], `${statementPath}.else`);
      } else if (controlKind === 'sequence') {
        if (Array.isArray(statementRecord.statements) && statementRecord.statements.length > 0) {
          throw new TaskSpecCompileError('unsupported_control_shape', 'Unsupported GraphWrite control shape.', [
            {
              code: 'unsupported_control_shape',
              path: `${statementPath}.statements`,
              message: 'Sequence control is an execution-flow node; place following statements after it.',
            },
          ]);
        }
      } else if (isGenericControlKind(controlKind)) {
        if (statementIndex < statements.length - 1) {
          throw new TaskSpecCompileError('unsupported_control_continuation', 'unsupported_control_continuation: Generic control statements do not provide an implicit linear continuation.', [
            {
              code: 'unsupported_control_continuation',
              path: statementPath,
              message: 'Place generic control statements as terminal statements, or use a dedicated control shape with explicit branch/body semantics.',
            },
          ]);
        }
        genericControlContextEvidence(statementRecord, controlKind, statementPath);
        validateExpressionMap(statementRecord.args, `${statementPath}.args`);
        if (Object.hasOwn(statementRecord, 'value')) {
          validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
        }
      } else if (Object.hasOwn(statementRecord, 'value')) {
        validateSupportedExpression(statementRecord.value, `${statementPath}.value`);
      }
    }
  });
}

function getControlStatementKind(statementRecord: Record<string, unknown>, path: string): string {
  const controlKind = normalizeSemanticToken(
    typeof statementRecord.control === 'string'
      ? statementRecord.control
      : statementRecord.control_operation,
  );
  if (SUPPORTED_GRAPH_BODY_CONTROL_KINDS.has(controlKind)) {
    return controlKind;
  }

  throw new TaskSpecCompileError('unsupported_control_kind', 'Unsupported GraphWrite control kind.', [
    {
      code: 'unsupported_control_kind',
      path: `${path}.control`,
      message: 'Use branch, sequence, return, switch_int, switch_string, switch_name, switch_enum, multi_gate, or a supported StandardMacros control operation.',
    },
  ]);
}

function validateExpressionMap(value: unknown, path: string): void {
  if (!isRecord(value)) return;
  for (const [key, expression] of Object.entries(value)) {
    validateSupportedExpression(expression, `${path}.${key}`);
  }
}

function validateExpressionList(value: unknown, path: string): void {
  if (!Array.isArray(value)) return;
  value.forEach((expression, index) => validateSupportedExpression(expression, `${path}[${index}]`));
}

function isExplicitlyImpureCallExpression(expression: Record<string, unknown>): boolean {
  if (expression.is_pure === false || expression.pure === false || expression.is_impure === true) {
    return true;
  }
  const purity = (optionalString(expression, 'purity') ?? optionalString(expression, 'function_purity') ?? '').toLowerCase();
  return purity === 'impure' || purity === 'not_pure' || purity === 'exec';
}

function validateSupportedExpression(expression: unknown, path: string): void {
  if (!isRecord(expression)) return;
  validateStructuredGraphWritePinTypeUsage(expression, path);
  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  graphWriteExpressionCompilerRegistry.requireForExpression({
    kind,
    path,
    capabilityId: optionalString(expression, 'capability_id'),
  });
  if (kind === CONTAINER_ACTION_KIND) {
    validateContainerActionShape(expression, path, 'expression');
    validateContainerActionRoleExpressions(expression, path);
    return;
  }
  if (kind === 'field') {
    const { operation } = fieldOperationScope(expression, path);
    if (operation !== 'get') {
      throw new TaskSpecCompileError('unsupported_field_operation', 'Field expressions require field_operation=get.', [
        {
          code: 'unsupported_field_operation',
          path: `${path}.field_operation`,
          message: 'Field expressions require field_operation=get.',
        },
      ]);
    }
  }
  if (kind === 'create') {
    validateCreateShape(expression, path);
  }
  if (kind === 'convert' || kind === 'schedule') {
    validateConvertScheduleOwnership(expression, path);
  }
  if (kind === 'schedule' || (kind === 'call' && isExplicitlyImpureCallExpression(expression))) {
    throw new TaskSpecCompileError('impure_expression_requires_statement', `impure ${kind} expressions require a statement result_symbol.`, [
      {
        code: 'impure_expression_requires_statement',
        path: `${path}.kind`,
        message: `Use a ${kind} statement with result_symbol, then read the symbol with kind=get.`,
      },
    ]);
  }
  if (kind === 'call' || kind === 'op' || kind === 'construct' || kind === 'deconstruct' || kind === 'create' || kind === 'convert' || kind === 'schedule') {
    validateExpressionMap(expression.args, `${path}.args`);
  }
  if (kind === 'op') {
    if (Object.hasOwn(expression, 'left')) validateSupportedExpression(expression.left, `${path}.left`);
    if (Object.hasOwn(expression, 'right')) validateSupportedExpression(expression.right, `${path}.right`);
  }
  if (kind === 'deconstruct') {
    if (Object.hasOwn(expression, 'source')) validateSupportedExpression(expression.source, `${path}.source`);
    if (Object.hasOwn(expression, 'value')) validateSupportedExpression(expression.value, `${path}.value`);
  }
  if (kind === 'select') {
    validateSupportedExpression(expression.condition, `${path}.condition`);
    validateExpressionList(expression.options, `${path}.options`);
  }
}

function validateCreateShape(record: Record<string, unknown>, path: string): void {
  validateCreateOwnership(record, path);
}

export function compileLogicBodyToImportPayload(
  body: { statements: BlueprintLogicStatement[] },
  prefix: string,
  path: string,
): { nodes: AgentImportNode[]; links: AgentImportLink[] } {
  const flow = compileStatementSequence(body.statements, `${toIdSegment(prefix)}_stmt`, `${path}.statements`, makeCompileFlowContext());
  return { nodes: flow.nodes, links: flow.links };
}

export function compileLogicBodyToSemanticLogicSpec(
  body: { statements: BlueprintLogicStatement[] },
  prefix: string,
  options: LogicCloneOptions = {},
): Record<string, unknown> {
  return {
    schema: 'BlueprintLogicSpec.v2',
    statements: cloneLogicStatementSequenceWithCompiledIds(body.statements, `${toIdSegment(prefix)}_stmt`, options),
  };
}

function statementResultOutputPin(kind: string): string | undefined {
  if (kind === 'create' || kind === 'convert' || kind === 'schedule') {
    return 'value';
  }
  if (kind === 'call') {
    return 'ReturnValue';
  }
  return undefined;
}

function containerActionResultOutputPin(containerKind: string, containerOperation: string): string {
  const outputPin = getContainerActionResultOutputPin(containerKind, containerOperation);
  if (!outputPin) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `container_action ${containerKind}.${containerOperation} has no single result output.`, [
      {
        code: 'container_action_no_single_result_output',
        path: 'container_operation',
        message: 'Use result_symbol only when the container_action has a single result output.',
      },
    ]);
  }
  return outputPin;
}

function makeCompileFlowContext(parent?: CompileFlowContext): CompileFlowContext {
  return {
    symbols: new Map(parent ? parent.symbols : []),
  };
}

function cloneContainerActionRoleExpressionWithCompiledIds(
  expression: unknown,
  nodeId: string,
  options: LogicCloneOptions,
): unknown {
  if (!isRecord(expression)) {
    return expression;
  }
  if (expression.kind === 'get') {
    const out: Record<string, unknown> = { ...expression, id: nodeId };
    copyContextEvidence(expression, out, `${nodeId}.context_evidence`);
    return out;
  }
  return cloneLogicExpressionWithCompiledIds(expression, nodeId, { ...options, defaultFieldOwnerClass: undefined });
}

function cloneContainerActionRoleValue(
  role: (typeof CONTAINER_ACTION_ROLE_FIELDS)[number],
  value: unknown,
  nodeId: string,
  options: LogicCloneOptions,
): unknown {
  const normalizedValue = normalizeContainerActionRoleValue(role, value);
  if (Array.isArray(normalizedValue)) {
    return normalizedValue.map((entry, index) => cloneContainerActionRoleExpressionWithCompiledIds(entry, `${nodeId}_${index + 1}`, options));
  }
  return cloneContainerActionRoleExpressionWithCompiledIds(normalizedValue, nodeId, options);
}

function cloneContainerActionWithCompiledIds(
  record: Record<string, unknown>,
  nodeId: string,
  options: LogicCloneOptions,
): Record<string, unknown> {
  const { containerKind, containerOperation } = validateContainerActionShape(
    record,
    record.kind === CONTAINER_ACTION_KIND ? nodeId : `${nodeId}.container_action`,
    'statement',
  );
  const out: Record<string, unknown> = { kind: CONTAINER_ACTION_KIND, id: nodeId };
  copyContainerActionSemanticFields(record, out);
  out.container_kind = containerKind;
  out.container_operation = containerOperation;
  CONTAINER_ACTION_ROLE_FIELDS.forEach((role) => {
    if (Object.hasOwn(record, role)) {
      out[role] = cloneContainerActionRoleValue(role, record[role], `${nodeId}_${role}`, options);
    }
  });
  copyContextEvidence(record, out, `${nodeId}.context_evidence`);
  return out;
}

function cloneLogicExpressionWithCompiledIds(
  expression: unknown,
  nodeId: string,
  options: LogicCloneOptions = {},
): unknown {
  if (!isRecord(expression)) {
    return expression;
  }

  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  if (kind === 'get') {
    const out: Record<string, unknown> = { ...expression, id: nodeId };
    copyContextEvidence(expression, out, `${nodeId}.context_evidence`);
    applyDefaultFieldOwnerEvidence(out, 'get', 'variable', options);
    return out;
  }
  if (kind === CONTAINER_ACTION_KIND) {
    const { containerKind, containerOperation } = validateContainerActionShape(expression, nodeId, 'expression');
    const out = cloneContainerActionWithCompiledIds(expression, nodeId, options);
    out.container_kind = containerKind;
    out.container_operation = containerOperation;
    return out;
  }
  const out: Record<string, unknown> = { ...expression, id: nodeId };
  copyContextEvidence(expression, out, `${nodeId}.context_evidence`);

  const fieldExpression = FIELD_EXPRESSION_KIND_MAP.get(kind);
  if (fieldExpression) {
    applyFieldTaxonomy(out, fieldExpression.operation, fieldExpression.scope);
    applyDefaultFieldOwnerEvidence(out, fieldExpression.operation, fieldExpression.scope, options);
    if (kind === 'get' && !Object.hasOwn(out, 'target') && typeof out.name === 'string' && out.name.trim().length > 0) {
      out.target = out.name.trim();
    }
    if (kind === 'get_property') {
      const propertyPath = requiredGraphBodyPropertyPath(expression, `${nodeId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
  } else if (kind === 'field') {
    const { operation, scope } = fieldOperationScope(expression, nodeId);
    applyFieldTaxonomy(out, operation, scope);
    applyDefaultFieldOwnerEvidence(out, operation, scope, options);
    if (fieldScopeUsesPropertyPath(scope)) {
      const propertyPath = requiredGraphBodyPropertyPath(expression, `${nodeId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
  } else if (kind === 'select') {
    out.condition = cloneLogicExpressionWithCompiledIds(expression.condition, `${nodeId}_index`, options);
    if (Array.isArray(expression.options)) {
      out.options = expression.options.map((option, index) => cloneLogicExpressionWithCompiledIds(option, `${nodeId}_option_${index}`, options));
    }
  } else if (kind === 'op') {
    if (Object.hasOwn(expression, 'left')) {
      out.left = cloneLogicExpressionWithCompiledIds(expression.left, `${nodeId}_left`, options);
    }
    if (Object.hasOwn(expression, 'right')) {
      out.right = cloneLogicExpressionWithCompiledIds(expression.right, `${nodeId}_right`, options);
    }
    if (isRecord(expression.args)) {
      out.args = Object.fromEntries(
        Object.entries(expression.args).map(([argName, argValue]) => [
          argName,
          cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
        ]),
      );
    }
  } else if (kind === 'construct' && isRecord(expression.args)) {
    out.args = Object.fromEntries(
      Object.entries(expression.args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
      ]),
    );
  } else if (kind === 'deconstruct') {
    if (Object.hasOwn(expression, 'source')) {
      out.source = cloneLogicExpressionWithCompiledIds(expression.source, `${nodeId}_source`, options);
    }
    if (Object.hasOwn(expression, 'value')) {
      out.value = cloneLogicExpressionWithCompiledIds(expression.value, `${nodeId}_value`, options);
    }
    if (isRecord(expression.args)) {
      out.args = Object.fromEntries(
        Object.entries(expression.args).map(([argName, argValue]) => [
          argName,
          cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
        ]),
      );
    }
  } else if (isRecord(expression.args)) {
    out.args = Object.fromEntries(
      Object.entries(expression.args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${nodeId}_${toIdSegment(argName)}`, options),
      ]),
    );
  }
  if (kind === 'create') {
    copyCreateSemanticFields(expression, out, nodeId);
    copyStructuredPinTypeFields(expression, out, nodeId);
  }

  return out;
}

function cloneLogicStatementWithCompiledIds(
  statement: BlueprintLogicStatement,
  statementId: string,
  options: LogicCloneOptions = {},
): BlueprintLogicStatement {
  const statementRecord = statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string'
    ? statementRecord.kind
    : '';
  if (kind === CONTAINER_ACTION_KIND) {
    return cloneContainerActionWithCompiledIds(statementRecord, statementId, options) as BlueprintLogicStatement;
  }
  const out: Record<string, unknown> = { ...statementRecord, id: statementId };
  copyContextEvidence(statementRecord, out, `${statementId}.context_evidence`);
  const delegateOperation = delegateStatementOperation(statementRecord);

  const fieldStatement = FIELD_STATEMENT_KIND_MAP.get(kind);
  if (fieldStatement) {
    applyFieldTaxonomy(out, fieldStatement.operation, fieldStatement.scope);
    if (kind === 'set_property') {
      const propertyPath = requiredGraphBodyPropertyPath(statementRecord, `${statementId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
    out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
  } else if (kind === 'field') {
    const { operation, scope } = fieldOperationScope(statementRecord, statementId);
    applyFieldTaxonomy(out, operation, scope);
    if (fieldScopeUsesPropertyPath(scope)) {
      const propertyPath = requiredGraphBodyPropertyPath(statementRecord, `${statementId}.property_path`);
      out.property_path = propertyPath;
      out.property = propertyPath;
    }
    if (Object.hasOwn(statementRecord, 'value')) {
      out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
    }
  } else if (kind === 'component_bound_event') {
    out.kind = 'component_bound_event';
  } else if (delegateOperation) {
    out.kind = 'delegate';
    out.delegate_operation = delegateOperation;
    if (delegateOperation === 'unbind') {
      out.unbind_mode = 'single';
    } else if (delegateOperation === 'clear') {
      out.unbind_mode = 'all';
    } else if (delegateOperation === 'call' && isRecord(statementRecord.args)) {
      out.args = Object.fromEntries(
        Object.entries(statementRecord.args).map(([argName, argValue]) => [
          argName,
          cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`, options),
        ]),
      );
    }
  } else if (kind === 'let') {
    out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
  } else if ((kind === 'call' || kind === 'create' || kind === 'convert' || kind === 'schedule') && isRecord(statementRecord.args)) {
    const args = statementRecord.args as Record<string, unknown>;
    out.args = Object.fromEntries(
      Object.entries(args).map(([argName, argValue]) => [
        argName,
        cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`, options),
      ]),
    );
    if (kind === 'create') {
      copyCreateSemanticFields(statementRecord, out, statementId);
      copyStructuredPinTypeFields(statementRecord, out, statementId);
    }
  } else if (kind === 'control') {
    const normalizedControlKind = getControlStatementKind(statementRecord, statementId);

    if (normalizedControlKind === 'branch') {
      out.kind = normalizedControlKind;
      delete out.control;
      out.condition = cloneLogicExpressionWithCompiledIds(statementRecord.condition, `${statementId}_condition`, options);
      if (Array.isArray(statementRecord.then)) {
        out.then = cloneLogicStatementSequenceWithCompiledIds(statementRecord.then as BlueprintLogicStatement[], `${statementId}_then`, options);
      }
      if (Array.isArray(statementRecord['else'])) {
        out.else = cloneLogicStatementSequenceWithCompiledIds(statementRecord['else'] as BlueprintLogicStatement[], `${statementId}_else`, options);
      }
    } else if (normalizedControlKind === 'sequence') {
      out.kind = normalizedControlKind;
      delete out.control;
      delete out.statements;
    } else if (normalizedControlKind === 'return') {
      out.kind = normalizedControlKind;
      delete out.control;
      if (Object.hasOwn(statementRecord, 'value')) {
        out.value = cloneLogicExpressionWithCompiledIds(statementRecord.value, `${statementId}_value`, options);
      }
    } else {
      applyGenericControlSemanticFields(statementRecord, out, normalizedControlKind, statementId);
      if (isRecord(statementRecord.args)) {
        out.args = Object.fromEntries(
          Object.entries(statementRecord.args).map(([argName, argValue]) => [
            argName,
            cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`, options),
          ]),
        );
      }
    }
  }
  if (kind === 'create') {
    copyCreateSemanticFields(statementRecord, out, statementId);
    copyStructuredPinTypeFields(statementRecord, out, statementId);
  }

  return out as BlueprintLogicStatement;
}

function cloneLogicStatementSequenceWithCompiledIds(
  statements: BlueprintLogicStatement[],
  idPrefix: string,
  options: LogicCloneOptions = {},
): BlueprintLogicStatement[] {
  const sequenceOptions: LogicCloneOptions = {
    ...options,
    graphLocalSymbols: new Set(options.graphLocalSymbols ?? []),
  };
  return statements.map((statement, statementIndex) => {
    const cloned = cloneLogicStatementWithCompiledIds(statement, `${idPrefix}_${statementIndex + 1}`, sequenceOptions);
    registerGraphLocalSymbols(statement, sequenceOptions);
    return cloned;
  });
}

function compileStatementSequence(
  statements: BlueprintLogicStatement[],
  idPrefix: string,
  path: string,
  context: CompileFlowContext,
): CompiledStatementFlow {
  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  let entry: string | undefined;
  let previousExits: string[] = [];

  statements.forEach((statement, statementIndex) => {
    const statementId = `${idPrefix}_${statementIndex + 1}`;
    const statementPath = `${path}[${statementIndex}]`;
    const flow = dispatchGraphWriteStatementFlow(statement, statementId, statementPath, context);
    nodes.push(...flow.nodes);
    links.push(...flow.links);
    if (!entry) {
      entry = flow.entry;
    }
    const flowEntry = flow.entry;
    if (flowEntry) {
      previousExits.forEach((exit) => {
        links.push({ kind: 'exec', from: exit, to: flowEntry });
      });
      previousExits = flow.exits;
    } else if (!flow.preservePreviousExits) {
      previousExits = flow.exits;
    }
  });

  return { nodes, links, entry, exits: previousExits };
}

function compileContainerActionRoleInputs(
  statementRecord: Record<string, unknown>,
  nodeId: string,
  path: string,
  node: AgentImportNode,
  nodes: AgentImportNode[],
  links: AgentImportLink[],
  context: CompileFlowContext,
): void {
  const inputValues: Record<string, unknown> = {};
  CONTAINER_ACTION_ROLE_FIELDS.forEach((role) => {
    if (!Object.hasOwn(statementRecord, role)) return;
    const rawValue = normalizeContainerActionRoleValueForFlow(role, statementRecord[role]);
    if (role === 'items' && Array.isArray(rawValue)) {
      inputValues[role] = rawValue.map((entry) => literalValue(normalizeContainerActionRoleValueForFlow(role, entry)));
      return;
    }
    const roleFlow = dispatchGraphWriteValueExpression(rawValue, `${nodeId}_${role}`, `${path}.${role}`, context);
    nodes.push(...roleFlow.nodes);
    links.push(...roleFlow.links);
    if (roleFlow.output) {
      links.push({ kind: 'data', from: roleFlow.output, to: `${nodeId}.${role}` });
    } else {
      inputValues[role] = roleFlow.defaultValue;
    }
  });
  node.inputs = {
    ...(node.inputs ?? {}),
    ...inputValues,
  };
}

function isContainerActionPureOperation(containerKind: string, containerOperation: string): boolean {
  return isExpressionContainerActionOperation(containerKind, containerOperation);
}

// Migration guard: new GraphWrite statement kinds must enter graphwrite-slot-source.json and StatementCompilerRegistry.
function dispatchGraphWriteStatementFlow(
  statement: BlueprintLogicStatement,
  nodeId: string,
  path: string,
  context: CompileFlowContext,
): CompiledStatementFlow {
  const statementRecord = statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  const delegateOperation = delegateStatementOperation(statementRecord);
  const controlKind = kind === 'control' ? getControlStatementKind(statementRecord, path) : undefined;
  const statementCompiler = graphWriteStatementCompilerRegistry.requireForStatement({
    kind,
    path,
    controlKind,
    delegateOperation,
  });
  return statementCompiler.compile_flow({
    statement,
    nodeId,
    path,
    context,
    compilerId: statementCompiler.compiler_id,
  });
}

function compileBranchStatementFlowFromCompilerService(input: GraphWriteStatementCompileInput): CompiledStatementFlow {
  return compileBranchStatementFlow(input.statement, input.nodeId, input.path, input.context);
}

function compileReturnStatementFlowFromCompilerService(input: GraphWriteStatementCompileInput): CompiledStatementFlow {
  return compileReturnStatementFlow(input.statement as Record<string, unknown>, input.nodeId, input.path, input.context);
}

function compileSequenceStatementFlowFromCompilerService(input: GraphWriteStatementCompileInput): CompiledStatementFlow {
  return compileSequenceControlStatementFlow(input.statement as Record<string, unknown>, input.nodeId, input.path, input.context);
}

function compileGenericControlStatementFlowFromCompilerService(input: GraphWriteStatementCompileInput): CompiledStatementFlow {
  const node = dispatchGraphWriteStatementNode(input.statement, input.nodeId, input.path);
  return {
    nodes: [node],
    links: [],
    entry: `${input.nodeId}.execute`,
    exits: [`${input.nodeId}.then`],
  };
}

function compileLetStatementFlowFromCompilerService(input: GraphWriteStatementCompileInput): CompiledStatementFlow {
  const statementRecord = input.statement as Record<string, unknown>;
  const name = getRequiredString(statementRecord, 'name', `${input.path}.name`);
  const valueFlow = dispatchGraphWriteValueExpression(
    statementRecord['value'],
    `${input.nodeId}_value`,
    `${input.path}.value`,
    input.context,
  );
  input.context.symbols.set(name.toLowerCase(), {
    output: valueFlow.output,
    defaultValue: valueFlow.defaultValue,
  });
  return {
    nodes: valueFlow.nodes,
    links: valueFlow.links,
    exits: [],
    preservePreviousExits: true,
  };
}

function compileContainerActionStatementFlowFromCompilerService(input: GraphWriteStatementCompileInput): CompiledStatementFlow {
  const statementRecord = input.statement as Record<string, unknown>;
  const { containerKind, containerOperation } = validateContainerActionShape(statementRecord, input.path, 'statement');
  const node = dispatchGraphWriteStatementNode(input.statement, input.nodeId, input.path);
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];
  compileContainerActionRoleInputs(statementRecord, input.nodeId, input.path, node, nodes, links, input.context);
  const resultSymbol = optionalString(statementRecord, 'result_symbol');
  if (resultSymbol) {
    input.context.symbols.set(resultSymbol.toLowerCase(), {
      output: `${input.nodeId}.${containerActionResultOutputPin(containerKind, containerOperation)}`,
    });
  }
  if (isContainerActionPureOperation(containerKind, containerOperation)) {
    return {
      nodes,
      links,
      exits: [],
      preservePreviousExits: true,
    };
  }
  return {
    nodes,
    links,
    entry: `${input.nodeId}.execute`,
    exits: [`${input.nodeId}.then`],
  };
}

function compileDefaultExecStatementFlowFromCompilerService(input: GraphWriteStatementCompileInput): CompiledStatementFlow {
  const {
    statement,
    nodeId,
    path,
    context,
  } = input;
  const statementRecord = statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  const delegateOperation = delegateStatementOperation(statementRecord);
  const node = dispatchGraphWriteStatementNode(statement, nodeId, path);
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];
  if (kind === 'call' || kind === 'create' || kind === 'convert' || kind === 'schedule' || delegateOperation === 'call') {
    const inputValues: Record<string, unknown> = {};
    if (isRecord(statementRecord['args'])) {
      for (const [argName, argValue] of Object.entries(statementRecord['args'])) {
        const argFlow = dispatchGraphWriteValueExpression(
          argValue,
          `${nodeId}_arg_${toIdSegment(argName)}`,
          `${path}.args.${argName}`,
          context,
        );
        nodes.push(...argFlow.nodes);
        links.push(...argFlow.links);
        if (argFlow.output) {
          links.push({ kind: 'data', from: argFlow.output, to: `${nodeId}.${argName}` });
        } else {
          inputValues[argName] = argFlow.defaultValue;
        }
      }
    }
    node.inputs = inputValues;
  }
  if (kind === 'set' || kind === 'set_property' || kind === 'field') {
    let valuePinName = kind === 'set'
      ? getRequiredString(statementRecord, 'target', `${path}.target`)
      : 'value';
    if (kind === 'field') {
      const { operation, scope } = fieldOperationScope(statementRecord, path);
      if (operation !== 'set') {
        throw new TaskSpecCompileError('unsupported_field_operation', 'Field statements require field_operation=set.', [
          {
            code: 'unsupported_field_operation',
            path: `${path}.field_operation`,
            message: 'Field statements require field_operation=set.',
          },
        ]);
      }
      valuePinName = scope === 'variable'
        ? getRequiredString(statementRecord, 'target', `${path}.target`)
        : 'value';
    }
    const valueFlow = dispatchGraphWriteValueExpression(statementRecord['value'], `${nodeId}_value`, `${path}.value`, context);
    nodes.push(...valueFlow.nodes);
    links.push(...valueFlow.links);
    if (valueFlow.output) {
      links.push({ kind: 'data', from: valueFlow.output, to: `${nodeId}.${valuePinName}` });
      delete node.value;
    } else {
      node.value = valueExprToString(valueFlow.defaultValue);
    }
  }
  const resultSymbol = optionalString(statementRecord, 'result_symbol');
  const outputPin = statementResultOutputPin(kind);
  if (resultSymbol && outputPin && statementKindSupportsResultSymbol(kind) && (!statementResultSymbolRequiresOutputEvidence(kind) || hasExplicitResultOutputEvidence(statementRecord))) {
    context.symbols.set(resultSymbol.toLowerCase(), { output: `${nodeId}.${outputPin}` });
  }
  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits: [`${nodeId}.then`],
  };
}

function compileReturnStatementFlow(statementRecord: Record<string, unknown>, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  const node: AgentImportNode = { id: nodeId, kind: 'return' } as AgentImportNode;
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];
  if (Object.hasOwn(statementRecord, 'value')) {
    const valueFlow = dispatchGraphWriteValueExpression(statementRecord.value, `${nodeId}_value`, `${path}.value`, context);
    nodes.push(...valueFlow.nodes);
    links.push(...valueFlow.links);
    if (valueFlow.output) {
      links.push({ kind: 'data', from: valueFlow.output, to: `${nodeId}.value` });
    } else {
      node.value = valueExprToString(valueFlow.defaultValue);
    }
  }
  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits: [],
  };
}

function compileSequenceControlStatementFlow(statementRecord: Record<string, unknown>, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  const sequenceNode: AgentImportNode = { id: nodeId, kind: 'sequence' } as AgentImportNode;
  const nodes: AgentImportNode[] = [sequenceNode];
  const links: AgentImportLink[] = [];
  const nestedStatements = Array.isArray(statementRecord.statements)
    ? statementRecord.statements as BlueprintLogicStatement[]
    : [];
  const nestedFlow = compileStatementSequence(nestedStatements, `${nodeId}_sequence`, `${path}.statements`, makeCompileFlowContext(context));
  nodes.push(...nestedFlow.nodes);
  links.push(...nestedFlow.links);
  if (nestedFlow.entry) {
    links.push({ kind: 'exec', from: `${nodeId}.then`, to: nestedFlow.entry });
  }
  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits: nestedFlow.entry ? nestedFlow.exits : [`${nodeId}.then`],
  };
}

function compileBranchStatementFlow(statement: BlueprintLogicStatement, nodeId: string, path: string, context: CompileFlowContext): CompiledStatementFlow {
  const branchStatement = statement as BlueprintLogicStatement & {
    condition?: unknown;
    then?: unknown;
    else?: unknown;
  };
  const branchNode: AgentImportNode = { id: nodeId, kind: 'branch' };
  const nodes: AgentImportNode[] = [branchNode];
  const links: AgentImportLink[] = [];
  const conditionFlow = compileBranchCondition(branchStatement.condition, `${nodeId}_condition`, `${path}.condition`, context);
  nodes.push(...conditionFlow.nodes);
  links.push(...conditionFlow.links);
  if (conditionFlow.output) {
    links.push({ kind: 'data', from: conditionFlow.output, to: `${nodeId}.Condition` });
  }
  if (conditionFlow.defaultValue !== undefined) {
    branchNode.inputs = { Condition: conditionFlow.defaultValue };
  }

  const thenStatements = Array.isArray(branchStatement.then)
    ? (branchStatement.then as BlueprintLogicStatement[])
    : [];
  const elseStatements = Array.isArray(branchStatement.else)
    ? (branchStatement.else as BlueprintLogicStatement[])
    : [];
  const thenFlow = compileStatementSequence(thenStatements, `${nodeId}_then`, `${path}.then`, makeCompileFlowContext(context));
  const elseFlow = compileStatementSequence(elseStatements, `${nodeId}_else`, `${path}.else`, makeCompileFlowContext(context));
  nodes.push(...thenFlow.nodes, ...elseFlow.nodes);
  links.push(...thenFlow.links, ...elseFlow.links);

  const exits: string[] = [];
  if (thenFlow.entry) {
    links.push({ kind: 'exec', from: `${nodeId}.then`, to: thenFlow.entry });
    exits.push(...thenFlow.exits);
  } else {
    exits.push(`${nodeId}.then`);
  }
  if (elseFlow.entry) {
    links.push({ kind: 'exec', from: `${nodeId}.else`, to: elseFlow.entry });
    exits.push(...elseFlow.exits);
  } else {
    exits.push(`${nodeId}.else`);
  }

  return {
    nodes,
    links,
    entry: `${nodeId}.execute`,
    exits,
  };
}

function compileBranchCondition(condition: unknown, nodeId: string, path: string, context: CompileFlowContext): CompiledConditionFlow {
  return dispatchGraphWriteValueExpression(condition, nodeId, path, context);
}

// Migration guard: new GraphWrite expression kinds must enter graphwrite-slot-source.json and ExpressionCompilerRegistry.
function dispatchGraphWriteValueExpression(
  expression: unknown,
  nodeId: string,
  path: string,
  context: CompileFlowContext,
): CompiledConditionFlow {
  const expressionRecord = isRecord(expression) ? expression : { kind: 'literal', value: expression };
  const kind = typeof expressionRecord.kind === 'string' ? expressionRecord.kind : 'literal';
  const expressionCompiler = graphWriteExpressionCompilerRegistry.requireForExpression({
    kind,
    path,
    capabilityId: optionalString(expressionRecord, 'capability_id'),
  });
  return expressionCompiler.compile({
    expression,
    nodeId,
    path,
    context,
    compilerId: expressionCompiler.compiler_id,
  });
}

function compileLiteralExpressionFromCompilerService(input: GraphWriteExpressionCompileInput): CompiledConditionFlow {
  return { nodes: [], links: [], defaultValue: literalValue(input.expression) };
}

function compileContainerActionExpressionFromCompilerService(input: GraphWriteExpressionCompileInput): CompiledConditionFlow {
  const {
    expression,
    nodeId,
    path,
    context,
  } = input;
  if (!isRecord(expression)) {
    return { nodes: [], links: [], defaultValue: literalValue(expression) };
  }

  const { containerKind, containerOperation } = validateContainerActionShape(expression, path, 'expression');
  const node: AgentImportNode = {
    id: nodeId,
    kind: CONTAINER_ACTION_KIND,
    inputs: {},
  };
  copyContainerActionSemanticFields(expression, node as Record<string, unknown>);
  (node as Record<string, unknown>).container_kind = containerKind;
  (node as Record<string, unknown>).container_operation = containerOperation;
  copyContextEvidence(expression, node as Record<string, unknown>, `${path}.context_evidence`);
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];
  compileContainerActionRoleInputs(expression, nodeId, path, node, nodes, links, context);
  return { nodes, links, output: `${nodeId}.${containerActionResultOutputPin(containerKind, containerOperation)}` };
}

function compileFieldGetExpressionFromCompilerService(input: GraphWriteExpressionCompileInput): CompiledConditionFlow {
  const {
    expression,
    nodeId,
    path,
    context,
  } = input;
  if (!isRecord(expression)) {
    return { nodes: [], links: [], defaultValue: literalValue(expression) };
  }

  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  const target = kind === 'get'
    ? (optionalString(expression, 'target') ?? getRequiredString(expression, 'name', `${path}.name`))
    : getRequiredString(expression, 'target', `${path}.target`);
  const fieldExpression = kind === 'field'
    ? fieldOperationScope(expression, path)
    : FIELD_EXPRESSION_KIND_MAP.get(kind);
  if (!fieldExpression || fieldExpression.operation !== 'get') {
    throw new TaskSpecCompileError('unsupported_field_operation', 'Field expressions require field_operation=get.', [
      {
        code: 'unsupported_field_operation',
        path: `${path}.field_operation`,
        message: 'Field expressions require field_operation=get.',
      },
    ]);
  }
  if (fieldExpression.scope === 'variable') {
    const symbol = context.symbols.get(target.toLowerCase());
    if (symbol) {
      return { nodes: [], links: [], output: symbol.output, defaultValue: symbol.defaultValue };
    }
  }
  const outputPin = fieldScopeUsesPropertyPath(fieldExpression.scope) ? 'value' : target;
  const node = { id: nodeId, kind: 'field', var: target, target } as AgentImportNode;
  copyContextEvidence(expression, node as Record<string, unknown>, `${path}.context_evidence`);
  applyFieldTaxonomy(node as Record<string, unknown>, fieldExpression.operation, fieldExpression.scope);
  if (fieldScopeUsesPropertyPath(fieldExpression.scope)) {
    const propertyPath = requiredGraphBodyPropertyPath(expression, path);
    (node as Record<string, unknown>).property_path = propertyPath;
    (node as Record<string, unknown>).property = propertyPath;
  }
  return { nodes: [node], links: [], output: `${nodeId}.${outputPin}` };
}

function compileGeneralExpressionFromCompilerService(input: GraphWriteExpressionCompileInput): CompiledConditionFlow {
  const {
    expression,
    nodeId,
    path,
    context,
  } = input;
  if (!isRecord(expression)) {
    return { nodes: [], links: [], defaultValue: literalValue(expression) };
  }

  const nodes: AgentImportNode[] = [];
  const links: AgentImportLink[] = [];
  const kind = typeof expression.kind === 'string' ? expression.kind : 'literal';
  const node: AgentImportNode = {
    id: nodeId,
    kind,
    inputs: {},
  };
  copyContextEvidence(expression, node as Record<string, unknown>, `${path}.context_evidence`);
  if (kind === 'call') {
    node.function = getRequiredString(expression, 'target', `${path}.target`);
  }
  if (kind === 'op') {
    node.function = getRequiredString(expression, 'op', `${path}.op`);
    if (Object.hasOwn(expression, 'left')) {
      compileExpressionInput(expression['left'], 'A', `${nodeId}_left`, `${path}.left`, node, nodes, links, context);
    }
    if (Object.hasOwn(expression, 'right')) {
      compileExpressionInput(expression['right'], 'B', `${nodeId}_right`, `${path}.right`, node, nodes, links, context);
    }
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (kind === 'select') {
    if (Array.isArray(expression.options)) {
      expression.options.forEach((option, index) => {
        compileExpressionInput(option, `Option${index}`, `${nodeId}_option_${index}`, `${path}.options[${index}]`, node, nodes, links, context);
      });
    }
    compileExpressionInput(expression['condition'], 'Index', `${nodeId}_index`, `${path}.condition`, node, nodes, links, context);
  } else if (kind === 'construct') {
    const structType = requiredConstructType(expression, path);
    (node as Record<string, unknown>).type = structType;
    (node as Record<string, unknown>).struct_path = structType;
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (kind === 'deconstruct') {
    const structType = optionalString(expression, 'type') ?? optionalString(expression, 'struct_path');
    if (structType) {
      (node as Record<string, unknown>).type = structType;
      (node as Record<string, unknown>).struct_path = structType;
    }
    const propertyPath = optionalGraphBodyPropertyPath(expression);
    if (propertyPath) {
      (node as Record<string, unknown>).property_path = propertyPath;
      (node as Record<string, unknown>).property = propertyPath;
    }
    if (Object.hasOwn(expression, 'source')) {
      compileExpressionInput(expression['source'], 'Input', `${nodeId}_source`, `${path}.source`, node, nodes, links, context);
    } else if (Object.hasOwn(expression, 'value')) {
      compileExpressionInput(expression['value'], 'Input', `${nodeId}_value`, `${path}.value`, node, nodes, links, context);
    }
  } else if (kind === 'create') {
    node.create_operation = getRequiredString(expression, 'create_operation', `${path}.create_operation`);
    copyCreateSemanticFields(expression, node as Record<string, unknown>, path);
    const target = optionalString(expression, 'target');
    const classPath = optionalString(expression, 'class_path');
    const assetPath = optionalString(expression, 'asset_path');
    if (target) node.target = target;
    if (classPath) node.class_path = classPath;
    if (assetPath) node.asset_path = assetPath;
    copyStructuredPinTypeFields(expression, node as Record<string, unknown>, path);
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (kind === 'convert' || kind === 'schedule') {
    copyConvertScheduleSemanticFields(expression, node as Record<string, unknown>);
    const target = optionalString(expression, 'target');
    if (target) node.target = target;
    if (isRecord(expression.args)) {
      for (const [argName, argValue] of Object.entries(expression.args)) {
        compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
      }
    }
  } else if (isRecord(expression.args)) {
    for (const [argName, argValue] of Object.entries(expression.args)) {
      compileExpressionInput(argValue, argName, `${nodeId}_${toIdSegment(argName)}`, `${path}.args.${argName}`, node, nodes, links, context);
    }
  }
  nodes.unshift(node);

  const outputPin = kind === 'construct' || kind === 'deconstruct' || kind === 'select' || kind === 'create' || kind === 'convert' || kind === 'schedule' ? 'value' : 'ReturnValue';
  return { nodes, links, output: `${nodeId}.${outputPin}` };
}

function optionalGraphBodyPropertyPath(record: Record<string, unknown>): string | undefined {
  return optionalString(record, 'property_path') ?? optionalString(record, 'property');
}

function requiredGraphBodyPropertyPath(record: Record<string, unknown>, path: string): string {
  const propertyPath = optionalGraphBodyPropertyPath(record);
  if (propertyPath) return propertyPath;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.property_path must be a non-empty string.`, [
    {
      code: 'missing_property_path',
      path: `${path}.property_path`,
      message: 'Provide property_path for graph-body property access.',
    },
  ]);
}

function requiredConstructType(record: Record<string, unknown>, path: string): string {
  const structType = optionalString(record, 'type') ?? optionalString(record, 'struct_path');
  if (structType) return structType;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}.type must be a non-empty string.`, [
    {
      code: 'missing_construct_type',
      path: `${path}.type`,
      message: 'Provide type for construct expressions.',
    },
  ]);
}

function compileExpressionInput(
  expression: unknown,
  pinName: string,
  nodeId: string,
  path: string,
  targetNode: AgentImportNode,
  nodes: AgentImportNode[],
  links: AgentImportLink[],
  context: CompileFlowContext,
): void {
  const valueFlow = dispatchGraphWriteValueExpression(expression, nodeId, path, context);
  nodes.push(...valueFlow.nodes);
  links.push(...valueFlow.links);
  if (valueFlow.output) {
    links.push({ kind: 'data', from: valueFlow.output, to: `${targetNode.id}.${pinName}` });
  } else {
    targetNode.inputs = targetNode.inputs ?? {};
    targetNode.inputs[pinName] = valueFlow.defaultValue;
  }
}

export function normalizeReplaceSelector(
  replaceScope: string,
  selector: Record<string, unknown>,
  descriptor: Parameters<typeof normalizeSelectorWithDescriptor>[0],
): Record<string, unknown> {
  return normalizeSelectorWithDescriptor(descriptor, selector, 'behavior.replace.selector', replaceScope);
}

export function normalizeMergeAnchor(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  assertBlockScopedGraphWriteRef(anchor, path);
  getRequiredString(anchor, 'node_ref', `${path}.node_ref`);
  getRequiredString(anchor, 'pin_ref', `${path}.pin_ref`);
  return { ...anchor };
}

function normalizeExternalGraphAnchorBase(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  const schema = getRequiredString(anchor, 'schema', `${path}.schema`);
  if (schema !== 'BlueprintHelper.ExternalGraphAnchor.v1') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite requires BlueprintHelper.ExternalGraphAnchor.v1.', [
      {
        code: 'unsupported_external_graph_anchor',
        path: `${path}.schema`,
        message: 'Use an external_anchor emitted by blueprinthelper_read_context.',
      },
    ]);
  }

  const semanticRole = getRequiredString(anchor, 'semantic_role', `${path}.semantic_role`);
  assertAllowedString(
    semanticRole,
    `${path}.semantic_role`,
    ['exec_boundary', 'node', 'body_entry'],
    'Use exec_boundary, node, or body_entry.',
  );

  const nodeGuid = getRequiredString(anchor, 'node_guid', `${path}.node_guid`);
  if (!/^[0-9a-fA-F]{32}$/u.test(nodeGuid)) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite anchor node_guid must be a stable UE GUID.', [
      {
        code: 'unsupported_external_graph_anchor_node_guid',
        path: `${path}.node_guid`,
        message: 'Do not use nodes[index], display names, or JSONPath selectors for external graph writes.',
      },
    ]);
  }

  const out = {
    schema,
    asset_path: getRequiredString(anchor, 'asset_path', `${path}.asset_path`),
    graph_name: getRequiredString(anchor, 'graph_name', `${path}.graph_name`),
    node_guid: nodeGuid,
    node_class: getRequiredString(anchor, 'node_class', `${path}.node_class`),
    semantic_role: semanticRole,
    fingerprint: getRequiredString(anchor, 'fingerprint', `${path}.fingerprint`),
  } as Record<string, unknown>;
  if (typeof anchor['pin_name'] === 'string' && anchor['pin_name'].trim().length > 0) {
    out['pin_name'] = anchor['pin_name'].trim();
  }
  if (typeof anchor['pin_direction'] === 'string' && anchor['pin_direction'].trim().length > 0) {
    const pinDirection = anchor['pin_direction'].trim();
    assertAllowedString(pinDirection, `${path}.pin_direction`, ['input', 'output'], 'Use input or output.');
    out['pin_direction'] = pinDirection;
  }
  return out;
}

function normalizeLogicJsonAnchorSelector(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  const schema = getRequiredString(anchor, 'schema', `${path}.schema`);
  if (schema !== 'BlueprintHelper.LogicJsonAnchorSelector.v1') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'External GraphWrite requires BlueprintHelper.ExternalGraphAnchor.v1 or BlueprintHelper.LogicJsonAnchorSelector.v1.', [
      {
        code: 'unsupported_external_graph_anchor',
        path: `${path}.schema`,
        message: 'Use an external_anchor or LogicJson anchor selector emitted by blueprinthelper_read_context.',
      },
    ]);
  }

  const nodeRef = optionalString(anchor, 'node_ref');
  const linkRef = optionalString(anchor, 'link_ref');
  if (Boolean(nodeRef) === Boolean(linkRef)) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson anchor selector requires exactly one of node_ref or link_ref.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path,
        message: 'Set exactly one of node_ref or link_ref.',
      },
    ]);
  }
  if (nodeRef && !optionalString(anchor, 'pin_ref')) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson node_ref selector requires pin_ref for merge_external_flow.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path: `${path}.pin_ref`,
        message: 'Set pin_ref to identify the exec output boundary on the selected node.',
      },
    ]);
  }
  const graphNameField = optionalString(anchor, 'graph_name');
  const graphAliasField = optionalString(anchor, 'graph');
  if (graphNameField && graphAliasField && graphNameField !== graphAliasField) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson anchor selector graph and graph_name must match when both are provided.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path: `${path}.graph`,
        message: 'Remove one field or make graph and graph_name match.',
      },
    ]);
  }
  const graphName = graphNameField ?? graphAliasField;
  if (!graphName) {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'LogicJson anchor selector requires graph_name or graph.', [
      {
        code: 'unsupported_logic_json_anchor_selector',
        path: `${path}.graph_name`,
        message: 'Set graph_name, or pass the graph field from logic_json.',
      },
    ]);
  }

  return omitUndefined({
    schema,
    asset_path: getRequiredString(anchor, 'asset_path', `${path}.asset_path`),
    graph_name: graphName,
    entry_name: optionalString(anchor, 'entry_name'),
    node_ref: nodeRef,
    link_ref: linkRef,
    pin_ref: optionalString(anchor, 'pin_ref'),
  });
}

export function normalizeExternalExecBoundaryAnchor(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  if (anchor['schema'] === 'BlueprintHelper.LogicJsonAnchorSelector.v1') {
    return normalizeLogicJsonAnchorSelector(anchor, path);
  }

  const out = normalizeExternalGraphAnchorBase(anchor, path);
  if (out['semantic_role'] !== 'exec_boundary') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'merge_external_flow requires an exec_boundary external anchor.', [
      {
        code: 'unsupported_external_graph_anchor_role',
        path: `${path}.semantic_role`,
        message: 'Use semantic_role="exec_boundary".',
      },
    ]);
  }

  const pinDirection = getRequiredString(out, 'pin_direction', `${path}.pin_direction`);
  if (pinDirection !== 'output') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'merge_external_flow requires an output exec boundary anchor.', [
      {
        code: 'unsupported_external_graph_anchor_pin_direction',
        path: `${path}.pin_direction`,
        message: 'Use pin_direction="output".',
      },
    ]);
  }

  return {
    ...out,
    pin_name: getRequiredString(out, 'pin_name', `${path}.pin_name`),
    pin_direction: pinDirection,
  };
}

export function normalizeExternalBodyEntryAnchor(anchor: Record<string, unknown>, path: string): Record<string, unknown> {
  const out = normalizeExternalGraphAnchorBase(anchor, path);
  if (out['semantic_role'] !== 'body_entry') {
    throw new TaskSpecCompileError('unsupported_external_graph_anchor', 'replace_external_body requires a body_entry external anchor.', [
      {
        code: 'unsupported_external_graph_anchor_role',
        path: `${path}.semantic_role`,
        message: 'Use semantic_role="body_entry".',
      },
    ]);
  }
  return out;
}

function assertBlockScopedGraphWriteRef(ref: Record<string, unknown>, path: string): void {
  const blockId = ref['block_id'];
  if (typeof blockId === 'string' && isRawLogicJsonArrayRef(blockId)) {
    throwUnsupportedGraphWriteAnchor(
      `${path}.block_id`,
      `${path}.block_id uses a read-view array index. Use a stable BlueprintHelper-owned block_id.`,
    );
  }

  for (const field of ['node_ref', 'pin_ref', 'link_ref']) {
    const value = ref[field];
    if (typeof value === 'string' && isRawLogicJsonArrayRef(value)) {
      throwUnsupportedGraphWriteAnchor(
        `${path}.${field}`,
        `${path}.${field} uses a read-view array index. Use block_id with group-local node_ref/pin_ref/link_ref.`,
      );
    }
  }

  const hasBlockId = typeof ref['block_id'] === 'string' && ref['block_id'].trim().length > 0;
  if (hasBlockId) return;

  throwUnsupportedGraphWriteAnchor(
    path,
    `${path} must identify a BlueprintHelper-owned block with block_id.`,
  );
}

function isRawLogicJsonArrayRef(value: string): boolean {
  return /^(nodes|pins|links)\[\d+\]$/u.test(value.trim());
}

function throwUnsupportedGraphWriteAnchor(path: string, message: string): never {
  throw new TaskSpecCompileError('unsupported_graph_write_anchor', 'GraphWrite patch/merge requires a block-scoped anchor.', [
    {
      code: 'unsupported_graph_write_anchor',
      path,
      message,
    },
  ]);
}

export function normalizeMergeInserted(mergeScope: string, inserted: Record<string, unknown>, path: string): Record<string, unknown> {
  const expectedCallKind = mergeScope;
  const callKind = getRequiredString(inserted, 'call_kind', `${path}.call_kind`);
  if (callKind !== expectedCallKind) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'merge inserted.call_kind must match merge scope.', [
      {
        code: 'merge_inserted_scope_mismatch',
        path: `${path}.call_kind`,
        message: `${mergeScope} requires inserted.call_kind="${expectedCallKind}".`,
      },
    ]);
  }
  if (mergeScope === 'function_call') {
    return { function: getRequiredString(inserted, 'name', `${path}.name`) };
  }
  if (mergeScope === 'custom_event_call') {
    return { custom_event: getRequiredString(inserted, 'name', `${path}.name`) };
  }
  return omitUndefined({
    block_id: getRequiredString(inserted, 'block_id', `${path}.block_id`),
    block_ref: typeof inserted['block_ref'] === 'string' && inserted['block_ref'].length > 0 ? inserted['block_ref'] : undefined,
  });
}

export function normalizeMergeSequenceOrder(record: Record<string, unknown>, insertStrategy: string, path: string): string[] | undefined {
  const raw = record['sequence_order'];
  if (insertStrategy !== 'branch_fork') {
    if (raw !== undefined) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'sequence_order is only valid for branch_fork.', [
        {
          code: 'sequence_order_not_allowed',
          path,
          message: 'Remove sequence_order unless insert_strategy is branch_fork.',
        },
      ]);
    }
    return undefined;
  }
  if (!Array.isArray(raw) || raw.length === 0) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'branch_fork requires sequence_order.', [
      {
        code: 'sequence_order_required',
        path,
        message: 'Provide sequence_order using inserted_logic and original_successor.',
      },
    ]);
  }
  const sequenceOrder = raw.map((value, index) => {
    if (typeof value === 'string' && (value === 'inserted_logic' || value === 'original_successor')) {
      return value;
    }
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Invalid branch_fork sequence_order entry.', [
      {
        code: 'sequence_order_invalid',
        path: `${path}[${index}]`,
        message: 'Use inserted_logic or original_successor.',
      },
    ]);
  });
  const uniqueEntries = new Set(sequenceOrder);
  if (sequenceOrder.length > 2 || uniqueEntries.size !== sequenceOrder.length) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'branch_fork sequence_order entries must be unique.', [
      {
        code: 'sequence_order_invalid',
        path,
        message: 'Provide each branch_fork sequence_order entry at most once.',
      },
    ]);
  }
  if (!sequenceOrder.includes('inserted_logic')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'branch_fork sequence_order must include inserted_logic.', [
      {
        code: 'sequence_order_invalid',
        path,
        message: 'Include inserted_logic.',
      },
    ]);
  }
  return sequenceOrder;
}

function copyOptionalStringFields(source: Record<string, unknown>, target: Record<string, unknown>, fields: string[]): void {
  fields.forEach((field) => {
    if (typeof source[field] === 'string' && source[field].length > 0) {
      target[field] = source[field];
    }
  });
}

export function assertAllowedString(value: string, path: string, allowed: string[], message: string): void {
  if (allowed.includes(value)) return;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} is not supported.`, [
    {
      code: 'unsupported_field_value',
      path,
      message,
    },
  ]);
}

function assertExactString(
  record: Record<string, unknown>,
  field: string,
  expected: string,
  path: string,
  message: string,
): void {
  const actual = getRequiredString(record, field, path);
  if (actual === expected) return;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be ${expected}.`, [
    {
      code: 'unsupported_field_value',
      path,
      message,
    },
  ]);
}

function requiredArray(record: Record<string, unknown>, field: string, path: string): unknown[] {
  const value = record[field];
  if (Array.isArray(value) && value.length > 0) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty array.`, [
    {
      code: 'missing_required_array',
      path,
      message: `Provide at least one item in ${path}.`,
    },
  ]);
}

function stringArrayOrEmpty(value: unknown, path: string): string[] {
  if (value === undefined || value === null) return [];
  if (!Array.isArray(value)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an array.`, [
      {
        code: 'invalid_string_array',
        path,
        message: `${path} must contain path strings.`,
      },
    ]);
  }
  return value.map((item, index) => {
    if (typeof item !== 'string' || item.length === 0) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}] must be a non-empty string.`, [
        {
          code: 'invalid_string_array_item',
          path: `${path}[${index}]`,
          message: `${path}[${index}] must be a non-empty string.`,
        },
      ]);
    }
    return item;
  });
}

function classSettingsDefaultArray(rawSettings: unknown, path: string): Record<string, unknown>[] {
  if (rawSettings === undefined || rawSettings === null) return [];
  if (!Array.isArray(rawSettings)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an array.`, [
      {
        code: 'invalid_property_settings',
        path,
        message: 'Use an array of { property_path, value } settings.',
      },
    ]);
  }
  return rawSettings.map((rawSetting, index) => {
    if (!isRecord(rawSetting)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}] must be an object.`, [
        {
          code: 'invalid_property_setting',
          path: `${path}[${index}]`,
          message: 'Use { property_path, value }.',
        },
      ]);
    }
    const setting = rawSetting as Record<string, unknown>;
    if (!Object.hasOwn(setting, 'value')) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path}[${index}].value is required.`, [
        {
          code: 'missing_property_value',
          path: `${path}[${index}].value`,
          message: 'Provide value.',
        },
      ]);
    }
    return {
      ...setting,
      property_path: getRequiredString(setting, 'property_path', `${path}[${index}].property_path`),
      value: literalValue(setting['value']),
    };
  });
}

export function requiredRecord(record: Record<string, unknown>, field: string, path: string): Record<string, unknown> {
  const value = record[field];
  if (isRecord(value)) return value;
  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be an object.`, [
    {
      code: 'missing_required_object',
      path,
      message: `${path} must be an object.`,
    },
  ]);
}

type BlueprintVariableCompiledOp = Record<string, unknown> & { op: string };

function throwMissingVariableType(path: string): never {
  throw new TaskSpecCompileError('taskspec_semantic_invalid', 'Blueprint variable type is required.', [
    {
      code: 'missing_variable_pin_type',
      path,
      message: 'Provide type or variable_type, for example {"category":"bool"}.',
    },
  ]);
}

export function omitUndefined(record: Record<string, unknown>): Record<string, unknown> {
  return Object.fromEntries(Object.entries(record).filter(([, value]) => value !== undefined));
}

// Migration guard: new GraphWrite statement node kinds must enter graphwrite-slot-source.json and StatementCompilerRegistry.
function dispatchGraphWriteStatementNode(statement: BlueprintLogicStatement, nodeId: string, path: string): AgentImportNode {
  const statementRecord = statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  const controlKind = kind === 'control' ? getControlStatementKind(statementRecord, path) : undefined;
  const delegateOperation = delegateStatementOperation(statementRecord);
  const statementCompiler = graphWriteStatementCompilerRegistry.requireForStatement({
    kind,
    path,
    controlKind,
    delegateOperation,
  });
  return statementCompiler.compile_node({
    statement,
    nodeId,
    path,
    compilerId: statementCompiler.compiler_id,
  });
}

function compileCallStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const {
    statement,
    nodeId,
    path,
  } = input;
  const statementRecord = statement as Record<string, unknown>;
  const functionName = getRequiredString(statementRecord, 'target', `${path}.target`);
  const node: Record<string, unknown> = {
    id: nodeId,
    kind: 'call',
    function: functionName,
    inputs: compileArgs(statement['args']),
  };
  copyContextEvidence(statementRecord, node, `${path}.context_evidence`);
  return node as AgentImportNode;
}

function compileCreateStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const node: Record<string, unknown> = {
    id: input.nodeId,
    kind: 'create',
    create_operation: getRequiredString(statementRecord, 'create_operation', `${input.path}.create_operation`),
    target: optionalString(statementRecord, 'target'),
    class_path: optionalString(statementRecord, 'class_path'),
    asset_path: optionalString(statementRecord, 'asset_path'),
    inputs: compileArgs(input.statement['args']),
  };
  copyStructuredPinTypeFields(statementRecord, node, input.path);
  copyCreateSemanticFields(statementRecord, node, input.path);
  copyContextEvidence(statementRecord, node, `${input.path}.context_evidence`);
  return omitUndefined(node) as AgentImportNode;
}

function compileConvertOrScheduleStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  const node: Record<string, unknown> = {
    id: input.nodeId,
    kind,
    target: optionalString(statementRecord, 'target'),
    inputs: compileArgs(input.statement['args']),
  };
  copyConvertScheduleSemanticFields(statementRecord, node);
  copyContextEvidence(statementRecord, node, `${input.path}.context_evidence`);
  return omitUndefined(node) as AgentImportNode;
}

function compileSetStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  const variableName = getRequiredString(statementRecord, 'target', `${input.path}.target`);
  const node = {
    id: input.nodeId,
    kind: 'field',
    var: variableName,
    target: variableName,
    value: valueExprToString(input.statement['value']),
  } as AgentImportNode;
  const fieldStatement = FIELD_STATEMENT_KIND_MAP.get(kind);
  if (fieldStatement) {
    applyFieldTaxonomy(node as Record<string, unknown>, fieldStatement.operation, fieldStatement.scope);
  }
  copyContextEvidence(statementRecord, node as Record<string, unknown>, `${input.path}.context_evidence`);
  return node;
}

function compileSetPropertyStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  const target = getRequiredString(statementRecord, 'target', `${input.path}.target`);
  const propertyPath = requiredGraphBodyPropertyPath(statementRecord, input.path);
  const node = {
    id: input.nodeId,
    kind: 'field',
    target,
    property_path: propertyPath,
    property: propertyPath,
    value: valueExprToString(statementRecord['value']),
  } as AgentImportNode;
  const fieldStatement = FIELD_STATEMENT_KIND_MAP.get(kind);
  if (fieldStatement) {
    applyFieldTaxonomy(node as Record<string, unknown>, fieldStatement.operation, fieldStatement.scope);
  }
  copyContextEvidence(statementRecord, node as Record<string, unknown>, `${input.path}.context_evidence`);
  return node;
}

function compileFieldStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const { operation, scope } = fieldOperationScope(statementRecord, input.path);
  if (operation !== 'set') {
    throw new TaskSpecCompileError('unsupported_field_operation', 'Field statements require field_operation=set.', [
      {
        code: 'unsupported_field_operation',
        path: `${input.path}.field_operation`,
        message: 'Field statements require field_operation=set.',
      },
    ]);
  }
  const target = getRequiredString(statementRecord, 'target', `${input.path}.target`);
  const node = {
    id: input.nodeId,
    kind: 'field',
    var: target,
    target,
    value: valueExprToString(statementRecord['value']),
  } as AgentImportNode;
  if (fieldScopeUsesPropertyPath(scope)) {
    const propertyPath = requiredGraphBodyPropertyPath(statementRecord, input.path);
    (node as Record<string, unknown>).property_path = propertyPath;
    (node as Record<string, unknown>).property = propertyPath;
  }
  applyFieldTaxonomy(node as Record<string, unknown>, operation, scope);
  copyContextEvidence(statementRecord, node as Record<string, unknown>, `${input.path}.context_evidence`);
  return node;
}

function compileContainerActionStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const { containerKind, containerOperation } = validateContainerActionShape(statementRecord, input.path, 'statement');
  const node: Record<string, unknown> = {
    id: input.nodeId,
    kind: CONTAINER_ACTION_KIND,
    inputs: {},
  };
  copyContainerActionSemanticFields(statementRecord, node);
  node.container_kind = containerKind;
  node.container_operation = containerOperation;
  copyContextEvidence(statementRecord, node, `${input.path}.context_evidence`);
  return node as AgentImportNode;
}

function compileGenericControlStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const genericControlKind = getControlStatementKind(statementRecord, input.path);
  const node: Record<string, unknown> = {
    id: input.nodeId,
    kind: 'control',
    control: genericControlKind,
    control_operation: genericControlKind,
    inputs: compileArgs(statementRecord.args),
  };
  applyGenericControlSemanticFields(statementRecord, node, genericControlKind, input.path);
  return omitUndefined(node) as AgentImportNode;
}

function compileComponentBoundEventStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const node: Record<string, unknown> = {
    id: input.nodeId,
    kind: 'component_bound_event',
    component: getRequiredString(statementRecord, 'component', `${input.path}.component`),
    delegate: getRequiredString(statementRecord, 'delegate', `${input.path}.delegate`),
    handler: getRequiredString(statementRecord, 'handler', `${input.path}.handler`),
  };
  copyContextEvidence(statementRecord, node, `${input.path}.context_evidence`);
  return omitUndefined(node) as AgentImportNode;
}

function compileDelegateStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): AgentImportNode {
  const statementRecord = input.statement as Record<string, unknown>;
  const delegateOperation = delegateStatementOperation(statementRecord);
  if (!delegateOperation) {
    return compileUnsupportedStatementNodeFromCompilerService(input);
  }
  const node: Record<string, unknown> = {
    id: input.nodeId,
    kind: 'delegate',
    target: getRequiredString(statementRecord, 'target', `${input.path}.target`),
    delegate: getRequiredString(statementRecord, 'delegate', `${input.path}.delegate`),
    handler: typeof statementRecord.handler === 'string' ? statementRecord.handler : undefined,
    delegate_operation: delegateOperation,
    unbind_mode: delegateOperation === 'unbind' ? 'single' : (delegateOperation === 'clear' ? 'all' : undefined),
    inputs: delegateOperation === 'call' ? compileArgs(statementRecord.args) : undefined,
  };
  copyContextEvidence(statementRecord, node, `${input.path}.context_evidence`);
  return omitUndefined(node) as AgentImportNode;
}

function compileUnsupportedStatementNodeFromCompilerService(input: GraphWriteStatementNodeCompileInput): never {
  const statementRecord = input.statement as Record<string, unknown>;
  const kind = typeof statementRecord.kind === 'string' ? statementRecord.kind : '';
  throw new TaskSpecCompileError('unsupported_statement_kind', `Unsupported statement kind: ${kind}`, [
    {
      code: 'unsupported_statement_kind',
      path: `${input.path}.kind`,
      message: `Unsupported statement kind: ${kind}`,
    },
  ]);
}

function compileEnsureEntryOpIntoAppendPayload(
  nodes: AgentImportNode[],
  links: AgentImportLink[],
  op: Record<string, unknown>,
  path: string,
  options: LogicCloneOptions = {},
): BlueprintLogicStatement[] {
  const entryType = getRequiredString(op, 'entry_type', `${path}.entry_type`);
  if (entryType !== 'custom_event') {
    throw new TaskSpecCompileError('unsupported_entry_type', 'Only custom_event entries are supported in the first MCP slice.', [
      {
        code: 'unsupported_entry_type',
        path: `${path}.entry_type`,
        message: 'Use entry_type="custom_event". Function/Event signature management is a later capability cluster.',
      },
    ]);
  }

  const entryName = getRequiredString(op, 'name', `${path}.name`);
  const body = getRequiredLogicBody(op, 'body', `${path}.body`);
  const entryId = `${toIdSegment(entryName)}_entry`;
  nodes.push({ id: entryId, kind: 'custom_event', name: entryName });

  const flow = compileStatementSequence(body.statements, `${toIdSegment(entryName)}_stmt`, `${path}.body.statements`, makeCompileFlowContext());
  nodes.push(...flow.nodes);
  links.push(...flow.links);
  if (flow.entry) {
    links.push({ kind: 'exec', from: `${entryId}.then`, to: flow.entry });
  }
  return cloneLogicStatementSequenceWithCompiledIds(body.statements, `${toIdSegment(entryName)}_stmt`, options);
}

function compileArgs(args: unknown): Record<string, unknown> {
  if (!isRecord(args)) return {};
  const out: Record<string, unknown> = {};
  for (const [key, value] of Object.entries(args)) {
    out[key] = literalValue(value);
  }
  return out;
}

export function literalValue(value: unknown): unknown {
  if (isRecord(value) && value['kind'] === 'literal') {
    return value['value'];
  }
  return value;
}

function valueExprToString(value: unknown): string {
  const literal = literalValue(value);
  if (typeof literal === 'string') return literal;
  if (typeof literal === 'number' || typeof literal === 'boolean') return String(literal);
  if (literal === null || literal === undefined) return '';
  return JSON.stringify(literal);
}

export function getRequiredString(record: Record<string, unknown>, field: string, path: string): string {
  const value = record[field];
  if (typeof value === 'string' && value.trim().length > 0) {
    return value;
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a non-empty string.`, [
    {
      code: 'missing_required_string',
      path,
      message: `${path} must be a non-empty string.`,
    },
  ]);
}

export function optionalString(record: Record<string, unknown>, field: string): string | undefined {
  const value = record[field];
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

export function graphWriteAppendEventKind(record: Record<string, unknown>): GraphWriteAppendEventKind {
  const eventKind = optionalString(record, 'event_kind');
  if (
    eventKind === 'custom_event'
    || eventKind === 'override_event'
    || eventKind === 'component_bound_event'
    || eventKind === 'input_action_event'
    || eventKind === 'dispatcher_event'
  ) {
    return eventKind;
  }
  return 'custom_event';
}

function graphWriteEnsureEntryEventKind(record: Record<string, unknown>): GraphWriteAppendEventKind {
  return graphWriteAppendEventKind(record);
}

export function graphWriteCatalogEvidence(value: unknown): GraphWriteCatalogEvidence | undefined {
  if (!isRecord(value)) {
    return undefined;
  }

  const source = optionalString(value, 'source');
  if (source !== 'signature' && source !== 'graph_action_catalog') {
    return undefined;
  }

  return omitUndefined({
    source,
    signature_evidence_id: optionalString(value, 'signature_evidence_id'),
    action_stable_id: optionalString(value, 'action_stable_id'),
    context_fingerprint: optionalString(value, 'context_fingerprint'),
  }) as GraphWriteCatalogEvidence;
}

export function getRequiredLogicBody(record: Record<string, unknown>, field: string, path: string): { statements: BlueprintLogicStatement[] } {
  const value = record[field];
  const logicBodySchema = isRecord(value) ? value['schema'] : undefined;
  if (
    isRecord(value)
    && (logicBodySchema === 'BlueprintLogicSpec.v1' || logicBodySchema === 'BlueprintLogicSpec.v2')
    && Array.isArray(value['statements'])
  ) {
    return {
      statements: value['statements'] as BlueprintLogicStatement[],
    };
  }

  throw new TaskSpecCompileError('taskspec_semantic_invalid', `${path} must be a BlueprintLogicSpec body.`, [
    {
      code: 'missing_required_logic_body',
      path,
      message: `${path} must be a BlueprintLogicSpec body.`,
    },
  ]);
}

function toIdSegment(value: string): string {
  const normalized = value.replace(/[^A-Za-z0-9_]/g, '_');
  return normalized.length > 0 ? normalized : 'entry';
}
