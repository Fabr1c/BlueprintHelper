# BlueprintHelper TaskSpec / TaskPlan Contract (2026-05-04)

## 1. Contract Status

This file is the human-readable contract for the TaskSpec-first implementation slice.

### 2026-05-16 Layout Deprecation Note

Layout is no longer a TaskPlan / GraphWrite responsibility. TaskPlan and GraphWrite must generate graph structure only: nodes, pins, links, defaults, ownership, review/debug facts. Any existing references to `set_node_position`, `node_position`, `preserve_layout`, `layout_hints`, fragment `layout`, or payload-level `layout:auto` are legacy compatibility notes, not current architecture direction.

The UE-side GraphLayout system is the only intended owner for configurable node placement. Existing low-level layout fields should be treated as deprecated until removed or migrated behind GraphLayout.

The machine-readable guard is:

```text
AgentFaceService/task-core/src/task/schema/task-contract.ts
AgentFaceService/task-core/src/tests/task/task-contract.test.ts
AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts
```

The supported chain is:

```text
Agent -> BlueprintHelper CLI -> AgentFace task-core TypeScript compiler -> TaskPlan structured IR -> Bridge/UE Task Runtime
```

Ordinary Agents submit `BlueprintHelper.TaskSpec.v1` only. They do not author `BlueprintHelper.TaskPlan.v1`. The canonical AgentFace task-core TypeScript compiler owns TaskPlan generation; UE Task Runtime executes the lowered adapter work derived from that IR.

TaskSpec compiler ownership: AgentFace task-core TypeScript compiler is the canonical production compiler. Legacy fallback and parity-gate paths are retired; new TaskSpec capabilities must be implemented and tested in TS first.

## 2. Versioned Schemas

| Data | Schema |
|---|---|
| Normalized tool result envelope | `BlueprintHelper.ToolResult.v1` |
| Read input | `BlueprintHelper.ReadSpec.v1` |
| Read context payload | `ReadContextPack.v1` |
| Reference context payload | `ReferenceContextPack.v1` |
| Function chain context payload | `FunctionChainContext.v1` |
| Agent input | `BlueprintHelper.TaskSpec.v1` |
| Compiler output | `BlueprintHelper.TaskPlan.v1` |
| Preview payload | `TaskPreviewResult.v1` |
| Execute summary payload | `TaskRunSummary.v1` |
| Runtime result | `BlueprintHelper.TaskRunJournal.v1` |
| Task error | `BlueprintHelper.TaskError.v1` |
| Contract metadata | `BlueprintHelper.TaskProtocolContract.v1` |

## 3. Ownership

| Owner | Writes | Reads |
|---|---|---|
| Agent | `BlueprintHelper.TaskSpec.v1`, `BlueprintHelper.ReadSpec.v1` | AgentGuide index, ReadContextPack, preview summary, task result |
| CLI/task-core Read Router | ReadContextPack / LogicFlow / LogicMD / LogicJson views | ReadSpec |
| CLI/task-core TypeScript Task Compiler | `BlueprintHelper.TaskPlan.v1` | TaskSpec |
| UE Task Runtime | `BlueprintHelper.TaskRunJournal.v1` | TaskPlan |
| Existing Capability Clusters | Bridge/UE operation result facts | TaskPlan step args |
| Review System | `BlueprintHelper.ReviewRecord.v1` from `BlueprintHelper.WriteReviewEvidence.v1` | Producer-owned write evidence, transaction rollback refs |
| Debug System | `BlueprintHelper.DebugCase.v1`, `BlueprintHelper.DebugBundleManifest.v1` | ToolResult failure summary, TaskRuntime/Bridge/Review/Transaction failure evidence |

Rules:

- Agent must not submit a TaskPlan.
- CLI/task-core task tools must not expose low-level atomic tool planning as the default Agent workflow.
- UE Task Runtime must treat TaskPlan as the internal execution contract.
- Existing Bridge/UE operation results remain internal facts and are normalized into task-level results.
- Read tools should not grow one tool per UE asset domain. New read capability domains should enter through `BlueprintHelper.ReadSpec.v1` and a read router, while write capability domains enter through `BlueprintHelper.TaskSpec.v1` and TaskPlan.
- TaskSpec keeps a small semantic top-level surface. A single semantic TaskSpec may combine multiple capability clusters through compiler decomposition, but those clusters must not become new default Agent-facing atomic tools.
- Preview may expose a TaskPlan summary and debug/expert details, but ordinary Agents do not depend on full TaskPlan payloads or adapter payloads.
- TaskRuntime execution policy is: dry-run all executable steps first, execute steps sequentially only after dry-run passes, record partial failure in TaskRunJournal if execution fails mid-run, and block dependent downstream steps by TaskPlan topology. It does not promise global rollback by default.
- Every asset-mutating write cluster must provide producer-owned Review evidence. ReviewStore consumes evidence and does not invent missing atomic anchors.
- DebugCase / DebugBundle are developer diagnostics. CLI/task-core may expose summary-only `debug_case_ids[]` and `get_debug_case`, but must not return DebugBundle artifact contents, local bundle paths, raw payloads, token/settings full values, or source content.
- task-core orchestration owns TaskSpec validation, TaskPlan generation, TaskPlan summary, and compiler error normalization. It never writes UE assets and never creates ReviewRecord or DebugBundle artifacts.
- TaskPlan execution uses TaskRuntime as the main Review / Debug / Transaction convergence point.
- Non-TaskPlan paths may call fixed System Entry points directly. Examples: malformed Bridge request -> DebugEntry, standalone compile/save failure -> DebugEntry, ReviewAction reject failure -> DebugEntry, rollback/cleanup expert failure -> DebugEntry, Debug export failure -> DebugEntry or DebugCaseStore failure result.
- Non-TaskPlan access does not permit arbitrary persistence. Producers must still use ReviewStore / ReviewAction / TransactionJournal / DebugEntry instead of writing ReviewRecord, Transaction Journal, DebugCase, or DebugBundle files directly.

## 4. Agent-Facing Tools

Default Agent-facing tools are:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
```

Editor lifecycle is not a CLI TaskSpec/ReadSpec surface. Agent-owned open/close uses the global MCP lifecycle tools `mcp__blueprint_helper__blueprint_open_editor` and `mcp__blueprint_helper__blueprint_close_editor`; compatibility for `blueprint_open_editor` / `blueprint_close_editor` must resolve to those MCP tools rather than CLI lifecycle aliases. CLI lifecycle invocation is blocked; if lifecycle MCP is unavailable, report `lifecycle_mcp_unavailable`.

Task tools take a wrapped TaskSpec parameter. Do not pass the TaskSpec fields directly as the CLI direct-tool root arguments:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Low-level tools remain available only as internal, debug, expert, or test entries while their TaskSpec / ReadSpec replacements are not complete.

Deprecated atomic tool removal policy:

- If a capability already has a TaskPlan adapter and TaskSpec compiler coverage, remove or hide the corresponding old Agent-facing atomic write tools first.
- If a capability does not yet have adapter + TaskSpec coverage, keep its old tools as legacy/internal/debug/expert entries until that coverage lands, then remove them in the same implementation slice.
- Direct read tools follow the same rule after `blueprinthelper_read_context` covers their read domain.
- Do not add new Agent-facing atomic tools for newly expanded UE capability clusters.

## 4.1 Result Envelope Layering

Default Agent-facing result layering is fixed as:

| Tool | Outer shape | `data.schema` / payload |
|---|---|---|
| `blueprinthelper_read_agent_guide` | Markdown text only | none |
| `blueprinthelper_read_context` | `BlueprintHelper.ToolResult.v1` | `ReadContextPack.v1`, with `payload.schema = LogicFlow.v1 / LogicMd.v1 / LogicJson.v1 / ...` |
| `blueprinthelper_read_context_capabilities` | `BlueprintHelper.ToolResult.v1` | `ReadContextCapabilities.v1` |
| `blueprinthelper_read_reference_context` | `BlueprintHelper.ToolResult.v1` | `ReferenceContextPack.v1` |
| `blueprinthelper_read_function_chain_context` | `BlueprintHelper.ToolResult.v1` | `FunctionChainContext.v1` |
| `blueprinthelper_preview_task` | `BlueprintHelper.ToolResult.v1` | `TaskPreviewResult.v1` |
| `blueprinthelper_execute_task` | `BlueprintHelper.ToolResult.v1` | `TaskRunSummary.v1` by default, or `TaskRunJournal.v1` when returning the full journal |
| `blueprinthelper_get_task_result` | `BlueprintHelper.ToolResult.v1` | `TaskRunJournal.v1` |

UE Agent-facing facade commands return `FBlueprintHelperToolResultBase`. task-core task/read tools normalize those results into `BlueprintHelper.ToolResult.v1` for full artifacts. Existing Bridge / UE operation results are internal facts for compiler/runtime/journal use; ordinary Agents should not depend on raw adapter payloads.

Debug data must not expand the default top-level shape. Put compact debug facts under `data.debug` only when directly useful. Large asset context should be read through targeted `logic_flow` / `logic_md` / `logic_json` slices. Developer diagnostics use summary `DebugCase` ids and local `DebugBundle` exports; default tool responses must not expose bundle artifacts, local paths, raw payloads, source content, or large payload refs.

Long-term read entry consolidation is:

```text
Agent -> CLI -> ReadSpec -> task-core/Python Read Router -> UE Read Capability Cluster -> ReadContextPack / LogicFlow / LogicMD / LogicJson
```

`blueprinthelper_read_agent_guide` returns the AgentGuide onboarding index document. Agents use that index to discover the currently documented capability surface and then open the specific AgentGuide files for concrete ReadSpec / TaskSpec formats. This is a documentation entry point, not a runtime capability-schema tool.

`blueprinthelper_read_context` is the generic and only asset-domain ReadSpec entry. Task-authoring summary reads must be expressed through ReadSpec slices, runtime/profile diagnostics, or task result queries instead of a separate task-context path.

`blueprinthelper_read_context_capabilities` is an independent local discovery tool for the ReadContext capability matrix. It does not read assets and does not call Bridge. Its payload uses full-set fields for `asset_types`, `formats`, and `read_type_ids`; `read_types[]` is a negative diff listing only unsupported asset types and unsupported formats for each read type.

`blueprinthelper_read_reference_context` remains an independent Agent-facing read tool. It is a generic reference viewer for asset, variable, function, graph, widget, data table row, and other reference scopes. Its implementation may internally compose multiple reference analyzers, but the Agent sees one reference context envelope.

`blueprinthelper_read_function_chain_context` remains an independent Agent-facing read tool for project-authored Blueprint logic traversal. It starts from one function/event/custom event and returns only compact `custom_logic_refs[]` indexes for follow-up `blueprinthelper_read_context` calls. It must not echo request `entry` / `target` / `query`, expose owner fields, expose `node_ref` / `node_path`, expose GUID fields, or list Engine/trusted plugin/native utility calls beyond summary counts.

Editor lifecycle tools should use the `blueprinthelper_*` prefix. `blueprint_undo` and `blueprint_redo` are not part of the default future surface; recovery should move to transaction-level undo/redo or task journal replay instead of global editor undo/redo.

ReadSpec uses explicit view formats only for logic reads:

| Format | Purpose |
|---|---|
| `logic_flow` | Most compact view. Use for simple function/event/custom event reads when the Agent first needs execution/data flow understanding. Returns `LogicFlow.v1` with `mode=execflow` or `mode=dataflow`. |
| `logic_md` | Medium compact human-readable view. Use for larger or more branched function/event/custom event reads. |
| `logic_json` | Structured view for full reads, precise analysis, diff, patch, merge, anchors, and debug. |
| `raw_json` | Debug/expert full-fidelity view; not a default Agent workflow. |

`logic_flow`, `logic_md`, and `logic_json` are not Blueprint-only formats. Future material, animation blueprint, widget, data table, and data asset reads should use the same formats where practical, so the tool surface does not expand one command per read shape.

Initial ReadSpec shape:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_",
    "asset_type": "_",
    "target_type": "event",
    "target_name": "_",
    "block_id": "_"
  },
  "view": {
    "format": "logic_flow",
    "max_items": 200
  },
  "context": {
    "context_id": "_",
    "task_run_id": "_"
  }
}
```

`target.target_name` is interpreted by `target.target_type`:

| `target_type` | `target_name` meaning |
|---|---|
| `graph` | Graph name |
| `function` | Function name |
| `event` | Event name |
| `custom_event` | Custom Event name |
| `component` | Component name |
| `member_variable` | Member variable name |
| `event_dispatcher` | Event dispatcher name |
| `widget` | Widget name |
| `data_table_row` | Row name |

`block_id` remains separate because it is an ownership/id target, not a display name.

Initial supported `read_type` values:

| `read_type` | Purpose |
|---|---|
| `asset_context` | Asset identity from the ToolResult target plus asset class, parent class, and capability summary. The payload does not repeat target path or asset name. |
| `blueprint_logic` | Blueprint logic by blueprint, graph, function, event, custom event, or block target. |
| `component_context` | Blueprint SCS component tree and component property summaries. |
| `variable_context` | Member variables, local variables, defaults, categories, and type summaries. |
| `graph_context` | Graphs, function graphs, macro graphs, and event dispatcher lists. |
| `widget_context` | WidgetTree and widget property summaries. |
| `data_table_context` | DataTable schema, rows, and row summaries. |
| `object_property_context` | UObject / DataAsset reflected property summaries. |

Future read domains such as material graphs and animation blueprints must enter through new `read_type` values instead of new tools.

`view.format=summary` is removed from ReadSpec. Runtime capability discovery belongs to `blueprinthelper_read_context_capabilities`, not to a ReadSpec view format. Non-logic read domains use their `read_type` as the compact view contract and omit `view.format`. Neither path performs writes or returns TaskSpec drafts.

Read result `payload.schema` is the single result-shape marker for the concrete read payload. `ReadContextPack.v1` does not include a separate `read_id`, does not echo request `read_type` / `view.format` under `data`, and read results do not carry a `diagnostics` array. Payload errors use the outer ToolResult error envelope; read completeness uses `truncated` plus a recommendation to reread a specific block or context slice.

```json
{
  "schema": "BlueprintHelper.ToolResult.v1",
  "operation": "read_context",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "_",
    "asset_type": "_",
    "target_type": "graph",
    "target_name": "_",
    "block_id": "_"
  },
  "data": {
    "schema": "ReadContextPack.v1",
    "payload": {
      "schema": "LogicMd.v1"
    },
    "truncated": false
  }
}
```

`data.schema` and nested payload schemas do not repeat the `BlueprintHelper.` prefix. `payload.schema` is kept because it distinguishes `LogicFlow.v1`, `LogicMd.v1`, `LogicJson.v1`, `AssetContext.v1`, and other concrete payload contracts without also echoing `data.read_type` or `data.format`.

For `LogicFlow.v1`, `payload.mode` is `execflow` or `dataflow`. `payload.flow` is the compact text body, `payload.stats` is the only place for node/link/count statistics, and `payload.warnings` records compression risk such as unknown links or ambiguous macro boundaries. `LogicFlow.v1` is a read-to-understand view only; write anchors still require `logic_json`.

For `LogicMd.v1`, `payload.stats` is the only place for node/link/count statistics. `payload.markdown` omits duplicate stats summary lines and keeps the readable graph, entry, execution, dependency, and orphan sections.

## 5. Supported GraphWrite Slice

GraphWrite is Agent-facing only through semantic `TaskSpec.behavior.graph_strategy` values. Adapter operation names remain runtime lowering details.

| Field | Value |
|---|---|
| `task_type` | `edit_blueprint_graph` |
| `target.target_type` | `blueprint` |
| `behavior.graph_strategy` | `append_new_owned_graph`, `replace_owned_graph`, `patch_owned_graph`, `merge_owned_graph`, `merge_external_flow`, `patch_external_graph`, `replace_external_body` |
| `TaskPlan.steps[].capability` | `graph_write` |
| `TaskPlan.steps[].write.strategy` | `owned_graph_edit` or `external_graph_edit` |
| `TaskPlan.steps[].write.ops[].op` | owned: `ensure_entry`, `replace_body`, `set_pin_default`, `set_node_comment`, `connect_pins`, `disconnect_link`, `replace_link`, `delete_owned_node`, `insert_flow`; external: `insert_external_flow`, `set_external_pin_default`, `set_external_node_comment`, `replace_external_body` |
| `TaskPlan.steps[].constraints.allow_modify_user_nodes` | `false` |
| Step batching | append entries may share one step; replace/patch/merge/external strategies compile to one structural op per step |

`append_new_owned_graph` uses `behavior.entries[]`:

```text
behavior.entries[]:
  - entry_type = custom_event
  - body = BlueprintLogicSpec.v1
Graph body uses compact semantic kinds such as `call`, `get`, `set`, `get_property`, `set_property`, `op`, `construct`, `deconstruct`, `select`, and `control`. Legacy graph body shapes such as `call_function`, `set_member_variable`, `ref`, `compare`, and `make_struct` are unsupported and must fail preview with an explicit unsupported kind diagnostic.
```

`replace_owned_graph` uses one `behavior.replace` object:

```text
behavior.replace.scope:
  - custom_event_body
  - function_body
  - event_body
  - block_implementation

behavior.replace.selector:
  - kind
  - name
  - block_id
  - graph_id
  - node_ref

behavior.replace.body:
  - schema = BlueprintLogicSpec.v1

behavior.replace.options:
  - strict
```

`patch_owned_graph` uses `behavior.patches[]`:

```text
patches[].kind:
  - set_pin_default
  - set_node_comment
  - connect_pins
  - disconnect_link
  - replace_link
  - delete_owned_node

set_pin_default:
  - target_ref.block_id
  - target_ref.group_entry_node_path
  - target_ref.node_ref
  - target_ref.pin_ref
  - value

set_node_comment:
  - target_ref.block_id
  - target_ref.group_entry_node_path
  - target_ref.node_ref
  - value

connect_pins:
  - target_ref.block_id
  - target_ref.group_entry_node_path
  - target_ref.node_ref
  - target_ref.pin_ref
  - source_ref.node_ref
  - source_ref.pin_ref

disconnect_link:
  - target_ref.block_id
  - target_ref.group_entry_node_path
  - target_ref.node_ref
  - target_ref.pin_ref
  - target_ref.link_ref

replace_link:
  - target_ref.block_id
  - target_ref.group_entry_node_path
  - target_ref.node_ref
  - target_ref.pin_ref
  - target_ref.link_ref
  - replacement_ref.node_ref
  - replacement_ref.pin_ref

delete_owned_node:
  - target_ref.block_id
  - target_ref.group_entry_node_path
  - target_ref.node_ref
  - delete_policy.break_links
  - delete_policy.allow_entry_node
  - delete_policy.allow_lifecycle_root

behavior.patches[].scope:
  May be derived by compiler; Agent does not need to provide it as mandatory.
```

`merge_owned_graph` uses `behavior.merges[]`:

```text
merges[].kind:
  - insert_flow

merges[].scope:
  - owned_block_call
  - custom_event_call
  - function_call

merges[].insert_strategy:
  - append_after
  - insert_between
  - branch_fork

merges[].anchor:
  - block_id
  - group_entry_node_path
  - node_ref
  - pin_ref
  - link_ref only when insert_strategy = insert_between

merges[].inserted:
  - call_kind
  - name
  - block_id

merges[].sequence_order:
  only when insert_strategy = branch_fork
```

Agents must not:
- merge these three strategies into `behavior.entries`.
- model them as generic ops.
- place Bridge/runtime payload fields (for example `append_blueprint_graph` args) directly in TaskSpec.
Function call statement arguments use `args` with structured literal values. Legacy `params` and legacy `call_function` shapes are unsupported.

Function call statement arguments use this shape:

`call.name` may be a native function name, a Blueprint display name, an owner-qualified native name, or an explicit component/member call for append-owned graph writes. Preview resolves the function portion against the target Blueprint graph. If the name is ambiguous, change `name` to an owner-qualified native name and preview again. Explicit component/member calls remain strategy-limited; merge-owned graph writes still require a separate target-wiring path before this syntax can be enabled there.

```json
{
  "kind": "call",
  "target": "PrintString",
  "args": {
    "InString": {
      "kind": "literal",
      "value_type": "string",
      "value": "message"
    }
  }
}
```

Blocked first-slice example:

```json
{
  "kind": "call",
  "target": "DoorMesh.AddAngularImpulseInDegrees",
  "args": {}
}
```

Runtime lowering targets `append_blueprint_graph`, `replace_blueprint_graph`, `patch_blueprint_graph`, or `merge_blueprint_graph` depending on the structural op. These are Runtime adapter targets only, not the primary Agent-authored contract.

### 5.1 Patch/Merge Write Anchor Contract

Patch/Merge write anchors follow the v0.3.6 grouped LogicJson / block-scoped contract.

For BlueprintHelper-owned content, the Agent-facing main path is:

```text
block_id
group_entry_node_path
group-local node_ref / pin_ref / link_ref
```

`block_id` scopes the owned block. Group-local refs select the concrete node, pin, or link inside that block. This lets the Agent patch values, comments, positions, or insert flow without treating the entire graph node array as a stable write surface.

The following are not the default Agent-facing write contract:

- unscoped graph-level LogicJson array indexes such as `nodes[0]`.
- display names as locators.
- GUID-first selectors.
- ad hoc JSONPath strings.

Inside a `block_id` scope, group-local refs such as `nodes[0]` or `links[0]` are valid because they are interpreted only within that BlueprintHelper-owned block. UE GUIDs may still appear in expert/debug paths and internal diagnostics, but ordinary TaskSpec generation should prefer block-scoped anchors. Non-BlueprintHelper-owned graph content still needs a separate stable read/write anchor decision.

Merge anchor rules verified by smoke:

- `append_after` uses `block_id + group_entry_node_path + node_ref + pin_ref`.
- `insert_between` uses `block_id + group_entry_node_path + node_ref + pin_ref + link_ref`.
- `link_ref` alone is never enough; it selects the old connection but does not identify the source node/pin anchor required by compiler and UE resolver validation.
- `branch_fork` remains contract-defined but still needs a UE smoke fixture.

Current owned-graph implementation note, 2026-05-06 Rerun 4:

```text
Contract/compiler coverage: append_new_owned_graph, replace_owned_graph, patch_owned_graph, merge_owned_graph.
Confirmed execute coverage: append_new_owned_graph with a fresh graph name, replace_owned_graph, patch_owned_graph on a BlueprintHelper-owned block, merge_owned_graph insert_between + function_call, merge_owned_graph append_after + function_call, and merge_owned_graph insert_between + custom_event_call.
Confirmed read/write anchor coverage: LogicJson grouped block output includes block_id and group_entry_node_path; Patch/Merge resolve group-local node_ref / pin_ref / link_ref within that owned block.
Ownership metadata write target: new writes store machine ownership fields in FMetaData, not NodeComment.
Remaining owned-graph gaps at that checkpoint: append_after + custom_event_call currently returns an empty preview error; branch_fork is not smoke-tested.
```

External user-authored graph strategies are separate contracts, not owned merge aliases. `merge_external_flow` uses `behavior.external_merges[]`, lowers to `external_graph_edit` / `insert_external_flow`, and requires `scope_policy.external_mutation_policy = { strategy: "merge_external_flow", allowed_mutations: ["exec_boundary_link"] }` with `allow_modify_user_nodes=false`.

Ordinary Agents should still prefer `append_new_owned_graph` with a new BlueprintHelper-owned graph for new isolated logic. When modifying BlueprintHelper-owned blocks, use `replace_owned_graph`, `patch_owned_graph`, or `merge_owned_graph` only with block-scoped LogicJson anchors. When connecting new BlueprintHelper-owned logic into user-authored execution flow, use `merge_external_flow` only with stable external exec-boundary anchors or `BlueprintHelper.LogicJsonAnchorSelector.v1`. Always preview first. If preview blocks, stop and report instead of attempting a lower-level write tool fallback.

Ownership metadata and NodeComment migration decision, 2026-05-07: new BlueprintHelper-owned graph writes must store machine ownership fields such as `block_id`, transaction ids, and other resolver metadata in UE `FMetaData`. They must not serialize `block_id` or `tx` into `NodeComment`.

`NodeComment` is human-facing comment text. A `block_id` found in `NodeComment` is legacy fallback only for older assets that do not yet expose the ownership metadata through `FMetaData`. Readers and resolvers may use that fallback to avoid breaking existing assets, but it must not become the default write path.

The current slice is not a migration that deletes or rewrites old asset comments. A later migration or repair pass may clean legacy `NodeComment` ownership fragments after the fallback behavior, audit output, and smoke coverage are agreed. This does not change the TaskSpec / TaskPlan mainline: Agents still use grouped LogicJson / block-scoped anchors, and the compiler still emits structured TaskPlan IR.

The second executable slice is Blueprint Variables:

| Field | Supported value |
|---|---|
| `TaskSpec.task_type` | `edit_blueprint_variables` |
| `TaskSpec.behavior.variable_strategy` | `member_variables`, `member_defaults`, `local_variables` |
| `TaskSpec.behavior.changes[].kind` | `ensure_member_variable`, `configure_member_variable`, `remove_member_variable`, `ensure_local_variable`, `configure_local_variable`, `remove_local_variable` |
| `TaskSpec.behavior.defaults[]` | member default changes |
| `TaskPlan.steps[].capability` | `blueprint_variable` |
| `TaskPlan.steps[].write.strategy` | `member_variables`, `member_defaults`, `local_variables` |

Current Blueprint Variables lowering targets `add_blueprint_member_variables` for ensure-only member batches, or `blueprint_variable_batch` for mixed member/default/local ops. Preview uses the service dry-run path where implemented.

The first composite slice is Blueprint Feature composition:

| Field | Supported value |
|---|---|
| `TaskSpec.task_type` | `create_blueprint_feature` |
| `TaskSpec.components[]` | Component ensure plus optional property settings |
| `TaskSpec.variables[]` | Member variable ensure plus optional member defaults |
| `TaskSpec.class_settings.implemented_interfaces[]` | Interface ensure via class settings |
| `TaskSpec.behavior` | Same GraphWrite semantic object as `edit_blueprint_graph` |
| `TaskSpec.integration.interface` | Ensure interface implementation entry and replace its function body |
| `TaskPlan.steps[].capability` | `blueprint_component`, `blueprint_variable`, `blueprint_class_settings`, `blueprint_signature`, `graph_write` |

`create_blueprint_feature` is not a new UE mega-tool. It is a compiler-owned decomposition layer: Agent writes one semantic feature TaskSpec, the canonical TypeScript compiler emits multiple existing capability steps, and UE Task Runtime lowers each step through the existing clusters. The current executable slice supports `integration.interface` by lowering it to class settings, function signature, and GraphWrite function-body steps. It rejects `integration.input` as an explicit out-of-scope area, matching the current UE-side capability boundary, and still rejects `scope_policy.allow_create_assets=true` so asset creation is not silently skipped.

Broader DataAsset/ObjectProperty, Cleanup/Ownership, and large debug payload export remain future contract extensions. Input mapping integration is intentionally cut from the current roadmap until it is rechartered as a separate capability area.

### Signature Agent-Facing Contract

Agents write only `BlueprintHelper.TaskSpec.v1`; they must not author `BlueprintHelper.TaskPlan.v1` or raw Bridge payloads directly. Signature lifecycle is now exposed as the Agent-facing `edit_blueprint_signature` TaskSpec type, and the compiler lowers it to compiler-owned `blueprint_signature` TaskPlan steps.

Signature inputs and outputs use `SignaturePinSpec`. `SignaturePinSpec.pin_type` uses the shared `BlueprintPinTypeSpec` object shape, for example `{ "category": "int" }` or `{ "category": "string", "container_type": "map", "value_type": { "category": "int" } }`. New Agent-facing input rejects legacy string PinType tokens such as `"int"` or `"category=wildcard|container=map"`.

When an existing function has a different signature from the requested `ensure_function` contract, preview and execute both block with `function_signature_mismatch`. The result includes structured signature differences; callers should treat that as a contract mismatch, not as a best-effort rewrite request.

## 6. TaskSpec Required Fields

Agent must provide these fields for GraphWrite:

```text
schema
task_type
target.asset_path
scope_policy.graph_name
scope_policy.allow_modify_user_nodes
behavior.graph_strategy
validation.should_compile
validation.should_save
```

For `append_new_owned_graph`:

```text
behavior.entries[]
behavior.entries[].entry_type
behavior.entries[].name
behavior.entries[].body
```

For `replace_owned_graph`:

```text
behavior.replace
behavior.replace.scope
behavior.replace.selector
behavior.replace.body
behavior.replace.options
```
`behavior.replace.body` must be `BlueprintLogicSpec.v1`.

For `patch_owned_graph`:

```text
behavior.patches[]
behavior.patches[].kind
behavior.patches[].value
behavior.patches[].target_ref
```
`behavior.patches[].target_ref` must follow strategy-specific shape described in section 5.

For `merge_owned_graph`:

```text
behavior.merges[]
behavior.merges[].kind
behavior.merges[].scope
behavior.merges[].insert_strategy
behavior.merges[].anchor
behavior.merges[].inserted
behavior.merges[].sequence_order (only when strategy is branch_fork)
```

Agent may provide:

```text
context_id
feature_name (display / journal label only)
target.target_type
execution_policy.dry_run_mode
execution_policy.on_missing_capability
```

`feature_name` must not be used to infer graph names, function names, variables, block ids, or any other edit target. Agent must read the runtime/profile naming guidance and explicitly fill target fields such as `scope_policy.graph_name`.

Agent must not provide:

```text
intent
keys named compile or save inside validation
Bridge/runtime payload fields (for example append_blueprint_graph/replace_blueprint_graph/patch_blueprint_graph/merge_blueprint_graph arguments)
```

`intent` is generated after a completed task by the task-core/Python orchestration layer and recorded in the TaskRunJournal as `generated_intent`; it is not an Agent-authored TaskSpec field.

For composite Blueprint feature creation, Agent must provide:

```text
schema = BlueprintHelper.TaskSpec.v1
task_type = create_blueprint_feature
target.asset_path
at least one of components[], variables[], class_settings, behavior
validation.should_compile
validation.should_save
```

When `behavior` is present, Agent must also provide `scope_policy.graph_name`. The current composite slice supports only `integration.interface`; `integration.input` is rejected and should not be modeled in TaskSpec for this phase. `scope_policy.allow_create_assets=true` must be split into later TaskSpecs after the relevant asset creation capability is explicitly enabled.

## 7. TaskSpec Example

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_example",
  "task_type": "edit_blueprint_graph",
  "feature_name": "StoneGateActivation",
  "target": {
    "asset_path": "/Game/Blueprints/BP_StoneGate",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "BH_StoneGateActivation",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "InitializeStoneGate",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "call",
              "target": "PrintString",
              "args": {
                "InString": {
                  "kind": "literal",
                  "value_type": "string",
                  "value": "Stone gate initialized"
                }
              }
            }
          ]
        }
      }
    ]
  },
  "integration": {
    "interface": {
      "interface_asset": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
      "function": "Interact",
      "implementation": {
        "call": "OpenPhysicsDoor"
      }
    }
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report"
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

Composite physical door core example:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_physics_door",
  "task_type": "create_blueprint_feature",
  "feature_name": "PhysicsDoor",
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "prefer_new_graph": true,
    "graph_name": "EG_PhysicsDoor",
    "allow_modify_user_nodes": false,
    "allow_create_assets": false
  },
  "asset_policy": {
    "if_target_asset_missing": "fail",
    "if_referenced_asset_missing": "fail",
    "if_component_exists": "reuse_if_type_matches"
  },
  "resources": {
    "static_meshes": {
      "door_mesh": "/Game/BlueprintHelperTest/Meshes/SM_Door"
    }
  },
  "components": [
    {
      "name": "SceneRoot",
      "class": "SceneComponent",
      "set_as_root": true
    },
    {
      "name": "DoorMesh",
      "class": "StaticMeshComponent",
      "attach_to": "SceneRoot",
      "properties": {
        "StaticMesh": "$resources.static_meshes.door_mesh",
        "Mobility": "Movable",
        "CollisionProfileName": "PhysicsActor",
        "BodyInstance.bSimulatePhysics": true
      }
    }
  ],
  "variables": [
    {
      "target": "bDoorOpen",
      "type": "bool",
      "default": false,
      "category": "Door"
    },
    {
      "name": "OpenImpulse",
      "type": "float",
      "default": 50000.0,
      "category": "Door"
    }
  ],
  "class_settings": {
    "implemented_interfaces": [
      "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
    ]
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "OpenPhysicsDoor",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "set",
              "target": "bDoorOpen",
              "value": {
                "kind": "literal",
                "value_type": "bool",
                "value": true
              }
            },
            {
              "kind": "call",
              "target": "DoorMesh.AddAngularImpulseInDegrees",
              "args": {
                "VelChange": {
                  "kind": "literal",
                  "value_type": "bool",
                  "value": true
                }
              }
            }
          ]
        }
      }
    ]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report"
  },
  "validation": {
    "should_compile": true,
    "should_save": false
  }
}
```

## 8. TaskPlan Required Fields

TaskPlan is compiler-owned. For the first GraphWrite slice, the compiler must produce:

```text
schema
task_type
target_assets[]
execution_policy.dry_run_mode
execution_policy.should_compile
execution_policy.should_save
steps[].step_id
steps[].capability
steps[].target.asset_path
steps[].target.graph
steps[].write.strategy
steps[].write.ops[]
steps[].constraints.allow_modify_user_nodes
steps[].constraints.ownership_scope
steps[].depends_on
```

For a structured GraphWrite IR step, `steps[].operation` is forbidden. Adapter operation names such as `append_blueprint_graph` may appear only in runtime lowering details, internal child execution results, or TaskRunJournal facts.

`steps[].depends_on` is an optional array of prior `step_id` values. Omitted or empty means the step has no explicit upstream dependency. The compiler should emit dependencies when a later step consumes a capability, entry, function, variable, component, asset, or graph produced by an earlier step.

The current TaskPlan is sent to Bridge task-level preview/execute as:

```json
{
  "task_plan": {
    "schema": "BlueprintHelper.TaskPlan.v1"
  }
}
```

Bridge task-level commands:

```text
preview_task_plan
execute_task_plan
get_task_run_journal
```

Runtime adapter payloads such as `append_blueprint_graph` remain lowering targets and may appear in internal child execution details or journal facts, but they are not the primary TaskPlan example contract.

## 9. TaskPlan Example

```json
{
  "schema": "BlueprintHelper.TaskPlan.v1",
  "task_name": "StoneGateActivation",
  "task_type": "edit_blueprint_graph",
  "context_id": "ctx_example",
  "target_assets": ["/Game/Blueprints/BP_StoneGate"],
  "execution_policy": {
    "dry_run_mode": "full",
    "should_compile": true,
    "should_save": false
  },
  "steps": [
    {
      "step_id": "step_001",
      "capability": "graph_write",
      "target": {
        "asset_path": "/Game/Blueprints/BP_StoneGate",
        "graph": "BH_StoneGateActivation"
      },
      "write": {
        "strategy": "owned_graph_edit",
        "ops": [
          {
            "op": "ensure_entry",
            "entry_type": "custom_event",
            "name": "InitializeStoneGate"
          }
        ]
      },
      "constraints": {
        "allow_modify_user_nodes": false,
        "ownership_scope": "blueprinthelper_owned"
      }
    }
  ]
}
```

When UE Task Runtime lowers this GraphWrite IR to an existing command cluster, the `custom_event` `ensure_entry` lowers to `append_blueprint_graph` with generated nodes and links. That adapter payload is runtime-internal and is not the primary Agent-facing TaskPlan example. Replace/Patch/Merge TaskSpecs produce separate `graph_write` steps with one structural op each, for example `replace_body`, `set_pin_default`, or `insert_flow`.

As of the 2026-05-06 Rerun 4 update, Level 5 GraphWrite Replace/Patch/Merge smoke verified the full TaskSpec -> TypeScript compiler -> Bridge preview -> Bridge execute -> compile/read-back path for the supported owned-block cases listed in section 5.1. Treat `append_after + custom_event_call`, `branch_fork`, and non-BlueprintHelper-owned anchors as known gaps until their fixtures pass.

## 9.1 Partial Failure And Topology Blocking

TaskRuntime execution policy is:

1. Dry-run all executable steps first.
2. If any dry-run blocks, do not execute writes.
3. If dry-run passes, execute steps in stable TaskPlan order.
4. If a step fails during execution, mark that step failed and mark downstream dependent steps blocked by topology.
5. Continue only steps whose dependencies are completed and not blocked.
6. Do not promise global rollback by default.

TaskPlan dependency field:

```json
{
  "step_id": "step_020",
  "depends_on": ["step_010"]
}
```

TaskRunJournal step status values are:

```text
completed
failed
blocked
skipped
```

`blocked` means a prior dependency failed or was blocked. `skipped` means the runtime intentionally did not run the step for a non-error policy reason.

Blocked step journal fields:

```json
{
  "step_id": "step_020",
  "status": "blocked",
  "blocked_by_step_ids": ["step_010"],
  "blocked_reason": "dependency_failed",
  "error": null
}
```

Partial failure recovery summary:

```json
{
  "status": "partial_failure",
  "recovery": {
    "recommended_action": "inspect_task_result_then_submit_followup_taskspec",
    "safe_to_retry": false,
    "rollback_available": false,
    "notes": []
  }
}
```

The recovery object is user-readable guidance, not an automatic rollback command. Transaction-level undo/redo/replay is a separate future contract.

Completed task journals may include `generated_intent`, produced from the executed capability, a tool-short-name mapping, an action mapping, and the resolved target name. Example:

```json
{
  "generated_intent": "Use GraphWrite to write Blueprint logic for BP_Door.BH_Door"
}
```

`blueprinthelper_get_task_result` treats the UE Task Runtime journal as authoritative when `get_task_run_journal` returns `BlueprintHelper.TaskRunJournal.v1`. If the UE journal is unavailable, task-core may return its in-process execution summary. Completed UE journals that do not yet include `generated_intent` are normalized by the orchestration layer before they are returned to the Agent.

## 10. Validation Policy

Validation policy is deliberately named the same way across TaskSpec and TaskPlan:

| Layer | Fields |
|---|---|
| TaskSpec | `validation.should_compile`, `validation.should_save` |
| TaskPlan | `execution_policy.should_compile`, `execution_policy.should_save` |
| UE Runtime | `FBlueprintHelperValidationSummary.bShouldCompile`, `bShouldSave` |

Legacy `compile` and `save` keys inside `validation` are rejected. They are not aliases.

## 11. Capability Catalog

The capability field source for TaskPlan expansion is:

```text
Develop/v0.3.6/DoneImplementaion
```

The directory name is intentionally kept as-is because it is the current repository path.

Current UE TaskRuntime GraphWrite contract covers the operations listed below. The Agent-authored TaskSpec first slice may still gate which strategies the canonical TypeScript compiler emits; this catalog is not permission for ordinary Agents to call low-level tools directly or author TaskPlan by hand.

### 11.0 GraphWrite TaskPlan IR And Lowering Adapter

`BlueprintHelper.TaskPlan.v1` is a compiler-owned structured edit language / IR. Ordinary Agents submit `BlueprintHelper.TaskSpec.v1`; they must not author TaskPlan directly.

The primary GraphWrite TaskPlan IR uses stable structural fields:

| Field | Meaning |
|---|---|
| `step_id` | Stable step id for diagnostics and journal references |
| `capability` | `graph_write` |
| `target.asset_path` | Target Blueprint asset |
| `target.graph` | Target graph or graph family |
| `write.strategy` | Structural write strategy such as `owned_graph_edit` |
| `write.ops[]` | Ordered structural edit ops |
| `constraints.allow_modify_user_nodes` | Whether user-owned graph nodes may be modified |
| `constraints.ownership_scope` | Ownership boundary, for example `blueprinthelper_owned` |

Initial GraphWrite IR ops include:

```text
ensure_entry
replace_body
set_pin_default
set_node_comment
connect_pins
disconnect_link
replace_link
delete_owned_node
insert_flow
```

The compiler/runtime lowering contract currently includes:

```text
ensure_entry(custom_event) -> append_blueprint_graph
replace_body -> replace_blueprint_graph
set_pin_default / set_node_comment / connect_pins / disconnect_link / replace_link / delete_owned_node -> patch_blueprint_graph
insert_flow -> merge_blueprint_graph
```

GraphWrite replace/patch/merge structural ops are emitted by the compiler as `capability/write/ops` TaskPlan IR. The runtime-owned adapter operation names may appear only in child results, runtime data, or TaskRunJournal facts.

Current confirmed Bridge execution covers `ensure_entry(custom_event)` through `append_blueprint_graph` for fresh owned graphs, plus owned-block `replace_body`, `set_pin_default` / `set_node_comment`, P0-D owned link/delete patches, and `insert_flow` for the smoke-verified merge cases. Runtime/profile reporting may still lag behind these source capabilities; use preview result and TaskRunJournal facts as the execution authority.

The existing UE GraphWrite commands are Runtime lowering adapter targets, not the primary TaskPlan abstraction:

| Operation | Required target fields | Required args fields | Optional args fields | Bridge dry-run placement |
|---|---|---|---|---|
| `append_blueprint_graph` | `asset_path`, `graph` | `nodes`, `links` | `feature_name` | root `dry_run` |
| `replace_blueprint_graph` | `asset_path`, `graph`, `replace_scope` | `selector`, `replacement.nodes`, `replacement.links` | `options.strict` | `options.dry_run` |
| `patch_blueprint_graph` | `asset_path`, `graph`, `patch_scope` | `patch_type`, `patched_ref`, `patch` | `expected_old_state` | root `dry_run` |
| `merge_blueprint_graph` | `asset_path`, `graph`, `merge_scope`, `insert_strategy` | `anchor`, `inserted` | `sequence_order` | root `dry_run` |

These operations are TaskRuntime / Existing Capability Cluster steps. They do not expand the default Agent tool surface. Agents must continue to send only `TaskSpec` semantic fields and must not send `append_blueprint_graph` / `replace_blueprint_graph` / `patch_blueprint_graph` / `merge_blueprint_graph` runtime payloads in Agent requests.

### 11.1 Task Runtime Clusters

| Cluster | Runtime adapters / internal operations | v0.3.6 source documents | Agent exposure |
|---|---|---|---|
| Graph Write | `append_blueprint_graph`, `replace_blueprint_graph`, `patch_blueprint_graph`, `merge_blueprint_graph` | `BlueprintHelper_AppendBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_ReplaceBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_PatchBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_MergeBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md` | TaskSpec only |
| Blueprint Variables | `add_blueprint_member_variables` runtime adapter for `ensure_member_variable`; wider read/default/local/remove commands remain internal until dry-run contracts are fixed | `BlueprintHelper_BlueprintVariables_Defaults_LocalVariables_UE_CPP_Implementation_Plan_20260503.md` | TaskSpec only for `ensure_member_variable`, otherwise TaskPlan internal |
| Function/Event Signature | Agent-facing `edit_blueprint_signature` TaskSpec lowered to compiler-owned `blueprint_signature` ops for function, custom event, interface entry, dispatcher, override/native create-if-missing, and remove preflight policies | `BlueprintHelper_FunctionEventSignature_UE_CPP_Implementation_Plan_20260503.md` | TaskSpec only |
| UMG Widget Blueprint | `read_widget_blueprint`, `set_widget_tree`, `set_widget_properties` | `BlueprintHelper_UMG_WidgetBlueprint_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| DataAsset | `read_data_asset`, `set_data_asset_properties` | `BlueprintHelper_DataAsset_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| DataTable | `read_data_table`, `update_data_table_rows` | `BlueprintHelper_DataTable_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| Compile / Save | `compile_blueprint_asset`, `save_asset` | `BlueprintHelper_CompileBlueprintAsset_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_SaveAsset_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |

Custom Event entry declaration is a Function/Event Signature responsibility at the TaskPlan / UE service boundary. GraphWrite append semantics may still use `ensure_entry` with `entry_type = "custom_event"` as structured GraphWrite IR, but the entry declaration / signature creation must be lowered through `blueprint_signature` or an internal BlueprintSignatureService call before GraphWrite writes the body.

Do not introduce a new Agent-facing custom-event atomic tool. `blueprint_signature.ensure_custom_event` is allowed as TaskPlan-internal IR, and `append_blueprint_graph` may internally delegate Custom Event entry creation to the signature service. Ordinary Agents still express the intent through TaskSpec semantics such as `append_new_owned_graph` or `create_blueprint_feature`.

Function/Event Signature expansion must cover creation, modification, and removal of function signatures, Custom Event signatures, interface function versus interface event entries, event dispatcher signatures, and override/native event entries. Function or event body logic, graph nodes, links, calls, binds, and unbinds remain GraphWrite responsibility.

P2 first slice, 2026-05-06:

- UE now has an internal `FBlueprintHelperSignatureService` under `Services/BlueprintSignature`, with DTOs under `Structure/BlueprintSignature`.
- TaskRuntime `blueprint_signature` steps call this internal service instead of keeping signature execution logic inside the runtime service.
- `ensure_function` supports dry-run, reuse-if-exists no-op, real function graph creation, and first-slice `inputs` / `outputs` forwarding from TaskPlan.
- `interface_entry_kind` distinguishes interface functions from interface events. Interface functions lower to `ensure_function` under `function_signature`; interface events lower to `ensure_custom_event` under `custom_event_signature` and require an explicit graph target.
- `ensure_custom_event` supports dry-run, reuse-if-exists no-op, and first-slice entry declaration through the signature service. GraphWrite remains responsible for body nodes and links. `replace_owned_graph` with `replace.scope=custom_event_definition` now lowers into a `blueprint_signature.ensure_custom_event` dependency step followed by `graph_write.replace_body` with `custom_event_body`.
- `ensure_event_dispatcher` is a TaskPlan-internal `event_dispatcher_signature` op. It can create a new dispatcher declaration through the internal structure service. Existing dispatcher signature mutation is blocked by policy; `signature_mismatch_policy` must be `block`.
- `ensure_override_event` is a TaskPlan-internal `override_event_signature` op. `event_kind` must be `native_event` or `override_event`; default `execute_policy=blocked_preflight` still returns a blocked preflight result, while explicit `execute_policy=create_if_missing` can create a missing native/override event entry in the target graph. UE build has passed, but FullTestLog still shows failing override create-if-missing Automation, so this path is not smoke-verified yet.
- `remove_signature` is TaskPlan-internal. It accepts function, interface function, custom event, interface event, event dispatcher, override event, and native event kinds, but `execute_policy` must be `blocked_preflight` and `require_reference_context` must stay true until reference analysis and cleanup policy are implemented.
- No new Agent-facing atomic signature tool is introduced.

### 11.2 Support Clusters

| Cluster | Purpose | v0.3.6 source documents | Agent exposure |
|---|---|---|---|
| Runtime Profile | Safety/profile facts used before TaskSpec and execution | `BlueprintHelper_RuntimeProfile_UE_CPP_Implementation_Plan_20260503.md` | Agent read |
| Logic Read | LogicFlow / LogicMD / LogicJson by asset, graph, function, event, custom event, or block target | `BlueprintHelper_LogicRead_Grouped_UE_FieldMapping_20260502.md` | Agent read |
| Diagnostics / Discovery | Asset discovery, editor navigation, debug export, internal dependency analysis, project context, setup state | `BlueprintHelper_Diagnostics_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_AssetDiscovery_EditorNavigation_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_InternalDependencyAnalysis_UE_ImplementationPlan_20260503.md`, `BlueprintHelper_ProjectContext_SetupState_UE_CPP_Implementation_Plan_20260503.md` | Agent read or debug |
| Transaction Journal | Task result, transaction query, rollback audit | `BlueprintHelper_TransactionJournalQuery_UE_CPP_Implementation_Plan_20260503.md` | Task result or debug |
| Editor Lifecycle | Risk commands for launching or closing editor | `BlueprintHelper_EditorLifecycle_RiskCommand_UE_CPP_Implementation_Plan_20260503.md` | Debug or risk command |
| Common Envelope | Shared result shape and error normalization | `BlueprintHelper_ToolResultBase_CommonEnvelope_UE_CPP_Implementation_Plan_20260503.md` | Protocol internal |

LogicFlow is the most compact read-only execution/data flow view for simple entries and must not carry TaskSpec drafts or write anchors. LogicMD keeps the v0.3.6 grouped logic information shape. It is a read-only human-readable logic view and must not carry TaskSpec drafts. LogicJson remains a read-only structured logic view and must not carry `taskspec_hints`.

Decision, 2026-05-06: LogicJson read refs map to write anchors only through the grouped LogicJson / block-scoped contract described in section 5.1. Raw `node_ref` values such as `nodes[0]` are local read-view indexes and are not TaskSpec-compatible write anchors by themselves.

Resolved smoke finding, 2026-05-06: LogicJson `target_type=custom_event` lookup now finds Custom Events in custom graphs, not only EventGraph.

## 12. Extension Policy

Any new TaskSpec or TaskPlan capability must update all of these together:

1. `AgentFaceService/task-core/src/task/schema/task-contract.ts`
2. `AgentFaceService/task-core/src/tests/task/task-contract.test.ts`
3. `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`
4. `AgentFaceService/task-core/src/task/compiler/*`
5. UE Task Runtime validation or execution code if the TaskPlan shape changes.
6. This contract document.

Do not silently add fields that alter semantics. Optional metadata may pass through schema validation, but new executable semantics require contract and fixture updates.
