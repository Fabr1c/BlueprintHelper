# BlueprintHelper Source 架构分层归类 v1

## 目的

本文把 `Source/BlueprintHelper` 按架构职责归成大类，作为后续 Review 闭环、DebugBundle、工具簇接入和源码整理的统一索引。

当前版本先定义逻辑分层和归属规则，不要求立刻大规模移动 C++ 文件。物理迁移会影响大量 include 路径和 Unreal Build 缓存，应作为独立重构阶段处理。

## 总体分层

```text
User Guidance & Setup
-> Agent Skill
-> MCP Server
   -> TypeScript MCP Gateway
   -> Python Orchestration
-> UE Plugin Source
   -> Entry / Bridge
   -> TaskRuntime
   -> Tool Clusters
   -> Review Audit
   -> Debug Diagnostics
   -> Transaction Facts
   -> Safety / Shared DTO / UI
```

## UE Plugin Source 大类

### 1. Entry / Bridge 入口层

职责：模块启动、Bridge 命令路由、Agent-facing MCP 请求到 UE 服务的固定入口。

当前源码归属：

```text
Source/BlueprintHelper/Private/BlueprintHelper.cpp
Source/BlueprintHelper/Private/Bridge
Source/BlueprintHelper/Public/Bridge
```

规则：

```text
1. 只做命令注册、参数解析、结果封装和服务转发。
2. 不直接实现工具簇业务逻辑。
3. 不创建 ReviewRecord，不生成 DebugBundle 正文。
```

### 2. TaskRuntime 编排执行层

职责：执行 Python Orchestration 产出的 TaskPlan，创建 task_run、ArchiveSession、Transaction、Review evidence 和执行摘要。

当前源码归属：

```text
Source/BlueprintHelper/Private/TaskRuntime
Source/BlueprintHelper/Public/TaskRuntime
Source/BlueprintHelper/Private/TaskRuntime/TaskPlanAdapters
Source/BlueprintHelper/Public/TaskRuntime/TaskPlanAdapters
```

规则：

```text
1. UE TaskRuntime 是执行事实来源。
2. Python Orchestration 只负责 TaskSpec -> TaskPlan、计划摘要和错误归一化。
3. TaskRuntime 不把 ReviewPanel 流程暴露给普通 Agent。
4. TaskRuntime 可以聚合工具簇产出的 WriteReviewEvidence，但不临时猜测 atomic target anchor。
```

### 3. Tool Clusters 工具簇大类

职责：所有具体 UE 资产读写能力。工具簇是 UE Plugin 内部能力，不是普通 Agent 直接选择的外部入口。

当前工具簇源码归属：

| 工具簇 | 当前 Service 目录 | 当前 Adapter 目录 | Review 要求 | Debug 要求 |
|---|---|---|---|---|
| GraphWrite | `Services/GraphWrite` | `TaskRuntime/TaskPlanAdapters/GraphWrite` 或 TaskRuntime 内部 IR lowering | 写入时产出 graph/block/node/link atomic targets | 导出 graph、block、anchor、node/link 摘要 |
| AssetFactory | `Services/AssetFactory` | `TaskRuntime/TaskPlanAdapters/AssetFactory` | 资产创建、复用、删除 visible change | 导出目标路径、资产类型、失败阶段 |
| BlueprintComponent | `Services/BlueprintComponent` | `TaskRuntime/TaskPlanAdapters/BlueprintComponent` | component / component_property targets | 导出组件树和属性路径摘要 |
| BlueprintClassSettings | `Services/BlueprintClassSettings` | `TaskRuntime/TaskPlanAdapters/BlueprintClassSettings` | class_setting / implemented_interface targets | 导出 parent、interface、metadata 摘要 |
| BlueprintSignature | `Services/BlueprintSignature` | `TaskRuntime/TaskPlanAdapters/BlueprintSignature` | function/event/dispatcher signature targets | 导出签名变更摘要 |
| BlueprintVariables | `Services/BlueprintVariables` | `TaskRuntime/TaskPlanAdapters/BlueprintVariables` | member/local variable targets | 导出变量名、类型、默认值摘要 |
| UMGWidget | `Services/UMGWidget` | `TaskRuntime/TaskPlanAdapters/UMGWidget` | widget tree / widget property targets | 导出 widget tree slice 和属性摘要 |
| DataTable | `Services/DataTable` | `TaskRuntime/TaskPlanAdapters/DataTable` | data_table_row targets | 导出 row name、row struct、field 摘要 |
| ObjectProperty | `Services/DataAssetObjectProperty` | `TaskRuntime/TaskPlanAdapters/DataAssetObjectProperty` | object_property targets | 导出 object path、property path、value 摘要 |
| CleanupOwnership | `Services/CleanupOwnership` | `TaskRuntime/TaskPlanAdapters/CleanupOwnership` | ownership cleanup / conversion targets | 导出 block ownership、anchor、rollback target 摘要 |

工具簇底层支撑归属：

```text
Source/BlueprintHelper/Private/GraphSupport
Source/BlueprintHelper/Public/GraphSupport
Source/BlueprintHelper/Private/GraphWrite
Source/BlueprintHelper/Public/GraphWrite
Source/BlueprintHelper/Private/Logic
Source/BlueprintHelper/Public/Logic
Source/BlueprintHelper/Private/NodeHandlers
Source/BlueprintHelper/Public/NodeHandlers
Source/BlueprintHelper/Private/OperationHandlers
Source/BlueprintHelper/Public/OperationHandlers
```

规则：

```text
1. 新工具优先归入已有工具簇，不引入动态注册模型。
2. 每个工具簇必须能提供脱敏 debug summary candidate；只有 failure、blocker、partial、review needs_action 经 DebugEntry 创建 DebugCase，DebugBundle 只在开发者导出时生成。
3. 每个写工具必须接入 Review，产出 WriteReviewEvidence.v1。
4. 每个写工具必须记录 transaction_id 和 rollback_data_ref。
5. ReviewStore 只消费 evidence，不推断缺失 anchor。
```

### 4. Review Audit 审计大类

职责：用户侧审查系统。管理 ArchiveSession、ReviewRecordQuery、Accept、Reject、RejectAll、ConvertOwnerBlock、action history 和状态传播。

当前源码归属：

```text
Source/BlueprintHelper/Private/Services/Review
Source/BlueprintHelper/Public/Services/Review
Source/BlueprintHelper/Private/Structure/Review
Source/BlueprintHelper/Public/Structure/Review
Source/BlueprintHelper/Private/Widgets/Review
Source/BlueprintHelper/Public/Widgets/Review
Source/BlueprintHelper/Private/Tests/Review
```

规则：

```text
1. ReviewRecord identity = archive_session_id + asset_path。
2. ReviewRecordQuery 是用户侧查询模型。
3. Reject 只做机械回滚和 TOCTOU 检查。
4. Review 不内联 DebugBundle payload，只保存稳定引用。
5. 普通 Agent task result 不展开 Review 内部状态。
```

### 5. Debug Diagnostics 排错大类

职责：开发者诊断、RuntimeProfile、Diagnostics、Compile/Save/AssetBrowse、DebugBundle 导出和失败复盘。

当前源码归属：

```text
Source/BlueprintHelper/Private/Services/RuntimeDiagnostics
Source/BlueprintHelper/Public/Services/RuntimeDiagnostics
Source/BlueprintHelper/Private/Structure/RuntimeDiagnostics
Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics
Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics
```

目标归类：

```text
RuntimeDiagnostics = 运行时诊断和资产读写辅助
DebugBundle = 排错 artifact 导出和 DebugCase 证据聚合
```

规则：

```text
1. DebugBundle 是开发者诊断系统，不是普通 Agent 默认返回内容。
2. standard bundle 不包含 token、settings 全文、本地绝对路径和完整 RawJson 正文。
3. Review 只显示 has_debug_info / debug_case_ids / stable ref。
4. Debug failure 不覆盖原始工具失败原因。
```

### 6. Transaction Facts 事实日志大类

职责：记录 UE 写入事实、rollback_data、Review evidence 关联和开发者诊断查询。

当前源码归属：

```text
Source/BlueprintHelper/Private/Transactions
Source/BlueprintHelper/Public/Transactions
```

规则：

```text
1. TransactionJournalQuery 是开发者诊断模型，不是用户侧 Review 主视图。
2. Review evidence 必须能通过 transaction_id 找到 source transaction summary。
3. rollback_data_ref 必须稳定、可解析、可失败上报。
```

### 7. Safety / Shared DTO / Developer UI

职责：公共数据结构、安全校验、编辑器 UI 和测试。

当前源码归属：

```text
Source/BlueprintHelper/Private/Safety
Source/BlueprintHelper/Public/Safety
Source/BlueprintHelper/Private/Structure
Source/BlueprintHelper/Public/Structure
Source/BlueprintHelper/Private/Widgets
Source/BlueprintHelper/Public/Widgets
Source/BlueprintHelper/Private/Tests
```

规则：

```text
1. Shared DTO 只表达合同，不承载跨系统业务流程。
2. Widgets 只消费 Query / Action 服务，不直接浏览底层 transaction 文件。
3. Tests 按 Review、Debug、工具簇和 TaskRuntime 分组。
```

## 建议的长期物理目录

若后续决定移动源码，建议按独立重构分阶段迁移到以下结构：

```text
Source/BlueprintHelper/Private/Entry
Source/BlueprintHelper/Private/Runtime/TaskRuntime
Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite
Source/BlueprintHelper/Private/Systems/ToolClusters/AssetFactory
Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintComponent
Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintClassSettings
Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintSignature
Source/BlueprintHelper/Private/Systems/ToolClusters/BlueprintVariables
Source/BlueprintHelper/Private/Systems/ToolClusters/UMGWidget
Source/BlueprintHelper/Private/Systems/ToolClusters/DataTable
Source/BlueprintHelper/Private/Systems/ToolClusters/ObjectProperty
Source/BlueprintHelper/Private/Systems/ToolClusters/CleanupOwnership
Source/BlueprintHelper/Private/Systems/Review
Source/BlueprintHelper/Private/Systems/Debug
Source/BlueprintHelper/Private/Systems/Transactions
Source/BlueprintHelper/Private/Shared
Source/BlueprintHelper/Private/UI
```

迁移约束：

```text
1. 先建立逻辑分层文档和测试覆盖，再移动文件。
2. 每次只迁移一个大类，避免 include 路径和 Unreal Build 缓存同时失效。
3. Public 头文件路径变更必须保留兼容 shim 或一次性更新所有 include。
4. Review 闭环和 DebugBundle 闭环优先于目录美化。
```
