import unittest

from blueprinthelper_task.orchestrator import TaskSpecCompileError, compile_task_spec


def make_base_spec(task_type, behavior, **overrides):
    spec = {
        "schema": "BlueprintHelper.TaskSpec.v1",
        "context_id": f"ctx_{task_type}",
        "task_type": task_type,
        "feature_name": "P1Feature",
        "target": {
            "asset_path": "/Game/Blueprints/BP_Door",
            "target_type": "blueprint",
        },
        "behavior": behavior,
        "execution_policy": {
            "dry_run_mode": "full",
            "on_missing_capability": "stop_and_report",
        },
        "validation": {
            "should_compile": True,
            "should_save": False,
        },
    }
    spec.update(overrides)
    return spec


def make_composite_physics_door_spec(**overrides):
    spec = {
        "schema": "BlueprintHelper.TaskSpec.v1",
        "context_id": "ctx_physics_door",
        "task_type": "create_blueprint_feature",
        "feature_name": "PhysicsDoor",
        "target": {
            "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
            "target_type": "blueprint",
        },
        "scope_policy": {
            "prefer_new_graph": True,
            "graph_name": "EG_PhysicsDoor",
            "allow_modify_user_nodes": False,
            "allow_create_assets": False,
        },
        "asset_policy": {
            "if_target_asset_missing": "fail",
            "if_referenced_asset_missing": "fail",
            "if_component_exists": "reuse_if_type_matches",
        },
        "resources": {
            "static_meshes": {
                "door_mesh": "/Game/BlueprintHelperTest/Meshes/SM_Door",
            },
        },
        "components": [
            {
                "name": "SceneRoot",
                "class": "SceneComponent",
                "set_as_root": True,
            },
            {
                "name": "DoorMesh",
                "class": "StaticMeshComponent",
                "attach_to": "SceneRoot",
                "properties": {
                    "StaticMesh": "$resources.static_meshes.door_mesh",
                    "Mobility": "Movable",
                    "CollisionProfileName": "PhysicsActor",
                    "BodyInstance.bSimulatePhysics": True,
                },
            },
        ],
        "variables": [
            {
                "name": "bDoorOpen",
                "type": "bool",
                "default": False,
                "category": "Door",
            },
            {
                "name": "OpenImpulse",
                "type": "float",
                "default": 50000.0,
                "category": "Door",
            },
        ],
        "class_settings": {
            "implemented_interfaces": [
                "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
            ],
        },
        "behavior": {
            "graph_strategy": "append_new_owned_graph",
            "entries": [
                {
                    "entry_type": "custom_event",
                    "name": "OpenPhysicsDoor",
                    "body": {
                        "schema": "BlueprintLogicSpec.v1",
                        "statements": [
                            {
                                "kind": "set_member_variable",
                                "name": "bDoorOpen",
                                "value": {
                                    "kind": "literal",
                                    "value_type": "bool",
                                    "value": True,
                                },
                            },
                            {
                                "kind": "call_function",
                                "name": "DoorMesh.AddAngularImpulseInDegrees",
                                "args": {
                                    "VelChange": {
                                        "kind": "literal",
                                        "value_type": "bool",
                                        "value": True,
                                    },
                                },
                            },
                        ],
                    },
                },
            ],
        },
        "execution_policy": {
            "dry_run_mode": "full",
            "on_missing_capability": "stop_and_report",
        },
        "validation": {
            "should_compile": True,
            "should_save": False,
        },
    }
    spec.update(overrides)
    return spec


class P1TaskCompilerTests(unittest.TestCase):
    def test_compiles_composite_create_blueprint_feature_to_existing_capability_steps(self):
        result = compile_task_spec(make_composite_physics_door_spec(), dry_run=True)

        task_plan = result["task_plan"]
        self.assertEqual(task_plan["task_type"], "create_blueprint_feature")
        self.assertEqual(task_plan["target_assets"], ["/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"])
        self.assertEqual(
            [step["capability"] for step in task_plan["steps"]],
            [
                "blueprint_component",
                "blueprint_component",
                "blueprint_component",
                "blueprint_variable",
                "blueprint_variable",
                "blueprint_class_settings",
                "blueprint_signature",
                "graph_write",
            ],
        )

        self.assertEqual(task_plan["steps"][0]["write"]["ops"][0]["op"], "add_component")
        self.assertEqual(task_plan["steps"][1]["write"]["ops"][0]["op"], "add_component")
        self.assertEqual(task_plan["steps"][2]["write"]["ops"][0]["op"], "set_component_properties")
        self.assertEqual(task_plan["steps"][2]["depends_on"], ["step_002"])
        self.assertEqual(task_plan["steps"][3]["write"]["strategy"], "member_variables")
        self.assertEqual(len(task_plan["steps"][3]["write"]["ops"]), 2)
        self.assertEqual(task_plan["steps"][4]["write"]["strategy"], "member_defaults")
        self.assertEqual(task_plan["steps"][4]["write"]["ops"][1]["value"], 50000.0)
        self.assertEqual(task_plan["steps"][5]["write"]["ops"][0]["op"], "add_implemented_interfaces")
        self.assertEqual(task_plan["steps"][6]["write"]["ops"][0]["op"], "ensure_custom_event")
        self.assertEqual(task_plan["steps"][7]["target"]["graph"], "EG_PhysicsDoor")
        self.assertEqual(task_plan["steps"][7]["write"]["ops"][0]["op"], "ensure_entry")
        self.assertEqual(task_plan["steps"][7]["depends_on"], ["step_007"])
        self.assertEqual(result["bridge_payload"], {"task_plan": task_plan})

    def test_compiles_composite_interface_integration_to_signature_and_graph_steps(self):
        spec = make_composite_physics_door_spec(
            class_settings=None,
            integration={
                "interface": {
                    "interface_asset": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
                    "function": "Interact",
                    "implementation": {
                        "call": "OpenPhysicsDoor",
                    },
                },
            },
        )

        result = compile_task_spec(spec, dry_run=True)
        task_plan = result["task_plan"]
        steps = task_plan["steps"]

        self.assertEqual(
            [step["capability"] for step in steps],
            [
                "blueprint_component",
                "blueprint_component",
                "blueprint_component",
                "blueprint_variable",
                "blueprint_variable",
                "blueprint_signature",
                "graph_write",
                "blueprint_class_settings",
                "blueprint_signature",
                "graph_write",
            ],
        )
        self.assertEqual(steps[5]["write"]["ops"][0]["op"], "ensure_custom_event")
        self.assertEqual(steps[6]["write"]["ops"][0]["op"], "ensure_entry")
        self.assertEqual(steps[6]["depends_on"], ["step_006"])
        self.assertEqual(steps[7]["write"]["ops"][0], {
            "op": "add_implemented_interfaces",
            "interface_paths": ["/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"],
        })
        self.assertEqual(steps[8]["write"]["ops"][0], {
            "op": "ensure_function",
            "function_name": "Interact",
            "interface_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
            "name_collision_policy": "reuse_if_exists",
        })
        self.assertEqual(steps[8]["depends_on"], ["step_008"])
        self.assertEqual(steps[9]["target"], {
            "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
            "graph": "Interact",
        })
        self.assertEqual(steps[9]["depends_on"], ["step_009"])
        self.assertEqual(steps[9]["write"]["ops"][0]["op"], "replace_body")
        self.assertEqual(steps[9]["write"]["ops"][0]["replace_scope"], "function_body")
        self.assertEqual(steps[9]["write"]["ops"][0]["selector"], {"function_name": "Interact"})
        self.assertEqual(steps[9]["write"]["ops"][0]["replacement"]["nodes"], [
            {
                "id": "interface_Interact_stmt_1",
                "kind": "call",
                "function": "OpenPhysicsDoor",
                "inputs": {},
            },
        ])

    def test_rejects_composite_input_integration_until_input_cluster_is_supported(self):
        spec = make_composite_physics_door_spec(
            integration={
                "input": {
                    "mode": "reference_existing_input_action",
                    "input_action": "/Game/Input/IA_Interact",
                },
            },
        )

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_composite_integration")
        self.assertEqual(ctx.exception.issues[0]["path"], "integration.input")

    def test_rejects_composite_property_settings_that_omit_value(self):
        spec = make_composite_physics_door_spec(
            components=[
                {
                    "name": "DoorMesh",
                    "class": "StaticMeshComponent",
                    "properties": [
                        {
                            "property_path": "Mobility",
                        },
                    ],
                },
            ],
        )

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "taskspec_semantic_invalid")
        self.assertEqual(ctx.exception.issues[0]["path"], "components[0].properties[0].value")

    def test_compiles_asset_factory_create_asset_taskspec(self):
        result = compile_task_spec(make_base_spec(
            "create_asset",
            {
                "asset_strategy": "ensure_asset",
                "asset": {
                    "asset_type": "input_action",
                    "value_type": "bool",
                    "collision_policy": "reuse_if_exists",
                },
            },
            target={
                "asset_path": "/Game/Input/IA_Interact",
                "target_type": "asset",
            },
        ), dry_run=True)

        self.assertEqual(result["task_plan"]["steps"], [
            {
                "step_id": "step_001",
                "capability": "asset_factory",
                "target": {
                    "asset_path": "/Game/Input/IA_Interact",
                },
                "write": {
                    "strategy": "asset_create",
                    "ops": [
                        {
                            "op": "create_asset",
                            "asset_type": "input_action",
                            "value_type": "bool",
                            "collision": "reuse_if_exists",
                        },
                    ],
                },
            },
        ])
        self.assertEqual(result["bridge_payload"], {
            "task_plan": result["task_plan"],
        })

    def test_compiles_actor_asset_alias_to_blueprint_class_parent(self):
        result = compile_task_spec(make_base_spec(
            "create_asset",
            {
                "asset_strategy": "ensure_asset",
                "asset": {
                    "asset_type": "Actor",
                    "collision_policy": "reuse_if_exists",
                },
            },
            target={
                "asset_path": "/Game/BlueprintHelperTest/Smoke/BP_BH_ActorFixture",
                "target_type": "blueprint",
            },
        ), dry_run=True)

        op = result["task_plan"]["steps"][0]["write"]["ops"][0]
        self.assertEqual(op["asset_type"], "blueprint_class")
        self.assertEqual(op["parent_class"], "Actor")
        self.assertEqual(op["collision"], "reuse_if_exists")

    def test_compiles_blueprint_asset_alias_to_blueprint_class_with_parent(self):
        result = compile_task_spec(make_base_spec(
            "create_asset",
            {
                "asset_strategy": "ensure_asset",
                "asset": {
                    "asset_type": "blueprint",
                    "parent_class": "Pawn",
                },
            },
            target={
                "asset_path": "/Game/BlueprintHelperTest/Smoke/BP_BH_PawnFixture",
                "target_type": "blueprint",
            },
        ), dry_run=True)

        op = result["task_plan"]["steps"][0]["write"]["ops"][0]
        self.assertEqual(op["asset_type"], "blueprint_class")
        self.assertEqual(op["parent_class"], "Pawn")

    def test_compiles_structure_asset_fields(self):
        result = compile_task_spec(make_base_spec(
            "create_asset",
            {
                "asset_strategy": "ensure_asset",
                "asset": {
                    "asset_type": "structure",
                    "fields": [
                        {"name": "Damage", "type": "float"},
                        {"name": "Ammo", "type": "int"},
                    ],
                    "collision_policy": "reuse_if_exists",
                },
            },
            target={
                "asset_path": "/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow",
                "target_type": "asset",
            },
        ), dry_run=True)

        op = result["task_plan"]["steps"][0]["write"]["ops"][0]
        self.assertEqual(op["asset_type"], "structure")
        self.assertEqual(op["fields"], [
            {"name": "Damage", "type": "float"},
            {"name": "Ammo", "type": "int"},
        ])
        self.assertEqual(op["collision"], "reuse_if_exists")

    def test_compiles_data_table_asset_with_row_struct(self):
        result = compile_task_spec(make_base_spec(
            "create_asset",
            {
                "asset_strategy": "ensure_asset",
                "asset": {
                    "asset_type": "datatable",
                    "row_struct": "/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow",
                    "collision_policy": "reuse_if_exists",
                },
            },
            target={
                "asset_path": "/Game/BlueprintHelper/Smoke/DT_DataTableSmoke",
                "target_type": "asset",
            },
        ), dry_run=True)

        op = result["task_plan"]["steps"][0]["write"]["ops"][0]
        self.assertEqual(op["asset_type"], "data_table")
        self.assertEqual(op["row_struct"], "/Game/BlueprintHelper/Smoke/ST_DataTableSmokeRow")
        self.assertEqual(op["collision"], "reuse_if_exists")

    def test_compiles_widget_blueprint_alias(self):
        result = compile_task_spec(make_base_spec(
            "create_asset",
            {
                "asset_strategy": "ensure_asset",
                "asset": {
                    "asset_type": "widget",
                    "collision_policy": "reuse_if_exists",
                },
            },
            target={
                "asset_path": "/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke",
                "target_type": "asset",
            },
        ), dry_run=True)

        op = result["task_plan"]["steps"][0]["write"]["ops"][0]
        self.assertEqual(op["asset_type"], "widget_blueprint")
        self.assertEqual(op["collision"], "reuse_if_exists")

    def test_compiles_component_ops_to_one_taskplan_step_per_op(self):
        result = compile_task_spec(make_base_spec(
            "edit_blueprint_components",
            {
                "component_strategy": "component_tree",
                "changes": [
                    {
                        "kind": "ensure_component_present",
                        "name": "DoorMesh",
                        "class": "StaticMeshComponent",
                        "attach": {
                            "parent": "DefaultSceneRoot",
                            "rule": "keep_relative",
                        },
                        "on_name_conflict": "fail_if_exists",
                    },
                    {
                        "kind": "configure_component",
                        "name": "DoorMesh",
                        "properties": [
                            {
                                "property_path": "Mobility",
                                "value": "Movable",
                            },
                        ],
                    },
                ],
            },
        ), dry_run=True)

        steps = result["task_plan"]["steps"]
        self.assertEqual(len(steps), 2)
        self.assertEqual([step["step_id"] for step in steps], ["step_001", "step_002"])
        self.assertEqual([step["capability"] for step in steps], ["blueprint_component", "blueprint_component"])
        self.assertEqual([step["write"]["ops"][0]["op"] for step in steps], ["add_component", "set_component_properties"])
        self.assertEqual(steps[1]["depends_on"], ["step_001"])
        self.assertNotIn("operation", steps[0])
        self.assertNotIn("operation", steps[1])

    def test_compiles_class_settings_ops(self):
        result = compile_task_spec(make_base_spec(
            "edit_blueprint_class_settings",
            {
                "class_settings_strategy": "class_settings",
                "interfaces": {
                    "ensure_present": [
                        "/Game/Interfaces/BPI_Interact",
                    ],
                },
                "class_defaults": [
                    {
                        "property_path": "OpenKickImpulse",
                        "value": 1200.0,
                    },
                ],
            },
        ), dry_run=False)

        steps = result["task_plan"]["steps"]
        self.assertEqual([step["capability"] for step in steps], [
            "blueprint_class_settings",
            "blueprint_class_settings",
        ])
        self.assertEqual([step["write"]["strategy"] for step in steps], [
            "class_settings",
            "class_settings",
        ])
        self.assertEqual([step["write"]["ops"][0]["op"] for step in steps], [
            "add_implemented_interfaces",
            "set_class_default_properties",
        ])

    def test_compiles_umg_widget_ops_with_per_op_strategy(self):
        result = compile_task_spec(make_base_spec(
            "edit_umg_widget",
            {
                "widget_strategy": "widget_blueprint_edit",
                "changes": [
                    {
                        "kind": "create_widget",
                        "parent_widget_name": "CanvasRoot",
                        "widget_class": "TextBlock",
                        "widget_name": "TitleText",
                    },
                    {
                        "kind": "update_widget_property",
                        "widget_name": "TitleText",
                        "property_path": "Text",
                        "value": {
                            "kind": "literal",
                            "value_type": "text",
                            "value": "Start Game",
                        },
                    },
                ],
            },
            target={
                "asset_path": "/Game/UI/WBP_MainMenu",
                "target_type": "widget_blueprint",
            },
        ), dry_run=True)

        steps = result["task_plan"]["steps"]
        self.assertEqual([step["capability"] for step in steps], ["umg_widget", "umg_widget"])
        self.assertEqual([step["write"]["strategy"] for step in steps], [
            "widget_tree_edit",
            "widget_property_edit",
        ])
        self.assertEqual([step["write"]["ops"][0]["op"] for step in steps], [
            "add_widget",
            "set_widget_property",
        ])
        self.assertEqual(steps[1]["write"]["ops"][0]["value"], "Start Game")

    def test_compiles_data_table_row_ops_to_one_taskplan_step_per_op(self):
        result = compile_task_spec(make_base_spec(
            "edit_data_table",
            {
                "row_strategy": "row_edit",
                "rows": [
                    {
                        "action": "add",
                        "row_name": "Pistol",
                        "fields": {
                            "Damage": "12",
                        },
                    },
                    {
                        "action": "delete",
                        "row_name": "OldPistol",
                    },
                ],
            },
            target={
                "asset_path": "/Game/Data/DT_Weapons",
                "target_type": "data_table",
            },
        ), dry_run=True)

        steps = result["task_plan"]["steps"]
        self.assertEqual([step["capability"] for step in steps], ["data_table", "data_table"])
        self.assertEqual([step["write"]["strategy"] for step in steps], ["row_edit", "row_edit"])
        self.assertEqual([step["write"]["ops"][0]["op"] for step in steps], ["add_row", "delete_row"])

    def test_rejects_parent_class_change_in_class_settings_compiler(self):
        spec = make_base_spec(
            "edit_blueprint_class_settings",
            {
                "class_settings_strategy": "class_settings",
                "parent_class": "/Script/Engine.Actor",
            },
        )

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_class_settings_parent_class_op")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.parent_class")

    def test_rejects_taskplan_language_in_asset_factory_taskspec(self):
        spec = make_base_spec(
            "create_asset",
            {
                "asset_strategy": "asset_create",
                "asset": {
                    "asset_type": "input_action",
                },
            },
            target={
                "asset_path": "/Game/Input/IA_Interact",
                "target_type": "asset",
            },
        )

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_asset_factory_strategy")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.asset_strategy")

    def test_rejects_taskplan_language_in_data_table_taskspec(self):
        spec = make_base_spec(
            "edit_data_table",
            {
                "row_strategy": "row_edit",
                "rows": [
                    {
                        "op": "add_row",
                        "row_name": "Pistol",
                    },
                ],
            },
            target={
                "asset_path": "/Game/Data/DT_Weapons",
                "target_type": "data_table",
            },
        )

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_data_table_op_field")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.rows[0].op")


if __name__ == "__main__":
    unittest.main()
