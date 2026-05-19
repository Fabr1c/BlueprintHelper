# BlueprintHelper UI Visual-Presenter-Data 架构调整记录

日期：2026-05-16

## 目标规则

- UI 按 `Visual -> Presenter -> Data` 组织。
- Visual 只负责 Slate 控件、用户输入事件、通知展示和轻量页面状态。
- Presenter 接收 Visual 事件，读取 Data 快照，向 Visual 发出展示事件。
- Presenter 不直接改 Data 对象。需要写入时，Presenter 只能调度 command/service。
- Data 写入完成后由 DataStore/service 发出 DataChanged，Presenter 再重新读取 Data 并驱动 Visual 刷新。
- Presenter 和 Visual 之间用事件互相驱动，避免 Visual 直接依赖业务服务细节。

## 写路径约定

清理按钮这类会改变数据的操作按下面路径处理：

```text
Visual Click Event
  -> Presenter VisualEvent
  -> Command/Service
  -> DataStore mutation
  -> DataChanged event
  -> Presenter reads Data
  -> PresenterEvent
  -> Visual refresh/notification
```

这意味着 Presenter 不是 Data writer；它只是把用户意图转换为 command/service 调用，并通过 DataChanged 保持读模型一致。

## 本轮切片

本轮先调整 MainWindow 的 `Clean Review Data` 链路：

- `SBlueprintHelperMainWindow` 从直接调用 `FBlueprintHelperReviewedDataCleanupService` 改为发出 `CleanupReviewDataClicked` VisualEvent。
- 新增 `FBlueprintHelperMainWindowPresenter` 接收 VisualEvent，调度清理 service，并通过 PresenterEvent 通知 Visual 显示/更新清理通知。
- 清理完成后 Presenter 调用 `FBlueprintHelperReviewStoreService::NotifyPendingReviewChanged()`，保持 ReviewPanel 继续通过 ReviewStore 的 DataChanged 路径刷新。
- 清理异步任务追踪从 MainWindow 匿名 namespace 移到 `FBlueprintHelperMainWindowCleanupAsyncUtils`，符合不使用 namespace、按功能拆分 Utils 类的约束。

## 验证结果

- `git diff --check`：通过，仅有工作区既有 CRLF 提示。
- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`：通过。
- `node --test AgentFaceService/task-core/build/tests/architecture/architecture-boundaries.test.js`：4/4 通过。

## 后续 P1/P2/P3

- ReviewPanel 仍存在较多 Visual、Presenter、Data 混合逻辑，后续需要继续拆分为 ReviewPanelPresenter、ReviewPanelDataSnapshot 和 Review action command/service。
- Accept/Reject 操作应按同样写路径改造：Visual 事件 -> Presenter command -> ReviewActionService -> ReviewStore DataChanged -> Presenter 只读快照 -> Visual 更新。
- 选择、过滤、Diff 展示等只读行为应优先落到 Presenter 读模型，减少 Slate widget 内直接拼装业务状态。
