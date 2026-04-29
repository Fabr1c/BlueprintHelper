# BlueprintHelper MCP Usage Rules

BlueprintHelper is a UE5 Editor plugin with an MCPServer that bridges IDE AI agents to Unreal Editor operations.

The MCPServer currently exposes 41 MCP tools.

Important distinction:

- Tools 1-39 are Unreal Editor / BlueprintHelper Bridge oriented.
- `blueprint_close_editor` is a Bridge command.
- `blueprint_build_project` and `blueprint_open_editor` are local MCPServer process tools, not UE Bridge commands.

Use BlueprintHelper MCP only for Unreal Editor related operations:

- Blueprint JSON rules, validation, export, import, and compile.
- Current editor context inspection.
- UE asset browsing, searching, opening, saving, and asset info.
- Blueprint graph, variable, event dispatcher, function graph, macro graph, and node operations.
- UMG Widget tree operations and Widget property edits.
- UObject / DataAsset property reflection and edits.
- DataTable row read/add/update/delete.
- Editor commands such as undo, redo, PIE start/stop, create Blueprint, Unreal console command, and close editor.
- Opening the Unreal Editor only when `UE_ENGINE_DIR` and `UE_PROJECT_FILE` are correctly configured.
- Building the Unreal project only through UnrealBuildTool when the editor is closed and `UE_ENGINE_DIR` / `UE_PROJECT_FILE` are configured.

Do not use BlueprintHelper MCP for:

- General repository search.
- General file system operations.
- C++ / TypeScript / config source edits.
- `.uproject`, `.uplugin`, `Build.cs`, `Target.cs`, or AGENTS.md edits.
- Codex memory edits.
- General shell commands.
- Non-Unreal build commands.

Tool-specific cautions:

- `blueprint_search_assets` searches Unreal Content Browser assets, not source code.
- `blueprint_exec_console_command` executes Unreal Editor console commands, not OS shell commands.
- `blueprint_build_project` invokes UnrealBuildTool through `Build.bat`; it is not a generic build tool.
- `blueprint_open_editor` launches `UnrealEditor.exe` with `UE_PROJECT_FILE`; it does not infer the `.uproject` from the current repository. Ensure `UE_PROJECT_FILE` is set to the absolute current project `.uproject` path before calling.
- For write operations, explicitly provide `asset_path`, `target_blueprint`, and `target_graph` whenever applicable. Do not rely on active editor focus unless the user explicitly asks to operate on the active context.