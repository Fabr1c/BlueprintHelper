import type { WriteValidationClassification } from './taskspec-template-types.js';

export type UmgWidgetOperationStatus = 'active';

export type UmgWidgetTaskPlanStrategy =
  | 'widget_tree_edit'
  | 'widget_property_edit'
  | 'widget_blueprint_class_edit';

export type UmgWidgetPlannedPreviewEffect =
  | 'widget_tree_structural'
  | 'widget_property'
  | 'widget_metadata'
  | 'widget_blueprint_class';

export interface UmgWidgetOperationDescriptor {
  readonly kind: string;
  readonly taskplan_op: string;
  readonly bridge_command: string;
  readonly taskplan_strategy: UmgWidgetTaskPlanStrategy;
  readonly required_fields: readonly string[];
  readonly optional_fields: readonly string[];
  readonly string_enum_fields?: Readonly<Record<string, readonly string[]>>;
  readonly review_target_subkind: string;
  readonly readback_view: 'tree_json' | 'property_json';
  readonly planned_preview_effect: UmgWidgetPlannedPreviewEffect;
  readonly validation_classification: WriteValidationClassification;
  readonly runtime_only_validation_notes?: readonly string[];
  readonly status: UmgWidgetOperationStatus;
}

export const UMG_WIDGET_OPERATION_DESCRIPTORS = [
  {
    kind: 'create_widget',
    taskplan_op: 'add_widget',
    bridge_command: 'add_widget',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['widget_name', 'widget_class'],
    optional_fields: ['parent_name', 'slot_name', 'virtual_index', 'expected_parent_name', 'dry_run'],
    review_target_subkind: 'widget_tree_add',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'update_widget_property',
    taskplan_op: 'set_widget_property',
    bridge_command: 'set_widget_property',
    taskplan_strategy: 'widget_property_edit',
    required_fields: ['widget_name', 'value'],
    optional_fields: ['property_path', 'property_name', 'dry_run'],
    review_target_subkind: 'widget_property',
    readback_view: 'property_json',
    planned_preview_effect: 'widget_property',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'delete_widget',
    taskplan_op: 'remove_widget',
    bridge_command: 'remove_widget',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['widget_name'],
    optional_fields: ['dry_run'],
    review_target_subkind: 'widget_tree_remove',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'move_widget',
    taskplan_op: 'move_widget',
    bridge_command: 'move_widget',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['widget_name', 'new_parent_name'],
    optional_fields: ['slot_name', 'virtual_index', 'expected_parent_name', 'expected_virtual_index', 'dry_run'],
    review_target_subkind: 'widget_tree_move',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'set_named_slot_content',
    taskplan_op: 'set_named_slot_content',
    bridge_command: 'set_named_slot_content',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['host_widget_name', 'slot_name', 'widget_name', 'widget_class'],
    optional_fields: ['virtual_index', 'replace_existing', 'expected_content_widget_name', 'dry_run'],
    review_target_subkind: 'named_slot_content',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'set_slot_property',
    taskplan_op: 'set_slot_property',
    bridge_command: 'set_slot_property',
    taskplan_strategy: 'widget_property_edit',
    required_fields: ['widget_name', 'property_path', 'value'],
    optional_fields: ['expected_slot_class_path', 'dry_run'],
    review_target_subkind: 'slot_property',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_property',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'set_widget_as_variable',
    taskplan_op: 'set_widget_as_variable',
    bridge_command: 'set_widget_as_variable',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['widget_name', 'is_variable'],
    optional_fields: ['expected_widget_class_path', 'dry_run'],
    review_target_subkind: 'widget_variable',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_metadata',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'rename_widget',
    taskplan_op: 'rename_widget',
    bridge_command: 'rename_widget',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['widget_name', 'new_widget_name'],
    optional_fields: ['expected_widget_class_path', 'dry_run'],
    review_target_subkind: 'widget_rename',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'remove_root_widget',
    taskplan_op: 'remove_root_widget',
    bridge_command: 'remove_root_widget',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['root_widget_name', 'replacement_policy'],
    optional_fields: ['replacement_widget_class', 'replacement_widget_name', 'expected_root_class_path', 'dry_run'],
    string_enum_fields: {
      replacement_policy: ['promote_single_child', 'replace_with_empty_root', 'remove_empty_root'],
    },
    review_target_subkind: 'root_widget_removal',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'reparent_widget_blueprint',
    taskplan_op: 'reparent_widget_blueprint',
    bridge_command: 'reparent_widget_blueprint',
    taskplan_strategy: 'widget_blueprint_class_edit',
    required_fields: ['new_parent_class'],
    optional_fields: ['expected_parent_class', 'dry_run'],
    review_target_subkind: 'widget_blueprint_reparent',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_blueprint_class',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'duplicate_widget_subtree',
    taskplan_op: 'duplicate_widget_subtree',
    bridge_command: 'duplicate_widget_subtree',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['source_widget_name', 'target_parent_name', 'name_mapping'],
    optional_fields: ['slot_name', 'virtual_index', 'dry_run'],
    review_target_subkind: 'widget_subtree_duplicate',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'wrap_widget',
    taskplan_op: 'wrap_widget',
    bridge_command: 'wrap_widget',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['widget_name', 'wrapper_class', 'wrapper_name'],
    optional_fields: ['dry_run'],
    review_target_subkind: 'widget_wrap',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
  {
    kind: 'replace_widget_class',
    taskplan_op: 'replace_widget_class',
    bridge_command: 'replace_widget_class',
    taskplan_strategy: 'widget_tree_edit',
    required_fields: ['widget_name', 'new_widget_class'],
    optional_fields: ['preserve_children', 'preserve_slot', 'expected_widget_class_path', 'dry_run'],
    review_target_subkind: 'widget_class_replace',
    readback_view: 'tree_json',
    planned_preview_effect: 'widget_tree_structural',
    validation_classification: 'shared_policy',
    status: 'active',
  },
] as const satisfies readonly UmgWidgetOperationDescriptor[];

export function getUmgWidgetOperationDescriptor(kind: string): UmgWidgetOperationDescriptor | undefined {
  return UMG_WIDGET_OPERATION_DESCRIPTORS.find((descriptor) => descriptor.kind === kind);
}

export function listActiveUmgWidgetOperationDescriptors(): readonly UmgWidgetOperationDescriptor[] {
  return UMG_WIDGET_OPERATION_DESCRIPTORS.filter((descriptor) => descriptor.status === 'active');
}
