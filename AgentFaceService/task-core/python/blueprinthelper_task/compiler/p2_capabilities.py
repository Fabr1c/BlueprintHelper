from __future__ import annotations

from typing import Any, Dict, List, Optional

from ..shared.errors import TaskSpecCompileError


TASK_COMPILER_RESULT_SCHEMA = "BlueprintHelper.TaskCompilerResult.v1"
TASK_PLAN_SCHEMA = "BlueprintHelper.TaskPlan.v1"


P2_TASK_TYPES = {
    "edit_object_properties",
    "edit_blueprint_signature",
}


def supports_p2_task_type(task_type: Any) -> bool:
    return task_type in P2_TASK_TYPES


def compile_p2_task_spec(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    task_type = task_spec.get("task_type")
    if task_type == "edit_object_properties":
        task_plan = _compile_object_property_task_plan(task_spec)
    elif task_type == "edit_blueprint_signature":
        task_plan = _compile_blueprint_signature_task_plan(task_spec)
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


def _compile_blueprint_signature_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    _require_exact_string(
        behavior,
        "signature_strategy",
        "signature_edit",
        "behavior.signature_strategy",
        "unsupported_signature_strategy",
    )
    changes = _required_non_empty_list(behavior, "changes", "behavior.changes")
    steps = []
    for index, raw_change in enumerate(changes):
        path = f"behavior.changes[{index}]"
        change = _required_object_value(raw_change, path)
        op = _compile_blueprint_signature_op(change, path)
        steps.append(_make_step(
            index + 1,
            "blueprint_signature",
            _target_asset_path(task_spec),
            _blueprint_signature_strategy_for_op(op),
            [op],
        ))
    return _make_task_plan(task_spec, steps)


def _compile_blueprint_signature_op(change: Dict[str, Any], path: str) -> Dict[str, Any]:
    kind = _required_string(change, "kind", f"{path}.kind")
    if kind in {"ensure_function", "ensure_interface_function"}:
        return _omit_none({
            "op": "ensure_function",
            "function_name": _required_string(change, "function_name", f"{path}.function_name"),
            "interface_path": _required_string(change, "interface_path", f"{path}.interface_path")
                if kind == "ensure_interface_function" else _optional_string(change, "interface_path"),
            "interface_entry_kind": "function" if kind == "ensure_interface_function" else None,
            "inputs": change.get("inputs"),
            "outputs": change.get("outputs"),
            "is_pure": change.get("is_pure"),
            "name_collision_policy": _optional_string(change, "name_collision_policy") or "reuse_if_exists",
        })

    if kind in {"ensure_custom_event", "ensure_interface_event"}:
        return _omit_none({
            "op": "ensure_custom_event",
            "event_name": _required_string(change, "event_name", f"{path}.event_name"),
            "graph_name": _required_string(change, "graph_name", f"{path}.graph_name"),
            "interface_path": _required_string(change, "interface_path", f"{path}.interface_path")
                if kind == "ensure_interface_event" else _optional_string(change, "interface_path"),
            "interface_entry_kind": "event" if kind == "ensure_interface_event" else None,
            "inputs": change.get("inputs"),
            "name_collision_policy": _optional_string(change, "name_collision_policy") or "reuse_if_exists",
        })

    if kind == "ensure_event_dispatcher":
        return _omit_none({
            "op": "ensure_event_dispatcher",
            "dispatcher_name": _required_string(change, "dispatcher_name", f"{path}.dispatcher_name"),
            "inputs": change.get("inputs"),
            "name_collision_policy": _optional_string(change, "name_collision_policy") or "reuse_if_exists",
            "signature_mismatch_policy": _optional_string(change, "signature_mismatch_policy") or "block",
        })

    if kind == "ensure_override_event":
        return _omit_none({
            "op": "ensure_override_event",
            "event_name": _required_string(change, "event_name", f"{path}.event_name"),
            "event_kind": _optional_string(change, "event_kind") or "native_event",
            "graph_name": _optional_string(change, "graph_name"),
            "inputs": change.get("inputs"),
            "execute_policy": _optional_string(change, "execute_policy") or "blocked_preflight",
        })

    if kind == "remove_signature":
        signature_kind = _optional_string(change, "signature_kind") or _infer_remove_signature_kind(change)
        if change.get("require_reference_context") is False:
            _raise(
                "invalid_signature_remove_policy",
                f"{path}.require_reference_context must be true.",
                f"{path}.require_reference_context",
                "Signature removal requires reference context in this slice.",
            )
        return _omit_none({
            "op": "remove_signature",
            "signature_kind": signature_kind,
            "signature_name": _remove_signature_name(change, signature_kind, path),
            "graph_name": change.get("graph_name"),
            "execute_policy": _optional_string(change, "execute_policy") or "blocked_preflight",
            "require_reference_context": True,
        })

    _raise(
        "unsupported_signature_change",
        f"Unsupported signature change kind: {kind}",
        f"{path}.kind",
        "Use a supported signature change kind.",
    )


def _blueprint_signature_strategy_for_op(op: Dict[str, Any]) -> str:
    op_name = op.get("op")
    if op_name == "ensure_event_dispatcher":
        return "event_dispatcher_signature"
    if op_name == "ensure_override_event":
        return "override_event_signature"
    if op_name == "ensure_custom_event":
        return "custom_event_signature"
    if op_name == "remove_signature":
        signature_kind = op.get("signature_kind", "function")
        if signature_kind == "event_dispatcher":
            return "event_dispatcher_signature"
        if signature_kind in {"override_event", "native_event"}:
            return "override_event_signature"
        if signature_kind in {"custom_event", "interface_event"}:
            return "custom_event_signature"
    return "function_signature"


def _infer_remove_signature_kind(change: Dict[str, Any]) -> str:
    if _optional_string(change, "dispatcher_name"):
        return "event_dispatcher"
    if _optional_string(change, "event_name"):
        return "custom_event"
    return "function"


def _remove_signature_name(change: Dict[str, Any], signature_kind: str, path: str) -> str:
    signature_name = _optional_string(change, "signature_name")
    if signature_name:
        return signature_name
    if signature_kind in {"function", "interface_function"}:
        function_name = _optional_string(change, "function_name")
        if function_name:
            return function_name
    if signature_kind in {"custom_event", "interface_event", "override_event", "native_event"}:
        event_name = _optional_string(change, "event_name")
        if event_name:
            return event_name
    if signature_kind == "event_dispatcher":
        dispatcher_name = _optional_string(change, "dispatcher_name")
        if dispatcher_name:
            return dispatcher_name
    return _required_string(change, "signature_name", f"{path}.signature_name")


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
            "review_baseline_dirty_asset_policy": execution_policy.get("review_baseline_dirty_asset_policy", "block") if isinstance(execution_policy, dict) else "block",
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
