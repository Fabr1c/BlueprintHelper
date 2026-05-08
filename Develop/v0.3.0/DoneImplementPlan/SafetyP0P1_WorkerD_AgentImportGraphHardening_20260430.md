# Worker D AgentImportGraph Hardening

## Goal

Keep existing `AgentImportGraph` implementation, but bind it to the completed safety base before any capability expansion.

## Dependencies

Worker D starts only after Worker A, Worker B, and Worker C pass their exit criteria.

## Files

Modify:

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAgentImportService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAgentImportService.cpp
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
MCPServer/src/tools.ts
Source/BlueprintHelper/Private/Tests/BlueprintHelperSafetyTests.cpp
```

Do not add new supported node kinds in this worker.

## Steps

- [ ] Confirm `import_agent_graph` requires explicit `target_blueprint` and `target_graph`.

- [ ] Route graph resolution through the hard-fail graph resolver. A typo such as `EventGrph` must return `graph_not_found` and available graph names.

- [ ] Make strict mode default for `AgentImportGraph`.

- [ ] Convert internal result status to the same vocabulary as raw import:

```text
full_success
partial_success
no_op
failed
```

- [ ] Ensure created node, created link, warning, error, and rollback counts are returned.

- [ ] Use stable ids for existing node references. Title matching may be accepted only as read-only diagnostics and must return `ambiguous_node_ref` when multiple candidates exist.

- [ ] Reconstruct only newly-created nodes unless the request explicitly asks for existing-node reconstruction.

- [ ] Add automation case:

```text
BlueprintHelper.Safety.AgentImportGraph.SimpleBeginPlayPrintStringStrict
```

- [ ] Run UE build, MCP build, and safety automation.

## Exit Criteria

- Existing simple BeginPlay to PrintString import still works.
- Graph typo does not write to EventGraph.
- Partial link or default failure rolls back in strict mode.
- No new AgentImportGraph node capability is introduced by this worker.

