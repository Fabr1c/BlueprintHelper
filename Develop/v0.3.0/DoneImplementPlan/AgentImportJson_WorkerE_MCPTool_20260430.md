# Worker E MCP Tool

## Goal

Add an Agent-facing MCP tool for semantic Blueprint graph import.

## Files

Modify:

```text
MCPServer/src/tools.ts
```

## Requirements

- Add `blueprint_import_agent_graph`.
- Use object fields, not a JSON string.
- Require `target_blueprint` and `target_graph`.
- Forward directly to Bridge command `import_agent_graph`.
- Describe that this tool is semantic import and does not replace `blueprint_import_json_to_graph`.

