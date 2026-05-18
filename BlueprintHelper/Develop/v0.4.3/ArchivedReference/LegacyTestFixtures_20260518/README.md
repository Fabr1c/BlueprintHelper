# Legacy Test Fixtures Archive

Archived on 2026-05-18.

This directory preserves the former `BlueprintHelper/Develop/TestFixtures` content as historical reference only. The files describe v0.3.x-era MCP/raw-json/AgentImport compatibility behavior and must not be used as current AgentFace, CLI, AgentGuide, or TaskSpec-first templates.

Current behavior intentionally rejects the retired compatibility fields covered by these fixtures:

- export scopes `full_graph` and `full_blueprint`
- RawJson string-first fields `include_json_text`, `json_text`, `legacy_text_json`, and string-valued `json`
- old `blueprint_import_agent_graph` semantic payload fields `nodes`, `links`, `declarations`, and `layout`

Use `AgentFaceService/agent-guide/Templates` for current CLI templates and the live source tests under `AgentFaceService/*/src/tests` or `BlueprintHelper/Source/BlueprintHelper/Private/Tests` for executable coverage.
