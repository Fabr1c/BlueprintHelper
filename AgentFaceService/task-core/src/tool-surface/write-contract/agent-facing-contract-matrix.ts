import { buildWriteCapabilityContractInventory } from './write-capability-contract-inventory.js';
import type { WriteCapabilityContract, WriteCapabilityVisibility } from './write-capability-contract-types.js';

export interface AgentFacingContractUnit {
  readonly capability_id: string;
  readonly tool_name: string;
  readonly family: string;
  readonly visibility: WriteCapabilityVisibility;
  readonly template_ids: readonly string[];
  readonly compiler_id?: string;
  readonly runtime_adapter_id?: string;
  readonly result_policy_id?: string;
}

export function buildAgentFacingContractMatrix(): readonly AgentFacingContractUnit[] {
  return buildWriteCapabilityContractInventory()
    .filter(shouldIncludeContract)
    .map(contractToUnit)
    .sort((left, right) => left.capability_id.localeCompare(right.capability_id));
}

function shouldIncludeContract(contract: WriteCapabilityContract): boolean {
  return contract.visibility === 'active' || contract.visibility === 'developer_only';
}

function contractToUnit(contract: WriteCapabilityContract): AgentFacingContractUnit {
  const compilerId = findEvidenceValue(contract.preview_execute.evidence, 'compiler:');
  return {
    capability_id: contract.capability_id,
    tool_name: contract.tool_name,
    family: contract.write_family,
    visibility: contract.visibility,
    template_ids: [...contract.input_evidence.template_refs],
    compiler_id: compilerId,
    runtime_adapter_id: contract.runtime_adapter_id,
    result_policy_id: resultPolicyIdForContract(contract),
  };
}

function findEvidenceValue(evidence: readonly string[], prefix: string): string | undefined {
  const entry = evidence.find((value) => value.startsWith(prefix));
  return entry ? entry.slice(prefix.length) : undefined;
}

function resultPolicyIdForContract(contract: WriteCapabilityContract): string | undefined {
  if (contract.capability_id === 'review.write.apply_action') {
    return 'review_action_result_projection';
  }
  if (contract.tool_name === 'blueprinthelper_execute_task') {
    return 'task_execute_result_projection';
  }
  return undefined;
}
