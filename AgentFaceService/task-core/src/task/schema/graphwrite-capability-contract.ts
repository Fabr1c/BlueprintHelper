export type GraphWriteSupportStatus = 'supported' | 'discussion-gated' | 'retired' | 'rejected';

export type GraphWriteEvidenceProjectionSource = 'SemanticStatement' | 'ActionContext' | 'UE ActionDatabase';

export type GraphWriteReviewEvidencePolicy = 'graph_surface_atomic_target';

export type GraphWriteRuntimeOwner = 'FunctionAction' | 'GenericAssetStructControlAction' | 'EventDelegateAction';
export type GraphWriteLogicalRuntimeCluster = GraphWriteRuntimeOwner;

export interface GraphWriteOperationGroupOperation {
  readonly id: string;
  readonly supportStatus: GraphWriteSupportStatus;
  readonly runtimeOwner?: GraphWriteRuntimeOwner;
  readonly runtimeCluster?: GraphWriteLogicalRuntimeCluster;
  readonly semanticKind?: string;
  readonly semanticFamily?: string;
  readonly secondStageOperation?: string;
  readonly priority?: 'P0' | 'P1' | 'P2';
  readonly requiredEvidenceKeys?: readonly string[];
  readonly excludedReason?: string;
  readonly rejectionReason?: string;
}

export interface GraphWriteOperationGroupContract {
  readonly id:
    | 'op_coverage'
    | 'event_delegate'
    | 'generic_ops.control'
    | 'generic_ops.container'
    | 'generic_ops.transform'
    | 'generic_ops.create'
    | 'generic_ops.schedule'
    | 'generic_ops.struct_select';
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
    runtimeOwner: 'FunctionAction',
    runtimeCluster: 'FunctionAction',
    semanticKind: 'op',
    semanticFamily: 'operator',
    secondStageOperation: `op.${id}`,
    priority,
    requiredEvidenceKeys,
  };
}

export const GENERIC_OPS_FORBIDDEN_RUNTIME_CLUSTER_IDS = [
  'control',
  'generic_transform',
  'generic_create',
  'struct_select',
  'generic_op',
] as const;

export const GENERIC_OPS_EVIDENCE_KEYS = [
  'generic.control.operation',
  'generic.control.case_values',
  'generic.control.enum_path',
  'generic.control.default_policy',
  'generic.control.dynamic_output_count',
  'generic.macro.graph_path',
  'generic.macro.pin_shape_snapshot',
  'generic.macro.world_context_policy',
  'generic.create.operation',
  'generic.create.class_path',
  'generic.create.asset_path',
  'generic.create.expose_on_spawn',
  'generic.transform.operation',
  'generic.transform.source_pin_type',
  'generic.transform.target_pin_type',
  'generic.transform.cast_policy',
  'generic.schedule.operation',
  'generic.schedule.graph_latent_allowed',
  'generic.schedule.handler_evidence_id',
  'generic.struct.struct_path',
  'generic.struct.selected_field_paths',
  'generic.struct.optional_pin_policy',
  'generic.select.result_type_proof',
  'container.kind',
  'container.operation',
  'container.collection_pin_type',
  'container.element_pin_type',
  'container.key_pin_type',
  'container.value_pin_type',
] as const;

type GenericOpsFamily = 'control' | 'container' | 'transform' | 'create' | 'schedule' | 'struct_select';

function makeSupportedGenericOp(
  family: GenericOpsFamily,
  operation: string,
  runtimeOwner: GraphWriteRuntimeOwner,
  requiredEvidenceKeys: readonly string[],
): GraphWriteOperationGroupOperation {
  return {
    id: `generic_ops.${family}.${operation}`,
    supportStatus: 'supported',
    runtimeOwner,
    runtimeCluster: runtimeOwner,
    semanticKind: family === 'struct_select' ? 'struct_select' : family,
    semanticFamily: `generic_ops.${family}`,
    secondStageOperation: `generic.${family}.${operation}`,
    requiredEvidenceKeys,
  };
}

function makeRejectedGenericOp(
  family: GenericOpsFamily,
  operation: string,
  rejectionReason: string,
): GraphWriteOperationGroupOperation {
  return {
    id: `generic_ops.${family}.${operation}`,
    supportStatus: 'rejected',
    semanticKind: family === 'struct_select' ? 'struct_select' : family,
    semanticFamily: `generic_ops.${family}`,
    secondStageOperation: `generic.${family}.${operation}`,
    requiredEvidenceKeys: [],
    excludedReason: rejectionReason,
    rejectionReason,
  };
}

const GENERIC_CONTROL_SINGLETON_OPERATIONS = ['branch', 'sequence', 'return'] as const;
const GENERIC_CONTROL_SWITCH_OPERATIONS = ['switch_int', 'switch_string', 'switch_name', 'switch_enum'] as const;
const GENERIC_CONTROL_DYNAMIC_OPERATIONS = ['multi_gate'] as const;
const GENERIC_CONTROL_MACRO_OPERATIONS = [
  'do_once',
  'do_n',
  'gate',
  'flip_flop',
  'for_loop',
  'for_loop_with_break',
  'foreach_loop',
  'foreach_loop_with_break',
  'while_loop',
] as const;

const GENERIC_CONTROL_EVIDENCE_KEYS = ['generic.control.operation'] as const;
const GENERIC_CONTROL_SWITCH_EVIDENCE_KEYS = [
  'generic.control.operation',
  'generic.control.case_values',
  'generic.control.default_policy',
] as const;
const GENERIC_CONTROL_SWITCH_ENUM_EVIDENCE_KEYS = [
  'generic.control.operation',
  'generic.control.case_values',
  'generic.control.enum_path',
  'generic.control.default_policy',
] as const;
const GENERIC_CONTROL_DYNAMIC_EVIDENCE_KEYS = [
  'generic.control.operation',
  'generic.control.dynamic_output_count',
] as const;
const GENERIC_MACRO_EVIDENCE_KEYS = [
  'generic.control.operation',
  'generic.macro.graph_path',
  'generic.macro.pin_shape_snapshot',
] as const;

const GENERIC_CONTAINER_OPERATIONS = {
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

const GENERIC_CONTAINER_EVIDENCE_KEYS = [
  'container.kind',
  'container.operation',
  'container.collection_pin_type',
] as const;

const GENERIC_TRANSFORM_GENERIC_OPERATIONS = ['dynamic_cast', 'class_cast', 'interface_dynamic_cast'] as const;
const GENERIC_TRANSFORM_FUNCTION_OPERATIONS = [
  'type_promotion',
  'function_conversion',
  'blueprint_autocast',
  'numeric_conversion',
  'string_name_text_conversion',
  'enum_conversion',
  'object_to_soft_object',
  'class_to_soft_class',
] as const;
const GENERIC_TRANSFORM_EVIDENCE_KEYS = [
  'generic.transform.operation',
  'generic.transform.source_pin_type',
  'generic.transform.target_pin_type',
] as const;

const GENERIC_CREATE_GENERIC_OPERATIONS = [
  'spawn_actor',
  'create_widget',
  'construct_object',
  'make_array',
  'make_map',
  'make_set',
  'asset_action',
  'asset_backed_graph_node',
] as const;
const GENERIC_CREATE_FUNCTION_OPERATIONS = [
  'async_action',
  'function_backed_create',
  'function_backed_spawn',
  'function_backed_construct',
] as const;
const GENERIC_CREATE_EVIDENCE_KEYS = [
  'generic.create.operation',
  'generic.create.class_path',
  'generic.create.expose_on_spawn',
] as const;
const GENERIC_ASSET_CREATE_EVIDENCE_KEYS = [
  'generic.create.operation',
  'generic.create.asset_path',
  'asset_action_stable_id',
  'asset_action_spawner_signature',
] as const;

const GENERIC_SCHEDULE_GENERIC_OPERATIONS = ['timer_delegate_node', 'latent_or_async_node'] as const;
const GENERIC_SCHEDULE_FUNCTION_OPERATIONS = [
  'timer_by_function_name',
  'timer_by_handle',
  'timer_clear_by_handle',
  'timer_clear_by_function_name',
  'timer_pause_by_handle',
  'timer_pause_by_function_name',
  'timer_unpause_by_handle',
  'timer_unpause_by_function_name',
  'delay',
  'retriggerable_delay',
  'delay_until_next_tick',
  'generic_latent_function_call',
  'async_proxy_output_delegate_connection',
] as const;
const GENERIC_SCHEDULE_EVIDENCE_KEYS = [
  'generic.schedule.operation',
  'schedule_action_stable_id',
  'schedule_spawner_signature',
] as const;
const GENERIC_LATENT_SCHEDULE_EVIDENCE_KEYS = [
  'generic.schedule.operation',
  'schedule_action_stable_id',
  'schedule_spawner_signature',
  'generic.schedule.graph_latent_allowed',
] as const;

const GENERIC_STRUCT_SELECT_OPERATIONS = ['make_struct', 'break_struct', 'set_fields_in_struct', 'select'] as const;
const GENERIC_STRUCT_SELECT_EVIDENCE_KEYS = [
  'generic.struct.struct_path',
  'generic.struct.selected_field_paths',
  'generic.select.result_type_proof',
] as const;

const GENERIC_OPS_OPERATION_GROUPS = [
  {
    id: 'generic_ops.control',
    responsibility:
      'GenericOps publishes control and StandardMacros as a logical capability group owned by GenericAssetStructControlAction; it is not a new runtime cluster.',
    operations: [
      ...GENERIC_CONTROL_SINGLETON_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('control', operation, 'GenericAssetStructControlAction', GENERIC_CONTROL_EVIDENCE_KEYS),
      ),
      ...GENERIC_CONTROL_SWITCH_OPERATIONS.map((operation) =>
        makeSupportedGenericOp(
          'control',
          operation,
          'GenericAssetStructControlAction',
          operation === 'switch_enum' ? GENERIC_CONTROL_SWITCH_ENUM_EVIDENCE_KEYS : GENERIC_CONTROL_SWITCH_EVIDENCE_KEYS,
        ),
      ),
      ...GENERIC_CONTROL_DYNAMIC_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('control', operation, 'GenericAssetStructControlAction', GENERIC_CONTROL_DYNAMIC_EVIDENCE_KEYS),
      ),
      ...GENERIC_CONTROL_MACRO_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('control', operation, 'GenericAssetStructControlAction', GENERIC_MACRO_EVIDENCE_KEYS),
      ),
    ],
  },
  {
    id: 'generic_ops.container',
    responsibility:
      'GenericOps exposes container operations publicly, while runtime execution stays FunctionAction-owned through the container/callable path.',
    operations: Object.entries(GENERIC_CONTAINER_OPERATIONS).flatMap(([containerKind, operations]) =>
      operations.map((operation) =>
        makeSupportedGenericOp(
          'container',
          `${containerKind}.${operation}`,
          'FunctionAction',
          GENERIC_CONTAINER_EVIDENCE_KEYS,
        ),
      ),
    ),
  },
  {
    id: 'generic_ops.transform',
    responsibility:
      'GenericOps transform operations split by existing owner: generic cast node evidence stays GenericAssetStructControlAction, function-backed conversion stays FunctionAction.',
    operations: [
      ...GENERIC_TRANSFORM_GENERIC_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('transform', operation, 'GenericAssetStructControlAction', GENERIC_TRANSFORM_EVIDENCE_KEYS),
      ),
      ...GENERIC_TRANSFORM_FUNCTION_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('transform', operation, 'FunctionAction', GENERIC_TRANSFORM_EVIDENCE_KEYS),
      ),
      makeRejectedGenericOp('transform', 'link_time_auto_conversion', 'link_time_auto_conversion_requires_linker_readback'),
    ],
  },
  {
    id: 'generic_ops.create',
    responsibility:
      'GenericOps create operations use projected generic spawner or asset evidence when generic-owned; function-backed factories stay FunctionAction-owned.',
    operations: [
      ...GENERIC_CREATE_GENERIC_OPERATIONS.map((operation) =>
        makeSupportedGenericOp(
          'create',
          operation,
          'GenericAssetStructControlAction',
          operation.startsWith('asset') ? GENERIC_ASSET_CREATE_EVIDENCE_KEYS : GENERIC_CREATE_EVIDENCE_KEYS,
        ),
      ),
      ...GENERIC_CREATE_FUNCTION_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('create', operation, 'FunctionAction', GENERIC_CREATE_EVIDENCE_KEYS),
      ),
    ],
  },
  {
    id: 'generic_ops.schedule',
    responsibility:
      'GenericOps schedule operations use generic spawner evidence only for generic schedule nodes; function-backed timer/latent calls stay FunctionAction-owned.',
    operations: [
      ...GENERIC_SCHEDULE_GENERIC_OPERATIONS.map((operation) =>
        makeSupportedGenericOp(
          'schedule',
          operation,
          'GenericAssetStructControlAction',
          operation === 'latent_or_async_node' ? GENERIC_LATENT_SCHEDULE_EVIDENCE_KEYS : GENERIC_SCHEDULE_EVIDENCE_KEYS,
        ),
      ),
      ...GENERIC_SCHEDULE_FUNCTION_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('schedule', operation, 'FunctionAction', GENERIC_SCHEDULE_EVIDENCE_KEYS),
      ),
    ],
  },
  {
    id: 'generic_ops.struct_select',
    responsibility:
      'GenericOps struct/select operations stay GenericAssetStructControlAction-owned and require field policy or result-type proof evidence.',
    operations: [
      ...GENERIC_STRUCT_SELECT_OPERATIONS.map((operation) =>
        makeSupportedGenericOp('struct_select', operation, 'GenericAssetStructControlAction', GENERIC_STRUCT_SELECT_EVIDENCE_KEYS),
      ),
      makeRejectedGenericOp('struct_select', 'split_pin', 'split_pin_is_not_a_graphwrite_statement_operation'),
      makeRejectedGenericOp('struct_select', 'recombine_pin', 'recombine_pin_is_not_a_graphwrite_statement_operation'),
    ],
  },
] as const satisfies readonly GraphWriteOperationGroupContract[];

export const GENERIC_OPS_OPERATION_GROUP_IDS = GENERIC_OPS_OPERATION_GROUPS.map((group) => group.id);
export const GENERIC_OPS_OPERATION_IDS = GENERIC_OPS_OPERATION_GROUPS.flatMap((group) =>
  group.operations.map((operation) => operation.id),
);

const EVENT_DELEGATE_COMMON_EVIDENCE_KEYS = [
  'event_delegate.delegate_owner_class_path',
  'event_delegate.delegate_property_name',
  'event_delegate.delegate_property_path',
  'event_delegate.delegate_signature_function_path',
] as const;

const EVENT_DELEGATE_HANDLER_EVIDENCE_KEYS = [
  'event_delegate.handler_function_path',
  'event_delegate.handler_source_cluster',
  'event_delegate.signature_evidence_id',
] as const;

function makeSupportedEventDelegateOp(
  operation: string,
  semanticKind: 'component_bound_event' | 'delegate',
  secondStageOperation: string,
  requiredEvidenceKeys: readonly string[],
): GraphWriteOperationGroupOperation {
  return {
    id: `event_delegate.${operation}`,
    supportStatus: 'supported',
    runtimeOwner: 'EventDelegateAction',
    runtimeCluster: 'EventDelegateAction',
    semanticKind,
    semanticFamily: 'event_delegate',
    secondStageOperation,
    requiredEvidenceKeys,
  };
}

function makeRejectedEventDelegateOp(
  operation: string,
  rejectionReason: string,
): GraphWriteOperationGroupOperation {
  return {
    id: `event_delegate.${operation}`,
    supportStatus: 'rejected',
    runtimeOwner: 'EventDelegateAction',
    runtimeCluster: 'EventDelegateAction',
    semanticKind: 'component_bound_event',
    semanticFamily: 'event_delegate',
    secondStageOperation: `event_delegate.${operation}`,
    requiredEvidenceKeys: [],
    excludedReason: rejectionReason,
    rejectionReason,
  };
}

const EVENT_DELEGATE_OPERATION_GROUP = {
  id: 'event_delegate',
  responsibility:
    'EventDelegate publishes use-site operations as a logical capability group owned by EventDelegateActionCluster; it never creates event declarations, handlers, or signature mutations.',
  operations: [
    makeSupportedEventDelegateOp('component_bound_event', 'component_bound_event', 'component_bound_event', [
      'event_delegate.component_binding_owner_class_path',
      'event_delegate.component_property_name',
      'event_delegate.component_binding_field_path',
      'event_delegate.component_class_path',
      ...EVENT_DELEGATE_COMMON_EVIDENCE_KEYS,
      ...EVENT_DELEGATE_HANDLER_EVIDENCE_KEYS,
      'event_delegate.duplicate_policy',
    ]),
    makeSupportedEventDelegateOp('delegate.bind', 'delegate', 'delegate.bind', [
      'event_delegate.binding_object_kind',
      'event_delegate.binding_object_evidence_id',
      ...EVENT_DELEGATE_COMMON_EVIDENCE_KEYS,
      ...EVENT_DELEGATE_HANDLER_EVIDENCE_KEYS,
    ]),
    makeSupportedEventDelegateOp('delegate.assign', 'delegate', 'delegate.assign', [
      'event_delegate.binding_object_kind',
      'event_delegate.binding_object_evidence_id',
      ...EVENT_DELEGATE_COMMON_EVIDENCE_KEYS,
      ...EVENT_DELEGATE_HANDLER_EVIDENCE_KEYS,
      'event_delegate.assign_factory',
    ]),
    makeSupportedEventDelegateOp('delegate.unbind', 'delegate', 'delegate.unbind', [
      'event_delegate.binding_object_kind',
      'event_delegate.binding_object_evidence_id',
      ...EVENT_DELEGATE_COMMON_EVIDENCE_KEYS,
      ...EVENT_DELEGATE_HANDLER_EVIDENCE_KEYS,
      'event_delegate.unbind_mode',
    ]),
    makeSupportedEventDelegateOp('delegate.call', 'delegate', 'delegate.call', [
      'event_delegate.binding_object_kind',
      'event_delegate.binding_object_evidence_id',
      ...EVENT_DELEGATE_COMMON_EVIDENCE_KEYS,
      'event_delegate.call_arg_policy',
    ]),
    makeSupportedEventDelegateOp('delegate.clear', 'delegate', 'delegate.clear', [
      'event_delegate.binding_object_kind',
      'event_delegate.binding_object_evidence_id',
      'event_delegate.delegate_owner_class_path',
      'event_delegate.delegate_property_name',
      'event_delegate.delegate_property_path',
      'event_delegate.unbind_mode',
    ]),
    makeRejectedEventDelegateOp('component_bound_duplicate_policy.replace', 'duplicate_mutation_policy_blocked'),
    makeRejectedEventDelegateOp('component_bound_duplicate_policy.merge', 'duplicate_mutation_policy_blocked'),
  ],
} as const satisfies GraphWriteOperationGroupContract;

function makeRejectedOp(id: string): GraphWriteOperationGroupOperation {
  return {
    id,
    supportStatus: 'rejected',
    excludedReason: 'excluded_op_operation',
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
    EVENT_DELEGATE_OPERATION_GROUP,
    ...GENERIC_OPS_OPERATION_GROUPS,
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
