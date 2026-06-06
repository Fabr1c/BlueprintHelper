import type { ToolResultProjectionPolicyId } from '../manifest/tool-command-manifest.js';
import type { ToolCommandManifestRegistry } from '../manifest/tool-command-manifest-registry.js';
import {
  GENERIC_RESULT_PROJECTION_POLICY,
  type ResultProjectionPolicy,
} from './result-projection-policy.js';

export type BuiltinResultProjectionPolicyId =
  | 'task.preview.default'
  | 'task.execute.default'
  | 'task.result.default'
  | 'context.read.default'
  | 'diagnostics.default'
  | 'metrics.report.default'
  | 'bridge.default'
  | 'local.default'
  | 'review.expert.default'
  | 'tool.generic.default';

const POLICY_BY_MANIFEST_ID = new Map<ToolResultProjectionPolicyId, BuiltinResultProjectionPolicyId>([
  ['task_preview_default', 'task.preview.default'],
  ['task_execute_default', 'task.execute.default'],
  ['task_result_default', 'task.result.default'],
  ['read_context_default', 'context.read.default'],
  ['diagnostics_default', 'diagnostics.default'],
  ['review_expert_default', 'review.expert.default'],
  ['bridge_default', 'bridge.default'],
  ['local_default', 'local.default'],
]);

const BUILTIN_POLICIES = new Map<BuiltinResultProjectionPolicyId, ResultProjectionPolicy>([
  ['task.preview.default', definePolicy('task.preview.default', ['ok', 'operation', 'status', 'modified', 'target', 'data.preview_id', 'data.preview_token', 'data.passed', 'data.blocked', 'data.issues', 'error'])],
  ['task.execute.default', definePolicy('task.execute.default', ['ok', 'operation', 'status', 'modified', 'target', 'validation', 'data.task_run_id', 'data.task', 'data.issues', 'error'])],
  ['task.result.default', definePolicy('task.result.default', ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'])],
  ['context.read.default', definePolicy('context.read.default', ['ok', 'operation', 'status', 'modified', 'target', 'data.payload', 'data.issues', 'error'])],
  ['diagnostics.default', definePolicy('diagnostics.default', ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'])],
  ['metrics.report.default', definePolicy('metrics.report.default', ['ok', 'operation', 'status', 'modified', 'data', 'error'])],
  ['bridge.default', definePolicy('bridge.default', ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'])],
  ['local.default', definePolicy('local.default', ['ok', 'operation', 'status', 'modified', 'data', 'error'])],
  ['review.expert.default', definePolicy('review.expert.default', ['ok', 'operation', 'status', 'modified', 'target', 'data', 'error'], ['debug', 'trace_id'])],
  ['tool.generic.default', GENERIC_RESULT_PROJECTION_POLICY],
]);

export function resolveResultProjectionPolicy(input: {
  manifestRegistry?: ToolCommandManifestRegistry;
  toolIdOrAlias?: string;
  commandKind?: string;
  resultPolicyId?: ToolResultProjectionPolicyId;
}): ResultProjectionPolicy {
  const manifestPolicyId = input.resultPolicyId
    ?? (input.toolIdOrAlias ? input.manifestRegistry?.get(input.toolIdOrAlias)?.result_policy_id : undefined);
  const policyId = manifestPolicyId
    ? POLICY_BY_MANIFEST_ID.get(manifestPolicyId)
    : policyIdForCommandKind(input.commandKind);
  if (!policyId) {
    return GENERIC_RESULT_PROJECTION_POLICY;
  }
  const policy = BUILTIN_POLICIES.get(policyId);
  if (!policy) {
    const error = new Error(`Unknown BlueprintHelper result projection policy: ${policyId}`);
    (error as Error & { code?: string; field?: string }).code = 'result_projection_policy_not_found';
    (error as Error & { code?: string; field?: string }).field = 'result_policy_id';
    throw error;
  }
  return policy;
}

export function getBuiltinResultProjectionPolicy(policyId: BuiltinResultProjectionPolicyId): ResultProjectionPolicy {
  const policy = BUILTIN_POLICIES.get(policyId);
  if (!policy) {
    throw new Error(`Unknown BlueprintHelper built-in result projection policy: ${policyId}`);
  }
  return policy;
}

function policyIdForCommandKind(commandKind: string | undefined): BuiltinResultProjectionPolicyId | undefined {
  if (commandKind === 'task.preview') return 'task.preview.default';
  if (commandKind === 'task.execute') return 'task.execute.default';
  if (commandKind === 'task.result') return 'task.result.default';
  if (commandKind === 'context.read') return 'context.read.default';
  if (commandKind === 'metrics.report') return 'metrics.report.default';
  return undefined;
}

function definePolicy(
  policyId: BuiltinResultProjectionPolicyId,
  baseFields: readonly string[],
  expertFields: readonly string[] = ['debug', 'trace_id'],
): ResultProjectionPolicy {
  return {
    ...GENERIC_RESULT_PROJECTION_POLICY,
    policy_id: policyId,
    default_fields: [...baseFields],
    json_fields: [...baseFields],
    full_fields: [...baseFields],
    expert_fields: [...expertFields],
    debug_artifact_fields: ['tool_result', 'extra', 'bridge_result', 'debug'],
  };
}
