# RestoreDevice TaskSpec 与 Spawn 差距报告

生成时间：2026-05-31 16:48:31 +08:00

## 来源

- TaskSpec：`Saved\BlueprintHelper\Cli\preview_1780216464697_0001\task_plan.json`
- 执行结果：`Saved\BlueprintHelper\Cli\task_F2F8B16F446D8F6ABF8157B18284200D\result.json`
- 真实 Spawn logicJson：`C:\Users\26227\Desktop\新建 文本文档.txt`
- 目标蓝图：`/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer`
- 临时入口：`CE_RestoreDevice`

## 结论

- 节点数量符合预期：TaskSpec 期望生成 1 个 CustomEvent 加 4 组玩家恢复节点，真实 logicJson 是 33 个节点。
- 主要数据线已生成：`ControllerDeviceMap -> Map_Contains`、`Map_Contains.ReturnValue -> Branch.Condition`、`Map_Find.Value -> BindInputDeviceToPlayer.DeviceId`、`GetGameInstanceSubsystem.ReturnValue -> DynamicCast.Object`、`DynamicCast.AsInput Routing Subsystem -> BindInputDeviceToPlayer.self` 都存在。
- 明确缺失 4 条执行线：每组 `Branch.then` 没有连到对应的 `BindInputDeviceToPlayer.execute`。
- 默认值存在偏差：`Map_Contains.Key`、`Map_Find.Key`、`GetGameInstanceSubsystem.Class` 没有出现在真实 logicJson 的 inputs 中；Player 1 到 Player 3 的 `LocalPlayerIndex` 被生成为 `0`。
- Cast 节点语义有偏差：TaskSpec 只写了 `dynamic_cast`，没有显式声明 `PureCast`；真实 Spawn 出来的是带 exec pin 的 Cast，但它的 exec pin 没有被接入执行链。
- `Map_Find` 的结果 pin 语义有偏差：UE 正确节点的 `ReturnValue` 是 BoolProperty，真正的 map value 输出是 `Value`；当前生成链路把 `container_action` 的 `result_symbol` 按通用 `ReturnValue` 处理，导致整数类型和 BoolProperty 冲突。
- `InputRoutingSubsystem` 获取节点形态有偏差：TaskSpec 明确写成 `GetGameInstanceSubsystem` 加 `dynamic_cast`，所以真实 Spawn 没有走 UE 右键菜单里的 typed `Get Input Routing Subsystem` 节点。

## TaskSpec 期望

TaskSpec 的语义是对 Player 0 到 Player 3 分别执行：

```text
if ControllerDeviceMap.Contains(PlayerIndex)
    DeviceId = ControllerDeviceMap.Find(PlayerIndex)
    InputRoutingSubsystem.BindInputDeviceToPlayer(DeviceId, PlayerIndex)
```

期望每组都包含这些固定值：

| Player | Contains.Key | Find.Key | GetGameInstanceSubsystem.Class | Bind.LocalPlayerIndex |
| --- | ---: | ---: | --- | ---: |
| 0 | 0 | 0 | `/Script/BallMaze.InputRoutingSubsystem` | 0 |
| 1 | 1 | 1 | `/Script/BallMaze.InputRoutingSubsystem` | 1 |
| 2 | 2 | 2 | `/Script/BallMaze.InputRoutingSubsystem` | 2 |
| 3 | 3 | 3 | `/Script/BallMaze.InputRoutingSubsystem` | 3 |

如果 Cast 是 PureCast，期望执行线数量是 35 条，真实 logicJson 只有 31 条，差额正好是 4 条 `Branch.then -> Bind.execute`。

如果 Cast 保持当前真实 Spawn 的 impure Cast，执行线不能直接跳到 Bind，应该变成 `Branch.then -> Cast.execute -> CastSucceeded -> Bind.execute`，并且 `CastFailed` 需要继续进入下一组玩家分支，避免某个玩家 Cast 失败后阻断后续恢复。

## 真实 Spawn 摘要

- schema：`ReadContextPack.v1`
- format：`logicjson`
- source：`t3d_clipboard`
- node_count：`33`
- link_count：`31`

## 差距明细

### 缺失执行线

| Player | 缺失连接 | 影响 |
| --- | --- | --- |
| 0 | `K2Node_IfThenElse_23.then -> K2Node_CallFunction_92.execute` | Player 0 命中 Contains 后不会执行绑定 |
| 1 | `K2Node_IfThenElse_24.then -> K2Node_CallFunction_96.execute` | Player 1 命中 Contains 后不会执行绑定 |
| 2 | `K2Node_IfThenElse_25.then -> K2Node_CallFunction_100.execute` | Player 2 命中 Contains 后不会执行绑定 |
| 3 | `K2Node_IfThenElse_26.then -> K2Node_CallFunction_104.execute` | Player 3 命中 Contains 后不会执行绑定 |

### 默认值差距

| Player | 节点 | Pin | TaskSpec 期望 | 真实 Spawn |
| --- | --- | --- | --- | --- |
| 0 | `K2Node_CallFunction_89` | `Key` | `0` | inputs 为空 |
| 0 | `K2Node_CallFunction_90` | `Key` | `0` | inputs 为空 |
| 0 | `K2Node_CallFunction_91` | `Class` | `/Script/BallMaze.InputRoutingSubsystem` | inputs 为空 |
| 0 | `K2Node_CallFunction_92` | `LocalPlayerIndex` | `0` | `0`，符合预期 |
| 1 | `K2Node_CallFunction_93` | `Key` | `1` | inputs 为空 |
| 1 | `K2Node_CallFunction_94` | `Key` | `1` | inputs 为空 |
| 1 | `K2Node_CallFunction_95` | `Class` | `/Script/BallMaze.InputRoutingSubsystem` | inputs 为空 |
| 1 | `K2Node_CallFunction_96` | `LocalPlayerIndex` | `1` | `0` |
| 2 | `K2Node_CallFunction_97` | `Key` | `2` | inputs 为空 |
| 2 | `K2Node_CallFunction_98` | `Key` | `2` | inputs 为空 |
| 2 | `K2Node_CallFunction_99` | `Class` | `/Script/BallMaze.InputRoutingSubsystem` | inputs 为空 |
| 2 | `K2Node_CallFunction_100` | `LocalPlayerIndex` | `2` | `0` |
| 3 | `K2Node_CallFunction_101` | `Key` | `3` | inputs 为空 |
| 3 | `K2Node_CallFunction_102` | `Key` | `3` | inputs 为空 |
| 3 | `K2Node_CallFunction_103` | `Class` | `/Script/BallMaze.InputRoutingSubsystem` | inputs 为空 |
| 3 | `K2Node_CallFunction_104` | `LocalPlayerIndex` | `3` | `0` |

### Cast 节点模式差距

| Player | 节点 | TaskSpec 表达 | 真实 Spawn | 影响 |
| --- | --- | --- | --- | --- |
| 0 | `K2Node_DynamicCast_16` | `target_object.convert.dynamic_cast`，未显式声明 PureCast | 带 exec pin 的 Cast | `AsInput Routing Subsystem` 被接到 Bind.self，但 Cast 没有执行路径 |
| 1 | `K2Node_DynamicCast_17` | `target_object.convert.dynamic_cast`，未显式声明 PureCast | 带 exec pin 的 Cast | `AsInput Routing Subsystem` 被接到 Bind.self，但 Cast 没有执行路径 |
| 2 | `K2Node_DynamicCast_18` | `target_object.convert.dynamic_cast`，未显式声明 PureCast | 带 exec pin 的 Cast | `AsInput Routing Subsystem` 被接到 Bind.self，但 Cast 没有执行路径 |
| 3 | `K2Node_DynamicCast_19` | `target_object.convert.dynamic_cast`，未显式声明 PureCast | 带 exec pin 的 Cast | `AsInput Routing Subsystem` 被接到 Bind.self，但 Cast 没有执行路径 |

TaskSpec 原文里每组 Cast 都是：

```json
"target_object": {
  "kind": "convert",
  "transform_operation": "dynamic_cast",
  "target_class_path": "/Script/BallMaze.InputRoutingSubsystem"
}
```

这里没有类似 `pure: true`、`cast_mode: pure`、`bIsPureCast: true` 的字段，所以不能说 TaskSpec 显式选择了 PureCast。更准确地说，TaskSpec 把 Cast 当成 `target_object` 表达式使用，语义上更接近 PureCast，但 graph writer 实际生成了 impure Cast。

### Map Find 返回 pin 差距

UE 右键菜单正确 Spawn 出来的 Map Find 语义是：

```text
Map_Find.TargetMap = ControllerDeviceMap
Map_Find.Key = PlayerIndex
Map_Find.Value -> BindInputDeviceToPlayer.DeviceId
Map_Find.ReturnValue 是 bool，表示是否找到
```

当前 TaskSpec 的业务期望也是使用 `Value` 作为 `DeviceId`：

```json
{
  "kind": "container_action",
  "container_kind": "map",
  "container_operation": "find",
  "key_type": "int",
  "value_type": "int",
  "result_symbol": "RestoredDeviceId0"
}
```

问题在于 `result_symbol` 的通用编译路径把容器动作结果当成了 `ReturnValue`。源码里可以看到这个通用规则：

```ts
if (kind === 'call' || kind === CONTAINER_ACTION_KIND) {
  return 'ReturnValue';
}
```

并且语句流里也把 `result_symbol` 注册成：

```ts
context.symbols.set(resultSymbol.toLowerCase(), { output: `${nodeId}.ReturnValue` });
```

这对 `Map_Contains` 是对的，因为 `Map_Contains.ReturnValue` 是 bool。但对 `Map_Find` 是错的，因为 `Map_Find.ReturnValue` 仍然是 bool，真正的值输出应该是 `Value`。所以当前错误“整数类型的 Return Value 和 BoolProperty 类型不匹配”本质上是 result pin 选择错了，不是业务逻辑需要把 bool 当 int。

正确修复方向：

```text
container.map.find.result_symbol -> Map_Find.Value
Map_Find.ReturnValue 保持 bool
Map_Find.Value 按 value_type=int 解析
```

同时 `container.map.find` 的能力证据虽然把 result 角色绑定到了 `Value`：

```cpp
BindOutputRole(TEXT("result"), TEXT("Value"))
```

但后续通用 `result_symbol -> ReturnValue` 规则覆盖了这个语义，这是当前 writer/compiler 的缺口。

### Subsystem Get 节点形态差距

UE 右键菜单直接 Spawn 的 `Get Input Routing Subsystem` 是 typed subsystem getter 节点，输出 pin 已经是 `InputRoutingSubsystem` 类型，不需要额外 Cast。

这次 TaskSpec 写的是：

```json
"target_object": {
  "kind": "convert",
  "transform_operation": "dynamic_cast",
  "target_class_path": "/Script/BallMaze.InputRoutingSubsystem",
  "args": {
    "value": {
      "kind": "call",
      "target": "/Script/Engine.SubsystemBlueprintLibrary:GetGameInstanceSubsystem",
      "args": {
        "Class": {
          "kind": "literal",
          "value_type": "class",
          "value": "/Script/BallMaze.InputRoutingSubsystem"
        }
      }
    }
  }
}
```

所以真实 Spawn 成 `GetGameInstanceSubsystem + Cast to InputRoutingSubsystem` 是符合当前 TaskSpec 字面描述的，但不符合 UE 右键菜单的最优节点形态。

造成这个差距的能力缺失是：

- TaskSpec 没有表达“按 Subsystem 类型生成 UE typed getter 节点”的语义，只表达了“调用通用 GameInstanceSubsystem 函数，再转成目标类型”。
- 当前 writer 没有把 `/Script/BallMaze.InputRoutingSubsystem` 映射到右键菜单里的 `Get Input Routing Subsystem` ActionDatabase spawner。
- 当前 graph write 路径走的是 function call/generic convert，而不是 `K2Node_GetSubsystem` 这类 typed subsystem getter 节点的专用 spawner。

正确修复方向是给 TaskSpec 或 writer 增加一个一等语义，例如：

```json
{
  "kind": "get_subsystem",
  "subsystem_scope": "game_instance",
  "class_path": "/Script/BallMaze.InputRoutingSubsystem",
  "spawn_policy": "typed_getter"
}
```

或者在 function/action resolver 里把 `InputRoutingSubsystem::Get`、`Get Input Routing Subsystem`、`/Script/BallMaze.InputRoutingSubsystem` 解析到 UE 右键菜单同款 typed getter spawner。

### 蓝图形态降级原因

当前这版多重 IfElse 链不是业务逻辑上的最优形态。它是为了绕开 BlueprintHelper 当前写图能力限制而选择的保守降级方案。

C++ 中更自然的实现会是：

```cpp
for (int32 PlayerIndex = 0; PlayerIndex < 4; ++PlayerIndex)
{
    if (const int32* DeviceId = ControllerDeviceMap.Find(PlayerIndex))
    {
        InputRoutingSubsystem->BindInputDeviceToPlayer(*DeviceId, PlayerIndex);
    }
}
```

或者直接遍历 `ControllerDeviceMap`，再按合法玩家下标过滤。

这次没有生成这种紧凑蓝图，主要缺失在这些能力：

- 稳定生成 `ForLoop` 或 `ForEach` 节点，并把循环索引可靠复用到 `Map.Contains`、`Map.Find`、`LocalPlayerIndex`。
- 稳定生成局部变量或临时结果，把 `Map.Find` 的返回值作为 `DeviceId` 在同一轮循环里复用。
- 稳定表达 `Map.Find` 的“存在则取值”语义。蓝图里 Map Find/Contains 的 pin 组合和默认值需要精确落到节点上，当前 writer 已经暴露出 Key 默认值丢失问题。
- 稳定处理 impure Cast 的执行流。TaskSpec 把 Cast 写成表达式，但真实生成了带 exec pin 的 Cast，说明 writer 还不能保证表达式式 Cast 自动降成 PureCast。
- 在插件误读 `RestoreDevice` 函数图为空的前提下，只能先写 `EventGraph` 自定义事件，进一步降低了使用函数内部局部结构和已有图上下文的可控性。

所以多重 IfElse 的真实含义是“固定 0 到 3 玩家槽位的展开版循环”。它牺牲了优雅性，但本来应该降低 writer 对循环、局部变量、Map pin 默认值、Cast 模式的依赖。现在诊断结果说明，即使是这个降级方案，writer 仍然漏了 exec 链和默认值。

### 已正确生成的连接

- `K2Node_CustomEvent_6.then -> K2Node_IfThenElse_23.execute`
- 4 组 `ControllerDeviceMap -> Map_Contains.TargetMap`
- 4 组 `Map_Contains.ReturnValue -> Branch.Condition`
- 4 组 `ControllerDeviceMap -> Map_Find.TargetMap`
- 4 组 `Map_Find.Value -> BindInputDeviceToPlayer.DeviceId`
- 4 组 `GetGameInstanceSubsystem.ReturnValue -> DynamicCast.Object`
- 4 组 `DynamicCast.AsInput Routing Subsystem -> BindInputDeviceToPlayer.self`
- Player 0 到 2 的继续执行线：`Bind.then -> 下一组 Branch.execute`
- Player 0 到 2 的未命中执行线：`Branch.else -> 下一组 Branch.execute`

## 修复目标

需要让真实 Spawn 满足下面这些条件：

### 方案 A：Cast 改成 PureCast

如果 Cast 节点改成 PureCast，修复目标是：

```text
K2Node_IfThenElse_23.then -> K2Node_CallFunction_92.execute
K2Node_IfThenElse_24.then -> K2Node_CallFunction_96.execute
K2Node_IfThenElse_25.then -> K2Node_CallFunction_100.execute
K2Node_IfThenElse_26.then -> K2Node_CallFunction_104.execute
```

### 方案 B：保留当前 impure Cast

如果 Cast 保持带 exec pin，修复目标应该是：

```text
K2Node_IfThenElse_23.then -> K2Node_DynamicCast_16.execute
K2Node_DynamicCast_16.then -> K2Node_CallFunction_92.execute
K2Node_DynamicCast_16.CastFailed -> K2Node_IfThenElse_24.execute
K2Node_CallFunction_92.then -> K2Node_IfThenElse_24.execute

K2Node_IfThenElse_24.then -> K2Node_DynamicCast_17.execute
K2Node_DynamicCast_17.then -> K2Node_CallFunction_96.execute
K2Node_DynamicCast_17.CastFailed -> K2Node_IfThenElse_25.execute
K2Node_CallFunction_96.then -> K2Node_IfThenElse_25.execute

K2Node_IfThenElse_25.then -> K2Node_DynamicCast_18.execute
K2Node_DynamicCast_18.then -> K2Node_CallFunction_100.execute
K2Node_DynamicCast_18.CastFailed -> K2Node_IfThenElse_26.execute
K2Node_CallFunction_100.then -> K2Node_IfThenElse_26.execute

K2Node_IfThenElse_26.then -> K2Node_DynamicCast_19.execute
K2Node_DynamicCast_19.then -> K2Node_CallFunction_104.execute
```

### 共同默认值修复

不管 Cast 选择 PureCast 还是 impure Cast，都还需要修复这些默认值：

```text

K2Node_CallFunction_89.Key = 0
K2Node_CallFunction_90.Key = 0
K2Node_CallFunction_92.LocalPlayerIndex = 0

K2Node_CallFunction_93.Key = 1
K2Node_CallFunction_94.Key = 1
K2Node_CallFunction_96.LocalPlayerIndex = 1

K2Node_CallFunction_97.Key = 2
K2Node_CallFunction_98.Key = 2
K2Node_CallFunction_100.LocalPlayerIndex = 2

K2Node_CallFunction_101.Key = 3
K2Node_CallFunction_102.Key = 3
K2Node_CallFunction_104.LocalPlayerIndex = 3

K2Node_CallFunction_91.Class = /Script/BallMaze.InputRoutingSubsystem
K2Node_CallFunction_95.Class = /Script/BallMaze.InputRoutingSubsystem
K2Node_CallFunction_99.Class = /Script/BallMaze.InputRoutingSubsystem
K2Node_CallFunction_103.Class = /Script/BallMaze.InputRoutingSubsystem
```

### Map Find pin 修复

`Map_Find` 的结果符号必须绑定 `Value`，不能绑定 `ReturnValue`：

```text
RestoredDeviceId0 -> K2Node_CallFunction_90.Value
RestoredDeviceId1 -> K2Node_CallFunction_94.Value
RestoredDeviceId2 -> K2Node_CallFunction_98.Value
RestoredDeviceId3 -> K2Node_CallFunction_102.Value

K2Node_CallFunction_90.ReturnValue 保持 bool
K2Node_CallFunction_94.ReturnValue 保持 bool
K2Node_CallFunction_98.ReturnValue 保持 bool
K2Node_CallFunction_102.ReturnValue 保持 bool
```

### Subsystem Getter 修复

推荐最终形态是直接生成 UE 右键菜单同款 typed getter：

```text
Get Input Routing Subsystem -> BindInputDeviceToPlayer.self
```

而不是：

```text
GetGameInstanceSubsystem(Class=InputRoutingSubsystem) -> Cast to InputRoutingSubsystem -> BindInputDeviceToPlayer.self
```

如果短期 writer 还不能生成 typed getter，才保留通用 `GetGameInstanceSubsystem + Cast`，但需要同步修复 Cast 的 Pure/impure 执行语义。

## 执行结果原文

```json
{
  "schema": "BlueprintHelper.CliFullResult.v1",
  "toolResult": {
    "ok": true,
    "operation": "execute_task",
    "status": "completed",
    "modified": true,
    "target": {
      "target_type": "blueprint",
      "asset_path": "/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer"
    },
    "data": {
      "task_run_id": "task_F2F8B16F446D8F6ABF8157B18284200D",
      "task": {
        "feature_name": "RestoreDevice_CustomEventTemp",
        "applied_steps": 2,
        "modified_assets": 1
      }
    }
  }
}
```

## TaskSpec 原文

```json
{
  "schema": "BlueprintHelper.TaskPlan.v1",
  "task_name": "RestoreDevice_CustomEventTemp",
  "task_type": "edit_blueprint_graph",
  "target_assets": [
    "/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer"
  ],
  "execution_policy": {
    "dry_run_mode": "full",
    "should_compile": true,
    "should_save": false,
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "steps": [
    {
      "step_id": "step_001",
      "capability": "blueprint_signature",
      "target": {
        "asset_path": "/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer"
      },
      "write": {
        "strategy": "custom_event_signature",
        "ops": [
          {
            "op": "ensure_custom_event",
            "event_name": "CE_RestoreDevice",
            "graph_name": "EventGraph",
            "name_collision_policy": "reuse_if_exists"
          }
        ]
      }
    },
    {
      "step_id": "step_002",
      "capability": "graph_write",
      "target": {
        "asset_path": "/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer",
        "graph": "EventGraph"
      },
      "write": {
        "strategy": "owned_graph_edit",
        "ops": [
          {
            "op": "ensure_entry",
            "entry_type": "custom_event",
            "name": "CE_RestoreDevice",
            "signature_evidence_id": "signature:custom_event:CE_RestoreDevice",
            "body": {
              "schema": "BlueprintLogicSpec.v2",
              "statements": [
                {
                  "kind": "branch",
                  "condition": {
                    "kind": "container_action",
                    "id": "CE_RestoreDevice_stmt_1_condition",
                    "container_kind": "map",
                    "container_operation": "contains",
                    "key_type": "int",
                    "value_type": "int",
                    "target": {
                      "kind": "get",
                      "target": "ControllerDeviceMap",
                      "id": "CE_RestoreDevice_stmt_1_condition_target"
                    },
                    "key": {
                      "kind": "literal",
                      "value_type": "int",
                      "value": 0,
                      "id": "CE_RestoreDevice_stmt_1_condition_key"
                    },
                    "context_evidence": {
                      "container.kind": "map",
                      "container.operation": "contains",
                      "container.collection_pin_type": "map<int,int>",
                      "container.key_pin_type": "int",
                      "container.value_pin_type": "int"
                    }
                  },
                  "then": [
                    {
                      "kind": "container_action",
                      "id": "CE_RestoreDevice_stmt_1_then_1",
                      "container_kind": "map",
                      "container_operation": "find",
                      "key_type": "int",
                      "value_type": "int",
                      "target": {
                        "kind": "get",
                        "target": "ControllerDeviceMap",
                        "id": "CE_RestoreDevice_stmt_1_then_1_target"
                      },
                      "key": {
                        "kind": "literal",
                        "value_type": "int",
                        "value": 0,
                        "id": "CE_RestoreDevice_stmt_1_then_1_key"
                      },
                      "result_symbol": "RestoredDeviceId0",
                      "context_evidence": {
                        "container.kind": "map",
                        "container.operation": "find",
                        "container.collection_pin_type": "map<int,int>",
                        "container.key_pin_type": "int",
                        "container.value_pin_type": "int"
                      }
                    },
                    {
                      "kind": "call",
                      "target": "/Script/BallMaze.InputRoutingSubsystem:BindInputDeviceToPlayer",
                      "target_object": {
                        "kind": "convert",
                        "transform_operation": "dynamic_cast",
                        "target_class_path": "/Script/BallMaze.InputRoutingSubsystem",
                        "context_evidence": {
                          "generic.transform.operation": "dynamic_cast",
                          "generic.transform.source_pin_type": "object",
                          "generic.transform.target_pin_type": "/Script/BallMaze.InputRoutingSubsystem"
                        },
                        "args": {
                          "value": {
                            "kind": "call",
                            "target": "/Script/Engine.SubsystemBlueprintLibrary:GetGameInstanceSubsystem",
                            "args": {
                              "Class": {
                                "kind": "literal",
                                "value_type": "class",
                                "value": "/Script/BallMaze.InputRoutingSubsystem"
                              }
                            }
                          }
                        }
                      },
                      "args": {
                        "DeviceId": {
                          "kind": "get",
                          "target": "RestoredDeviceId0",
                          "id": "CE_RestoreDevice_stmt_1_then_2_arg_DeviceId"
                        },
                        "LocalPlayerIndex": {
                          "kind": "literal",
                          "value_type": "int",
                          "value": 0,
                          "id": "CE_RestoreDevice_stmt_1_then_2_arg_LocalPlayerIndex"
                        }
                      },
                      "id": "CE_RestoreDevice_stmt_1_then_2"
                    }
                  ],
                  "else": [],
                  "id": "CE_RestoreDevice_stmt_1"
                },
                {
                  "kind": "branch",
                  "condition": {
                    "kind": "container_action",
                    "id": "CE_RestoreDevice_stmt_2_condition",
                    "container_kind": "map",
                    "container_operation": "contains",
                    "key_type": "int",
                    "value_type": "int",
                    "target": {
                      "kind": "get",
                      "target": "ControllerDeviceMap",
                      "id": "CE_RestoreDevice_stmt_2_condition_target"
                    },
                    "key": {
                      "kind": "literal",
                      "value_type": "int",
                      "value": 1,
                      "id": "CE_RestoreDevice_stmt_2_condition_key"
                    },
                    "context_evidence": {
                      "container.kind": "map",
                      "container.operation": "contains",
                      "container.collection_pin_type": "map<int,int>",
                      "container.key_pin_type": "int",
                      "container.value_pin_type": "int"
                    }
                  },
                  "then": [
                    {
                      "kind": "container_action",
                      "id": "CE_RestoreDevice_stmt_2_then_1",
                      "container_kind": "map",
                      "container_operation": "find",
                      "key_type": "int",
                      "value_type": "int",
                      "target": {
                        "kind": "get",
                        "target": "ControllerDeviceMap",
                        "id": "CE_RestoreDevice_stmt_2_then_1_target"
                      },
                      "key": {
                        "kind": "literal",
                        "value_type": "int",
                        "value": 1,
                        "id": "CE_RestoreDevice_stmt_2_then_1_key"
                      },
                      "result_symbol": "RestoredDeviceId1",
                      "context_evidence": {
                        "container.kind": "map",
                        "container.operation": "find",
                        "container.collection_pin_type": "map<int,int>",
                        "container.key_pin_type": "int",
                        "container.value_pin_type": "int"
                      }
                    },
                    {
                      "kind": "call",
                      "target": "/Script/BallMaze.InputRoutingSubsystem:BindInputDeviceToPlayer",
                      "target_object": {
                        "kind": "convert",
                        "transform_operation": "dynamic_cast",
                        "target_class_path": "/Script/BallMaze.InputRoutingSubsystem",
                        "context_evidence": {
                          "generic.transform.operation": "dynamic_cast",
                          "generic.transform.source_pin_type": "object",
                          "generic.transform.target_pin_type": "/Script/BallMaze.InputRoutingSubsystem"
                        },
                        "args": {
                          "value": {
                            "kind": "call",
                            "target": "/Script/Engine.SubsystemBlueprintLibrary:GetGameInstanceSubsystem",
                            "args": {
                              "Class": {
                                "kind": "literal",
                                "value_type": "class",
                                "value": "/Script/BallMaze.InputRoutingSubsystem"
                              }
                            }
                          }
                        }
                      },
                      "args": {
                        "DeviceId": {
                          "kind": "get",
                          "target": "RestoredDeviceId1",
                          "id": "CE_RestoreDevice_stmt_2_then_2_arg_DeviceId"
                        },
                        "LocalPlayerIndex": {
                          "kind": "literal",
                          "value_type": "int",
                          "value": 1,
                          "id": "CE_RestoreDevice_stmt_2_then_2_arg_LocalPlayerIndex"
                        }
                      },
                      "id": "CE_RestoreDevice_stmt_2_then_2"
                    }
                  ],
                  "else": [],
                  "id": "CE_RestoreDevice_stmt_2"
                },
                {
                  "kind": "branch",
                  "condition": {
                    "kind": "container_action",
                    "id": "CE_RestoreDevice_stmt_3_condition",
                    "container_kind": "map",
                    "container_operation": "contains",
                    "key_type": "int",
                    "value_type": "int",
                    "target": {
                      "kind": "get",
                      "target": "ControllerDeviceMap",
                      "id": "CE_RestoreDevice_stmt_3_condition_target"
                    },
                    "key": {
                      "kind": "literal",
                      "value_type": "int",
                      "value": 2,
                      "id": "CE_RestoreDevice_stmt_3_condition_key"
                    },
                    "context_evidence": {
                      "container.kind": "map",
                      "container.operation": "contains",
                      "container.collection_pin_type": "map<int,int>",
                      "container.key_pin_type": "int",
                      "container.value_pin_type": "int"
                    }
                  },
                  "then": [
                    {
                      "kind": "container_action",
                      "id": "CE_RestoreDevice_stmt_3_then_1",
                      "container_kind": "map",
                      "container_operation": "find",
                      "key_type": "int",
                      "value_type": "int",
                      "target": {
                        "kind": "get",
                        "target": "ControllerDeviceMap",
                        "id": "CE_RestoreDevice_stmt_3_then_1_target"
                      },
                      "key": {
                        "kind": "literal",
                        "value_type": "int",
                        "value": 2,
                        "id": "CE_RestoreDevice_stmt_3_then_1_key"
                      },
                      "result_symbol": "RestoredDeviceId2",
                      "context_evidence": {
                        "container.kind": "map",
                        "container.operation": "find",
                        "container.collection_pin_type": "map<int,int>",
                        "container.key_pin_type": "int",
                        "container.value_pin_type": "int"
                      }
                    },
                    {
                      "kind": "call",
                      "target": "/Script/BallMaze.InputRoutingSubsystem:BindInputDeviceToPlayer",
                      "target_object": {
                        "kind": "convert",
                        "transform_operation": "dynamic_cast",
                        "target_class_path": "/Script/BallMaze.InputRoutingSubsystem",
                        "context_evidence": {
                          "generic.transform.operation": "dynamic_cast",
                          "generic.transform.source_pin_type": "object",
                          "generic.transform.target_pin_type": "/Script/BallMaze.InputRoutingSubsystem"
                        },
                        "args": {
                          "value": {
                            "kind": "call",
                            "target": "/Script/Engine.SubsystemBlueprintLibrary:GetGameInstanceSubsystem",
                            "args": {
                              "Class": {
                                "kind": "literal",
                                "value_type": "class",
                                "value": "/Script/BallMaze.InputRoutingSubsystem"
                              }
                            }
                          }
                        }
                      },
                      "args": {
                        "DeviceId": {
                          "kind": "get",
                          "target": "RestoredDeviceId2",
                          "id": "CE_RestoreDevice_stmt_3_then_2_arg_DeviceId"
                        },
                        "LocalPlayerIndex": {
                          "kind": "literal",
                          "value_type": "int",
                          "value": 2,
                          "id": "CE_RestoreDevice_stmt_3_then_2_arg_LocalPlayerIndex"
                        }
                      },
                      "id": "CE_RestoreDevice_stmt_3_then_2"
                    }
                  ],
                  "else": [],
                  "id": "CE_RestoreDevice_stmt_3"
                },
                {
                  "kind": "branch",
                  "condition": {
                    "kind": "container_action",
                    "id": "CE_RestoreDevice_stmt_4_condition",
                    "container_kind": "map",
                    "container_operation": "contains",
                    "key_type": "int",
                    "value_type": "int",
                    "target": {
                      "kind": "get",
                      "target": "ControllerDeviceMap",
                      "id": "CE_RestoreDevice_stmt_4_condition_target"
                    },
                    "key": {
                      "kind": "literal",
                      "value_type": "int",
                      "value": 3,
                      "id": "CE_RestoreDevice_stmt_4_condition_key"
                    },
                    "context_evidence": {
                      "container.kind": "map",
                      "container.operation": "contains",
                      "container.collection_pin_type": "map<int,int>",
                      "container.key_pin_type": "int",
                      "container.value_pin_type": "int"
                    }
                  },
                  "then": [
                    {
                      "kind": "container_action",
                      "id": "CE_RestoreDevice_stmt_4_then_1",
                      "container_kind": "map",
                      "container_operation": "find",
                      "key_type": "int",
                      "value_type": "int",
                      "target": {
                        "kind": "get",
                        "target": "ControllerDeviceMap",
                        "id": "CE_RestoreDevice_stmt_4_then_1_target"
                      },
                      "key": {
                        "kind": "literal",
                        "value_type": "int",
                        "value": 3,
                        "id": "CE_RestoreDevice_stmt_4_then_1_key"
                      },
                      "result_symbol": "RestoredDeviceId3",
                      "context_evidence": {
                        "container.kind": "map",
                        "container.operation": "find",
                        "container.collection_pin_type": "map<int,int>",
                        "container.key_pin_type": "int",
                        "container.value_pin_type": "int"
                      }
                    },
                    {
                      "kind": "call",
                      "target": "/Script/BallMaze.InputRoutingSubsystem:BindInputDeviceToPlayer",
                      "target_object": {
                        "kind": "convert",
                        "transform_operation": "dynamic_cast",
                        "target_class_path": "/Script/BallMaze.InputRoutingSubsystem",
                        "context_evidence": {
                          "generic.transform.operation": "dynamic_cast",
                          "generic.transform.source_pin_type": "object",
                          "generic.transform.target_pin_type": "/Script/BallMaze.InputRoutingSubsystem"
                        },
                        "args": {
                          "value": {
                            "kind": "call",
                            "target": "/Script/Engine.SubsystemBlueprintLibrary:GetGameInstanceSubsystem",
                            "args": {
                              "Class": {
                                "kind": "literal",
                                "value_type": "class",
                                "value": "/Script/BallMaze.InputRoutingSubsystem"
                              }
                            }
                          }
                        }
                      },
                      "args": {
                        "DeviceId": {
                          "kind": "get",
                          "target": "RestoredDeviceId3",
                          "id": "CE_RestoreDevice_stmt_4_then_2_arg_DeviceId"
                        },
                        "LocalPlayerIndex": {
                          "kind": "literal",
                          "value_type": "int",
                          "value": 3,
                          "id": "CE_RestoreDevice_stmt_4_then_2_arg_LocalPlayerIndex"
                        }
                      },
                      "id": "CE_RestoreDevice_stmt_4_then_2"
                    }
                  ],
                  "else": [],
                  "id": "CE_RestoreDevice_stmt_4"
                }
              ]
            }
          }
        ]
      },
      "constraints": {
        "allow_modify_user_nodes": false,
        "ownership_scope": "blueprinthelper_owned"
      },
      "depends_on": [
        "step_001"
      ]
    }
  ]
}
```
