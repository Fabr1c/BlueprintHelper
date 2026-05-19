# ReadContext LogicFlow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `view.format=logic_flow` for `blueprinthelper_read_context` as the most compact read format, with `execflow` and `dataflow` modes derived from structured LogicJson.

**Architecture:** `logic_flow` stays in the ReadContext service boundary and is generated from `LogicJson.v1`; it must not parse `LogicMd.v1` markdown text. The bridge call for `logic_flow` reuses `read_blueprint_logic_json`, then task-core converts the structured logic payload into `LogicFlow.v1` before wrapping it in `ReadContextPack.v1`.

**Tech Stack:** TypeScript task-core, Zod schemas, Node test runner, existing BlueprintHelper CLI/task-core build scripts.

---

## Execution Result 2026-05-19

Status: completed.

Implemented:

1. `blueprinthelper_read_context` now accepts `view.format=logic_flow` for `blueprint_logic` reads.
2. `logic_flow` calls the structured `read_blueprint_logic_json` Bridge command and converts the payload into `LogicFlow.v1` in task-core.
3. `LogicFlow.v1` returns `mode`, `flow`, `stats`, and `warnings`, with `execflow` and `dataflow` modes covered by contract tests.
4. `graph_context` remains restricted to `logic_json`.
5. ReadContext capabilities now advertise formats in compression order: `logic_flow`, `logic_md`, `logic_json`.
6. Agent-facing docs, CLI reference, read templates, semantic index, and BP_ThirdPersonCharacter ReadSpecs now describe the three-tier read strategy.
7. `logic_flow` tests verify that raw LogicJson anchors and UE identity fields are not exposed in the compact payload.
8. Develop timing now records `read_context.resolve_bridge_request`, `read_context.strip_bridge_timing`, and `read_context.logic_flow_build_payload` for `logic_flow`, so JSON-to-LogicFlow conversion is visible separately from Bridge round-trip and generic post-process work.

Verification:

1. `AgentFaceService/task-core`: `node ..\scripts\run-tsc.mjs` passed.
2. `AgentFaceService/task-core`: `node scripts\run-node-tests.mjs` passed, 120/120.
3. `AgentFaceService/cli`: `node ..\scripts\run-tsc.mjs` passed.
4. `AgentFaceService/cli`: `node scripts\run-node-tests.mjs` passed, 39/39.
5. New JSON templates and ReadSpec parsed successfully.
6. `git diff --check` passed; only existing CRLF normalization warnings were reported.
7. MCP lifecycle retest passed after reopening Editor with `mcp__blueprint_helper__blueprint_open_editor`: all 11 `BP_ThirdPersonCharacter_20260519` ReadSpecs completed with `--develop`, including `11_blueprint_logic_flow.json`.

Latest MCP retest artifact:

`D:\UEProjects\Template\Plugins\BlueprintHelper\.tmp\read_timing_20260519_mcp_reopen_logicflow`

Representative `logic_flow` stages from `11_blueprint_logic_flow.json`:

| Stage | duration_ms |
| --- | ---: |
| `cli.parse_args` | 0.449 |
| `read_context.parse_input` | 0.113 |
| `read_context.resolve_format` | 0.025 |
| `read_context.resolve_bridge_request` | 0.037 |
| `read_context.build_bridge_payload` | 0.066 |
| `read_context.bridge.read_blueprint_logic_json` | 1901.729 |
| `read_context.extract_bridge_payload` | 0.096 |
| `read_context.strip_bridge_timing` | 0.027 |
| `read_context.logic_flow_build_payload` | 1.014 |
| `read_context.result_wrap` | 0.293 |
| nested `ue.read_blueprint_logic_json.route_execute` | 0.378 |

UE compile was not run because this implementation only touched TypeScript, Markdown, and JSON template files.

---

## Repository Rule For This Plan

Agents must not run `git add`, `git commit`, or `git push` in this repository. Each task includes a "manual commit checkpoint" so the final implementer can tell the user which files are ready to stage; it is not an instruction for the agent to execute git commands.

## Source Specification

Primary rules document:

- `BlueprintHelper/Develop/Plan/BlueprintHelper_ReadContext_LogicFlow_Rules_20260519_CN.md`

Key constraints from the rules:

1. `LogicFlow.v1` has `mode: "execflow" | "dataflow"`, `flow`, `stats`, and `warnings`.
2. Format order by compression is `logic_flow`, `logic_md`, `logic_json`.
3. `logic_flow` is not an anchor source for patch/merge/write workflows.
4. Macro / Collapsed Graph stays as a boundary node; internal expansion is delegated to function chain or a later dedicated read tool.
5. `logic_flow` generation must come from structured logic data, not markdown parsing.

## File Structure

- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-schemas.ts`
  - Accept `view.format=logic_flow`.
  - Keep `graph_context` restricted to `logic_json`.

- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-route-builder.ts`
  - Add `logic_flow` to `ReadContextLogicFormat`.
  - Keep default `blueprint_logic` as `logic_flow` only if the product decision is to make it default; this plan keeps the existing default as `logic_md` to avoid a behavior change until docs/templates are updated by agents.
  - Expose a helper that maps `logic_flow` and `logic_json` to `read_blueprint_logic_json`.

- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.ts`
  - Use the route helper for bridge command selection.
  - Pass `LogicFlow.v1` as the requested payload schema when the user requested `logic_flow`.

- Create: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts`
  - Convert `LogicJson.v1` bridge payload to `LogicFlow.v1`.
  - Own all `execflow` / `dataflow` formatting logic.

- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload.ts`
  - Call the LogicFlow builder before generic logic compaction when requested schema is `LogicFlow.v1`.
  - Preserve existing LogicMd and LogicJson cleanup behavior.

- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capabilities.ts`
  - Add `logic_flow` to the global format set before `logic_md` and `logic_json`.
  - Support `logic_flow` for `blueprint_logic`.
  - Keep `graph_context` as `logic_json` only.

- Modify: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`
  - Add schema/capability tests.
  - Add contract tests proving `logic_flow` calls `read_blueprint_logic_json` and returns `LogicFlow.v1`.
  - Add execflow, branch, and dataflow examples.

- Modify: `AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`
  - Document the three format tiers and the `LogicFlow.v1` payload shape.

- Modify: `AgentFaceService/docs/CLI_Tools_API_Reference.md`
  - Update ReadSpec examples and selection guidance.

- Modify: `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
  - Add `logic_flow` to the ReadSpec field template.
  - Warn that anchors still require `logic_json`.

- Modify: `AgentFaceService/agent-guide/Templates/read/README.md`
  - Update read-format guidance.

- Modify: `AgentFaceService/agent-guide/Templates/read/SEMANTIC_INDEX.md`
  - Add `logic_flow` function/event/custom event template entries.

- Create:
  - `AgentFaceService/agent-guide/Templates/read/read_context_function_logic_flow_template.json`
  - `AgentFaceService/agent-guide/Templates/read/read_context_event_logic_flow_template.json`
  - `AgentFaceService/agent-guide/Templates/read/read_context_custom_event_logic_flow_template.json`

- Modify: `BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/README.md`
  - Add a `logic_flow` run command.

- Create:
  - `BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/11_blueprint_logic_flow.json`

- Rename or renumber existing sample ReadSpecs only if needed to keep the folder order readable. If renumbering creates unrelated churn, keep existing names and add the new file as `11_blueprint_logic_flow.json`.

---

### Task 1: Accept And Advertise `logic_flow`

**Files:**
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-schemas.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-route-builder.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capabilities.ts`
- Test: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`

- [ ] **Step 1: Write failing schema and capabilities tests**

Add these assertions to `read_context rejects removed and unsupported view formats` and `read context capabilities is a compact local discovery tool`:

```ts
assert.doesNotThrow(() => ReadContextInputSchema.parse({
  schema: 'BlueprintHelper.ReadSpec.v1',
  read_type: 'blueprint_logic',
  target: {
    asset_path: '/Game/BP_Player',
    target_type: 'event',
    target_name: 'Input_Fire',
  },
  view: {
    format: 'logic_flow',
  },
}));

assert.throws(() => ReadContextInputSchema.parse({
  schema: 'BlueprintHelper.ReadSpec.v1',
  read_type: 'graph_context',
  target: {
    asset_path: '/Game/BP_Player',
    target_type: 'graph',
    target_name: 'EventGraph',
  },
  view: {
    format: 'logic_flow',
  },
}), /graph_context only supports logic_json/);
```

Update the capability assertions:

```ts
assert.deepEqual(result.data?.['formats'], ['logic_flow', 'logic_md', 'logic_json']);
assert.deepEqual(assetContext['unsupported_formats'], ['logic_flow', 'logic_md', 'logic_json']);
assert.deepEqual(graphContext['unsupported_formats'], ['logic_flow', 'logic_md']);
```

- [ ] **Step 2: Run the failing focused tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
node scripts\run-node-tests.mjs
```

Expected before implementation: failures mention invalid enum value for `logic_flow` and mismatched capability format arrays.

- [ ] **Step 3: Update ReadContext schema**

Change `read-context-schemas.ts`:

```ts
view: z.object({
  format: z.enum(['logic_flow', 'logic_md', 'logic_json']).optional(),
  max_items: z.number().int().positive().optional(),
  detail: z.enum(['brief', 'normal', 'full', 'debug']).optional(),
}).optional().default({}),
```

Keep the existing `graph_context` guard, but make sure the message remains accurate:

```ts
if (input.read_type === 'graph_context' && format && format !== 'logic_json') {
  ctx.addIssue({
    code: z.ZodIssueCode.custom,
    path: ['view', 'format'],
    message: 'graph_context only supports logic_json format.',
  });
}
```

- [ ] **Step 4: Update format type and bridge command helper**

Change `read-context-route-builder.ts`:

```ts
export type ReadContextLogicFormat = 'logic_flow' | 'logic_md' | 'logic_json';

export function resolveReadContextLogicFormat(input: ReadContextInput): ReadContextLogicFormat | undefined {
  if (input.read_type === 'graph_context') {
    return 'logic_json';
  }
  if (input.read_type === 'blueprint_logic') {
    return input.view?.format ?? 'logic_md';
  }
  return undefined;
}

export function resolveReadContextBridgeCommand(format: ReadContextLogicFormat): 'read_blueprint_logic_md' | 'read_blueprint_logic_json' {
  return format === 'logic_md' ? 'read_blueprint_logic_md' : 'read_blueprint_logic_json';
}

export function resolveReadContextPayloadSchema(format: ReadContextLogicFormat): 'LogicFlow.v1' | 'LogicMd.v1' | 'LogicJson.v1' {
  if (format === 'logic_flow') return 'LogicFlow.v1';
  if (format === 'logic_json') return 'LogicJson.v1';
  return 'LogicMd.v1';
}
```

- [ ] **Step 5: Update capability discovery**

Change `read-context-capabilities.ts`:

```ts
const FORMATS = [
  'logic_flow',
  'logic_md',
  'logic_json',
] as const;
```

Change the `blueprint_logic` row:

```ts
{
  read_type: 'blueprint_logic',
  asset_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
  formats: ['logic_flow', 'logic_md', 'logic_json'],
},
```

Keep `graph_context` unchanged:

```ts
{
  read_type: 'graph_context',
  asset_types: ['blueprint', 'graph', 'function', 'event', 'custom_event', 'block'],
  formats: ['logic_json'],
},
```

- [ ] **Step 6: Run focused tests again**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
node scripts\run-node-tests.mjs
```

Expected after this task: schema and capability assertions pass; later `logic_flow` contract tests are not present yet.

- [ ] **Step 7: Manual commit checkpoint**

Do not execute git commands. Report these files as ready for the user's manual staging after Task 1:

```text
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-schemas.ts
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-route-builder.ts
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capabilities.ts
AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts
```

---

### Task 2: Build `LogicFlow.v1` From Structured LogicJson

**Files:**
- Create: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload.ts`
- Test: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`

- [ ] **Step 1: Write failing `execflow` contract test**

Add this test near the existing LogicMD read_context test:

```ts
test('read_context logic_flow returns execflow from structured logic_json payload', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Player',
      target_type: 'event',
      target_name: 'Secondary Thumbstick',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string) {
        assert.equal(command, 'read_blueprint_logic_json');
        return {
          success: true,
          request_id: 'read_context_logic_flow_exec',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              graph: 'EventGraph',
              entry: { node_ref: 'nodes[0]', name: '事件Secondary Thumbstick' },
              nodes: [
                { node_ref: 'nodes[0]', name: '事件Secondary Thumbstick' },
                { node_ref: 'nodes[1]', name: 'DoLook' },
              ],
              links: [
                { type: 'exec', from_node: 'nodes[0]', from_pin: 'then', to_node: 'nodes[1]', to_pin: 'execute' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'Axis_X', to_node: 'nodes[1]', to_pin: 'Yaw' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'Axis_Y', to_node: 'nodes[1]', to_pin: 'Pitch' },
              ],
            },
            stats: {
              nodes: 2,
              exec_links: 1,
              data_links: 2,
              orphan_nodes: 0,
            },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  const payload = (result.data as Record<string, unknown>)['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'execflow');
  assert.equal(payload['flow'], '事件Secondary Thumbstick(Axis_X,Axis_Y) -> DoLook[Yaw=&.Axis_X, Pitch=&.Axis_Y]');
  assert.deepEqual(payload['stats'], {
    nodes: 2,
    exec_links: 1,
    data_links: 2,
    orphan_nodes: 0,
  });
  assert.deepEqual(payload['warnings'], []);
});
```

- [ ] **Step 2: Write failing `dataflow` contract test**

Add this test:

```ts
test('read_context logic_flow returns dataflow when structured logic has no exec links', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Math',
      target_type: 'function',
      target_name: 'ComputeOffset',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand(command: string) {
        assert.equal(command, 'read_blueprint_logic_json');
        return {
          success: true,
          request_id: 'read_context_logic_flow_data',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              graph: 'ComputeOffset',
              nodes: [
                { node_ref: 'nodes[0]', name: 'GetActorLocation' },
                { node_ref: 'nodes[1]', name: 'GetVelocity' },
                { node_ref: 'nodes[2]', name: '*' },
                { node_ref: 'nodes[3]', name: '+' },
                { node_ref: 'nodes[4]', name: 'ReturnValue' },
              ],
              links: [
                { type: 'data', from_node: 'nodes[1]', from_pin: 'ReturnValue', to_node: 'nodes[2]', to_pin: 'A' },
                { type: 'data', from_node: 'nodes[2]', from_pin: 'ReturnValue', to_node: 'nodes[3]', to_pin: 'B' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'ReturnValue', to_node: 'nodes[3]', to_pin: 'A' },
                { type: 'data', from_node: 'nodes[3]', from_pin: 'ReturnValue', to_node: 'nodes[4]', to_pin: 'ReturnValue' },
              ],
            },
            stats: {
              nodes: 5,
              exec_links: 0,
              data_links: 4,
              orphan_nodes: 0,
            },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  const payload = (result.data as Record<string, unknown>)['payload'] as Record<string, unknown>;
  assert.equal(payload['schema'], 'LogicFlow.v1');
  assert.equal(payload['mode'], 'dataflow');
  assert.match(String(payload['flow']), /^dataflow:/);
  assert.match(String(payload['flow']), /\$p0 = GetActorLocation/);
  assert.match(String(payload['flow']), /ReturnValue = /);
});
```

- [ ] **Step 3: Run tests to confirm they fail**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
node scripts\run-node-tests.mjs
```

Expected before implementation: tests fail because `logic_flow` still returns `LogicJson.v1` or has no builder.

- [ ] **Step 4: Create the LogicFlow builder file**

Create `read-context-logic-flow.ts` with this implementation skeleton. Keep helper functions in this new file so `read-context-payload.ts` remains a small orchestration file.

```ts
import type { ReadContextInput } from './read-context-schemas.js';
import { isRecord } from '../bridge-tool-result-utils.js';

type LogicFlowMode = 'execflow' | 'dataflow';

type LogicFlowNode = {
  ref: string;
  name: string;
};

type LogicFlowLink = {
  type: 'exec' | 'data' | 'unknown';
  fromNode: string;
  fromPin: string;
  toNode: string;
  toPin: string;
};

type LogicFlowGraph = {
  nodes: LogicFlowNode[];
  links: LogicFlowLink[];
  stats: Record<string, unknown>;
};

export function buildLogicFlowPayload(
  input: ReadContextInput,
  payload: Record<string, unknown>,
): Record<string, unknown> {
  const graph = normalizeLogicFlowGraph(payload);
  const execLinks = graph.links.filter((link) => link.type === 'exec');
  const dataLinks = graph.links.filter((link) => link.type === 'data');
  const mode: LogicFlowMode = execLinks.length > 0 ? 'execflow' : 'dataflow';
  const warnings = buildLogicFlowWarnings(graph);
  const flow = mode === 'execflow'
    ? buildExecFlow(input, graph, execLinks, dataLinks)
    : buildDataFlow(graph, dataLinks);

  return {
    schema: 'LogicFlow.v1',
    mode,
    flow,
    stats: graph.stats,
    warnings,
  };
}

function normalizeLogicFlowGraph(payload: Record<string, unknown>): LogicFlowGraph {
  const logic = isRecord(payload['logic']) ? payload['logic'] : payload;
  const nodes = Array.isArray(logic['nodes'])
    ? logic['nodes'].map((node, index) => normalizeNode(node, index)).filter(Boolean) as LogicFlowNode[]
    : [];
  const links = collectLinks(logic);
  const stats = isRecord(payload['stats']) ? payload['stats'] : buildStats(nodes, links);
  return { nodes, links, stats };
}

function normalizeNode(value: unknown, index: number): LogicFlowNode | undefined {
  if (!isRecord(value)) return undefined;
  const ref = readString(value, ['node_ref', 'ref', 'id']) ?? `nodes[${index}]`;
  const name = readString(value, ['name', 'display_name', 'title', 'ref']) ?? ref;
  return { ref, name };
}

function collectLinks(logic: Record<string, unknown>): LogicFlowLink[] {
  const links: LogicFlowLink[] = [];
  if (Array.isArray(logic['links'])) {
    for (const value of logic['links']) {
      const link = normalizeLink(value);
      if (link) links.push(link);
    }
  }
  if (Array.isArray(logic['nodes'])) {
    for (const node of logic['nodes']) {
      if (!isRecord(node) || !Array.isArray(node['links'])) continue;
      const fromNode = readString(node, ['node_ref', 'ref', 'id']) ?? '';
      for (const value of node['links']) {
        const link = normalizeLink(value, fromNode);
        if (link) links.push(link);
      }
    }
  }
  return dedupeLinks(links);
}

function normalizeLink(value: unknown, fallbackFromNode = ''): LogicFlowLink | undefined {
  if (!isRecord(value)) return undefined;
  const fromNode = readString(value, ['from_node', 'source_node', 'source', 'fromNode']) ?? fallbackFromNode;
  const toNode = readString(value, ['to_node', 'target_node', 'target', 'toNode']) ?? '';
  if (!fromNode || !toNode) return undefined;
  return {
    type: normalizeLinkType(readString(value, ['type', 'kind'])),
    fromNode,
    fromPin: readString(value, ['from_pin', 'source_pin', 'fromPin', 'pin_ref']) ?? '',
    toNode,
    toPin: readString(value, ['to_pin', 'target_pin', 'toPin']) ?? '',
  };
}

function normalizeLinkType(value: string | undefined): 'exec' | 'data' | 'unknown' {
  if (value?.toLowerCase() === 'exec') return 'exec';
  if (value?.toLowerCase() === 'data') return 'data';
  return 'unknown';
}

function buildExecFlow(
  input: ReadContextInput,
  graph: LogicFlowGraph,
  execLinks: LogicFlowLink[],
  dataLinks: LogicFlowLink[],
): string {
  const byRef = new Map(graph.nodes.map((node) => [node.ref, node]));
  const outgoingExec = groupLinks(execLinks, 'fromNode');
  const incomingExec = groupLinks(execLinks, 'toNode');
  const incomingData = groupLinks(dataLinks, 'toNode');
  const roots = findExecRoots(graph.nodes, execLinks, incomingExec);
  const lines = roots.map((root) => buildExecLine(root.ref, byRef, outgoingExec, incomingData, new Set()));
  const orphanSummary = buildOrphanSummary(graph.nodes, execLinks, dataLinks, roots.map((node) => node.ref));
  if (orphanSummary) lines.push(orphanSummary);
  return lines.filter(Boolean).join('\n');
}

function buildExecLine(
  nodeRef: string,
  byRef: Map<string, LogicFlowNode>,
  outgoingExec: Map<string, LogicFlowLink[]>,
  incomingData: Map<string, LogicFlowLink[]>,
  visited: Set<string>,
): string {
  const node = byRef.get(nodeRef);
  if (!node) return nodeRef;
  if (visited.has(nodeRef)) return `${formatNode(node, incomingData.get(nodeRef) ?? [])} -> <cycle:${nodeRef}>`;
  visited.add(nodeRef);

  const current = formatNode(node, incomingData.get(nodeRef) ?? []);
  const nextLinks = outgoingExec.get(nodeRef) ?? [];
  if (nextLinks.length === 0) return current;
  if (nextLinks.length === 1) {
    return `${current} -> ${buildExecLine(nextLinks[0].toNode, byRef, outgoingExec, incomingData, new Set(visited))}`;
  }
  const branches = nextLinks.map((link) => (
    `  ${link.fromPin || 'then'} -> ${buildExecLine(link.toNode, byRef, outgoingExec, incomingData, new Set(visited))}`
  ));
  return [current, ...branches].join('\n');
}

function buildDataFlow(graph: LogicFlowGraph, dataLinks: LogicFlowLink[]): string {
  const byRef = new Map(graph.nodes.map((node) => [node.ref, node]));
  const incomingData = groupLinks(dataLinks, 'toNode');
  const lines = ['dataflow:'];
  const names = new Map<string, string>();
  graph.nodes.forEach((node, index) => {
    const varName = node.name.toLowerCase() === 'returnvalue' ? 'ReturnValue' : `$p${index}`;
    names.set(node.ref, varName);
    const inputs = (incomingData.get(node.ref) ?? [])
      .map((link) => names.get(link.fromNode) ?? formatPinReference(byRef.get(link.fromNode), link.fromPin))
      .join(', ');
    const rendered = inputs ? `${node.name}[${inputs}]` : `${node.name}[]`;
    lines.push(`  ${varName} = ${rendered}`);
  });
  return lines.join('\n');
}

function formatNode(node: LogicFlowNode, incomingData: LogicFlowLink[]): string {
  const inputs = incomingData.map((link) => `${link.toPin || 'Value'}=&.${link.fromPin || 'Value'}`);
  if (inputs.length === 0) return node.name;
  return `${node.name}[${inputs.join(', ')}]`;
}

function findExecRoots(
  nodes: LogicFlowNode[],
  execLinks: LogicFlowLink[],
  incomingExec: Map<string, LogicFlowLink[]>,
): LogicFlowNode[] {
  const withOutgoing = new Set(execLinks.map((link) => link.fromNode));
  const roots = nodes.filter((node) => withOutgoing.has(node.ref) && !(incomingExec.get(node.ref)?.length));
  return roots.length ? roots : nodes.slice(0, 1);
}

function buildOrphanSummary(
  nodes: LogicFlowNode[],
  execLinks: LogicFlowLink[],
  dataLinks: LogicFlowLink[],
  roots: string[],
): string | undefined {
  const touched = new Set<string>(roots);
  for (const link of [...execLinks, ...dataLinks]) {
    touched.add(link.fromNode);
    touched.add(link.toNode);
  }
  const orphans = nodes.filter((node) => !touched.has(node.ref));
  if (!orphans.length) return undefined;
  const counts = new Map<string, number>();
  for (const node of orphans) counts.set(node.name, (counts.get(node.name) ?? 0) + 1);
  return `orphans: ${[...counts.entries()].map(([name, count]) => count > 1 ? `${name} x${count}` : name).join(', ')}`;
}

function groupLinks(links: LogicFlowLink[], key: 'fromNode' | 'toNode'): Map<string, LogicFlowLink[]> {
  const groups = new Map<string, LogicFlowLink[]>();
  for (const link of links) {
    const group = groups.get(link[key]) ?? [];
    group.push(link);
    groups.set(link[key], group);
  }
  return groups;
}

function buildLogicFlowWarnings(graph: LogicFlowGraph): string[] {
  const warnings: string[] = [];
  if (graph.links.some((link) => link.type === 'unknown')) warnings.push('unknown_link');
  if (graph.nodes.length === 0) warnings.push('empty_logic');
  return warnings;
}

function buildStats(nodes: LogicFlowNode[], links: LogicFlowLink[]): Record<string, unknown> {
  return {
    nodes: nodes.length,
    exec_links: links.filter((link) => link.type === 'exec').length,
    data_links: links.filter((link) => link.type === 'data').length,
  };
}

function readString(record: Record<string, unknown>, keys: string[]): string | undefined {
  for (const key of keys) {
    const value = record[key];
    if (typeof value === 'string' && value.length > 0) return value;
  }
  return undefined;
}

function formatPinReference(node: LogicFlowNode | undefined, pin: string): string {
  return node ? `${node.name}.${pin || 'Value'}` : pin || 'Value';
}

function dedupeLinks(links: LogicFlowLink[]): LogicFlowLink[] {
  const seen = new Set<string>();
  return links.filter((link) => {
    const key = `${link.type}|${link.fromNode}|${link.fromPin}|${link.toNode}|${link.toPin}`;
    if (seen.has(key)) return false;
    seen.add(key);
    return true;
  });
}
```

- [ ] **Step 5: Wire the builder into payload post-processing**

Modify `read-context-payload.ts`:

```ts
import { buildLogicFlowPayload } from './read-context-logic-flow.js';
```

Then change the start of `postProcessReadContextPayload`:

```ts
export function postProcessReadContextPayload(
  input: ReadContextInput,
  payloadSchema: string,
  payload: Record<string, unknown>,
): Record<string, unknown> {
  if (payloadSchema === 'LogicFlow.v1') {
    return buildLogicFlowPayload(input, payload);
  }

  const normalized: Record<string, unknown> = {
    schema: payload['schema'] ?? payloadSchema,
    ...payload,
  };
  delete normalized['format'];
```

- [ ] **Step 6: Wire bridge command and requested payload schema in handler**

Modify imports in `read-context-handler.ts`:

```ts
import {
  buildBlueprintLogicReadPayload,
  buildReadContextBridgeRequest,
  resolveReadContextBridgeCommand,
  resolveReadContextLogicFormat,
  resolveReadContextPayloadSchema,
} from './read-context-route-builder.js';
```

Replace the command/schema selection block:

```ts
const bridgeFormat = format ?? 'logic_md';
const command = resolveReadContextBridgeCommand(bridgeFormat);
```

Replace payload schema selection:

```ts
const readPayload = measureTaskTiming(timing, 'read_context.post_process_payload', () => postProcessReadContextPayload(
  input,
  resolveReadContextPayloadSchema(bridgeFormat),
  stripTimingPayload(payloadResult.payload),
));
```

- [ ] **Step 7: Run focused tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
node scripts\run-node-tests.mjs
```

Expected after this task: the new `logic_flow` tests pass. Existing LogicMd and LogicJson tests must still pass.

- [ ] **Step 8: Manual commit checkpoint**

Do not execute git commands. Report these files as ready for the user's manual staging after Task 2:

```text
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload.ts
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.ts
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-route-builder.ts
AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts
```

---

### Task 3: Preserve Anchor Safety And Format Boundaries

**Files:**
- Modify: `AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts`
- Modify: `AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts`

- [ ] **Step 1: Add a test that `logic_flow` strips raw LogicJson identity fields**

Add this test:

```ts
test('read_context logic_flow does not expose raw LogicJson anchors or UE identity fields', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'event',
      target_name: 'BeginPlay',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand() {
        return {
          success: true,
          request_id: 'read_context_logic_flow_no_anchor',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              asset_path: '/Game/BP_Door',
              graph: 'EventGraph',
              nodes: [
                {
                  node_ref: 'nodes[0]',
                  node_guid: 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
                  name: 'BeginPlay',
                },
                {
                  node_ref: 'nodes[1]',
                  node_path: '$.graphs.EventGraph.nodes[1]',
                  name: 'PrintString',
                },
              ],
              links: [
                {
                  link_ref: 'links[0]',
                  type: 'exec',
                  from_node: 'nodes[0]',
                  from_pin: 'then',
                  to_node: 'nodes[1]',
                  to_pin: 'execute',
                },
              ],
            },
            stats: { nodes: 2, exec_links: 1, data_links: 0 },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  assert.equal(result.ok, true);
  const serialized = JSON.stringify(result);
  assert.doesNotMatch(serialized, /node_guid/);
  assert.doesNotMatch(serialized, /node_path/);
  assert.doesNotMatch(serialized, /link_ref/);
  assert.doesNotMatch(serialized, /asset_path.*BP_Door.*logic/);
  assert.match(serialized, /BeginPlay -> PrintString/);
});
```

- [ ] **Step 2: Add a test for branch formatting**

Add this test:

```ts
test('read_context logic_flow keeps multi-exec output pin names', async () => {
  const tool = getBlueprintHelperToolRegistry().find((candidate) => candidate.name === 'blueprinthelper_read_context');
  assert.ok(tool);

  const result = await tool.execute({
    schema: 'BlueprintHelper.ReadSpec.v1',
    read_type: 'blueprint_logic',
    target: {
      asset_path: '/Game/BP_Door',
      target_type: 'event',
      target_name: 'Interact',
    },
    view: {
      format: 'logic_flow',
    },
  }, {
    cwd: process.cwd(),
    bridge: {
      async sendCommand() {
        return {
          success: true,
          request_id: 'read_context_logic_flow_branch',
          result: {
            schema: 'LogicJson.v1',
            logic: {
              nodes: [
                { node_ref: 'nodes[0]', name: 'Interact' },
                { node_ref: 'nodes[1]', name: 'Branch' },
                { node_ref: 'nodes[2]', name: 'OpenDoor' },
                { node_ref: 'nodes[3]', name: 'CloseDoor' },
              ],
              links: [
                { type: 'exec', from_node: 'nodes[0]', from_pin: 'then', to_node: 'nodes[1]', to_pin: 'execute' },
                { type: 'exec', from_node: 'nodes[1]', from_pin: 'True', to_node: 'nodes[2]', to_pin: 'execute' },
                { type: 'exec', from_node: 'nodes[1]', from_pin: 'False', to_node: 'nodes[3]', to_pin: 'execute' },
                { type: 'data', from_node: 'nodes[0]', from_pin: 'bDoorOpen', to_node: 'nodes[1]', to_pin: 'Condition' },
              ],
            },
            stats: { nodes: 4, exec_links: 3, data_links: 1 },
          },
        };
      },
    } as never,
    taskRunner: {} as TaskSpecRunner,
  });

  const payload = (result.data as Record<string, unknown>)['payload'] as Record<string, unknown>;
  assert.match(String(payload['flow']), /Interact -> Branch\[Condition=&\.bDoorOpen\]/);
  assert.match(String(payload['flow']), /  True -> OpenDoor/);
  assert.match(String(payload['flow']), /  False -> CloseDoor/);
});
```

- [ ] **Step 3: Run tests and adjust the builder only if the exact expectations fail**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
node scripts\run-node-tests.mjs
```

Expected after implementation: all tests pass. If branch output pin order differs, sort multi-exec links by `fromPin` with `Then0`, `Then1`, `True`, `False`, `Completed` preserving natural order before generic alphabetical order.

- [ ] **Step 4: Add deterministic branch ordering when needed**

If the branch test fails because order is unstable, add this helper to `read-context-logic-flow.ts` and use it before rendering multi-exec branches:

```ts
const EXEC_PIN_ORDER = ['then', 'execute', 'true', 'false', 'then0', 'then1', 'then2', 'loopbody', 'completed', 'finished'];

function sortExecLinks(links: LogicFlowLink[]): LogicFlowLink[] {
  return [...links].sort((a, b) => execPinRank(a.fromPin) - execPinRank(b.fromPin)
    || a.fromPin.localeCompare(b.fromPin)
    || a.toNode.localeCompare(b.toNode));
}

function execPinRank(pin: string): number {
  const normalized = pin.toLowerCase();
  const index = EXEC_PIN_ORDER.indexOf(normalized);
  return index >= 0 ? index : EXEC_PIN_ORDER.length;
}
```

Then change:

```ts
const nextLinks = outgoingExec.get(nodeRef) ?? [];
```

to:

```ts
const nextLinks = sortExecLinks(outgoingExec.get(nodeRef) ?? []);
```

- [ ] **Step 5: Manual commit checkpoint**

Do not execute git commands. Report these files as ready for the user's manual staging after Task 3:

```text
AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts
AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts
```

---

### Task 4: Update Agent-Facing Docs And Templates

**Files:**
- Modify: `AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`
- Modify: `AgentFaceService/docs/CLI_Tools_API_Reference.md`
- Modify: `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
- Modify: `AgentFaceService/agent-guide/Templates/read/README.md`
- Modify: `AgentFaceService/agent-guide/Templates/read/SEMANTIC_INDEX.md`
- Create: `AgentFaceService/agent-guide/Templates/read/read_context_function_logic_flow_template.json`
- Create: `AgentFaceService/agent-guide/Templates/read/read_context_event_logic_flow_template.json`
- Create: `AgentFaceService/agent-guide/Templates/read/read_context_custom_event_logic_flow_template.json`

- [ ] **Step 1: Update contract docs format table**

In `TaskSpec_TaskPlan_Contract_20260504.md`, replace the ReadSpec format table with:

```markdown
| Format | Purpose |
|---|---|
| `logic_flow` | Most compact view. Use for simple function/event/custom event reads when the Agent first needs execution/data flow understanding. Returns `LogicFlow.v1` with `mode=execflow` or `mode=dataflow`. |
| `logic_md` | Medium compact human-readable view. Use for larger or more branched function/event/custom event reads. |
| `logic_json` | Structured view for full reads, precise analysis, diff, patch, merge, anchors, and debug. |
| `raw_json` | Debug/expert full-fidelity view; not a default Agent workflow. |
```

Change the initial ReadSpec example to:

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

Add this paragraph near the `LogicMd.v1` paragraph:

```markdown
For `LogicFlow.v1`, `payload.mode` is `execflow` or `dataflow`. `payload.flow` is the compact text body, `payload.stats` is the only place for node/link/count statistics, and `payload.warnings` records compression risk such as unknown links or ambiguous macro boundaries. `LogicFlow.v1` is a read-to-understand view only; write anchors still require `logic_json`.
```

- [ ] **Step 2: Update CLI API reference guidance**

In `CLI_Tools_API_Reference.md`, replace guidance that says to use `logic_json` before whole-graph `logic_md` with:

```markdown
Use `logic_flow` for simple function/event/custom event reads. Use `logic_md` for larger or more branched entry reads. Use `logic_json` before whole-graph reads, when graph size is unknown, or whenever stable anchors are needed for patch/merge/debug. `logic_flow` and `logic_md` are not anchor sources.
```

- [ ] **Step 3: Update field template reference**

In `04_Tool_Surface_Field_Templates_20260512.md`, change the `view.format` row to:

```markdown
"format": "optional for logic reads. logic_flow, logic_md, or logic_json. Omit for non-logic read types.",
```

Add:

```markdown
`view.format=logic_flow` is recommended for simple `target_type=function`, `target_type=event`, or `target_type=custom_event` reads when the Agent needs fast execution/data flow understanding. It returns `LogicFlow.v1` and must not be used as a patch/merge anchor source.
```

- [ ] **Step 4: Add templates**

Create `read_context_function_logic_flow_template.json`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Path/To/BP_Asset",
    "target_type": "function",
    "target_name": "FunctionName"
  },
  "view": {
    "format": "logic_flow"
  }
}
```

Create `read_context_event_logic_flow_template.json`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Path/To/BP_Asset",
    "target_type": "event",
    "target_name": "EventName"
  },
  "view": {
    "format": "logic_flow"
  }
}
```

Create `read_context_custom_event_logic_flow_template.json`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Path/To/BP_Asset",
    "target_type": "custom_event",
    "target_name": "CustomEventName"
  },
  "view": {
    "format": "logic_flow"
  }
}
```

- [ ] **Step 5: Update read template README and semantic index**

In `Templates/read/README.md`, add:

```markdown
Use `logic_flow` first for simple function/event/custom event reads. Use `logic_md` when the entry is larger or has enough branches that a separated Entry / Execution / Data view is easier to scan. Use `logic_json` for full graph reads, unknown size, block anchors, patch/merge, or debug.
```

In `Templates/read/SEMANTIC_INDEX.md`, add rows:

```markdown
| Read one simple function body as LogicFlow | `read_context_function_logic_flow_template.json` | `blueprinthelper_read_context` |
| Read one simple event body as LogicFlow | `read_context_event_logic_flow_template.json` | `blueprinthelper_read_context` |
| Read one simple custom event body as LogicFlow | `read_context_custom_event_logic_flow_template.json` | `blueprinthelper_read_context` |
```

- [ ] **Step 6: Manual commit checkpoint**

Do not execute git commands. Report these files as ready for the user's manual staging after Task 4:

```text
AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md
AgentFaceService/docs/CLI_Tools_API_Reference.md
AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md
AgentFaceService/agent-guide/Templates/read/README.md
AgentFaceService/agent-guide/Templates/read/SEMANTIC_INDEX.md
AgentFaceService/agent-guide/Templates/read/read_context_function_logic_flow_template.json
AgentFaceService/agent-guide/Templates/read/read_context_event_logic_flow_template.json
AgentFaceService/agent-guide/Templates/read/read_context_custom_event_logic_flow_template.json
```

---

### Task 5: Update BP_ThirdPersonCharacter ReadSpecs

**Files:**
- Modify: `BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/README.md`
- Create: `BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/11_blueprint_logic_flow.json`

- [ ] **Step 1: Add a LogicFlow ReadSpec sample**

Create `11_blueprint_logic_flow.json`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter",
    "asset_type": "blueprint",
    "target_type": "event",
    "target_name": "Secondary Thumbstick"
  },
  "view": {
    "format": "logic_flow"
  }
}
```

- [ ] **Step 2: Add README command**

In `BP_ThirdPersonCharacter_20260519/README.md`, add this command after the capabilities command and before the `logic_json` command:

```powershell
bh blueprinthelper_read_context --file D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\v0.4.4\ReadSpecs\BP_ThirdPersonCharacter_20260519\11_blueprint_logic_flow.json --fields status,summary,artifacts.full_result
```

Change the final guidance sentence to:

```markdown
ReadSpec no longer supports `view.format=summary` or `view.format=schema`. Non-logic ReadSpecs omit `view.format`; logic reads use `logic_flow`, `logic_md`, or `logic_json`. Use `logic_flow` for simple function/event/custom event reads, `logic_md` for larger entry reads, and `logic_json` for whole graph reads or anchor/debug work.
```

- [ ] **Step 3: Manual commit checkpoint**

Do not execute git commands. Report these files as ready for the user's manual staging after Task 5:

```text
BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/README.md
BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/11_blueprint_logic_flow.json
```

---

### Task 6: Full Verification

**Files:**
- Verify all files changed in Tasks 1-5.

- [ ] **Step 1: Run task-core TypeScript build**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
node ..\scripts\run-tsc.mjs
```

Expected: command exits with code `0`.

- [ ] **Step 2: Run task-core node tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core
node scripts\run-node-tests.mjs
```

Expected: command exits with code `0`, including the new `logic_flow` tests.

- [ ] **Step 3: Run CLI TypeScript build**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli
node ..\scripts\run-tsc.mjs
```

Expected: command exits with code `0`. This catches exported task-core type changes used by CLI.

- [ ] **Step 4: Run CLI node tests**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli
node scripts\run-node-tests.mjs
```

Expected: command exits with code `0`.

- [ ] **Step 5: Run whitespace check**

Run:

```powershell
cd D:\UEProjects\Template\Plugins\BlueprintHelper
git diff --check
```

Expected: no whitespace errors. CRLF warnings are acceptable if they match existing repository behavior and no whitespace errors are reported.

- [ ] **Step 6: UE compile decision**

Do not run UE C++ compile for this plan if only `AgentFaceService` TypeScript, docs, and JSON templates changed. If an implementer moves any `logic_flow` generation into `BlueprintHelper/Source`, run the normal UE 5.6 compile path before closure.

- [ ] **Step 7: Final manual commit handoff**

Do not execute git commands. Tell the user to stage only files changed by this plan. Suggested manual command:

```powershell
git add AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-schemas.ts `
  AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-route-builder.ts `
  AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-handler.ts `
  AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-payload.ts `
  AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-logic-flow.ts `
  AgentFaceService/task-core/src/tool-surface/bridge/read-context/read-context-capabilities.ts `
  AgentFaceService/task-core/src/tests/tool-surface/tool-registry.contract.test.ts `
  AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md `
  AgentFaceService/docs/CLI_Tools_API_Reference.md `
  AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md `
  AgentFaceService/agent-guide/Templates/read/README.md `
  AgentFaceService/agent-guide/Templates/read/SEMANTIC_INDEX.md `
  AgentFaceService/agent-guide/Templates/read/read_context_function_logic_flow_template.json `
  AgentFaceService/agent-guide/Templates/read/read_context_event_logic_flow_template.json `
  AgentFaceService/agent-guide/Templates/read/read_context_custom_event_logic_flow_template.json `
  BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/README.md `
  BlueprintHelper/Develop/v0.4.4/ReadSpecs/BP_ThirdPersonCharacter_20260519/11_blueprint_logic_flow.json
```

Suggested commit message:

```text
新增内容：
1. 增加 ReadContext logic_flow 压缩读格式
2. 增加 LogicFlow 文档模板和 BP_ThirdPersonCharacter 示例 ReadSpec

变更需求：
1. 将 ReadContext 逻辑读取格式调整为 logic_flow / logic_md / logic_json 三档
2. 保持 logic_flow 只读理解用途，写入锚点继续使用 logic_json
```

## Self-Review

Spec coverage:

1. Three format tiers are covered by Tasks 1, 4, and 5.
2. `execflow` and `dataflow` are covered by Task 2 tests and builder.
3. Macro boundary behavior is covered by the docs in Task 4 and by the builder design that does not expand nested bodies.
4. Anchor safety is covered by Task 3.
5. ReadSpec samples and AgentGuide templates are covered by Tasks 4 and 5.

Placeholder scan:

1. No task uses placeholder markers or deferred-work wording.
2. Edge behavior is concrete: unknown links become `warnings`, graph_context remains `logic_json`, and macro ambiguity is not expanded in `logic_flow`.

Type consistency:

1. `ReadContextLogicFormat` includes `logic_flow`, `logic_md`, and `logic_json`.
2. `resolveReadContextBridgeCommand()` returns only existing bridge commands.
3. `resolveReadContextPayloadSchema()` returns `LogicFlow.v1`, `LogicMd.v1`, or `LogicJson.v1`.
4. `buildLogicFlowPayload()` returns the payload shape documented in `BlueprintHelper_ReadContext_LogicFlow_Rules_20260519_CN.md`.
