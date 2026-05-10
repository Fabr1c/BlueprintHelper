# BlueprintHelper MergeBlueprintGraph UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：MergeBlueprintGraph 字段确认稿  
本文边界：确认 MergeBlueprintGraph 的 Agent-facing 返回字段、UE 侧结构体映射、merge_scope、insert_strategy、merged_ref、dry_run、正式失败、write_ref 与 validation 规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

MergeBlueprintGraph 采用 Graph Write 成功极简返回规则：

```text
1. operation 固定为 merge_blueprint_graph。
2. 顶层 target 只保留 asset_path / graph / merge_scope / insert_strategy，不返回 target_type。
3. merge_scope 使用 owned_block_call / custom_event_call / function_call / inline_nodes / event_entry_logic。
4. 第一版优先支持 owned_block_call / custom_event_call / function_call。
5. insert_strategy 固定为 append_after / insert_between / branch_fork。
6. branch_fork 必须显式 sequence_order。
7. 成功返回使用 data.merge_result.merged_ref。
8. merged_ref 只返回 graph_id / anchor_ref / inserted_ref，branch_fork 可额外返回 sequence_ref。
9. 成功不返回 disconnected_links / created_links / execution_order_changed / affected_user_nodes。
10. write_ref 沿用 transaction_id / journal_recorded。
11. dry_run 必须支持，且 Merge 默认必须 dry_run。
12. dry_run passed 极简，blocked 返回 blocked_by / conflicts / errors。
13. 正式失败不返回 merge_result，但 error 必须保留完整诊断。
14. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
```

---

## 1. 工具定位

`MergeBlueprintGraph` 只负责：

```text
把新逻辑或已有逻辑引用接入一个明确的已有执行流。
```

它不同于：

```text
AppendBlueprintGraph：新增独立 owned block，不接入已有执行链。
ReplaceBlueprintGraph：替换明确目标的完整实现。
PatchBlueprintGraph：精确修改一个节点 / Pin / Link / 默认值。
```

Merge 是 Graph Write 中风险最高的一类，因为它会改变已有执行流连接关系。

---

## 2. ToolResultBase 约束

正式成功允许返回：

```text
ok
schema
operation
trace_id
status
modified
target
data.merge_result.merged_ref
data.write_ref
validation
```

默认不返回：

```text
target_type
target_kind
merge_plan
disconnected_links
created_links
execution_order_changed
affected_user_nodes
before / after
full_diff
ownership
review
safety
diagnostics
next
journal_path
rollback_data
```

---

## 3. operation

固定使用：

```json
"operation": "merge_blueprint_graph"
```

---

## 4. data.schema

正式成功：

```json
"schema": "MergeBlueprintGraph.v1"
```

dry_run：

```json
"schema": "MergeBlueprintGraphDryRun.v1"
```

失败：

```text
不返回 data.merge_result。
失败原因只返回 error。
```

---

# 5. 顶层 target

顶层 `target` 只表达执行路由范围：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EventGraph",
  "merge_scope": "owned_block_call",
  "insert_strategy": "insert_between"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 目标蓝图资产路径。 |
| `GraphName` | `FString` | `target.graph` | `string` | 是 | 接入执行流所在图表。 |
| `MergeScope` | `EBlueprintHelperMergeScope` | `target.merge_scope` | `string enum` | 是 | 接入逻辑类型。 |
| `InsertStrategy` | `EBlueprintHelperInsertStrategy` | `target.insert_strategy` | `string enum` | 是 | 接入策略。 |

不返回：

```text
target.target_type
```

---

# 6. merge_scope

建议枚举：

```text
owned_block_call
custom_event_call
function_call
inline_nodes
event_entry_logic
```

含义：

| merge_scope | 含义 |
|---|---|
| `owned_block_call` | 把已有 BlueprintHelper-owned block 作为调用接入执行流。 |
| `custom_event_call` | 插入 Custom Event 调用。 |
| `function_call` | 插入函数调用。 |
| `inline_nodes` | 直接插入一段新节点。 |
| `event_entry_logic` | 把逻辑接到事件入口后方。 |

第一版优先支持：

```text
owned_block_call
custom_event_call
function_call
```

`inline_nodes` 风险更高，容易混合 Append + Merge 职责，第一版可后置。

---

# 7. insert_strategy

必须显式指定：

```text
append_after
insert_between
branch_fork
```

## 7.1 append_after

语义：

```text
把新逻辑接到 anchor Exec Pin 后方。
```

要求：

```text
anchor Exec Pin 当前没有后继。
```

若已有后继，dry_run blocked：

```text
code = anchor_exec_pin_already_connected
```

不得自动改成 `insert_between` 或 `branch_fork`。

## 7.2 insert_between

语义：

```text
断开 anchor Exec Pin 的原后继。
插入新逻辑。
新逻辑执行完后重接原后继。
```

必须 dry_run。

## 7.3 branch_fork

语义：

```text
插入 Sequence 或等价分发节点，把原后继和新逻辑分到不同分支。
```

必须显式给出：

```text
sequence_order
```

建议参数语义：

```json
"sequence_order": [
  "original_successor",
  "inserted_logic"
]
```

不允许工具默认决定执行顺序。

---

# 8. 正式成功返回

## 8.1 insert_between 成功示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1501",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "insert_between"
  },

  "data": {
    "schema": "MergeBlueprintGraph.v1",

    "merge_result": {
      "merged_ref": {
        "graph_id": "EventGraph",
        "anchor_ref": "BeginPlay0.Then",
        "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0"
      }
    },

    "write_ref": {
      "transaction_id": "tx_20260503_1501",
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

## 8.2 branch_fork 成功示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1502",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "branch_fork"
  },

  "data": {
    "schema": "MergeBlueprintGraph.v1",

    "merge_result": {
      "merged_ref": {
        "graph_id": "EventGraph",
        "anchor_ref": "BeginPlay0.Then",
        "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0",
        "sequence_ref": "Sequence0"
      }
    },

    "write_ref": {
      "transaction_id": "tx_20260503_1502",
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

---

# 9. data.merge_result.merged_ref

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `GraphId` | `FString` | `data.merge_result.merged_ref.graph_id` | `string` | 是 | 执行流所在图表 ID。第一版可与 graph 名相同。 |
| `AnchorRef` | `FString` | `data.merge_result.merged_ref.anchor_ref` | `string` | 是 | 接入锚点引用，例如 `BeginPlay0.Then`。 |
| `InsertedRef` | `FString` | `data.merge_result.merged_ref.inserted_ref` | `string` | 是 | 被插入逻辑引用，例如 block_id / function ref / custom event ref。 |
| `SequenceRef` | `FString` | `data.merge_result.merged_ref.sequence_ref` | `string` | branch_fork 时 | 插入的 Sequence 或等价分发节点引用。 |

不返回：

```text
disconnected_links
created_links
execution_order_changed
affected_user_nodes
old_successor
new_successor
summary
```

这些进入 Journal / Review / verbose/debug。

---

# 10. data.write_ref

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `data.write_ref.transaction_id` | `string` | 是 | 本次正式写工具调用的 transaction ID。 |
| `bJournalRecorded` | `bool` | `data.write_ref.journal_recorded` | `boolean` | 是 | Journal 是否已成功记录。 |

---

# 11. dry_run 返回

Merge 默认必须 dry_run。

## 11.1 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1503",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "insert_between"
  },

  "data": {
    "schema": "MergeBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

## 11.2 dry_run blocked：append_after 遇到已有后继

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1504",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "append_after"
  },

  "data": {
    "schema": "MergeBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "anchor_exec_pin_already_connected"
      ],
      "conflicts": [
        {
          "code": "anchor_exec_pin_already_connected",
          "message": "append_after cannot be used because the anchor Exec Pin already has a successor."
        }
      ],
      "errors": []
    }
  }
}
```

## 11.3 dry_run blocked：branch_fork 缺少顺序

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1505",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "branch_fork"
  },

  "data": {
    "schema": "MergeBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "sequence_order_required"
      ],
      "conflicts": [
        {
          "code": "sequence_order_required",
          "message": "branch_fork requires explicit sequence_order."
        }
      ],
      "errors": []
    }
  }
}
```

dry_run 不返回：

```text
merge_plan
would_disconnect_links
would_create_links
would_insert_sequence
execution_order_preview
affected_user_nodes
before / after
```

---

# 12. 正式失败返回

正式失败不返回：

```text
data.merge_result
data.write_ref
ownership
review
safety
diagnostics
next
```

但 `error` 必须完整。

## 12.1 anchor 解析失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1506",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "insert_between"
  },

  "error": {
    "code": "anchor_pin_not_found",
    "stage": "resolve_anchor",
    "message": "The requested anchor pin was not found.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

## 12.2 写入中失败并回滚

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1507",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "insert_between"
  },

  "error": {
    "code": "link_create_failed",
    "stage": "create_links",
    "message": "Merge links could not be created.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "link",
      "ref": "merge_link[1]"
    },
    "conflicts": [
      {
        "code": "pin_type_mismatch",
        "source": "InsertedLogic.Then",
        "target": "OriginalSuccessor.Execute"
      }
    ]
  }
}
```

## 12.3 rollback blocked / failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "trace_id": "trace_20260503_1508",
  "status": "failed",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "insert_between"
  },

  "error": {
    "code": "rollback_failed",
    "stage": "rollback",
    "message": "MergeBlueprintGraph failed and rollback could not restore the previous execution flow.",
    "retryable": false,
    "rollback_result": "failed",
    "conflicts": [
      {
        "code": "asset_state_changed_during_write",
        "target": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      }
    ]
  }
}
```

---

# 13. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperMergeErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperGraphWriteStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `RollbackResult` | `EBlueprintHelperRollbackResult` | `error.rollback_result` | `string enum` | 是 | 回滚结果。 |
| `FailedItem` | `FBlueprintHelperFailedItem` | `error.failed_item` | `object` | 可选 | 失败对象摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表，仅失败定位必要时返回。 |

---

# 14. validation

成功通常返回：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bShouldCompile` | `bool` | `validation.should_compile` | `boolean` | 是 | 是否建议后续编译。 |
| `bShouldSave` | `bool` | `validation.should_save` | `boolean` | 是 | 是否建议后续保存。 |
| `bCompiled` | `bool` | `validation.compiled` | `boolean` | 是 | 本工具调用中是否已编译。 |
| `bSaved` | `bool` | `validation.saved` | `boolean` | 是 | 本工具调用中是否已保存。 |

---

# 15. UE 侧建议结构体

```cpp
struct FBlueprintHelperMergeGraphResultData
{
    FString Schema; // MergeBlueprintGraph.v1
    FBlueprintHelperMergeGraphResult MergeResult;
    FBlueprintHelperWriteRef WriteRef;
};

struct FBlueprintHelperMergeGraphResult
{
    FBlueprintHelperMergedRef MergedRef;
};

struct FBlueprintHelperMergedRef
{
    FString GraphId;
    FString AnchorRef;
    FString InsertedRef;
    FString SequenceRef; // Optional for branch_fork.
};

struct FBlueprintHelperWriteRef
{
    FString TransactionId;
    bool bJournalRecorded;
};
```

不包含：

```cpp
DisconnectedLinks
CreatedLinks
ExecutionOrderChanged
AffectedUserNodes
Summary
```

这些进入 Journal / Review / verbose/debug。

---

# 16. 验收标准

```text
1. operation 固定为 merge_blueprint_graph。
2. data.schema 固定为 MergeBlueprintGraph.v1。
3. dry_run schema 固定为 MergeBlueprintGraphDryRun.v1。
4. 顶层 target 不返回 target_type。
5. target 包含 asset_path / graph / merge_scope / insert_strategy。
6. merge_scope 支持 owned_block_call / custom_event_call / function_call / inline_nodes / event_entry_logic。
7. 第一版优先支持 owned_block_call / custom_event_call / function_call。
8. insert_strategy 固定为 append_after / insert_between / branch_fork。
9. branch_fork 必须显式 sequence_order。
10. 成功返回 data.merge_result.merged_ref。
11. merged_ref 只返回 graph_id / anchor_ref / inserted_ref，branch_fork 可返回 sequence_ref。
12. 成功不返回 disconnected_links / created_links / execution_order_changed / affected_user_nodes。
13. dry_run passed 极简。
14. dry_run blocked 返回 blocked_by / conflicts / errors。
15. 正式失败不返回 merge_result，但 error 保留诊断信息。
16. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
