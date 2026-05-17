# BlueprintHelper CLI Active Surface Implementation Plan

> **Status:** Revised after implementation review on 2026-05-12.

## Goal

Make BlueprintHelper CLI define the active non-frozen Agent-facing tool surface through direct tool-name calls such as:

```powershell
bh blueprinthelper_preview_task --file .\task-spec.json --select status,summary,artifacts.full_result
bh blueprint_get_runtime_profile --json "{}" --select status,summary
```

The migration establishes CLI as the only supported Agent entry. It must preserve:

- TaskSpec-first writes.
- Python Task Compiler orchestration.
- Bridge preview/execute boundaries.
- UE Task Runtime lowering.
- Compact selected-field CLI stdout.
- Full result artifacts for audit and follow-up reads.

## Boundary Correction

The CLI must not reintroduce MCP tools that were intentionally frozen and unregistered.

`MCP surface` means the current Agent-facing and preflight surface, not the historical low-level direct-tool inventory. Frozen legacy/expert names such as `blueprint_list_assets`, `blueprint_exec_console_command`, direct graph import/export tools, component mutation tools, PIE tools, and raw editor lifecycle tools are not CLI direct invocation targets. Passing `--expert` must not make them callable again.

Low-level capability execution remains behind TaskSpec, ReadSpec, Python compiler output, Bridge preview/execute, and UE Task Runtime lowering.

## Active Tool Surface

The shared CLI/MCP registry exposes only these non-frozen tools:

```text
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_read_agent_guide
blueprinthelper_get_debug_case
blueprinthelper_read_context
blueprint_get_runtime_profile
blueprinthelper_request_write_session
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprint_open_editor
```

## Implementation Shape

- `ClaudePlugin/task-core/src/tool-surface` owns the active tool registry and shared handlers.
- `ClaudePlugin/cli` dispatches `bh <tool_name>` only when the name exists in that active registry.
- `ClaudePlugin/mcp` uses the shared registry for TaskSpec task tools and keeps existing mature MCP wrappers where their behavior is MCP-specific.
- Frozen direct tools stay out of both the CLI direct registry and ordinary Agent documentation.
- CLI field projection with `--select` or `--fields` remains the main token-saving mechanism.

## Verification Requirements

Required tests:

```powershell
npm.cmd --prefix ClaudePlugin\task-core test -- --test-name-pattern "shared registry"
npm.cmd --prefix ClaudePlugin\cli test -- --test-name-pattern "frozen direct"
npm.cmd --prefix ClaudePlugin\mcp test
npm.cmd --prefix ClaudePlugin\cli test
```

Required smoke checks:

```powershell
node ClaudePlugin\cli\build\cli\index.js --help
node ClaudePlugin\cli\build\cli\index.js blueprinthelper_read_agent_guide --json "{}" --select status,artifacts.full_result
node ClaudePlugin\cli\build\cli\index.js blueprint_exec_console_command --json "{ ""command"": ""stat fps"" }" --expert
```

Expected result for the frozen smoke: `blueprint_exec_console_command` is rejected as an unsupported CLI command and does not contact the Bridge.
