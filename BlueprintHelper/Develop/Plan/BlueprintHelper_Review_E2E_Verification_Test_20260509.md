# BlueprintHelper Review E2E Verification Test

2026-05-09 清理: 本文件保留 Review E2E 的详细手动步骤。当前全线执行顺序以 `Develop/Plan/BlueprintHelper_Unified_SmokeRun_Verification_20260509.md` 为准，ReviewPanel 与 DebugBundle 验证迁入 Ring 8。

日期: 2026-05-09

## 目标

验证 Review 系统从 Agent 触发一次真实 TaskSpec 写入开始，到 UE 侧生成 ReviewRecord、用户在 ReviewPanel 审查、Accept 或 Reject、失败路径进入 DebugCase、最终可通过诊断摘要追踪的完整链路。

本测试文档不是新增 Agent-facing Review 流程。Review 仍然是用户侧持久审查系统，Agent 只负责通过 TaskSpec 触发资产写入并报告任务结果。

## 2026-05-09 当前现场状态

- UE build 已由用户侧确认通过，本测试不再把 C++ 编译列为当前阻塞。
- 用户已打开 ReviewPanel，但当前没有待审阅内容。日志:

```text
[15:16:37] GraphDiff jump skipped because selection, preview graph, or editor is invalid.
[15:16:37] GraphEditor build sourceGraph="<none>" previewGraph="EdGraph_0" previewNodes=0 timer=disabled
```

该日志表示 ReviewPanel 进入空 pending visible change 状态，不等同于 GraphDiff 失败。下一步必须先完成 Level 3 disposable TaskSpec 写入并确认 ReviewRecord / visible_changes 落盘，再重开 ReviewPanel 验证 Level 4。

- 用户提供的 `FullTestLog.txt` 中，Review / Debug 关键 Automation 已有通过记录: `RejectNeedsActionCreatesDebugCase`、`RejectFailedCreatesDebugCase`、`PersistsDebugCaseIds`、`LoadPendingVisibleChangesUsesRecordQuery`、`BundleSummaryExportIncludesReviewSummaryArtifact`。
- 同一日志里 grouped Automation 尚未全绿，`BlueprintHelper.TaskRuntime.Cluster.FinalBatchClustersRecognizeOnlyOwnedSteps` 使用旧 `cleanup_ownership` capability 名导致失败；active TaskPlan capability 是 `graph_cleanup_ownership`。测试口径已修正，targeted UE Automation 复跑 `ResolvesLoweredSteps` 与 `FinalBatchClustersRecognizeOnlyOwnedSteps` 已通过。

## 验证范围

覆盖:

- MCP/Node/Python 合同回归。
- UE 插件编译。
- UE Automation 中 Review、Producer、Rollback、RuntimeDiagnostics 相关测试。
- TaskRuntime 执行时创建 ArchiveSession。
- 首次真实写入前捕获 baseline snapshot。
- Producer 或 Journal 生成 WriteReviewEvidence。
- ReviewStore 按 archive_session_id + asset_path 持久化 ReviewRecord。
- ReviewPanel 通过 ReviewStore 展示 pending visible changes。
- Accept 写入 review_actions 并传播状态。
- Reject 成功时执行机械 rollback 并传播状态。
- Reject needs_action 或 reject_failed 时通过 DebugEntry 创建 DebugCase。
- ReviewRecord 只持久化 debug_case_ids，不持久化 DebugBundle 本地路径。
- DebugCase summary 回链 review_record_ids 和 transaction_links。

不覆盖:

- Agent 操作 ReviewPanel。
- Agent-facing Accept / Reject MCP 工具。
- 生产资产验证。
- Crash recovery 最终实现确认。
- 非 graph target 的全类型机械 rollback 完整性。

## 前置条件

1. 当前仓库路径:

```text
G:/UnrealPractise/MrStone/Plugins/BlueprintHelper
```

2. 目标项目路径:

```text
G:/UnrealPractise/MrStone/MrStone.uproject
```

3. UE 5.6 构建工具路径:

```text
F:/UE_5.6/Engine/Build/BatchFiles/Build.bat
F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe
```

4. 使用一次性测试资产，不得使用生产 gameplay 资产。

推荐资产:

```text
/Game/BlueprintHelper/ReviewE2E/BP_ReviewE2E
```

5. 每次运行使用唯一名称:

```text
_RUN_ID_ = 20260509_001
_GRAPH_NAME_ = BH_ReviewE2E_20260509_001
_EVENT_NAME_ = BH_ReviewE2E_Event_20260509_001
```

## 运行前清理

删除上一轮同名运行产生的临时 Review 和 Debug 文件，避免旧记录误判。

```powershell
Remove-Item -Recurse -Force "G:/UnrealPractise/MrStone/Saved/BlueprintHelper/Review/ArchiveSessions/archive_*" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "G:/UnrealPractise/MrStone/Saved/BlueprintHelper/Review/Records/review_*" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "G:/UnrealPractise/MrStone/Saved/BlueprintHelper/Review/Snapshots/archive_*" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "G:/UnrealPractise/MrStone/Saved/BlueprintHelper/Debug/Cases/dbg_*" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "G:/UnrealPractise/MrStone/Saved/BlueprintHelper/Debug/Bundles/bundle_*" -ErrorAction SilentlyContinue
```

如果需要保留历史记录，改为先复制 `Saved/BlueprintHelper` 到人工归档目录，再清理本轮使用的 run id 相关文件。

## Level 0: 仓库和 MCP 回归

### 0.1 Git 状态记录

```powershell
git status --short
git diff --stat
git diff --check
```

通过标准:

- `git diff --check` 没有空白错误。
- 已知脏文件记录清楚。
- 不要求工作树完全干净，但 Review 相关源码变更必须能解释来源。

### 0.2 MCP 测试

```powershell
Set-Location G:/UnrealPractise/MrStone/Plugins/BlueprintHelper/BlueprintHelper_MCP_Server
npm.cmd test
```

通过标准:

- TypeScript build 成功。
- Python unittest 成功。
- Node regression tests 成功。
- `debug-case.regression.test.ts` 确认 `get_debug_case` 只返回 summary，不暴露 DebugBundle artifact reader。

失败处理:

- MCP 测试失败时不继续跑 UE E2E。
- 先修 MCP 合同或记录为 `blocked_by_mcp_regression`。

## Level 1: UE 编译

```powershell
F:/UE_5.6/Engine/Build/BatchFiles/Build.bat MrStoneEditor Win64 Development -Project=G:/UnrealPractise/MrStone/MrStone.uproject -WaitMutex -NoUBA -MaxParallelActions=1
```

通过标准:

- Exit code 为 0。
- BlueprintHelper 模块编译完成。
- 无 Review、Debug、TaskRuntime、TransactionJournal 相关编译错误。

失败处理:

- 如果停在 `Intermediate` 写权限或文件锁，记录为 `blocked_by_intermediate_lock`。
- 如果进入 C++ 编译后失败，记录具体文件和行号，不继续 Level 2。

## Level 2: UE Automation

推荐命令:

```powershell
F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe G:/UnrealPractise/MrStone/MrStone.uproject -Unattended -NullRHI -NoSplash -NoSound -ExecCmds="Automation RunTests BlueprintHelper.Review; Automation RunTests BlueprintHelper.Review.Producer; Automation RunTests BlueprintHelper.Review.Rollback; Automation RunTests BlueprintHelper.RuntimeDiagnostics; Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="G:/UnrealPractise/MrStone/Saved/Automation/ReviewE2E"
```

如果命令行批量执行不稳定，在 Editor Session 内分别运行:

```text
Automation RunTests BlueprintHelper.Review
Automation RunTests BlueprintHelper.Review.Producer
Automation RunTests BlueprintHelper.Review.Rollback
Automation RunTests BlueprintHelper.RuntimeDiagnostics
```

通过标准:

- Review Store/action tests 通过。
- Producer evidence tests 通过。
- Rollback tests 通过。
- DebugCase review linkage tests 通过。
- DebugBundle review summary tests 通过。

关键测试能力:

- ReviewRecord identity 为 archive_session_id + asset_path。
- Review action history 会持久化。
- Accept/Reject/RejectAll 会传播 target/change/record 状态。
- Producer-owned evidence 包含 target key、archive session、task run、asset path、hash、rollback ref。
- Reject needs_action 和 reject_failed 创建 DebugCase。
- DebugCase summary 包含 review_record_ids。
- DebugCase transaction_links 包含 `review_reject_failed`。

失败处理:

- 单个 Automation test 失败时记录 test name、error、日志路径。
- 不把 UE Automation 失败归因为 MCP，除非日志明确显示 Bridge/MCP 输入错误。

## Level 3: MCP 到 TaskRuntime 到 ReviewStore

本层验证真实 TaskSpec 执行后，Review 记录被创建。执行必须通过普通 Agent TaskSpec-first 路径。

### 3.1 Runtime preflight

调用:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_task_context
```

`blueprinthelper_read_task_context` payload:

```json
{
  "target": {
    "asset_path": "/Game/BlueprintHelper/ReviewE2E/BP_ReviewE2E"
  },
  "feature_name": "ReviewE2E"
}
```

通过标准:

- Bridge reachable。
- Target asset exists。
- Runtime profile 不阻塞写入。
- Diagnostics 没有 fatal issue。

### 3.2 Preview TaskSpec

调用 `blueprinthelper_preview_task`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_review_e2e_20260509_001",
    "task_type": "edit_blueprint_graph",
    "feature_name": "ReviewE2E",
    "target": {
      "asset_path": "/Game/BlueprintHelper/ReviewE2E/BP_ReviewE2E",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "BH_ReviewE2E_20260509_001",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "append_new_owned_graph",
      "entries": [
        {
          "entry_type": "custom_event",
          "name": "BH_ReviewE2E_Event_20260509_001",
          "body": {
            "schema": "BlueprintLogicSpec.v1",
            "statements": [
              {
                "kind": "call_function",
                "name": "PrintString",
                "args": {
                  "InString": {
                    "kind": "literal",
                    "value_type": "string",
                    "value": "Review E2E 20260509"
                  },
                  "Duration": {
                    "kind": "literal",
                    "value_type": "float",
                    "value": 2
                  }
                }
              }
            ]
          }
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": false
    }
  }
}
```

通过标准:

- `operation = preview_task`。
- `modified = false`。
- Preview passed。
- Preview blocked 为 false。
- TaskPlan summary 包含 graph_write 或 blueprint_signature + graph_write。

失败处理:

- Preview blocked 时停止，不执行。
- 记录 blocked reason。

### 3.3 Execute TaskSpec

用同一 TaskSpec 调用 `blueprinthelper_execute_task`。

通过标准:

- `operation = execute_task`。
- 返回非空 `task_run_id`。
- applied steps 大于 0。
- 目标资产在 modified assets 中。
- 编译成功或 compile result 明确返回失败信息。
- `should_save = false` 时资产不应自动保存。

记录:

```text
_TASK_RUN_ID_ = returned task_run_id
```

### 3.4 Get Task Result

调用:

```json
{
  "task_run_id": "_TASK_RUN_ID_"
}
```

通过标准:

- 返回 `BlueprintHelper.TaskRunJournal.v1`。
- 状态为 completed。
- steps 内记录执行结果。
- Journal 不暴露 ReviewRecord 完整 payload。

### 3.5 Review 文件落盘检查

检查:

```powershell
Get-ChildItem -Recurse G:/UnrealPractise/MrStone/Saved/BlueprintHelper/Review
```

通过标准:

- `ArchiveSessions/<archive_session_id>.json` 存在。
- `Records/<review_record_id>.json` 存在。
- `Snapshots/<archive_session_id>/...` 至少包含目标资产 baseline snapshot，前提是资产包文件存在。
- ReviewRecord 中 `schema = BlueprintHelper.ReviewRecord.v1`。
- `archive_session_id` 非空。
- `asset_path = /Game/BlueprintHelper/ReviewE2E/BP_ReviewE2E`。
- `source_task_run_ids` 包含 `_TASK_RUN_ID_`。
- `visible_changes` 非空。
- 至少一个 atomic target 包含:
  - `target_key`
  - `target_kind`
  - `visual_group_key`
  - `baseline_hash`
  - `recorded_after_hash`
  - `rollback_data_ref`
  - `status = pending`

失败处理:

- ArchiveSession 缺失: `review_archive_session_missing`。
- Record 缺失: `review_record_missing`。
- Snapshot 缺失但资产包存在: `review_baseline_snapshot_missing`。
- Atomic target 缺字段: `review_evidence_incomplete`。

## Level 4: ReviewPanel 用户侧审查

本层必须由用户在 UE 插件 UI 中操作。Agent 不调用 Accept 或 Reject。

步骤:

1. 打开 Blueprint Helper 窗口。
2. 进入 Review 页。
3. 找到 `/Game/BlueprintHelper/ReviewE2E/BP_ReviewE2E`。
4. 选择本轮 `BH_ReviewE2E_20260509_001` 对应 visible change。

通过标准:

- 左侧按 asset-first tree 展示。
- Review page 能加载 pending visible changes。
- Graph 区域为只读 Review graph。
- Components、My Blueprint、Details 面板不允许直接编辑真实资产。
- Graph diff frame 能展示本轮新增 graph 或 block。
- Debug panel 可复制当前 Review 调试信息。

失败处理:

- UI 没有记录: 先检查 Level 3 ReviewRecord 是否存在。
- 如果 Debug 面板出现 `sourceGraph="<none>"`、`previewNodes=0`、`GraphDiff jump skipped`，先按空记录处理：确认 `LoadPendingVisibleChanges()` 是否返回 0，确认 `Saved/BlueprintHelper/Review/Records` 是否存在本轮 ReviewRecord，不要直接归因为 GraphDiff 渲染 bug。
- UI 打开崩溃: 记录 callstack 和 selected change。
- UI 选错图: 检查 graph resolver DebugMessage。

## Level 5: Accept 链路

准备:

- 重新运行 Level 3，使用新 `_RUN_ID_`，避免与 Reject 用例混用。

用户操作:

1. 在 ReviewPanel 选择本轮 visible change。
2. 点击 Accept。

通过标准:

- ReviewRecord 仍存在。
- 对应 target status 变为 `accepted`。
- visible change status 派生为 `accepted` 或当前实现等价终态。
- record status 派生为 `accepted` 或当前实现等价终态。
- `review_actions[]` 增加一条:

```json
{
  "action": "accept",
  "ownership_policy": "keep_managed"
}
```

- Accept 不创建 DebugCase。
- ReviewRecord 不新增 DebugBundle 本地路径。

失败处理:

- 状态未变化: `review_accept_status_not_persisted`。
- action 缺失: `review_accept_action_missing`。
- 误创建 DebugCase: `review_accept_unexpected_debug_case`。

## Level 6: Reject 成功链路

准备:

- 使用 journal-backed GraphWrite append 记录。
- 目标 graph/block 当前状态未被用户修改。
- current hash 与 recorded_after_hash 一致。

用户操作:

1. 在 ReviewPanel 选择本轮 visible change。
2. 点击 Reject。

通过标准:

- Reject 仅处理用户选中的 target。
- 目标新增节点或 block 被机械回滚。
- 未选中的其他 Review target 不被级联处理。
- target status 变为 `rejected`。
- `review_actions[]` 增加 `reject`。
- 成功 Reject 不创建 DebugCase。
- 资产进入 dirty 状态，除非测试显式保存。

读回验证:

```text
blueprinthelper_read_context
```

通过标准:

- 本轮新增 custom event 或 graph 不再存在，或不再包含被 Reject 的目标。
- 其他 graph 和用户节点未被删除。

失败处理:

- Anchor 找不到: `review_reject_anchor_not_found`。
- Hash 不匹配: 进入 Level 7 needs_action 验证。
- Rollback executor 不支持: 记录为 `review_reject_executor_unimplemented`。

## Level 7: Reject needs_action 到 DebugCase

目的:

验证 Review 失败路径不会直接写 Debug 文件，而是调用 DebugEntry，并把返回的 debug_case_id 持久化到 ReviewRecord。

准备:

1. 重新运行 Level 3，使用新 `_RUN_ID_`。
2. 在 ReviewPanel 操作前，手动改动同一目标，使 current hash 不再等于 recorded_after_hash。

推荐触发方式:

- 打开目标 Blueprint。
- 对本轮新增节点移动位置或修改节点注释。
- 不保存。

用户操作:

1. 回到 ReviewPanel。
2. 对同一 visible change 点击 Reject。

通过标准:

- Reject 不覆盖用户的新改动。
- target status 变为 `needs_action`。
- visible change 或 record 派生为 `needs_action`。
- ReviewRecord 的 `diagnostics.debug_case_ids[]` 增加一个 id。
- ReviewRecord 不再包含 `diagnostics.debug_export_refs[]`。

调用 `blueprinthelper_get_debug_case`:

```json
{
  "debug_case_id": "_DEBUG_CASE_ID_"
}
```

通过标准:

- 返回 `BlueprintHelper.DebugCaseSummary.v1`。
- `review_record_ids[]` 包含当前 ReviewRecord id。
- `asset_paths[]` 包含测试资产。
- `source = review_reject_needs_action` 或等价 Review reject source。
- `operation = reject_review_targets`。
- `transaction_links[]` 至少包含一条:

```json
{
  "role": "review_reject_failed",
  "source": "review"
}
```

失败处理:

- ReviewRecord 没有 debug_case_ids: `review_debug_link_missing`。
- DebugCase 没有 review_record_ids: `debug_review_backlink_missing`。
- DebugCase 暴露 events/raw payload: `debug_case_summary_leak`。

## Level 8: Reject failed 到 DebugCase

目的:

验证 rollback executor 已尝试执行但 UE API 或原生操作失败时，状态进入 reject_failed，并且 DebugCase 链路完整。

触发方式:

- 使用一个 rollback_data_ref 可解析但 executor 返回失败的 disposable case。
- 或通过 UE Automation 中 `BlueprintHelper.Review.Integration.RejectFailedCreatesDebugCase` 验证。

通过标准:

- target status 为 `reject_failed`。
- ReviewRecord 写入 reject action。
- ReviewRecord diagnostics 写入 debug_case_ids。
- DebugCase source 为 `review_reject_failed`。
- DebugCase error message 包含原始 rollback failure message。
- transaction link role 为 `review_reject_failed`。

失败处理:

- 如果无法构造可靠手动失败 fixture，本层可由 UE Automation 结果代替，但必须在报告中标记为 `automation_only`。

## Level 9: DebugBundle Review summary

目的:

验证 DebugBundle 可包含 Review summary artifact，但 ReviewRecord 不保存 bundle 本地路径。

执行方式:

- 优先使用 UE Automation 中 DebugBundle Review summary 测试。
- 如 Developer Debug UI 已可用，可在 UI 中对 Level 7 或 Level 8 的 DebugCase 执行 summary export。

通过标准:

- Bundle manifest 存在。
- Bundle manifest 中包含相对路径形式的 `review/*.summary.json` artifact。
- Review summary artifact 包含:
  - `schema = BlueprintHelper.ReviewSummaryArtifact.v1`
  - `review_record_id`
  - `archive_session_id`
  - `asset_path`
  - `status`
  - `source_transaction_summary`
- ReviewRecord 中不出现本地 bundle 绝对路径。
- MCP 没有 DebugBundle artifact reader。

失败处理:

- Bundle 含绝对路径: `debug_bundle_absolute_path_leak`。
- ReviewRecord 存 bundle path: `review_record_bundle_path_leak`。
- MCP 暴露 bundle reader: `mcp_debug_bundle_reader_leak`。

## Level 10: 负向边界验证

### 10.1 Agent 不操作 ReviewPanel

通过标准:

- Agent 最终报告不输出 ReviewRecord 完整 payload。
- Agent 不调用 Accept/Reject。
- Agent 不把 Non-BH-owned anchor 当作默认写入 handle。

### 10.2 ReviewPanel 不编辑真实资产

通过标准:

- ReviewPanel GraphEditor 是只读预览。
- Components、MyBlueprint、Details 不允许通过 ReviewPanel 直接修改真实资产。
- Accept/Reject 以 ReviewActionService 为唯一 mutation 通道。

### 10.3 ReviewRecord 不变成 Debug 存储

通过标准:

- ReviewRecord 只保存 debug_case_ids。
- DebugBundle payload 不内联到 ReviewRecord。
- `debug_export_refs[]` 已从 active ReviewRecord contract 删除。

## 最终验收矩阵

| 层级 | 必须通过 | 允许替代 |
|---|---|---|
| Level 0 | `npm.cmd test` | 无 |
| Level 1 | UE build exit 0 | 无 |
| Level 2 | UE Automation Review/Debug 测试通过 | 无 |
| Level 3 | TaskSpec 写入生成 ReviewRecord | 无 |
| Level 4 | ReviewPanel 展示 pending changes | 仅 UI 崩溃调查时可阻塞 |
| Level 5 | Accept action 持久化 | UE Automation 可辅助，不替代手动 UI |
| Level 6 | Reject success rollback | 对非 graph target 可记录 known gap |
| Level 7 | needs_action DebugCase 链路 | 无 |
| Level 8 | reject_failed DebugCase 链路 | UE Automation 可替代手动 fixture |
| Level 9 | DebugBundle Review summary | UE Automation 可替代 UI export |
| Level 10 | 边界负向验证 | 静态检查加手动确认 |

全链路通过定义:

```text
Level 0, 1, 2, 3, 4, 5, 7, 9, 10 必须通过。
Level 6 至少 graph append rollback 成功一次。
Level 8 可由 UE Automation 代替。
所有失败、阻塞和 known gap 必须写入本测试报告。
```

## 测试报告模板

```text
Review E2E run:
Date:
Branch:
Commit:
Project:
UE version:
MCP server version:

Disposable asset:
Run id:
Graph:
Event:

Level 0 MCP tests:
Level 1 UE build:
Level 2 UE Automation:
Level 3 TaskSpec -> ReviewRecord:
Level 4 ReviewPanel:
Level 5 Accept:
Level 6 Reject success:
Level 7 Reject needs_action -> DebugCase:
Level 8 Reject failed -> DebugCase:
Level 9 DebugBundle Review summary:
Level 10 Boundary:

Created task_run_ids:
Created archive_session_ids:
Created review_record_ids:
Created debug_case_ids:

Known gaps:
Blocking issues:
Decision:
```

## 已知风险

- Codex 环境曾多次因 `Intermediate` 写权限或文件锁无法完成 UE build。若再次发生，必须由用户侧本地 UE 5.6 环境完成 Level 1 和 Level 2。
- Crash recovery 目前不能作为已完成项验收，除非后续代码实现并补测试。
- 非 graph target 的 Reject 可能因 hash 或 rollback executor 不支持进入 needs_action，这是当前实现风险，不应误判为 Debug 链路失败。
- GraphWrite journal-backed 路径和 TaskRuntime generic producer evidence 路径不同，检查 rollback_data_ref 时必须区分 `transaction://` 和 `review://archive`。
- ReviewPanel 是用户侧工具，测试过程不能把 UI Accept/Reject 包装成普通 Agent 工具调用。
