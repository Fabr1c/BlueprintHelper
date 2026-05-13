# BlueprintHelper Codex Plugin

This is a Codex-compatible package for BlueprintHelper.

## Contents

- `.codex-plugin/plugin.json` is the Codex plugin manifest.
- `skills/blueprint-helper/SKILL.md` is the Codex-facing workflow entry.
- `skills/blueprint-helper/references/` mirrors the BlueprintHelper agent references from `ClaudePlugin`.
- `assets/blueprint-helper.svg` is the local plugin icon referenced by the manifest.

## Runtime Model

The active Agent-facing transport is the BlueprintHelper CLI. The MCP endpoint from `AgentFaceService/mcp` is deprecated compatibility/debug surface and is not registered by this Codex plugin as a normal Agent entry.

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

For editor-asset writes, keep the workflow TaskSpec-first: read context, author `BlueprintHelper.TaskSpec.v1`, preview, request write approval when needed, execute, then inspect the result artifact.
