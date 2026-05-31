# GraphWrite RestoreDevice 根因确认

日期：2026-05-31

## 结论（现场证据更新后）

根因不在 TypeScript TaskSpec lowering，也不在 `dynamic_cast` 纯化本身。真实现场的新证据进一步修正了原判断：这次 `RestoreDevice_CustomEventTemp` run 的 GraphWrite 写入没有失败，`execute_task` 返回 `ok=true` / `status=completed` / `modified=true`，并记录 `applied_steps=2`、`modified_assets=1`。

因此，这次真实现场的 ReviewEvent / ReviewRecord 缺失，不能再归因成 `owned_replace_target_not_blueprinthelper_owned` 或 GraphWrite 写图失败。此前自动化里确认过的 ownership gate 仍然成立，但它对应的是 `replace_body/custom_event_body -> replace_blueprint_graph` 分支；用户提供的现场 TaskPlan 是 `ensure_entry(custom_event) -> append_blueprint_graph` 分支，而且该 run 已成功执行。

当前可以确认的断点是：成功修改蓝图之后，Review evidence / PostIO / ReviewStore 的持久化链路没有产出 `Review/Records`。现场文件系统里 `ArchiveSessions/<archive_session_id>.json` 存在，但 `Review/Records` 目录不存在；源码上 `SaveReviewRecords` 会先创建 `Records` 目录，所以这能证明 `SaveArchiveSession` 已执行，而 `SaveReviewRecords` 没有被调用。

剩余未被当前证据区分的二级分支只有两个：

1. `PostIoBatch.ReviewEvidences.Num()==0`，即成功 step 没有被加入 ReviewEvidence batch。
2. `ReviewEvidences.Num()>0`，但 `BuildReviewRecordsFromEvidence(...)` 产出 0 条记录，PostIO 只产生 `review_evidence_produced_zero_records` 诊断而不写 `Records`。

普通 CLI execute 结果没有暴露 raw `post_io`、`review_evidence_count`、`review_record_count` 或 record ids，所以这批现场文件足以确认断点区间，但还不能在上述两个二级分支之间二选一。

## 补充：实际 bug TaskPlan 证据链

用户提供的 bug 现场 compiled TaskPlan：

- `D:/UEProjects/Template/Plugins/BlueprintHelper/.tmp/tmp_restore_device_custom_event.json`

这份文件补全了编译产物层的证据，但也修正了一个边界：它不是 `replace_body` 形态，而是 `ensure_entry(custom_event)` 形态。

TaskPlan 关键字段：

- `schema=BlueprintHelper.TaskPlan.v1`
- `task_name=RestoreDevice_CustomEventTemp`
- `target_assets[0]=/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer`
- `execution_policy.dry_run_mode=full`
- `execution_policy.should_compile=true`
- `execution_policy.should_save=false`
- `execution_policy.review_baseline_dirty_asset_policy=save_before_archive`
- `step_001.capability=blueprint_signature`
- `step_001.write.strategy=custom_event_signature`
- `step_001.write.ops[0].op=ensure_custom_event`
- `step_001.write.ops[0].event_name=CE_RestoreDevice`
- `step_001.write.ops[0].graph_name=EventGraph`
- `step_002.capability=graph_write`
- `step_002.target.graph=EventGraph`
- `step_002.write.strategy=owned_graph_edit`
- `step_002.write.ops[0].op=ensure_entry`
- `step_002.write.ops[0].entry_type=custom_event`
- `step_002.write.ops[0].name=CE_RestoreDevice`
- `step_002.write.ops[0].signature_evidence_id=signature:custom_event:CE_RestoreDevice`
- `step_002.constraints.allow_modify_user_nodes=false`
- `step_002.constraints.ownership_scope=blueprinthelper_owned`
- `step_002.depends_on[0]=step_001`

Body 结构：

- 4 个 `branch`
- 4 个 `container_action map.contains`
- 4 个 `container_action map.find`
- 4 个 `dynamic_cast` 到 `/Script/BallMaze.InputRoutingSubsystem`
- 4 个 `/Script/BallMaze.InputRoutingSubsystem:BindInputDeviceToPlayer`

因此该 TaskPlan 能证明：

1. Agent/TaskSpec 编译层确实产生了 `blueprint_signature.ensure_custom_event -> graph_write.owned_graph_edit.ensure_entry` 的依赖链。
2. GraphWrite step 明确要求 `ownership_scope=blueprinthelper_owned`，并拒绝修改 user nodes。
3. 逻辑 body 已经在 TaskPlan 内完整存在；如果蓝图内确实生成了对应函数逻辑 node，问题不在 TS 编译 body 缺失。

但它不能单独证明：

1. 现场运行时一定触发了 `owned_replace_target_not_blueprinthelper_owned`，因为该 TaskPlan 没有 `replace_body` / `replace_scope=custom_event_body`。
2. ReviewEvent 未生成的最后一跳一定是 ownership gate，而不是 `BuildReviewEvidence` / `PostIo` / `ReviewStore` 后续过滤。

UE runtime 合同说明 `ensure_entry(custom_event)` 会降成 `append_blueprint_graph`，而 `replace_body` 才会降成 `replace_blueprint_graph`：

- `AgentFaceService/docs/TaskSpec_TaskPlan_Contract.md:824`
- `AgentFaceService/docs/TaskSpec_TaskPlan_Contract.md:952-955`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:3017-3021`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp:3033-3036`

所以当前证据链应拆成两条：

- 已确认的自动化复现链：`ensure_custom_event -> replace_body/custom_event_body -> replace_blueprint_graph`，失败码为 `owned_replace_target_not_blueprinthelper_owned`。
- 用户现场 TaskPlan 链：`ensure_custom_event -> ensure_entry/custom_event -> append_blueprint_graph`。它最初只能证明同类 handoff 风险存在；后续 execute / filesystem 证据已把真实现场断点收窄到写图成功后的 ReviewEvidence / PostIO / ReviewStore 持久化链路。

后续用户补充的现场文件补齐了关键 run 级证据：

- `D:/UEProjects/Template/Plugins/BlueprintHelper/.tmp/tmp_restore_device_execute_result.json`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/.tmp/tmp_restore_device_query_review_records_request.json`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/.tmp/tmp_restore_device_query_review_records_result.json`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/.tmp/tmp_restore_device_review_filesystem_evidence.json`

关键字段：

- `execute_result.toolResult.ok=true`
- `execute_result.toolResult.status=completed`
- `execute_result.toolResult.modified=true`
- `execute_result.toolResult.data.task_run_id=task_F2F8B16F446D8F6ABF8157B18284200D`
- `execute_result.toolResult.data.task.applied_steps=2`
- `execute_result.toolResult.data.task.modified_assets=1`
- `query_request.archive_session_id=archive_BBCE7C82435C2A156DEFB9B7BDF7942D`
- `query_request.pending_only=false`
- `query_result.status=bridge_unavailable`
- `query_result.message=Bridge connection error: connect ECONNREFUSED 127.0.0.1:54321`
- `filesystem_evidence.records_dir_exists=false`
- `filesystem_evidence.archive_session_file_exists=true`

这些字段能证明：

1. 现场 execute 已成功完成，不能用 step failure 解释 ReviewEvent / ReviewRecord 缺失。
2. `bridge_unavailable` 只解释后续查询失败，不解释为什么执行后的 `Review/Records` 目录没有生成。
3. `ArchiveSession` 已经落盘，说明 PostIO 的 archive-session 分支执行过。
4. `Records` 目录不存在不是“人为删除”的必要假设；在源码合同下它本身就是证据，表示 `SaveReviewRecords` 没有被调用。
5. 当前缺的不是 TaskPlan body，也不是蓝图写入结果，而是成功写图后的 ReviewEvidence 到 ReviewRecord 持久化观测。

但这些字段仍不能证明 `PostIoBatch.ReviewEvidences.Num()` 到底是 0，还是 `BuildReviewRecordsFromEvidence(...)` 返回了 0，因为 execute 摘要被 task-core/CLI 裁剪，raw `post_io` 和计数字段没有出现在保存的结果里。

## Dynamic Cast 状态

`dynamic_cast` 现在不是全局强制 pure cast。

- 表达式上下文的 `convert.dynamic_cast` 会被强制为 pure cast：`BlueprintHelperGraphStatementBuilder.cpp` 在 expression `Convert + dynamic_cast` 路径调用 `CastNode->SetPurity(true)`，并由 `BlueprintHelper.GraphWrite.CallFunctionResolver.ConvertExpression.DynamicCastIsPure` 覆盖。
- Generic transform resolver 仍创建 `UK2Node_DynamicCast` spawner，只设置 target class，没有在该 resolver 层强制 `SetPurity(true)`。
- 目前 TaskSpec 里没有显式 `cast_mode=pure|exec` 字段；实际 pure/exec 由使用上下文决定。RestoreDevice 旧问题里的 impure cast exec pin 已经不是本次根因。

## 证据

### TaskSpec / TS lowering

只读 worker 和本地 Node 测试确认 TS lowering 正确：

- `npm.cmd --prefix AgentFaceService/task-core run build`：PASS。
- `npm.cmd --prefix AgentFaceService/task-core run test:node`：298 tests / 15 suites / 0 fail。
- RestoreDevice compiled TaskPlan 已包含目标资产、`EventGraph`、`owned_graph_edit`、`ensure_entry`、`CE_RestoreDevice`、`signature_evidence_id=signature:custom_event:CE_RestoreDevice`。

对应输入在：

- `BlueprintHelper/Develop/Gap/RestoreDevice_TaskSpec_Spawn_Diff.md:394-425`
- `BlueprintHelper/Develop/Gap/RestoreDevice_TaskSpec_Spawn_Audit_20260531.md:455-486`

### UE 自动化测试

已运行：

```powershell
npm.cmd --prefix AgentFaceService\task-core run test
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development "D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphWrite_Full_20260531_Current_001"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\TaskRuntime_Full_20260531_Current_001"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.TaskRuntime.Composite.CreateBlueprintFeatureExecuteReadBack;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\TaskRuntime_CompositeCreateFeature_Diagnostic_20260531_Current_001"
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.TaskRuntime.Replace.CustomEventBodyReconnectsEntryExec;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\TaskRuntime_GraphWriteReplaceBody_20260531_Current_001"
```

结果：

- UE 5.6 build：PASS。
- `BlueprintHelper.GraphWrite`：report leaf count `Success=344`，`Fail=0`。
- `BlueprintHelper.TaskRuntime`：report leaf count `Success=39`，`Fail=1`。
- 唯一失败：`BlueprintHelper.TaskRuntime.Composite.CreateBlueprintFeatureExecuteReadBack`。
- 独立复跑该失败测试：稳定失败。
- 邻近直接 body replace 测试 `BlueprintHelper.GraphWrite.TaskRuntime.Replace.CustomEventBodyReconnectsEntryExec`：PASS。

诊断报告关键 step：

```text
step_component: applied
step_variable: applied
step_signature_custom_event: applied
  operation=ensure_custom_event
  deferred_to_graph_write=true
  exists=true
  signature_matches=true
step_graph_body: failed
  operation=replace_blueprint_graph
  error_code=owned_replace_target_not_blueprinthelper_owned
  error_stage=resolve_target
```

报告路径：

- `D:/UEProjects/Template/Saved/Automation/GraphWrite_Full_20260531_Current_001/index.json`
- `D:/UEProjects/Template/Saved/Automation/TaskRuntime_Full_20260531_Current_001/index.json`
- `D:/UEProjects/Template/Saved/Automation/TaskRuntime_CompositeCreateFeature_Diagnostic_20260531_Current_001/index.json`
- `D:/UEProjects/Template/Saved/Automation/TaskRuntime_GraphWriteReplaceBody_20260531_Current_001/index.json`

## 源码定位

`EnsureCustomEvent` 创建 CustomEvent 的路径只创建节点、加 pins、记录 layout；没有写 ownership metadata：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.cpp:791-824`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.cpp:1307-1386`

`replace_body` 的 custom event body owned replace 会读取 entry 的 `BlueprintHelperBlockId`。如果缺失，就按设计拒绝接管：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp:944-972`

ownership 写入服务存在，GraphWrite replace 在成功解析 owned target 后会写入新 body 节点，但它要求入口先是 owned anchor：

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.cpp:9-42`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp:754-766`

测试 payload 与 RestoreDevice 同构：

- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp:1213-1296`

## ReviewRecord 断点与输出合同缺口

真实现场不是 `step_graph_body` 失败；它已经成功执行并修改蓝图。当前问题是：CLI/TaskRuntime 的普通 execute 输出不能稳定表达 ReviewEvidence 是否生成、ReviewRecord 是否写入，也不能暴露 `review_evidence_produced_zero_records` 这类关键 PostIO 诊断。

源码现状：

- 非 dry-run 执行会设置并持久化 ArchiveSession：`BlueprintHelperTaskRuntimeService.cpp:5688`、`BlueprintHelperTaskRuntimePostIoService.cpp:26`。
- TaskRuntime 只在 `StepResult.bOk` 后聚合 review evidence：`BlueprintHelperTaskRuntimeService.cpp:6271`。
- PostIO 只有 `Batch.ReviewEvidences.Num() > 0` 时才构建并保存 ReviewRecord：`BlueprintHelperTaskRuntimePostIoService.cpp:43-63`。
- `BuildReviewRecordsFromEvidence(...)` 为 0 时只写 `review_evidence_produced_zero_records` 诊断，不调用 `SaveReviewRecords`：`BlueprintHelperTaskRuntimePostIoService.cpp:49-57`。
- `SaveReviewRecords` 会先创建 `Review/Records` 目录：`BlueprintHelperReviewStoreService.cpp:660-670`。
- `SaveArchiveSession` 使用独立的 `Review/ArchiveSessions` 目录：`BlueprintHelperReviewStoreService.cpp:728`。
- `post_io` JSON 目前只有 `ok` 和 `diagnostics`，没有 `review_evidence_count` / `review_record_count` / ids：`BlueprintHelperTaskRuntimePostIoBatch.cpp:8-26`。
- task-core execute 摘要只返回 `task_run_id` 和 task 概要；raw bridge result 被挂在 non-enumerable `debug.bridge_result` 上，不会进入普通保存结果：`AgentFaceService/task-core/src/task/service/task-spec-runner.ts:151-164`。
- `query_review_records` schema 接受 `pending_only`，但 Bridge route 当前没有解析该字段；不过本次查询直接 `bridge_unavailable`，所以它不是 ReviewRecord 缺失的根因：`AgentFaceService/task-core/src/tool-surface/bridge/bridge-tool-schemas.ts:19`、`BlueprintHelperBridgeRouter.cpp:1318-1337`。

所以用户侧看到“ArchiveSession 有、ReviewRecord 没有”时，不应先假设文件被删除；在当前源码合同下，`Records` 目录不存在更直接说明 `SaveReviewRecords` 分支没有被走到。下一步应让 execute 输出或 TaskRunJournal 明确暴露 `review_evidence_count`、`review_record_count`、`review_record_ids` 和 PostIO diagnostics。

## 修复边界建议

1. 不要把 owned replace 放宽成“可以接管任意用户节点”。当前拒绝 user-authored node 是正确安全边界。
2. 修 `blueprint_signature -> graph_write` handoff：
   - 方案 A：当 `ensure_custom_event` 是为后续 owned `graph_write` body step 创建的新入口时，写入 BlueprintHelper ownership metadata，并生成可追踪 block id。
   - 方案 B：GraphWrite replace 使用依赖 step 的 signature evidence token，只允许接管同一 task run 刚创建的 signature entry，而不是任意既有用户节点。
3. 增加回归测试：
   - 保持 `BlueprintHelper.TaskRuntime.Composite.CreateBlueprintFeatureExecuteReadBack` 作为 red/green 主回归。
   - 增加 RestoreDevice 形态 E2E：`ensure_custom_event CE_RestoreDevice` + `owned_graph_edit replace_body` 必须完成并产生 review evidence / ReviewRecord。
   - 增加现场形态 E2E：`ensure_custom_event CE_RestoreDevice` + `owned_graph_edit ensure_entry(custom_event)` / `append_blueprint_graph` 成功执行后，必须产生同一 `task_run_id` / `archive_session_id` 可查询的 ReviewRecord。
   - 增加 PostIO/execute contract 测试：输出或 debug 至少包含 `archive_session_id`、`review_evidence_count`、`review_record_count`、`review_record_ids` 或等价可查询状态。

## 当前改动

为定位根因，在 `BlueprintHelperGraphWriteToolResultBaseTests.cpp` 的失败测试中保留了 `AddToolResultFailureDetail` 调用。它不改变生产行为，只让该自动化测试在失败时打印完整 `ToolResult`，包括 failed step、error code、post_io。
