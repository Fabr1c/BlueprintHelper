from __future__ import annotations

import json
import re
from typing import Any, Dict, List

from .errors import TaskSpecCompileError
from .p1_capabilities import compile_p1_task_spec, supports_p1_task_type


TASK_COMPILER_RESULT_SCHEMA = "BlueprintHelper.TaskCompilerResult.v1"
TASK_PLAN_SCHEMA = "BlueprintHelper.TaskPlan.v1"
GRAPH_WRITE_ADAPTER_OPERATIONS = {
    "append_blueprint_graph",
    "replace_blueprint_graph",
    "patch_blueprint_graph",
    "merge_blueprint_graph",
}


def compile_graph_write_append(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    _assert_supported_task_spec(task_spec)

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
                "capability": "graph_write",
                "target": {
                    "asset_path": target["asset_path"],
                    "graph": scope_policy["graph_name"],
                },
                "write": {
                    "strategy": "owned_graph_edit",
                    "ops": [
                        {
                            "op": "ensure_entry",
                            "entry_type": entry["entry_type"],
                            "name": entry["name"],
                            "body": entry["body"],
                        }
                        for entry in task_spec["behavior"]["entries"]
                    ],
                },
                "constraints": {
                    "allow_modify_user_nodes": scope_policy["allow_modify_user_nodes"],
                    "ownership_scope": "blueprinthelper_owned",
                },
            },
        ],
    }

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": task_plan_to_append_bridge_payload(task_plan, dry_run),
        "task_plan_summary": summarize_task_plan(task_plan),
    }


def compile_task_spec(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    task_type = task_spec.get("task_type")
    if task_type == "edit_blueprint_graph":
        return compile_graph_write_append(task_spec, dry_run)
    if task_type == "edit_blueprint_variables":
        return compile_blueprint_variables(task_spec, dry_run)
    if supports_p1_task_type(task_type):
        return compile_p1_task_spec(task_spec, dry_run)

    raise TaskSpecCompileError(
        "unsupported_task_type",
        f"Unsupported TaskSpec task_type: {task_type}",
        [{
            "code": "unsupported_task_type",
            "path": "task_type",
            "message": "Use a supported BlueprintHelper TaskSpec task_type.",
        }],
    )


def compile_blueprint_variables(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    _assert_supported_blueprint_variables_task_spec(task_spec)

    target = task_spec["target"]
    behavior = task_spec["behavior"]
    execution_policy = task_spec.get("execution_policy", {})
    validation = task_spec.get("validation", {})
    strategy = behavior["variable_strategy"]
    ops = _compile_blueprint_variable_ops(behavior)
    step_target = {
        "asset_path": target["asset_path"],
    }
    if strategy == "local_variables":
        step_target["function_name"] = _required_string(behavior, "function_name", "behavior.function_name")

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
                "capability": "blueprint_variable",
                "target": step_target,
                "write": {
                    "strategy": strategy,
                    "ops": ops,
                },
                "constraints": {
                    "allow_remove_referenced_variables": False,
                },
            },
        ],
    }

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": _blueprint_variable_bridge_payload(task_plan, task_plan["steps"][0], dry_run),
        "task_plan_summary": summarize_task_plan(task_plan),
    }


def task_plan_to_append_bridge_payload(task_plan: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    steps = task_plan.get("steps", [])
    first_step = steps[0] if steps else None
    if not first_step:
        raise TaskSpecCompileError(
            "unsupported_taskplan_operation",
            "TaskPlan does not contain an append_blueprint_graph step.",
            [{
                "code": "unsupported_taskplan_operation",
                "path": "steps[0]",
                "message": "TaskPlan requires a first GraphWrite step.",
            }],
        )

    if first_step.get("capability") == "graph_write":
        return _graph_write_taskplan_to_append_bridge_payload(task_plan, first_step, dry_run)

    if first_step.get("operation") != "append_blueprint_graph":
        raise TaskSpecCompileError(
            "unsupported_taskplan_operation",
            "TaskPlan does not contain an append_blueprint_graph step.",
            [{
                "code": "unsupported_taskplan_operation",
                "path": "steps[0].operation",
                "message": "Only append_blueprint_graph lowering adapter TaskPlan steps are supported in the first MCP slice.",
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


def validate_graph_write_task_plan(task_plan: Dict[str, Any]) -> None:
    if not isinstance(task_plan, dict):
        _raise_taskplan_invalid("invalid_taskplan", "TaskPlan must be an object.", "task_plan")

    if task_plan.get("schema") != TASK_PLAN_SCHEMA:
        _raise_taskplan_invalid("invalid_taskplan_schema", f"TaskPlan schema must be {TASK_PLAN_SCHEMA}.", "schema")

    target_assets = task_plan.get("target_assets")
    if not isinstance(target_assets, list) or not target_assets:
        _raise_taskplan_invalid("missing_target_assets", "TaskPlan requires target_assets[].", "target_assets")

    execution_policy = task_plan.get("execution_policy")
    if not isinstance(execution_policy, dict):
        _raise_taskplan_invalid("missing_execution_policy", "TaskPlan requires execution_policy.", "execution_policy")
    if "compile" in execution_policy or "save" in execution_policy:
        _raise_taskplan_invalid(
            "unsupported_execution_policy_fields",
            "Use execution_policy.should_compile / execution_policy.should_save.",
            "execution_policy",
        )

    steps = task_plan.get("steps")
    if not isinstance(steps, list) or not steps:
        _raise_taskplan_invalid("missing_taskplan_steps", "TaskPlan requires at least one step.", "steps")

    for index, step in enumerate(steps):
        _validate_graph_write_step(step, f"steps[{index}]")


def summarize_task_plan(task_plan: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "schema": task_plan["schema"],
        "task_name": task_plan.get("task_name"),
        "task_type": task_plan["task_type"],
        "target_assets": task_plan["target_assets"],
        "steps": [
            _summarize_task_plan_step(step)
            for step in task_plan["steps"]
        ],
    }


def _summarize_task_plan_step(step: Dict[str, Any]) -> Dict[str, Any]:
    if step.get("capability") in {"graph_write", "blueprint_variable"}:
        return {
            "step_id": step["step_id"],
            "capability": step["capability"],
            "target": step["target"],
            "strategy": step["write"]["strategy"],
            "ops": len(step["write"]["ops"]),
        }

    args = step.get("args", {})
    replacement = args.get("replacement", {}) if isinstance(args, dict) else {}
    nodes = None
    links = None
    if isinstance(args, dict) and isinstance(args.get("nodes"), list):
        nodes = len(args["nodes"])
    elif isinstance(replacement, dict) and isinstance(replacement.get("nodes"), list):
        nodes = len(replacement["nodes"])
    if isinstance(args, dict) and isinstance(args.get("links"), list):
        links = len(args["links"])
    elif isinstance(replacement, dict) and isinstance(replacement.get("links"), list):
        links = len(replacement["links"])

    summary = {
        "step_id": step["step_id"],
        "operation": step["operation"],
        "target": step["target"],
    }
    if nodes is not None:
        summary["nodes"] = nodes
    if links is not None:
        summary["links"] = links
    return summary


def _validate_graph_write_step(step: Any, path: str) -> None:
    if not isinstance(step, dict):
        _raise_taskplan_invalid("invalid_taskplan_step", "TaskPlan step must be an object.", path)

    _required_string(step, "step_id", f"{path}.step_id")
    if step.get("capability") == "graph_write":
        _validate_graph_write_ir_step(step, path)
        return

    operation = _required_string(step, "operation", f"{path}.operation")
    if operation not in GRAPH_WRITE_ADAPTER_OPERATIONS:
        _raise_taskplan_invalid(
            "unsupported_taskplan_operation",
            f"Unsupported GraphWrite TaskPlan operation: {operation}.",
            f"{path}.operation",
        )

    target = step.get("target")
    if not isinstance(target, dict):
        _raise_taskplan_invalid("missing_taskplan_step_target", "TaskPlan step requires target.", f"{path}.target")
    _required_string(target, "asset_path", f"{path}.target.asset_path")
    _required_string(target, "graph", f"{path}.target.graph")

    args = step.get("args")
    if not isinstance(args, dict):
        _raise_taskplan_invalid("missing_taskplan_step_args", "TaskPlan step requires args.", f"{path}.args")

    if operation == "append_blueprint_graph":
        _required_list(args, "nodes", f"{path}.args.nodes", min_length=1)
        _required_list(args, "links", f"{path}.args.links")
    elif operation == "replace_blueprint_graph":
        replacement = args.get("replacement")
        if not isinstance(replacement, dict):
            _raise_taskplan_invalid("missing_replacement", "replace_blueprint_graph requires args.replacement.", f"{path}.args.replacement")
        _required_list(replacement, "nodes", f"{path}.args.replacement.nodes")
        _required_list(replacement, "links", f"{path}.args.replacement.links")
    elif operation == "patch_blueprint_graph":
        _required_string(args, "patch_type", f"{path}.args.patch_type")
    elif operation == "merge_blueprint_graph":
        if "anchor" in args and not isinstance(args["anchor"], dict):
            _raise_taskplan_invalid("invalid_anchor", "merge_blueprint_graph args.anchor must be an object.", f"{path}.args.anchor")
        if "inserted" in args and not isinstance(args["inserted"], dict):
            _raise_taskplan_invalid("invalid_inserted", "merge_blueprint_graph args.inserted must be an object.", f"{path}.args.inserted")


def _validate_graph_write_ir_step(step: Dict[str, Any], path: str) -> None:
    if "operation" in step:
        _raise_taskplan_invalid(
            "unsupported_graph_write_operation_field",
            "GraphWrite IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.",
            f"{path}.operation",
        )

    target = step.get("target")
    if not isinstance(target, dict):
        _raise_taskplan_invalid("missing_taskplan_step_target", "TaskPlan step requires target.", f"{path}.target")
    _required_string(target, "asset_path", f"{path}.target.asset_path")
    _required_string(target, "graph", f"{path}.target.graph")

    write = step.get("write")
    if not isinstance(write, dict):
        _raise_taskplan_invalid("missing_graph_write", "GraphWrite TaskPlan step requires write.", f"{path}.write")
    _required_string(write, "strategy", f"{path}.write.strategy")
    ops = _required_list(write, "ops", f"{path}.write.ops", min_length=1)
    for index, op in enumerate(ops):
        if not isinstance(op, dict):
            _raise_taskplan_invalid("invalid_graph_write_op", "GraphWrite op must be an object.", f"{path}.write.ops[{index}]")
        _required_string(op, "op", f"{path}.write.ops[{index}].op")

    constraints = step.get("constraints")
    if not isinstance(constraints, dict):
        _raise_taskplan_invalid("missing_graph_write_constraints", "GraphWrite TaskPlan step requires constraints.", f"{path}.constraints")
    if not isinstance(constraints.get("allow_modify_user_nodes"), bool):
        _raise_taskplan_invalid(
            "invalid_graph_write_constraint",
            "constraints.allow_modify_user_nodes must be a boolean.",
            f"{path}.constraints.allow_modify_user_nodes",
        )
    _required_string(constraints, "ownership_scope", f"{path}.constraints.ownership_scope")


def _graph_write_taskplan_to_append_bridge_payload(
    task_plan: Dict[str, Any],
    step: Dict[str, Any],
    dry_run: bool,
) -> Dict[str, Any]:
    write = step.get("write")
    if not isinstance(write, dict):
        _raise_taskplan_invalid("missing_graph_write", "GraphWrite TaskPlan step requires write.", "steps[0].write")
    strategy = _required_string(write, "strategy", "steps[0].write.strategy")
    if strategy != "owned_graph_edit":
        raise TaskSpecCompileError(
            "unsupported_graph_write_strategy",
            f"Unsupported GraphWrite strategy: {strategy}",
            [{
                "code": "unsupported_graph_write_strategy",
                "path": "steps[0].write.strategy",
                "message": "Only owned_graph_edit can lower to append_blueprint_graph in the first MCP slice.",
            }],
        )

    nodes: List[Dict[str, Any]] = []
    links: List[Dict[str, Any]] = []
    ops = _required_list(write, "ops", "steps[0].write.ops", min_length=1)
    for op_index, op in enumerate(ops):
        if not isinstance(op, dict):
            _raise_taskplan_invalid("invalid_graph_write_op", "GraphWrite op must be an object.", f"steps[0].write.ops[{op_index}]")
        if op.get("op") != "ensure_entry":
            raise TaskSpecCompileError(
                "unsupported_graph_write_op",
                f"Unsupported GraphWrite op for append lowering: {op.get('op')}",
                [{
                    "code": "unsupported_graph_write_op",
                    "path": f"steps[0].write.ops[{op_index}].op",
                    "message": "Only ensure_entry lowers to append_blueprint_graph in the first MCP slice.",
                }],
            )
        _compile_ensure_entry_op_into_append_payload(nodes, links, op, f"steps[0].write.ops[{op_index}]")

    return _omit_none({
        "target": {
            "asset_path": step["target"]["asset_path"],
            "graph": step["target"]["graph"],
        },
        "feature_name": task_plan.get("task_name"),
        "nodes": nodes,
        "links": links,
        "dry_run": dry_run,
    })


def _blueprint_variable_taskplan_to_bridge_payload(
    task_plan: Dict[str, Any],
    step: Dict[str, Any],
    dry_run: bool,
) -> Dict[str, Any]:
    write = step.get("write")
    if not isinstance(write, dict):
        _raise_taskplan_invalid("missing_variable_write", "Blueprint variable TaskPlan step requires write.", "steps[0].write")
    strategy = _required_string(write, "strategy", "steps[0].write.strategy")
    if strategy != "member_variables":
        raise TaskSpecCompileError(
            "unsupported_variable_strategy",
            f"Unsupported Blueprint Variable strategy: {strategy}",
            [{
                "code": "unsupported_variable_strategy",
                "path": "steps[0].write.strategy",
                "message": "Only member_variables can lower to add_blueprint_member_variables in this slice.",
            }],
        )

    variables = []
    for op_index, op in enumerate(_required_list(write, "ops", "steps[0].write.ops", min_length=1)):
        if not isinstance(op, dict):
            _raise_taskplan_invalid("invalid_variable_op", "Blueprint variable op must be an object.", f"steps[0].write.ops[{op_index}]")
        if op.get("op") != "ensure_member_variable":
            raise TaskSpecCompileError(
                "unsupported_variable_op",
                f"Unsupported Blueprint Variable op: {op.get('op')}",
                [{
                    "code": "unsupported_variable_op",
                    "path": f"steps[0].write.ops[{op_index}].op",
                    "message": "Only ensure_member_variable is supported in this slice.",
                }],
            )
        payload = {key: value for key, value in op.items() if key != "op"}
        variables.append(payload)

    return _omit_none({
        "asset_path": step["target"]["asset_path"],
        "variables": variables,
        "dry_run": dry_run,
    })


def _blueprint_variable_bridge_payload(
    task_plan: Dict[str, Any],
    step: Dict[str, Any],
    dry_run: bool,
) -> Dict[str, Any]:
    write = step.get("write")
    strategy = write.get("strategy") if isinstance(write, dict) else None
    if strategy == "member_variables":
        ops = write.get("ops") if isinstance(write, dict) else []
        if isinstance(ops, list) and all(isinstance(op, dict) and op.get("op") == "ensure_member_variable" for op in ops):
            return _blueprint_variable_taskplan_to_bridge_payload(task_plan, step, dry_run)
    return {
        "task_plan": _omit_none_deep(task_plan),
    }


def _required_list(record: Dict[str, Any], field: str, path: str, min_length: int = 0) -> List[Any]:
    value = record.get(field)
    if isinstance(value, list) and len(value) >= min_length:
        return value
    _raise_taskplan_invalid("missing_required_list", f"{path} must be a list.", path)


def _raise_taskplan_invalid(code: str, message: str, path: str) -> None:
    raise TaskSpecCompileError(
        code,
        message,
        [{
            "code": code,
            "path": path,
            "message": message,
        }],
    )


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


def _assert_supported_blueprint_variables_task_spec(task_spec: Dict[str, Any]) -> None:
    validation = task_spec.get("validation", {})
    if isinstance(validation, dict) and ("compile" in validation or "save" in validation):
        raise TaskSpecCompileError(
            "unsupported_validation_fields",
            "Use validation.should_compile / validation.should_save; validation.compile / validation.save are not TaskSpec fields.",
            [{
                "code": "unsupported_validation_fields",
                "path": "validation",
                "message": "Replace validation.compile with validation.should_compile and validation.save with validation.should_save.",
            }],
        )

    behavior = task_spec.get("behavior", {})
    strategy = behavior.get("variable_strategy")
    if strategy not in {"member_variables", "member_defaults", "local_variables"}:
        raise TaskSpecCompileError(
            "unsupported_variable_strategy",
            "Unsupported Blueprint Variables strategy.",
            [{
                "code": "unsupported_variable_strategy",
                "path": "behavior.variable_strategy",
                "message": "Use member_variables, member_defaults, or local_variables.",
                "suggested_patch": {
                    "op": "replace",
                    "path": "/behavior/variable_strategy",
                    "value": "member_variables",
                },
            }],
        )

    if strategy == "member_variables":
        entries = behavior.get("changes")
        if entries is None:
            entries = behavior.get("variables")
        path = "behavior.changes" if "changes" in behavior else "behavior.variables"
    elif strategy == "member_defaults":
        entries = behavior.get("defaults")
        path = "behavior.defaults"
    else:
        entries = behavior.get("changes")
        path = "behavior.changes"
        _required_string(behavior, "function_name", "behavior.function_name")

    if not isinstance(entries, list) or not entries:
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            f"{path} must contain at least one variable change.",
            [{
                "code": "missing_variables",
                "path": path,
                "message": "Provide at least one variable change.",
            }],
        )

    for variable_index, variable in enumerate(entries):
        if not isinstance(variable, dict):
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "Blueprint variable entry must be an object.",
                [{
                    "code": "invalid_variable",
                    "path": f"{path}[{variable_index}]",
                    "message": "Blueprint variable entry must be an object.",
                }],
            )
        if "op" in variable and "kind" not in variable:
            if variable.get("op") == "ensure_member_variable":
                _require_pin_type_alias(variable, f"{path}[{variable_index}]")
                continue
            raise TaskSpecCompileError(
                "unsupported_variable_op",
                "Agent-facing Blueprint variable TaskSpec entries use kind, not op.",
                [{
                    "code": "unsupported_variable_op",
                    "path": f"{path}[{variable_index}].op",
                    "message": "Replace adapter-style op with a semantic kind.",
                }],
            )

        kind = variable.get("kind")
        if strategy == "member_defaults" and kind is None:
            if "value" not in variable:
                raise TaskSpecCompileError(
                    "taskspec_semantic_invalid",
                    "set_member_default requires value.",
                    [{
                        "code": "missing_variable_default_value",
                        "path": f"{path}[{variable_index}].value",
                        "message": "Provide a default value.",
                    }],
                )
            continue
        allowed = {
            "member_variables": {"ensure_member_variable", "configure_member_variable", "remove_member_variable"},
            "member_defaults": {"set_member_default"},
            "local_variables": {"ensure_local_variable", "configure_local_variable", "remove_local_variable"},
        }[strategy]
        if kind not in allowed:
            raise TaskSpecCompileError(
                "unsupported_variable_op",
                f"Unsupported Blueprint variable change kind: {kind}",
                [{
                    "code": "unsupported_variable_op",
                    "path": f"{path}[{variable_index}].kind",
                    "message": f"Use one of: {', '.join(sorted(allowed))}.",
                }],
            )
        if kind in {"ensure_member_variable", "ensure_local_variable"}:
            _require_pin_type_alias(variable, f"{path}[{variable_index}]")
        if kind in {"configure_member_variable", "configure_local_variable"}:
            _required_non_empty_list(variable, "properties", f"{path}[{variable_index}].properties")
        if kind == "set_member_default" and "value" not in variable:
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "set_member_default requires value.",
                [{
                    "code": "missing_variable_default_value",
                    "path": f"{path}[{variable_index}].value",
                    "message": "Provide a default value.",
                }],
            )


def _compile_blueprint_variable_ops(behavior: Dict[str, Any]) -> List[Dict[str, Any]]:
    strategy = behavior["variable_strategy"]
    if strategy == "member_variables":
        entries = behavior.get("changes")
        if entries is None:
            entries = behavior.get("variables", [])
        return [
            _compile_member_variable_change(entry, f"behavior.changes[{index}]")
            for index, entry in enumerate(entries)
        ]
    if strategy == "member_defaults":
        return [
            _compile_member_default_change(entry, f"behavior.defaults[{index}]")
            for index, entry in enumerate(behavior.get("defaults", []))
        ]

    function_name = _required_string(behavior, "function_name", "behavior.function_name")
    return [
        _compile_local_variable_change(entry, function_name, f"behavior.changes[{index}]")
        for index, entry in enumerate(behavior.get("changes", []))
    ]


def _compile_member_variable_change(change: Dict[str, Any], path: str) -> Dict[str, Any]:
    if "op" in change and "kind" not in change:
        op = dict(change)
        if "variable_type" in op and "pin_type" not in op:
            op["pin_type"] = op.pop("variable_type")
        return op

    kind = _required_string(change, "kind", f"{path}.kind")
    if kind == "ensure_member_variable":
        op = _copy_known_fields(change, ["name", "category", "tooltip", "flags", "metadata", "name_collision"])
        op["op"] = "ensure_member_variable"
        op["pin_type"] = _variable_pin_type(change, path)
        return op
    if kind == "configure_member_variable":
        return {
            "op": "set_member_variable_properties",
            "name": _required_string(change, "name", f"{path}.name"),
            "settings": _required_non_empty_list(change, "properties", f"{path}.properties"),
        }
    if kind == "remove_member_variable":
        return {
            "op": "remove_member_variable",
            "name": _required_string(change, "name", f"{path}.name"),
        }
    raise AssertionError(f"unsupported member variable kind after validation: {kind}")


def _compile_member_default_change(change: Dict[str, Any], path: str) -> Dict[str, Any]:
    return {
        "op": "set_member_default",
        "name": _required_string(change, "name", f"{path}.name"),
        "value": _literal_value(change.get("value")),
    }


def _compile_local_variable_change(change: Dict[str, Any], function_name: str, path: str) -> Dict[str, Any]:
    kind = _required_string(change, "kind", f"{path}.kind")
    if kind == "ensure_local_variable":
        return {
            "op": "ensure_local_variable",
            "function_name": function_name,
            "name": _required_string(change, "name", f"{path}.name"),
            "pin_type": _variable_pin_type(change, path),
        }
    if kind == "configure_local_variable":
        return {
            "op": "set_local_variable_properties",
            "function_name": function_name,
            "name": _required_string(change, "name", f"{path}.name"),
            "settings": _required_non_empty_list(change, "properties", f"{path}.properties"),
        }
    if kind == "remove_local_variable":
        return {
            "op": "remove_local_variable",
            "function_name": function_name,
            "name": _required_string(change, "name", f"{path}.name"),
        }
    raise AssertionError(f"unsupported local variable kind after validation: {kind}")


def _copy_known_fields(source: Dict[str, Any], fields: List[str]) -> Dict[str, Any]:
    out: Dict[str, Any] = {}
    for field in fields:
        if field in source:
            out[field] = source[field]
    return out


def _variable_pin_type(record: Dict[str, Any], path: str) -> Dict[str, Any]:
    value = record.get("pin_type")
    if value is None:
        value = record.get("variable_type")
    if isinstance(value, dict):
        return dict(value)
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        "Blueprint variable type is required.",
        [{
            "code": "missing_variable_pin_type",
            "path": f"{path}.variable_type",
            "message": 'Provide variable_type, for example {"category":"bool"}.',
        }],
    )


def _require_pin_type_alias(record: Dict[str, Any], path: str) -> None:
    _variable_pin_type(record, path)


def _required_non_empty_list(record: Dict[str, Any], field: str, path: str) -> List[Any]:
    value = record.get(field)
    if isinstance(value, list) and value:
        return value
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path} must be a non-empty list.",
        [{
            "code": "missing_required_list",
            "path": path,
            "message": f"{path} must be a non-empty list.",
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


def _compile_ensure_entry_op_into_append_payload(
    nodes: List[Dict[str, Any]],
    links: List[Dict[str, Any]],
    op: Dict[str, Any],
    path: str,
) -> None:
    entry_type = _required_string(op, "entry_type", f"{path}.entry_type")
    if entry_type != "custom_event":
        raise TaskSpecCompileError(
            "unsupported_entry_type",
            "Only custom_event entries are supported in the first MCP slice.",
            [{
                "code": "unsupported_entry_type",
                "path": f"{path}.entry_type",
                "message": 'Use entry_type="custom_event". Function/Event signature management is a later capability cluster.',
            }],
        )

    entry_name = _required_string(op, "name", f"{path}.name")
    body = _required_logic_body(op, "body", f"{path}.body")
    entry_id = f"{_to_id_segment(entry_name)}_entry"
    nodes.append({"id": entry_id, "kind": "custom_event", "name": entry_name})

    previous_exec_endpoint = f"{entry_id}.then"
    for statement_index, statement in enumerate(body["statements"]):
        node_id = f"{_to_id_segment(entry_name)}_stmt_{statement_index + 1}"
        node = _compile_statement_node(
            statement,
            node_id,
            f"{path}.body.statements[{statement_index}]",
        )
        nodes.append(node)
        links.append({"kind": "exec", "from": previous_exec_endpoint, "to": f"{node_id}.execute"})
        previous_exec_endpoint = f"{node_id}.then"


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


def _required_logic_body(record: Dict[str, Any], field: str, path: str) -> Dict[str, List[Dict[str, Any]]]:
    value = record.get(field)
    if isinstance(value, dict) and isinstance(value.get("statements"), list):
        return {"statements": value["statements"]}
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path} must be a BlueprintLogicSpec body.",
        [{
            "code": "missing_required_logic_body",
            "path": path,
            "message": f"{path} must be a BlueprintLogicSpec body.",
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
