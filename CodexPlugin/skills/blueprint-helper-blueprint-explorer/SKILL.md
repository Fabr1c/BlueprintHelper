---
name: blueprint-helper-blueprint-explorer
description: Fork the blueprint-explorer sideAgent to collect compact BlueprintHelper UE editor-asset evidence from a BlueprintHelper.BlueprintExplorerPackage.v1 package.
context: fork
agent: blueprint-explorer
---

# BlueprintHelper Blueprint Explorer Fork

Invoke the `blueprint-explorer` sideAgent when the MainAgent needs UE editor-asset evidence for Blueprint, Material, Animation, UMG, DataAsset, DataTable, object, property, graph, variable, component, widget tree, row, pin, link, or adapter-boundary context.

The MainAgent owns user clarification, lifecycle MCP, safety/write boundary decisions, and final response. BlueprintExplorer owns read discovery inside the package scope.

Input must be compact YAML:

```yaml
schema: BlueprintHelper.BlueprintExplorerPackage.v1
exploration_package_id: "<stable id>"
user_goal: "<editor/gameplay intent>"
target_hint:
  asset_path: "<known asset path or empty>"
  graph_or_scope: "<graph/function/event/widget/table/object scope or hint>"
evidence_scope:
  requested_facts:
    - "asset_candidates"
    - "graph_or_scope"
    - "anchors"
    - "nodes"
    - "pins"
    - "links"
    - "variables"
    - "components"
    - "widget_tree"
    - "table_rows"
    - "adapter_boundary"
  family_hint:
    - "normal_blueprint | material_blueprint | animation_blueprint | umg_widget | data_table | data_asset | object"
stop_conditions:
  - "asset_ambiguous"
  - "read_capability_missing"
  - "evidence_conflict"
  - "bridge_unavailable"
return_format: "compact Chinese YAML with evidence summary, confidence, missing facts, conflicts, and suggested next exploration"
```

BlueprintExplorer must discover the supported read surface itself: `bh context read`, ReadContext/ReadSpec shapes, read-template families, read-template lists, composed read specs, or other supported BlueprintHelper CLI read commands. Do not ask MainAgent for concrete read-template IDs.

Before reporting `read_capability_missing`, BlueprintExplorer must prove indexed ReadContext discovery was checked in scope:

```powershell
bh tools read-templates families --format json
bh tools read-templates clusters --family <family> --format json
bh tools read-templates list --family <family> --cluster <cluster> --format json
```

Treat `output.format` in the template index as the returned evidence shape, not as `view.format`. `view.format` must come from the leaf ReadSpec/template fields and current CLI schema. Classify failures precisely: `bridge_unavailable` means the Bridge request path is unavailable, `route_missing` means no active indexed route/template exists, wrong input or invalid enum means the ReadSpec is malformed, and `graph_body_target_unresolved` means the graph/function/event target could not be resolved from evidence.

For graph body evidence, `function_body` is a body kind, not ownership proof. User-authored event/function bodies require external-body evidence from `logic_flow`, especially `adapter_boundary.body_entry` and `body_fingerprint`; BlueprintHelper-owned replacement evidence is a separate route decision.

Return compact evidence summary as the primary response, not raw CLI output. Include only the evidence needed by MainAgent to decide target/scope/capability or whether to dispatch TaskWorker.
