import unittest
import importlib.util

from blueprinthelper_task.orchestrator import (
    TaskSpecCompileError,
    compile_graph_write_append,
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


class GraphWriteAppendCompilerTests(unittest.TestCase):
    def test_orchestrator_reexports_focused_compiler_api(self):
        self.assertIsNotNone(importlib.util.find_spec("blueprinthelper_task.errors"))
        self.assertIsNotNone(importlib.util.find_spec("blueprinthelper_task.graph_write_append"))

        from blueprinthelper_task.errors import TaskSpecCompileError as focused_error
        from blueprinthelper_task.graph_write_append import compile_graph_write_append as focused_compile

        self.assertIs(TaskSpecCompileError, focused_error)
        self.assertIs(compile_graph_write_append, focused_compile)

    def test_compiles_taskspec_to_taskplan_and_bridge_payload(self):
        result = compile_graph_write_append(make_task_spec(), dry_run=True)

        self.assertEqual(result["schema"], "BlueprintHelper.TaskCompilerResult.v1")
        self.assertEqual(result["task_plan"]["schema"], "BlueprintHelper.TaskPlan.v1")
        self.assertEqual(result["task_plan"]["execution_policy"], {
            "dry_run_mode": "full",
            "should_compile": False,
            "should_save": False,
        })
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

    def test_rejects_legacy_validation_compile_save_fields(self):
        spec = make_task_spec(validation={
            "compile": True,
            "save": True,
        })

        with self.assertRaises(TaskSpecCompileError) as ctx:
            compile_graph_write_append(spec, dry_run=True)

        self.assertEqual(ctx.exception.code, "unsupported_validation_fields")
        self.assertEqual(ctx.exception.issues[0]["path"], "validation")


if __name__ == "__main__":
    unittest.main()
