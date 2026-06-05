# BlueprintHelper TaskSpec / TaskPlan Contract

This document records ownership boundaries for the TaskSpec-first workflow. It
does not contain concrete template payloads, wrapper shapes, field catalogs, or
copyable JSON examples.

Concrete Agent input fields are owned by the current CLI discovery/index output
and the template files it returns. Agents must read those template files before
constructing CLI input.

## Ownership

- Ordinary Agents author `BlueprintHelper.TaskSpec.v1` only.
- AgentFace task-core owns TaskSpec validation, compilation, and TaskPlan
  generation.
- `BlueprintHelper.TaskPlan.v1` is compiler-owned intermediate representation.
  Ordinary Agents must not author it directly.
- UE Task Runtime owns dry-run, execution, Review evidence production,
  transaction/journal facts, compile/save side effects, and runtime diagnostics.
- Raw Bridge payloads and runtime adapter payloads are internal details.

## Source Of Truth

- Current CLI discovery/index output selects concrete tools and template files.
- Template files are the only Agent-facing source for exact request fields.
- `AgentFaceService/task-core/src/task/schema/` and
  `AgentFaceService/task-core/src/task/compiler/` are the implementation source
  for TaskSpec and TaskPlan behavior.
- UE TaskRuntime and its cluster adapters are the implementation source for
  editor-side execution behavior.

## Agent Boundary

Agents should follow this flow:

```text
profile/read preflight
-> use CLI discovery/index to locate one template file
-> copy and edit the template
-> preview
-> repair or stop
-> request source-control checkout or write session only when required
-> execute
-> read task result when needed
```

Docs may state workflow and ownership rules, but must not duplicate template
content. If a workflow needs an exact field, wrapper, enum, strategy, or
payload shape, use the CLI-discovered template file instead of this document.

## Read Boundary

- Asset-domain reads go through ReadSpec-oriented CLI entries.
- Reference reads and function-chain reads remain separate read workflows when
  the task needs dependency or traversal context.
- Capability discovery for read support is local to task-core and does not read
  UE assets.
- Read outputs are read-only context; they must not be treated as TaskSpec
  drafts or write anchors unless the current template and workflow explicitly
  require that read-back form.

## Write Boundary

- Preview is mandatory before execute.
- Preview blockers are authoritative. Agents either repair the TaskSpec using
  current diagnostics or stop and report.
- Write sessions are requested only after successful preview when write
  permission is disabled.
- Source-control checkout is a separate pre-execute gate when target assets may
  be read-only.
- Deprecated MCP ordinary tools, frozen atomic tools, and raw Bridge commands are
  not fallback paths for ordinary writes.

## Result Boundary

Default Agent-facing responses stay compact:

| Entry family | Contract |
|---|---|
| Guide/document entry | Returns documentation text only. |
| Read entries | Return compact ToolResult envelopes plus artifact links for full payloads. |
| Preview entry | Returns preview status, blockers, summary, and artifact links. |
| Execute entry | Returns execution status, task id, summary, and artifact links. |
| Task result entry | Returns task journal/result facts through the supported result surface. |
| Debug/review entries | Return compact ids or summaries unless the user explicitly asks for developer diagnostics. |

Large payloads, raw Bridge data, DebugBundle internals, local artifact contents,
and source content are not expanded in ordinary Agent responses.

## Validation Boundary

Compile/save policy is expressed through the current TaskSpec template and
lowered by task-core into runtime execution policy. Compile/save are execution
pipeline side effects, not independent Agent-authored editor operations.

Legacy `compile` and `save` keys inside validation are rejected; use the current
template fields returned by CLI discovery.

## Extension Policy

New capabilities must add or update:

- CLI discovery metadata and template files.
- task-core schema and compiler coverage.
- UE runtime adapter or service coverage.
- Preview/execute diagnostics and read-back verification.
- Focused tests and, for editor behavior, real E2E validation.

When a concrete field shape changes, update the template files and CLI discovery
surface first. Do not copy the changed shape into docs.
