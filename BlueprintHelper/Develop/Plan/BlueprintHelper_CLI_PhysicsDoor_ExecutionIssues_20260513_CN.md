# BlueprintHelper CLI Physics Door Execution Issues 2026-05-13

## Scope

本文件单独记录物理门 CLI 执行期间遇到的能力缺失和 Bug。实现进度仍以 `BlueprintHelper_CLI_PhysicsDoor_TestExecution_20260513_CN.md` 为主。

## Issue 1: PowerShell blocks `bh.ps1`

- Status: workaround used.
- Symptom: `bh blueprint_get_runtime_profile --json "{}"` failed because PowerShell execution policy blocked `C:\Users\CharlieNotFound\AppData\Roaming\npm\bh.ps1`.
- Impact: direct `bh` command cannot run from this shell session.
- Workaround: use `bh.cmd` for CLI calls.

## Issue 2: CLI rejects UTF-8 BOM JSON files

- Status: workaround used.
- Symptom: `blueprinthelper_read_task_context --file Saved\CodexTest\physics_door_context.json` returned `cli_error` with `Unexpected token '﻿'`.
- Cause: PowerShell `Set-Content -Encoding UTF8` wrote a BOM-prefixed JSON file.
- Impact: CLI JSON parser rejects the parameter file before reaching BlueprintHelper.
- Workaround: write temporary CLI JSON files with ASCII / BOM-free encoding.

## Issue 3: Append GraphWrite dry-run still blocks explicit component/member calls

- Status: source fix applied, pending build/editor reload verification.
- Symptom: `blueprinthelper_preview_task` for `DoorPanel.AddAngularImpulseInDegrees` returned `preview_blocked`.
- Error code: `explicit_member_call_not_supported`.
- Root cause: runtime node generation already has explicit object-call support in `CallFunctionNodeHandler`, but append GraphWrite dry-run still treated the resolver's old explicit-member diagnostic as a hard block.
- Fix: append dry-run now parses `Object.Function`, resolves the `Function` part for graph compatibility, and allows the request to reach runtime node generation.
- Boundary: merge GraphWrite is intentionally not opened yet because it uses a separate direct call-node creation path and does not have the explicit object target wiring path.
