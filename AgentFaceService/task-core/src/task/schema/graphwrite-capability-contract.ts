export type GraphWriteSupportStatus = 'supported' | 'discussion-gated' | 'retired' | 'rejected';

export type GraphWriteEvidenceProjectionSource = 'SemanticStatement' | 'ActionContext' | 'UE ActionDatabase';

export type GraphWriteReviewEvidencePolicy = 'graph_surface_atomic_target';

export type GraphWriteLogicalRuntimeCluster = 'FunctionAction';

export interface GraphWriteOperationGroupOperation {
  readonly id: string;
  readonly supportStatus: GraphWriteSupportStatus;
  readonly runtimeCluster?: GraphWriteLogicalRuntimeCluster;
  readonly semanticKind?: 'op';
  readonly semanticFamily?: 'operator';
  readonly secondStageOperation?: `op.${string}`;
  readonly priority?: 'P0' | 'P1' | 'P2';
  readonly requiredEvidenceKeys?: readonly string[];
  readonly rejectionReason?: string;
}

export interface GraphWriteOperationGroupContract {
  readonly id: 'op_coverage';
  readonly responsibility: string;
  readonly operations: readonly GraphWriteOperationGroupOperation[];
}

export interface GraphWriteOperationContract {
  readonly id: string;
  readonly kind: string;
  readonly supportStatus: GraphWriteSupportStatus;
  readonly reviewEvidence: GraphWriteReviewEvidencePolicy;
  readonly requiredEvidenceKeys?: readonly string[];
}

export interface GraphWriteClusterContract {
  readonly id: 'function_action' | 'field' | 'event' | 'asset_action' | 'container_action' | 'generic_schedule';
  readonly responsibility: string;
  readonly operations: readonly GraphWriteOperationContract[];
  readonly evidence: {
    readonly projectionSource: GraphWriteEvidenceProjectionSource;
    readonly requiredKeys: readonly string[];
  };
  readonly executeRevalidation: 'required' | 'not-required';
}

export interface GraphWriteCapabilityContract {
  readonly version: 1;
  readonly status: 'stable-candidate';
  readonly clusters: readonly GraphWriteClusterContract[];
  readonly operationGroups: readonly GraphWriteOperationGroupContract[];
  readonly finalAcceptance: {
    readonly generalityPreflightAfterStable: true;
    readonly smokePlan: 'BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md';
  };
}

const OP_COVERAGE_P0_OPERATIONS = [
  'bitwise_and',
  'bitwise_or',
  'boolean_and',
  'boolean_or',
  'boolean_nand',
  'max',
  'min',
  'string_append',
] as const;

const OP_COVERAGE_P1_OPERATIONS = [
  'boolean_not',
  'boolean_xor',
  'boolean_nor',
  'bitwise_not',
  'bitwise_xor',
  'abs',
  'modulo',
  'negate',
  'dot',
  'dot3',
  'cross',
  'cross3',
  'near_equal',
  'intpoint_equal',
  'transform_compose',
  'equal_exact',
  'not_equal_exact',
  'equal_ignore_case',
  'not_equal_ignore_case',
  'datetime_add_datetime',
  'datetime_add_timespan',
  'datetime_subtract_datetime',
  'datetime_subtract_timespan',
  'datetime_equal',
  'datetime_not_equal',
  'datetime_greater',
  'datetime_greater_equal',
  'datetime_less',
  'datetime_less_equal',
] as const;

const OP_COVERAGE_P2_OPERATIONS = ['array_identical'] as const;

export const OP_COVERAGE_SUPPORTED_OPERATION_IDS = [
  ...OP_COVERAGE_P0_OPERATIONS,
  ...OP_COVERAGE_P1_OPERATIONS,
  ...OP_COVERAGE_P2_OPERATIONS,
] as const;

export const OP_COVERAGE_EXCLUDED_OPERATION_IDS = [
  'enum_equal',
  'enum_not_equal',
  'slate_brush_equal',
  'slate_brush_not_equal',
  'convert_numeric',
  'convert_string_text_name',
  'array_map_set_mutation',
  'validity_predicate',
] as const;

export const OP_COVERAGE_EVIDENCE_KEYS = [
  'op.operation_id',
  'op.spawn_family',
  'op.stable_callable_id',
  'op.required_node_class_path',
  'op.argument_pin_type.0',
  'op.argument_pin_type.1',
  'op.expected_return_pin_type',
  'op.array_lhs_pin_type',
  'op.array_rhs_pin_type',
] as const;

function makeSupportedOp(
  id: string,
  priority: 'P0' | 'P1' | 'P2',
  requiredEvidenceKeys: readonly string[] = [],
): GraphWriteOperationGroupOperation {
  return {
    id,
    supportStatus: 'supported',
    runtimeCluster: 'FunctionAction',
    semanticKind: 'op',
    semanticFamily: 'operator',
    secondStageOperation: `op.${id}`,
    priority,
    requiredEvidenceKeys,
  };
}

function makeRejectedOp(id: string): GraphWriteOperationGroupOperation {
  return {
    id,
    supportStatus: 'rejected',
    rejectionReason: 'excluded_op_operation',
  };
}

const ASSET_ACTION_REQUIRED_EVIDENCE_KEYS = [
  'asset_action_stable_id',
  'asset_action_node_class',
  'asset_action_spawner_signature',
  'asset_action_owner_path',
] as const;

const GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS = [
  'schedule_action_stable_id',
  'schedule_node_class',
  'schedule_spawner_signature',
  'schedule_owner_path',
] as const;

const TIMER_DELEGATE_REQUIRED_EVIDENCE_KEYS = [
  ...GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS,
  'handler_name',
  'handler_function_path',
  'handler_source_cluster',
  'signature_evidence_id',
] as const;

const LATENT_OR_ASYNC_REQUIRED_EVIDENCE_KEYS = [
  ...GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS,
  'graph_latent_allowed',
] as const;

export const GRAPHWRITE_CAPABILITY_CONTRACT: GraphWriteCapabilityContract = {
  version: 1,
  status: 'stable-candidate',
  clusters: [
    {
      id: 'function_action',
      responsibility: 'GraphWrite owns function-like statements that resolve through ActionContext and shared action adapters.',
      operations: [
        {
          id: 'call_function',
          kind: 'function',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
        },
        {
          id: 'macro_like',
          kind: 'function',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
        },
      ],
      evidence: {
        projectionSource: 'ActionContext',
        requiredKeys: [],
      },
      executeRevalidation: 'not-required',
    },
    {
      id: 'field',
      responsibility: 'GraphWrite owns property path, linked typed pin, component_ref and field_access statements.',
      operations: [
        {
          id: 'field_access',
          kind: 'field',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
        },
        {
          id: 'component_ref',
          kind: 'field',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
        },
      ],
      evidence: {
        projectionSource: 'SemanticStatement',
        requiredKeys: [],
      },
      executeRevalidation: 'not-required',
    },
    {
      id: 'event',
      responsibility: 'GraphWrite owns custom_event statement creation/reference; override/native and delegate-bound entries stay discussion-gated to their owning tools.',
      operations: [
        {
          id: 'custom_event',
          kind: 'event',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
        },
        {
          id: 'override_native_event',
          kind: 'event',
          supportStatus: 'discussion-gated',
          reviewEvidence: 'graph_surface_atomic_target',
        },
        {
          id: 'delegate_component_bound_event',
          kind: 'event',
          supportStatus: 'discussion-gated',
          reviewEvidence: 'graph_surface_atomic_target',
        },
      ],
      evidence: {
        projectionSource: 'SemanticStatement',
        requiredKeys: [],
      },
      executeRevalidation: 'not-required',
    },
    {
      id: 'asset_action',
      responsibility: 'GraphWrite may execute ActionDatabase-backed asset action spawners only when projected evidence selects exactly one current spawner.',
      operations: [
        {
          id: 'create.asset_action',
          kind: 'create',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
          requiredEvidenceKeys: ASSET_ACTION_REQUIRED_EVIDENCE_KEYS,
        },
      ],
      evidence: {
        projectionSource: 'UE ActionDatabase',
        requiredKeys: ASSET_ACTION_REQUIRED_EVIDENCE_KEYS,
      },
      executeRevalidation: 'required',
    },
    {
      id: 'container_action',
      responsibility:
        'GraphWrite owns first-class ordinary Blueprint array/map/set container semantics; execution resolves through ActionContext and FunctionAction-backed callable evidence.',
      operations: [
        'container.array.get',
        'container.array.set',
        'container.array.add',
        'container.array.add_unique',
        'container.array.append',
        'container.array.insert',
        'container.array.remove_item',
        'container.array.remove_index',
        'container.array.clear',
        'container.array.contains',
        'container.array.find',
        'container.array.length',
        'container.map.add',
        'container.map.remove',
        'container.map.find',
        'container.map.contains',
        'container.map.keys',
        'container.map.values',
        'container.map.clear',
        'container.map.length',
        'container.set.add',
        'container.set.remove',
        'container.set.contains',
        'container.set.clear',
        'container.set.length',
        'container.set.to_array',
      ].map((id) => ({
        id,
        kind: 'container_action',
        supportStatus: 'supported' as const,
        reviewEvidence: 'graph_surface_atomic_target' as const,
      })),
      evidence: {
        projectionSource: 'ActionContext',
        requiredKeys: [],
      },
      executeRevalidation: 'not-required',
    },
    {
      id: 'generic_schedule',
      responsibility:
        'GraphWrite owns Generic schedule use-site nodes only when selected ActionDatabase spawner evidence and external handler/signature evidence are projected.',
      operations: [
        {
          id: 'schedule.timer_delegate_node',
          kind: 'schedule',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
          requiredEvidenceKeys: TIMER_DELEGATE_REQUIRED_EVIDENCE_KEYS,
        },
        {
          id: 'schedule.latent_or_async_node',
          kind: 'schedule',
          supportStatus: 'supported',
          reviewEvidence: 'graph_surface_atomic_target',
          requiredEvidenceKeys: LATENT_OR_ASYNC_REQUIRED_EVIDENCE_KEYS,
        },
      ],
      evidence: {
        projectionSource: 'UE ActionDatabase',
        requiredKeys: GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS,
      },
      executeRevalidation: 'required',
    },
  ],
  operationGroups: [
    {
      id: 'op_coverage',
      responsibility:
        'GraphWrite treats op coverage as FunctionAction-owned operator semantics; it does not publish a graphwrite_op runtime cluster.',
      operations: [
        ...OP_COVERAGE_P0_OPERATIONS.map((id) => makeSupportedOp(id, 'P0')),
        ...OP_COVERAGE_P1_OPERATIONS.map((id) => makeSupportedOp(id, 'P1')),
        makeSupportedOp('array_identical', 'P2', ['op.array_lhs_pin_type', 'op.array_rhs_pin_type']),
        ...OP_COVERAGE_EXCLUDED_OPERATION_IDS.map((id) => makeRejectedOp(id)),
      ],
    },
  ],
  finalAcceptance: {
    generalityPreflightAfterStable: true,
    smokePlan: 'BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md',
  },
};
