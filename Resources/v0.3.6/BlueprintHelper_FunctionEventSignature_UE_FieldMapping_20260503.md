# BlueprintHelper Blueprint Function / Event Signature Management UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Blueprint Function / Event Signature Management 字段确认稿  
本文边界：确认蓝图 Function / Custom Event 的声明与签名层工具字段，包括读取函数/事件签名、创建函数/Custom Event、设置非签名属性、删除函数/Custom Event、dry_run、external_dependents 阻断、成功极简返回、错误详细返回、validation，以及与 Graph Write / Replace / Interface 的边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

```text
1. Function / Event Signature Management 只处理声明和签名层，不处理函数体 / 事件体逻辑。
2. 函数体 / 事件体逻辑仍由 Graph Write / Replace function_body / event_body 处理。
3. add_blueprint_function 只创建函数声明和入口，不写函数体。
4. add_blueprint_custom_event 只创建 Custom Event 声明和入口，不接入执行流。
5. 单个 add / set / remove 成功只返回 success=true。
6. 单个 no_op 通过 status=no_op 表达，不返回 reused_existing。
7. name_collision 只支持 fail_if_exists / reuse_if_exists。
8. 不支持 auto_rename / replace_existing。
9. 第一版不支持 rename function / event。
10. 第一版不支持 change existing function/event signature。
11. 第一版不支持 add/remove/reorder existing parameters。
12. set_blueprint_function_properties 只改非签名属性。
13. set_blueprint_event_properties 只改非签名属性。
14. remove_blueprint_function 必须 dry_run。
15. remove_blueprint_custom_event 必须 dry_run。
16. remove function 有 external_dependents 时 blocked。
17. remove custom event 只允许 custom_event，不允许 engine_event / interface_event。
18. read_blueprint_functions 不返回函数体 graph。
19. read_blueprint_events 不返回事件体 graph。
20. 写工具 validation 只返回 should_compile / should_save。
21. 写工具成功不返回 write_ref / transaction_id / review / safety。
22. data.schema 使用短命名。
23. 本簇所有成功写操作只返回 success=true。
24. 成功不返回计数、guid、entry_node_ref、graph_id、reused_existing。
25. no_op 通过 status=no_op 表达，data 中仍只返回 success=true。
26. 失败 / dry_run blocked 返回详细 error 或 conflicts。
```

---

## 1. 工具簇边界

本簇只处理：

```text
Function / Event 的声明与签名：
- 函数名 / 事件名
- 参数列表
- 返回值列表
- 函数 Flags / Metadata
- Category / Tooltip / Access
```

不处理：

```text
函数体逻辑
事件体逻辑
Graph 节点实现
接口函数体实现
调用点重连
自动迁移外部调用节点
```

这些仍归属：

```text
Graph Write
ReplaceBlueprintGraph
PatchBlueprintGraph
MergeBlueprintGraph
```

---

## 2. 第一版覆盖范围

第一版覆盖：

```text
read_blueprint_functions
read_blueprint_events

add_blueprint_function
add_blueprint_custom_event

set_blueprint_function_properties
set_blueprint_event_properties

remove_blueprint_function
remove_blueprint_custom_event
```

第一版不支持：

```text
rename_blueprint_function
rename_blueprint_event
change_function_parameter_type
change_event_parameter_type
add_parameter_to_existing_function
remove_parameter_from_existing_function
reorder_function_parameters
change_return_value_type
change_existing_function_signature
change_existing_event_signature
```

原因：

```text
修改已有签名会影响所有调用点，可能导致外部 Blueprint 编译错误。
这类操作后置为高风险 signature migration 工具，必须依赖 external_dependents 分析和调用点迁移策略。
```

---

## 3. 通用成功 / no_op / 错误返回原则

### 3.1 成功写操作

本簇所有成功写操作只返回：

```json
{
  "success": true
}
```

不返回：

```text
requested_count
added_count
removed_count
changed_count
no_op_count
reused_existing
function_guid
event_guid
entry_node_ref
graph_id
transaction_id
write_ref
review
safety
```

`status` 表达业务状态：

```text
applied = 已应用
no_op = 已存在或无需修改
dry_run = 预检
failed = 工具自身失败
```

### 3.2 no_op

no_op 也只返回：

```json
{
  "success": true
}
```

并通过：

```json
"status": "no_op",
"modified": false
```

表达“没有实际修改”。

### 3.3 失败 / blocked

失败返回详细定位信息：

```text
error.code
error.stage
error.message
error.retryable
error.conflicts
```

dry_run blocked 返回：

```text
dry_run.blocked_by
dry_run.conflicts
dry_run.errors
```

---

## 4. signature 参数表达

参数结构：

```json
{
  "name": "TargetActor",
  "direction": "input",
  "type": {
    "category": "object",
    "subtype": "/Script/Engine.Actor",
    "container": "single"
  },
  "default_value": null,
  "pass_by_reference": false
}
```

函数返回值：

```json
{
  "name": "Success",
  "direction": "output",
  "type": {
    "category": "bool",
    "container": "single"
  }
}
```

复用变量类型规则：

```text
bool
int
float
name
string
text
vector
rotator
transform
object
class
struct
enum
```

容器：

```text
single
array
set
map
```

第一版建议：

```text
1. 支持 primitive / object / class / struct / enum / array。
2. set / map 可先只读或后置。
3. 不支持修改已有 parameter type。
```

---

# 5. read_blueprint_functions

## 5.1 工具定位

读取蓝图函数声明 / 签名摘要。

不读取：

```text
完整函数体 Graph
函数内部节点
Local Variables 详情
外部调用点列表
```

函数体逻辑应通过：

```text
ReadBlueprintLogicMdByTarget
ReadBlueprintLogicJsonByTarget
```

读取。

---

## 5.2 operation

```json
"operation": "read_blueprint_functions"
```

---

## 5.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_functions",
  "trace_id": "trace_20260503_6601",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "function_signatures"
  },

  "data": {
    "schema": "ReadBlueprintFunctions.v1",
    "functions": {
      "function_count": 2,
      "items": [
        {
          "function_name": "TogglePhysicsDoor",
          "category": "Door",
          "access": "public",
          "pure": false,
          "callable": true,
          "inputs": [
            {
              "name": "bOpen",
              "type": {
                "category": "bool",
                "container": "single"
              }
            }
          ],
          "outputs": [
            {
              "name": "Success",
              "type": {
                "category": "bool",
                "container": "single"
              }
            }
          ]
        },
        {
          "function_name": "GetDoorOpenAngle",
          "category": "Door",
          "access": "public",
          "pure": true,
          "callable": true,
          "inputs": [],
          "outputs": [
            {
              "name": "Angle",
              "type": {
                "category": "float",
                "container": "single"
              }
            }
          ]
        }
      ]
    }
  }
}
```

成功不返回：

```text
function_guid
graph snapshot
function body
node list
external_dependents
```

---

## 5.4 functions 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `FunctionCount` | `int32` | `data.functions.function_count` | `number` | 是 | 函数数量。 |
| `Items` | `TArray<FBlueprintHelperFunctionSignatureSummary>` | `data.functions.items` | `array<object>` | 是 | 函数签名摘要。 |

`items[]` 字段：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `FunctionName` | `FString` | `function_name` | `string` | 是 | 函数名。 |
| `Category` | `FString` | `category` | `string` | 可选 | Category。 |
| `Access` | `FString` 或 enum | `access` | `string` | 可选 | `public` / `protected` / `private`。 |
| `bPure` | `bool` | `pure` | `boolean` | 可选 | 是否纯函数。 |
| `bCallable` | `bool` | `callable` | `boolean` | 可选 | 是否 Blueprint callable。 |
| `Inputs` | `TArray<FBlueprintHelperSignatureParam>` | `inputs` | `array<object>` | 是 | 输入参数。 |
| `Outputs` | `TArray<FBlueprintHelperSignatureParam>` | `outputs` | `array<object>` | 是 | 输出参数。 |

---

# 6. read_blueprint_events

## 6.1 工具定位

读取蓝图事件声明 / 自定义事件签名摘要。

第一版区分：

```text
custom_event
override_event
engine_event
interface_event
```

但只把 `custom_event` 作为可新增 / 可删除目标。

全局 / 引擎事件：

```text
BeginPlay
Tick
ConstructionScript
ActorBeginOverlap
Hit
InputAction
```

不应通过普通 add event 重复创建。

---

## 6.2 operation

```json
"operation": "read_blueprint_events"
```

---

## 6.3 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_events",
  "trace_id": "trace_20260503_6602",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "event_signatures"
  },

  "data": {
    "schema": "ReadBlueprintEvents.v1",
    "events": {
      "event_count": 2,
      "items": [
        {
          "event_name": "ShotBullet",
          "event_kind": "custom_event",
          "inputs": [
            {
              "name": "MuzzleLocation",
              "type": {
                "category": "vector",
                "container": "single"
              }
            }
          ]
        },
        {
          "event_name": "BeginPlay",
          "event_kind": "engine_event",
          "inputs": []
        }
      ]
    }
  }
}
```

---

## 6.4 events 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `EventCount` | `int32` | `data.events.event_count` | `number` | 是 | 事件数量。 |
| `Items` | `TArray<FBlueprintHelperEventSignatureSummary>` | `data.events.items` | `array<object>` | 是 | 事件签名摘要。 |

`items[]` 字段：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `EventName` | `FString` | `event_name` | `string` | 是 | 事件名。 |
| `EventKind` | `FString` 或 enum | `event_kind` | `string` | 是 | `custom_event` / `engine_event` / `interface_event` 等。 |
| `Inputs` | `TArray<FBlueprintHelperSignatureParam>` | `inputs` | `array<object>` | 是 | 输入参数。 |

---

# 7. add_blueprint_function

## 7.1 工具定位

只创建函数声明和函数入口。

不负责：

```text
写函数体节点
创建 Local Variables
接入 EventGraph
创建外部调用点
替换同名函数
自动重命名
```

函数体应后续由：

```text
ReplaceBlueprintGraph with replace_scope=function_body
Append / Patch / Merge according to target
```

实现。

---

## 7.2 operation

```json
"operation": "add_blueprint_function"
```

---

## 7.3 单函数成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_function",
  "trace_id": "trace_20260503_6701",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "data": {
    "schema": "AddBlueprintFunction.v1",
    "function_result": {
      "success": true
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

## 7.4 name_collision：reuse_if_exists no_op

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_function",
  "trace_id": "trace_20260503_6702",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "data": {
    "schema": "AddBlueprintFunction.v1",
    "function_result": {
      "success": true
    }
  },

  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

不返回 `reused_existing`，由 `status=no_op` 表达。

---

## 7.5 name_collision：fail_if_exists 失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_function",
  "trace_id": "trace_20260503_6703",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "error": {
    "code": "function_already_exists",
    "stage": "name_collision_check",
    "message": "A function with the requested name already exists.",
    "retryable": false,
    "conflicts": [
      {
        "code": "function_already_exists",
        "function_name": "TogglePhysicsDoor"
      }
    ]
  }
}
```

---

# 8. add_blueprint_custom_event

## 8.1 工具定位

只创建 Custom Event 声明和入口节点。

不负责：

```text
写事件体逻辑
接入已有执行流
替换已有 Custom Event
创建全局 Engine Event
创建 InputAction Event
重复创建 BeginPlay / Tick
```

接入已有执行流应使用：

```text
MergeBlueprintGraph
```

事件体实现应使用：

```text
ReplaceBlueprintGraph with replace_scope=event_body
```

或 Graph Write 相关工具。

---

## 8.2 operation

```json
"operation": "add_blueprint_custom_event"
```

---

## 8.3 单 Custom Event 成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_custom_event",
  "trace_id": "trace_20260503_6801",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "custom_event"
  },

  "data": {
    "schema": "AddBlueprintCustomEvent.v1",
    "event_result": {
      "success": true
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

# 9. set_blueprint_function_properties

## 9.1 工具定位

修改函数非签名属性。

第一版支持：

```text
category
tooltip
access
pure
callable
const
deprecated
deprecation_message
```

需要谨慎的字段：

```text
pure
const
callable
```

因为可能影响 Blueprint 可调用性或节点行为。

第一版不支持：

```text
function_name
input parameter list
output parameter list
parameter type
parameter order
```

---

## 9.2 operation

```json
"operation": "set_blueprint_function_properties"
```

---

## 9.3 单函数属性修改成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_function_properties",
  "trace_id": "trace_20260503_6901",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "data": {
    "schema": "SetBlueprintFunctionProperties.v1",
    "property_result": {
      "success": true
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

## 9.4 修改签名字段失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_function_properties",
  "trace_id": "trace_20260503_6902",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "error": {
    "code": "unsupported_signature_mutation",
    "stage": "validate_properties",
    "message": "Changing an existing function signature is not supported in the first version.",
    "retryable": false,
    "conflicts": [
      {
        "code": "unsupported_property",
        "property": "inputs"
      }
    ]
  }
}
```

---

# 10. set_blueprint_event_properties

## 10.1 工具定位

修改 Custom Event 的非签名属性。

第一版支持：

```text
category
tooltip
deprecated
deprecation_message
```

不支持：

```text
event_name
input parameter list
parameter type
engine event mutation
```

---

## 10.2 operation

```json
"operation": "set_blueprint_event_properties"
```

---

## 10.3 单事件属性修改成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_event_properties",
  "trace_id": "trace_20260503_7001",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "custom_event"
  },

  "data": {
    "schema": "SetBlueprintEventProperties.v1",
    "property_result": {
      "success": true
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

# 11. remove_blueprint_function

## 11.1 工具定位

删除明确函数声明和函数图。

这是破坏性操作，必须 dry_run。

删除前必须检查：

```text
external_dependents
内部调用点
接口实现关系
override / inherited function 关系
latent use / delegate binding
```

第一版建议：

```text
1. 只允许删除 Blueprint 自己声明的普通函数。
2. 不允许删除 inherited function。
3. 不允许删除 interface implementation function；应通过 Interface workflow 处理。
4. 有 external_dependents 时 blocked。
```

---

## 11.2 operation

```json
"operation": "remove_blueprint_function"
```

---

## 11.3 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_function",
  "trace_id": "trace_20260503_7101",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "data": {
    "schema": "RemoveBlueprintFunctionDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

---

## 11.4 dry_run blocked：external_dependents

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_function",
  "trace_id": "trace_20260503_7102",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "data": {
    "schema": "RemoveBlueprintFunctionDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "external_dependents_exist"
      ],
      "conflicts": [
        {
          "code": "external_dependents_exist",
          "function_name": "TogglePhysicsDoor",
          "external_dependent_count": 2,
          "message": "The function is called by external assets."
        }
      ],
      "errors": []
    }
  }
}
```

---

## 11.5 正式成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_function",
  "trace_id": "trace_20260503_7103",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },

  "data": {
    "schema": "RemoveBlueprintFunction.v1",
    "function_result": {
      "success": true
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

# 12. remove_blueprint_custom_event

## 12.1 工具定位

删除明确 Custom Event 声明和事件体。

必须 dry_run。

第一版建议：

```text
1. 只允许删除 custom_event。
2. 不允许删除 engine_event。
3. 不允许删除 interface_event。
4. 有 graph references / external dependents 时 blocked。
```

---

## 12.2 operation

```json
"operation": "remove_blueprint_custom_event"
```

---

## 12.3 dry_run blocked：不是 custom_event

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_custom_event",
  "trace_id": "trace_20260503_7201",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "custom_event"
  },

  "data": {
    "schema": "RemoveBlueprintCustomEventDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "event_kind_not_removable"
      ],
      "conflicts": [
        {
          "code": "event_kind_not_removable",
          "event_name": "BeginPlay",
          "event_kind": "engine_event",
          "message": "Only custom events can be removed by this tool."
        }
      ],
      "errors": []
    }
  }
}
```

---

## 12.4 正式成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_custom_event",
  "trace_id": "trace_20260503_7202",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "custom_event"
  },

  "data": {
    "schema": "RemoveBlueprintCustomEvent.v1",
    "event_result": {
      "success": true
    }
  },

  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

# 13. validation

写工具成功 / no_op 返回：

```json
"validation": {
  "should_compile": true,
  "should_save": true
}
```

或 no_op：

```json
"validation": {
  "should_compile": false,
  "should_save": false
}
```

不返回：

```text
compiled
saved
```

---

# 14. 与 ReplaceBlueprintGraph 的关系

已有 Replace 规则保持：

```text
function_definition / event_definition：
- 替换完整定义。
- 若有 external_dependents，默认阻止并报告。

function_body / event_body：
- 只替换内部实现。
- 保留入口、签名、参数、返回值、调用身份稳定。
- 不因 external_dependents 直接阻止，但 dry_run 必须报告。
```

本簇和 Replace 的分工：

```text
Signature Management：
- 创建 / 读取 / 删除函数或事件声明。
- 设置非签名 metadata/properties。

ReplaceBlueprintGraph：
- 替换函数体 / 事件体 / 定义级实现。
- 处理图节点和执行流。
```

---

# 15. 与 Blueprint Interface 的关系

Interface 函数签名属于：

```text
Blueprint Interface Asset Editing
```

本簇不负责：

```text
编辑 BPI 资产函数签名
添加接口函数到 BPI
删除接口函数
修改接口函数参数
```

Blueprint 实现接口函数体也不属于本簇：

```text
Graph Write / Replace function_body
```

---

# 16. UE/MCP 建议结构体

```cpp
struct FBlueprintHelperSignatureParam
{
    FString Name;
    FString Direction; // input | output
    FBlueprintHelperVariableType Type;
    TSharedPtr<FJsonValue> DefaultValue;
    bool bPassByReference = false;
};

struct FBlueprintHelperFunctionSignatureSummary
{
    FString FunctionName;
    FString Category;
    FString Access; // public | protected | private
    bool bPure = false;
    bool bCallable = true;
    TArray<FBlueprintHelperSignatureParam> Inputs;
    TArray<FBlueprintHelperSignatureParam> Outputs;
};

struct FBlueprintHelperEventSignatureSummary
{
    FString EventName;
    FString EventKind; // custom_event | engine_event | interface_event | override_event
    TArray<FBlueprintHelperSignatureParam> Inputs;
};

struct FBlueprintHelperReadBlueprintFunctionsResultData
{
    FString Schema; // ReadBlueprintFunctions.v1
    FBlueprintHelperFunctionSignatureList Functions;
};

struct FBlueprintHelperReadBlueprintEventsResultData
{
    FString Schema; // ReadBlueprintEvents.v1
    FBlueprintHelperEventSignatureList Events;
};

struct FBlueprintHelperFunctionSignatureList
{
    int32 FunctionCount = 0;
    TArray<FBlueprintHelperFunctionSignatureSummary> Items;
};

struct FBlueprintHelperEventSignatureList
{
    int32 EventCount = 0;
    TArray<FBlueprintHelperEventSignatureSummary> Items;
};

struct FBlueprintHelperSingleSuccessResult
{
    bool bSuccess = true;
};
```

明确不包含：

```cpp
FString FunctionGuid;
FString EventGuid;
FString EntryNodeRef;
FString GraphId;
FString TransactionId;
FString WriteRef;
FString ReviewStatus;
FString SafetyProfile;
```

---

# 17. 验收标准

```text
1. Signature Management 只处理声明 / 签名层。
2. add_blueprint_function 不写函数体。
3. add_blueprint_custom_event 不接入执行流。
4. 所有成功写操作只返回 success=true。
5. no_op 通过 status=no_op 表达。
6. 成功不返回 guid / entry_node_ref / graph_id / reused_existing。
7. 失败返回详细 error / conflicts。
8. dry_run blocked 返回 blocked_by / conflicts。
9. remove_blueprint_function 必须 dry_run。
10. remove_blueprint_custom_event 必须 dry_run。
11. external_dependents 阻断 function 删除。
12. custom event 删除不允许 engine_event / interface_event。
13. 第一版不支持 rename / signature mutation。
14. read_blueprint_functions 不返回函数体 graph。
15. read_blueprint_events 不返回事件体 graph。
16. validation 只返回 should_compile / should_save。
17. 成功不返回 write_ref / transaction_id / review / safety。
18. data.schema 使用短命名。
