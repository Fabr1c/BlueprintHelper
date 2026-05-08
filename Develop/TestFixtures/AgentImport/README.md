# AgentImport Fixtures

These fixtures are the first validation set for `BlueprintHelper.AgentImportGraph`.

Expected outcomes:

- `simple_beginplay_print.agent_import.json`: imports `BeginPlay -> PrintString`.
- `set_variable.agent_import.json`: creates or reuses `Health`, then sets it.
- `branch_flow.agent_import.json`: imports True and False execution branches.
- `invalid_pin.agent_import.json`: returns `InvalidLinkEndpoint`.
- `forbidden_pos.strict.agent_import.json`: returns a strict forbidden-field diagnostic.

The target paths are placeholders for editor validation and should be changed to an existing test Blueprint when running MCP acceptance checks.

