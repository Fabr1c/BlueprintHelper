# 07 - Safety Validation And Recovery

## Safety Levels

| Level | Operation | Default requirement |
|---|---|---|
| S0 | Read-only | May run after target is clear |
| S1 | Add variable, component, widget, or row | Read current state, preview, execute, read back |
| S2 | Modify properties, defaults, or graph flow | Read current state, preview, execute minimal change, validate |
| S3 | Delete, rename, or batch mutation | Read references, preview impact, execute only after blocker-free preview |

## Preview Gate

Preview is the write gate for every S1-S3 write. If preview returns `preview_blocked`, `context_required`, `context_stale`, or `failed`, do not execute; either repair the TaskSpec, refresh context, or stop and report.

For GraphWrite branch insertion into an owned block, preview must verify:

- The anchor is a block-scoped LogicJson anchor from a BlueprintHelper-owned block.
- ordering policy matches the current template.
- the inserted target resolves to an existing BlueprintHelper-owned callable block.

If any of these checks block, execution is forbidden. A successful preview is still not a guarantee that execute will succeed, because the UE asset may change or Editor state may fail during write; execute failures must return a non-empty error code/message/stage that the Agent can report.

## Write Session Gate

Preview and write authorization are separate gates. After preview succeeds, if runtime profile or execute preflight reports missing write permission, call `blueprinthelper_request_write_session`. The Editor approval UI is intentionally minimal: accept or reject. If the user rejects, stop and report the denied write session. Approval is scoped to the running Editor/Bridge for the approved scope and lifetime, so SideAgents can continue the tool chain without receiving raw session data. Do not fall back to `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or direct `auth_session` handling.

## Source Control Gate

Source-control checkout is separate from write authorization. In P4/Perforce or other UE source-control projects, run `blueprinthelper_source_control_status` or `blueprinthelper_source_control_checkout` after preview and before execute when target assets may be read-only or when close/save reports `checkout_required`. Stop and report on `checked_out_by_other`, `source_control_conflicted`, `source_control_unavailable`, `checkout_failed`, or `not_editable`.

## Validation

Use the compile/save validation policy from the current CLI-discovered template. If validation fails, stop further writes and report the failing asset, stage, and diagnostic summary.

When compile or save is an explicit tool step, discover the tool through `bh tools list blueprint diagnose --format json` or `bh tools list editor write --format json`, fetch the matching template with `bh tools templates`, and run the returned `allowed_tools` only. `blueprint_compile_blueprint` should target a specific Blueprint asset even though the Bridge accepts an empty compatibility payload. `blueprint_save_asset` must target a specific `asset_path`, requires write-session authorization, and must not bypass source-control/editability stops.

## Recovery

If a write fails:

- Prefer task result and journal data from `blueprinthelper_get_task_result`.
- Report which steps ran and which were blocked.
- Stay on the TaskSpec-first recovery path unless the user explicitly requests expert recovery.

## Read-back

After writing, do at least one relevant read-back:

- Blueprint logic summary or structured anchors.
- Variable, component, class setting, Widget, object property, or DataTable target context.
- Task result journal for status and validation.
- For GraphWrite `branch_fork`, confirm the Sequence or equivalent distribution node is connected, the inserted call is reachable, the original successor remains reachable, and the affected flow has no orphaned nodes.

## Report Format

Report status, target, task summary, main changes, validation result, and
remaining risks. Do not copy raw TaskSpec fields or runtime payloads into the
ordinary user report.
