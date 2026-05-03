# BlueprintHelper 第 5 簇：Blueprint Component UE 字段映射计划

日期：2026-05-02  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Blueprint Component 字段确认稿  
本文边界：确认 Blueprint Component 工具簇的 Agent 可见返回字段、UE 结构体映射、组件创建/挂接、单属性修改、批量属性修改、组件读取、组件删除的字段规则。Agent 使用规则见独立文档。

---

## 1. 工具簇定位

Blueprint Component 工具簇负责蓝图组件树的读取、创建、删除和属性修改。

第一版覆盖：

```text
read_components
add_component
set_component_property
set_component_properties
remove_component
```

其中：

```text
read_components            读
add_component              增：只创建组件 + 建立挂接关系
set_component_property     改：单个组件属性
set_component_properties   改：多个组件属性
remove_component           删：删除明确目标组件
```

---

## 2. 工具边界

Blueprint Component 负责：

```text
1. 读取蓝图组件树。
2. 添加组件。
3. 设置组件父子挂接关系。
4. 设置组件属性。
5. 删除明确目标组件。
```

Blueprint Component 不负责：

```text
1. 创建蓝图资产。属于 Asset Factory。
2. 修改蓝图图表节点。属于 Graph Write。
3. 添加 Implemented Interface。属于 Blueprint Class Settings。
4. 设置父类。属于 Blueprint Class Settings。
5. 编辑 Enhanced Input。属于 Enhanced Input。
6. 写 ConstructionScript 节点。属于 Graph Write 或后续专门设计。
7. 创建 BlueprintHelper-owned graph block。
```

---

## 3. ToolResultBase 约束

Blueprint Component 使用精简后的 Agent 可见 ToolResultBase。

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

## 4. Operation 列表

| operation | 工具语义 |
|---|---|
| `read_components` | 读取组件树。 |
| `add_component` | 添加组件并建立挂接关系。 |
| `set_component_property` | 设置单个组件属性。 |
| `set_component_properties` | 批量设置组件属性。 |
| `remove_component` | 删除明确目标组件。 |

---

## 5. data.schema

统一使用：

```json
"schema": "BlueprintComponent.v1"
```

不按操作拆分：

```text
BlueprintComponentProperty.v1
BlueprintComponents.v1
```

原因：

```text
1. 都属于同一工具簇。
2. operation 已经区分 add / set / remove / read。
3. 减少 schema 版本维护数量。
```

---

# 6. add_component 字段设计

## 6.1 职责

`add_component` 只负责：

```text
1. 创建组件。
2. 建立组件挂接关系。
3. 处理组件名冲突。
```

`add_component` 不负责：

```text
1. 设置 Transform。
2. 设置 Mobility。
3. 设置 Collision。
4. 设置 Physics。
5. 设置 Mesh。
6. 设置 Constraint 参数。
7. 设置任意组件属性。
```

这些都必须通过：

```text
set_component_property
set_component_properties
```

完成。

---

## 6.2 add_component 返回示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_component",
  "trace_id": "trace_20260502_0701",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "component",
    "component_name": "DoorMesh",
    "component_class": "StaticMeshComponent"
  },

  "data": {
    "schema": "BlueprintComponent.v1",

    "component": {
      "component_name": "DoorMesh",
      "component_class": "StaticMeshComponent",
      "created": true,
      "already_existed": false
    },

    "attachment": {
      "parent_component": "DefaultSceneRoot",
      "socket_name": null,
      "attach_rule": "keep_relative"
    },

    "name_collision": {
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

---

## 6.3 add_component data 字段

### component

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ComponentName` | `FString` | `data.component.component_name` | `string` | 是 | 组件名。 |
| `ComponentClass` | `FString` | `data.component.component_class` | `string` | 是 | 组件类短名。 |
| `bCreated` | `bool` | `data.component.created` | `boolean` | add 时 | 本次是否创建。 |
| `bAlreadyExisted` | `bool` | `data.component.already_existed` | `boolean` | add 时 | 执行前是否已存在。 |

### attachment

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ParentComponent` | `FString` | `data.attachment.parent_component` | `string/null` | 是 | 父组件名。 |
| `SocketName` | `FString` | `data.attachment.socket_name` | `string/null` | 否 | Socket 名。 |
| `AttachRule` | `EBlueprintHelperAttachRule` | `data.attachment.attach_rule` | `string enum` | 是 | 附着规则。 |

### name_collision

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Policy` | `EBlueprintHelperComponentNameCollisionPolicy` | `data.name_collision.policy` | `string enum` | 是 | 名称冲突策略。 |
| `bHandled` | `bool` | `data.name_collision.handled` | `boolean` | 是 | 是否发生并处理了冲突。 |
| `ExistingComponentName` | `FString` | `data.name_collision.existing_component_name` | `string` | 可选 | 已存在组件名。 |

不使用字段名 `collision`，避免和物理碰撞属性混淆。

---

# 7. set_component_property / set_component_properties 字段设计

## 7.1 职责

属性修改工具负责：

```text
1. 设置 Transform。
2. 设置 Mobility。
3. 设置 Collision。
4. 设置 Physics。
5. 设置 Mesh。
6. 设置 Constraint 参数。
7. 设置其他可写组件属性。
```

`set_component_property` 与 `set_component_properties` 在调用层区分：

```text
set_component_property      单个属性修改
set_component_properties    多个属性批量修改
```

返回结构统一使用：

```text
data.property_result
```

---

## 7.2 单属性返回示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_component_property",
  "trace_id": "trace_20260502_0702",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "component",
    "component_name": "DoorMesh",
    "property_path": "BodyInstance.bSimulatePhysics"
  },

  "data": {
    "schema": "BlueprintComponent.v1",

    "component": {
      "component_name": "DoorMesh",
      "component_class": "StaticMeshComponent"
    },

    "property_result": {
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

---

## 7.3 批量属性返回示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_component_properties",
  "trace_id": "trace_20260502_0703",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "component",
    "component_name": "DoorMesh"
  },

  "data": {
    "schema": "BlueprintComponent.v1",

    "component": {
      "component_name": "DoorMesh",
      "component_class": "StaticMeshComponent"
    },

    "property_result": {
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

---

## 7.4 无效设置返回示例

批量属性修改默认事务式。只要存在无效设置，默认不应用任何属性。

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_component_properties",
  "trace_id": "trace_20260502_0704",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "component",
    "component_name": "DoorMesh"
  },

  "error": {
    "code": "invalid_component_property_settings",
    "stage": "preflight",
    "message": "One or more component property settings are invalid.",
    "retryable": false,
    "rollback_result": "not_needed"
  },

  "data": {
    "schema": "BlueprintComponent.v1",

    "component": {
      "component_name": "DoorMesh",
      "component_class": "StaticMeshComponent"
    },

    "property_result": {
      "mode": "batch",
      "requested_count": 4,
      "applied_count": 0,
      "changed_count": 0,
      "no_op_count": 0,
      "invalid_settings": [
        {
          "property_path": "BodyInstance.bSimulatePhysics",
          "code": "property_not_writable",
          "expected_type": "bool"
        }
      ]
    }
  }
}
```

---

## 7.5 property_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Mode` | `EBlueprintHelperComponentPropertyMode` | `data.property_result.mode` | `string enum` | 是 | `single` 或 `batch`。 |
| `RequestedCount` | `int32` | `data.property_result.requested_count` | `number` | 是 | 请求设置数量。 |
| `AppliedCount` | `int32` | `data.property_result.applied_count` | `number` | 是 | 实际应用数量。 |
| `ChangedCount` | `int32` | `data.property_result.changed_count` | `number` | 是 | 实际产生变化的数量。 |
| `NoOpCount` | `int32` | `data.property_result.no_op_count` | `number` | 是 | 值相同或无需修改的数量。 |
| `InvalidSettings` | `TArray<FBlueprintHelperInvalidComponentPropertySetting>` | `data.property_result.invalid_settings` | `array<object>` | 是 | 无效设置列表。成功时为空数组。 |

---

## 7.6 invalid_settings 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `PropertyPath` | `FString` | `invalid_settings[].property_path` | `string` | 是 | 无效属性路径。 |
| `Code` | `EBlueprintHelperComponentPropertyInvalidCode` | `invalid_settings[].code` | `string enum` | 是 | 无效原因码。 |
| `ExpectedType` | `FString` | `invalid_settings[].expected_type` | `string` | 可选 | 期望类型。 |
| `ActualType` | `FString` | `invalid_settings[].actual_type` | `string` | 可选 | 实际类型。 |
| `ValueSummary` | `FString` | `invalid_settings[].value_summary` | `string` | 可选 | 错误定位所需的短摘要。不得回显大对象。 |

不返回：

```text
before
after
all_properties
```

原因：

```text
1. before / after 属于 UE 内部 diff / review / debug。
2. 成功时回显 property 会浪费 Token。
3. 大对象属性回显会污染上下文。
```

---

# 8. remove_component 字段设计

## 8.1 返回示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_component",
  "trace_id": "trace_20260502_0705",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "component",
    "component_name": "InteractionBox"
  },

  "data": {
    "schema": "BlueprintComponent.v1",

    "component": {
      "component_name": "InteractionBox",
      "component_class": "BoxComponent",
      "removed": true
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

## 8.2 remove_component 约束

第一版只允许删除明确目标组件：

```text
component_name 必须明确。
不允许模糊删除。
不允许按 class 批量删除。
```

---

# 9. read_components 字段设计

## 9.1 返回示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_components",
  "trace_id": "trace_20260502_0706",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },

  "data": {
    "schema": "BlueprintComponent.v1",

    "components": [
      {
        "component_name": "DefaultSceneRoot",
        "component_class": "SceneComponent",
        "parent_component": null,
        "children": [
          "DoorMesh",
          "InteractionBox",
          "DoorConstraint"
        ]
      },
      {
        "component_name": "DoorMesh",
        "component_class": "StaticMeshComponent",
        "parent_component": "DefaultSceneRoot",
        "children": []
      }
    ],

    "stats": {
      "components": 2,
      "root_components": 1
    }
  }
}
```

---

# 10. Enums

## 10.1 AttachRule

```text
keep_relative
snap_to_target
```

## 10.2 NameCollisionPolicy

```text
fail_if_exists
reuse_if_exists
```

不支持：

```text
auto_rename
replace_existing
```

## 10.3 ComponentPropertyMode

```text
single
batch
```

## 10.4 Invalid property code

```text
property_not_found
property_not_writable
type_mismatch
value_out_of_range
object_reference_not_found
enum_value_invalid
struct_field_invalid
component_not_found
component_type_mismatch
unsupported_property_type
```

---

# 11. component_class 表达规则

输入允许：

```text
StaticMeshComponent
/Script/Engine.StaticMeshComponent
```

返回默认使用短名：

```text
StaticMeshComponent
```

默认不返回：

```text
component_class_path
```

如果未来需要调试，可在 debug/verbose 模式增加。

---

# 12. validation 规则

| operation | should_compile | should_save |
|---|---:|---:|
| `add_component` | true | true |
| `set_component_property` | true | true |
| `set_component_properties` | true | true |
| `remove_component` | true | true |
| `read_components` | false | false |

当属性修改为 no_op 且未修改资产：

```text
status=no_op
modified=false
should_compile=false
should_save=false
```

---

# 13. 错误码

建议第一版支持：

```text
component_already_exists
component_not_found
component_type_mismatch
parent_component_not_found
invalid_attach_rule
invalid_component_property_settings
property_not_found
property_not_writable
type_mismatch
unsupported_component_class
remove_component_blocked
```

---

# 14. UE 服务建议

建议新增：

```cpp
FBlueprintHelperComponentService
```

职责：

```text
1. 读取组件树。
2. 添加组件。
3. 建立组件挂接关系。
4. 设置单个组件属性。
5. 批量设置组件属性。
6. 事务式校验属性设置。
7. 删除明确组件。
8. 生成 BlueprintComponent.v1 data。
9. 内部写 transaction / review / rollback_data。
10. 默认不向 Agent 返回 transaction / review / safety。
```

---

# 15. 验收标准

```text
1. add_component 只返回 component / attachment / name_collision。
2. add_component 不返回 transform / properties。
3. transform / collision / physics / mesh / constraint 参数全部通过 set_component_property 或 set_component_properties 设置。
4. set_component_property 与 set_component_properties 在工具调用层区分。
5. 两者返回统一 property_result 结构。
6. property_result 必须包含 mode / requested_count / applied_count / changed_count / no_op_count / invalid_settings。
7. 成功时不返回 before / after。
8. 成功时不回显所有 property。
9. 无效设置只返回 invalid_settings。
10. 批量属性修改默认事务式。
11. 第一版不支持 partial apply。
12. component_class 返回短名，输入允许短名或完整类路径。
13. name_collision 不命名为 collision。
14. 第一版不支持 auto_rename / replace_existing。
15. Blueprint Component 默认不返回 transaction / review / safety。
