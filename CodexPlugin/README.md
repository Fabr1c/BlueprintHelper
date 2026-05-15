# BlueprintHelper Codex Plugin

This is a Codex-compatible package for BlueprintHelper.

## Contents

- `.codex-plugin/plugin.json` is the Codex plugin manifest.
- `skills/blueprint-helper/SKILL.md` is the Codex-facing workflow entry.
- `skills/blueprint-helper/references/` mirrors the BlueprintHelper agent references from `ClaudePlugin`.
- `assets/blueprint-helper.svg` is the local plugin icon referenced by the manifest.

## Runtime Model

The active Agent-facing transport for ordinary TaskSpec reads and writes is the BlueprintHelper CLI. The MCP endpoint from `AgentFaceService/mcp` is registered only for editor lifecycle commands such as opening or closing the Unreal Editor.

Important: editor lifecycle commands must be called through the global MCP server (`mcp__blueprint_helper__blueprint_open_editor` and `mcp__blueprint_helper__blueprint_close_editor`). Plugin-local MCP or one-shot shell MCP clients may be reaped by the sandbox and should not be used for lifecycle validation.

Build the CLI when needed:

```powershell
cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli
npm install
npm run build
```

Use either `bh` on PATH or the built CLI entry:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js blueprint_get_runtime_profile --json "{}" --select status,summary
```

Install the editor lifecycle MCP globally when plugin-local MCP cannot start the editor from the current sandbox:

```powershell
node <BLUEPRINTHELPER_ROOT>\plugins\blueprint-helper\scripts\install-global-mcp.cjs
```

For editor-asset writes, keep the workflow TaskSpec-first: read context, author `BlueprintHelper.TaskSpec.v1`, preview, request write approval when needed, execute, then inspect the result artifact. Use MCP only for editor lifecycle commands; do not register or call ordinary BlueprintHelper read/write tools through MCP.
