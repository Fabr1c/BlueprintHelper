# BlueprintHelper AgentGuide Template Pack 20260515

## Goal

Create copy-and-edit AgentGuide JSON templates that reduce CLI field-shape
errors, avoid fragile PowerShell `--json` quoting, and keep TaskSpec-first
authoring compact.

## Agreed Scope

- `Resources/AgentGuide/Templates/` contains the total category index and the
  template README.
- `Resources/AgentGuide/Templates/read/` is only for UE asset, logic, dependency,
  function-chain, member-reference, and task-context reads.
- `Resources/AgentGuide/Templates/write/` is for TaskSpec preview/execute and
  TaskSpec-first write templates.
- `Resources/AgentGuide/Templates/other/` contains non-context utility
  templates: runtime profile, diagnostics, AgentGuide, write-session, task
  result, debug-summary, debug bundle, and Review query inputs.
- `blueprinthelper_apply_review_action` is not included in ordinary AgentGuide
  templates. It remains plugin-development/internal only.

## Expected Verification

- Every template JSON file parses as valid JSON.
- Read templates match `ReadContextInputSchema`, `ReadTaskContextInputSchema`,
  or `ReadReferenceContextInputSchema` where applicable.
- Write templates match `TaskSpecSchema`, `PreviewTaskInputSchema`, or
  `ExecuteTaskInputSchema` where applicable.
- Root utility templates match their current tool input schemas where those
  schemas exist.
- CLI/task-core TypeScript build still succeeds after documentation/template
  changes.

## Progress

- [x] Template directories created.
- [x] Root category index created.
- [x] Other utility templates created under `other/`.
- [x] Read context templates created.
- [x] Write TaskSpec templates created.
- [x] AgentGuide index updated to point Agents to the template navigation.
- [x] Static JSON/schema validation completed.
- [x] TaskSpec compiler static validation completed for write templates.
- [x] TypeScript build completed.
- [x] CLI smoke completed with `blueprinthelper_read_agent_guide_template.json`.

## Notes

- This phase is documentation/template work. It does not require starting Unreal
  Editor because no UE asset read/write is executed.
- Validation found and fixed one variable template issue:
  `variable_strategy=blueprint_variables` and `kind=ensure_variable` were
  replaced with the compiler-supported `member_variables` /
  `ensure_member_variable` shape.
- Write-session asset-scoped template uses `scope=asset_list`, matching the
  current authorization service and MCP schema.

## Verification Results

- JSON parse: 64 template files passed.
- Schema validation: 58 schema-backed templates passed; the remaining root
  utility templates are object-only inputs for tools with open or empty schemas.
- Task compiler static check: 31 write templates compiled to TaskPlan without
  Bridge execution.
- TypeScript build: `npm.cmd --prefix AgentFaceService\cli run build` passed.
- CLI smoke: `blueprinthelper_read_agent_guide --file
  BlueprintHelper\Resources\AgentGuide\Templates\other\blueprinthelper_read_agent_guide_template.json`
  returned `status=completed`.

## 2026-05-17 Category And Semantic Index Sync

- Added `Resources/AgentGuide/Templates/INDEX.md` as the total category index
  for read, write, and other tool templates.
- Added `SEMANTIC_INDEX.md` under `read/`, `write/`, and `other/` so Agents can
  pick templates by intent instead of scanning filenames.
- Moved runtime/profile/diagnostics/guide/write-session/task-result/debug/review
  utility templates from the template root into `other/`.
- Added missing read templates for `blueprinthelper_read_function_chain_context`
  and member-level `blueprinthelper_read_reference_context` calls.
- Updated reference-context templates from legacy `scope/include_samples` to
  current `search_scope/resolution_policy/detail` fields.
- Verification after sync:
  - JSON parse: 69 template files passed.
  - Schema validation: 63 schema-backed templates passed.
  - Open or empty-shape utility inputs not schema-checked: runtime profile,
    diagnostics, AgentGuide, and write-session templates.
