# BlueprintHelper AgentFaceService

This folder contains runtime code shared by the Claude and Codex plugin shells.

## Contents

- `task-core/` owns the Bridge client, TaskSpec schemas, Python orchestration, shared task runner, result helpers, and active tool registry.
- `cli/` owns the BlueprintHelper CLI transport used by shell-capable Agents.
- `mcp/` owns the deprecated MCP compatibility/debug transport.
- `scripts/` contains shared package build helpers.

`ClaudePlugin/` and `CodexPlugin/` should stay lightweight. They provide product-specific manifests, skills, commands, and documentation while pointing to this shared runtime for CLI, Python orchestration, and MCP compatibility.

## Build

```powershell
cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli
npm install
npm run build
```

The CLI entry is:

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js --help
```

The deprecated MCP compatibility server entry is:

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\mcp\build\index.js
```
