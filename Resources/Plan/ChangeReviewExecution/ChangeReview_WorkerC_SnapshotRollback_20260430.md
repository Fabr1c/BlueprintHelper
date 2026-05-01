# Worker C Snapshot And Rollback Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 为审阅 session 提供修改前/后快照、回滚分级和 mixed changes 检测基础。

**Architecture:** Snapshot 服务不负责命令分发。它接收 asset path 和变更类型，按资产类型捕获 raw JSON、logic summary、属性值或 DataTable row JSON。Rollback 服务根据 `RollbackMode` 执行安全回滚，不能证明安全时返回 ManualOnly。

**Tech Stack:** UE5 C++、ExportService、LogicProcessor、PropertyReflectionService、DataTableService、AssetRegistry。

---

## 写入边界

允许新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeSnapshotService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeSnapshotService.cpp
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeRollbackService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeRollbackService.cpp
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

## Snapshot 策略

第一版必须支持：

- Blueprint raw JSON before/after。
- Blueprint logic Markdown before/after。
- UObject 属性 old/new value。
- DataTable row before/after fields。

第一版可延后：

- 临时资产复制。
- 通用 UE Asset Diff 前置资产。
- 新建资产引用扫描的完整自动处理。

## API

```cpp
struct FBlueprintHelperSnapshotRequest
{
	FString SessionId;
	FString AssetPath;
	FString TargetGraphName;
	FString ChangeKind;
	TArray<FString> PropertyNames;
	TArray<FString> DataTableRows;
};

struct FBlueprintHelperSnapshotResult
{
	bool bSuccess = false;
	FString SnapshotId;
	FString SnapshotPath;
	FString SummaryMarkdown;
	FString ErrorMessage;
	EBlueprintHelperRollbackMode SuggestedRollbackMode = EBlueprintHelperRollbackMode::ManualOnly;
};
```

## 任务

### Task C1: 新增 Snapshot 服务

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeSnapshotService.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeSnapshotService.cpp`

- [ ] 构造函数接收 `FBlueprintHelperExportService`、`FBlueprintHelperPropertyReflectionService`、`FBlueprintHelperDataTableService`。
- [ ] `CaptureBefore` 写入 `Saved/BlueprintHelper/ReviewSessions/<SessionId>/Snapshots/<SnapshotId>.json`。
- [ ] `CaptureAfter` 使用相同格式写 after snapshot。
- [ ] Blueprint snapshot 复用 `ExportService.Export` 和 `FBlueprintHelperLogicProcessor::ProcessRawJson`。

验收：

- 对 Blueprint 资产能保存 raw JSON 和 logic Markdown。
- 对 UObject 属性能保存属性名和值。
- 对 DataTable 行能保存 fields。

### Task C2: 新增 Rollback 服务

**Files:**

- Create: `Source/BlueprintHelper/Public/Services/BlueprintHelperChangeRollbackService.h`
- Create: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeRollbackService.cpp`

- [ ] `RejectSession` 遍历 asset records。
- [ ] `Transaction` 模式调用已有 Undo 入口前必须检查 session 没有 mixed changes。
- [ ] `ValueSnapshot` 模式对 UObject 属性调用属性反射恢复值。
- [ ] DataTable row 按 before snapshot 恢复 add/update/delete。
- [ ] 无法证明安全时返回 `ManualResolutionRequired`。

验收：

- 添加变量后，如果记录为 Transaction 且无 mixed changes，Reject 可回滚。
- 修改 UObject 属性后，Reject 可恢复 old value。
- DataTable row update 可恢复 before fields。

### Task C3: Mixed changes 检测

**Files:**

- Modify: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeSnapshotService.cpp`
- Modify: `Source/BlueprintHelper/Private/Services/BlueprintHelperChangeRollbackService.cpp`

- [ ] Snapshot 记录 capture 时的 package dirty 状态和 asset object path。
- [ ] Reject 前重新读取 after snapshot 或当前属性值。
- [ ] 当前状态既不等于 before 也不等于 recorded after 时，标记 mixed changes。

验收：

- 用户在 pending 期间改同一属性，Reject 不静默覆盖。
- 状态进入 `ManualResolutionRequired` 并写入诊断。

### Task C4: 模块注入

**Files:**

- Modify: `Source/BlueprintHelper/Public/BlueprintHelper.h`
- Modify: `Source/BlueprintHelper/Private/BlueprintHelper.cpp`

- [ ] StartupModule 构造 SnapshotService 和 RollbackService。
- [ ] 构造顺序在 Export/Property/DataTable/EditorCommand 之后。
- [ ] ShutdownModule 逆序销毁。

验收命令：

```powershell
& 'G:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

预期：编译通过。

