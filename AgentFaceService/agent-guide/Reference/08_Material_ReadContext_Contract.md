# 08 - Material ReadContext Contract

This document records the Agent-facing contract for Material graph reads. It is
not a copyable template. Concrete request shapes remain in CLI-discovered read
templates.

## Request Boundary

1. Use `read_type: "material_graph_context"` for `UMaterial` graph reads.
2. Provide `target.asset_path`.
3. `target.target_type` may be omitted or set to `material_graph` / `asset`.
4. Do not provide `domain`, `graph_path`, material-specific strategy fields, or
   a third Material-specific format.
5. `material_graph_context` supports exactly `logic_json` and `logic_flow`.

## Format Semantics

1. `logic_json` is the source-of-truth readback payload for Material graph facts
   and write alignment.
2. `logic_flow` is an Agent-readable data-flow projection. It is not a write
   anchor source.
3. Material parameter facts come from `UMaterialExpression` nodes in the graph.
   They are not Material Instance override facts.

## Identity and Anchors

1. `expression_guid` is an internal/readback identity for precise UE
   expression selection.
2. Ordinary write requests should use semantic selectors, `node_key`,
   `block_id`, or preview-returned `candidate_id`, not hand-authored GUIDs.
3. `logic_json` may expose `expression_guid`, `block_id`, `node_key`,
   expression class, properties, links, and material output facts for
   read/write closure.
4. `logic_flow` may summarize those facts for readability, but anchors in that
   projection are expert/debug evidence only.

## Read/Write Closure

1. After `edit_material_graph` preview/execute, run
   `material_graph_context + logic_json` to confirm generated expressions,
   parameters, connections, material outputs, and owned anchors.
2. Use `logic_flow` to check the resulting Material data flow.
