---
name: blueprint-helper-sourcecode-explorer
description: Fork the sourcecode-explorer sideAgent to collect compact C++/TypeScript/CLI/schema source evidence complementary to BlueprintExplorer.
context: fork
agent: sourcecode-explorer
---

# BlueprintHelper Source-Code Explorer Fork

Invoke the `sourcecode-explorer` sideAgent only when MainAgent needs source-side grounding that complements BlueprintExplorer evidence.

The MainAgent owns user clarification, lifecycle MCP, safety/write boundary decisions, TaskWorker dispatch, and final response. SourceExplorer returns compact source evidence to MainAgent and does not provide a TaskWorker package field named like an upstream source summary.

Input must be compact YAML:

```yaml
user_goal: "<what the user wants>"
source_search_goal: "<what source facts are needed>"
suspected_files_or_symbols: []
evidence_scope:
  - "C++ runtime / adapter / coordinator / service / Review / TaskRuntime"
  - "TypeScript CLI / task-core / schema / result-shape"
  - "config / test / template / generated contract"
required_output: []
stop_conditions: []
reasoning: "maximum_available"
```

Return only compact YAML evidence to MainAgent. Do not touch UE editor assets, request write session, run preview/execute, or act as TaskWorker's template chooser.
