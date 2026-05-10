# Worker D Bridge Integration

## Goal

Expose AgentImportGraph through the UE Bridge without affecting raw JSON import.

## Files

Modify:

```text
Source/BlueprintHelper/Public/BlueprintHelper.h
Source/BlueprintHelper/Private/BlueprintHelper.cpp
Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
```

## Requirements

- Register `FBlueprintHelperAgentImportService`.
- Add Bridge command `import_agent_graph`.
- Serialize the payload object into JSON text and pass it to the service.
- Return `BlueprintHelper.AgentImportResult`.
- Keep `import_json` behavior unchanged.

