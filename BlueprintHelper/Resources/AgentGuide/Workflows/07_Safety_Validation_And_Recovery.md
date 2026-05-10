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

For GraphWrite `merge_owned_graph` using `branch_fork + owned_block_call`, preview must verify:

- The anchor is a block-scoped LogicJson anchor from a BlueprintHelper-owned block.
- `sequence_order` is explicit and uses only `original_successor` and `inserted_logic`.
- `inserted.block_id` resolves to an existing BlueprintHelper-owned CustomEvent block that can be called.

If any of these checks block, execution is forbidden. A successful preview is still not a guarantee that execute will succeed, because the UE asset may change or Editor state may fail during write; execute failures must return a non-empty error code/message/stage that the Agent can report.

## Write Session Gate

Preview and write authorization are separate gates. After preview succeeds, if runtime profile or execute preflight reports missing write permission, call `blueprinthelper_request_write_session`. The Editor approval UI is intentionally minimal: accept or reject. If the user rejects, stop and report the denied write session. Do not fall back to `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or direct `auth_session` handling.

## Validation

Use TaskSpec `validation.should_compile` and `validation.should_save`. If validation fails, stop further writes and report the failing asset, stage, and diagnostic summary.

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

```text
Status: completed / preview_blocked / failed
Target: asset path
Task: task_type or feature_name
Changes: concise list
Validation: compile/save/read-back status
Remaining risks: concise list
```
