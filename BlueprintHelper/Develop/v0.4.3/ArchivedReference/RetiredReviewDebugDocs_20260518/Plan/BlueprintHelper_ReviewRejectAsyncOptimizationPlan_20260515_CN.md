# BlueprintHelper Review Reject 异步优化计划

日期：2026-05-15

## 目标

让 ReviewPanel 的 Reject 点击后 UI 立即响应，回滚过程可观察、可排队、可失败恢复，避免当前按钮点击后明显阻塞的体验。

## 核心约束

1. UObject、UBlueprint、UEdGraph、FScopedTransaction、Slate 刷新必须在 GameThread。
2. 文件读取、JSON parse、ReviewStore 写盘、DebugBundle 写盘可以异步。
3. 同一资产的 Reject 任务第一版必须串行，避免并发修改同一蓝图。
4. UI 不能在持久化成功前虚假移除 Review 记录。

## 阶段 1：UI 异步状态机

新增每条 Review 的操作状态：

```text
Idle
Queued
Preparing
Mutating
Persisting
Completed
Failed
```

行为：

1. 点击 Reject 后立即把该 Row 标记为 Queued 或 Preparing。
2. 当前 Row 的 Accept/Reject 禁用，显示 Rejecting 或处理中状态。
3. 不立即执行重活，用 ActiveTimer 或 AsyncTask(ENamedThreads::GameThread, ...) 延后一帧启动。
4. 这一帧先让 Slate 重绘，用户能立刻看到状态变化。
5. 失败时保留该 Row，显示失败原因。
6. 成功后再从最终变更列表移除，并执行同资产优先选择策略。

## 阶段 2：后台 Prepare

后台线程只做纯数据工作：

1. 解析 RollbackDataRef。
2. 读取 transaction journal 文件。
3. 解析 rollback JSON。
4. 准备 immutable rollback plan。
5. 预构建 DebugBundle event 文本。
6. 不访问任何 UObject / UBlueprint / UEdGraph。

输出 PreparedRejectJob，回到 GameThread 执行。

## 阶段 3：GameThread Mutation 最小化

真正回滚仍在 GameThread，但缩短单次阻塞：

1. 进入 Mutating 状态。
2. 只在 GameThread 解析资产对象、图表对象、节点对象。
3. Graph 节点删除改为批处理，每帧删除 N 个节点。
4. MarkBlueprintAsStructurallyModified 只在全部节点处理完后调用一次。
5. Graph->NotifyGraphChanged 只在最后调用一次。
6. FScopedTransaction 覆盖整个 mutation，但内部节点操作分批执行。

第一版批大小建议：

```text
每帧 16 或 32 个节点
```

后续根据实测调优。

## 阶段 4：异步 Persist

Mutation 成功后进入 Persisting：

1. ReviewRecord purge/save 放后台串行写入队列。
2. DebugBundle 写入放后台。
3. 写盘完成后回到 GameThread：成功则移除 Row 并刷新 ReviewPanel；失败则 Row 进入 Failed 并显示持久化失败原因。

目标是避免“图已经删了，但最终变更还在”的状态残留。

## 阶段 5：队列策略

第一版使用简单可靠策略：

1. 全局 Reject 队列 FIFO。
2. 同一资产串行。
3. 不允许同一 ChangeId 重复入队。
4. RejectAllAssetChange 展开为同资产串行任务。
5. 如果某个任务失败，后续同资产任务暂停，避免基线被继续污染。

## 建议新增/调整文件

新增：

1. FBlueprintHelperReviewAsyncActionQueue.h/.cpp
2. FBlueprintHelperReviewAsyncRejectJob.h/.cpp
3. FBlueprintHelperReviewRollbackPrepareService.h/.cpp
4. FBlueprintHelperReviewPersistQueue.h/.cpp

调整：

1. SBlueprintHelperReviewPanel.cpp
2. SBlueprintHelperReviewPanel.h
3. BlueprintHelperReviewActionService.cpp
4. BlueprintHelperReviewDebugBundleService.cpp
5. 必要时调整 BlueprintHelperReviewStoreService.cpp

## 风险点

1. FScopedTransaction 跨帧使用需要验证；如果 UE 不稳定，则批量删除降级为单帧 mutation，但保留 UI 延迟刷新和后台 prepare/persist。
2. ReviewStore 后台写盘必须串行，不能多个线程同时写同一个 ReviewRecord。
3. 如果 mutation 成功但 persist 失败，需要保留失败状态并提供重试，而不是假装完成。

## 验收标准

1. 点击 Reject 后 UI 立即变为处理中状态。
2. 大 Graph Reject 不再明显卡住首帧。
3. Graph 内容删除后，最终变更记录同步消失。
4. Reject 失败时 Row 不消失，并显示明确原因。
5. Accept/Reject 后选择仍停留在同资产，直到该资产无剩余 Review。
6. DebugBundle 记录完整状态流：queued、preparing、mutating、persisting、completed/failed。

## 阻塞内容

1. 需要实现时验证 FScopedTransaction 是否能安全跨帧；如果不能，批量删除只能降级为单帧 mutation，但 UI 延迟刷新和后台 prepare/persist 仍然有效。
## 2026-05-15 实现验证结果

### FScopedTransaction 跨帧验证结论
- 结论：不启用跨帧 `FScopedTransaction`。
- 原因：当前 Reject/Rollback 仍依赖 UE 编辑器事务栈和 UObject mutation。`FScopedTransaction` 是 RAII 语义，跨 Slate ActiveTimer 帧持有会把一次 Undo 事务延伸到多轮 UI/event loop，中间如果出现用户操作、刷新或对象失效，事务边界和回滚状态都不稳定。
- 降级策略：不做跨帧批量删除。Graph 节点删除和 UObject mutation 继续保持在下一帧单帧 GameThread 内完成；UI 点击反馈、rollback journal 读取/JSON 解析、DebugBundle 写盘从主点击路径中移出。

### 已完成实现
1. `SBlueprintHelperReviewPanel` 增加 Reject 队列和异步状态机：点击 Reject 后立即刷新 UI 状态，不在按钮回调内直接执行 rollback。
2. rollback journal 文件读取和 JSON 解析移动到 ThreadPool 后台线程，准备完成后回到 GameThread。
3. UObject mutation 保持 GameThread 单帧执行，且显式记录 `single_frame_transaction` 路径，避免跨帧持有 `FScopedTransaction`。
4. `FBlueprintHelperReviewActionService` 支持使用提前准备好的 rollback journal 数据，减少 mutation 阶段的文件读取/解析成本。
5. `DebugBundle` 事件追加改为后台线程执行，并用静态锁串行化同一 bundle 的 read-modify-write，降低 UI 点击阻塞。
6. 编译验证通过：`E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`。

### 距离期望差距
1. `ReviewStore` 写盘暂未异步化。当前刷新路径仍以磁盘 Review record 为数据源；如果只把写盘改成异步，`RefreshFromReviewStoreIfChanged` 可能读到旧记录，导致 Reject 后最终变更残留。要安全异步化，需要先引入权威内存态 ReviewStore 和 write-behind flush。
2. Graph 节点删除没有分帧执行。由于跨帧事务不启用，批量删除按要求降级为单帧 mutation。
3. 本次只完成编译验证，尚未进行编辑器 UI 交互实测；需要在编辑器中验证 Reject 点击阻塞感、DebugBundle 追加完整性和最终变更移除行为。

### 阻塞内容
1. `ReviewStore` 异步写盘需要内存态 store 架构前置，否则会破坏现有刷新一致性。
## 2026-05-15 关闭编辑器线程清理与残留数据清理按钮

### 问题
- 关闭编辑器时出现 `Assertion failed: Work->GetRefCount() == 0`，说明 ThreadPool 里仍存在未清理的 async work 引用。
- 根因范围：ReviewPanel rollback journal prepare 后台任务、DebugBundle 后台写盘任务没有统一 flush/shutdown 入口。

### 已完成实现
1. `SBlueprintHelperReviewPanel` 增加全局 async task 跟踪表，后台 rollback journal prepare 任务会被记录。
2. `SBlueprintHelperReviewPanel::FlushAsyncTasks()` 用于等待当前 ReviewPanel prepare 任务完成。
3. `SBlueprintHelperReviewPanel::ShutdownAsyncTasks()` 用于模块关闭时设置 shutdown 标记并等待任务完成，后台任务在 shutdown 后不再回投 GameThread 回调。
4. `FBlueprintHelperReviewDebugBundleService` 增加后台写盘任务跟踪表。
5. `FBlueprintHelperReviewDebugBundleService::FlushAsyncWrites()` 用于等待 DebugBundle 写盘完成。
6. `FBlueprintHelperReviewDebugBundleService::ShutdownAsyncWrites()` 用于模块关闭时设置 shutdown 标记并等待写盘任务完成。
7. `FBlueprintHelperModule::ShutdownModule()` 开始阶段调用 ReviewPanel 和 DebugBundle shutdown flush，避免模块卸载后仍有 ThreadPool work 存活。
8. BlueprintHelper 主面板顶部新增 `Clean Review Data` 按钮。
9. `Clean Review Data` 点击后先 flush ReviewPanel/DebugBundle async task，再删除以下 Saved 数据：
   - `Saved/BlueprintHelper/Review`
   - `Saved/BlueprintHelper/Transactions`
   - `Saved/BlueprintHelper/Debug/ReviewPanelBundles`
10. 清理后触发 ReviewStore pending-review changed 通知，使 ReviewPanel 有机会刷新。
11. 编译验证通过：`E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`。

### 距离期望差距
1. 尚未在编辑器内实际验证关闭编辑器是否不再触发 `QueuedThreadPoolWrapper` 断言。
2. 尚未在 UI 中点击 `Clean Review Data` 验证目录删除和 ReviewPanel 刷新表现。
3. 清理按钮当前是直接清理 Saved 下 Review/Transactions/ReviewPanelBundles，不做二次确认；这是按“一键清除”实现，但误点会清空当前 pending review 的回滚依据。

### 阻塞内容
1. 需要编辑器运行态验证关闭流程和清理按钮行为。
## 2026-05-15 Clean Review Data 语义更正

### 更正原因
- 上一版 `Clean Review Data` 直接删除 `Saved/BlueprintHelper/Review`、`Saved/BlueprintHelper/Transactions`、`Saved/BlueprintHelper/Debug/ReviewPanelBundles`，会误删待审阅 Review 和 rollback 依据。
- 正确语义应为：清理已经审阅过的 Accept/Reject/Superseded 残留内容，不清理仍待处理的 Review 数据。

### 已完成实现
1. `Clean Review Data` 改为后台工作线程执行，按钮点击后立即返回，清理完成后回到 GameThread 记录状态并通知 ReviewPanel 刷新。
2. 清理前会 flush ReviewPanel rollback prepare task 和 DebugBundle async write task，避免与清理过程竞态。
3. 清理范围改为只处理终态 Review：`Accepted`、`Rejected`、`Superseded`。
4. 以下状态全部保留：`Pending`、`NeedsAction`、`RejectFailed`。
5. 对每个 ReviewRecord：
   - 若只包含终态变更，则删除该 ReviewRecord。
   - 若同时包含终态和非终态变更，则只移除终态 VisibleChange，并保存剩余记录。
6. Transaction 文件不再整目录删除；只删除不被保留 Review 引用的 transaction JSON：
   - `Saved/BlueprintHelper/Transactions/Active/*.json`
   - `Saved/BlueprintHelper/Review/*.json`
7. Archive session 文件只删除不被保留 ReviewRecord 引用的 session JSON：
   - `Saved/BlueprintHelper/Review/Sessions/*.json`
8. `SBlueprintHelperMainWindow` 增加清理任务跟踪和 shutdown flush，关闭编辑器时会等待清理 worker 结束。
9. 编译验证通过：`E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`。

### 距离期望差距
1. 尚未在编辑器 UI 中点击验证 `Clean Review Data` 是否只移除已审阅项且保留待审阅项。
2. 尚未验证清理过程中关闭编辑器是否不再触发 ThreadPool work 断言。

### 阻塞内容
1. 需要编辑器运行态验证清理按钮行为和关闭编辑器行为。
## 2026-05-15 Clean Review Data 体积增长问题修正

### 问题
- 用户验证发现点击 `Clean Review Data` 后 `Saved/BlueprintHelper` 体积反而变大。
- 原因 1：上一版清理前调用 `FBlueprintHelperReviewDebugBundleService::FlushAsyncWrites()`，会把尚未落盘的 DebugBundle 后台日志立即写入磁盘。
- 原因 2：上一版会重写包含待审阅项但没有实际移除终态项的 ReviewRecord，JSON 重序列化后可能比原文件更大。

### 已完成实现
1. `Clean Review Data` 不再主动 flush DebugBundle 写盘，避免清理按钮触发未落盘日志写入导致磁盘增长。
2. ReviewRecord 只有在实际移除了 `Accepted`、`Rejected`、`Superseded` 终态 VisibleChange 后才会保存；没有变化的 ReviewRecord 不再重写。
3. DebugBundle 清理改为选择性删除：
   - 旧 bundle：文件时间早于当前时间 7 天。
   - 已完成 session bundle：bundle 中出现 Review change 状态，且没有 `pending`、`needs_action`、`reject_failed` 状态。
4. 当前 session 或仍含待处理 Review 状态的 DebugBundle 会被保留。
5. 清理结果日志新增统计：
   - `oldDebugBundlesDeleted`
   - `completedDebugBundlesDeleted`
6. 编译验证通过：`E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`。

### 距离期望差距
1. 旧 bundle 阈值当前固定为 7 天，尚未做 UI/配置项。
2. “已完成 session bundle”的判断基于 DebugBundle 内已有的 `selected_change` / `change` 状态字段；如果 bundle 缺少 Review 状态字段，则不会被视为已完成。
3. 尚未在编辑器 UI 中验证清理后 `Saved/BlueprintHelper` 体积是否按预期下降或保持不增长。

### 阻塞内容
1. 需要编辑器运行态验证 Clean Review Data 的实际磁盘变化和 DebugBundle 保留/删除策略。