import { BRIDGE_TOOL_DESCRIPTORS } from './bridge-tool-descriptor.js';

export type CliBridgeCallPolicy = 'forbidden' | 'expert_only' | 'agent_visible';
export type CliBridgeCallRisk = 'low' | 'medium' | 'high';
export type CliBridgeCallSource =
  | 'descriptor'
  | 'read_context_route'
  | 'debug_case'
  | 'lifecycle_compat';

export interface CliBridgeCallPolicyDescriptor {
  readonly bridge_command: string;
  readonly policy: CliBridgeCallPolicy;
  readonly risk: CliBridgeCallRisk;
  readonly source: CliBridgeCallSource;
}

const EXTRA_BRIDGE_CALL_POLICIES: readonly CliBridgeCallPolicyDescriptor[] = [
  { bridge_command: 'read_reference_context', policy: 'expert_only', risk: 'low', source: 'read_context_route' },
  { bridge_command: 'get_task_run_journal', policy: 'expert_only', risk: 'low', source: 'debug_case' },
];

export function listCliBridgeCallPolicies(): readonly CliBridgeCallPolicyDescriptor[] {
  const descriptorPolicies = BRIDGE_TOOL_DESCRIPTORS
    .filter((descriptor) => typeof descriptor.bridge_command === 'string')
    .map((descriptor): CliBridgeCallPolicyDescriptor => ({
      bridge_command: descriptor.bridge_command as string,
      policy: descriptor.cli_bridge_call_policy ?? 'forbidden',
      risk: descriptor.risk ?? 'medium',
      source: descriptor.source ?? 'descriptor',
    }));
  return [...descriptorPolicies, ...EXTRA_BRIDGE_CALL_POLICIES];
}

export function getCliBridgeCallPolicy(bridgeCommand: string): CliBridgeCallPolicyDescriptor {
  return listCliBridgeCallPolicies().find((descriptor) => descriptor.bridge_command === bridgeCommand) ?? {
    bridge_command: bridgeCommand,
    policy: 'forbidden',
    risk: 'medium',
    source: 'descriptor',
  };
}

export function isCliBridgeCallAllowed(bridgeCommand: string): boolean {
  const policy = getCliBridgeCallPolicy(bridgeCommand).policy;
  return policy === 'expert_only' || policy === 'agent_visible';
}
