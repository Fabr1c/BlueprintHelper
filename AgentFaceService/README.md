# BlueprintHelper AgentFaceService

This folder contains runtime code shared by the Claude and Codex plugin shells.

## Contents

- `task-core/` owns the Bridge client, TaskSpec schemas, Python orchestration, shared task runner, result helpers, and active tool registry.
- `cli/` owns the BlueprintHelper CLI transport used by shell-capable Agents.
- `mcp/` owns the long-lived deprecated/frozen MCP compatibility transport for editor launch/lifecycle, debug, recovery, and capabilities that are unreliable from one-shot CLI processes.
- `scripts/` contains shared package build helpers.

`ClaudePlugin/` and `CodexPlugin/` should stay lightweight. They provide product-specific manifests, skills, commands, and documentation while pointing to this shared runtime for CLI, Python orchestration, and the Deprecated/frozen MCP compatibility surface.

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

The deprecated/frozen MCP compatibility server entry is:

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\mcp\build\index.js
```
