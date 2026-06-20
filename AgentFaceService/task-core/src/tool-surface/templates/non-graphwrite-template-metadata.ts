import type {
  NonGraphWriteTemplateFamilyMetadata,
  TaskSpecTemplateFamilyNavigation,
} from './taskspec-template-types.js';
import {
  NON_GRAPHWRITE_OPERATION_DESCRIPTORS,
  type NonGraphWriteOperationDescriptor,
} from './non-graphwrite-operation-metadata.js';
import { UMG_WIDGET_OPERATION_MANIFEST } from './generated/umg-widget-operation-manifest.generated.js';
import type {
  TaskSpecTemplateClusterItem,
  TaskSpecTemplateOperationItem,
  TaskSpecTemplateQuickAccessItem,
} from './taskspec-template-types.js';

const CLUSTER_OPERATION_NAVIGATION: TaskSpecTemplateFamilyNavigation = {
  levels: ['cluster', 'operation', 'quick_access', 'leaf_template'],
  next_command: 'bh tools templates clusters --family <family> --format json',
  compose_command: 'bh tools templates compose --template <leaf_template_id> --out <task-spec.json> --format json',
  requires_write_mode: false,
};

const OPERATION_NAVIGATION: TaskSpecTemplateFamilyNavigation = {
  levels: ['operation', 'quick_access', 'leaf_template'],
  next_command: 'bh tools templates operations --family <family> --format json',
  compose_command: 'bh tools templates compose --template <leaf_template_id> --out <task-spec.json> --format json',
  requires_write_mode: false,
};

export const NON_GRAPHWRITE_TEMPLATE_FAMILIES: readonly NonGraphWriteTemplateFamilyMetadata[] = [
  {
    family: 'blueprint_variables',
    task_type: 'edit_blueprint_variables',
    description: 'Edit Blueprint member variables, defaults, and related variable metadata.',
    strategy_field: 'variable_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_variables_template.json',
    insert_targets: ['behavior.variables[]', 'behavior.changes[]', 'behavior.defaults[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'blueprint_components',
    task_type: 'edit_blueprint_components',
    description: 'Edit Blueprint component tree entries when the dedicated component template path is active.',
    strategy_field: 'component_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_components_template.json',
    insert_targets: ['behavior.changes[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'blueprint_class_settings',
    task_type: 'edit_blueprint_class_settings',
    description: 'Edit Blueprint class settings such as interfaces, defaults, and reparenting.',
    strategy_field: 'class_settings_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_class_settings_template.json',
    insert_targets: ['behavior.interfaces.ensure_present[]', 'behavior.interfaces.ensure_absent[]', 'behavior.class_defaults[]', 'behavior.reparent'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'blueprint_signature',
    task_type: 'edit_blueprint_signature',
    description: 'Edit function, event, or macro signatures before writing bodies.',
    strategy_field: 'signature_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_edit_signature_template.json',
    insert_targets: ['behavior.changes[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'blueprint_create_feature',
    task_type: 'create_blueprint_feature',
    description: 'Create a composite Blueprint feature including components, variables, class settings, and behavior.',
    strategy_field: 'composite',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
    insert_targets: ['components[]', 'variables[]', 'class_settings', 'behavior'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'umg_widget',
    task_type: 'edit_umg_widget',
    description: 'Edit Widget Blueprint tree, widget properties, named slots, and widget class settings.',
    strategy_field: 'widget_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/umg_widget_edit_template.json',
    insert_targets: ['behavior.changes[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'data_table',
    task_type: 'edit_data_table',
    description: 'Edit DataTable rows through TaskSpec row operations.',
    strategy_field: 'row_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_table_rows_edit_template.json',
    insert_targets: ['behavior.rows[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'object_properties',
    task_type: 'edit_object_properties',
    description: 'Edit UObject, DataAsset, or Blueprint object properties.',
    strategy_field: 'property_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_object_properties_edit_template.json',
    insert_targets: ['behavior.changes[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'asset_factory',
    task_type: 'create_asset',
    description: 'Create Agent-facing supported Unreal assets through specialized asset factory root TaskSpec templates.',
    strategy_field: 'asset_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_asset_create_template.json',
    insert_targets: ['behavior.asset'],
    navigation: OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'material_graph',
    task_type: 'edit_material_graph',
    description: 'Edit Material graph expressions, owned blocks, and material output links through MaterialGraph TaskSpec routes.',
    strategy_field: 'graph_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/material_graph_edit_template.json',
    insert_targets: ['behavior.entries[]', 'behavior.replace', 'behavior.patches[]', 'behavior.merges[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
  {
    family: 'material_instance',
    task_type: 'edit_material_instance',
    description: 'Edit MaterialInstance parent and parameter overrides through MaterialInstance TaskSpec routes.',
    strategy_field: 'material_instance_strategy',
    internal_scaffold_template_path: 'AgentFaceService/agent-guide/Templates/write/routes/material_instance_edit_template.json',
    insert_targets: ['behavior.operations[]'],
    navigation: CLUSTER_OPERATION_NAVIGATION,
    status: 'supported',
  },
];

export function getSupportedNonGraphWriteTemplateFamilies(): readonly NonGraphWriteTemplateFamilyMetadata[] {
  return NON_GRAPHWRITE_TEMPLATE_FAMILIES.filter((entry) => entry.status === 'supported');
}

export function getNonGraphWriteTemplateFamily(
  family: string,
): NonGraphWriteTemplateFamilyMetadata | undefined {
  return NON_GRAPHWRITE_TEMPLATE_FAMILIES.find((entry) => entry.family === family);
}

export function listNonGraphWriteTemplateClusters(input: {
  family: string;
}): TaskSpecTemplateClusterItem[] {
  const descriptorClusters = uniqueSorted(
    NON_GRAPHWRITE_OPERATION_DESCRIPTORS.filter((descriptor) => descriptor.family === input.family),
    (descriptor) => descriptor.cluster_id,
  );
  if (descriptorClusters.length > 0) {
    return descriptorClusters.map((clusterId) => ({
      family: input.family as TaskSpecTemplateClusterItem['family'],
      cluster_id: clusterId,
      description: describeNonGraphWriteCluster(clusterId),
      unsupported_write_modes: [],
    }));
  }
  if (input.family !== 'umg_widget') {
    return [];
  }
  return [
    {
      family: 'umg_widget',
      cluster_id: 'widget_tree',
      description: 'Widget tree and Widget Blueprint structural edit operations.',
      unsupported_write_modes: [],
    },
  ];
}

export function listNonGraphWriteTemplateOperations(input: {
  family: string;
  cluster?: string;
}): TaskSpecTemplateOperationItem[] {
  const cluster = input.cluster ?? '';
  const descriptors = NON_GRAPHWRITE_OPERATION_DESCRIPTORS.filter((descriptor) =>
    descriptor.family === input.family
    && (cluster.length === 0 || descriptor.cluster_id === cluster));
  if (descriptors.length > 0) {
    return descriptors.map((descriptor) => ({
      family: descriptor.family,
      cluster_id: descriptor.cluster_id,
      operation_id: descriptor.operation_id,
      description: descriptor.description,
      validation_classification: descriptor.validation_classification,
      runtime_only_validation_notes: descriptor.runtime_only_validation_notes ? [...descriptor.runtime_only_validation_notes] : undefined,
    }));
  }
  if (input.family !== 'umg_widget'
    || (cluster.length > 0 && cluster !== 'widget_tree')) {
    return [];
  }
  return UMG_WIDGET_OPERATION_MANIFEST.map((descriptor) => ({
    family: 'umg_widget',
    cluster_id: 'widget_tree',
    operation_id: descriptor.kind,
    description: describeUmgWidgetOperation(descriptor.kind),
    validation_classification: descriptor.validation_classification,
    runtime_only_validation_notes: runtimeOnlyValidationNotes(descriptor),
  }));
}

function describeUmgWidgetOperation(kind: string): string {
  const words = kind.replaceAll('_', ' ');
  return `Use for ${words} Widget Blueprint operations.`;
}

export function listNonGraphWriteTemplateQuickAccess(input: {
  family: string;
  cluster?: string;
  operation?: string;
}): TaskSpecTemplateQuickAccessItem[] {
  const cluster = input.cluster ?? '';
  const operation = input.operation ?? '';
  const descriptors = NON_GRAPHWRITE_OPERATION_DESCRIPTORS.filter((descriptor) =>
    descriptor.family === input.family
    && (cluster.length === 0 || descriptor.cluster_id === cluster)
    && (operation.length === 0 || descriptor.operation_id === operation));
  if (descriptors.length > 0) {
    return descriptors.map((descriptor) => ({
      template_id: descriptor.template_id,
      family: descriptor.family,
      cluster_id: descriptor.cluster_id,
      operation_id: descriptor.operation_id,
      quick_access_id: descriptor.operation_id,
      source_slot_id: descriptor.source_slot_id,
      slot_type: 'statement',
      arg_slots: [...descriptor.arg_slots],
      template_path: descriptor.template_path,
      insert_paths: [...descriptor.insert_paths],
      unsupported_write_modes: [],
      validation_classification: descriptor.validation_classification,
      runtime_only_validation_notes: descriptor.runtime_only_validation_notes ? [...descriptor.runtime_only_validation_notes] : undefined,
    }));
  }
  if (input.family !== 'umg_widget'
    || (cluster.length > 0 && cluster !== 'widget_tree')) {
    return [];
  }
  return UMG_WIDGET_OPERATION_MANIFEST
    .filter((descriptor) => operation.length === 0 || descriptor.kind === operation)
    .map((descriptor) => ({
      template_id: `umg.widget_tree.${descriptor.kind}`,
      family: 'umg_widget',
      cluster_id: 'widget_tree',
      operation_id: descriptor.kind,
      quick_access_id: descriptor.kind,
      source_slot_id: descriptor.kind,
      slot_type: 'statement',
      arg_slots: descriptor.required_fields.map((field) => `${field}(*)`),
      template_path: 'AgentFaceService/agent-guide/Templates/write/routes/umg_widget_edit_template.json',
      insert_paths: ['behavior.changes[]'],
      unsupported_write_modes: [],
      validation_classification: descriptor.validation_classification,
      runtime_only_validation_notes: runtimeOnlyValidationNotes(descriptor),
    }));
}

export function listNonGraphWriteValidationClassificationDescriptors(): readonly NonGraphWriteOperationDescriptor[] {
  return NON_GRAPHWRITE_OPERATION_DESCRIPTORS;
}

function runtimeOnlyValidationNotes(
  descriptor: object,
): string[] | undefined {
  if (!('runtime_only_validation_notes' in descriptor)) {
    return undefined;
  }
  const notes = descriptor.runtime_only_validation_notes;
  return Array.isArray(notes) ? notes.filter((note): note is string => typeof note === 'string') : undefined;
}

function describeNonGraphWriteCluster(clusterId: string): string {
  const descriptions: Readonly<Record<string, string>> = {
    component_tree: 'Blueprint component tree mutation operations.',
    class_settings: 'Blueprint class setting operations including interfaces, defaults, and reparenting.',
    signature: 'Blueprint function, event, macro, and dispatcher signature operations.',
    material_graph: 'MaterialGraph expression block operations and material output link operations.',
    material_instance: 'MaterialInstance parent and parameter override operations.',
  };
  return descriptions[clusterId] ?? `Non-GraphWrite ${clusterId} operations.`;
}

function uniqueSorted<T>(items: readonly T[], keyOf: (item: T) => string): string[] {
  return [...new Set(items.map(keyOf))].sort((left, right) => left.localeCompare(right));
}
