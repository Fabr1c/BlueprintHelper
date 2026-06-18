#include "Entry/Bridge/BlueprintHelperBridgeSystemCommandRegistry.h"

static const FBlueprintHelperBridgeSystemCommandDescriptor GBlueprintHelperBridgeSystemCommandDescriptors[] = {
	{TEXT("get_rule_markdown"), EBlueprintHelperBridgeRouteCluster::Core, EBlueprintHelperBridgeCommandSource::SystemCore, TEXT("system.core.internal"), true, false},
	{TEXT("get_editor_context"), EBlueprintHelperBridgeRouteCluster::Core, EBlueprintHelperBridgeCommandSource::SystemCore, TEXT("system.core.internal"), true, false},
	{TEXT("request_write_session"), EBlueprintHelperBridgeRouteCluster::Core, EBlueprintHelperBridgeCommandSource::SystemCore, TEXT("system.core.internal"), true, false},

	{TEXT("get_runtime_profile"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("diagnostics_runtime"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("get_debug_case"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("list_debug_cases"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("compile_blueprint"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("compile_blueprint_asset"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("focus_blueprint_editor_target"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("capture_editor_screenshot"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},
	{TEXT("capture_focused_graph_screenshot"), EBlueprintHelperBridgeRouteCluster::Debug, EBlueprintHelperBridgeCommandSource::SystemDebug, TEXT("system.debug.internal"), true, false},

	{TEXT("read_reference_context"), EBlueprintHelperBridgeRouteCluster::SharedServices, EBlueprintHelperBridgeCommandSource::SystemReadHelper, TEXT("system.read_helper.internal"), true, false},
	{TEXT("read_function_chain_context"), EBlueprintHelperBridgeRouteCluster::SharedServices, EBlueprintHelperBridgeCommandSource::SystemReadHelper, TEXT("system.read_helper.internal"), true, false},
	{TEXT("validate_json"), EBlueprintHelperBridgeRouteCluster::SharedServices, EBlueprintHelperBridgeCommandSource::SystemReadHelper, TEXT("system.read_helper.internal"), true, false},
	{TEXT("export_to_json"), EBlueprintHelperBridgeRouteCluster::SharedServices, EBlueprintHelperBridgeCommandSource::SystemReadHelper, TEXT("system.read_helper.internal"), true, false},
	{TEXT("export_logic"), EBlueprintHelperBridgeRouteCluster::SharedServices, EBlueprintHelperBridgeCommandSource::SystemReadHelper, TEXT("system.read_helper.internal"), true, false},

	{TEXT("open_asset"), EBlueprintHelperBridgeRouteCluster::AssetBrowser, EBlueprintHelperBridgeCommandSource::SystemAssetBrowser, TEXT("system.asset_browser.internal"), true, false},
	{TEXT("save_asset"), EBlueprintHelperBridgeRouteCluster::AssetBrowser, EBlueprintHelperBridgeCommandSource::SystemAssetBrowser, TEXT("system.asset_browser.internal"), true, false},
	{TEXT("find_assets"), EBlueprintHelperBridgeRouteCluster::AssetDiscovery, EBlueprintHelperBridgeCommandSource::SystemAssetDiscovery, TEXT("system.asset_discovery.internal"), true, false},

	{TEXT("list_graphs"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_graph"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_graph"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_event_dispatcher"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("delete_nodes"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},

	{TEXT("read_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_blueprint_member_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_blueprint_member_variable_properties"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_blueprint_member_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("read_blueprint_member_defaults"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_blueprint_member_default"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_blueprint_member_defaults"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("read_blueprint_local_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_blueprint_local_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_blueprint_local_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_blueprint_local_variable_properties"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_blueprint_local_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_blueprint_local_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},

	{TEXT("add_datatable_row"), EBlueprintHelperBridgeRouteCluster::DataTable, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("update_datatable_row"), EBlueprintHelperBridgeRouteCluster::DataTable, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("delete_datatable_row"), EBlueprintHelperBridgeRouteCluster::DataTable, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_object_property"), EBlueprintHelperBridgeRouteCluster::ObjectProperty, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},

	{TEXT("undo"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("redo"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("play_in_editor"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("stop_pie"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("create_blueprint"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("exec_console_command"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("source_control_status"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("source_control_checkout"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("dismiss_editor_dialogs"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},
	{TEXT("close_editor"), EBlueprintHelperBridgeRouteCluster::EditorCommand, EBlueprintHelperBridgeCommandSource::SystemEditorCommand, TEXT("system.editor_command.internal"), true, false},

	{TEXT("create_asset"), EBlueprintHelperBridgeRouteCluster::AssetFactory, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_component"), EBlueprintHelperBridgeRouteCluster::Component, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_component_property"), EBlueprintHelperBridgeRouteCluster::Component, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_component_properties"), EBlueprintHelperBridgeRouteCluster::Component, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_component"), EBlueprintHelperBridgeRouteCluster::Component, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("read_class_settings"), EBlueprintHelperBridgeRouteCluster::ClassSettings, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_implemented_interface"), EBlueprintHelperBridgeRouteCluster::ClassSettings, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("add_implemented_interfaces"), EBlueprintHelperBridgeRouteCluster::ClassSettings, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_implemented_interface"), EBlueprintHelperBridgeRouteCluster::ClassSettings, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("remove_implemented_interfaces"), EBlueprintHelperBridgeRouteCluster::ClassSettings, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_class_default_property"), EBlueprintHelperBridgeRouteCluster::ClassSettings, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},
	{TEXT("set_class_default_properties"), EBlueprintHelperBridgeRouteCluster::ClassSettings, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.internal_direct_route"), true, false},

	{TEXT("preview_task_plan"), EBlueprintHelperBridgeRouteCluster::TaskRuntime, EBlueprintHelperBridgeCommandSource::SystemTaskRuntime, TEXT("system.task_runtime.preview"), true, false},
	{TEXT("get_task_run_journal"), EBlueprintHelperBridgeRouteCluster::TaskRuntime, EBlueprintHelperBridgeCommandSource::SystemTaskRuntime, TEXT("system.task_runtime.journal"), true, false},

	{TEXT("append_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("replace_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("patch_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("merge_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("merge_external_flow"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("patch_external_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("patch_external_links"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("replace_external_body"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},
	{TEXT("project_graphwrite_spawner_evidence"), EBlueprintHelperBridgeRouteCluster::GraphWrite, EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute, TEXT("system.graphwrite_direct_route"), true, false},

	{TEXT("query_review_records"), EBlueprintHelperBridgeRouteCluster::Review, EBlueprintHelperBridgeCommandSource::SystemReview, TEXT("system.review.internal"), true, false},
	{TEXT("apply_review_action"), EBlueprintHelperBridgeRouteCluster::Review, EBlueprintHelperBridgeCommandSource::SystemReview, TEXT("system.review.internal"), true, false},
};

TArray<FBlueprintHelperBridgeSystemCommandDescriptor>
FBlueprintHelperBridgeSystemCommandRegistry::ListDescriptors()
{
	TArray<FBlueprintHelperBridgeSystemCommandDescriptor> Descriptors;
	Descriptors.Append(
		GBlueprintHelperBridgeSystemCommandDescriptors,
		UE_ARRAY_COUNT(GBlueprintHelperBridgeSystemCommandDescriptors));
	return Descriptors;
}

bool FBlueprintHelperBridgeSystemCommandRegistry::TryFindDescriptor(
	const FString& Command,
	FBlueprintHelperBridgeSystemCommandDescriptor& OutDescriptor)
{
	for (const FBlueprintHelperBridgeSystemCommandDescriptor& Descriptor :
		GBlueprintHelperBridgeSystemCommandDescriptors)
	{
		if (Command.Equals(Descriptor.Command, ESearchCase::IgnoreCase))
		{
			OutDescriptor = Descriptor;
			return true;
		}
	}

	OutDescriptor = FBlueprintHelperBridgeSystemCommandDescriptor();
	return false;
}

const TCHAR* FBlueprintHelperBridgeSystemCommandRegistry::GetSourceName(
	EBlueprintHelperBridgeCommandSource Source)
{
	switch (Source)
	{
	case EBlueprintHelperBridgeCommandSource::SystemCore:
		return TEXT("system.core");
	case EBlueprintHelperBridgeCommandSource::SystemDebug:
		return TEXT("system.debug");
	case EBlueprintHelperBridgeCommandSource::SystemReadHelper:
		return TEXT("system.read_helper");
	case EBlueprintHelperBridgeCommandSource::SystemAssetBrowser:
		return TEXT("system.asset_browser");
	case EBlueprintHelperBridgeCommandSource::SystemAssetDiscovery:
		return TEXT("system.asset_discovery");
	case EBlueprintHelperBridgeCommandSource::SystemEditorCommand:
		return TEXT("system.editor_command");
	case EBlueprintHelperBridgeCommandSource::SystemTaskRuntime:
		return TEXT("system.task_runtime");
	case EBlueprintHelperBridgeCommandSource::SystemReview:
		return TEXT("system.review");
	case EBlueprintHelperBridgeCommandSource::SystemInternalDirectRoute:
		return TEXT("system.internal_direct_route");
	case EBlueprintHelperBridgeCommandSource::SystemInternal:
	default:
		return TEXT("system.internal");
	}
}
