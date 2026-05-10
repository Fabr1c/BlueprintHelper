# BlueprintHelper PatchBlueprintGraph UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：PatchBlueprintGraph 字段确认稿  
本文边界：确认 PatchBlueprintGraph 的 Agent-facing 返回字段、UE 侧结构体映射、成功极简返回、patched_ref、patch_type、expected_old_state、dry_run、失败诊断、write_ref 与 validation 规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

PatchBlueprintGraph 采用 Graph Write 成功极简返回规则：

```text
1. 成功返回只保留后续 handle，不返回 diff / summary / 节点计数。
2. 顶层 target 只表达执行路由范围，不默认返回 target_type。
3. data.patch_result 内不再使用 target / target_kind。
4. data.patch_result 使用 patched_ref。
5. patched_ref 默认返回 node_ref / pin_ref / link_ref。
6. 只有无法从 LogicJson group 上下文反推完整路径时，才返回 node_path / pin_path / link_path。
7. patch 只返回 patch_type / expected_old_state_provided / changed。
8. write_ref 返回 transaction_id / journal_recorded。
9. dry_run 采用极简返回。
10. 正式失败不返回 patch_result，但 error 必须保留诊断信息。
```

---

## 1. 工具定位

`PatchBlueprintGraph` 只负责：

```text
精确修改一个明确目标点。
```

可修改：

```text
节点属性
Pin 默认值
Pin 连接
单条 Link
节点位置 / 注释 / 展示属性
BlueprintHelper-owned block 内的局部节点
```

不负责：

```text
1. 新增完整独立逻辑块。使用 AppendBlueprintGraph。
2. 替换完整实现。使用 ReplaceBlueprintGraph。
3. 接入已有执行流。使用 MergeBlueprintGraph。
4. 模糊查找节点。
5. 根据自然语言猜测目标。
```

---

## 2. ToolResultBase 约束

PatchBlueprintGraph 使用精简 ToolResultBase。

正式成功返回允许：

```text
ok
schema
operation
trace_id
status
modified
target
data.patch_result.patched_ref
data.patch_result.patch
data.write_ref
validation
```

默认不返回：

```text
target_type
target_kind
summary
before
after
old_value
new_value
patch_plan
full_diff
modified_nodes / modified_pins 计数
created_links / deleted_links 计数
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
"operation": "patch_blueprint_graph"
```

---

## 4. data.schema

正式成功：

```json
"schema": "PatchBlueprintGraph.v1"
```

dry_run：

```json
"schema": "PatchBlueprintGraphDryRun.v1"
```

失败：

```text
不返回 data.patch_result。
失败原因只返回 error。
```

---

## 5. patch_scope

`patch_scope` 放在顶层 `target` 中。

枚举建议：

```text
pin_default
node_property
node_comment
node_position
connect_pins
disconnect_link
replace_link
call_target
local_variable_ref
```

目标类型由：

```text
target.patch_scope
data.patch_result.patch.patch_type
```

推导，不再使用 `target_type` 或 `target_kind`。

---

## 6. patch_type

`patch_type` 放在 `data.patch_result.patch.patch_type`。

枚举建议：

```text
set_pin_default
set_node_property
set_node_comment
set_node_position
connect_pins
disconnect_link
replace_link
set_call_target
rename_local_variable_ref
```

---

# 7. 正式成功返回

## 7.1 JSON 示例：设置 Pin 默认值

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1401",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "pin_default"
  },

  "data": {
    "schema": "PatchBlueprintGraph.v1",

    "patch_result": {
      "patched_ref": {
        "graph_id": "EG_PhysicsDoor",
        "node_ref": "Branch0",
        "pin_ref": "Condition"
      },

      "patch": {
        "patch_type": "set_pin_default",
        "expected_old_state_provided": true,
        "changed": true
      }
    },

    "write_ref": {
      "transaction_id": "tx_20260503_1401",
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

## 7.2 JSON 示例：替换连接

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1402",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "replace_link"
  },

  "data": {
    "schema": "PatchBlueprintGraph.v1",

    "patch_result": {
      "patched_ref": {
        "graph_id": "EG_PhysicsDoor",
        "link_ref": "links[4]"
      },

      "patch": {
        "patch_type": "replace_link",
        "expected_old_state_provided": true,
        "changed": true
      }
    },

    "write_ref": {
      "transaction_id": "tx_20260503_1402",
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

# 8. 字段映射

## 8.1 target

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 目标蓝图资产路径。 |
| `GraphName` | `FString` | `target.graph` | `string` | 是 | 目标图表 / 函数图名。 |
| `PatchScope` | `EBlueprintHelperPatchScope` | `target.patch_scope` | `string enum` | 是 | Patch 范围。 |

不返回：

```text
target.target_type
```

---

## 8.2 data.patch_result.patched_ref

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `GraphId` | `FString` | `data.patch_result.patched_ref.graph_id` | `string` | 是 | 目标所在图表 ID。第一版可与 graph 名相同。 |
| `NodeRef` | `FString` | `data.patch_result.patched_ref.node_ref` | `string` | 视 patch 类型 | 命中节点局部引用。 |
| `PinRef` | `FString` | `data.patch_result.patched_ref.pin_ref` | `string` | 视 patch 类型 | 命中 Pin 局部引用。 |
| `LinkRef` | `FString` | `data.patch_result.patched_ref.link_ref` | `string` | 视 patch 类型 | 命中 Link 局部引用。 |
| `NodePath` | `FString` | `data.patch_result.patched_ref.node_path` | `string` | 可选 | 无法从上下文反推时才返回。 |
| `PinPath` | `FString` | `data.patch_result.patched_ref.pin_path` | `string` | 可选 | 无法从上下文反推时才返回。 |
| `LinkPath` | `FString` | `data.patch_result.patched_ref.link_path` | `string` | 可选 | 无法从上下文反推时才返回。 |

默认返回局部 ref：

```text
node_ref
pin_ref
link_ref
```

只有无法从 LogicJson group 上下文反推完整路径时，才返回完整 path。

---

## 8.3 data.patch_result.patch

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `PatchType` | `EBlueprintHelperPatchType` | `data.patch_result.patch.patch_type` | `string enum` | 是 | Patch 类型。 |
| `bExpectedOldStateProvided` | `bool` | `data.patch_result.patch.expected_old_state_provided` | `boolean` | 是 | 是否提供 expected_old_state / expected_old_value。 |
| `bChanged` | `bool` | `data.patch_result.patch.changed` | `boolean` | 是 | 是否产生实际修改。 |

不返回：

```text
before
after
old_value
new_value
```

---

## 8.4 data.write_ref

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `data.write_ref.transaction_id` | `string` | 是 | 本次正式写工具调用的 transaction ID。 |
| `bJournalRecorded` | `bool` | `data.write_ref.journal_recorded` | `boolean` | 是 | Journal 是否已成功记录。 |

---

# 9. expected_old_state

Patch 不强制所有场景都携带 `expected_old_state` / `expected_old_value`。

必须或建议携带：

```text
用户手写节点
高风险修改
连接关系修改
影响执行流的 Pin
目标存在多义性
```

可省略：

```text
BlueprintHelper-owned 节点
目标定位明确
低风险默认值修改
old/new value 是长文本，重复传输 Token 成本高
```

即使省略，UE 侧仍必须：

```text
执行前读取当前状态
Journal / Review 记录 before / after
```

Agent-facing 成功结果只返回：

```text
expected_old_state_provided
```

---

# 10. LogicJson 定位规则

Patch 目标定位优先使用 LogicJson。

规则：

```text
1. target_graph / blueprint / multi_target 使用 logic.groups[]。
2. node_ref / link_ref 是 group 内局部引用。
3. pin_ref 也只在对应 node/group 上下文内有效。
4. 需要完整 node_path / pin_path / link_path 时，应从 group.entry.node_path 反推。
5. 不得跨 group 使用局部引用。
6. 仅靠节点显示名 / Pin 名不允许直接 Patch。
```

Patch 成功返回的 `patched_ref` 只是执行确认，不替代 Patch 前的 LogicJson 定位过程。

---

# 11. dry_run 返回

## 11.1 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1403",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "pin_default"
  },

  "data": {
    "schema": "PatchBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

## 11.2 dry_run blocked

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1404",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "pin_default"
  },

  "data": {
    "schema": "PatchBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "expected_old_state_mismatch"
      ],
      "conflicts": [
        {
          "code": "expected_old_state_mismatch",
          "message": "Target state no longer matches expected_old_state."
        }
      ],
      "errors": []
    }
  }
}
```

dry_run 不返回：

```text
patch_plan
would_modify_nodes
would_create_links
would_delete_links
before
after
ownership
review
safety
diagnostics
next
```

---

# 12. 正式失败返回

正式失败不返回：

```text
data.patch_result
data.write_ref
ownership
review
safety
diagnostics
next
```

但 `error` 必须包含足够诊断信息。

## 12.1 preflight / resolve_target 失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1405",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "pin_default"
  },

  "error": {
    "code": "target_pin_not_found",
    "stage": "resolve_target",
    "message": "The requested target pin was not found.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

## 12.2 expected_old_state mismatch

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1406",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "pin_default"
  },

  "error": {
    "code": "expected_old_state_mismatch",
    "stage": "preflight",
    "message": "Target state no longer matches expected_old_state.",
    "retryable": false,
    "rollback_result": "not_needed",
    "conflicts": [
      {
        "code": "expected_old_state_mismatch",
        "target": "Branch0.Condition"
      }
    ]
  }
}
```

## 12.3 写入中失败并成功回滚

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1407",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "replace_link"
  },

  "error": {
    "code": "pin_type_mismatch",
    "stage": "connect_pins",
    "message": "The requested pins cannot be connected because their types are incompatible.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "link",
      "ref": "link_ref[3]"
    }
  }
}
```

## 12.4 rollback blocked / failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "trace_id": "trace_20260503_1408",
  "status": "failed",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "replace_link"
  },

  "error": {
    "code": "rollback_failed",
    "stage": "rollback",
    "message": "PatchBlueprintGraph failed and rollback could not restore the previous graph state.",
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

# 13. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperPatchErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
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
struct FBlueprintHelperPatchGraphResultData
{
    FString Schema; // PatchBlueprintGraph.v1
    FBlueprintHelperPatchGraphResult PatchResult;
    FBlueprintHelperWriteRef WriteRef;
};

struct FBlueprintHelperPatchGraphResult
{
    FBlueprintHelperPatchedRef PatchedRef;
    FBlueprintHelperPatchSummary Patch;
};

struct FBlueprintHelperPatchedRef
{
    FString GraphId;

    // Default local refs.
    FString NodeRef;
    FString PinRef;
    FString LinkRef;

    // Optional fallback only when local refs are insufficient.
    FString NodePath;
    FString PinPath;
    FString LinkPath;
};

struct FBlueprintHelperPatchSummary
{
    FString PatchType;
    bool bExpectedOldStateProvided;
    bool bChanged;
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
Summary
Before
After
OldValue
NewValue
```

这些进入 Journal / Review / verbose/debug。

---

# 16. 验收标准

```text
1. operation 固定为 patch_blueprint_graph。
2. data.schema 固定为 PatchBlueprintGraph.v1。
3. dry_run schema 固定为 PatchBlueprintGraphDryRun.v1。
4. 顶层 target 不返回 target_type。
5. 成功返回 data.patch_result.patched_ref。
6. 不返回 data.patch_result.target。
7. 不返回 target_kind。
8. 不返回 summary。
9. patch 只返回 patch_type / expected_old_state_provided / changed。
10. 成功不返回 before / after / old_value / new_value。
11. 成功使用 data.write_ref，不使用顶层 transaction。
12. dry_run passed 只返回 result / can_execute。
13. dry_run blocked 返回 blocked_by / conflicts / errors。
14. 正式失败不返回 patch_result，但 error 保留诊断信息。
15. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
