from __future__ import annotations

from typing import Any, Dict, List, Optional

from .errors import TaskSpecCompileError


TASK_COMPILER_RESULT_SCHEMA = "BlueprintHelper.TaskCompilerResult.v1"
TASK_PLAN_SCHEMA = "BlueprintHelper.TaskPlan.v1"


P2_TASK_TYPES = {
    "edit_object_properties",
    "manage_blueprinthelper_ownership",
}


def supports_p2_task_type(task_type: Any) -> bool:
    return task_type in P2_TASK_TYPES


def compile_p2_task_spec(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    task_type = task_spec.get("task_type")
    if task_type == "edit_object_properties":
        task_plan = _compile_object_property_task_plan(task_spec)
    elif task_type == "manage_blueprinthelper_ownership":
        task_plan = _compile_graph_cleanup_ownership_task_plan(task_spec)
    else:
        raise TaskSpecCompileError(
            "unsupported_task_type",
            f"Unsupported P2 TaskSpec task_type: {task_type}",
            [_issue("unsupported_task_type", "task_type", "Use a supported BlueprintHelper P2 TaskSpec task_type.")],
        )

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": {
            "task_plan": _omit_none_deep(task_plan),
        },
        "task_plan_summary": _summarize_task_plan(task_plan),
    }


def _compile_object_property_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    _require_exact_string(
        behavior,
        "property_strategy",
        "property_edit",
        "behavior.property_strategy",
        "unsupported_object_property_strategy",
    )
    changes = _required_non_empty_list(behavior, "changes", "behavior.changes")
    settings: List[Dict[str, Any]] = []
    for index, raw_change in enumerate(changes):
        path = f"behavior.changes[{index}]"
        change = _required_object_value(raw_change, path)
        property_path = _required_string(change, "property_path", f"{path}.property_path")
        if "value" not in change:
            _raise("taskspec_semantic_invalid", f"{path}.value is required.", f"{path}.value", "Provide value.")
        settings.append({
            "property_path": property_path,
            "value": _literal_value(change["value"]),
        })

    op: Dict[str, Any]
    if len(settings) == 1:
        op = {
            "op": "set_object_property",
            "property_path": settings[0]["property_path"],
            "value": settings[0]["value"],
        }
    else:
        op = {
            "op": "set_object_properties",
            "settings": settings,
        }

    return _make_task_plan(task_spec, [
        _make_step(
            1,
            "object_property",
            _target_asset_path(task_spec),
            "property_edit",
            [op],
            {"property_scope": "uobject"},
        ),
    ])


def _compile_graph_cleanup_ownership_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    _require_exact_string(
        behavior,
        "ownership_strategy",
        "owned_block_lifecycle",
        "behavior.ownership_strategy",
        "unsupported_ownership_strategy",
    )
    changes = _required_non_empty_list(behavior, "changes", "behavior.changes")
    steps = []
    for index, raw_change in enumerate(changes):
        path = f"behavior.changes[{index}]"
        change = _required_object_value(raw_change, path)
        steps.append(_make_step(
            index + 1,
            "graph_cleanup_ownership",
            _target_asset_path(task_spec),
            "owned_block_lifecycle",
            [_compile_graph_cleanup_ownership_op(change, path)],
        ))
    return _make_task_plan(task_spec, steps)


def _compile_graph_cleanup_ownership_op(change: Dict[str, Any], path: str) -> Dict[str, Any]:
    kind = _required_string(change, "kind", f"{path}.kind")
    op_by_kind = {
        "cleanup_block": "cleanup_blueprint_helper_block",
        "cleanup_blueprint_helper_block": "cleanup_blueprint_helper_block",
        "convert_block_to_user_owned": "convert_blueprint_helper_block_to_user_owned",
        "convert_blueprint_helper_block_to_user_owned": "convert_blueprint_helper_block_to_user_owned",
        "rollback_cleanup_transaction": "rollback_cleanup_transaction",
    }
    op_name = op_by_kind.get(kind)
    if not op_name:
        _raise(
            "unsupported_ownership_op",
            f"Unsupported ownership change kind: {kind}",
            f"{path}.kind",
            "Use cleanup_block, convert_block_to_user_owned, or rollback_cleanup_transaction.",
        )

    if op_name == "rollback_cleanup_transaction":
        return _omit_none({
            "op": op_name,
            "transaction_id": _required_string(change, "transaction_id", f"{path}.transaction_id"),
            "asset_path": change.get("asset_path"),
            "rollback_scope": change.get("rollback_scope", "cleanup_transaction"),
            "already_rolled_back_policy": change.get("already_rolled_back_policy"),
        })

    block_id = _optional_string(change, "block_id")
    graph_id = _optional_string(change, "graph_id")
    block_ref = _optional_string(change, "block_ref")
    if not block_id and not (graph_id and block_ref):
        _raise(
            "taskspec_semantic_invalid",
            "Owned block operation requires block_id or graph_id + block_ref.",
            path,
            "Provide block_id or graph_id + block_ref.",
        )

    return _omit_none({
        "op": op_name,
        "graph": change.get("graph_name"),
        "graph_id": graph_id,
        "block_ref": block_ref,
        "block_id": block_id,
        "missing_policy": change.get("missing_policy"),
        "already_user_owned_policy": change.get("already_user_owned_policy"),
    })


def _make_task_plan(task_spec: Dict[str, Any], steps: List[Dict[str, Any]]) -> Dict[str, Any]:
    target_asset = _target_asset_path(task_spec)
    validation = task_spec.get("validation", {})
    execution_policy = task_spec.get("execution_policy", {})
    return {
        "schema": TASK_PLAN_SCHEMA,
        "task_name": task_spec.get("feature_name"),
        "task_type": task_spec["task_type"],
        "context_id": task_spec.get("context_id"),
        "target_assets": [target_asset],
        "execution_policy": {
            "dry_run_mode": execution_policy.get("dry_run_mode", "full") if isinstance(execution_policy, dict) else "full",
            "should_compile": validation.get("should_compile", False) if isinstance(validation, dict) else False,
            "should_save": validation.get("should_save", False) if isinstance(validation, dict) else False,
        },
        "steps": steps,
    }


def _make_step(
    index: int,
    capability: str,
    asset_path: str,
    strategy: str,
    ops: List[Dict[str, Any]],
    constraints: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    step = {
        "step_id": f"step_{index:03d}",
        "capability": capability,
        "target": {
            "asset_path": asset_path,
        },
        "write": {
            "strategy": strategy,
            "ops": ops,
        },
    }
    if constraints:
        step["constraints"] = constraints
    return step


def _summarize_task_plan(task_plan: Dict[str, Any]) -> Dict[str, Any]:
    def summarize_step(step: Dict[str, Any]) -> Dict[str, Any]:
        summary = {
            "step_id": step["step_id"],
            "capability": step["capability"],
            "target": step["target"],
            "strategy": step["write"]["strategy"],
            "ops": len(step["write"]["ops"]),
        }
        if isinstance(step.get("depends_on"), list):
            summary["depends_on"] = list(step["depends_on"])
        return summary

    return {
        "schema": task_plan["schema"],
        "task_name": task_plan.get("task_name"),
        "task_type": task_plan["task_type"],
        "target_assets": task_plan["target_assets"],
        "steps": [
            summarize_step(step)
            for step in task_plan["steps"]
        ],
    }


def _assert_no_legacy_validation_fields(task_spec: Dict[str, Any]) -> None:
    validation = task_spec.get("validation", {})
    if isinstance(validation, dict) and ("compile" in validation or "save" in validation):
        raise TaskSpecCompileError(
            "unsupported_validation_fields",
            "Use validation.should_compile / validation.should_save; validation.compile / validation.save are not TaskSpec fields.",
            [_issue("unsupported_validation_fields", "validation", "Replace validation.compile/save with should_compile/should_save.")],
        )


def _target_asset_path(task_spec: Dict[str, Any]) -> str:
    target = _required_object(task_spec, "target", "target")
    return _required_string(target, "asset_path", "target.asset_path")


def _required_object(record: Dict[str, Any], field: str, path: str) -> Dict[str, Any]:
    value = record.get(field)
    if isinstance(value, dict):
        return value
    _raise("taskspec_semantic_invalid", f"{path} must be an object.", path, f"Provide {path}.")


def _required_object_value(value: Any, path: str) -> Dict[str, Any]:
    if isinstance(value, dict):
        return value
    _raise("taskspec_semantic_invalid", f"{path} must be an object.", path, f"Provide {path} as an object.")


def _required_non_empty_list(record: Dict[str, Any], field: str, path: str) -> List[Any]:
    value = record.get(field)
    if isinstance(value, list) and value:
        return value
    _raise("taskspec_semantic_invalid", f"{path} must be a non-empty list.", path, f"Provide at least one item in {path}.")


def _required_string(record: Dict[str, Any], field: str, path: str) -> str:
    value = record.get(field)
    if isinstance(value, str) and value.strip():
        return value
    _raise("taskspec_semantic_invalid", f"{path} must be a non-empty string.", path, f"Provide {path}.")


def _optional_string(record: Dict[str, Any], field: str) -> Optional[str]:
    value = record.get(field)
    return value if isinstance(value, str) and value.strip() else None


def _require_exact_string(record: Dict[str, Any], field: str, expected: str, path: str, code: str) -> None:
    actual = _required_string(record, field, path)
    if actual != expected:
        _raise(code, f"{path} must be {expected}.", path, f'Use {field}="{expected}".')


def _literal_value(value: Any) -> Any:
    if isinstance(value, dict) and value.get("kind") == "literal":
        return value.get("value")
    return value


def _raise(code: str, message: str, path: str, suggestion: str) -> None:
    raise TaskSpecCompileError(code, message, [_issue(code, path, suggestion)])


def _issue(code: str, path: str, message: str) -> Dict[str, str]:
    return {
        "code": code,
        "path": path,
        "message": message,
    }


def _omit_none(value: Dict[str, Any]) -> Dict[str, Any]:
    return {key: item for key, item in value.items() if item is not None}


def _omit_none_deep(value: Any) -> Any:
    if isinstance(value, dict):
        return {key: _omit_none_deep(item) for key, item in value.items() if item is not None}
    if isinstance(value, list):
        return [_omit_none_deep(item) for item in value]
    return value
