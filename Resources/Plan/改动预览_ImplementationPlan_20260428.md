# BlueprintHelper 改动审阅功能实现计划（2026-04-28）

## 一、实施目标

在 BlueprintHelper 现有 MCP / Bridge 写能力和用户 Widget 面板基础上，新增一套 Agent 改动审阅系统。

第一版目标：

```text
Agent 修改资产后，用户可以在面板中查看本次改动、打开 Diff、确认保留或回滚。
```

第一版采用“已应用待审阅”模式，不做全量 dry-run。

---

## 二、实施原则

### 2.1 不破坏现有写工具

现有 MCP 写工具继续可用。审阅能力应在 Bridge Router 或写命令调度层统一包裹，而不是要求每个工具完全重写。

### 2.2 默认安全

- 默认进入 pending review。
- 默认不自动保存资产。
- 默认不允许 Agent 代替用户 approve。
- 回滚不可靠时必须提示用户手动处理。

### 2.3 审阅数据与资产数据分离

审阅 session、operation log、summary、snapshot metadata 应存放在插件的 Saved 目录，不写入业务资产。

推荐目录：

```text
Saved/BlueprintHelper/ReviewSessions/
```

### 2.4 优先保证可回滚

第一阶段的核心验收不是视觉 Diff 多完整，而是：

- 能记录 Agent 修改了什么。
- 能知道哪些资产被改了。
- 能在安全条件下回滚。
- 不能安全回滚时不误回滚。

---

## 三、模块划分

### 3.1 新增模块 / 类型

推荐新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewTypes.h
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeReviewManager.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeReviewManager.cpp
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeSnapshot.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeSnapshot.cpp
Source/BlueprintHelper/Public/Services/BlueprintHelperChangeDiffService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperChangeDiffService.cpp
```

如项目当前 Services 目录较少，也可以第一阶段先合并为：

```text
BlueprintHelperChangeReviewManager.h/.cpp
```

待稳定后再拆分。

### 3.2 类型职责

| 类型 | 职责 |
|------|------|
| `FBlueprintHelperChangeReviewManager` | session 生命周期、操作记录、approve/reject、状态持久化 |
| `FBlueprintHelperReviewSession` | 单次 Agent 修改会话数据 |
| `FBlueprintHelperAssetChangeRecord` | 单资产改动摘要 |
| `FBlueprintHelperOperationRecord` | 单条 MCP / Bridge 写操作记录 |
| `FBlueprintHelperChangeSnapshot` | 修改前/后快照引用与恢复信息 |
| `FBlueprintHelperChangeDiffService` | 生成摘要 Diff、打开 UE Diff、导出 Markdown |

---

## 四、数据结构草案

### 4.1 Session 状态

```cpp
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
```

### 4.2 回滚能力

```cpp
enum class EBlueprintHelperRollbackMode : uint8
{
    Transaction,
    SnapshotRestore,
    DeleteCreatedAsset,
    NotSupported,
    ManualOnly
};
```

### 4.3 审阅策略

```cpp
enum class EBlueprintHelperReviewPolicy : uint8
{
    Pending,
    Bypass,
    AutoApprove,
    AutoApproveAndSave
};
```

默认值：

```text
Pending
```

### 4.4 Session 结构

```cpp
struct FBlueprintHelperReviewSession
{
    FString SessionId;
    FString DisplayName;
    FString Initiator;
    FDateTime StartedAt;
    FDateTime LastUpdatedAt;
    EBlueprintHelperReviewSessionState State;
    TArray<FBlueprintHelperOperationRecord> Operations;
    TArray<FBlueprintHelperAssetChangeRecord> Assets;
    TArray<FString> Diagnostics;
    bool bHasMixedUserChanges = false;
    bool bRequiresManualResolution = false;
};
```

### 4.5 Operation Record

```cpp
struct FBlueprintHelperOperationRecord
{
    FString OperationId;
    FString CommandName;
    FString TargetAssetPath;
    FString TargetGraphName;
    FString RequestJson;
    FString ResponseJson;
    bool bSuccess = false;
    FString ErrorMessage;
};
```

### 4.6 Asset Change Record

```cpp
struct FBlueprintHelperAssetChangeRecord
{
    FString AssetPath;
    FString AssetClass;
    FString ChangeKind; // created | modified | deleted | renamed
    FString BeforeSnapshotId;
    FString AfterSnapshotId;
    FString LogicDiffMarkdown;
    FString SummaryMarkdown;
    FString CompileStatus;
    TArray<FString> Warnings;
    TArray<FString> Errors;
    EBlueprintHelperRollbackMode RollbackMode;
};
```

---

## 五、快照策略

### 5.1 Blueprint / WidgetBlueprint

第一阶段建议同时捕获两类快照：

1. 逻辑快照：使用现有 raw JSON / 后续 LogicProcessor 导出。
2. 资产快照：复制修改前资产到临时包。

推荐临时资产路径：

```text
/Game/__BlueprintHelperReviewTemp__/<SessionId>/<AssetName>_Before
```

注意：临时资产应标记为隐藏或放在明确目录，Reject / Cleanup 时删除。

### 5.2 DataAsset / UObject 属性

捕获：

- 属性路径。
- 修改前值。
- 修改后值。
- 对象路径。

若对象可 JSON 序列化，可存储结构化 JSON；否则存储字符串化属性值。

### 5.3 DataTable

捕获：

- 行名。
- 修改前行 JSON。
- 修改后行 JSON。
- 增删改类型。

### 5.4 新建资产

捕获：

- 新资产路径。
- 创建命令。
- 是否存在外部引用。

Reject 时可删除新建资产，但必须先检查：

- 资产是否仍存在。
- 是否被用户另存或重命名。
- 是否已经被其他资产引用。

---

## 六、事务策略

### 6.1 每个审阅会话一个顶层事务

在写操作开始时创建：

```cpp
FScopedTransaction Transaction(NSLOCTEXT("BlueprintHelper", "AgentReviewSession", "BlueprintHelper Agent Change Review"));
```

对所有被修改对象调用：

```cpp
Object->Modify();
```

注意：如果写命令内部已经使用 `FScopedTransaction`，需要避免嵌套事务导致 Undo 栈混乱。推荐由 ReviewManager 提供统一入口：

```cpp
RunWriteCommandWithReview(...)
```

### 6.2 Reject 优先使用事务回滚

Reject 流程：

```text
1. 确认 session 仍为 PendingReview。
2. 检查是否存在 mixed user changes。
3. 若可用 Transaction Undo，则执行 Undo。
4. 否则尝试 Snapshot Restore。
5. 重新编译相关蓝图。
6. 刷新面板和 Content Browser。
```

### 6.3 事务不可用时退化

以下情况不应静默自动回滚：

- 资产已经保存到磁盘。
- 用户在审阅期间手动修改了同一资产。
- 资产被重命名或移动。
- 新建资产已经被其他资产引用。
- 临时 before snapshot 丢失。

这类 session 状态应设置为：

```text
ManualResolutionRequired
```

---

## 七、Diff 策略

### 7.1 Operation Diff

第一阶段必须实现。

展示内容：

- 命令名。
- 目标资产。
- 目标图表。
- 参数摘要。
- 执行是否成功。
- 错误信息。

示例：

```text
[add_variable]
Asset: /Game/BP/BP_Player
Variable: Health
Type: float
Default: 100.0
Result: Success
```

### 7.2 Logic Diff

如果已实现 LogicProcessor，则使用：

```text
before raw json -> logic_md
after raw json -> logic_md
logic_md diff
```

若 LogicProcessor 尚未落地，第一版可先显示 raw summary：

- node count delta。
- variable count delta。
- function graph count delta。
- widget count delta。
- property count delta。

### 7.3 UE Asset Diff

新增服务方法：

```cpp
bool OpenAssetDiff(const FString& BeforeAssetPath, const FString& AfterAssetPath, FString& OutError);
```

实现策略：

1. 如果 before snapshot 临时资产存在，打开 before asset vs current asset 的 Diff。
2. 如果项目启用 Source Control 且资产有 depot revision，可提供 “Diff Against Source Control”。
3. 如果资产类型不支持 UE Diff，则退化为 summary diff。

### 7.4 Raw JSON Diff

Raw JSON Diff 只用于调试，不作为默认用户视图。

原因：

- GUID、坐标、Pin 重建会制造大量噪音。
- 用户通常更关心逻辑变化。

---

## 八、Widget 面板改造计划

### 8.1 面板新增区域

在现有用户面板增加：

```text
Change Review
```

列表字段：

- Session。
- 状态。
- 发起者。
- 涉及资产数量。
- 成功 / 警告 / 错误。
- 最后更新时间。

### 8.2 Session 详情

详情页分区：

1. Summary。
2. Assets。
3. Operations。
4. Diagnostics。
5. Diff Actions。
6. Approval Actions。

### 8.3 操作按钮

推荐按钮：

```text
Open Asset
Open UE Diff
Show Logic Diff
Approve
Approve & Save
Reject / Revert
Export Review Markdown
```

危险按钮要求确认弹窗：

- Reject / Revert。
- Approve & Save。
- Delete temporary snapshots。

### 8.4 状态颜色

建议状态：

| 状态 | 面板表现 |
|------|----------|
| PendingReview | 黄色 / 待处理 |
| Approved | 绿色 |
| Reverted | 灰色 |
| Failed | 红色 |
| MixedChanges | 橙色 |
| ManualResolutionRequired | 红色 + 禁止自动回滚 |

---

## 九、Bridge 集成计划

### 9.1 Router 写命令包裹

在 Bridge Router 统一判断命令是否为写操作。

写操作包括：

- 创建 / 删除 / 修改蓝图变量。
- 创建 / 删除 / 修改函数图、宏图、事件分发器。
- 添加 / 删除 / 修改节点。
- UMG Widget 树修改。
- UObject / DataAsset 属性写入。
- DataTable 行增删改。
- 创建资产。
- 保存资产。

读操作不进入 ReviewSession。

### 9.2 请求字段

所有写命令支持可选字段：

```json
{
  "review_policy": "pending",
  "review_session_id": "optional",
  "review_display_name": "optional"
}
```

若未提供：

```text
review_policy = pending
```

### 9.3 响应字段

写命令响应增加：

```json
{
  "success": true,
  "review": {
    "enabled": true,
    "session_id": "BPHR_20260428_001",
    "state": "pending_review",
    "requires_user_review": true,
    "assets": ["/Game/BP/BP_Player"]
  }
}
```

兼容要求：

- 原有 `success`、`message`、`error` 字段保持不变。
- 新增 `review` 字段不应破坏旧 Agent。

---

## 十、MCP Server 集成计划

### 10.1 新增工具

推荐新增：

```text
blueprint_review_begin_session
blueprint_review_list_pending
blueprint_review_get_session
blueprint_review_get_summary
blueprint_review_open_diff
blueprint_review_approve
blueprint_review_reject
blueprint_review_export_markdown
```

### 10.2 工具边界

| 工具 | 类型 | 是否需要用户明确授权 |
|------|------|----------------------|
| list_pending | 读 | 否 |
| get_session | 读 | 否 |
| get_summary | 读 | 否 |
| open_diff | UI 操作 | 建议需要明确目标 session |
| approve | 写 | 是 |
| reject | 写 | 是 |
| export_markdown | 读 / 文件输出 | 视输出路径而定 |

### 10.3 Agent 行为建议

Agent 收到写操作结果后，应提示：

```text
修改已进入待审阅状态，session_id=...。请在 BlueprintHelper 面板中审阅，或让我打开 Diff。
```

Agent 不应默认继续调用 approve。

---

## 十一、实现阶段

### 阶段 0：确认现有写命令清单

目标：列出所有会修改资产或编辑器状态的 Bridge 命令。

输出：

```text
Resources/Plan/Module_BlueprintHelper_ChangeReview_WriteCommandInventory_20260428.md
```

验收：

- 每个命令标记为 read / write / ui / build / editor-process。
- 写命令标记目标资产来源字段。
- 无法识别目标资产的命令标记为 high risk。

### 阶段 1：ReviewManager 数据模型

任务：

- 新增类型定义。
- 创建 session。
- 记录 operation。
- 记录 asset change。
- 保存 / 读取 session JSON。

验收：

- 能通过单元或编辑器命令创建 session。
- session 可持久化到 Saved。
- 重启编辑器后能读取 pending session 摘要。

### 阶段 2：Bridge 写命令包裹

任务：

- 在 Router 识别写命令。
- 写前调用 `BeginCaptureAsset`。
- 写后调用 `EndCaptureAsset`。
- 响应增加 review 字段。

验收：

- 添加变量、添加节点、修改 UObject 属性至少三类命令进入 pending review。
- 旧调用方不传 review 字段也能工作。

### 阶段 3：快照与回滚

任务：

- Blueprint / WidgetBlueprint before snapshot。
- DataTable 行 before snapshot。
- UObject 属性 before value。
- Transaction Undo。
- Snapshot Restore fallback。

验收：

- 添加蓝图变量后 Reject 可恢复。
- 修改 DataTable 行后 Reject 可恢复。
- 修改 UMG Widget 属性后 Reject 可恢复。
- 已保存或 mixed_changes 状态不静默回滚。

### 阶段 4：Widget 面板

任务：

- 增加 Change Review tab。
- 列出 pending sessions。
- 展示 session 详情。
- 提供 Approve / Reject / Open Diff。

验收：

- 用户可从面板审阅并处理 session。
- 操作后列表状态实时刷新。

### 阶段 5：Diff 服务

任务：

- Operation summary。
- Asset summary。
- UE Diff 打开入口。
- Logic Diff 接入。
- Markdown 导出。

验收：

- 至少 Blueprint 资产可打开 before vs after Diff。
- 不支持 UE Diff 的资产显示可读摘要。
- 可导出审阅报告 Markdown。

### 阶段 6：MCP Server 工具

任务：

- 新增 review 工具。
- 写工具响应透出 review 信息。
- 文档更新。

验收：

- Agent 可列出 pending review。
- Agent 可读取 summary。
- Agent 可请求打开 diff。
- approve / reject 必须有明确用户指令。

### 阶段 7：测试与回归

任务：

- 创建 fixture。
- 覆盖 Blueprint / UMG / DataTable / UObject。
- 覆盖成功、失败、回滚、mixed changes。

验收：

- 回滚后资产摘要与 before snapshot 匹配。
- 编译失败 session 能正确标记。
- 大蓝图不会阻塞 UI。

---

## 十二、测试用例

### CASE-001：添加蓝图变量后审阅

步骤：

1. Agent 调用添加变量工具。
2. 面板出现 pending session。
3. 查看 Operation Diff。
4. Reject。

预期：

- 变量消失。
- 资产恢复 dirty 状态前后符合预期。
- session 状态为 Reverted。

### CASE-002：添加节点并连线后打开 Diff

步骤：

1. Agent 在 EventGraph 添加 PrintString 并连线。
2. 面板打开 UE Diff。

预期：

- UE Diff 能显示图表变化。
- Summary 显示新增节点和新增 link。

### CASE-003：编译失败

步骤：

1. Agent 添加不合法连线或缺失输入。
2. 自动编译或手动编译。

预期：

- session 标记 Failed 或 PendingReviewWithErrors。
- 面板显示错误摘要。
- Reject 可恢复。

### CASE-004：用户混入编辑

步骤：

1. Agent 修改资产进入 pending。
2. 用户手动修改同一资产。
3. 点击 Reject。

预期：

- 面板提示 mixed_changes。
- 禁止静默事务回滚。
- 允许用户选择打开 Diff 或手动处理。

### CASE-005：新建资产

步骤：

1. Agent 创建 Blueprint。
2. 面板 Reject。

预期：

- 若无外部引用，删除新建资产。
- 若存在引用，要求手动处理。

---

## 十三、文档与规则更新

需要新增或更新：

```text
Resources/Plan/Module_BlueprintHelper_ChangeReview_FeasibilityRiskBenefit_20260428.md
Resources/Plan/Module_BlueprintHelper_ChangeReview_ImplementationPlan_20260428.md
Resources/Docs/MCP_ReviewTools.md
Resources/Docs/Widget_ChangeReview_UserGuide.md
Resources/Rules/AgentReviewPolicy.md
```

其中 `AgentReviewPolicy.md` 应明确：

- Agent 写操作默认 pending review。
- Agent 不应自动 approve。
- 写操作必须显式目标资产路径和图表。
- 高风险操作必须提示用户审阅。

---

## 十四、推荐最小可行版本

MVP 只做以下内容即可形成闭环：

1. Bridge 写命令增加 review session。
2. 添加变量 / 添加节点 / 修改属性三类操作进入审阅。
3. Widget 面板显示 pending session。
4. Operation summary。
5. Reject 通过 Transaction Undo 回滚。
6. Approve 后 session 关闭。
7. 不自动保存。

MVP 完成后再加入 UE Diff、Logic Diff、快照恢复和 MCP review 工具。

---

## 十五、推荐开发顺序

```text
1. ChangeReviewTypes
2. ChangeReviewManager
3. Router 写命令包裹
4. OperationRecord
5. Transaction 回滚
6. Widget pending session 列表
7. Approve / Reject
8. Blueprint before snapshot
9. Open UE Diff
10. Logic Diff / Markdown report
11. MCP review tools
12. 高风险边界与 mixed changes
```

---

## 十六、最终交付标准

功能交付后，应满足：

- Agent 任何蓝图写操作都能被归入审阅会话。
- 用户能从面板明确看到 Agent 修改了什么。
- 用户能在未保存前保留或回滚。
- 编译失败不会被隐藏。
- 不支持自动回滚的场景会明确进入人工处理状态。
- 现有 MCP 工具保持兼容。
- 审阅能力可以逐步覆盖更多资产类型。
