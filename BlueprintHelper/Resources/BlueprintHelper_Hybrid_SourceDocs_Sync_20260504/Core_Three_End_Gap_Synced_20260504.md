我检查了设计稿、`BlueprintHelper-v0.3.2.7z`、`BlueprintHelper_MCP_Server.7z` 的源码。下面结论基于**静态代码检查**，没有编译运行。

结论先给出：**当前 v0.3.2 已经能复用一大半“读 / 基础写 / object-first 返回 / Token 校验 / AgentImportGraph”基础，但满血原型需要新建一层统一字段协议和事务系统。**  
最核心的缺口不是“怎么多写几个工具”，而是：**UE 侧写操作没有统一 transaction / block / ownership / diff / review 字段，MCP 侧也没有统一把这些字段稳定返回给 Agent。**

---

# 1. 当前已有实现可以复用的部分

## 1.1 UE Bridge 基础协议：可复用，需扩字段

当前 UE 侧已有基础 Bridge envelope：

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

可以保留。它已经适合做所有工具的底层通道。

需要补的是：

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

`request_id` 继续由 MCP 侧生成；`trace_id` 应允许 MCP 传入，也允许 UE 侧补齐。

---

## 1.2 MCP response mode：可复用，但 RawJson 返回要修正

MCP 侧已经有：

```ts id="41hlq3"
summary_text
structured_json
resource_ref
legacy_text_json
```

以及：

```ts id="9dlqmi"
logic_md
logic_json
raw_json
raw_json_ref
resource_link
structuredContent
```

这个方向符合之前“LogicMD / LogicJson / RawJson / resource_ref 四主路径”的设计。v0.3.0 的版本定位本来就是降低 RawJson 依赖、引入 LogicJson / LogicMD、减少 Token 消耗。fileciteturn7file2

但当前 `blueprint_export_to_json` 在 `resource_ref` 模式下仍把 `json` 放进 `structuredContent`，这会抵消 resource_ref 的上下文节省。应改为：

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

不要默认内联：

```json id="xscvc8"
"json": { ...巨大 RawJson... }
```

只有 `legacy_text_json` 或显式 `inline_payload=true` 才返回完整 RawJson。

---

## 1.3 LogicMD / LogicJson：可复用，但 schema 和字段要统一

当前 UE 侧已有：

```text id="wdri9h"
export_logic
format = logic_md / logic_json
importable = false
markdown / logic
stats
```

MCP 侧已有：

```text id="gdyb5n"
blueprint_get_logic
blueprint_get_logic_json
```

这部分应复用。

但要改造：

| 当前问题 | 改造 |
|---|---|
| UE 侧 schema 返回 `BlueprintHelper.LogicMarkdown` / `BlueprintHelper.LogicGraph` | 改为 `BlueprintHelper.LogicMd.v1` / `BlueprintHelper.LogicJson.v1` |
| UE 侧 Logic 返回缺少稳定 `asset_path` / `graph` | UE 侧直接返回，不只让 MCP 侧 fallback |
| MCP 侧使用 `assetPath`，设计里多为 `asset_path` | 新字段统一用 `asset_path`，短期保留 `assetPath` 兼容 |
| LogicJson 只适合分析，不可导入 | 保留 `importable=false`，MCP 继续拒绝传给 import |

设计稿里已经明确 LogicMD / LogicJson 是读和分析路径，RawJson 才是保真 / 导入导出路径；MCP 返回协议最低要求也包括 `format`、`schema`、`importable=false` 等字段。fileciteturn3file1

---

## 1.4 AgentImportGraph：可复用为 Append 原型底座，但不应继续作为主工具名

当前 `blueprint_import_agent_graph` 已有：

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

UE 侧 `FBlueprintHelperAgentImportResult` 已经有：

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

这非常适合复用为 **AppendBlueprintGraph 的第一版底层实现**。

但设计稿已经明确废弃含糊的 Import 命名，Graph Write 主工具应改为 `AppendBlueprintGraph / ReplaceBlueprintGraph / PatchBlueprintGraph / MergeBlueprintGraph`。fileciteturn5file0

因此建议：

```text id="9r5nrk"
blueprint_import_agent_graph
保留 Legacy / Deprecated

新增：
blueprint_append_blueprint_graph
内部第一版可以调用现有 AgentImportService
```

---

## 1.5 现有服务层：大部分可复用

当前 UE 侧这些服务都可以保留并成为新字段协议的执行层：

| 现有服务 | 复用方式 |
|---|---|
| `FBlueprintHelperGraphResolver` | 所有 read/write/patch/merge 的目标定位底座 |
| `FBlueprintHelperExportService` | RawJson / Logic 生成底座 |
| `FBlueprintHelperLogicProcessor` | LogicMD / LogicJson 继续复用 |
| `FBlueprintHelperImportService` | RawJson replay / 兼容导入继续保留 |
| `FBlueprintHelperAgentImportService` | AppendGraph 原型底座 |
| `FBlueprintHelperValidationService` | 扩展为 dry_run / graph write validation |
| `FBlueprintHelperCompileService` | 编译闭环复用 |
| `FBlueprintHelperAssetBrowseService` | open/list/search/save 复用 |
| `FBlueprintHelperBlueprintStructureService` | 变量、函数图、宏图、dispatcher 复用 |
| `FBlueprintHelperWidgetService` | UMG 复用 |
| `FBlueprintHelperPropertyReflectionService` | DataAsset / UObject 属性复用 |
| `FBlueprintHelperDataTableService` | DataTable 复用 |
| `FBlueprintHelperScopedAssetMutation` | 写操作 rollback / commit 基础可继续扩展 |

---

# 2. 当前必须改造的部分

## 2.1 统一 UE 侧返回字段

建议所有 UE Bridge 命令最终都返回这种结构：

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

其中 `result` 按工具类型变化。

不要让每个工具随意返回：

```json id="13n6rw"
{ "saved": "..." }
{ "opened": "..." }
{ "generated_node_count": 3 }
{ "compile_success": true }
```

这些可以保留为内部结果，但 MCP 给 Agent 的字段要稳定。

---

## 2.2 统一 MCP 侧返回字段

MCP 侧给 Agent 的 `structuredContent` 建议统一为：

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

MCP 的 `content[0].text` 只放摘要：

```text id="u5l7ui"
AppendBlueprintGraph applied: /Game/Blueprints/BP_Door.EG_PhysicsDoor, blocks=3, nodes=12, links=14.
```

不要默认把完整 BridgeResponse JSON 塞进 text。当前 `toToolResult()` 会直接 `JSON.stringify(resp, null, 2)`，这对调试有用，但对 Agent 正常工作会浪费上下文。

---

## 2.3 字段命名需要收敛

当前混用：

```text id="xxqee2"
assetPath
target_blueprint
asset_path
target_graph
graph
rawUri
error_code
```

建议规则：

| 层 | 规范 |
|---|---|
| UE Bridge payload 输入 | 保持现有 `target_blueprint` / `target_graph`，减少破坏 |
| UE Bridge result 输出 | 新增统一 `snake_case` |
| MCP structuredContent | 统一 `snake_case` |
| 兼容字段 | 短期保留 `assetPath` / `rawUri`，但标记 legacy alias |
| 新工具 | 只用 `asset_path` / `raw_uri` |

最终建议：

```json id="b703sv"
{
  "asset_path": "/Game/...",
  "graph": "EventGraph",
  "raw_uri": "blueprint://asset/..."
}
```

---

## 2.4 现有 Token / risk_command 要接入 runtime profile

当前 UE 侧已经有：

```text id="38c9oc"
BLUEPRINTHELPER_BRIDGE_TOKEN
BLUEPRINTHELPER_ENABLE_HIGH_RISK_COMMANDS
write command auth_token 校验
exec_console_command / close_editor high risk 限制
```

这部分可以复用。

但 Agent 不应该靠调用失败才知道不能写。需要新增：

```text id="0pk057"
blueprinthelper_get_runtime_profile
```

UE 侧返回字段：

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

这属于新建工具，不建议从 `get_editor_context` 里硬塞。

---

## 2.5 BridgeClient 要从短连接改为持久连接

当前 MCP `BridgeClient` 注释写得很清楚：每次 `sendCommand` 都新建 TCP 连接、发送、读响应、关闭。它已经有 Length-Prefixed JSON framing 和 request timeout，可以复用；但要改造成持久连接池 / 单连接复用。

改造字段：

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

运行时状态：

```ts id="2w95qr"
BridgeConnectionState {
  connected: boolean
  last_connected_at?: string
  last_error?: string
  in_flight_request_count: number
}
```

这与之前通信侧计划一致：v0.5.0 已经把持久 BridgeClient、request_id / trace_id、自动重连、timeout、协议降级列入范围。fileciteturn3file3

---

# 3. 必须新建的 UE 侧字段系统

## 3.1 Transaction / Ownership / Review 字段

设计稿已经明确：`transaction_id` 是一次写工具调用，由 UE 插件侧生成；`block_id` 是可独立审阅、替换、Patch、Cleanup 的逻辑块，也由 UE 插件侧生成。fileciteturn5file1

UE 侧需要新增：

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

MCP 返回给 Agent：

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

注意：**Agent 最终报告默认不需要展开 journal_path、本地路径、完整 diff。**

---

## 3.2 节点 Metadata 字段

UE 节点 Metadata 只放最小 ownership 索引，不放完整 diff、tool input、LogicJson 快照。设计稿已经明确这一点。fileciteturn5file1

节点 Metadata：

```json id="qfkn1x"
{
  "BlueprintHelperOwned": true,
  "BlueprintHelperBlockId": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "BlueprintHelperTransactionId": "tx_20260502_0001",
  "BlueprintHelperTool": "AppendBlueprintGraph",
  "BlueprintHelperFeatureName": "PhysicsDoor"
}
```

NodeComment：

```text id="lricaj"
[BlueprintHelper]
block_id=EG_PhysicsDoor_TogglePhysicsDoor0
tx=tx_20260502_0001
tool=AppendBlueprintGraph
```

需要新建：

```text id="97h3j8"
FBlueprintHelperOwnershipService
FBlueprintHelperBlockIdGenerator
FBlueprintHelperTransactionIdGenerator
```

现有代码里没有发现真正写入 `BlueprintHelperOwned` / `BlueprintHelperBlockId` 的逻辑。

---

## 3.3 Journal 字段

UE 侧新建本地文件：

```text id="wm1jf8"
<Project>/Saved/BlueprintHelper/Transactions/Active/tx_xxx.json
<Project>/Saved/BlueprintHelper/Review/
```

Journal 最小结构：

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

# 4. Graph Write 返回字段设计

## 4.1 AppendBlueprintGraph

UE result：

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

MCP structuredContent：

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

## 4.2 dry_run 返回字段

所有高风险写入统一返回：

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

设计稿已明确 dry_run 是写入前安全预检，Review 是写入后审阅，两者不能互相替代。fileciteturn5file0

---

## 4.3 ReplaceBlueprintGraph

UE result 必须体现 replace scope：

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

Patch 需要精确定位字段：

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

Merge 必须返回执行流变化：

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

Merge 是高风险工具，必须 dry_run。设计稿也要求明确目标图表、接入点、目标 Pin、插入策略，不能猜。fileciteturn5file0

---

# 5. Read / Logic 返回字段设计

## 5.1 LogicMD

UE result：

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

MCP `content[0].text` 可以直接放 markdown；`structuredContent` 放 metadata。

---

## 5.2 LogicJson

UE result：

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

MCP structuredContent：

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

UE result 可以继续 object-first：

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

MCP 默认返回：

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

# 6. Cleanup / Rollback 返回字段设计

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

Cleanup 设计稿要求只接受明确 `block_id`，feature cleanup 才允许多字段匹配，并且正式执行必须生成 transaction、写 Journal、记录 rollback_data、进入 Review。fileciteturn4file14

---

# 7. 现有功能分类：复用 / 改造 / 新建

## 7.1 可直接复用

| 模块 | 说明 |
|---|---|
| Bridge request / response 基础结构 | 保留 `request_id / command / auth_token / payload / result` |
| Length-prefixed JSON framing | 已有，继续用 |
| Token 写权限校验 | 继续用，接入 runtime profile |
| high risk command gate | 继续用，接入 runtime profile |
| ExportService | RawJson 导出继续用 |
| LogicProcessor | LogicMD / LogicJson 继续用 |
| ImportService | RawJson 兼容导入继续用 |
| AgentImportService | 作为 AppendGraph 原型底层实现 |
| GraphResolver | 所有定位逻辑继续用 |
| ScopedAssetMutation | 写操作事务/rollback 基础继续用 |
| CompileService / SaveAsset | 写后验证闭环复用 |
| Widget / ObjectProperty / DataTable 服务 | 暂不重构，只接统一返回 envelope |

---

## 7.2 需要改造

| 模块 | 改造点 |
|---|---|
| `mcp-response.ts` | 新增统一 `McpToolResult.v1` envelope；修正 raw_json_ref 默认不内联 RawJson |
| `tools.ts` | 不再让普通工具默认 `JSON.stringify(resp)`；按 read/write/diagnostic/asset 四类包装 |
| `BridgeClient` | 从每次短连接改为持久连接 + reconnect + heartbeat |
| `export_logic` | UE 侧 schema 改名，补 `asset_path / graph / scope / detail / diagnostics` |
| `export_to_json` | UE 侧保留 payload；MCP 默认转 resource_ref，不内联 payload |
| `import_agent_graph` | 标记 legacy；新增 `append_blueprint_graph` 包装 |
| `create_blueprint` | 当前只支持 `BPTYPE_Normal`，需要支持 asset_type / factory_type |
| `compile_blueprint` | 统一 diagnostics 字段，增加 `should_save / recommended_next_tool` |
| `save_asset` | 返回 `modified=false` 或 `saved=true`，但不生成 transaction_id |
| `add_variable / add_graph / add_event_dispatcher` | 接入 transaction / diff / ownership / review |
| `delete_nodes` | 改为只允许明确目标，接入 Review / rollback / ownership 检查 |

---

## 7.3 必须新建

| 新模块 | 用途 |
|---|---|
| `RuntimeProfileService` | 返回当前版本、Bridge、config、write_permission、risk_command、tool_capabilities |
| `DiagnosticsService` | `/blueprinthelper-diagnostics` 和 `--runtime` 对应 UE/MCP 只读诊断 |
| `SettingsService` | 读取 settings.json，暴露 active_profile 摘要 |
| `TransactionJournalService` | 生成 / 写入 / 查询 transaction journal |
| `ReviewStoreService` | 保存 review_status，支持 AcceptAll / RejectAll |
| `OwnershipService` | 写 Metadata / NodeComment，扫描 owned nodes |
| `BlockIdService` | 按规则生成 block_id |
| `DiffSnapshotService` | before / after / deleted snapshot |
| `AppendBlueprintGraphService` | 新 Graph Write 主工具 |
| `ReplaceBlueprintGraphService` | 替换 block / function_body / event_body |
| `PatchBlueprintGraphService` | 精确修改 node/pin/value/link |
| `MergeBlueprintGraphService` | 接入已有执行流 |
| `CleanupService` | block / feature cleanup |
| `RollbackService` | cleanup / transaction rollback |
| `AssetFactoryService` | 创建 Blueprint Interface、Structure、InputAction 等 |
| `BlueprintComponentService` | 添加 / 修改 / 删除组件 |
| `BlueprintClassSettingsService` | parent、interfaces、class defaults、implemented interfaces |
| `EnhancedInputService` | 创建 IA、编辑 IMC、绑定按键 |
| `TargetLogicReadService` | 按 function/event/block/graph 精确读取 LogicMD / LogicJson |

---

# 8. Asset / Component / Input 侧字段设计

物理门测试已经暴露出 P0 缺口：不支持 Blueprint Interface、Structure、Input Action、IMC 编辑、蓝图实现接口等。这个测试报告指出 Agent 的方案设计能力超过了当前工具覆盖范围，接口、输入、C++ override 链路逐环断裂。fileciteturn3file6

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

# 9. 错误字段设计

UE Bridge error：

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

MCP `isError=true`，但仍保留 `structuredContent`：

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

错误码需要从现在的：

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

扩展为：

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

# 10. 推荐实现顺序

不要一次性重写全部工具。建议按这个顺序实现字段协议：

## Phase A：统一返回 envelope

先改 MCP 和 UE 基础字段，不动复杂业务：

```text id="pd7l3k"
1. UE Bridge response 增加 schema / trace_id / diagnostics / meta
2. MCP tools.ts 新增 normalizeToolResult()
3. read 工具统一 structuredContent
4. write 工具统一 status / modified / diagnostics
5. RawJson resource_ref 不再默认内联 json
```

## Phase B：runtime profile / diagnostics

```text id="56sxhq"
1. UE RuntimeProfileService
2. MCP blueprinthelper_get_runtime_profile
3. UE DiagnosticsService
4. MCP blueprinthelper_diagnostics_runtime
```

## Phase C：Transaction / Ownership

```text id="ws9jf4"
1. TransactionIdGenerator
2. BlockIdGenerator
3. OwnershipService
4. TransactionJournalService
5. 写入 Metadata + NodeComment
```

## Phase D：AppendBlueprintGraph

```text id="kpbgzi"
1. 新 MCP tool: blueprint_append_blueprint_graph
2. UE 内部先复用 AgentImportService
3. 返回 transaction_id / block_ids / ownership / validation
4. 旧 blueprint_import_agent_graph 标记 deprecated
```

## Phase E：Review / Cleanup / Rollback

```text id="bl8fdy"
1. ReviewStoreService
2. CleanupBlueprintHelperBlock
3. CleanupBlueprintHelperFeature dry_run
4. RollbackCleanupTransaction
5. ConvertBlueprintHelperBlockToUserOwned
```

## Phase F：补实战 P0 工具

```text id="76dka3"
1. AssetFactory: Interface / Structure / InputAction
2. BlueprintClassSettings: AddImplementedInterface
3. BlueprintComponentService
4. EnhancedInputService
5. Override / parent function visibility 诊断
```

---

# 11. 最终判断

当前实现状态可以概括为：

| 分类 | 当前状态 | 处理 |
|---|---|---|
| Bridge 基础协议 | 已有 | 复用，扩 `trace_id / schema / diagnostics` |
| Token / high risk gate | 已有 | 复用，接 runtime profile |
| LogicMD / LogicJson | 已有 | 复用，统一 schema 和字段 |
| RawJson object-first | 已有 | 复用，MCP 默认 resource_ref，不内联 payload |
| AgentImportGraph | 已有 | 复用为 Append 原型底座，主工具名改造 |
| 基础资产 / 蓝图 / UMG / DataTable 工具 | 已有 | 复用，套统一 result envelope |
| dry_run | AgentImportGraph 局部已有 | 扩成所有高风险写工具通用字段 |
| rollback | AgentImportGraph 局部已有 | 扩为 Transaction rollback |
| transaction_id / block_id | 未实现 | 新建 |
| ownership metadata / NodeComment | 未实现 | 新建 |
| Transaction Journal / Review Store | 未实现 | 新建 |
| Append / Replace / Patch / Merge | 未实现为正式工具 | 新建，Append 可复用 AgentImport |
| runtime profile | 未实现 | 新建 |
| diagnostics runtime | 未实现 | 新建 |
| Blueprint Interface / Structure / InputAction / IMC | 缺失 | 新建 |
| 持久 BridgeClient | 未实现，当前短连接 | 改造 |

建议现在先做 **字段协议收敛 + runtime profile + transaction/ownership + AppendBlueprintGraph**。这四件完成后，满血原型才有稳定骨架；否则继续堆工具会让返回字段、审阅、回滚和 Agent 判断逻辑越来越散。
---

# 2026-05-04 三端能力缺口同步：混合任务编排

## 新增核心缺口

当前三端缺口需要新增一类：

```text
Task Orchestration Gap / 任务编排缺口
```

具体包括：

```text
1. Agent 直接面对底层 MCP 工具过多，容易漏步骤或顺序错误。
2. 缺少 TaskContextPack，Agent 生成 TaskSpec 前上下文不足。
3. 缺少 TaskSpec schema / semantic / policy 错误层。
4. 缺少 MCP/Python Task Compiler。
5. 缺少 UE Task Runtime。
6. 缺少 task_run_id / TaskRunJournal。
7. Review 目前只能按 transaction 看，缺少 task_run 分组。
```

## 新增推荐补齐顺序

```text
1. TaskContextPack / read_task_context。
2. TaskSpec schema 与错误层。
3. preview_task。
4. TaskPlan v1。
5. UE Task Runtime v1。
6. task_run_id / TaskRunJournal。
7. Review UI 按 task_run_id 分组。
8. execute_task。
```

## 与原工具缺口关系

原来的 Asset / Component / Class Settings / Graph Write / Validation 缺口仍然有效。

混合任务编排不是替代这些能力，而是把它们组织成更稳定的任务执行链。
