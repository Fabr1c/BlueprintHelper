# Worker E MCP Regression

## Goal

Keep MCP public interfaces compatible while exposing the new safety result fields.

## Dependencies

Worker E starts after Worker A and Worker B result fields are stable.

## Files

Modify:

```text
MCPServer/src/bridge-client.ts
MCPServer/src/tools.ts
MCPServer/package.json
MCPServer/tsconfig.json
Resources/TestFixtures/*
```

Do not modify C++ service code in this worker.

## Steps

- [ ] Confirm `blueprint_export_to_json.scope` accepts:

```text
graph
blueprint
selection
full_graph
full_blueprint
```

- [ ] Confirm `blueprint_import_json_to_graph` sends `strict=true` by default and exposes `allow_partial`.

- [ ] Preserve old call compatibility for `blueprint_get_logic`, `blueprint_get_logic_json`, and `blueprint_import_json_to_graph`.

- [ ] Ensure MCP result surfaces these fields when Bridge returns them:

```text
effective_scope
status
operations_applied
nodes_created
links_connected
warnings
errors
rolled_back
```

- [ ] Add fixture payloads for:

```text
legacy_full_graph_scope
legacy_full_blueprint_scope
strict_import_link_failure
strict_import_default_failure
```

- [ ] Run:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
npm.cmd run build
```

## Exit Criteria

- TypeScript build passes.
- Legacy scope values still compile and map to the new effective scope.
- MCP does not require callers to pass `auth_token` manually; it uses `BLUEPRINTHELPER_BRIDGE_TOKEN`.

