import type {
  TaskPlan,
  TaskSpec,
} from '../../schema/task-schemas.js';
import { isRecord } from '../compiler-helpers.js';
import {
  getGraphWriteRouteByScope,
} from './graphwrite-route-registry.js';
import {
  graphWriteAppendEventKind,
  type GraphWriteCompiledOp,
  type GraphWriteSignatureSplit,
  makeCustomEventSignatureEvidenceId,
  omitUndefined,
} from './graphwrite-logic-body-compiler.js';

type TaskPlanStep = TaskPlan['steps'][number];

export function makeGraphWriteTaskPlanSteps(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>,
  graphWriteOps: GraphWriteCompiledOp[],
): TaskPlanStep[] {
  const behavior = taskSpec.behavior as Record<string, unknown>;
  const strategy = String(behavior['graph_strategy']);
  const autoSearchPolicy = graphWriteAutoSearchPolicyForTaskSpec(taskSpec);
  if (strategy === 'append_new_owned_graph') {
    const signatureOps = graphWriteOps.filter((op) => graphWriteAppendEventKind(op) === 'custom_event');
    const signatureSteps = signatureOps.map((op, index) => ({
      step_id: `step_${String(index + 1).padStart(3, '0')}`,
      capability: 'blueprint_signature',
      target: {
        asset_path: taskSpec.target.asset_path,
      },
      write: {
        strategy: 'custom_event_signature',
        ops: [
          omitUndefined({
            op: 'ensure_custom_event',
            event_name: String(op['name']),
            graph_name: taskSpec.scope_policy.graph_name,
            inputs: (op as GraphWriteCompiledOp & { __signature_split?: GraphWriteSignatureSplit }).__signature_split?.inputs,
            name_collision_policy: 'reuse_if_exists',
          }),
        ],
      },
    } as TaskPlanStep));
    const signatureStepIds = signatureSteps.map((step) => step.step_id);
    const graphWriteSteps = graphWriteOps.map((op, index) => {
      const stepId = `step_${String(signatureSteps.length + index + 1).padStart(3, '0')}`;
      const previousGraphWriteStepId = index > 0
        ? `step_${String(signatureSteps.length + index).padStart(3, '0')}`
        : undefined;
      return {
        step_id: stepId,
        capability: 'graph_write',
        target: {
          asset_path: taskSpec.target.asset_path,
          graph: targetGraphForGraphWriteOp(taskSpec, op),
        },
        write: {
          strategy: 'owned_graph_edit',
          ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
          ops: [withSignatureEvidence(stripGraphWriteCompilerMetadata(op))],
        },
        constraints: {
          allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
          ownership_scope: 'blueprinthelper_owned',
        },
        depends_on: previousGraphWriteStepId
          ? [...signatureStepIds, previousGraphWriteStepId]
          : signatureStepIds,
      } as TaskPlanStep;
    });
    return [...signatureSteps, ...graphWriteSteps];
  }

  if (strategy === 'replace_owned_graph' && graphWriteOps.length === 1) {
    const op = graphWriteOps[0] as GraphWriteCompiledOp & { __signature_split?: GraphWriteSignatureSplit };
    if (op.__signature_split) {
      const signatureOp = op.__signature_split;
      return [
        {
          step_id: 'step_001',
          capability: 'blueprint_signature',
          target: {
            asset_path: taskSpec.target.asset_path,
          },
          write: {
            strategy: 'custom_event_signature',
            ops: [
              omitUndefined({
                op: signatureOp.op,
                event_name: signatureOp.event_name,
                graph_name: taskSpec.scope_policy.graph_name,
                inputs: signatureOp.inputs,
                name_collision_policy: signatureOp.name_collision_policy,
              }),
            ],
          },
        } as TaskPlanStep,
        {
          step_id: 'step_002',
          capability: 'graph_write',
          target: {
            asset_path: taskSpec.target.asset_path,
            graph: targetGraphForGraphWriteOp(taskSpec, op),
          },
          write: {
            strategy: 'owned_graph_edit',
            ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
            ops: [withSignatureEvidence(stripGraphWriteCompilerMetadata(op))],
          },
          constraints: {
            allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
            ownership_scope: 'blueprinthelper_owned',
          },
          depends_on: ['step_001'],
        } as TaskPlanStep,
      ];
    }
  }

  if (
    strategy === 'merge_external_flow'
    || strategy === 'patch_external_graph'
    || strategy === 'patch_external_links'
    || strategy === 'replace_external_body'
  ) {
    const mutationPolicyByStrategy: Record<string, string[]> = {
      merge_external_flow: ['exec_boundary_link'],
      patch_external_graph: ['pin_default', 'node_comment', 'node_property'],
      patch_external_links: ['link_connect', 'link_disconnect', 'link_replace'],
      replace_external_body: ['body_replace'],
    };
    return graphWriteOps.map((op, index) => ({
      step_id: `step_${String(index + 1).padStart(3, '0')}`,
      ...graphWriteRouteIdFieldForOp(strategy, op),
      capability: 'graph_write',
      target: {
        asset_path: taskSpec.target.asset_path,
        graph: targetGraphForGraphWriteOp(taskSpec, op),
      },
      write: {
        strategy: 'external_graph_edit',
        ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
        ops: [stripGraphWriteCompilerMetadata(op)],
      },
      constraints: {
        allow_modify_user_nodes: false,
        ownership_scope: 'external_user_authored',
        external_mutation_policy: {
          strategy,
          allowed_mutations: mutationPolicyByStrategy[strategy],
        },
      },
    } as TaskPlanStep));
  }

  const opBatches = strategy === 'append_new_owned_graph'
    ? [graphWriteOps]
    : graphWriteOps.map((op) => [stripGraphWriteCompilerMetadata(op)]);

  return opBatches.map((ops, index) => ({
    step_id: `step_${String(index + 1).padStart(3, '0')}`,
    capability: 'graph_write',
    target: {
      asset_path: taskSpec.target.asset_path,
      graph: ops.length === 1 ? targetGraphForGraphWriteOp(taskSpec, ops[0]) : taskSpec.scope_policy.graph_name,
    },
    write: {
      strategy: 'owned_graph_edit',
      ...graphWriteAutoSearchPolicyWriteField(autoSearchPolicy),
      ops,
    },
    constraints: {
      allow_modify_user_nodes: taskSpec.scope_policy.allow_modify_user_nodes,
      ownership_scope: 'blueprinthelper_owned',
    },
  }));
}

function graphWriteAutoSearchPolicyForTaskSpec(
  taskSpec: Extract<TaskSpec, { task_type: 'edit_blueprint_graph' }>,
): Record<string, unknown> | undefined {
  const policy = (taskSpec as Record<string, unknown>)['graph_write_policy'];
  if (!isRecord(policy) || !isRecord(policy['auto_search'])) {
    return undefined;
  }
  return policy['auto_search'];
}

function graphWriteAutoSearchPolicyWriteField(
  autoSearchPolicy: Record<string, unknown> | undefined,
): Record<string, unknown> {
  if (!autoSearchPolicy) {
    return {};
  }
  const sanitized = omitUndefined({
    mode: typeof autoSearchPolicy['mode'] === 'string' ? autoSearchPolicy['mode'] : undefined,
    max_candidates_per_statement: typeof autoSearchPolicy['max_candidates_per_statement'] === 'number'
      ? autoSearchPolicy['max_candidates_per_statement']
      : undefined,
    max_auto_search_statements: typeof autoSearchPolicy['max_auto_search_statements'] === 'number'
      ? autoSearchPolicy['max_auto_search_statements']
      : undefined,
    max_total_auto_search_ms: typeof autoSearchPolicy['max_total_auto_search_ms'] === 'number'
      ? autoSearchPolicy['max_total_auto_search_ms']
      : undefined,
    detail_level: typeof autoSearchPolicy['detail_level'] === 'string' ? autoSearchPolicy['detail_level'] : undefined,
  });
  return Object.keys(sanitized).length > 0 ? { auto_search_policy: sanitized } : {};
}

function stripGraphWriteCompilerMetadata(op: GraphWriteCompiledOp): GraphWriteCompiledOp {
  const { __signature_split: _signatureSplit, ...cleanOp } = op as GraphWriteCompiledOp & { __signature_split?: unknown };
  return cleanOp as GraphWriteCompiledOp;
}

function targetGraphForGraphWriteOp(taskSpec: TaskSpec, op: GraphWriteCompiledOp): string {
  if (op.op !== 'replace_body' || !isRecord(op.selector)) {
    return taskSpec.scope_policy.graph_name;
  }

  const route = typeof op.replace_scope === 'string'
    ? getGraphWriteRouteByScope('replace_owned_graph', op.replace_scope)
    : undefined;
  const graphNameOutputField = route?.selector?.graph_name_output_field;
  if (
    graphNameOutputField
    && typeof op.selector[graphNameOutputField] === 'string'
    && op.selector[graphNameOutputField].trim().length > 0
  ) {
    return op.selector[graphNameOutputField].trim();
  }

  if (typeof op.selector.graph_id === 'string' && op.selector.graph_id.trim().length > 0) {
    return op.selector.graph_id.trim();
  }

  return taskSpec.scope_policy.graph_name;
}

function graphWriteRouteIdFieldForOp(strategy: string, op: GraphWriteCompiledOp): Record<string, string> {
  const publicScope = graphWritePublicScopeForOp(strategy, op);
  if (!publicScope) {
    return {};
  }

  const route = getGraphWriteRouteByScope(strategy, publicScope);
  return route ? { route_id: route.route_id } : {};
}

function graphWritePublicScopeForOp(strategy: string, op: GraphWriteCompiledOp): string | undefined {
  if (strategy === 'merge_external_flow') {
    return typeof op.insert_strategy === 'string' ? op.insert_strategy : undefined;
  }
  if (strategy === 'patch_external_graph') {
    return typeof op.patch_scope === 'string' ? op.patch_scope : undefined;
  }
  if (strategy === 'patch_external_links') {
    return externalLinkPatchPublicScopeForOp(op);
  }
  if (strategy === 'replace_external_body') {
    return 'body';
  }

  return undefined;
}

function externalLinkPatchPublicScopeForOp(op: GraphWriteCompiledOp): string | undefined {
  if (typeof op.patch_scope === 'string') {
    return op.patch_scope;
  }

  switch (op.op) {
    case 'connect_external_pins':
      return 'connect_pins';
    case 'disconnect_external_link':
      return 'disconnect_link';
    case 'replace_external_link':
      return 'replace_link';
    default:
      return undefined;
  }
}

function withSignatureEvidence(op: GraphWriteCompiledOp): GraphWriteCompiledOp {
  if (op.op !== 'ensure_entry' || typeof op.name !== 'string' || op.name.trim().length === 0) {
    return op;
  }
  if (typeof op.signature_evidence_id === 'string' && op.signature_evidence_id.trim().length > 0) {
    return {
      ...op,
      signature_evidence_id: op.signature_evidence_id.trim(),
    };
  }
  if (graphWriteAppendEventKind(op) !== 'custom_event') {
    return op;
  }
  return {
    ...op,
    signature_evidence_id: makeCustomEventSignatureEvidenceId(op.name.trim()),
  };
}
