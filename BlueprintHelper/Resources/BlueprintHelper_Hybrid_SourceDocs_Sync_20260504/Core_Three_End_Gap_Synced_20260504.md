鎴戞鏌ヤ簡璁捐绋裤€乣BlueprintHelper-v0.3.2.7z`銆乣ClaudePlugin/mcp.7z` 鐨勬簮鐮併€備笅闈㈢粨璁哄熀浜?*闈欐€佷唬鐮佹鏌?*锛屾病鏈夌紪璇戣繍琛屻€?

缁撹鍏堢粰鍑猴細**褰撳墠 v0.3.2 宸茬粡鑳藉鐢ㄤ竴澶у崐鈥滆 / 鍩虹鍐?/ object-first 杩斿洖 / Token 鏍￠獙 / AgentImportGraph鈥濆熀纭€锛屼絾婊¤鍘熷瀷闇€瑕佹柊寤轰竴灞傜粺涓€瀛楁鍗忚鍜屼簨鍔＄郴缁熴€?*  
鏈€鏍稿績鐨勭己鍙ｄ笉鏄€滄€庝箞澶氬啓鍑犱釜宸ュ叿鈥濓紝鑰屾槸锛?*UE 渚у啓鎿嶄綔娌℃湁缁熶竴 transaction / block / ownership / diff / review 瀛楁锛孧CP 渚т篃娌℃湁缁熶竴鎶婅繖浜涘瓧娈电ǔ瀹氳繑鍥炵粰 Agent銆?*

---

# 1. 褰撳墠宸叉湁瀹炵幇鍙互澶嶇敤鐨勯儴鍒?

## 1.1 UE Bridge 鍩虹鍗忚锛氬彲澶嶇敤锛岄渶鎵╁瓧娈?

褰撳墠 UE 渚у凡鏈夊熀纭€ Bridge envelope锛?

```cpp id="w8vvbb"
FBlueprintHelperBridgeRequest
{
    RequestId;
    Command;
    AuthToken;
    Payload;
}

FBlueprintHelperBridgeResponse
{
    RequestId;
    bSuccess;
    ErrorCode;
    Message;
    Result;
}
```

鍙互淇濈暀銆傚畠宸茬粡閫傚悎鍋氭墍鏈夊伐鍏风殑搴曞眰閫氶亾銆?

闇€瑕佽ˉ鐨勬槸锛?

```json id="t7kmgy"
{
  "trace_id": "...",
  "protocol_version": "BlueprintHelper.Bridge.v1",
  "duration_ms": 0,
  "server_time": "...",
  "result_schema": "...",
  "diagnostics": []
}
```

`request_id` 缁х画鐢?CLI 渚х敓鎴愶紱`trace_id` 搴斿厑璁?CLI 浼犲叆锛屼篃鍏佽 UE 渚цˉ榻愩€?

---

## 1.2 CLI response mode锛氬彲澶嶇敤锛屼絾 RawJson 杩斿洖瑕佷慨姝?

CLI 渚у凡缁忔湁锛?

```ts id="41hlq3"
summary_text
structured_json
resource_ref
legacy_text_json
```

浠ュ強锛?

```ts id="9dlqmi"
logic_md
logic_json
raw_json
raw_json_ref
resource_link
structuredContent
```

杩欎釜鏂瑰悜绗﹀悎涔嬪墠鈥淟ogicMD / LogicJson / RawJson / resource_ref 鍥涗富璺緞鈥濈殑璁捐銆倂0.3.0 鐨勭増鏈畾浣嶆湰鏉ュ氨鏄檷浣?RawJson 渚濊禆銆佸紩鍏?LogicJson / LogicMD銆佸噺灏?Token 娑堣€椼€傤垁filecite顖倀urn7file2顖?

浣嗗綋鍓?`blueprint_export_to_json` 鍦?`resource_ref` 妯″紡涓嬩粛鎶?`json` 鏀捐繘 `structuredContent`锛岃繖浼氭姷娑?resource_ref 鐨勪笂涓嬫枃鑺傜渷銆傚簲鏀逛负锛?

```json id="qdc6ct"
{
  "format": "raw_json_ref",
  "schema": "BlueprintHelper.RawJsonRef.v1",
  "asset_path": "/Game/...",
  "graph": "EventGraph",
  "importable": true,
  "raw_uri": "blueprint://asset/...",
  "stats": { "nodes": 10, "links": 9 }
}
```

涓嶈榛樿鍐呰仈锛?

```json id="xscvc8"
"json": { ...宸ㄥぇ RawJson... }
```

鍙湁 `legacy_text_json` 鎴栨樉寮?`inline_payload=true` 鎵嶈繑鍥炲畬鏁?RawJson銆?

---

## 1.3 LogicMD / LogicJson锛氬彲澶嶇敤锛屼絾 schema 鍜屽瓧娈佃缁熶竴

褰撳墠 UE 渚у凡鏈夛細

```text id="wdri9h"
export_logic
format = logic_md / logic_json
importable = false
markdown / logic
stats
```

CLI 渚у凡鏈夛細

```text id="gdyb5n"
blueprint_get_logic
blueprint_get_logic_json
```

杩欓儴鍒嗗簲澶嶇敤銆?

浣嗚鏀归€狅細

| 褰撳墠闂 | 鏀归€?|
|---|---|
| UE 渚?schema 杩斿洖 `BlueprintHelper.LogicMarkdown` / `BlueprintHelper.LogicGraph` | 鏀逛负 `BlueprintHelper.LogicMd.v1` / `BlueprintHelper.LogicJson.v1` |
| UE 渚?Logic 杩斿洖缂哄皯绋冲畾 `asset_path` / `graph` | UE 渚х洿鎺ヨ繑鍥烇紝涓嶅彧璁?CLI 渚?fallback |
| CLI 渚т娇鐢?`assetPath`锛岃璁￠噷澶氫负 `asset_path` | 鏂板瓧娈电粺涓€鐢?`asset_path`锛岀煭鏈熶繚鐣?`assetPath` 鍏煎 |
| LogicJson 鍙€傚悎鍒嗘瀽锛屼笉鍙鍏?| 淇濈暀 `importable=false`锛孧CP 缁х画鎷掔粷浼犵粰 import |

璁捐绋块噷宸茬粡鏄庣‘ LogicMD / LogicJson 鏄鍜屽垎鏋愯矾寰勶紝RawJson 鎵嶆槸淇濈湡 / 瀵煎叆瀵煎嚭璺緞锛汳CP 杩斿洖鍗忚鏈€浣庤姹備篃鍖呮嫭 `format`銆乣schema`銆乣importable=false` 绛夊瓧娈点€傤垁filecite顖倀urn3file1顖?

---

## 1.4 AgentImportGraph锛氬彲澶嶇敤涓?Append 鍘熷瀷搴曞骇锛屼絾涓嶅簲缁х画浣滀负涓诲伐鍏峰悕

褰撳墠 `blueprint_import_agent_graph` 宸叉湁锛?

```json id="3c55mt"
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "...",
  "target_graph": "...",
  "mode": "append",
  "nodes": [],
  "links": [],
  "options": {
    "compile": true,
    "save": false,
    "strict": true,
    "dry_run": false,
    "create_missing_variables": true,
    "reconstruct_existing_nodes": false
  }
}
```

UE 渚?`FBlueprintHelperAgentImportResult` 宸茬粡鏈夛細

```text id="wo9008"
success
status
error_code
message
created_nodes
created_links
created_variables
warnings
diagnostics
rolled_back
rollback_count
compiled
saved
dry_run
```

杩欓潪甯搁€傚悎澶嶇敤涓?**AppendBlueprintGraph 鐨勭涓€鐗堝簳灞傚疄鐜?*銆?

浣嗚璁＄宸茬粡鏄庣‘搴熷純鍚硦鐨?Import 鍛藉悕锛孏raph Write 涓诲伐鍏峰簲鏀逛负 `AppendBlueprintGraph / ReplaceBlueprintGraph / PatchBlueprintGraph / MergeBlueprintGraph`銆傤垁filecite顖倀urn5file0顖?

鍥犳寤鸿锛?

```text id="9r5nrk"
blueprint_import_agent_graph
淇濈暀 Legacy / Deprecated

鏂板锛?
blueprint_append_blueprint_graph
鍐呴儴绗竴鐗堝彲浠ヨ皟鐢ㄧ幇鏈?AgentImportService
```

---

## 1.5 鐜版湁鏈嶅姟灞傦細澶ч儴鍒嗗彲澶嶇敤

褰撳墠 UE 渚ц繖浜涙湇鍔￠兘鍙互淇濈暀骞舵垚涓烘柊瀛楁鍗忚鐨勬墽琛屽眰锛?

| 鐜版湁鏈嶅姟 | 澶嶇敤鏂瑰紡 |
|---|---|
| `FBlueprintHelperGraphResolver` | 鎵€鏈?read/write/patch/merge 鐨勭洰鏍囧畾浣嶅簳搴?|
| `FBlueprintHelperExportService` | RawJson / Logic 鐢熸垚搴曞骇 |
| `FBlueprintHelperLogicProcessor` | LogicMD / LogicJson 缁х画澶嶇敤 |
| `FBlueprintHelperImportService` | RawJson replay / 鍏煎瀵煎叆缁х画淇濈暀 |
| `FBlueprintHelperAgentImportService` | AppendGraph 鍘熷瀷搴曞骇 |
| `FBlueprintHelperValidationService` | 鎵╁睍涓?dry_run / graph write validation |
| `FBlueprintHelperCompileService` | 缂栬瘧闂幆澶嶇敤 |
| `FBlueprintHelperAssetBrowseService` | open/list/search/save 澶嶇敤 |
| `FBlueprintHelperBlueprintStructureService` | 鍙橀噺銆佸嚱鏁板浘銆佸畯鍥俱€乨ispatcher 澶嶇敤 |
| `FBlueprintHelperWidgetService` | UMG 澶嶇敤 |
| `FBlueprintHelperPropertyReflectionService` | DataAsset / UObject 灞炴€у鐢?|
| `FBlueprintHelperDataTableService` | DataTable 澶嶇敤 |
| `FBlueprintHelperScopedAssetMutation` | 鍐欐搷浣?rollback / commit 鍩虹鍙户缁墿灞?|

---

# 2. 褰撳墠蹇呴』鏀归€犵殑閮ㄥ垎

## 2.1 缁熶竴 UE 渚ц繑鍥炲瓧娈?

寤鸿鎵€鏈?UE Bridge 鍛戒护鏈€缁堥兘杩斿洖杩欑缁撴瀯锛?

```json id="zvazzn"
{
  "schema": "BlueprintHelper.<CommandResult>.v1",
  "ok": true,
  "status": "applied",
  "modified": true,
  "command": "append_blueprint_graph",
  "request_id": "mcp_1_...",
  "trace_id": "trace_...",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "asset_class": "Blueprint",
    "blueprint_path": "/Game/Blueprints/BP_Door.BP_Door",
    "graph": "EG_PhysicsDoor",
    "target_type": "graph"
  },
  "result": {},
  "diagnostics": [],
  "recommended_next_actions": []
}
```

鍏朵腑 `result` 鎸夊伐鍏风被鍨嬪彉鍖栥€?

涓嶈璁╂瘡涓伐鍏烽殢鎰忚繑鍥烇細

```json id="13n6rw"
{ "saved": "..." }
{ "opened": "..." }
{ "generated_node_count": 3 }
{ "compile_success": true }
```

杩欎簺鍙互淇濈暀涓哄唴閮ㄧ粨鏋滐紝浣?CLI 缁?Agent 鐨勫瓧娈佃绋冲畾銆?

---

## 2.2 缁熶竴 CLI 渚ц繑鍥炲瓧娈?

CLI 渚х粰 Agent 鐨?`structuredContent` 寤鸿缁熶竴涓猴細

```json id="5r67gy"
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "tool": "blueprint_append_blueprint_graph",
  "command": "append_blueprint_graph",
  "request_id": "mcp_1_...",
  "trace_id": "trace_...",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "graph": "EG_PhysicsDoor"
  },
  "status": "applied",
  "modified": true,
  "data": {},
  "safety": {},
  "transaction": {},
  "review": {},
  "validation": {},
  "diagnostics": [],
  "next": {
    "should_compile": true,
    "should_save": true,
    "recommended_next_tool": "blueprint_compile_blueprint"
  }
}
```

CLI 鐨?`content[0].text` 鍙斁鎽樿锛?

```text id="u5l7ui"
AppendBlueprintGraph applied: /Game/Blueprints/BP_Door.EG_PhysicsDoor, blocks=3, nodes=12, links=14.
```

涓嶈榛樿鎶婂畬鏁?BridgeResponse JSON 濉炶繘 text銆傚綋鍓?`toToolResult()` 浼氱洿鎺?`JSON.stringify(resp, null, 2)`锛岃繖瀵硅皟璇曟湁鐢紝浣嗗 Agent 姝ｅ父宸ヤ綔浼氭氮璐逛笂涓嬫枃銆?

---

## 2.3 瀛楁鍛藉悕闇€瑕佹敹鏁?

褰撳墠娣风敤锛?

```text id="xxqee2"
assetPath
target_blueprint
asset_path
target_graph
graph
rawUri
error_code
```

寤鸿瑙勫垯锛?

| 灞?| 瑙勮寖 |
|---|---|
| UE Bridge payload 杈撳叆 | 淇濇寔鐜版湁 `target_blueprint` / `target_graph`锛屽噺灏戠牬鍧?|
| UE Bridge result 杈撳嚭 | 鏂板缁熶竴 `snake_case` |
| CLI structuredContent | 缁熶竴 `snake_case` |
| 鍏煎瀛楁 | 鐭湡淇濈暀 `assetPath` / `rawUri`锛屼絾鏍囪 legacy alias |
| 鏂板伐鍏?| 鍙敤 `asset_path` / `raw_uri` |

鏈€缁堝缓璁細

```json id="b703sv"
{
  "asset_path": "/Game/...",
  "graph": "EventGraph",
  "raw_uri": "blueprint://asset/..."
}
```

---

## 2.4 鐜版湁 Token / risk_command 瑕佹帴鍏?runtime profile

褰撳墠 UE 渚у凡缁忔湁锛?

```text id="38c9oc"
BLUEPRINTHELPER_BRIDGE_TOKEN
BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS
write command auth_token 鏍￠獙
exec_console_command / close_editor high risk 闄愬埗
```

杩欓儴鍒嗗彲浠ュ鐢ㄣ€?

浣?Agent 涓嶅簲璇ラ潬璋冪敤澶辫触鎵嶇煡閬撲笉鑳藉啓銆傞渶瑕佹柊澧烇細

```text id="0pk057"
blueprint_get_runtime_profile
```

UE 渚ц繑鍥炲瓧娈碉細

```json id="okfnmq"
{
  "schema": "BlueprintHelper.RuntimeProfile.v1",
  "version": "0.5.0-dev",
  "bridge_status": "connected",
  "config_status": "valid",
  "write_permission": {
    "enabled": true,
    "reason": "ok"
  },
  "risk_command": {
    "enabled": false,
    "reason": "risk_command_missing",
    "blocked_commands": ["close_editor", "exec_console_command"]
  },
  "active_profile": {
    "safety_profile": "conservative",
    "missing_capability_policy": "stop_and_report",
    "naming_preference_summary": "...",
    "blueprint_cpp_boundary_summary": "..."
  },
  "tool_capabilities": {
    "logic_read": true,
    "raw_json_export": true,
    "agent_import_graph": true,
    "append_blueprint_graph": true,
    "replace_blueprint_graph": false,
    "patch_blueprint_graph": false,
    "merge_blueprint_graph": false,
    "transaction_journal": false,
    "review": false,
    "cleanup": false
  },
  "diagnostics": []
}
```

杩欏睘浜庢柊寤哄伐鍏凤紝涓嶅缓璁粠 `get_editor_context` 閲岀‖濉炪€?

---

## 2.5 BridgeClient 瑕佷粠鐭繛鎺ユ敼涓烘寔涔呰繛鎺?

褰撳墠 CLI `BridgeClient` 娉ㄩ噴鍐欏緱寰堟竻妤氾細姣忔 `sendCommand` 閮芥柊寤?TCP 杩炴帴銆佸彂閫併€佽鍝嶅簲銆佸叧闂€傚畠宸茬粡鏈?Length-Prefixed JSON framing 鍜?request timeout锛屽彲浠ュ鐢紱浣嗚鏀归€犳垚鎸佷箙杩炴帴姹?/ 鍗曡繛鎺ュ鐢ㄣ€?

鏀归€犲瓧娈碉細

```ts id="qgvajj"
BridgeClientOptions {
  host
  port
  connect_timeout_ms
  request_timeout_ms
  reconnect
  max_retries
  heartbeat_interval_ms
}
```

杩愯鏃剁姸鎬侊細

```ts id="2w95qr"
BridgeConnectionState {
  connected: boolean
  last_connected_at?: string
  last_error?: string
  in_flight_request_count: number
}
```

杩欎笌涔嬪墠閫氫俊渚ц鍒掍竴鑷达細v0.5.0 宸茬粡鎶婃寔涔?BridgeClient銆乺equest_id / trace_id銆佽嚜鍔ㄩ噸杩炪€乼imeout銆佸崗璁檷绾у垪鍏ヨ寖鍥淬€傤垁filecite顖倀urn3file3顖?

---

# 3. 蹇呴』鏂板缓鐨?UE 渚у瓧娈电郴缁?

## 3.1 Transaction / Ownership / Review 瀛楁

璁捐绋垮凡缁忔槑纭細`transaction_id` 鏄竴娆″啓宸ュ叿璋冪敤锛岀敱 UE 鎻掍欢渚х敓鎴愶紱`block_id` 鏄彲鐙珛瀹￠槄銆佹浛鎹€丳atch銆丆leanup 鐨勯€昏緫鍧楋紝涔熺敱 UE 鎻掍欢渚х敓鎴愩€傤垁filecite顖倀urn5file1顖?

UE 渚ч渶瑕佹柊澧烇細

```cpp id="7x1pr9"
FBlueprintHelperTransactionInfo
{
    FString TransactionId;
    FString Tool;
    FString Status; // dry_run | applied | failed | rolled_back
    FString CreatedAt;
    TArray<FString> TargetAssets;
    TArray<FBlueprintHelperOperationInfo> Operations;
    TArray<FBlueprintHelperBlockInfo> Blocks;
    FBlueprintHelperDiffSummary DiffSummary;
    FBlueprintHelperValidationSummary Validation;
    FBlueprintHelperRollbackData RollbackData;
    TArray<FBlueprintHelperDiagnosticItem> Diagnostics;
}
```

CLI 杩斿洖缁?Agent锛?

```json id="x3aay0"
{
  "transaction": {
    "transaction_id": "tx_20260502_0001",
    "status": "applied",
    "journal_path_visible": false,
    "affected_assets": ["/Game/BP/BP_Door"],
    "block_ids": [
      "EG_PhysicsDoor_TogglePhysicsDoor0"
    ]
  },
  "review": {
    "review_status": "pending",
    "review_required": true,
    "review_grouping": ["asset", "graph", "block_id"]
  }
}
```

娉ㄦ剰锛?*Agent 鏈€缁堟姤鍛婇粯璁や笉闇€瑕佸睍寮€ journal_path銆佹湰鍦拌矾寰勩€佸畬鏁?diff銆?*

---

## 3.2 鑺傜偣 Metadata 瀛楁

UE 鑺傜偣 Metadata 鍙斁鏈€灏?ownership 绱㈠紩锛屼笉鏀惧畬鏁?diff銆乼ool input銆丩ogicJson 蹇収銆傝璁＄宸茬粡鏄庣‘杩欎竴鐐广€傤垁filecite顖倀urn5file1顖?

鑺傜偣 Metadata锛?

```json id="qfkn1x"
{
  "BlueprintHelperOwned": true,
  "BlueprintHelperBlockId": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "BlueprintHelperTransactionId": "tx_20260502_0001",
  "BlueprintHelperTool": "AppendBlueprintGraph",
  "BlueprintHelperFeatureName": "PhysicsDoor"
}
```

NodeComment锛?

```text id="lricaj"
[BlueprintHelper]
block_id=EG_PhysicsDoor_TogglePhysicsDoor0
tx=tx_20260502_0001
tool=AppendBlueprintGraph
```

闇€瑕佹柊寤猴細

```text id="97h3j8"
FBlueprintHelperOwnershipService
FBlueprintHelperBlockIdGenerator
FBlueprintHelperTransactionIdGenerator
```

鐜版湁浠ｇ爜閲屾病鏈夊彂鐜扮湡姝ｅ啓鍏?`BlueprintHelperOwned` / `BlueprintHelperBlockId` 鐨勯€昏緫銆?

---

## 3.3 Journal 瀛楁

UE 渚ф柊寤烘湰鍦版枃浠讹細

```text id="wm1jf8"
<Project>/Saved/BlueprintHelper/Transactions/Active/tx_xxx.json
<Project>/Saved/BlueprintHelper/Review/
```

Journal 鏈€灏忕粨鏋勶細

```json id="s4r2ny"
{
  "schema": "BlueprintHelper.TransactionJournal.v1",
  "transaction_id": "tx_20260502_0001",
  "tool": "AppendBlueprintGraph",
  "status": "applied",
  "created_at": "2026-05-02T00:00:00Z",
  "target_assets": ["/Game/BP/BP_Door"],
  "operations": [
    {
      "operation_id": "op_0001",
      "type": "create_graph",
      "asset_path": "/Game/BP/BP_Door",
      "graph": "EG_PhysicsDoor"
    }
  ],
  "blocks": [
    {
      "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
      "entry_type": "custom_event",
      "entry_name": "TogglePhysicsDoor",
      "graph": "EG_PhysicsDoor",
      "created_nodes": [],
      "created_links": [],
      "references_block_ids": [],
      "references_events": []
    }
  ],
  "diff_summary": {},
  "rollback_data": {},
  "diagnostics": [],
  "validation": {
    "should_compile": true,
    "should_save": true,
    "recommended_next_tool": "blueprint_compile_blueprint"
  }
}
```

---

# 4. Graph Write 杩斿洖瀛楁璁捐

## 4.1 AppendBlueprintGraph

UE result锛?

```json id="zpmehf"
{
  "schema": "BlueprintHelper.AppendBlueprintGraphResult.v1",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "graph": "EG_PhysicsDoor"
  },
  "safety": {
    "safety_profile": "conservative",
    "risk_level": "low",
    "dry_run_required": false,
    "dry_run_performed": true
  },
  "transaction": {
    "transaction_id": "tx_20260502_0001",
    "block_ids": [
      "EG_PhysicsDoor_TogglePhysicsDoor0"
    ]
  },
  "operations": {
    "created_graphs": ["EG_PhysicsDoor"],
    "created_events": ["TogglePhysicsDoor"],
    "created_nodes_count": 8,
    "created_links_count": 9,
    "called_existing_functions": [],
    "called_existing_events": []
  },
  "ownership": {
    "owned_nodes_count": 8,
    "metadata_written": true,
    "node_comments_written": true
  },
  "validation": {
    "should_compile": true,
    "should_save": true,
    "recommended_next_tool": "blueprint_compile_blueprint"
  },
  "diagnostics": []
}
```

CLI structuredContent锛?

```json id="ar0b79"
{
  "ok": true,
  "tool": "blueprint_append_blueprint_graph",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "graph": "EG_PhysicsDoor"
  },
  "transaction": {
    "transaction_id": "tx_20260502_0001",
    "block_ids": ["EG_PhysicsDoor_TogglePhysicsDoor0"]
  },
  "summary": {
    "created_graphs": 1,
    "created_events": 1,
    "created_nodes": 8,
    "created_links": 9
  },
  "next": {
    "should_compile": true,
    "should_save": true,
    "recommended_next_tool": "blueprint_compile_blueprint"
  }
}
```

---

## 4.2 dry_run 杩斿洖瀛楁

鎵€鏈夐珮椋庨櫓鍐欏叆缁熶竴杩斿洖锛?

```json id="s4cj20"
{
  "schema": "BlueprintHelper.DryRunResult.v1",
  "status": "dry_run",
  "modified": false,
  "can_execute": true,
  "risk_level": "high",
  "safety_profile": "conservative",
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "graph": "EventGraph"
  },
  "plan": {
    "will_create_nodes": [],
    "will_create_links": [],
    "will_delete_nodes": [],
    "will_delete_links": [],
    "will_modify_nodes": [],
    "will_preserve_nodes": [],
    "affected_user_nodes": [],
    "affected_blueprinthelper_blocks": []
  },
  "conflicts": [],
  "warnings": [],
  "errors": [],
  "recommended_next_actions": [
    {
      "action": "execute_write",
      "tool": "blueprint_append_blueprint_graph"
    }
  ]
}
```

璁捐绋垮凡鏄庣‘ dry_run 鏄啓鍏ュ墠瀹夊叏棰勬锛孯eview 鏄啓鍏ュ悗瀹￠槄锛屼袱鑰呬笉鑳戒簰鐩告浛浠ｃ€傤垁filecite顖倀urn5file0顖?

---

## 4.3 ReplaceBlueprintGraph

UE result 蹇呴』浣撶幇 replace scope锛?

```json id="akmbgh"
{
  "schema": "BlueprintHelper.ReplaceBlueprintGraphResult.v1",
  "status": "applied",
  "modified": true,
  "replace_scope": "function_body",
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "target_type": "function",
    "target_name": "OpenDoor",
    "graph": "OpenDoor"
  },
  "transaction": {
    "transaction_id": "tx_...",
    "preserved_block_ids": [],
    "new_block_ids": []
  },
  "replace_plan": {
    "deleted_nodes_count": 4,
    "created_nodes_count": 6,
    "modified_nodes_count": 0,
    "preserved_nodes_count": 1,
    "affected_user_nodes_count": 4,
    "external_dependents": [],
    "external_dependencies": []
  },
  "review": {
    "review_required": true,
    "review_reason": "user_nodes_affected"
  },
  "diagnostics": []
}
```

---

## 4.4 PatchBlueprintGraph

Patch 闇€瑕佺簿纭畾浣嶅瓧娈碉細

```json id="ri31u6"
{
  "schema": "BlueprintHelper.PatchBlueprintGraphResult.v1",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "graph": "EG_PhysicsDoor",
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
    "node_path": "$.graphs[0].nodes[3]",
    "pin_path": "$.graphs[0].nodes[3].inputs.OpenAngle"
  },
  "patch": {
    "patch_type": "set_pin_default",
    "expected_old_value_provided": true,
    "before_summary": "OpenAngle=90.0",
    "after_summary": "OpenAngle=110.0"
  },
  "transaction": {
    "transaction_id": "tx_..."
  },
  "diagnostics": []
}
```

---

## 4.5 MergeBlueprintGraph

Merge 蹇呴』杩斿洖鎵ц娴佸彉鍖栵細

```json id="uvgyft"
{
  "schema": "BlueprintHelper.MergeBlueprintGraphResult.v1",
  "status": "applied",
  "modified": true,
  "insert_strategy": "insert_between",
  "target": {
    "asset_path": "/Game/BP/BP_Player",
    "graph": "EventGraph",
    "target_node_path": "$.nodes[BeginPlay]",
    "target_pin": "Then"
  },
  "merge_plan": {
    "disconnected_links": [
      {
        "from": "BeginPlay.Then",
        "to": "OldNode.Execute"
      }
    ],
    "created_links": [
      {
        "from": "BeginPlay.Then",
        "to": "NewLogic.Execute"
      },
      {
        "from": "NewLogic.Then",
        "to": "OldNode.Execute"
      }
    ],
    "execution_order_changed": true,
    "affected_user_nodes": ["BeginPlay", "OldNode"]
  },
  "transaction": {
    "transaction_id": "tx_...",
    "block_ids": []
  },
  "review": {
    "review_required": true,
    "review_reason": "execution_flow_changed"
  }
}
```

Merge 鏄珮椋庨櫓宸ュ叿锛屽繀椤?dry_run銆傝璁＄涔熻姹傛槑纭洰鏍囧浘琛ㄣ€佹帴鍏ョ偣銆佺洰鏍?Pin銆佹彃鍏ョ瓥鐣ワ紝涓嶈兘鐚溿€傤垁filecite顖倀urn5file0顖?

---

# 5. Read / Logic 杩斿洖瀛楁璁捐

## 5.1 LogicMD

UE result锛?

```json id="ybvkqd"
{
  "schema": "BlueprintHelper.LogicMd.v1",
  "format": "logic_md",
  "importable": false,
  "asset_path": "/Game/BP/BP_Door",
  "graph": "EventGraph",
  "scope": "single_graph",
  "detail": "normal",
  "markdown": "...",
  "stats": {
    "nodes": 12,
    "exec_links": 9,
    "data_links": 4,
    "entry_points": 1,
    "orphans": 0
  },
  "diagnostics": []
}
```

CLI `content[0].text` 鍙互鐩存帴鏀?markdown锛沗structuredContent` 鏀?metadata銆?

---

## 5.2 LogicJson

UE result锛?

```json id="uapl5p"
{
  "schema": "BlueprintHelper.LogicJson.v1",
  "format": "logic_json",
  "importable": false,
  "asset_path": "/Game/BP/BP_Door",
  "graph": "EventGraph",
  "scope": "single_graph",
  "logic": {},
  "stats": {},
  "diagnostics": []
}
```

CLI structuredContent锛?

```json id="9elwiv"
{
  "ok": true,
  "format": "logic_json",
  "schema": "BlueprintHelper.LogicJson.v1",
  "importable": false,
  "asset_path": "/Game/BP/BP_Door",
  "graph": "EventGraph",
  "logic": {},
  "stats": {}
}
```

---

## 5.3 RawJson / RawJsonRef

UE result 鍙互缁х画 object-first锛?

```json id="mw566f"
{
  "schema": "BlueprintHelper.JsonToBlueprint.v2.2",
  "format": "raw_json",
  "importable": true,
  "asset_path": "/Game/BP/BP_Door",
  "graph": "EventGraph",
  "effective_scope": "graph",
  "payload": {},
  "stats": {},
  "diagnostics": []
}
```

CLI 榛樿杩斿洖锛?

```json id="p0pi9a"
{
  "format": "raw_json_ref",
  "schema": "BlueprintHelper.RawJsonRef.v1",
  "importable": true,
  "asset_path": "/Game/BP/BP_Door",
  "graph": "EventGraph",
  "raw_uri": "blueprint://asset/Game%2FBP%2FBP_Door?view=raw-json",
  "stats": {}
}
```

---

# 6. Cleanup / Rollback 杩斿洖瀛楁璁捐

## 6.1 CleanupBlueprintHelperBlock

```json id="hljii7"
{
  "schema": "BlueprintHelper.CleanupBlockResult.v1",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0"
  },
  "cleanup": {
    "deleted_nodes_count": 8,
    "deleted_links_count": 9,
    "external_dependents": [],
    "external_dependencies": [],
    "missing_policy": "error"
  },
  "transaction": {
    "transaction_id": "tx_...",
    "rollback_available": true
  },
  "review": {
    "review_status": "pending"
  },
  "diagnostics": []
}
```

## 6.2 Cleanup dry_run

```json id="xobp2n"
{
  "status": "dry_run",
  "modified": false,
  "can_execute": true,
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0"
  },
  "plan": {
    "would_delete_nodes": [],
    "would_delete_links": [],
    "will_keep": [],
    "blocked_by": [],
    "external_dependents": [],
    "external_dependencies": []
  },
  "requires_confirmation": false
}
```

Cleanup 璁捐绋胯姹傚彧鎺ュ彈鏄庣‘ `block_id`锛宖eature cleanup 鎵嶅厑璁稿瀛楁鍖归厤锛屽苟涓旀寮忔墽琛屽繀椤荤敓鎴?transaction銆佸啓 Journal銆佽褰?rollback_data銆佽繘鍏?Review銆傤垁filecite顖倀urn4file14顖?

---

# 7. 鐜版湁鍔熻兘鍒嗙被锛氬鐢?/ 鏀归€?/ 鏂板缓

## 7.1 鍙洿鎺ュ鐢?

| 妯″潡 | 璇存槑 |
|---|---|
| Bridge request / response 鍩虹缁撴瀯 | 淇濈暀 `request_id / command / auth_token / payload / result` |
| Length-prefixed JSON framing | 宸叉湁锛岀户缁敤 |
| Token 鍐欐潈闄愭牎楠?| 缁х画鐢紝鎺ュ叆 runtime profile |
| high risk command gate | 缁х画鐢紝鎺ュ叆 runtime profile |
| ExportService | RawJson 瀵煎嚭缁х画鐢?|
| LogicProcessor | LogicMD / LogicJson 缁х画鐢?|
| ImportService | RawJson 鍏煎瀵煎叆缁х画鐢?|
| AgentImportService | 浣滀负 AppendGraph 鍘熷瀷搴曞眰瀹炵幇 |
| GraphResolver | 鎵€鏈夊畾浣嶉€昏緫缁х画鐢?|
| ScopedAssetMutation | 鍐欐搷浣滀簨鍔?rollback 鍩虹缁х画鐢?|
| CompileService / SaveAsset | 鍐欏悗楠岃瘉闂幆澶嶇敤 |
| Widget / ObjectProperty / DataTable 鏈嶅姟 | 鏆備笉閲嶆瀯锛屽彧鎺ョ粺涓€杩斿洖 envelope |

---

## 7.2 闇€瑕佹敼閫?

| 妯″潡 | 鏀归€犵偣 |
|---|---|
| `mcp-response.ts` | 鏂板缁熶竴 `McpToolResult.v1` envelope锛涗慨姝?raw_json_ref 榛樿涓嶅唴鑱?RawJson |
| `tools.ts` | 涓嶅啀璁╂櫘閫氬伐鍏烽粯璁?`JSON.stringify(resp)`锛涙寜 read/write/diagnostic/asset 鍥涚被鍖呰 |
| `BridgeClient` | 浠庢瘡娆＄煭杩炴帴鏀逛负鎸佷箙杩炴帴 + reconnect + heartbeat |
| `export_logic` | UE 渚?schema 鏀瑰悕锛岃ˉ `asset_path / graph / scope / detail / diagnostics` |
| `export_to_json` | UE 渚т繚鐣?payload锛汳CP 榛樿杞?resource_ref锛屼笉鍐呰仈 payload |
| `import_agent_graph` | 鏍囪 legacy锛涙柊澧?`append_blueprint_graph` 鍖呰 |
| `create_blueprint` | 褰撳墠鍙敮鎸?`BPTYPE_Normal`锛岄渶瑕佹敮鎸?asset_type / factory_type |
| `compile_blueprint` | 缁熶竴 diagnostics 瀛楁锛屽鍔?`should_save / recommended_next_tool` |
| `save_asset` | 杩斿洖 `modified=false` 鎴?`saved=true`锛屼絾涓嶇敓鎴?transaction_id |
| `add_variable / add_graph / add_event_dispatcher` | 鎺ュ叆 transaction / diff / ownership / review |
| `delete_nodes` | 鏀逛负鍙厑璁告槑纭洰鏍囷紝鎺ュ叆 Review / rollback / ownership 妫€鏌?|

---

## 7.3 蹇呴』鏂板缓

| 鏂版ā鍧?| 鐢ㄩ€?|
|---|---|
| `RuntimeProfileService` | 杩斿洖褰撳墠鐗堟湰銆丅ridge銆乧onfig銆亀rite_permission銆乺isk_command銆乼ool_capabilities |
| `DiagnosticsService` | `/blueprinthelper-diagnostics` 鍜?`--runtime` 瀵瑰簲 UE/CLI 鍙璇婃柇 |
| `SettingsService` | 璇诲彇 settings.json锛屾毚闇?active_profile 鎽樿 |
| `TransactionJournalService` | 鐢熸垚 / 鍐欏叆 / 鏌ヨ transaction journal |
| `ReviewStoreService` | 淇濆瓨 review_status锛屾敮鎸?AcceptAll / RejectAll |
| `OwnershipService` | 鍐?Metadata / NodeComment锛屾壂鎻?owned nodes |
| `BlockIdService` | 鎸夎鍒欑敓鎴?block_id |
| `DiffSnapshotService` | before / after / deleted snapshot |
| `AppendBlueprintGraphService` | 鏂?Graph Write 涓诲伐鍏?|
| `ReplaceBlueprintGraphService` | 鏇挎崲 block / function_body / event_body |
| `PatchBlueprintGraphService` | 绮剧‘淇敼 node/pin/value/link |
| `MergeBlueprintGraphService` | 鎺ュ叆宸叉湁鎵ц娴?|
| `CleanupService` | block / feature cleanup |
| `RollbackService` | cleanup / transaction rollback |
| `AssetFactoryService` | 鍒涘缓 Blueprint Interface銆丼tructure銆両nputAction 绛?|
| `BlueprintComponentService` | 娣诲姞 / 淇敼 / 鍒犻櫎缁勪欢 |
| `BlueprintClassSettingsService` | parent銆乮nterfaces銆乧lass defaults銆乮mplemented interfaces |
| `EnhancedInputService` | 鍒涘缓 IA銆佺紪杈?IMC銆佺粦瀹氭寜閿?|
| `TargetLogicReadService` | 鎸?function/event/block/graph 绮剧‘璇诲彇 LogicMD / LogicJson |

---

# 8. Asset / Component / Input 渚у瓧娈佃璁?

鐗╃悊闂ㄦ祴璇曞凡缁忔毚闇插嚭 P0 缂哄彛锛氫笉鏀寔 Blueprint Interface銆丼tructure銆両nput Action銆両MC 缂栬緫銆佽摑鍥惧疄鐜版帴鍙ｇ瓑銆傝繖涓祴璇曟姤鍛婃寚鍑?Agent 鐨勬柟妗堣璁¤兘鍔涜秴杩囦簡褰撳墠宸ュ叿瑕嗙洊鑼冨洿锛屾帴鍙ｃ€佽緭鍏ャ€丆++ override 閾捐矾閫愮幆鏂銆傤垁filecite顖倀urn3file6顖?

## 8.1 AssetFactoryResult

```json id="v3pkej"
{
  "schema": "BlueprintHelper.AssetFactoryResult.v1",
  "status": "applied",
  "modified": true,
  "asset": {
    "asset_path": "/Game/Input/IA_Interact",
    "asset_name": "IA_Interact",
    "asset_class": "InputAction",
    "asset_type": "input_action"
  },
  "factory": {
    "factory_type": "input_action",
    "parent_class": null
  },
  "transaction": {
    "transaction_id": "tx_..."
  },
  "validation": {
    "should_save": true,
    "recommended_next_tool": "blueprint_save_asset"
  },
  "diagnostics": []
}
```

## 8.2 AddComponentResult

```json id="9mfn66"
{
  "schema": "BlueprintHelper.AddComponentResult.v1",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BP/BP_Door"
  },
  "component": {
    "component_name": "DoorMesh",
    "component_class": "StaticMeshComponent",
    "parent_component": "Root",
    "created": true
  },
  "transaction": {
    "transaction_id": "tx_..."
  },
  "diagnostics": []
}
```

## 8.3 AddInterfaceResult

```json id="h72lm1"
{
  "schema": "BlueprintHelper.AddInterfaceResult.v1",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BP/BP_Door"
  },
  "interface": {
    "interface_path": "/Game/BPI/BPI_Interactable",
    "interface_name": "BPI_Interactable",
    "already_implemented": false
  },
  "transaction": {
    "transaction_id": "tx_..."
  },
  "validation": {
    "should_compile": true
  },
  "diagnostics": []
}
```

## 8.4 EnhancedInputMappingResult

```json id="xs104v"
{
  "schema": "BlueprintHelper.EnhancedInputMappingResult.v1",
  "status": "applied",
  "modified": true,
  "mapping_context": {
    "asset_path": "/Game/Input/IMC_Default",
    "action": "/Game/Input/IA_Interact",
    "key": "F",
    "triggers": [],
    "modifiers": []
  },
  "transaction": {
    "transaction_id": "tx_..."
  },
  "diagnostics": []
}
```

---

# 9. 閿欒瀛楁璁捐

UE Bridge error锛?

```json id="aph5p2"
{
  "request_id": "mcp_...",
  "trace_id": "trace_...",
  "success": false,
  "error_code": "target_not_found",
  "message": "Target graph was not found.",
  "result": {
    "schema": "BlueprintHelper.Error.v1",
    "stage": "resolve_target",
    "field": "target_graph",
    "expected": "existing graph name",
    "actual": "EG_PhysicsDoor",
    "retryable": false,
    "stop_reason": "target_not_found",
    "modified": false,
    "rollback_result": "not_needed",
    "recommended_next_actions": [
      {
        "action": "read_graphs",
        "tool": "blueprint_list_graphs"
      }
    ],
    "diagnostics": []
  }
}
```

CLI `isError=true`锛屼絾浠嶄繚鐣?`structuredContent`锛?

```json id="2cdni2"
{
  "ok": false,
  "error": {
    "code": "target_not_found",
    "stage": "resolve_target",
    "message": "Target graph was not found.",
    "retryable": false,
    "stop_reason": "target_not_found"
  },
  "target": {},
  "diagnostics": [],
  "next": {
    "recommended_next_actions": []
  }
}
```

閿欒鐮侀渶瑕佷粠鐜板湪鐨勶細

```text id="faf6fb"
invalid_request
unknown_command
editor_not_ready
asset_not_found
graph_not_found
json_parse_failed
unauthorized
command_disabled
execution_failed
internal_error
```

鎵╁睍涓猴細

```text id="tu2urj"
target_not_found
target_ambiguous
target_not_owned
ownership_conflict
dry_run_conflict
pin_not_found
pin_type_mismatch
schema_rejected
external_dependents_blocked
write_permission_disabled
safety_profile_blocked
bridge_disconnected
config_unavailable
transaction_write_failed
rollback_failed
```

---

# 10. 鎺ㄨ崘瀹炵幇椤哄簭

涓嶈涓€娆℃€ч噸鍐欏叏閮ㄥ伐鍏枫€傚缓璁寜杩欎釜椤哄簭瀹炵幇瀛楁鍗忚锛?

## Phase A锛氱粺涓€杩斿洖 envelope

鍏堟敼 CLI 鍜?UE 鍩虹瀛楁锛屼笉鍔ㄥ鏉備笟鍔★細

```text id="pd7l3k"
1. UE Bridge response 澧炲姞 schema / trace_id / diagnostics / meta
2. CLI tool surface 鏂板 normalizeToolResult()
3. read 宸ュ叿缁熶竴 structuredContent
4. write 宸ュ叿缁熶竴 status / modified / diagnostics
5. RawJson resource_ref 涓嶅啀榛樿鍐呰仈 json
```

## Phase B锛歳untime profile / diagnostics

```text id="56sxhq"
1. UE RuntimeProfileService
2. CLI blueprint_get_runtime_profile
3. UE DiagnosticsService
4. CLI blueprinthelper_diagnostics_runtime
```

## Phase C锛歍ransaction / Ownership

```text id="ws9jf4"
1. TransactionIdGenerator
2. BlockIdGenerator
3. OwnershipService
4. TransactionJournalService
5. 鍐欏叆 Metadata + NodeComment
```

## Phase D锛欰ppendBlueprintGraph

```text id="kpbgzi"
1. 鏂?CLI tool: blueprint_append_blueprint_graph
2. UE 鍐呴儴鍏堝鐢?AgentImportService
3. 杩斿洖 transaction_id / block_ids / ownership / validation
4. 鏃?blueprint_import_agent_graph 鏍囪 deprecated
```

## Phase E锛歊eview / Cleanup / Rollback

```text id="bl8fdy"
1. ReviewStoreService
2. CleanupBlueprintHelperBlock
3. CleanupBlueprintHelperFeature dry_run
4. RollbackCleanupTransaction
5. ConvertBlueprintHelperBlockToUserOwned
```

## Phase F锛氳ˉ瀹炴垬 P0 宸ュ叿

```text id="76dka3"
1. AssetFactory: Interface / Structure / InputAction
2. BlueprintClassSettings: AddImplementedInterface
3. BlueprintComponentService
4. EnhancedInputService
5. Override / parent function visibility 璇婃柇
```

---

# 11. 鏈€缁堝垽鏂?

褰撳墠瀹炵幇鐘舵€佸彲浠ユ鎷负锛?

| 鍒嗙被 | 褰撳墠鐘舵€?| 澶勭悊 |
|---|---|---|
| Bridge 鍩虹鍗忚 | 宸叉湁 | 澶嶇敤锛屾墿 `trace_id / schema / diagnostics` |
| Token / high risk gate | 宸叉湁 | 澶嶇敤锛屾帴 runtime profile |
| LogicMD / LogicJson | 宸叉湁 | 澶嶇敤锛岀粺涓€ schema 鍜屽瓧娈?|
| RawJson object-first | 宸叉湁 | 澶嶇敤锛孧CP 榛樿 resource_ref锛屼笉鍐呰仈 payload |
| AgentImportGraph | 宸叉湁 | 澶嶇敤涓?Append 鍘熷瀷搴曞骇锛屼富宸ュ叿鍚嶆敼閫?|
| 鍩虹璧勪骇 / 钃濆浘 / UMG / DataTable 宸ュ叿 | 宸叉湁 | 澶嶇敤锛屽缁熶竴 result envelope |
| dry_run | AgentImportGraph 灞€閮ㄥ凡鏈?| 鎵╂垚鎵€鏈夐珮椋庨櫓鍐欏伐鍏烽€氱敤瀛楁 |
| rollback | AgentImportGraph 灞€閮ㄥ凡鏈?| 鎵╀负 Transaction rollback |
| transaction_id / block_id | 鏈疄鐜?| 鏂板缓 |
| ownership metadata / NodeComment | 鏈疄鐜?| 鏂板缓 |
| Transaction Journal / Review Store | 鏈疄鐜?| 鏂板缓 |
| Append / Replace / Patch / Merge | 鏈疄鐜颁负姝ｅ紡宸ュ叿 | 鏂板缓锛孉ppend 鍙鐢?AgentImport |
| runtime profile | 鏈疄鐜?| 鏂板缓 |
| diagnostics runtime | 鏈疄鐜?| 鏂板缓 |
| Blueprint Interface / Structure / InputAction / IMC | 缂哄け | 鏂板缓 |
| 鎸佷箙 BridgeClient | 鏈疄鐜帮紝褰撳墠鐭繛鎺?| 鏀归€?|

寤鸿鐜板湪鍏堝仛 **瀛楁鍗忚鏀舵暃 + runtime profile + transaction/ownership + AppendBlueprintGraph**銆傝繖鍥涗欢瀹屾垚鍚庯紝婊¤鍘熷瀷鎵嶆湁绋冲畾楠ㄦ灦锛涘惁鍒欑户缁爢宸ュ叿浼氳杩斿洖瀛楁銆佸闃呫€佸洖婊氬拰 Agent 鍒ゆ柇閫昏緫瓒婃潵瓒婃暎銆?
---

# 2026-05-04 涓夌鑳藉姏缂哄彛鍚屾锛氭贩鍚堜换鍔＄紪鎺?

## 鏂板鏍稿績缂哄彛

褰撳墠涓夌缂哄彛闇€瑕佹柊澧炰竴绫伙細

```text
Task Orchestration Gap / 浠诲姟缂栨帓缂哄彛
```

鍏蜂綋鍖呮嫭锛?

```text
1. Agent 鐩存帴闈㈠搴曞眰鍛戒护/宸ュ叿杩囧锛屽鏄撴紡姝ラ鎴栭『搴忛敊璇€?2. 缂哄皯 TaskContextPack锛孉gent 鐢熸垚 TaskSpec 鍓嶄笂涓嬫枃涓嶈冻銆?
3. 缂哄皯 TaskSpec schema / semantic / policy 閿欒灞傘€?
4. 缂哄皯 CLI/Python Task Compiler銆?
5. 缂哄皯 UE Task Runtime銆?
6. 缂哄皯 task_run_id / TaskRunJournal銆?
7. Review 鐩墠鍙兘鎸?transaction 鐪嬶紝缂哄皯 task_run 鍒嗙粍銆?
```

## 鏂板鎺ㄨ崘琛ラ綈椤哄簭

```text
1. TaskContextPack / read_task_context銆?
2. TaskSpec schema 涓庨敊璇眰銆?
3. preview_task銆?
4. TaskPlan v1銆?
5. UE Task Runtime v1銆?
6. task_run_id / TaskRunJournal銆?
7. Review UI 鎸?task_run_id 鍒嗙粍銆?
8. execute_task銆?
```

## 涓庡師宸ュ叿缂哄彛鍏崇郴

鍘熸潵鐨?Asset / Component / Class Settings / Graph Write / Validation 缂哄彛浠嶇劧鏈夋晥銆?

娣峰悎浠诲姟缂栨帓涓嶆槸鏇夸唬杩欎簺鑳藉姏锛岃€屾槸鎶婂畠浠粍缁囨垚鏇寸ǔ瀹氱殑浠诲姟鎵ц閾俱€?


