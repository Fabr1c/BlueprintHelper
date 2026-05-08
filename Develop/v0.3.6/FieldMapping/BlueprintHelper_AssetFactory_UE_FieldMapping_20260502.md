# BlueprintHelper 第 4 簇：Asset Factory UE 字段映射计划

日期：2026-05-02  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Asset Factory 字段确认稿  
本文边界：确认 Asset Factory 工具簇的 Agent 可见返回字段、UE 结构体映射、枚举、collision policy、validation 规则。Agent 使用规则另见独立 Agent 侧规则文档。

---

## 1. 工具定位

Asset Factory 负责创建 UE 资产。

第一版建议覆盖：

```text
blueprint_class
blueprint_interface
structure
input_action
input_mapping_context
data_asset
data_table
widget_blueprint
```

第一版 P0 建议至少实现：

```text
blueprint_class
blueprint_interface
structure
input_action
input_mapping_context
```

Asset Factory 不负责：

```text
1. 添加蓝图组件。
2. 修改蓝图 Class Settings。
3. 添加 Implemented Interface。
4. 编辑 Input Mapping Context 的按键映射。
5. 写图表节点。
6. 创建 BlueprintHelper-owned block。
7. 替换已有资产。
8. 删除已有资产。
```

---

## 2. ToolResultBase 约束

Asset Factory 使用精简后的 Agent 可见 ToolResultBase。

不默认返回：

```text
transaction
review
safety
diagnostics
next
tool
command
request_id
```

使用：

```text
ok
schema
operation
trace_id
status
modified
target
data
validation
error
```

---

## 3. operation

统一使用：

```json
"operation": "create_asset"
```

不建议每类资产一个 operation，例如：

```text
create_blueprint_class
create_input_action
create_blueprint_interface
```

原因：

```text
1. 都属于同一个 Asset Factory 语义。
2. 资产类型由 data.factory.asset_type 区分。
3. MCP tool 可以拆分，operation 可以统一。
```

---

## 4. 成功返回示例

### 4.1 创建 Blueprint Class

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "create_asset",
  "trace_id": "trace_20260502_0601",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "asset",
    "asset_class": "Blueprint"
  },
  "data": {
    "schema": "AssetFactory.v1",
    "factory": {
      "asset_type": "blueprint_class",
      "factory_type": "blueprint",
      "parent_class": "/Script/Engine.Actor"
    },
    "asset": {
      "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
      "asset_class": "Blueprint",
      "created": true,
      "already_existed": false
    },
    "collision": {
      "policy": "fail_if_exists",
      "handled": false
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

### 4.2 创建 Input Action

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "create_asset",
  "trace_id": "trace_20260502_0602",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Input/IA_Interact",
    "target_type": "asset",
    "asset_class": "InputAction"
  },
  "data": {
    "schema": "AssetFactory.v1",
    "factory": {
      "asset_type": "input_action",
      "factory_type": "enhanced_input_action",
      "value_type": "boolean"
    },
    "asset": {
      "asset_path": "/Game/BlueprintHelperTest/Input/IA_Interact",
      "asset_class": "InputAction",
      "created": true,
      "already_existed": false
    },
    "collision": {
      "policy": "fail_if_exists",
      "handled": false
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

### 4.3 reuse_if_exists 命中已有同类型资产

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "create_asset",
  "trace_id": "trace_20260502_0603",
  "status": "no_op",
  "modified": false,
  "target": {
    "asset_path": "/Game/Input/IA_Interact",
    "target_type": "asset",
    "asset_class": "InputAction"
  },
  "data": {
    "schema": "AssetFactory.v1",
    "factory": {
      "asset_type": "input_action",
      "factory_type": "enhanced_input_action",
      "value_type": "boolean"
    },
    "asset": {
      "asset_path": "/Game/Input/IA_Interact",
      "asset_class": "InputAction",
      "created": false,
      "already_existed": true
    },
    "collision": {
      "policy": "reuse_if_exists",
      "handled": true,
      "existing_asset_path": "/Game/Input/IA_Interact"
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": false,
    "compiled": false,
    "saved": false
  }
}
```

---

## 5. UE 侧建议结构体

建议新增或整理：

```cpp
FBlueprintHelperAssetFactoryData
FBlueprintHelperAssetFactorySpec
FBlueprintHelperCreatedAssetSummary
FBlueprintHelperAssetCollisionSummary
```

顶层仍使用：

```cpp
FBlueprintHelperToolResultBase
```

其中 `Data` 指向：

```cpp
FBlueprintHelperAssetFactoryData
```

---

## 6. 字段映射表

### 6.1 FBlueprintHelperToolResultBase 到 MCP 返回体

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bOk` | `bool` | `ok` | `boolean` | 是 | 工具是否成功。 |
| `Schema` | `FString` | `schema` | `string` | 是 | 固定为 `BlueprintHelper.McpToolResult.v1`。 |
| `Operation` | `FString` 或 enum | `operation` | `string` | 是 | 固定为 `create_asset`。 |
| `TraceId` | `FString` | `trace_id` | `string` | 是 | 跨层追踪 ID。 |
| `Status` | `EBlueprintHelperToolStatus` | `status` | `string enum` | 是 | `applied / no_op / dry_run / failed`。 |
| `bModified` | `bool` | `modified` | `boolean` | 是 | 是否创建或修改资产。 |
| `Target` | `FBlueprintHelperTargetRef` | `target` | `object` | 是 | 待创建资产目标。 |
| `Data` | `FBlueprintHelperAssetFactoryData` | `data` | `object` | 成功 / no_op / dry_run | Asset Factory payload。 |
| `Validation` | `FBlueprintHelperValidationSummary` | `validation` | `object` | 成功 / no_op | compile/save 建议。 |
| `Error` | `FBlueprintHelperToolError` | `error` | `object` | 失败时 | 失败原因。 |
| `Transaction` | 内部使用 | 不返回 | - | 否 | UE 内部生成，不默认暴露。 |
| `Review` | 内部使用 | 不返回 | - | 否 | UE Review UI 内部使用。 |
| `Safety` | 不使用 | 不返回 | - | 否 | dry_run 信息进入 `data.dry_run`。 |

---

### 6.2 TargetRef

Asset Factory 的 target 指向待创建资产。

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | UE 内容路径。 |
| `TargetType` | `EBlueprintHelperTargetType` | `target.target_type` | `string enum` | 是 | 固定为 `asset`。 |
| `AssetClass` | `FString` | `target.asset_class` | `string` | 是 | 目标资产类。 |

不返回：

```text
asset_name
package_path
```

---

### 6.3 FBlueprintHelperAssetFactoryData

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Schema` | `FString` | `data.schema` | `string` | 是 | 固定为短名 `AssetFactory.v1`。 |
| `Factory` | `FBlueprintHelperAssetFactorySpec` | `data.factory` | `object` | 是 | 创建方式。 |
| `Asset` | `FBlueprintHelperCreatedAssetSummary` | `data.asset` | `object` | 是 | 创建结果。 |
| `Collision` | `FBlueprintHelperAssetCollisionSummary` | `data.collision` | `object` | 是 | 冲突处理结果。 |
| `DryRun` | `FBlueprintHelperAssetFactoryDryRun` | `data.dry_run` | `object` | dry_run 时 | dry_run 专属。 |

---

### 6.4 FBlueprintHelperAssetFactorySpec

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetType` | `EBlueprintHelperAssetType` | `data.factory.asset_type` | `string enum` | 是 | BlueprintHelper 语义资产类型。 |
| `FactoryType` | `EBlueprintHelperFactoryType` | `data.factory.factory_type` | `string enum` | 是 | UE 工厂 / 内部创建路径。 |
| `ParentClass` | `FString` | `data.factory.parent_class` | `string` | 条件 | Blueprint Class / Widget / DataAsset 需要。 |
| `ValueType` | `FString` 或 enum | `data.factory.value_type` | `string` | 条件 | Input Action 需要。 |
| `RowStruct` | `FString` | `data.factory.row_struct` | `string` | 条件 | DataTable 需要。 |
| `DataAssetClass` | `FString` | `data.factory.data_asset_class` | `string` | 条件 | DataAsset 需要。 |

---

### 6.5 FBlueprintHelperCreatedAssetSummary

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `data.asset.asset_path` | `string` | 是 | UE 内容路径。 |
| `AssetClass` | `FString` | `data.asset.asset_class` | `string` | 是 | UE asset class。 |
| `bCreated` | `bool` | `data.asset.created` | `boolean` | 是 | 本次是否创建。 |
| `bAlreadyExisted` | `bool` | `data.asset.already_existed` | `boolean` | 是 | 执行前是否已存在。 |

删除：

```text
AssetName
PackagePath
```

原因：

```text
asset_path 已可反推 asset_name 和 package_path。
```

---

### 6.6 FBlueprintHelperAssetCollisionSummary

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Policy` | `EBlueprintHelperAssetCollisionPolicy` | `data.collision.policy` | `string enum` | 是 | 冲突策略。 |
| `bHandled` | `bool` | `data.collision.handled` | `boolean` | 是 | 是否发生并处理了冲突。 |
| `ExistingAssetPath` | `FString` | `data.collision.existing_asset_path` | `string` | 可选 | 已存在资产路径。 |
| `FinalAssetPath` | 暂不使用 | 不返回 | - | 否 | 第一版不支持 auto_rename。 |

---

## 7. 枚举

### 7.1 asset_type

```text
blueprint_class
blueprint_interface
structure
input_action
input_mapping_context
data_asset
data_table
widget_blueprint
material
unknown
```

### 7.2 factory_type

```text
blueprint
blueprint_interface
structure
enhanced_input_action
enhanced_input_mapping_context
data_asset
data_table
widget_blueprint
native_factory
unknown
```

### 7.3 collision policy

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

---

## 8. validation 规则

| asset_type | should_compile | should_save |
|---|---:|---:|
| `blueprint_class` | true | true |
| `blueprint_interface` | true | true |
| `structure` | false | true |
| `input_action` | false | true |
| `input_mapping_context` | false | true |
| `data_asset` | false | true |
| `data_table` | false | true |
| `widget_blueprint` | true | true |
| `material` | false | true |

`reuse_if_exists` 命中同类型资产且 no_op 时：

```json
{
  "should_compile": false,
  "should_save": false
}
```

---

## 9. 错误返回

### 9.1 asset_already_exists

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "create_asset",
  "trace_id": "trace_20260502_0604",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/Input/IA_Interact",
    "target_type": "asset",
    "asset_class": "InputAction"
  },
  "error": {
    "code": "asset_already_exists",
    "stage": "preflight",
    "message": "Target asset already exists.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

### 9.2 asset_type_mismatch

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "create_asset",
  "trace_id": "trace_20260502_0605",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/Input/IA_Interact",
    "target_type": "asset",
    "asset_class": "InputAction"
  },
  "error": {
    "code": "asset_type_mismatch",
    "stage": "preflight",
    "message": "Existing asset type does not match requested asset type.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

---

## 10. UE 内部 transaction / review 规则

Agent 默认返回不包含 transaction / review。

UE 内部仍应：

```text
1. 正式创建资产时生成内部 transaction。
2. 保存 rollback_data。
3. 写入 Transaction Journal。
4. 进入 Review UI / Review Store。
```

`reuse_if_exists` 命中同类型资产且 `status=no_op` 时：

```text
1. 不生成 transaction。
2. 不进入 Review。
3. 不产生 rollback_data。
```

---

## 11. dry_run 规则

dry_run 返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "create_asset",
  "trace_id": "trace_20260502_0606",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/Input/IA_Interact",
    "target_type": "asset",
    "asset_class": "InputAction"
  },
  "data": {
    "schema": "AssetFactoryDryRun.v1",
    "dry_run": {
      "risk_level": "low",
      "required": false,
      "can_execute": true,
      "blocked_by": [],
      "warnings": [],
      "conflicts": [],
      "errors": []
    }
  }
}
```

---

## 12. UE 服务建议

建议新增：

```cpp
class FBlueprintHelperAssetFactoryService
```

职责：

```text
1. 校验 asset_path。
2. 校验 asset_type / factory_type。
3. 处理 collision policy。
4. 创建资产。
5. 生成 AssetFactory.v1 data。
6. 生成 validation。
7. 内部写 transaction / review / rollback_data。
8. 默认不向 Agent 返回 transaction / review。
```

---

## 13. 验收标准

```text
1. Asset Factory 返回使用 ToolResultBase。
2. operation 固定为 create_asset。
3. data.schema 固定为 AssetFactory.v1。
4. 不返回 transaction。
5. 不返回 review。
6. 不返回 safety。
7. 不返回 asset_name。
8. 不返回 package_path。
9. data.asset 必须包含 asset_path / asset_class / created / already_existed。
10. collision 第一版只支持 fail_if_exists / reuse_if_exists。
11. 不支持 auto_rename。
12. 不支持 replace_existing。
13. 创建成功 modified=true、status=applied。
14. reuse_if_exists 命中同类型资产时 modified=false、status=no_op。
15. 创建成功时 validation 根据 asset_type 返回 should_compile / should_save。
```
