# BlueprintHelper Agent Profile Schema

文档版本：2026-04-30

This document defines `.blueprinthelper/agent-profile.json`. The profile stores project and user preferences for BlueprintHelper-aware agents and future MCP guard resources.

## File Location

```text
.blueprinthelper/agent-profile.json
```

## Top-Level Shape

```json
{
  "version": 1,
  "development_boundary": {},
  "agent_permissions": {},
  "blueprint_style": {},
  "logic_export_policy": {},
  "naming_policy": {},
  "search_policy": {},
  "mutation_policy": {}
}
```

## Field Reference

### `version`

| Property | Value |
|---|---|
| Type | integer |
| Default | `1` |
| Allowed | `1` |
| Impact | Selects this schema version |

### `development_boundary`

| Field | Type | Default | Allowed values | Agent impact |
|---|---|---|---|---|
| `blueprint_responsibilities` | string array | `["presentation","orchestration","gameplay_simple"]` | `presentation`, `orchestration`, `gameplay_simple`, `gameplay_complex`, `prototype_only` | Tasks outside this set should be questioned or routed to C++ |
| `cpp_responsibilities` | string array | `["core_systems","performance_sensitive","networking","save_load"]` | `core_systems`, `performance_sensitive`, `networking`, `save_load`, `math_heavy`, `engine_integration` | Agent suggests C++ when requested behavior fits these areas |
| `prefer_cpp_when_threshold_exceeded` | boolean | `true` | `true`, `false` | If true, complexity threshold violations trigger C++ or refactor suggestions |

### `agent_permissions`

| Field | Type | Default | Allowed values | Agent impact |
|---|---|---|---|---|
| `cpp_edit_policy` | string | `suggest_only` | `never`, `suggest_only`, `ask_before_edit`, `allowed_in_repo` | Controls whether Agent can edit C++ source |
| `blueprint_mutation` | string | `allow_with_plan` | `read_only`, `ask_every_time`, `allow_low_risk`, `allow_with_plan` | Controls Blueprint graph and structure mutations |
| `umg_mutation` | string | `ask_every_time` | `read_only`, `ask_every_time`, `allow_layout_only`, `allow_properties_and_layout` | Controls Widget tree and property mutations |
| `data_asset_mutation` | string | `ask_every_time` | `read_only`, `ask_every_time`, `allow_properties` | Controls UObject property writes |
| `datatable_mutation` | string | `ask_every_time` | `read_only`, `ask_every_time`, `allow_update_only`, `allow_add_update_delete` | Controls DataTable row writes |
| `editor_command_policy` | string | `ask_every_time` | `ask_every_time`, `allow_pie_only`, `allow_build_only`, `allow_lifecycle_tools` | Controls PIE, editor close/open, build, and console commands |

### `blueprint_style`

| Field | Type | Default | Allowed values | Agent impact |
|---|---|---|---|---|
| `complexity_thresholds.max_nodes_per_function` | integer | `30` | positive integer | Logic reads flag functions above this size |
| `complexity_thresholds.max_branches_per_function` | integer | `5` | positive integer | Logic reads flag high branch count |
| `complexity_thresholds.max_loops_per_function` | integer | `2` | positive integer | Logic reads flag loop-heavy graphs |
| `complexity_thresholds.max_cross_asset_calls` | integer | `5` | positive integer | Logic reads flag high coupling |
| `tick_policy` | string | `allow_small` | `forbid`, `allow_small`, `allow_with_comment` | Controls Tick edits and warnings |
| `cast_policy` | string | `prefer_interface` | `avoid`, `allow_when_simple`, `prefer_interface`, `no_preference` | Guides communication node choices |
| `dispatcher_policy` | string | `prefer_for_decoupling` | `prefer_for_ui`, `prefer_for_decoupling`, `avoid_unless_needed` | Guides dispatcher suggestions |

### `logic_export_policy`

| Field | Type | Default | Allowed values | Agent impact |
|---|---|---|---|---|
| `default_format` | string | `logic_md` | `logic_md`, `logic_json`, `raw_json_when_needed` | Selects first read tool |
| `raw_json_policy` | string | `when_importing` | `never_without_reason`, `when_importing`, `when_debugging_links`, `always_allowed` | Restricts raw JSON export |
| `include_node_ids` | string | `true_when_planning_edits` | `false`, `true_when_planning_edits`, `always` | Controls LogicJson node IDs |
| `include_positions` | string | `true_when_layout_debugging` | `false`, `true_when_layout_debugging`, `always` | Controls LogicJson positions |
| `include_raw_node_types` | string | `false` | `false`, `true_when_debugging`, `always` | Controls raw Unreal node type exposure |

### `naming_policy`

| Field | Type | Default | Allowed values | Agent impact |
|---|---|---|---|---|
| `function_style` | string | `VerbObject` | `VerbObject`, `Verb_Object`, `ProjectConvention` | Names generated functions |
| `variable_style` | string | `ProjectConvention` | `bPrefixForBool`, `NoBoolPrefix`, `ProjectConvention` | Names generated variables |
| `dispatcher_style` | string | `OnEventName` | `OnEventName`, `EventNameChanged`, `ProjectConvention` | Names generated dispatchers |
| `documentation_requirement` | string | `recommended` | `required_for_public_api`, `recommended`, `not_required` | Adds category, tooltip, or comments where supported |
| `forbidden_low_semantic_names` | string array | `["DoIt","Handle","Update","Process","Logic","Temp","NewFunction"]` | string array | Blocks low-value generated names |

### `search_policy`

| Field | Type | Default | Allowed values | Agent impact |
|---|---|---|---|---|
| `asset_search_order` | string array | `["asset_path","class_filter_then_name","symbol_index_when_available"]` | `asset_path`, `class_filter_then_name`, `symbol_index_when_available` | Controls asset discovery order |
| `require_exact_asset_path_for_mutation` | boolean | `true` | `true`, `false` | If true, mutations need explicit asset path |
| `allow_active_context_for_read` | boolean | `true` | `true`, `false` | Allows active graph reads |
| `allow_active_context_for_mutation` | boolean | `false` | `true`, `false` | Disallows destructive active-tab assumptions by default |

### `mutation_policy`

| Field | Type | Default | Allowed values | Agent impact |
|---|---|---|---|---|
| `low_risk_confirmation` | string | `not_required_after_plan` | `always`, `when_batching`, `not_required_after_plan` | Controls confirmations for low-risk writes |
| `high_risk_requires_plan` | boolean | `true` | `true` | High-risk writes must be planned |
| `compile_after_mutation` | string | `when_blueprint_changed` | `always`, `when_blueprint_changed`, `ask` | Controls compile step |
| `save_policy` | string | `ask` | `ask`, `save_after_successful_compile`, `never_auto_save` | Controls saving |
| `partial_failure_policy` | string | `rollback` | `rollback`, `allow_partial_only_when_user_asks` | Controls strict import and partial results |
| `default_strict_import` | boolean | `true` | `true`, `false` | Sets default `strict` for import tools |
| `default_allow_partial` | boolean | `false` | `true`, `false` | Sets default `allow_partial` for import tools |

## Complete Conservative Example

```json
{
  "version": 1,
  "development_boundary": {
    "blueprint_responsibilities": ["presentation", "orchestration", "gameplay_simple"],
    "cpp_responsibilities": ["core_systems", "performance_sensitive", "networking", "save_load"],
    "prefer_cpp_when_threshold_exceeded": true
  },
  "agent_permissions": {
    "cpp_edit_policy": "suggest_only",
    "blueprint_mutation": "allow_with_plan",
    "umg_mutation": "ask_every_time",
    "data_asset_mutation": "ask_every_time",
    "datatable_mutation": "ask_every_time",
    "editor_command_policy": "ask_every_time"
  },
  "blueprint_style": {
    "complexity_thresholds": {
      "max_nodes_per_function": 30,
      "max_branches_per_function": 5,
      "max_loops_per_function": 2,
      "max_cross_asset_calls": 5
    },
    "tick_policy": "allow_small",
    "cast_policy": "prefer_interface",
    "dispatcher_policy": "prefer_for_decoupling"
  },
  "logic_export_policy": {
    "default_format": "logic_md",
    "raw_json_policy": "when_importing",
    "include_node_ids": "true_when_planning_edits",
    "include_positions": "true_when_layout_debugging",
    "include_raw_node_types": "false"
  },
  "naming_policy": {
    "function_style": "VerbObject",
    "variable_style": "ProjectConvention",
    "dispatcher_style": "OnEventName",
    "documentation_requirement": "recommended",
    "forbidden_low_semantic_names": ["DoIt", "Handle", "Update", "Process", "Logic", "Temp", "NewFunction"]
  },
  "search_policy": {
    "asset_search_order": ["asset_path", "class_filter_then_name", "symbol_index_when_available"],
    "require_exact_asset_path_for_mutation": true,
    "allow_active_context_for_read": true,
    "allow_active_context_for_mutation": false
  },
  "mutation_policy": {
    "low_risk_confirmation": "not_required_after_plan",
    "high_risk_requires_plan": true,
    "compile_after_mutation": "when_blueprint_changed",
    "save_policy": "ask",
    "partial_failure_policy": "rollback",
    "default_strict_import": true,
    "default_allow_partial": false
  }
}
```

## Validation Rules

- `version` must be present and equal to `1`.
- Unknown top-level fields should be ignored by agents, not treated as errors.
- Unknown enum values should fall back to the default and emit a warning.
- Missing sections should use the conservative defaults in this document.
- Mutating tools should read this profile before writing when a future MCP resource exposes it.

## MCP Resource Plan

The profile is designed to be exposed later as:

```text
blueprint://setup/profile
```

Until that resource exists, agents should read `.blueprinthelper/agent-profile.json` through normal repository tools when working inside the project checkout.
