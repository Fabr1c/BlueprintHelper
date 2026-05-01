# MCP Tool Boundary

BlueprintHelper MCP tools are for Unreal Editor assets through the local Bridge.

Use MCP for Blueprint graphs, UMG widgets, UObject properties, DataTables, asset open/save/compile, PIE commands, and editor diagnostics.

Do not use MCP for C++, TypeScript, Python, JSON config, documentation, source search, or build script edits. Use normal repository tools for those tasks.

Editor-asset write tools need an explicit asset path. Graph writes also need an explicit target graph. Active editor context is acceptable for low-risk reads, not for destructive writes unless the user requests it.
