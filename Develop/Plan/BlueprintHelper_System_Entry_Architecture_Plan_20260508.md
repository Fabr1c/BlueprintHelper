# BlueprintHelper System Entry Architecture Implementation Plan

> **For agentic workers:** Use `subagent-driven-development` or `executing-plans` if this plan is implemented later. Track steps with checkboxes and keep each change small.

**Goal:** 给 BlueprintHelper 每个系统固定一个统一入口，并把 DebugBundle 与 Review 作为所有工具簇必须遵守的横切能力。

**Architecture:** 保持现有四层架构不改名：User Guidance & Setup Layer、Agent Skill Layer、MCP Server Layer、UE Plugin Layer。MCP Server Layer 内部明确拆成 TypeScript MCP Gateway 与 Python Orchestration 两段；UE Plugin Layer 内部保留现有工具簇和 TaskRuntime 分发方式，只补齐统一系统入口、DebugBundle 导出能力、写工具 Review 接入。

**Tech Stack:** Unreal Engine C++ 插件、TypeScript MCP Server、Python Orchestration / Task Compiler、Markdown 架构文档、UE Automation、Node regression tests。

---

## 0. 决策摘要

本计划不引入动态能力注册模型。

统一入口的含义是：

```text
1. 每个系统有固定入口，便于理解、测试和后续接入。
2. 所有工具簇都必须能导出 DebugBundle。
3. 所有写工具都必须接入 Review 系统。
4. 普通 Agent 仍只走 TaskSpec-first / ReadSpec-first 主线。
5. 底层工具簇仍是 UE Plugin Layer 内部 capability、debug / expert 工具和测试入口。
```

四层链路保持：

```text
Agent 可见入口
-> TypeScript MCP Gateway
-> Python Orchestration Layer
-> Python Task Compiler / Read Router / Error Normalizer
-> Bridge Client
-> UE TaskRuntime / System Entry
-> Existing UE Capability Clusters
-> Unreal Editor
```

---

## 1. 架构原则

### 1.1 四层不变

| 层 | 固定职责 | 入口方向 |
|---|---|---|
| User Guidance & Setup Layer | 安装、配置、Setup Profile、项目规则、用户偏好 | `AGENTS.md`、AgentGuide、SetupProfile |
| Agent Skill Layer | 生成 ReadSpec / TaskSpec，处理 preview 错误和 stop_and_report | AgentGuide 主线入口 |
| MCP Server Layer | 暴露少量 Agent-facing tools，管理 schema、Python 编排、read router、Bridge、错误归一化 | TypeScript MCP Gateway、Python Orchestration、Task Tools、Read Router、DebugCase 摘要 |
| UE Plugin Layer | 读取和修改 UE 资产，执行 TaskPlan，管理 Transaction、Review、Debug、Diagnostics | 固定 System Entry + 现有工具簇 |

Capability Clusters 不是新的外部层。它们是 UE Plugin Layer 内部能力。

### 1.1.1 MCP Server Layer 内部链路

MCP Server Layer 必须明确分成两段：

```text
TypeScript MCP Gateway
-> Python Orchestration Layer
-> Bridge Client
```

职责边界：

| 子层 | 职责 | 当前代码依据 |
|---|---|---|
| TypeScript MCP Gateway | 注册 Agent-facing tools、包装输入输出、调用 Python、调用 Bridge、归一化 MCP result | `BlueprintHelper_MCP_Server/src/task-tools.ts`、`tool-result.ts`、`bridge-client.ts` |
| Python Orchestration Layer | TaskSpec schema / semantic 校验、上下文消费、resource disambiguation、TaskPlan 生成、TaskPlan summary、Python 侧错误对象 | `BlueprintHelper_MCP_Server/src/task-python-orchestrator.ts`、`python/blueprinthelper_task/orchestrator.py` |
| Bridge Client | 把编排后的 TaskPlan / command payload 传给 UE Bridge，并返回底层事实 | `BlueprintHelper_MCP_Server/src/bridge-client.ts` |

固定规则：

```text
1. Agent 不直接调用 Python。
2. TypeScript MCP Gateway 不承载完整 TaskSpec 语义编译逻辑。
3. Python Orchestration 不直接修改 UE 资产。
4. UE TaskRuntime 是执行事实来源，Python Orchestration 是计划和错误解释来源。
5. DebugBundle / Review evidence 的最终事实来自 UE，但 Python Orchestration 必须保留 preview_id、task_run_id、trace_id、TaskPlan summary 和 error normalization linkage。
```

### 1.2 横切约束

```text
1. 写入默认走 TaskSpec -> TaskPlan -> TaskRuntime。
2. TaskSpec -> TaskPlan 由 Python Orchestration 负责，TypeScript MCP Gateway 只负责调用和封装。
3. 读取默认走 ReadSpec -> Python / MCP Read Router -> Read Capability。
4. 所有工具簇都必须能提供 DebugBundle 证据摘要或 artifact candidate。
5. 所有修改 UE 资产的工具都必须生成 ReviewRecord / ReviewVisibleChange / ReviewAtomicTarget。
6. Debug 默认只在失败恢复或开发诊断路径暴露。
7. Review 是用户侧审查系统，不进入普通 Agent 执行闭环。
8. Transaction 是内部事实来源，不作为用户主查询面。
9. 新工具不得绕过 ToolResultBase、DebugEvent、Transaction、Review 边界。
```

### 1.3 新工具接入口径

新工具接入不是新增动态 capability。接入时只检查它要补齐哪些固定横切接口：

```text
1. 属于哪个现有工具簇或系统。
2. 是否读取 UE 资产。
3. 是否修改 UE 资产。
4. 是否需要 TaskSpec / TaskPlan adapter。
5. Python Orchestration 是否需要新增 schema / semantic validation / compiler lowering。
6. 是否需要 ReadSpec / Read Router 支持。
7. DebugBundle 需要导出哪些证据摘要。
8. 写入后 Review 要展示哪些 visible changes。
9. Transaction 需要记录哪些 rollback_data。
10. 是否允许普通 Agent-facing。
```

---

## 2. 系统入口模型

### 2.1 固定 System Entry

System Entry 是每个系统的稳定门面，不承担动态能力发现。

建议入口：

| system_id | 固定入口 | 职责 |
|---|---|---|
| `mcp_gateway` | `registerTaskTools / registerTools` | Agent-facing tool 注册、输入输出封装、MCP result 归一化 |
| `python_orchestration` | `task-python-orchestrator.ts` + `python/blueprinthelper_task/orchestrator.py` | TaskSpec 编译、TaskPlan summary、Python 错误归一化 |
| `task_runtime` | `FBlueprintHelperTaskRuntimeService` | TaskPlan preview / execute / get journal |
| `read_context` | `FBlueprintHelperContextService` | ReadSpec / context pack / reference context |
| `runtime_diagnostics` | `FBlueprintHelperRuntimeDiagnosticsEntry` | RuntimeProfile、Diagnostics、Compile、Save、AssetBrowse |
| `transaction` | `FBlueprintHelperTransactionEntry` | Transaction Journal 查询、rollback linkage、debug summary |
| `review` | `FBlueprintHelperReviewEntry` | ReviewRecordQuery、ReviewAction、ReviewStore |
| `debug` | `FBlueprintHelperDebugEntry` | DebugEvent、DebugCase、DebugBundle export |
| `developer_ui` | `FBlueprintHelperDeveloperDebugEntry` | Developer Debug tab、本地导出、清理 |

### 2.2 工具簇 DebugBundle 合同

每个工具簇必须提供 DebugBundle 证据摘要。

最低字段：

```json
{
  "schema": "BlueprintHelper.ToolClusterDebugEvidence.v1",
  "tool_cluster": "graph_write",
  "operation": "merge_blueprint_graph",
  "stage": "execute",
  "trace_id": "trace_...",
  "task_run_id": "task_...",
  "transaction_id": "tx_...",
  "asset_paths": ["/Game/..."],
  "status": "completed|failed|blocked|partial_failure",
  "error_summary": {
    "code": "pin_type_mismatch",
    "message": "Pin types are incompatible.",
    "retryable": false
  },
  "artifact_candidates": [
    {
      "kind": "logic_md_slice",
      "safe_for_standard_bundle": true
    }
  ],
  "redaction": {
    "requires_redaction": true,
    "profile": "standard"
  }
}
```

规则：

```text
1. evidence 是 DebugBundle 输入，不是普通 Agent 默认返回。
2. standard DebugBundle 不包含 token、settings 全文、本地绝对路径、完整 RawJson 资产正文。
3. DebugBundle 通过 UE 本地开发者入口导出，不通过 MCP 传输 artifact 内容。
4. MCP 只保留 DebugCase 摘要查询。
```

### 2.3 写工具 Review 合同

每个写工具必须输出 Review 可聚合事实。

最低字段：

```json
{
  "schema": "BlueprintHelper.WriteReviewEvidence.v1",
  "tool_cluster": "blueprint_component",
  "operation": "set_component_properties",
  "task_run_id": "task_...",
  "archive_session_id": "archive_...",
  "transaction_id": "tx_...",
  "asset_path": "/Game/...",
  "visible_changes": [
    {
      "surface": "component",
      "visual_group_key": "component:DoorMesh",
      "summary": "Changed DoorMesh physics settings.",
      "atomic_targets": [
        {
          "target_kind": "component_property",
          "target_key": "hash(normalized_anchor_identity)",
          "anchor": {},
          "rollback_data_ref": "tx_...",
          "recorded_after_hash": "sha256:..."
        }
      ]
    }
  ]
}
```

规则：

```text
1. ReviewRecord identity 仍是 archive_session_id + asset_path。
2. ReviewStore 负责折叠 visible changes，不临时猜 anchor。
3. producer 写工具负责提供 affected atomic targets。
4. Reject 只做机械回滚和 TOCTOU 检查，不做依赖判断。
5. Agent 默认不操作 ReviewPanel。
```

---

## 3. 第一批工具簇覆盖范围

| 工具簇 | DebugBundle | Review |
|---|---|---|
| GraphWrite Append / Replace / Patch / Merge | 必须导出 graph、block、anchor、node/link 摘要和失败 stage | 必须输出 graph_node、graph_link、graph_pin、block visible changes |
| AssetFactory | 必须导出创建目标、资产类型、路径、失败原因 | 创建、删除、重用策略产生的资产级 visible change |
| BlueprintComponent | 必须导出组件树摘要、组件名、属性路径、失败 stage | component 和 component_property visible changes |
| BlueprintClassSettings | 必须导出 class setting、interface、parent / metadata 摘要 | implemented_interface、class_setting visible changes |
| BlueprintSignature | 必须导出 function、event、dispatcher signature 摘要 | function_signature、event_signature visible changes |
| BlueprintVariables | 必须导出 member / local variable 摘要 | variable、local_variable visible changes |
| UMGWidget | 必须导出 widget tree slice、widget property 摘要 | widget tree、widget property visible changes |
| DataTable | 必须导出 row name、row struct、field 摘要 | data_table_row visible changes |
| DataAsset / ObjectProperty | 必须导出 object path、property path、value summary | object_property visible changes |
| CleanupOwnership | 必须导出 block ownership、rollback target、anchor 摘要 | ownership conversion 和 cleanup visible changes |
| RuntimeDiagnostics / Compile / Save | 必须导出 diagnostics markdown、compile/save summary | 不写资产时不进入 Review；若 save 状态作为用户可见变更，需要记录为 asset persistence summary |
| Transaction / Review action | 必须导出 transaction role、review action、rollback failure 摘要 | Review action 本身进入 Review history，不再生成新的普通 asset visible change |

---

## 4. 目标文件结构

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperDebugBundleTypes.h
Source/BlueprintHelper/Public/Services/Debug/BlueprintHelperDebugEntry.h
Source/BlueprintHelper/Private/Services/Debug/BlueprintHelperDebugEntry.cpp
Source/BlueprintHelper/Public/Services/Debug/BlueprintHelperDebugBundleService.h
Source/BlueprintHelper/Private/Services/Debug/BlueprintHelperDebugBundleService.cpp
Source/BlueprintHelper/Public/Services/Review/BlueprintHelperReviewEntry.h
Source/BlueprintHelper/Private/Services/Review/BlueprintHelperReviewEntry.cpp
Source/BlueprintHelper/Public/Structure/Review/BlueprintHelperWriteReviewEvidenceTypes.h
BlueprintHelper_MCP_Server/python/blueprinthelper_task/debug_evidence.py
```

### 4.2 修改文件

```text
Source/BlueprintHelper/Public/TaskRuntime/BlueprintHelperTaskRuntimeService.h
Source/BlueprintHelper/Private/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp
Source/BlueprintHelper/Private/BlueprintHelper.cpp
Source/BlueprintHelper/Public/Structure/BlueprintHelperToolResultTypes.h
Source/BlueprintHelper/Private/Structure/BlueprintHelperToolResultBuilder.cpp
Source/BlueprintHelper/Public/Services/Review/BlueprintHelperReviewStoreService.h
Source/BlueprintHelper/Private/Services/Review/BlueprintHelperReviewStoreService.cpp
BlueprintHelper_MCP_Server/src/task-tools.ts
BlueprintHelper_MCP_Server/src/task-python-orchestrator.ts
BlueprintHelper_MCP_Server/src/tool-result.ts
BlueprintHelper_MCP_Server/src/task-result-store.ts
BlueprintHelper_MCP_Server/python/blueprinthelper_task/orchestrator.py
BlueprintHelper_MCP_Server/python/blueprinthelper_task/errors.py
Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md
Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md
Resources/AgentGuide/Reference/02_TaskSpec_First_Tool_Selection.md
```

### 4.3 测试文件

```text
Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperDebugBundleTests.cpp
Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperWriteReviewEvidenceTests.cpp
Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteReviewEvidenceTests.cpp
Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperToolClusterDebugEvidenceTests.cpp
BlueprintHelper_MCP_Server/src/tools.regression.test.ts
BlueprintHelper_MCP_Server/src/task-tools.regression.test.ts
BlueprintHelper_MCP_Server/src/task-python-orchestrator.regression.test.ts
BlueprintHelper_MCP_Server/src/task-result-store.test.ts
BlueprintHelper_MCP_Server/python/tests/test_orchestrator.py
```

---

## 5. 实施阶段

### Phase 0：合同冻结

- [ ] 固定 `BlueprintHelper.ToolClusterDebugEvidence.v1`。
- [ ] 固定 `BlueprintHelper.WriteReviewEvidence.v1`。
- [ ] 固定 Python Orchestration 在 DebugBundle / Review 链路中的职责边界。
- [ ] 在 TaskSpec / TaskPlan 合同中明确所有工具簇的 DebugBundle / Review 横切要求。
- [ ] 在 AgentGuide 中明确普通 Agent 不操作 ReviewPanel，也不读取 DebugBundle artifact 内容。

验收：

```text
文档能回答每个工具簇导出哪些 DebugBundle evidence。
文档能回答每个写工具产生哪些 Review atomic targets。
文档能回答 Python Orchestration 保存哪些关联 ID 和错误归一化事实。
文档明确不引入动态能力注册。
```

### Phase 1：Python Orchestration linkage

- [ ] `task-python-orchestrator.ts` 明确输出 `preview_id`、TaskPlan summary、compiler issues、error code。
- [ ] Python `orchestrator.py` 保持只做编译和错误解释，不直接写 UE 资产。
- [ ] Python compiler result 增加 debug linkage summary，供 TypeScript MCP Gateway 和后续 DebugCase 使用。
- [ ] TypeScript MCP Gateway 在 preview / execute 失败时保留 Python error、Bridge error、TaskPlan summary 的关联。
- [ ] `get_task_result` 继续以 UE TaskRunJournal 为事实来源，Python / MCP 只做 fallback normalization。

验收：

```text
Python Orchestration 是 TaskSpec -> TaskPlan 和 Task Error 的权威位置。
UE TaskRuntime 是执行、Transaction、Review、Debug evidence 的权威位置。
MCP result 可以关联 Python compiler issue、Bridge trace_id、UE task_run_id。
```

### Phase 2：DebugBundle 横切基础

- [ ] 新增 DebugBundle DTO 和 manifest。
- [ ] 新增 DebugEntry / DebugBundleService。
- [ ] ToolResultBase 增加 debug evidence 引用字段或内部 attachment。
- [ ] TaskRuntime 在 preview / execute / partial_failure / compile / save 失败时收集 evidence。
- [ ] GraphWrite、Component、ClassSettings、Signature、Widget、DataTable、ObjectProperty、CleanupOwnership 输出第一批 evidence。
- [ ] Python Orchestration 失败时输出 compiler evidence summary，但不生成最终 DebugBundle。

验收：

```text
每个工具簇至少能提供一份 DebugBundle evidence summary。
Debug 入口 best-effort，失败不掩盖原始错误。
standard profile 不泄露 token、settings 全文、本地绝对路径、完整 RawJson。
MCP 不返回 DebugBundle artifact 内容。
```

### Phase 3：Review 横切基础

- [ ] 新增 WriteReviewEvidence DTO。
- [ ] ReviewStore 增加 consume evidence 聚合入口。
- [ ] TaskRuntime 在每个成功写 step 后提交 Review evidence。
- [ ] GraphWrite、Component、ClassSettings、Signature、Variable、Widget、DataTable、ObjectProperty、CleanupOwnership 接入第一批 Review evidence。
- [ ] ArchiveSession 仍按 task_run 创建，ReviewRecord 仍按 asset_path 拆分。
- [ ] Python Orchestration 不创建 ReviewRecord，只把 TaskPlan steps、preview_id 和 generated intent linkage 留给 TaskRunJournal。

验收：

```text
所有修改 UE 资产的写工具都生成 ReviewRecord。
ReviewRecord identity = archive_session_id + asset_path。
ReviewVisibleChange 由 producer evidence 聚合，不由 ReviewStore 临时猜测。
Agent 默认最终报告不展开 Review 内部状态。
```

### Phase 4：Transaction linkage

- [ ] 每个写工具保证 transaction_id 可链接到 Review evidence。
- [ ] rollback_data_ref 和 target_key 保持一致。
- [ ] Transaction failure / rollback failed 自动创建 DebugCase。
- [ ] DebugBundle 包含 transaction role summary。
- [ ] TypeScript MCP Gateway 和 Python Orchestration 保留 transaction summary linkage，但不替代 UE Transaction Journal。

验收：

```text
Review Reject 可通过 target_key 定位 rollback_data。
TOCTOU current_hash mismatch 时进入 needs_action 或 reject_failed。
DebugCase 能链接 transaction_id、task_run_id、review_record_id。
```

### Phase 5：工具簇补齐

- [ ] 按工具簇表逐个补齐 DebugBundle evidence。
- [ ] 按工具簇表逐个补齐 Review evidence。
- [ ] 每个工具簇新增一条成功写入 Review 测试。
- [ ] 每个工具簇新增一条失败 DebugBundle 测试。
- [ ] MCP regression 保持 Agent-facing 工具面不扩散。
- [ ] Python compiler / orchestrator tests 覆盖新增 TaskSpec lowering 和 error normalization。

验收：

```text
新增工具簇不要求普通 Agent 知道底层 operation。
所有工具簇都有 DebugBundle 出口。
所有写工具都有 Review 出口。
TaskSpec / ReadSpec 仍是普通 Agent 的唯一主线输入。
```

### Phase 6：Developer Debug UI 和清理

- [ ] Developer Debug tab 列出 DebugCase。
- [ ] Developer Debug tab 支持导出 DebugBundle。
- [ ] DebugBundle cleanup 遵守 retention policy。
- [ ] Review tab 只显示 has_debug_info / debug_case_ids，不显示 DebugBundle 内容。

验收：

```text
Developer Debug tab 和 Review tab 分离。
open / needs_action / rollback_failed / export_failed 不自动清理。
MCP 不提供删除 DebugBundle 的普通工具。
```

---

## 6. 新工具接入清单

每次新增工具或工具簇必须完成：

```text
1. 归入现有系统入口。
2. 定义 ToolResultBase 成功 / 失败摘要。
3. 定义 DebugBundle evidence。
4. 如果会修改 UE 资产，定义 WriteReviewEvidence。
5. 如果会修改 UE 资产，定义 transaction rollback_data。
6. 如果属于 TaskSpec-first 主线，增加 TaskPlan adapter。
7. 如果属于 ReadSpec-first 主线，增加 Read Router 支持。
8. 更新 Python Orchestration、Task Compiler 或 Read Router。
9. 更新 TypeScript MCP Gateway contract / schema。
10. 增加 Python compiler / orchestrator 测试。
11. 增加 UE automation 测试。
12. 增加 MCP regression 测试。
13. 更新 AgentGuide 或 Setup 文档。
```

禁止：

```text
1. 先新增普通 Agent-facing 原子写 tool。
2. 让工具簇私自写 Debug JSON。
3. 让写工具绕过 ReviewStore。
4. 让 DebugBundle 通过 MCP 返回 artifact 内容。
5. 让 ReviewRecord 内联 DebugBundle payload。
```

---

## 7. 风险和取舍

### 7.1 主要风险

```text
1. DebugBundle evidence 过大，重新变成 bulk payload。
2. Review evidence 缺 anchor，导致 Reject 无法稳定定位。
3. Debug、Review、Transaction 边界混用。
4. MCP legacy tools 仍被误认为普通 Agent 主线入口。
5. 部分写工具漏接 Review，造成用户审查视图不完整。
```

### 7.2 控制方式

```text
1. DebugBundle evidence 先存摘要和 artifact candidate，不直接塞完整资产正文。
2. producer 写工具必须提供 atomic target anchor。
3. ReviewStore 只折叠 evidence，不猜测 anchor。
4. ToolResultBase 只暴露 DebugCase 摘要引用，不暴露 artifact 内容。
5. legacy/debug/expert tools 通过描述和测试保持隔离。
```

---

## 8. 验证命令

MCP 侧：

```powershell
npm.cmd test
```

建议工作目录：

```text
BlueprintHelper_MCP_Server
```

Python 编排层：

```powershell
python -m unittest discover -s python/tests
```

建议工作目录：

```text
BlueprintHelper_MCP_Server
```

UE 侧：

```text
运行 BlueprintHelper 相关 UE Automation tests。
重点覆盖 DebugBundle evidence、WriteReviewEvidence、ToolResultBase、Review、RuntimeDiagnostics。
```

当前 Codex 沙箱可能无法完成上层项目 Build。需要本地具备写入 `G:\UnrealPractise\MrStone\Intermediate` 的环境后再执行 UE 编译和 smoke。

---

## 9. 完成标准

```text
1. 每个系统都有明确固定入口。
2. MCP Server Layer 明确包含 TypeScript MCP Gateway 和 Python Orchestration。
3. Python Orchestration 负责 TaskSpec -> TaskPlan、TaskPlan summary、Task Error，不直接写 UE 资产。
4. UE TaskRuntime 负责真实执行、Transaction、Review、Debug evidence。
5. 所有工具簇都能导出 DebugBundle evidence。
6. 所有写工具都能提交 Review evidence。
7. 所有写工具都能链接 transaction_id 和 rollback_data。
8. DebugCase 能关联 trace_id、preview_id、task_run_id、transaction_id、review_record_id。
9. MCP 不传输 DebugBundle artifact 内容。
10. ReviewRecord 不内联 DebugBundle payload。
11. AgentGuide 仍保持 TaskSpec-first / ReadSpec-first 主线。
12. Review 和 Debug 继续保持用户审查与开发诊断分离。
```

最终口径：

```text
系统入口负责收敛边界。
Python Orchestration 负责计划编译、错误解释和关联 ID 传递。
工具簇负责提供 DebugBundle evidence。
写工具负责提供 Review evidence。
Service 负责执行 UE 操作。
TaskSpec / ReadSpec 负责普通 Agent 输入。
Debug / Review / Transaction 负责失败诊断、用户审查和审计事实。
```
