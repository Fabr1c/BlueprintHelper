# Worker F Diff Summary And Export Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 为 Change Review 提供可读摘要、Logic Diff、Markdown 导出和可退化的 UE Diff 打开能力。

**Architecture:** DiffService 只读 session 和 snapshot，不负责状态流转。Blueprint 逻辑摘要优先复用 `FBlueprintHelperLogicProcessor`。UE Asset Diff 第一版只覆盖 Blueprint before snapshot 可用的场景，不支持时返回 summary fallback。

**Tech Stack:** UE5 C++、LogicProcessor、Json、Markdown、AssetTools/ContentBrowser 可选。

---

## 写入边界

允许新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeDiffService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeDiffService.cpp
Resources/Docs/MCP_ReviewTools.md
```

允许修改：

```text
Source/BlueprintHelper/Public/BlueprintHelper.h
Source/BlueprintHelper/Private/BlueprintHelper.cpp
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
Source/BlueprintHelper/BlueprintHelper.Build.cs
```

不允许修改：

```text
MCPServer/src/tools.ts
Source/BlueprintHelper/Private/SHelperMainWidget.cpp
```

## 服务 API

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperChangeDiffService
{
public:
	FString BuildOperationSummaryMarkdown(const FBlueprintHelperReviewSession& Session) const;
	FString BuildAssetSummaryMarkdown(const FBlueprintHelperReviewSession& Session) const;
	FString BuildReviewReportMarkdown(const FBlueprintHelperReviewSession& Session) const;
	bool ExportReviewReportMarkdown(const FBlueprintHelperReviewSession& Session, const FString& OutputPath, FString& OutError) const;
	bool OpenAssetDiff(const FBlueprintHelperAssetChangeRecord& AssetChange, FString& OutError) const;
};
```

## 任务

### Task F1: 新增 DiffService

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeDiffService.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeDiffService.cpp`

- [ ] 实现 operation summary。
- [ ] 实现 asset summary。
- [ ] 实现 diagnostics summary。
- [ ] Markdown 不输出完整 raw JSON。

验收：

- 一个包含 operation 和 asset 的 session 可生成可读 Markdown。
- 大字段被截断或引用 snapshot 文件路径。

### Task F2: 接入 LogicProcessor 摘要

**Files:**

- Modify: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeDiffService.cpp`

- [ ] 如果 asset record 已有 before/after logic markdown，则生成简短差异。
- [ ] 如果没有 logic markdown，则退化到 counts 和 operation summary。
- [ ] 不把 `logic_json` 标记为可导入格式。

验收：

- Blueprint 资产显示变量、节点、执行线摘要。
- UObject/DataTable 显示属性或行级摘要。

### Task F3: Markdown 导出

**Files:**

- Modify: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeDiffService.cpp`
- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`

- [ ] `review_export_summary` 支持可选 `output_path`。
- [ ] 未传 output_path 时导出到 session 目录。
- [ ] 返回 `markdown` 和 `output_path`。

验收：

- Bridge 命令可导出 Markdown 报告。
- 路径在项目 Saved 目录内时不需要额外确认。

### Task F4: Open UE Diff

**Files:**

- Modify: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeDiffService.cpp`
- Modify: `Source/BlueprintHelper/BlueprintHelper.Build.cs`

- [ ] 优先尝试 before asset snapshot vs current asset。
- [ ] 如果缺少可 Diff 的 before asset，返回 summary fallback。
- [ ] 不支持的资产类型返回 `false` 和明确错误，不崩溃。

验收：

- `review_open_asset_diff` 对不支持资产返回 fallback 信息。
- Blueprint before asset 可用时能打开 UE Diff。

### Task F5: MCP review docs 草案

**Files:**

- Create: `Resources/Docs/MCP_ReviewTools.md`

- [ ] 记录每个 Bridge review 命令。
- [ ] 为 Worker G 提供 MCP 工具描述来源。
- [ ] 明确 approve/reject 需要用户明确授权。

验收命令：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

预期：编译通过。

