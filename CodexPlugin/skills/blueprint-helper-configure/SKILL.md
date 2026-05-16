---
name: blueprint-helper-configure
description: description: Use for BlueprintHelper Codex setup/configuration, safety profiles ReadOnly/Conservative/Standard/AutoRepair, .blueprinthelper/agent-profile.json, UserPreferences, missing capability policy, save policy, or the Codex equivalent of Claude /blueprint-helper:configure.
---

# BlueprintHelper Configure for Codex

Use this skill as the Codex-compatible replacement for the Claude `/blueprint-helper:configure` command. It is intentionally a skill, not a slash command.

## Scope

Configure these files when requested:

- `CodexPlugin/skills/blueprint-helper/references/08_User_Preferences.md`
- `plugins/blueprint-helper/skills/blueprint-helper/references/08_User_Preferences.md`
- `<ProjectDir>/.blueprinthelper/agent-profile.json` when the user provides or confirms a project profile path

Do not edit ClaudePlugin files from this skill unless the user explicitly asks for ClaudePlugin compatibility.

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
3. Ask for missing decisions concisely. Prefer native UI questions when available; otherwise ask plain text.
4. Show a short preview before writing.
5. Write only after the user confirms.

## Questions

Ask these in order when values are not already supplied by the user:

1. Safety profile:
   - `Conservative` recommended: preview required, no auto-save, write session approval required.
   - `ReadOnly`: no real UE asset writes.
   - `Standard`: preview required, save only when requested or explicitly configured.
   - `AutoRepair`: BlueprintHelper-owned repair is allowed and write approval is bypassed by default.
2. Missing capability policy:
   - `stop_and_report` recommended.
   - `ask_user`.
   - `debug_tools_fallback`.
3. Save policy:
   - `never_auto_save` recommended.
   - `save_when_requested`.
   - `workflow_save`.
4. Boundary policy:
   - Ordinary BlueprintHelper asset reads/writes use CLI.
   - Editor lifecycle open/close uses global MCP.
   - Repository source/docs/config edits use normal Codex tools.
5. Review/debug policy:
   - Keep Journal and Review evidence enabled.
   - Export DebugBundle only when needed.
6. Collaboration preference:
   - Concise Chinese final reports.
   - Accurate completion claims.
   - Use subagents only when explicitly requested or when the user asks for parallel work.

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
    "agent_entry_mode": "cli_task_spec_first",
    "fallback_when_task_tools_unavailable": "stop_and_report"
  },
  "editor_lifecycle": {
    "entry": "global_mcp",
    "open_tool": "mcp__blueprint_helper__blueprint_open_editor",
    "close_tool": "mcp__blueprint_helper__blueprint_close_editor"
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

- Default entry mode is `cli_task_spec_first`.
- Editor lifecycle commands must use global MCP tools.
- Ordinary reads/writes must use the CLI.
- Fourth safety profile `AutoRepair` skips the write approval popup and defaults write permission to enabled.
- Missing capability default is `stop_and_report`.
- Do not request or pass Bridge tokens or raw auth session values.

## Preview Format

Before writing, show:

```text
BlueprintHelper Configure Preview

UserPreferences:
  <path>

SetupProfile:
  <path or not updated>

Safety:
  <old> -> <new>

Policies:
  missing_capability_policy: <value>
  auto_save_policy: <value>
  editor_lifecycle: global_mcp
  approval_bypass: true only for AutoRepair
```

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
