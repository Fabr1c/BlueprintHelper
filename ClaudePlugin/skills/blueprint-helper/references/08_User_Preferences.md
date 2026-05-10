# 08 - BlueprintHelper User Preferences

schema: BlueprintHelper.UserPreferences.v1
generated_by: ClaudePlugin/commands/setup.md
saved_at: 2026-05-10
source: setup_user_preference_wizard
updated_by: ClaudePlugin/commands/configure.md

## Purpose

This file records durable user-facing Agent preferences for BlueprintHelper work. It is intentionally separate from `BlueprintHelper.SetupProfile.v1`, runtime_profile, project markers, and MCP tool results.

Agents should read this file before BlueprintHelper planning, status review, implementation, verification, or DebugBundle work. If it conflicts with a newer direct user instruction, follow the newer direct instruction and report the conflict briefly.

## SetupProfile Separation

- SetupProfile stores executable machine policy: paths, safety profile, fallback policy, save policy, and compact boundary summaries.
- This file stores collaboration, workflow, documentation, Debug, review, and preference-collection behavior.
- Do not write tokens, Bridge auth, raw payloads, local DebugBundle contents, or private environment details into this file.
- Do not copy this full file into `CLAUDE.md`, `AGENTS.md`, or project marker text. Markers should only point to this path.

## Active Preferences

### Safety And Task Flow

- Default safety profile: `Conservative`.
- Default entry mode: `task_spec_first`.
- Preview is the write gate. Do not execute writes when preview is blocked.
- Missing capability default: `stop_and_report`.
- Do not fall back to frozen or legacy low-level MCP tools unless the user explicitly requests expert recovery.
- If `write_permission` is disabled, request a write session after preview and before execute; a user rejection is a stop-and-report condition.
- Do not ask for or inject `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session` for ordinary interactive writes.

### Save And Validation

- Default save policy: no automatic save.
- Save only when the user requests it or when a validated workflow explicitly allows it.
- Use TaskSpec `validation.should_compile` and `validation.should_save` to make compile/save intent explicit.
- If validation fails, report the failing asset, stage, and diagnostic summary instead of retrying blindly.

### Blueprint, C++, And Repository Boundary

- Use BlueprintHelper MCP for UE editor assets only: Blueprint graphs, UMG, DataAssets, DataTables, compile/save/open, PIE/editor commands, diagnostics, and related asset operations.
- Use normal repository tools for C++, TypeScript, Python, JSON, config, build scripts, documentation, AGENTS files, and memory files.
- Default C++ edit permission: disabled unless the user explicitly asks for code edits.
- Default `.uasset` edit permission: allowed only through BlueprintHelper MCP and TaskSpec-first flow.
- Parent Class or reparent changes are unsupported by default; stop and report if a task requires them.
- Do not rely on the currently focused Unreal Editor tab for destructive writes unless the user explicitly asks for active-context editing.

### Graph Write And Naming

- New feature graph naming pattern: `EG_{FeatureName}`.
- Function and Custom Event names should use descriptive PascalCase.
- Variables should follow common UE style, such as `bDoorOpen` or `OpenImpulse`.
- Components should use descriptive names such as `SceneRoot`, `DoorMesh`, or `InteractionBox`.
- Avoid generic names such as `NewFunction`, `DoThing`, `Temp`, or `MyVar`.
- Default graph policy: do not modify user-owned nodes and do not merge into existing execution flow unless the user approves it.
- Patch/Merge of BlueprintHelper-owned blocks must use stable anchors from grouped context, not display names or first-node guesses.

### Input And Asset Policy

- Do not create Input Actions or edit Input Mapping Contexts by default.
- Use an Input Action only when the user provides it explicitly or there is a unique safe match.
- If multiple Input Action candidates exist, stop and ask or report the ambiguity.
- Missing target or referenced assets default to fail/stop-and-report.
- Existing same-name assets default to error, not silent reuse.
- Low-level `factory_class` or `asset_class` choices are disabled by default unless Expert mode is explicitly selected.

## Collaboration Preferences

- Read repository `AGENTS.md` or the current equivalent instruction block before work when available.
- Use parallel workers as much as practical for independent tasks.
- Keep responses concise in Chinese by default and reduce parenthetical explanations unless the user asks for extra detail.
- Status and completion wording must be precise. If the worktree is dirty or targeted verification is incomplete, say that instead of claiming full completion.
- Do not invent durable memory or verification facts; keep memory-derived claims evidence-based.

## Debug And Review Preferences

- When asked about current Debug system progress, compare actual code against both the Debug design document and the Debug implementation plan.
- When a Debug gap is found and the user asks to fill it, move to the concrete implementation surface instead of stopping at analysis.
- Preserve the DebugBundle filesystem contract: `summary.md + artifacts/`.
- Treat the older `summary.json`-centric layout as insufficient when the design document requires `summary.md + artifacts/`.
- MCP should stay summary-only for DebugCase lookup through `get_debug_case`.
- Do not expose DebugBundle artifact contents, local bundle paths, raw payloads, source content, token/settings text, or full ReviewRecord internals through normal MCP Agent flow.
- ReviewRecord should link stable `debug_case_ids[]`; it should not inline DebugBundle payloads.
- DebugBundle is a UE/local developer export artifact and can be cleaned independently from ReviewRecord.
- Prefer targeted DebugBundle verification over broad noisy automation runs when proving export-shape changes.

## Preference Collection Forms

The setup command should create this file during first-run configuration. The configure command should update this file and the active safety profile after setup.

Both commands should call `AskUserQuestion` to render native interactive forms. Do not print the form schema as plain Markdown unless `AskUserQuestion` is unavailable in the current Claude environment.

Each native form should include:

- `header`
- `question`
- `multiSelect`
- `options`

Recommended options should be marked `(Recommended)`. Custom free-text notes should be collected only after the user selects a custom-text option.

## Manual Notes

- No manual notes recorded yet.
