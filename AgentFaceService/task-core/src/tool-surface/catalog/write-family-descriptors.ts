import { getAgentVisibleGraphWriteRoutes } from '../../task/compiler/graphwrite/graphwrite-route-registry.js';

export type WriteFamilyCapabilityStatus = 'active' | 'hidden' | 'planned' | 'reserved';

export interface WriteFamilyDescriptor {
  write_family: string;
  runtime_adapter_id: string;
  task_spec_strategy: string;
  bridge_command: string;
  cluster_family: string;
  dry_run_policy_id: string;
  readback_projection_mode: string;
  result_projection_policy_id: string;
  metrics_identity: string;
  status: WriteFamilyCapabilityStatus;
}

export const WRITE_FAMILY_DESCRIPTORS: readonly WriteFamilyDescriptor[] = [
  {
    write_family: 'graphwrite',
    runtime_adapter_id: 'graphwrite',
    task_spec_strategy: 'graphwrite_route_descriptor',
    bridge_command: 'execute_task_plan',
    cluster_family: 'GraphWrite',
    dry_run_policy_id: 'write_family.graphwrite.full_preview',
    readback_projection_mode: 'graph_body_adapter',
    result_projection_policy_id: 'task_runtime.write_family.graphwrite',
    metrics_identity: 'blueprint.write.graphwrite',
    status: 'active',
  },
  {
    write_family: 'asset_factory',
    runtime_adapter_id: 'asset_factory',
    task_spec_strategy: 'asset_factory',
    bridge_command: 'execute_task_plan',
    cluster_family: 'AssetFactory',
    dry_run_policy_id: 'write_family.asset_factory.full_preview',
    readback_projection_mode: 'asset_factory',
    result_projection_policy_id: 'task_runtime.write_family.asset_factory',
    metrics_identity: 'blueprint.write.asset_factory',
    status: 'active',
  },
  {
    write_family: 'blueprint_signature',
    runtime_adapter_id: 'blueprint_signature',
    task_spec_strategy: 'blueprint_signature',
    bridge_command: 'execute_task_plan',
    cluster_family: 'Signature',
    dry_run_policy_id: 'write_family.blueprint_signature.full_preview',
    readback_projection_mode: 'blueprint_signature',
    result_projection_policy_id: 'task_runtime.write_family.blueprint_signature',
    metrics_identity: 'blueprint.write.signature',
    status: 'active',
  },
  {
    write_family: 'blueprint_variables',
    runtime_adapter_id: 'blueprint_variables',
    task_spec_strategy: 'blueprint_variables',
    bridge_command: 'execute_task_plan',
    cluster_family: 'BlueprintVariables',
    dry_run_policy_id: 'write_family.blueprint_variables.full_preview',
    readback_projection_mode: 'blueprint_variables',
    result_projection_policy_id: 'task_runtime.write_family.blueprint_variables',
    metrics_identity: 'blueprint.write.variables',
    status: 'active',
  },
  {
    write_family: 'class_settings',
    runtime_adapter_id: 'class_settings',
    task_spec_strategy: 'class_settings',
    bridge_command: 'execute_task_plan',
    cluster_family: 'ClassSettings',
    dry_run_policy_id: 'write_family.class_settings.full_preview',
    readback_projection_mode: 'class_settings',
    result_projection_policy_id: 'task_runtime.write_family.class_settings',
    metrics_identity: 'blueprint.write.class_settings',
    status: 'active',
  },
  {
    write_family: 'blueprint_component',
    runtime_adapter_id: 'blueprint_component',
    task_spec_strategy: 'blueprint_component',
    bridge_command: 'execute_task_plan',
    cluster_family: 'Component',
    dry_run_policy_id: 'write_family.blueprint_component.full_preview',
    readback_projection_mode: 'blueprint_component',
    result_projection_policy_id: 'task_runtime.write_family.blueprint_component',
    metrics_identity: 'blueprint.write.component',
    status: 'active',
  },
  {
    write_family: 'object_property',
    runtime_adapter_id: 'object_property',
    task_spec_strategy: 'property_strategy',
    bridge_command: 'execute_task_plan',
    cluster_family: 'ObjectProperty',
    dry_run_policy_id: 'write_family.object_property.full_preview',
    readback_projection_mode: 'object_property',
    result_projection_policy_id: 'task_runtime.write_family.object_property',
    metrics_identity: 'blueprint.write.object_property',
    status: 'active',
  },
  {
    write_family: 'data_table',
    runtime_adapter_id: 'data_table',
    task_spec_strategy: 'row_strategy',
    bridge_command: 'execute_task_plan',
    cluster_family: 'DataTable',
    dry_run_policy_id: 'write_family.data_table.full_preview',
    readback_projection_mode: 'data_table',
    result_projection_policy_id: 'task_runtime.write_family.data_table',
    metrics_identity: 'blueprint.write.data_table',
    status: 'active',
  },
  {
    write_family: 'umg_widget',
    runtime_adapter_id: 'umg_widget',
    task_spec_strategy: 'widget_strategy',
    bridge_command: 'execute_task_plan',
    cluster_family: 'UMGWidget',
    dry_run_policy_id: 'write_family.umg_widget.full_preview',
    readback_projection_mode: 'widget_tree',
    result_projection_policy_id: 'task_runtime.write_family.umg_widget',
    metrics_identity: 'umg.write.umg_widget',
    status: 'active',
  },
] as const;

const DESCRIPTORS_BY_WRITE_FAMILY = new Map(
  WRITE_FAMILY_DESCRIPTORS.map((descriptor) => [descriptor.write_family, descriptor] as const),
);

export function getAllWriteFamilyDescriptors(): readonly WriteFamilyDescriptor[] {
  return WRITE_FAMILY_DESCRIPTORS;
}

export function getActiveWriteFamilyDescriptors(): readonly WriteFamilyDescriptor[] {
  return WRITE_FAMILY_DESCRIPTORS.filter((descriptor) => descriptor.status === 'active');
}

export function getAgentVisibleWriteFamilyDescriptors(): readonly WriteFamilyDescriptor[] {
  return getActiveWriteFamilyDescriptors();
}

export function getWriteFamilyDescriptor(writeFamily: string): WriteFamilyDescriptor | undefined {
  return DESCRIPTORS_BY_WRITE_FAMILY.get(writeFamily);
}

export function requireWriteFamilyDescriptor(writeFamily: string): WriteFamilyDescriptor {
  const descriptor = getWriteFamilyDescriptor(writeFamily);
  if (!descriptor) {
    throw new Error(`Unknown write family descriptor: ${writeFamily}`);
  }
  return descriptor;
}

export function resolveRuntimeAdapterIdsForDescriptor(
  descriptor: WriteFamilyDescriptor,
): readonly string[] {
  if (descriptor.write_family !== 'graphwrite') {
    return [descriptor.runtime_adapter_id];
  }

  return [...new Set(
    getAgentVisibleGraphWriteRoutes()
      .map((route) => route.runtime_adapter_id)
      .filter((runtimeAdapterId) => runtimeAdapterId.length > 0),
  )];
}
