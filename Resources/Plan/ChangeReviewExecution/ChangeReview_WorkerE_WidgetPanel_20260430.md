# Worker E Widget Panel Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 在 BlueprintHelper 主面板中增加 Change Review 区域，用户能查看 pending session 并执行 approve/reject/open diff。

**Architecture:** 不重写整个 `SHelperMainWidget`。先增加一个可折叠或分栏的 Change Review 区域，复用 ReviewManager 读取列表和详情。所有危险动作必须弹确认。

**Tech Stack:** UE5 Slate、SListView、SButton、SBorder、STextBlock、SMultiLineEditableTextBox。

---

## 写入边界

允许修改：

```text
Source/BlueprintHelper/Public/SHelperMainWidget.h
Source/BlueprintHelper/Private/SHelperMainWidget.cpp
Source/BlueprintHelper/Public/BlueprintHelper.h
Source/BlueprintHelper/Private/BlueprintHelper.cpp
```

允许新增：

```text
Resources/Docs/Widget_ChangeReview_UserGuide.md
```

不允许修改：

```text
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
MCPServer/src/tools.ts
```

## UI 范围

第一版显示：

- Pending sessions 列表。
- Session 状态、发起者、资产数量、错误/警告数量、更新时间。
- 详情区域：Summary、Assets、Operations、Diagnostics。
- 操作按钮：Refresh、Approve、Approve & Save、Reject、Open Diff、Export Markdown。

第一版不做：

- 复杂过滤器。
- 全屏 diff viewer。
- 历史归档分页。

## 任务

### Task E1: Widget 依赖注入

**Files:**

- Modify: `Source/BlueprintHelper/Public/SHelperMainWidget.h`
- Modify: `Source/BlueprintHelper/Private/BlueprintHelper.cpp`

- [ ] `SHelperMainWidget::FArguments` 增加 ReviewManager 指针。
- [ ] `OnSpawnPluginTab` 构造 Widget 时传入 ReviewManager。
- [ ] Widget 保存裸指针但不拥有生命周期。

验收：

- 现有按钮和文本区行为不变。

### Task E2: 增加数据结构和刷新逻辑

**Files:**

- Modify: `Source/BlueprintHelper/Public/SHelperMainWidget.h`
- Modify: `Source/BlueprintHelper/Private/SHelperMainWidget.cpp`

- [ ] 增加 `FReviewSessionListItem`。
- [ ] 增加 `TArray<TSharedPtr<FReviewSessionListItem>> ReviewSessionSource`。
- [ ] 增加 `RefreshReviewSessions()`。
- [ ] 增加 `GenerateReviewSessionRow()`。
- [ ] 选择 session 时加载详情文本。

验收：

- 空列表显示“暂无待审阅改动”。
- 有 pending session 时显示行。

### Task E3: 添加 Change Review 区域

**Files:**

- Modify: `Source/BlueprintHelper/Private/SHelperMainWidget.cpp`

- [ ] 在现有主区域中增加 Change Review 分区。
- [ ] 避免把卡片嵌套在卡片里，使用简单 `SBorder` 或 `SVerticalBox` 分区。
- [ ] 列表宽度稳定，详情区域可滚动。

验收：

- 面板仍能容纳原 JSON 工具区。
- 文本不重叠。

### Task E4: Approve/Reject 操作

**Files:**

- Modify: `Source/BlueprintHelper/Public/SHelperMainWidget.h`
- Modify: `Source/BlueprintHelper/Private/SHelperMainWidget.cpp`

- [ ] `OnApproveReviewClicked` 调用 ReviewManager `ApproveSession(false)`。
- [ ] `OnApproveAndSaveReviewClicked` 先确认，再调用 `ApproveSession(true)`。
- [ ] `OnRejectReviewClicked` 必须确认，再调用 Rollback 或 Bridge review reject。
- [ ] 操作后刷新列表和详情。

验收：

- Reject/Approve 后列表状态改变。
- ManualResolutionRequired 显示清楚，不执行静默回滚。

### Task E5: 用户指南

**Files:**

- Create: `Resources/Docs/Widget_ChangeReview_UserGuide.md`

- [ ] 说明 PendingReview、Approved、Reverted、MixedChanges、ManualResolutionRequired。
- [ ] 说明 Approve 与 Approve & Save 的区别。
- [ ] 说明 Reject 不安全时为什么需要手动处理。

验收命令：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

预期：编译通过。

