# Worker B Review Manager And Persistence Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 新增 ReviewManager、共享数据结构和 session JSON 持久化能力。

**Architecture:** ReviewManager 是 Change Review 的核心状态服务，不直接执行写命令。它负责创建 session、记录 operation/asset、读写 Saved JSON、审批状态流转，并提供 Bridge/Widget/MCP 可复用的查询接口。

**Tech Stack:** UE5 C++、Json、Saved directory persistence。

---

## 写入边界

允许新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewTypes.h
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewManager.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewManager.cpp
```

允许修改：

```text
Source/BlueprintHelper/Public/BlueprintHelper.h
Source/BlueprintHelper/Private/BlueprintHelper.cpp
Source/BlueprintHelper/BlueprintHelper.Build.cs
```

不允许修改：

```text
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
MCPServer/src/tools.ts
Source/BlueprintHelper/Private/SHelperMainWidget.cpp
```

## 公开类型

必须提供：

```cpp
enum class EBlueprintHelperReviewPolicy : uint8;
enum class EBlueprintHelperReviewSessionState : uint8;
enum class EBlueprintHelperRollbackMode : uint8;

struct FBlueprintHelperReviewPolicyOptions;
struct FBlueprintHelperOperationRecord;
struct FBlueprintHelperAssetChangeRecord;
struct FBlueprintHelperReviewSession;
struct FBlueprintHelperReviewSessionSummary;
```

`FBlueprintHelperReviewSession` 必须包含：

```text
SessionId
DisplayName
Initiator
StartedAt
LastUpdatedAt
State
Operations
Assets
Diagnostics
bHasMixedUserChanges
bRequiresManualResolution
```

## Manager API

必须提供：

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperChangeReviewManager
{
public:
	FBlueprintHelperReviewSession BeginSession(const FBlueprintHelperReviewPolicyOptions& Options);
	bool AddOperation(const FString& SessionId, const FBlueprintHelperOperationRecord& Operation, FString& OutError);
	bool AddAssetChange(const FString& SessionId, const FBlueprintHelperAssetChangeRecord& AssetChange, FString& OutError);
	bool MarkPendingReview(const FString& SessionId, FString& OutError);
	bool ApproveSession(const FString& SessionId, bool bSaveAssets, FString& OutError);
	bool MarkRejected(const FString& SessionId, FString& OutError);
	bool MarkReverted(const FString& SessionId, FString& OutError);
	bool MarkManualResolutionRequired(const FString& SessionId, const FString& Reason, FString& OutError);
	bool GetSession(const FString& SessionId, FBlueprintHelperReviewSession& OutSession) const;
	TArray<FBlueprintHelperReviewSessionSummary> ListSessions() const;
	TArray<FBlueprintHelperReviewSessionSummary> ListPendingSessions() const;
	bool SaveSession(const FString& SessionId, FString& OutError) const;
	bool LoadAllSessions(FString& OutError);
};
```

## 持久化格式

保存路径：

```text
Saved/BlueprintHelper/ReviewSessions/<SessionId>.json
```

JSON 顶层字段：

```json
{
  "schema": "BlueprintHelper.ChangeReviewSession",
  "version": 1,
  "session_id": "BPHR_20260430_0001",
  "state": "pending_review",
  "operations": [],
  "assets": [],
  "diagnostics": []
}
```

## 任务

### Task B1: 新增类型定义

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewTypes.h`

- [ ] 定义 enum 和 struct。
- [ ] 提供 enum 到字符串、字符串到 enum 的函数。
- [ ] 提供 `ToJson` 和 `FromJson` 辅助函数，避免各调用方手写字段。

验收：

- 类型头不依赖 Bridge、Slate、MCP。
- 字符串值使用 snake_case：`pending_review`、`manual_resolution_required`。

### Task B2: 实现 Manager 内存状态

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewManager.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewManager.cpp`

- [ ] SessionId 使用 `BPHR_yyyyMMdd_HHmmss_<counter>`。
- [ ] `BeginSession` 创建 Open session。
- [ ] `MarkPendingReview` 只允许 Open/Failed 转 PendingReview。
- [ ] `ApproveSession` 只允许 PendingReview/MixedChanges/ManualResolutionRequired 转 Approved。
- [ ] `MarkRejected` 不执行回滚，只记录用户意图。

验收：

- Manager 能在无编辑器资产参与时创建、更新、查询 session。

### Task B3: 实现 JSON 持久化

**Files:**

- Modify: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewManager.cpp`

- [ ] 使用 `FPaths::ProjectSavedDir()` 拼出 `Saved/BlueprintHelper/ReviewSessions/`。
- [ ] `SaveSession` 写 UTF-8 JSON。
- [ ] `LoadAllSessions` 忽略损坏文件并记录诊断，不导致插件启动失败。

验收：

- 启动时能读取历史 pending session。
- 损坏 JSON 只生成 warning，不崩溃。

### Task B4: 注入模块生命周期

**Files:**

- Modify: `Source/BlueprintHelper/Public/BlueprintHelper.h`
- Modify: `Source/BlueprintHelper/Private/BlueprintHelper.cpp`

- [ ] 增加 `TUniquePtr<FBlueprintHelperChangeReviewManager> ChangeReviewManager`。
- [ ] StartupModule 中构造并调用 `LoadAllSessions`。
- [ ] ShutdownModule 中 Reset。
- [ ] 提供 `GetChangeReviewManager()` 只读或可变访问器。

验收命令：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

预期：编译通过。

