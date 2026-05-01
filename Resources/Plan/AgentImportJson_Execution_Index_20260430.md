# AgentImportGraph Execution Index

## Goal

Implement `BlueprintHelper.AgentImportGraph` as an Agent-facing semantic Blueprint import protocol without changing the existing raw JSON import path.

## Scope

- Add `append` mode only.
- Support `event`, `custom_event`, `call`, `get`, `set`, `branch`, `sequence`, and `comment`.
- Support `exec` and `data` links.
- Generate node positions inside the plugin.
- Add Bridge command `import_agent_graph`.
- Add MCP tool `blueprint_import_agent_graph`.
- Keep `blueprint_import_json_to_graph` unchanged.

## Parallel Execution

| Worker | Document | Dependency |
|---|---|---|
| A | `AgentImportJson_WorkerA_ContractValidator_20260430.md` | none |
| B | `AgentImportJson_WorkerB_ImportExecution_20260430.md` | A + C |
| C | `AgentImportJson_WorkerC_AutoLayout_20260430.md` | none |
| D | `AgentImportJson_WorkerD_BridgeIntegration_20260430.md` | B |
| E | `AgentImportJson_WorkerE_MCPTool_20260430.md` | D |
| F | `AgentImportJson_WorkerF_FixturesValidation_20260430.md` | none |

Recommended order:

```text
A + C + F
-> B
-> D + E
-> build and MCP validation
```

## Verification

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

```powershell
npm run build
```

Run the MCP build from:

```text
G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
```

