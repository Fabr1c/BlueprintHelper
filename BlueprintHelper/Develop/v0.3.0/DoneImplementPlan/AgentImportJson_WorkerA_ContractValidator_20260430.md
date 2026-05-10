# Worker A Contract And Validator

## Goal

Create the AgentImportGraph contract, parser, and validator independent from the raw JSON validator.

## Files

Create:

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAgentImportService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAgentImportService.cpp
```

## Requirements

- Parse `BlueprintHelper.AgentImportGraph` version `1.0`.
- Require explicit `target_blueprint` and `target_graph`.
- Support only `mode: append`.
- Support `layout: auto` and `layout: append_right`.
- Normalize shorthand links like `begin_play.then`.
- Reject duplicate node ids.
- Reject unsupported node kinds and link kinds.
- Treat forbidden fields as warnings unless `options.strict` is true.

## Forbidden Fields

```text
Pos
PosX
PosY
NodePosX
NodePosY
NodeWidth
NodeHeight
GraphGuid
NodeGuid
PinGuid
PersistentGuid
CompilerMessage
ErrorType
ErrorMsg
AdvancedPinDisplay
bCommentBubbleVisible
CommentBubblePinned
```

