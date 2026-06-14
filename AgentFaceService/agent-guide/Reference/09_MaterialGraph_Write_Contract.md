# 09 - MaterialGraph Write Contract

This document records the Agent-facing contract for Material graph writes. Use
the CLI template index for copyable TaskSpec files; do not hand-copy this
reference as a request body.

## Request Boundary

1. Use `task_type: "edit_material_graph"` for `UMaterial` graph writes.
2. Provide `target.asset_path`; omit `target.graph_path`.
3. `target.target_type` may be omitted or set to `material_graph`, `material`,
   or `asset`.
4. Do not provide `domain`; the task type selects the MaterialGraph domain.
5. Do not provide top-level `position`, `comment`, or `label`; layout is owned
   by the layout/readback/editor presentation layer.

## Strategies

1. `append_new_owned_graph` uses `behavior.entries[]`.
2. `replace_owned_graph` uses `behavior.replace`.
3. `patch_owned_graph` uses `behavior.patches[]`.
4. `merge_owned_graph` uses `behavior.merges[]`.

Each block must have a stable `block_id`. Each expression must have a stable
`node_key`. Ordinary Agent requests should use `node_key`, `block_id`, common
selectors, or preview-returned `candidate_id`; do not hand-author
`expression_guid`.

## Selectors

P0 common selectors:

1. `constant`
2. `scalar_parameter`
3. `vector_parameter`
4. `texture_object_parameter`
5. `texture_sample`
6. `add`
7. `multiply`
8. `static_switch_parameter`

Candidate selector:

1. Preview may use `selector: { "query": "..." }`.
2. Execute should use the preview-returned `selector: { "candidate_id": "..." }`.
3. A candidate selector must contain exactly one of `query` or `candidate_id`.

## Material Output Links

1. `$material_output` is a pseudo node and may only be used as `to.node_key`.
2. Common output pins include `BaseColor`, `Roughness`, `Specular`, `Metallic`,
   and `EmissiveColor`.
3. Generated expressions must be consumed by an outgoing material data
   connection unless the same operation deletes them.

## Read/Write Closure

After preview or execute, use `material_graph_context` with `logic_json` to
confirm `node_key`, `block_id`, expression properties, links, and material
outputs. Use `logic_flow` or `logic_md` only for compact human-readable
summaries.
