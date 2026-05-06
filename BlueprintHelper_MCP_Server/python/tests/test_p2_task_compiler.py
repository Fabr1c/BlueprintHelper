import unittest

from blueprinthelper_task.graph_write_append import compile_task_spec


def make_base_spec(task_type, behavior):
    return {
        "schema": "BlueprintHelper.TaskSpec.v1",
        "task_type": task_type,
        "feature_name": "P2Smoke",
        "target": {
            "asset_path": "/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke",
            "target_type": "blueprint",
        },
        "execution_policy": {"dry_run_mode": "full"},
        "validation": {"should_compile": False, "should_save": True},
        "behavior": behavior,
    }


class P2TaskCompilerTests(unittest.TestCase):
    def test_compiles_object_property_task_spec(self):
        result = compile_task_spec(make_base_spec("edit_object_properties", {
            "property_strategy": "property_edit",
            "changes": [
                {"kind": "set_property", "property_path": "DisplayName", "value": {"kind": "literal", "value_type": "string", "value": "Door"}},
                {"kind": "set_property", "property_path": "OpenSpeed", "value": {"kind": "literal", "value_type": "float", "value": 1.5}},
            ],
        }), True)

        steps = result["task_plan"]["steps"]
        self.assertEqual(steps[0]["capability"], "object_property")
        self.assertEqual(steps[0]["write"]["strategy"], "property_edit")
        self.assertEqual(steps[0]["write"]["ops"][0]["op"], "set_object_properties")

    def test_compiles_graph_cleanup_ownership_task_spec(self):
        result = compile_task_spec(make_base_spec("manage_blueprinthelper_ownership", {
            "ownership_strategy": "owned_block_lifecycle",
            "changes": [
                {
                    "kind": "cleanup_block",
                    "graph_id": "DoorLogic",
                    "block_ref": "OpenDoor0",
                    "missing_policy": "ignore",
                },
                {
                    "kind": "convert_block_to_user_owned",
                    "block_id": "DoorLogic_OpenDoor0",
                    "already_user_owned_policy": "ignore",
                },
            ],
        }), True)

        steps = result["task_plan"]["steps"]
        self.assertEqual(len(steps), 2)
        self.assertEqual(steps[0]["capability"], "graph_cleanup_ownership")
        self.assertEqual(steps[0]["write"]["ops"][0]["op"], "cleanup_blueprint_helper_block")
        self.assertEqual(steps[1]["write"]["ops"][0]["op"], "convert_blueprint_helper_block_to_user_owned")


if __name__ == "__main__":
    unittest.main()
