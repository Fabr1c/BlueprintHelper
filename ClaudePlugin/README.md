# BlueprintHelper

BlueprintHelper is an Unreal Engine editor plugin with an MCP Server for AI agents. It lets an agent inspect and modify Unreal Editor assets through a local Bridge: Blueprint graphs, UMG widgets, DataAssets, DataTables, asset browser operations, compile/save/open commands, PIE commands, and related diagnostics.

BlueprintHelper MCP is not a general source editing API. Use normal repository tools for C++, TypeScript, Python, JSON, config files, code search, build scripts, and documentation edits.

## TaskSpec-First Architecture

The current documentation mainline is moving from direct Agent calls to many low-level MCP tools toward a task orchestration layer:

```text
Agent -> MCP Task Tools -> Python/MCP Task Compiler -> UE Task Runtime -> Existing Capability Clusters
```

The intended default flow is:

```text
blueprinthelper_get_runtime_profile
-> blueprinthelper_read_task_context
-> Agent produces BlueprintHelper.TaskSpec.v1
-> blueprinthelper_preview_task
-> blueprinthelper_execute_task
-> blueprinthelper_get_task_result when needed
```

Existing tool clusters are not removed. They remain as UE Task Runtime capabilities, debug / expert tools, and automation test entry points. See [Resources/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md](Resources/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md).

Agents submit `BlueprintHelper.TaskSpec.v1` only; they do not submit TaskPlan. Python / MCP compiles TaskPlan, and UE Task Runtime executes TaskPlan.

## Version

Current source metadata:

| Component | Current value |
|---|---|
| Unreal plugin `BlueprintHelper.uplugin` | `VersionName` 0.3.8 |
| MCP Server `BlueprintHelper_MCP_Server/package.json` | 0.3.8 |
| Documentation batch | 2026-05-04 TaskSpec mainline |
| Intended UE version | UE 5.3 or newer |

The documentation mainline for this batch targets the TaskSpec / TaskPlan orchestration architecture. Current checked-in plugin and MCP Server metadata may still report older implementation versions. Treat plugin version, MCP Server version, and documentation date as separate version sources until the compatibility matrix is completed.

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

### Claude Code 插件安装

1. 添加 marketplace 并安装插件：

```text
/plugin marketplace add <your-git-remote-or-local-path>
/plugin install blueprint-helper@blueprint-helper-dev
```

2. 重启 Claude Code。

3. 配置项目 agent profile：

Store the UE version and engine root in the project agent profile:

```json
{
  "environment": {
    "ue_version": "5.6",
    "ue_engine_dir": "<UE_ENGINE_ROOT>"
  }
}
```

Save this under `<ProjectDir>/.blueprinthelper/agent-profile.json`. Project `.uproject` paths are not stored in Claude global settings. Agents discover the target `.uproject` from the current workspace and pass it as the explicit `project_file` tool argument when launching or building a project.

4. 安装 MCP Server 依赖并构建：

```powershell
cd BlueprintHelper_MCP_Server
npm install
npm run build
```

5. 启动 Unreal Editor 打开目标项目，确保 Bridge 在 `127.0.0.1:54321` 运行。

### 手动 MCP Server 安装

1. Put this plugin under a UE project plugin directory, for example `YourProject/Plugins/BlueprintHelper`.
2. Enable the plugin in Unreal Editor.
3. Build the project if Unreal asks for a rebuild.
4. Build the MCP Server:

```powershell
cd <PLUGIN_ROOT>\ClaudePlugin\mcp
npm install
npm run build
```

5. Set MCP Server environment variables:

```powershell
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

Store the UE engine root in `<ProjectDir>/.blueprinthelper/agent-profile.json` as `environment.ue_engine_dir`; do not put UE version-specific project configuration in global Claude settings.

6. Start Unreal Editor with the project, then connect your Agent MCP client to:

```powershell
node <PLUGIN_ROOT>\ClaudePlugin\mcp\build\index.js
```

For full setup details, read [Docs/Install_MCP_QuickStart.md](Docs/Install_MCP_QuickStart.md).

## Agent Entry Points

Agents should read these in order:

1. [AGENTS.md](AGENTS.md)
2. [Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md](Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md)
3. [Resources/Setup/Setup_Questionnaire_20260430.md](Resources/Setup/Setup_Questionnaire_20260430.md)
4. [Resources/Setup/Setup_Profile_Schema_20260430.md](Resources/Setup/Setup_Profile_Schema_20260430.md)
5. [Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md](Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md)
6. [Resources/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md](Resources/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md)
7. [Docs/MCP_Tools_API_Reference.md](Docs/MCP_Tools_API_Reference.md)

Claude-style agents can also load [.claude/skills/blueprinthelper/SKILL.md](.claude/skills/blueprinthelper/SKILL.md).

## Claude Plugin Commands

Claude Code discovers plugin commands from the plugin root `commands/` directory.

| Command | Purpose |
|---|---|
| `/blueprint-helper:setup` | Initial setup: UE paths, MCP build, Bridge check, first-run preferences, SetupProfile |
| `/blueprint-helper:configure` | Update user preferences and active safety profile after setup |

## User Documentation

| Document | Purpose |
|---|---|
| [Docs/Install_MCP_QuickStart.md](Docs/Install_MCP_QuickStart.md) | Install, build, configure, and verify MCP connection |
| [Docs/MCP_Tools_API_Reference.md](Docs/MCP_Tools_API_Reference.md) | Task-level tools plus legacy/internal/debug tool inventory |
| [Resources/Setup/Setup_Questionnaire_20260430.md](Resources/Setup/Setup_Questionnaire_20260430.md) | Questions for collecting user and project preferences |
| [Resources/Setup/Setup_Profile_Schema_20260430.md](Resources/Setup/Setup_Profile_Schema_20260430.md) | Stable `.blueprinthelper/agent-profile.json` structure |
| [Resources/AgentGuide/](Resources/AgentGuide/) | Agent task routing and editor-asset workflows |
| [Resources/JsonToBlueprintRules.md](Resources/JsonToBlueprintRules.md) | Raw JSON-to-Blueprint conversion rules |
| [Resources/KnownBugs.md](Resources/KnownBugs.md) | Known bugs and implementation notes |

## Safe Write Workflow

For ordinary Agent editor-asset mutations, use the TaskSpec-first loop:

1. Confirm Unreal Editor is running, or `<ProjectDir>/.blueprinthelper/agent-profile.json` has `environment.ue_engine_dir` and the target `.uproject` can be passed as `project_file`.
2. Confirm the Bridge is reachable.
3. Read runtime profile and TaskContextPack.
4. Produce an explicit `BlueprintHelper.TaskSpec.v1` with `validation.should_compile` and `validation.should_save`.
5. Run preview and stop on schema, semantic, policy, capability, or dry-run blockers.
6. Execute only after preview passes.
7. Let UE Task Runtime manage TaskPlan execution, `execution_policy.should_compile` / `execution_policy.should_save`, transaction grouping, rollback, and diagnostics.
8. Report task-level results without repeated blind retries.

Do not rely on the currently focused editor tab for destructive operations unless the user explicitly asks for active-context editing.

## Bridge 鍗忚 (v2.2+)

BlueprintHelper Bridge 浣跨敤 object-first 鍗忚浼犻€?RawJson 鏁版嵁銆?
### 瀵煎嚭
- `payload` 鈥?缁撴瀯鍖?RawJson 瀵硅薄锛堜富瑕佸瓧娈碉級
- `json` 鈥?鍏煎鎬у埆鍚?- `json_text` 鈥?浠?`include_json_text: true` 鏃跺嚭鐜?
### 瀵煎叆
- 鎺ュ彈 `json` 涓?object 鎴?string
- 鎷掔粷 LogicJson/LogicMD锛坄importable=false` 鎴?`schema` 浠?`BlueprintHelper.Logic` 寮€澶达級

### MCP 榛樿琛屼负
- `blueprint_export_to_json` 杩斿洖 `raw_json_ref` (resource link)
- RawJson resource 鐩存帴杩斿洖 RawJson 鏈綋锛堜笉棰濆鍖呰９锛?- `legacy_text_json` 妯″紡鐢ㄤ簬鍏煎/璋冭瘯
