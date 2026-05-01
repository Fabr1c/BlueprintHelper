# Worker B Import Execution

## Goal

Execute validated AgentImportGraph requests by creating real Blueprint nodes and links.

## Files

Modify:

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAgentImportService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAgentImportService.cpp
Source/BlueprintHelper/Public/TextToBlueprintGenerator.h
```

## Requirements

- Reuse existing `IBlueprintNodeHandler` implementations.
- Keep a local `Agent node id -> UEdGraphNode` map.
- Support member variable declarations through `declarations.variables`.
- Respect `options.create_missing_variables`.
- Return structured diagnostics with `error_code`, `path`, and `suggestion`.
- `dry_run=true` must not modify the Blueprint.
- Compile only when `options.compile=true`.
- Save only when `options.save=true`.

## Supported Nodes

```text
event
custom_event
call
get
set
branch
sequence
comment
```

