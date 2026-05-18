# BlueprintHelper Debug System Architecture v1

日期：2026-05-08  
状态：讨论确认后的 v1 边界文档  
适用范围：BlueprintHelper MCP / Task Compiler / UE TaskRuntime / UE 插件 / Review / Transaction / Developer Debug UI

---

## 0. 文档目的

本文固定 BlueprintHelper Debug 系统的 v1 架构边界、核心对象、统一入口、自动捕获策略、文件导出策略、隐私裁剪、Review / Transaction 集成和阶段验收标准。

Debug 系统是独立的 developer diagnostics / debug bundle 系统，用于定位失败、阻断、partial failure、rollback failed、Review needs_action、Bridge / MCP 错误等问题。

Debug 系统不是普通 Agent 读写路径，不是 Review UI 的替代物，不是大 payload 读取通道。

---

## 1. P0 总边界

### 1.1 Debug 系统定位

```text
BlueprintHelper Debug System
= 独立的 developer diagnostics / debug bundle 系统
```

Debug 负责：

```text
1. 记录一次失败 / 阻断 / 异常 / 回滚失败的可调试上下文。
2. 把 MCP、Bridge、Task Compiler、UE TaskRuntime、UE capability service、Transaction、Review、Compile、Save、Diagnostics 等证据串起来。
3. 生成可导出的 DebugBundle 文件包。
4. 执行隐私裁剪、路径脱敏、token / settings / env 脱敏。
5. 支持开发者复盘问题。
```

Debug 不负责：

```text
1. 不作为正常 Agent 读取蓝图的大 payload 通道。
2. 不替代 RuntimeProfile。
3. 不替代 Diagnostics。
4. 不替代 TaskRunJournal。
5. 不替代 Transaction Journal。
6. 不替代 ReviewRecord。
7. 不作为用户 Review UI。
8. 不默认进入 AgentGuide 正常写入流程。
9. 不通过 MCP 导出 DebugBundle 内容。
```

固定一句话：

```text
Debug 系统是失败证据聚合与文件导出系统，不是 Agent 操作系统。
```

---

## 2. 统一固定入口原则

### 2.1 新增 P0 架构原则

Debug 系统在每一层都有统一固定入口，所有执行信息都必须经过该层的统一 Debug 入口，再决定是否创建 DebugCase、追加 DebugEvent、记录摘要、关联 artifact candidate 或忽略。

固定原则：

```text
所有执行信息必须经过统一 Debug 入口。
禁止各工具、各服务、各 UI 模块私自拼 DebugCase / DebugBundle。
禁止散落式写 Debug JSON。
禁止绕过隐私裁剪直接写 artifact。
```

这里的“所有执行信息”不是指完整大 payload 全量保存，而是指每个执行节点必须把可调试摘要通过统一入口上报：

```text
operation
stage
trace_id
task_run_id / preview_id
transaction_id / review_record_id
status
error / blocker / warning summary
target refs
artifact candidates
redaction requirements
```

统一入口负责判断：

```text
1. 是否属于自动 capture point。
2. 是否应创建新的 DebugCase。
3. 是否应聚合到已有 DebugCase。
4. 是否只记录 transient event。
5. 是否丢弃或降采样。
6. 哪些 evidence 只能保存摘要。
7. 哪些 evidence 可进入 bundle artifact。
```

### 2.2 分层统一入口

| 层 | 统一入口职责 | 示例入口名 |
| --- | --- | --- |
| MCP Tool Wrapper | 捕获工具调用、ToolResultBase、空错误 fallback、trace_id、MCP error | `McpDebugEntry.recordToolEvent` |
| MCP BridgeClient | 捕获连接失败、timeout、malformed response、Bridge response summary | `BridgeDebugEntry.recordTransportEvent` |
| Task Compiler / Orchestration | 捕获 TaskSpec schema / semantic / plan compile 错误 | `TaskCompilerDebugEntry.recordCompileEvent` |
| UE Bridge Command Dispatcher | 捕获 command dispatch、auth 结果、command stage、Bridge request summary | `UEBridgeDebugEntry.recordCommandEvent` |
| UE TaskRuntime | 捕获 preview blocker、execute failure、partial_failure、blocked steps | `TaskRuntimeDebugEntry.recordTaskEvent` |
| UE Capability Service | 捕获具体能力服务失败：Asset Factory、Component、Class Settings、GraphWrite 等 | `CapabilityDebugEntry.recordCapabilityEvent` |
| ToolResultBase Builder | 统一结果 / 错误 / trace / debug hints 归一化 | `ToolResultDebugEntry.recordToolResult` |
| Transaction Journal | 捕获 transaction write failure、rollback failure、role linkage | `TransactionDebugEntry.recordTransactionEvent` |
| Review Action / Store | 捕获 Review Reject failed / needs_action、ReviewRecord linkage | `ReviewDebugEntry.recordReviewEvent` |
| Compile / Save / Diagnostics | 捕获 compile/save failure、diagnostics artifact candidate | `ValidationDebugEntry.recordValidationEvent` |
| Developer Debug UI | 查询 DebugCase、导出 DebugBundle、清理 / prune | `DeveloperDebugEntry.exportBundle / cleanup` |

### 2.3 统一入口的最低合同

所有层的 Debug 入口必须最终落到同一类规范化事件：

```json
{
  "schema": "BlueprintHelper.DebugEvent.v1",
  "debug_event_id": "ev_...",
  "debug_case_id": "dbg_...",
  "created_at": "...",
  "source_layer": "mcp|bridge|task_compiler|ue_bridge|task_runtime|capability|transaction|review|validation|ui",
  "operation": "execute_task",
  "stage": "bridge.execute_task_plan",
  "severity": "info|warning|error|critical",
  "status": "captured|ignored|aggregated",
  "trace_id": "trace_...",
  "task_run_id": "task_...",
  "preview_id": "preview_...",
  "transaction_id": "tx_...",
  "review_record_id": "review_...",
  "asset_paths": ["/Game/..."],
  "error": {
    "code": "bridge_write_failed",
    "message": "Bridge write failed.",
    "retryable": false
  },
  "evidence": {
    "tool_result_summary": {},
    "bridge_response_summary": {},
    "artifact_candidates": []
  },
  "redaction": {
    "profile": "standard",
    "requires_redaction": true
  }
}
```

### 2.4 统一入口的硬性要求

```text
1. Debug 入口不能修改 UE 资产。
2. Debug 入口失败不能掩盖原始工具失败。
3. Debug 入口必须 best-effort；失败时只写内部 fallback log，不再制造二次任务失败。
4. Debug 入口必须先做 redaction classification，再持久化 evidence。
5. Debug 入口不得保存 token / auth_token / bridge token。
6. Debug 入口不得保存 settings.json 全文。
7. Debug 入口不得保存本地绝对路径到 Agent-facing 结果。
8. Debug 入口不得把完整 RawJson 资产正文放入 standard bundle。
9. Debug 入口产生的文件只写入 Saved/BlueprintHelper/Debug。
10. Debug 入口不得通过 MCP 返回 bundle artifact 内容。
```

---

## 3. 核心对象

## 3.1 DebugCase

```text
DebugCase = 一次 user-visible incident 的可调试索引记录。
```

DebugCase 不是单个 trace，也不是单个 transaction。

一个 DebugCase 可以关联：

```text
trace_ids[]
failure_events[]
task_run_id
preview_id
transaction_links[]
review_record_ids[]
bundle_ids[]
asset_paths[]
```

建议 schema：

```json
{
  "schema": "BlueprintHelper.DebugCase.v1",
  "debug_case_id": "dbg_...",
  "created_at": "...",
  "updated_at": "...",
  "source": "task_execute_failure",
  "severity": "error",
  "status": "open|resolved|needs_action|archived",
  "operation": "execute_task",
  "stage": "bridge.execute_task_plan",
  "trace_ids": ["trace_..."],
  "task_run_id": "task_...",
  "preview_id": "preview_...",
  "transaction_links": [],
  "review_record_ids": [],
  "bundle_ids": [],
  "asset_paths": ["/Game/..."],
  "error": {
    "code": "bridge_write_failed",
    "message": "Bridge write failed.",
    "retryable": false
  },
  "runtime_profile_snapshot_summary": {},
  "diagnostics_status": "available|not_available|not_run",
  "privacy_profile": "standard",
  "redactions": [],
  "recommended_next": "inspect_debug_case"
}
```

## 3.2 DebugBundle

```text
DebugBundle = DebugCase 的文件导出证据包。
```

DebugBundle 通过 UE 插件侧 / 本地开发者入口导出，不通过 MCP 工具导出，不通过 MCP 传输内容。

建议 manifest：

```json
{
  "schema": "BlueprintHelper.DebugBundleManifest.v1",
  "bundle_id": "bundle_...",
  "debug_case_id": "dbg_...",
  "format": "directory",
  "created_at": "...",
  "manifest_version": 1,
  "contents": [
    "manifest.json",
    "summary.md",
    "artifacts/runtime_profile.json",
    "artifacts/diagnostics.md",
    "artifacts/task_run_journal.json",
    "artifacts/tool_result.json",
    "artifacts/bridge_response_summary.json",
    "artifacts/transactions/tx_....summary.json",
    "artifacts/review/review_....summary.json",
    "artifacts/assets/BP_Example.logic.md"
  ],
  "privacy": {
    "profile": "standard",
    "contains_tokens": false,
    "contains_full_settings": false,
    "contains_local_absolute_paths": false,
    "contains_full_asset_raw_json": false,
    "contains_source_files": false,
    "redactions": [
      "tokens",
      "env_values",
      "settings_full",
      "local_absolute_paths",
      "full_raw_json"
    ]
  }
}
```

## 3.3 DebugArtifact

```text
DebugArtifact = DebugBundle 内的单个证据文件。
```

例如：

```text
diagnostics.md
runtime_profile.json
task_run_journal.json
tool_result.json
bridge_response_summary.json
compile_result.json
review_record_summary.json
BP_Door.EventGraph.logic.md
```

---

## 4. 13 个核心决策

## 4.1 DebugCase identity

采用：

```text
DebugCase = user-visible incident
```

不是一个 `trace_id`，不是一个 `task_run_id`，不是一个 `transaction_id`。

一个 user-visible incident 可以包含多个 trace / event / transaction。

## 4.2 Capture points

采用：failure / blocker / partial_failure / rollback failed / empty error fallback 自动创建 DebugCase。

自动 capture point：

```text
1. MCP wrapper failure
2. Bridge transport failure
3. empty / malformed Bridge response fallback
4. UE TaskRuntime preview blocker
5. UE TaskRuntime execute failure
6. TaskRunJournal partial_failure
7. compile failure
8. save failure
9. transaction write / rollback failed
10. Review Reject failed / needs_action
11. DebugExport / DebugBundle export itself failed
```

不自动创建 DebugCase：

```text
1. 普通成功写入
2. 普通 warning
3. 普通 Diagnostics warning
4. 普通 Review pending
5. 成功高风险写入
```

## 4.3 Tool surface

采用：

```text
MCP 只保留 blueprinthelper_get_debug_case。
DebugBundle 通过 UE 插件侧 / 本地文件导出。
```

MCP 不提供：

```text
blueprinthelper_export_debug_bundle
read_large_payload_ref
read_debug_artifact_manifest
read_debug_artifact_chunk
DebugBundle 内容读取工具
DebugBundle 删除工具
```

固定一句话：

```text
MCP 只查 DebugCase 摘要；DebugBundle 是 UE 侧文件导出物，不通过 MCP 传输。
```

## 4.4 Agent exposure

采用：failure recovery only。

规则：

```text
Debug 系统只在失败、阻断、partial_failure、rollback failed、用户要求诊断时暴露为 recovery / developer diagnostics path。
正常 Agent workflow 不使用 Debug 系统。
Agent 不得把 Debug 系统当成默认蓝图读取 / 大 payload 读取路径。
```

## 4.5 large_payload_ref

采用：删除，不提供替代 MCP artifact reader。

规则：

```text
不提供 read_large_payload_ref。
不提供 read_debug_artifact_manifest。
不提供 read_debug_artifact_chunk。
不通过 MCP 读取 DebugBundle 内容。
```

## 4.6 Artifact storage

采用：Cases / Bundles / Artifacts 分层。

目录：

```text
Saved/BlueprintHelper/Debug/Cases/<debug_case_id>.json

Saved/BlueprintHelper/Debug/Bundles/<bundle_id>/
  manifest.json
  summary.md
  artifacts/
    runtime_profile.json
    diagnostics.md
    task_run_journal.json
    tool_result.json
    bridge_response_summary.json
    transactions/
    review/
    assets/
    logs/
```

v1 artifact 可跟随 bundle 存储，避免 orphan artifact。

## 4.7 Retention / cleanup

采用：Setting Profile policy + UE Developer Debug UI 手动清理 + 安全自动 prune。

规则：

```text
1. open / needs_action / rollback_failed / export_failed 不自动清理。
2. resolved + low severity + old bundle 可按 policy prune。
3. DebugCase index 不轻易删除。
4. DebugBundle / artifacts 可以清理。
5. 用户可在 UE Debug UI 中手动清理。
6. MCP 不提供删除 DebugBundle 的普通工具。
```

建议 Setting Profile：

```json
{
  "debug_retention": {
    "enabled": true,
    "max_cases": 200,
    "max_total_mb": 512,
    "resolved_case_ttl_days": 30,
    "bundle_ttl_days": 14,
    "keep_needs_action": true,
    "keep_rollback_failed": true,
    "auto_prune_on_startup": false
  }
}
```

## 4.8 Privacy / redaction

采用：privacy profile 分级；默认 `standard`。

Profiles：

```text
standard:
  默认中等脱敏。

strict:
  更少内容，只保留错误摘要、ID、状态和必要 metadata。

expert_full_local:
  用户显式导出更多本地证据，仅限本地开发者入口。
```

standard 默认不得包含：

```text
1. token / auth_token / bridge token
2. 环境变量完整值
3. settings.json 全文
4. 本地绝对路径
5. 完整 AgentGuide / Skill 全文
6. 完整源码
7. 完整 RawJson 资产正文
8. 未裁剪的大型 uasset / binary
```

standard 允许包含：

```text
1. trace_id / task_run_id / transaction_id / review_record_id
2. asset_path，但不含本地磁盘绝对路径
3. operation / stage / status / error code
4. RuntimeProfile 摘要
5. Diagnostics markdown artifact
6. TaskRunJournal
7. Transaction summary
8. ReviewRecord refs
9. targeted logic_md / logic_json slices
10. compile / save diagnostics
11. redacted UE log excerpts
```

## 4.9 Review integration

采用：

```text
ReviewRecord -> debug_case_ids[]
DebugCase -> review_record_ids[] + bundle_ids[]
```

ReviewRecord 不保存：

```text
debug_bundle_refs
artifact paths
bundle local paths
debug payload
```

原因：ReviewRecord 是用户审查事实，DebugBundle 是可清理导出物。ReviewRecord 只链接稳定 DebugCase，不依赖 bundle path。

## 4.10 Transaction integration

采用：失败相关 transaction 全部链接，并标注 role。

```json
{
  "transaction_links": [
    {
      "transaction_id": "tx_001",
      "role": "succeeded_before_failure"
    },
    {
      "transaction_id": "tx_002",
      "role": "failed_transaction"
    },
    {
      "transaction_id": "tx_003",
      "role": "rollback_failed"
    }
  ]
}
```

不因为成功高风险写入自动创建 DebugCase。

Debug 不是审计系统；成功写入由 Transaction Journal / Review 管。

## 4.11 RuntimeProfile / Diagnostics snapshots

采用：RuntimeProfile 摘要内联；Diagnostics 作为 bundle artifact。

规则：

```text
DebugCase 内联 runtime_profile_snapshot_summary。
Diagnostics markdown 不直接塞进 DebugCase 主体。
DebugBundle 导出时包含 diagnostics.md。
未导出 Bundle 时，DebugCase 只保留 diagnostics_status / diagnostics_available。
```

## 4.12 UI integration

采用：独立 Developer Debug tab。

```text
Review tab:
  用户审查真实改动。
  只显示 has_debug_info / debug_case_ids。

Developer Debug tab:
  查看 DebugCase。
  导出 DebugBundle。
  清理 DebugBundle / resolved cases。
  查看隐私裁剪状态。
```

Debug 不混入 ReviewPanel，Review 不承担开发诊断 UI。

## 4.13 Regression tests

采用：完整链路测试，但按阶段落地。

测试链路：

```text
capture -> correlation -> redaction -> file bundle export -> cleanup
```

阶段：

```text
Phase 0: Schema / Storage
Phase 1: Auto Capture / Correlation
Phase 2: Privacy / Redaction
Phase 3: File Bundle Export
Phase 4: Review / Transaction / Diagnostics Integration
Phase 5: Developer Debug Tab / Cleanup
Phase 6: Full-chain Smoke
```

---

## 5. 阶段验收标准

## Phase 0：Schema / Storage 基础验收

目标：固定 DebugCase / DebugBundle 的持久结构。

必须实现：

```text
DebugCase.v1
DebugBundleManifest.v1
Saved/BlueprintHelper/Debug/Cases/
Saved/BlueprintHelper/Debug/Bundles/
Saved/BlueprintHelper/Debug/Artifacts/ 或 bundle-local artifacts/
```

验收标准：

```text
1. 可以创建 DebugCase JSON。
2. DebugCase 必须包含：
   - debug_case_id
   - created_at
   - source
   - severity
   - operation
   - stage
   - trace_ids[]
   - task_run_id?
   - transaction_links[]
   - review_record_ids[]
   - asset_paths[]
   - error
   - runtime_profile_snapshot_summary?
   - bundle_ids[]
   - privacy_profile
3. 可以创建 DebugBundle manifest。
4. DebugBundle 必须包含：
   - bundle_id
   - debug_case_id
   - manifest_version
   - contents[]
   - privacy{}
5. 所有 Debug 写入只写 Saved/BlueprintHelper/Debug，不修改 UE 资产。
6. DebugCase / DebugBundle 不是 Transaction Journal，也不是 ReviewRecord。
```

必测用例：

```text
CreateDebugCase_MinimalFields_Pass
CreateDebugBundleManifest_MinimalFields_Pass
DebugStorage_DoesNotModifyAsset_Pass
DebugCase_SchemaRejectsMissingId_Fail
```

## Phase 1：Auto Capture / Correlation 验收

目标：失败、阻断、partial failure 自动创建 DebugCase，并能串联跨层 ID。

必须实现 capture point：

```text
1. MCP wrapper failure
2. Bridge transport failure
3. empty / malformed Bridge response fallback
4. UE TaskRuntime preview blocker
5. UE TaskRuntime execute failure
6. TaskRunJournal partial_failure
7. compile failure
8. save failure
9. transaction write / rollback failed
10. Review Reject failed / needs_action
```

验收标准：

```text
1. 每个 capture point 都能创建 DebugCase。
2. DebugCase 粒度是 user-visible incident，而不是单个 trace_id。
3. 同一 incident 下多个 trace_id 必须聚合到同一 DebugCase。
4. partial_failure 必须记录：
   - task_run_id
   - failed step
   - blocked downstream steps
   - transaction_links
5. transaction_links 必须带 role：
   - succeeded_before_failure
   - failed_transaction
   - rollback_failed
6. get_debug_case 能按 debug_case_id 返回摘要。
7. get_debug_case modified=false。
8. get_debug_case 不返回 bundle artifact 内容。
```

必测用例：

```text
EmptyBridgeError_CreatesNonEmptyDebugCase
BridgeTransportFailure_CapturesTraceAndStage
PreviewBlocker_CapturesPreviewIdAndReason
ExecutePartialFailure_CapturesTaskRunAndBlockedSteps
CompileFailure_CapturesCompileStage
RollbackFailed_CapturesTransactionRole
GetDebugCase_ModifiedFalse_NoArtifacts
```

## Phase 2：Privacy / Redaction 验收

目标：DebugBundle 默认 standard privacy profile，不能泄露敏感内容。

验收标准：

```text
1. standard bundle 中 contains_tokens=false。
2. standard bundle 中 contains_full_settings=false。
3. standard bundle 中 contains_local_absolute_paths=false。
4. standard bundle 中 contains_full_asset_raw_json=false。
5. standard bundle 中 contains_source_files=false。
6. manifest.privacy.redactions 必须列出实际执行的脱敏规则。
7. 如果检测到无法脱敏的敏感内容，bundle export 必须失败或跳过该 artifact，并写入 skipped_artifacts。
```

必测用例：

```text
Redaction_RemovesBridgeToken
Redaction_RemovesEnvValues
Redaction_RemovesFullSettings
Redaction_RemovesLocalAbsolutePaths
Redaction_BlocksFullRawJsonInStandardProfile
Redaction_ManifestReportsPrivacyFlags
```

## Phase 3：File Bundle Export 验收

目标：DebugBundle 通过 UE 插件侧 / 本地文件导出，不通过 MCP 传输 bundle 内容。

必须实现本地开发者入口：

```text
Export Debug Bundle
Export Task Debug Bundle
Export Transaction Debug Bundle
Export Review Debug Bundle
```

MCP 不提供：

```text
blueprinthelper_export_debug_bundle
read_large_payload_ref
read_debug_artifact_manifest
read_debug_artifact_chunk
```

验收标准：

```text
1. Bundle 导出到 Saved/BlueprintHelper/Debug/Bundles/<bundle_id>/。
2. 必须生成 manifest.json。
3. 必须生成 summary.md。
4. artifacts/ 目录存在。
5. manifest.contents 必须列出所有导出文件。
6. DebugCase.bundle_ids[] 记录 bundle_id。
7. ReviewRecord 不直接保存 bundle path。
8. MCP get_debug_case 只显示 bundle_ids / export status，不返回本地绝对路径。
9. Bundle export modified=false，不修改 UE 资产。
```

必测用例：

```text
ExportDebugBundle_CreatesManifestAndSummary
ExportDebugBundle_WritesBundleIdToDebugCase
ExportDebugBundle_DoesNotExposeLocalPathToMcp
ExportDebugBundle_DoesNotModifyAssets
NoMcpDebugArtifactReader_RegisteredToolsDoNotContainReader
NoLargePayloadRef_DebugPathDoesNotExposeLargePayloadRef
```

## Phase 4：Review / Transaction / Diagnostics 集成验收

目标：DebugCase 能链接 Review、Transaction、RuntimeProfile、Diagnostics，但不替代它们。

验收标准：

```text
1. Review Reject failed / needs_action 时创建 DebugCase。
2. ReviewRecord.debug_case_ids[] 写入 debug_case_id。
3. DebugCase.review_record_ids[] 写入 review_record_id。
4. DebugCase 不内联 ReviewRecord 全量内容。
5. DebugCase 内联 RuntimeProfile 摘要，不含 token / full settings。
6. Diagnostics markdown 作为 bundle artifact，不塞入 DebugCase 主体。
7. Transaction links 必须包含 role。
```

必测用例：

```text
ReviewRejectFailed_AddsDebugCaseIdToReviewRecord
DebugCase_LinksBackToReviewRecord
DebugCase_DoesNotStoreBundlePathInReviewRecord
RuntimeProfileSummary_Inline_NoSensitiveFields
DiagnosticsMarkdown_ExportedAsArtifact
TransactionLinks_IncludeRole
```

## Phase 5：Developer Debug Tab / Cleanup 验收

目标：UE 窗口中 Debug UI 独立于 Review UI，支持查看、导出、清理。

验收标准：

```text
1. Developer Debug tab 独立存在。
2. Review tab 不展示 DebugBundle 内容。
3. open / needs_action / rollback_failed / export_failed 不自动清理。
4. resolved + low severity + old bundle 可按 policy prune。
5. prune 不删除 active needs_action case。
6. MCP 不提供删除 DebugBundle 工具。
7. Debug UI 手动清理必须记录 cleanup result。
```

必测用例：

```text
DeveloperDebugTab_ListsDebugCases
ReviewTab_ShowsOnlyHasDebugInfo
Cleanup_PrunesResolvedOldBundle
Cleanup_DoesNotPruneNeedsAction
Cleanup_DoesNotPruneRollbackFailed
McpTools_DoNotExposeDeleteDebugBundle
```

## Phase 6：Full-chain Smoke 验收

目标：验证系统从失败产生到 bundle 导出再到 cleanup 的闭环。

### Smoke 1：Bridge 空错误

```text
模拟 Bridge 返回空错误
-> MCP fallback 生成非空 ToolResult error
-> 自动创建 DebugCase
-> get_debug_case 可读摘要
-> UE 导出 DebugBundle
-> manifest / summary / bridge_response_summary 存在
```

验收：

```text
DebugCase 非空。
error.code 非空。
stage 非空。
trace_id 存在。
Bundle 不含 token / local absolute path。
```

### Smoke 2：Task partial failure

```text
Task 执行：
step_1 成功并产生 tx_001
step_2 失败并产生 tx_002 failure
step_3 blocked

-> TaskRunJournal partial_failure
-> DebugCase 创建
-> transaction_links 记录 tx_001 / tx_002
-> blocked steps 进入 DebugCase / artifact
```

验收：

```text
tx_001 role=succeeded_before_failure
tx_002 role=failed_transaction
blocked step 可追踪
get_debug_case 不返回 artifact 内容
```

### Smoke 3：Review Reject failed

```text
Review Reject 某 atomic target
-> TOCTOU current_hash mismatch
-> reject_failed / needs_action
-> DebugCase 创建
-> ReviewRecord.debug_case_ids 写入
-> Bundle 导出含 rollback fragment / Review summary
```

验收：

```text
ReviewRecord 只保存 debug_case_ids。
DebugCase 保存 review_record_ids。
Bundle manifest 包含 review summary artifact。
ReviewRecord 不保存 bundle path。
```

---

## 6. 实现顺序建议

```text
1. DebugEvent / DebugCase / DebugBundleManifest DTO。
2. UE DebugCaseStoreService。
3. 分层 DebugEntry 统一入口。
4. MCP get_debug_case。
5. 自动 capture points 接入。
6. RuntimeProfile summary / Diagnostics artifact candidate 接入。
7. Transaction / Review linkage 接入。
8. UE Developer Debug tab。
9. File bundle export。
10. Retention / cleanup。
11. Full-chain smoke tests。
```

---

## 7. 最终架构口径

```text
BlueprintHelper Debug System v1

- DebugCase 是 user-visible incident。
- DebugEvent 是每层统一入口上报的规范化执行事件。
- DebugBundle 是 UE 本地文件导出的证据包。
- MCP 只提供 get_debug_case 摘要查询。
- 所有执行信息必须经过每层统一 Debug 入口。
- Debug 不进入正常 Agent 读写流程。
- Debug 不提供 large_payload_ref 或 artifact reader。
- Debug 可以链接 ReviewRecord / TransactionJournal / TaskRunJournal，但不替代它们。
- DebugBundle 必须执行 privacy profile redaction。
- Developer Debug tab 独立于 Review tab。
```
