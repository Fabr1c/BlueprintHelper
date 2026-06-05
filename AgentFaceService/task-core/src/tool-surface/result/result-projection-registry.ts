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
  | 'tool.generic.default';

const POLICY_BY_MANIFEST_ID = new Map<ToolResultProjectionPolicyId, BuiltinResultProjectionPolicyId>([
  ['task_preview_default', 'task.preview.default'],
  ['task_execute_default', 'task.execute.default'],
  ['task_result_default', 'task.result.default'],
  ['read_context_default', 'context.read.default'],
  ['diagnostics_default', 'diagnostics.default'],
  ['review_expert_default', 'tool.generic.default'],
  ['bridge_default', 'tool.generic.default'],
  ['local_default', 'tool.generic.default'],
]);

const BUILTIN_POLICIES = new Map<BuiltinResultProjectionPolicyId, ResultProjectionPolicy>([
  ['task.preview.default', { ...GENERIC_RESULT_PROJECTION_POLICY, policy_id: 'task.preview.default' }],
  ['task.execute.default', { ...GENERIC_RESULT_PROJECTION_POLICY, policy_id: 'task.execute.default' }],
  ['task.result.default', { ...GENERIC_RESULT_PROJECTION_POLICY, policy_id: 'task.result.default' }],
  ['context.read.default', { ...GENERIC_RESULT_PROJECTION_POLICY, policy_id: 'context.read.default' }],
  ['diagnostics.default', { ...GENERIC_RESULT_PROJECTION_POLICY, policy_id: 'diagnostics.default' }],
  ['metrics.report.default', { ...GENERIC_RESULT_PROJECTION_POLICY, policy_id: 'metrics.report.default' }],
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
