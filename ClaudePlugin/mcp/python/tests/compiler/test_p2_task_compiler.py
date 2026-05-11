import unittest

from blueprinthelper_task.shared.errors import TaskSpecCompileError
from blueprinthelper_task.compiler.graph_write_append import compile_task_spec


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

    def test_compiles_interface_function_and_interface_event_signatures(self):
        result = compile_task_spec(make_base_spec("edit_blueprint_signature", {
            "signature_strategy": "signature_edit",
            "changes": [
                {
                    "kind": "ensure_interface_function",
                    "interface_path": "/Game/Interfaces/BPI_Door",
                    "function_name": "CanInteract",
                    "inputs": [
                        {"name": "Instigator", "pin_type": {"category": "object", "subcategory_object": "/Script/Engine.Actor"}},
                    ],
                    "outputs": [
                        {"name": "bCanInteract", "pin_type": {"category": "bool"}},
                    ],
                },
                {
                    "kind": "ensure_interface_event",
                    "interface_path": "/Game/Interfaces/BPI_Door",
                    "event_name": "OnInteract",
                    "graph_name": "EventGraph",
                    "inputs": [
                        {"name": "Instigator", "pin_type": {"category": "object", "subcategory_object": "/Script/Engine.Actor"}},
                    ],
                },
            ],
        }), True)

        steps = result["task_plan"]["steps"]
        self.assertEqual(steps[0]["capability"], "blueprint_signature")
        self.assertEqual(steps[0]["write"]["strategy"], "function_signature")
        self.assertEqual(steps[0]["write"]["ops"][0], {
            "op": "ensure_function",
            "function_name": "CanInteract",
            "interface_path": "/Game/Interfaces/BPI_Door",
            "interface_entry_kind": "function",
            "inputs": [
                {"name": "Instigator", "pin_type": {"category": "object", "subcategory_object": "/Script/Engine.Actor"}},
            ],
            "outputs": [
                {"name": "bCanInteract", "pin_type": {"category": "bool"}},
            ],
            "name_collision_policy": "reuse_if_exists",
        })
        self.assertEqual(steps[1]["write"]["strategy"], "custom_event_signature")
        self.assertEqual(steps[1]["write"]["ops"][0], {
            "op": "ensure_custom_event",
            "event_name": "OnInteract",
            "graph_name": "EventGraph",
            "interface_path": "/Game/Interfaces/BPI_Door",
            "interface_entry_kind": "event",
            "inputs": [
                {"name": "Instigator", "pin_type": {"category": "object", "subcategory_object": "/Script/Engine.Actor"}},
            ],
            "name_collision_policy": "reuse_if_exists",
        })

    def test_compiles_dispatcher_override_and_remove_signature_policies(self):
        result = compile_task_spec(make_base_spec("edit_blueprint_signature", {
            "signature_strategy": "signature_edit",
            "changes": [
                {
                    "kind": "ensure_event_dispatcher",
                    "dispatcher_name": "OnDoorOpened",
                    "inputs": [
                        {"name": "bIsOpen", "pin_type": {"category": "bool"}},
                    ],
                    "signature_mismatch_policy": "block",
                },
                {
                    "kind": "ensure_override_event",
                    "event_name": "ReceiveBeginPlay",
                    "event_kind": "native_event",
                    "execute_policy": "blocked_preflight",
                },
                {
                    "kind": "remove_signature",
                    "signature_kind": "event_dispatcher",
                    "signature_name": "OnDeprecatedDoorOpened",
                    "execute_policy": "blocked_preflight",
                    "require_reference_context": True,
                },
            ],
        }), True)

        steps = result["task_plan"]["steps"]
        self.assertEqual(steps[0]["write"]["strategy"], "event_dispatcher_signature")
        self.assertEqual(steps[0]["write"]["ops"][0], {
            "op": "ensure_event_dispatcher",
            "dispatcher_name": "OnDoorOpened",
            "inputs": [
                {"name": "bIsOpen", "pin_type": {"category": "bool"}},
            ],
            "name_collision_policy": "reuse_if_exists",
            "signature_mismatch_policy": "block",
        })
        self.assertEqual(steps[1]["write"]["strategy"], "override_event_signature")
        self.assertEqual(steps[1]["write"]["ops"][0], {
            "op": "ensure_override_event",
            "event_name": "ReceiveBeginPlay",
            "event_kind": "native_event",
            "execute_policy": "blocked_preflight",
        })
        self.assertEqual(steps[2]["write"]["strategy"], "event_dispatcher_signature")
        self.assertEqual(steps[2]["write"]["ops"][0], {
            "op": "remove_signature",
            "signature_kind": "event_dispatcher",
            "signature_name": "OnDeprecatedDoorOpened",
            "execute_policy": "blocked_preflight",
            "require_reference_context": True,
        })

    def test_rejects_remove_signature_without_reference_context(self):
        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(make_base_spec("edit_blueprint_signature", {
                "signature_strategy": "signature_edit",
                "changes": [
                    {
                        "kind": "remove_signature",
                        "signature_kind": "event_dispatcher",
                        "signature_name": "OnDeprecatedDoorOpened",
                        "execute_policy": "blocked_preflight",
                        "require_reference_context": False,
                    },
                ],
            }), True)

        self.assertEqual(ctx.exception.code, "invalid_signature_remove_policy")

    def test_compiles_custom_event_definition_into_signature_then_graph_body_steps(self):
        spec = make_base_spec("edit_blueprint_graph", {
            "graph_strategy": "replace_owned_graph",
            "replace": {
                "scope": "custom_event_definition",
                "selector": {
                    "kind": "custom_event",
                    "name": "OnInteract",
                },
                "inputs": [
                    {"name": "Instigator", "pin_type": {"category": "object", "subcategory_object": "/Script/Engine.Actor"}},
                ],
                "body": {
                    "schema": "BlueprintLogicSpec.v1",
                    "statements": [{
                        "kind": "call_function",
                        "name": "PrintString",
                        "args": {
                            "InString": {
                                "kind": "literal",
                                "value_type": "string",
                                "value": "interact",
                            },
                        },
                    }],
                },
            },
        })
        spec["scope_policy"] = {
            "graph_name": "EventGraph",
            "allow_modify_user_nodes": False,
        }

        result = compile_task_spec(spec, True)
        steps = result["task_plan"]["steps"]

        self.assertEqual(len(steps), 2)
        self.assertEqual(steps[0]["capability"], "blueprint_signature")
        self.assertEqual(steps[0]["write"]["strategy"], "custom_event_signature")
        self.assertEqual(steps[0]["write"]["ops"][0], {
            "op": "ensure_custom_event",
            "event_name": "OnInteract",
            "graph_name": "EventGraph",
            "inputs": [
                {"name": "Instigator", "pin_type": {"category": "object", "subcategory_object": "/Script/Engine.Actor"}},
            ],
            "name_collision_policy": "reuse_if_exists",
        })
        self.assertEqual(steps[1]["capability"], "graph_write")
        self.assertEqual(steps[1]["depends_on"], ["step_001"])
        self.assertEqual(steps[1]["write"]["ops"][0]["op"], "replace_body")
        self.assertEqual(steps[1]["write"]["ops"][0]["replace_scope"], "custom_event_body")
        self.assertEqual(steps[1]["write"]["ops"][0]["selector"]["entry_name"], "OnInteract")

    def test_compiles_override_event_create_if_missing_policy(self):
        result = compile_task_spec(make_base_spec("edit_blueprint_signature", {
            "signature_strategy": "signature_edit",
            "changes": [
                {
                    "kind": "ensure_override_event",
                    "event_name": "ReceiveBeginPlay",
                    "event_kind": "native_event",
                    "graph_name": "EventGraph",
                    "execute_policy": "create_if_missing",
                },
            ],
        }), True)

        step = result["task_plan"]["steps"][0]
        self.assertEqual(step["write"]["strategy"], "override_event_signature")
        self.assertEqual(step["write"]["ops"][0], {
            "op": "ensure_override_event",
            "event_name": "ReceiveBeginPlay",
            "event_kind": "native_event",
            "graph_name": "EventGraph",
            "execute_policy": "create_if_missing",
        })


if __name__ == "__main__":
    unittest.main()
