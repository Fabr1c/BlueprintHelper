# Worker F Fixtures And Validation

## Goal

Add fixture inputs and manual validation notes for the first AgentImportGraph implementation.

## Files

Create:

```text
Resources/TestFixtures/AgentImport/simple_beginplay_print.agent_import.json
Resources/TestFixtures/AgentImport/set_variable.agent_import.json
Resources/TestFixtures/AgentImport/branch_flow.agent_import.json
Resources/TestFixtures/AgentImport/invalid_pin.agent_import.json
Resources/TestFixtures/AgentImport/forbidden_pos.strict.agent_import.json
Resources/TestFixtures/AgentImport/README.md
```

## Validation

- `simple_beginplay_print` imports and compiles.
- `set_variable` creates `Health` and sets it.
- `branch_flow` connects True and False branches.
- `invalid_pin` returns `InvalidLinkEndpoint`.
- `forbidden_pos.strict` returns a strict forbidden-field error.

