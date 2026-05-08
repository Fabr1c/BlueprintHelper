# BlueprintHelper ReplaceBlueprintGraph UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：ReplaceBlueprintGraph 字段确认稿  
本文边界：确认 ReplaceBlueprintGraph 的 Agent-facing 返回字段、UE 侧结构体映射、成功极简返回、dry_run、失败诊断、replace_scope、write_ref 与 validation 规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

ReplaceBlueprintGraph 采用 Graph Write 成功极简返回规则：

```text
1. 成功返回只保留后续 handle，不返回 diff / summary / 节点计数。
2. 顶层 target 只表达执行路由范围，不默认返回 target_type。
3. data.replace_result 内不再使用 target / target_kind。
4. data.replace_result 使用 replaced_ref。
5. 替换 BlueprintHelper-owned block 时，replaced_ref.target_ref 为原 block_ref，并保留原 block_id。
6. 用户手写目标默认不生成 block_ref，不接管 ownership。
7. write_ref 返回 transaction_id / journal_recorded。
8. dry_run 采用极简返回。
9. 正式失败不返回 replace_result，但 error 必须保留诊断信息。
```

---

## 1. 工具定位

`ReplaceBlueprintGraph` 只负责：

```text
替换一个明确目标的完整实现。
```

可替换目标：

```text
BlueprintHelper-owned block
function_body
event_body
custom_event_body
function_definition
event_definition
graph
```

不负责：

```text
1. 追加新独立逻辑块。使用 AppendBlueprintGraph。
2. 接入已有执行流。使用 MergeBlueprintGraph。
3. 精确修改节点 / Pin / 默认值 / 连接。使用 PatchBlueprintGraph。
4. 清理旧 block。使用 Cleanup 工具簇。
5. 模糊查找目标并替换。
```

---

## 2. ToolResultBase 约束

ReplaceBlueprintGraph 使用精简 ToolResultBase。

正式成功返回允许：

```text
ok
schema
operation
trace_id
status
modified
target
data.replace_result.replaced_ref
data.write_ref
validation
```

默认不返回：

```text
target_type
target_kind
summary
replace_plan
before
after
full_diff
deleted_nodes / created_nodes / modified_nodes 计数
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
"operation": "replace_blueprint_graph"
```

---

## 4. data.schema

正式成功：

```json
"schema": "ReplaceBlueprintGraph.v1"
```

dry_run：

```json
"schema": "ReplaceBlueprintGraphDryRun.v1"
```

失败：

```text
不返回 data.replace_result。
失败原因只返回 error。
```

---

## 5. replace_scope

`replace_scope` 是 Replace 的核心字段，放在顶层 `target` 中。

枚举建议：

```text
block_implementation
function_body
event_body
custom_event_body
function_definition
event_definition
graph
```

含义：

| replace_scope | 含义 |
|---|---|
| `block_implementation` | 替换 BlueprintHelper-owned block 的实现，保留 block_id。 |
| `function_body` | 保留函数入口、签名、参数、返回值，只替换内部逻辑。 |
| `event_body` | 保留事件入口身份，只替换内部逻辑。 |
| `custom_event_body` | 保留 Custom Event 入口，只替换后方逻辑。 |
| `function_definition` | 替换函数定义，可能影响外部调用方。 |
| `event_definition` | 替换事件定义，可能影响外部调用方。 |
| `graph` | 替换明确图表范围内完整实现，高风险。 |

Conservative 下可自动执行的范围应限制在：

```text
block_implementation
function_body
event_body
custom_event_body
```

前提：

```text
1. 用户明确指定目标。
2. dry_run passed。
3. 不改变入口身份。
4. 不改变签名。
5. 不破坏 external_dependents。
```

---

# 6. 正式成功返回

## 6.1 JSON 示例：替换 owned block

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_20260503_1301",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },

  "data": {
    "schema": "ReplaceBlueprintGraph.v1",

    "replace_result": {
      "replaced_ref": {
        "graph_id": "EG_PhysicsDoor",
        "target_ref": "TogglePhysicsDoor0"
      }
    },

    "write_ref": {
      "transaction_id": "tx_20260503_1301",
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

## 6.2 JSON 示例：替换函数体

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_20260503_1302",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "OpenPhysicsDoor",
    "replace_scope": "function_body"
  },

  "data": {
    "schema": "ReplaceBlueprintGraph.v1",

    "replace_result": {
      "replaced_ref": {
        "graph_id": "OpenPhysicsDoor",
        "target_ref": "OpenPhysicsDoor"
      }
    },

    "write_ref": {
      "transaction_id": "tx_20260503_1302",
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

# 7. 字段映射

## 7.1 target

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 目标蓝图资产路径。 |
| `GraphName` | `FString` | `target.graph` | `string` | 是 | 目标图表 / 函数图 / 事件图名。 |
| `ReplaceScope` | `EBlueprintHelperReplaceScope` | `target.replace_scope` | `string enum` | 是 | 替换范围。 |

不返回：

```text
target.target_type
```

---

## 7.2 data.replace_result.replaced_ref

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `GraphId` | `FString` | `data.replace_result.replaced_ref.graph_id` | `string` | 是 | 目标所在图表 ID。第一版可与 graph 名相同。 |
| `TargetRef` | `FString` | `data.replace_result.replaced_ref.target_ref` | `string` | 是 | 被替换目标引用。owned block 时为 block_ref。 |

不返回：

```text
target_kind
entry_type
entry_name
summary
```

目标类型由：

```text
target.replace_scope
```

推导。

---

## 7.3 data.write_ref

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `data.write_ref.transaction_id` | `string` | 是 | 本次正式写工具调用的 transaction ID。 |
| `bJournalRecorded` | `bool` | `data.write_ref.journal_recorded` | `boolean` | 是 | Journal 是否已成功记录。 |

规则：

```text
1. write_ref 只在正式成功时返回。
2. 不返回顶层 transaction。
3. 不返回 review_status。
4. 不返回 journal_path。
5. 不返回 rollback_data。
6. Journal 写入失败时，Graph Write 不能报告成功。
```

---

# 8. dry_run 返回

Replace 替换任何已有目标前都必须支持 dry_run。

## 8.1 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_20260503_1303",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },

  "data": {
    "schema": "ReplaceBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

## 8.2 dry_run blocked

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_20260503_1304",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "OpenPhysicsDoor",
    "replace_scope": "function_definition"
  },

  "data": {
    "schema": "ReplaceBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "external_dependents_may_break"
      ],
      "conflicts": [
        {
          "code": "external_dependents_may_break",
          "message": "Function definition replacement may break external callers."
        }
      ],
      "errors": []
    }
  }
}
```

dry_run 不返回：

```text
replace_plan
would_xxx
will_delete_nodes
will_create_nodes
affected_user_nodes
block_ref
transaction_id
ownership
review
safety
diagnostics
next
```

完整 replace plan 进入 Journal / Review / verbose/debug。

---

# 9. 正式失败返回

正式失败不返回：

```text
data.replace_result
data.write_ref
ownership
review
safety
diagnostics
next
```

但 `error` 必须包含足够诊断信息。

## 9.1 preflight / resolve_target 失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_20260503_1305",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },

  "error": {
    "code": "target_block_not_found",
    "stage": "resolve_target",
    "message": "The requested BlueprintHelper-owned block was not found.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

## 9.2 写入中失败并成功回滚

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_20260503_1306",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },

  "error": {
    "code": "link_create_failed",
    "stage": "create_links",
    "message": "Replacement graph links could not be created.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "link",
      "ref": "links[4]"
    }
  }
}
```

## 9.3 rollback blocked / failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_20260503_1307",
  "status": "failed",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },

  "error": {
    "code": "rollback_failed",
    "stage": "rollback",
    "message": "ReplaceBlueprintGraph failed and rollback could not restore the previous graph state.",
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

规则：

```text
rollback_result=blocked / failed 时，允许 modified=true。
Agent 必须 stop_and_report。
不得继续 compile / save / patch / merge / replace。
```

---

# 10. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperReplaceErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperGraphWriteStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `RollbackResult` | `EBlueprintHelperRollbackResult` | `error.rollback_result` | `string enum` | 是 | 回滚结果。 |
| `FailedItem` | `FBlueprintHelperFailedItem` | `error.failed_item` | `object` | 可选 | 失败对象摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表，仅失败定位必要时返回。 |

---

# 11. validation

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

# 12. UE 侧建议结构体

```cpp
struct FBlueprintHelperReplaceGraphResultData
{
    FString Schema; // ReplaceBlueprintGraph.v1
    FBlueprintHelperReplaceGraphResult ReplaceResult;
    FBlueprintHelperWriteRef WriteRef;
};

struct FBlueprintHelperReplaceGraphResult
{
    FBlueprintHelperReplacedRef ReplacedRef;
};

struct FBlueprintHelperReplacedRef
{
    FString GraphId;
    FString TargetRef;
};

struct FBlueprintHelperWriteRef
{
    FString TransactionId;
    bool bJournalRecorded;
};
```

不包含：

```cpp
TargetKind
EntryType
EntryName
Summary
```

这些进入 Journal / Review / verbose/debug。

---

# 13. 验收标准

```text
1. operation 固定为 replace_blueprint_graph。
2. data.schema 固定为 ReplaceBlueprintGraph.v1。
3. dry_run schema 固定为 ReplaceBlueprintGraphDryRun.v1。
4. 顶层 target 不返回 target_type。
5. 成功返回 data.replace_result.replaced_ref。
6. 不返回 data.replace_result.target。
7. 不返回 target_kind。
8. 不返回 summary。
9. owned block 替换保留原 block_id / block_ref。
10. 用户手写目标默认不生成 block_ref，不接管 ownership。
11. 成功使用 data.write_ref，不使用顶层 transaction。
12. dry_run passed 只返回 result / can_execute。
13. dry_run blocked 返回 blocked_by / conflicts / errors。
14. 正式失败不返回 replace_result，但 error 保留诊断信息。
15. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
