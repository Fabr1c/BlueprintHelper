# BlueprintHelper MCP QuickStart

This guide connects a local Unreal Editor project, the BlueprintHelper Bridge, the BlueprintHelper MCP Server, and an AI agent.

Task orchestration mainline: Agent -> MCP Task Tools -> Python Task Compiler -> UE Task Runtime -> Existing Capability Clusters.

## Prerequisites

- Unreal Engine 5.3 or newer.
- A UE project that can compile editor plugins.
- Node.js and npm.
- BlueprintHelper installed under the project `Plugins` directory.
- A terminal that can run Windows PowerShell commands.

Path placeholders used in this guide:

```text
Plugin:  <PLUGIN_ROOT>
Project file: discovered by the Agent and passed as explicit `project_file`
```

## 1. Install The Plugin

Place the plugin at:

```text
<YourProject>\Plugins\BlueprintHelper
```

Open the project in Unreal Editor, enable BlueprintHelper if needed, and rebuild the project if prompted.

If you use an Unreal `BuildPlugin` package, that package covers only the UE plugin directory. It does not compile or include the sibling `ClaudePlugin/mcp` server. Keep the agent plugin package available separately, then build `ClaudePlugin/mcp` from that package before configuring the MCP client.

## 2. Build The MCP Server

```powershell
cd <PLUGIN_ROOT>\ClaudePlugin\mcp
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

## 3. Configure Project Agent Profile

Store UE version-specific configuration in the project profile:

```json
{
  "environment": {
    "ue_version": "5.6",
    "ue_engine_dir": "<UE_ENGINE_ROOT>"
  }
}
```

Save this file as `<ProjectDir>/.blueprinthelper/agent-profile.json`. The MCP Server reads these environment variables only for Bridge connectivity:

| Variable | Required | Default | Purpose |
|---|---:|---|---|
| `BRIDGE_HOST` | No | `127.0.0.1` | BlueprintHelper Bridge host |
| `BRIDGE_PORT` | No | `54321` | BlueprintHelper Bridge port |

Interactive write access is granted through `blueprinthelper_request_write_session` after the Unreal Editor is running. The Editor shows a simple accept/reject approval dialog, and the approval is held by the running Editor/Bridge for its approved scope and lifetime. Ordinary agents and delegated SideAgents should not configure or pass a Bridge token or raw `auth_session` for writes.

PowerShell example:

```powershell
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

Project `.uproject` paths and UE version-specific paths are intentionally not stored in global MCP environment variables. Agents should discover the target `.uproject` from the current workspace and pass it as the explicit `project_file` tool argument.

## 4. Start Unreal Editor

Either start Unreal Editor normally with the project, or let the MCP tool launch it later through `blueprint_open_editor` after the project agent profile has `environment.ue_engine_dir` and the Agent can pass the target `.uproject` as `project_file`.

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
node <PLUGIN_ROOT>\ClaudePlugin\mcp\build\index.js
```

Using npm:

```powershell
cd <PLUGIN_ROOT>\ClaudePlugin\mcp
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
        "<PLUGIN_ROOT>\\ClaudePlugin\\mcp\\build\\index.js"
      ],
      "env": {
        "BRIDGE_HOST": "127.0.0.1",
        "BRIDGE_PORT": "54321"
      }
    }
  }
}
```

For Codex, Claude, or IDE agents, use the same command, args, and env fields in that client's MCP configuration format.

## 7. Optional TaskSpec CLI Entry

The CLI is an alternate Agent entry for shell-capable environments. It does not replace MCP or the TaskSpec flow. It calls the same Python compiler, Bridge preview, and UE Task Runtime execution path as the MCP task tools.

Use it when an Agent should keep output compact and avoid large MCP escaped JSON responses. Prefer `--format summary` for normal planning and review loops. Use `--format json` only when the Agent truly needs the full payload in context.

See [TaskSpec_CLI_QuickStart.md](TaskSpec_CLI_QuickStart.md) for the CLI commands and usage rules.

## 8. Minimal Verification

Repository verification:

```powershell
cd <PLUGIN_ROOT>\ClaudePlugin\mcp
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

## 9. First Safe Asset Read

List Blueprints:

```text
Call blueprint_list_assets with path=/Game, class_filter=Blueprint, recursive=true
```

Read a graph as Markdown:

```text
Call blueprint_get_logic with target_blueprint=/Game/Blueprints/BP_Player.BP_Player and target_graph=EventGraph
```

Use LogicMD or LogicJson for review and planning. Use raw JSON only for precise replay, import, or low-level graph diagnostics.

## 10. Safe Write Checklist

The same TaskSpec-first checklist applies whether the Agent enters through MCP task tools or the optional TaskSpec CLI.

For ordinary Agent editor-asset mutations, use the TaskSpec-first flow:

- Confirm the Bridge is reachable.
- Call `blueprint_get_runtime_profile`.
- Call `blueprinthelper_read_task_context`.
- Produce `BlueprintHelper.TaskSpec.v1` with exact `asset_path`, target graph when relevant, allowed scope, resource references, failure policy, `validation.should_compile`, and `validation.should_save`.
- Do not submit TaskPlan directly; it is produced by the Python Task Compiler.
- Run `blueprinthelper_preview_task` and stop on blocked / failed preview.
- Run `blueprinthelper_execute_task` only after preview passes.
- Let UE Task Runtime handle TaskPlan execution, `execution_policy.should_compile` / `execution_policy.should_save`, transaction grouping, rollback, and diagnostics.
- Report partial failures and do not retry blindly.

Low-level tools that modify assets are marked in [MCP_Tools_API_Reference.md](MCP_Tools_API_Reference.md) for legacy/internal/debug/expert use.
