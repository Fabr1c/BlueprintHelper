# BlueprintHelper Agent Onboarding Index

CLI is the ordinary TaskSpec, ReadSpec, diagnostics, and result-query mainline.
Global MCP owns Unreal Editor open/close lifecycle. Deprecated MCP ordinary
tools are not fallback paths for asset reads, writes, or lifecycle work.
When BlueprintHelper evidence sources disagree, report `evidence_conflict` and
stop. Direct `.uasset`, `.umap`, or other Unreal binary asset reads are not a
fallback fact source.

SideAgent unavailability is controlled by the active installed plugin skill, not
by this shared guide. Codex must report `sideagent_unavailable` when required
Codex SideAgents cannot be dispatched and must not use local Main Agent execution
as a fallback. Claude may use `main_agent_direct_fallback` only when the Claude
skill explicitly permits a single-command fallback under the SideAgent contract.
Report `tool_unavailable` only when the required BlueprintHelper CLI command is
not installed or callable.

This guide intentionally does not duplicate template content. Agents use the
current CLI discovery/index output to locate concrete template files, copy one,
edit placeholders, and pass the edited file to the CLI.

## Mainline Workflow

```text
runtime/profile preflight
-> read this guide when needed
-> resolve an explicit Unreal asset path before writes
-> read context or reference context
-> evidence_conflict means stop_and_report, not binary fallback
-> build BlueprintHelper.TaskSpec.v1
-> preview
-> repair TaskSpec or stop_and_report
-> source-control status/checkout if required
-> request write session only when write_permission is disabled
-> execute
-> query task result when needed
-> report summary
```

Write authorization is running Editor/Bridge based. Request a write session only
after a successful preview when `write_permission` is disabled. The approved
scope and lifetime are held by the running Editor, so delegated SideAgents can
call BlueprintHelper tools after approval as long as they stay within that
scope. If approval is rejected, stop and report.

Source-control checkout is a separate pre-write gate. In P4/Perforce or other UE
source-control projects, check or request checkout after preview and before
execute when target assets may be read-only or when close/save reports
`checkout_required`. Stop on `checked_out_by_other`,
`source_control_conflicted`, `source_control_unavailable`, `checkout_failed`, or
`not_editable` and report the returned agent message.

Ordinary Agents must not request, set, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`; raw session data is not part of the Agent contract.

Unknown Unreal asset paths must be resolved before writes. Do not infer
Unreal asset paths from filesystem `.uasset` paths. If multiple
candidates are returned, narrow the request or ask for confirmation before any
write flow.

CLI output is optimized for Agent use. Use `--omit`, `--select`, or `--fields`
when only a small result slice is needed; use current CLI help/discovery for
exact field paths. Use `--max-bytes` as a hard budget guard; the full payload
remains available through the artifact path.

## Read Order

1. `AgentFaceService/agent-guide/Reference/01_Preflight_And_Boundary.md`
2. `AgentFaceService/agent-guide/Reference/03_Runtime_Profile_And_Diagnostics.md`
3. `AgentFaceService/agent-guide/Reference/05_UE_Blueprint_Write_Architecture_Rules.md`
4. `AgentFaceService/agent-guide/Reference/06_UE_Blueprint_Write_CodingStyle.md`
5. `AgentFaceService/agent-guide/Reference/07_LogicFlow_Syntax_Rules.md`
6. `AgentFaceService/agent-guide/Reference/08_Material_ReadContext_Contract.md`
7. `AgentFaceService/agent-guide/Reference/09_MaterialGraph_Write_Contract.md`
8. `AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md`
9. `AgentFaceService/agent-guide/Workflows/05_Edit_Blueprint_Workflow.md`
10. `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md`
11. `AgentFaceService/agent-guide/Workflows/07_Safety_Validation_And_Recovery.md`

## Rules

- UE asset writes submit `BlueprintHelper.TaskSpec.v1` only.
- TaskPlan, low-level capability, Bridge command, and frozen tool names are not
  ordinary Agent selection targets.
- If preview is blocked, stop and report or repair the TaskSpec; do not fall
  back to frozen or deprecated entry points.
- Deprecated MCP ordinary tools are not ordinary Agent entry points or fallback
  paths.
- Evidence conflicts between `read_context`, screenshots/visible Editor state,
  preview, execute, or readback are `stop_and_report`; do not inspect Unreal
  binary asset files as fallback evidence.
