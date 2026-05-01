# BlueprintHelper

BlueprintHelper is an Unreal Engine editor plugin with an MCP Server for AI agents. It lets an agent inspect and modify Unreal Editor assets through a local Bridge: Blueprint graphs, UMG widgets, DataAssets, DataTables, asset browser operations, compile/save/open commands, PIE commands, and related diagnostics.

BlueprintHelper MCP is not a general source editing API. Use normal repository tools for C++, TypeScript, Python, JSON, config files, code search, build scripts, and documentation edits.

## Version

Current source metadata:

| Component | Current value |
|---|---|
| Unreal plugin `BlueprintHelper.uplugin` | `VersionName` 0.2.9 |
| MCP Server `MCPServer/package.json` | 0.1.0 |
| Documentation batch | 2026-04-30 |
| Intended UE version | UE 5.3 or newer |

The documentation plan for this batch targets the v0.3.0 source package, while current checked-in metadata still reports the values above. Treat plugin version, MCP Server version, and documentation date as separate version sources until the compatibility matrix is completed.

## Core Capabilities

| Area | Supported |
|---|---|
| Blueprint logic | Read LogicMD, read LogicJson, export raw JSON, import raw JSON, import AgentImportGraph |
| Blueprint structure | List graphs, variables, dispatchers, add/remove variables, add/remove graphs, add dispatchers, delete nodes |
| UMG | Read widget trees, add/remove/move widgets, read/set widget properties |
| Data assets | Read/set editable UObject properties |
| DataTables | Read, add, update, delete rows |
| Editor commands | Compile/save/open assets, create Blueprint, PIE start/stop, undo/redo, console commands |
| Local process commands | Open Unreal Editor, build the Unreal project |

## Boundaries

Use BlueprintHelper MCP for editor assets through the running Unreal Editor.

Do not use BlueprintHelper MCP for:

- C++ source edits.
- TypeScript MCP Server edits.
- Python, JSON, config, or build script edits.
- General file-system search.
- General source-code search.

For repository work, use normal shell and editor tools. For editor assets, use the MCP tools after Bridge preflight.

## Quick Install Path

1. Put this plugin under a UE project plugin directory, for example `YourProject/Plugins/BlueprintHelper`.
2. Enable the plugin in Unreal Editor.
3. Build the project if Unreal asks for a rebuild.
4. Build the MCP Server:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
npm install
npm run build
```

5. Set MCP Server environment variables:

```powershell
$env:UE_ENGINE_DIR = "F:\UE_5.6"
$env:UE_PROJECT_FILE = "G:\UnrealPractise\MrStone\MrStone.uproject"
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

6. Start Unreal Editor with the project, then connect your Agent MCP client to:

```powershell
node G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer\build\index.js
```

For full setup details, read [Docs/Install_MCP_QuickStart.md](Docs/Install_MCP_QuickStart.md).

## Agent Entry Points

Agents should read these in order:

1. [AGENTS.md](AGENTS.md)
2. [Resources/AgentGuide/00_Agent_Onboarding_Index_20260430.md](Resources/AgentGuide/00_Agent_Onboarding_Index_20260430.md)
3. [Resources/Setup/Setup_Questionnaire_20260430.md](Resources/Setup/Setup_Questionnaire_20260430.md)
4. [Resources/Setup/Setup_Profile_Schema_20260430.md](Resources/Setup/Setup_Profile_Schema_20260430.md)
5. [Docs/MCP_Tools_API_Reference.md](Docs/MCP_Tools_API_Reference.md)

Claude-style agents can also load [.claude/skills/blueprinthelper/SKILL.md](.claude/skills/blueprinthelper/SKILL.md).

## User Documentation

| Document | Purpose |
|---|---|
| [Docs/Install_MCP_QuickStart.md](Docs/Install_MCP_QuickStart.md) | Install, build, configure, and verify MCP connection |
| [Docs/MCP_Tools_API_Reference.md](Docs/MCP_Tools_API_Reference.md) | Current 44 MCP tools, inputs, risks, and return shape |
| [Resources/Setup/Setup_Questionnaire_20260430.md](Resources/Setup/Setup_Questionnaire_20260430.md) | Questions for collecting user and project preferences |
| [Resources/Setup/Setup_Profile_Schema_20260430.md](Resources/Setup/Setup_Profile_Schema_20260430.md) | Stable `.blueprinthelper/agent-profile.json` structure |
| [Resources/AgentGuide/](Resources/AgentGuide/) | Agent task routing and editor-asset workflows |
| [Resources/JsonToBlueprintRules.md](Resources/JsonToBlueprintRules.md) | Raw JSON-to-Blueprint conversion rules |
| [Resources/KnownBugs.md](Resources/KnownBugs.md) | Known bugs and implementation notes |

## Safe Write Workflow

For any editor-asset mutation:

1. Confirm Unreal Editor is running or `UE_ENGINE_DIR` and `UE_PROJECT_FILE` are configured.
2. Confirm the Bridge is reachable.
3. Identify the exact `asset_path`.
4. Identify the exact `target_graph` for graph edits.
5. Read first, then plan the smallest mutation.
6. Mutate, compile or validate, save only when intended.
7. Report errors without repeated blind retries.

Do not rely on the currently focused editor tab for destructive operations unless the user explicitly asks for active-context editing.
