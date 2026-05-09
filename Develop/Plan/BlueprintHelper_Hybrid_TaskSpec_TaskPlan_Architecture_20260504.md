# BlueprintHelper 混合任务编排架构方案

日期：2026-05-04  
状态：已确认的新架构方案  
适用范围：BlueprintHelper v0.4 / v0.5 之后的 Agent→MCP→Python→UE Task Runtime 架构收敛  
核心目标：减少 Agent 直接调用大量底层 MCP 工具的复杂度，同时保留既有工具簇、字段协议、Transaction / Review / rollback 设计。

文档同步范围：本文是本轮文档主线同步的权威架构说明，不代表本次已经实现新的 MCP Task 工具、Python Task Compiler 或 UE Task Runtime。现有字段映射、DoneImplementation 和 agent-to-MCP cluster 字段文档保留作为底层能力簇、内部协议、debug / expert 工具和测试入口资料；历史按工具拆分的 Agent 规则文档不再作为 AgentGuide 主线入口。

---

## 0. 结论摘要

本次确认的架构不是推倒现有 MCP 工具簇，而是新增一层任务编译与任务运行时：

```text
Agent
→ MCP Agent-facing Task Tools
→ Python / MCP Task Compiler
→ UE Plugin Task Runtime
→ Existing UE Capability Clusters
→ Unreal Editor
```

最终分工：

```text
Agent：负责把用户目标整理成 TaskSpec。
MCP：负责标准工具入口、schema、权限边界、返回协议。
Python / MCP Task Compiler：负责 TaskSpec 校验、上下文打包、语义检查、错误 suggested_patch、生成 TaskPlan。
UE Plugin Task Runtime：负责执行 TaskPlan、重新读取真实 UE 状态、TOCTOU 检查、事务、Review、rollback、compile/save。
现有工具簇：保留为 UE 侧内部 capability / debug tool / 测试入口。
```

关键判断：

```text
1. 不推翻现有 11 类工具簇。
2. 不让 Agent 直接面对大量底层原子 MCP 工具。
3. 不把完整 Agent-facing TaskSpec 编译逻辑塞进 UE C++。
4. UE 插件侧新增 Task Runtime，而不是新增 Agent 大脑。
5. TaskSpec 不替代 transaction；TaskSpec 是多个 transaction 的上层任务容器。
```

---

## 1. 架构动机

### 1.1 原始问题

如果 Agent 直接调用大量底层 MCP 工具完成一个复杂蓝图任务，例如物理门，会出现以下问题：

```text
1. Agent 必须记住大量工具边界。
2. Agent 容易漏掉 read / dry_run / compile / save / diagnostics。
3. Agent 容易把 Asset Factory、Component、Class Settings、Graph Write 的职责混用。
4. Agent 缺少上下文时会反复提交错误参数。
5. 多个写操作生成多个 transaction，Review 面板缺少任务级分组。
6. 错误返回如果只面向底层工具，Agent 不知道如何修 TaskSpec。
```

典型错误包括：

```text
add_component 里混入 physics / collision / mesh 属性。
add_implemented_interface 后误认为接口函数逻辑已实现。
AppendBlueprintGraph 被用来接入已有执行流。
InputAction 创建被误认为 IMC 按键映射完成。
Graph Write 失败后 Agent 继续 compile/save。
```

### 1.2 新架构目标

新架构目标是：

```text
1. Agent 不直接拼几十个底层 MCP 调用。
2. Agent 也不能只传一句自然语言。
3. Agent 必须传结构化 TaskSpec。
4. Python / MCP 编译 TaskSpec 为 TaskPlan。
5. UE Task Runtime 执行 TaskPlan 并管理真实事务。
6. 现有工具簇作为内部能力边界继续保留。
```

---

## 2. 层级总览

### 2.1 总体链路

```text
┌──────────────────────────────────────────────────────────┐
│ Agent                                                    │
│ - 理解用户目标                                           │
│ - 请求 TaskContextPack                                   │
│ - 生成 TaskSpec                                          │
│ - 根据 preview error 修正 TaskSpec                       │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ MCP Agent-facing Task Tools                              │
│ - read_task_context                                      │
│ - preview_task                                           │
│ - execute_task                                           │
│ - get_task_result                                        │
│ - runtime_profile / diagnostics                          │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ Python / MCP Task Compiler                               │
│ - TaskSpec schema validation                             │
│ - semantic validation                                    │
│ - context packing                                        │
│ - resource disambiguation                                │
│ - suggested_patch generation                             │
│ - TaskSpec → TaskPlan                                    │
│ - Bridge error normalization                             │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ UE Plugin Task Runtime                                   │
│ - TaskPlan validation                                    │
│ - TOCTOU check                                           │
│ - asset lock / execution context                         │
│ - task_run_id generation                                 │
│ - per write transaction_id generation                    │
│ - Transaction Journal / Review / rollback                │
│ - compile / save / diagnostics                           │
└───────────────────────┬──────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────┐
│ Existing UE Capability Clusters                          │
│ AssetFactory / Component / ClassSettings / GraphWrite    │
│ Validation / Safety / Cleanup / LogicRead / Journal      │
└──────────────────────────────────────────────────────────┘
```

### 2.2 两段任务编排

本架构将“任务编排层”拆成两段：

```text
Task Compiler：Python / MCP 侧
Task Runtime：UE 插件侧
```

| 模块 | 建议位置 | 职责 |
|---|---|---|
| TaskSpec schema 校验 | Python / MCP | 校验字段结构、类型、必填项、版本 |
| TaskSpec semantic 校验 | Python / MCP | 检查语义冲突、资源引用、scope policy 矛盾 |
| TaskContextPack | Python / MCP | 给 Agent 返回足够生成 TaskSpec 的压缩上下文 |
| suggested_patch | Python / MCP | 告诉 Agent 如何修 TaskSpec |
| TaskSpec → TaskPlan | Python / MCP | 生成可执行任务计划 |
| TaskPlan 执行 | UE 插件 | 真实执行 UE 操作 |
| TOCTOU 检查 | UE 插件 | 确认执行前 UE 状态未变化 |
| task_run_id / transaction_id | UE 插件 | 任务级与写操作级审计 ID |
| Review / rollback | UE 插件 | 保存 diff、snapshot、rollback_data、Review UI |

---

## 3. Agent-facing MCP 工具

### 3.1 默认暴露工具

普通 Agent 默认只面对少量任务级工具：

```text
blueprinthelper_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_task_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
```

底层工具不再作为普通 Agent 的主入口。

### 3.2 Debug / Expert 工具

现有底层工具可继续存在，但应归类为：

```text
1. Python Orchestrator 内部 capability。
2. Debug / Expert 模式 MCP 工具。
3. 自动化测试入口。
4. 失败定位入口。
```

例如：

```text
asset_create
add_component
set_component_properties
add_implemented_interface
read_logic_md
read_logic_json
append_blueprint_graph
replace_blueprint_graph
patch_blueprint_graph
merge_blueprint_graph
compile_blueprint
save_asset
cleanup_blueprint_helper_block
rollback_transaction
```

---

## 4. 核心数据结构

## 4.1 TaskContextPack

`TaskContextPack` 是 Agent 生成 TaskSpec 前需要读取的上下文包。它用于避免 Agent 靠 preview 报错反复猜参数。

推荐工具：

```text
blueprinthelper_read_task_context
```

返回示例：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_task_context",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "BlueprintHelper.TaskContextPack.v1",
    "context_id": "ctx_20260504_0001",
    "runtime": {
      "write_permission": "enabled",
      "safety_profile": "Conservative",
      "missing_capability_policy": "stop_and_report"
    },
    "target": {
      "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
      "exists": true,
      "asset_type": "blueprint",
      "parent_class": "/Script/Engine.Actor"
    },
    "blueprint_summary": {
      "components": [
        {
          "name": "DefaultSceneRoot",
          "class": "SceneComponent"
        }
      ],
      "graphs": [
        {
          "name": "EventGraph",
          "is_empty": false,
          "has_user_nodes": true
        }
      ],
      "implemented_interfaces": [],
      "variables": []
    },
    "resource_candidates": {
      "input_actions": {
        "interact": [
          {
            "asset_path": "/Game/Input/IA_Interact",
            "asset_type": "input_action"
          }
        ]
      },
      "static_meshes": {
        "door": [
          {
            "asset_path": "/Game/BlueprintHelperTest/Meshes/SM_Door",
            "asset_type": "static_mesh"
          }
        ]
      }
    },
    "recommended_constraints": {
      "prefer_new_graph": true,
      "recommended_graph_name": "EG_PhysicsDoor",
      "allow_modify_user_nodes": false,
      "allow_edit_input_mapping": false
    }
  }
}
```

TaskContextPack 不应该默认返回完整 RawJson 或巨大 LogicJson。Python / UE 可以内部保留更完整上下文，Agent 只拿决策摘要。

---

## 4.2 TaskSpec

TaskSpec 是 Agent 提交的语义级任务规格。

它不是自然语言，也不是底层节点图。它描述：

```text
在哪个蓝图里
用哪些资源
创建哪些组件
设置哪些属性
新增哪些变量
创建哪些事件 / 函数
实现什么行为
如何接入输入 / 接口 / 已有执行流
允许修改哪些范围
失败如何处理
```

物理门示例：

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_20260504_0001",
  "task_type": "create_blueprint_feature",
  "feature_name": "PhysicsDoor",
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "prefer_new_graph": true,
    "graph_name": "EG_PhysicsDoor",
    "allow_modify_user_nodes": false,
    "allow_merge_existing_execution_flow": false,
    "allow_create_assets": true,
    "allow_edit_input_mapping": false
  },
  "asset_policy": {
    "if_target_asset_missing": "fail",
    "if_referenced_asset_missing": "fail",
    "if_component_exists": "reuse_if_type_matches",
    "if_graph_exists_non_empty": "fail"
  },
  "resources": {
    "static_meshes": {
      "door_mesh": "/Game/BlueprintHelperTest/Meshes/SM_Door"
    },
    "input_actions": {
      "interact": "/Game/Input/IA_Interact"
    },
    "interfaces": {
      "interactable": {
        "asset_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
        "create_if_missing": true
      }
    }
  },
  "components": [
    {
      "name": "SceneRoot",
      "class": "SceneComponent",
      "attach_to": null,
      "set_as_root": true
    },
    {
      "name": "DoorMesh",
      "class": "StaticMeshComponent",
      "attach_to": "SceneRoot",
      "properties": {
        "StaticMesh": "$resources.static_meshes.door_mesh",
        "RelativeLocation": [0, 0, 0],
        "Mobility": "Movable",
        "CollisionProfileName": "PhysicsActor",
        "BodyInstance.bSimulatePhysics": true
      }
    },
    {
      "name": "InteractionBox",
      "class": "BoxComponent",
      "attach_to": "SceneRoot",
      "properties": {
        "RelativeLocation": [100, 0, 100],
        "BoxExtent": [120, 120, 120],
        "CollisionProfileName": "OverlapAllDynamic"
      }
    },
    {
      "name": "DoorConstraint",
      "class": "PhysicsConstraintComponent",
      "attach_to": "SceneRoot",
      "properties": {
        "ComponentName1": "SceneRoot",
        "ComponentName2": "DoorMesh",
        "AngularSwing1Motion": "Limited",
        "AngularSwing1Limit": 90,
        "AngularSwing2Motion": "Locked",
        "AngularTwistMotion": "Locked"
      }
    }
  ],
  "class_settings": {
    "implemented_interfaces": [
      "$resources.interfaces.interactable.asset_path"
    ]
  },
  "variables": [
    {
      "name": "bDoorOpen",
      "type": "bool",
      "default": false
    },
    {
      "name": "OpenImpulse",
      "type": "float",
      "default": 50000.0
    },
    {
      "name": "CloseImpulse",
      "type": "float",
      "default": -50000.0
    }
  ],
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "TogglePhysicsDoor",
        "logic": [
          {
            "type": "branch",
            "condition": "bDoorOpen",
            "false": [
              { "call": "OpenPhysicsDoor" }
            ],
            "true": [
              { "call": "ClosePhysicsDoor" }
            ]
          }
        ]
      },
      {
        "entry_type": "custom_event",
        "name": "OpenPhysicsDoor",
        "logic": [
          { "set": "bDoorOpen", "value": true },
          {
            "call": "DoorMesh.AddAngularImpulseInDegrees",
            "args": {
              "Impulse": [0, 0, "$variables.OpenImpulse"],
              "BoneName": "None",
              "VelChange": true
            }
          }
        ]
      },
      {
        "entry_type": "custom_event",
        "name": "ClosePhysicsDoor",
        "logic": [
          { "set": "bDoorOpen", "value": false },
          {
            "call": "DoorMesh.AddAngularImpulseInDegrees",
            "args": {
              "Impulse": [0, 0, "$variables.CloseImpulse"],
              "BoneName": "None",
              "VelChange": true
            }
          }
        ]
      }
    ]
  },
  "integration": {
    "input": {
      "mode": "reference_existing_input_action",
      "input_action": "$resources.input_actions.interact",
      "on_triggered": {
        "call": "TogglePhysicsDoor"
      },
      "if_event_entry_exists": "merge_requires_dry_run",
      "if_event_entry_missing": "create_entry_if_supported"
    },
    "interface": {
      "interface_asset": "$resources.interfaces.interactable.asset_path",
      "function": "Interact",
      "implementation": {
        "call": "TogglePhysicsDoor"
      }
    }
  },
  "execution_policy": {
    "preview_required": true,
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "on_dry_run_blocked": "stop_and_report",
    "on_write_partial_failure": "record_partial_failure_and_report"
  },
  "validation": {
    "should_compile": true,
    "should_save": false,
    "run_asset_diagnostics": true
  }
}
```

---

## 4.3 TaskPlan

TaskPlan 是 Python / MCP Task Compiler 输出给 UE Task Runtime 的计划。它比 TaskSpec 更接近 UE 执行层。

TaskPlan 不应该包含 Agent-facing suggested_patch，也不应该包含自然语言意图推理。

TaskPlan step 的主面是能力簇结构化 IR，例如 `capability`、`target`、`write`、`constraints`。`append_blueprint_graph` 这类 Bridge adapter operation 只属于 UE Task Runtime lowering 和 journal child fact，不应该作为 GraphWrite TaskPlan IR step 字段出现。

示例：

```json
{
  "schema": "BlueprintHelper.TaskPlan.v1",
  "task_name": "PhysicsDoor",
  "task_type": "create_blueprint_feature",
  "context_id": "ctx_20260504_0001",
  "target_assets": [
    "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  ],
  "execution_policy": {
    "dry_run_mode": "full",
    "should_compile": true,
    "should_save": false,
    "on_write_partial_failure": "record_partial_failure_and_report"
  },
  "steps": [
    {
      "step_id": "step_001",
      "capability": "asset_write",
      "target": {
        "asset_path": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
        "asset_type": "blueprint_interface"
      },
      "write": {
        "strategy": "ensure_asset",
        "create_if_missing": true
      }
    },
    {
      "step_id": "step_002",
      "capability": "component_write",
      "target": {
        "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      },
      "write": {
        "strategy": "ensure_component",
        "component_name": "DoorMesh",
        "component_class": "StaticMeshComponent",
        "attach_to": "SceneRoot",
        "name_collision": "reuse_if_exists"
      }
    },
    {
      "step_id": "step_003",
      "capability": "component_write",
      "depends_on": ["step_002"],
      "target": {
        "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
        "component_name": "DoorMesh"
      },
      "write": {
        "strategy": "set_properties",
        "properties": {
          "StaticMesh": "/Game/BlueprintHelperTest/Meshes/SM_Door",
          "BodyInstance.bSimulatePhysics": true
        }
      }
    },
    {
      "step_id": "step_004",
      "capability": "graph_write",
      "target": {
        "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
        "graph": "EG_PhysicsDoor"
      },
      "write": {
        "strategy": "owned_graph_edit",
        "ops": [
          {
            "op": "ensure_entry",
            "entry_type": "custom_event",
            "name": "TogglePhysicsDoor"
          },
          {
            "op": "ensure_entry",
            "entry_type": "custom_event",
            "name": "OpenPhysicsDoor"
          },
          {
            "op": "ensure_entry",
            "entry_type": "custom_event",
            "name": "ClosePhysicsDoor"
          }
        ]
      },
      "constraints": {
        "allow_modify_user_nodes": false,
        "ownership_scope": "blueprinthelper_owned"
      }
    }
  ]
}
```

---

## 4.4 TaskRunJournal

TaskRunJournal 是 TaskSpec / TaskPlan 执行后的任务级审计记录。

它不替代 Transaction Journal，而是作为多个 transaction 的上层容器。

推荐路径：

```text
<Project>/Saved/BlueprintHelper/Tasks/task_20260504_0001.json
```

结构示例：

```json
{
  "schema": "BlueprintHelper.TaskRunJournal.v1",
  "task_run_id": "task_20260504_0001",
  "task_type": "create_blueprint_feature",
  "feature_name": "PhysicsDoor",
  "status": "completed",
  "target_assets": [
    "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  ],
  "task_spec_hash": "sha256:...",
  "context_snapshot_ref": "ctx_20260504_0001",
  "preview_id": "preview_20260504_0001",
  "steps": [
    {
      "step_id": "step_001",
      "operation": "asset_create",
      "status": "applied",
      "transaction_id": "tx_001"
    },
    {
      "step_id": "step_002",
      "operation": "add_component",
      "status": "applied",
      "transaction_id": "tx_002"
    },
    {
      "step_id": "step_003",
      "capability": "graph_write",
      "operation": "graph_write",
      "adapter_operation": "append_blueprint_graph",
      "status": "applied",
      "transaction_id": "tx_006"
    }
  ],
  "child_transactions": [
    "tx_001",
    "tx_002",
    "tx_003",
    "tx_004",
    "tx_005",
    "tx_006"
  ],
  "validation": {
    "compiled": true,
    "saved": false,
    "diagnostics": "passed"
  }
}
```

---

## 5. Transaction / Review 模型

### 5.1 ID 层级

最终 ID 规则：

```text
task_run_id = 一次 TaskSpec / TaskPlan 执行。
transaction_id = 一次真实 UE 写操作。
block_id = Graph Write 创建或接管的 BlueprintHelper-owned 蓝图逻辑块。
```

关系：

```text
一个 task_run_id 下可以有多个 transaction_id。
一个 transaction_id 下可以有多个 block_id，但 block_id 只用于 Graph Write owned 逻辑块。
```

### 5.2 不合并 transaction

不要把整个 TaskSpec 合并成一个 transaction_id。

原因：

```text
1. 每个写操作的 rollback_data 不同。
2. 资产创建、组件修改、类设置修改、图表写入的 diff 类型不同。
3. Review UI 需要可以精确展示和回滚具体变更。
4. 出错时需要知道具体哪一步写入失败。
```

### 5.3 Review UI 分组

Review UI 默认按 task_run_id 展示任务级入口：

```text
Task: PhysicsDoor
Target: /Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor
Status: pending_review
Transactions: 7
```

展开后展示子 transaction：

```text
1. Created interface asset
2. Added components
3. Configured component properties
4. Added implemented interface
5. Added variables
6. Appended graph EG_PhysicsDoor
7. Connected input/interface entry
```

### 5.4 Reject / Rollback

支持：

```text
单个 transaction Reject。
整个 task_run RejectAll。
```

Task-level RejectAll：

```text
1. 按 child transaction 创建时间倒序 rollback。
2. 遇到 rollback blocked / failed 立刻停止。
3. 不跨 task_run 自动级联 rollback。
4. 不自动回滚 task_run 之外的其他 transaction。
```

---

## 6. 两层错误协议

### 6.1 总体分层

必须存在两套错误层：

```text
Agent / Python / MCP Task Error Layer
Python / Bridge / UE Operation Error Layer
```

关系：

```text
Bridge Error 是事实。
Task Error 是解释。
Agent Action 是修复指令。
```

### 6.2 Task Error Layer

面向 Agent，回答：

```text
TaskSpec 哪个字段错？
为什么错？
允许什么值？
是否可以自动 patch？
Agent 应该修参数、问用户，还是 stop_and_report？
```

推荐结构：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "preview_task",
  "status": "failed",
  "modified": false,
  "error": {
    "code": "taskspec_semantic_invalid",
    "category": "semantic_error",
    "message": "TaskSpec contains contradictory instructions.",
    "retryable": true,
    "agent_action": "fix_taskspec_and_retry",
    "issues": [
      {
        "path": "$.resources.interfaces.interactable.create_if_missing",
        "code": "conflicts_with_scope_policy",
        "message": "create_if_missing=true conflicts with scope_policy.allow_create_assets=false.",
        "conflicts_with": "$.scope_policy.allow_create_assets",
        "suggested_patches": [
          {
            "description": "Do not create missing interface assets.",
            "patch": {
              "op": "replace",
              "path": "/resources/interfaces/interactable/create_if_missing",
              "value": false
            }
          },
          {
            "description": "Allow asset creation for this task.",
            "patch": {
              "op": "replace",
              "path": "/scope_policy/allow_create_assets",
              "value": true
            }
          }
        ]
      }
    ]
  }
}
```

Task Error 分类：

```text
taskspec_schema_error
taskspec_semantic_error
task_policy_error
task_capability_error
task_preview_blocked
task_execution_failed
```

### 6.3 Bridge / UE Operation Error Layer

面向 Python / UE Runtime，回答：

```text
哪条 UE 命令失败？
失败在哪个 stage？
是否已修改资产？
是否已回滚？
Journal 是否写入？
是否允许继续后续步骤？
```

推荐结构：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.BridgeCommandResult.v1",
  "command": "append_blueprint_graph",
  "trace_id": "trace_20260504_001",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "graph": "EG_PhysicsDoor"
  },
  "error": {
    "code": "pin_type_mismatch",
    "stage": "connect_pins",
    "message": "Generated link could not be connected because pin types are incompatible.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "link",
      "ref": "links[7]"
    },
    "conflicts": []
  }
}
```

### 6.4 Error Normalizer

Python / MCP Task Compiler 必须有统一错误归一化模块：

```text
error_normalizer.py
```

职责：

```text
BridgeError → TaskIssue
BridgeError → TaskExecutionError
BridgeError → StopAndReportReason
```

规则：

```text
1. Agent 默认只消费 Task Error。
2. Python 默认消费 Bridge Error。
3. MCP 返回给 Agent 的 execution error 必须是 Bridge Error 的归一化结果。
4. 原始 Bridge Error 默认不直接暴露给 Agent。
5. verbose=true / debug 模式可返回 raw_error_ref 或底层 trace 摘要。
```

---

## 7. preview / execute 语义

### 7.1 read_task_context

职责：

```text
1. 收集 runtime profile。
2. 收集目标资产摘要。
3. 收集组件 / graph / class settings / variable 摘要。
4. 搜索资源候选。
5. 返回 recommended_constraints。
```

不写资产。

### 7.2 preview_task

职责：

```text
1. 校验 TaskSpec schema。
2. 校验 TaskSpec semantic。
3. 检查 context_id 是否过期。
4. 检查 runtime profile / write_permission / safety_profile。
5. 生成 TaskPlan。
6. 调 UE Task Runtime 做 preflight / dry_run。
7. 返回 passed / blocked / context_required。
```

`preview_task` 不写资产。

### 7.3 execute_task

职责：

```text
1. 要求 preview passed，或内部先执行 preview。
2. 将 TaskPlan 交给 UE Task Runtime。
3. UE Runtime 生成 task_run_id。
4. 每个真实写操作生成 transaction_id。
5. 写 TaskRunJournal / Transaction Journal / Review。
6. 执行 compile / diagnostics / save。
7. 返回任务级摘要。
```

默认成功返回不展开所有 child transactions。

示例：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "execute_task",
  "status": "completed",
  "modified": true,
  "data": {
    "schema": "BlueprintHelper.TaskExecution.v1",
    "task": {
      "task_run_id": "task_20260504_0001",
      "feature_name": "PhysicsDoor",
      "target_assets": [
        "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      ],
      "applied_steps": 7,
      "created_assets": 1,
      "modified_assets": 1
    }
  },
  "validation": {
    "compiled": true,
    "should_save": true,
    "saved": false
  }
}
```

---

## 8. 现有工具簇的去向

现有工具簇继续保留，但不作为普通 Agent 的直接主入口。

| 工具簇 | 新架构中的位置 |
|---|---|
| Asset Factory | UE Capability，供 Task Runtime 调用；可保留 debug MCP 工具 |
| Blueprint Component | UE Capability；组件创建和属性设置仍分离 |
| Blueprint Class Settings | UE Capability；只修改类设置，不写接口函数 body |
| Enhanced Input Boundary | 仍作为边界规则；默认不编辑 IA / IMC |
| Validation / Diagnostics | Runtime 和 Agent-facing diagnostics 都保留，职责分层 |
| Safety Profile / dry_run | Runtime profile 为 Agent 事实来源；UE Runtime 强制执行 |
| Transaction / Journal / Review | UE 插件内部事实来源，新增 task_run_id 分组 |
| LogicMD / LogicJson Read | Python / Agent 生成上下文与 TaskPlan 时使用 |
| Graph Write | UE Capability；Append / Replace / Patch / Merge 语义不变 |
| Cleanup / Ownership | UE Capability；Review / rollback / AutoRepair 使用 |
| 未来 Member/Signature/Interface/Struct/Enum | 作为能力簇继续扩展，由 TaskPlan 调用 |

---

## 9. UE 插件侧新增模块

建议新增：

```text
FBlueprintHelperTaskRuntime
FBlueprintHelperTaskPlanValidator
FBlueprintHelperTaskExecutionContext
FBlueprintHelperTaskRunJournalService
FBlueprintHelperTaskRollbackCoordinator
FBlueprintHelperTaskReviewGrouper
```

职责：

```text
1. 接收 TaskPlan。
2. 执行前重新验证 UE 当前状态。
3. 锁定目标资产或建立执行上下文。
4. 将 TaskPlan step 映射到已有工具簇能力。
5. 每个真实写操作生成 transaction_id。
6. 整个 TaskPlan 生成 task_run_id。
7. 失败时记录 partial_failure 并按 TaskPlan topology 阻断依赖步骤；不默认全局 rollback。
8. 写 TaskRunJournal + Transaction Journal + Review。
9. compile / diagnostics / save。
```

不建议 UE Task Runtime 负责：

```text
1. Agent-facing suggested_patch。
2. TaskSpec JSONPath 级错误修正。
3. 自然语言目标理解。
4. 不同 Agent 客户端兼容逻辑。
5. TaskSpec 大版本迁移。
```

---

## 10. Python / MCP 侧新增模块

建议新增：

```text
task_context_service.py
taskspec_schema_validator.py
taskspec_semantic_validator.py
task_compiler.py
taskplan_builder.py
error_normalizer.py
preview_controller.py
```

职责：

```text
1. read_task_context。
2. TaskSpec schema 校验。
3. TaskSpec semantic 校验。
4. resource candidates 消歧。
5. context stale 检查请求。
6. suggested_patch 生成。
7. TaskSpec → TaskPlan。
8. Bridge / UE error 归一化。
```

---

## 11. 推荐执行流程

标准 Agent 流程：

```text
A. Agent 调 read_task_context。
B. Agent 基于 TaskContextPack 生成 TaskSpec。
C. Agent 调 preview_task。
D. 如果 preview 返回 TaskSpec 错误，Agent 按 suggested_patch 修正。
E. 如果 preview 返回 context_required，Agent 重新读上下文。
F. 如果 preview 返回 blocked，Agent stop_and_report 或修改 TaskSpec。
G. preview passed 后，Agent 调 execute_task。
H. UE Task Runtime 执行 TaskPlan。
I. Agent 最终只报告任务级摘要。
```

---

## 12. 迁移计划

### Phase 1：Python / MCP TaskSpec 原型

```text
1. 新增 read_task_context / preview_task / execute_task 草案。
2. Python 实现 TaskSpec schema validation。
3. Python 实现 TaskContextPack。
4. Python 先将 TaskSpec 展开成现有底层 MCP/Bridge 调用。
5. UE 侧暂不新增 TaskRuntime。
```

目标：验证 TaskSpec 字段设计和 Agent 交互闭环。

### Phase 2：TaskPlan 协议

```text
1. 定义 BlueprintHelper.TaskPlan.v1。
2. Python 从 TaskSpec 编译 TaskPlan。
3. TaskPlan step 映射到现有工具簇。
4. preview_task 返回 TaskPlan 摘要，不返回底层细节。
```

目标：让 TaskSpec 和 UE 执行细节解耦。

### Phase 3：UE Task Runtime

```text
1. UE 插件新增 TaskRuntime。
2. UE 接收 TaskPlan。
3. UE 执行多步骤计划。
4. UE 生成 task_run_id。
5. UE 记录 child transaction_id。
6. UE 负责 TaskRunJournal。
```

目标：减少 Bridge 往返，提高事务一致性。

### Phase 4：Review UI task_run 分组

```text
1. Review UI 默认按 task_run_id 分组。
2. 子 transaction 可展开查看。
3. 支持 task-level AcceptAll / RejectAll。
4. RejectAll 倒序 rollback child transactions。
```

目标：用户看到的是任务，而不是零散工具流水账。

### Phase 5：底层工具降级为 internal / debug

```text
1. 普通 Agent 默认只看到任务级工具。
2. 底层工具保留 debug / Expert / 自动化测试入口。
3. Agent Skill 更新为 TaskSpec-first 工作流。
4. /agentplan 输出 TaskSpec / TaskPlan 摘要，而不是底层 MCP 工具序列。
```

---

## 13. 对 /agentplan 的影响

旧定位：

```text
/agentplan 将语义计划细化成 MCP 工具调用顺序。
```

新定位：

```text
/agentplan 将语义计划细化成 TaskSpec，并在必要时展示 TaskPlan 摘要。
```

不再要求 Agent 输出完整底层工具调用序列。

高风险任务仍需展示：

```text
1. 目标资产。
2. 允许修改范围。
3. 是否会创建资产。
4. 是否会修改用户节点。
5. 是否会接入已有执行流。
6. 是否需要 dry_run。
7. 是否会产生 task_run_id 和多个 transaction。
```

---

## 14. 验收标准

### 14.1 架构验收

```text
1. Agent 默认不直接调用大量底层 MCP 工具。
2. Agent 可以通过 read_task_context 获取生成 TaskSpec 所需上下文。
3. Agent 可以提交 TaskSpec 进行 preview。
4. preview_task 能返回结构化 TaskSpec 错误和 suggested_patch。
5. execute_task 能返回 task_run_id 和任务级摘要。
6. UE 写操作仍生成各自 transaction_id。
7. Review UI 能按 task_run_id 分组展示 child transactions。
8. 底层工具簇边界不变。
```

### 14.2 错误协议验收

```text
1. TaskSpec schema 错误返回 error.issues[].path。
2. 语义冲突返回 conflicts_with / suggested_patches。
3. 资源不唯一返回 candidates。
4. 能力缺失返回 capability / cluster / reason。
5. preview blocked 时 ok=true / status=preview_blocked。
6. 执行失败时返回 execution_state。
7. rollback failed / blocked 时 Agent 必须 stop_and_report。
8. Bridge 原始错误不默认直接暴露给 Agent。
```

### 14.3 Review / Transaction 验收

```text
1. TaskSpec 不替代 transaction。
2. 每个真实 UE 写操作仍进入 Transaction Journal / Review。
3. 一个 task_run_id 可关联多个 transaction_id。
4. TaskRunJournal 保存 TaskSpec hash、context_id、TaskPlan steps、child_transactions、validation。
5. Task-level RejectAll 按 child transaction 创建时间倒序 rollback。
6. 遇到 rollback blocked / failed 停止。
```

---

## 15. 最终定义

BlueprintHelper 新混合任务编排架构定义为：

> BlueprintHelper 采用 Agent-facing TaskSpec、MCP/Python Task Compiler、UE Plugin Task Runtime、UE Capability Clusters 四层协作模式。Agent 负责提交结构化 TaskSpec，MCP/Python 负责上下文打包、TaskSpec 校验、错误修正建议和 TaskPlan 生成，UE 插件负责执行 TaskPlan、管理 task_run_id / transaction_id、Journal / Review / rollback、compile/save 和真实编辑器状态一致性。现有工具簇不废弃，而是降级为 UE Task Runtime 的内部能力、Debug 工具和测试入口。

一句话版本：

```text
Agent 不直接写蓝图工具调用；Agent 写 TaskSpec。
Python 不直接替代 UE；Python 编译 TaskSpec。
UE 不理解 Agent；UE 执行 TaskPlan。
Transaction 不消失；task_run_id 组织多个 transaction。
```
