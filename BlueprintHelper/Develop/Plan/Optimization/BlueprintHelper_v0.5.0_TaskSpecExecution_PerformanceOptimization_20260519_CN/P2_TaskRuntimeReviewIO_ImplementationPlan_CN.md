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

- [ ] 测试：同一 TaskPlan 内相同 review target 只捕获一次等价 snapshot。
- [ ] 测试：Review record batch 保持 parent/child 顺序和 `ParentChangeId` 语义。
- [ ] 测试：PostIO 写入失败返回诊断，不伪装成 mutation 失败。
- [ ] 测试：Reject 仍以 evidence before snapshot 为回滚目标。
- [ ] 实现 `FBlueprintHelperTaskRuntimeReviewIoBatch`。
- [ ] 为 baseline snapshot、semantic snapshot、record merge/write 增加 timing。
- [ ] 可延迟的 archive/journal/debug entry 写入进入 PostIO batch。

### P2-7 三层拆分

- [ ] 测试：`PurePrepare` 不需要 UObject 上下文即可运行。
- [ ] 测试：`PurePrepare` 输出 `PreparedTaskRun`，保持 step dependency。
- [ ] 测试：`MainThreadCommit` 是唯一调用 UE object access helper 的层。
- [ ] 测试：`PostIO` 只消费 `CommitResult` 和纯 DTO batch。
- [ ] 拆出 prepare service。
- [ ] 拆出 commit service。
- [ ] 拆出 post IO service。
- [ ] `BlueprintHelperTaskRuntimeService.cpp` 只保留层间 orchestration 和错误聚合。
- [ ] timing 至少区分 `pure_prepare`、`main_thread_commit`、`post_io`。

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

