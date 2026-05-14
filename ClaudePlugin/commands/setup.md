---
description: Run minimal BlueprintHelper first-run setup: project profile, default safety policy, CLI install pointer, and one diagnostics check
allowed-tools: Read, Write, AskUserQuestion
---

# BlueprintHelper Setup

This command is intentionally short. It prepares the project for BlueprintHelper CLI use without running a full preference wizard and without shell-command permissions.

Use `/blueprint-helper:configure` after setup when the user wants to change safety, fallback, save, naming, collaboration, Debug, or review preferences.

## Scope

Setup does:

1. Discover or confirm the target UE project.
2. Confirm the UE engine root and write it to the project agent profile.
3. Check whether the BlueprintHelper CLI build artifact appears to exist.
4. Create default Conservative preferences only when the preference file is missing.
5. Ask for one optional CLI diagnostics result.
6. Report exact setup status.

Setup does not:

- run `npm`, `node`, `bh`, UnrealBuildTool, or UE compile commands;
- collect the full user preference questionnaire;
- list the full CLI command surface;
- write `.uproject` paths into global Claude settings, plugin env, SetupProfile, or RuntimeProfile;
- modify global Claude settings.

For detailed CLI installation instructions, point the user to `BlueprintHelper/Docs/Install_CLI_QuickStart.md`. For the current command surface, point the user to `BlueprintHelper/Docs/CLI_Tools_API_Reference.md`.

## Step 1 - Project And Engine Profile

Find the target `.uproject` from the current workspace when possible. If there is no unique `.uproject`, ask the user for the absolute project file path.

Ask the user for the absolute UE Engine root only when it is not already known from `<ProjectDir>/.blueprinthelper/agent-profile.json`.

Validation rules:

- `environment.ue_engine_dir` must be an absolute path.
- The engine root should contain `Engine/Binaries/Win64/UnrealEditor.exe` on Windows.
- The project file must end with `.uproject`.
- Do not store the project file path in the profile. It is passed only as an explicit `project_file` argument when a CLI command needs it.

Write or update:

```text
<ProjectDir>/.blueprinthelper/agent-profile.json
```

Minimum profile shape:

```json
{
  "environment": {
    "ue_version": "5.6",
    "ue_engine_dir": "<UE_ENGINE_ROOT>"
  },
  "active_profile": {
    "safety_profile": "Conservative",
    "missing_capability_policy": "stop_and_report",
    "auto_save_policy": "never_auto_save"
  },
  "agent": {
    "agent_entry_mode": "cli_task_spec_first",
    "fallback_when_task_tools_unavailable": "stop_and_report"
  }
}
```

When updating an existing profile, preserve unknown fields.

## Step 2 - CLI Build Status

Check whether this file appears to exist:

```text
<PLUGIN_ROOT>/AgentFaceService/cli/build/cli/index.js
```

If it exists, record `CLI build: present`.

If it is missing, do not run build commands. Report `CLI build: missing` and point the user to `BlueprintHelper/Docs/Install_CLI_QuickStart.md`.

## Step 3 - Default Preferences

Read `ClaudePlugin/skills/blueprint-helper/references/08_User_Preferences.md`.

If it exists, keep it unchanged and report `Preferences: kept`.

If it is missing, create a compact default file with:

- title `# 08 - BlueprintHelper User Preferences`
- `schema: BlueprintHelper.UserPreferences.v1`
- `generated_by: ClaudePlugin/commands/setup.md`
- `source: setup_default_conservative_profile`
- default safety `Conservative`
- entry mode `cli_task_spec_first`
- preview as the write gate
- missing capability policy `stop_and_report`
- no automatic save
- CLI only for UE editor assets; normal repo tools for source/docs/config/tests
- no tokens, Bridge auth, raw payloads, or private environment details

Report `Preferences: created default Conservative`. Tell the user to run `/blueprint-helper:configure` to customize these defaults.

## Step 4 - One Diagnostics Check

Do not ask for broad CLI or MCP command-surface permission. CLI is the TaskSpec write mainline; MCP remains available for editor lifecycle and long-lived debug/recovery flows. The full CLI command list belongs in `CLI_Tools_API_Reference.md`.

Because this command has no shell permission, ask the user whether they want to run exactly one diagnostics command themselves:

```powershell
bh blueprinthelper_diagnostics --json "{}" --select status,summary
```

Use `AskUserQuestion`:

- header: `Diagnostics`
- question: `Run one BlueprintHelper diagnostics check now?`
- multiSelect: false
- options:
  - label: `I will run it (Recommended)`
    description: `User runs the diagnostics command and reports whether it passed.`
  - label: `Skip`
    description: `Setup continues, final report marks diagnostics as skipped.`
  - label: `Blocked`
    description: `CLI or Unreal Editor is not ready; setup reports the blocker.`

If the user chooses to run it, ask for the observed result in plain text. Accept a compact status such as `passed`, `failed`, or the important error summary. Do not request or paste large raw JSON.

## Step 5 - Final Report

Report:

```text
BlueprintHelper setup status

Project profile: <path>
UE Engine: <environment.ue_engine_dir>
UE Project: <discovered .uproject, not stored in profile>
CLI build: present | missing
Preferences: kept | created default Conservative
Diagnostics: passed | failed | skipped | blocked
Entry mode: cli_task_spec_first
Next command for customization: /blueprint-helper:configure
```

If any required step is blocked, stop and report the specific blocker. Do not continue into preference configuration from setup.
