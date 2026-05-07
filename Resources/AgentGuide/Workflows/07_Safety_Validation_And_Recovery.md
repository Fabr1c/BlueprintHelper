# 07 - Safety Validation And Recovery

## Safety Levels

| Level | Operation | Default requirement |
|---|---|---|
| S0 | Read-only | May run after target is clear |
| S1 | Add variable, component, widget, or row | Read current state, preview, execute, read back |
| S2 | Modify properties, defaults, or graph flow | Read current state, preview, execute minimal change, validate |
| S3 | Delete, rename, or batch mutation | Read references, preview impact, execute only after blocker-free preview |

## Validation

Use TaskSpec `validation.should_compile` and `validation.should_save`. If validation fails, stop further writes and report the failing asset, stage, and diagnostic summary.

## Recovery

If a write fails:

- Prefer task result and journal data from `blueprinthelper_get_task_result`.
- Report which steps ran and which were blocked.
- Do not call frozen recovery tools unless the user explicitly requests expert recovery.

## Read-back

After writing, do at least one relevant read-back:

- Blueprint logic summary or structured anchors.
- Variable, component, class setting, Widget, object property, or DataTable target context.
- Task result journal for status and validation.

## Report Format

```text
Status: completed / preview_blocked / failed
Target: asset path
Task: task_type or feature_name
Changes: concise list
Validation: compile/save/read-back status
Remaining risks: concise list
```
