#include "Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.h"

static const TPair<const TCHAR*, EBlueprintHelperBridgeRouteCluster> GBlueprintHelperBridgeRouteCommandClusters[] = {
	{TEXT("get_rule_markdown"), EBlueprintHelperBridgeRouteCluster::Core},
	{TEXT("get_editor_context"), EBlueprintHelperBridgeRouteCluster::Core},
	{TEXT("request_write_session"), EBlueprintHelperBridgeRouteCluster::Core},
	{TEXT("get_runtime_profile"), EBlueprintHelperBridgeRouteCluster::Debug},
	{TEXT("diagnostics_runtime"), EBlueprintHelperBridgeRouteCluster::Debug},
	{TEXT("get_debug_case"), EBlueprintHelperBridgeRouteCluster::Debug},
	{TEXT("list_debug_cases"), EBlueprintHelperBridgeRouteCluster::Debug},
	{TEXT("export_debug_bundle"), EBlueprintHelperBridgeRouteCluster::Debug},
	{TEXT("compile_blueprint"), EBlueprintHelperBridgeRouteCluster::Debug},
	{TEXT("compile_blueprint_asset"), EBlueprintHelperBridgeRouteCluster::Debug},
	{TEXT("read_reference_context"), EBlueprintHelperBridgeRouteCluster::SharedServices},
	{TEXT("read_function_chain_context"), EBlueprintHelperBridgeRouteCluster::SharedServices},
	{TEXT("read_blueprint_logic_md"), EBlueprintHelperBridgeRouteCluster::SharedServices},
	{TEXT("read_blueprint_logic_json"), EBlueprintHelperBridgeRouteCluster::SharedServices},
	{TEXT("validate_json"), EBlueprintHelperBridgeRouteCluster::SharedServices},
	{TEXT("export_to_json"), EBlueprintHelperBridgeRouteCluster::SharedServices},
	{TEXT("export_logic"), EBlueprintHelperBridgeRouteCluster::SharedServices},
	{TEXT("open_asset"), EBlueprintHelperBridgeRouteCluster::AssetBrowser},
	{TEXT("list_assets"), EBlueprintHelperBridgeRouteCluster::AssetBrowser},
	{TEXT("search_assets"), EBlueprintHelperBridgeRouteCluster::AssetBrowser},
	{TEXT("save_asset"), EBlueprintHelperBridgeRouteCluster::AssetBrowser},
	{TEXT("get_asset_info"), EBlueprintHelperBridgeRouteCluster::AssetBrowser},
	{TEXT("list_graphs"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("list_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("list_event_dispatchers"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("add_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("remove_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("add_graph"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("remove_graph"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("add_event_dispatcher"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("delete_nodes"), EBlueprintHelperBridgeRouteCluster::BlueprintStructure},
	{TEXT("read_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("add_blueprint_member_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("add_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("set_blueprint_member_variable_properties"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("remove_blueprint_member_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("remove_blueprint_member_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("read_blueprint_member_defaults"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("set_blueprint_member_default"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("set_blueprint_member_defaults"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("read_blueprint_local_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("add_blueprint_local_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("add_blueprint_local_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("set_blueprint_local_variable_properties"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("remove_blueprint_local_variable"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("remove_blueprint_local_variables"), EBlueprintHelperBridgeRouteCluster::BlueprintVariables},
	{TEXT("get_widget_tree"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
	{TEXT("add_widget"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
	{TEXT("remove_widget"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
	{TEXT("move_widget"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
	{TEXT("get_widget_properties"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
	{TEXT("set_widget_property"), EBlueprintHelperBridgeRouteCluster::UMGWidget},
	{TEXT("get_datatable_rows"), EBlueprintHelperBridgeRouteCluster::DataTable},
	{TEXT("add_datatable_row"), EBlueprintHelperBridgeRouteCluster::DataTable},
	{TEXT("update_datatable_row"), EBlueprintHelperBridgeRouteCluster::DataTable},
	{TEXT("delete_datatable_row"), EBlueprintHelperBridgeRouteCluster::DataTable},
	{TEXT("get_object_properties"), EBlueprintHelperBridgeRouteCluster::ObjectProperty},
	{TEXT("set_object_property"), EBlueprintHelperBridgeRouteCluster::ObjectProperty},
	{TEXT("undo"), EBlueprintHelperBridgeRouteCluster::EditorCommand},
	{TEXT("redo"), EBlueprintHelperBridgeRouteCluster::EditorCommand},
	{TEXT("play_in_editor"), EBlueprintHelperBridgeRouteCluster::EditorCommand},
	{TEXT("stop_pie"), EBlueprintHelperBridgeRouteCluster::EditorCommand},
	{TEXT("create_blueprint"), EBlueprintHelperBridgeRouteCluster::EditorCommand},
	{TEXT("exec_console_command"), EBlueprintHelperBridgeRouteCluster::EditorCommand},
	{TEXT("close_editor"), EBlueprintHelperBridgeRouteCluster::EditorCommand},
	{TEXT("create_asset"), EBlueprintHelperBridgeRouteCluster::AssetFactory},
	{TEXT("read_components"), EBlueprintHelperBridgeRouteCluster::Component},
	{TEXT("add_component"), EBlueprintHelperBridgeRouteCluster::Component},
	{TEXT("set_component_property"), EBlueprintHelperBridgeRouteCluster::Component},
	{TEXT("set_component_properties"), EBlueprintHelperBridgeRouteCluster::Component},
	{TEXT("remove_component"), EBlueprintHelperBridgeRouteCluster::Component},
	{TEXT("read_class_settings"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
	{TEXT("add_implemented_interface"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
	{TEXT("add_implemented_interfaces"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
	{TEXT("remove_implemented_interface"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
	{TEXT("remove_implemented_interfaces"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
	{TEXT("set_class_default_property"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
	{TEXT("set_class_default_properties"), EBlueprintHelperBridgeRouteCluster::ClassSettings},
	{TEXT("preview_task_plan"), EBlueprintHelperBridgeRouteCluster::TaskRuntime},
	{TEXT("execute_task_plan"), EBlueprintHelperBridgeRouteCluster::TaskRuntime},
	{TEXT("get_task_run_journal"), EBlueprintHelperBridgeRouteCluster::TaskRuntime},
	{TEXT("append_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("replace_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("patch_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("merge_blueprint_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("merge_external_flow"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("patch_external_graph"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("replace_external_body"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("project_graphwrite_spawner_evidence"), EBlueprintHelperBridgeRouteCluster::GraphWrite},
	{TEXT("query_review_records"), EBlueprintHelperBridgeRouteCluster::Review},
	{TEXT("apply_review_action"), EBlueprintHelperBridgeRouteCluster::Review},
};

static const TPair<EBlueprintHelperBridgeRouteCluster, const TCHAR*> GBlueprintHelperBridgeRouteClusterNames[] = {
	{EBlueprintHelperBridgeRouteCluster::Core, TEXT("Core")},
	{EBlueprintHelperBridgeRouteCluster::Debug, TEXT("Debug")},
	{EBlueprintHelperBridgeRouteCluster::SharedServices, TEXT("SharedServices")},
	{EBlueprintHelperBridgeRouteCluster::AssetBrowser, TEXT("AssetBrowser")},
	{EBlueprintHelperBridgeRouteCluster::TaskRuntime, TEXT("TaskRuntime")},
	{EBlueprintHelperBridgeRouteCluster::GraphWrite, TEXT("GraphWrite")},
	{EBlueprintHelperBridgeRouteCluster::BlueprintVariables, TEXT("BlueprintVariables")},
	{EBlueprintHelperBridgeRouteCluster::BlueprintStructure, TEXT("BlueprintStructure")},
	{EBlueprintHelperBridgeRouteCluster::AssetFactory, TEXT("AssetFactory")},
	{EBlueprintHelperBridgeRouteCluster::Component, TEXT("Component")},
	{EBlueprintHelperBridgeRouteCluster::ClassSettings, TEXT("ClassSettings")},
	{EBlueprintHelperBridgeRouteCluster::Signature, TEXT("Signature")},
	{EBlueprintHelperBridgeRouteCluster::UMGWidget, TEXT("UMGWidget")},
	{EBlueprintHelperBridgeRouteCluster::DataTable, TEXT("DataTable")},
	{EBlueprintHelperBridgeRouteCluster::ObjectProperty, TEXT("ObjectProperty")},
	{EBlueprintHelperBridgeRouteCluster::EditorCommand, TEXT("EditorCommand")},
	{EBlueprintHelperBridgeRouteCluster::Review, TEXT("Review")},
};

FBlueprintHelperBridgeRoutePlan FBlueprintHelperBridgeRoutePlannerUtils::MakePlan(
	const FString& Command,
	EBlueprintHelperBridgeRouteCluster Cluster)
{
	FBlueprintHelperBridgeRoutePlan Plan;
	Plan.Command = Command;
	Plan.Cluster = Cluster;
	Plan.bKnownCommand = Cluster != EBlueprintHelperBridgeRouteCluster::Unknown;
	Plan.bRequiresGameThread = Plan.bKnownCommand;
	return Plan;
}

EBlueprintHelperBridgeRouteCluster FBlueprintHelperBridgeRoutePlannerUtils::FindClusterForCommand(const FString& Command)
{
	for (const TPair<const TCHAR*, EBlueprintHelperBridgeRouteCluster>& Entry : GBlueprintHelperBridgeRouteCommandClusters)
	{
		if (Command == Entry.Key)
		{
			return Entry.Value;
		}
	}
	return EBlueprintHelperBridgeRouteCluster::Unknown;
}

const TCHAR* FBlueprintHelperBridgeRoutePlannerUtils::ResolveClusterName(EBlueprintHelperBridgeRouteCluster Cluster)
{
	for (const TPair<EBlueprintHelperBridgeRouteCluster, const TCHAR*>& Entry : GBlueprintHelperBridgeRouteClusterNames)
	{
		if (Cluster == Entry.Key)
		{
			return Entry.Value;
		}
	}
	return TEXT("Unknown");
}
