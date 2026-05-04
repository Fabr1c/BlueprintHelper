from __future__ import annotations

import json
import re
from typing import Any, Dict, List

from .errors import TaskSpecCompileError


TASK_COMPILER_RESULT_SCHEMA = "BlueprintHelper.TaskCompilerResult.v1"
TASK_PLAN_SCHEMA = "BlueprintHelper.TaskPlan.v1"


def compile_graph_write_append(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    _assert_supported_task_spec(task_spec)

    nodes: List[Dict[str, Any]] = []
    links: List[Dict[str, Any]] = []
    entries = task_spec["behavior"]["entries"]

    for entry_index, entry in enumerate(entries):
        entry_name = _required_string(entry, "name", f"behavior.entries[{entry_index}].name")
        entry_id = f"{_to_id_segment(entry_name)}_entry"
        nodes.append({"id": entry_id, "kind": "custom_event", "name": entry_name})

        previous_exec_endpoint = f"{entry_id}.then"
        statements = entry["body"].get("statements", [])
        for statement_index, statement in enumerate(statements):
            node_id = f"{_to_id_segment(entry_name)}_stmt_{statement_index + 1}"
            node = _compile_statement_node(
                statement,
                node_id,
                f"behavior.entries[{entry_index}].body.statements[{statement_index}]",
            )
            nodes.append(node)
            links.append({"kind": "exec", "from": previous_exec_endpoint, "to": f"{node_id}.execute"})
            previous_exec_endpoint = f"{node_id}.then"

    target = task_spec["target"]
    scope_policy = task_spec["scope_policy"]
    execution_policy = task_spec.get("execution_policy", {})
    validation = task_spec.get("validation", {})

    task_plan = {
        "schema": TASK_PLAN_SCHEMA,
        "task_name": task_spec.get("feature_name"),
        "task_type": task_spec["task_type"],
        "context_id": task_spec.get("context_id"),
        "target_assets": [target["asset_path"]],
        "execution_policy": {
            "dry_run_mode": execution_policy.get("dry_run_mode", "full"),
            "should_compile": validation.get("should_compile", False),
            "should_save": validation.get("should_save", False),
        },
        "steps": [
            {
                "step_id": "step_001",
                "operation": "append_blueprint_graph",
                "target": {
                    "asset_path": target["asset_path"],
                    "graph": scope_policy["graph_name"],
                },
                "args": _omit_none({
                    "feature_name": task_spec.get("feature_name"),
                    "nodes": nodes,
                    "links": links,
                }),
            },
        ],
    }

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": task_plan_to_append_bridge_payload(task_plan, dry_run),
        "task_plan_summary": summarize_task_plan(task_plan),
    }


def task_plan_to_append_bridge_payload(task_plan: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    steps = task_plan.get("steps", [])
    first_step = steps[0] if steps else None
    if not first_step or first_step.get("operation") != "append_blueprint_graph":
        raise TaskSpecCompileError(
            "unsupported_taskplan_operation",
            "TaskPlan does not contain an append_blueprint_graph step.",
            [{
                "code": "unsupported_taskplan_operation",
                "path": "steps[0].operation",
                "message": "Only append_blueprint_graph TaskPlan steps are supported in the first MCP slice.",
            }],
        )

    args = first_step["args"]
    return _omit_none({
        "target": {
            "asset_path": first_step["target"]["asset_path"],
            "graph": first_step["target"]["graph"],
        },
        "feature_name": args.get("feature_name"),
        "nodes": args["nodes"],
        "links": args["links"],
        "dry_run": dry_run,
    })


def summarize_task_plan(task_plan: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "schema": task_plan["schema"],
        "task_name": task_plan.get("task_name"),
        "task_type": task_plan["task_type"],
        "target_assets": task_plan["target_assets"],
        "steps": [
            {
                "step_id": step["step_id"],
                "operation": step["operation"],
                "target": step["target"],
                "nodes": len(step["args"]["nodes"]),
                "links": len(step["args"]["links"]),
            }
            for step in task_plan["steps"]
        ],
    }


def _assert_supported_task_spec(task_spec: Dict[str, Any]) -> None:
    validation = task_spec.get("validation", {})
    if isinstance(validation, dict) and ("compile" in validation or "save" in validation):
        raise TaskSpecCompileError(
            "unsupported_validation_fields",
            "Use validation.should_compile / validation.should_save; validation.compile / validation.save are not TaskSpec fields.",
            [{
                "code": "unsupported_validation_fields",
                "path": "validation",
                "message": "Replace validation.compile with validation.should_compile and validation.save with validation.should_save.",
                "suggested_patch": {
                    "op": "rename",
                    "from": "/validation/compile,/validation/save",
                    "to": "/validation/should_compile,/validation/should_save",
                },
            }],
        )

    behavior = task_spec.get("behavior", {})
    if behavior.get("graph_strategy") != "append_new_owned_graph":
        raise TaskSpecCompileError(
            "unsupported_graph_strategy",
            "Only append_new_owned_graph is supported in the first MCP slice.",
            [{
                "code": "unsupported_graph_strategy",
                "path": "behavior.graph_strategy",
                "message": 'Use behavior.graph_strategy="append_new_owned_graph" for this milestone.',
                "suggested_patch": {
                    "op": "replace",
                    "path": "/behavior/graph_strategy",
                    "value": "append_new_owned_graph",
                },
            }],
        )

    scope_policy = task_spec.get("scope_policy", {})
    if scope_policy.get("allow_modify_user_nodes"):
        raise TaskSpecCompileError(
            "unsupported_scope_policy",
            "Modifying user nodes is not supported for append_new_owned_graph.",
            [{
                "code": "unsupported_scope_policy",
                "path": "scope_policy.allow_modify_user_nodes",
                "message": "Set allow_modify_user_nodes=false and target a new or empty graph.",
                "suggested_patch": {
                    "op": "replace",
                    "path": "/scope_policy/allow_modify_user_nodes",
                    "value": False,
                },
            }],
        )

    for entry_index, entry in enumerate(behavior.get("entries", [])):
        if entry.get("entry_type") != "custom_event":
            raise TaskSpecCompileError(
                "unsupported_entry_type",
                "Only custom_event entries are supported in the first MCP slice.",
                [{
                    "code": "unsupported_entry_type",
                    "path": f"behavior.entries[{entry_index}].entry_type",
                    "message": 'Use entry_type="custom_event". Function/Event signature management is a later capability cluster.',
                    "suggested_patch": {
                        "op": "replace",
                        "path": f"/behavior/entries/{entry_index}/entry_type",
                        "value": "custom_event",
                    },
                }],
            )

        statements = entry.get("body", {}).get("statements", [])
        for statement_index, statement in enumerate(statements):
            if statement.get("kind") not in {"call_function", "set_member_variable"}:
                raise TaskSpecCompileError(
                    "unsupported_statement_kind",
                    "Only call_function and set_member_variable statements are supported in the first MCP slice.",
                    [{
                        "code": "unsupported_statement_kind",
                        "path": f"behavior.entries[{entry_index}].body.statements[{statement_index}].kind",
                        "message": "Use call_function or set_member_variable, or split this work into a later GraphWrite capability.",
                    }],
                )


def _compile_statement_node(statement: Dict[str, Any], node_id: str, path: str) -> Dict[str, Any]:
    kind = statement.get("kind")
    if kind == "call_function":
        return {
            "id": node_id,
            "kind": "call",
            "function": _required_string(statement, "name", f"{path}.name"),
            "inputs": _compile_args(statement.get("args")),
        }

    if kind == "set_member_variable":
        return {
            "id": node_id,
            "kind": "set",
            "var": _required_string(statement, "name", f"{path}.name"),
            "value": _value_expr_to_string(statement.get("value")),
        }

    raise TaskSpecCompileError(
        "unsupported_statement_kind",
        f"Unsupported statement kind: {kind}",
        [{
            "code": "unsupported_statement_kind",
            "path": f"{path}.kind",
            "message": f"Unsupported statement kind: {kind}",
        }],
    )


def _compile_args(args: Any) -> Dict[str, Any]:
    if not isinstance(args, dict):
        return {}
    return {key: _literal_value(value) for key, value in args.items()}


def _literal_value(value: Any) -> Any:
    if isinstance(value, dict) and value.get("kind") == "literal":
        return value.get("value")
    return value


def _value_expr_to_string(value: Any) -> str:
    literal = _literal_value(value)
    if isinstance(literal, str):
        return literal
    if isinstance(literal, bool):
        return "true" if literal else "false"
    if isinstance(literal, (int, float)):
        return str(literal)
    if literal is None:
        return ""
    return json.dumps(literal, ensure_ascii=False, separators=(",", ":"))


def _required_string(record: Dict[str, Any], field: str, path: str) -> str:
    value = record.get(field)
    if isinstance(value, str) and value.strip():
        return value
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path} must be a non-empty string.",
        [{
            "code": "missing_required_string",
            "path": path,
            "message": f"{path} must be a non-empty string.",
        }],
    )


def _to_id_segment(value: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9_]", "_", value)
    return normalized if normalized else "entry"


def _omit_none(record: Dict[str, Any]) -> Dict[str, Any]:
    return {key: value for key, value in record.items() if value is not None}


def _omit_none_deep(value: Any) -> Any:
    if isinstance(value, dict):
        return {key: _omit_none_deep(item) for key, item in value.items() if item is not None}
    if isinstance(value, list):
        return [_omit_none_deep(item) for item in value]
    return value
