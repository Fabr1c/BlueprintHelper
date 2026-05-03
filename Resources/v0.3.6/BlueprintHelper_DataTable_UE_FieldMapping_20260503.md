# BlueprintHelper DataTable UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：DataTable 字段确认稿  
本文边界：确认 DataTable 读取、行新增、行更新、行删除工具的 Agent-facing 返回字段、UE 侧结构体映射、事务化批量写入、dry_run、validation 规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

DataTable 工具簇采用以下字段口径：

```text
1. DataTable 第一版不做 CSV / JSON 全量导入导出。
2. DataTable 第一版不修改 C++ struct/class 或 RowStruct。
3. read_data_table 只返回 row_struct / row_count / row_names，不默认返回所有行。
4. read_data_table_rows 只返回请求行 values。
5. add_data_table_row 成功只返回 added_count。
6. add_data_table_row name_collision 只支持 fail_if_exists / reuse_if_exists。
7. 不支持 auto_rename / replace_existing。
8. update_data_table_row / rows 成功只返回 row_result 计数。
9. DataTable 批量更新默认事务化，invalid 时整批失败。
10. remove_data_table_row / rows 必须 dry_run。
11. remove 成功只返回 removed_count。
12. DataTable 写工具成功不返回 write_ref / transaction_id / review / safety。
13. DataTable 写工具成功保留 validation，但 validation 只返回 should_compile / should_save。
14. DataTable 写工具 validation 不返回 compiled / saved。
15. 通常 should_compile=false / should_save=true。
16. 所有 data.schema 使用短命名。
```

---

## 1. 工具边界

第一版覆盖：

```text
read_data_table
read_data_table_rows
add_data_table_row
update_data_table_row
update_data_table_rows
remove_data_table_row
remove_data_table_rows
```

第一版不覆盖：

```text
CSV / JSON 全量导入导出
批量结构迁移
RowStruct 自动修改
DataTable RowStruct 替换
C++ struct/class 修改
复杂嵌套 UObject 引用批量解析
```

---

## 2. 通用返回原则

DataTable 写工具成功只返回：

```text
row_result
validation
```

成功不返回：

```text
row_name
row_values
before
after
all rows
write_ref
transaction_id
review
safety
```

错误 / blocked 时才返回：

```text
row_name
field
expected_type
ref
```

---

## 3. data.schema 短命名

使用：

```text
ReadDataTable.v1
ReadDataTableRows.v1
AddDataTableRow.v1
UpdateDataTableRow.v1
RemoveDataTableRow.v1
RemoveDataTableRowDryRun.v1
```

不使用 BlueprintHelper / MCP / Tools 前缀。

---

# 4. read_data_table

## 4.1 工具定位

`read_data_table` 负责：

```text
读取 DataTable 的结构摘要和行名列表。
```

不默认返回所有行内容。

---

## 4.2 operation

```json
"operation": "read_data_table"
```

---

## 4.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_data_table",
  "trace_id": "trace_20260503_3101",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "read_scope": "data_table"
  },

  "data": {
    "schema": "ReadDataTable.v1",
    "data_table": {
      "row_struct": "/Script/Game.WeaponRow",
      "row_count": 3,
      "row_names": [
        "Pistol",
        "Rifle",
        "Shotgun"
      ]
    }
  }
}
```

---

## 4.4 data_table 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `RowStruct` | `FString` | `data.data_table.row_struct` | `string` | 是 | RowStruct 路径。 |
| `RowCount` | `int32` | `data.data_table.row_count` | `number` | 是 | 行数。 |
| `RowNames` | `TArray<FName>` | `data.data_table.row_names` | `array<string>` | 是 | 行名列表。 |

默认不返回：

```text
all rows
row values
full struct schema
serialized binary data
```

---

# 5. read_data_table_rows

## 5.1 工具定位

`read_data_table_rows` 负责读取指定行内容。

---

## 5.2 operation

```json
"operation": "read_data_table_rows"
```

---

## 5.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_data_table_rows",
  "trace_id": "trace_20260503_3102",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "read_scope": "data_table_rows"
  },

  "data": {
    "schema": "ReadDataTableRows.v1",
    "rows": [
      {
        "row_name": "Pistol",
        "values": {
          "Damage": 12,
          "Cooldown": 0.25,
          "Ammo": 12
        }
      },
      {
        "row_name": "Rifle",
        "values": {
          "Damage": 20,
          "Cooldown": 0.1,
          "Ammo": 30
        }
      }
    ]
  }
}
```

---

## 5.4 rows[] 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `RowName` | `FName` | `data.rows[].row_name` | `string` | 是 | 行名。 |
| `Values` | `TMap<FString, FJsonValue>` | `data.rows[].values` | `object` | 是 | 行字段值。 |

---

## 5.5 请求行不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_data_table_rows",
  "trace_id": "trace_20260503_3103",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "read_scope": "data_table_rows"
  },

  "error": {
    "code": "row_not_found",
    "stage": "resolve_rows",
    "message": "One or more requested DataTable rows were not found.",
    "retryable": false,
    "conflicts": [
      {
        "code": "row_not_found",
        "row_name": "Laser"
      }
    ]
  }
}
```

---

# 6. add_data_table_row

## 6.1 工具定位

`add_data_table_row` 只负责：

```text
新增一行 DataTable row。
```

不负责：

```text
修改 RowStruct
批量导入 CSV
自动重命名 row
替换已有 row
```

---

## 6.2 operation

```json
"operation": "add_data_table_row"
```

---

## 6.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_data_table_row",
  "trace_id": "trace_20260503_3201",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },

  "data": {
    "schema": "AddDataTableRow.v1",
    "row_result": {
      "added_count": 1
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

成功不返回：

```text
row_name
row_values
before
after
write_ref
transaction_id
```

---

## 6.4 row 已存在

第一版只支持：

```text
fail_if_exists
reuse_if_exists
```

不支持：

```text
auto_rename
replace_existing
```

冲突失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_data_table_row",
  "trace_id": "trace_20260503_3202",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },

  "error": {
    "code": "row_already_exists",
    "stage": "row_collision_check",
    "message": "A DataTable row with the requested name already exists.",
    "retryable": false,
    "conflicts": [
      {
        "code": "row_already_exists",
        "row_name": "Pistol"
      }
    ]
  }
}
```

reuse_if_exists：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_data_table_row",
  "trace_id": "trace_20260503_3203",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },

  "data": {
    "schema": "AddDataTableRow.v1",
    "row_result": {
      "added_count": 0,
      "reused_existing": true
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

---

# 7. update_data_table_row / update_data_table_rows

## 7.1 工具定位

更新一个或多个已有 row 的字段。

不负责：

```text
新增 row
修改 RowStruct
全表迁移
自动创建缺失 row
```

---

## 7.2 operation

单行：

```json
"operation": "update_data_table_row"
```

批量：

```json
"operation": "update_data_table_rows"
```

---

## 7.3 单行成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "update_data_table_row",
  "trace_id": "trace_20260503_3301",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },

  "data": {
    "schema": "UpdateDataTableRow.v1",
    "row_result": {
      "mode": "single",
      "requested_count": 1,
      "updated_count": 1,
      "changed_count": 1,
      "no_op_count": 0
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

---

## 7.4 批量成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "update_data_table_rows",
  "trace_id": "trace_20260503_3302",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_rows"
  },

  "data": {
    "schema": "UpdateDataTableRow.v1",
    "row_result": {
      "mode": "batch",
      "requested_count": 3,
      "updated_count": 3,
      "changed_count": 2,
      "no_op_count": 1
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

成功不返回 row values。

---

## 7.5 row_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Mode` | `EBlueprintHelperRowWriteMode` | `data.row_result.mode` | `string enum` | update 时 | `single` 或 `batch`。 |
| `RequestedCount` | `int32` | `data.row_result.requested_count` | `number` | update 时 | 请求更新行数。 |
| `UpdatedCount` | `int32` | `data.row_result.updated_count` | `number` | update 时 | 更新行数。 |
| `ChangedCount` | `int32` | `data.row_result.changed_count` | `number` | update 时 | 实际变化数量。 |
| `NoOpCount` | `int32` | `data.row_result.no_op_count` | `number` | update 时 | 无变化数量。 |
| `AddedCount` | `int32` | `data.row_result.added_count` | `number` | add 时 | 新增行数。 |
| `RemovedCount` | `int32` | `data.row_result.removed_count` | `number` | remove 时 | 删除行数。 |
| `bReusedExisting` | `bool` | `data.row_result.reused_existing` | `boolean` | add no_op 时 | 已复用已存在行。 |

---

## 7.6 批量失败

默认事务化：

```text
任何 row / field invalid，整批失败。
```

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "update_data_table_rows",
  "trace_id": "trace_20260503_3303",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_rows"
  },

  "error": {
    "code": "invalid_data_table_row_settings",
    "stage": "validate_rows",
    "message": "One or more DataTable row updates are invalid.",
    "retryable": false,
    "conflicts": [
      {
        "code": "row_not_found",
        "row_name": "Laser"
      },
      {
        "code": "field_not_found",
        "row_name": "Pistol",
        "field": "DamageWrong"
      }
    ]
  }
}
```

---

# 8. remove_data_table_row / remove_data_table_rows

## 8.1 工具定位

删除明确 row。破坏性操作，必须 dry_run。

---

## 8.2 operation

单行：

```json
"operation": "remove_data_table_row"
```

批量：

```json
"operation": "remove_data_table_rows"
```

---

## 8.3 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_data_table_row",
  "trace_id": "trace_20260503_3401",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },

  "data": {
    "schema": "RemoveDataTableRowDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

---

## 8.4 dry_run blocked

如果第一版无法可靠检测外部引用，不应返回假精确依赖。

可返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_data_table_row",
  "trace_id": "trace_20260503_3402",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },

  "data": {
    "schema": "RemoveDataTableRowDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "row_external_dependents_exist"
      ],
      "conflicts": [
        {
          "code": "row_external_dependents_exist",
          "row_name": "Pistol",
          "message": "The row appears to be referenced by external logic or assets."
        }
      ],
      "errors": []
    }
  }
}
```

---

## 8.5 正式成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_data_table_row",
  "trace_id": "trace_20260503_3403",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },

  "data": {
    "schema": "RemoveDataTableRow.v1",
    "row_result": {
      "removed_count": 1
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

成功不返回 removed row values。

---

# 9. validation

DataTable 写工具成功通常返回：

```json
"validation": {
  "should_compile": false,
  "should_save": true
}
```

no_op 返回：

```json
"validation": {
  "should_compile": false,
  "should_save": false
}
```

写工具 validation 只返回：

```text
should_compile
should_save
```

不返回：

```text
compiled
saved
```

读工具不返回 validation。

---

# 10. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperDataTableErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperDataTableStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

---

# 11. UE 侧建议结构体

```cpp
struct FBlueprintHelperReadDataTableResultData
{
    FString Schema; // ReadDataTable.v1
    FBlueprintHelperDataTableSummary DataTable;
};

struct FBlueprintHelperDataTableSummary
{
    FString RowStruct;
    int32 RowCount = 0;
    TArray<FString> RowNames;
};

struct FBlueprintHelperReadDataTableRowsResultData
{
    FString Schema; // ReadDataTableRows.v1
    TArray<FBlueprintHelperDataTableRowValue> Rows;
};

struct FBlueprintHelperDataTableRowValue
{
    FString RowName;
    TMap<FString, TSharedPtr<FJsonValue>> Values;
};

struct FBlueprintHelperDataTableRowWriteResultData
{
    FString Schema; // AddDataTableRow.v1 / UpdateDataTableRow.v1 / RemoveDataTableRow.v1
    FBlueprintHelperDataTableRowWriteResult RowResult;
};

struct FBlueprintHelperDataTableRowWriteResult
{
    FString Mode; // single | batch, update only
    int32 RequestedCount = 0;
    int32 UpdatedCount = 0;
    int32 ChangedCount = 0;
    int32 NoOpCount = 0;
    int32 AddedCount = 0;
    int32 RemovedCount = 0;
    bool bReusedExisting = false;
};
```

明确不包含：

```cpp
FBlueprintHelperWriteRef
FString TransactionId
TMap<FString, FJsonValue> Before
TMap<FString, FJsonValue> After
TArray<FBlueprintHelperDataTableRowValue> AllRows
```

---

# 12. 验收标准

```text
1. read_data_table 只返回 row_struct / row_count / row_names。
2. read_data_table 不默认返回所有行。
3. read_data_table_rows 只返回请求行 values。
4. add_data_table_row 成功只返回 added_count。
5. name_collision 只支持 fail_if_exists / reuse_if_exists。
6. update_data_table_row / rows 成功只返回 row_result 计数。
7. DataTable 批量更新默认事务化。
8. remove_data_table_row / rows 必须 dry_run。
9. remove 成功只返回 removed_count。
10. 写工具不返回 write_ref / transaction_id / review / safety。
11. 写工具 validation 只返回 should_compile / should_save。
12. 写工具 validation 不返回 compiled / saved。
13. data.schema 使用短命名。
