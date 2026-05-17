---
name: blueprint-helper-configure
description: Use for BlueprintHelper Codex setup/configuration, plan-mode configuration questions, safety profiles ReadOnly/Conservative/Standard/AutoRepair, .blueprinthelper/agent-profile.json, UserPreferences, missing capability policy, save policy, and editor lifecycle policy.
---

# BlueprintHelper Configure for Codex

Use this skill for BlueprintHelper Codex configuration. It is intentionally a skill, not a slash command.

## Scope

Configure these files when requested:

- `CodexPlugin/skills/blueprint-helper/references/08_User_Preferences.md`
- `<ProjectDir>/.blueprinthelper/agent-profile.json` when the user provides or confirms a project profile path

Do not edit ClaudePlugin files from this skill unless the user explicitly asks for ClaudePlugin compatibility.

Do not configure Codex subagent workflow or a subagent model map here. The mandatory Codex subagent workflow is fixed in `CodexPlugin/skills/blueprint-helper/SKILL.md`.

## Current Safety Reality

The runtime currently supports four safety profile names:

1. `ReadOnly`
2. `Conservative`
3. `Standard`
4. `AutoRepair`

Important: the fourth profile, `AutoRepair`, bypasses the write approval popup and has default write permission. Preview, Journal, Review, and normal safety evidence still apply.

## Configure Flow

1. Read the current UserPreferences file if present.
2. Read the project `.blueprinthelper/agent-profile.json` if the user provided a path or if a single obvious project profile exists.
3. Build a single plan-mode decision sheet for every missing configuration decision.
4. Present options as IDs with recommended defaults and consequences.
5. Stop after the decision sheet. Do not write files in the same turn that first asks for choices.
6. After the user selects options, show `BlueprintHelper Configure Apply Preview`.
7. Write only after the user explicitly confirms the apply preview.

## Plan-Mode Question Output

Do not ask configuration questions as direct conversational questions such as "Which safety profile do you want?".

When decisions are missing, output one structured plan-mode block instead. Prefer native Codex plan/question UI if available. If native UI is unavailable, use this exact text structure:

```text
BlueprintHelper Configure Plan

Status: waiting_for_selection
Files:
  UserPreferences: <path or not found>
  SetupProfile: <path or not selected>

Decisions:
  [S] Safety profile
      A) Conservative  [recommended]
         Preview required, no auto-save, write session approval required.
      B) ReadOnly
         No real UE asset writes.
      C) Standard
         Preview required, save only when requested or explicitly configured.
      D) AutoRepair
         BlueprintHelper-owned repair is allowed; write approval popup is bypassed by default.

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
      B) CLI lifecycle fallback only

  [R] Review/debug policy
      A) Keep Journal and Review evidence enabled; DebugBundle only when needed  [recommended]
      B) Minimal evidence, no DebugBundle unless explicitly requested

Suggested selection:
  S=A M=A V=A B=A R=A

Reply with one of:
  apply recommended
  S=A M=A V=A B=A R=A
  custom: <your requested policy changes>
```

If some values were already supplied by the user, pre-fill them in the plan and mark them as `selected_from_user`. Still show remaining options in the same plan-mode block.

## Apply Preview Format

After the user selects options, show this preview before writing:

```text
BlueprintHelper Configure Apply Preview

Status: waiting_for_apply_confirmation

Selected options:
  S=<value>
  M=<value>
  V=<value>
  B=<value>
  R=<value>

Files to update:
  UserPreferences: <path>
  SetupProfile: <path or not updated>

Planned changes:
  Safety: <old> -> <new>
  missing_capability_policy: <old> -> <new>
  auto_save_policy: <old> -> <new>
  editor_lifecycle: global_lifecycle_only_mcp
  approval_bypass: true only for AutoRepair

Confirm with:
  apply

Cancel with:
  cancel
```

Do not write until the user confirms with `apply` or an equivalent explicit confirmation.

## SetupProfile Mapping

When writing `<ProjectDir>/.blueprinthelper/agent-profile.json`, preserve unknown fields and update only these fields:

```json
{
  "active_profile": {
    "safety_profile": "Conservative",
    "missing_capability_policy": "stop_and_report",
    "auto_save_policy": "never_auto_save"
  },
  "agent": {
    "fallback_when_task_tools_unavailable": "stop_and_report"
  },
  "editor_lifecycle": {
    "entry": "global_lifecycle_only_mcp",
    "open_tool": "mcp__blueprint_helper__blueprint_open_editor",
    "close_tool": "mcp__blueprint_helper__blueprint_close_editor",
    "main_agent_only": true
  }
}
```

For `AutoRepair`, write:

```json
{
  "active_profile": {
    "safety_profile": "AutoRepair"
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
- Editor lifecycle commands must use global MCP allowlist tools. Deprecated MCP ordinary tools are not ordinary Agent entry points or fallback paths.
- Only the Main Agent may call `mcp__blueprint_helper__blueprint_open_editor` and `mcp__blueprint_helper__blueprint_close_editor`.
- Ordinary reads/writes must use the CLI.
- Fourth safety profile `AutoRepair` skips the write approval popup and defaults write permission to enabled.
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
