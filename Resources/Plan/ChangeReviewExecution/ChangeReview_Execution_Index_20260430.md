# BlueprintHelper Change Review Execution Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 在现有 BlueprintHelper Bridge/MCP/Widget 基础上实现 Applied Pending Review 审阅闭环。

**Architecture:** 新增 ChangeReview 服务族，Bridge Router 通过命令分类器包裹写操作，ReviewManager 持久化 session 并提供 approve/reject/open diff/export summary 命令。Widget 面板展示 pending sessions，MCPServer 暴露只读审阅工具和需要用户授权的 approve/reject 工具。

**Tech Stack:** UE5 Editor plugin C++、Slate、Bridge TCP JSON、TypeScript MCP Server、Saved JSON 持久化、BlueprintHelper LogicProcessor。

---

## 总体结论

可执行。收益高于继续单纯增加更多写工具。

当前仓库已经具备大部分写能力和逻辑摘要能力，计划不需要重写 MCP/Bridge 架构。最大工程风险是回滚边界，因此执行线必须先建立命令清单、审阅状态和回滚分级，再逐步接入写命令。

## 并行分发

| 执行线 | 文档 | 批次 | 依赖 | 写入边界 |
|---|---|---:|---|---|
| A | `ChangeReview_WorkerA_CommandInventoryContract_20260430.md` | 1 | 无 | 文档、命令分类辅助类型 |
| B | `ChangeReview_WorkerB_ReviewManagerPersistence_20260430.md` | 1 | 无 | Review types、manager、module 注入 |
| C | `ChangeReview_WorkerC_SnapshotRollback_20260430.md` | 2 | B 的 public types | Snapshot、rollback、dirty/mixed checks |
| D | `ChangeReview_WorkerD_BridgeIntegration_20260430.md` | 2 | A、B | Router review 命令、写命令包裹 |
| E | `ChangeReview_WorkerE_WidgetPanel_20260430.md` | 2 | B 的 manager API | Slate 面板审阅列表和详情 |
| F | `ChangeReview_WorkerF_DiffSummaryExport_20260430.md` | 3 | B、C，复用 LogicProcessor | Diff service、Markdown export、Open UE Diff |
| G | `ChangeReview_WorkerG_MCPToolsDocs_20260430.md` | 3 | D 的 Bridge 命令契约 | MCP review tools、AGENT/Docs/Rules |
| H | `ChangeReview_WorkerH_ValidationIntegration_20260430.md` | 4 | B、C、D、E | 验证用例、build、MCP build、人工验收记录 |

推荐顺序：

```text
第一批：A + B
第二批：C + D + E
第三批：F + G
第四批：H
```

## 第一版范围

MVP 必须完成：

- Review session 创建、读取、列出、approve、reject。
- session JSON 持久化到 `Saved/BlueprintHelper/ReviewSessions/`。
- 至少三类写操作进入审阅：Blueprint 变量、图表节点或 import_json、UObject 属性。
- 响应追加 `review` 字段，不破坏旧 `success/result/error`。
- Widget 面板显示 pending session，支持查看 operations/assets/diagnostics。
- Reject 对已覆盖的安全场景可回滚，不安全场景进入 `ManualResolutionRequired`。
- 逻辑摘要或 operation summary 可读。

MVP 不做：

- 全量 dry-run 预览。
- 自动处理多用户或多 Agent 冲突。
- 所有资产类型的视觉 Diff。
- 审阅通过后自动提交版本控制。

## 共享契约

所有 worker 必须使用相同命名：

```cpp
enum class EBlueprintHelperReviewPolicy : uint8
{
	Pending,
	Bypass,
	AutoApprove,
	AutoApproveAndSave
};

enum class EBlueprintHelperReviewSessionState : uint8
{
	Open,
	PendingReview,
	Approved,
	Rejected,
	Reverted,
	Failed,
	MixedChanges,
	ManualResolutionRequired
};

enum class EBlueprintHelperRollbackMode : uint8
{
	Transaction,
	ValueSnapshot,
	AssetSnapshot,
	DeleteCreatedAsset,
	NotSupported,
	ManualOnly
};
```

Bridge 响应追加字段：

```json
{
  "review": {
    "enabled": true,
    "session_id": "BPHR_20260430_0001",
    "state": "pending_review",
    "requires_user_review": true,
    "assets": ["/Game/BP/BP_Player.BP_Player"]
  }
}
```

该字段位于 `result.review`。旧字段不变。

## 共享验证命令

UE 编译：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

MCP Server 编译：

```powershell
npm run build
```

执行目录：

```text
G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
```

## 变更控制

每个 worker 只能修改自己文档列出的文件。如果发现必须改动其他文件，先新增：

```text
Resources/Plan/ChangeReviewExecution/ChangeReview_CR_<YYYYMMDD>_<short_name>.md
```

未被用户接受前，不修改越界文件。

## 完成标准

- C++ 编译通过。
- MCP Server `npm run build` 通过。
- 旧 MCP 工具调用仍返回兼容 JSON。
- `blueprint_add_variable` 等接入命令能返回 `result.review`。
- 面板能显示 pending session 并执行 approve/reject。
- Reject 在安全场景能恢复，在不安全场景不会静默回滚。
- 文档包含用户指南、Agent 审阅策略和 MCP review 工具说明。

