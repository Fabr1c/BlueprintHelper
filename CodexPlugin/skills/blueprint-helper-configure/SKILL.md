---
name: blueprint-helper-configure
description: Use for BlueprintHelper Codex setup/configuration, plan-mode configuration questions, .blueprinthelper/project-profile.json, .blueprinthelper/AgentWorkFlow.md, UserPreferences, missing capability policy, save policy, and editor lifecycle policy. Safety profiles are configured through .blueprinthelper/setting.json, not project-profile.
---

# BlueprintHelper Configure for Codex

Use this skill for BlueprintHelper Codex configuration. It is intentionally a skill, not a slash command.

## Scope

Configure these files when requested:

- `CodexPlugin/skills/blueprint-helper/references/08_User_Preferences.md`
- `<ProjectDir>/.blueprinthelper/project-profile.json` when the user provides or confirms a project profile path
- `<ProjectDir>/.blueprinthelper/AgentWorkFlow.md` when the user asks to refresh the project workflow prompt document
- `<ProjectDir>/.blueprinthelper/setting.json` only when the user explicitly asks to change runtime safety/profile settings

Do not edit ClaudePlugin files from this skill unless the user explicitly asks for ClaudePlugin compatibility.

Do not configure a Codex subagent model map here. The project Agent workflow prompt is installed as `.blueprinthelper/AgentWorkFlow.md`; project-root `AGENTS.md` / `CLAUDE.md` should only contain the managed BlueprintHelper entry that points to that file.

## Current Safety Reality

The runtime currently supports four safety profile names through `setting.json`:

1. `ReadOnly`
2. `Conservative`
3. `Standard`
4. `AutoRepair`

Important: the fourth profile, `AutoRepair`, bypasses the write approval popup and has default write permission. Preview, Journal, Review, and normal safety evidence still apply.

## Configure Flow

1. Read the current UserPreferences file if present.
2. Read the project `.blueprinthelper/project-profile.json` if the user provided a path or if a single obvious project profile exists.
3. Read `.blueprinthelper/AgentWorkFlow.md` when the user asks to inspect or refresh Agent workflow guidance.
4. Do not write `active_profile.safety_profile`, `safety.preview_required`, `safety.write_approval_required`, `safety.approval_bypass`, `agent`, or `editor_lifecycle` to `project-profile.json`; those are runtime settings or workflow prompt content, not machine profile fields.
5. Build a single plan-mode decision sheet for every missing configuration decision.
6. Present options as IDs with recommended defaults and consequences.
7. Stop after the decision sheet. Do not write files in the same turn that first asks for choices.
8. After the user selects options, show `BlueprintHelper Configure Apply Preview`.
9. Write only after the user explicitly confirms the apply preview.

## Plan-Mode Question Output

Do not ask configuration questions as direct conversational questions such as "Which safety profile do you want?".

When decisions are missing, output one structured plan-mode block instead. Prefer native Codex plan/question UI if available. If native UI is unavailable, use this exact text structure:

```text
BlueprintHelper Configure Plan

Status: waiting_for_selection
Files:
  UserPreferences: <path or not found>
  ProjectProfile: <path or not selected>
  AgentWorkFlow: <path or not found>

Decisions:
  [M] Missing capability policy
      A) stop_and_report  [recommended]
      B) ask_user
      C) debug_tools_fallback

  [V] Save policy
      A) never_auto_save  [recommended]
      B) save_when_requested
      C) workflow_save

  [B] Boundary policy
      A) CLI TaskSpec reads/writes + global MCP allowlist  [recommended]
      B) MCP lifecycle only, no CLI lifecycle fallback  [required]
      C) No plugin source reads for ordinary plugin usage  [required]

  [R] Review/debug policy
      A) Keep Journal and Review evidence enabled; DebugBundle only when needed  [recommended]
      B) Minimal evidence, no DebugBundle unless explicitly requested

Suggested selection:
  M=A V=A B=A R=A

Reply with one of:
  apply recommended
  M=A V=A B=A R=A
  custom: <your requested policy changes>
```

If some values were already supplied by the user, pre-fill them in the plan and mark them as `selected_from_user`. Still show remaining options in the same plan-mode block.

## Apply Preview Format

After the user selects options, show this preview before writing:

```text
BlueprintHelper Configure Apply Preview

Status: waiting_for_apply_confirmation

Selected options:
  M=<value>
  V=<value>
  B=<value>
  R=<value>

Files to update:
  UserPreferences: <path>
  ProjectProfile: <path or not updated>
  AgentWorkFlow: <path or not updated>

Planned changes:
  missing_capability_policy: <old> -> <new>
  auto_save_policy: <old> -> <new>
  editor_lifecycle: global_lifecycle_only_mcp
  safety_profile: not written by configure; use setting.json

Confirm with:
  apply

Cancel with:
  cancel
```

Do not write until the user confirms with `apply` or an equivalent explicit confirmation.

## ProjectProfile Mapping

When writing `<ProjectDir>/.blueprinthelper/project-profile.json`, preserve unknown fields and update only these fields:

```json
{
  "schema": "BlueprintHelper.ProjectProfile.v1",
  "environment": {
    "ue_engine_dir": "<UE root>",
    "ue_version": "<UE version>"
  },
  "workflow_docs": {
    "agent_workflow": ".blueprinthelper/AgentWorkFlow.md"
  }
}
```

Do not write these fields to `project-profile.json`: `active_profile.safety_profile`, `active_profile.missing_capability_policy`, `active_profile.auto_save_policy`, `agent`, `editor_lifecycle`, `safety.preview_required`, `safety.write_approval_required`, or `safety.approval_bypass`.

Agent workflow policy belongs in `<ProjectDir>/.blueprinthelper/AgentWorkFlow.md`. Project-root `AGENTS.md` / `CLAUDE.md` files should only contain the managed BlueprintHelper entry that points to that workflow document.

Runtime safety belongs in `<ProjectDir>/.blueprinthelper/setting.json`:

```json
{
  "active_profile": "default",
  "profiles": {
    "default": {
      "safety_profile": "AutoRepair"
    }
  },
  "safety": {
    "preview_required": true,
    "write_approval_required": false,
    "approval_bypass": true
  }
}
```

## UserPreferences Mapping

Update the active preference sections with these facts:

- Do not write subagent workflow or subagent model-map preferences here; the main `blueprint-helper` skill owns the mandatory Codex subagent workflow.
- Editor lifecycle commands must use global MCP allowlist tools. Do not enable CLI lifecycle fallback; if lifecycle MCP is unavailable, Agents must report `lifecycle_mcp_unavailable`. Deprecated MCP ordinary tools are not ordinary Agent entry points or fallback paths.
- Only the Main Agent may call `mcp__blueprint_helper__blueprint_open_editor` and `mcp__blueprint_helper__blueprint_close_editor`.
- Ordinary reads/writes must use the CLI.
- Ordinary plugin usage must not read BlueprintHelper plugin package or implementation source; use installed skill instructions, AgentGuide, CLI reference, and templates. Plugin source reads are allowed only for explicit plugin development, installation repair, or debugging.
- Fourth safety profile `AutoRepair` skips the write approval popup and defaults write permission to enabled when set in `setting.json`.
- Missing capability default is `stop_and_report`.
- Do not request or pass Bridge tokens or raw auth session values.

## Final Report

Report in the user's required task format:

```text
新增内容：
1. ...
修复内容：
1. ...
变更需求：
1. ...
快速修复：
1. ...
阻塞内容：
1. ...
```
