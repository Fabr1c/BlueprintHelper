# BlueprintHelper CLI Tools API Reference / CLI 宸ュ叿 API 鍙傝€?
Document version / 鏂囨。鐗堟湰: `2026-05-17`

## 涓枃

鏈弬鑰冧笌褰撳墠瀹炵幇瀵归綈锛欳LI 鏄櫘閫?Agent 鎵ц TaskSpec銆丷eadSpec銆乨iagnostics銆乨ebug summary銆亀rite-session 鍜?result query 鐨勬敮鎸佸叆鍙ｃ€俙blueprint_open_editor` 鍜?`blueprint_close_editor` lifecycle 鍏ュ彛缁熶竴浣跨敤鍏ㄥ眬 MCP 宸ュ叿锛涗笉瑕侀€氳繃 CLI lifecycle alias 鍋氬吋瀹硅矾寰勩€?
### 鏋舵瀯

```text
Agent -> CLI command -> task-core -> Python Task Compiler / Read Router -> Bridge preview/execute/read -> UE Task Runtime -> Existing UE capability clusters
```

鏅€?Agent 鍙紪鍐?`BlueprintHelper.TaskSpec.v1`锛屼笉鐩存帴鎻愪氦 `TaskPlan`锛屼篃涓嶉粯璁や娇鐢?legacy low-level direct tools銆?
### 鍏ュ彛瑙勫垯

- 鏀寔鐨?TaskSpec/read/debug summary 鑳藉姏搴旈€氳繃 `bh <tool_name>` 鍙揪銆?- Agent-owned Editor lifecycle 搴斾娇鐢ㄥ叏灞€ MCP allowlist銆?- `blueprint_open_editor` / `blueprint_close_editor` 鏄叏灞€ MCP lifecycle 鍏ュ彛锛屼笉鏄?CLI direct tool锛涗笉瑕佽皟鐢?`bh open_editor` / `bh close_editor` 浣滀负鍏煎璺緞銆?- CLI 鍐欏叆蹇呴』缁忚繃 TaskSpec validation銆乸review 鍜?UE Task Runtime銆?- Raw Bridge write command 涓嶆槸鍏紑 Agent surface銆?- 搴熷純 MCP 鏅€氬伐鍏蜂笉鏄?fallback锛屼篃涓嶆槸鏅€?Agent 鍙€夊叆鍙ｃ€?
閫氱敤鍛戒护褰㈡€侊細

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

PowerShell-safe input rule: `--file` and `--stdin` are the stable JSON input paths. Inline `--json` is acceptable for `{}` and very small literals, but generated JSON should be piped:

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```

### 鏀寔鍛戒护闈?
榛樿 Agent-facing commands:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_context_capabilities
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

Global MCP lifecycle commands:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
```

鍒嗙粍 CLI commands:

```text
bh task preview --file <bare-task-spec.json>
bh task execute --file <bare-task-spec.json>
bh task result --id <task_run_id>
bh context read --file <read-task-context.json>
bh bridge ping
bh bridge call --command <read_only_bridge_command>
```

`blueprinthelper_apply_review_action` 鏄?plugin-development/internal 鍛戒护锛屼笉灞炰簬鏅€?Agent 宸ヤ綔娴併€?
### 杩斿洖褰㈡€?
CLI 閫氬父鍚?stdout 杈撳嚭绮剧畝鐨?`BlueprintHelper.CliResult.v1`銆俙artifacts.full_result` 浣跨敤绮剧畝鐨?`BlueprintHelper.CliFullResult.v1`锛屼笉杈撳嚭 nested ToolResult schema銆乼race id銆乺aw `bridge_result`銆侀噸澶?`target_assets` 鎴栭噸澶?nested `task_run_id`銆傞渶瑕?raw Bridge/trace 璇婃柇鏃朵娇鐢?`--expert`锛屽苟璇诲彇 `artifacts.debug_result`銆?
绀轰緥锛?
```json
{
  "status": "executed",
  "task_run_id": "task_cli_001",
  "summary": {
    "target_assets": ["/Game/BP_Player"],
    "planned_steps": 1,
    "modified": true
  },
  "artifacts": {
    "full_result": ".blueprinthelper/cli-runs/task_cli_001/result.json"
  }
}
```

浣跨敤 `--select` 鎴?`--fields` 淇濇寔 stdout 鏈€灏忋€傚ぇ鍨嬩笂涓嬫枃銆乺aw payload 鍜?debug artifact 搴旂暀鍦?artifact 鏂囦欢涓€?
UE Bridge 闀跨瓑寰呬細鍚?`stderr` 杈撳嚭 keep-alive 鎻愮ず锛沗stdout` 淇濈暀缁欐渶缁?JSON銆傜湅鍒?`waiting for UE Bridge response` 鏃跺簲缁х画绛夊緟锛岄櫎闈?CLI 閫€鍑恒€?
### TaskSpec 鍛戒护

TaskSpec-first 鍐欏叆寰幆锛?
```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_read_task_context or bh blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> bh blueprinthelper_preview_task
-> bh blueprinthelper_request_write_session when write_permission is disabled
-> bh blueprinthelper_execute_task
-> bh blueprinthelper_get_task_result when needed
```

Tool-name task commands 浣跨敤 `task_spec` wrapper锛?
```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Grouped commands 浣跨敤 bare TaskSpec锛?
```powershell
bh task preview --file .\task_spec.json
bh task execute --file .\task_spec.json
```

### CallFunction 瑙ｆ瀽

TaskSpec `body.statements[]` 鍙互浣跨敤 GraphStatement short form `kind: "call"` + `target`锛屼篃鍙互浣跨敤 legacy-compatible `kind: "call_function"` + `name`銆傝В鏋愬彂鐢熷湪 preview/execute 闃舵锛岀敱 UE 渚?GraphWrite CallFunctionResolver 瀹屾垚銆?
鍏佽鐨?call target锛?
- Native function name锛屼緥濡?`PrintString`銆?- Blueprint display name锛屼緥濡?`Print String`銆?- Owner-qualified native name锛屼緥濡?`/Script/Engine.KismetSystemLibrary:PrintString`銆?
| Error code | 鍚箟 | 淇 |
|---|---|---|
| `ambiguous_function_call` | 澶氫釜 graph-usable functions 鍖归厤 | 浣跨敤 owner-qualified native name |
| `function_call_not_found` | 娌℃湁鍖归厤鐨?graph-usable function | 璇诲彇 task context 鍚庨€夋嫨鍙敤鍑芥暟 |
| `explicit_member_call_not_supported` | 褰撳墠 graph write 璺緞涓嶆敮鎸佽 component/member target | 鍙湪 append-owned graph writes 涓娇鐢?`Object.Function`锛屾垨鏀圭敤鏀寔瀛楁 |

### ReadSpec 鍛戒护

Read command 浣跨敤鏍瑰璞?`BlueprintHelper.ReadSpec.v1`锛?
```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "target_type": "event",
    "target_name": "BeginPlay"
  },
  "view": {
    "format": "logic_flow"
  }
}
```

绠€鍗?function/event/custom event 璇诲彇浼樺厛浣跨敤 `logic_flow`銆傚叆鍙ｈ緝澶ф垨鍒嗘敮杈冨鏃朵娇鐢?`logic_md`銆傚浘琛ㄥぇ灏忔湭鐭ャ€佽鍙栧叏鍥俱€侀渶瑕佺ǔ瀹?owned-block anchors銆乸atch/merge/debug 鏃朵娇鐢?`logic_json`銆俙logic_flow` 鍜?`logic_md` 閮戒笉鏄啓鍏ラ敋鐐规潵婧愩€俁eadSpec 涓嶅啀鏀寔 `view.format=summary`锛涢潪 logic 璇诲彇鐩存帴鐪佺暐 `view.format`銆?
ReadContext capability discovery uses a separate local command and does not read UE assets:

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select status,artifacts.full_result
```

The full artifact payload schema is `ReadContextCapabilities.v1`. `asset_types`, `formats`, and `read_type_ids` are full sets; `read_types[]` lists only unsupported asset types and unsupported formats for each read type.

### Function Chain Context

`blueprinthelper_read_function_chain_context` 鐢ㄤ簬浠庡叆鍙?function/event/custom event 鏌ユ壘涓嬩竴灞傞」鐩嚜瀹氫箟 Blueprint 閫昏緫銆傚畠杩斿洖 compact index锛涢渶瑕佸嚱鏁颁綋鏃跺啀浣跨敤 `blueprinthelper_read_context`銆?
```json
{
  "asset_path": "/Game/BP_PlayerController",
  "target_type": "custom_event",
  "target_name": "Input_Fire",
  "graph_name": "EventGraph",
  "max_depth": 3,
  "include_data_dependencies": true,
  "expand_cross_asset": true
}
```

瑙勫垯锛?
- `target_type` 涓?`function`銆乣event` 鎴?`custom_event`銆?- 涓嶈鍙戦€?`target_guid`銆乣entry`銆乣target`銆乣query` 鎴?owner fields銆?- 缁撴灉 schema 涓?`FunctionChainContext.v1`銆?- Engine/trusted plugin/native utility calls 鍙繘鍏?summary counts銆?
### 涓昏 Schema

| Schema | Owner | Purpose / 鐢ㄩ€?|
|---|---|---|
| `BlueprintHelper.ReadSpec.v1` | Agent | Generic read request / 閫氱敤璇诲彇璇锋眰 |
| `ReadContextCapabilities.v1` | task-core local read service | Compact ReadContext capability matrix |
| `FunctionChainContext.v1` | Bridge read service | Compact custom logic call-chain index / 璋冪敤閾剧储寮?|
| `BlueprintHelper.TaskSpec.v1` | Agent | Semantic task specification / 璇箟浠诲姟鎻忚堪 |
| `BlueprintHelper.TaskPlan.v1` | task-core / Python compiler | Compiler-owned execution plan / 缂栬瘧鍣ㄧ敓鎴愭墽琛岃鍒?|
| `BlueprintHelper.TaskRunJournal.v1` | UE Task Runtime | Task execution journal / 鎵ц鏃ュ織 |
| `BlueprintHelper.CliResult.v1` | CLI stdout | Compact Agent summary / Agent 绮剧畝鎽樿 |
| `BlueprintHelper.CliFullResult.v1` | CLI full artifact | Compact full artifact / 绮剧畝 full artifact |
| `BlueprintHelper.CliDebugResult.v1` | CLI expert debug artifact | Raw Bridge and trace diagnostics / expert 璇婃柇 |

### 鍐欏叆鎺堟潈

- 浜や簰寮忓啓鍏ヤ娇鐢?`blueprinthelper_request_write_session`銆?- Unreal Editor 浼氭樉绀?accept/reject prompt銆?- 鎺堟潈褰掑睘浜庢鍦ㄨ繍琛岀殑 Editor/Bridge锛岄檺瀹?scope 鍜?lifetime銆?- `scope` 涓?`project` 鎴?`asset_list`锛沗asset_list` 闇€瑕?`asset_paths`銆?- 鏅€氫氦浜掑紡鍐欏叆涓紝Agent 涓嶅緱璇锋眰銆佹敞鍏ユ垨杞彂 `BLUEPRINTHELPER_BRIDGE_TOKEN`銆乣auth_token`銆乣auth_session`銆?
### Legacy / Internal

Legacy/internal/debug/expert commands 鍙兘浠嶅瓨鍦ㄤ簬鍐呴儴 transport 鍚庨潰锛屼絾涓嶅睘浜庢櫘閫?Agent surface锛屼篃涓嶄綔涓?Agent-facing fallback 鏆撮湶銆?
## English

This reference matches the current implementation: the CLI is the supported entry for ordinary Agent TaskSpec, ReadSpec, diagnostics, debug-summary, write-session, and result-query work. The `blueprint_open_editor` and `blueprint_close_editor` lifecycle entries are global MCP lifecycle tools; do not use CLI lifecycle aliases as compatibility paths.

### Architecture

```text
Agent -> CLI command -> task-core -> Python Task Compiler / Read Router -> Bridge preview/execute/read -> UE Task Runtime -> Existing UE capability clusters
```

Ordinary Agents author `BlueprintHelper.TaskSpec.v1` only. They do not submit `TaskPlan` directly and do not use legacy low-level direct tools as their default workflow.

### Entry Rule

- Every supported CLI-facing TaskSpec/read/debug summary capability should be reachable through `bh <tool_name>`.
- Agent-owned Editor lifecycle should use the global MCP allowlist.
- `blueprint_open_editor` / `blueprint_close_editor` are global MCP lifecycle entries, not CLI direct tools; do not call `bh open_editor` / `bh close_editor` as compatibility paths.
- CLI write commands must pass through TaskSpec validation, preview, and UE Task Runtime.
- Raw Bridge write commands are not part of the public Agent surface.
- Deprecated MCP ordinary tools are not fallback entries. Do not use, test, or restore them.

Canonical shell form:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

PowerShell-safe input rule: prefer `--file` or `--stdin`; inline `--json $json` can arrive at Node with quotes stripped.

### Supported Command Surface

Default Agent-facing commands:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_context_capabilities
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

Global MCP lifecycle commands:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
```

Grouped CLI commands:

```text
bh task preview --file <bare-task-spec.json>
bh task execute --file <bare-task-spec.json>
bh task result --id <task_run_id>
bh context read --file <read-task-context.json>
bh bridge ping
bh bridge call --command <read_only_bridge_command>
```

`blueprinthelper_apply_review_action` is a plugin-development/internal command and is not part of ordinary Agent workflows.

### Return Shape

The CLI usually prints compact `BlueprintHelper.CliResult.v1` summaries to stdout. `artifacts.full_result` uses the compact `BlueprintHelper.CliFullResult.v1` shape and omits nested ToolResult schemas, trace ids, raw `bridge_result`, duplicate `target_assets`, and duplicate nested `task_run_id`. Use `--expert` only when raw Bridge/trace diagnostics are needed, then read `artifacts.debug_result`.

Example:

```json
{
  "status": "executed",
  "task_run_id": "task_cli_001",
  "summary": {
    "target_assets": ["/Game/BP_Player"],
    "planned_steps": 1,
    "modified": true
  },
  "artifacts": {
    "full_result": ".blueprinthelper/cli-runs/task_cli_001/result.json"
  }
}
```

Use `--select` or `--fields` for the smallest possible stdout payload. Large context, raw payloads, and debug artifacts belong in artifact files.

Long UE Bridge waits emit keep-alive hints to `stderr`; `stdout` is reserved for final JSON. Continue waiting when you see `waiting for UE Bridge response` unless the CLI exits.

### TaskSpec Commands

TaskSpec-first write loop:

```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_read_task_context or bh blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> bh blueprinthelper_preview_task
-> bh blueprinthelper_request_write_session when write_permission is disabled
-> bh blueprinthelper_execute_task
-> bh blueprinthelper_get_task_result when needed
```

Tool-name task commands use a `task_spec` wrapper:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Grouped commands use a bare TaskSpec:

```powershell
bh task preview --file .\task_spec.json
bh task execute --file .\task_spec.json
```

### CallFunction Resolution

TaskSpec `body.statements[]` entries can use the GraphStatement short form `kind: "call"` with `target`, or the legacy-compatible `kind: "call_function"` with `name`. Resolution happens during preview/execute in the UE-side GraphWrite CallFunctionResolver.

Allowed call targets:

- Native function name, for example `PrintString`.
- Blueprint display name, for example `Print String`.
- Owner-qualified native name, for example `/Script/Engine.KismetSystemLibrary:PrintString`.

| Error code | Meaning | Repair |
|---|---|---|
| `ambiguous_function_call` | Multiple graph-usable functions match | Use owner-qualified native name |
| `function_call_not_found` | No graph-usable function matches | Read task context and choose an available function |
| `explicit_member_call_not_supported` | Current graph write path does not support this component/member target | Use `Object.Function` only with append-owned graph writes, or model the target through supported fields |

### ReadSpec Commands

Read commands use root object `BlueprintHelper.ReadSpec.v1`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "target_type": "event",
    "target_name": "BeginPlay"
  },
  "view": {
    "format": "logic_flow"
  }
}
```

`blueprint_logic` defaults to `logic_flow` when `view.format` is omitted. Use explicit `logic_md` for larger or more branched entry reads. Use explicit `logic_json` before whole-graph reads, when graph size is unknown, or whenever stable anchors are needed for patch/merge/debug. `logic_flow` and `logic_md` are not anchor sources. ReadSpec no longer supports `view.format=summary`; non-logic reads omit `view.format`.

ReadContext capability discovery uses a separate local command and does not read UE assets:

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select status,artifacts.full_result
```

The full artifact payload schema is `ReadContextCapabilities.v1`. `asset_types`, `formats`, and `read_type_ids` are full sets; `read_types[]` lists only unsupported asset types and unsupported formats for each read type.

### Function Chain Context

`blueprinthelper_read_function_chain_context` follows an entry function/event/custom event to find the next project-authored Blueprint logic entries. It returns a compact index; use `blueprinthelper_read_context` for full bodies.

```json
{
  "asset_path": "/Game/BP_PlayerController",
  "target_type": "custom_event",
  "target_name": "Input_Fire",
  "graph_name": "EventGraph",
  "max_depth": 3,
  "include_data_dependencies": true,
  "expand_cross_asset": true
}
```

Rules:

- `target_type` is `function`, `event`, or `custom_event`.
- Do not send `target_guid`, `entry`, `target`, `query`, or owner fields.
- Result schema is `FunctionChainContext.v1`.
- Engine/trusted plugin/native utility calls are summarized by count only.

### Primary Schemas

| Schema | Owner | Purpose / 鐢ㄩ€?|
|---|---|---|
| `BlueprintHelper.ReadSpec.v1` | Agent | Generic read request / 閫氱敤璇诲彇璇锋眰 |
| `ReadContextCapabilities.v1` | task-core local read service | Compact ReadContext capability matrix |
| `FunctionChainContext.v1` | Bridge read service | Compact custom logic call-chain index / 璋冪敤閾剧储寮?|
| `BlueprintHelper.TaskSpec.v1` | Agent | Semantic task specification / 璇箟浠诲姟鎻忚堪 |
| `BlueprintHelper.TaskPlan.v1` | task-core / Python compiler | Compiler-owned execution plan / 缂栬瘧鍣ㄧ敓鎴愭墽琛岃鍒?|
| `BlueprintHelper.TaskRunJournal.v1` | UE Task Runtime | Task execution journal / 鎵ц鏃ュ織 |
| `BlueprintHelper.CliResult.v1` | CLI stdout | Compact Agent summary / Agent 绮剧畝鎽樿 |
| `BlueprintHelper.CliFullResult.v1` | CLI full artifact | Compact full artifact |
| `BlueprintHelper.CliDebugResult.v1` | CLI expert debug artifact | Raw Bridge and trace diagnostics |

### Write Authorization

- Interactive writes use `blueprinthelper_request_write_session`.
- Unreal Editor shows an accept/reject prompt.
- Approval belongs to the running Editor/Bridge for the approved scope and lifetime.
- `scope` is `project` or `asset_list`; include `asset_paths` for `asset_list`.
- Agents must not request, inject, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session` for ordinary interactive writes.

### Legacy / Internal

Legacy/internal/debug/expert commands may still exist behind internal transport, but they are not part of the ordinary Agent surface and are not exposed as Agent-facing fallbacks.

