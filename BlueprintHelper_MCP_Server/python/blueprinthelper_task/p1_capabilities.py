from __future__ import annotations

from typing import Any, Dict, List

from .errors import TaskSpecCompileError


TASK_COMPILER_RESULT_SCHEMA = "BlueprintHelper.TaskCompilerResult.v1"
TASK_PLAN_SCHEMA = "BlueprintHelper.TaskPlan.v1"


P1_TASK_TYPES = {
    "create_asset",
    "edit_blueprint_components",
    "edit_blueprint_class_settings",
    "edit_umg_widget",
    "edit_data_table",
}


def supports_p1_task_type(task_type: Any) -> bool:
    return task_type in P1_TASK_TYPES


def compile_p1_task_spec(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    task_type = task_spec.get("task_type")
    if task_type == "create_asset":
        task_plan = _compile_asset_factory_task_plan(task_spec)
    elif task_type == "edit_blueprint_components":
        task_plan = _compile_component_task_plan(task_spec)
    elif task_type == "edit_blueprint_class_settings":
        task_plan = _compile_class_settings_task_plan(task_spec)
    elif task_type == "edit_umg_widget":
        task_plan = _compile_umg_widget_task_plan(task_spec)
    elif task_type == "edit_data_table":
        task_plan = _compile_data_table_task_plan(task_spec)
    else:
        raise TaskSpecCompileError(
            "unsupported_task_type",
            f"Unsupported P1 TaskSpec task_type: {task_type}",
            [_issue("unsupported_task_type", "task_type", "Use a supported BlueprintHelper P1 TaskSpec task_type.")],
        )

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": {
            "task_plan": _omit_none_deep(task_plan),
        },
        "task_plan_summary": _summarize_task_plan(task_plan),
    }


def _compile_asset_factory_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    _require_exact_string(
        behavior,
        "asset_strategy",
        "ensure_asset",
        "behavior.asset_strategy",
        "unsupported_asset_factory_strategy",
    )
    asset = behavior.get("asset") if isinstance(behavior.get("asset"), dict) else behavior
    op = {"op": "create_asset"}
    _copy_required_string(asset, op, "asset_type", "behavior.asset.asset_type")
    _copy_optional_string(asset, op, "parent_class", "behavior.asset.parent_class")
    _copy_optional_string(asset, op, "value_type", "behavior.asset.value_type")
    _copy_optional_string(asset, op, "collision", "behavior.asset.collision")
    _copy_optional_string_as(asset, op, "collision_policy", "collision", "behavior.asset.collision_policy")

    return _make_task_plan(task_spec, [
        _make_step(
            1,
            "asset_factory",
            _target_asset_path(task_spec),
            "asset_create",
            [op],
        ),
    ])


def _compile_component_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    _require_exact_string(
        behavior,
        "component_strategy",
        "component_tree",
        "behavior.component_strategy",
        "unsupported_blueprint_component_strategy",
    )
    components = _required_non_empty_list(behavior, "changes", "behavior.changes")

    steps = []
    planned_component_step_ids: Dict[str, str] = {}
    for index, raw_op in enumerate(components):
        path = f"behavior.changes[{index}]"
        op = _required_object_value(raw_op, path)
        change_kind = _required_string(op, "kind", f"{path}.kind")
        kind_to_op = {
            "ensure_component_present": "add_component",
            "configure_component": "set_component_properties",
            "remove_component": "remove_component",
        }
        op_name = kind_to_op.get(change_kind, change_kind)
        if op_name not in {"add_component", "set_component_properties", "remove_component"}:
            _raise(
                "unsupported_blueprint_component_op",
                f"Unsupported Blueprint Component change kind: {change_kind}",
                f"{path}.kind",
                "Use ensure_component_present, configure_component, or remove_component.",
            )

        component_name = _required_string(op, "name", f"{path}.name")
        compiled = {
            "op": op_name,
            "component_name": component_name,
        }
        if op_name == "add_component":
            _copy_required_string_as(op, compiled, "class", "component_class", f"{path}.class")
            attach = op.get("attach")
            if isinstance(attach, dict):
                _copy_optional_string_as(attach, compiled, "parent", "parent_component", f"{path}.attach.parent")
                _copy_optional_string_as(attach, compiled, "socket", "socket_name", f"{path}.attach.socket")
                _copy_optional_string_as(attach, compiled, "rule", "attach_rule", f"{path}.attach.rule")
            _copy_optional_string_as(op, compiled, "on_name_conflict", "name_collision_policy", f"{path}.on_name_conflict")
        elif op_name == "set_component_properties":
            compiled["settings"] = _validate_settings(op, "properties", f"{path}.properties")

        step = _make_step(
            index + 1,
            "blueprint_component",
            _target_asset_path(task_spec),
            "component_tree",
            [compiled],
        )
        if op_name == "add_component":
            planned_component_step_ids[component_name] = step["step_id"]
        elif op_name in {"set_component_properties", "remove_component"} and component_name in planned_component_step_ids:
            step["depends_on"] = [planned_component_step_ids[component_name]]
        steps.append(step)
    return _make_task_plan(task_spec, steps)


def _compile_class_settings_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    _require_exact_string(
        behavior,
        "class_settings_strategy",
        "class_settings",
        "behavior.class_settings_strategy",
        "unsupported_class_settings_strategy",
    )
    if "parent_class" in behavior:
        _raise(
            "unsupported_class_settings_parent_class_op",
            "Parent class changes are not supported by blueprint_class_settings.",
            "behavior.parent_class",
            "Use a future class signature capability instead.",
        )

    steps = []
    interfaces = behavior.get("interfaces")
    if isinstance(interfaces, dict):
        present = interfaces.get("ensure_present")
        if isinstance(present, list) and present:
            steps.append(_make_step(
                len(steps) + 1,
                "blueprint_class_settings",
                _target_asset_path(task_spec),
                "class_settings",
                [{
                    "op": "add_implemented_interfaces",
                    "interface_paths": _required_string_list(interfaces, "ensure_present", "behavior.interfaces.ensure_present"),
                }],
            ))
        absent = interfaces.get("ensure_absent")
        if isinstance(absent, list) and absent:
            steps.append(_make_step(
                len(steps) + 1,
                "blueprint_class_settings",
                _target_asset_path(task_spec),
                "class_settings",
                [{
                    "op": "remove_implemented_interfaces",
                    "interface_paths": _required_string_list(interfaces, "ensure_absent", "behavior.interfaces.ensure_absent"),
                }],
            ))

    defaults = behavior.get("class_defaults")
    if isinstance(defaults, list) and defaults:
        steps.append(_make_step(
            len(steps) + 1,
            "blueprint_class_settings",
            _target_asset_path(task_spec),
            "class_settings",
            [{
                "op": "set_class_default_properties",
                "settings": _validate_settings(behavior, "class_defaults", "behavior.class_defaults"),
            }],
        ))

    if not steps:
        _raise(
            "taskspec_semantic_invalid",
            "edit_blueprint_class_settings requires interfaces or class_defaults.",
            "behavior",
            "Provide behavior.interfaces or behavior.class_defaults.",
        )
    return _make_task_plan(task_spec, steps)


def _compile_umg_widget_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    widget_strategy = _required_string(behavior, "widget_strategy", "behavior.widget_strategy")
    if widget_strategy != "widget_blueprint_edit":
        _raise(
            "unsupported_umg_widget_strategy",
            "behavior.widget_strategy must be widget_blueprint_edit.",
            "behavior.widget_strategy",
            'Use widget_strategy="widget_blueprint_edit".',
        )
    widget_ops = _required_non_empty_list(behavior, "changes", "behavior.changes")

    steps = []
    for index, raw_op in enumerate(widget_ops):
        path = f"behavior.changes[{index}]"
        op = _required_object_value(raw_op, path)
        change_kind = _required_string(op, "kind", f"{path}.kind")
        if change_kind == "move_widget":
            _raise(
                "unsupported_umg_widget_move",
                "move_widget is not supported by the current UMG Widget TaskPlan adapter.",
                f"{path}.kind",
                "Split this into a later UMG move_widget capability.",
            )
        kind_to_op = {
            "create_widget": "add_widget",
            "update_widget_property": "set_widget_property",
            "delete_widget": "remove_widget",
        }
        op_name = kind_to_op.get(change_kind, change_kind)
        if op_name not in {"add_widget", "set_widget_property", "remove_widget"}:
            _raise(
                "unsupported_umg_widget_op",
                f"Unsupported UMG widget change kind: {change_kind}",
                f"{path}.kind",
                "Use create_widget, update_widget_property, or delete_widget.",
            )

        compiled = {"op": op_name}
        if op_name == "add_widget":
            strategy = "widget_tree_edit"
            _copy_required_string(op, compiled, "widget_class", f"{path}.widget_class")
            _copy_required_string(op, compiled, "widget_name", f"{path}.widget_name")
            for field in ["parent_widget_name", "parent_name"]:
                _copy_optional_string(op, compiled, field, f"{path}.{field}")
        elif op_name == "set_widget_property":
            strategy = "widget_property_edit"
            _copy_required_string(op, compiled, "widget_name", f"{path}.widget_name")
            if "property_name" in op:
                _copy_required_string(op, compiled, "property_name", f"{path}.property_name")
            else:
                _copy_required_string(op, compiled, "property_path", f"{path}.property_path")
            if "value" not in op:
                _raise("taskspec_semantic_invalid", "set_widget_property requires value.", f"{path}.value", "Provide a non-null value.")
            compiled["value"] = _literal_value(op["value"])
        else:
            strategy = "widget_tree_edit"
            _copy_required_string(op, compiled, "widget_name", f"{path}.widget_name")

        steps.append(_make_step(
            index + 1,
            "umg_widget",
            _target_asset_path(task_spec),
            strategy,
            [compiled],
        ))
    return _make_task_plan(task_spec, steps)


def _compile_data_table_task_plan(task_spec: Dict[str, Any]) -> Dict[str, Any]:
    _assert_no_legacy_validation_fields(task_spec)
    behavior = _required_object(task_spec, "behavior", "behavior")
    _require_exact_string(
        behavior,
        "row_strategy",
        "row_edit",
        "behavior.row_strategy",
        "unsupported_data_table_strategy",
    )
    rows = _required_non_empty_list(behavior, "rows", "behavior.rows")

    steps = []
    for index, raw_op in enumerate(rows):
        path = f"behavior.rows[{index}]"
        op = _required_object_value(raw_op, path)
        op_name = _read_data_table_op_name(op, path)
        if op_name not in {"add_row", "update_row", "delete_row"}:
            _raise(
                "unsupported_data_table_op",
                f"Unsupported DataTable row op: {op_name}",
                f"{path}.op",
                "Use add_row, update_row, or delete_row.",
            )

        compiled = {"op": op_name}
        _copy_required_string(op, compiled, "row_name", f"{path}.row_name")
        if op_name == "update_row":
            compiled["fields"] = _required_non_empty_object(op, "fields", f"{path}.fields")
        elif op_name == "add_row" and isinstance(op.get("fields"), dict):
            compiled["fields"] = dict(op["fields"])

        steps.append(_make_step(
            index + 1,
            "data_table",
            _target_asset_path(task_spec),
            "row_edit",
            [compiled],
        ))
    return _make_task_plan(task_spec, steps)


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


def _make_step(index: int, capability: str, asset_path: str, strategy: str, ops: List[Dict[str, Any]]) -> Dict[str, Any]:
    return {
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


def _required_non_empty_object(record: Dict[str, Any], field: str, path: str) -> Dict[str, Any]:
    value = record.get(field)
    if isinstance(value, dict) and value:
        return dict(value)
    _raise("taskspec_semantic_invalid", f"{path} must be a non-empty object.", path, f"Provide {path}.")


def _required_non_empty_list(record: Dict[str, Any], field: str, path: str) -> List[Any]:
    value = record.get(field)
    if isinstance(value, list) and value:
        return value
    _raise("taskspec_semantic_invalid", f"{path} must be a non-empty list.", path, f"Provide at least one item in {path}.")


def _required_string_list(record: Dict[str, Any], field: str, path: str) -> List[str]:
    values = _required_non_empty_list(record, field, path)
    out = []
    for index, value in enumerate(values):
        if not isinstance(value, str) or not value.strip():
            _raise("taskspec_semantic_invalid", f"{path}[{index}] must be a non-empty string.", f"{path}[{index}]", "Provide a valid path string.")
        out.append(value)
    return out


def _required_string(record: Dict[str, Any], field: str, path: str) -> str:
    value = record.get(field)
    if isinstance(value, str) and value.strip():
        return value
    _raise("taskspec_semantic_invalid", f"{path} must be a non-empty string.", path, f"Provide {path}.")


def _require_exact_string(record: Dict[str, Any], field: str, expected: str, path: str, code: str) -> None:
    actual = _required_string(record, field, path)
    if actual != expected:
        _raise(code, f"{path} must be {expected}.", path, f'Use {field}="{expected}".')


def _copy_required_string(source: Dict[str, Any], destination: Dict[str, Any], field: str, path: str) -> None:
    destination[field] = _required_string(source, field, path)


def _copy_required_string_as(
    source: Dict[str, Any],
    destination: Dict[str, Any],
    source_field: str,
    destination_field: str,
    path: str,
) -> None:
    destination[destination_field] = _required_string(source, source_field, path)


def _copy_optional_string(source: Dict[str, Any], destination: Dict[str, Any], field: str, path: str) -> None:
    if field not in source:
        return
    value = source[field]
    if not isinstance(value, str):
        _raise("taskspec_semantic_invalid", f"{path} must be a string when present.", path, "Use a string value.")
    if value:
        destination[field] = value


def _copy_optional_string_as(
    source: Dict[str, Any],
    destination: Dict[str, Any],
    source_field: str,
    destination_field: str,
    path: str,
) -> None:
    if source_field not in source:
        return
    value = source[source_field]
    if not isinstance(value, str):
        _raise("taskspec_semantic_invalid", f"{path} must be a string when present.", path, "Use a string value.")
    if value:
        destination[destination_field] = value


def _read_data_table_op_name(op: Dict[str, Any], path: str) -> str:
    if "op" in op:
        _raise(
            "unsupported_data_table_op_field",
            "DataTable TaskSpec rows use action, not op.",
            f"{path}.op",
            "Replace op with action: add, update, or delete.",
        )
    action = _required_string(op, "action", f"{path}.action")
    action_to_op = {
        "add": "add_row",
        "update": "update_row",
        "delete": "delete_row",
    }
    if action not in action_to_op:
        _raise(
            "unsupported_data_table_action",
            f"Unsupported DataTable row action: {action}",
            f"{path}.action",
            "Use action: add, update, or delete.",
        )
    return action_to_op[action]


def _literal_value(value: Any) -> Any:
    if isinstance(value, dict) and value.get("kind") == "literal":
        return value.get("value")
    return value


def _validate_settings(record: Dict[str, Any], field: str, path: str) -> List[Dict[str, Any]]:
    raw_settings = _required_non_empty_list(record, field, path)
    settings = []
    for index, raw_setting in enumerate(raw_settings):
        setting_path = f"{path}[{index}]"
        setting = _required_object_value(raw_setting, setting_path)
        _required_string(setting, "property_path", f"{setting_path}.property_path")
        if "value" not in setting:
            _raise("taskspec_semantic_invalid", f"{setting_path}.value is required.", f"{setting_path}.value", "Provide value.")
        settings.append(dict(setting))
    return settings


def _raise(code: str, message: str, path: str, suggestion: str) -> None:
    raise TaskSpecCompileError(code, message, [_issue(code, path, suggestion)])


def _issue(code: str, path: str, message: str) -> Dict[str, str]:
    return {
        "code": code,
        "path": path,
        "message": message,
    }


def _omit_none_deep(value: Any) -> Any:
    if isinstance(value, dict):
        return {key: _omit_none_deep(item) for key, item in value.items() if item is not None}
    if isinstance(value, list):
        return [_omit_none_deep(item) for item in value]
    return value
