---
name: blueprint-helper-sourcecode-worker
description: Fork the sourcecode-worker sideAgent to implement architecture-approved C++/DataAsset/interface source changes before BlueprintHelper TaskWorker wiring.
context: fork
agent: sourcecode-worker
---

# BlueprintHelper Sourcecode Worker Fork

Invoke the `sourcecode-worker` sideAgent only after MainAgent has completed the practical architecture gate and decided that source changes are required before UE asset wiring.

The MainAgent owns user clarification, lifecycle MCP, safety/write boundary decisions, source-control gate for UE assets, TaskWorker dispatch, and final response. SourcecodeWorker owns bounded repository source edits, source verification, and source-side handoff details.

- Do not touch UE editor assets.
- Do not call MCP tools.
- Do not run BlueprintHelper task preview commands.
- Do not run BlueprintHelper task execute commands.
- Do not run BlueprintHelper readback commands.
- Do not act as TaskWorker's template chooser.

Input must be compact YAML:

```yaml
schema: BlueprintHelper.SourcecodeWorkerPackage.v1
source_package_id: "<stable id>"
user_goal: "<gameplay/editor intent>"
architecture_decision:
  decision: "cpp_plus_blueprint | data_asset_contract | interface_extension | source_only_support"
  reason: "<why source work is required>"
  blueprint_node_budget:
    simple_limit: "less than 25 nodes per function/event/macro"
    complex_threshold: "30+ nodes belongs in C++"
write_scope:
  allowed_paths:
    - "<repo source path or directory>"
  forbidden_paths:
    - "<paths that must not be edited>"
source_tasks:
  - "<specific source change>"
blueprint_contract:
  extension_points:
    - "BlueprintImplementableEvent | BlueprintNativeEvent | interface override"
  data_driven_assets:
    - "UDataAsset | config struct | Blueprint exposed config variable"
verification:
  commands:
    - "<focused source verification command>"
  required: true
stop_conditions:
  - "write_scope_violation"
  - "architecture_gate_mismatch"
  - "verification_failed"
  - "source_capability_missing"
return_format: "compact Chinese YAML with files_changed, verification, blockers, and next step"
```

SourcecodeWorker must preserve the architecture rule that complex functionality belongs in C++ while Blueprint stays as light control-flow orchestration. Use `BlueprintImplementableEvent`, `BlueprintNativeEvent`, or overridable interfaces for Blueprint extension points; use `UDataAsset`, config structs, or explicitly exposed Blueprint config variables for data-driven content instead of hardcoded content. Do not add unnamed namespace blocks in C++.

Success requires source edits to stay inside `write_scope`, source verification to pass or be reported with exact failure evidence, and a clear handoff for MainAgent to either dispatch TaskWorker for Blueprint wiring or stop with a blocker. Return only the sideAgent's compact YAML result to MainAgent.
