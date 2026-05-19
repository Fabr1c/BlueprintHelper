# P2 TaskRuntime Review IO 与三层执行模型实施计划

日期：2026-05-19

## 目标

P2 处理真实 execute 阶段的 Review IO、TaskRunJournal、DebugEntry、archive session 和 TaskRuntime 编排耦合。目标是把当前 `RunTaskPlan` 中混在一起的准备、UE 主线程写入和结果 IO 拆成可测的三层模型：

`PurePrepare -> MainThreadCommit -> PostIO`

该拆分为后续安全并发和批处理提供边界，但 v0.5.0 不把 UObject 写入并发化。

## 范围

- P2-6：Review baseline snapshot、semantic snapshot、review record merge/write 的 timing、去重、批处理或后台化。
- P2-7：UE TaskRuntime 三层执行模型。
- 保持 Review v2 单一数据模型，不新增 Review v1 / Transaction / legacy anchor fallback。

## 架构边界

- `PurePrepare` 只处理纯数据，不触碰 UObject。
- `MainThreadCommit` 是唯一允许触碰 UObject / Blueprint / UEdGraph / transaction / compile / save 的层。
- `PostIO` 只处理不改变 UE 对象状态的结果收口和持久化。
- baseline snapshot 是 mutation 前屏障，不能延后到 PostIO。
- Review evidence、DebugBundle、UI overlay、Accept/Reject 必须消费同一 Review 数据模型。

## 文件结构

- 修改 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
  - 从大 orchestration 拆为三层 coordinator 调用。
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePrepareService.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePrepareService.cpp`
  - 负责 `PurePrepare`。
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.cpp`
  - 负责 `MainThreadCommit`。
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoService.cpp`
  - 负责 `PostIO`。
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeReviewIoBatch.h`
- 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeReviewIoBatch.cpp`
  - 收集 Review record、archive session、journal、debug entry 写入批次。
- 新增 DTO header
  - `BlueprintHelperTaskRuntimePreparedTaskRun.h`
  - `BlueprintHelperTaskRuntimePreparedStep.h`
  - `BlueprintHelperTaskRuntimeCommitResult.h`
  - `BlueprintHelperTaskRuntimePostIoBatch.h`
  - 结构体/纯数据类可以在 DTO header 聚合，但行为类必须独立 `.h/.cpp`。
- 新增/修改测试
  - `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePrepareServiceTests.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp`
  - Review action/store regression tests。

## 三层职责

### PurePrepare

允许：
- TaskPlan JSON 结构读取。
- step id、depends_on、target_assets、execution_policy 规范化。
- step 顺序或 DAG plan 构建。
- JSON 到 lowered payload 的纯数据转换。
- CallFunction query 收集和去重 key 构建。

禁止：
- `ResolveBlueprint`。
- 查找或遍历 `UEdGraph` / node / pin。
- 访问 `FBlueprintActionDatabase`。
- `Modify()`、`MarkPackageDirty()`、compile、save。
- 捕获 Review snapshot。

### MainThreadCommit

允许：
- baseline policy 写入前屏障。
- mutation 前 Review baseline snapshot。
- Blueprint / Graph 解析。
- CallFunction 对 UE 上下文解析。
- cluster dry-run 或真实写入。
- before / after target snapshot 采集。
- graph layout flush。
- compile / save。

要求：
- 按 asset lock 和 step dependency 串行提交。
- 只在该层触碰 UObject 和 Editor API。
- 每个阶段写入 timing：`main_thread_commit.resolve_targets`、`main_thread_commit.cluster_execute`、`main_thread_commit.compile`、`main_thread_commit.save`。

### PostIO

允许：
- ReviewRecord 批量 merge/write。
- TaskRunJournal 持久化或内存登记。
- DebugEntry best-effort 写入。
- runtime facts 附加。
- archive session 元数据 flush。

禁止：
- 修改 UObject。
- 捕获 mutation 前必需 snapshot。
- 影响 Accept/Reject 的 evidence 完整性。

## TDD Checklist

### P2-6 Review IO

- [x] 测试：同一 TaskPlan 内相同 review target 只捕获一次等价 snapshot。
- [x] 测试：Review record batch 保持 parent/child 顺序和 `ParentChangeId` 语义。
- [x] 测试：PostIO 写入失败返回诊断，不伪装成 mutation 失败。
- [x] 测试：Reject 仍以 evidence before snapshot 为回滚目标。
- [x] 实现 `FBlueprintHelperTaskRuntimeReviewIoBatch`。
- [x] 为 baseline snapshot、semantic snapshot、record merge/write 增加 timing。
- [x] 可延迟的 archive/journal/debug entry 写入进入 PostIO batch。

### P2-7 三层拆分

- [x] 测试：`PurePrepare` 不需要 UObject 上下文即可运行。
- [x] 测试：`PurePrepare` 输出 `PreparedTaskRun`，保持 step dependency。
- [x] 测试：`MainThreadCommit` 是唯一调用 UE object access helper 的层。
- [x] 测试：`PostIO` 只消费 `CommitResult` 和纯 DTO batch。
- [x] 拆出 prepare service。
- [x] 拆出 commit service。
- [x] 拆出 post IO service。
- [x] `BlueprintHelperTaskRuntimeService.cpp` 只保留层间 orchestration 和错误聚合。
- [x] timing 至少区分 `pure_prepare`、`main_thread_commit`、`post_io`。

## 验收命令

```powershell
E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild
git diff --check
```

如果存在可单独运行的 automation tests，补跑 TaskRuntime / Review 相关测试，并把命令和结果写回本文。

## 性能验证

记录字段：

| 样本 | pure_prepare_ms | main_thread_commit_ms | post_io_ms | review_snapshot_ms | review_record_write_ms | compile_save_ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 04b_write_function_body |  |  |  |  |  |  |

完成标准：
- `RunTaskPlan` 从结构上清晰映射三层模型。
- 只有 `MainThreadCommit` 触碰 UObject 和 Editor API。
- Review record 写入可批处理，不破坏 Reject / Accept 可恢复性。
- 分阶段 timing 能定位 Review IO 和 compile/save 成本。

## 风险控制

- baseline snapshot 不能为了异步化延后到 mutation 之后。
- PostIO 失败不能导致 Review 模型和 DebugBundle 互相矛盾。
- 不新增旧字段兼容，不保留 Review v1 fallback。
- 不把跨层共享状态塞进单个巨型 utils/service。

## 实施记录

实施时间：2026-05-20。

新增边界：

- `FBlueprintHelperTaskRuntimePrepareService`：负责 `PurePrepare`，只读取 payload / TaskPlan JSON、规范化 target assets / step id / dependency，并完成 adapter lowering，输出 `FBlueprintHelperTaskRuntimePreparedTaskRun`。
- `FBlueprintHelperTaskRuntimeCommitService`：负责 `MainThreadCommit` 中的 cluster execution、graph layout flush、compile/save 等会触碰 UE Editor 状态的动作。
- `FBlueprintHelperTaskRuntimeReviewIoBatch`：负责收集 archive session、Review evidence、TaskRunJournal、DebugEntry、pending review notification。
- `FBlueprintHelperTaskRuntimePostIoService`：负责 `PostIO` flush；Review record/archive/journal/debug 写入失败进入 `data.post_io.diagnostics`，不再把已经成功的 mutation 伪装成 step mutation 失败。
- `FBlueprintHelperTaskRuntimePostIoBatch`、`FBlueprintHelperTaskRuntimePreparedStep`、`FBlueprintHelperTaskRuntimePreparedTaskRun`、`FBlueprintHelperTaskRuntimeCommitResult`：三层之间只传 DTO。

关键语义：

- baseline archive snapshot 和 semantic snapshot 仍在 mutation 前完成，未延后到 PostIO。
- target before snapshot 增加 TaskRun 内缓存：同一 TaskPlan 内同一 review target 的 before snapshot 只捕获一次并复用；after snapshot 不做跨 step 缓存，避免多次 mutation 后复用旧 after 状态。
- Review record 从 step loop 内即时写入改为 PostIO batch 一次性 `BuildReviewRecordsFromEvidence` + `SaveReviewRecords`。
- `TaskRunJournal` 和 DebugEntry 写入改由 PostIO batch 收口。
- UE nested timing 新增/稳定返回：`pure_prepare`、`main_thread_commit`、`post_io`、`post_io.archive_session_write`、`post_io.review_record_write`、`main_thread_commit.compile`、`main_thread_commit.save`。

新增测试文件：

- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePrepareServiceTests.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatchTests.cpp`

## 验证结果

编译：

```powershell
E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild
```

结果：Succeeded。

静态检查：

```powershell
git diff --check
```

结果：通过；仅有 Git 行尾提示。

真实 CLI execute 测试：

- Editor lifecycle：使用 MCP `blueprint_open_editor` 启动，MCP `blueprint_close_editor` 关闭。
- Spec 产物目录：`D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\PerfProbe\P2TaskRuntimeReviewIO_20260520020946`
- 测试资产：`/Game/BlueprintHelperCliSmoke/P2RuntimeReviewIO_20260520020946/BP_P2_RuntimeReviewIO_20260520020946`

| 样本 | status | cli_total_ms | ue_execute_total_ms | pure_prepare_ms | main_thread_commit_ms | post_io_ms | review_snapshot_ms | review_record_write_ms | archive_session_write_ms | compile_ms | save_ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `01_create_blueprint_actor.json` | executed | 425.581 | 299.783 | 0.026 | 297.096 | 1.672 | 1.698 | 1.323 | 0.344 | 231.899 | 60.276 |
| `04_edit_blueprint_signatures.json` | executed | 1849.219 | 277.390 | 0.034 | 273.102 | 2.271 | 2.074 | 1.701 | 0.563 | 152.938 | 97.317 |
| `04b_write_function_body.json` | executed | 2854.826 | 779.309 | 0.017 | 777.225 | 0.451 | 1.522 |  | 0.446 | 156.410 | 62.954 |

说明：`04b_write_function_body.json` 的 graph write 样本不产生 runtime fallback Review record，因此没有 `post_io.review_record_write`；`01` 和 `04` 已覆盖 PostIO Review record batch write。

## 完成结论

- P2-6 已完成：Review IO 写入已从 step mutation loop 中拆到 PostIO batch，写入失败以 PostIO 诊断返回，不改变 mutation 结果语义。
- P2-7 已完成：`RunTaskPlan` 已形成 `PurePrepare -> MainThreadCommit -> ResultWrap -> PostIO` 的结构化链路，并通过 UE nested timing 暴露三层耗时。
- v0.5.0 不并发 UObject 写入；当前拆分只建立可并发/批处理的边界。
