# BlueprintHelper DataAsset UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：DataAsset 字段确认稿  
本文边界：确认 DataAsset 读取与属性写入工具的 Agent-facing 返回字段、UE 侧结构体映射、事务化批量写入、失败诊断、validation 规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

DataAsset 工具簇采用以下字段口径：

```text
1. DataAsset 第一版不做 CSV / JSON 全量导入导出。
2. DataAsset 第一版不修改 C++ struct/class。
3. read_data_asset_properties 返回 values；写工具不回显 before/after。
4. set_data_asset_property / set_data_asset_properties 成功只返回 property_result 计数。
5. DataAsset 批量属性写默认事务化，invalid 时整批失败。
6. DataAsset 写工具成功不返回 write_ref / transaction_id / review / safety。
7. DataAsset 写工具成功保留 validation，但 validation 只返回 should_compile / should_save。
8. DataAsset 写工具 validation 不返回 compiled / saved。
9. DataAsset 写工具通常 should_compile=false / should_save=true。
10. 所有 data.schema 使用短命名。
```

---

## 1. 工具边界

第一版覆盖：

```text
read_data_asset_properties
set_data_asset_property
set_data_asset_properties
```

第一版不覆盖：

```text
创建 DataAsset
修改 DataAsset class
修改 C++ class / struct
CSV / JSON 全量导入导出
复杂嵌套 UObject 引用批量解析
资产迁移 / Schema migration
```

---

## 2. 通用返回原则

DataAsset 写工具成功返回只包含：

```text
property_result
validation
```

成功不返回：

```text
before
after
all_properties
invalid_settings
property_paths
write_ref
transaction_id
journal_recorded
review
safety
diagnostics
```

读取工具允许返回 values，因为读取值是它的职责。

---

## 3. data.schema 短命名

使用：

```text
ReadDataAssetProperties.v1
SetDataAssetProperty.v1
```

不使用：

```text
BlueprintHelper.ReadDataAssetProperties.v1
BlueprintHelper.Tools.DataAsset.SetDataAssetProperty.v1
BlueprintHelper.MCP.SetDataAssetProperty.v1
```

---

# 4. read_data_asset_properties

## 4.1 工具定位

`read_data_asset_properties` 负责：

```text
读取一个 DataAsset 的指定属性或属性摘要。
```

它是只读工具。

不负责：

```text
修改属性
创建 DataAsset
修改 DataAsset class
修改 C++ struct/class
```

---

## 4.2 operation

```json
"operation": "read_data_asset_properties"
```

---

## 4.3 target

```json
"target": {
  "asset_path": "/Game/Data/DA_WeaponConfig",
  "read_scope": "data_asset_properties"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | DataAsset 资产路径。 |
| `ReadScope` | `EBlueprintHelperDataReadScope` | `target.read_scope` | `string enum` | 是 | 固定为 `data_asset_properties`。 |

---

## 4.4 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_data_asset_properties",
  "trace_id": "trace_20260503_2901",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "read_scope": "data_asset_properties"
  },

  "data": {
    "schema": "ReadDataAssetProperties.v1",
    "properties": {
      "asset_class": "/Script/Game.WeaponConfigDataAsset",
      "property_count": 3,
      "values": {
        "Damage": 25.0,
        "Cooldown": 0.35,
        "DisplayName": "Blaster"
      }
    }
  }
}
```

读取工具不返回 validation。

---

## 4.5 properties 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetClass` | `FString` | `data.properties.asset_class` | `string` | 是 | DataAsset class 路径。 |
| `PropertyCount` | `int32` | `data.properties.property_count` | `number` | 是 | 返回属性数量。 |
| `Values` | `TMap<FString, FJsonValue>` | `data.properties.values` | `object` | 是 | 属性值映射。 |

建议输入层支持：

```text
property_names[]
include_defaults
max_depth
```

默认不深展开复杂对象引用。

---

# 5. set_data_asset_property / set_data_asset_properties

## 5.1 工具定位

这组工具只负责：

```text
设置 DataAsset 的属性值。
```

不负责：

```text
创建 DataAsset
修改 C++ class
修改 Blueprint Graph
修改外部引用资产
```

---

## 5.2 operation

单属性：

```json
"operation": "set_data_asset_property"
```

批量属性：

```json
"operation": "set_data_asset_properties"
```

---

## 5.3 target

```json
"target": {
  "asset_path": "/Game/Data/DA_WeaponConfig",
  "data_scope": "data_asset_property"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | DataAsset 资产路径。 |
| `DataScope` | `EBlueprintHelperDataScope` | `target.data_scope` | `string enum` | 是 | 固定为 `data_asset_property`。 |

---

## 5.4 单属性成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_data_asset_property",
  "trace_id": "trace_20260503_3001",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "data_scope": "data_asset_property"
  },

  "data": {
    "schema": "SetDataAssetProperty.v1",
    "property_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 1,
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

## 5.5 批量成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_data_asset_properties",
  "trace_id": "trace_20260503_3002",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "data_scope": "data_asset_property"
  },

  "data": {
    "schema": "SetDataAssetProperty.v1",
    "property_result": {
      "mode": "batch",
      "requested_count": 3,
      "applied_count": 3,
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

---

## 5.6 no_op

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_data_asset_property",
  "trace_id": "trace_20260503_3004",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "data_scope": "data_asset_property"
  },

  "data": {
    "schema": "SetDataAssetProperty.v1",
    "property_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 1,
      "changed_count": 0,
      "no_op_count": 1
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

---

## 5.7 property_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Mode` | `EBlueprintHelperPropertyWriteMode` | `data.property_result.mode` | `string enum` | 是 | `single` 或 `batch`。 |
| `RequestedCount` | `int32` | `data.property_result.requested_count` | `number` | 是 | 请求设置数量。 |
| `AppliedCount` | `int32` | `data.property_result.applied_count` | `number` | 是 | 应用数量。 |
| `ChangedCount` | `int32` | `data.property_result.changed_count` | `number` | 是 | 实际变更数量。 |
| `NoOpCount` | `int32` | `data.property_result.no_op_count` | `number` | 是 | 无变化数量。 |

成功不返回：

```text
invalid_settings
before
after
all_properties
property_paths
write_ref
transaction_id
```

---

## 5.8 批量失败

批量 property 写默认事务化：

```text
只要存在 invalid 项，整批失败，不做部分应用。
```

失败不返回 property_result，只通过 error.conflicts 返回问题项。

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_data_asset_properties",
  "trace_id": "trace_20260503_3003",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "data_scope": "data_asset_property"
  },

  "error": {
    "code": "invalid_data_asset_property_settings",
    "stage": "validate_properties",
    "message": "One or more DataAsset property settings are invalid.",
    "retryable": false,
    "conflicts": [
      {
        "code": "property_not_found",
        "property": "DamageWrong"
      },
      {
        "code": "type_mismatch",
        "property": "Cooldown",
        "expected_type": "float"
      }
    ]
  }
}
```

---

# 6. validation

DataAsset 写工具成功通常返回：

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

# 7. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperDataAssetErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperDataAssetStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

---

# 8. UE 侧建议结构体

```cpp
struct FBlueprintHelperReadDataAssetPropertiesResultData
{
    FString Schema; // ReadDataAssetProperties.v1
    FBlueprintHelperDataAssetProperties Properties;
};

struct FBlueprintHelperDataAssetProperties
{
    FString AssetClass;
    int32 PropertyCount = 0;
    TMap<FString, TSharedPtr<FJsonValue>> Values;
};

struct FBlueprintHelperSetDataAssetPropertyResultData
{
    FString Schema; // SetDataAssetProperty.v1
    FBlueprintHelperPropertyWriteResult PropertyResult;
};

struct FBlueprintHelperPropertyWriteResult
{
    FString Mode; // single | batch
    int32 RequestedCount = 0;
    int32 AppliedCount = 0;
    int32 ChangedCount = 0;
    int32 NoOpCount = 0;
};
```

明确不包含：

```cpp
FBlueprintHelperWriteRef
FString TransactionId
TArray<FBlueprintHelperInvalidSetting> InvalidSettings
TMap<FString, FJsonValue> Before
TMap<FString, FJsonValue> After
```

---

# 9. 验收标准

```text
1. read_data_asset_properties 返回 values。
2. set_data_asset_property / set_data_asset_properties 成功只返回 property_result 计数。
3. 成功不返回 before / after。
4. 批量写 invalid 时整批失败。
5. 失败不返回 property_result。
6. 失败通过 error.conflicts 返回问题项。
7. 写工具不返回 write_ref / transaction_id / review / safety。
8. 写工具 validation 只返回 should_compile / should_save。
9. 写工具 validation 不返回 compiled / saved。
10. data.schema 使用短命名。
