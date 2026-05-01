# Worker H Validation And Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 对 Change Review MVP 做集成验证，确认旧工具兼容、审阅闭环可用、回滚不误伤。

**Architecture:** 以人工可复现的 Editor 验收为主，辅以 MCPServer build 和 C++ build。测试覆盖首批接入命令，再扩展 UMG/DataTable。

**Tech Stack:** UE5 Editor、UnrealBuildTool、MCPServer TypeScript build、manual fixture assets。

---

## 写入边界

允许新增：

```text
Resources/Plan/ChangeReviewExecution/ChangeReview_ValidationReport_20260430.md
Resources/TestFixtures/ChangeReview/README.md
```

允许修改：

```text
Resources/Plan/ChangeReviewExecution/ChangeReview_Execution_Index_20260430.md
```

不允许修改：

```text
Source/BlueprintHelper/**
MCPServer/src/**
```

除非验证发现必须修复阻塞问题。修复阻塞问题前先写 CR 文档。

## 验收用例

### CASE-001: Blueprint 添加变量后审阅

步骤：

1. 调用 `blueprint_add_variable`，目标资产为测试 Blueprint。
2. 检查响应 `result.review.session_id`。
3. 打开 Widget 面板，确认 pending session 可见。
4. 查看 operation summary。
5. 点击 Reject。

预期：

- 变量被移除或恢复到 before 状态。
- session 状态为 `reverted`。
- 旧响应字段仍包含 `added_variable`。

### CASE-002: UObject 属性修改后审阅

步骤：

1. 调用 `blueprint_set_object_property` 修改测试 DataAsset 或 UObject 属性。
2. 查看 session details。
3. 点击 Reject。

预期：

- 属性恢复 old value。
- session 记录 old/new value。

### CASE-003: 用户混入编辑

步骤：

1. Agent 写入进入 pending。
2. 用户手动修改同一资产。
3. 点击 Reject。

预期：

- session 进入 `manual_resolution_required` 或 `mixed_changes`。
- 不静默覆盖用户手工修改。

### CASE-004: Approve 与 Approve & Save

步骤：

1. Agent 写入进入 pending。
2. 点击 Approve。
3. 再执行另一条写入，点击 Approve & Save。

预期：

- Approve 只关闭 session，不保存资产。
- Approve & Save 保存涉及资产。
- 保存失败时 session 不应错误标记为 approved。

### CASE-005: 旧工具兼容

步骤：

1. 调用旧 `blueprint_get_logic`。
2. 调用旧 `blueprint_export_to_json`。
3. 调用未接入 review 的读工具。

预期：

- 返回结构和文档说明一致。
- 无多余错误。

## 任务

### Task H1: 运行构建

**Files:**

- Create: `Resources/Plan/ChangeReviewExecution/ChangeReview_ValidationReport_20260430.md`

- [ ] 运行 UE 编译。
- [ ] 运行 MCPServer build。
- [ ] 记录命令、时间、结果、失败摘要。

UE 编译：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

MCP build：

```powershell
npm run build
```

执行目录：

```text
G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
```

### Task H2: 执行人工验收

**Files:**

- Modify: `Resources/Plan/ChangeReviewExecution/ChangeReview_ValidationReport_20260430.md`

- [ ] 记录 CASE-001 到 CASE-005。
- [ ] 每个 case 写明 pass/fail、session id、涉及资产、残留风险。
- [ ] 如果阻塞，新增 CR 文档，不直接扩大范围。

### Task H3: 默认 pending 切换判断

**Files:**

- Modify: `Resources/Plan/ChangeReviewExecution/ChangeReview_ValidationReport_20260430.md`
- Modify: `Resources/Plan/ChangeReviewExecution/ChangeReview_Execution_Index_20260430.md`

- [ ] 如果 CASE-001 到 CASE-005 通过，建议把首批接入写命令默认 `pending`。
- [ ] 如果回滚仍不稳定，保持 opt-in pending。
- [ ] 明确下一批接入命令列表。

### Task H4: 完成报告

**Files:**

- Modify: `Resources/Plan/ChangeReviewExecution/ChangeReview_ValidationReport_20260430.md`

- [ ] 写明是否达到 MVP。
- [ ] 写明不可自动回滚的已知边界。
- [ ] 写明下一阶段：UE Diff、DataTable、UMG、新建资产、MCP report。

验收：

- 报告可直接支撑是否继续扩大默认 pending 覆盖。

