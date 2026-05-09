# BlueprintHelper P1 Remaining Gap Smoke (2026-05-07)

2026-05-09 清理: 本文件保留 P1 gap smoke 的历史失败、rerun 证据和 fixture 细节。当前 P1/P2 全线测试入口已迁入 `Develop/Plan/BlueprintHelper_Unified_SmokeRun_Verification_20260509.md`。

## Purpose

This smoke targets the remaining P1 verification gaps that were not covered by the 2026-05-06 Rerun 4 smoke:

```text
1. branch_fork merge strategy UE smoke
2. append_after + custom_event_call empty error normalization
3. ClassSettings / UMGWidget / DataTable disposable fixture execute smoke
```

The smoke must stay TaskSpec-first:

```text
Agent
-> blueprinthelper_preview_task / blueprinthelper_execute_task
-> BlueprintHelper.TaskSpec.v1
-> MCP/Python compiler
-> BlueprintHelper.TaskPlan.v1 structured IR
-> Bridge preview_task_plan / execute_task_plan
-> UE Task Runtime
-> existing UE capability clusters
-> TaskRunJournal / read-back
```

Do not call old Agent-facing atomic write tools in this smoke. Low-level read/debug tools may be used only for final read-back when `blueprinthelper_read_context` does not yet cover the target read domain.

## Required Default Tools

```text
blueprinthelper_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_open_editor
blueprinthelper_close_editor
```

## Non-goals

- Do not edit production gameplay assets.
- Do not test non-BlueprintHelper-owned graph anchors.
- Do not use global undo/redo as a pass criterion.
- Do not test P2 Signature / ObjectProperty / CleanupOwnership here.
- Do not require `should_save=true`; all examples use `should_save=false`.

## Preconditions

1. Unreal Editor is running for:

```text
G:/UnrealPractise/MrStone/MrStone.uproject
```

2. Bridge is reachable.
3. Use disposable assets only.
4. Use fresh graph/event/widget/row names for every execute run.
5. Run `blueprinthelper_preview_task` before every execute. If preview is blocked, do not force execute.

## Placeholder Table

| Placeholder | Meaning |
|---|---|
| `_BP_SMOKE_` | Disposable Actor Blueprint, for example `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke` |
| `_GRAPH_NAME_` | Existing BlueprintHelper-owned smoke graph from Append/Replace smoke |
| `_BLOCK_ID_` | `block_id` from grouped LogicJson for the owned block |
| `_GROUP_ENTRY_NODE_PATH_` | `group_entry_node_path` from grouped LogicJson |
| `_GROUP_NODE_REF_` | Group-local `node_ref`, for example `nodes[1]` |
| `_GROUP_PIN_REF_` | Group-local `pin_ref`, for example `then` |
| `_GROUP_LINK_REF_` | Group-local `link_ref`, only needed for `insert_between` |
| `_INSERTED_BLOCK_ID_` | Existing BlueprintHelper-owned block id to call from `branch_fork` |
| `_CUSTOM_EVENT_NAME_` | Existing callable custom event name |
| `_BPI_SMOKE_` | Disposable Blueprint Interface asset |
| `_WBP_SMOKE_` | Disposable Widget Blueprint |
| `_DT_SMOKE_` | Disposable DataTable |
| `_TASK_RUN_ID_` | Result id from `blueprinthelper_execute_task` |

## Level 0: Local Contract Regression

Run from:

```text
G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\BlueprintHelper_MCP_Server
```

Command:

```powershell
npm.cmd test
```

Pass criteria:

- TypeScript tests pass.
- Python tests pass.
- Contract tests still reject raw TaskPlan language in Agent-authored TaskSpec.
- `validation.should_compile` / `validation.should_save` remain the only accepted validation fields.
- GraphWrite `branch_fork` remains modeled as `merge_owned_graph -> behavior.merges[]`.
- P1 ClassSettings / UMGWidget / DataTable fixtures still compile to `blueprint_class_settings`, `umg_widget`, and `data_table` TaskPlan capability steps.

This level does not require Unreal Editor.

## Level 1: Editor And Bridge Preflight

### 1. Runtime Profile

Call `blueprinthelper_get_runtime_profile`.

Pass criteria:

- Tool succeeds.
- Bridge is reachable.
- If profile still reports stale GraphWrite not-implemented flags, record it as `profile_stale`, but do not fail this smoke if preview/execute proves the path is available.

### 2. Diagnostics

Call `blueprinthelper_diagnostics`.

Pass criteria:

- No fatal diagnostics block editor asset operations.
- Warnings are recorded.

### 3. Agent Guide

Call `blueprinthelper_read_agent_guide`.

Pass criteria:

- Returned document is the AgentGuide onboarding index.
- It points to TaskSpec / ReadSpec flow.
- It does not tell ordinary Agents to call atomic write tools as the main path.

## Level 2: Graph Anchor Preparation

Before running branch and append-after cases, read grouped LogicJson for the target owned graph:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "graph",
    "target_name": "_GRAPH_NAME_"
  },
  "view": {
    "format": "logic_json",
    "detail": "debug",
    "max_items": 300
  }
}
```

Pass criteria:

- `data.schema = ReadContextPack.v1`.
- `data.payload.schema = LogicJson.v1`.
- At least one grouped BlueprintHelper-owned block exists.
- The selected group contains:
  - `block_id`
  - `group_entry_node_path`
  - group-local `node_ref`
  - group-local `pin_ref`
  - group-local `link_ref` if using `insert_between`
- Do not use whole-graph `nodes[index]` outside a grouped block as a write anchor.
- Do not use GUID-first selectors as the ordinary smoke path.

If no owned block exists, first create one through the existing Append/Replace smoke. Record this level as `blocked_by_fixture` until the owned block is available.

## Level 3: Merge branch_fork Smoke

### Intent

Verify `merge_owned_graph` with `insert_strategy = branch_fork` reaches UE preview and execute with a useful result.

The goal is not to invent a new Agent-facing tool. The Agent sends TaskSpec only; the compiler emits `graph_write` IR; UE runtime lowers to `merge_blueprint_graph`.

### Fixture Requirement

The graph needs:

1. An existing BlueprintHelper-owned source block with a valid `block_id`.
2. A second existing BlueprintHelper-owned block that can be called by `owned_block_call`.
3. A source anchor node with an exec output pin.

If `_INSERTED_BLOCK_ID_` cannot be prepared, record `blocked_by_fixture`.

### Preview

Call `blueprinthelper_preview_task`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_p1_gap_branch_fork_20260507",
    "task_type": "edit_blueprint_graph",
    "feature_name": "P1GapBranchFork",
    "target": {
      "asset_path": "_BP_SMOKE_",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "_GRAPH_NAME_",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "merge_owned_graph",
      "merges": [
        {
          "kind": "insert_flow",
          "scope": "owned_block_call",
          "insert_strategy": "branch_fork",
          "anchor": {
            "block_id": "_BLOCK_ID_",
            "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
            "node_ref": "_GROUP_NODE_REF_",
            "pin_ref": "_GROUP_PIN_REF_"
          },
          "inserted": {
            "call_kind": "owned_block_call",
            "block_id": "_INSERTED_BLOCK_ID_"
          },
          "sequence_order": [
            "inserted_logic",
            "original_successor"
          ]
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": false
    }
  }
}
```

Preview pass criteria:

- `operation = preview_task`.
- `status = dry_run`.
- `modified = false`.
- `data.passed = true`.
- `data.blocked = false`.
- TaskPlan summary contains `capability = graph_write`.
- TaskPlan summary contains `strategy = owned_graph_edit`.
- The structural op is `insert_flow`.
- No Agent-authored request field contains `merge_blueprint_graph`.

Accepted blocked result:

- If branch fork is not implemented or the fixture is invalid, preview may block.
- Blocked preview must include a non-empty error `code` and `message`.
- Empty `preview_task failed: , modified=false.` is a smoke failure.

### Execute

Execute only if preview passes.

Pass criteria:

- `operation = execute_task`.
- `modified = true` only after write succeeds.
- `data.schema = TaskRunSummary.v1` or `TaskRunJournal.v1`.
- `task_run_id` is present.
- Child result or journal records `capability = graph_write`.
- Runtime details may mention `adapter_operation = merge_blueprint_graph`, but this remains internal/journal data.
- Compile succeeds if `validation.should_compile = true`.

### Read-back

Read the graph as LogicMD and LogicJson:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "graph",
    "target_name": "_GRAPH_NAME_"
  },
  "view": {
    "format": "logic_md",
    "detail": "normal",
    "max_items": 300
  },
  "context": {
    "task_run_id": "_TASK_RUN_ID_"
  }
}
```

Pass criteria:

- LogicMD/LogicJson show that the inserted owned block call is attached to the target flow.
- Existing original successor remains reachable according to `sequence_order`.
- No orphan nodes are introduced for the modified owned block.

## Level 4: append_after + custom_event_call Error Detail Smoke

### Intent

This level is allowed to remain blocked. The required fix is that failure becomes diagnosable. The smoke fails only when preview/execute returns an empty error.

### Preview

Call `blueprinthelper_preview_task`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_p1_gap_append_after_custom_event_call_20260507",
    "task_type": "edit_blueprint_graph",
    "feature_name": "P1GapAppendAfterCustomEventCall",
    "target": {
      "asset_path": "_BP_SMOKE_",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "_GRAPH_NAME_",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "merge_owned_graph",
      "merges": [
        {
          "kind": "insert_flow",
          "scope": "custom_event_call",
          "insert_strategy": "append_after",
          "anchor": {
            "block_id": "_BLOCK_ID_",
            "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
            "node_ref": "_GROUP_NODE_REF_",
            "pin_ref": "_GROUP_PIN_REF_"
          },
          "inserted": {
            "call_kind": "custom_event_call",
            "name": "_CUSTOM_EVENT_NAME_"
          }
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": false
    }
  }
}
```

Pass criteria, if preview passes:

- `data.passed = true`.
- `data.blocked = false`.
- TaskPlan summary contains `capability = graph_write`.
- Execute may proceed and must satisfy the same GraphWrite journal/read-back rules as Level 3.

Pass criteria, if preview blocks:

- `data.blocked = true`, or equivalent task-level failed preview result.
- Error has non-empty:
  - `code`
  - `message`
  - `stage` when available
  - path or context identifying the merge op when available
- Suggested acceptable codes include:
  - `unsupported_insert_scope`
  - `unsupported_custom_event_call_append_after`
  - `anchor_exec_pin_already_connected`
  - `target_event_not_found`
  - `merge_preflight_failed`

Failure criteria:

- Empty error text.
- `preview_task failed: , modified=false.`
- `ok=false` with no task-level `error.code`.
- Bridge/UE failure swallowed by MCP without normalized diagnostic details.

### Execute

Execute only if preview passes.

Pass criteria:

- Custom event call node is appended after the anchor.
- Compile succeeds.
- Read-back shows the inserted call and no new orphan nodes.

## Level 5: BlueprintClassSettings Disposable Execute Smoke

### Fixture Requirement

Required disposable assets:

```text
_BP_CLASS_SMOKE_ = /Game/BlueprintHelper/Smoke/BP_ClassSettingsSmoke
_BPI_SMOKE_      = /Game/BlueprintHelper/Smoke/BPI_ClassSettingsSmoke
```

The Blueprint should be disposable and derived from Actor. The interface must also be disposable. If either asset is missing, record `blocked_by_fixture`.

### Preview / Execute TaskSpec

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_p1_gap_class_settings_execute_20260507",
    "task_type": "edit_blueprint_class_settings",
    "feature_name": "P1GapClassSettingsExecute",
    "target": {
      "asset_path": "_BP_CLASS_SMOKE_",
      "target_type": "blueprint"
    },
    "behavior": {
      "class_settings_strategy": "class_settings",
      "interfaces": {
        "ensure_present": [
          "_BPI_SMOKE_"
        ]
      },
      "class_defaults": [
        {
          "property_path": "bCanBeDamaged",
          "value": true
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": false
    }
  }
}
```

Preview pass criteria:

- `capability = blueprint_class_settings`.
- `strategy = class_settings`.
- Separate TaskPlan steps are acceptable for interface and class default operations.
- `modified = false`.

Execute pass criteria:

- `task_run_id` is returned.
- Journal step statuses are `completed`.
- The interface is implemented on the disposable Blueprint.
- The class default property mutation is reported as applied or no-op if already set.
- Compile succeeds.
- Asset is not saved.

Read-back:

- Prefer `blueprinthelper_read_context` when class settings read domain is available.
- If not available, use an internal/debug read command only for smoke inspection and record which read path was used.

## Level 6: UMGWidget Disposable Execute Smoke

### Fixture Requirement

Required disposable asset:

```text
_WBP_SMOKE_ = /Game/BlueprintHelper/Smoke/WBP_WidgetSmoke
```

The Widget Blueprint must have:

- A root widget named `Root`.
- A disposable legacy text widget named `BH_LegacyText_20260507`, if testing remove.

If the root widget or asset is missing, record `blocked_by_fixture`.

### Preview / Execute TaskSpec

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_p1_gap_umg_execute_20260507",
    "task_type": "edit_umg_widget",
    "feature_name": "P1GapUMGExecute",
    "target": {
      "asset_path": "_WBP_SMOKE_",
      "target_type": "widget_blueprint"
    },
    "behavior": {
      "widget_strategy": "widget_blueprint_edit",
      "changes": [
        {
          "kind": "create_widget",
          "widget_class": "TextBlock",
          "widget_name": "BH_SmokeTitle_20260507",
          "parent_widget_name": "Root"
        },
        {
          "kind": "update_widget_property",
          "widget_name": "BH_SmokeTitle_20260507",
          "property_path": "Text",
          "value": "BlueprintHelper UMG smoke"
        },
        {
          "kind": "delete_widget",
          "widget_name": "BH_LegacyText_20260507"
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": false
    }
  }
}
```

Preview pass criteria:

- `capability = umg_widget`.
- Widget tree edit and widget property edit are represented as capability steps.
- `modified = false`.
- If remove target is absent, the preview must block with a clear missing widget error, or the remove case should be skipped by using a fixture that contains the legacy widget.

Execute pass criteria:

- `task_run_id` is returned.
- Journal step statuses are `completed`.
- `BH_SmokeTitle_20260507` exists under `Root`.
- Its `Text` property is set to `BlueprintHelper UMG smoke`.
- `BH_LegacyText_20260507` is removed if it existed.
- Compile succeeds if applicable.
- Asset is not saved.

Read-back:

- Prefer `blueprinthelper_read_context` when `widget_context` is available.
- Otherwise use an internal/debug read command only for smoke inspection and record the read path.

## Level 7: DataTable Disposable Execute Smoke

### Fixture Requirement

Required disposable DataTable:

```text
_DT_SMOKE_ = /Game/BlueprintHelper/Smoke/DT_DataTableSmoke
```

The DataTable row struct must contain at least:

```text
Damage
Ammo
```

The table should contain:

```text
BH_UpdateRow_20260507
BH_DeleteRow_20260507
```

If the table, row struct, or fixture rows are missing, record `blocked_by_fixture`.

### Preview / Execute TaskSpec

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_p1_gap_data_table_execute_20260507",
    "task_type": "edit_data_table",
    "feature_name": "P1GapDataTableExecute",
    "target": {
      "asset_path": "_DT_SMOKE_",
      "target_type": "data_table"
    },
    "behavior": {
      "row_strategy": "row_edit",
      "rows": [
        {
          "action": "add",
          "row_name": "BH_AddRow_20260507",
          "fields": {
            "Damage": "12",
            "Ammo": "10"
          }
        },
        {
          "action": "update",
          "row_name": "BH_UpdateRow_20260507",
          "fields": {
            "Ammo": "16"
          }
        },
        {
          "action": "delete",
          "row_name": "BH_DeleteRow_20260507"
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": false,
      "should_save": false
    }
  }
}
```

Preview pass criteria:

- `capability = data_table`.
- `strategy = row_edit`.
- Add, update, and delete operations may appear as separate TaskPlan steps.
- `modified = false`.
- Do not use `behavior.rows[].op`; DataTable TaskSpec uses `action`.

Execute pass criteria:

- `task_run_id` is returned.
- Journal step statuses are `completed`.
- `BH_AddRow_20260507` exists with the requested fields.
- `BH_UpdateRow_20260507` has updated `Ammo`.
- `BH_DeleteRow_20260507` no longer exists.
- Asset is not saved.

Read-back:

- Prefer `blueprinthelper_read_context` when `data_table_context` is available.
- Otherwise use an internal/debug DataTable row read only for smoke inspection and record the read path.

## Level 8: Result Recording Template

Copy this block into the bottom of the document after the smoke run.

```text
Smoke run:
Date:
Editor project:
MCP server version:
Target assets:

Level 0 npm test:
Level 1 preflight:
Level 2 anchor preparation:
Level 3 branch_fork:
Level 4 append_after + custom_event_call error detail:
Level 5 ClassSettings execute:
Level 6 UMG execute:
Level 7 DataTable execute:

Task run ids:
- branch_fork:
- append_after_custom_event_call:
- class_settings:
- umg:
- data_table:

Blocked by fixture:
- None recorded.

Known implementation failures:
- None recorded.

Empty-error failures:
- None recorded.

Read-back method used:
- GraphWrite:
- ClassSettings:
- UMG:
- DataTable:
```

## Pass Summary Rules

Mark the smoke `PASS` only if:

- Branch fork preview/execute/read-back passes, or a clearly unsupported state is reported with useful diagnostics and is intentionally accepted for the run.
- `append_after + custom_event_call` no longer returns an empty error. It may pass or block, but must be diagnosable.
- ClassSettings execute passes against disposable fixtures.
- UMG execute passes against disposable fixtures.
- DataTable execute passes against disposable fixtures.
- No old atomic write MCP tool is used as a substitute for TaskSpec execution.

Mark the smoke `PARTIAL` if:

- Any fixture is missing and recorded as `blocked_by_fixture`.
- A capability returns a clear, actionable blocked preview.
- Read-back requires debug read tools because `blueprinthelper_read_context` does not yet cover that domain.

Mark the smoke `FAIL` if:

- Preview is skipped before execute.
- Any execute writes production assets.
- Any failure returns an empty task-level error.
- A low-level write tool is used to bypass TaskSpec.
- `behavior.entries` is used for replace/patch/merge.
- DataTable uses `behavior.rows[].op` instead of `action`.

---

## Level 8: Result Recording — EXECUTED

Smoke run: 2026-05-07
Date: 2026-05-07
Editor project: G:/UnrealPractise/MrStone/MrStone.uproject
MCP server version: 0.3.8
Target assets: /Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke (exists), BP_ClassSettingsSmoke (missing), BPI_ClassSettingsSmoke (missing), WBP_WidgetSmoke (missing), DT_DataTableSmoke (missing)

### Level 0 npm test: PASS
- TypeScript build: OK
- Python tests: 37/37 OK
- Node tests: 128/128 passed, 0 failures
- Contract tests reject raw TaskPlan language in Agent-authored TaskSpec ✓
- validation.should_compile / should_save remain the only accepted validation fields ✓
- GraphWrite branch_fork modeled as merge_owned_graph → behavior.merges[] ✓
- P1 ClassSettings / UMGWidget / DataTable fixtures compile to correct capability steps ✓

### Level 1 preflight: PASS
- 1.1 Runtime Profile: degraded. graph_write.merge = not_implemented. Recorded as profile_stale — do not block smoke.
- 1.2 Diagnostics (static): No blocking. Warnings: skill_entry.invalid, version.invalid.
- 1.2 Diagnostics (runtime): No blocking. Editor running, Bridge connected, all clear.
- 1.3 Agent Guide: Returns TaskSpec-first onboarding index. Points to TaskSpec/ReadSpec flow. Does not direct to atomic write tools ✓

### Level 2 anchor preparation: PASS
- Two BlueprintHelper-owned blocks identified in BH_TaskSpecSmoke_20260504_001 and BH_Smoke_Rerun_20260505.
- Block IDs: BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010, BH_Smoke_Rerun_20260505_BH_SmokeRerunEvent_202605050
- Group-local anchors resolved: node_ref=nodes[0], pin_ref=then, group_entry_node_path present.
- ReadContextPack.v1 + LogicJson.v1 schema verified ✓

### Level 3 branch_fork: PARTIAL
- Preview: PASS (passed=true, blocked=false, capability=graph_write, strategy=owned_graph_edit, ops=1 insert_flow)
- **Execute: FAIL — empty error: "execute_task failed: , modified=false."**
- Read-back: Confirmed no modifications applied to graph (4 nodes, 2 exec links intact)
- Target graph: BH_TaskSpecSmoke_20260504_001 (4 nodes)

### Level 4 append_after + custom_event_call error detail: PASS (diagnosable blocked)
- Preview: Blocked with non-empty diagnostic ✓
- code: anchor_exec_pin_already_connected ✓
- message: "append_after 要求 Anchor Pin 没有后继。" ✓
- path: anchor ✓
- Empty-error anti-pattern FIXED for preview ✓
- Execute: Skipped (preview blocked — correct behavior)

### Level 5 ClassSettings execute: blocked_by_fixture
- BP_ClassSettingsSmoke: missing (search returns 0 results)
- BPI_ClassSettingsSmoke: missing (search returns 0 results)
- create_asset preview: blocked with code=unsupported_asset_type, message="Unsupported asset_type: Actor" — diagnostic present ✓
- create_blueprint_feature preview: empty error returned (anti-pattern)

### Level 6 UMG execute: blocked_by_fixture
- WBP_WidgetSmoke: missing (search returns 0 results)

### Level 7 DataTable execute: blocked_by_fixture
- DT_DataTableSmoke: missing (search returns 0 results)

### Task run ids:
- branch_fork preview: preview_1778119605478_0001
- branch_fork execute: FAILED (no task_run_id returned)
- append_after_custom_event_call preview: preview_1778119652628_0003
- class_settings: N/A (blocked_by_fixture)
- umg: N/A (blocked_by_fixture)
- data_table: N/A (blocked_by_fixture)

### Blocked by fixture:
- Level 5: BP_ClassSettingsSmoke, BPI_ClassSettingsSmoke
- Level 6: WBP_WidgetSmoke
- Level 7: DT_DataTableSmoke

### Known implementation failures:
- Level 3 execute: graph_write branch_fork not implemented at UE runtime (confirmed by Runtime Profile: merge=not_implemented)
- create_blueprint_feature preview: returns empty error (same anti-pattern)

### Empty-error failures:
- **Level 3 execute: "execute_task failed: , modified=false."** ← CRITICAL
- Level 3 get_task_result: "get_task_result failed: , modified=false." (empty error)
- create_blueprint_feature preview: "preview_task failed: , modified=false." (empty error)

### Read-back method used:
- GraphWrite: blueprinthelper_read_context (ReadSpec.v1 → LogicMd/LogicJson) ✓ — no debug tools needed
- ClassSettings: N/A (no fixture)
- UMG: N/A (no fixture)
- DataTable: N/A (no fixture)

### Overall Verdict: FAIL

Reasoning:
- PASS rules violated: Level 3 execute returns empty task-level error (FAIL criterion). Level 5-7 fixtures missing (PARTIAL criterion).
- The single most impactful finding: branch_fork preview compiles correctly through the full pipeline (Agent → TaskSpec → Python compiler → TaskPlan → Bridge → UE preview), but the UE runtime lacks the merge capability. The execute failure is expected, but the **empty error message** prevents diagnosis.
- Level 4 represents a clear improvement: append_after + custom_event_call previously returned empty errors; now it returns actionable diagnostics (anchor_exec_pin_already_connected).
- No old atomic write tools were used ✓
- No production assets were modified ✓

### Post-smoke source fix: 2026-05-07

- MCP empty Bridge message fallback fixed: task wrappers now ignore empty nested `error.message` and use the operation fallback message.
- Bridge TaskRuntime preview / execute / get-journal entry points now emit a non-empty top-level fallback if the UE `ToolResultBase` error message is empty.
- `branch_fork + owned_block_call` source path integrated: MergeService resolves an existing BlueprintHelper-owned CustomEvent block, creates a call node, inserts a Sequence node, and preserves `sequence_order`.
- Runtime profile source no longer lists `graph_write.merge = not_implemented` when write permission is otherwise enabled.
- Verification: `npm.cmd test` passed after the MCP fix. Project-level `Build.bat` was attempted but stopped before plugin C++ compilation because Codex cannot rename/write MrStone and sibling-plugin `Intermediate` files.
- Follow-up: rebuild/reload UE locally, then rerun Level 1 runtime profile and Level 3 `branch_fork` execute/read-back smoke.
