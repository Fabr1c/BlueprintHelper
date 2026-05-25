export type GraphWriteSupportStatus = 'supported' | 'discussion-gated' | 'retired';

export type GraphWriteEvidenceProjectionSource = 'SemanticStatement' | 'ActionContext' | 'UE ActionDatabase';

export type GraphWriteReviewEvidencePolicy = 'graph_surface_atomic_target';

export interface GraphWriteOperationContract {
  readonly id: string;
  readonly kind: string;
  readonly supportStatus: GraphWriteSupportStatus;
  readonly reviewEvidence: GraphWriteReviewEvidencePolicy;
  readonly requiredEvidenceKeys?: readonly string[];
}

export interface GraphWriteClusterContract {
  readonly id: 'function_action' | 'field' | 'event' | 'asset_action' | 'container_action';
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
  readonly finalAcceptance: {
    readonly generalityPreflightAfterStable: true;
    readonly smokePlan: 'BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md';
  };
}

const ASSET_ACTION_REQUIRED_EVIDENCE_KEYS = [
  'asset_action_stable_id',
  'asset_action_node_class',
  'asset_action_spawner_signature',
  'asset_action_owner_path',
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
  ],
  finalAcceptance: {
    generalityPreflightAfterStable: true,
    smokePlan: 'BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md',
  },
};
