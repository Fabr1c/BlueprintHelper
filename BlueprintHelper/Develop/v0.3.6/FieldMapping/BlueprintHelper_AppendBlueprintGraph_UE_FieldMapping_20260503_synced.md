# BlueprintHelper 第 7 簇：AppendBlueprintGraph UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：AppendBlueprintGraph 字段确认稿  
本文边界：确认 Graph Write / AppendBlueprintGraph 的 Agent-facing 返回字段、UE 侧结构体映射、正式成功、dry_run、正式失败、transaction / block_ref / validation 规则。Agent 使用规则见独立文档。

---

## 1. 工具定位

`AppendBlueprintGraph` 是 Graph Write 工具簇中的“增量追加”工具。

它只负责：

```text
追加新的独立 BlueprintHelper-owned 逻辑块。
```

允许：

```text
1. 创建新的 EG_{FeatureName} 事件图表。
2. 向已有空图表追加独立逻辑块。
3. 创建唯一命名的 Custom Event。
4. 创建普通节点。
5. 创建新节点之间的连线。
6. 写入 BlueprintHelper-owned Metadata + NodeComment。
7. 内部写 Transaction Journal / Review / rollback_data。
```

禁止：

```text
1. 自动连接已有节点。
2. 自动接入已有执行流。
3. 覆盖旧节点。
4. 删除旧节点。
5. 清理旧 block。
6. 修改用户节点。
7. 创建 BeginPlay / Tick / InputAction 等全局事件节点。
8. 创建函数图。
9. 在 Conservative 下追加到函数图。
```

接入已有执行流属于 `MergeBlueprintGraph`。替换已有实现属于 `ReplaceBlueprintGraph`。精确修改节点 / Pin / 默认值 / 连线属于 `PatchBlueprintGraph`。

---

## 2. ToolResultBase 约束

`AppendBlueprintGraph` 使用精简后的 Agent-facing `ToolResultBase`。

默认不返回：

```text
review
safety
diagnostics
next
tool
command
request_id
operation_id
created_nodes 明细
created_links 明细
node_guid 列表
pin_guid 列表
rollback_data
journal_path
ownership
```

正式成功时允许返回 Graph Write 专属最小引用：

```text
data.write_ref.transaction_id
data.write_ref.journal_recorded
data.append_result.block_refs[]
```

注意：

```text
write_ref 不是通用顶层 transaction。
AppendBlueprintGraph 不恢复顶层 transaction 字段。
```

---

## 3. operation

固定使用：

```json
"operation": "append_blueprint_graph"
```

---

## 4. data.schema

正式成功：

```json
"schema": "AppendBlueprintGraph.v1"
```

dry_run：

```json
"schema": "AppendBlueprintGraphDryRun.v1"
```

失败：不返回 `data.append_result`。失败原因只返回 `error`。

---

# 5. 正式成功返回字段

## 5.1 成功返回示例

正式成功返回采用极简 Agent-facing 结构，只保留后续操作必需的 handle：`graph_id / graph_name / block_refs / write_ref / validation`。

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1001",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "data": {
    "schema": "AppendBlueprintGraph.v1",
    "append_result": {
      "graph": {
        "graph_id": "EG_PhysicsDoor",
        "graph_name": "EG_PhysicsDoor"
      },
      "block_refs": [
        "TogglePhysicsDoor0",
        "OpenPhysicsDoor0",
        "ClosePhysicsDoor0"
      ]
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1001",
      "journal_recorded": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

默认不返回：

```text
summary
created_nodes / created_links / created_blocks 计数
blocks[].entry_type
blocks[].entry_name
ownership
review
safety
diagnostics
```

---

## 5.2 target 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 目标蓝图资产路径。 |
| `TargetType` | `EBlueprintHelperTargetType` | `target.target_type` | `string enum` | 是 | 固定为 `graph`。 |
| `GraphName` | `FString` | `target.graph` | `string` | 是 | 目标图表名。 |

---

## 5.3 data.append_result.graph 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `GraphId` | `FString` | `data.append_result.graph.graph_id` | `string` | 是 | 图表 ID。第一版可与 graph_name 相同。用于反推完整 block_id。 |
| `GraphName` | `FString` | `data.append_result.graph.graph_name` | `string` | 是 | 图表名。 |

图表创建状态、是否已存在、是否为空等信息不默认返回给 Agent；如需审计或调试，进入 Transaction Journal / Review / verbose/debug。

图表规则：

```text
不存在：可创建。
已存在且为空：可写入。
已存在且非空：失败。
不自动改名。
```

---

## 5.4 data.append_result.block_refs 字段映射

`block_refs` 只返回局部压缩引用，不默认返回完整 `block_id`，也不返回 block 对象快照。

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `BlockRefs` | `TArray<FString>` | `data.append_result.block_refs[]` | `array<string>` | 是 | 局部 block 引用，例如 `TogglePhysicsDoor0`。 |

完整 `block_id` 反推规则：

```text
full_block_id = graph_id + "_" + block_ref
```

示例：

```text
graph_id = EG_PhysicsDoor
block_ref = TogglePhysicsDoor0
full_block_id = EG_PhysicsDoor_TogglePhysicsDoor0
```

不返回：

```text
blocks[].block_ref 对象包装
blocks[].entry_type
blocks[].entry_name
node_guid 列表
pin_guid 列表
created_nodes 明细
created_links 明细
完整 node_path / pin_path / link_path
```

`entry_name` 已包含在 block_ref 中；`entry_type` 如需判断，应后续读取 LogicMD / LogicJson。

这些明细进入 Transaction Journal / Review / rollback_data。后续精确编辑应重新读取 LogicJson。

---

## 5.5 成功结果不返回 summary

Append 成功结果不默认返回 summary 或创建计数。

不返回：

```text
data.append_result.summary
created_blocks
created_nodes
created_links
created_variables
called_existing_functions
called_existing_events
```

这些信息进入 Transaction Journal / Review / verbose/debug，而不是默认 Agent-facing 返回。

---

## 5.6 data.write_ref 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `data.write_ref.transaction_id` | `string` | 是 | 本次正式写工具调用的 transaction ID。 |
| `bJournalRecorded` | `bool` | `data.write_ref.journal_recorded` | `boolean` | 是 | Journal 是否已成功记录。 |

规则：

```text
1. write_ref 只在正式成功时返回。
2. 不返回 review_status。
3. 不返回 journal_path。
4. 不返回 rollback_data。
5. Journal 写入失败时，Graph Write 不能报告成功。
```

可选但第一版不默认返回：

```text
rollback_available
```

Rollback 专用工具再处理 rollback 细节。

---

## 5.7 ownership 字段规则

Agent-facing 成功结果不返回：

```text
data.ownership
ownership
ownership_summary
metadata_written
node_comments_written
```

规则：

```text
AppendBlueprintGraph 成功写入的 blocks 默认都是 BlueprintHelper-owned blocks。
Agent 不需要从返回体读取 ownership 字段。
```

如果 ownership metadata / NodeComment 写入失败，应整体失败并回滚：

```text
error.code = ownership_write_failed
error.stage = write_metadata
```

---

# 6. dry_run 返回字段

## 6.1 dry_run 成功

dry_run 成功默认极简返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1101",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "data": {
    "schema": "AppendBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

成功时不返回：

```text
plan
would_xxx
references
graph action
block_ref
block_id
write_ref
transaction_id
ownership
review
safety
diagnostics
next
warnings
```

---

## 6.2 dry_run blocked

dry_run blocked 仍表示 dry_run 工具本身执行成功，因此：

```text
ok=true
status=dry_run
modified=false
```

示例：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1102",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "data": {
    "schema": "AppendBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "target_graph_not_empty"
      ],
      "conflicts": [
        {
          "code": "target_graph_not_empty",
          "message": "AppendBlueprintGraph cannot append to a non-empty existing graph."
        }
      ],
      "errors": []
    }
  }
}
```

blocked 默认只返回：

```text
result
can_execute
blocked_by
conflicts
errors
```

不返回：

```text
plan
would_xxx
references
graph action
```

warnings 默认不返回。需要保留时进入 Journal / verbose / debug。

---

## 6.3 dry_run 工具自身失败

如果是 dry_run 工具自身失败，例如 schema 解析失败、Bridge 异常、payload 非法，返回：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1103",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "error": {
    "code": "invalid_graph_write_schema",
    "stage": "preflight",
    "message": "AppendBlueprintGraph dry_run could not parse the requested graph specification.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

---

## 6.4 dry_run 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Result` | `EBlueprintHelperDryRunResult` | `data.dry_run.result` | `string enum` | 是 | `passed` 或 `blocked`。 |
| `bCanExecute` | `bool` | `data.dry_run.can_execute` | `boolean` | 是 | 是否可正式执行。 |
| `BlockedBy` | `TArray<FString>` | `data.dry_run.blocked_by` | `array<string>` | blocked 时 | 阻断 code 摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperDryRunIssue>` | `data.dry_run.conflicts` | `array<object>` | blocked 时 | 冲突列表。 |
| `Errors` | `TArray<FBlueprintHelperDryRunIssue>` | `data.dry_run.errors` | `array<object>` | blocked 时 | 错误列表。 |

---

# 7. 正式失败返回字段

## 7.1 正式失败总规则

正式失败只返回：

```text
ok=false
status=failed
modified=false 或 modified=true
target
error
```

不返回：

```text
data.append_result
data.write_ref
data.ownership
review
safety
diagnostics
next
created_nodes
created_links
rollback_data
```

---

## 7.2 preflight 失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1201",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "error": {
    "code": "target_graph_not_empty",
    "stage": "preflight",
    "message": "AppendBlueprintGraph cannot append to a non-empty existing graph.",
    "retryable": false,
    "rollback_result": "not_needed",
    "conflicts": [
      {
        "code": "target_graph_not_empty",
        "target": "EG_PhysicsDoor"
      }
    ]
  }
}
```

规则：

```text
preflight 失败 modified=false。
rollback_result=not_needed。
```

---

## 7.3 写入中失败并成功回滚

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1202",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "error": {
    "code": "pin_type_mismatch",
    "stage": "connect_pins",
    "message": "Generated link could not be connected because pin types are incompatible.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "link",
      "ref": "links[7]"
    },
    "conflicts": [
      {
        "code": "pin_type_mismatch",
        "source": "SetSimulatePhysics.Then",
        "target": "Branch.Condition"
      }
    ]
  }
}
```

规则：

```text
写入中失败但成功回滚 modified=false。
rollback_result=rolled_back。
```

---

## 7.4 写入中失败但回滚 blocked / failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1203",
  "status": "failed",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "error": {
    "code": "rollback_blocked",
    "stage": "rollback",
    "message": "AppendBlueprintGraph failed and rollback could not be completed safely.",
    "retryable": false,
    "rollback_result": "blocked",
    "failed_item": {
      "type": "rollback",
      "ref": "transaction_internal"
    },
    "conflicts": [
      {
        "code": "asset_state_changed_during_write",
        "target": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      }
    ]
  }
}
```

规则：

```text
写入失败且 rollback blocked / failed 时，可允许 modified=true。
Agent 必须 stop_and_report。
不得继续 compile/save/patch。
```

---

## 7.5 ownership_write_failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1204",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "error": {
    "code": "ownership_write_failed",
    "stage": "write_metadata",
    "message": "BlueprintHelper ownership metadata could not be written.",
    "retryable": false,
    "rollback_result": "rolled_back"
  }
}
```

规则：

```text
Append 成功 = 新 block 默认 owned-block。
如果 owned metadata / comment 写入失败 = 整体失败并回滚。
不返回 ownership 字段。
```

---

## 7.6 journal_write_failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "trace_id": "trace_20260503_1205",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "error": {
    "code": "journal_write_failed",
    "stage": "write_journal",
    "message": "Transaction Journal could not be written.",
    "retryable": false,
    "rollback_result": "rolled_back"
  }
}
```

规则：

```text
Journal 写入失败时，Graph Write 不能报告成功。
不返回 write_ref。
```

---

# 8. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperAppendErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperAppendStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `RollbackResult` | `EBlueprintHelperRollbackResult` | `error.rollback_result` | `string enum` | 是 | 回滚结果。 |
| `FailedItem` | `FBlueprintHelperFailedItem` | `error.failed_item` | `object` | 可选 | 失败对象摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表，仅失败定位必要时返回。 |

---

# 9. 错误码

第一版建议：

```text
target_blueprint_not_found
target_not_blueprint
target_graph_not_empty
target_graph_type_invalid
custom_event_already_exists
global_event_creation_disallowed
function_not_found
event_not_found
call_signature_mismatch
pin_not_found
pin_type_mismatch
schema_rejected
node_create_failed
link_create_failed
ownership_write_failed
journal_write_failed
rollback_blocked
rollback_failed
write_permission_disabled
profile_policy_violation
bridge_disconnected
```

---

# 10. UE 侧建议结构体

建议新增或整理：

```cpp
FBlueprintHelperAppendGraphResultData
FBlueprintHelperAppendGraphResult
FBlueprintHelperAppendGraphInfo
FBlueprintHelperWriteRef
FBlueprintHelperAppendDryRunData
FBlueprintHelperDryRunIssue
FBlueprintHelperToolError
FBlueprintHelperFailedItem
FBlueprintHelperConflictItem
```

## 10.1 FBlueprintHelperAppendGraphResultData

```cpp
struct FBlueprintHelperAppendGraphResultData
{
    FString Schema; // AppendBlueprintGraph.v1
    FBlueprintHelperAppendGraphResult AppendResult;
    FBlueprintHelperWriteRef WriteRef;
};
```

## 10.2 FBlueprintHelperAppendGraphResult

```cpp
struct FBlueprintHelperAppendGraphResult
{
    FBlueprintHelperAppendGraphInfo Graph;
    TArray<FString> BlockRefs;
};
```

## 10.3 FBlueprintHelperAppendGraphInfo

```cpp
struct FBlueprintHelperAppendGraphInfo
{
    FString GraphId;
    FString GraphName;
};
```

`bCreated` / `bAlreadyExisted` / `bWasEmpty` 不进入默认 Agent-facing 结构；如需审计，写入 Transaction Journal / Review / verbose/debug。

---

## 10.6 FBlueprintHelperWriteRef

```cpp
struct FBlueprintHelperWriteRef
{
    FString TransactionId;
    bool bJournalRecorded;
};
```

## 10.7 FBlueprintHelperAppendDryRunData

```cpp
struct FBlueprintHelperAppendDryRunData
{
    FString Schema; // AppendBlueprintGraphDryRun.v1
    FBlueprintHelperAppendDryRunResult DryRun;
};

struct FBlueprintHelperAppendDryRunResult
{
    FString Result; // passed | blocked
    bool bCanExecute;
    TArray<FString> BlockedBy;
    TArray<FBlueprintHelperDryRunIssue> Conflicts;
    TArray<FBlueprintHelperDryRunIssue> Errors;
};
```

---

# 11. validation 规则

Append 正式成功通常返回：

```json
{
  "should_compile": true,
  "should_save": true,
  "compiled": false,
  "saved": false
}
```

规则：

```text
1. Append 写蓝图图表，通常 should_compile=true。
2. Append 修改资产，通常 should_save=true。
3. compiled / saved 表示本工具调用中是否已经执行。
4. Conservative 默认不自动 save。
5. 是否自动 compile / save 由 Safety Profile 和 workflow 参数决定。
```

---

# 12. 内部 Journal / Review 规则

正式成功时 UE 内部必须：

```text
1. 生成 transaction_id。
2. 为每个独立入口生成 block_id。
3. 写 BlueprintHelper-owned Metadata。
4. 写 NodeComment。
5. 记录 created_nodes / created_links / diff / rollback_data。
6. 写 Transaction Journal。
7. 进入 Review Store。
```

Agent-facing 成功结果只返回：

```text
block_refs
write_ref.transaction_id
journal_recorded
validation
```

---

# 13. 验收标准

```text
1. operation 固定为 append_blueprint_graph。
2. 成功 data.schema 固定为 AppendBlueprintGraph.v1。
3. dry_run data.schema 固定为 AppendBlueprintGraphDryRun.v1。
4. 成功返回 data.append_result.graph / block_refs。
5. block_refs 是 string[]，不返回 blocks[].entry_type / blocks[].entry_name。
6. full block_id 由 graph_id + "_" + block_ref 反推。
7. 成功不返回 ownership。
8. ownership 写入失败整体失败并回滚。
9. 成功使用 data.write_ref，不使用顶层 transaction。
10. write_ref 第一版只包含 transaction_id / journal_recorded。
11. dry_run 成功默认只返回 result=passed / can_execute=true。
12. dry_run blocked 默认只返回 blocked_by / conflicts / errors。
13. dry_run 不返回 plan / would_xxx / block_ref / transaction_id。
14. 正式失败只返回 error，不返回 append_result。
15. preflight 失败 rollback_result=not_needed。
16. 写入中失败但成功回滚 rollback_result=rolled_back。
17. 回滚 blocked / failed 可允许 modified=true。
18. Journal 写入失败时不能报告成功。
19. 默认不返回 review / safety / diagnostics / next。
20. 默认不返回 summary / created_nodes / created_links 计数。
```
