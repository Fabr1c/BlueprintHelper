# RestoreDevice TaskSpec Spawn 审计报告

生成时间：2026-05-31 19:27:56 +08:00

## 来源

- TaskSpec：`Saved\BlueprintHelper\Cli\preview_1780216464697_0001\task_plan.json`
- 执行结果：`Saved\BlueprintHelper\Cli\task_F2F8B16F446D8F6ABF8157B18284200D\result.json`
- 诊断 logicJson：`C:\Users\26227\Desktop\新建 文本文档.txt`
- Review archive：`Saved\BlueprintHelper\Review\ArchiveSessions\archive_BBCE7C82435C2A156DEFB9B7BDF7942D.json`
- 目标蓝图：`/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer`
- 临时入口：`CE_RestoreDevice`

## 总结

这次写图不是单一连线错误，而是多个层面的语义不等价：

- TaskSpec 的业务期望是恢复 0 到 3 号本地玩家原先绑定的设备。
- 真实 Spawn 存在执行链、默认值、Cast 形态、Map Find 返回 pin、Subsystem Getter 形态、ReviewRecord 生命周期问题。
- 多重 IfElse 是对循环写图能力不足的保守降级，不是最终推荐的蓝图实现。
- 执行成功不等于审计成功。这次只生成了 ArchiveSession，没有生成 ReviewRecord。

## TaskSpec 期望逻辑

语义上等价于：

```cpp
for (int32 PlayerIndex = 0; PlayerIndex < 4; ++PlayerIndex)
{
    if (const int32* DeviceId = ControllerDeviceMap.Find(PlayerIndex))
    {
        InputRoutingSubsystem->BindInputDeviceToPlayer(*DeviceId, PlayerIndex);
    }
}
```

TaskSpec 展开成 4 组固定玩家槽位：

| Player | Contains.Key | Find.Key | Bind.LocalPlayerIndex | Subsystem |
| --- | ---: | ---: | ---: | --- |
| 0 | 0 | 0 | 0 | InputRoutingSubsystem |
| 1 | 1 | 1 | 1 | InputRoutingSubsystem |
| 2 | 2 | 2 | 2 | InputRoutingSubsystem |
| 3 | 3 | 3 | 3 | InputRoutingSubsystem |

## 真实 Spawn 差距

### 1. 执行链缺失

诊断版 logicJson 中，4 组 Branch.then 没有进入对应绑定调用。

如果 Cast 是 PureCast，缺失的是：

```text
K2Node_IfThenElse_23.then -> K2Node_CallFunction_92.execute
K2Node_IfThenElse_24.then -> K2Node_CallFunction_96.execute
K2Node_IfThenElse_25.then -> K2Node_CallFunction_100.execute
K2Node_IfThenElse_26.then -> K2Node_CallFunction_104.execute
```

但真实 Spawn 出来的 Cast 是带 exec pin 的 impure Cast，所以更准确的目标应是：

```text
Branch.then -> Cast.execute -> CastSucceeded -> Bind.execute
CastFailed -> 下一组玩家 Branch.execute
Bind.then -> 下一组玩家 Branch.execute
```

### 2. 默认值缺失或错误

诊断版 logicJson 中这些输入没有正确落值：

| Player | 节点 | Pin | 期望 | 真实问题 |
| --- | --- | --- | --- | --- |
| 0 | K2Node_CallFunction_89 | Key | 0 | inputs 为空 |
| 0 | K2Node_CallFunction_90 | Key | 0 | inputs 为空 |
| 0 | K2Node_CallFunction_91 | Class | /Script/BallMaze.InputRoutingSubsystem | inputs 为空 |
| 1 | K2Node_CallFunction_93 | Key | 1 | inputs 为空 |
| 1 | K2Node_CallFunction_94 | Key | 1 | inputs 为空 |
| 1 | K2Node_CallFunction_95 | Class | /Script/BallMaze.InputRoutingSubsystem | inputs 为空 |
| 1 | K2Node_CallFunction_96 | LocalPlayerIndex | 1 | 生成成 0 |
| 2 | K2Node_CallFunction_97 | Key | 2 | inputs 为空 |
| 2 | K2Node_CallFunction_98 | Key | 2 | inputs 为空 |
| 2 | K2Node_CallFunction_99 | Class | /Script/BallMaze.InputRoutingSubsystem | inputs 为空 |
| 2 | K2Node_CallFunction_100 | LocalPlayerIndex | 2 | 生成成 0 |
| 3 | K2Node_CallFunction_101 | Key | 3 | inputs 为空 |
| 3 | K2Node_CallFunction_102 | Key | 3 | inputs 为空 |
| 3 | K2Node_CallFunction_103 | Class | /Script/BallMaze.InputRoutingSubsystem | inputs 为空 |
| 3 | K2Node_CallFunction_104 | LocalPlayerIndex | 3 | 生成成 0 |

### 3. Cast 没有显式 PureCast

TaskSpec 里 Cast 写法是：

```json
"target_object": {
  "kind": "convert",
  "transform_operation": "dynamic_cast",
  "target_class_path": "/Script/BallMaze.InputRoutingSubsystem"
}
```

这里没有 pure: true、cast_mode: pure、IsPureCast: true 之类字段。TaskSpec 把 Cast 当成表达式使用，语义上更接近 PureCast，但 writer 实际 Spawn 了带 exec pin 的 impure Cast。

这会导致数据线看起来存在：

```text
DynamicCast.AsInput Routing Subsystem -> BindInputDeviceToPlayer.self
```

但 Cast 本身没有执行路径，不能保证输出有效。

### 4. Map Find 返回 pin 错误

UE 正确的 Map Find 节点语义是：

```text
Map_Find.Value -> DeviceId
Map_Find.ReturnValue -> bool 是否找到
```

当前错误信息：

```text
整数类型的 Return Value 和属性ReturnValue（属于BoolProperty类型）不匹配
```

说明生成链路把 esult_symbol 绑定到了 ReturnValue，但对 Map_Find 来说 ReturnValue 是 bool，不是 map value。

源码证据：

- container.map.find 能力证据实际声明 result 应绑定 Value：`Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\Utils\GraphWriteActionEvidenceUtils.cpp:900`
- 但 TaskSpec compiler 对 container_action 通用返回 pin 使用 ReturnValue：`Plugins\BlueprintHelper\AgentFaceService\task-core\src\task\compiler\task-compiler.ts:2380`
- 语句 result_symbol 也注册成 ${nodeId}.ReturnValue：`Plugins\BlueprintHelper\AgentFaceService\task-core\src\task\compiler\task-compiler.ts:2759`

正确修复方向：

```text
container.map.find.result_symbol -> Map_Find.Value
Map_Find.ReturnValue 保持 bool
Map_Find.Value 按 value_type=int 解析
```

### 5. Subsystem Getter 节点形态不等价

UE 右键菜单正确 Spawn 的是 typed getter：

```text
Get Input Routing Subsystem -> BindInputDeviceToPlayer.self
```

这次 TaskSpec 写的是：

```text
GetGameInstanceSubsystem(Class=InputRoutingSubsystem)
-> Cast to InputRoutingSubsystem
-> BindInputDeviceToPlayer.self
```

所以真实 Spawn 成 GetGameInstanceSubsystem + Cast 符合 TaskSpec 字面描述，但不符合 UE 右键菜单的最优节点形态。

缺失能力：

- TaskSpec 没有一等表达 get_subsystem 或 	yped_getter。
- writer 没有把 /Script/BallMaze.InputRoutingSubsystem 映射到 UE 右键菜单的 Get Input Routing Subsystem ActionDatabase spawner。
- 当前路径走 function call/generic convert，而不是 typed subsystem getter 专用节点。

建议语义形态：

```json
{
  "kind": "get_subsystem",
  "subsystem_scope": "game_instance",
  "class_path": "/Script/BallMaze.InputRoutingSubsystem",
  "spawn_policy": "typed_getter"
}
```

### 6. 多重 IfElse 是能力降级

多重 IfElse 链路不是业务逻辑最优形态，只是固定玩家 0 到 3 的展开版循环。

选择这种形态的原因：

- 当前 writer 不能稳定生成 ForLoop 或 ForEach，并可靠复用循环索引。
- 当前 writer 不能稳定生成局部变量或临时结果承接 Map.Find 的值。
- Map 的 wildcard pin、Key 默认值、Value 输出 pin 需要精确类型传播，当前已经暴露出缺陷。
- Cast 模式不能稳定控制 Pure/Impure。
- 插件当时误读 RestoreDevice 函数图为空，只能临时写 EventGraph custom event。

所以 IfElse 是为了降低 writer 对循环和局部变量的依赖，但实际结果说明这个降级方案仍然不够可靠。

### 7. ReviewRecord 缺失

这次执行后，Saved\BlueprintHelper\Review 下只有：

```text
ArchiveSessions
Snapshots
```

没有：

```text
Records
```

rchive_BBCE7C82435C2A156DEFB9B7BDF7942D.json 里记录：

```json
{
  "archive_session_id": "archive_BBCE7C82435C2A156DEFB9B7BDF7942D",
  "task_run_id": "task_F2F8B16F446D8F6ABF8157B18284200D",
  "baseline": {
    "dirty_asset_policy": "save_before_archive",
    "snapshot_trust": "saved_before_archive"
  }
}
```

这个 archive session 是 baseline 归档，不是已审计的 ReviewEvent。

ReviewPanel debug bundle 显示：

```text
ReviewAssetContext asset="" package="" object="" kind=unknown valid=0 blueprint=0 detailsSurface=details
GraphEditor hidden reason=no_selected_change
```

所以真实问题是执行后没有生成可审计的 ReviewRecord。面板没有 ReviewEvent 是符合当前磁盘状态的。

运行时代码预期 ReviewRecord 写入目录：

```cpp
return GetReviewRootDir() / TEXT("Records");
```

ReviewRecord 只有在 ReviewEvidences.Num() > 0 时才会写入：

```cpp
if (Batch.ReviewEvidences.Num() > 0)
{
    ReviewStore.BuildReviewRecordsFromEvidence(Batch.ReviewEvidences);
    ReviewStore.SaveReviewRecords(ReviewRecords, ReviewRecordError);
}
```

这次 CLI 执行结果只返回了成功和 task_run_id，没有返回 rchive_session_id、eview_record_id 或 pending review 数量。因此执行成功和审计成功被错误地混在了一起。

后续必须加的检查：

```text
execute_task 成功
-> query_review_records(task_run_id 或 archive_session_id)
-> 如果 0 条，报告 review_record_missing
```

### 8. Systematic Debug 结论：execute success 与 Review ready 被错误混同

并行 explorer 与本地主线源码追踪确认：这次更严重的问题不是 ReviewPanel bug，而是 runtime / PostIo / agent-facing execute contract 没有把 ReviewRecord 完整性作为可观测状态暴露。

ReviewPanel debug bundle 里的空状态是正确投影：

```text
ReviewAssetContext asset="" package="" object="" kind=unknown valid=0 blueprint=0 detailsSurface=details
GraphEditor hidden reason=no_selected_change
```

它说明当前 canonical ReviewStore 里没有可选中的 ReviewRecord / VisibleChange。面板不应该在本地猜测 archive-only，也不应该从 ArchiveSession 自行构造 ReviewEvent。

代码链路：

- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:5486-5699`
  - real execute 会启用 pending review notify，并创建 `ArchiveSession`。
  - archive baseline 捕获和 ReviewRecord 生成不是同一个不变量。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:6184-6281`
  - 每个 step 只有在 `StepResult.bOk` 后，且 pre-step evidence 或 cluster `BuildReviewEvidence` 成功时，才会 `PostIoBatch.AddReviewEvidence(RuntimeEvidence)`。
  - 这意味着“资产已修改 + archive 已保存”仍可能没有任何 `ReviewEvidence`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.cpp:20-80`
  - `SaveArchiveSession` 独立执行。
  - `SaveReviewRecords` 只在 `Batch.ReviewEvidences.Num() > 0` 时执行。
  - `ReviewEvidences.Num() == 0` 时不会产生诊断。
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatch.cpp:8-25`
  - `post_io` 当前只暴露 `ok` 和 `diagnostics`。
  - 没有 `archive_session_id`、`review_evidence_count`、`review_record_count`、`review_record_ids`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp:123-220`
  - `BuildReviewRecordsFromEvidence` 会丢弃缺 `ArchiveSessionId` / `AssetPath` 的 evidence。
  - 如果最终 `VisibleChanges.Num() == 0`，也不会产出 ReviewRecord。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp:302-360`
  - `QueryReviewRecords` 只扫描 `Records/*.json`。
  - 如果没有 Records，它无法区分 archive-only、被 filter 掉、还是 records 写入失败。
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp:1318-1338`
  - `query_review_records` 只返回 `records` 和 `count`，没有 session state。
- `AgentFaceService/task-core/src/task/service/task-spec-runner.ts:121-163` 和 `:450-480`
  - `blueprinthelper_execute_task` 只把 `task_run_id` 与 task summary 暴露给 agent/CLI。
  - `debug.bridge_result` 是 non-enumerable，不会成为正常 CLI/MCP 结果契约。
  - 没有自动 follow-up `query_review_records`。

当前 runtime 允许以下状态静默成功：

```text
execute_task_plan ok
ArchiveSession saved
ReviewEvidences empty
ReviewRecords not written
query_review_records count=0
ReviewPanel no_selected_change
```

这个状态对 UI 来说是正确空状态；对 agent-facing execute contract 来说是危险假阳性。

根因拆分：

1. Runtime/PostIo 缺少“mutating execute 必须产生 Review 结果状态”的不变量。
2. PostIo 缺少 review evidence / record 计数与 id 的结构化输出。
3. `execute_task` 没有把 Review 状态映射到 agent-facing 结果。
4. `query_review_records` 只能查询 Records，不能表达 archive-only session。
5. ReviewPanel 和 DebugBundle 消费的是 canonical ReviewRecord，因此它们在 0 Records 下显示 no selected change 是正确行为。

后续必须加的检查：

```text
execute_task 成功
-> runtime 返回 archive_session_id + review_evidence_count + review_record_count
-> query_review_records(task_run_id 或 archive_session_id) 能区分 review_ready / archive_only / filtered_empty
-> 如果 mutating execute 产生 archive session 但 ReviewRecord 为 0，报告 review_record_missing
```

推荐修复边界：

- Runtime/PostIo 是主修复点，不应在 ReviewPanel 里猜测 archive-only。
- `FBlueprintHelperTaskRuntimePostIoFlushResult` 增加：
  - `archive_session_id`
  - `review_evidence_count`
  - `review_record_count`
  - `review_record_ids`
  - `review_state`: `review_ready | archive_only | evidence_zero_records | write_failed`
- `FBlueprintHelperTaskRuntimePostIoService::Flush` 对 `ArchiveSession.IsSet() && ReviewEvidences.Num() == 0` 产生 `review_record_missing` 诊断或 warning。
- `execute_task` 正常结果必须暴露 Review summary；至少包含 `archive_session_id`、`review_record_count` 和 warning。
- `query_review_records` 应补充 session-state summary，避免 `count=0` 同时表示多种状态。
- ReviewPanel 后续可以消费同一套 Review 数据模型显示 archive-only 状态，但不应维护本地特殊解释。

最小回归测试建议：

- C++ PostIo：archive session 存在、0 evidence 时，flush 输出明确 `review_record_missing` / `archive_only`。
- C++ ReviewStore/Bridge：`query_review_records` 能区分 archive-only 与真正无 session。
- C++ Runtime：`blueprint_signature + graph_write owned_graph_edit` 成功执行后，`post_io.review_record_count > 0`。
- TypeScript runner：`execute_task` 成功结果包含 Review summary；缺失时输出明确 warning，不再只返回 task_run_id。

## 修复目标汇总

### 图生成修复

- Map_Find 的 result_symbol 绑定到 Value，不要绑定 ReturnValue。
- Map_Find.ReturnValue 保持 bool。
- Contains.Key、Find.Key、LocalPlayerIndex 正确落默认值。
- GetGameInstanceSubsystem.Class 如果仍使用通用节点，必须正确落类默认值。
- Cast 要么显式 PureCast，要么完整连接 impure Cast 的 exec、then、CastFailed。
- 优先支持 typed Get Input Routing Subsystem 节点，避免通用 Get + Cast。

### TaskSpec 能力修复

- 增加 get_subsystem 或 typed getter 一等语义。
- 增加容器 action 输出 pin 映射，至少区分 Map_Find.Value 与 ReturnValue。
- 增加 PureCast/ImpureCast 显式模式。
- 增加循环与局部变量稳定写图能力，减少展开式 IfElse。

### Review 流程修复

- execute_task 返回 rchive_session_id。
- execute_task 返回生成的 eview_record_id 或 pending review summary。
- 执行完成后自动 query review records。
- 如果 archive session 存在但 ReviewRecord 为 0，应产生 warning 或 failure。
- ReviewPanel 应能显示 archive-only 状态，避免用户误以为已经审计归档。

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

## ArchiveSession 原文

```json
{
	"schema": "BlueprintHelper.ArchiveSession.v2",
	"archive_session_id": "archive_BBCE7C82435C2A156DEFB9B7BDF7942D",
	"task_run_id": "task_F2F8B16F446D8F6ABF8157B18284200D",
	"allowed_target_assets": [
		"/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer"
	],
	"baseline_snapshot_refs": [
		"review://archive/archive_BBCE7C82435C2A156DEFB9B7BDF7942D/baseline/_Game_Gameplay_Core_LocalMpMode_BP_BallMazeGameMode_LocalMultiPlayer.uasset"
	],
	"baseline_semantic_snapshot_refs": [
		"review://archive/archive_BBCE7C82435C2A156DEFB9B7BDF7942D/baseline/_Game_Gameplay_Core_LocalMpMode_BP_BallMazeGameMode_LocalMultiPlayer_d3122db2/baseline.semantic.json"
	],
	"baseline":
	{
		"dirty_asset_policy": "save_before_archive",
		"snapshot_trust": "saved_before_archive",
		"dirty_target_assets": [
			"/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer"
		],
		"warnings": [],
		"disk_snapshot_refs": [
			"review://archive/archive_BBCE7C82435C2A156DEFB9B7BDF7942D/baseline/_Game_Gameplay_Core_LocalMpMode_BP_BallMazeGameMode_LocalMultiPlayer.uasset"
		],
		"semantic_snapshot_refs": [
			"review://archive/archive_BBCE7C82435C2A156DEFB9B7BDF7942D/baseline/_Game_Gameplay_Core_LocalMpMode_BP_BallMazeGameMode_LocalMultiPlayer_d3122db2/baseline.semantic.json"
		]
	},
	"created_at": "2026-05-31T08:34:45.042Z"
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
``

