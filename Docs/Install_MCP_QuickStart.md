# BlueprintHelper MCP QuickStart

This guide connects a local Unreal Editor project, the BlueprintHelper Bridge, the BlueprintHelper MCP Server, and an AI agent.

## Prerequisites

- Unreal Engine 5.3 or newer.
- A UE project that can compile editor plugins.
- Node.js and npm.
- BlueprintHelper installed under the project `Plugins` directory.
- A terminal that can run Windows PowerShell commands.

Current repository example:

```text
Plugin:  G:\UnrealPractise\MrStone\Plugins\BlueprintHelper
Project: G:\UnrealPractise\MrStone\MrStone.uproject
```

## 1. Install The Plugin

Place the plugin at:

```text
<YourProject>\Plugins\BlueprintHelper
```

Open the project in Unreal Editor, enable BlueprintHelper if needed, and rebuild the project if prompted.

## 2. Build The MCP Server

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server
npm install
npm run build
```

Available scripts:

| Script | Purpose |
|---|---|
| `npm run build` | Compile TypeScript to `build/` |
| `npm test` | Build and run MCP regression tests |
| `npm start` | Start the stdio MCP Server from `build/index.js` |
| `npm run dev` | Watch TypeScript sources |

## 3. Configure Environment Variables

The MCP Server reads these environment variables:

| Variable | Required | Default | Purpose |
|---|---:|---|---|
| `UE_ENGINE_DIR` | For `blueprint_open_editor` and `blueprint_build_project` | empty | Unreal Engine root, for example `F:\UE_5.6` |
| `UE_PROJECT_FILE` | For `blueprint_open_editor` and `blueprint_build_project` | empty | Absolute `.uproject` path |
| `BRIDGE_HOST` | No | `127.0.0.1` | BlueprintHelper Bridge host |
| `BRIDGE_PORT` | No | `54321` | BlueprintHelper Bridge port |
| `BLUEPRINTHELPER_BRIDGE_TOKEN` | Optional | empty | Optional Bridge auth token |

PowerShell example:

```powershell
$env:UE_ENGINE_DIR = "F:\UE_5.6"
$env:UE_PROJECT_FILE = "G:\UnrealPractise\MrStone\MrStone.uproject"
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

`UE_PROJECT_FILE` must point to the `.uproject` file, not the project directory.

## 4. Start Unreal Editor

Either start Unreal Editor normally with the project, or let the MCP tool launch it later through `blueprint_open_editor` after `UE_ENGINE_DIR` and `UE_PROJECT_FILE` are set.

The editor must load the BlueprintHelper plugin so the Bridge can listen on the configured host and port.

Bridge smoke check:

```powershell
Test-NetConnection 127.0.0.1 -Port 54321
```

If the port is not open, wait for the editor to finish loading, confirm the plugin is enabled, and check the Unreal output log.

## 5. Start The MCP Server

The MCP Server uses stdio transport. Most MCP clients launch it directly.

Manual command:

```powershell
node G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server\build\index.js
```

Using npm:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server
npm start
```

When run by an MCP client, stdout is reserved for JSON-RPC. Diagnostic logs are written to stderr.

## 6. Agent Configuration Examples

Generic MCP client configuration:

```json
{
  "mcpServers": {
    "blueprint-helper": {
      "command": "node",
      "args": [
        "G:\\UnrealPractise\\MrStone\\Plugins\\BlueprintHelper_MCP_Server\\build\\index.js"
      ],
      "env": {
        "UE_ENGINE_DIR": "F:\\UE_5.6",
        "UE_PROJECT_FILE": "G:\\UnrealPractise\\MrStone\\MrStone.uproject",
        "BRIDGE_HOST": "127.0.0.1",
        "BRIDGE_PORT": "54321"
      }
    }
  }
}
```

For Codex, Claude, or IDE agents, use the same command, args, and env fields in that client's MCP configuration format.

## 7. Minimal Verification

Repository verification:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper_MCP_Server
npm test
```

Editor connection verification from an MCP client:

```text
Call blueprint_get_editor_context
```

Expected result:

```json
{
  "success": true,
  "result": {
    "active_blueprint": "...",
    "active_graph": "..."
  }
}
```

If no Blueprint is active, the tool may still return a successful editor context with empty asset details. For destructive edits, do not use active context by default. Provide explicit `asset_path` and `target_graph`.

## 8. First Safe Asset Read

List Blueprints:

```text
Call blueprint_list_assets with path=/Game, class_filter=Blueprint, recursive=true
```

Read a graph as Markdown:

```text
Call blueprint_get_logic with target_blueprint=/Game/Blueprints/BP_Player.BP_Player and target_graph=EventGraph
```

Use LogicMD or LogicJson for review and planning. Use raw JSON only for precise replay, import, or low-level graph diagnostics.

## 9. Safe Write Checklist

Before using any mutate tool:

- Confirm the Bridge is reachable.
- Confirm the exact `asset_path`.
- Confirm the exact `target_graph` for graph edits.
- Read the current asset state.
- Produce a small write plan.
- Run compile, validation, or save as required.
- Report partial failures and do not retry blindly.

Tools that modify assets are marked in [MCP_Tools_API_Reference.md](MCP_Tools_API_Reference.md).
