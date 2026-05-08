# BlueprintHelper Blueprint Variables / Defaults / Local Variables UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Blueprint Member Variables / Defaults / Local Variables 字段确认稿  
本文边界：确认蓝图成员变量声明、成员变量默认值、函数 Local Variables 的 Agent-facing 返回字段、UE/MCP 侧结构体映射、单个/批量操作结果、dry_run、事务化批量写入、validation 以及与 Graph Write / Class Defaults 的边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

```text
1. 成员变量声明、成员变量默认值、Local Variables 分工具。
2. add_blueprint_member_variable 只创建变量声明，不设置默认值。
3. 默认值通过 set_blueprint_member_default / set_blueprint_member_defaults 设置。
4. read_blueprint_member_variables 不默认返回默认值。
5. read_blueprint_member_defaults 只读取请求的成员变量默认值。
6. 成员变量 name_collision 只支持 fail_if_exists / reuse_if_exists。
7. 不支持 auto_rename / replace_existing。
8. 第一版不支持 rename member variable。
9. 第一版不支持 change member variable type。
10. set_blueprint_member_variable_properties 不允许修改 variable_name / variable_type。
11. remove_blueprint_member_variable 必须 dry_run。
12. remove member variable 有 Graph 引用时 blocked。
13. Local Variable 必须指定 function_name。
14. Local Variable 不支持 instance_editable / expose_on_spawn / replication / class default。
15. remove_blueprint_local_variable 必须 dry_run。
16. remove local variable 有函数图引用时 blocked。
17. 批量写默认事务化，任一 invalid 整批失败。
18. 写工具 validation 只返回 should_compile / should_save。
19. 写工具成功不返回 write_ref / transaction_id / review / safety。
20. data.schema 使用短命名。
21. 单个变量 add / set / remove 成功只返回 success=true。
22. 单个变量操作不返回 mode / requested_count / added_count / removed_count / changed_count / no_op_count。
23. 单个 no_op 通过 status=no_op 表达，不额外返回 reused_existing。
24. 批量变量操作可保留 requested_count / added_count / removed_count / changed_count / no_op_count 等计数。
25. 批量变量操作不需要 mode=batch，因为 operation 名已经区分。
```

---

## 1. 工具簇边界

本簇拆成三层：

```text
1. Member Variable Declaration
   蓝图成员变量声明：名称、类型、Flags、Category、Tooltip、Expose on Spawn、Instance Editable 等。

2. Blueprint Member Defaults
   蓝图成员变量默认值：Class Defaults 中对应成员变量的默认值。

3. Local Variables
   函数内部局部变量：只存在于明确函数作用域内。
```

第一版覆盖：

```text
read_blueprint_member_variables
add_blueprint_member_variable
add_blueprint_member_variables
set_blueprint_member_variable_properties
set_blueprint_member_variables_properties
remove_blueprint_member_variable
remove_blueprint_member_variables

read_blueprint_member_defaults
set_blueprint_member_default
set_blueprint_member_defaults

read_blueprint_local_variables
add_blueprint_local_variable
add_blueprint_local_variables
set_blueprint_local_variable_properties
set_blueprint_local_variables_properties
remove_blueprint_local_variable
remove_blueprint_local_variables
```

第一版不覆盖：

```text
rename_blueprint_member_variable
change_blueprint_member_variable_type
rename_blueprint_local_variable
change_blueprint_local_variable_type
自动替换所有 Graph 引用
自动迁移默认值
自动修复变量节点
```

原因：

```text
重命名 / 改类型会触发大范围 Graph 引用迁移和编译风险，应后置或走专门高风险工具。
```

---

## 2. 通用返回原则

### 2.1 单个变量操作成功

单个变量 add / set / remove 成功只返回：

```json
{
  "success": true
}
```

不返回：

```text
mode
requested_count
added_count
removed_count
changed_count
no_op_count
reused_existing
variable_guid
before
after
all_variables
write_ref
transaction_id
review
safety
```

`status` 负责表达业务状态：

```text
applied = 已应用变更
no_op = 已满足，无需修改
dry_run = 预检
failed = 工具失败
```

### 2.2 批量变量操作成功

批量操作可返回计数：

```json
{
  "requested_count": 3,
  "added_count": 3,
  "no_op_count": 0
}
```

批量不需要：

```json
"mode": "batch"
```

因为 operation 名已经区分单个 / 批量。

### 2.3 批量事务化

批量写默认事务化：

```text
任一 invalid 项，整批失败，不做部分应用。
```

失败通过 `error.conflicts` 返回问题项。

---

## 3. variable_type 表达

变量类型建议统一用 `variable_type`，不直接暴露 UE 内部 `FEdGraphPinType` 全量字段。

```json
"variable_type": {
  "category": "object",
  "subtype": "/Script/Engine.StaticMeshComponent",
  "container": "single"
}
```

常见 category：

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

Map 示例：

```json
"variable_type": {
  "category": "map",
  "key_type": {
    "category": "name"
  },
  "value_type": {
    "category": "float"
  }
}
```

第一版建议：

```text
1. 支持常见 primitive / object / class / struct / enum / array。
2. set / map 可后置，或先只读不写。
3. 类型变更不支持，避免变量节点和默认值迁移风险。
```

---

## 4. Member Variable Declaration 工具

### 4.1 read_blueprint_member_variables

读取蓝图成员变量声明摘要。

不读取：

```text
完整 Class Defaults
所有变量默认值
Graph 变量节点引用详情
Local Variables
```

成功返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_member_variables",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "member_variables"
  },
  "data": {
    "schema": "ReadBlueprintMemberVariables.v1",
    "member_variables": {
      "variable_count": 2,
      "variables": [
        {
          "variable_name": "DoorMesh",
          "variable_type": {
            "category": "object",
            "subtype": "/Script/Engine.StaticMeshComponent",
            "container": "single"
          },
          "category": "Components",
          "instance_editable": false,
          "expose_on_spawn": false
        },
        {
          "variable_name": "OpenAngle",
          "variable_type": {
            "category": "float",
            "container": "single"
          },
          "category": "Door",
          "instance_editable": true,
          "expose_on_spawn": false
        }
      ]
    }
  }
}
```

### 4.2 add_blueprint_member_variable

只创建成员变量声明。

不负责：

```text
设置默认值
创建 Getter / Setter 节点
写 Graph 逻辑
替换已有变量
自动重命名
```

单变量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_member_variable",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "AddBlueprintMemberVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

单变量 no_op：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_member_variable",
  "status": "no_op",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "AddBlueprintMemberVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

不返回 `reused_existing`，因为 `status=no_op` 已表达。

批量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_member_variables",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "AddBlueprintMemberVariable.v1",
    "variable_result": {
      "requested_count": 3,
      "added_count": 3,
      "no_op_count": 0
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

### 4.3 name_collision

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
  "operation": "add_blueprint_member_variable",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "error": {
    "code": "variable_already_exists",
    "stage": "name_collision_check",
    "message": "A member variable with the requested name already exists.",
    "retryable": false,
    "conflicts": [
      {
        "code": "variable_already_exists",
        "variable_name": "OpenAngle"
      }
    ]
  }
}
```

### 4.4 set_blueprint_member_variable_properties

修改成员变量的声明属性。

不修改：

```text
variable_name
variable_type
default_value
```

第一版支持：

```text
category
tooltip
instance_editable
expose_on_spawn
private
blueprint_read_only
```

单变量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_member_variable_properties",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "SetBlueprintMemberVariableProperties.v1",
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

批量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_member_variables_properties",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "SetBlueprintMemberVariableProperties.v1",
    "property_result": {
      "requested_count": 3,
      "applied_count": 3,
      "changed_count": 2,
      "no_op_count": 1
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

### 4.5 remove_blueprint_member_variable

删除明确成员变量声明。破坏性操作，必须 dry_run。

删除前需要检查：

```text
Graph 变量节点引用
默认值依赖
函数 / 宏 / 事件图引用
外部 Blueprint 调用或属性访问
```

dry_run passed：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_member_variable",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "RemoveBlueprintMemberVariableDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

dry_run blocked：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_member_variable",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "RemoveBlueprintMemberVariableDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "variable_references_exist"
      ],
      "conflicts": [
        {
          "code": "variable_references_exist",
          "variable_name": "OpenAngle",
          "reference_count": 4,
          "message": "The variable is referenced by Blueprint graph nodes."
        }
      ],
      "errors": []
    }
  }
}
```

单变量正式成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_member_variable",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "RemoveBlueprintMemberVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

批量正式成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_member_variables",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "RemoveBlueprintMemberVariable.v1",
    "variable_result": {
      "requested_count": 2,
      "removed_count": 2
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

## 5. Member Defaults 工具

### 5.1 read_blueprint_member_defaults

读取成员变量默认值。它读取的是 Blueprint Class Defaults 中对应变量的默认值。

不读取：

```text
所有 Class Settings
所有变量声明
Graph 默认 literal
DataAsset 属性
```

成功返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_member_defaults",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "member_defaults"
  },
  "data": {
    "schema": "ReadBlueprintMemberDefaults.v1",
    "defaults": {
      "default_count": 2,
      "values": {
        "OpenAngle": 90.0,
        "AutoCloseDelay": 2.5
      }
    }
  }
}
```

读取默认值可以返回 values，因为这是读工具职责。

### 5.2 set_blueprint_member_default / set_blueprint_member_defaults

设置成员变量默认值，本质是设置 Blueprint Class Defaults 中指定变量的默认值。

不负责：

```text
创建变量声明
修改变量类型
修改 DataAsset
修改运行时实例
```

单默认值成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_member_default",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "default_scope": "member_default"
  },
  "data": {
    "schema": "SetBlueprintMemberDefault.v1",
    "default_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

批量默认值成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_member_defaults",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "default_scope": "member_defaults"
  },
  "data": {
    "schema": "SetBlueprintMemberDefault.v1",
    "default_result": {
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

批量失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_member_defaults",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "default_scope": "member_defaults"
  },
  "error": {
    "code": "invalid_member_default_settings",
    "stage": "validate_defaults",
    "message": "One or more member default settings are invalid.",
    "retryable": false,
    "conflicts": [
      {
        "code": "variable_not_found",
        "variable_name": "MissingVariable"
      },
      {
        "code": "type_mismatch",
        "variable_name": "OpenAngle",
        "expected_type": "float"
      }
    ]
  }
}
```

---

## 6. Local Variables 工具

### 6.1 边界

Local Variable 必须绑定明确函数。

不允许：

```text
只指定 asset_path
只指定 graph
只指定当前编辑器打开函数
```

Local Variable 不支持：

```text
instance_editable
expose_on_spawn
replication
class defaults
外部 Blueprint 访问
```

### 6.2 read_blueprint_local_variables

成功返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_local_variables",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "read_scope": "local_variables"
  },
  "data": {
    "schema": "ReadBlueprintLocalVariables.v1",
    "local_variables": {
      "variable_count": 1,
      "variables": [
        {
          "variable_name": "TargetAngle",
          "variable_type": {
            "category": "float",
            "container": "single"
          }
        }
      ]
    }
  }
}
```

### 6.3 add_blueprint_local_variable / add_blueprint_local_variables

单变量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_local_variable",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "AddBlueprintLocalVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

批量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_local_variables",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "AddBlueprintLocalVariable.v1",
    "variable_result": {
      "requested_count": 2,
      "added_count": 2,
      "no_op_count": 0
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

### 6.4 set_blueprint_local_variable_properties

Local variable 第一版建议只支持：

```text
category
description
tooltip
```

不支持：

```text
instance_editable
expose_on_spawn
replication
class default
```

单变量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_local_variable_properties",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "SetBlueprintLocalVariableProperties.v1",
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

批量成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_local_variables_properties",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "SetBlueprintLocalVariableProperties.v1",
    "property_result": {
      "requested_count": 2,
      "applied_count": 2,
      "changed_count": 1,
      "no_op_count": 1
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

### 6.5 remove_blueprint_local_variable / remove_blueprint_local_variables

必须 dry_run，因为函数图内可能存在 Local Variable Get/Set 节点。

dry_run blocked：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_local_variable",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "RemoveBlueprintLocalVariableDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "local_variable_references_exist"
      ],
      "conflicts": [
        {
          "code": "local_variable_references_exist",
          "variable_name": "TargetAngle",
          "reference_count": 2,
          "message": "The local variable is referenced by nodes in the function graph."
        }
      ],
      "errors": []
    }
  }
}
```

单变量正式成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_local_variable",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "RemoveBlueprintLocalVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

批量正式成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_local_variables",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "RemoveBlueprintLocalVariable.v1",
    "variable_result": {
      "requested_count": 2,
      "removed_count": 2
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

---

## 7. 与 Graph Write 的关系

Graph Write 可以使用变量节点，但变量声明不应隐式创建，除非 Graph Write 工具明确支持并在 plan 中声明依赖。

规则：

```text
1. Graph Write 不默认创建成员变量。
2. 如果 LogicJson 需要新变量，Agent 应先调用 add_blueprint_member_variable。
3. 然后再 Graph Write 引用该变量。
4. 如果变量已存在，使用 reuse_if_exists。
5. Graph Write 成功结果不回显变量声明详情。
```

---

## 8. 与 Class Settings / Class Defaults 的关系

边界：

```text
set_blueprint_member_default / defaults 是面向 Blueprint 变量默认值的窄口工具。
set_class_default_properties 是更通用的 Class Defaults 工具。
```

Agent-facing 语义：

```text
1. 修改成员变量默认值：优先使用 set_blueprint_member_default(s)。
2. 修改非变量类默认属性或组件默认属性：使用 set_class_default_properties 或对应专用工具。
3. 不要把 add_blueprint_member_variable 当成设置默认值工具。
```

---

## 9. UE/MCP 建议结构体

```cpp
struct FBlueprintHelperVariableType
{
    FString Category;  // bool | int | float | object | class | struct | enum ...
    FString Subtype;   // class/struct/enum/object path when relevant
    FString Container; // single | array | set | map

    TOptional<FBlueprintHelperVariableType> KeyType;
    TOptional<FBlueprintHelperVariableType> ValueType;
};

struct FBlueprintHelperVariableSummary
{
    FString VariableName;
    FBlueprintHelperVariableType VariableType;
    FString Category;
    bool bInstanceEditable = false;
    bool bExposeOnSpawn = false;
};

struct FBlueprintHelperSingleSuccessResult
{
    bool bSuccess = true;
};

struct FBlueprintHelperBatchVariableResult
{
    int32 RequestedCount = 0;
    int32 AddedCount = 0;
    int32 RemovedCount = 0;
    int32 AppliedCount = 0;
    int32 ChangedCount = 0;
    int32 NoOpCount = 0;
};

struct FBlueprintHelperMemberDefaults
{
    int32 DefaultCount = 0;
    TMap<FString, TSharedPtr<FJsonValue>> Values;
};
```

---

## 10. 验收标准

```text
1. 成员变量声明、默认值、Local Variables 分工具。
2. 单个变量 add / set / remove 成功只返回 success=true。
3. 单个变量操作不返回 mode / requested_count / added_count / removed_count / changed_count / no_op_count。
4. 单个 no_op 通过 status=no_op 表达，不返回 reused_existing。
5. 批量变量操作保留计数，但不返回 mode=batch。
6. add_blueprint_member_variable 不设置默认值。
7. 默认值通过 set_blueprint_member_default(s) 设置。
8. read_blueprint_member_variables 不默认返回默认值。
9. read_blueprint_member_defaults 返回请求变量的 values。
10. remove member/local variable 必须 dry_run。
11. 有 Graph 引用时 blocked。
12. Local Variable 必须指定 function_name。
13. 批量写默认事务化。
14. validation 只返回 should_compile / should_save。
15. 成功不返回 write_ref / transaction_id / review / safety。
16. data.schema 使用短命名。
```
