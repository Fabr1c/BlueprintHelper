# BlueprintHelper Template Category Index

Use this file as the first stop when choosing a copy-and-edit JSON template.
The category semantic indexes contain the per-intent template lists.

| Category | Directory | Semantic index | Use for |
|---|---|---|---|
| Read tools | `read/` | `read/SEMANTIC_INDEX.md` | Read-only context before planning, authoring, verifying, or tracing custom Blueprint logic. |
| Write tools | `write/` | `write/SEMANTIC_INDEX.md` | TaskSpec-first preview, execute, asset creation, graph edits, signatures, components, variables, UMG, DataTable, DataAsset, and ownership operations. |
| Other tools | `other/` | `other/SEMANTIC_INDEX.md` | Runtime profile, diagnostics, AgentGuide entry, write authorization, task result lookup, debug summaries, debug bundle manifest, and Review record summaries. |

## Selection Rules

1. Use `read/` before authoring or repairing a TaskSpec.
2. Use `write/` only for TaskSpec preview or execute inputs.
3. Use `other/` for workflow support commands that are not asset-context reads
   and not TaskSpec authoring.
4. Do not add templates for deprecated MCP ordinary tools. The ordinary
   Agent-facing surface is the BlueprintHelper CLI; MCP is retained only for
   the explicit editor lifecycle and developer exec allowlist.

## Category Ownership

| New tool kind | Template category |
|---|---|
| Asset, graph, property, dependency, task-context, or function-chain read | `read/` |
| TaskSpec wrapper, bare TaskSpec, or write behavior example | `write/` |
| Runtime, diagnostics, guide, authorization, result, debug, or Review summary query | `other/` |

