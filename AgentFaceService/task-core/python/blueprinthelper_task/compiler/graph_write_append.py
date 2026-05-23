from __future__ import annotations

import json
import re
from typing import Any, Dict, List, Optional

from ..shared.errors import TaskSpecCompileError
from .p1_capabilities import compile_p1_task_spec, supports_p1_task_type
from .p2_capabilities import compile_p2_task_spec, supports_p2_task_type


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
    ops = _compile_graph_write_ops(task_spec["behavior"])

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
            "review_baseline_dirty_asset_policy": execution_policy.get("review_baseline_dirty_asset_policy", "block"),
        },
        "steps": _make_graph_write_task_plan_steps(task_spec, ops),
    }

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": (
            task_plan_to_append_bridge_payload(task_plan, dry_run)
            if task_spec["behavior"]["graph_strategy"] == "append_new_owned_graph"
            else {"task_plan": _omit_none_deep(task_plan)}
        ),
        "task_plan_summary": summarize_task_plan(task_plan),
    }


def _make_graph_write_task_plan_steps(task_spec: Dict[str, Any], ops: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    target = task_spec["target"]
    scope_policy = task_spec["scope_policy"]
    strategy = task_spec["behavior"]["graph_strategy"]
    if strategy == "append_new_owned_graph":
        signature_steps = [
            {
                "step_id": f"step_{index + 1:03d}",
                "capability": "blueprint_signature",
                "target": {
                    "asset_path": target["asset_path"],
                },
                "write": {
                    "strategy": "custom_event_signature",
                    "ops": [{
                        "op": "ensure_custom_event",
                        "event_name": op["name"],
                        "graph_name": scope_policy["graph_name"],
                        "name_collision_policy": "reuse_if_exists",
                    }],
                },
            }
            for index, op in enumerate(ops)
        ]
        graph_write_step = {
            "step_id": f"step_{len(signature_steps) + 1:03d}",
            "capability": "graph_write",
            "target": {
                "asset_path": target["asset_path"],
                "graph": scope_policy["graph_name"],
            },
            "write": {
                "strategy": "owned_graph_edit",
                "ops": [_strip_graph_write_compiler_metadata(op) for op in ops],
            },
            "constraints": {
                "allow_modify_user_nodes": scope_policy["allow_modify_user_nodes"],
                "ownership_scope": "blueprinthelper_owned",
            },
            "depends_on": [step["step_id"] for step in signature_steps],
        }
        return [*signature_steps, graph_write_step]

    if strategy == "replace_owned_graph" and len(ops) == 1 and isinstance(ops[0].get("__signature_split"), dict):
        signature_op = ops[0]["__signature_split"]
        graph_write_step = {
            "step_id": "step_002",
            "capability": "graph_write",
            "target": {
                "asset_path": target["asset_path"],
                "graph": scope_policy["graph_name"],
            },
            "write": {
                "strategy": "owned_graph_edit",
                "ops": [_strip_graph_write_compiler_metadata(ops[0])],
            },
            "constraints": {
                "allow_modify_user_nodes": scope_policy["allow_modify_user_nodes"],
                "ownership_scope": "blueprinthelper_owned",
            },
            "depends_on": ["step_001"],
        }
        signature_step = {
            "step_id": "step_001",
            "capability": "blueprint_signature",
            "target": {
                "asset_path": target["asset_path"],
            },
            "write": {
                "strategy": "custom_event_signature",
                "ops": [_omit_none({
                    "op": signature_op["op"],
                    "event_name": signature_op["event_name"],
                    "graph_name": scope_policy["graph_name"],
                    "inputs": signature_op.get("inputs"),
                    "name_collision_policy": signature_op.get("name_collision_policy", "reuse_if_exists"),
                })],
            },
        }
        return [signature_step, graph_write_step]

    op_batches = [ops] if strategy == "append_new_owned_graph" else [[op] for op in ops]
    return [
        {
            "step_id": f"step_{index + 1:03d}",
            "capability": "graph_write",
            "target": {
                "asset_path": target["asset_path"],
                "graph": scope_policy["graph_name"],
            },
            "write": {
                "strategy": "owned_graph_edit",
                "ops": [_strip_graph_write_compiler_metadata(op) for op in batch],
            },
            "constraints": {
                "allow_modify_user_nodes": scope_policy["allow_modify_user_nodes"],
                "ownership_scope": "blueprinthelper_owned",
            },
        }
        for index, batch in enumerate(op_batches)
    ]


def _strip_graph_write_compiler_metadata(op: Dict[str, Any]) -> Dict[str, Any]:
    return {
        key: value
        for key, value in op.items()
        if key != "__signature_split"
    }


def compile_task_spec(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    task_type = task_spec.get("task_type")
    if task_type == "create_blueprint_feature":
        return compile_blueprint_feature(task_spec, dry_run)
    if task_type == "edit_blueprint_graph":
        return compile_graph_write_append(task_spec, dry_run)
    if task_type == "edit_blueprint_variables":
        return compile_blueprint_variables(task_spec, dry_run)
    if supports_p1_task_type(task_type):
        return compile_p1_task_spec(task_spec, dry_run)
    if supports_p2_task_type(task_type):
        return compile_p2_task_spec(task_spec, dry_run)

    raise TaskSpecCompileError(
        "unsupported_task_type",
        f"Unsupported TaskSpec task_type: {task_type}",
        [{
            "code": "unsupported_task_type",
            "path": "task_type",
            "message": "Use a supported BlueprintHelper TaskSpec task_type.",
        }],
    )


def compile_blueprint_feature(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    _assert_supported_composite_blueprint_feature_task_spec(task_spec)

    steps: List[Dict[str, Any]] = []
    steps.extend(_compile_composite_component_steps(task_spec, dry_run))
    steps.extend(_compile_composite_variable_steps(task_spec, dry_run))
    steps.extend(_compile_composite_class_settings_steps(task_spec, dry_run))
    steps.extend(_compile_composite_graph_write_steps(task_spec, dry_run))
    steps.extend(_compile_composite_integration_steps(task_spec, dry_run))

    if not steps:
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            "create_blueprint_feature did not produce any TaskPlan steps.",
            [{
                "code": "empty_composite_feature",
                "path": "task_spec",
                "message": "Provide components, variables, class_settings, or behavior.",
            }],
        )

    execution_policy = task_spec.get("execution_policy", {})
    validation = task_spec.get("validation", {})
    task_plan = {
        "schema": TASK_PLAN_SCHEMA,
        "task_name": task_spec.get("feature_name"),
        "task_type": task_spec["task_type"],
        "context_id": task_spec.get("context_id"),
        "target_assets": [_target_asset_path(task_spec)],
        "execution_policy": {
            "dry_run_mode": execution_policy.get("dry_run_mode", "full") if isinstance(execution_policy, dict) else "full",
            "should_compile": validation.get("should_compile", False) if isinstance(validation, dict) else False,
            "should_save": validation.get("should_save", False) if isinstance(validation, dict) else False,
            "review_baseline_dirty_asset_policy": execution_policy.get("review_baseline_dirty_asset_policy", "block") if isinstance(execution_policy, dict) else "block",
        },
        "steps": _renumber_steps(steps),
    }

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": {
            "task_plan": _omit_none_deep(task_plan),
        },
        "task_plan_summary": summarize_task_plan(task_plan),
    }


def compile_blueprint_variables(task_spec: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    _assert_supported_blueprint_variables_task_spec(task_spec)

    target = task_spec["target"]
    behavior = task_spec["behavior"]
    execution_policy = task_spec.get("execution_policy", {})
    validation = task_spec.get("validation", {})
    steps = _compile_blueprint_variable_steps(target, behavior)

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
            "review_baseline_dirty_asset_policy": execution_policy.get("review_baseline_dirty_asset_policy", "block"),
        },
        "steps": steps,
    }

    return {
        "schema": TASK_COMPILER_RESULT_SCHEMA,
        "task_plan": _omit_none_deep(task_plan),
        "bridge_payload": (
            _blueprint_variable_bridge_payload(task_plan, task_plan["steps"][0], dry_run)
            if len(task_plan["steps"]) == 1
            else {"task_plan": _omit_none_deep(task_plan)}
        ),
        "task_plan_summary": summarize_task_plan(task_plan),
    }


def _assert_supported_composite_blueprint_feature_task_spec(task_spec: Dict[str, Any]) -> None:
    _assert_no_legacy_validation_fields(task_spec)
    integration = task_spec.get("integration")
    if isinstance(integration, dict) and integration:
        unsupported_keys = [key for key in integration.keys() if key != "interface"]
        if unsupported_keys:
            raise TaskSpecCompileError(
                "unsupported_composite_integration",
                "Unsupported composite integration fields.",
                [{
                    "code": "unsupported_composite_integration",
                    "path": f"integration.{unsupported_keys[0]}",
                    "message": "Input binding and other integration fields need dedicated capability clusters; keep only integration.interface for this slice.",
                }],
            )
        if not isinstance(integration.get("interface"), dict):
            raise TaskSpecCompileError(
                "unsupported_composite_integration",
                "Unsupported composite integration fields.",
                [{
                    "code": "unsupported_composite_integration",
                    "path": "integration",
                    "message": "integration currently supports only an interface object.",
                }],
            )

    if integration is not None and not isinstance(integration, dict):
        raise TaskSpecCompileError(
            "unsupported_composite_integration",
            "Unsupported composite integration fields.",
            [{
                "code": "unsupported_composite_integration",
                "path": "integration",
                "message": "integration currently supports only an interface object.",
            }],
        )

    scope_policy = task_spec.get("scope_policy")
    if isinstance(scope_policy, dict) and scope_policy.get("allow_create_assets") is True:
        raise TaskSpecCompileError(
            "unsupported_composite_asset_creation",
            "Composite asset creation is not supported in this slice.",
            [{
                "code": "unsupported_composite_asset_creation",
                "path": "scope_policy.allow_create_assets",
                "message": "Set allow_create_assets=false and reference existing assets, or split asset creation into create_asset TaskSpecs.",
            }],
        )


def _compile_composite_component_steps(task_spec: Dict[str, Any], dry_run: bool) -> List[Dict[str, Any]]:
    components = task_spec.get("components")
    if not isinstance(components, list) or not components:
        return []

    asset_policy = task_spec.get("asset_policy") if isinstance(task_spec.get("asset_policy"), dict) else {}
    name_collision_policy = _normalize_component_collision_policy(asset_policy.get("if_component_exists"))
    changes: List[Dict[str, Any]] = []
    for index, raw_component in enumerate(components):
        path = f"components[{index}]"
        if not isinstance(raw_component, dict):
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "Component entry must be an object.",
                [{"code": "invalid_component", "path": path, "message": "Component entry must be an object."}],
            )
        component = raw_component
        add_change = _omit_none({
            "kind": "ensure_component_present",
            "name": _required_string(component, "name", f"{path}.name"),
            "class": _required_string(component, "class", f"{path}.class"),
            "attach": _composite_component_attach(component),
            "on_name_conflict": (
                _normalize_component_collision_policy(component.get("on_name_conflict"))
                or name_collision_policy
            ),
        })
        changes.append(add_change)

        settings = _composite_settings_array(component.get("properties"), f"{path}.properties", task_spec)
        if settings:
            changes.append({
                "kind": "configure_component",
                "name": add_change["name"],
                "properties": settings,
            })

    sub_spec = _composite_sub_spec(task_spec, "edit_blueprint_components", {
        "component_strategy": "component_tree",
        "changes": changes,
    })
    return compile_p1_task_spec(sub_spec, dry_run)["task_plan"]["steps"]


def _compile_composite_variable_steps(task_spec: Dict[str, Any], dry_run: bool) -> List[Dict[str, Any]]:
    variables = task_spec.get("variables")
    if not isinstance(variables, list) or not variables:
        return []

    variable_changes: List[Dict[str, Any]] = []
    default_changes: List[Dict[str, Any]] = []
    for index, raw_variable in enumerate(variables):
        path = f"variables[{index}]"
        if not isinstance(raw_variable, dict):
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "Variable entry must be an object.",
                [{"code": "invalid_variable", "path": path, "message": "Variable entry must be an object."}],
            )
        variable = raw_variable
        name = _required_string(variable, "name", f"{path}.name")
        variable_changes.append(_omit_none({
            "kind": "ensure_member_variable",
            "name": name,
            "pin_type": _composite_variable_pin_type(variable, path),
            "category": variable.get("category"),
            "tooltip": variable.get("tooltip"),
            "flags": variable.get("flags"),
            "metadata": variable.get("metadata"),
        }))
        if "default" in variable:
            default_changes.append({
                "kind": "set_member_default",
                "name": name,
                "value": _literal_value(variable.get("default")),
            })

    steps: List[Dict[str, Any]] = []
    if variable_changes:
        steps.extend(compile_blueprint_variables(_composite_sub_spec(task_spec, "edit_blueprint_variables", {
            "variable_strategy": "member_variables",
            "changes": variable_changes,
        }), dry_run)["task_plan"]["steps"])
    if default_changes:
        steps.extend(compile_blueprint_variables(_composite_sub_spec(task_spec, "edit_blueprint_variables", {
            "variable_strategy": "member_defaults",
            "defaults": default_changes,
        }), dry_run)["task_plan"]["steps"])
    return steps


def _compile_composite_class_settings_steps(task_spec: Dict[str, Any], dry_run: bool) -> List[Dict[str, Any]]:
    class_settings = task_spec.get("class_settings")
    if not isinstance(class_settings, dict):
        return []

    behavior: Dict[str, Any] = {
        "class_settings_strategy": "class_settings",
    }
    interfaces = class_settings.get("implemented_interfaces")
    if isinstance(interfaces, list) and interfaces:
        behavior["interfaces"] = {
            "ensure_present": [
                _resolve_composite_reference(value, task_spec)
                for value in interfaces
            ],
        }

    class_defaults = _composite_settings_array(class_settings.get("class_defaults"), "class_settings.class_defaults", task_spec)
    if class_defaults:
        behavior["class_defaults"] = class_defaults

    if len(behavior) == 1:
        return []
    return compile_p1_task_spec(_composite_sub_spec(task_spec, "edit_blueprint_class_settings", behavior), dry_run)["task_plan"]["steps"]


def _compile_composite_graph_write_steps(task_spec: Dict[str, Any], dry_run: bool) -> List[Dict[str, Any]]:
    behavior = task_spec.get("behavior")
    if not isinstance(behavior, dict):
        return []

    scope_policy = task_spec.get("scope_policy")
    if not isinstance(scope_policy, dict):
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            "create_blueprint_feature behavior requires scope_policy.",
            [{"code": "missing_scope_policy", "path": "scope_policy", "message": "Provide scope_policy.graph_name and allow_modify_user_nodes."}],
        )

    graph_spec = _composite_sub_spec(task_spec, "edit_blueprint_graph", behavior)
    graph_spec["scope_policy"] = {
        "graph_name": _required_string(scope_policy, "graph_name", "scope_policy.graph_name"),
        "allow_modify_user_nodes": scope_policy.get("allow_modify_user_nodes") is True,
    }
    return compile_graph_write_append(graph_spec, dry_run)["task_plan"]["steps"]


def _compile_composite_integration_steps(task_spec: Dict[str, Any], dry_run: bool) -> List[Dict[str, Any]]:
    integration = task_spec.get("integration")
    if not isinstance(integration, dict):
        return []
    interface_integration = integration.get("interface")
    if not isinstance(interface_integration, dict):
        return []

    interface_path = _resolve_composite_reference(
        _required_string(interface_integration, "interface_asset", "integration.interface.interface_asset"),
        task_spec,
    )
    function_name = _required_string(interface_integration, "function", "integration.interface.function")

    steps: List[Dict[str, Any]] = []
    class_settings_step_id = None
    if not _composite_class_settings_contains_interface(task_spec, interface_path):
        class_settings_step = _make_capability_step(
            len(steps) + 1,
            "blueprint_class_settings",
            _target_asset_path(task_spec),
            "class_settings",
            [{
                "op": "add_implemented_interfaces",
                "interface_paths": [interface_path],
            }],
        )
        class_settings_step_id = class_settings_step["step_id"]
        steps.append(class_settings_step)

    signature_step = _make_capability_step(
        len(steps) + 1,
        "blueprint_signature",
        _target_asset_path(task_spec),
        "function_signature",
        [{
            "op": "ensure_function",
            "function_name": function_name,
            "interface_path": interface_path,
            "name_collision_policy": "reuse_if_exists",
        }],
    )
    if class_settings_step_id:
        signature_step["depends_on"] = [class_settings_step_id]
    steps.append(signature_step)

    steps.append({
        "step_id": f"step_{len(steps) + 1:03d}",
        "capability": "graph_write",
        "target": {
            "asset_path": _target_asset_path(task_spec),
            "graph": function_name,
        },
        "write": {
            "strategy": "owned_graph_edit",
            "ops": [
                {
                    "op": "replace_body",
                    "replace_scope": "function_body",
                    "selector": {
                        "function_name": function_name,
                    },
                    "replacement": _compile_composite_interface_implementation_replacement(interface_integration, function_name),
                    "options": {
                        "strict": True,
                        "preserve_layout": False,
                    },
                },
            ],
        },
        "constraints": {
            "allow_modify_user_nodes": False,
            "ownership_scope": "blueprinthelper_owned",
        },
        "depends_on": [signature_step["step_id"]],
    })
    return steps


def _make_capability_step(index: int, capability: str, asset_path: str, strategy: str, ops: List[Dict[str, Any]]) -> Dict[str, Any]:
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


def _composite_class_settings_contains_interface(task_spec: Dict[str, Any], interface_path: str) -> bool:
    class_settings = task_spec.get("class_settings")
    if not isinstance(class_settings, dict):
        return False
    interfaces = class_settings.get("implemented_interfaces")
    if not isinstance(interfaces, list):
        return False
    return any(
        isinstance(value, str) and _resolve_composite_reference(value, task_spec) == interface_path
        for value in interfaces
    )


def _compile_composite_interface_implementation_replacement(
    interface_integration: Dict[str, Any],
    function_name: str,
) -> Dict[str, Any]:
    body = _composite_interface_implementation_body(interface_integration, function_name)
    _validate_supported_statements(body["statements"], "integration.interface.implementation.body.statements")
    return _compile_logic_body_to_import_payload(
        body,
        f"interface_{function_name}",
        "integration.interface.implementation.body",
    )


def _composite_interface_implementation_body(
    interface_integration: Dict[str, Any],
    function_name: str,
) -> Dict[str, List[Dict[str, Any]]]:
    implementation = _required_object(interface_integration, "implementation", "integration.interface.implementation")
    if isinstance(implementation.get("body"), dict):
        return _required_logic_body(implementation, "body", "integration.interface.implementation.body")
    if isinstance(implementation.get("statements"), list):
        return {"statements": implementation["statements"]}
    if isinstance(implementation.get("call"), str) and implementation["call"].strip():
        return {
            "statements": [
                {
                    "kind": "call",
                    "target": implementation["call"],
                    "args": implementation.get("args"),
                },
            ],
        }
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        "integration.interface.implementation must provide call, body, or statements.",
        [{
            "code": "missing_interface_implementation",
            "path": "integration.interface.implementation",
            "message": f"Provide implementation.call=\"{function_name}\" target logic or a BlueprintLogicSpec body.",
        }],
    )


def _composite_sub_spec(task_spec: Dict[str, Any], task_type: str, behavior: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "schema": task_spec.get("schema", "BlueprintHelper.TaskSpec.v1"),
        "context_id": task_spec.get("context_id"),
        "task_type": task_type,
        "feature_name": task_spec.get("feature_name"),
        "target": task_spec.get("target"),
        "behavior": behavior,
        "execution_policy": task_spec.get("execution_policy", {}),
        "validation": task_spec.get("validation", {}),
    }


def _renumber_steps(steps: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    old_ids = [step.get("step_id") for step in steps]
    new_ids = [f"step_{index + 1:03d}" for index, _ in enumerate(steps)]
    out = []
    for index, step in enumerate(steps):
        copied = dict(step)
        copied["step_id"] = new_ids[index]
        depends_on = copied.get("depends_on")
        if isinstance(depends_on, list):
            mapped_depends_on = []
            for step_id in depends_on:
                mapped_id = step_id
                for candidate in range(index - 1, -1, -1):
                    if old_ids[candidate] == step_id:
                        mapped_id = new_ids[candidate]
                        break
                mapped_depends_on.append(mapped_id)
            copied["depends_on"] = mapped_depends_on
        out.append(copied)
    return out


def _composite_component_attach(component: Dict[str, Any]) -> Dict[str, Any] | None:
    attach: Dict[str, Any] = {}
    raw_attach = component.get("attach")
    if isinstance(raw_attach, dict):
        attach.update(raw_attach)
    if isinstance(component.get("attach_to"), str) and component["attach_to"]:
        attach["parent"] = component["attach_to"]
    if isinstance(component.get("attach_rule"), str) and component["attach_rule"]:
        attach["rule"] = component["attach_rule"]
    return attach or None


def _normalize_component_collision_policy(value: Any) -> str | None:
    if value in {"reuse_if_type_matches", "reuse_if_exists"}:
        return "reuse_if_exists"
    if value == "fail_if_exists":
        return "fail_if_exists"
    return None


def _composite_variable_pin_type(variable: Dict[str, Any], path: str) -> Dict[str, Any]:
    if isinstance(variable.get("pin_type"), dict):
        return variable["pin_type"]
    if isinstance(variable.get("variable_type"), dict):
        return variable["variable_type"]
    if isinstance(variable.get("type"), str) and variable["type"].strip():
        return {"category": variable["type"]}
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        "Blueprint variable type is required.",
        [{
            "code": "missing_variable_pin_type",
            "path": f"{path}.type",
            "message": 'Provide type or variable_type, for example {"category":"bool"}.',
        }],
    )


def _composite_settings_array(raw_settings: Any, path: str, task_spec: Dict[str, Any]) -> List[Dict[str, Any]]:
    if raw_settings is None:
        return []
    if isinstance(raw_settings, list):
        settings = []
        for index, raw_setting in enumerate(raw_settings):
            setting_path = f"{path}[{index}]"
            if not isinstance(raw_setting, dict):
                raise TaskSpecCompileError(
                    "taskspec_semantic_invalid",
                    "Property setting must be an object.",
                    [{"code": "invalid_property_setting", "path": setting_path, "message": "Use {property_path, value}."}],
            )
            property_path = _required_string(raw_setting, "property_path", f"{setting_path}.property_path")
            if "value" not in raw_setting:
                raise TaskSpecCompileError(
                    "taskspec_semantic_invalid",
                    "Property setting requires value.",
                    [{"code": "missing_property_value", "path": f"{setting_path}.value", "message": "Provide value."}],
                )
            settings.append({
                **raw_setting,
                "property_path": property_path,
                "value": _resolve_composite_value(raw_setting.get("value"), task_spec),
            })
        return settings
    if isinstance(raw_settings, dict):
        return [
            {
                "property_path": key,
                "value": _resolve_composite_value(value, task_spec),
            }
            for key, value in raw_settings.items()
        ]
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path} must be an object or array.",
        [{"code": "invalid_property_settings", "path": path, "message": "Use an object map or a settings array."}],
    )


def _resolve_composite_value(value: Any, task_spec: Dict[str, Any]) -> Any:
    if isinstance(value, str):
        return _resolve_composite_reference(value, task_spec)
    if isinstance(value, list):
        return [_resolve_composite_value(item, task_spec) for item in value]
    if isinstance(value, dict):
        if value.get("kind") == "literal":
            return _literal_value(value)
        return {
            key: _resolve_composite_value(item, task_spec)
            for key, item in value.items()
        }
    return value


def _resolve_composite_reference(value: Any, task_spec: Dict[str, Any]) -> Any:
    if not isinstance(value, str) or not value.startswith("$resources."):
        return value
    resources = task_spec.get("resources")
    if not isinstance(resources, dict):
        return value
    cursor: Any = resources
    for segment in value[len("$resources."):].split("."):
        if not isinstance(cursor, dict) or segment not in cursor:
            return value
        cursor = cursor[segment]
    if isinstance(cursor, str):
        return cursor
    if isinstance(cursor, dict) and isinstance(cursor.get("asset_path"), str):
        return cursor["asset_path"]
    return value


def task_plan_to_append_bridge_payload(task_plan: Dict[str, Any], dry_run: bool) -> Dict[str, Any]:
    steps = task_plan.get("steps", [])
    append_step = next(
        (
            step for step in steps
            if isinstance(step, dict)
            and (step.get("capability") == "graph_write" or step.get("operation") == "append_blueprint_graph")
        ),
        None,
    )
    if not append_step:
        raise TaskSpecCompileError(
            "unsupported_taskplan_operation",
            "TaskPlan does not contain an append_blueprint_graph step.",
            [{
                "code": "unsupported_taskplan_operation",
                "path": "steps",
                "message": "TaskPlan requires a GraphWrite append step.",
            }],
        )

    if append_step.get("capability") == "graph_write":
        return _graph_write_taskplan_to_append_bridge_payload(task_plan, append_step, dry_run)

    if append_step.get("operation") != "append_blueprint_graph":
        raise TaskSpecCompileError(
            "unsupported_taskplan_operation",
            "TaskPlan does not contain an append_blueprint_graph step.",
            [{
                "code": "unsupported_taskplan_operation",
                "path": "steps[0].operation",
                "message": "Only append_blueprint_graph lowering adapter TaskPlan steps are supported in the first MCP slice.",
            }],
        )

    args = append_step["args"]
    return _omit_none({
        "target": {
            "asset_path": append_step["target"]["asset_path"],
            "graph": append_step["target"]["graph"],
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
    if step.get("capability") and isinstance(step.get("write"), dict):
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
    if step.get("capability") == "blueprint_signature":
        _validate_blueprint_signature_step(step, path)
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
        if not isinstance(args.get("logic_spec"), dict):
            _raise_taskplan_invalid(
                "logic_spec_required",
                "append_blueprint_graph requires args.logic_spec/SemanticIR.",
                f"{path}.args.logic_spec",
            )
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


def _validate_blueprint_signature_step(step: Dict[str, Any], path: str) -> None:
    if "operation" in step:
        _raise_taskplan_invalid(
            "unsupported_signature_operation_field",
            "blueprint_signature IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details.",
            f"{path}.operation",
        )

    target = step.get("target")
    if not isinstance(target, dict):
        _raise_taskplan_invalid("missing_taskplan_step_target", "TaskPlan step requires target.", f"{path}.target")
    _required_string(target, "asset_path", f"{path}.target.asset_path")

    write = step.get("write")
    if not isinstance(write, dict):
        _raise_taskplan_invalid("missing_signature_write", "Blueprint signature TaskPlan step requires write.", f"{path}.write")
    strategy = _required_string(write, "strategy", f"{path}.write.strategy")
    if strategy not in (
        "function_signature",
        "custom_event_signature",
        "event_dispatcher_signature",
        "override_event_signature",
    ):
        _raise_taskplan_invalid(
            "unsupported_signature_strategy",
            f"Unsupported blueprint_signature strategy: {strategy}.",
            f"{path}.write.strategy",
        )
    ops = _required_list(write, "ops", f"{path}.write.ops", min_length=1)
    for index, op in enumerate(ops):
        if not isinstance(op, dict):
            _raise_taskplan_invalid("invalid_signature_op", "Blueprint signature op must be an object.", f"{path}.write.ops[{index}]")
        _required_string(op, "op", f"{path}.write.ops[{index}].op")


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
    logic_statements: List[Dict[str, Any]] = []
    logic_entry: Optional[Dict[str, Any]] = None
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
        logic_statements.extend(_compile_ensure_entry_op_into_append_payload(nodes, links, op, f"steps[0].write.ops[{op_index}]"))
        if logic_entry is None and op.get("entry_type") == "custom_event" and isinstance(op.get("name"), str):
            logic_entry = {
                "kind": "custom_event",
                "name": op["name"],
                "id": f"{_to_id_segment(op['name'])}_entry",
            }

    return _omit_none({
        "target": {
            "asset_path": step["target"]["asset_path"],
            "graph": step["target"]["graph"],
        },
        "feature_name": task_plan.get("task_name"),
        "logic_spec": {
            "schema": "BlueprintLogicSpec.v2",
            **({"entry": logic_entry} if logic_entry else {}),
            "statements": logic_statements,
        },
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


def _assert_no_legacy_validation_fields(task_spec: Dict[str, Any]) -> None:
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


def _target_asset_path(task_spec: Dict[str, Any]) -> str:
    target = task_spec.get("target")
    if isinstance(target, dict):
        return _required_string(target, "asset_path", "target.asset_path")
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        "target must be an object.",
        [{"code": "missing_target", "path": "target", "message": "Provide target.asset_path."}],
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
    strategy = behavior.get("graph_strategy")
    if strategy not in {"append_new_owned_graph", "replace_owned_graph", "patch_owned_graph", "merge_owned_graph"}:
        raise TaskSpecCompileError(
            "unsupported_graph_strategy",
            "Unsupported GraphWrite graph_strategy.",
            [{
                "code": "unsupported_graph_strategy",
                "path": "behavior.graph_strategy",
                "message": "Use append_new_owned_graph, replace_owned_graph, patch_owned_graph, or merge_owned_graph.",
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
            "Modifying user nodes is not supported for GraphWrite owned strategies.",
            [{
                "code": "unsupported_scope_policy",
                "path": "scope_policy.allow_modify_user_nodes",
                "message": "Set allow_modify_user_nodes=false and target BlueprintHelper-owned graph logic.",
                "suggested_patch": {
                    "op": "replace",
                    "path": "/scope_policy/allow_modify_user_nodes",
                    "value": False,
                },
            }],
        )

    _compile_graph_write_ops(behavior)


def _compile_graph_write_ops(behavior: Dict[str, Any]) -> List[Dict[str, Any]]:
    strategy = _required_string(behavior, "graph_strategy", "behavior.graph_strategy")
    if strategy == "append_new_owned_graph":
        return _compile_append_graph_write_ops(behavior)
    if strategy == "replace_owned_graph":
        return [_compile_replace_graph_write_op(behavior)]
    if strategy == "patch_owned_graph":
        return _compile_patch_graph_write_ops(behavior)
    if strategy == "merge_owned_graph":
        return _compile_merge_graph_write_ops(behavior)

    raise TaskSpecCompileError(
        "unsupported_graph_strategy",
        "Unsupported GraphWrite graph_strategy.",
        [{
            "code": "unsupported_graph_strategy",
            "path": "behavior.graph_strategy",
            "message": "Use append_new_owned_graph, replace_owned_graph, patch_owned_graph, or merge_owned_graph.",
            "suggested_patch": {
                "op": "replace",
                "path": "/behavior/graph_strategy",
                "value": "append_new_owned_graph",
            },
        }],
    )


def _compile_append_graph_write_ops(behavior: Dict[str, Any]) -> List[Dict[str, Any]]:
    entries = _required_non_empty_list(behavior, "entries", "behavior.entries")
    ops = []
    for entry_index, entry_value in enumerate(entries):
        if not isinstance(entry_value, dict):
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "GraphWrite entry must be an object.",
                [{
                    "code": "invalid_graph_write_entry",
                    "path": f"behavior.entries[{entry_index}]",
                    "message": "GraphWrite entry must be an object.",
                }],
            )

        entry_type = _required_string(entry_value, "entry_type", f"behavior.entries[{entry_index}].entry_type")
        if entry_type != "custom_event":
            raise TaskSpecCompileError(
                "unsupported_entry_type",
                "Only custom_event entries are supported in this GraphWrite slice.",
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

        body = _required_logic_body(entry_value, "body", f"behavior.entries[{entry_index}].body")
        _validate_supported_statements(body["statements"], f"behavior.entries[{entry_index}].body.statements")
        entry_name = _required_string(entry_value, "name", f"behavior.entries[{entry_index}].name")
        ops.append({
            "op": "ensure_entry",
            "entry_type": entry_type,
            "name": entry_name,
            "body": _compile_logic_body_to_semantic_logic_spec(body, entry_name),
        })
    return ops


def _compile_replace_graph_write_op(behavior: Dict[str, Any]) -> Dict[str, Any]:
    replace = _required_object(behavior, "replace", "behavior.replace")
    replace_scope = _required_string(replace, "scope", "behavior.replace.scope")
    _assert_allowed_string(
        replace_scope,
        "behavior.replace.scope",
        {"custom_event_definition", "custom_event_body", "function_body", "event_body", "block_implementation"},
        "Use custom_event_definition, custom_event_body, function_body, event_body, or block_implementation.",
    )
    graph_write_replace_scope = "custom_event_body" if replace_scope == "custom_event_definition" else replace_scope
    selector = _normalize_replace_selector(
        graph_write_replace_scope,
        _required_object(replace, "selector", "behavior.replace.selector"),
    )
    body = _required_logic_body(replace, "body", "behavior.replace.body")
    _validate_supported_statements(body["statements"], "behavior.replace.body.statements")
    logic_spec = _compile_logic_body_to_semantic_logic_spec(body, "replace")
    if len(logic_spec["statements"]) == 0:
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            "replace_owned_graph requires at least one replacement statement.",
            [{
                "code": "empty_replacement",
                "path": "behavior.replace.body.statements",
                "message": "Provide at least one replacement statement.",
            }],
        )

    return _omit_none({
        "op": "replace_body",
        "replace_scope": graph_write_replace_scope,
        "selector": selector,
        "logic_spec": logic_spec,
        "options": replace.get("options") if isinstance(replace.get("options"), dict) else None,
        "__signature_split": {
            "op": "ensure_custom_event",
            "event_name": selector.get("entry_name"),
            "inputs": replace.get("inputs"),
            "name_collision_policy": replace.get("name_collision_policy", "reuse_if_exists"),
        } if replace_scope == "custom_event_definition" else None,
    })


def _compile_patch_graph_write_ops(behavior: Dict[str, Any]) -> List[Dict[str, Any]]:
    patches = _required_non_empty_list(behavior, "patches", "behavior.patches")
    ops = []
    for index, patch_value in enumerate(patches):
        path = f"behavior.patches[{index}]"
        if not isinstance(patch_value, dict):
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "GraphWrite patch must be an object.",
                [{
                    "code": "invalid_graph_write_patch",
                    "path": path,
                    "message": "GraphWrite patch must be an object.",
                }],
            )
        kind = _required_string(patch_value, "kind", f"{path}.kind")
        if kind not in {"set_pin_default", "set_node_comment", "set_node_position"}:
            raise TaskSpecCompileError(
                "unsupported_graph_write_patch",
                f"Unsupported GraphWrite patch kind: {kind}",
                [{
                    "code": "unsupported_graph_write_patch",
                    "path": f"{path}.kind",
                    "message": "Use set_pin_default, set_node_comment, or set_node_position.",
                }],
            )
        patch_scope = patch_value.get("scope") if isinstance(patch_value.get("scope"), str) and patch_value.get("scope") else _default_patch_scope(kind)
        expected_scope = _default_patch_scope(kind)
        if patch_scope != expected_scope:
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                f"GraphWrite patch scope must match {kind}.",
                [{
                    "code": "patch_scope_mismatch",
                    "path": f"{path}.scope",
                    "message": f"{kind} uses scope {expected_scope}. Omit scope or set it to {expected_scope}.",
                }],
            )
        ops.append(_omit_none({
            "op": kind,
            "patch_scope": patch_scope,
            "patched_ref": _normalize_patch_target_ref(kind, _required_object(patch_value, "target_ref", f"{path}.target_ref"), f"{path}.target_ref"),
            "patch": _compile_patch_payload(kind, patch_value, path),
            "expected_old_state": _normalize_expected_old_state(patch_value["expected_old_state"]) if isinstance(patch_value.get("expected_old_state"), dict) else None,
        }))
    return ops


def _compile_merge_graph_write_ops(behavior: Dict[str, Any]) -> List[Dict[str, Any]]:
    merges = _required_non_empty_list(behavior, "merges", "behavior.merges")
    ops = []
    for index, merge_value in enumerate(merges):
        path = f"behavior.merges[{index}]"
        if not isinstance(merge_value, dict):
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "GraphWrite merge must be an object.",
                [{
                    "code": "invalid_graph_write_merge",
                    "path": path,
                    "message": "GraphWrite merge must be an object.",
                }],
            )
        kind = _required_string(merge_value, "kind", f"{path}.kind")
        if kind != "insert_flow":
            raise TaskSpecCompileError(
                "unsupported_graph_write_merge",
                f"Unsupported GraphWrite merge kind: {kind}",
                [{
                    "code": "unsupported_graph_write_merge",
                    "path": f"{path}.kind",
                    "message": "Use insert_flow.",
                }],
            )
        merge_scope = _required_string(merge_value, "scope", f"{path}.scope")
        _assert_allowed_string(
            merge_scope,
            f"{path}.scope",
            {"owned_block_call", "custom_event_call", "function_call"},
            "Use owned_block_call, custom_event_call, or function_call.",
        )
        insert_strategy = _required_string(merge_value, "insert_strategy", f"{path}.insert_strategy")
        _assert_allowed_string(
            insert_strategy,
            f"{path}.insert_strategy",
            {"append_after", "insert_between", "branch_fork"},
            "Use append_after, insert_between, or branch_fork.",
        )
        sequence_order = _normalize_merge_sequence_order(merge_value, insert_strategy, f"{path}.sequence_order")
        ops.append(_omit_none({
            "op": "insert_flow",
            "merge_scope": merge_scope,
            "insert_strategy": insert_strategy,
            "anchor": _normalize_merge_anchor(_required_object(merge_value, "anchor", f"{path}.anchor"), f"{path}.anchor"),
            "inserted": _normalize_merge_inserted(merge_scope, _required_object(merge_value, "inserted", f"{path}.inserted"), f"{path}.inserted"),
            "sequence_order": sequence_order,
        }))
    return ops


PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES = {
    "component_bound_event": "component_bound_event",
    "delegate.bind": "bind",
    "delegate.assign": "assign",
    "delegate.unbind": "unbind",
    "delegate.unbind_all": "clear",
    "delegate.call": "call",
}
INTERNAL_DELEGATE_STATEMENT_KIND = "delegate"
DELEGATE_STATEMENT_OPERATION_KINDS = {"bind", "assign", "unbind", "clear", "call"}
FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS = {
    "delegate",
    "bind",
    "assign",
    "unbind",
    "unbind_all",
    "delegate_call",
    "delegate_clear",
}
SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = {
    "call",
    "field",
    "set",
    "set_property",
    "let",
    "control",
    *PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.keys(),
}
SUPPORTED_GRAPH_BODY_CONTROL_KINDS = {"branch", "sequence", "return"}
SUPPORTED_GRAPH_BODY_EXPRESSION_KINDS = {
    "literal",
    "field",
    "get",
    "get_property",
    "call",
    "op",
    "construct",
    "deconstruct",
    "select",
}

FIELD_STATEMENT_KIND_MAP = {
    "set": ("set", "variable"),
    "set_property": ("set", "property_path"),
}
FIELD_EXPRESSION_KIND_MAP = {
    "get": ("get", "variable"),
    "get_property": ("get", "property_path"),
}


def _apply_field_taxonomy(out: Dict[str, Any], operation: str, scope: str) -> None:
    out["kind"] = "field"
    out["field_operation"] = operation
    out["field_scope"] = scope


def _field_operation_scope(record: Dict[str, Any], path: str) -> tuple[str, str]:
    operation = _required_string(record, "field_operation", f"{path}.field_operation").strip().lower()
    scope = _required_string(record, "field_scope", f"{path}.field_scope").strip().lower()
    if operation not in {"get", "set"}:
        raise TaskSpecCompileError(
            "unsupported_field_operation",
            f"Unsupported field_operation: {operation}",
            [{
                "code": "unsupported_field_operation",
                "path": f"{path}.field_operation",
                "message": "Use get or set.",
            }],
        )
    if scope not in {"variable", "property_path"}:
        raise TaskSpecCompileError(
            "unsupported_field_scope",
            f"Unsupported field_scope: {scope}",
            [{
                "code": "unsupported_field_scope",
                "path": f"{path}.field_scope",
                "message": "Use variable or property_path.",
            }],
        )
    return operation, scope


def _normalized_delegate_statement_kind(kind: str) -> str:
    return PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.get(kind, kind)


def _delegate_statement_operation(statement: Dict[str, Any]) -> Optional[str]:
    kind = statement.get("kind")
    if not isinstance(kind, str):
        return None
    if kind == INTERNAL_DELEGATE_STATEMENT_KIND:
        operation = statement.get("delegate_operation")
        if isinstance(operation, str) and operation in DELEGATE_STATEMENT_OPERATION_KINDS:
            return operation
        return None

    if kind not in PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES or kind == "component_bound_event":
        return None

    normalized_kind = _normalized_delegate_statement_kind(kind)
    if normalized_kind in DELEGATE_STATEMENT_OPERATION_KINDS:
        return normalized_kind
    return None


def _validate_supported_statements(statements: List[Dict[str, Any]], path: str) -> None:
    for statement_index, statement in enumerate(statements):
        kind = statement.get("kind")
        statement_path = f"{path}[{statement_index}]"
        if kind in FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS:
            raise TaskSpecCompileError(
                "unsupported_statement_kind",
                "Use component_bound_event or delegate.bind/delegate.assign/delegate.unbind/delegate.unbind_all/delegate.call in Agent-facing TaskSpec. The compiler owns kind=delegate + delegate_operation lowering.",
                [{
                    "code": "unsupported_statement_kind",
                    "path": f"{statement_path}.kind",
                    "message": "Use component_bound_event or delegate.bind/delegate.assign/delegate.unbind/delegate.unbind_all/delegate.call in Agent-facing TaskSpec. The compiler owns kind=delegate + delegate_operation lowering.",
                }],
            )
        if kind not in SUPPORTED_GRAPH_BODY_STATEMENT_KINDS:
            raise TaskSpecCompileError(
                "unsupported_statement_kind",
                "Unsupported GraphWrite statement kind.",
                [{
                    "code": "unsupported_statement_kind",
                    "path": f"{statement_path}.kind",
                    "message": "Use call, field, set, set_property, let, control, or a supported delegate statement kind.",
                }],
            )
        if kind in PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES:
            _validate_delegate_statement_shape(statement, statement_path)
        elif kind == "call":
            _validate_expression_map(statement.get("args"), f"{statement_path}.args")
        elif kind == "field":
            operation, _scope = _field_operation_scope(statement, statement_path)
            if operation != "set":
                raise TaskSpecCompileError(
                    "unsupported_field_operation",
                    "Field statements require field_operation=set.",
                    [{
                        "code": "unsupported_field_operation",
                        "path": f"{statement_path}.field_operation",
                        "message": "Field statements require field_operation=set.",
                    }],
                )
            _validate_supported_expression(statement.get("value"), f"{statement_path}.value")
        elif kind in {"let", "set", "set_property"}:
            _validate_supported_expression(statement.get("value"), f"{statement_path}.value")
        elif kind == "control":
            control_kind = _control_statement_kind(statement, statement_path)
            if control_kind == "branch":
                _validate_supported_expression(statement.get("condition"), f"{statement_path}.condition")
                _validate_supported_statements(statement.get("then") if isinstance(statement.get("then"), list) else [], f"{statement_path}.then")
                _validate_supported_statements(statement.get("else") if isinstance(statement.get("else"), list) else [], f"{statement_path}.else")
            elif control_kind == "sequence":
                if isinstance(statement.get("statements"), list) and statement["statements"]:
                    raise TaskSpecCompileError(
                        "unsupported_control_shape",
                        "Unsupported GraphWrite control shape.",
                        [{
                            "code": "unsupported_control_shape",
                            "path": f"{statement_path}.statements",
                            "message": "Sequence control is an execution-flow node; place following statements after it.",
                        }],
                    )
            elif "value" in statement:
                _validate_supported_expression(statement.get("value"), f"{statement_path}.value")


def _validate_delegate_statement_shape(statement: Dict[str, Any], path: str) -> None:
    kind = _required_string(statement, "kind", f"{path}.kind")
    normalized_kind = _normalized_delegate_statement_kind(kind)
    if normalized_kind in {"bind", "assign", "unbind"}:
        _required_string(statement, "target", f"{path}.target")
        _required_string(statement, "delegate", f"{path}.delegate")
        _required_string(statement, "handler", f"{path}.handler")
        return
    if normalized_kind == "component_bound_event":
        _required_string(statement, "component", f"{path}.component")
        _required_string(statement, "delegate", f"{path}.delegate")
        _required_string(statement, "handler", f"{path}.handler")
        return
    if normalized_kind == "clear":
        _required_string(statement, "target", f"{path}.target")
        _required_string(statement, "delegate", f"{path}.delegate")
        if "handler" in statement:
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "delegate.unbind_all must not include handler.",
                [{
                    "code": "delegate_clear_handler_forbidden",
                    "path": f"{path}.handler",
                    "message": "delegate.unbind_all must not include handler; clear() removes all handlers for the delegate.",
                }],
            )
        return
    if normalized_kind == "call":
        _required_string(statement, "target", f"{path}.target")
        _required_string(statement, "delegate", f"{path}.delegate")
        _validate_expression_map(statement.get("args"), f"{path}.args")


def _control_statement_kind(statement: Dict[str, Any], path: str) -> str:
    control_kind = statement.get("control")
    if isinstance(control_kind, str) and control_kind in SUPPORTED_GRAPH_BODY_CONTROL_KINDS:
        return control_kind
    raise TaskSpecCompileError(
        "unsupported_control_kind",
        "Unsupported GraphWrite control kind.",
        [{
            "code": "unsupported_control_kind",
            "path": f"{path}.control",
            "message": "Use branch, sequence, or return.",
        }],
    )


def _validate_expression_map(value: Any, path: str) -> None:
    if not isinstance(value, dict):
        return
    for key, expression in value.items():
        _validate_supported_expression(expression, f"{path}.{key}")


def _validate_expression_list(value: Any, path: str) -> None:
    if not isinstance(value, list):
        return
    for index, expression in enumerate(value):
        _validate_supported_expression(expression, f"{path}[{index}]")


def _validate_supported_expression(expression: Any, path: str) -> None:
    if not isinstance(expression, dict):
        return
    kind = expression.get("kind") if isinstance(expression.get("kind"), str) else "literal"
    if kind not in SUPPORTED_GRAPH_BODY_EXPRESSION_KINDS:
        raise TaskSpecCompileError(
            "unsupported_expression_kind",
            "Unsupported GraphWrite expression kind.",
            [{
                "code": "unsupported_expression_kind",
                "path": f"{path}.kind",
                "message": "Use literal, field, get, get_property, call, op, construct, deconstruct, or select.",
            }],
        )
    if kind == "field":
        operation, _scope = _field_operation_scope(expression, path)
        if operation != "get":
            raise TaskSpecCompileError(
                "unsupported_field_operation",
                "Field expressions require field_operation=get.",
                [{
                    "code": "unsupported_field_operation",
                    "path": f"{path}.field_operation",
                    "message": "Field expressions require field_operation=get.",
                }],
            )
    if kind in {"call", "op", "construct", "deconstruct"}:
        _validate_expression_map(expression.get("args"), f"{path}.args")
    if kind == "op":
        if "left" in expression:
            _validate_supported_expression(expression.get("left"), f"{path}.left")
        if "right" in expression:
            _validate_supported_expression(expression.get("right"), f"{path}.right")
    if kind == "deconstruct":
        if "source" in expression:
            _validate_supported_expression(expression.get("source"), f"{path}.source")
        if "value" in expression:
            _validate_supported_expression(expression.get("value"), f"{path}.value")
    if kind == "select":
        _validate_supported_expression(expression.get("condition"), f"{path}.condition")
        _validate_expression_list(expression.get("options"), f"{path}.options")


def _compile_logic_body_to_import_payload(
    body: Dict[str, List[Dict[str, Any]]],
    prefix: str,
    path: str,
) -> Dict[str, List[Dict[str, Any]]]:
    flow = _compile_statement_sequence(body["statements"], f"{_to_id_segment(prefix)}_stmt", f"{path}.statements", _make_compile_flow_context())
    return {"nodes": flow["nodes"], "links": flow["links"]}


def _compile_logic_body_to_semantic_logic_spec(
    body: Dict[str, List[Dict[str, Any]]],
    prefix: str,
) -> Dict[str, Any]:
    return {
        "schema": "BlueprintLogicSpec.v2",
        "statements": _clone_logic_statement_sequence_with_compiled_ids(body["statements"], f"{_to_id_segment(prefix)}_stmt"),
    }


def _make_compile_flow_context(parent: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    return {"symbols": dict(parent.get("symbols", {})) if isinstance(parent, dict) else {}}


def _clone_logic_expression_with_compiled_ids(expression: Any, node_id: str) -> Any:
    if not isinstance(expression, dict):
        return expression

    kind = expression.get("kind") if isinstance(expression.get("kind"), str) else "literal"
    out = dict(expression)
    out["id"] = node_id

    if kind in FIELD_EXPRESSION_KIND_MAP:
        operation, scope = FIELD_EXPRESSION_KIND_MAP[kind]
        _apply_field_taxonomy(out, operation, scope)
        if kind == "get_property":
            property_path = _required_graph_body_property_path(expression, f"{node_id}.property_path")
            out["property_path"] = property_path
            out["property"] = property_path
    elif kind == "field":
        operation, scope = _field_operation_scope(expression, node_id)
        _apply_field_taxonomy(out, operation, scope)
        if scope == "property_path":
            property_path = _required_graph_body_property_path(expression, f"{node_id}.property_path")
            out["property_path"] = property_path
            out["property"] = property_path
    elif kind == "select":
        out["condition"] = _clone_logic_expression_with_compiled_ids(expression.get("condition"), f"{node_id}_index")
        options = expression.get("options")
        if isinstance(options, list):
            out["options"] = [
                _clone_logic_expression_with_compiled_ids(option, f"{node_id}_option_{index}")
                for index, option in enumerate(options)
            ]
    elif kind == "op":
        if "left" in expression:
            out["left"] = _clone_logic_expression_with_compiled_ids(expression.get("left"), f"{node_id}_left")
        if "right" in expression:
            out["right"] = _clone_logic_expression_with_compiled_ids(expression.get("right"), f"{node_id}_right")
        if isinstance(expression.get("args"), dict):
            out["args"] = {
                arg_name: _clone_logic_expression_with_compiled_ids(arg_value, f"{node_id}_{_to_id_segment(str(arg_name))}")
                for arg_name, arg_value in expression["args"].items()
            }
    elif kind == "construct" and isinstance(expression.get("args"), dict):
        out["args"] = {
            arg_name: _clone_logic_expression_with_compiled_ids(arg_value, f"{node_id}_{_to_id_segment(str(arg_name))}")
            for arg_name, arg_value in expression["args"].items()
        }
    elif kind == "deconstruct":
        if "source" in expression:
            out["source"] = _clone_logic_expression_with_compiled_ids(expression.get("source"), f"{node_id}_source")
        if "value" in expression:
            out["value"] = _clone_logic_expression_with_compiled_ids(expression.get("value"), f"{node_id}_value")
        if isinstance(expression.get("args"), dict):
            out["args"] = {
                arg_name: _clone_logic_expression_with_compiled_ids(arg_value, f"{node_id}_{_to_id_segment(str(arg_name))}")
                for arg_name, arg_value in expression["args"].items()
            }
    elif isinstance(expression.get("args"), dict):
        out["args"] = {
            arg_name: _clone_logic_expression_with_compiled_ids(arg_value, f"{node_id}_{_to_id_segment(str(arg_name))}")
            for arg_name, arg_value in expression["args"].items()
        }

    return out


def _clone_logic_statement_with_compiled_ids(statement: Dict[str, Any], statement_id: str) -> Dict[str, Any]:
    out = dict(statement)
    out["id"] = statement_id
    kind = statement.get("kind")
    if kind in FIELD_STATEMENT_KIND_MAP:
        operation, scope = FIELD_STATEMENT_KIND_MAP[kind]
        _apply_field_taxonomy(out, operation, scope)
        if kind == "set_property":
            property_path = _required_graph_body_property_path(statement, f"{statement_id}.property_path")
            out["property_path"] = property_path
            out["property"] = property_path
        out["value"] = _clone_logic_expression_with_compiled_ids(statement.get("value"), f"{statement_id}_value")
    elif kind == "field":
        operation, scope = _field_operation_scope(statement, statement_id)
        _apply_field_taxonomy(out, operation, scope)
        if scope == "property_path":
            property_path = _required_graph_body_property_path(statement, f"{statement_id}.property_path")
            out["property_path"] = property_path
            out["property"] = property_path
        if "value" in statement:
            out["value"] = _clone_logic_expression_with_compiled_ids(statement.get("value"), f"{statement_id}_value")
    elif kind == "component_bound_event":
        out["kind"] = "component_bound_event"
        return out
    delegate_operation = _delegate_statement_operation(statement)
    if delegate_operation is not None:
        out["kind"] = "delegate"
        out["delegate_operation"] = delegate_operation
        if delegate_operation == "unbind":
            out["unbind_mode"] = "single"
        elif delegate_operation == "clear":
            out["unbind_mode"] = "all"
        elif delegate_operation == "call" and isinstance(statement.get("args"), dict):
            out["args"] = {
                arg_name: _clone_logic_expression_with_compiled_ids(arg_value, f"{statement_id}_arg_{_to_id_segment(str(arg_name))}")
                for arg_name, arg_value in statement["args"].items()
            }
        return out

    if kind == "let":
        out["value"] = _clone_logic_expression_with_compiled_ids(statement.get("value"), f"{statement_id}_value")
    elif kind == "call" and isinstance(statement.get("args"), dict):
        out["args"] = {
            arg_name: _clone_logic_expression_with_compiled_ids(arg_value, f"{statement_id}_arg_{_to_id_segment(str(arg_name))}")
            for arg_name, arg_value in statement["args"].items()
        }
    elif kind == "control":
        control_kind = statement.get("control") if isinstance(statement.get("control"), str) else ""
        out["kind"] = control_kind
        out.pop("control", None)
        if control_kind == "branch":
            out["condition"] = _clone_logic_expression_with_compiled_ids(statement.get("condition"), f"{statement_id}_condition")
            if isinstance(statement.get("then"), list):
                out["then"] = _clone_logic_statement_sequence_with_compiled_ids(statement["then"], f"{statement_id}_then")
            if isinstance(statement.get("else"), list):
                out["else"] = _clone_logic_statement_sequence_with_compiled_ids(statement["else"], f"{statement_id}_else")
        elif control_kind == "sequence":
            out.pop("statements", None)
        elif control_kind == "return" and "value" in statement:
            out["value"] = _clone_logic_expression_with_compiled_ids(statement.get("value"), f"{statement_id}_value")

    return out


def _clone_logic_statement_sequence_with_compiled_ids(statements: List[Dict[str, Any]], id_prefix: str) -> List[Dict[str, Any]]:
    return [
        _clone_logic_statement_with_compiled_ids(statement, f"{id_prefix}_{statement_index + 1}")
        for statement_index, statement in enumerate(statements)
        if isinstance(statement, dict)
    ]


def _compile_statement_sequence(statements: List[Dict[str, Any]], id_prefix: str, path: str, context: Dict[str, Any]) -> Dict[str, Any]:
    nodes: List[Dict[str, Any]] = []
    links: List[Dict[str, Any]] = []
    entry = None
    previous_exits: List[str] = []

    for statement_index, statement in enumerate(statements):
        statement_id = f"{id_prefix}_{statement_index + 1}"
        statement_path = f"{path}[{statement_index}]"
        flow = _compile_statement_flow(statement, statement_id, statement_path, context)
        nodes.extend(flow["nodes"])
        links.extend(flow["links"])
        if not entry:
            entry = flow.get("entry")
        if flow.get("entry"):
            for exit_endpoint in previous_exits:
                links.append({"kind": "exec", "from": exit_endpoint, "to": flow["entry"]})
            previous_exits = flow["exits"]
        elif not flow.get("preserve_previous_exits"):
            previous_exits = flow["exits"]

    return {"nodes": nodes, "links": links, "entry": entry, "exits": previous_exits}


def _compile_statement_flow(statement: Dict[str, Any], node_id: str, path: str, context: Dict[str, Any]) -> Dict[str, Any]:
    kind = statement.get("kind")
    delegate_operation = _delegate_statement_operation(statement)
    if kind == "control":
        control_kind = _control_statement_kind(statement, path)
        if control_kind == "branch":
            branch_statement = dict(statement)
            branch_statement["kind"] = "branch"
            return _compile_branch_statement_flow(branch_statement, node_id, path, context)
        if control_kind == "return":
            return _compile_return_statement_flow(statement, node_id, path, context)
        return _compile_sequence_control_statement_flow(statement, node_id, path, context)
    if kind == "branch":
        return _compile_branch_statement_flow(statement, node_id, path, context)
    if kind == "return":
        return _compile_return_statement_flow(statement, node_id, path, context)
    if kind == "sequence":
        return _compile_sequence_control_statement_flow(statement, node_id, path, context)
    if kind == "let":
        name = _required_string(statement, "name", f"{path}.name")
        value_flow = _compile_value_expression(statement.get("value"), f"{node_id}_value", f"{path}.value", context)
        context["symbols"][name.lower()] = {
            "output": value_flow.get("output"),
            "default_value": value_flow.get("default_value"),
        }
        return {"nodes": value_flow["nodes"], "links": value_flow["links"], "exits": [], "preserve_previous_exits": True}
    node = _compile_statement_node(statement, node_id, path)
    nodes: List[Dict[str, Any]] = [node]
    links: List[Dict[str, Any]] = []
    if kind == "call" or delegate_operation == "call":
        input_values: Dict[str, Any] = {}
        args = statement.get("args")
        if isinstance(args, dict):
            for arg_name, arg_value in args.items():
                arg_flow = _compile_value_expression(arg_value, f"{node_id}_arg_{_to_id_segment(str(arg_name))}", f"{path}.args.{arg_name}", context)
                nodes.extend(arg_flow["nodes"])
                links.extend(arg_flow["links"])
                if arg_flow.get("output"):
                    links.append({"kind": "data", "from": arg_flow["output"], "to": f"{node_id}.{arg_name}"})
                else:
                    input_values[arg_name] = arg_flow.get("default_value")
        if kind == "call":
            node["inputs"] = input_values
        else:
            node["args"] = input_values
    if kind in {"set", "set_property", "field"}:
        if kind == "field":
            operation, scope = _field_operation_scope(statement, path)
            if operation != "set":
                raise TaskSpecCompileError(
                    "unsupported_field_operation",
                    "Field statements require field_operation=set.",
                    [{
                        "code": "unsupported_field_operation",
                        "path": f"{path}.field_operation",
                        "message": "Field statements require field_operation=set.",
                    }],
                )
            value_pin_name = _required_string(statement, "target", f"{path}.target") if scope == "variable" else "value"
        else:
            value_pin_name = _required_string(statement, "target", f"{path}.target") if kind == "set" else "value"
        value_flow = _compile_value_expression(statement.get("value"), f"{node_id}_value", f"{path}.value", context)
        nodes.extend(value_flow["nodes"])
        links.extend(value_flow["links"])
        if value_flow.get("output"):
            links.append({"kind": "data", "from": value_flow["output"], "to": f"{node_id}.{value_pin_name}"})
            node.pop("value", None)
        else:
            node["value"] = _value_expr_to_string(value_flow.get("default_value"))
    return {
        "nodes": nodes,
        "links": links,
        "entry": f"{node_id}.execute",
        "exits": [f"{node_id}.then"],
    }


def _compile_return_statement_flow(statement: Dict[str, Any], node_id: str, path: str, context: Dict[str, Any]) -> Dict[str, Any]:
    node: Dict[str, Any] = {"id": node_id, "kind": "return"}
    nodes: List[Dict[str, Any]] = [node]
    links: List[Dict[str, Any]] = []
    if "value" in statement:
        value_flow = _compile_value_expression(statement.get("value"), f"{node_id}_value", f"{path}.value", context)
        nodes.extend(value_flow["nodes"])
        links.extend(value_flow["links"])
        if value_flow.get("output"):
            links.append({"kind": "data", "from": value_flow["output"], "to": f"{node_id}.value"})
        else:
            node["value"] = _value_expr_to_string(value_flow.get("default_value"))
    return {"nodes": nodes, "links": links, "entry": f"{node_id}.execute", "exits": []}


def _compile_sequence_control_statement_flow(statement: Dict[str, Any], node_id: str, path: str, context: Dict[str, Any]) -> Dict[str, Any]:
    sequence_node: Dict[str, Any] = {"id": node_id, "kind": "sequence"}
    nodes: List[Dict[str, Any]] = [sequence_node]
    links: List[Dict[str, Any]] = []
    nested_statements = statement.get("statements") if isinstance(statement.get("statements"), list) else []
    nested_flow = _compile_statement_sequence(nested_statements, f"{node_id}_sequence", f"{path}.statements", _make_compile_flow_context(context))
    nodes.extend(nested_flow["nodes"])
    links.extend(nested_flow["links"])
    if nested_flow.get("entry"):
        links.append({"kind": "exec", "from": f"{node_id}.then", "to": nested_flow["entry"]})
    return {
        "nodes": nodes,
        "links": links,
        "entry": f"{node_id}.execute",
        "exits": nested_flow["exits"] if nested_flow.get("entry") else [f"{node_id}.then"],
    }


def _compile_branch_statement_flow(statement: Dict[str, Any], node_id: str, path: str, context: Dict[str, Any]) -> Dict[str, Any]:
    branch_node: Dict[str, Any] = {"id": node_id, "kind": "branch"}
    nodes: List[Dict[str, Any]] = [branch_node]
    links: List[Dict[str, Any]] = []
    condition_flow = _compile_branch_condition(statement.get("condition"), f"{node_id}_condition", f"{path}.condition", context)
    nodes.extend(condition_flow["nodes"])
    links.extend(condition_flow["links"])
    if condition_flow.get("output"):
        links.append({"kind": "data", "from": condition_flow["output"], "to": f"{node_id}.Condition"})
    if "default_value" in condition_flow:
        branch_node["inputs"] = {"Condition": condition_flow["default_value"]}

    then_statements = statement.get("then") if isinstance(statement.get("then"), list) else []
    else_statements = statement.get("else") if isinstance(statement.get("else"), list) else []
    then_flow = _compile_statement_sequence(then_statements, f"{node_id}_then", f"{path}.then", _make_compile_flow_context(context))
    else_flow = _compile_statement_sequence(else_statements, f"{node_id}_else", f"{path}.else", _make_compile_flow_context(context))
    nodes.extend(then_flow["nodes"])
    nodes.extend(else_flow["nodes"])
    links.extend(then_flow["links"])
    links.extend(else_flow["links"])

    exits: List[str] = []
    if then_flow.get("entry"):
        links.append({"kind": "exec", "from": f"{node_id}.then", "to": then_flow["entry"]})
        exits.extend(then_flow["exits"])
    else:
        exits.append(f"{node_id}.then")
    if else_flow.get("entry"):
        links.append({"kind": "exec", "from": f"{node_id}.else", "to": else_flow["entry"]})
        exits.extend(else_flow["exits"])
    else:
        exits.append(f"{node_id}.else")

    return {"nodes": nodes, "links": links, "entry": f"{node_id}.execute", "exits": exits}


def _compile_branch_condition(condition: Any, node_id: str, path: str, context: Dict[str, Any]) -> Dict[str, Any]:
    return _compile_value_expression(condition, node_id, path, context)


def _compile_value_expression(expression: Any, node_id: str, path: str, context: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(expression, dict):
        return {"nodes": [], "links": [], "default_value": _literal_value(expression)}

    kind = expression.get("kind") if isinstance(expression.get("kind"), str) else "literal"
    if kind == "literal":
        return {"nodes": [], "links": [], "default_value": _literal_value(expression)}

    if kind not in SUPPORTED_GRAPH_BODY_EXPRESSION_KINDS:
        raise TaskSpecCompileError(
            "unsupported_expression_kind",
            f"Unsupported expression kind: {kind}",
            [{
                "code": "unsupported_expression_kind",
                "path": f"{path}.kind",
                "message": "Use literal, field, get, get_property, call, op, construct, deconstruct, or select.",
            }],
        )

    if kind in {"get", "get_property", "field"}:
        target = _required_string(expression, "target", f"{path}.target")
        if kind == "field":
            operation, scope = _field_operation_scope(expression, path)
            if operation != "get":
                raise TaskSpecCompileError(
                    "unsupported_field_operation",
                    "Field expressions require field_operation=get.",
                    [{
                        "code": "unsupported_field_operation",
                        "path": f"{path}.field_operation",
                        "message": "Field expressions require field_operation=get.",
                    }],
                )
        else:
            operation, scope = FIELD_EXPRESSION_KIND_MAP[kind]
        if scope == "variable":
            symbol = context.get("symbols", {}).get(target.lower())
            if symbol:
                return {"nodes": [], "links": [], "output": symbol.get("output"), "default_value": symbol.get("default_value")}
        output_pin = "value" if scope == "property_path" else target
        node = {"id": node_id, "var": target, "target": target}
        _apply_field_taxonomy(node, operation, scope)
        if scope == "property_path":
            property_path = _required_graph_body_property_path(expression, path)
            node["property_path"] = property_path
            node["property"] = property_path
        return {
            "nodes": [node],
            "links": [],
            "output": f"{node_id}.{output_pin}",
        }

    nodes: List[Dict[str, Any]] = []
    links: List[Dict[str, Any]] = []
    node: Dict[str, Any] = {"id": node_id, "kind": kind, "inputs": {}}
    if kind == "call":
        node["function"] = _required_string(expression, "target", f"{path}.target")
    if kind == "op":
        node["function"] = _required_string(expression, "op", f"{path}.op")
        if "left" in expression:
            _compile_expression_input(expression.get("left"), "A", f"{node_id}_left", f"{path}.left", node, nodes, links, context)
        if "right" in expression:
            _compile_expression_input(expression.get("right"), "B", f"{node_id}_right", f"{path}.right", node, nodes, links, context)
        args = expression.get("args")
        if isinstance(args, dict):
            for arg_name, arg_value in args.items():
                _compile_expression_input(arg_value, str(arg_name), f"{node_id}_{_to_id_segment(str(arg_name))}", f"{path}.args.{arg_name}", node, nodes, links, context)
    elif kind == "select":
        options = expression.get("options")
        if isinstance(options, list):
            for index, option in enumerate(options):
                _compile_expression_input(option, f"Option{index}", f"{node_id}_option_{index}", f"{path}.options[{index}]", node, nodes, links, context)
        _compile_expression_input(expression.get("condition"), "Index", f"{node_id}_index", f"{path}.condition", node, nodes, links, context)
    elif kind == "construct":
        struct_type = _required_construct_type(expression, path)
        node["type"] = struct_type
        node["struct_path"] = struct_type
        args = expression.get("args")
        if isinstance(args, dict):
            for arg_name, arg_value in args.items():
                _compile_expression_input(arg_value, str(arg_name), f"{node_id}_{_to_id_segment(str(arg_name))}", f"{path}.args.{arg_name}", node, nodes, links, context)
    elif kind == "deconstruct":
        struct_type = _optional_graph_body_type(expression)
        if struct_type:
            node["type"] = struct_type
            node["struct_path"] = struct_type
        property_path = _optional_graph_body_property_path(expression)
        if property_path:
            node["property_path"] = property_path
            node["property"] = property_path
        if "source" in expression:
            _compile_expression_input(expression.get("source"), "Input", f"{node_id}_source", f"{path}.source", node, nodes, links, context)
        elif "value" in expression:
            _compile_expression_input(expression.get("value"), "Input", f"{node_id}_value", f"{path}.value", node, nodes, links, context)
    elif isinstance(expression.get("args"), dict):
        for arg_name, arg_value in expression["args"].items():
            _compile_expression_input(arg_value, str(arg_name), f"{node_id}_{_to_id_segment(str(arg_name))}", f"{path}.args.{arg_name}", node, nodes, links, context)
    nodes.insert(0, node)

    output_pin = "value" if kind in {"construct", "deconstruct", "select"} else "ReturnValue"
    return {"nodes": nodes, "links": links, "output": f"{node_id}.{output_pin}"}


def _optional_graph_body_property_path(record: Dict[str, Any]) -> Optional[str]:
    property_path = record.get("property_path")
    if isinstance(property_path, str) and property_path.strip():
        return property_path
    property_name = record.get("property")
    if isinstance(property_name, str) and property_name.strip():
        return property_name
    return None


def _required_graph_body_property_path(record: Dict[str, Any], path: str) -> str:
    property_path = _optional_graph_body_property_path(record)
    if property_path:
        return property_path
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path}.property_path must be a non-empty string.",
        [{
            "code": "missing_property_path",
            "path": f"{path}.property_path",
            "message": "Provide property_path for graph-body property access.",
        }],
    )


def _optional_graph_body_type(record: Dict[str, Any]) -> Optional[str]:
    type_name = record.get("type")
    if isinstance(type_name, str) and type_name.strip():
        return type_name
    struct_path = record.get("struct_path")
    if isinstance(struct_path, str) and struct_path.strip():
        return struct_path
    return None


def _required_construct_type(record: Dict[str, Any], path: str) -> str:
    struct_type = _optional_graph_body_type(record)
    if struct_type:
        return struct_type
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path}.type must be a non-empty string.",
        [{
            "code": "missing_construct_type",
            "path": f"{path}.type",
            "message": "Provide type for construct expressions.",
        }],
    )


def _compile_expression_input(
    expression: Any,
    pin_name: str,
    node_id: str,
    path: str,
    target_node: Dict[str, Any],
    nodes: List[Dict[str, Any]],
    links: List[Dict[str, Any]],
    context: Dict[str, Any],
) -> None:
    value_flow = _compile_value_expression(expression, node_id, path, context)
    nodes.extend(value_flow["nodes"])
    links.extend(value_flow["links"])
    if value_flow.get("output"):
        links.append({"kind": "data", "from": value_flow["output"], "to": f"{target_node['id']}.{pin_name}"})
    else:
        target_node.setdefault("inputs", {})[pin_name] = value_flow.get("default_value")


def _default_patch_scope(kind: str) -> str:
    if kind == "set_node_comment":
        return "node_comment"
    if kind == "set_node_position":
        return "node_position"
    return "pin_default"


def _normalize_replace_selector(replace_scope: str, selector: Dict[str, Any]) -> Dict[str, Any]:
    kind = _required_string(selector, "kind", "behavior.replace.selector.kind")
    out: Dict[str, Any] = {}
    _copy_optional_string_fields(selector, out, ["graph_id", "node_ref", "node_path"])

    if replace_scope == "custom_event_body":
        _require_selector_kind(kind, "custom_event", replace_scope)
        out["entry_name"] = _required_string(selector, "name", "behavior.replace.selector.name")
        return out
    if replace_scope == "event_body":
        _require_selector_kind(kind, "event", replace_scope)
        out["entry_name"] = _required_string(selector, "name", "behavior.replace.selector.name")
        return out
    if replace_scope == "function_body":
        _require_selector_kind(kind, "function", replace_scope)
        out["function_name"] = _required_string(selector, "name", "behavior.replace.selector.name")
        return out

    _require_selector_kind(kind, "block", replace_scope)
    out["block_id"] = _required_string(selector, "block_id", "behavior.replace.selector.block_id")
    _copy_optional_string_fields(selector, out, ["target_ref", "block_ref"])
    return out


def _require_selector_kind(actual: str, expected: str, replace_scope: str) -> None:
    if actual == expected:
        return
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"replace selector kind must match {replace_scope}.",
        [{
            "code": "replace_selector_scope_mismatch",
            "path": "behavior.replace.selector.kind",
            "message": f'{replace_scope} requires selector.kind="{expected}".',
        }],
    )


def _normalize_patch_target_ref(kind: str, target_ref: Dict[str, Any], path: str) -> Dict[str, Any]:
    _assert_block_scoped_graph_write_ref(target_ref, path)
    _required_string(target_ref, "node_ref", f"{path}.node_ref")
    if kind == "set_pin_default":
        _required_string(target_ref, "pin_ref", f"{path}.pin_ref")
    return dict(target_ref)


def _compile_patch_payload(kind: str, patch: Dict[str, Any], path: str) -> Dict[str, Any]:
    if kind == "set_pin_default":
        if "value" not in patch:
            _throw_missing_patch_value(path, "set_pin_default requires value.")
        return {"value": _patch_value_to_string(_literal_value(patch.get("value")))}
    if kind == "set_node_comment":
        if "value" not in patch:
            _throw_missing_patch_value(path, "set_node_comment requires value.")
        return {"comment": _patch_value_to_string(_literal_value(patch.get("value")))}
    if kind == "set_node_position":
        payload = _required_object(patch, "patch", f"{path}.patch")
        if not isinstance(payload.get("x"), (int, float)) and not isinstance(payload.get("y"), (int, float)):
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "set_node_position requires patch.x or patch.y.",
                [{
                    "code": "missing_node_position",
                    "path": f"{path}.patch",
                    "message": "Provide patch.x and/or patch.y as numbers.",
                }],
            )
        return _literal_record_values(payload)
    raise TaskSpecCompileError(
        "unsupported_graph_write_patch",
        f"Unsupported GraphWrite patch kind: {kind}",
        [{
            "code": "unsupported_graph_write_patch",
            "path": f"{path}.kind",
            "message": "Use set_pin_default, set_node_comment, or set_node_position.",
        }],
    )


def _throw_missing_patch_value(path: str, message: str) -> None:
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        message,
        [{
            "code": "missing_patch_payload",
            "path": f"{path}.value",
            "message": "Provide value.",
        }],
    )


def _normalize_expected_old_state(record: Dict[str, Any]) -> Dict[str, Any]:
    out = _literal_record_values(record)
    if "value" in record:
        out["value"] = _patch_value_to_string(_literal_value(record.get("value")))
    return out


def _normalize_merge_anchor(anchor: Dict[str, Any], path: str) -> Dict[str, Any]:
    _assert_block_scoped_graph_write_ref(anchor, path)
    _required_string(anchor, "node_ref", f"{path}.node_ref")
    _required_string(anchor, "pin_ref", f"{path}.pin_ref")
    return dict(anchor)


def _assert_block_scoped_graph_write_ref(ref: Dict[str, Any], path: str) -> None:
    has_block_id = isinstance(ref.get("block_id"), str) and ref["block_id"].strip()
    if has_block_id:
        return

    for field in ("node_ref", "pin_ref", "link_ref"):
        value = ref.get(field)
        if isinstance(value, str) and _is_raw_logicjson_array_ref(value):
            _raise_unsupported_graph_write_anchor(
                f"{path}.{field}",
                f"{path}.{field} uses a read-view array index. Use block_id with group-local node_ref/pin_ref/link_ref.",
            )

    _raise_unsupported_graph_write_anchor(
        path,
        f"{path} must identify a BlueprintHelper-owned block with block_id.",
    )


def _is_raw_logicjson_array_ref(value: str) -> bool:
    return re.fullmatch(r"(?:nodes|pins|links)\[\d+\]", value.strip()) is not None


def _raise_unsupported_graph_write_anchor(path: str, message: str) -> None:
    raise TaskSpecCompileError(
        "unsupported_graph_write_anchor",
        "GraphWrite patch/merge requires a block-scoped anchor.",
        [{
            "code": "unsupported_graph_write_anchor",
            "path": path,
            "message": message,
        }],
    )


def _normalize_merge_inserted(merge_scope: str, inserted: Dict[str, Any], path: str) -> Dict[str, Any]:
    call_kind = _required_string(inserted, "call_kind", f"{path}.call_kind")
    if call_kind != merge_scope:
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            "merge inserted.call_kind must match merge scope.",
            [{
                "code": "merge_inserted_scope_mismatch",
                "path": f"{path}.call_kind",
                "message": f'{merge_scope} requires inserted.call_kind="{merge_scope}".',
            }],
        )
    if merge_scope == "function_call":
        return {"function": _required_string(inserted, "name", f"{path}.name")}
    if merge_scope == "custom_event_call":
        return {"custom_event": _required_string(inserted, "name", f"{path}.name")}
    return _omit_none({
        "block_id": _required_string(inserted, "block_id", f"{path}.block_id"),
        "block_ref": inserted.get("block_ref") if isinstance(inserted.get("block_ref"), str) and inserted.get("block_ref") else None,
    })


def _normalize_merge_sequence_order(record: Dict[str, Any], insert_strategy: str, path: str) -> List[str] | None:
    raw = record.get("sequence_order")
    if insert_strategy != "branch_fork":
        if raw is not None:
            raise TaskSpecCompileError(
                "taskspec_semantic_invalid",
                "sequence_order is only valid for branch_fork.",
                [{
                    "code": "sequence_order_not_allowed",
                    "path": path,
                    "message": "Remove sequence_order unless insert_strategy is branch_fork.",
                }],
            )
        return None
    if not isinstance(raw, list) or not raw:
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            "branch_fork requires sequence_order.",
            [{
                "code": "sequence_order_required",
                "path": path,
                "message": "Provide sequence_order using inserted_logic and original_successor.",
            }],
        )
    sequence_order: List[str] = []
    for index, value in enumerate(raw):
        if value in {"inserted_logic", "original_successor"}:
            sequence_order.append(value)
            continue
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            "Invalid branch_fork sequence_order entry.",
            [{
                "code": "sequence_order_invalid",
                "path": f"{path}[{index}]",
                "message": "Use inserted_logic or original_successor.",
            }],
        )
    if "inserted_logic" not in sequence_order:
        raise TaskSpecCompileError(
            "taskspec_semantic_invalid",
            "branch_fork sequence_order must include inserted_logic.",
            [{
                "code": "sequence_order_invalid",
                "path": path,
                "message": "Include inserted_logic.",
            }],
        )
    return sequence_order


def _patch_value_to_string(value: Any) -> str:
    if isinstance(value, str):
        return value
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return str(value)
    if value is None:
        return ""
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def _copy_optional_string_fields(source: Dict[str, Any], target: Dict[str, Any], fields: List[str]) -> None:
    for field in fields:
        value = source.get(field)
        if isinstance(value, str) and value:
            target[field] = value


def _assert_allowed_string(value: str, path: str, allowed: set[str], message: str) -> None:
    if value in allowed:
        return
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path} is not supported.",
        [{
            "code": "unsupported_field_value",
            "path": path,
            "message": message,
        }],
    )


def _literal_record_values(record: Dict[str, Any]) -> Dict[str, Any]:
    return {key: _literal_value(value) for key, value in record.items()}


def _required_object(record: Dict[str, Any], field: str, path: str) -> Dict[str, Any]:
    value = record.get(field)
    if isinstance(value, dict):
        return value
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        f"{path} must be an object.",
        [{
            "code": "missing_required_object",
            "path": path,
            "message": f"{path} must be an object.",
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
        if strategy == "member_variables" and kind is None and "op" not in variable:
            _require_pin_type_alias(variable, f"{path}[{variable_index}]")
            continue
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
            _compile_member_variable_change(entry, f"behavior.{ 'changes' if 'changes' in behavior else 'variables' }[{index}]")
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


def _compile_blueprint_variable_steps(target: Dict[str, Any], behavior: Dict[str, Any]) -> List[Dict[str, Any]]:
    strategy = behavior["variable_strategy"]
    if strategy != "member_variables":
        step_target = {"asset_path": target["asset_path"]}
        if strategy == "local_variables":
            step_target["function_name"] = _required_string(behavior, "function_name", "behavior.function_name")
        return [_blueprint_variable_step("step_001", step_target, strategy, _compile_blueprint_variable_ops(behavior))]

    entries = behavior.get("changes")
    path_prefix = "behavior.changes"
    if entries is None:
        entries = behavior.get("variables", [])
        path_prefix = "behavior.variables"

    member_ops = [
        _compile_member_variable_change(entry, f"{path_prefix}[{index}]")
        for index, entry in enumerate(entries)
    ]
    default_ops = [
        op
        for index, entry in enumerate(entries)
        for op in [_compile_member_default_from_variable_entry(entry, f"{path_prefix}[{index}]")]
        if op is not None
    ]

    step_target = {"asset_path": target["asset_path"]}
    steps = [_blueprint_variable_step("step_001", step_target, "member_variables", member_ops)]
    if default_ops:
        default_step = _blueprint_variable_step("step_002", step_target, "member_defaults", default_ops)
        default_step["depends_on"] = ["step_001"]
        steps.append(default_step)
    return steps


def _blueprint_variable_step(
    step_id: str,
    target: Dict[str, Any],
    strategy: str,
    ops: List[Dict[str, Any]],
) -> Dict[str, Any]:
    return {
        "step_id": step_id,
        "capability": "blueprint_variable",
        "target": target,
        "write": {
            "strategy": strategy,
            "ops": ops,
        },
        "constraints": {
            "allow_remove_referenced_variables": False,
        },
    }


def _compile_member_variable_change(change: Dict[str, Any], path: str) -> Dict[str, Any]:
    if "op" in change and "kind" not in change:
        op = dict(change)
        if "variable_type" in op and "pin_type" not in op:
            op["pin_type"] = op.pop("variable_type")
        return op

    if "kind" not in change:
        op = _copy_known_fields(change, ["name", "category", "tooltip", "flags", "metadata", "name_collision"])
        op["op"] = "ensure_member_variable"
        op["pin_type"] = _variable_pin_type(change, path)
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


def _compile_member_default_from_variable_entry(change: Any, path: str) -> Optional[Dict[str, Any]]:
    if not isinstance(change, dict) or "default" not in change:
        return None
    kind = change.get("kind")
    op = change.get("op")
    if kind not in {None, "ensure_member_variable"} or op not in {None, "ensure_member_variable"}:
        return None
    return {
        "op": "set_member_default",
        "name": _required_string(change, "name", f"{path}.name"),
        "value": _literal_value(change.get("default")),
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
    if isinstance(record.get("type"), str) and record["type"].strip():
        return {"category": record["type"]}
    raise TaskSpecCompileError(
        "taskspec_semantic_invalid",
        "Blueprint variable type is required.",
        [{
            "code": "missing_variable_pin_type",
            "path": f"{path}.type",
            "message": 'Provide type or variable_type, for example {"category":"bool"}.',
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
    if kind == "component_bound_event":
        node = dict(statement)
        node["id"] = node_id
        node["kind"] = "component_bound_event"
        return node
    delegate_operation = _delegate_statement_operation(statement)
    if delegate_operation is not None:
        node = dict(statement)
        node["id"] = node_id
        node["kind"] = "delegate"
        node["delegate_operation"] = delegate_operation
        if delegate_operation == "unbind":
            node["unbind_mode"] = "single"
        elif delegate_operation == "clear":
            node["unbind_mode"] = "all"
        if delegate_operation == "call" and isinstance(statement.get("args"), dict):
            node["args"] = _compile_args(statement.get("args"))
        return node

    if kind == "call":
        return {
            "id": node_id,
            "kind": "call",
            "function": _required_string(statement, "target", f"{path}.target"),
            "inputs": _compile_args(statement.get("args")),
        }

    if kind == "set":
        operation, scope = FIELD_STATEMENT_KIND_MAP[kind]
        node = {
            "id": node_id,
            "var": _required_string(statement, "target", f"{path}.target"),
            "target": _required_string(statement, "target", f"{path}.target"),
            "value": _value_expr_to_string(statement.get("value")),
        }
        _apply_field_taxonomy(node, operation, scope)
        return node

    if kind == "set_property":
        property_path = _required_graph_body_property_path(statement, path)
        operation, scope = FIELD_STATEMENT_KIND_MAP[kind]
        node = {
            "id": node_id,
            "target": _required_string(statement, "target", f"{path}.target"),
            "property_path": property_path,
            "property": property_path,
            "value": _value_expr_to_string(statement.get("value")),
        }
        _apply_field_taxonomy(node, operation, scope)
        return node

    if kind == "field":
        operation, scope = _field_operation_scope(statement, path)
        if operation != "set":
            raise TaskSpecCompileError(
                "unsupported_field_operation",
                "Field statements require field_operation=set.",
                [{
                    "code": "unsupported_field_operation",
                    "path": f"{path}.field_operation",
                    "message": "Field statements require field_operation=set.",
                }],
            )
        target = _required_string(statement, "target", f"{path}.target")
        node = {
            "id": node_id,
            "var": target,
            "target": target,
            "value": _value_expr_to_string(statement.get("value")),
        }
        if scope == "property_path":
            property_path = _required_graph_body_property_path(statement, path)
            node["property_path"] = property_path
            node["property"] = property_path
        _apply_field_taxonomy(node, operation, scope)
        return node

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
) -> List[Dict[str, Any]]:
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

    flow = _compile_statement_sequence(body["statements"], f"{_to_id_segment(entry_name)}_stmt", f"{path}.body.statements", _make_compile_flow_context())
    nodes.extend(flow["nodes"])
    links.extend(flow["links"])
    if flow.get("entry"):
        links.append({"kind": "exec", "from": f"{entry_id}.then", "to": flow["entry"]})
    return _clone_logic_statement_sequence_with_compiled_ids(body["statements"], f"{_to_id_segment(entry_name)}_stmt")


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

