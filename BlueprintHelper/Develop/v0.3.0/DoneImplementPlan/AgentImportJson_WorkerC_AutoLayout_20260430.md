# Worker C Auto Layout

## Goal

Generate deterministic node positions for AgentImportGraph requests.

## Requirements

- `auto` lays execution flow left to right.
- `append_right` starts to the right of existing graph nodes.
- Data nodes sit near their first consumer.
- Orphan nodes move to a lower separate area.
- Comment nodes are positioned after contained nodes and sized from their bounds.

## Initial Constants

```text
ExecLayerSpacingX = 420
ExecNodeSpacingY = 220
DataNodeOffsetX = -260
DataNodeOffsetY = -120
BranchSpacingY = 260
OrphanAreaOffsetY = 900
```

