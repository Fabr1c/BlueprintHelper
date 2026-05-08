#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"

namespace
{
FBlueprintHelperBridgeRoutePlan MakePlan(
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
}

FBlueprintHelperBridgeRoutePlan FBlueprintHelperBridgeRoutePlanner::BuildPlan(const FString& Command)
{
	if (Command == TEXT("get_rule_markdown") ||
		Command == TEXT("get_editor_context"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::Core);
	}

	if (Command == TEXT("get_runtime_profile") ||
		Command == TEXT("diagnostics_runtime") ||
		Command == TEXT("compile_blueprint") ||
		Command == TEXT("compile_blueprint_asset"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::Debug);
	}

	if (Command == TEXT("read_reference_context") ||
		Command == TEXT("read_blueprint_logic_md") ||
		Command == TEXT("read_blueprint_logic_json") ||
		Command == TEXT("validate_json") ||
		Command == TEXT("export_to_json") ||
		Command == TEXT("export_logic") ||
		Command == TEXT("import_json") ||
		Command == TEXT("import_agent_graph"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::SharedServices);
	}

	if (Command == TEXT("open_asset") ||
		Command == TEXT("list_assets") ||
		Command == TEXT("search_assets") ||
		Command == TEXT("save_asset") ||
		Command == TEXT("get_asset_info"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::AssetBrowser);
	}

	if (Command == TEXT("list_graphs") ||
		Command == TEXT("list_variables") ||
		Command == TEXT("list_event_dispatchers") ||
		Command == TEXT("add_variable") ||
		Command == TEXT("remove_variable") ||
		Command == TEXT("add_graph") ||
		Command == TEXT("remove_graph") ||
		Command == TEXT("add_event_dispatcher") ||
		Command == TEXT("delete_nodes"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::BlueprintStructure);
	}

	if (Command == TEXT("read_blueprint_member_variables") ||
		Command == TEXT("add_blueprint_member_variable") ||
		Command == TEXT("add_blueprint_member_variables") ||
		Command == TEXT("set_blueprint_member_variable_properties") ||
		Command == TEXT("remove_blueprint_member_variable") ||
		Command == TEXT("remove_blueprint_member_variables") ||
		Command == TEXT("read_blueprint_member_defaults") ||
		Command == TEXT("set_blueprint_member_default") ||
		Command == TEXT("set_blueprint_member_defaults") ||
		Command == TEXT("read_blueprint_local_variables") ||
		Command == TEXT("add_blueprint_local_variable") ||
		Command == TEXT("add_blueprint_local_variables") ||
		Command == TEXT("set_blueprint_local_variable_properties") ||
		Command == TEXT("remove_blueprint_local_variable") ||
		Command == TEXT("remove_blueprint_local_variables"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::BlueprintVariables);
	}

	if (Command == TEXT("get_widget_tree") ||
		Command == TEXT("add_widget") ||
		Command == TEXT("remove_widget") ||
		Command == TEXT("move_widget") ||
		Command == TEXT("get_widget_properties") ||
		Command == TEXT("set_widget_property"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::UMGWidget);
	}

	if (Command == TEXT("get_datatable_rows") ||
		Command == TEXT("add_datatable_row") ||
		Command == TEXT("update_datatable_row") ||
		Command == TEXT("delete_datatable_row"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::DataTable);
	}

	if (Command == TEXT("get_object_properties") ||
		Command == TEXT("set_object_property"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::ObjectProperty);
	}

	if (Command == TEXT("undo") ||
		Command == TEXT("redo") ||
		Command == TEXT("play_in_editor") ||
		Command == TEXT("stop_pie") ||
		Command == TEXT("create_blueprint") ||
		Command == TEXT("exec_console_command") ||
		Command == TEXT("close_editor"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::EditorCommand);
	}

	if (Command == TEXT("create_asset"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::AssetFactory);
	}

	if (Command == TEXT("read_components") ||
		Command == TEXT("add_component") ||
		Command == TEXT("set_component_property") ||
		Command == TEXT("set_component_properties") ||
		Command == TEXT("remove_component"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::Component);
	}

	if (Command == TEXT("read_class_settings") ||
		Command == TEXT("add_implemented_interface") ||
		Command == TEXT("add_implemented_interfaces") ||
		Command == TEXT("remove_implemented_interface") ||
		Command == TEXT("remove_implemented_interfaces") ||
		Command == TEXT("set_class_default_property") ||
		Command == TEXT("set_class_default_properties"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::ClassSettings);
	}

	if (Command == TEXT("preview_task_plan") ||
		Command == TEXT("execute_task_plan") ||
		Command == TEXT("get_task_run_journal"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::TaskRuntime);
	}

	if (Command == TEXT("append_blueprint_graph") ||
		Command == TEXT("replace_blueprint_graph") ||
		Command == TEXT("patch_blueprint_graph") ||
		Command == TEXT("merge_blueprint_graph"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::GraphWrite);
	}

	if (Command == TEXT("cleanup_blueprint_helper_block") ||
		Command == TEXT("rollback_cleanup_transaction") ||
		Command == TEXT("convert_blueprint_helper_block_to_user_owned"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::CleanupOwnership);
	}

	if (Command == TEXT("list_blueprint_helper_transactions") ||
		Command == TEXT("read_blueprint_helper_transaction"))
	{
		return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::Transactions);
	}

	return MakePlan(Command, EBlueprintHelperBridgeRouteCluster::Unknown);
}

const TCHAR* FBlueprintHelperBridgeRoutePlanner::GetClusterName(EBlueprintHelperBridgeRouteCluster Cluster)
{
	switch (Cluster)
	{
	case EBlueprintHelperBridgeRouteCluster::Core:
		return TEXT("Core");
	case EBlueprintHelperBridgeRouteCluster::Debug:
		return TEXT("Debug");
	case EBlueprintHelperBridgeRouteCluster::SharedServices:
		return TEXT("SharedServices");
	case EBlueprintHelperBridgeRouteCluster::AssetBrowser:
		return TEXT("AssetBrowser");
	case EBlueprintHelperBridgeRouteCluster::TaskRuntime:
		return TEXT("TaskRuntime");
	case EBlueprintHelperBridgeRouteCluster::GraphWrite:
		return TEXT("GraphWrite");
	case EBlueprintHelperBridgeRouteCluster::BlueprintVariables:
		return TEXT("BlueprintVariables");
	case EBlueprintHelperBridgeRouteCluster::BlueprintStructure:
		return TEXT("BlueprintStructure");
	case EBlueprintHelperBridgeRouteCluster::AssetFactory:
		return TEXT("AssetFactory");
	case EBlueprintHelperBridgeRouteCluster::Component:
		return TEXT("Component");
	case EBlueprintHelperBridgeRouteCluster::ClassSettings:
		return TEXT("ClassSettings");
	case EBlueprintHelperBridgeRouteCluster::Signature:
		return TEXT("Signature");
	case EBlueprintHelperBridgeRouteCluster::UMGWidget:
		return TEXT("UMGWidget");
	case EBlueprintHelperBridgeRouteCluster::DataTable:
		return TEXT("DataTable");
	case EBlueprintHelperBridgeRouteCluster::ObjectProperty:
		return TEXT("ObjectProperty");
	case EBlueprintHelperBridgeRouteCluster::EditorCommand:
		return TEXT("EditorCommand");
	case EBlueprintHelperBridgeRouteCluster::CleanupOwnership:
		return TEXT("CleanupOwnership");
	case EBlueprintHelperBridgeRouteCluster::Transactions:
		return TEXT("Transactions");
	case EBlueprintHelperBridgeRouteCluster::AnimationBlueprint:
		return TEXT("AnimationBlueprint");
	case EBlueprintHelperBridgeRouteCluster::Material:
		return TEXT("Material");
	default:
		return TEXT("Unknown");
	}
}
