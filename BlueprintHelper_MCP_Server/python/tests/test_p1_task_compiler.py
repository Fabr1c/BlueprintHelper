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


class P1TaskCompilerTests(unittest.TestCase):
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
