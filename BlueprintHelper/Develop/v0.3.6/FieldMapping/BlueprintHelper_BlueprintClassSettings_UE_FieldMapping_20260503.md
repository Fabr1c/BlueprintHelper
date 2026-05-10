# BlueprintHelper 第 6 簇：Blueprint Class Settings UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Blueprint Class Settings 字段确认稿（移除 set_parent_class 修正版）  
本文边界：确认 Blueprint Class Settings 工具簇的 Agent 可见返回字段、UE 结构体映射、接口实现、Class Defaults 修改与 validation 规则。Agent 使用规则见独立文档。

---

## 1. 工具簇定位

Blueprint Class Settings 工具簇负责蓝图类级设置读取和修改。

第一版覆盖：

```text
read_class_settings
add_implemented_interface
add_implemented_interfaces
remove_implemented_interface
remove_implemented_interfaces
set_class_default_property
set_class_default_properties
```

---

## 2. 工具边界

Blueprint Class Settings 负责：

```text
1. 读取蓝图 Class Settings。
2. 添加 / 移除 Implemented Interface。
3. 设置 Class Defaults / CDO 默认属性。
4. 修改蓝图类级配置。

Parent Class 第一版只读：

```text
read_class_settings 可以返回 parent_class。
第一版不提供 set_parent_class，不修改 Blueprint Parent Class。
```
```

Blueprint Class Settings 不负责：

```text
1. 创建 Blueprint Interface 资产。属于 Asset Factory。
2. 创建接口函数实现图。属于 Graph Write 或后续 Interface Implementation 工具。
3. 在接口函数内写逻辑。属于 Graph Write。
4. 创建组件。属于 Blueprint Component。
5. 编辑 Input Mapping Context。属于 Enhanced Input。
6. 修改 C++ 源码。
7. 生成 EventGraph 执行流。
```

---

## 3. ToolResultBase 约束

Blueprint Class Settings 使用精简后的 Agent 可见 ToolResultBase。

默认不返回：

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

内部仍可生成 transaction / review / rollback_data，但默认不返回给 Agent。

---

## 4. data.schema

统一使用：

```json
"schema": "BlueprintClassSettings.v1"
```

不按操作拆分：

```text
BlueprintImplementedInterface.v1
BlueprintClassDefaultProperty.v1
```

原因：

```text
1. 都属于同一工具簇。
2. operation 已经区分 read / interface / default property。
3. 减少 schema 版本维护数量。
```

---

# 5. read_class_settings 字段设计

## 5.1 返回示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_class_settings",
  "trace_id": "trace_20260502_0801",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "class_settings": {
      "parent_class": "/Script/Engine.Actor",
      "generated_class": "BP_BH_PhysicsDoor_C",
      "implemented_interfaces": [
        "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
      ],
      "class_default_count": 12
    }
  }
}
```

## 5.2 class_settings 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ParentClass` | `FString` | `data.class_settings.parent_class` | `string` | 是 | 父类完整类路径，例如 `/Script/Engine.Actor`。 |
| `GeneratedClass` | `FString` | `data.class_settings.generated_class` | `string` | 是 | 生成类短名，例如 `BP_BH_PhysicsDoor_C`。不返回完整路径。 |
| `ImplementedInterfaces` | `TArray<FString>` | `data.class_settings.implemented_interfaces` | `array<string>` | 是 | 已实现接口资产路径。 |
| `ClassDefaultCount` | `int32` | `data.class_settings.class_default_count` | `number` | 是 | 可见 Class Default 摘要数量。 |

说明：

```text
1. generated_class 只返回短名，不返回完整路径。
2. parent_class 保留完整类路径，便于后续校验。
3. read_class_settings 不返回完整 Class Defaults 快照。
4. 需要读取具体默认属性时，应使用专门读取能力或带 filter 的读取参数，后续单独设计。
```

---

# 6. add_implemented_interface / add_implemented_interfaces 字段设计

## 6.1 单接口添加返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_implemented_interface",
  "trace_id": "trace_20260502_0802",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint",
    "interface_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "interface_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 1,
      "already_implemented_count": 0,
      "removed_count": 0,
      "invalid_interfaces": []
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

## 6.2 批量接口添加返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_implemented_interfaces",
  "trace_id": "trace_20260502_0803",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "interface_result": {
      "mode": "batch",
      "requested_count": 3,
      "applied_count": 2,
      "already_implemented_count": 1,
      "removed_count": 0,
      "invalid_interfaces": []
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

## 6.3 no_op：已经实现接口

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_implemented_interface",
  "trace_id": "trace_20260502_0804",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint",
    "interface_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "interface_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 0,
      "already_implemented_count": 1,
      "removed_count": 0,
      "invalid_interfaces": []
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

# 7. remove_implemented_interface / remove_implemented_interfaces 字段设计

## 7.1 单接口移除返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_implemented_interface",
  "trace_id": "trace_20260502_0805",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint",
    "interface_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "interface_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 0,
      "already_implemented_count": 0,
      "removed_count": 1,
      "invalid_interfaces": []
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

## 7.2 接口不存在时 no_op

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_implemented_interface",
  "trace_id": "trace_20260502_0806",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint",
    "interface_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "interface_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 0,
      "already_implemented_count": 0,
      "removed_count": 0,
      "invalid_interfaces": []
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

# 8. interface_result 字段映射

Interface 工具在调用层区分单个 / 多个，返回层统一使用 `interface_result`。

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Mode` | `EBlueprintHelperInterfaceOperationMode` | `data.interface_result.mode` | `string enum` | 是 | `single` 或 `batch`。 |
| `RequestedCount` | `int32` | `data.interface_result.requested_count` | `number` | 是 | 请求接口数量。 |
| `AppliedCount` | `int32` | `data.interface_result.applied_count` | `number` | 是 | 成功添加接口数量。 |
| `AlreadyImplementedCount` | `int32` | `data.interface_result.already_implemented_count` | `number` | 是 | 已实现接口数量。 |
| `RemovedCount` | `int32` | `data.interface_result.removed_count` | `number` | 是 | 成功移除接口数量。 |
| `InvalidInterfaces` | `TArray<FBlueprintHelperInvalidInterface>` | `data.interface_result.invalid_interfaces` | `array<object>` | 是 | 无效接口列表。成功时为空数组。 |

### invalid_interfaces 字段

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `InterfacePath` | `FString` | `invalid_interfaces[].interface_path` | `string` | 是 | 无效接口路径。 |
| `Code` | `EBlueprintHelperInvalidInterfaceCode` | `invalid_interfaces[].code` | `string enum` | 是 | 无效原因码。 |

错误码建议：

```text
interface_not_found
not_blueprint_interface
interface_asset_invalid
interface_load_failed
interface_compile_required
```

---

# 9. Interface 事务规则

批量 Interface 操作默认事务式：

```text
只要存在 invalid_interfaces，默认不应用任何接口修改。
```

出现无效接口时：

```text
ok=false
status=failed
modified=false
applied_count=0
removed_count=0
```

第一版不支持：

```text
partial apply
allow_partial=true
```

---

# 10. add_implemented_interface 行为边界

`add_implemented_interface` 只修改 Class Settings。

它不自动：

```text
1. 创建 BPI 资产。
2. 创建接口函数实现图。
3. 写接口函数 body。
4. 将接口函数接入 EventGraph。
```

如果添加接口后需要实现接口函数，应由 Graph Write 或后续 Interface Implementation 工具处理。

---


# 11. Parent Class 只读规则

第一版仅在 `read_class_settings` 中读取 Parent Class：

```text
parent_class = 当前蓝图父类的完整类路径，例如 /Script/Engine.Actor。
```

第一版不提供：

```text
set_parent_class
parent_class_result
requested_parent_class
before_parent_class
after_parent_class
confirmed_after_dry_run
```

如果任务要求修改蓝图父类，Agent 应 `stop_and_report`，说明当前 Blueprint Class Settings 第一版不支持修改 Parent Class。

# 12. Class Default 属性设置字段设计

## 12.1 单属性返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_class_default_property",
  "trace_id": "trace_20260502_0808",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint",
    "property_path": "OpenKickImpulse"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "default_property_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 1,
      "changed_count": 1,
      "no_op_count": 0,
      "invalid_settings": []
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

## 12.2 批量属性返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_class_default_properties",
  "trace_id": "trace_20260502_0809",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "default_property_result": {
      "mode": "batch",
      "requested_count": 4,
      "applied_count": 4,
      "changed_count": 3,
      "no_op_count": 1,
      "invalid_settings": []
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

## 12.3 无效默认属性返回

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_class_default_properties",
  "trace_id": "trace_20260502_0810",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BP_Door",
    "target_type": "blueprint"
  },

  "error": {
    "code": "invalid_class_default_property_settings",
    "stage": "preflight",
    "message": "One or more class default property settings are invalid.",
    "retryable": false,
    "rollback_result": "not_needed"
  },

  "data": {
    "schema": "BlueprintClassSettings.v1",

    "default_property_result": {
      "mode": "batch",
      "requested_count": 4,
      "applied_count": 0,
      "changed_count": 0,
      "no_op_count": 0,
      "invalid_settings": [
        {
          "property_path": "OpenKickImpulse",
          "code": "type_mismatch",
          "expected_type": "float",
          "actual_type": "string"
        }
      ]
    }
  }
}
```

---

# 13. default_property_result 字段映射

与 Blueprint Component 的 `property_result` 保持同构。

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Mode` | `EBlueprintHelperPropertyOperationMode` | `data.default_property_result.mode` | `string enum` | 是 | `single` 或 `batch`。 |
| `RequestedCount` | `int32` | `data.default_property_result.requested_count` | `number` | 是 | 请求设置数量。 |
| `AppliedCount` | `int32` | `data.default_property_result.applied_count` | `number` | 是 | 实际应用数量。 |
| `ChangedCount` | `int32` | `data.default_property_result.changed_count` | `number` | 是 | 实际产生变化数量。 |
| `NoOpCount` | `int32` | `data.default_property_result.no_op_count` | `number` | 是 | 值相同或无需修改数量。 |
| `InvalidSettings` | `TArray<FBlueprintHelperInvalidClassDefaultSetting>` | `data.default_property_result.invalid_settings` | `array<object>` | 是 | 无效设置列表。成功时为空数组。 |

### invalid_settings 字段

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `PropertyPath` | `FString` | `invalid_settings[].property_path` | `string` | 是 | 无效属性路径。 |
| `Code` | `EBlueprintHelperClassDefaultInvalidCode` | `invalid_settings[].code` | `string enum` | 是 | 无效原因码。 |
| `ExpectedType` | `FString` | `invalid_settings[].expected_type` | `string` | 可选 | 期望类型。 |
| `ActualType` | `FString` | `invalid_settings[].actual_type` | `string` | 可选 | 实际类型。 |
| `ValueSummary` | `FString` | `invalid_settings[].value_summary` | `string` | 可选 | 短摘要，不得回显大对象。 |

不返回：

```text
before
after
all_defaults
```

---

# 14. Class Default 事务规则

批量 Class Default 修改默认事务式：

```text
只要存在 invalid_settings，默认不应用任何默认属性修改。
```

出现无效设置时：

```text
ok=false
status=failed
modified=false
applied_count=0
changed_count=0
no_op_count=0
```

第一版不支持 partial apply。

---

# 15. validation 规则

| operation | should_compile | should_save |
|---|---:|---:|
| `read_class_settings` | false | false |
| `add_implemented_interface` | true | true |
| `add_implemented_interfaces` | true | true |
| `remove_implemented_interface` | true | true |
| `remove_implemented_interfaces` | true | true |
| `set_class_default_property` | true | true |
| `set_class_default_properties` | true | true |

no_op 且 modified=false 时：

```text
should_compile=false
should_save=false
```

---

# 16. 错误码

建议第一版支持：

```text
blueprint_not_found
not_a_blueprint
interface_not_found
not_blueprint_interface
invalid_blueprint_interface
interface_asset_invalid
interface_load_failed
interface_compile_required
interface_already_implemented
interface_not_implemented
class_default_property_not_found
class_default_property_not_writable
invalid_class_default_property_settings
type_mismatch
object_reference_not_found
enum_value_invalid
struct_field_invalid
unsupported_property_type
```

---

# 17. UE 服务建议

建议新增：

```cpp
FBlueprintHelperClassSettingsService
```

职责：

```text
1. 读取 Class Settings。
2. 添加单个 / 多个 Implemented Interface。
3. 移除单个 / 多个 Implemented Interface。
4. 事务式校验 Interface。
5. 设置单个 / 多个 Class Defaults。
6. 事务式校验 Class Defaults。
7. 生成 BlueprintClassSettings.v1 data。
8. 内部可写 transaction / review / rollback_data。
9. 默认不向 Agent 返回 transaction / review / safety。
```

---

# 18. 验收标准

```text
1. read_class_settings 返回 parent_class / generated_class / implemented_interfaces / class_default_count。
2. parent_class 只读，返回完整类路径。
3. 第一版不提供 set_parent_class，不返回 parent_class_result。
4. generated_class 只返回短名，不返回完整路径。
5. data.schema 固定为 BlueprintClassSettings.v1。
6. Interface 工具调用层区分单个 / 多个。
7. Interface 返回统一 interface_result。
8. Interface 无效时默认事务式，不应用任何接口。
9. add_implemented_interface 不自动创建 Blueprint Interface 资产。
10. add_implemented_interface 不自动创建接口函数实现图。
11. add_implemented_interface 不写接口函数 body。
12. Class Defaults 使用 default_property_result。
13. default_property_result 与 Component property_result 同构。
14. 成功时不回显所有默认属性。
15. 成功时不返回 before / after。
16. 无效项只返回 invalid_interfaces 或 invalid_settings。
17. 批量 Class Default 设置默认事务式。
18. 第一版不支持 partial apply。
19. 默认不返回 transaction / review / safety。
```
