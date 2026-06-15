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

Return compact evidence summary as the primary response, not raw CLI output. Include only the evidence needed by MainAgent to decide target/scope/capability or whether to dispatch TaskWorker.
