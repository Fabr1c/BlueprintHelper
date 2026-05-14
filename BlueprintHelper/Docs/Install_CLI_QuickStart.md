# BlueprintHelper CLI QuickStart

This guide connects a local Unreal Editor project, the BlueprintHelper Bridge, the BlueprintHelper CLI, and an AI agent.

Task orchestration mainline:

```text
Agent -> BlueprintHelper CLI -> task-core / Python Task Compiler -> UE Task Runtime -> Existing Capability Clusters
```

## Prerequisites

- Unreal Engine 5.3 or newer.
- A UE project that can compile editor plugins.
- Node.js and npm.
- BlueprintHelper installed under the project `Plugins` directory.
- A terminal that can run Windows PowerShell commands.

Path placeholders used in this guide:

```text
Plugin: <PLUGIN_ROOT>
Project file: discovered by the Agent and passed as explicit `project_file`
```

## 1. Install The Plugin

Place the plugin at:

```text
<YourProject>\Plugins\BlueprintHelper
```

Open the project in Unreal Editor, enable BlueprintHelper if needed, and rebuild the project if prompted.

If you use an Unreal `BuildPlugin` package, keep the sibling `AgentFaceService` package available separately. UE packaging does not compile or include `AgentFaceService/cli` or `AgentFaceService/task-core`.

## 2. Build The CLI

```powershell
cd <PLUGIN_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\AgentFaceService\cli
npm install
npm run build
```

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

Save this file as `<ProjectDir>/.blueprinthelper/agent-profile.json`.

Bridge connectivity environment variables:

```powershell
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

Project `.uproject` paths should not be stored globally. Agents discover the target `.uproject` from the current workspace and pass it explicitly as `project_file`.

## 4. Start Unreal Editor

Either start Unreal Editor normally with the project, or use the MCP lifecycle command `blueprint_open_editor` after the project agent profile has `environment.ue_engine_dir`. CLI lifecycle helpers are best-effort in one-shot shell environments.

Bridge smoke check:

```powershell
Test-NetConnection 127.0.0.1 -Port 54321
```

If the port is not open, wait for the editor to finish loading, confirm the plugin is enabled, and check the Unreal output log.

## 5. Run The CLI

The CLI is the supported Agent entry for ordinary TaskSpec writes. MCP remains the supported companion entry for editor lifecycle and long-lived debug/recovery flows.

Examples:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh blueprinthelper_read_task_context --file .\context-params.json --select status,summary,artifacts.full_result
bh blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
bh blueprinthelper_execute_task --file .\task_spec.json --select status,task_run_id,summary
```

See [TaskSpec_CLI_QuickStart.md](TaskSpec_CLI_QuickStart.md) for command syntax and output rules.

## 6. Minimal Verification

Repository verification:

```powershell
cd <PLUGIN_ROOT>\AgentFaceService\cli
npm test
```

Editor connection verification:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
```

Expected bridge facts:

```json
{
  "status": "completed",
  "summary": {
    "bridge": {
      "reachable": true
    },
    "write_permission": {
      "enabled": true
    }
  }
}
```

## 7. First Safe Asset Read

Read a Blueprint summary:

```powershell
bh blueprinthelper_read_context --json "{ \"schema\": \"BlueprintHelper.ReadSpec.v1\", \"read_type\": \"blueprint_logic\", \"target\": { \"asset_path\": \"/Game/Blueprints/BP_Player.BP_Player\" }, \"view\": { \"format\": \"summary\" } }" --select status,summary,artifacts.full_result
```

Read a graph as Markdown:

```powershell
bh blueprinthelper_read_context --json "{ \"schema\": \"BlueprintHelper.ReadSpec.v1\", \"read_type\": \"blueprint_logic\", \"target\": { \"asset_path\": \"/Game/Blueprints/BP_Player.BP_Player\", \"target_type\": \"graph\", \"target_name\": \"EventGraph\" }, \"view\": { \"format\": \"logic_md\" } }" --select status,summary,artifacts.full_result
```

## 8. Safe Write Checklist

For ordinary Agent editor-asset mutations, use the TaskSpec-first flow:

- Confirm the Bridge is reachable.
- Run `bh blueprint_get_runtime_profile --json "{}" --select status,summary`.
- Run `bh blueprinthelper_read_task_context --file .\context-params.json --select status,summary,artifacts.full_result`.
- Produce `BlueprintHelper.TaskSpec.v1` with exact `asset_path`, target graph when relevant, allowed scope, resource references, failure policy, `validation.should_compile`, and `validation.should_save`.
- Do not submit TaskPlan directly; it is produced by the Python Task Compiler.
- Run `bh blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result` and stop on blocked / failed preview.
- Run `bh blueprinthelper_execute_task --file .\task_spec.json --select status,task_run_id,summary` only after preview passes.
- Let UE Task Runtime handle TaskPlan execution, compile/save policy, transaction grouping, rollback, and diagnostics.

Low-level legacy/internal/debug/expert commands are documented in [CLI_Tools_API_Reference.md](CLI_Tools_API_Reference.md), but they are not part of the supported Agent entry.
