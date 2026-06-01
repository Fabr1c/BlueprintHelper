# BlueprintHelper Template Category Index

Use this file as the first stop when choosing a copy-and-edit JSON template.
The category semantic indexes contain the per-intent template lists.

| Category | Directory | Semantic index | Use for |
|---|---|---|---|
| Support tools | `./` | `SEMANTIC_INDEX.md` | Runtime profile, diagnostics, AgentGuide entry, unknown asset-path discovery, screenshot evidence capture, write authorization, task result lookup, debug summaries, debug bundle manifest, and Review record summaries. |
| Read tools | `read/` | `read/SEMANTIC_INDEX.md` | Read-only context before planning, authoring, verifying, or tracing custom Blueprint logic. |
| Write tools | `write/` | `write/SEMANTIC_INDEX.md` | TaskSpec-first preview, execute, asset creation, graph edits, signatures, components, variables, UMG, DataTable, DataAsset, and GraphWrite statement examples. |

## Selection Rules

1. If the Unreal `asset_path` is unknown, start with the root `blueprinthelper_find_assets_template.json` template.
2. Use `read/` only after one explicit Unreal `asset_path` is known.
3. Use `write/` only for TaskSpec preview or execute inputs.
4. Use root-level templates for workflow support commands that are not asset-context reads
   and not TaskSpec authoring.
5. Do not infer Unreal `asset_path` values from filesystem `.uasset` paths.
6. If `blueprinthelper_find_assets` returns multiple candidates, narrow the request or ask for confirmation before any write flow.
7. Templates are only for the ordinary BlueprintHelper CLI surface. MCP is
   retained only for explicit editor lifecycle open/close and is not a
   template surface for ordinary asset workflows.

## Category Ownership

| New tool kind | Template category |
|---|---|
| Asset, graph, property, dependency, task-context, or function-chain read | `read/` |
| TaskSpec wrapper, bare TaskSpec, or write behavior example | `write/` |
| Runtime, diagnostics, guide, asset discovery, screenshot evidence, authorization, result, debug, or Review summary query | `./` |
