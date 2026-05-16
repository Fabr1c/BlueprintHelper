# BlueprintHelper Review 系统 Visual-Presenter-Data 架构拆分记录

日期：2026-05-16

## 目标

- Review UI 遵循 `Visual -> Presenter -> Data`。
- Visual 只负责 Slate 控件、用户输入、通知和局部刷新，不直接执行 Review 写命令。
- Presenter 接收 VisualEvent，读取 DataSnapshot，并调度 CommandService。
- CommandService 包装 ReviewActionService 等写命令。
- 写命令完成后仍通过 ReviewStore 的 DataChanged 路径刷新 Visual。
- Review 目录不再使用 namespace；原 namespace 内容按职责移动到 Utils 类。
- Review UI 内不保留长 switch，枚举映射改为表驱动。

## 本轮拆分

- 新增 `FBlueprintHelperReviewPanelPresenter`：
  - 包装 ReviewStore 读取和 PendingReviewChanged 订阅。
  - 接收 Accept/Reject VisualEvent。
  - 只通过 `FBlueprintHelperReviewPanelDataSnapshot` 读取 pending changes。
- 新增 `FBlueprintHelperReviewPanelCommandService`：
  - 集中包装 AcceptVisibleChange、RejectVisibleChange、RejectLifecycleRootVisibleChange。
  - 保留无 ActionService 时的 fallback 行为。
- 新增 `FBlueprintHelperReviewPanelDataSnapshot`：
  - 承载 PendingChanges、SelectedChangeId、SelectedAssetPath。
  - 用于 Presenter 读取 Data，而不是从 Visual 直接散传状态。
- `SBlueprintHelperReviewPanel` 改造：
  - Store 读取、DataChanged 订阅、Accept/Reject 写命令均改为通过 Presenter。
  - Reject 异步准备继续由 Visual 定时器驱动，但异步任务追踪移入 Utils。
  - 删除旧的不可达 Reject 分支。
- Utils 拆分：
  - `FBlueprintHelperReviewPanelAsyncUtils`
  - `FBlueprintHelperReviewPanelLocalUtils`
  - `FBlueprintHelperReviewDebugBundleUtils`
  - `FBlueprintHelperReviewAssetContextUtils`
- 表驱动替换：
  - ReviewPanel surface overlay 刷新。
  - ReviewPanel geometry overlay 解析。
  - AssetKind/Surface/ChangeKind 文本映射。
  - SurfaceRouter main workspace route 和 legacy fallback route。
- Systems/Shared Review 追加拆分：
  - `FBlueprintHelperReviewedDataCleanupServiceUtils` 接管 ReviewedDataCleanupService 原匿名 namespace 工具函数。
  - `FBlueprintHelperReviewBaselineSnapshotServiceUtils` 接管 BaselineSnapshotService 原局部 namespace 工具函数。
  - `BlueprintHelperReviewTypes.h` 的 enum -> string/color 映射改为表驱动。

## 当前边界

当前 Review UI 的主要写路径已经变为：

```text
Visual click/selection event
  -> ReviewPanel VisualEvent
  -> ReviewPanelPresenter
  -> ReviewPanelCommandService
  -> ReviewActionService / ReviewStore mutation
  -> ReviewStore DataChanged
  -> ReviewPanelPresenter reads pending visible changes
  -> Visual refresh
```

## 验证结果

- `git diff --check`：通过，仅有工作区既有 CRLF 提示。
- `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`：通过。
- `node --test AgentFaceService/task-core/build/tests/architecture/architecture-boundaries.test.js`：4/4 通过。
- `rg -n "^namespace|namespace \{|switch \(|case " BlueprintHelper/Source/BlueprintHelper/Private/UI/Review BlueprintHelper/Source/BlueprintHelper/Public/UI/Review BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review`：无匹配。

## 后续风险

- ReviewPanel 仍有较多 Visual 内部刷新编排函数，这是 Slate 层复杂度，不是服务写命令耦合；后续可以继续把刷新组合拆为 ViewBinder。
- Native row/presenter 组件仍通过回调把按钮事件传回 ReviewPanel；目前已保持 Visual 事件路径，后续可以进一步统一成 ReviewPanelPresenterEventSink。
