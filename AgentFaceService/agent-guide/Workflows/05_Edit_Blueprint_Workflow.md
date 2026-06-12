# 05 - Edit Blueprint Workflow

## Goal

Use the TaskSpec-first flow for UE asset edits. Ordinary Agents submit
`BlueprintHelper.TaskSpec.v1`; the canonical AgentFace task-core compiler emits
TaskPlan, and UE Task Runtime executes internal capability clusters.

This workflow document does not duplicate concrete JSON template fields. When an
Agent needs an input shape, use current CLI discovery to locate the matching
template file, copy it, edit placeholders, and run the CLI with `--file`.

## Preflight

Confirm the target asset, graph or equivalent TaskSpec target, ownership scope,
source-control state, compile/save policy, and whether user-owned nodes or
asset creation are allowed before preview.

## Standard Flow

```text
profile
-> bh context read, plus reference context capability when needed
-> select the matching CLI-discovered template file
-> build TaskSpec
-> preview_task
-> repair or stop
-> request_write_session if write_permission is disabled
-> execute_task
-> get_task_result when needed
-> report summary
```

## Rules

- Variable, graph, component, class-settings, signature, UMG, DataTable,
  DataAsset, and object-property writes are expressed through TaskSpec semantics,
  not low-level runtime commands.
- Graph anchors must come from supported read-back context; do not invent anchors
  from display labels, filesystem paths, or local JSON indexes.
- Reference-sensitive removal or rename work must read reference context before
  preview and stop when preview blocks the write.
- If a write session is required, the Editor prompt is accept/reject. Rejection
  stops the write.
- Reports include task status, target assets, main changes, validation status,
  and remaining risk. Do not expand TaskPlan, internal capability payloads, or
  raw Bridge JSON in ordinary reports.
