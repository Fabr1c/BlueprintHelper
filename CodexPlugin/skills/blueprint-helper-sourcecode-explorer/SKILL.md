---
name: blueprint-helper-sourcecode-explorer
description: Fork the sourcecode-explorer sideAgent to collect compact repository source-code, schema, CLI, config, test, and template context for a BlueprintHelper task.
context: fork
agent: sourcecode-explorer
---

# BlueprintHelper Source-Code Explorer Fork

Invoke the `sourcecode-explorer` sideAgent with the smallest possible task package.

The Main Agent, not this sideAgent, owns user clarification, lifecycle MCP, safety/write boundary decisions, and final response.

Input must be compact YAML:

```yaml
user_goal: "<what the user wants>"
source_search_goal: "<what source facts are needed>"
suspected_files_or_symbols: []
required_output: []
stop_conditions: []
reasoning: "maximum_available"
```

Return only the sideAgent's compact YAML result to the Main Agent.

