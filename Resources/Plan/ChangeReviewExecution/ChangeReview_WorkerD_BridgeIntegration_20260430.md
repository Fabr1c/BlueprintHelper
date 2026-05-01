# Worker D Bridge Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 在 Bridge 层暴露 review 命令，并把首批写命令接入审阅 session。

**Architecture:** Router 保持唯一 Bridge 入口。新增 review 命令处理函数和 `RunReviewedWriteCommand` 辅助流程。首批只接入低风险闭环命令，验证后再扩大覆盖。

**Tech Stack:** UE5 C++、Bridge JSON protocol、ChangeReviewManager、Snapshot/Rollback 服务。

---

## 写入边界

允许修改：

```text
Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
Source/BlueprintHelper/Private/BlueprintHelper.cpp
Source/BlueprintHelper/Public/BlueprintHelper.h
```

允许新增：

```text
Resources/Plan/ChangeReviewExecution/ChangeReview_BridgeContract_20260430.md
```

不允许修改：

```text
MCPServer/src/tools.ts
Source/BlueprintHelper/Private/SHelperMainWidget.cpp
```

## 新增 Bridge 命令

```text
review_begin_session
review_list_sessions
review_get_session
review_get_summary
review_approve_session
review_reject_session
review_open_asset_diff
review_export_summary
```

`review_open_asset_diff` 可先返回 `not_supported`，由 Worker F 实现实际打开。

## 首批接入写命令

必须接入：

```text
add_variable
delete_nodes
set_object_property
```

建议接入：

```text
import_json
update_datatable_row
```

暂不接入：

```text
save_asset
undo
redo
play_in_editor
stop_pie
exec_console_command
close_editor
```

## Review policy

从 payload 读取：

```json
{
  "review_policy": "pending",
  "review_session_id": "optional",
  "review_display_name": "optional"
}
```

MVP 默认策略：

```text
pending for commands explicitly connected by Worker D
bypass for commands not yet connected
```

最终默认策略在 Worker H 验收后切换到：

```text
pending
```

## 任务

### Task D1: Router 构造注入 Review 服务

**Files:**

- Modify: `Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `Source/BlueprintHelper/Private/BlueprintHelper.cpp`

- [ ] Router 构造函数接收 `FBlueprintHelperChangeReviewManager&`。
- [ ] 若 Worker C 已完成，再接收 Snapshot/Rollback 服务。
- [ ] 保持现有服务参数顺序清晰，不使用全局单例绕过注入。

验收：

- 旧命令仍能编译。
- StartupModule 构造顺序明确。

### Task D2: 新增 review 命令处理函数

**Files:**

- Modify: `Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h`
- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`

- [ ] 在 `HandleRequest` 前部注册 review 命令。
- [ ] `review_begin_session` 返回 session id。
- [ ] `review_list_sessions` 支持 `state` 可选过滤。
- [ ] `review_get_session` 返回完整 JSON。
- [ ] `review_get_summary` 返回摘要和 markdown。
- [ ] `review_approve_session` 支持 `save_assets` bool。
- [ ] `review_reject_session` 调用 RollbackService 或先标记 ManualOnly。

验收：

- 不传 `session_id` 的 get/approve/reject 返回 invalid_request。
- pending session 能通过 Bridge 查询。

### Task D3: 写命令包裹辅助函数

**Files:**

- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`

- [ ] 新增 `ParseReviewPolicyOptions`。
- [ ] 新增 `AppendReviewResult(FBlueprintHelperBridgeResponse& Resp, const FBlueprintHelperReviewSession& Session)`。
- [ ] 新增 `RunReviewedWriteCommand` 本地 lambda 或私有方法。
- [ ] 捕获 request payload 和 response result 到 OperationRecord。

验收：

- 响应中的 `result.review` 包含 session_id、state、requires_user_review、assets。
- 旧响应字段仍存在。

### Task D4: 接入首批写命令

**Files:**

- Modify: `Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp`

- [ ] `HandleAddVariable` 接入 review。
- [ ] `HandleDeleteNodes` 接入 review。
- [ ] `HandleSetObjectProperty` 接入 review。
- [ ] 每个接入命令都记录 command name、target asset、request、success/error。
- [ ] 失败命令也记录 Failed session，方便用户追踪。

验收：

- `add_variable` 成功响应含 `result.review`。
- `set_object_property` 成功响应含 old/new value 和 `result.review`。
- 接入命令失败时 session 状态为 Failed 或记录 diagnostics。

### Task D5: Bridge 契约文档

**Files:**

- Create: `Resources/Plan/ChangeReviewExecution/ChangeReview_BridgeContract_20260430.md`

- [ ] 写明所有 review 命令 payload 和 response。
- [ ] 写明 `result.review` 的兼容性规则。
- [ ] 写明 approve/reject 需要用户明确授权。

验收命令：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

预期：编译通过。

