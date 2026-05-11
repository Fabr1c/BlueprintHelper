import unittest
import importlib.util

from blueprinthelper_task.runtime.orchestrator import (
    TaskSpecCompileError,
    compile_task_spec,
    compile_graph_write_append,
    validate_graph_write_task_plan,
)


def make_task_spec(**overrides):
    spec = {
        "schema": "BlueprintHelper.TaskSpec.v1",
        "context_id": "ctx_test",
        "task_type": "edit_blueprint_graph",
        "feature_name": "DoorFeature",
        "target": {
            "asset_path": "/Game/BP/BP_Door",
            "target_type": "blueprint",
        },
        "scope_policy": {
            "graph_name": "EG_DoorFeature",
            "allow_modify_user_nodes": False,
        },
        "behavior": {
            "graph_strategy": "append_new_owned_graph",
            "entries": [
                {
                    "entry_type": "custom_event",
                    "name": "ToggleDoor",
                    "body": {
                        "schema": "BlueprintLogicSpec.v1",
                        "statements": [
                            {
                                "kind": "call_function",
                                "name": "PrintString",
                                "args": {
                                    "InString": {
                                        "kind": "literal",
                                        "value_type": "string",
                                        "value": "hello",
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
            "should_compile": False,
            "should_save": False,
        },
    }
    spec.update(overrides)
    return spec


def make_variable_task_spec(**overrides):
    spec = {
        "schema": "BlueprintHelper.TaskSpec.v1",
        "context_id": "ctx_variables",
        "task_type": "edit_blueprint_variables",
        "feature_name": "DoorVariables",
        "target": {
            "asset_path": "/Game/BP/BP_Door",
            "target_type": "blueprint",
        },
        "behavior": {
            "variable_strategy": "member_variables",
            "variables": [
                {
                    "op": "ensure_member_variable",
                    "name": "bDoorOpen",
                    "pin_type": {
                        "category": "bool",
                    },
                    "category": "Door",
                    "flags": {
                        "expose_on_spawn": False,
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


class GraphWriteAppendCompilerTests(unittest.TestCase):
    def test_accepts_replace_taskplan_shape_from_graph_write_contract(self):
        task_plan = {
            "schema": "BlueprintHelper.TaskPlan.v1",
            "task_name": "DoorBodyRewrite",
            "task_type": "edit_blueprint_graph",
            "context_id": "ctx_replace",
            "target_assets": ["/Game/BP/BP_Door"],
            "execution_policy": {
                "dry_run_mode": "full",
                "should_compile": True,
                "should_save": False,
            },
            "steps": [
                {
                    "step_id": "step_001",
                    "operation": "replace_blueprint_graph",
                    "target": {
                        "asset_path": "/Game/BP/BP_Door",
                        "graph": "EventGraph",
                        "replace_scope": "custom_event_body",
                    },
                    "args": {
                        "selector": {
                            "entry_name": "ToggleDoor",
                            "node_path": "logic.groups[0].entry.node_path",
                        },
                        "replacement": {
                            "nodes": [],
                            "links": [],
                        },
                        "options": {
                            "strict": True,
                            "preserve_layout": False,
                        },
                    },
                },
            ],
        }

        validate_graph_write_task_plan(task_plan)

    def test_accepts_patch_taskplan_shape_from_graph_write_contract(self):
        task_plan = {
            "schema": "BlueprintHelper.TaskPlan.v1",
            "task_name": "DoorConditionPatch",
            "task_type": "edit_blueprint_graph",
            "context_id": "ctx_patch",
            "target_assets": ["/Game/BP/BP_Door"],
            "execution_policy": {
                "dry_run_mode": "full",
                "should_compile": True,
                "should_save": True,
            },
            "steps": [
                {
                    "step_id": "step_001",
                    "operation": "patch_blueprint_graph",
                    "target": {
                        "asset_path": "/Game/BP/BP_Door",
                        "graph": "EventGraph",
                        "patch_scope": "pin_default",
                    },
                    "args": {
                        "patch_type": "set_pin_default",
                        "patched_ref": {
                            "block_id": "BH_DoorFeature_ToggleDoor",
                            "group_entry_node_path": "logic.groups[0].entry.node_path",
                            "node_ref": "nodes[1]",
                            "pin_ref": "Condition",
                        },
                        "patch": {
                            "value": True,
                        },
                        "expected_old_state": {
                            "value": False,
                        },
                    },
                },
            ],
        }

        validate_graph_write_task_plan(task_plan)

    def test_accepts_merge_taskplan_shape_from_graph_write_contract(self):
        task_plan = {
            "schema": "BlueprintHelper.TaskPlan.v1",
            "task_name": "DoorFlowMerge",
            "task_type": "edit_blueprint_graph",
            "context_id": "ctx_merge",
            "target_assets": ["/Game/BP/BP_Door"],
            "execution_policy": {
                "dry_run_mode": "quick",
                "should_compile": True,
                "should_save": True,
            },
            "steps": [
                {
                    "step_id": "step_001",
                    "operation": "merge_blueprint_graph",
                    "target": {
                        "asset_path": "/Game/BP/BP_Door",
                        "graph": "EventGraph",
                        "merge_scope": "owned_block_call",
                        "insert_strategy": "insert_between",
                    },
                    "args": {
                        "anchor": {
                            "block_id": "BH_DoorFeature_ToggleDoor",
                            "group_entry_node_path": "logic.groups[0].entry.node_path",
                            "node_ref": "nodes[0]",
                            "pin_ref": "Then",
                        },
                        "inserted": {
                            "block_id": "EG_DoorFeature_ToggleDoor0",
                        },
                    },
                },
            ],
        }

        validate_graph_write_task_plan(task_plan)

    def test_orchestrator_reexports_focused_compiler_api(self):
        self.assertIsNotNone(importlib.util.find_spec("blueprinthelper_task.shared.errors"))
        self.assertIsNotNone(importlib.util.find_spec("blueprinthelper_task.compiler.graph_write_append"))

        from blueprinthelper_task.shared.errors import TaskSpecCompileError as focused_error
        from blueprinthelper_task.compiler.graph_write_append import compile_graph_write_append as focused_compile

        self.assertIs(TaskSpecCompileError, focused_error)
        self.assertIs(compile_graph_write_append, focused_compile)

    def test_compiles_taskspec_to_taskplan_and_bridge_payload(self):
        result = compile_graph_write_append(make_task_spec(), dry_run=True)

        self.assertEqual(result["schema"], "BlueprintHelper.TaskCompilerResult.v1")
        self.assertEqual(result["task_plan"]["schema"], "BlueprintHelper.TaskPlan.v1")
        validate_graph_write_task_plan(result["task_plan"])
        self.assertEqual(result["task_plan"]["execution_policy"], {
            "dry_run_mode": "full",
            "should_compile": False,
            "should_save": False,
        })
        self.assertNotIn("operation", result["task_plan"]["steps"][0])
        self.assertEqual(result["task_plan"]["steps"], [
            {
                "step_id": "step_001",
                "capability": "blueprint_signature",
                "target": {
                    "asset_path": "/Game/BP/BP_Door",
                },
                "write": {
                    "strategy": "custom_event_signature",
                    "ops": [
                        {
                            "op": "ensure_custom_event",
                            "event_name": "ToggleDoor",
                            "graph_name": "EG_DoorFeature",
                            "name_collision_policy": "reuse_if_exists",
                        },
                    ],
                },
            },
            {
                "step_id": "step_002",
                "capability": "graph_write",
                "target": {
                    "asset_path": "/Game/BP/BP_Door",
                    "graph": "EG_DoorFeature",
                },
                "write": {
                    "strategy": "owned_graph_edit",
                    "ops": [
                        {
                            "op": "ensure_entry",
                            "entry_type": "custom_event",
                            "name": "ToggleDoor",
                            "body": {
                                "schema": "BlueprintLogicSpec.v1",
                                "statements": [
                                    {
                                        "kind": "call_function",
                                        "name": "PrintString",
                                        "args": {
                                            "InString": {
                                                "kind": "literal",
                                                "value_type": "string",
                                                "value": "hello",
                                            },
                                        },
                                    },
                                ],
                            },
                        },
                    ],
                },
                "constraints": {
                    "allow_modify_user_nodes": False,
                    "ownership_scope": "blueprinthelper_owned",
                },
                "depends_on": ["step_001"],
            },
        ])
        self.assertEqual(result["bridge_payload"], {
            "target": {
                "asset_path": "/Game/BP/BP_Door",
                "graph": "EG_DoorFeature",
            },
            "feature_name": "DoorFeature",
            "nodes": [
                {"id": "ToggleDoor_entry", "kind": "custom_event", "name": "ToggleDoor"},
                {
                    "id": "ToggleDoor_stmt_1",
                    "kind": "call",
                    "function": "PrintString",
                    "inputs": {"InString": "hello"},
                },
            ],
            "links": [
                {"kind": "exec", "from": "ToggleDoor_entry.then", "to": "ToggleDoor_stmt_1.execute"},
            ],
            "dry_run": True,
        })

    def test_emits_signature_dependencies_before_graph_write_ensure_entry_ops_for_custom_event_entries(self):
        spec = make_task_spec(behavior={
            "graph_strategy": "append_new_owned_graph",
            "entries": [
                {
                    "entry_type": "custom_event",
                    "name": "ToggleDoor",
                    "body": {
                        "schema": "BlueprintLogicSpec.v1",
                        "statements": [
                            {
                                "kind": "call_function",
                                "name": "PrintString",
                                "args": {
                                    "InString": {
                                        "kind": "literal",
                                        "value_type": "string",
                                        "value": "hello",
                                    },
                                },
                            },
                        ],
                    },
                },
                {
                    "entry_type": "custom_event",
                    "name": "CloseDoor",
                    "body": {
                        "schema": "BlueprintLogicSpec.v1",
                        "statements": [
                            {
                                "kind": "set_member_variable",
                                "name": "bDoorOpen",
                                "value": {
                                    "kind": "literal",
                                    "value_type": "bool",
                                    "value": False,
                                },
                            },
                        ],
                    },
                },
            ],
        })

        result = compile_graph_write_append(spec, dry_run=False)
        signature_steps = [
            step for step in result["task_plan"]["steps"]
            if step["capability"] == "blueprint_signature"
        ]
        graph_write_step = next(
            step for step in result["task_plan"]["steps"]
            if step["capability"] == "graph_write"
        )
        self.assertEqual(
            [
                {
                    "step_id": step["step_id"],
                    "strategy": step["write"]["strategy"],
                    "op": step["write"]["ops"][0]["op"],
                    "name": step["write"]["ops"][0]["event_name"],
                }
                for step in signature_steps
            ],
            [
                {
                    "step_id": "step_001",
                    "strategy": "custom_event_signature",
                    "op": "ensure_custom_event",
                    "name": "ToggleDoor",
                },
                {
                    "step_id": "step_002",
                    "strategy": "custom_event_signature",
                    "op": "ensure_custom_event",
                    "name": "CloseDoor",
                },
            ],
        )
        self.assertEqual(graph_write_step["depends_on"], ["step_001", "step_002"])
        self.assertEqual(
            [{"op": op["op"], "name": op["name"]} for op in graph_write_step["write"]["ops"]],
            [
                {"op": "ensure_entry", "name": "ToggleDoor"},
                {"op": "ensure_entry", "name": "CloseDoor"},
            ],
        )

    def test_rejects_unsupported_graph_strategy(self):
        spec = make_task_spec(behavior={
            "graph_strategy": "replace_graph",
            "entries": [
                {
                    "entry_type": "custom_event",
                    "name": "ToggleDoor",
                    "body": {"schema": "BlueprintLogicSpec.v1", "statements": []},
                },
            ],
        })

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_graph_write_append(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_graph_strategy")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.graph_strategy")

    def test_compiles_replace_owned_graph_to_structured_ir(self):
        spec = make_task_spec(behavior={
            "graph_strategy": "replace_owned_graph",
            "replace": {
                "scope": "custom_event_body",
                "selector": {
                    "kind": "custom_event",
                    "name": "ToggleDoor",
                    "graph_id": "EventGraph",
                    "node_ref": "ToggleDoorEntry",
                },
                "body": {
                    "schema": "BlueprintLogicSpec.v1",
                    "statements": [
                        {
                            "kind": "call_function",
                            "name": "PrintString",
                            "args": {
                                "InString": {
                                    "kind": "literal",
                                    "value_type": "string",
                                    "value": "replaced",
                                },
                            },
                        },
                    ],
                },
                "options": {
                    "strict": True,
                    "preserve_layout": False,
                },
            },
        })

        result = compile_graph_write_append(spec, dry_run=True)
        step = result["task_plan"]["steps"][0]

        self.assertNotIn("operation", step)
        self.assertEqual(step["capability"], "graph_write")
        self.assertEqual(step["write"]["strategy"], "owned_graph_edit")
        self.assertEqual(step["write"]["ops"], [
            {
                "op": "replace_body",
                "replace_scope": "custom_event_body",
                "selector": {
                    "entry_name": "ToggleDoor",
                    "graph_id": "EventGraph",
                    "node_ref": "ToggleDoorEntry",
                },
                "replacement": {
                    "nodes": [
                        {
                            "id": "replace_stmt_1",
                            "kind": "call",
                            "function": "PrintString",
                            "inputs": {
                                "InString": "replaced",
                            },
                        },
                    ],
                    "links": [],
                },
                "options": {
                    "strict": True,
                    "preserve_layout": False,
                },
            },
        ])
        self.assertEqual(result["bridge_payload"], {
            "task_plan": result["task_plan"],
        })

    def test_compiles_patch_owned_graph_to_structured_ir(self):
        spec = make_task_spec(behavior={
            "graph_strategy": "patch_owned_graph",
            "patches": [
                {
                    "kind": "set_pin_default",
                    "target_ref": {
                        "block_id": "BH_DoorFeature_ToggleDoor",
                        "group_entry_node_path": "logic.groups[0].entry.node_path",
                        "node_ref": "nodes[1]",
                        "pin_ref": "Condition",
                        "link_ref": "links[0]",
                    },
                    "value": {
                        "kind": "literal",
                        "value_type": "bool",
                        "value": True,
                    },
                    "expected_old_state": {
                        "value": {
                            "kind": "literal",
                            "value_type": "bool",
                            "value": False,
                        },
                    },
                },
            ],
        })

        result = compile_graph_write_append(spec, dry_run=True)
        step = result["task_plan"]["steps"][0]

        self.assertNotIn("operation", step)
        self.assertEqual(step["write"]["ops"], [
            {
                "op": "set_pin_default",
                "patch_scope": "pin_default",
                "patched_ref": {
                    "block_id": "BH_DoorFeature_ToggleDoor",
                    "group_entry_node_path": "logic.groups[0].entry.node_path",
                    "node_ref": "nodes[1]",
                    "pin_ref": "Condition",
                    "link_ref": "links[0]",
                },
                "patch": {
                    "value": "true",
                },
                "expected_old_state": {
                    "value": "false",
                },
            },
        ])

    def test_compiles_merge_owned_graph_to_structured_ir(self):
        spec = make_task_spec(behavior={
            "graph_strategy": "merge_owned_graph",
            "merges": [
                {
                    "kind": "insert_flow",
                    "scope": "function_call",
                    "insert_strategy": "insert_between",
                    "anchor": {
                        "block_id": "BH_DoorFeature_ToggleDoor",
                        "group_entry_node_path": "logic.groups[0].entry.node_path",
                        "node_ref": "nodes[0]",
                        "pin_ref": "Then",
                        "link_ref": "links[0]",
                    },
                    "inserted": {
                        "call_kind": "function_call",
                        "name": "OpenDoor",
                    },
                },
            ],
        })

        result = compile_graph_write_append(spec, dry_run=True)
        step = result["task_plan"]["steps"][0]

        self.assertNotIn("operation", step)
        self.assertEqual(step["write"]["ops"], [
            {
                "op": "insert_flow",
                "merge_scope": "function_call",
                "insert_strategy": "insert_between",
                "anchor": {
                    "block_id": "BH_DoorFeature_ToggleDoor",
                    "group_entry_node_path": "logic.groups[0].entry.node_path",
                    "node_ref": "nodes[0]",
                    "pin_ref": "Then",
                    "link_ref": "links[0]",
                },
                "inserted": {
                    "function": "OpenDoor",
                },
            },
        ])

    def test_rejects_patch_owned_graph_with_bare_nodes_index_anchor(self):
        spec = make_task_spec(behavior={
            "graph_strategy": "patch_owned_graph",
            "patches": [
                {
                    "kind": "set_pin_default",
                    "target_ref": {
                        "graph_id": "EventGraph",
                        "node_ref": "nodes[0]",
                        "pin_ref": "Condition",
                    },
                    "value": True,
                },
            ],
        })

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_graph_write_append(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_graph_write_anchor")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.patches[0].target_ref.node_ref")

    def test_rejects_merge_owned_graph_with_bare_nodes_index_anchor(self):
        spec = make_task_spec(behavior={
            "graph_strategy": "merge_owned_graph",
            "merges": [
                {
                    "kind": "insert_flow",
                    "scope": "function_call",
                    "insert_strategy": "insert_between",
                    "anchor": {
                        "graph_id": "EventGraph",
                        "node_ref": "nodes[0]",
                        "pin_ref": "Then",
                    },
                    "inserted": {
                        "call_kind": "function_call",
                        "name": "OpenDoor",
                    },
                },
            ],
        })

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_graph_write_append(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_graph_write_anchor")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.merges[0].anchor.node_ref")

    def test_rejects_legacy_validation_compile_save_fields(self):
        spec = make_task_spec(validation={
            "compile": True,
            "save": True,
        })

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_graph_write_append(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_validation_fields")
        self.assertEqual(ctx.exception.issues[0]["path"], "validation")

    def test_rejects_structured_graph_write_ir_with_adapter_operation_field(self):
        task_plan = compile_graph_write_append(make_task_spec(), dry_run=True)["task_plan"]
        graph_write_index = next(
            index for index, step in enumerate(task_plan["steps"])
            if step["capability"] == "graph_write"
        )
        task_plan["steps"][graph_write_index]["operation"] = "append_blueprint_graph"

        with self.assertRaises(TaskSpecCompileError) as ctx:
            validate_graph_write_task_plan(task_plan)

        self.assertEqual(ctx.exception.code, "unsupported_graph_write_operation_field")
        self.assertEqual(ctx.exception.issues[0]["path"], f"steps[{graph_write_index}].operation")

    def test_compiles_blueprint_variable_taskspec_to_structured_ir(self):
        result = compile_task_spec(make_variable_task_spec(), dry_run=True)

        self.assertEqual(result["schema"], "BlueprintHelper.TaskCompilerResult.v1")
        self.assertEqual(result["task_plan"]["schema"], "BlueprintHelper.TaskPlan.v1")
        self.assertNotIn("operation", result["task_plan"]["steps"][0])
        self.assertEqual(result["task_plan"]["steps"], [
            {
                "step_id": "step_001",
                "capability": "blueprint_variable",
                "target": {
                    "asset_path": "/Game/BP/BP_Door",
                },
                "write": {
                    "strategy": "member_variables",
                    "ops": [
                        {
                            "op": "ensure_member_variable",
                            "name": "bDoorOpen",
                            "pin_type": {
                                "category": "bool",
                            },
                            "category": "Door",
                            "flags": {
                                "expose_on_spawn": False,
                            },
                        },
                    ],
                },
                "constraints": {
                    "allow_remove_referenced_variables": False,
                },
            },
        ])
        self.assertEqual(result["bridge_payload"], {
            "asset_path": "/Game/BP/BP_Door",
            "variables": [
                {
                    "name": "bDoorOpen",
                    "pin_type": {
                        "category": "bool",
                    },
                    "category": "Door",
                    "flags": {
                        "expose_on_spawn": False,
                    },
                },
            ],
            "dry_run": True,
        })

    def test_rejects_unsupported_blueprint_variable_strategy(self):
        spec = make_variable_task_spec(behavior={
            "variable_strategy": "graph_reference_rewrite",
            "variables": [
                {
                    "op": "ensure_member_variable",
                    "name": "TempValue",
                    "pin_type": {"category": "float"},
                },
            ],
        })

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_variable_strategy")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.variable_strategy")

    def test_rejects_unsupported_blueprint_variable_op(self):
        spec = make_variable_task_spec(behavior={
            "variable_strategy": "member_variables",
            "variables": [
                {
                    "op": "remove_member_variable",
                    "name": "bDoorOpen",
                },
            ],
        })

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_task_spec(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_variable_op")
        self.assertEqual(ctx.exception.issues[0]["path"], "behavior.variables[0].op")

    def test_compiles_semantic_member_variable_changes_to_structured_ir(self):
        spec = make_variable_task_spec(behavior={
            "variable_strategy": "member_variables",
            "changes": [
                {
                    "kind": "ensure_member_variable",
                    "name": "Health",
                    "variable_type": {"category": "float"},
                    "category": "Stats",
                },
                {
                    "kind": "configure_member_variable",
                    "name": "Health",
                    "properties": [
                        {
                            "property_path": "Tooltip",
                            "value": "Current health.",
                        },
                    ],
                },
                {
                    "kind": "remove_member_variable",
                    "name": "DeprecatedHealth",
                },
            ],
        })

        result = compile_task_spec(spec, dry_run=True)

        step = result["task_plan"]["steps"][0]
        self.assertNotIn("operation", step)
        self.assertEqual(step["capability"], "blueprint_variable")
        self.assertEqual(step["write"]["strategy"], "member_variables")
        self.assertEqual(step["write"]["ops"], [
            {
                "op": "ensure_member_variable",
                "name": "Health",
                "pin_type": {"category": "float"},
                "category": "Stats",
            },
            {
                "op": "set_member_variable_properties",
                "name": "Health",
                "settings": [
                    {
                        "property_path": "Tooltip",
                        "value": "Current health.",
                    },
                ],
            },
            {
                "op": "remove_member_variable",
                "name": "DeprecatedHealth",
            },
        ])
        self.assertEqual(result["bridge_payload"], {
            "task_plan": result["task_plan"],
        })

    def test_compiles_member_defaults_to_structured_ir(self):
        spec = make_variable_task_spec(behavior={
            "variable_strategy": "member_defaults",
            "defaults": [
                {
                    "name": "Health",
                    "value": {
                        "kind": "literal",
                        "value_type": "float",
                        "value": 100.0,
                    },
                },
            ],
        })

        result = compile_task_spec(spec, dry_run=True)

        step = result["task_plan"]["steps"][0]
        self.assertNotIn("operation", step)
        self.assertEqual(step["write"]["strategy"], "member_defaults")
        self.assertEqual(step["write"]["ops"], [
            {
                "op": "set_member_default",
                "name": "Health",
                "value": 100.0,
            },
        ])

    def test_compiles_local_variable_changes_to_structured_ir(self):
        spec = make_variable_task_spec(behavior={
            "variable_strategy": "local_variables",
            "function_name": "CalculateDamage",
            "changes": [
                {
                    "kind": "ensure_local_variable",
                    "name": "DamageScale",
                    "variable_type": {"category": "float"},
                },
                {
                    "kind": "configure_local_variable",
                    "name": "DamageScale",
                    "properties": [
                        {
                            "property_path": "DefaultValue",
                            "value": "1.0",
                        },
                    ],
                },
                {
                    "kind": "remove_local_variable",
                    "name": "OldDamageScale",
                },
            ],
        })

        result = compile_task_spec(spec, dry_run=True)

        step = result["task_plan"]["steps"][0]
        self.assertNotIn("operation", step)
        self.assertEqual(step["target"], {
            "asset_path": "/Game/BP/BP_Door",
            "function_name": "CalculateDamage",
        })
        self.assertEqual(step["write"]["strategy"], "local_variables")
        self.assertEqual(step["write"]["ops"], [
            {
                "op": "ensure_local_variable",
                "function_name": "CalculateDamage",
                "name": "DamageScale",
                "pin_type": {"category": "float"},
            },
            {
                "op": "set_local_variable_properties",
                "function_name": "CalculateDamage",
                "name": "DamageScale",
                "settings": [
                    {
                        "property_path": "DefaultValue",
                        "value": "1.0",
                    },
                ],
            },
            {
                "op": "remove_local_variable",
                "function_name": "CalculateDamage",
                "name": "OldDamageScale",
            },
        ])


if __name__ == "__main__":
    unittest.main()
