# BlueprintHelper AgentGuide Templates

These templates are copy-and-edit JSON inputs for the TaskSpec-first CLI workflow.
They are intentionally small and valid JSON. Copy one template to a working file,
replace placeholders, then call the CLI with `--file`.

Recommended PowerShell flow:

```powershell
$src = 'AgentFaceService\agent-guide\Templates\write\taskspec_create_asset_blueprint_class_template.json'
$dst = 'Saved\BlueprintHelper\CodexTaskSpecs\task.json'
Copy-Item -LiteralPath $src -Destination $dst -Force
# Edit $dst, then run preview and execute.
node .\AgentFaceService\cli\build\cli\index.js task preview --file $dst --fields status,summary,artifacts.full_result
node .\AgentFaceService\cli\build\cli\index.js task execute --file $dst --fields status,task_run_id,summary,artifacts.full_result
```

Use `--format full` when debugging CLI parse, schema, or Bridge errors. Use
`--fields` / `--select` for normal Agent loops.

## Directory Map

| Directory | Purpose |
|---|---|
| `Templates/` | Environment, diagnostics, guide, authorization, task result, and debug-summary tool inputs. |
| `Templates/read/` | UE asset or logic context reads used before authoring or verifying TaskSpecs. |
| `Templates/write/` | Preview/execute wrappers and bare `BlueprintHelper.TaskSpec.v1` write templates. |

## CLI Root Shape Rule

There are two supported TaskSpec command shapes:

| CLI entry | File root |
|---|---|
| `bh blueprinthelper_preview_task --file x.json` | `{ "task_spec": { ... } }` or bare TaskSpec when supported by the current tool registry. Prefer wrapper templates when using this tool name. |
| `bh blueprinthelper_execute_task --file x.json` | `{ "task_spec": { ... } }` or bare TaskSpec when supported by the current tool registry. Prefer wrapper templates when using this tool name. |
| `bh task preview --file x.json` | bare `BlueprintHelper.TaskSpec.v1` |
| `bh task execute --file x.json` | bare `BlueprintHelper.TaskSpec.v1` |

## Not Exposed Here

`blueprinthelper_apply_review_action` is intentionally not included in ordinary
AgentGuide templates. It is a plugin-development/internal Review action, not a
normal Agent-facing workflow.

