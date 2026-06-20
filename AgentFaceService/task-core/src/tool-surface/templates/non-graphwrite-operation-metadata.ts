import type {
  NonGraphWriteFamily,
  WriteValidationClassification,
} from './taskspec-template-types.js';

export interface NonGraphWriteOperationDescriptor {
  readonly family: NonGraphWriteFamily;
  readonly cluster_id: string;
  readonly operation_id: string;
  readonly description: string;
  readonly template_id: string;
  readonly source_slot_id: string;
  readonly template_path: string;
  readonly insert_paths: readonly string[];
  readonly arg_slots: readonly string[];
  readonly validation_classification: WriteValidationClassification;
  readonly runtime_only_validation_notes?: readonly string[];
  readonly agent_visible_asset_kind?: 'data_asset' | 'blueprint' | 'widget_blueprint';
}

export const NON_GRAPHWRITE_OPERATION_DESCRIPTORS: readonly NonGraphWriteOperationDescriptor[] = [
  op('blueprint_variables', 'variables', 'ensure_member_variable', 'Ensure a Blueprint member variable exists.', 'blueprint_variables_ensure_member_variable', ['behavior.changes[]'], ['name(*)', 'pin_type(*)']),
  op('blueprint_variables', 'variables', 'configure_member_variable', 'Configure Blueprint member variable metadata, defaults, replication, or editability.', 'blueprint_variables_configure_member_variable', ['behavior.changes[]'], ['name(*)']),
  op('blueprint_components', 'component_tree', 'ensure_component_present', 'Ensure a component exists in the Blueprint component tree.', 'blueprint_component_ensure_present', ['behavior.changes[]'], ['name(*)', 'class(*)']),
  op('blueprint_components', 'component_tree', 'configure_component', 'Set properties on an owned SCS Blueprint component; route native/inherited component defaults through blueprint_class_settings.class_default.', 'blueprint_component_configure', ['behavior.changes[]'], ['name(*)', 'properties(*)']),
  op('blueprint_components', 'component_tree', 'rename_component', 'Rename a Blueprint component.', 'blueprint_component_rename', ['behavior.changes[]'], ['name(*)', 'new_name(*)']),
  op('blueprint_components', 'component_tree', 'reparent_component', 'Move a component under a new parent component.', 'blueprint_component_reparent', ['behavior.changes[]'], ['name(*)', 'new_parent(*)']),
  op('blueprint_components', 'component_tree', 'attach_component', 'Attach a component to a parent component or socket.', 'blueprint_component_attach', ['behavior.changes[]'], ['name(*)', 'parent(*)']),
  op('blueprint_components', 'component_tree', 'detach_component', 'Detach a component while preserving transform policy.', 'blueprint_component_detach', ['behavior.changes[]'], ['name(*)']),
  op('blueprint_components', 'component_tree', 'set_root_component', 'Set the Blueprint root component.', 'blueprint_component_set_root', ['behavior.changes[]'], ['name(*)']),
  op('blueprint_components', 'component_tree', 'remove_component', 'Remove a Blueprint component.', 'blueprint_component_remove', ['behavior.changes[]'], ['name(*)']),
  op('blueprint_class_settings', 'class_settings', 'add_interface', 'Ensure an implemented interface is present.', 'blueprint_class_settings_add_interface', ['behavior.interfaces.ensure_present[]'], ['interface_path(*)']),
  op('blueprint_class_settings', 'class_settings', 'remove_interface', 'Ensure an implemented interface is absent.', 'blueprint_class_settings_remove_interface', ['behavior.interfaces.ensure_absent[]'], ['interface_path(*)']),
  op('blueprint_class_settings', 'class_settings', 'set_class_default', 'Set a Blueprint class default property, including native component default properties such as WeaponComponent.PrimaryWeapon.', 'blueprint_class_settings_class_default', ['behavior.class_defaults[]'], ['property_path(*)', 'value(*)']),
  op('blueprint_class_settings', 'class_settings', 'reparent', 'Reparent the Blueprint class.', 'blueprint_class_settings_reparent', ['behavior.reparent'], ['new_parent_class(*)']),
  op('blueprint_signature', 'signature', 'ensure_function', 'Ensure a function signature exists.', 'blueprint_signature_ensure_function', ['behavior.changes[]'], ['function_name(*)']),
  op('blueprint_signature', 'signature', 'ensure_interface_function', 'Ensure an interface function signature exists.', 'blueprint_signature_ensure_interface_function', ['behavior.changes[]'], ['function_name(*)', 'interface_path(*)']),
  op('blueprint_signature', 'signature', 'ensure_custom_event', 'Ensure a custom event signature exists.', 'blueprint_signature_ensure_custom_event', ['behavior.changes[]'], ['event_name(*)', 'graph_name(*)']),
  op('blueprint_signature', 'signature', 'ensure_interface_event', 'Ensure an interface event signature exists.', 'blueprint_signature_ensure_interface_event', ['behavior.changes[]'], ['event_name(*)', 'graph_name(*)', 'interface_path(*)']),
  op('blueprint_signature', 'signature', 'ensure_macro', 'Ensure a macro signature exists.', 'blueprint_signature_ensure_macro', ['behavior.changes[]'], ['macro_name(*)']),
  op('blueprint_signature', 'signature', 'ensure_event_dispatcher', 'Ensure an event dispatcher signature exists.', 'blueprint_signature_ensure_event_dispatcher', ['behavior.changes[]'], ['dispatcher_name(*)']),
  op('blueprint_signature', 'signature', 'ensure_override_event', 'Ensure an override event signature exists.', 'blueprint_signature_ensure_override_event', ['behavior.changes[]'], ['event_name(*)']),
  op('blueprint_signature', 'signature', 'remove_signature', 'Remove a signature with reference-context protection.', 'blueprint_signature_remove', ['behavior.changes[]'], ['signature_name(*)']),
  baseOp(
    'blueprint_create_feature',
    'feature',
    'create_feature',
    'Create a composite Blueprint feature scaffold.',
    'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_feature_template.json',
  ),
  baseOp(
    'data_table',
    'rows',
    'edit_rows',
    'Create a DataTable row edit scaffold.',
    'AgentFaceService/agent-guide/Templates/write/routes/data_table_rows_edit_template.json',
  ),
  baseOp(
    'object_properties',
    'properties',
    'edit_properties',
    'Create a UObject property edit scaffold.',
    'AgentFaceService/agent-guide/Templates/write/routes/data_object_properties_edit_template.json',
  ),
  {
    family: 'asset_factory',
    cluster_id: 'asset',
    operation_id: 'create_data_asset',
    template_id: 'asset_factory.asset.create_data_asset',
    template_path: 'AgentFaceService/agent-guide/Templates/write/routes/data_asset_create_template.json',
    source_slot_id: 'asset',
    insert_paths: ['behavior.asset'],
    arg_slots: [],
    description: 'Create a concrete DataAsset instance through the Agent-facing asset factory template.',
    validation_classification: 'shared_policy',
    agent_visible_asset_kind: 'data_asset',
  },
  {
    family: 'asset_factory',
    cluster_id: 'asset',
    operation_id: 'create_blueprint',
    template_id: 'asset_factory.asset.create_blueprint',
    template_path: 'AgentFaceService/agent-guide/Templates/write/routes/blueprint_create_template.json',
    source_slot_id: 'asset',
    insert_paths: ['behavior.asset'],
    arg_slots: [],
    description: 'Create a Blueprint asset with an explicit parent class through the Agent-facing asset factory template.',
    validation_classification: 'shared_policy',
    agent_visible_asset_kind: 'blueprint',
  },
  {
    family: 'asset_factory',
    cluster_id: 'asset',
    operation_id: 'create_widget_blueprint',
    template_id: 'asset_factory.asset.create_widget_blueprint',
    template_path: 'AgentFaceService/agent-guide/Templates/write/routes/widget_blueprint_create_template.json',
    source_slot_id: 'asset',
    insert_paths: ['behavior.asset'],
    arg_slots: [],
    description: 'Create a WidgetBlueprint asset with an explicit UserWidget parent class through the Agent-facing asset factory template.',
    validation_classification: 'shared_policy',
    agent_visible_asset_kind: 'widget_blueprint',
  },
  op('material_graph', 'material_graph', 'append_block', 'Append a new owned Material expression block and connect it to a Material output.', 'material_graph_append_block', ['behavior.entries[]'], ['block_id(*)', 'node_key(*)']),
  op('material_graph', 'material_graph', 'replace_block', 'Replace an owned Material expression block by block_id.', 'material_graph_replace_block', ['behavior.replace'], ['block_id(*)', 'node_key(*)']),
  op('material_graph', 'material_graph', 'patch_block', 'Patch owned Material expressions, links, or deletes by block_id.', 'material_graph_patch_block', ['behavior.patches[]'], ['block_id(*)']),
  op('material_graph', 'material_graph', 'merge_block', 'Merge additional Material expressions or links into an owned block.', 'material_graph_merge_block', ['behavior.merges[]'], ['block_id(*)']),
  op('material_instance', 'material_instance', 'create_material_instance', 'Create a MaterialInstanceConstant asset with an optional parent material.', 'material_instance_create_asset', ['behavior.operations[]'], ['parent_material']),
  op('material_instance', 'material_instance', 'set_parent', 'Set the parent material for a MaterialInstanceConstant.', 'material_instance_set_parent', ['behavior.operations[]'], ['parent_material(*)']),
  op('material_instance', 'material_instance', 'set_scalar_override', 'Set a scalar parameter override on a MaterialInstanceConstant.', 'material_instance_set_scalar_override', ['behavior.operations[]'], ['parameter_name(*)', 'value(*)']),
  op('material_instance', 'material_instance', 'set_vector_override', 'Set a vector parameter override on a MaterialInstanceConstant.', 'material_instance_set_vector_override', ['behavior.operations[]'], ['parameter_name(*)', 'value(*)']),
  op('material_instance', 'material_instance', 'set_texture_override', 'Set a texture parameter override on a MaterialInstanceConstant.', 'material_instance_set_texture_override', ['behavior.operations[]'], ['parameter_name(*)', 'texture_asset(*)']),
  op('material_instance', 'material_instance', 'set_static_switch_override', 'Set a static switch parameter override on a MaterialInstanceConstant.', 'material_instance_set_static_switch_override', ['behavior.operations[]'], ['parameter_name(*)', 'value(*)']),
  op('material_instance', 'material_instance', 'clear_override', 'Clear a parameter override on a MaterialInstanceConstant.', 'material_instance_clear_override', ['behavior.operations[]'], ['parameter_name(*)', 'parameter_type']),
  op('material_instance', 'material_instance', 'read_parameter_schema', 'Read MaterialInstance parameter metadata through the MaterialInstance task route.', 'material_instance_read_parameter_schema', ['behavior.operations[]'], ['parameter_name', 'parameter_type']),
  op('material_instance', 'material_instance', 'read_effective_value', 'Read the effective MaterialInstance parameter value through the MaterialInstance task route.', 'material_instance_read_effective_value', ['behavior.operations[]'], ['parameter_name(*)', 'parameter_type']),
];

function op(
  family: NonGraphWriteFamily,
  clusterId: string,
  operationId: string,
  description: string,
  sourceSlotId: string,
  insertPaths: readonly string[],
  argSlots: readonly string[],
): NonGraphWriteOperationDescriptor {
  return {
    family,
    cluster_id: clusterId,
    operation_id: operationId,
    description,
    template_id: `${family}.${clusterId}.${operationId}`,
    source_slot_id: sourceSlotId,
    template_path: `AgentFaceService/agent-guide/Templates/write/slots/${sourceSlotId}_template.json`,
    insert_paths: [...insertPaths],
    arg_slots: [...argSlots],
    validation_classification: 'shared_policy',
  };
}

function baseOp(
  family: NonGraphWriteFamily,
  clusterId: string,
  operationId: string,
  description: string,
  templatePath: string,
): NonGraphWriteOperationDescriptor {
  return {
    family,
    cluster_id: clusterId,
    operation_id: operationId,
    description,
    template_id: `${family}.${clusterId}.${operationId}`,
    source_slot_id: `${family}_${operationId}`,
    template_path: templatePath,
    insert_paths: [],
    arg_slots: [],
    validation_classification: 'shared_policy',
  };
}
